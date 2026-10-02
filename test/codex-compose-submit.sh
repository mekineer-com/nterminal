#!/bin/sh
# Isolated PTY check; needs Xvfb, xdotool, xclip, and Python 3. No live CLI/session.
set -eu
binary=$(realpath "${1:-$(dirname "$0")/../build/qterminal}")
tmp=$(mktemp -d /tmp/nterminal-codex-submit.XXXXXX)
xvfb=''
app=''
cleanup() {
    test -z "$app" || kill "$app" 2>/dev/null || true
    test -z "$xvfb" || kill "$xvfb" 2>/dev/null || true
    rm -rf "$tmp"
}
trap cleanup EXIT HUP INT TERM
cat > "$tmp/record.py" <<'PY'
import os, select, sys, tty
from pathlib import Path
root = Path(sys.argv[1])
tty.setraw(0)
root.joinpath('ready').touch()
data = b''
while True:
    if select.select([0], [], [], 3)[0]:
        data += os.read(0, 65536)
        root.joinpath('received').write_bytes(data)
PY
Xvfb -displayfd 3 -screen 0 1024x768x24 -nolisten tcp 3> "$tmp/display" > "$tmp/xvfb.log" 2>&1 &
xvfb=$!
wait_file() {
    for attempt in $(seq 1 50); do
        if test -e "$1" && { test "$1" = "$tmp/ready" || test -s "$1"; }; then
            return 0
        fi
        sleep 0.1
    done
    cat "$tmp"/*.log
    echo "Timed out waiting for $1" >&2
    exit 1
}
wait_file "$tmp/display"
export DISPLAY=":$(cat "$tmp/display")"
XDG_CONFIG_HOME="$tmp/config" NTERMINAL_COMPOSE=1 "$binary" -e python3 "$tmp/record.py" "$tmp" > "$tmp/app.log" 2>&1 &
app=$!
wait_file "$tmp/ready"
sleep 1 # Let the compose widget finish its initial layout and focus setup.
window=$(xdotool search --pid "$app" | head -1)
xdotool windowfocus "$window" key ctrl+shift+Down
awk 'BEGIN {for(i=0;i<4000;i++) printf "long message line %d\n",i; printf "final text"}' > "$tmp/payload"
xclip -selection clipboard -i "$tmp/payload"
xdotool key ctrl+v
sleep 0.5
xdotool key ctrl+Return
sleep 0.5
python3 - "$tmp" <<'PY'
import sys
from pathlib import Path
root = Path(sys.argv[1])
expected = root.joinpath('payload').read_bytes()
actual = root.joinpath('received').read_bytes()
start = actual.find(b'\x1b[200~')
assert start >= 0, 'Missing explicit paste marker'
assert actual[start:] == b'\x1b[200~' + expected + b'\x1b[201~\r', repr(actual[-80:])
print(f'PASS: {len(expected)} bytes, 4000 newlines retained, exactly one submit')
PY
