#!/usr/bin/env python3
"""
Shared helpers for the BRL-CAD object-type catalog image generator.

Everything visual is produced with BRL-CAD's own tools:

  * ``rt``      ray traces the shaded solid into a disk framebuffer,
  * ``rtedge``  overlays silhouette + crease edges onto the same framebuffer
                (so every shape reads clearly), and
  * ``fbline`` / ``fblabel`` / ``pix-fb`` draw the parameter callouts and
                assemble the variety montages,
  * ``fb-png``  grabs the finished framebuffer to a PNG.

Python is used only to orchestrate those tools, to size each view so the
object fills the frame, and to project 3D parameter points to 2D pixels.  The
projection is reconstructed from the exact view ``rt`` reports (orientation,
size, model bounds), so callouts land precisely on the rendered geometry.

No image-drawing libraries (matplotlib, PIL drawing, ...) are used; PIL is used
only to measure the rendered object's extent so the view can be fit to it.
"""

import os
import re
import subprocess
import sys

import numpy as np


# ----------------------------------------------------------------------------
# Tool discovery
# ----------------------------------------------------------------------------

def find_bin():
    env = os.environ.get("BRLCAD_BIN")
    if env and os.path.isdir(env):
        return env
    here = os.path.dirname(os.path.abspath(__file__))
    root = os.path.abspath(os.path.join(here, "..", "..", "..", "..", ".."))
    for c in (os.path.join(root, "out", "build", "x64-Debug", "bin"),
              os.path.join(root, "out", "build", "x64-Release", "bin"),
              os.path.join(root, "build", "bin")):
        if os.path.isdir(c) and (os.path.exists(os.path.join(c, "rt.exe"))
                                 or os.path.exists(os.path.join(c, "rt"))):
            return c
    raise RuntimeError("Could not find BRL-CAD bin dir; set BRLCAD_BIN")


BIN = find_bin()


def _exe(name):
    for ext in (".exe", ""):
        p = os.path.join(BIN, name + ext)
        if os.path.exists(p):
            return p
    return os.path.join(BIN, name)


def _cwd(gfile):
    return os.path.dirname(os.path.abspath(gfile)) or None


# ----------------------------------------------------------------------------
# Palette
# ----------------------------------------------------------------------------

BG = (236, 238, 242)            # panel background
FG = (95, 140, 205)             # default object color
EDGE = (28, 30, 34)             # rtedge line color
ACCENT = (214, 82, 28)          # vector / dimension arrows
VERTEX = (196, 28, 28)          # defining points
INK = (20, 20, 20)              # label text
_CHARW = 17.7                   # measured nonie.r.12 glyph advance (px)


def _c(rgb):
    return "%d/%d/%d" % (rgb[0], rgb[1], rgb[2])


# ----------------------------------------------------------------------------
# mged drivers
# ----------------------------------------------------------------------------

def mged(gfile, *args):
    cmd = [_exe("mged"), "-c", gfile] + [str(a) for a in args]
    return subprocess.run(cmd, capture_output=True, text=True, cwd=_cwd(gfile))


def mged_batch(gfile, cmds):
    script = "\n".join(" ".join(str(a) for a in c) for c in cmds) + "\nquit\n"
    return subprocess.run([_exe("mged"), "-c", gfile], input=script,
                          capture_output=True, text=True, cwd=_cwd(gfile))


def make_in(gfile, name, gtype, params):
    return mged(gfile, "in", name, gtype, *[str(x) for x in params])


def build(gfile, gtype, params, color=None, region="r", solid="s"):
    """Create a solid and wrap it in a colored region, in one mged process."""
    color = color or FG
    mged_batch(gfile, [
        ["in", solid, gtype] + [str(x) for x in params],
        ["r", region, "u", solid],
        ["mater", region, "plastic", color[0], color[1], color[2], 0],
    ])


# ----------------------------------------------------------------------------
# View parsing + projection
# ----------------------------------------------------------------------------

_re_orient = re.compile(r"Orientation:\s*([-\d.eE]+),\s*([-\d.eE]+),\s*([-\d.eE]+),\s*([-\d.eE]+)")
_re_size = re.compile(r"Size:\s*([-\d.eE]+)")
_re_model = re.compile(r"Model:\s*X\(([-\d.eE]+),\s*([-\d.eE]+)\),\s*Y\(([-\d.eE]+),\s*([-\d.eE]+)\),\s*Z\(([-\d.eE]+),\s*([-\d.eE]+)\)")


