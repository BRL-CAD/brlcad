#!/bin/bash
#                      R U N _ S I M . S H
# BRL-CAD
#
# Published in 2026 by the United States Government.
# This work is in the public domain.
#
#
###
# run_sim.sh - Simulate M35 truck falling onto terra terrain; render video.
#
# Usage: ./run_sim.sh [brlcad_build_dir] [output_dir]
# Default build: /home/user/brlcad/brlcad-build
# Default output: /tmp/sim_output
#
# The truck starts 20 degrees nose-down (pitched around Y axis) so the
# front wheels contact the terrain before the rear wheels.
#
# Pipeline:
#  1. Build combined .g database (truck + terrain)  [<1 second]
#  2. Run 25 simulation steps x 0.5 s = 12.5 s total  [seconds/step]
#     - Steps 1-20: truck in free fall
#     - Steps 21-25: truck landing and settling
#  3. Render each frame with rt (chase camera, per-frame eye_pt + orientation)
#  4. Assemble video with ffmpeg (~24 fps, ~12 s)

set -e

BRLCAD_BUILD=${1:-/home/user/brlcad/brlcad-build}
OUTPUT_DIR=${2:-/tmp/sim_output}
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

export PATH="$BRLCAD_BUILD/bin:$PATH"
# Include system library path for Bullet physics (used by the ged-simulate plugin)
export LD_LIBRARY_PATH="$BRLCAD_BUILD/lib:/usr/lib/x86_64-linux-gnu:$LD_LIBRARY_PATH"
# Tell BRL-CAD where to find its plugin directory
export BRLCAD_ROOT="$BRLCAD_BUILD"

DB_DIR="$BRLCAD_BUILD/share/db"
SIM_DB="$OUTPUT_DIR/truck_terrain_sim.g"
FRAMES_DIR="$OUTPUT_DIR/frames"

mkdir -p "$OUTPUT_DIR" "$FRAMES_DIR"

# ----------------------------------------------------------------
# Step 1: Build simulation database
# Run from DB_DIR so terra.bin is in CWD (required by DSP primitive).
# All mged operations here are pure metadata writes (< 0.3 s total):
#   dbconcat  - copies object records
#   comb      - writes a combination tree record
#   attr set  - writes key/value attribute
#   arced     - writes a 4x4 matrix into a combination leaf
# None of these tessellate or process geometry.
# ----------------------------------------------------------------
echo "=== Step 1: Building simulation database ==="
cd "$DB_DIR"
rm -f "$SIM_DB"

mged -c "$SIM_DB" < "$SCRIPT_DIR/setup_sim.tcl"
echo "  Created: $SIM_DB"

# Verify scene structure
mged -c "$SIM_DB" "ls scene.c" 2>/dev/null | head -3

# ----------------------------------------------------------------
# Step 2: Step through simulation, saving .g snapshot each step.
# 25 steps × 0.5 s = 12.5 s total.
#   t=0–10 s : truck in free fall (~490 m, nose pitched 20° down)
#   t=10–12.5s: front wheels contact and settle, rear follows
# Minimum Bullet timestep = 1/60 s ≈ 0.017 s; 0.5 s >> minimum.
# ----------------------------------------------------------------
NSTEPS=25
STEP_DUR=0.5

echo "=== Step 2: Simulation ($NSTEPS × ${STEP_DUR}s = $(python3 -c "print($NSTEPS*$STEP_DUR)")s) ==="
cd "$DB_DIR"

for i in $(seq 1 $NSTEPS); do
    STEPNUM=$(printf "%04d" $i)
    if [ "$i" -eq 1 ]; then
	mged -c "$SIM_DB" "simulate --steps 1 scene.c $STEP_DUR" 2>/dev/null
    else
	mged -c "$SIM_DB" "simulate --resume --steps 1 scene.c $STEP_DUR" 2>/dev/null
    fi
    cp "$SIM_DB" "$FRAMES_DIR/frame_${STEPNUM}.g"
    echo "  Step $i/$NSTEPS done"
done
echo "  Frames saved to $FRAMES_DIR/"

# ----------------------------------------------------------------
# Step 3: Render frames using the Python chase-camera renderer.
# render_frames.py computes per-frame eye_pt from truck bounding box
# and uses a fixed orientation quaternion matching az=225, el=35 view.
# ----------------------------------------------------------------
echo "=== Step 3: Rendering $NSTEPS frames ==="

python3 "$SCRIPT_DIR/render_frames.py" \
    --build  "$BRLCAD_BUILD" \
    --frames "$FRAMES_DIR" \
    --output "$FRAMES_DIR" \
    --width  1280 \
    --height 720 \
    --first  1 \
    --last   $NSTEPS

echo "  Frames rendered."

# ----------------------------------------------------------------
# Step 4: Assemble video with ffmpeg.
# Input: 25 frames (12.5 s simulation) at 2 fps input framerate.
# Output: smooth 24 fps video with scene-change interpolation.
# Each simulated second = ~2.4 output seconds → ~25 s video total.
# High quality: CRF 18, slow preset, lanczos scale filter.
# ----------------------------------------------------------------
echo "=== Step 4: Assembling video ==="

VIDEO_OUT="$OUTPUT_DIR/truck_terrain_simulation.mp4"

ffmpeg -y \
    -framerate 2 \
    -pattern_type glob \
    -i "$FRAMES_DIR/frame_*.png" \
    -c:v libx264 \
    -preset slow \
    -crf 18 \
    -pix_fmt yuv420p \
    -vf "fps=24,scale=1280:720:flags=lanczos" \
    "$VIDEO_OUT" 2>&1

echo ""
echo "=== DONE ==="
echo "Video:  $VIDEO_OUT"
ls -lh "$VIDEO_OUT"

# Local Variables:
# tab-width: 8
# mode: sh
# sh-indentation: 4
# sh-basic-offset: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8 cino=N-s
