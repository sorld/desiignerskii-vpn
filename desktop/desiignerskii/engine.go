package main

import (
	"context"
	"crypto/sha256"
	"errors"
	"fmt"

	"net"
	"net/url"
	"os"
	"os/exec"
	"path/filepath"
	"regexp"
	"strings"
	"sync"
	"time"

	throne "github.com/throneproj/Throne-api"
)

type node struct {
	Group   int    `json:"group"`
	ID      int    `json:"id"`
	Name    string `json:"name"`
	Country string `json:"country"`
	Latency int    `json:"latency"`
	Up      int64  `json:"up"`
	Down    int64  `json:"down"`
}
type deviceGroup struct {
	ID      int    `json:"id"`
	Name    string `json:"name"`
	Updated int64  `json:"updated"`
}
type viewState struct {
	Settings  map[string]any   `json:"settings"`
	Routes    []throne.Route   `json:"routes"`
	Editor    *routeEditor     `json:"editor"`
	Logs      []throne.LogLine `json:"logs"`
	Restart   bool             `json:"restart"`
	Uncertain bool             `json:"uncertain"`
	Groups    []deviceGroup    `json:"groups"`
	Mode      string           `json:"mode"`
	Favorites []int            `json:"favorites"`
	State     string           `json:"state"`
	Nodes     []node           `json:"nodes"`
	Selected  int              `json:"selected"`
	Active    bool             `json:"active"`
	Since     int64            `json:"since"`
	Error     string           `json:"error"`
	Busy      bool             `json:"busy"`
}
type systemRoute interface {
	Apply(string) error
	Restore() error
}
type engineAPI interface {
	Settings(context.Context) (map[string]any, error)
	SetSettings(context.Context, map[string]any) (*throne.SettingsChange, error)
	Routes(context.Context) ([]throne.Route, error)
	RouteDetail(context.Context, int) (*throne.RouteDetail, error)
	UpdateRoute(context.Context, int, throne.RouteEdit) (*throne.RouteChange, error)
	SetRoute(context.Context, int, ...throne.Option) (*throne.Status, error)
	Logs(context.Context, throne.LogQuery) (*throne.Logs, error)
	SetModes(context.Context, throne.Modes, ...throne.Option) (*throne.Status, error)
	RunTest(context.Context, throne.TestKind, throne.Selection) (*throne.Job, error)
	Profiles(context.Context, throne.Selection) ([]throne.Profile, error)
	Groups(context.Context) ([]throne.Group, error)
	Status(context.Context) (*throne.Status, error)
	AddGroup(context.Context, throne.NewGroup, ...throne.Option) (*throne.GroupResult, error)
	UpdateGroup(context.Context, int, ...throne.Option) (*throne.GroupResult, error)
	Connect(context.Context, int, ...throne.Option) (*throne.Status, error)
	Disconnect(context.Context, ...throne.Option) (*throne.Status, error)
}
type controller struct {
	engineDone     <-chan struct{}
	prefDir        string
	grantAndReload func(context.Context) (engineAPI, error)
	probe          func(context.Context, string) error
	mu             sync.Mutex
	view           viewState
	api            engineAPI
	route          systemRoute
	ctx            context.Context
	stop           context.CancelFunc
	tasks          sync.WaitGroup
}

func newController(r systemRoute) *controller {
	ctx, stop := context.WithCancel(context.Background())
	return &controller{ctx: ctx, stop: stop, route: r, probe: checkExit, view: viewState{State: "starting", Selected: -1, Mode: tunMode}}
}
func (c *controller) snapshot() viewState { c.mu.Lock(); defer c.mu.Unlock(); return c.view }

var linkPath = regexp.MustCompile(`^/(?:s|join)/([A-Za-z0-9_-]{43})$`)