class View:
    """Maps model coordinates to final image pixels (top-left origin)."""

    def __init__(self, orient, size, model, w, h):
        x, y, z, wq = orient
        n = np.sqrt(x*x + y*y + z*z + wq*wq) or 1.0
        x, y, z, wq = x/n, y/n, z/n, wq/n
        R = np.array([
            [1-2*(y*y+z*z),   2*(x*y-wq*z),   2*(x*z+wq*y)],
            [2*(x*y+wq*z),    1-2*(x*x+z*z),  2*(y*z-wq*x)],
            [2*(x*z-wq*y),    2*(y*z+wq*x),   1-2*(x*x+y*y)],
        ])
        self.R = R.T                        # rt reports view->model; we need model->view
        self.size = size
        xmn, xmx, ymn, ymx, zmn, zmx = model
        self.center = np.array([(xmn+xmx)/2, (ymn+ymx)/2, (zmn+zmx)/2])
        self.w, self.h = w, h

    def project(self, P):
        v = self.R @ (np.asarray(P, float) - self.center)
        px = (v[0] / self.size + 0.5) * self.w
        py = (0.5 - v[1] / self.size) * self.h
        return px, py


def _parse_view(err, w, h):
    mo, ms, mm = _re_orient.search(err), _re_size.search(err), _re_model.search(err)
    if not (mo and ms and mm):
        sys.stderr.write(err + "\n")
        raise RuntimeError("rt did not report a view")
    return (tuple(float(g) for g in mo.groups()),
            float(ms.group(1)),
            tuple(float(g) for g in mm.groups()))


# ----------------------------------------------------------------------------
# Framebuffer primitives (all via BRL-CAD tools)
# ----------------------------------------------------------------------------

# IMPORTANT: libfb reads a framebuffer name containing ":" (e.g. a Windows
# "D:\..." path) as host:port and tries to open a *network* framebuffer.  So
# every framebuffer device below is a bare relative name and the tool is run
# with its cwd set to the directory that holds it.

def _run(cmd, cwd=None, timeout=25, retries=3):
    """Run a tool, killing and retrying if it hangs.

    The tiny framebuffer tools (fbline/fblabel/fb-png/...) occasionally hang on
    Windows (a virus scanner touching the frequently-rewritten .fb file); a
    bounded timeout + retry keeps a single stuck call from stalling the whole
    render.  rt/rtedge pass a larger timeout for slow (e.g. -P1 NMG) shots.
    """
    last = None
    for _ in range(retries):
        try:
            return subprocess.run(cmd, capture_output=True, text=True,
                                  cwd=cwd, timeout=timeout)
        except subprocess.TimeoutExpired as e:
            last = e
            continue
    sys.stderr.write("timed out after %d tries: %s\n" % (retries, cmd[0]))
    return subprocess.CompletedProcess(cmd, 1, "", str(last))


def _prepare_out(path):
    """Ensure `path` is writable (clear a stale read-only attribute) so tools
    can overwrite it, then remove it so a fresh file is created."""
    import stat
    if os.path.exists(path):
        try:
            os.chmod(path, stat.S_IWRITE | stat.S_IREAD)
            os.remove(path)
        except OSError:
            pass


def fbclear(fb, size, rgb, cwd):
    _run([_exe("fbclear"), "-F", fb, "-w", str(size), "-n", str(size),
          str(rgb[0]), str(rgb[1]), str(rgb[2])], cwd=cwd)


def fbline(fb, size, x0, y0, x1, y1, rgb, cwd):
    # framebuffer origin is bottom-left; caller supplies top-left pixels
    y0 = size - 1 - y0
    y1 = size - 1 - y1
    _run([_exe("fbline"), "-F", fb, "-W", str(size), "-N", str(size),
          "-r", str(rgb[0]), "-g", str(rgb[1]), "-b", str(rgb[2]),
          str(int(round(x0))), str(int(round(y0))),
          str(int(round(x1))), str(int(round(y1)))], cwd=cwd)


def fblabel(fb, size, x, y, text, rgb, cwd):
    y = size - 1 - y
    _run([_exe("fblabel"), "-F", fb, "-W", str(size), "-N", str(size),
          "-C", _c(rgb), str(int(round(x))), str(int(round(y))), text], cwd=cwd)


def _white_block(path, w, h, rgb):
    with open(path, "wb") as fh:
        fh.write(bytes(rgb) * (w * h))


def fb_png(fb, size, out, cwd):
    _run([_exe("fb-png"), "-F", fb, "-w", str(size), "-n", str(size), out], cwd=cwd)


def fb_pix(fb, size, out, cwd):
    _run([_exe("fb-pix"), "-F", fb, "-w", str(size), "-n", str(size), out], cwd=cwd)


# ----------------------------------------------------------------------------
# Object measurement (fit view to object)
# ----------------------------------------------------------------------------

def _object_fraction(png, bg):
    """max(width, height) of the non-background region, as a fraction of frame."""
    from PIL import Image
    im = np.asarray(Image.open(png).convert("RGB")).astype(int)
    diff = np.abs(im - np.array(bg)).sum(2)
    mask = diff > 24
    if not mask.any():
        return 1.0
    ys, xs = np.where(mask)
    fw = (xs.max() - xs.min() + 1) / im.shape[1]
    fh = (ys.max() - ys.min() + 1) / im.shape[0]
    return max(fw, fh)


