#!/bin/bash
set -e

VERSION="$1"
ARCH="$2"
SUFFIX=""
[[ $3 == "systemqt" ]] && SUFFIX="-system-qt"
DIST="$(cd "$(dirname "$0")/../dist/linux" && pwd)"

if [[ $3 == "systemqt" ]]; then
    DEPENDS="desktop-file-utils, libqt6core6, libqt6gui6, libqt6network6, libqt6widgets6, qt6-qpa-plugins, qt6-wayland, qt6-gtk-platformtheme, qt6-xdgdesktopportal-platformtheme, libxcb-cursor0, fonts-noto-color-emoji"
else
    # usr/lib bundles only Qt and xcb-cursor/xcb-util, and the TLS plugin links OpenSSL statically. Throne links libxcb
    # itself, and the bundled Qt loads the rest from the system even headless, so slim images and Pi OS Lite fail in the
    # loader without them.
    DEPENDS="desktop-file-utils, libegl1, libgl1, libopengl0, libglx0, libfontconfig1, libfreetype6, libxkbcommon0, libx11-6, libx11-xcb1, libxcb1, libdbus-1-3, libglib2.0-0 | libglib2.0-0t64, ca-certificates"
fi

# No dpkg-shlibdeps here, so the glibc floor comes from the highest GLIBC_ symbol version any packed ELF file needs
# (readelf skips the rest): apt then refuses a system whose loader would reject them.
GLIBC_NEED=$( (find "linux-$ARCH$SUFFIX" -type f ! -name Throne.debug -exec readelf -V -W {} + 2>/dev/null || true) \
    | { grep -o 'GLIBC_[0-9][0-9.]*' || true; } | sed 's/^GLIBC_//' | sort -Vu | tail -n1)
if [ -n "$GLIBC_NEED" ]; then
    DEPENDS="libc6 (>= $GLIBC_NEED), $DEPENDS"
fi

# Private staging dir so pack_release.sh can build every package concurrently.
PKG=$(mktemp -d)
trap 'rm -rf "$PKG"' EXIT
chmod 0755 "$PKG"

[ -x "linux-$ARCH$SUFFIX/thronectl" ] || { echo "linux-$ARCH$SUFFIX has no thronectl" >&2; exit 1; }
mkdir -p "$PKG/DEBIAN" "$PKG/opt" "$PKG/usr/bin" "$PKG/usr/lib/systemd/system" "$PKG/etc/throne"
cp -r "linux-$ARCH$SUFFIX" "$PKG/opt/Throne"
rm -f "$PKG/opt/Throne/Throne.debug"
ln -s /opt/Throne/thronectl "$PKG/usr/bin/thronectl"

# headless service and web UI: shipped disabled, never enabled or started by the package
install -m 0644 "$DIST/throne.service" "$PKG/usr/lib/systemd/system/throne.service"
install -m 0644 "$DIST/throne-web.service" "$PKG/usr/lib/systemd/system/throne-web.service"
install -m 0644 "$DIST/throne.env" "$PKG/etc/throne/throne.env"
install -m 0644 "$DIST/throne-web.env" "$PKG/etc/throne/web.env"
printf '%s\n' /etc/throne/throne.env /etc/throne/web.env >"$PKG/DEBIAN/conffiles"

deb_version=$VERSION
maintainer='Mahdi <Mahdi.zrei@gmail.com>'
description='Qt based cross-platform GUI proxy configuration manager (backend: sing-box)'
desktop_name=Throne
desktop_exec='sh -c "PATH=/opt/Throne:$PATH /opt/Throne/Throne -appdata"'
if [[ ${3:-} == desiignerskii ]]; then
    # Preserve upgrades from Throne 1.4.2, its config directory and the installed application path.
    deb_version="1.4.2+ds${VERSION}"
    maintainer='Sorld <sorld@users.noreply.github.com>'
    description='Desiignerskii VPN desktop client based on Throne'
    desktop_name='Desiignerskii VPN'
    desktop_exec='/opt/Throne/Throne -appdata'
    DEPENDS+=", pkexec, libstdc++6, libxkbcommon-x11-0, libxcb-render0, libxcb-icccm4, libxcb-image0, libxcb-keysyms1, libxcb-randr0, libxcb-render-util0, libxcb-shape0, libxcb-shm0, libxcb-sync1, libxcb-xfixes0, libxcb-xkb1, libwayland-client0, libwayland-cursor0"
fi
cat >"$PKG/DEBIAN/control" <<-EOF
Package: throne
Version: $deb_version
Architecture: $ARCH
Maintainer: $maintainer
Depends: $DEPENDS
Description: $description
EOF

mkdir -p "$PKG/usr/share/applications"
cat >"$PKG/usr/share/applications/Throne.desktop" <<-EOF
[Desktop Entry]
Name=$desktop_name
Comment=$description
Exec=$desktop_exec
Icon=/opt/Throne/Throne.png
Terminal=false
Type=Application
Categories=Network;
StartupWMClass=Throne
EOF

# Maintainer scripts never fail the transaction, and touch systemd only when it is running (not in a chroot or container).
cat >"$PKG/DEBIAN/postinst" <<-'EOF'
#!/bin/sh
update-desktop-database >/dev/null 2>&1 || true

# $2 is the previously configured version: set on upgrade, empty on a fresh install.
if [ "$1" = configure ] && [ -d /run/systemd/system ]; then
    systemctl daemon-reload >/dev/null 2>&1 || true
    if [ -n "$2" ]; then
        systemctl try-restart throne.service throne-web.service >/dev/null 2>&1 || true
    fi
fi
exit 0
EOF

# An upgrade keeps the services running on the old files until postinst restarts them.
cat >"$PKG/DEBIAN/prerm" <<-'EOF'
#!/bin/sh
if [ "$1" = remove ]; then
    if [ -d /run/systemd/system ]; then
        systemctl stop throne-web.service throne.service >/dev/null 2>&1 || true
    fi
    if command -v systemctl >/dev/null 2>&1; then
        systemctl --no-reload disable throne-web.service >/dev/null 2>&1 || true
        systemctl --no-reload disable throne.service >/dev/null 2>&1 || true
    fi
fi
exit 0
EOF

cat >"$PKG/DEBIAN/postrm" <<-'EOF'
#!/bin/sh
if [ "$1" = remove ] || [ "$1" = purge ]; then
    command -v update-desktop-database >/dev/null 2>&1 && update-desktop-database >/dev/null 2>&1 || true
fi
if [ -d /run/systemd/system ]; then
    systemctl daemon-reload >/dev/null 2>&1 || true
fi
exit 0
EOF

chmod 0755 "$PKG/DEBIAN/postinst" "$PKG/DEBIAN/prerm" "$PKG/DEBIAN/postrm"

# desktop && PATH

# xz, not Ubuntu's zstd default: dpkg before Debian 12 (Debian 10/11, Raspberry Pi OS bullseye) cannot unpack zstd.
dpkg-deb -Zxz --root-owner-group --build "$PKG" "Throne-$VERSION-debian-$ARCH$SUFFIX.deb"
