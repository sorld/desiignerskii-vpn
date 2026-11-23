//go:build !windows

package main

import (
	"net"
	"time"
)

func tunnelDialer() *net.Dialer { return &net.Dialer{Timeout: 8 * time.Second} }
