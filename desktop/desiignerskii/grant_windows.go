package main

import (
	"context"
	"crypto/sha256"
	"errors"
	"fmt"
	"golang.org/x/sys/windows"
	"io"
	"os"
	"path/filepath"
	"runtime"
	"unsafe"
)

// SHELLEXECUTEINFOW is used to wait for the result of Throne's own permission dialog.
type shellExecuteInfo struct {
	Size                              uint32
	Mask                              uint32
	Window                            uintptr
	Verb, File, Parameters, Directory *uint16
	Show                              int32
	Instance, IDList                  uintptr
	Class                             *uint16
	ClassKey                          uintptr
	HotKey                            uint32
	Icon                              uintptr
	Process                           windows.Handle
}

func corePermission(ctx context.Context, exe string) error {
	known := map[string]string{"Throne.exe": "7c8ee3c4d706f1e660230e7527d9ac15535adf522d5bf26c0601eb3b16d60c8a", "ThroneCore.exe": "c892a1eacb73df9576bfb5639cfe771868343d77b9e7cd31c4b11f6b1dd15482", "libcronet.dll": "8b005a933ac0d0899c047617a9cd18418810d9426d0919c824520094a2c893f5"}
	for file, want := range known {
		f, e := os.Open(filepath.Join(filepath.Dir(exe), file))
		if e != nil {
			return errors.New("Не хватает файлов Throne. Распакуй ZIP целиком.")
		}
		h := sha256.New()
		_, e = io.Copy(h, f)
		f.Close()
		if e != nil || fmt.Sprintf("%x", h.Sum(nil)) != want {
			return errors.New("Файлы движка отличаются от официальной сборки. Распакуй архив заново.")
		}
	}
	user, e := windows.GetCurrentProcessToken().GetTokenUser()
	if e != nil {
		return errors.New("Не удалось определить учётную запись Windows.")
	}
	runtime.LockOSThread()
	defer runtime.UnlockOSThread()
	if e = windows.CoInitializeEx(0, windows.COINIT_APARTMENTTHREADED); e == nil {
		defer windows.CoUninitialize()
	}
	op := shellExecuteInfo{Mask: 0x40 | 0x100, Verb: windows.StringToUTF16Ptr("runas"), File: windows.StringToUTF16Ptr(exe), Directory: windows.StringToUTF16Ptr(filepath.Dir(exe)), Parameters: windows.StringToUTF16Ptr("--elevated-helper grant " + user.User.Sid.String()), Show: windows.SW_HIDE}
	op.Size = uint32(unsafe.Sizeof(op))
	if e = ctx.Err(); e != nil {
		return e
	}
	ok, _, last := windows.NewLazySystemDLL("shell32.dll").NewProc("ShellExecuteExW").Call(uintptr(unsafe.Pointer(&op)))
	runtime.KeepAlive(&op)
	if ok == 0 || op.Process == 0 {
		if errors.Is(last, windows.ERROR_CANCELLED) {
			return errors.New("Запрос Windows отменён. TUN выключен.")
		}
		return errors.New("Windows не разрешила настроить адаптер TUN.")
	}
	defer windows.CloseHandle(op.Process)
	for {
		status, e := windows.WaitForSingleObject(op.Process, 250)
		if e != nil {
			return errors.New("Не удалось проверить разрешение Windows.")
		}
		if status == windows.WAIT_OBJECT_0 {
			break
		}
		if e := ctx.Err(); e != nil {
			return e
		}
	}
	var code uint32
	if windows.GetExitCodeProcess(op.Process, &code) != nil || code != 0 {
		if code == 3 {
			return errors.New("Throne требует подтверждения администратора из той же учётной записи Windows.")
		}
		return errors.New("Throne не получил разрешение для TUN. Подключение осталось выключенным.")
	}
	return nil
}
