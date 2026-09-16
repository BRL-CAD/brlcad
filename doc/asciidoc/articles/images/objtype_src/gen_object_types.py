#!/usr/bin/env python3
"""
Generate every figure used by doc/asciidoc/articles/object_types.adoc.

Each renderable primitive gets:
  * objtype_<key>.png          - an annotated parameter diagram (the "hero"),
                                 rt-shaded with rtedge silhouette/crease edges,
                                 with parameter callouts drawn by fbline/fblabel
  * objtype_<key>_variety.png  - a montage of distinct parameterizations

All rendering and all on-image text is produced by BRL-CAD tools (rt, rtedge,
fbline, fblabel); see objtype_lib.py.  Output lands in ../ (articles/images).

Usage:
  python gen_object_types.py [key ...]      # build all, or only the named keys
  BRLCAD_BIN=/path/to/bin python gen_object_types.py
"""

import math
import os
import struct
import sys

import objtype_lib as L
from objtype_lib import FG

HERE = os.path.dirname(os.path.abspath(__file__))
IMG = os.path.dirname(HERE)                       # .../articles/images


def OUT(key):
    return os.path.join(IMG, "objtype_%s.png" % key)


def VAR(key):
    return os.path.join(IMG, "objtype_%s_variety.png" % key)


def scratch(name):
    return os.path.join(HERE, "_%s" % name)


def put_data(name, data):
    # Data files are referenced by bare basename and the tools run with cwd in
    # this directory (a Windows "D:\..." path is misread as host:port / .\path).
    path = scratch(name)
    with open(path, "wb") as fh:
        fh.write(bytes(data))
    return os.path.basename(path)


def fresh(key):
    g = scratch("%s.g" % key)
    if os.path.exists(g):
        os.remove(g)
    return g


def add(v, w):
    return tuple(a + b for a, b in zip(v, w))


# ---------------------------------------------------------------------------
# generic builders
# ---------------------------------------------------------------------------

def hero(key, gtype, params, anns, az=35, el=25, color=FG, fill=0.80):
    g = fresh(key)
    L.build(g, gtype, params, color=color)
    L.render(g, "r", OUT(key), az=az, el=el, anns=anns, fill=fill)
    print("hero", key)


def variety(key, entries, az=35, el=25, cols=None, fill=0.80):
    """entries: (caption, gtype, params) or (caption, gtype, params, color)."""
    cells = []
    for i, e in enumerate(entries):
        cap, gt, pr = e[0], e[1], e[2]
        col = e[3] if len(e) > 3 else FG
        g = fresh("%s_v%d" % (key, i))
        L.build(g, gt, pr, color=col)
        png = scratch("%s_v%d.png" % (key, i))
        L.render_cell(g, "r", png, size=360, caption=cap, az=az, el=el, fill=fill)
        cells.append(png)
    L.montage(cells, VAR(key), cols=cols)
    print("variety", key)


# ===========================================================================
# Planar-faced
# ===========================================================================

def do_arb8():
    pts = [0,0,0, 6,0,0, 6,4,0, 0,4,0, 0,0,3, 6,0,3, 6,4,3, 0,4,3]

    def pt(i):
        return tuple(pts[3*(i-1):3*i])
    anns = [{"t": "pt", "p": pt(i), "label": str(i), "off": (10, -14)}
            for i in (2, 3, 4, 5, 6, 7, 8)]
    hero("arb8", "arb8", pts, anns)
    # The full arb family as real arbs (arb4-arb8).  Rotated off-axis so the
    # tetrahedron and the others show three faces instead of a flat silhouette.
    variety("arb8", [
        ("arb4 tetrahedron", "arb4", [0,0,0, 5,0,0, 2.5,4.5,0, 2.5,1.8,4]),
        ("arb5 pyramid", "arb5", [0,0,0, 5,0,0, 5,5,0, 0,5,0, 2.5,2.5,4.5]),
        ("arb6 prism", "arb6", [0,0,0, 5,0,0, 5,4,0, 0,4,0, 2.5,0,4, 2.5,4,4]),
        ("arb7", "arb7", [0,0,0, 5,0,0, 5,4,0, 0,4,0, 0,0,3, 5,0,3, 5,4,1.5]),
        ("arb8 box", "arb8",
         [0,0,0, 4,0,0, 4,4,0, 0,4,0, 0,0,4, 4,0,4, 4,4,4, 0,4,4]),
    ], cols=5, fill=0.70, az=55, el=28)


