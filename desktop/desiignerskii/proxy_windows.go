package main

import (
	"encoding/json"
	"errors"
	"fmt"
	"os"
	"path/filepath"
	"runtime"
	"unsafe"

	"golang.org/x/sys/windows"
)

// INTERNET_PER_CONN_OPTION_LISTW targets the user's LAN connection.
type inetOption struct {
	ID    uint32
	Value uintptr
}
type inetList struct {
	Size       uint32
	Connection *uint16
	Count      uint32
	Failed     uint32
	Options    *inetOption
}
type proxyConfig struct {
	Flags  uint32
	Server string
}
type proxyJournal struct {
	Owner   string
	Prior   proxyConfig
	Address string
}
type winRoute struct{ dir, owner string }

var inet = windows.NewLazySystemDLL("wininet.dll")
var queryOption = inet.NewProc("InternetQueryOptionW")
var setOption = inet.NewProc("InternetSetOptionW")
var globalFree = windows.NewLazySystemDLL("kernel32.dll").NewProc("GlobalFree")

func inetCall(set bool, opts []inetOption) error {
	list := inetList{Count: uint32(len(opts)), Options: &opts[0]}
	list.Size = uint32(unsafe.Sizeof(list))
	size := list.Size
	var ok uintptr
	var err error
	if set {
		ok, _, err = setOption.Call(0, 75, uintptr(unsafe.Pointer(&list)), uintptr(size))
	} else {
		ok, _, err = queryOption.Call(0, 75, uintptr(unsafe.Pointer(&list)), uintptr(unsafe.Pointer(&size)))
	}
	runtime.KeepAlive(opts)
	runtime.KeepAlive(&list)
	if ok == 0 {
		return err
	}
	return nil
}
func readProxy() (proxyConfig, error) {
	// FLAGS_UI retains the automatic detection preference shown in Windows settings.
	opts := []inetOption{{ID: 10}, {ID: 2}}
	if e := inetCall(false, opts); e != nil {
		opts = []inetOption{{ID: 1}, {ID: 2}}
		if e = inetCall(false, opts); e != nil {
			return proxyConfig{}, e
		}
	}
	p := proxyConfig{Flags: uint32(opts[0].Value)}
	// INTERNET_PER_CONN_PROXY_SERVER returns a WinINet-owned LPWSTR in the option union.
	server := *(**uint16)(unsafe.Pointer(&opts[1].Value))
	if server != nil {
		p.Server = windows.UTF16PtrToString(server)
		globalFree.Call(uintptr(unsafe.Pointer(server)))
	}
	return p, nil
}
func writeProxy(p proxyConfig) error {
	server, e := windows.UTF16PtrFromString(p.Server)
	if e != nil {
		return e
	}
	opts := []inetOption{{ID: 1, Value: uintptr(p.Flags)}, {ID: 2, Value: uintptr(unsafe.Pointer(server))}}
	e = inetCall(true, opts)
	runtime.KeepAlive(server)
	if e != nil {
		return e
	}
	setOption.Call(0, 39, 0, 0)
	setOption.Call(0, 37, 0, 0)
	return nil
}
func (w winRoute) locked(f func(string) error) error {
	runtime.LockOSThread()
	defer runtime.UnlockOSThread()
	m, e := windows.CreateMutex(nil, false, windows.StringToUTF16Ptr(`Local\DesiignerskiiVPN.ProxyChange`))
	if m == 0 {
		return e
	}
	defer windows.CloseHandle(m)
	ret, e := windows.WaitForSingleObject(m, 5000)
	if e != nil || (ret != 0 && ret != 0x80) {
		return errors.New("proxy settings are busy")
	}
	defer windows.ReleaseMutex(m)
	return f(filepath.Join(w.dir, "proxy-recovery.json"))
}
func (w winRoute) Apply(addr string) error {
	return w.locked(func(file string) error {
		if _, e := os.Stat(file); !os.IsNotExist(e) {
			return errors.New("previous proxy state still pending")
		}
		prior, e := readProxy()
		if e != nil {
			return e
		}
		data, e := json.Marshal(proxyJournal{w.owner, prior, addr})
		if e != nil {
			return e
		}
		temp := file + ".tmp"
		if e = os.WriteFile(temp, data, 0600); e != nil {
			return e
		}
		if e = os.Rename(temp, file); e != nil {
			return e
		}
		if e = writeProxy(proxyConfig{3, addr}); e != nil {
			return e
		}
		return nil
	})
}
func (w winRoute) recover(anyOwner bool) error {
	return w.locked(func(file string) error {
		data, e := os.ReadFile(file)
		if os.IsNotExist(e) {
			return nil
		}
		if e != nil {
			return e
		}
		var j proxyJournal
		if e = json.Unmarshal(data, &j); e != nil {
			return e
		}
		if !anyOwner && w.owner != j.Owner {
			return nil
		}
		current, e := readProxy()
		if e != nil {
			return e
		}
		if current.Server == j.Address && current.Flags&15 == 3 {
			if e = writeProxy(j.Prior); e != nil {
				return e
			}
		}
		return os.Remove(file)
	})
}
func (w winRoute) Restore() error { return w.recover(false) }
func guard(parent uint32, r winRoute, readyFile string) {
	h, e := windows.OpenProcess(windows.SYNCHRONIZE, false, parent)
	if e != nil {
		return
	}
	defer windows.CloseHandle(h)
	if os.WriteFile(readyFile, []byte(fmt.Sprint(parent)), 0600) != nil {
		return
	}
	windows.WaitForSingleObject(h, windows.INFINITE)
	r.Restore()
}