# ----------------------------------------------------------------------------
# Annotation drawing
# ----------------------------------------------------------------------------

def _arrow(fb, size, a, b, rgb, cwd, head=True):
    ax, ay = a
    bx, by = b
    fbline(fb, size, ax, ay, bx, by, rgb, cwd)
    if head:
        dx, dy = bx - ax, by - ay
        L = (dx*dx + dy*dy) ** 0.5 or 1.0
        dx, dy = dx / L, dy / L
        hl = max(9, size * 0.02)
        for ang in (2.618, -2.618):        # +/-150 deg
            ca, sa = np.cos(ang), np.sin(ang)
            hx = bx + hl * (dx * ca - dy * sa)
            hy = by + hl * (dx * sa + dy * ca)
            fbline(fb, size, bx, by, hx, hy, rgb, cwd)


def _marker(fb, size, p, rgb, cwd):
    x, y = p
    r = max(4, int(size * 0.009))           # small cross (2 fb calls)
    fbline(fb, size, x - r, y, x + r, y, rgb, cwd)
    fbline(fb, size, x, y - r, x, y + r, rgb, cwd)


_HALO = (248, 249, 251)


def _label(fb, size, cwd, x, y, text, rgb):
    """Text with a light halo so it reads on light or dark areas.

    Drawn as several offset copies in the halo color, then the text on top;
    this avoids the disk-framebuffer quirks of clear-color and image blits.
    """
    lx, ly = int(x), int(y)
    for dx, dy in ((-2, -2), (2, 2)):       # light halo, few draws
        fblabel(fb, size, lx + dx, ly + dy, text, _HALO, cwd)
    fblabel(fb, size, lx, ly, text, rgb, cwd)


def _draw_annotations(fb, size, view, anns, cwd):
    for a in anns:
        t = a["t"]
        if t == "pt":
            p = view.project(a["p"])
            _marker(fb, size, p, VERTEX, cwd)
            if a.get("label"):
                off = a.get("off", (10, -18))
                _label(fb, size, cwd, p[0] + off[0], p[1] + off[1],
                       a["label"], INK)
        elif t in ("vec", "dim", "seg"):
            pa = view.project(a["a"])
            pb = view.project(a["b"])
            if t == "seg":
                fbline(fb, size, pa[0], pa[1], pb[0], pb[1], ACCENT, cwd)
            elif t == "dim":
                _arrow(fb, size, pb, pa, ACCENT, cwd)
                _arrow(fb, size, pa, pb, ACCENT, cwd)
            else:
                _arrow(fb, size, pa, pb, ACCENT, cwd)
            if a.get("label"):
                off = a.get("off", (10, -16))
                mx = (pa[0] + pb[0]) / 2 + off[0]
                my = (pa[1] + pb[1]) / 2 + off[1]
                _label(fb, size, cwd, mx, my, a["label"], ACCENT)


# ----------------------------------------------------------------------------
# The rendering entry point
# ----------------------------------------------------------------------------

def probe(gfile, obj, az=35, el=25, ambient=0.42, extra=None):
    """Autoview probe: returns (orientation, autosize, model_bbox, fill_fraction)."""
    scratch_dir = _cwd(gfile)
    gname = os.path.basename(gfile)
    pn = os.path.splitext(gname)[0] + ".pr.png"
    p = _run([_exe("rt"), "-o", pn, "-w", "220", "-n", "220", "-C", _c(BG),
              "-A", str(ambient), "-a", str(az), "-e", str(el)]
             + (extra or []) + [gname, obj], cwd=scratch_dir, timeout=90)
    orient, autosize, model = _parse_view(p.stderr + p.stdout, 220, 220)
    frac = _object_fraction(os.path.join(scratch_dir, pn), BG)
    try:
        os.remove(os.path.join(scratch_dir, pn))
    except OSError:
        pass
    return orient, autosize, model, frac


def eye_for(center, az, el, dist):
    """Eye point that looks at `center` from the given az/el at `dist` mm."""
    import math
    a, e = math.radians(az), math.radians(el)
    d = (math.cos(e)*math.cos(a), math.cos(e)*math.sin(a), math.sin(e))
    return (center[0]+d[0]*dist, center[1]+d[1]*dist, center[2]+d[2]*dist)