def do_arbn():
    R, Hh = 3.0, 2.2
    planes = []
    for k in range(6):
        a = math.radians(60*k)
        planes += [math.cos(a), math.sin(a), 0, R]
    planes += [0,0,1, Hh,  0,0,-1, Hh]
    params = [8] + planes
    hero("arbn", "arbn", params, [
        {"t": "seg", "a": (0,0,0), "b": (R,0,0)},
        {"t": "pt", "p": (R,0,0), "label": "plane: n, d", "off": (58, -8)},
    ], az=25, el=32, fill=0.72)
    # "hex prism" removed: it duplicates the highlight image above.
    variety("arbn", [
        ("oct prism", "arbn",
         [10] + [c for k in range(8)
                 for c in (math.cos(math.radians(45*k)),
                           math.sin(math.radians(45*k)), 0, 3)]
             + [0,0,1, 2, 0,0,-1, 2]),
        ("clipped box", "arbn",
         [7, 1,0,0,3, -1,0,0,3, 0,1,0,3, 0,-1,0,3, 0,0,1,3, 0,0,-1,3,
          1,1,1,4.2]),
    ], az=25, el=32, fill=0.70, cols=2)


def do_half():
    g = fresh("half")
    L.mged_batch(g, [
        ["in", "sp", "sph", 0,0,0, 3],
        ["in", "hf", "half", 0,0,1, 0],
        ["r", "r", "u", "sp", "-", "hf"],
        ["mater", "r", "plastic", FG[0], FG[1], FG[2], 0],
    ])
    L.render(g, "r", OUT("half"), az=35, el=20, anns=[
        {"t": "vec", "a": (0,0,0), "b": (0,0,3), "label": "N", "off": (22, 2)},
        {"t": "pt", "p": (0,0,0), "label": "d", "off": (-26, -14)},
    ])
    print("hero half")


# ===========================================================================
# Ellipsoids
# ===========================================================================

def do_ell():
    V=(0,0,0); A=(5,0,0); B=(0,3,0); C=(0,0,2)
    hero("ell", "ell", [*V,*A,*B,*C], [
        {"t": "pt", "p": V, "label": "V", "off": (-34, 8)},
        {"t": "vec", "a": V, "b": A, "label": "A", "off": (0, 22)},
        {"t": "vec", "a": V, "b": B, "label": "B", "off": (16, -2)},
        {"t": "vec", "a": V, "b": C, "label": "C", "off": (-14, 0)},
    ])
    # "tri-axial" removed: the highlight image above is already tri-axial.
    variety("ell", [
        ("sphere  |A|=|B|=|C|", "ell", [0,0,0, 3,0,0, 0,3,0, 0,0,3]),
        ("flat disc  |C| small", "ell", [0,0,0, 4,0,0, 0,4,0, 0,0,0.6]),
        ("spike  |C| large", "ell", [0,0,0, 0.7,0,0, 0,0.7,0, 0,0,5]),
    ], cols=3)


def do_sph():
    V=(0,0,0); r=3.0
    hero("sph", "sph", [*V, r], [
        {"t": "pt", "p": V, "label": "V", "off": (-30, 8)},
        {"t": "dim", "a": V, "b": (r,0,0), "label": "r", "off": (0, 18)},
    ])


def do_superell():
    V=(0,0,0); A=(3,0,0); B=(0,3,0); C=(0,0,3)
    hero("superell", "superell", [*V,*A,*B,*C, 2.5, 2.5], [
        {"t": "pt", "p": V, "label": "V", "off": (-32, 8)},
        {"t": "vec", "a": V, "b": A, "label": "A", "off": (0, 20)},
        {"t": "vec", "a": V, "b": B, "label": "B", "off": (16, -2)},
        {"t": "vec", "a": V, "b": C, "label": "C", "off": (-14, 0)},
    ])
    variety("superell", [
        ("n=e=1  ellipsoid", "superell", [0,0,0,3,0,0,0,3,0,0,0,3, 1,1]),
        ("n=e=0.4  pinched", "superell", [0,0,0,3,0,0,0,3,0,0,0,3, 0.4,0.4]),
        ("n=e=2.5  rounded box", "superell", [0,0,0,3,0,0,0,3,0,0,0,3, 2.5,2.5]),
        ("n=1 e=4  cushion", "superell", [0,0,0,3,0,0,0,3,0,0,0,3, 1.0,4.0]),
    ])


# ===========================================================================
# Tori
# ===========================================================================

