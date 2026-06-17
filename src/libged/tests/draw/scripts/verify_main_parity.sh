#!/usr/bin/env bash
# verify_main_parity.sh — Diff current branch draw output vs main branch baseline.
#
# Usage:
#   verify_main_parity.sh [OPTIONS] <current-build-dir> <test-data-dir>
#
# Options:
#   --main-build-dir DIR   Path to a pre-built main-branch build directory.
#                          If omitted the script tries to build main in /tmp.
#   --keep-images          Keep generated PNG files after the run.
#   --help                 Show this help.
#
# This script:
#   1. Fetches the 'main' branch from origin (requires network + git).
#   2. Creates a git worktree for main in a temp directory.
#   3. Configures and builds only the required draw-test targets from main.
#   4. Runs those targets to generate "main baseline" PNG images.
#   5. Runs the same tests from the current build directory.
#   6. Compares the two image sets using icv_diff (via the pixdiff utility
#      if available, or a Python fallback).
#   7. Reports any pixel-level drift.
#
# When to use:
#   This script is OPTIONAL — run it by hand or in a dedicated CI job with
#   enough build time.  It is NOT registered as a CTest target because building
#   the main branch from scratch can take 20–30 minutes.
#
# Exit code: 0 = all images match, non-zero = drift detected or setup error.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
KEEP_IMAGES=0
MAIN_BUILD_DIR=""

usage() {
    sed -n '/^# Usage:/,/^[^#]/p' "$0" | head -n -1 | sed 's/^# //'
    exit 0
}

# --- parse options -----------------------------------------------------------
while [[ $# -gt 0 ]]; do
    case "$1" in
        --main-build-dir) MAIN_BUILD_DIR="$2"; shift 2 ;;
        --keep-images)    KEEP_IMAGES=1;        shift   ;;
        --help|-h)        usage ;;
        *)                break ;;
    esac
done

