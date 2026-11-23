package main

import (
	"crypto/rand"
	"crypto/subtle"
	_ "embed"

	"encoding/hex"
	"errors"
	"fmt"

	"os"
	"os/exec"
	"path/filepath"
	"runtime"
	"strconv"

	"sync"
	"syscall"
	"time"
	"unsafe"

	"github.com/getlantern/systray"
	webview "github.com/jchv/go-webview2"
	"github.com/jchv/go-webview2/webviewloader"
	"golang.org/x/sys/windows"
)

//go:embed icon.ico
var icon []byte

const title = "Desiignerskii VPN"

func msg(text string) {
	windows.MessageBox(0, windows.StringToUTF16Ptr(text), windows.StringToUTF16Ptr(title), 0x40)
}
func token() string {
	var b [24]byte
	if _, e := rand.Read(b[:]); e != nil {
		panic(e)
	}
	return hex.EncodeToString(b[:])
}
func hidden(cmd *exec.Cmd) { cmd.SysProcAttr = &syscall.SysProcAttr{HideWindow: true} }
func userDir() (string, error) {
	d, e := os.UserCacheDir()
	if e != nil {
		return "", e
	}
	p := filepath.Join(d, "Desiignerskii VPN")
	return p, wantDir(p)
}
func ownEngine() (windows.Handle, error) {
	job, e := windows.CreateJobObject(nil, nil)
	if e != nil {
		return 0, e
	}
	limits := windows.JOBOBJECT_EXTENDED_LIMIT_INFORMATION{}
	limits.BasicLimitInformation.LimitFlags = windows.JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE
	_, e = windows.SetInformationJobObject(job, windows.JobObjectExtendedLimitInformation, uintptr(unsafe.Pointer(&limits)), uint32(unsafe.Sizeof(limits)))
	if e != nil {
		windows.CloseHandle(job)
		return 0, e
	}
	return job, nil
}
func attach(job windows.Handle, cmd *exec.Cmd) error {
	p, e := windows.OpenProcess(windows.PROCESS_SET_QUOTA|windows.PROCESS_TERMINATE, false, uint32(cmd.Process.Pid))
	if e != nil {
		return e
	}
	defer windows.CloseHandle(p)
	return windows.AssignProcessToJobObject(job, p)
}

var user32 = windows.NewLazySystemDLL("user32.dll")

