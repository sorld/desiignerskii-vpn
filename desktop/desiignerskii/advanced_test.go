package main

import (
	"context"
	"errors"
	throne "github.com/throneproj/Throne-api"
	"reflect"
	"strings"
	"testing"
)

type settingsFixture struct {
	fakeEngine
	data   map[string]any
	writes []map[string]any
	reject bool
	route  throne.RouteDetail
	edit   *throne.RouteEdit
}

func (f *settingsFixture) Settings(context.Context) (map[string]any, error) { return f.data, nil }
func (f *settingsFixture) SetSettings(_ context.Context, patch map[string]any) (*throne.SettingsChange, error) {
	if f.reject {
		return nil, &throne.APIError{Code: throne.CodeBusy}
	}
	f.writes = append(f.writes, patch)
	for k, v := range patch {
		f.data[k] = v
	}
	return &throne.SettingsChange{Settings: f.data, RestartNeeded: true}, nil
}
func (f *settingsFixture) Routes(context.Context) ([]throne.Route, error) {
	return []throne.Route{{ID: 5, Active: true}, {ID: f.route.ID, InUse: true}}, nil
}
func (f *settingsFixture) RouteDetail(_ context.Context, id int) (*throne.RouteDetail, error) {
	if id != f.route.ID {
		return nil, errors.New("bad id")
	}
	return &f.route, nil
}
func (f *settingsFixture) UpdateRoute(_ context.Context, id int, edit throne.RouteEdit) (*throne.RouteChange, error) {
	f.edit = &edit
	f.route.Simple.Direct = *edit.Simple.Direct
	return &throne.RouteChange{Route: f.route, RestartNeeded: true}, nil
}
func TestAdvancedSettingsApplyWhileConnected(t *testing.T) {
	f := &settingsFixture{data: map[string]any{"tun_mtu": float64(1500), "sub_auto_update": float64(30), "kill_switch": false}}
	c := newController(&memoryRoute{})
	defer c.stop()
	c.api = f
	c.view.Active = true
	if err := c.change(c.ctx, "settings", "", 0); err != nil {
		t.Fatal(err)
	}
	saved := c.snapshot().Settings
	if len(saved) != 2 {
		t.Fatal(saved)
	}
	for _, patch := range []string{`{}`, `{"kill_switch":true}`, `{"tun_mtu":null}`, `{"tun_mtu":[]}`, `{"tun_ipv6":true}`} {
		if err := c.change(c.ctx, "save-settings", patch, 0); err == nil {
			t.Fatal(patch)
		}
	}
	f.reject = true
	if err := c.change(c.ctx, "save-settings", `{"tun_mtu":1300}`, 0); err == nil {
		t.Fatal("failure hidden")
	}
	f.reject = false
	if err := c.change(c.ctx, "save-settings", `{"tun_mtu":1400}`, 0); err != nil {
		t.Fatal(err)
	}
	if len(f.writes) != 1 || len(f.writes[0]) != 1 || saved["tun_mtu"] != float64(1500) || c.snapshot().Settings["tun_mtu"] != float64(1400) || !c.snapshot().Restart || !c.snapshot().Active || f.stopped != 0 {
		t.Fatal("setting or live connection corrupted")
	}
}
func TestRouteEditsProtectSubscriptionAndUntouchedRules(t *testing.T) {
	f := &settingsFixture{route: throne.RouteDetail{ID: 7, Kind: throne.RouteKindRules, SourceGroupID: -1, Simple: throne.SimpleRules{Direct: "suffix:old.org", Proxy: "processName:editor.exe", WarpBypass: "ip:10.0.0.1"}}}
	c := newController(&memoryRoute{})
	defer c.stop()
	c.api = f
	if err := c.change(c.ctx, "routes", "", -1); err != nil || c.snapshot().Editor.ID != 7 {
		t.Fatal(err)
	}
	patch := `{"before":{"direct":"suffix:old.org"},"after":{"direct":"processName:app.exe"}}`
	f.route.SourceGroupID = 8
	if err := c.change(c.ctx, "save-rules", patch, 7); err == nil {
		t.Fatal("subscription route modified")
	}
	f.route.SourceGroupID = -1
	f.route.Simple.Direct = ""
	if err := c.change(c.ctx, "save-rules", patch, 7); err == nil || f.edit != nil {
		t.Fatal("concurrent edit overwritten")
	}
	f.route.Simple.Direct = "suffix:old.org"
	if err := c.change(c.ctx, "save-rules", patch, 7); err != nil {
		t.Fatal(err)
	}
	if !reflect.DeepEqual(*f.edit, throne.RouteEdit{Simple: &throne.SimpleRulesEdit{Direct: &f.route.Simple.Direct}}) || c.snapshot().Editor.Simple.Proxy != "processName:editor.exe" || c.snapshot().Editor.Simple.WarpBypass != "ip:10.0.0.1" {
		t.Fatal("unrelated rules modified")
	}
}
func TestShownLogConcealsSubscription(t *testing.T) {
	line := "DNS started for https://vpn.invalid/s/private; bearer secret-key"
	clean := logSecrets.ReplaceAllString(line, "[скрыто]")
	if strings.Contains(clean, "private") || strings.Contains(clean, "secret-key") || !strings.Contains(clean, "DNS started") {
		t.Fatal("log redaction")
	}
}
