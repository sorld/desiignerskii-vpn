package main

import (
	_ "embed"
	"encoding/base64"
	"strings"
)

//go:embed ui/index.html
var interfaceHTML string

//go:embed ui/inter.woff2
var interFont []byte

//go:embed ui/Inter-LICENSE.txt
var fontLicense string

//go:embed brand-icon.png
var logo []byte

func renderInterface(bridgeKey string) string {
	return strings.NewReplacer(
		"__LOGO__", base64.StdEncoding.EncodeToString(logo),
		"__INTER__", base64.StdEncoding.EncodeToString(interFont),
		"__FONT_LICENSE__", fontLicense,
		"__BRIDGE_KEY__", bridgeKey,
	).Replace(interfaceHTML)
}
