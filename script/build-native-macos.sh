#!/bin/bash
# Build a single disk image for Intel and Apple Silicon.
set -euo pipefail
root=$(cd "$(dirname "$0")/.." && pwd)
version=0.6.5
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
build="$work/build"
disk="$work/disk"
app="$disk/Desiignerskii VPN.app"
mkdir -p "$build" "$disk" "$root/deployment"
curl -fsSL --retry 3 https://raw.githubusercontent.com/throneproj/routeprofiles/2faa1c8cd18fa1e3d4b931ab9864c08fe629db25/srslist.h -o "$build/srslist.h"
git init -q "$work/dashboard"
git -C "$work/dashboard" fetch -q --depth=1 https://github.com/SagerNet/sing-box-dashboard.git d77c2f016a7681aa1704e9f83c69139edb230c97
git -C "$work/dashboard" archive FETCH_HEAD | tar -xf - -C "$root/res/dashboard"
cmake -S "$root" -B "$build" -G Ninja -DCMAKE_BUILD_TYPE=Release -DINPUT_VERSION="$version" -DNKR_PACKAGE_MACOS=1 \
    -DCMAKE_OSX_DEPLOYMENT_TARGET=14.4 '-DCMAKE_OSX_ARCHITECTURES=arm64;x86_64'
cmake --build "$build" --target Throne --parallel 3
mv "$build/Throne.app" "$app"
bin="$app/Contents/MacOS"
for arch in amd64 arm64; do
    case "$arch" in
        amd64) sha=b7ef9f8a2a0c8b8f798ae03bb76479ce2f24f835e0ee440a6beaa3e9ed2df938 ;;
        arm64) sha=c56b99af9153e09ad152b42cfd2975027a097a364ddc65f66f8ea6c4476f4c0d ;;
    esac
    curl -fsSL --retry 3 "https://github.com/throneproj/Throne/releases/download/1.4.2/Throne-1.4.2-macos-$arch.zip" -o "$work/$arch.zip"
    echo "$sha  $work/$arch.zip" | shasum -a 256 --check --status
    unzip -p "$work/$arch.zip" Throne/Throne.app/Contents/MacOS/ThroneCore > "$work/core-$arch"
done
lipo -create "$work/core-amd64" "$work/core-arm64" -output "$bin/ThroneCore"
chmod 755 "$bin/ThroneCore"
macdeployqt "$app" -always-overwrite -verbose=1
plist="$app/Contents/Info.plist"
plutil -replace CFBundleIdentifier -string pro.desiignerskii.vpn "$plist"
plutil -replace CFBundleURLTypes.0.CFBundleURLName -string pro.desiignerskii.vpn "$plist"
for key in CFBundleName CFBundleDisplayName; do
    /usr/libexec/PlistBuddy -c "Set :$key Desiignerskii VPN" "$plist" 2>/dev/null ||
        /usr/libexec/PlistBuddy -c "Add :$key string Desiignerskii VPN" "$plist"
done
for key in CFBundleVersion CFBundleShortVersionString; do plutil -replace "$key" -string "$version" "$plist"; done
iconset="$work/Throne.iconset"
mkdir "$iconset"
for n in 16 32 128 256 512; do
    sips -z "$n" "$n" "$root/res/public/Throne.png" --out "$iconset/icon_${n}x$n.png" >/dev/null
    sips -z "$((n*2))" "$((n*2))" "$root/res/public/Throne.png" --out "$iconset/icon_${n}x$n@2x.png" >/dev/null
done
iconutil -c icns "$iconset" -o "$app/Contents/Resources/Throne.icns"
codesign --force --sign - "$bin/ThroneCore"
codesign --force --deep --sign - "$app"
codesign --verify --deep --strict "$app"
while IFS= read -r -d '' binary; do
    if file -b "$binary" | grep 'Mach-O' >/dev/null; then lipo -verify_arch arm64 x86_64 "$binary"; fi
done < <(find "$app" -type f -print0)
ln -s /Applications "$disk/Applications"
hdiutil create -quiet -fs HFS+ -format UDZO -volname 'Desiignerskii VPN' -srcfolder "$disk" "$root/deployment/Desiignerskii-VPN-$version-macOS.dmg"