func reveal(hwnd uintptr) {
	user32.NewProc("ShowWindow").Call(hwnd, 9)
	user32.NewProc("SetForegroundWindow").Call(hwnd)
}
func main() {
	runtime.LockOSThread()
	defer runtime.UnlockOSThread()
	dir, e := userDir()
	if e != nil {
		msg("Не удалось открыть папку данных приложения.")
		return
	}
	exe, e := os.Executable()
	if e != nil {
		return
	}
	if len(os.Args) == 4 && os.Args[1] == "--proxy-guardian" {
		pid, e := strconv.ParseUint(os.Args[2], 10, 32)
		if e != nil || len(os.Args[3]) != 48 {
			return
		}
		if _, e = hex.DecodeString(os.Args[3]); e != nil {
			return
		}
		id := os.Args[3]
		guard(uint32(pid), winRoute{dir, id}, filepath.Join(dir, id+".ready"))
		return
	}
	singleton, e := windows.CreateMutex(nil, true, windows.StringToUTF16Ptr(`Local\DesiignerskiiVPN.Desktop.01`))
	if singleton == 0 {
		return
	}
	defer windows.CloseHandle(singleton)
	if errors.Is(e, windows.ERROR_ALREADY_EXISTS) {
		hwnd, _, _ := user32.NewProc("FindWindowW").Call(0, uintptr(unsafe.Pointer(windows.StringToUTF16Ptr(title))))
		if hwnd != 0 {
			reveal(hwnd)
		}
		return
	}
	id := token()
	route := winRoute{dir, id}
	if e = route.recover(true); e != nil {
		msg("Не удалось восстановить предыдущие настройки прокси Windows.")
		return
	}
	if v, e := webviewloader.GetInstalledVersion(); e != nil || v == "" {
		msg("Для окна приложения требуется Microsoft Edge WebView2 Runtime.")
		return
	}
	base := filepath.Dir(exe)
	engineExe := filepath.Join(base, "engine", "Throne.exe")
	if _, e = os.Stat(engineExe); e != nil {
		msg("Распакуй весь ZIP в одну папку и запусти приложение из неё.")
		return
	}
	ready := filepath.Join(dir, id+".ready")
	watch := exec.Command(exe, "--proxy-guardian", fmt.Sprint(os.Getpid()), id)
	hidden(watch)
	if e = watch.Start(); e != nil {
		msg("Не удалось запустить восстановление настроек сети.")
		return
	}
	go watch.Wait()
	for i := 0; i < 120; i++ {
		if _, e = os.Stat(ready); e == nil {
			break
		}
		time.Sleep(50 * time.Millisecond)
	}
	if _, e = os.Stat(ready); e != nil {
		msg("Не удалось подготовить восстановление настроек сети.")
		return
	}
	os.Remove(ready)
	w := webview.NewWithOptions(webview.WebViewOptions{DataPath: filepath.Join(dir, "window"), AutoFocus: true, WindowOptions: webview.WindowOptions{Title: title, Width: 890, Height: 660, Center: true, IconId: 1}})
	if w == nil {
		msg("Не удалось открыть окно приложения.")
		return
	}
	defer w.Destroy()
	w.SetSize(360, 420, webview.HintMin)
	hwnd := uintptr(w.Window())
	c := newController(route)
	prefs := readPreferences(dir)
	c.prefDir = dir
	c.view.Mode = prefs.Mode
	c.view.Favorites = prefs.Favorites
	key := token()
	allowed := func(k string) bool { return subtle.ConstantTimeCompare([]byte(k), []byte(key)) == 1 }
	w.Bind("dsState", func(k string) viewState {
		if !allowed(k) {
			return viewState{}
		}
		return c.snapshot()
	})
	w.Bind("dsAction", func(k, op, arg string, id int) {
		if allowed(k) {
			c.action(op, arg, id)
		}
	})
	w.Bind("dsOpen", func(k, dest string) {
		if !allowed(k) {
			return
		}
		links := map[string]string{"account": "https://desiignerskii.pro/account/connect", "help": "https://desiignerskii.pro/help"}
		if u := links[dest]; u != "" {
			windows.ShellExecute(0, nil, windows.StringToUTF16Ptr(u), nil, nil, windows.SW_SHOWNORMAL)
		}
	})
	w.SetHtml(renderInterface(key))
	var bg sync.WaitGroup
	bg.Add(1)
	go func() { defer bg.Done(); c.serveDesktop(engineExe, dir) }()
	go func() {
		runtime.LockOSThread()
		defer runtime.UnlockOSThread()
		systray.Run(func() {
			systray.SetIcon(icon)
			systray.SetTooltip(title)
			show := systray.AddMenuItem("Открыть клиент", "")
			on := systray.AddMenuItem("Подключить", "")
			off := systray.AddMenuItem("Отключить", "")
			systray.AddSeparator()
			quit := systray.AddMenuItem("Выйти", "")
			go func() {
				for {
					select {
					case <-show.ClickedCh:
						w.Dispatch(func() { reveal(hwnd) })
					case <-on.ClickedCh:
						if c.snapshot().Selected < 0 {
							w.Dispatch(func() { reveal(hwnd) })
						} else {
							c.action("connect", "", -1)
						}
					case <-off.ClickedCh:
						c.action("disconnect", "", -1)
					case <-quit.ClickedCh:
						w.Dispatch(w.Terminate)
						return
					case <-c.ctx.Done():
						return
					}
				}
			}()
		}, nil)
	}()
	w.Run()
	c.stop()
	systray.Quit()
	bg.Wait()
}
