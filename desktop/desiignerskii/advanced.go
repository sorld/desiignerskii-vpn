package main

import (
	"context"
	"encoding/json"
	"errors"
	"fmt"
	"regexp"
	"strings"

	throne "github.com/throneproj/Throne-api"
)

var supportedSettings = strings.Fields("tun_mtu tun_ipv6 tun_strict_route tun_ipv4_cidr tun_ipv6_cidr remote_dns direct_dns dns_in_port sub_auto_update sub_respect_server_interval route_auto_update test_url test_timeout_ms test_concurrency log_level")

type routeEditor struct {
	ID        int                `json:"id"`
	Editable  bool               `json:"editable"`
	Simple    throne.SimpleRules `json:"simple"`
	CustomDNS bool               `json:"customDNS"`
}

func allowedSettings(all map[string]any) map[string]any {
	visible := make(map[string]any)
	for _, k := range supportedSettings {
		if value, ok := all[k]; ok {
			visible[k] = value
		}
	}
	return visible
}
func settingsError(err error, fallback string) error {
	switch throne.ErrorCode(err) {
	case throne.CodeBusy:
		return errors.New("Движок занят. Повтори после завершения текущей операции.")
	case throne.CodeEditing:
		return errors.New("Заверши редактирование в другом окне Throne и повтори.")
	case throne.CodeBadRequest:
		for _, k := range supportedSettings {
			if strings.Contains(err.Error(), k) {
				return fmt.Errorf("Движок отклонил значение %s. Проверь формат или диапазон.", k)
			}
		}
	}
	return errors.New(fallback)
}
func (c *controller) advanced(ctx context.Context, op, arg string, id int) error {
	switch op {
	case "settings":
		all, err := c.api.Settings(ctx)
		if err != nil {
			return settingsError(err, "Не удалось загрузить настройки движка.")
		}
		c.mu.Lock()
		c.view.Settings = allowedSettings(all)
		c.mu.Unlock()
	case "save-settings":
		var changes map[string]any
		if len(arg) > 16384 || json.Unmarshal([]byte(arg), &changes) != nil || len(changes) == 0 {
			return errors.New("Нет корректных изменений для сохранения.")
		}
		current := c.snapshot().Settings
		for key, val := range changes {
			if _, ok := current[key]; !ok || val == nil {
				return errors.New("Параметр недоступен. Открой настройки заново.")
			}
			switch val.(type) {
			case string, bool, float64:
			default:
				return errors.New("Некорректное значение параметра.")
			}
		}
		res, err := c.api.SetSettings(ctx, changes)
		if err != nil || res == nil {
			return settingsError(err, "Движок отклонил настройки. Проверь введённые значения.")
		}
		c.mu.Lock()
		c.view.Settings = allowedSettings(res.Settings)
		c.view.Restart = res.RestartNeeded
		c.mu.Unlock()
	case "routes":
		return c.readRoutes(ctx, id)
	case "activate-route":
		if c.snapshot().Active {
			return errors.New("Отключи VPN перед сменой маршрута по умолчанию.")
		}
		_, err := c.api.SetRoute(ctx, id)
		if err != nil {
			return settingsError(err, "Не удалось выбрать маршрут.")
		}
		return c.readRoutes(ctx, id)
	case "save-rules":
		return c.saveRules(ctx, arg, id)
	case "logs":
		result, err := c.api.Logs(ctx, throne.LogQuery{Limit: 80})
		if err != nil || result == nil {
			return settingsError(err, "Не удалось прочитать журнал движка.")
		}
		lines := append([]throne.LogLine{}, result.Lines...)
		for i := range lines {
			lines[i].Text = logSecrets.ReplaceAllString(lines[i].Text, "[скрыто]")
		}
		c.mu.Lock()
		c.view.Logs = lines
		c.mu.Unlock()
	}
	return nil
}

var logSecrets = regexp.MustCompile(`(?i)(?:(?:vless|vmess|ss|trojan|hysteria2|tuic|https?)://[^\s<>"']+|(?:bearer\s+|(?:api[_-]?key|password|token|secret)\s*[:=]\s*)[^\s,;]+)`)

func (c *controller) readRoutes(ctx context.Context, id int) error {
	list, err := c.api.Routes(ctx)
	if err != nil {
		return settingsError(err, "Не удалось прочитать маршруты.")
	}
	if id < 0 && len(list) > 0 {
		id = list[0].ID
		for _, r := range list {
			if r.InUse {
				id = r.ID
				break
			}
			if r.Active {
				id = r.ID
			}
		}
	}
	var editor *routeEditor
	if id >= 0 {
		r, err := c.api.RouteDetail(ctx, id)
		if err != nil || r == nil {
			return settingsError(err, "Не удалось открыть маршрут.")
		}
		editor = &routeEditor{r.ID, r.Kind == throne.RouteKindRules && r.SourceGroupID < 0, r.Simple, r.CustomDNS}
	}
	c.mu.Lock()
	c.view.Routes = list
	c.view.Editor = editor
	c.mu.Unlock()
	return nil
}
func (c *controller) saveRules(ctx context.Context, arg string, id int) error {
	var patch struct {
		Before map[string]string `json:"before"`
		After  map[string]string `json:"after"`
	}
	if len(arg) > 1024*1024 || json.Unmarshal([]byte(arg), &patch) != nil || len(patch.After) == 0 {
		return errors.New("Некорректные правила.")
	}
	r, err := c.api.RouteDetail(ctx, id)
	if err != nil || r == nil {
		return settingsError(err, "Не удалось загрузить текущие правила маршрута.")
	}
	if r.Kind != throne.RouteKindRules || r.SourceGroupID >= 0 {
		return errors.New("Этот маршрут доступен только для чтения.")
	}
	current := map[string]string{"direct": r.Simple.Direct, "proxy": r.Simple.Proxy, "block": r.Simple.Block}
	edit := &throne.SimpleRulesEdit{}
	for key, value := range patch.After {
		existing, ok := current[key]
		old, known := patch.Before[key]
		if !known || !ok || old != existing {
			return errors.New("Правила изменились. Обнови маршрут перед сохранением.")
		}
		text := strings.ReplaceAll(value, "\r\n", "\n")
		switch key {
		case "direct":
			edit.Direct = &text
		case "proxy":
			edit.Proxy = &text
		case "block":
			edit.Block = &text
		}
	}
	result, err := c.api.UpdateRoute(ctx, id, throne.RouteEdit{Simple: edit})
	if err != nil || result == nil {
		return settingsError(err, "Не удалось сохранить правила маршрута. Проверь синтаксис.")
	}
	c.mu.Lock()
	c.view.Editor = &routeEditor{result.Route.ID, true, result.Route.Simple, result.Route.CustomDNS}
	c.view.Restart = result.RestartNeeded
	c.mu.Unlock()
	return nil
}
