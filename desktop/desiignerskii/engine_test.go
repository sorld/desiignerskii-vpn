package main

import (
	"context"
	"errors"
	"strings"
	"testing"

	throne "github.com/throneproj/Throne-api"
)

func TestDeviceLink(t *testing.T) {
	k := strings.Repeat("a", 43)
	want := "https://desiignerskii.pro/s/" + k
	for _, v := range []string{want, "https://desiignerskii.pro/join/" + k, " " + want + "?client=happ "} {
		out, e := subscription(v)
		if e != nil || out != want {
			t.Fatalf("valid link rejected")
		}
	}
	for _, v := range []string{"https://desiignerskii.pro.evil.invalid/s/" + k, "http://desiignerskii.pro/s/" + k, "https://bad@desiignerskii.pro/s/" + k, want[:len(want)-1], want + "/status", want + "/x", ""} {
		if _, e := subscription(v); e == nil {
			t.Fatal("accepted invalid link")
		}
	}
}

type fakeEngine struct {
	engineAPI
	groups         []throne.Group
	profiles       []throne.Profile
	ok             bool
	added, updated int
	modes          []bool
	mode           bool
	modeErr        error
	working        bool
	stopped        int
}

func (f *fakeEngine) Groups(context.Context) ([]throne.Group, error) { return f.groups, nil }
func (f *fakeEngine) Profiles(context.Context, throne.Selection) ([]throne.Profile, error) {
	return f.profiles, nil
}
func (f *fakeEngine) Status(context.Context) (*throne.Status, error) {
	return &throne.Status{State: throne.StateIdle, LastProfileID: -1}, nil
}
func (f *fakeEngine) AddGroup(ctx context.Context, g throne.NewGroup, opts ...throne.Option) (*throne.GroupResult, error) {
	f.added++
	f.groups = append(f.groups, throne.Group{ID: 4, Name: g.Name, Subscription: true})
	return &throne.GroupResult{Group: f.groups[0], Update: &throne.UpdateResult{OK: f.ok}}, nil
}
func (f *fakeEngine) UpdateGroup(ctx context.Context, id int, opts ...throne.Option) (*throne.GroupResult, error) {
	f.updated++
	return &throne.GroupResult{Group: f.groups[0], Update: &throne.UpdateResult{OK: f.ok}}, nil
}
func (f *fakeEngine) SetModes(ctx context.Context, m throne.Modes, opts ...throne.Option) (*throne.Status, error) {
	f.modes = append(f.modes, *m.Tun)
	if f.modeErr != nil {
		return nil, f.modeErr
	}
	f.mode = *m.Tun
	return &throne.Status{Tun: f.mode}, nil
}
func (f *fakeEngine) Connect(ctx context.Context, id int, opts ...throne.Option) (*throne.Status, error) {
	if !f.working {
		return nil, errors.New("fixture: server unavailable")
	}
	return &throne.Status{State: throne.StateRunning, Core: true, Tun: f.mode, Profile: &throne.Profile{ID: id}, Inbound: throne.Inbound{Enabled: true, Address: "127.0.0.1", Port: 2080}}, nil
}
func (f *fakeEngine) Disconnect(context.Context, ...throne.Option) (*throne.Status, error) {
	f.stopped++
	return &throne.Status{State: throne.StateIdle}, nil
}

type memoryRoute struct {
	applied  int
	restored int
}

func (r *memoryRoute) Apply(string) error { r.applied++; return nil }
func (r *memoryRoute) Restore() error     { r.restored++; return nil }
func TestImportRetryAndFailureState(t *testing.T) {
	f := &fakeEngine{}
	r := &memoryRoute{}
	c := newController(r)
	defer c.stop()
	c.api = f
	url := "https://desiignerskii.pro/join/" + strings.Repeat("A", 43)
	if c.importDevice(c.ctx, url) == nil {
		t.Fatal("failure reported as success")
	}
	f.ok = true
	if c.importDevice(c.ctx, url) == nil {
		t.Fatal("empty group reported as success")
	}
	f.profiles = []throne.Profile{{ID: 0, Name: "test", Group: 4}}
	if e := c.importDevice(c.ctx, url); e != nil {
		t.Fatal(e)
	}
	if f.added != 1 || f.updated != 2 {
		t.Fatal("retries duplicated subscription")
	}
	if e := c.refresh(c.ctx); e != nil {
		t.Fatal(e)
	}
	if v := c.snapshot(); v.Selected != 0 || len(v.Nodes) != 1 {
		t.Fatal("profile zero lost")
	}
	if e := c.change(c.ctx, "connect", "", 0); e == nil || c.snapshot().Active || r.applied != 0 {
		t.Fatal("broken start affected proxy")
	}
	c.view.Active = true
	if c.change(c.ctx, "import", url, -1) == nil {
		t.Fatal("changed subscription during active connection")
	}
	if e := c.change(c.ctx, "disconnect", "", 0); e != nil || r.restored != 0 || c.snapshot().Active {
		t.Fatal("failed to restore route")
	}
}

