package main

import (
	"context"
	"net/http"
	"net/url"
	"time"
)

func checkExit(ctx context.Context, addr string) error {
	tr := &http.Transport{DisableKeepAlives: true}
	if addr != "" {
		tr.Proxy = http.ProxyURL(&url.URL{Scheme: "http", Host: addr})
	} else {
		tr.DialContext = tunnelDialer().DialContext
	}
	defer tr.CloseIdleConnections()
	client := &http.Client{Transport: tr, Timeout: 12 * time.Second, CheckRedirect: func(*http.Request, []*http.Request) error { return http.ErrUseLastResponse }}
	req, e := http.NewRequestWithContext(ctx, "HEAD", "https://desiignerskii.pro/", nil)
	if e != nil {
		return e
	}
	response, e := client.Do(req)
	if e != nil {
		return e
	}
	response.Body.Close()
	return nil
}
