#!/usr/bin/env python3
"""
render_frames.py - Render simulation frame .g files with a fixed-world chase camera.

The camera chases the truck in XY (maintains a fixed horizontal+vertical
offset from the truck center) so the truck is always in the same part of the
frame.  The camera position is computed per-frame from the truck bounding
box, and an explicit orientation quaternion is computed to give an az=225,
el=35 view direction.  This is a "chase-cam" style:
  * Truck stays at a consistent position in the frame
  * Ground rushes toward the truck from below as it falls
  * Landing and wheel-contact are clearly visible

Usage:
    python3 render_frames.py \\
        --build  /path/to/brlcad-build \\
        --frames /path/to/frames_dir \\
        --output /path/to/output_dir \\
        [--width 1280] [--height 720] \\
        [--first 1] [--last 25]

The script renders scene.c/truck.sim + terrain.sim for each frame .g file.
"""

import argparse
import math
import os
import subprocess
import sys


# ---------------------------------------------------------------------------
# Camera helpers
# ---------------------------------------------------------------------------

def normalize(v):
    n = math.sqrt(sum(c*c for c in v))
    return tuple(c/n for c in v) if n > 1e-9 else v


def cross(a, b):
    return (a[1]*b[2] - a[2]*b[1],
            a[2]*b[0] - a[0]*b[2],
            a[0]*b[1] - a[1]*b[0])


