#!/usr/bin/env python3
"""
cinematic_render.py - Render simulation frames with a cinematic perspective camera.

Camera design: "Trackside Vantage"
  Fixed world position 150 m south of the truck's landing zone, 3 m above
  terrain level.  The camera pans/tilts to keep the truck in the center of
  the frame each step.

  Result across the 30-second fall+settle animation:
    - Early frames (truck 490 m up): camera looks ~73° above horizontal.
      Mostly sky fills the frame; truck is a small silhouette in the upper
      portion.  The far ground is visible in the bottom edge.
    - Mid frames (truck ~150 m up): camera looks ~45° upward.  Both sky and
      terrain horizon clearly visible.
    - Landing frames (truck at terrain level): camera looks nearly horizontal
      or slightly down.  Truck fills the frame, horizon is prominently visible
      across the upper-center portion — the most cinematic moment.
    - Settling frames: truck at rest on terrain, camera looks slightly down.
      Dust-settling pose.

Perspective projection is used throughout (perspective 35 = 35° half-angle =
70° full FOV ≈ a 28 mm cinema lens — slightly wide, very dramatic).

Background color: sky blue (RGB 100/150/220) via rt -C flag.

Usage:
    python3 cinematic_render.py \\
        --build  /path/to/brlcad-build \\
        --frames /path/to/frames_dir \\
        --output /path/to/output_dir \\
        [--width 1280] [--height 720] \\
        [--first 1] [--last 300]
"""

import argparse
import math
import os
import subprocess
import sys

# ---------------------------------------------------------------------------
# Camera math helpers
# ---------------------------------------------------------------------------

def normalize(v):
    n = math.sqrt(sum(c * c for c in v))
    return tuple(c / n for c in v) if n > 1e-12 else (0.0, 0.0, 1.0)


def cross(a, b):
    return (
        a[1] * b[2] - a[2] * b[1],
        a[2] * b[0] - a[0] * b[2],
        a[0] * b[1] - a[1] * b[0],
    )


def mat3_to_quaternion(m):
    """Convert a 3×3 rotation matrix (row-major list-of-lists) to (qx,qy,qz,qw)."""
    trace = m[0][0] + m[1][1] + m[2][2]
    if trace > 0.0:
        s = 0.5 / math.sqrt(trace + 1.0)
        w = 0.25 / s
        x = (m[2][1] - m[1][2]) * s
        y = (m[0][2] - m[2][0]) * s
        z = (m[1][0] - m[0][1]) * s
    elif m[0][0] > m[1][1] and m[0][0] > m[2][2]:
        s = 2.0 * math.sqrt(1.0 + m[0][0] - m[1][1] - m[2][2])
        w = (m[2][1] - m[1][2]) / s
        x = 0.25 * s
        y = (m[0][1] + m[1][0]) / s
        z = (m[0][2] + m[2][0]) / s
    elif m[1][1] > m[2][2]:
        s = 2.0 * math.sqrt(1.0 + m[1][1] - m[0][0] - m[2][2])
        w = (m[0][2] - m[2][0]) / s
        x = (m[0][1] + m[1][0]) / s
        y = 0.25 * s
        z = (m[1][2] + m[2][1]) / s
    else:
        s = 2.0 * math.sqrt(1.0 + m[2][2] - m[0][0] - m[1][1])
        w = (m[1][0] - m[0][1]) / s
        x = (m[0][2] + m[2][0]) / s
        y = (m[1][2] + m[2][1]) / s
        z = 0.25 * s
    return (x, y, z, w)


def camera_quaternion(eye, lookat, world_up=(0, 0, 1)):
    """
    Compute the BRL-CAD rt orientation quaternion.

    BRL-CAD's standard orientation is: looking in -Z with +X right, +Y up.
    We need the rotation M such that:
      M*(1,0,0) = R  (right)
      M*(0,1,0) = U  (up)
      M*(0,0,1) = -F (back-of-camera = opposite of forward)

    Columns of M: [R, U, -F].
    """
    ex, ey, ez = eye
    lx, ly, lz = lookat
    F = normalize((lx - ex, ly - ey, lz - ez))      # forward (into scene)

    # Avoid degenerate case when F is parallel to world_up
    if abs(abs(F[0] * world_up[0] + F[1] * world_up[1] + F[2] * world_up[2]) - 1.0) < 1e-6:
        world_up = (0.0, 1.0, 0.0)

    R = normalize(cross(F, world_up))                # right (+X)
    U = cross(R, F)                                  # view up (+Y)

    rx, ry, rz = R
    ux, uy, uz = U
    fx, fy, fz = F

    mat = [
        [rx, ux, -fx],
        [ry, uy, -fy],
        [rz, uz, -fz],
    ]
    return mat3_to_quaternion(mat)


