#!/bin/sh
# Run QGED's embedded raytrace on a private X server.
set -eu

build_dir=$(cd "$(dirname "$0")/../.." && pwd)
QGED_BIN=${QGED_BIN:-$build_dir/bin/qged}
RT_BIN=${RT_BIN:-$build_dir/bin/rt}
TEST_DB=${TEST_DB:-$build_dir/share/db/boolean-ops.g}
XVFB_BIN=${XVFB_BIN:-Xvfb}
DISPLAY_ATTEMPTS=300
WINDOW_ATTEMPTS=300
RENDER_START_ATTEMPTS=300
RENDER_FINISH_ATTEMPTS=600
EXIT_ATTEMPTS=100

scratch=$(mktemp -d "${TMPDIR:-/tmp}/test_qged_ert.XXXXXX")
qged_pid=''
xvfb_pid=''
cleanup()
{
    if [ "$1" -ne 0 ]; then
        for log in Xvfb.log qged.log; do
            if [ -s "$scratch/$log" ]; then
                echo "$log" >&2
                tail -40 "$scratch/$log" >&2
            fi
        done
    fi
    for pid in "$qged_pid" "$xvfb_pid"; do
        if [ -n "$pid" ]; then
            kill "$pid" 2>/dev/null || true
            wait "$pid" 2>/dev/null || true
        fi
    done
    rm -rf "$scratch"
}
trap 'cleanup "$?"' 0
trap 'exit 1' HUP INT TERM

fail() { printf '%s\n' "$*" >&2; exit 1; }

wait_for()
{
    attempts=$1
    shift
    while ! "$@"; do
        [ "$attempts" -gt 0 ] || return 1
        attempts=$((attempts - 1))
        sleep 0.1
    done
}

running()
{
    # ps reports a defunct child even where procfs is unavailable.
    ps -o stat= -p "$1" | awk 'NF && $1 !~ /^Z/ { found = 1 } END { exit !found }'
}

exited() { ! running "$1"; }

visible_window()
{
    window=$(xdotool search --onlyvisible --pid "$qged_pid" 2>/dev/null | head -n 1)
    [ -n "$window" ]
}

[ -x "$QGED_BIN" ] || fail "QGED executable missing: $QGED_BIN"
[ -x "$RT_BIN" ] || fail "rt executable missing: $RT_BIN"
[ -f "$TEST_DB" ] || fail "Test database missing: $TEST_DB"

# Allocate a display without guessing a display number or using the desktop.
"$XVFB_BIN" -displayfd 3 -screen 0 1280x960x24 -nolisten tcp \
    3>"$scratch/display" >"$scratch/Xvfb.log" 2>&1 &
xvfb_pid=$!
wait_for "$DISPLAY_ATTEMPTS" test -s "$scratch/display" || fail 'Xvfb did not report a display'
DISPLAY=:$(cat "$scratch/display")
XDG_CONFIG_HOME="$scratch/config"
LIBGL_ALWAYS_SOFTWARE=1
export DISPLAY XDG_CONFIG_HOME LIBGL_ALWAYS_SOFTWARE
unset XAUTHORITY
wait_for "$DISPLAY_ATTEMPTS" xdpyinfo -display "$DISPLAY" >/dev/null 2>&1 || fail 'Xvfb did not accept a connection'

# Keep the renderer's environment and PID even if it exits before polling.
BRLCAD_ROOT="$scratch/root"
RT_IPC_ENV_FILE="$scratch/rt-ipc-env"
RT_PID_FILE="$scratch/rt-pid"
mkdir -p "$XDG_CONFIG_HOME" "$BRLCAD_ROOT/bin"
for subdir in lib libexec share; do
    if [ -d "$build_dir/$subdir" ]; then
        ln -s "$build_dir/$subdir" "$BRLCAD_ROOT/$subdir"
    fi
done
cat > "$BRLCAD_ROOT/bin/rt" <<'RT_WRAPPER'
#!/bin/sh
printf '%s\n' "${PKG_ADDR:-}" > "$RT_IPC_ENV_FILE"
printf '%s\n' "$$" > "$RT_PID_FILE"
exec "$RT_BIN" "$@"
RT_WRAPPER
chmod +x "$BRLCAD_ROOT/bin/rt"
export BRLCAD_ROOT RT_BIN RT_IPC_ENV_FILE RT_PID_FILE

"$QGED_BIN" -s "$TEST_DB" >"$scratch/qged.log" 2>&1 &
qged_pid=$!
wait_for "$WINDOW_ATTEMPTS" visible_window || fail 'QGED main window did not appear'
xdotool windowfocus --sync "$window"
xdotool windowraise "$window"

# Click the console near the bottom center of the default window.
height=$(xdotool getwindowgeometry "$window" | awk '/Geometry/{split($2,g,"x"); print g[2]}')
xdotool mousemove --window "$window" 550 "$((height * 79 / 100))" click 1
xdotool type --clearmodifiers 'draw all'
xdotool key Return
sleep 1
xdotool type --clearmodifiers 'ert -P 1 -H 4'
xdotool key Return

wait_for "$RENDER_START_ATTEMPTS" test -s "$RT_PID_FILE" || fail 'ert did not launch rt'
[ -n "$(cat "$RT_IPC_ENV_FILE")" ] || fail 'rt did not receive the framebuffer IPC address'
rt_pid=$(cat "$RT_PID_FILE")
wait_for "$RENDER_FINISH_ATTEMPTS" exited "$rt_pid" || fail 'rt did not finish'
running "$qged_pid" || fail 'QGED terminated during rt'

# Select the last File menu action independently of the menu's row heights.
xdotool mousemove --window "$window" 18 11 click 1
sleep 0.2
xdotool key Up Return
wait_for "$EXIT_ATTEMPTS" exited "$qged_pid" || fail 'QGED did not respond to File > Exit'
wait "$qged_pid" || fail 'QGED exited unsuccessfully'
qged_pid=''
echo 'PASS: QGED launched rt over framebuffer IPC and remained responsive'