func subscription(raw string) (string, error) {
	u, e := url.Parse(strings.TrimSpace(raw))
	invalid := errors.New("Вставь ссылку устройства из личного кабинета.")
	if e != nil || u.Scheme != "https" || !strings.EqualFold(u.Host, "desiignerskii.pro") || u.User != nil || u.Opaque != "" {
		return "", invalid
	}
	m := linkPath.FindStringSubmatch(u.EscapedPath())
	if m == nil {
		return "", invalid
	}
	return "https://desiignerskii.pro/s/" + m[1], nil
}
func freeAddr() (string, error) {
	l, e := net.Listen("tcp4", "127.0.0.1:0")
	if e != nil {
		return "", e
	}
	defer l.Close()
	return l.Addr().String(), nil
}
func launchEngine(ctx context.Context, exe, dir string, configure func(*exec.Cmd), started func(*exec.Cmd) error) (*throne.Client, *exec.Cmd, <-chan struct{}, error) {
	if e := wantDir(dir); e != nil {
		return nil, nil, nil, e
	}
	addr, e := freeAddr()
	if e != nil {
		return nil, nil, nil, e
	}
	cmd := exec.Command(exe, "--headless", "--data-dir", dir, "--api-listen", addr, "--restart-mode", "exit", "--no-resume")
	cmd.Dir = filepath.Dir(exe)
	configure(cmd)
	if e = cmd.Start(); e != nil {
		return nil, nil, nil, e
	}
	if e = started(cmd); e != nil {
		cmd.Process.Kill()
		cmd.Wait()
		return nil, nil, nil, e
	}
	done := make(chan struct{})
	go func() { cmd.Wait(); close(done) }()
	timer := time.NewTicker(180 * time.Millisecond)
	defer timer.Stop()
	for {
		select {
		case <-done:
			return nil, nil, nil, errors.New("Движок завершил работу при запуске. Закрой другой экземпляр клиента и открой приложение снова.")
		case <-ctx.Done():
			cmd.Process.Kill()
			<-done
			return nil, nil, nil, ctx.Err()
		case <-timer.C:
			key, e := throne.ReadKeyFile(filepath.Join(dir, "config", "api.key"))
			if e != nil {
				continue
			}
			api := throne.New(addr, key)
			ping, end := context.WithTimeout(ctx, time.Second)
			st, e := api.Status(ping)
			end()
			if e == nil && st.Core && st.Capabilities.Tun != nil {
				return api, cmd, done, nil
			}
		}
	}
}
func (c *controller) refresh(ctx context.Context) error {
	profiles, e := c.api.Profiles(ctx, throne.Selection{})
	if e != nil {
		return errors.New("Нет ответа от движка подключения.")
	}
	st, e := c.api.Status(ctx)
	if e != nil {
		return errors.New("Не удалось проверить соединение.")
	}
	gs, e := c.api.Groups(ctx)
	if e != nil {
		return errors.New("Не удалось прочитать устройства.")
	}
	groups := make([]deviceGroup, 0, len(gs))
	for _, g := range gs {
		if !g.Archived && (g.Subscription || g.ProfileCount > 0) {
			name := g.Name
			if strings.HasPrefix(name, "Устройство ") {
				name = fmt.Sprintf("Устройство %d", len(groups)+1)
			}
			groups = append(groups, deviceGroup{g.ID, name, g.UpdatedAt})
		}
	}
	ns := make([]node, 0, len(profiles))
	for _, p := range profiles {
		ns = append(ns, node{ID: p.ID, Name: p.Name, Group: p.Group, Country: p.Country, Latency: p.LatencyMS, Up: p.TrafficUp, Down: p.TrafficDown})
	}
	c.mu.Lock()
	defer c.mu.Unlock()
	c.view.Nodes = ns
	c.view.Groups = groups
	c.view.State = st.State
	c.view.Restart = st.RestartNeeded
	selected := c.view.Selected
	if st.Profile != nil {
		selected = st.Profile.ID
	} else if selected < 0 {
		selected = st.LastProfileID
	}
	found := false
	for _, p := range ns {
		if p.ID == selected {
			found = true
			break
		}
	}
	if !found {
		selected = -1
		if len(ns) > 0 {
			selected = ns[0].ID
		}
	}
	c.view.Selected = selected
	return nil
}
func (c *controller) importDevice(ctx context.Context, raw string) error {
	link, e := subscription(raw)
	if e != nil {
		return e
	}
	sum := sha256.Sum256([]byte(link))
	name := fmt.Sprintf("Устройство %x", sum[:8])
	groups, e := c.api.Groups(ctx)
	if e != nil {
		return errors.New("Не удалось прочитать список устройств.")
	}
	var res *throne.GroupResult
	for _, g := range groups {
		if g.Name == name && g.Subscription {
			res, e = c.api.UpdateGroup(ctx, g.ID)
			break
		}
	}
	if res == nil && e == nil {
		res, e = c.api.AddGroup(ctx, throne.NewGroup{Name: name, URL: link})
	}
	if e != nil || res == nil || res.Update == nil || !res.Update.OK {
		return errors.New("Не удалось получить профили. Проверь, включено ли устройство в кабинете, и повтори импорт.")
	}
	profiles, e := c.api.Profiles(ctx, throne.SelectGroup(res.Group.ID, ""))
	if e != nil || len(profiles) == 0 {
		return errors.New("В подписке пока нет доступных серверов.")
	}
	c.mu.Lock()
	c.view.Selected = profiles[0].ID
	c.mu.Unlock()
	return nil
}
func (c *controller) change(ctx context.Context, op, arg string, id int) error {
	v := c.snapshot()
	switch op {
	case "settings", "save-settings", "routes", "save-rules", "activate-route", "logs":
		return c.advanced(ctx, op, arg, id)
	case "reconnect":
		if !v.Active {
			return errors.New("Соединение уже отключено.")
		}
		if err := c.change(ctx, "disconnect", "", -1); err != nil {
			return err
		}
		return c.connect(ctx, c.snapshot())
	case "import":
		if v.Active {
			return errors.New("Сначала отключи текущее соединение.")
		}
		return c.importDevice(ctx, arg)
	case "select":
		if id == v.Selected {
			return nil
		}
		valid := false
		for _, n := range v.Nodes {
			if n.ID == id {
				valid = true
				break
			}
		}
		if !valid {
			return errors.New("Сервер больше недоступен.")
		}
		if v.Active {
			if e := c.change(ctx, "disconnect", "", -1); e != nil {
				return e
			}
		}
		c.mu.Lock()
		c.view.Selected = id
		c.mu.Unlock()
		if v.Active {
			return c.connect(ctx, c.snapshot())
		}
		return nil
	case "mode", "favorite":
		return c.updatePreference(op, arg, id)
	case "update":
		if v.Active {
			return errors.New("Сначала отключи соединение.")
		}
		result, e := c.api.UpdateGroup(ctx, id)
		if e != nil || result == nil || result.Update == nil || !result.Update.OK {
			return errors.New("Не удалось обновить подписку.")
		}
		return nil
	case "latency":
		if len(v.Nodes) == 0 {
			return nil
		}
		ids := make([]int, 0, len(v.Nodes))
		for _, n := range v.Nodes {
			ids = append(ids, n.ID)
		}
		_, err := c.api.RunTest(ctx, throne.TestLatency, throne.SelectProfiles(ids...))
		if err != nil {
			return errors.New("Проверка задержки не завершилась.")
		}
		return nil
	case "connect":
		return c.connect(ctx, v)
	case "disconnect":
		if v.Mode == proxyMode {
			if e := c.route.Restore(); e != nil {
				return errors.New("Не удалось восстановить настройки прокси Windows.")
			}
		}
		stopped, e := c.api.Disconnect(ctx)
		if e != nil || stopped == nil || stopped.State != throne.StateIdle {
			return errors.New("Движок ещё не подтвердил отключение. Повтори попытку.")
		}
		c.mu.Lock()
		c.view.Active = false
		c.view.Uncertain = false
		c.view.Since = 0
		c.mu.Unlock()
		return nil
	}
	return errors.New("Неизвестное действие.")
}
func (c *controller) disconnectEngine() {
	ctx, end := context.WithTimeout(context.Background(), 5*time.Second)
	defer end()
	c.api.Disconnect(ctx)
}
func (c *controller) action(op, arg string, id int) {
	c.mu.Lock()
	if c.view.State == "error" || c.view.State == "starting" || c.view.Busy || c.api == nil || c.ctx.Err() != nil {
		c.mu.Unlock()
		return
	}
	c.view.Busy = true
	c.view.Error = ""
	c.tasks.Add(1)
	c.mu.Unlock()
	go func() {
		defer c.tasks.Done()
		ctx, end := context.WithTimeout(c.ctx, 80*time.Second)
		defer end()
		e := c.change(ctx, op, arg, id)
		switch op {
		case "settings", "save-settings", "routes", "save-rules", "activate-route", "logs":
			// These responses already contain the committed state.
		default:
			if re := c.refresh(ctx); e == nil {
				e = re
			}
		}
		c.mu.Lock()
		defer c.mu.Unlock()
		c.view.Busy = false
		if e != nil {
			c.view.Error = e.Error()
		}
	}()
}
func (c *controller) monitor() {
	tick := time.NewTicker(3 * time.Second)
	defer tick.Stop()
	for {
		select {
		case <-c.ctx.Done():
			return
		case <-tick.C:
		}
		c.mu.Lock()
		v, api, done := c.view, c.api, c.engineDone
		if !v.Busy && done != nil {
			select {
			case <-done:
				if v.Mode == proxyMode {
					c.route.Restore()
				}
				c.view.Active = false
				c.view.Uncertain = false
				c.view.Since = 0
				c.view.State = "error"
				c.view.Error = "Движок остановлен. Закрой и открой клиент снова."
				c.mu.Unlock()
				return
			default:
			}
		}
		c.mu.Unlock()
		if !v.Active || v.Busy {
			continue
		}
		ctx, cancel := context.WithTimeout(c.ctx, 2*time.Second)
		st, e := api.Status(ctx)
		cancel()
		c.mu.Lock()
		if c.view.Active && !c.view.Busy && c.view.Since == v.Since {
			if e != nil || st == nil {
				c.view.Uncertain = true
				c.view.Error = "Не удалось проверить состояние соединения."
			} else if st.State == throne.StateRunning && st.Core && st.Tun == (v.Mode == tunMode) {
				if c.view.Uncertain {
					c.view.Error = ""
				}
				c.view.Uncertain = false
				if st.Profile != nil {
					nodes := append([]node(nil), c.view.Nodes...)
					for i := range nodes {
						if nodes[i].ID == st.Profile.ID {
							nodes[i].Up = st.Profile.TrafficUp
							nodes[i].Down = st.Profile.TrafficDown
							break
						}
					}
					c.view.Nodes = nodes
				}
			} else if st.State == throne.StateIdle || !st.Core {
				if v.Mode == tunMode || c.route.Restore() == nil {
					c.view.Active = false
					c.view.Uncertain = false
					c.view.Since = 0
				}
				c.view.Error = "Движок остановил соединение."
			} else {
				c.view.Uncertain = true
				c.view.Error = "Движок изменил режим. Отключи соединение и подключись снова."
			}
		}
		c.mu.Unlock()
	}
}
func (c *controller) fail(message string) {
	c.mu.Lock()
	c.view.State = "error"
	c.view.Error = message
	c.mu.Unlock()
}
func shutdown(api *throne.Client) {
	ctx, end := context.WithTimeout(context.Background(), 5*time.Second)
	defer end()
	api.Shutdown(ctx)
}
func wantDir(path string) error { return os.MkdirAll(path, 0700) }
