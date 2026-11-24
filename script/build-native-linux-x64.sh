#!/usr/bin/env bash
# Build the branded desktop from this checkout, using the matching upstream core and Qt runtime.
set -euo pipefail
root=$(cd "$(dirname "$0")/.." && pwd)
version=0.6.5
work=${LINUX_BUILD_DIR:-"$root/build-native-linux"}
mkdir -p "$work/downloads" "$work/cmake" "$work/package"
work=$(cd "$work" && pwd)
fetch() {
    local url=$1 file=$2 sha=$3
    if ! (echo "$sha  $file" | sha256sum --check --status) 2>/dev/null; then
        curl --fail --location --retry 3 "$url" -o "$file.part"
        echo "$sha  $file.part" | sha256sum --check --status
        mv "$file.part" "$file"
    fi
}
upstream="$work/downloads/Throne-1.4.2-linux-amd64.zip"
sdk="$work/downloads/Qt_6.12.0_linux_x86_64.tar.xz"
fetch 'https://github.com/throneproj/Throne/releases/download/1.4.2/Throne-1.4.2-linux-amd64.zip' "$upstream" e7a29891481c6f59acc3580957a34a5fd1c687cde0bc2b1ce0641176b956a7e9
fetch 'https://github.com/throneproj/buildqt/releases/download/Qt_6.12.0/Qt_6.12.0_linux_x86_64.tar.xz' "$sdk" d454055c631adf2e050de16ad7c06303d729eb9856044548f42731b0aa9ee196
tar -xJf "$sdk" -C "$work"
curl -fsSL --retry 3 https://raw.githubusercontent.com/throneproj/routeprofiles/2faa1c8cd18fa1e3d4b931ab9864c08fe629db25/srslist.h -o "$work/cmake/srslist.h"
dashboard="$work/downloads/dashboard"
if [[ ! -d "$dashboard/.git" ]]; then git init -q "$dashboard"; fi
git -C "$dashboard" fetch --depth=1 https://github.com/SagerNet/sing-box-dashboard.git d77c2f016a7681aa1704e9f83c69139edb230c97
git -C "$dashboard" archive FETCH_HEAD | tar -x -C "$root/res/dashboard"
cmake -S "$root" -B "$work/cmake" -G Ninja -DCMAKE_BUILD_TYPE=Release \
    -DINPUT_VERSION="$version" -DCMAKE_PREFIX_PATH="$work/Qt/lib/cmake"
cmake --build "$work/cmake" --target Throne --parallel "${BUILD_JOBS:-3}"
stage="$work/package/linux-amd64"
mkdir -p "$stage"
unzip -oq "$upstream" 'Throne/usr/*' 'Throne/ThroneCore' 'Throne/thronectl' 'Throne/updater' -d "$work/downloads/payload"
cp -a "$work/downloads/payload/Throne/." "$stage/"
install -m755 "$work/cmake/Throne" "$stage/Throne"
strip --strip-unneeded "$stage/Throne"
patchelf --set-rpath '$ORIGIN/usr/lib' "$stage/Throne"
chmod 755 "$stage/ThroneCore" "$stage/thronectl" "$stage/updater"
install -m644 "$root/res/public/Throne.png" "$root/LICENSE" "$stage"
printf '[Paths]\nPlugins=usr/plugins\n' > "$stage/qt.conf"
cd "$work/package"
bash "$root/script/pack_debian.sh" "$version" amd64 desiignerskii
out="$root/deployment"
mkdir -p "$out"
mv "Throne-$version-debian-amd64.deb" "$out/Desiignerskii-VPN-$version-linux-amd64.deb"
# The in-app update checker expects one top-level Throne directory in a Linux zip.
ln -sfn linux-amd64 Throne
archive="$out/Desiignerskii-VPN-$version-linux-amd64.zip"
rm -f "$archive"
zip -qr "$archive" Throne/
