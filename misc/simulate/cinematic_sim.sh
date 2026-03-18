#!/bin/bash
# cinematic_sim.sh - Cinematic M35 truck simulation + render + video
#
# Usage: ./cinematic_sim.sh [brlcad_build_dir] [output_dir]
# Default build:  /home/runner/work/brlcad/brlcad-build
# Default output: /tmp/cinematic_output
#
# Produces a ~30-second cinematic video showing the M35 truck falling 500 m
# onto terrain, landing, and settling to its final stable position.
#
# Key improvements over the basic run_sim.sh:
#   * 300 simulation steps × 0.1 s = 30 s of physics (full fall + settle)
#   * Perspective projection (35° half-angle ≈ 70° full FOV ≈ 28 mm lens)
#   * "Trackside Vantage" fixed camera at terrain level, 150 m from landing
#     zone — shows the horizon naturally throughout the landing sequence
#   * Ground-like terrain: earthy-brown colour, low-gloss diffuse shader
#   * Military olive-drab truck: US Army FS 34087 paint, moderate gloss
#   * Sky-blue background (RGB 100/150/220)
#   * 10 fps output → exactly 300 s / 10 = 30-second video
#   * Assembled with ffmpeg at high quality (CRF 18, slow preset)
#
# Simulation timeline (30 × 0.1 s steps = 3 s; full: 300 × 0.1 s = 30 s):
#   t = 0 – 10 s  : truck in free fall (~490 m under 9.8 m/s² gravity)
#   t = 10 – 30 s : truck contacts terrain, bounces, and settles to rest
#
# Prerequisites:
#   1. BRL-CAD built with Bullet support:
#        cmake /path/to/brlcad -DBRLCAD_ENABLE_BULLET=ON ...
#        cmake --build . -j$(nproc)
#   2. ffmpeg installed
#
# Output files:
#   $OUTPUT_DIR/cinematic_truck_sim.g          — simulation database
#   $OUTPUT_DIR/frames/frame_0001.g …          — per-step geometry
#   $OUTPUT_DIR/frames/frame_0001.png …        — rendered frames
#   $OUTPUT_DIR/truck_terrain_cinematic.mp4    — final video

set -e

BRLCAD_BUILD=${1:-/home/runner/work/brlcad/brlcad-build}
OUTPUT_DIR=${2:-/tmp/cinematic_output}
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

export PATH="$BRLCAD_BUILD/bin:$PATH"
# Include system library path for Bullet physics (used by the ged-simulate plugin)
export LD_LIBRARY_PATH="$BRLCAD_BUILD/lib:/usr/lib/x86_64-linux-gnu:$LD_LIBRARY_PATH"
# Tell BRL-CAD where to find its plugin directory
export BRLCAD_ROOT="$BRLCAD_BUILD"

DB_DIR="$BRLCAD_BUILD/share/db"
SIM_DB="$OUTPUT_DIR/cinematic_truck_sim.g"
FRAMES_DIR="$OUTPUT_DIR/frames"

# Render resolution
WIDTH=1280
HEIGHT=720

# Simulation parameters
NSTEPS=300        # 300 × 0.1 s = 30 s total physics time
STEP_DUR=0.1      # seconds per step
# Steps 1-100 : free fall  (~10 s)
# Steps 101-300 : landing + settling (~20 s)

# Video parameters
FPS_IN=10         # input frame rate (300 frames / 30 s = 10 fps)
VIDEO_OUT="$OUTPUT_DIR/truck_terrain_cinematic.mp4"

mkdir -p "$OUTPUT_DIR" "$FRAMES_DIR"

echo "========================================================"
echo " Cinematic M35 Simulation  |  ${NSTEPS} steps × ${STEP_DUR}s = $(python3 -c "print(${NSTEPS}*${STEP_DUR})") s"
echo " Resolution: ${WIDTH}×${HEIGHT}  |  ${FPS_IN} fps → 30-second video"
echo "========================================================"

# ----------------------------------------------------------------
# Step 1: Build simulation database with cinematic shaders
# ----------------------------------------------------------------
echo ""
echo "=== Step 1: Building cinematic simulation database ==="
cd "$DB_DIR"
rm -f "$SIM_DB"
mged -c "$SIM_DB" < "$SCRIPT_DIR/cinematic_setup.tcl"
echo "  Created: $SIM_DB"
mged -c "$SIM_DB" "ls scene.c" 2>/dev/null | head -5

# ----------------------------------------------------------------
# Step 2: Run simulation — 300 steps of 0.1 s each
# ----------------------------------------------------------------
echo ""
echo "=== Step 2: Simulation (${NSTEPS} × ${STEP_DUR} s) ==="
cd "$DB_DIR"

for i in $(seq 1 $NSTEPS); do
    STEPNUM=$(printf "%04d" $i)
    FRAME_G="$FRAMES_DIR/frame_${STEPNUM}.g"

    # Skip if frame already exists (allows resuming interrupted runs)
    if [ -f "$FRAME_G" ]; then
        echo "  Step $i/$NSTEPS already exists, skipping"
        continue
    fi

    if [ "$i" -eq 1 ]; then
        mged -c "$SIM_DB" "simulate --steps 1 scene.c $STEP_DUR" 2>/dev/null
    else
        mged -c "$SIM_DB" "simulate --resume --steps 1 scene.c $STEP_DUR" 2>/dev/null
    fi

    cp "$SIM_DB" "$FRAME_G"

    # Progress report every 10 steps
    if [ $(( i % 10 )) -eq 0 ]; then
        echo "  Step $i/$NSTEPS done  (sim time $(python3 -c "print(f'{$i*$STEP_DUR:.1f}')") s)"
    fi
done
echo "  All $NSTEPS simulation frames saved to $FRAMES_DIR/"

# ----------------------------------------------------------------
# Step 3: Render frames with cinematic perspective camera
# ----------------------------------------------------------------
echo ""
echo "=== Step 3: Rendering ${NSTEPS} frames (${WIDTH}×${HEIGHT}, perspective) ==="

python3 "$SCRIPT_DIR/cinematic_render.py" \
    --build  "$BRLCAD_BUILD" \
    --frames "$FRAMES_DIR" \
    --output "$FRAMES_DIR" \
    --width  "$WIDTH" \
    --height "$HEIGHT" \
    --first  1 \
    --last   "$NSTEPS" \
    --jobs   1

RENDERED=$(ls "$FRAMES_DIR"/frame_*.png 2>/dev/null | wc -l)
echo "  Rendered: $RENDERED / $NSTEPS frames"

# ----------------------------------------------------------------
# Step 4: Assemble cinematic video
#
# Input:  300 frames at 10 fps  → 30-second output
# Output: 1280×720, CRF 18 (high quality), slow preset, H.264
#
# The -vf "fps=10" filter ensures steady 10 fps even if some frames
# are missing; gaps are filled with the previous frame.
# ----------------------------------------------------------------
echo ""
echo "=== Step 4: Assembling video ==="

ffmpeg -y \
    -framerate "$FPS_IN" \
    -pattern_type glob \
    -i "$FRAMES_DIR/frame_*.png" \
    -c:v libx264 \
    -preset slow \
    -crf 18 \
    -pix_fmt yuv420p \
    -vf "fps=${FPS_IN},scale=${WIDTH}:${HEIGHT}:flags=lanczos" \
    "$VIDEO_OUT" 2>&1 | tail -5

echo ""
echo "========================================================"
echo " DONE"
echo " Video : $VIDEO_OUT"
ls -lh "$VIDEO_OUT"
echo "========================================================"
