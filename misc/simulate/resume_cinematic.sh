#!/bin/bash
# resume_cinematic.sh - Resume (or start from scratch) the cinematic simulation render.
#
# This script is designed to be run multiple times.  It skips any step whose
# output already exists so a partial run can be completed in follow-on sessions.
#
# Usage:
#   ./resume_cinematic.sh [brlcad_build_dir] [output_dir]
#
# Defaults:
#   brlcad_build_dir = /home/runner/work/brlcad_quickiterate/brlcad_quickiterate/brlcad_build
#   output_dir       = /tmp/cinematic_output
#
# Prerequisites (Ubuntu):
#   sudo apt-get install -y libbullet-dev ffmpeg
#
# BRL-CAD must be built with Bullet support:
#   cmake /path/to/brlcad \
#     -DBRLCAD_ENABLE_BULLET=ON -DBRLCAD_BUNDLED_LIBS=SYSTEM \
#     -DBULLET_DYNAMICS_LIBRARY=/usr/lib/x86_64-linux-gnu/libBulletDynamics.so \
#     -DBULLET_COLLISION_LIBRARY=/usr/lib/x86_64-linux-gnu/libBulletCollision.so \
#     -DBULLET_MATH_LIBRARY=/usr/lib/x86_64-linux-gnu/libLinearMath.so \
#     -DBULLET_SOFTBODY_LIBRARY=/usr/lib/x86_64-linux-gnu/libBulletSoftBody.so
#   cmake --build . -j$(nproc)
#
# Output:
#   $OUTPUT_DIR/cinematic_truck_sim.g          — simulation database
#   $OUTPUT_DIR/frames/frame_0001.g …          — per-step .g snapshots
#   $OUTPUT_DIR/rendered/frame_0001.png …      — rendered frames
#   $OUTPUT_DIR/truck_terrain_cinematic.mp4    — final 30-second video

set -e

BRLCAD_BUILD=${1:-/home/runner/work/brlcad_quickiterate/brlcad_quickiterate/brlcad_build}
OUTPUT_DIR=${2:-/tmp/cinematic_output}
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

export PATH="$BRLCAD_BUILD/bin:$PATH"
# Bullet .so lives in the system lib dir (single-precision, BUNDLED_LIBS=SYSTEM build)
export LD_LIBRARY_PATH="$BRLCAD_BUILD/lib:/usr/lib/x86_64-linux-gnu:${LD_LIBRARY_PATH:-}"
export BRLCAD_ROOT="$BRLCAD_BUILD"

DB_DIR="$BRLCAD_BUILD/share/db"
SIM_DB="$OUTPUT_DIR/cinematic_truck_sim.g"
FRAMES_DIR="$OUTPUT_DIR/frames"
RENDER_DIR="$OUTPUT_DIR/rendered"
VIDEO_OUT="$OUTPUT_DIR/truck_terrain_cinematic.mp4"

NSTEPS=300
STEP_DUR=0.1
WIDTH=1280
HEIGHT=720
FPS=10

mkdir -p "$FRAMES_DIR" "$RENDER_DIR"

# ----------------------------------------------------------------
# Verify required binaries
# ----------------------------------------------------------------
for BIN in mged rt pix-png; do
    if [ ! -x "$BRLCAD_BUILD/bin/$BIN" ]; then
        echo "ERROR: $BRLCAD_BUILD/bin/$BIN not found."
        echo "Build BRL-CAD first (see script header for cmake flags)."
        exit 1
    fi
done
if [ ! -f "$BRLCAD_BUILD/libexec/ged/libged-simulate.so" ]; then
    echo "ERROR: ged-simulate plugin not found at $BRLCAD_BUILD/libexec/ged/libged-simulate.so"
    echo "Rebuild with -DBRLCAD_ENABLE_BULLET=ON -DBRLCAD_BUNDLED_LIBS=SYSTEM"
    exit 1
fi

echo "========================================================"
echo " Cinematic M35 Simulation Resume Script"
echo " Build:  $BRLCAD_BUILD"
echo " Output: $OUTPUT_DIR"
echo " Steps:  $NSTEPS × ${STEP_DUR}s = $(python3 -c "print($NSTEPS*$STEP_DUR)")s physics"
echo " Video:  ${WIDTH}×${HEIGHT} @ ${FPS}fps → 30s"
echo "========================================================"

# ----------------------------------------------------------------
# Step 1: Build simulation database (skip if exists and non-empty)
# ----------------------------------------------------------------
if [ ! -s "$SIM_DB" ]; then
    echo ""
    echo "=== Step 1: Building cinematic simulation database ==="
    cd "$DB_DIR"
    rm -f "$SIM_DB"
    mged -c "$SIM_DB" < "$SCRIPT_DIR/cinematic_setup.tcl" 2>/dev/null
    echo "  Created: $SIM_DB"