# ---------------------------------------------------------------------------
# Truck bounding-box query
# ---------------------------------------------------------------------------

def get_truck_bbox(mged_bin, frame_g, db_dir):
    """Return (minX,minY,minZ, maxX,maxY,maxZ) of scene.c/truck.sim in mm."""
    env = _rt_env(mged_bin)
    try:
        r = subprocess.run(
            [mged_bin, '-c', frame_g, 'bb -e scene.c/truck.sim'],
            capture_output=True, text=True, env=env, cwd=db_dir, timeout=30,
        )
        for line in (r.stdout + r.stderr).splitlines():
            if 'min' in line and 'max' in line:
                line = line.replace('{', '').replace('}', '')
                parts = line.split()
                try:
                    minX = float(parts[1]); minY = float(parts[2]); minZ = float(parts[3])
                    maxX = float(parts[5]); maxY = float(parts[6]); maxZ = float(parts[7])
                    return minX, minY, minZ, maxX, maxY, maxZ
                except (IndexError, ValueError):
                    continue
    except subprocess.TimeoutExpired:
        pass
    # Fallback: truck at initial position
    return (12796640.0, 12798750.0, 1736440.0, 12803360.0, 12801250.0, 1739058.0)


def _rt_env(bin_path):
    env = dict(os.environ)
    bin_dir = os.path.dirname(bin_path)
    lib_dir = os.path.abspath(os.path.join(bin_dir, '..', 'lib'))
    env['PATH'] = bin_dir + ':' + env.get('PATH', '')
    env['LD_LIBRARY_PATH'] = lib_dir + ':' + env.get('LD_LIBRARY_PATH', '')
    return env


# ---------------------------------------------------------------------------
# Main render loop
# ---------------------------------------------------------------------------