func TestDefaultTunGrantAndNoFallback(t *testing.T) {
	denied := &throne.APIError{Status: 412, Code: throne.CodeNeedsPrivileges}
	for _, tc := range []struct {
		name                       string
		initial, permission, probe error
		success                    bool
		ask                        bool
	}{
		{"already-granted", nil, nil, nil, true, false},
		{"accept-uac", denied, nil, nil, true, true},
		{"cancel-uac", denied, errors.New("cancelled"), nil, false, true},
		{"conflicting-adapter", &throne.APIError{Status: 409, Code: throne.CodeTunConflict}, nil, nil, false, false},
		{"wrong-interface", nil, nil, errors.New("wrong route"), false, false},
	} {
		t.Run(tc.name, func(t *testing.T) {
			r := &memoryRoute{}
			f := &fakeEngine{working: true, modeErr: tc.initial}
			c := newController(r)
			defer c.stop()
			c.api = f
			c.view.Selected = 4
			asked := false
			newCore := &fakeEngine{working: true}
			c.grantAndReload = func(context.Context) (engineAPI, error) { asked = true; return newCore, tc.permission }
			c.probe = func(ctx context.Context, addr string) error {
				if addr != "" {
					t.Fatalf("TUN checked through a local proxy")
				}
				return tc.probe
			}
			e := c.change(c.ctx, "connect", "", -1)
			if (e == nil) != tc.success || asked != tc.ask || c.snapshot().Active != tc.success {
				t.Fatalf("connect err=%v active=%v asked=%v", e, c.snapshot().Active, asked)
			}
			if len(f.modes) != 1 || !f.modes[0] || r.applied != 0 || r.restored != 0 {
				t.Fatal("unexpected proxy fallback")
			}
			if tc.probe != nil && f.stopped != 1 {
				t.Fatal("failed route check left engine running")
			}
			if tc.success {
				if e = c.change(c.ctx, "disconnect", "", -1); e != nil || r.restored != 0 || c.snapshot().Active {
					t.Fatal("TUN did not disconnect independently")
				}
			}
		})
	}
}
func TestExplicitProxyAndPreferences(t *testing.T) {
	dir := t.TempDir()
	r := &memoryRoute{}
	c := newController(r)
	defer c.stop()
	c.prefDir = dir
	c.api = &fakeEngine{working: true}
	c.view.Selected = 0
	c.view.Nodes = []node{{ID: 0, Name: "test"}}
	if e := c.updatePreference("mode", "proxy", -1); e != nil {
		t.Fatal(e)
	}
	if e := c.updatePreference("favorite", "", 0); e != nil {
		t.Fatal(e)
	}
	p := readPreferences(dir)
	if p.Mode != proxyMode || len(p.Favorites) != 1 || p.Favorites[0] != 0 {
		t.Fatal("settings did not survive restart")
	}
	c.probe = func(ctx context.Context, addr string) error {
		if addr != "127.0.0.1:2080" {
			t.Fatal("missing proxy")
		}
		return nil
	}
	if e := c.change(c.ctx, "connect", "", -1); e != nil || r.applied != 1 {
		t.Fatalf("proxy not enabled: %v", e)
	}
	if c.updatePreference("mode", "tun", -1) == nil {
		t.Fatal("mode changed on a live connection")
	}
	if e := c.change(c.ctx, "disconnect", "", -1); e != nil || r.restored != 1 {
		t.Fatal("proxy not restored")
	}
}