def do_tor():
    V=(0,0,0); N=(0,0,1); r1=4.0; r2=1.2
    hero("tor", "tor", [*V,*N, r1, r2], [
        {"t": "pt", "p": V, "label": "V", "off": (-30, 8)},
        {"t": "vec", "a": V, "b": (0,0,2.4), "label": "N", "off": (-20, 6)},
        {"t": "dim", "a": V, "b": (r1,0,0), "label": "r1", "off": (0, -26)},
        {"t": "dim", "a": (r1,0,0), "b": (r1+r2,0,0), "label": "r2", "off": (34, -12)},
    ], az=35, el=42)
    variety("tor", [
        ("ring  r1 >> r2", "tor", [0,0,0,0,0,1, 4, 0.8]),
        ("fat torus  r1 ~ r2", "tor", [0,0,0,0,0,1, 2.2, 1.8]),
        ("washer  thin", "tor", [0,0,0,0,0,1, 4, 0.35]),
    ], az=35, el=42)


def do_eto():
    V=(0,0,0); N=(0,0,1); r=4.0; C=(1.4,0,1.4); d=0.7
    tube = (r, 0, 0)                       # tube cross-section center (on +x)
    # semi-minor axis d: perpendicular to C within the cross-section (x-z) plane
    import math as _m
    cn = _m.hypot(C[0], C[2])
    dperp = (C[2]/cn * d, 0.0, -C[0]/cn * d)
    hero("eto", "eto", [*V,*N, r, *C, d], [
        {"t": "pt", "p": V, "label": "V", "off": (-28, 8)},
        {"t": "vec", "a": V, "b": (0,0,2.6), "label": "N", "off": (-20, 6)},
        {"t": "dim", "a": V, "b": tube, "label": "r", "off": (0, -24)},
        {"t": "vec", "a": tube, "b": add(tube, C), "label": "C", "off": (22, 0)},
        {"t": "vec", "a": tube, "b": add(tube, dperp), "label": "d", "off": (-16, 14)},
    ], az=35, el=42)
    variety("eto", [
        ("upright ellipse", "eto", [0,0,0,0,0,1, 4, 0,0,1.6, 0.7]),
        ("tilted ellipse", "eto", [0,0,0,0,0,1, 4, 1.4,0,1.4, 0.6]),
        ("flat tube", "eto", [0,0,0,0,0,1, 4, 1.8,0,0, 0.5]),
    ], az=35, el=42)


# ===========================================================================
# Cone / cylinder (TGC family)
# ===========================================================================

def do_tgc():
    # a genuine frustum so the top scale factors read clearly
    V=(0,0,0); Hh=(0,0,6); A=(2.6,0,0); B=(0,1.7,0); c=0.5; d=0.5
    top = (0, 0, 6)
    hero("tgc", "tgc", [*V,*Hh,*A,*B, c, d], [
        {"t": "pt", "p": V, "label": "V", "off": (-30, 4)},
        {"t": "vec", "a": V, "b": (0,0,6), "label": "H", "off": (22, 0)},
        {"t": "vec", "a": V, "b": A, "label": "A", "off": (2, -16)},
        {"t": "vec", "a": V, "b": B, "label": "B", "off": (18, 2)},
        {"t": "vec", "a": top, "b": add(top, (c*2.6, 0, 0)),
         "label": "c*A", "off": (2, -16)},
    ])
    variety("tgc", [
        ("cylinder  rcc", "rcc", [0,0,0,0,0,6, 2.2]),
        ("cone  trc", "trc", [0,0,0,0,0,6, 2.4, 0.4]),
        ("elliptical  rec", "rec", [0,0,0,0,0,6, 2.6,0,0, 0,1.3,0]),
        ("skewed funnel", "tgc", [0,0,0, 1.5,0,6, 2.4,0,0, 0,2.4,0, 0.4,0.9]),
    ])


def do_rcc():
    V=(0,0,0)
    hero("rcc", "rcc", [*V, 0,0,6, 2.5], [
        {"t": "pt", "p": V, "label": "V", "off": (-30, 0)},
        {"t": "vec", "a": V, "b": (0,0,6), "label": "H", "off": (20, 0)},
        {"t": "dim", "a": V, "b": (2.5,0,0), "label": "r", "off": (0, 18)},
    ])


def do_trc():
    hero("trc", "trc", [0,0,0, 0,0,6, 2.6, 1.0], [
        {"t": "pt", "p": (0,0,0), "label": "V", "off": (-28, 0)},
        {"t": "vec", "a": (0,0,0), "b": (0,0,6), "label": "H", "off": (20, 0)},
        {"t": "dim", "a": (0,0,0), "b": (2.6,0,0), "label": "r_base", "off": (0, 16)},
        {"t": "dim", "a": (0,0,6), "b": (1.0,0,6), "label": "r_top", "off": (0, 16)},
    ])