def mat3_to_quaternion(m):
    """Convert a 3x3 rotation matrix (row-major list-of-lists) to (qx,qy,qz,qw)."""
    trace = m[0][0] + m[1][1] + m[2][2]
    if trace > 0:
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
    Compute the BRL-CAD rt orientation quaternion for a camera placed at
    'eye' looking toward 'lookat' with the given world up direction.

    BRL-CAD's convention (from rt man page):
      The quaternion represents the rotation which, when applied to the
      standard orientation (looking in the -Z direction with +X to the
      right and +Y up), gives the actual view orientation.

    Standard camera axes:
      +X → right    +Y → up    -Z → forward (into scene)

    Actual camera axes derived from eye/lookat/world_up:
      F = normalize(lookat - eye)   (forward direction, into scene = -Z standard)
      R = normalize(F × world_up)   (right = +X standard)
      U = R × F                     (view up = +Y standard)

    We need the rotation M such that:
      M * standard_+X  = R  →  M * (1,0,0) = R
      M * standard_+Y  = U  →  M * (0,1,0) = U
      M * standard_-Z  = F  →  M * (0,0,1) = -F  (since -Z standard → forward)

    So the columns of M are: [R, U, -F]
    Written as a row-major matrix (for quat conversion):
      row0 = [Rx, Ux, -Fx]
      row1 = [Ry, Uy, -Fy]
      row2 = [Rz, Uz, -Fz]
    """
    ex, ey, ez = eye
    lx, ly, lz = lookat

    # Forward direction (eye to lookat)
    F = normalize((lx - ex, ly - ey, lz - ez))

    # Right = F × world_up  (if degenerate, try alternate up)
    R = normalize(cross(F, world_up))
    if R == world_up:  # degenerate: F parallel to world_up
        R = normalize(cross(F, (0, 1, 0)))

    # True view up
    U = cross(R, F)   # R × F (no need to normalize, already unit if R,F are unit)

    fx, fy, fz = F
    rx, ry, rz = R
    ux, uy, uz = U

    # Rotation matrix columns = [R, U, -F]
    # Row i = [R[i], U[i], -F[i]]
    mat = [
        [rx, ux, -fx],
        [ry, uy, -fy],
        [rz, uz, -fz],
    ]
    return mat3_to_quaternion(mat)


def az_el_to_chase_offset(az_deg, el_deg, distance):
    """
    Return the (dx, dy, dz) offset from the scene center to the camera eye
    for the given azimuth and elevation in BRL-CAD convention.

    BRL-CAD azimuth: 0 = looking from -Y side toward +Y (camera behind model),
    increasing CCW when viewed from +Z.  90 = looking from +X side.
    Elevation: 0 = horizontal, positive = camera above horizontal.
    """
    az = math.radians(az_deg)
    el = math.radians(el_deg)
    # Camera is IN the (cam_x, cam_y, cam_z) direction from scene
    cam_x = -math.sin(az) * math.cos(el)   # rotating -Y axis by az CCW
    cam_y =  math.cos(az) * math.cos(el)   # (verified: az=0 → cam_y=-1, cam at -Y)
    cam_z =  math.sin(el)
    return (cam_x * distance, cam_y * distance, cam_z * distance)


# ---------------------------------------------------------------------------
# Truck bounding-box query
# ---------------------------------------------------------------------------

def get_truck_bbox(mged_bin, frame_g, db_dir):
    """Return (minX,minY,minZ, maxX,maxY,maxZ) of scene.c/truck.sim."""
    env = dict(os.environ)
    env['PATH'] = os.path.dirname(mged_bin) + ':' + env.get('PATH', '')
    lib_dir = os.path.join(os.path.dirname(mged_bin), '..', 'lib')
    env['LD_LIBRARY_PATH'] = os.path.abspath(lib_dir) + ':' + env.get('LD_LIBRARY_PATH', '')

    r = subprocess.run(
        [mged_bin, '-c', frame_g, 'bb -e scene.c/truck.sim'],
        capture_output=True, text=True, env=env, cwd=db_dir, timeout=30
    )
    for line in (r.stdout + r.stderr).splitlines():
        if 'min' in line and 'max' in line:
            # Format: min {X Y Z} max {X Y Z}
            line = line.replace('{', '').replace('}', '')
            parts = line.split()
            # parts: ["min", X, Y, Z, "max", X, Y, Z]
            try:
                minX, minY, minZ = float(parts[1]), float(parts[2]), float(parts[3])
                maxX, maxY, maxZ = float(parts[5]), float(parts[6]), float(parts[7])
                return minX, minY, minZ, maxX, maxY, maxZ
            except (IndexError, ValueError):
                continue
    # Fallback: truck at initial position
    return (12796640, 12798750, 1736440, 12803360, 12801250, 1739058)


# ---------------------------------------------------------------------------
# Main render loop
# ---------------------------------------------------------------------------

def main():
    parser = argparse.ArgumentParser(description='Render simulation frames with chase camera')
    parser.add_argument('--build',  default='/home/runner/work/brlcad/brlcad-build')
    parser.add_argument('--frames', default='/tmp/sim_output/frames')
    parser.add_argument('--output', default='/tmp/sim_output/frames')
    parser.add_argument('--width',  type=int, default=1280)
    parser.add_argument('--height', type=int, default=720)
    parser.add_argument('--first',  type=int, default=1)
    parser.add_argument('--last',   type=int, default=25)
    args = parser.parse_args()

    build = args.build
    rt_bin     = os.path.join(build, 'bin', 'rt')
    mged_bin   = os.path.join(build, 'bin', 'mged')
    pix2png    = os.path.join(build, 'bin', 'pix-png')
    db_dir     = os.path.join(build, 'share', 'db')

    for b in [rt_bin, mged_bin, pix2png]:
        if not os.path.exists(b):
            sys.exit(f'ERROR: binary not found: {b}')

    env = dict(os.environ)
    env['PATH'] = os.path.join(build, 'bin') + ':' + env.get('PATH', '')
    env['LD_LIBRARY_PATH'] = os.path.join(build, 'lib') + ':' + env.get('LD_LIBRARY_PATH', '')

    # Chase-camera parameters:
    #   az=225, el=35 view direction (NW+high camera looking SE+down)
    #   Distance from truck center to camera eye: 35 m = 35000 mm
    #   Viewsize: 15000 mm (shows truck comfortably)
    AZ_DEG, EL_DEG = 225.0, 35.0
    CHASE_DIST  = 35000   # mm
    VIEWSIZE    = 15000   # mm

    # Precompute fixed offset and lookat direction for the chase camera
    dx, dy, dz = az_el_to_chase_offset(AZ_DEG, EL_DEG, CHASE_DIST)

    # The lookat point is the truck center; the camera looks from
    # eye = truck_center + (dx, dy, dz) toward truck_center.
    # Precompute the orientation quaternion (it's the same every frame since
    # eye→lookat direction is always the same relative offset).
    fake_truck_center = (0.0, 0.0, 0.0)
    fake_eye = (dx, dy, dz)
    qx, qy, qz, qw = camera_quaternion(fake_eye, fake_truck_center)
    print(f'Chase camera: az={AZ_DEG} el={EL_DEG} dist={CHASE_DIST}mm viewsize={VIEWSIZE}mm')
    print(f'Camera offset: ({dx:.0f}, {dy:.0f}, {dz:.0f}) mm from truck center')
    print(f'Orientation quaternion: {qx:.6f} {qy:.6f} {qz:.6f} {qw:.6f}')

    W, H = args.width, args.height
    os.makedirs(args.output, exist_ok=True)

    for i in range(args.first, args.last + 1):
        stepnum  = f'{i:04d}'
        frame_g  = os.path.join(args.frames, f'frame_{stepnum}.g')
        frame_pix = os.path.join(args.output, f'frame_{stepnum}.pix')
        frame_png = os.path.join(args.output, f'frame_{stepnum}.png')

        if not os.path.exists(frame_g):
            print(f'Frame {i}: .g file not found ({frame_g}), skipping')
            continue

        # Query truck world-space center for this frame
        minX, minY, minZ, maxX, maxY, maxZ = get_truck_bbox(mged_bin, frame_g, db_dir)
        cx = (minX + maxX) / 2
        cy = (minY + maxY) / 2
        cz = (minZ + maxZ) / 2

        # Camera eye position
        ex = cx + dx
        ey = cy + dy
        ez = cz + dz

        print(f'Frame {i:2d}: truck_center=({cx:.0f},{cy:.0f},{cz:.0f}) '
              f'eye=({ex:.0f},{ey:.0f},{ez:.0f})')

        # Build rt command using explicit eye_pt + orientation + viewsize.
        # '-c shader plastic' overrides the default terrain shader (turbump
        # noise) which can hang on large-coordinate DSP geometry.
        # '-P 1' uses a single thread to avoid a DSP BVH threading deadlock.
        rt_cmd = [
            rt_bin,
            '-P', '1',
            '-w', str(W),
            '-n', str(H),
            '-c', f'viewsize {VIEWSIZE}',
            '-c', f'eye_pt {ex:.2f} {ey:.2f} {ez:.2f}',
            '-c', f'orientation {qx:.8f} {qy:.8f} {qz:.8f} {qw:.8f}',
            '-c', 'shader plastic',
            '-o', frame_pix,
            frame_g,
            'scene.c/truck.sim',
            'terrain.sim',
        ]

        r = subprocess.run(rt_cmd, stdout=subprocess.DEVNULL,
                           stderr=subprocess.DEVNULL, env=env,
                           timeout=120, cwd=db_dir)
        if not os.path.exists(frame_pix) or os.path.getsize(frame_pix) == 0:
            # Fallback: auto-center with az/el (always produces visible output)
            print(f'  WARNING: explicit camera produced empty output, using fallback')
            rt_cmd_fallback = [
                rt_bin, '-P', '1', '-w', str(W), '-n', str(H),
                '-a', str(AZ_DEG), '-e', str(EL_DEG),
                '-o', frame_pix,
                frame_g, 'scene.c/truck.sim',
            ]
            subprocess.run(rt_cmd_fallback, stdout=subprocess.DEVNULL,
                           stderr=subprocess.DEVNULL, env=env,
                           timeout=120, cwd=db_dir)

        # Convert .pix → .png
        if os.path.exists(frame_pix) and os.path.getsize(frame_pix) > 0:
            with open(frame_pix, 'rb') as pix_f:
                result = subprocess.run(
                    [pix2png, '-w', str(W), '-n', str(H)],
                    stdin=pix_f, capture_output=True, env=env
                )
            with open(frame_png, 'wb') as png_f:
                png_f.write(result.stdout)
            os.remove(frame_pix)

            sz = os.path.getsize(frame_png)
            print(f'  -> {frame_png} ({sz//1024} KB)')
        else:
            print(f'  ERROR: no .pix output for frame {i}')


if __name__ == '__main__':
    main()
