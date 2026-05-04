# M35 Truck on Terrain Physics Simulation

This directory contains scripts to run a BRL-CAD physics simulation
of the M35 "component" truck falling onto the `terra` terrain using
the Bullet-backed `simulate` command, and render the result as a video.

## Prerequisites

1. BRL-CAD built with Bullet support:
   ```
   cmake /path/to/brlcad -DBRLCAD_ENABLE_BULLET=ON -DBRLCAD_EXTRADOCS=OFF \
       -DBRLCAD_ENABLE_STEP=OFF -DBRLCAD_BUNDLED_LIBS=SYSTEM -G Ninja
   ninja -j4
   ```

2. `ffmpeg` for video assembly

## Files

| File | Purpose |
|------|---------|
| `setup_sim.tcl` | Tcl script fed to `mged -c` to build the combined simulation database |
| `run_sim.sh` | Master shell script: runs simulation, renders frames, assembles video |

## Running

```bash
cd /path/to/brlcad/misc/simulate
./run_sim.sh /path/to/brlcad-build [output_dir]
```

Default output dir is `/tmp/sim_output`. The final video is written to
`output_dir/truck_terrain_simulation.mp4`.

## Simulation Setup

### Geometry

- **Terrain** (`terra_ground.r` / `terra.g`): DSP (Displacement Surface
  Primitive) terrain covering ~25.6 km × 25.6 km at scales representative
  of real terrain. Set as a **static** body (`mass=0`) with `roi_proxy=1`
  so Bullet uses a dynamic axis-aligned bounding box for broad-phase
  collision detection without processing all 256×256 elevation samples
  each frame.

- **Truck** (`m35_component` / `m35.g`): The M35 2½-ton truck assembly,
  approximately 6.7 m × 2.5 m × 2.6 m. Set as a **dynamic** body
  (`mass=2000 kg`).

### Initial Position

The truck is placed via a matrix on its entry in `scene.c`. The translation
moves the truck to hover 500 m above the terrain centre elevation:

```
dx = 12,798,468 mm  (terrain centre X − truck model X-centre)
dy = 12,800,000 mm  (terrain centre Y)
dz =  1,737,625 mm  (terrain centre elevation 1,237,625 mm + 500,000 mm)
```

### Why `comb` / `arced` are Fast

`comb` creates a combination record (a tree node) in the database. It
does **not** load, tessellate, or process any geometry; it just writes a
small binary record. Similarly, `arced` writes a 4×4 matrix into an
existing combination tree leaf. Both operations are O(1) and complete in
milliseconds even for multi-gigabyte databases.

The `simulate` command itself does the geometry work (ray-tracing to
build collision shapes) only when it runs.

### Simulation Parameters

- **Duration**: 10 s total (truck falls ~490 m under 9.8 m/s² gravity)
- **Steps**: 20 steps × 0.5 s = 10 s (well above Bullet's 1/60 s minimum)
- **Resume**: Each step uses `--resume` to continue from the saved
  velocity state so the full trajectory is captured incrementally

## Output

- `frames/frame_0001.g` … `frame_0020.g` — BRL-CAD database snapshot after each step
- `frames/frame_0001.png` … `frame_0020.png` — Raytraced renderings
- `truck_terrain_simulation.mp4` — Assembled video (~24 s at 24 fps)
