package main

import (
	"context"
	"errors"
	throne "github.com/throneproj/Throne-api"
	"golang.org/x/sys/windows"
	"net"
	"os/exec"
	"path/filepath"
	"strconv"
	"time"
)

// Calls are serialized by the controller, including replacement after the first TUN grant.
type desktopEngine struct {
	exe, dir string
	job      windows.Handle
	api      *throne.Client
	cmd      *exec.Cmd
	done     <-chan struct{}
}

func (r *desktopEngine) start(ctx context.Context) (*throne.Client, error) {
	api, cmd, done, e := launchEngine(ctx, r.exe, r.dir, hidden, func(cmd *exec.Cmd) error { return attach(r.job, cmd) })
	if e != nil {
		return nil, e
	}
	r.api, r.cmd, r.done = api, cmd, done
	addr, e := freeAddr()
	if e == nil {
		_, p, _ := net.SplitHostPort(addr)
		port, _ := strconv.Atoi(p)
		_, e = api.SetSettings(ctx, map[string]any{"inbound_address": "127.0.0.1", "inbound_port": port, "inbound_enabled": true})
	}
	if e != nil {
		r.close()
		return nil, e
	}
	return api, nil
}
func (r *desktopEngine) close() {
	if r.api == nil {
		return
	}
	shutdown(r.api)
	select {
	case <-r.done:
	case <-time.After(4 * time.Second):
		r.cmd.Process.Kill()
		<-r.done
	}
	r.api = nil
}
func (r *desktopEngine) grant(ctx context.Context) (engineAPI, error) {
	if e := corePermission(ctx, r.exe); e != nil {
		return nil, e
	}
	if e := ctx.Err(); e != nil {
		return nil, e
	}
	r.close()
	api, e := r.start(ctx)
	if e != nil {
		return nil, errors.New("Не удалось повторно запустить движок после настройки TUN. Закрой и открой клиент.")
	}
	return api, nil
}
func (c *controller) serveDesktop(exe, dir string) {
	job, e := ownEngine()
	if e != nil {
		c.fail("Windows не разрешила запустить движок.")
		return
	}
	defer windows.CloseHandle(job)
	runner := desktopEngine{job: job, exe: exe, dir: filepath.Join(dir, "throne")}
	ctx, cancel := context.WithTimeout(c.ctx, 30*time.Second)
	api, e := runner.start(ctx)
	if e == nil {
		c.mu.Lock()
		c.api = api
		c.engineDone = runner.done
		c.grantAndReload = func(ctx context.Context) (engineAPI, error) {
			next, e := runner.grant(ctx)
			if e == nil {
				c.mu.Lock()
				c.engineDone = runner.done
				c.mu.Unlock()
			}
			return next, e
		}
		c.mu.Unlock()
		e = c.refresh(ctx)
	}
	cancel()
	if e != nil {
		runner.close()
		c.fail("Не удалось запустить движок. Распакуй ZIP целиком и открой приложение снова.")
		return
	}
	c.monitor()
	c.stop()
	c.mu.Lock()
	c.mu.Unlock()
	c.tasks.Wait()
	c.route.Restore()
	runner.close()
}
