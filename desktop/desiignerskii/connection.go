package main

import (
	"context"
	"errors"
	"fmt"
	throne "github.com/throneproj/Throne-api"
	"net"
	"time"
)

func modeError(err error) error {
	if throne.ErrorCode(err) == throne.CodeNeedsPrivileges {
		return errors.New("Движок пока не получил права для TUN. Повтори подключение.")
	}
	if throne.ErrorCode(err) == throne.CodeTunConflict {
		return errors.New("Другой VPN уже использует адаптер TUN. Отключи его и повтори.")
	}
	return errors.New("Throne не смог включить выбранный режим подключения.")
}
func (c *controller) connect(ctx context.Context, v viewState) error {
	if v.Active {
		return nil
	}
	if v.Selected < 0 {
		return errors.New("Сначала добавь устройство из кабинета.")
	}
	tun := v.Mode == tunMode
	modes := throne.Modes{Tun: &tun}
	state, err := c.api.SetModes(ctx, modes)
	if tun && throne.ErrorCode(err) == throne.CodeNeedsPrivileges {
		if c.grantAndReload == nil {
			return errors.New("Для TUN нужны права на создание сетевого адаптера.")
		}
		next, e := c.grantAndReload(ctx)
		if e != nil {
			return e
		}
		c.mu.Lock()
		c.api = next
		c.mu.Unlock()
		state, err = next.SetModes(ctx, modes)
	}
	if err != nil {
		return modeError(err)
	}
	if state == nil || state.Tun != tun {
		return errors.New("Движок не подтвердил выбранный режим.")
	}
	st, err := c.api.Connect(ctx, v.Selected)
	if err != nil || st == nil || st.State != throne.StateRunning || st.Tun != tun || !st.Core || st.Profile == nil || st.Profile.ID != v.Selected {
		c.disconnectEngine()
		return errors.New("Throne не смог запустить выбранный профиль.")
	}
	var addr string
	if !tun {
		ip := net.ParseIP(st.Inbound.Address)
		if !st.Inbound.Enabled || st.Inbound.Port <= 0 || ip == nil || !ip.IsLoopback() {
			c.disconnectEngine()
			return errors.New("Локальный порт прокси недоступен.")
		}
		addr = net.JoinHostPort(st.Inbound.Address, fmt.Sprint(st.Inbound.Port))
	}
	if e := c.probe(ctx, addr); e != nil {
		c.disconnectEngine()
		if tun {
			return errors.New("HTTPS через адаптер TUN не прошёл. Соединение остановлено.")
		}
		return errors.New("Сервер не ответил через прокси. Попробуй другой сервер.")
	}
	if !tun {
		if e := c.route.Apply(addr); e != nil {
			c.route.Restore()
			c.disconnectEngine()
			return errors.New("Не удалось включить системный прокси Windows.")
		}
	}
	c.mu.Lock()
	c.view.Active = true
	c.view.Uncertain = false
	c.view.Since = time.Now().Unix()
	c.mu.Unlock()
	return nil
}