def do_rec():
    hero("rec", "rec", [0,0,0, 0,0,6, 2.8,0,0, 0,1.4,0], [
        {"t": "pt", "p": (0,0,0), "label": "V", "off": (-26, 0)},
        {"t": "vec", "a": (0,0,0), "b": (0,0,6), "label": "H", "off": (20, 0)},
        {"t": "vec", "a": (0,0,0), "b": (2.8,0,0), "label": "A", "off": (2, -14)},
        {"t": "vec", "a": (0,0,0), "b": (0,1.4,0), "label": "B", "off": (16, 2)},
    ])


# ===========================================================================
# Quadric / conic
# ===========================================================================

def do_rpc():
    V=(0,0,0)
    hero("rpc", "rpc", [*V, 0,0,6, 0,3,0, 1.5], [
        {"t": "pt", "p": V, "label": "V", "off": (-28, 6)},
        {"t": "vec", "a": V, "b": (0,0,6), "label": "H", "off": (20, 0)},
        {"t": "vec", "a": V, "b": (0,3,0), "label": "B", "off": (18, 2)},
        {"t": "dim", "a": V, "b": (1.5,0,0), "label": "r", "off": (0, 16)},
    ])
    variety("rpc", [
        ("tall narrow", "rpc", [0,0,0, 0,0,6, 0,3,0, 0.8]),
        ("shallow wide", "rpc", [0,0,0, 0,0,6, 0,1.2,0, 2.4]),
        ("stubby", "rpc", [0,0,0, 0,0,2.5, 0,3,0, 2.0]),
    ])


def do_rhc():
    V=(0,0,0)
    hero("rhc", "rhc", [*V, 0,0,6, 0,3,0, 1.5, 1.2], [
        {"t": "pt", "p": V, "label": "V", "off": (-28, 6)},
        {"t": "vec", "a": V, "b": (0,0,6), "label": "H", "off": (20, 0)},
        {"t": "vec", "a": V, "b": (0,3,0), "label": "B", "off": (18, 2)},
        {"t": "dim", "a": V, "b": (1.5,0,0), "label": "r", "off": (0, 16)},
    ])
    variety("rhc", [
        ("sharp  small c", "rhc", [0,0,0, 0,0,6, 0,3,0, 1.5, 0.4]),
        ("blunt  large c", "rhc", [0,0,0, 0,0,6, 0,3,0, 1.5, 3.0]),
        ("wide", "rhc", [0,0,0, 0,0,6, 0,2,0, 3.0, 1.2]),
    ])


def do_epa():
    V=(0,0,0)
    hero("epa", "epa", [*V, 0,0,6, 2.5,0,0, 1.5], [
        {"t": "pt", "p": V, "label": "V", "off": (-28, 4)},
        {"t": "vec", "a": V, "b": (0,0,6), "label": "H", "off": (20, 0)},
        {"t": "vec", "a": V, "b": (2.5,0,0), "label": "A", "off": (8, -14)},
    ])
    variety("epa", [
        ("bullet  r1=r2", "epa", [0,0,0, 0,0,6, 2,0,0, 2.0]),
        ("dish  short H", "epa", [0,0,0, 0,0,2, 3,0,0, 3.0]),
        ("elliptical base", "epa", [0,0,0, 0,0,6, 3,0,0, 1.2]),
    ])


def do_ehy():
    V=(0,0,0)
    hero("ehy", "ehy", [*V, 0,0,6, 2.5,0,0, 1.5, 1.0], [
        {"t": "pt", "p": V, "label": "V", "off": (-28, 4)},
        {"t": "vec", "a": V, "b": (0,0,6), "label": "H", "off": (20, 0)},
        {"t": "vec", "a": V, "b": (2.5,0,0), "label": "A", "off": (8, -14)},
    ])
    variety("ehy", [
        ("horn  small c", "ehy", [0,0,0, 0,0,6, 2,0,0, 2.0, 0.4]),
        ("near-cone  large c", "ehy", [0,0,0, 0,0,6, 2,0,0, 2.0, 4.0]),
        ("elliptical base", "ehy", [0,0,0, 0,0,6, 3,0,0, 1.2, 1.0]),
    ])


def do_hyp():
    V=(0,0,0)
    hero("hyp", "hyp", [*V, 0,0,6, 3,0,0, 1.6, 0.4], [
        {"t": "vec", "a": V, "b": (0,0,6), "label": "H", "off": (24, 0)},
        {"t": "vec", "a": V, "b": (3,0,0), "label": "A", "off": (0, 20)},
    ], az=35, el=15)
    # "cooling tower" removed: the highlight above is already the pinched form.
    variety("hyp", [
        ("near-cylinder  c=0.9", "hyp", [0,0,0, 0,0,6, 2.4,0,0, 2.4, 0.9]),
        ("elliptical section", "hyp", [0,0,0, 0,0,6, 3.6,0,0, 0.9, 0.55]),
    ], az=35, el=15, cols=2)