def render(gfile, obj, out_png, size=600, az=35, el=25, fill=0.82,
           color=FG, anns=None, hyper=2, ambient=0.42, edge=EDGE, caption=None,
           view=None, extra=None):
    """Shaded rt render + rtedge overlay + parameter callouts.

    If `view` = (orientation, eye_pt, viewsize) is given, that exact view is
    used (so several images can share one frame -- e.g. the Boolean montage);
    otherwise the object is probed and a viewsize chosen to fill `fill` of the
    frame.  Callouts require the probe path (they need the projection).

    `extra` is a list of extra flags passed to rt and rtedge (e.g. ["-P1"] to
    force single-threaded shooting -- the NMG hitmiss freelist is a global that
    is not thread-safe).
    """
    extra = extra or []
    scratch_dir = _cwd(gfile)
    gname = os.path.basename(gfile)
    gbase = os.path.splitext(gname)[0]      # unique per object -> parallel-safe
    fb = gbase + ".render.fb"

    vobj = None
    if view is not None:
        orient, eye, viewsize = view
        common = ["-w", str(size), "-n", str(size),
                  "-c", "orientation %g %g %g %g" % tuple(orient),
                  "-c", "eye_pt %g %g %g" % tuple(eye),
                  "-c", "viewsize %g" % viewsize]
    else:
        orient, autosize, model, frac = probe(gfile, obj, az, el, ambient, extra)
        viewsize = autosize * frac / fill
        vobj = View(orient, viewsize, model, size, size)
        common = ["-w", str(size), "-n", str(size), "-a", str(az), "-e", str(el),
                  "-c", "viewsize %g" % viewsize]

    # shaded render into a pre-sized disk framebuffer
    fbclear(fb, size, BG, scratch_dir)
    _run([_exe("rt"), "-F", fb] + common + extra + ["-C", _c(BG),
         "-A", str(ambient), "-H", str(hyper), gname, obj],
         cwd=scratch_dir, timeout=90)

    # overlay silhouette + crease edges (both sides, antialiased)
    _run([_exe("rtedge"), "-F", fb] + common + extra + [
        "-c", "set overlay=1", "-c", "set detect_distance=1",
        "-c", "set detect_normals=1", "-c", "set both_sides=1",
        "-c", "set antialias=1", "-c", "set fg=%s" % _c(edge),
        gname, obj], cwd=scratch_dir, timeout=90)

    # parameter callouts
    if anns and vobj is not None:
        _draw_annotations(fb, size, vobj, anns, scratch_dir)
    if caption:
        cx = int(size / 2 - len(caption) * (_CHARW / 2.0))
        _label(fb, size, scratch_dir, cx, size - 20, caption, INK)

    _prepare_out(out_png)
    fb_png(fb, size, out_png, scratch_dir)
    fp = os.path.join(scratch_dir, fb)
    if os.path.exists(fp):
        os.remove(fp)
    return vobj


def render_cell(gfile, obj, out_png, size=360, caption=None, az=35, el=25,
                fill=0.82, color=FG, hyper=2, ambient=0.42, view=None, extra=None):
    """Render one montage cell (shaded + edges + burned-in caption) to a PNG.

    Rendered at 512 (the disk framebuffer only behaves at >= 512), with the
    caption drawn on the framebuffer, then downsampled to `size`.
    """
    scratch_dir = _cwd(gfile)
    tmp_png = os.path.basename(out_png) + ".r.png"
    render(gfile, obj, os.path.join(scratch_dir, tmp_png), size=512,
           az=az, el=el, fill=fill, anns=None, hyper=hyper, ambient=ambient,
           caption=caption, view=view, extra=extra)
    from PIL import Image
    im = Image.open(os.path.join(scratch_dir, tmp_png)).convert("RGB")
    im.load()
    _prepare_out(out_png)
    im.resize((size, size), Image.LANCZOS).save(out_png)
    im.close()
    try:
        os.remove(os.path.join(scratch_dir, tmp_png))
    except OSError:
        pass


def montage(cell_pngs, out_png, cell=360, gap=18, cols=None):
    """Tile captioned cell PNGs (rendered + labelled by BRL-CAD) into a grid.

    This is pure image layout (PIL paste): every pixel of every cell, including
    its caption, was produced by rt/rtedge/fblabel above.
    """
    from PIL import Image
    n = len(cell_pngs)
    if cols is None:
        cols = n if n <= 4 else (n + 1) // 2
    rows = (n + cols - 1) // cols
    W = cols * cell + (cols + 1) * gap
    H = rows * cell + (rows + 1) * gap
    canvas = Image.new("RGB", (W, H), BG)
    for i, p in enumerate(cell_pngs):
        r, c = divmod(i, cols)
        im = Image.open(p).convert("RGB")
        im.load()
        if im.size != (cell, cell):
            im = im.resize((cell, cell), Image.LANCZOS)
        canvas.paste(im, (gap + c * (cell + gap), gap + r * (cell + gap)))
        im.close()
    _prepare_out(out_png)
    canvas.save(out_png)


if __name__ == "__main__":
    print("BIN =", BIN)