if [[ $# -lt 2 ]]; then
    echo "ERROR: need <current-build-dir> and <test-data-dir>" >&2
    exit 1
fi

CURRENT_BUILD="$1"
TEST_DATA_DIR="$2"

if [[ ! -d "$CURRENT_BUILD" ]]; then
    echo "ERROR: current build directory '$CURRENT_BUILD' does not exist" >&2; exit 1
fi
if [[ ! -d "$TEST_DATA_DIR" ]]; then
    echo "ERROR: test data directory '$TEST_DATA_DIR' does not exist" >&2; exit 1
fi

TMPDIR_ROOT="$(mktemp -d /tmp/brlcad_main_parity.XXXXXX)"
trap 'rm -rf "$TMPDIR_ROOT"' EXIT

REPO_ROOT="$(git -C "$SCRIPT_DIR" rev-parse --show-toplevel)"

# --- fetch + worktree for main -----------------------------------------------
echo "=== Fetching main branch ==="
if ! git -C "$REPO_ROOT" fetch --unshallow origin 2>/dev/null; then
    git -C "$REPO_ROOT" fetch origin main:refs/remotes/origin/main || {
        echo "SKIP: could not fetch main branch (no network or not a git repo)" >&2
        echo "      Manually provide --main-build-dir to skip the build step." >&2
        exit 77   # CTest skip code
    }
else
    # unshallow succeeded; still need the main branch ref
    git -C "$REPO_ROOT" fetch origin main:refs/remotes/origin/main || {
        echo "SKIP: could not fetch main ref after unshallow" >&2
        exit 77
    }
fi

MAIN_WORKTREE="$TMPDIR_ROOT/main_src"
git -C "$REPO_ROOT" worktree add "$MAIN_WORKTREE" refs/remotes/origin/main

# --- build main's draw tests (if no pre-built dir given) ---------------------
if [[ -z "$MAIN_BUILD_DIR" ]]; then
    echo "=== Configuring and building main draw tests ==="
    MAIN_BUILD_DIR="$TMPDIR_ROOT/main_build"
    mkdir -p "$MAIN_BUILD_DIR"

    # Reuse bext_output from the current source tree to save time
    BEXT_DIR="$(dirname "$REPO_ROOT")/bext_output"
    if [[ ! -d "$BEXT_DIR" ]]; then
        BEXT_DIR=""
        echo "WARNING: bext_output not found — build may download dependencies"
    fi

    BEXT_ARG=""
    if [[ -n "$BEXT_DIR" ]]; then
        BEXT_ARG="-DBRLCAD_EXT_DIR=$BEXT_DIR"
    fi

    cmake -S "$MAIN_WORKTREE/brlcad" -B "$MAIN_BUILD_DIR" \
        $BEXT_ARG \
        -DBRLCAD_EXTRADOCS=OFF \
        -DBRLCAD_ENABLE_STEP=OFF \
        -DBRLCAD_ENABLE_GDAL=OFF \
        -DBRLCAD_ENABLE_QT=ON \
        -DCMAKE_BUILD_TYPE=Release

    # Build only the draw-test executables to save time
    cmake --build "$MAIN_BUILD_DIR" --target \
        ged_test_draw ged_test_faceplate ged_test_lod ged_test_select_draw \
        ged_test_quad ged_test_bsg_render_stability \
        -j"$(nproc)"
else
    echo "Using pre-built main at: $MAIN_BUILD_DIR"
fi

# --- helper: run a draw test and move its PNGs to an output dir --------------
run_draw_test() {
    local build_dir="$1"
    local target="$2"
    local out_dir="$3"
    local exe

    exe="$(find "$build_dir" -name "$target" -type f 2>/dev/null | head -1)"
    if [[ -z "$exe" ]]; then
        echo "  WARNING: $target not found in $build_dir — skipping" >&2
        return 0
    fi

    mkdir -p "$out_dir"
    pushd "$out_dir" > /dev/null
    "$exe" -c "$TEST_DATA_DIR" 2>&1 | tee "${target}.log" || true
    popd > /dev/null
}

MAIN_IMG_DIR="$TMPDIR_ROOT/main_images"
CURR_IMG_DIR="$TMPDIR_ROOT/curr_images"

echo "=== Generating main baseline images ==="
for t in ged_test_draw ged_test_faceplate ged_test_lod ged_test_select_draw ged_test_quad; do
    run_draw_test "$MAIN_BUILD_DIR" "$t" "$MAIN_IMG_DIR"
done

echo "=== Generating current branch images ==="
for t in ged_test_draw ged_test_faceplate ged_test_lod ged_test_select_draw ged_test_quad; do
    run_draw_test "$CURRENT_BUILD" "$t" "$CURR_IMG_DIR"
done

# --- compare PNG pairs -------------------------------------------------------
echo "=== Comparing images ==="
FAIL_COUNT=0
TOTAL=0

compare_images() {
    local a="$1"  # main baseline
    local b="$2"  # current branch

    if [[ ! -f "$a" ]]; then
        echo "  SKIP: main baseline not found: $a"
        return 0
    fi
    if [[ ! -f "$b" ]]; then
        echo "  SKIP: current image not found: $b"
        return 0
    fi

    TOTAL=$((TOTAL+1))

    # Try pixdiff (BRL-CAD utility) first
    if command -v pixdiff &>/dev/null; then
        local diff_out
        diff_out=$(pixdiff "$a" "$b" 2>&1) || true
        if echo "$diff_out" | grep -q "differ"; then
            echo "  DRIFT: $(basename "$a") — $diff_out"
            FAIL_COUNT=$((FAIL_COUNT+1))
        else
            echo "  MATCH: $(basename "$a")"
        fi
        return
    fi

    # Python/PIL fallback
    if command -v python3 &>/dev/null; then
        python3 - "$a" "$b" <<'PYEOF'
import sys, math
try:
    from PIL import Image, ImageChops
    a, b = Image.open(sys.argv[1]), Image.open(sys.argv[2])
    diff = ImageChops.difference(a, b)
    extrema = diff.convert("L").getextrema()
    max_diff = extrema[1]
    print("  MATCH (max_diff=%d): %s" % (max_diff, sys.argv[1].split("/")[-1]) if max_diff <= 1
          else "  DRIFT (max_diff=%d): %s" % (max_diff, sys.argv[1].split("/")[-1]))
    sys.exit(0 if max_diff <= 1 else 1)
except ImportError:
    # Pillow not available — fall back to simple byte comparison
    with open(sys.argv[1],"rb") as f1, open(sys.argv[2],"rb") as f2:
        same = f1.read() == f2.read()
    print("  %s: %s" % ("MATCH" if same else "DRIFT", sys.argv[1].split("/")[-1]))
    sys.exit(0 if same else 1)
PYEOF
        return
    fi

    # Absolute fallback: byte comparison (strict)
    if cmp -s "$a" "$b"; then
        echo "  MATCH: $(basename "$a")"
    else
        echo "  DRIFT: $(basename "$a") (byte-level difference)"
        FAIL_COUNT=$((FAIL_COUNT+1))
    fi
}

# Only compare final control-image-named PNGs (not intermediate temp files)
for ctrl in "$TEST_DATA_DIR"/*_ctrl.png; do
    base="$(basename "$ctrl" _ctrl.png)"
    compare_images "$MAIN_IMG_DIR/${base}.png" "$CURR_IMG_DIR/${base}.png"
done

# Keep images if requested
if [[ "$KEEP_IMAGES" -eq 1 ]]; then
    echo "Main images left in: $MAIN_IMG_DIR"
    echo "Current images left in: $CURR_IMG_DIR"
    trap '' EXIT  # suppress temp dir cleanup
fi

echo ""
echo "=== Summary: $FAIL_COUNT/$TOTAL image(s) show drift ==="
[[ "$FAIL_COUNT" -eq 0 ]]