def do_part():
    V=(0,0,0)
    hero("part", "part", [*V, 0,0,6, 1.0, 2.0], [
        {"t": "pt", "p": V, "label": "V", "off": (-28, 4)},
        {"t": "vec", "a": V, "b": (0,0,6), "label": "H", "off": (22, 0)},
        {"t": "dim", "a": V, "b": (1.0,0,0), "label": "r_v", "off": (-6, 14)},
        {"t": "dim", "a": (0,0,6), "b": (2.0,0,6), "label": "r_h", "off": (0, 16)},
    ])
    variety("part", [
        ("sphere  H->0", "part", [0,0,0, 0,0,0.01, 2.5, 2.5]),
        ("capsule  r_v=r_h", "part", [0,0,0, 0,0,6, 1.5, 1.5]),
        ("lozenge  r_v!=r_h", "part", [0,0,0, 0,0,6, 0.6, 2.2]),
    ])


def do_hrt():
    V=(0,0,0)
    hero("hrt", "hrt", [*V, 3,0,0, 0,3,0, 0,0,3, 3.0], [
        {"t": "pt", "p": V, "label": "V", "off": (-34, -10)},
        {"t": "vec", "a": V, "b": (3,0,0), "label": "xdir", "off": (4, 20)},
        {"t": "vec", "a": V, "b": (0,3,0), "label": "ydir", "off": (20, 2)},
        {"t": "vec", "a": V, "b": (0,0,3), "label": "zdir", "off": (-18, -2)},
    ], az=0, el=20)
    variety("hrt", [
        ("d = 2  rounded", "hrt", [0,0,0, 3,0,0, 0,3,0, 0,0,3, 2.0]),
        ("d = 3  classic", "hrt", [0,0,0, 3,0,0, 0,3,0, 0,0,3, 3.0]),
        ("d = 4.5  pointed", "hrt", [0,0,0, 3,0,0, 0,3,0, 0,0,3, 4.5]),
    ], az=0, el=20)


# ===========================================================================
# Implicit
# ===========================================================================

def do_metaball():
    pts = [-2.8,-1.4,0, 2.3,  2.8,-1.4,0, 2.3,  0,2.8,0, 2.3]
    g = fresh("metaball")
    L.build(g, "metaball", [1, 1.0, 3] + pts)
    offs = [(-34, -10), (34, -10), (0, 28)]
    anns = [{"t": "pt", "p": (pts[4*i], pts[4*i+1], pts[4*i+2]),
             "label": "P%d" % (i+1), "off": offs[i]} for i in range(3)]
    L.render(g, "r", OUT("metaball"), az=20, el=60, anns=anns)
    print("hero metaball")
    variety("metaball", [
        ("far apart", "metaball", [1,1.0,2, -3,0,0,2.5, 3,0,0,2.5]),
        ("touching", "metaball", [1,1.0,2, -1.6,0,0,2.5, 1.6,0,0,2.5]),
        ("merged", "metaball", [1,1.0,2, -1.0,0,0,2.5, 1.0,0,0,2.5]),
    ], az=20, el=60)


# ===========================================================================
# Mesh & procedural
# ===========================================================================