else
    echo ""
    echo "=== Step 1: Database exists, skipping ($SIM_DB) ==="
fi

# ----------------------------------------------------------------
# Step 2: Run simulation steps (skip frames that already exist)
# ----------------------------------------------------------------
EXISTING_FRAMES=$(ls "$FRAMES_DIR"/frame_*.g 2>/dev/null | wc -l)
echo ""
echo "=== Step 2: Simulation ($NSTEPS × ${STEP_DUR}s) — $EXISTING_FRAMES/$NSTEPS frames exist ==="

if [ "$EXISTING_FRAMES" -lt "$NSTEPS" ]; then
    cd "$DB_DIR"
    RESUMED=0
    for i in $(seq 1 $NSTEPS); do
        STEPNUM=$(printf "%04d" $i)
        FRAME_G="$FRAMES_DIR/frame_${STEPNUM}.g"

        if [ -f "$FRAME_G" ]; then continue; fi

        # Determine whether this is the first-ever step or a resume
        if [ "$i" -eq 1 ]; then
            mged -c "$SIM_DB" "simulate --steps 1 scene.c $STEP_DUR" 2>/dev/null
        elif [ "$RESUMED" -eq 0 ]; then
            # First missing step after existing ones — always use --resume
            mged -c "$SIM_DB" "simulate --resume --steps 1 scene.c $STEP_DUR" 2>/dev/null
            RESUMED=1
        else
            mged -c "$SIM_DB" "simulate --resume --steps 1 scene.c $STEP_DUR" 2>/dev/null
        fi
        cp "$SIM_DB" "$FRAME_G"

        if [ $(( i % 30 )) -eq 0 ]; then
            echo "  Step $i/$NSTEPS done"
        fi
    done
    echo "  All $NSTEPS simulation frames saved."
else
    echo "  All $NSTEPS simulation frames already exist, skipping."
fi

# ----------------------------------------------------------------
# Step 3: Render frames (cinematic_render.py skips existing .png)
# ----------------------------------------------------------------
EXISTING_PNG=$(ls "$RENDER_DIR"/frame_*.png 2>/dev/null | wc -l)
echo ""
echo "=== Step 3: Rendering frames — $EXISTING_PNG/$NSTEPS already rendered ==="

if [ "$EXISTING_PNG" -lt "$NSTEPS" ]; then
    python3 "$SCRIPT_DIR/cinematic_render.py" \
        --build  "$BRLCAD_BUILD" \
        --frames "$FRAMES_DIR" \
        --output "$RENDER_DIR" \
        --width  "$WIDTH" \
        --height "$HEIGHT" \
        --first  1 \
        --last   "$NSTEPS" \
        --jobs   "$(nproc)"
    echo "  Rendering complete."
else
    echo "  All $NSTEPS frames already rendered, skipping."
fi

# ----------------------------------------------------------------
# Step 4: Assemble video (skip if already exists)
# ----------------------------------------------------------------
echo ""
echo "=== Step 4: Assembling video ==="

RENDERED_COUNT=$(ls "$RENDER_DIR"/frame_*.png 2>/dev/null | wc -l)
if [ "$RENDERED_COUNT" -lt "$NSTEPS" ]; then
    echo "  WARNING: only $RENDERED_COUNT/$NSTEPS frames rendered."
    echo "  Assembling partial video from available frames."
fi

if [ ! -f "$VIDEO_OUT" ] || [ "$RENDERED_COUNT" -gt 0 ]; then
    ffmpeg -y \
        -framerate "$FPS" \
        -pattern_type glob \
        -i "$RENDER_DIR/frame_*.png" \
        -c:v libx264 \
        -preset slow \
        -crf 18 \
        -pix_fmt yuv420p \
        -vf "fps=${FPS},scale=${WIDTH}:${HEIGHT}:flags=lanczos" \
        "$VIDEO_OUT" 2>&1 | tail -5
    echo ""
    echo "  Video assembled."
else
    echo "  Video already exists: $VIDEO_OUT"
fi

echo ""
echo "========================================================"
echo " DONE"
echo " Frames rendered: $(ls "$RENDER_DIR"/frame_*.png 2>/dev/null | wc -l) / $NSTEPS"
echo " Video: $VIDEO_OUT"
ls -lh "$VIDEO_OUT" 2>/dev/null || echo "  (video not yet produced)"
echo "========================================================"