def main():
    parser = argparse.ArgumentParser(
        description='Cinematic perspective render of simulate frames'
    )
    parser.add_argument('--build',  default='/home/runner/work/brlcad/brlcad-build')
    parser.add_argument('--frames', default='/tmp/cinematic_output/frames')
    parser.add_argument('--output', default='/tmp/cinematic_output/frames')
    parser.add_argument('--width',  type=int, default=1280)
    parser.add_argument('--height', type=int, default=720)
    parser.add_argument('--first',  type=int, default=1)
    parser.add_argument('--last',   type=int, default=300)
    parser.add_argument('--jobs',   type=int, default=1,
                        help='rt threads (-P); keep at 1 unless DSP threading is fixed')
    args = parser.parse_args()

    build   = args.build
    rt_bin  = os.path.join(build, 'bin', 'rt')
    mged_bin = os.path.join(build, 'bin', 'mged')
    pix2png = os.path.join(build, 'bin', 'pix-png')
    db_dir  = os.path.join(build, 'share', 'db')

    for b in [rt_bin, mged_bin, pix2png]:
        if not os.path.exists(b):
            sys.exit(f'ERROR: binary not found: {b}')

    env = _rt_env(rt_bin)
    os.makedirs(args.output, exist_ok=True)

    # ----------------------------------------------------------------
    # Camera constants
    #
    # Fixed "Trackside Vantage" position (world mm):
    #   X = terrain centre X        = 12 800 000
    #   Y = terrain centre Y − 150m = 12 650 000   (150 m south)
    #   Z = terrain centre elev + 3m = 1 240 625   (3 m above ground)
    #
    # The camera pans/tilts each frame to point at the truck centre.
    # The camera world position never changes.
    #
    # Perspective: 35° half-angle ≈ 70° full FOV ≈ 28 mm cinema lens.
    # Viewsize: set to 2 × camera-to-truck distance so the view frustum
    # captures a reasonable swath; the truck grows from a tiny silhouette
    # high up to filling most of the frame as it nears the camera.
    # ----------------------------------------------------------------
    TERRAIN_ELEV  = 1_237_625.0   # mm  (terrain centre Z)
    CAM_X = 12_800_000.0
    CAM_Y = 12_650_000.0          # 150 m south of landing zone
    CAM_Z = TERRAIN_ELEV + 3_000.0  # 3 m above terrain

    PERSPECTIVE = 35              # degrees half-angle (70° total FOV)
    SKY_R, SKY_G, SKY_B = 100, 150, 220   # sky-blue background

    W, H = args.width, args.height

    print(f'Cinematic camera position: ({CAM_X:.0f}, {CAM_Y:.0f}, {CAM_Z:.0f}) mm')
    print(f'Perspective: {PERSPECTIVE}° half-angle ({PERSPECTIVE*2}° full FOV)')
    print(f'Background (sky): RGB({SKY_R},{SKY_G},{SKY_B})')
    print(f'Resolution: {W}×{H}')

    for i in range(args.first, args.last + 1):
        stepnum   = f'{i:04d}'
        frame_g   = os.path.join(args.frames, f'frame_{stepnum}.g')
        frame_pix = os.path.join(args.output, f'frame_{stepnum}.pix')
        frame_png = os.path.join(args.output, f'frame_{stepnum}.png')

        if not os.path.exists(frame_g):
            print(f'Frame {i}: .g file not found, skipping')
            continue

        if os.path.exists(frame_png) and os.path.getsize(frame_png) > 0:
            print(f'Frame {i}: already rendered, skipping')
            continue

        # Get truck centre for this frame
        minX, minY, minZ, maxX, maxY, maxZ = get_truck_bbox(mged_bin, frame_g, db_dir)
        cx = (minX + maxX) / 2.0
        cy = (minY + maxY) / 2.0
        cz = (minZ + maxZ) / 2.0

        # Camera eye (fixed) → truck centre (varies per frame)
        eye  = (CAM_X, CAM_Y, CAM_Z)
        look = (cx, cy, cz)

        # Orientation quaternion: camera looks at truck, +Z world up
        qx, qy, qz, qw = camera_quaternion(eye, look, world_up=(0.0, 0.0, 1.0))

        # Viewsize: 2× the straight-line distance eye→truck.
        # This keeps the perspective scale consistent across all frames.
        dist = math.sqrt((cx - CAM_X)**2 + (cy - CAM_Y)**2 + (cz - CAM_Z)**2)
        viewsize = max(dist * 2.0, 5_000.0)   # at least 5 m viewsize

        # Elevation angle for logging
        horiz_dist = math.sqrt((cx - CAM_X)**2 + (cy - CAM_Y)**2)
        el_deg = math.degrees(math.atan2(cz - CAM_Z, horiz_dist)) if horiz_dist > 0 else 90.0

        print(
            f'Frame {i:3d}: truck_ctr=({cx:.0f},{cy:.0f},{cz:.0f})  '
            f'el={el_deg:+.1f}°  dist={dist/1000:.1f}m  vs={viewsize/1000:.0f}m'
        )

        rt_cmd = [
            rt_bin,
            '-P', str(args.jobs),
            '-w', str(W),
            '-n', str(H),
            '-C', f'{SKY_R}/{SKY_G}/{SKY_B}',   # background sky colour
            '-c', f'perspective {PERSPECTIVE}',
            '-c', f'viewsize {viewsize:.2f}',
            '-c', f'eye_pt {eye[0]:.2f} {eye[1]:.2f} {eye[2]:.2f}',
            '-c', f'orientation {qx:.8f} {qy:.8f} {qz:.8f} {qw:.8f}',
            '-o', frame_pix,
            frame_g,
            'scene.c/truck.sim',
            'terrain.sim',
        ]

        try:
            subprocess.run(
                rt_cmd,
                stdout=subprocess.DEVNULL,
                stderr=subprocess.DEVNULL,
                env=env,
                timeout=300,
                cwd=db_dir,
            )
        except subprocess.TimeoutExpired:
            print(f'  WARNING: rt timed out for frame {i}')

        if not os.path.exists(frame_pix) or os.path.getsize(frame_pix) == 0:
            # Fallback to simple az/el render without explicit camera
            print(f'  WARNING: perspective render empty, using az/el fallback')
            fallback = [
                rt_bin, '-P', str(args.jobs),
                '-w', str(W), '-n', str(H),
                '-C', f'{SKY_R}/{SKY_G}/{SKY_B}',
                '-c', f'perspective {PERSPECTIVE}',
                '-a', '225', '-e', '35',
                '-o', frame_pix,
                frame_g, 'scene.c/truck.sim', 'terrain.sim',
            ]
            try:
                subprocess.run(
                    fallback,
                    stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
                    env=env, timeout=300, cwd=db_dir,
                )
            except subprocess.TimeoutExpired:
                print(f'  WARNING: fallback also timed out for frame {i}')

        # Convert .pix → .png
        if os.path.exists(frame_pix) and os.path.getsize(frame_pix) > 0:
            with open(frame_pix, 'rb') as pix_f:
                result = subprocess.run(
                    [pix2png, '-w', str(W), '-n', str(H)],
                    stdin=pix_f, capture_output=True, env=env,
                )
            with open(frame_png, 'wb') as png_f:
                png_f.write(result.stdout)
            os.remove(frame_pix)
            sz = os.path.getsize(frame_png)
            print(f'  -> {os.path.basename(frame_png)} ({sz // 1024} KB)')
        else:
            print(f'  ERROR: no output for frame {i}')


if __name__ == '__main__':
    main()