def do_bot():
    verts = [1,0,0, -1,0,0, 0,1,0, 0,-1,0, 0,0,1.4, 0,0,-1.4]
    faces = [0,2,4, 2,1,4, 1,3,4, 3,0,4, 2,0,5, 1,2,5, 3,1,5, 0,3,5]
    g = fresh("bot")
    L.build(g, "bot", [6, 8, 2, 2] + verts + faces)
    L.render(g, "r", OUT("bot"), az=25, el=25, anns=[
        {"t": "pt", "p": (1,0,0), "label": "vertex", "off": (34, 0)},
        # point the "triangle" label at a face centroid, not a vertex
        {"t": "pt", "p": (1/3.0, 1/3.0, 1.4/3.0), "label": "triangle", "off": (40, -6)},
    ])
    print("hero bot")

    # Surface vs solid look identical from outside, so show cutaways: an
    # open-topped shell (surface mode -> hollow) beside a corner-cut solid.
    cv = [0,0,0, 4,0,0, 4,4,0, 0,4,0, 0,0,4, 4,0,4, 4,4,4, 0,4,4]
    top = [4,5,6, 4,6,7]
    walls = [0,2,1, 0,3,2, 0,1,5, 0,5,4, 3,7,6, 3,6,2, 0,4,7, 0,7,3, 1,2,6, 1,6,5]
    cells = []
    gs = fresh("bot_surf")
    L.build(gs, "bot", [8, 10, 1, 2] + cv + walls)      # mode 1 = surface, open top
    ps = scratch("bot_surf.png")
    L.render_cell(gs, "r", ps, size=380, caption="surface mode (open shell)",
                  az=45, el=35, fill=0.74)
    cells.append(ps)
    gd = fresh("bot_sol")
    cutter = [2.4,2.4,2.4, 5,2.4,2.4, 5,5,2.4, 2.4,5,2.4,
              2.4,2.4,5, 5,2.4,5, 5,5,5, 2.4,5,5]
    L.mged_batch(gd, [
        ["in", "cube", "bot", 8, 12, 2, 2] + cv + walls + top,
        ["in", "cut", "arb8"] + cutter,
        ["r", "r", "u", "cube", "-", "cut"],
        ["mater", "r", "plastic", FG[0], FG[1], FG[2], 0],
    ])
    pd = scratch("bot_sol.png")
    L.render_cell(gd, "r", pd, size=380, caption="solid mode (cut away)",
                  az=45, el=35, fill=0.74)
    cells.append(pd)
    L.montage(cells, VAR("bot"), cols=2)
    print("variety bot")


def do_ars():
    npts, ncrv = 6, 5
    rows = []
    for j in range(ncrv):
        z = j * 1.2
        rad = 2.2 * math.sin(math.pi * (j + 0.5) / ncrv) + 0.6
        row = []
        for i in range(npts):
            a = 2*math.pi*i/npts
            row += [rad*math.cos(a), rad*math.sin(a), z]
        rows.append(row)
    flat = [npts, ncrv] + [c for row in rows for c in row]
    hero("ars", "ars", flat, [
        {"t": "pt", "p": (rows[2][0], rows[2][1], 2.4), "label": "waterline pts",
         "off": (40, 0)},
    ], az=35, el=20)


def do_pipe():
    params = [3,
              0,0,0,    1.0, 2.0, 1.5,
              0,0,4,    1.0, 2.0, 1.5,
              4,0,4,    1.0, 2.0, 1.5]
    hero("pipe", "pipe", params, [
        {"t": "pt", "p": (0,0,0), "label": "pt 1", "off": (-30, 0)},
        {"t": "pt", "p": (0,0,4), "label": "pt 2 (bend)", "off": (-40, 10)},
        {"t": "pt", "p": (4,0,4), "label": "pt 3", "off": (28, 8)},
    ], az=35, el=20)
    variety("pipe", [
        ("hollow tube", "pipe", [2, 0,0,0, 1.2,2.4,1.3, 0,0,5, 1.2,2.4,1.3]),
        ("solid rod", "pipe", [2, 0,0,0, 0,2.0,1.1, 0,0,5, 0,2.0,1.1]),
        ("tapered", "pipe", [2, 0,0,0, 0,1.0,1.5, 0,0,5, 0,2.6,1.5]),
    ], az=35, el=20)


def do_pnts():
    coords = []
    n = 0
    for x in range(4):
        for y in range(4):
            for z in range(4):
                coords += [x*1.2, y*1.2, z*1.2]
                n += 1
    g = fresh("pnts")
    L.build(g, "pnts", ["no", n, "no", "no", "no", 0.18] + coords)
    L.render(g, "r", OUT("pnts"), az=35, el=25, anns=[])
    print("hero pnts")


def do_extrude():
    g = fresh("extrude")
    L.mged_batch(g, [
        ["put", "sk", "sketch", "V", "{0 0 0}", "A", "{1 0 0}", "B", "{0 1 0}",
         "VL", "{ {0 0} {4 0} {4 1.5} {1.5 1.5} {1.5 4} {0 4} }",
         "SL", "{ {line S 0 E 1} {line S 1 E 2} {line S 2 E 3} "
               "{line S 3 E 4} {line S 4 E 5} {line S 5 E 0} }"],
        ["in", "s", "extrude", 0,0,0, 0,0,4, 1,0,0, 0,1,0, "sk"],
        ["r", "r", "u", "s"],
        ["mater", "r", "plastic", FG[0], FG[1], FG[2], 0],
    ])
    L.render(g, "r", OUT("extrude"), az=35, el=25, anns=[
        {"t": "pt", "p": (0,0,0), "label": "V", "off": (-28, 8)},
        {"t": "vec", "a": (0,0,0), "b": (0,0,4), "label": "H", "off": (22, 0)},
    ])
    print("hero extrude")


