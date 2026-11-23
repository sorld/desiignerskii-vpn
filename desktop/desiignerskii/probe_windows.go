package main

import (
	"context"
	"errors"
	"golang.org/x/sys/windows"
	"net"
	"net/netip"
	"strings"
	"syscall"
	"time"
)

// Validate the actual route of each address selected by the HTTPS dialer, including IPv6 fallback.
func tunnelDialer() *net.Dialer {
	return &net.Dialer{Timeout: 8 * time.Second, ControlContext: func(ctx context.Context, network, address string, raw syscall.RawConn) error {
		endpoint, e := netip.ParseAddrPort(address)
		if e != nil {
			return e
		}
		ip := endpoint.Addr().Unmap()
		var sa windows.Sockaddr
		if ip.Is4() {
			sa = &windows.SockaddrInet4{Addr: ip.As4()}
		} else {
			sa = &windows.SockaddrInet6{Addr: ip.As16()}
		}
		var index uint32
		if e = windows.GetBestInterfaceEx(sa, &index); e != nil {
			return e
		}
		device, e := net.InterfaceByIndex(int(index))
		if e != nil {
			return e
		}
		name := strings.ToLower(device.Name)
		if device.Flags&net.FlagUp == 0 || (name != "throne-tun" && !strings.HasPrefix(name, "throne-tun ")) {
			return errors.New("destination is outside Throne TUN")
		}
		return nil
	}}
}
