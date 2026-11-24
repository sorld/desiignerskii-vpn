#!/usr/bin/env bash
# Runs as an ordinary desktop user with its own D-Bus session and X server.
set -euo pipefail
desktop=/usr/share/applications/Throne.desktop
desktop-file-validate "$desktop"
grep -qx 'Name=Desiignerskii VPN' "$desktop"
test -r /opt/Throne/Throne.png
/opt/Throne/Throne --headless --version |& grep -F '0.6.5'
python3 /src/script/ci/native_subscriptions.py /opt/Throne/Throne /opt/Throne/thronectl
gio launch "$desktop"
main=$(timeout 25 xdotool search --sync --onlyvisible --name '^Desiignerskii VPN$' | head -1)
xdotool getwindowgeometry --shell "$main" > "$HOME/window"
source "$HOME/window"
[[ $WIDTH -ge 480 && $WIDTH -le 530 && $HEIGHT -ge 500 && $HEIGHT -le 560 ]]
test -s "$HOME/.config/Throne/config/desiignerskii-ui.ini"
# The button immediately before Advanced View is the settings control in the compact header.
for offset in 190 180 200 165; do
    xdotool mousemove --window "$main" "$((WIDTH-offset))" 28 click 1
    if timeout 2 xdotool search --sync --onlyvisible --name 'Настройки.*Desiignerskii VPN' >/dev/null; then
        echo "PASS: installed desktop launcher, compact ${WIDTH}x${HEIGHT} home and native preferences"
        exit 0
    fi
done
xdotool search --onlyvisible --name '.*' getwindowname %@ || true
echo 'Native preferences did not open' >&2
exit 1