def do_revolve():
    g = fresh("revolve")
    L.mged_batch(g, [
        ["put", "pk", "sketch", "V", "{0 0 0}", "A", "{1 0 0}", "B", "{0 1 0}",
         "VL", "{ {2 0} {3.5 0} {3 2} {3.5 4} {2 4} }",
         "SL", "{ {line S 0 E 1} {line S 1 E 2} {line S 2 E 3} "
               "{line S 3 E 4} {line S 4 E 0} }"],
        ["in", "s", "revolve", 0,0,0, 0,0,1, 1,0,0, 300, "pk"],
        ["r", "r", "u", "s"],
        ["mater", "r", "plastic", FG[0], FG[1], FG[2], 0],
    ])
    L.render(g, "r", OUT("revolve"), az=35, el=25, anns=[
        {"t": "vec", "a": (0,0,0), "b": (0,0,4), "label": "axis", "off": (-26, 4)},
    ])
    print("hero revolve")


def do_dsp():
    W = Ht = 48
    data = bytearray()
    for y in range(Ht):
        for x in range(W):
            dx = (x-W/2)/(W/4); dy = (y-Ht/2)/(Ht/4)
            h = math.exp(-(dx*dx+dy*dy)) \
                + 0.4*math.exp(-(((x-10)/6.0)**2 + ((y-34)/6.0)**2))
            data += struct.pack(">H", int(h*45000))
    df = put_data("dsp.data", data)
    g = fresh("dsp")
    L.build(g, "dsp", ["f", df, W, Ht, 1, "a", 1.0, 0.00042])
    L.render(g, "r", OUT("dsp"), az=45, el=35, anns=[])
    print("hero dsp")


def do_ebm():
    W = Ht = 40
    data = bytearray()
    for y in range(Ht):
        for x in range(W):
            dx = x-W/2; dy = y-Ht/2
            r = math.hypot(dx, dy)
            data.append(255 if 7 < r < 15 else 0)
    df = put_data("ebm.data", data)
    g = fresh("ebm")
    L.build(g, "ebm", ["f", df, W, Ht, 14])
    L.render(g, "r", OUT("ebm"), az=35, el=45, anns=[])
    print("hero ebm")


def do_vol():
    W = Ht = D = 24
    data = bytearray()
    for z in range(D):
        for y in range(Ht):
            for x in range(W):
                dx=x-W/2; dy=y-Ht/2; dz=z-D/2
                inside = (dx*dx+dy*dy+dz*dz) < 100 and not (abs(dx)<3 and abs(dy)<3)
                data.append(200 if inside else 0)
    df = put_data("vol.data", data)
    g = fresh("vol")
    L.build(g, "vol", ["f", df, W, Ht, D, 128, 255, 0.5, 0.5, 0.5])
    L.render(g, "r", OUT("vol"), az=35, el=30, anns=[])
    print("hero vol")


def do_nmg():
    # A real n-Manifold Geometry solid (facetize -n).  NMG ray tracing uses a
    # global hitmiss freelist that is not thread-safe, so shoot single-threaded
    # (-P1) to avoid the race; the shaded result is a genuine NMG.
    g = fresh("nmg")
    L.mged_batch(g, [
        ["in", "src", "ell", 0,0,0, 3,0,0, 0,2,0, 0,0,2],
        ["facetize", "-n", "s", "src"],
        ["r", "r", "u", "s"],
        ["mater", "r", "plastic", FG[0], FG[1], FG[2], 0],
    ])
    L.render(g, "r", OUT("nmg"), az=35, el=25, anns=[], extra=["-P1"], hyper=2)
    print("hero nmg")


def do_brep():
    g = fresh("brep")
    L.mged_batch(g, [
        ["in", "src", "ell", 0,0,0, 3,0,0, 0,2,0, 0,0,2],
        ["brep", "src", "brep", "s"],
        ["r", "r", "u", "s"],
        ["mater", "r", "plastic", FG[0], FG[1], FG[2], 0],
    ])
    L.render(g, "r", OUT("brep"), az=35, el=25, anns=[])
    print("hero brep")


def do_cline():
    g = fresh("cline")
    L.mged_batch(g, [
        ["put", "c", "cline", "V", "{0 0 0}", "H", "{0 0 8}", "R", "1.5", "T", "0.4"],
        ["r", "r", "u", "c"],
        ["mater", "r", "plastic", FG[0], FG[1], FG[2], 0],
    ])
    L.render(g, "r", OUT("cline"), az=35, el=20, anns=[
        {"t": "pt", "p": (0,0,0), "label": "V", "off": (-28, 6)},
        {"t": "vec", "a": (0,0,0), "b": (0,0,8), "label": "H", "off": (20, 0)},
        {"t": "dim", "a": (0,0,0), "b": (1.5,0,0), "label": "R", "off": (0, 16)},
    ])
    print("hero cline")


