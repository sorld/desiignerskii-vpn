package main

import (
	"encoding/json"
	"errors"
	"os"
	"path/filepath"
	"slices"
)

const tunMode = "tun"
const proxyMode = "proxy"

type preferences struct {
	Mode      string `json:"mode"`
	Favorites []int  `json:"favorites"`
}

func readPreferences(dir string) preferences {
	var p preferences
	data, err := os.ReadFile(filepath.Join(dir, "desktop.json"))
	if err == nil {
		json.Unmarshal(data, &p)
	}
	if p.Mode != proxyMode {
		p.Mode = tunMode
	}
	return p
}
func (c *controller) updatePreference(op, arg string, id int) error {
	v := c.snapshot()
	p := preferences{Mode: v.Mode, Favorites: slices.Clone(v.Favorites)}
	if op == "mode" {
		if v.Active {
			return errors.New("Отключи VPN перед сменой режима.")
		}
		if arg != tunMode && arg != proxyMode {
			return errors.New("Неизвестный режим.")
		}
		p.Mode = arg
	} else {
		if !slices.ContainsFunc(v.Nodes, func(n node) bool { return n.ID == id }) {
			return errors.New("Сервер больше недоступен.")
		}
		if i := slices.Index(p.Favorites, id); i >= 0 {
			p.Favorites = slices.Delete(p.Favorites, i, i+1)
		} else {
			p.Favorites = append(p.Favorites, id)
		}
	}
	if c.prefDir != "" {
		data, e := json.Marshal(p)
		if e != nil {
			return e
		}
		path := filepath.Join(c.prefDir, "desktop.json")
		if e = os.WriteFile(path+".tmp", data, 0600); e == nil {
			e = os.Rename(path+".tmp", path)
		}
		if e != nil {
			return errors.New("Не удалось сохранить настройки.")
		}
	}
	c.mu.Lock()
	c.view.Mode = p.Mode
	c.view.Favorites = p.Favorites
	c.mu.Unlock()
	return nil
}