def do_submodel():
    # A real submodel: instance the treetop "widget" from a separate .g file as
    # a single primitive.  (The relative-path resolver bug is fixed in librt.)
    parts = fresh("parts")
    partsname = os.path.basename(parts)         # referenced relative to cwd
    L.mged_batch(parts, [
        ["in", "hub", "sph", 0,0,0, 1.6],
        ["in", "arm1", "rcc", 0,0,0, 4,0,0, 0.7],
        ["in", "arm2", "rcc", 0,0,0, 0,4,0, 0.7],
        ["in", "arm3", "rcc", 0,0,0, 0,0,4, 0.7],
        ["in", "cap1", "sph", 4,0,0, 1.0],
        ["in", "cap2", "sph", 0,4,0, 1.0],
        ["in", "cap3", "sph", 0,0,4, 1.0],
        ["r", "widget", "u", "hub", "u", "arm1", "u", "arm2", "u", "arm3",
         "u", "cap1", "u", "cap2", "u", "cap3"],
        ["mater", "widget", "plastic", FG[0], FG[1], FG[2], 0],
    ])
    g = fresh("submodel")
    L.mged_batch(g, [
        ["in", "sm", "submodel", "widget", "0", partsname],
        ["r", "r", "u", "sm"],
        ["mater", "r", "plastic", FG[0], FG[1], FG[2], 0],
    ])
    L.render(g, "r", OUT("submodel"), az=35, el=25, anns=[])
    print("hero submodel")


# ===========================================================================
# Boolean combinations
# ===========================================================================

def do_comb():
    spheres = [["in", "a", "sph", -1,0,0, 2.6], ["in", "b", "sph", 1.4,0,0, 2.0]]
    ops = [("u", "union   a u b"),
           ("-", "difference   a - b"),
           ("+", "intersection   a + b")]
    # One shared view (from the union, the largest result) so all three panels
    # are at the same scale and the spheres line up across them.
    gu = fresh("comb_u")
    L.mged_batch(gu, spheres + [["r", "r", "u", "a", "u", "b"],
                                ["mater", "r", "plastic", FG[0], FG[1], FG[2], 0]])
    orient, autosize, model, frac = L.probe(gu, "r", az=35, el=20)
    C = ((model[0]+model[1])/2, (model[2]+model[3])/2, (model[4]+model[5])/2)
    V = autosize * frac / 0.72
    view = (orient, L.eye_for(C, 35, 20, V*3), V)
    cells = []
    for i, (op, cap) in enumerate(ops):
        g = fresh("comb_%d" % i)
        L.mged_batch(g, spheres + [["r", "r", "u", "a", op, "b"],
                                   ["mater", "r", "plastic", FG[0], FG[1], FG[2], 0]])
        png = scratch("comb_%d.png" % i)
        L.render_cell(g, "r", png, size=380, caption=cap, view=view)
        cells.append(png)
    L.montage(cells, VAR("comb"), cols=3)
    print("comb done")


# ===========================================================================
# registry
# ===========================================================================

OBJECTS = {
    "arb8": do_arb8, "arbn": do_arbn, "half": do_half,
    "ell": do_ell, "sph": do_sph, "superell": do_superell,
    "tor": do_tor, "eto": do_eto,
    "tgc": do_tgc, "rcc": do_rcc, "trc": do_trc, "rec": do_rec,
    "rpc": do_rpc, "rhc": do_rhc, "epa": do_epa, "ehy": do_ehy, "hyp": do_hyp,
    "part": do_part, "hrt": do_hrt, "metaball": do_metaball,
    "bot": do_bot, "ars": do_ars, "pipe": do_pipe, "pnts": do_pnts,
    "extrude": do_extrude, "revolve": do_revolve,
    "dsp": do_dsp, "ebm": do_ebm, "vol": do_vol,
    "nmg": do_nmg, "brep": do_brep, "cline": do_cline, "submodel": do_submodel,
    "comb": do_comb,
}


def main():
    keys = sys.argv[1:] or list(OBJECTS.keys())
    for k in keys:
        if k not in OBJECTS:
            print("unknown:", k)
            continue
        try:
            OBJECTS[k]()
        except Exception as e:
            print("FAILED", k, "->", e)


if __name__ == "__main__":
    main()
