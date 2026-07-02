/*                      S C A D _ G E O M . C
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 *
 */
/** @file scad_geom.c
 *
 * Geometry back-end for the OpenSCAD importer: turns the resolved CSG
 * geometry tree (struct scad_geom, produced by scad_eval.c) into native
 * BRL-CAD objects.
 *
 * Mapping strategy (CSG-native, favouring round-trippable primitives):
 *
 *   cube                 -> arb8 (mk_rpp)
 *   sphere               -> ell  (mk_sph)
 *   cylinder (smooth)    -> tgc/trc/rcc
 *   cylinder ($fn n-gon) -> bot prism/frustum
 *   polyhedron           -> bot (solid)
 *   linear_extrude       -> sketch + extrusion (bot when twisted/scaled)
 *   rotate_extrude       -> sketch + revolve (bot when axis-crossing)
 *   hull                 -> bot from the 3D convex hull of child vertices
 *   translate/rotate/... -> matrices on combination member arcs
 *   union/difference/intersection -> boolean combinations
 *   color                -> combination color attribute
 *
 * Each top-level object becomes its own region so it raytraces and can
 * carry an independent color; interior boolean groups are plain combs.
 * Fidelity losses are catalogued in TODO.scad.
 */

#include "common.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "bu/log.h"
#include "bu/malloc.h"
#include "bu/vls.h"
#include "bu/magic.h"
#include "bn/mat.h"
#include "vmath.h"
#include "rt/geom.h"
#include "raytrace.h"
#include "rt/wdb.h"
#include "bg/chull.h"
#include "wdb.h"

#include "scad.h"

#ifndef M_PI
#  define M_PI 3.14159265358979323846
#endif


/* ------------------------------------------------------------------ */
/* geometry tree helpers                                              */
/* ------------------------------------------------------------------ */

struct scad_geom *
scad_geom_new(enum scad_geom_kind kind)
{
    struct scad_geom *g;
    BU_ALLOC(g, struct scad_geom);
    memset(g, 0, sizeof(*g));
    g->kind = kind;
    MAT_IDN(g->mat);
    g->rgba[0] = g->rgba[1] = g->rgba[2] = g->rgba[3] = 1.0;
    return g;
}

void
scad_geom_add(struct scad_geom *parent, struct scad_geom *child)
{
    if (!parent || !child) return;
    if (parent->nkids == parent->kcap) {
	parent->kcap = parent->kcap ? parent->kcap * 2 : 4;
	parent->kids = (struct scad_geom **)bu_realloc(parent->kids,
		       parent->kcap * sizeof(struct scad_geom *), "scad kids");
    }
    parent->kids[parent->nkids++] = child;
}

void
scad_geom_free(struct scad_geom *g)
{
    size_t i;
    if (!g) return;
    for (i = 0; i < g->nkids; i++) scad_geom_free(g->kids[i]);
    if (g->kids) bu_free(g->kids, "scad kids");
    if (g->pts) bu_free(g->pts, "scad pts");
    if (g->fidx) bu_free(g->fidx, "scad fidx");
    if (g->flen) bu_free(g->flen, "scad flen");
    bu_free(g, "scad geom");
}


/* OpenSCAD get_fragments_from_r */
int
scad_fragments(double r, int fn, double fa, double fs)
{
    if (r < 1e-6) return 3;
    if (fn > 0) return fn >= 3 ? fn : 3;
    if (fa <= 0) fa = 12;
    if (fs <= 0) fs = 2;
    {
	double a = 360.0 / fa;
	double b = r * 2.0 * M_PI / fs;
	double n = ceil(a < b ? (a < 5 ? 5 : a) : (b < 5 ? 5 : b));
	return (int)n;
    }
}


/* ------------------------------------------------------------------ */
/* small dynamic mesh builder                                         */
/* ------------------------------------------------------------------ */

struct mesh {
    double *v;			/* nv*3 */
    size_t nv, vcap;
    int *f;			/* nf*3 */
    size_t nf, fcap;
};

static int
mesh_vert(struct mesh *m, double x, double y, double z)
{
    if (m->nv == m->vcap) {
	m->vcap = m->vcap ? m->vcap * 2 : 32;
	m->v = (double *)bu_realloc(m->v, m->vcap * 3 * sizeof(double), "mesh v");
    }
    m->v[m->nv * 3 + 0] = x;
    m->v[m->nv * 3 + 1] = y;
    m->v[m->nv * 3 + 2] = z;
    return (int)m->nv++;
}

static void
mesh_tri(struct mesh *m, int a, int b, int c)
{
    if (a == b || b == c || a == c) return;	/* skip degenerate */
    if (m->nf == m->fcap) {
	m->fcap = m->fcap ? m->fcap * 2 : 32;
	m->f = (int *)bu_realloc(m->f, m->fcap * 3 * sizeof(int), "mesh f");
    }
    m->f[m->nf * 3 + 0] = a;
    m->f[m->nf * 3 + 1] = b;
    m->f[m->nf * 3 + 2] = c;
    m->nf++;
}

static void
mesh_quad(struct mesh *m, int a, int b, int c, int d)
{
    mesh_tri(m, a, b, c);
    mesh_tri(m, a, c, d);
}

static void
mesh_free(struct mesh *m)
{
    if (m->v) bu_free(m->v, "mesh v");
    if (m->f) bu_free(m->f, "mesh f");
    memset(m, 0, sizeof(*m));
}

/* emit a mesh as a solid BOT; returns 1 if written */
static int
mesh_to_bot(struct rt_wdb *wdbp, const char *name, struct mesh *m)
{
    int ret;
    if (m->nv < 4 || m->nf < 4) return 0;
    ret = mk_bot(wdbp, name, RT_BOT_SOLID, RT_BOT_UNORIENTED, 0,
		 m->nv, m->nf, m->v, m->f, NULL, NULL);
    return ret >= 0;
}


/* ------------------------------------------------------------------ */
/* naming                                                             */
/* ------------------------------------------------------------------ */

static void
uniq_name(struct scad_state *st, struct bu_vls *out, const char *prefix, const char *suffix)
{
    bu_vls_sprintf(out, "%s%lu%s", prefix, ++st->name_count, suffix);
}


/* ------------------------------------------------------------------ */
/* 2D profile collection (for extrusions)                             */
/* ------------------------------------------------------------------ */

struct scad_loop {
    double *xy;			/* n*2 */
    size_t n;
};

struct looplist {
    struct scad_loop *l;
    size_t n, cap;
};

static struct scad_loop *
looplist_add(struct looplist *ll, size_t npts)
{
    struct scad_loop *lp;
    if (ll->n == ll->cap) {
	ll->cap = ll->cap ? ll->cap * 2 : 4;
	ll->l = (struct scad_loop *)bu_realloc(ll->l, ll->cap * sizeof(struct scad_loop), "looplist");
    }
    lp = &ll->l[ll->n++];
    lp->n = npts;
    lp->xy = (double *)bu_calloc(npts ? npts * 2 : 1, sizeof(double), "loop xy");
    return lp;
}

static void
looplist_free(struct looplist *ll)
{
    size_t i;
    for (i = 0; i < ll->n; i++)
	if (ll->l[i].xy) bu_free(ll->l[i].xy, "loop xy");
    if (ll->l) bu_free(ll->l, "looplist");
    memset(ll, 0, sizeof(*ll));
}

/* transform a 2D point by the xy part of a 4x4 and store into loop */
static void
loop_set(struct scad_loop *lp, size_t i, const mat_t M, double x, double y)
{
    point_t in, out;
    VSET(in, x, y, 0.0);
    MAT4X3PNT(out, M, in);
    lp->xy[i * 2 + 0] = out[0];
    lp->xy[i * 2 + 1] = out[1];
}

static void
collect_profiles(struct scad_state *st, struct scad_geom *g, const mat_t M, struct looplist *ll)
{
    size_t i;
    if (!g) return;

    switch (g->kind) {
	case G_PRIM:
	    if (!g->is2d) return;
	    if (g->prim == PR_CIRCLE) {
		int n = scad_fragments(g->r1, g->fn, g->fa, g->fs);
		struct scad_loop *lp;
		int k;
		if (g->r1 <= 0 || n < 3) return;
		lp = looplist_add(ll, (size_t)n);
		for (k = 0; k < n; k++) {
		    double a = 2.0 * M_PI * k / n;
		    loop_set(lp, k, M, g->r1 * cos(a), g->r1 * sin(a));
		}
	    } else if (g->prim == PR_SQUARE) {
		double sx = g->size[0], sy = g->size[1];
		double ox = g->center ? -sx / 2 : 0;
		double oy = g->center ? -sy / 2 : 0;
		struct scad_loop *lp;
		if (sx <= 0 || sy <= 0) return;
		lp = looplist_add(ll, 4);
		loop_set(lp, 0, M, ox, oy);
		loop_set(lp, 1, M, ox + sx, oy);
		loop_set(lp, 2, M, ox + sx, oy + sy);
		loop_set(lp, 3, M, ox, oy + sy);
	    } else if (g->prim == PR_POLYGON) {
		/* every path becomes a curve; nested paths become holes */
		if (g->nfaces >= 1 && g->flen) {
		    size_t fi, k = 0;
		    for (fi = 0; fi < g->nfaces; fi++) {
			size_t cnt = g->flen[fi];
			struct scad_loop *lp = looplist_add(ll, cnt);
			for (i = 0; i < cnt; i++) {
			    int vi = g->fidx[k + i];
			    if (vi < 0 || (size_t)vi >= g->npts) continue;
			    loop_set(lp, i, M, g->pts[vi * 3 + 0], g->pts[vi * 3 + 1]);
			}
			k += cnt;
		    }
		} else if (g->npts >= 3) {
		    struct scad_loop *lp = looplist_add(ll, g->npts);
		    for (i = 0; i < g->npts; i++)
			loop_set(lp, i, M, g->pts[i * 3 + 0], g->pts[i * 3 + 1]);
		}
	    }
	    return;
	case G_TRANSFORM: {
	    mat_t M2;
	    bn_mat_mul(M2, M, g->mat);
	    for (i = 0; i < g->nkids; i++) collect_profiles(st, g->kids[i], M2, ll);
	    return;
	}
	case G_DIFFERENCE:
	case G_INTERSECTION:
	    /* 2D boolean profiles are not supported; keep the outer child */
	    if (g->nkids >= 1) {
		if (g->nkids > 1)
		    bu_log("scad: 2D boolean inside extrude not supported; using first child\n");
		collect_profiles(st, g->kids[0], M, ll);
	    }
	    return;
	default:
	    for (i = 0; i < g->nkids; i++) collect_profiles(st, g->kids[i], M, ll);
	    return;
    }
}


/* ------------------------------------------------------------------ */
/* primitive + operation emission                                     */
/* ------------------------------------------------------------------ */

/* triangulate a polygon loop (fan from centroid) into the mesh at a
 * given z, returning the created vertex indices via ring[] */
static void
cap_fan(struct mesh *m, const double *xy, size_t n, double z, int *ring, int top)
{
    size_t i;
    double cx = 0, cy = 0;
    int c;
    for (i = 0; i < n; i++) { cx += xy[i * 2]; cy += xy[i * 2 + 1]; }
    cx /= n; cy /= n;
    c = mesh_vert(m, cx, cy, z);
    for (i = 0; i < n; i++) {
	int a = ring[i];
	int b = ring[(i + 1) % n];
	if (top) mesh_tri(m, c, a, b);
	else mesh_tri(m, c, b, a);
    }
}

/* build a linear extrusion of one loop into a mesh */
static void
build_linext(struct mesh *m, const struct scad_loop *lp, struct scad_geom *g)
{
    size_t i;
    int s, slices;
    double z0 = g->ext_center ? -g->ext_height / 2 : 0;
    double z1 = g->ext_center ?  g->ext_height / 2 : g->ext_height;
    int *prev = NULL;
    int *bottom = NULL, *top = NULL;

    if (lp->n < 3 || g->ext_height <= 0) return;

    slices = g->ext_slices;
    if (slices <= 0)
	slices = (g->ext_twist != 0.0 || g->ext_scale[0] != 1.0 || g->ext_scale[1] != 1.0)
		 ? (g->fn > 0 ? g->fn : (int)ceil(fabs(g->ext_twist) / 12.0) + 2) : 1;
    if (slices < 1) slices = 1;

    for (s = 0; s <= slices; s++) {
	double t = (double)s / slices;
	double z = z0 + t * (z1 - z0);
	double sxf = 1.0 + t * (g->ext_scale[0] - 1.0);
	double syf = 1.0 + t * (g->ext_scale[1] - 1.0);
	double rot = (-g->ext_twist * t) * DEG2RAD;	/* twist is left-handed */
	double cr = cos(rot), sr = sin(rot);
	int *ringidx = (int *)bu_calloc(lp->n, sizeof(int), "linext ringidx");
	for (i = 0; i < lp->n; i++) {
	    double x = lp->xy[i * 2] * sxf;
	    double y = lp->xy[i * 2 + 1] * syf;
	    double xr = x * cr - y * sr;
	    double yr = x * sr + y * cr;
	    ringidx[i] = mesh_vert(m, xr, yr, z);
	}
	if (s == 0) bottom = ringidx;
	if (s == slices) top = ringidx;
	if (prev) {
	    for (i = 0; i < lp->n; i++) {
		size_t j = (i + 1) % lp->n;
		mesh_quad(m, prev[i], prev[j], ringidx[j], ringidx[i]);
	    }
	    if (prev != bottom) bu_free(prev, "linext ring");
	}
	prev = ringidx;
    }

    /* caps */
    if (bottom) cap_fan(m, lp->xy, lp->n, z0, bottom, 0);
    if (top) {
	/* recompute the transformed top outline for the cap centroid */
	double sxf = g->ext_scale[0], syf = g->ext_scale[1];
	double rot = (-g->ext_twist) * DEG2RAD;
	double cr = cos(rot), sr = sin(rot);
	double *txy = (double *)bu_calloc(lp->n * 2, sizeof(double), "top xy");
	for (i = 0; i < lp->n; i++) {
	    double x = lp->xy[i * 2] * sxf;
	    double y = lp->xy[i * 2 + 1] * syf;
	    txy[i * 2] = x * cr - y * sr;
	    txy[i * 2 + 1] = x * sr + y * cr;
	}
	cap_fan(m, txy, lp->n, z1, top, 1);
	bu_free(txy, "top xy");
    }
    if (bottom && bottom != top) bu_free(bottom, "linext ring");
    if (top) bu_free(top, "linext ring");
}

/* build a rotational extrusion of one closed loop into a mesh */
static void
build_rotext(struct mesh *m, const struct scad_loop *lp, struct scad_geom *g)
{
    size_t i;
    int j, nseg, nring;
    double maxr = 0, ang = g->ext_angle == 0 ? 360.0 : g->ext_angle;
    int full = NEAR_EQUAL(fabs(ang), 360.0, 1e-6);
    int **rings;

    if (lp->n < 3) return;
    for (i = 0; i < lp->n; i++)
	if (lp->xy[i * 2] > maxr) maxr = lp->xy[i * 2];
    nseg = scad_fragments(maxr, g->fn, g->fa, g->fs);
    if (nseg < 3) nseg = 3;
    if (!full) nseg = (int)ceil(nseg * fabs(ang) / 360.0) + 1;
    nring = full ? nseg : nseg + 1;

    rings = (int **)bu_calloc(nring, sizeof(int *), "rot rings");
    for (j = 0; j < nring; j++) {
	double a = ang * ((double)j / nseg) * DEG2RAD;
	double c = cos(a), s = sin(a);
	rings[j] = (int *)bu_calloc(lp->n, sizeof(int), "rot ring");
	for (i = 0; i < lp->n; i++) {
	    double r = lp->xy[i * 2];		/* x -> radius */
	    double zz = lp->xy[i * 2 + 1];	/* y -> z */
	    if (r < 0) r = 0;
	    rings[j][i] = mesh_vert(m, r * c, r * s, zz);
	}
    }
    for (j = 0; j < (full ? nseg : nseg); j++) {
	int jn = full ? ((j + 1) % nseg) : (j + 1);
	if (!full && jn >= nring) break;
	for (i = 0; i < lp->n; i++) {
	    size_t in = (i + 1) % lp->n;
	    mesh_quad(m, rings[j][i], rings[j][in], rings[jn][in], rings[jn][i]);
	}
    }
    if (!full) {
	/* planar end caps at the start and end angle */
	int *r0 = rings[0], *r1 = rings[nring - 1];
	int c0, c1;
	double cx = 0, cy = 0, cz = 0;
	for (i = 0; i < lp->n; i++) { cx += lp->xy[i * 2]; cz += lp->xy[i * 2 + 1]; }
	cx /= lp->n; cz /= lp->n; cy = 0;
	c0 = mesh_vert(m, cx, cy, cz);
	{
	    double a = ang * DEG2RAD;
	    c1 = mesh_vert(m, cx * cos(a), cx * sin(a), cz);
	}
	for (i = 0; i < lp->n; i++) {
	    size_t in = (i + 1) % lp->n;
	    mesh_tri(m, c0, r0[in], r0[i]);
	    mesh_tri(m, c1, r1[i], r1[in]);
	}
    }
    for (j = 0; j < nring; j++) bu_free(rings[j], "rot ring");
    bu_free(rings, "rot rings");
}

/* collect world-space sample vertices of a subtree (for hull) */
static void
collect_verts(struct scad_geom *g, const mat_t M, point_t **pts, int *n, int *cap)
{
    size_t i;
#define PUSH(px, py, pz) do { \
    point_t _in, _out; VSET(_in, px, py, pz); MAT4X3PNT(_out, M, _in); \
    if (*n == *cap) { *cap = *cap ? *cap * 2 : 64; \
	*pts = (point_t *)bu_realloc(*pts, (size_t)(*cap) * sizeof(point_t), "hull pts"); } \
    VMOVE((*pts)[*n], _out); (*n)++; \
} while (0)

    if (!g) return;
    switch (g->kind) {
	case G_PRIM: {
	    if (g->prim == PR_CUBE) {
		double sx = g->size[0], sy = g->size[1], sz = g->size[2];
		double ox = g->center ? -sx / 2 : 0, oy = g->center ? -sy / 2 : 0, oz = g->center ? -sz / 2 : 0;
		int a, b, c;
		for (a = 0; a < 2; a++) for (b = 0; b < 2; b++) for (c = 0; c < 2; c++)
		    PUSH(ox + a * sx, oy + b * sy, oz + c * sz);
	    } else if (g->prim == PR_SPHERE) {
		int la, lo, N = 8;
		for (la = 0; la <= N; la++) {
		    double th = M_PI * la / N;
		    for (lo = 0; lo < N; lo++) {
			double ph = 2 * M_PI * lo / N;
			PUSH(g->r1 * sin(th) * cos(ph), g->r1 * sin(th) * sin(ph), g->r1 * cos(th));
		    }
		}
	    } else if (g->prim == PR_CYLINDER) {
		int k, N = 16;
		double z0 = g->center ? -g->h / 2 : 0, z1 = g->center ? g->h / 2 : g->h;
		for (k = 0; k < N; k++) {
		    double a = 2 * M_PI * k / N;
		    PUSH(g->r1 * cos(a), g->r1 * sin(a), z0);
		    PUSH(g->r2 * cos(a), g->r2 * sin(a), z1);
		}
	    } else if (g->prim == PR_POLYHEDRON || g->prim == PR_POLYGON) {
		for (i = 0; i < g->npts; i++)
		    PUSH(g->pts[i * 3], g->pts[i * 3 + 1], g->pts[i * 3 + 2]);
	    }
	    return;
	}
	case G_TRANSFORM: {
	    mat_t M2;
	    bn_mat_mul(M2, M, g->mat);
	    for (i = 0; i < g->nkids; i++) collect_verts(g->kids[i], M2, pts, n, cap);
	    return;
	}
	default:
	    for (i = 0; i < g->nkids; i++) collect_verts(g->kids[i], M, pts, n, cap);
	    return;
    }
#undef PUSH
}


/* create a single BRL-CAD solid from a primitive node; name -> out */
static int
emit_prim(struct scad_state *st, struct scad_geom *g, struct bu_vls *out)
{
    struct rt_wdb *wdbp = st->wdbp;
    int facet;

    if (g->is2d) {
	bu_log("scad: 2D object outside an extrude cannot be represented; skipped\n");
	st->dropped++;
	return 0;
    }

    switch (g->prim) {
	case PR_CUBE: {
	    point_t mn, mx;
	    double sx = g->size[0], sy = g->size[1], sz = g->size[2];
	    if (sx <= 0 || sy <= 0 || sz <= 0) return 0;
	    if (g->center) {
		VSET(mn, -sx / 2, -sy / 2, -sz / 2);
		VSET(mx, sx / 2, sy / 2, sz / 2);
	    } else {
		VSET(mn, 0, 0, 0);
		VSET(mx, sx, sy, sz);
	    }
	    uniq_name(st, out, "cube", ".s");
	    return mk_rpp(wdbp, bu_vls_cstr(out), mn, mx) >= 0;
	}
	case PR_SPHERE: {
	    point_t c = VINIT_ZERO;
	    if (g->r1 <= 0) return 0;
	    if (st->opts && st->opts->facetize) {
		struct mesh m;
		int la, lo, N = scad_fragments(g->r1, g->fn, g->fa, g->fs);
		memset(&m, 0, sizeof(m));
		if (N < 3) N = 3;
		/* simple lat/long tessellation */
		{
		    int *row = NULL, *prev = NULL, top, bot;
		    top = mesh_vert(&m, 0, 0, g->r1);
		    bot = mesh_vert(&m, 0, 0, -g->r1);
		    for (la = 1; la < N; la++) {
			double th = M_PI * la / N;
			row = (int *)bu_calloc(N, sizeof(int), "sph row");
			for (lo = 0; lo < N; lo++) {
			    double ph = 2 * M_PI * lo / N;
			    row[lo] = mesh_vert(&m, g->r1 * sin(th) * cos(ph),
						g->r1 * sin(th) * sin(ph), g->r1 * cos(th));
			}
			if (la == 1)
			    for (lo = 0; lo < N; lo++)
				mesh_tri(&m, top, row[lo], row[(lo + 1) % N]);
			if (prev)
			    for (lo = 0; lo < N; lo++)
				mesh_quad(&m, prev[lo], prev[(lo + 1) % N],
					  row[(lo + 1) % N], row[lo]);
			if (prev) bu_free(prev, "sph row");
			prev = row;
		    }
		    if (prev) {
			for (lo = 0; lo < N; lo++)
			    mesh_tri(&m, bot, prev[(lo + 1) % N], prev[lo]);
			bu_free(prev, "sph row");
		    }
		}
		uniq_name(st, out, "sphere", ".s");
		{ int ok = mesh_to_bot(wdbp, bu_vls_cstr(out), &m); mesh_free(&m); return ok; }
	    }
	    uniq_name(st, out, "sphere", ".s");
	    return mk_sph(wdbp, bu_vls_cstr(out), c, g->r1) >= 0;
	}
	case PR_CYLINDER: {
	    point_t base;
	    vect_t h;
	    int n = g->fn;
	    double z0 = g->center ? -g->h / 2 : 0;
	    if (g->h <= 0 || (g->r1 <= 0 && g->r2 <= 0)) return 0;
	    facet = (st->opts && st->opts->facetize) ||
		    (n >= 3 && n <= (st->opts ? st->opts->facet_max : 12));
	    if (facet) {
		struct mesh m;
		double r1 = g->r1, r2 = g->r2, z1 = z0 + g->h;
		int nn = (n >= 3) ? n : scad_fragments(r1 > r2 ? r1 : r2, 0, g->fa, g->fs);
		int k, *bot, *top, cb, ct;
		memset(&m, 0, sizeof(m));
		bot = (int *)bu_calloc(nn, sizeof(int), "cyl bot");
		top = (int *)bu_calloc(nn, sizeof(int), "cyl top");
		for (k = 0; k < nn; k++) {
		    double a = 2 * M_PI * k / nn;
		    bot[k] = mesh_vert(&m, r1 * cos(a), r1 * sin(a), z0);
		    top[k] = mesh_vert(&m, r2 * cos(a), r2 * sin(a), z1);
		}
		for (k = 0; k < nn; k++) {
		    int kn = (k + 1) % nn;
		    mesh_quad(&m, bot[k], bot[kn], top[kn], top[k]);
		}
		cb = mesh_vert(&m, 0, 0, z0);
		ct = mesh_vert(&m, 0, 0, z1);
		for (k = 0; k < nn; k++) {
		    int kn = (k + 1) % nn;
		    mesh_tri(&m, cb, bot[kn], bot[k]);
		    mesh_tri(&m, ct, top[k], top[kn]);
		}
		bu_free(bot, "cyl bot"); bu_free(top, "cyl top");
		uniq_name(st, out, "cyl", ".s");
		{ int ok = mesh_to_bot(wdbp, bu_vls_cstr(out), &m); mesh_free(&m); return ok; }
	    }
	    VSET(base, 0, 0, z0);
	    VSET(h, 0, 0, g->h);
	    uniq_name(st, out, "cyl", ".s");
	    if (NEAR_EQUAL(g->r1, g->r2, 1e-9))
		return mk_rcc(wdbp, bu_vls_cstr(out), base, h, g->r1) >= 0;
	    return mk_trc_h(wdbp, bu_vls_cstr(out), base, h, g->r1, g->r2) >= 0;
	}
	case PR_POLYHEDRON: {
	    struct mesh m;
	    size_t fi, k = 0;
	    memset(&m, 0, sizeof(m));
	    for (fi = 0; fi < g->npts; fi++)
		mesh_vert(&m, g->pts[fi * 3], g->pts[fi * 3 + 1], g->pts[fi * 3 + 2]);
	    for (fi = 0; fi < g->nfaces; fi++) {
		size_t len = g->flen[fi];
		size_t j;
		if (len >= 3) {
		    int v0 = g->fidx[k];
		    for (j = 1; j + 1 < len; j++) {
			int v1 = g->fidx[k + j], v2 = g->fidx[k + j + 1];
			/* drop faces with out-of-range indices (a malformed
			 * .scad must not produce a BoT that reads OOB) */
			if (v0 < 0 || (size_t)v0 >= g->npts ||
			    v1 < 0 || (size_t)v1 >= g->npts ||
			    v2 < 0 || (size_t)v2 >= g->npts)
			    continue;
			mesh_tri(&m, v0, v1, v2);
		    }
		}
		k += len;
	    }
	    /* vertices already appended; reuse mk_bot with our arrays */
	    uniq_name(st, out, "poly", ".s");
	    {
		int ok = 0;
		if (m.nv >= 4 && m.nf >= 4)
		    ok = mk_bot(wdbp, bu_vls_cstr(out), RT_BOT_SOLID, RT_BOT_UNORIENTED, 0,
				m.nv, m.nf, m.v, m.f, NULL, NULL) >= 0;
		mesh_free(&m);
		return ok;
	    }
	}
	default:
	    return 0;
    }
}


/* forward */
static void emit_node(struct scad_state *st, struct scad_geom *g,
		      struct wmember *head, const mat_t M, int op);

/* emit the children of a container as a union into head (with matrix M,
 * op).  Inlines when it is safe (union into an identity matrix). */
static void
emit_union_children(struct scad_state *st, struct scad_geom *g,
		    struct wmember *head, const mat_t M, int op)
{
    size_t i;
    if (g->nkids == 0) return;
    if (op == WMOP_UNION && bn_mat_is_identity(M)) {
	for (i = 0; i < g->nkids; i++)
	    emit_node(st, g->kids[i], head, M, WMOP_UNION);
	return;
    }
    if (g->nkids == 1) {
	emit_node(st, g->kids[0], head, M, op);
	return;
    }
    /* wrap in a comb */
    {
	struct bu_vls name = BU_VLS_INIT_ZERO;
	struct wmember h;
	BU_LIST_INIT(&h.l);
	for (i = 0; i < g->nkids; i++)
	    emit_node(st, g->kids[i], &h, (const fastf_t *)bn_mat_identity, WMOP_UNION);
	if (BU_LIST_NON_EMPTY(&h.l)) {
	    uniq_name(st, &name, "grp", ".c");
	    mk_lcomb(st->wdbp, bu_vls_cstr(&name), &h, 0, NULL, NULL, NULL, 0);
	    (void)mk_addmember(bu_vls_cstr(&name), &head->l, (matp_t)M, op);
	}
	bu_vls_free(&name);
    }
}

/* emit a boolean (difference/intersection) node as a comb.
 *
 * Each operand is emitted in isolation and collapsed to exactly ONE
 * member before being combined, so the first (union) operand of a
 * difference/intersection binds as a unit: ((a u b) - c), never the
 * distributed (a - c) u (b - c).  A flat member list mixing union and
 * subtract operators does not build the intended tree. */
static void
emit_boolean(struct scad_state *st, struct scad_geom *g,
	     struct wmember *head, const mat_t M, int op)
{
    struct wmember h;
    struct bu_vls name = BU_VLS_INIT_ZERO;
    size_t i;
    int rest = (g->kind == G_DIFFERENCE) ? WMOP_SUBTRACT : WMOP_INTERSECT;
    mat_t idn;
    MAT_IDN(idn);

    BU_LIST_INIT(&h.l);
    for (i = 0; i < g->nkids; i++) {
	int cop = (i == 0) ? WMOP_UNION : rest;
	struct wmember th;
	struct wmember *wm;
	int cnt = 0;

	BU_LIST_INIT(&th.l);
	emit_node(st, g->kids[i], &th, idn, WMOP_UNION);
	for (BU_LIST_FOR(wm, wmember, &th.l)) cnt++;

	if (cnt == 0) {
	    continue;
	} else if (cnt == 1) {
	    wm = BU_LIST_FIRST(wmember, &th.l);
	    (void)mk_addmember(wm->wm_name, &h.l, wm->wm_mat, cop);
	    mk_freemembers(&th.l);
	} else {
	    struct bu_vls gn = BU_VLS_INIT_ZERO;
	    uniq_name(st, &gn, "grp", ".c");
	    mk_lcomb(st->wdbp, bu_vls_cstr(&gn), &th, 0, NULL, NULL, NULL, 0);
	    (void)mk_addmember(bu_vls_cstr(&gn), &h.l, NULL, cop);
	    bu_vls_free(&gn);
	}
    }
    if (BU_LIST_NON_EMPTY(&h.l)) {
	uniq_name(st, &name, (g->kind == G_DIFFERENCE) ? "diff" : "isect", ".c");
	mk_lcomb(st->wdbp, bu_vls_cstr(&name), &h, 0, NULL, NULL, NULL, 0);
	(void)mk_addmember(bu_vls_cstr(&name), &head->l, (matp_t)M, op);
    }
    bu_vls_free(&name);
}

/* emit a hull node: convex hull of collected child vertices -> bot */
static void
emit_hull(struct scad_state *st, struct scad_geom *g, struct wmember *head, const mat_t M, int op)
{
    point_t *pts = NULL;
    int n = 0, cap = 0, i;
    int *faces = NULL, nfaces = 0, nverts = 0;
    point_t *hverts = NULL;
    mat_t idn;
    MAT_IDN(idn);
    for (i = 0; i < (int)g->nkids; i++)
	collect_verts(g->kids[i], idn, &pts, &n, &cap);
    if (n < 4) { if (pts) bu_free(pts, "hull pts"); st->dropped++; return; }

    if (bg_3d_chull(&faces, &nfaces, &hverts, &nverts, (const point_t *)pts, n) <= 0 ||
	nfaces < 4 || nverts < 4) {
	bu_log("scad: hull() computation failed\n");
	st->dropped++;
	if (pts) bu_free(pts, "hull pts");
	if (faces) bu_free(faces, "hull faces");
	if (hverts) bu_free(hverts, "hull verts");
	return;
    }
    {
	struct bu_vls name = BU_VLS_INIT_ZERO;
	uniq_name(st, &name, "hull", ".s");
	if (mk_bot(st->wdbp, bu_vls_cstr(&name), RT_BOT_SOLID, RT_BOT_UNORIENTED, 0,
		   (size_t)nverts, (size_t)nfaces, (fastf_t *)hverts, faces, NULL, NULL) >= 0)
	    (void)mk_addmember(bu_vls_cstr(&name), &head->l, (matp_t)M, op);
	bu_vls_free(&name);
    }
    bu_free(pts, "hull pts");
    bu_free(faces, "hull faces");
    bu_free(hverts, "hull verts");
}

/* build a BRL-CAD sketch object holding every profile loop as a closed
 * run of line segments.  Nested loops become holes via the even/odd
 * fill rule shared by the extrude and revolve primitives.  Returns 1 on
 * success, with the sketch name written to *out. */
static int
build_profile_sketch(struct scad_state *st, struct looplist *ll, struct bu_vls *out)
{
    struct rt_sketch_internal skt;
    size_t total = 0, vi = 0, si = 0, i, j;
    int ret;

    for (i = 0; i < ll->n; i++) total += ll->l[i].n;
    if (total < 3) return 0;

    memset(&skt, 0, sizeof(skt));
    skt.magic = RT_SKETCH_INTERNAL_MAGIC;
    VSETALL(skt.V, 0.0);
    VSET(skt.u_vec, 1, 0, 0);
    VSET(skt.v_vec, 0, 1, 0);
    skt.vert_count = total;
    skt.verts = (point2d_t *)bu_calloc(total, sizeof(point2d_t), "sketch verts");
    skt.curve.count = total;
    skt.curve.reverse = (int *)bu_calloc(total, sizeof(int), "sketch reverse");
    skt.curve.segment = (void **)bu_calloc(total, sizeof(void *), "sketch segs");

    for (i = 0; i < ll->n; i++) {
	size_t base = vi, cnt = ll->l[i].n;
	for (j = 0; j < cnt; j++) {
	    skt.verts[vi][0] = ll->l[i].xy[j * 2 + 0];
	    skt.verts[vi][1] = ll->l[i].xy[j * 2 + 1];
	    vi++;
	}
	for (j = 0; j < cnt; j++) {
	    struct line_seg *ls;
	    BU_ALLOC(ls, struct line_seg);
	    ls->magic = CURVE_LSEG_MAGIC;
	    ls->start = (int)(base + j);
	    ls->end = (int)(base + (j + 1) % cnt);
	    skt.curve.segment[si++] = ls;
	}
    }

    uniq_name(st, out, "prof", ".skt");
    ret = mk_sketch(st->wdbp, bu_vls_cstr(out), &skt);	/* mk_sketch deep-copies */

    for (i = 0; i < skt.curve.count; i++)
	bu_free(skt.curve.segment[i], "line_seg");
    bu_free(skt.curve.segment, "sketch segs");
    bu_free(skt.curve.reverse, "sketch reverse");
    bu_free(skt.verts, "sketch verts");
    return ret >= 0;
}

/* fallback: tessellate each profile loop into a BoT and union them */
static void
emit_extrude_bot(struct scad_state *st, struct scad_geom *g, struct looplist *ll,
		 struct wmember *head, const mat_t M, int op)
{
    struct wmember h;
    struct bu_vls name = BU_VLS_INIT_ZERO;
    size_t i;
    int made = 0;

    BU_LIST_INIT(&h.l);
    for (i = 0; i < ll->n; i++) {
	struct mesh m;
	struct bu_vls sn = BU_VLS_INIT_ZERO;
	memset(&m, 0, sizeof(m));
	if (g->kind == G_LINEXT) build_linext(&m, &ll->l[i], g);
	else build_rotext(&m, &ll->l[i], g);
	uniq_name(st, &sn, (g->kind == G_LINEXT) ? "linext" : "rotext", ".s");
	if (mesh_to_bot(st->wdbp, bu_vls_cstr(&sn), &m)) {
	    (void)mk_addmember(bu_vls_cstr(&sn), &h.l, NULL, WMOP_UNION);
	    made++;
	}
	mesh_free(&m);
	bu_vls_free(&sn);
    }

    if (made == 1) {
	struct wmember *wm = BU_LIST_FIRST(wmember, &h.l);
	(void)mk_addmember(wm->wm_name, &head->l, (matp_t)M, op);
	mk_freemembers(&h.l);
    } else if (made > 1) {
	uniq_name(st, &name, "extrude", ".c");
	mk_lcomb(st->wdbp, bu_vls_cstr(&name), &h, 0, NULL, NULL, NULL, 0);
	(void)mk_addmember(bu_vls_cstr(&name), &head->l, (matp_t)M, op);
    } else {
	mk_freemembers(&h.l);
    }
    bu_vls_free(&name);
}

/* emit a rotate_extrude as a native revolve of the profile sketch.
 * OpenSCAD sweeps a profile (x=radius, y=height) about +Z starting from
 * +X, which maps directly onto the revolve frame. */
static int
emit_revolve(struct scad_state *st, struct scad_geom *g, const char *sktname,
	     struct wmember *head, const mat_t M, int op)
{
    struct rt_revolve_internal *rip;
    struct bu_vls name = BU_VLS_INIT_ZERO;
    int ret;

    BU_ALLOC(rip, struct rt_revolve_internal);	/* freed by wdb_export */
    rip->magic = RT_REVOLVE_INTERNAL_MAGIC;
    VSET(rip->v3d, 0, 0, 0);
    VSET(rip->axis3d, 0, 0, 1);		/* revolve about +Z         */
    VSET(rip->r, 1, 0, 0);		/* sketch u (radius) -> +X  */
    rip->ang = (ZERO(g->ext_angle) ? 360.0 : g->ext_angle) * DEG2RAD;
    bu_vls_init(&rip->sketch_name);
    bu_vls_strcpy(&rip->sketch_name, sktname);
    rip->skt = NULL;

    uniq_name(st, &name, "rotext", ".s");
    ret = wdb_export(st->wdbp, bu_vls_cstr(&name), rip, ID_REVOLVE, mk_conv2mm);
    if (ret >= 0)
	(void)mk_addmember(bu_vls_cstr(&name), &head->l, (matp_t)M, op);
    bu_vls_free(&name);
    return ret >= 0;
}

/* emit a linear_extrude / rotate_extrude node.
 *
 * A plain linear_extrude (no twist, unit scale) maps to a native
 * sketch + extrusion, and a rotate_extrude whose profile stays on one
 * side of the axis maps to a sketch + revolve -- both round-trippable.
 * twist/scale extrusions and axis-crossing revolves fall back to a
 * tessellated BoT. */
static void
emit_extrude(struct scad_state *st, struct scad_geom *g, struct wmember *head, const mat_t M, int op)
{
    struct looplist ll;
    struct bu_vls skt = BU_VLS_INIT_ZERO;
    mat_t idn;
    size_t i, j;

    MAT_IDN(idn);
    memset(&ll, 0, sizeof(ll));
    for (i = 0; i < g->nkids; i++)
	collect_profiles(st, g->kids[i], idn, &ll);
    if (ll.n == 0) { st->dropped++; looplist_free(&ll); bu_vls_free(&skt); return; }

    if (g->kind == G_LINEXT) {
	int simple = ZERO(g->ext_twist) &&
		     NEAR_EQUAL(g->ext_scale[0], 1.0, VUNITIZE_TOL) &&
		     NEAR_EQUAL(g->ext_scale[1], 1.0, VUNITIZE_TOL);
	if (g->ext_height <= 0) {
	    st->dropped++;
	} else if (simple && build_profile_sketch(st, &ll, &skt)) {
	    point_t V;
	    vect_t h, u, v;
	    struct bu_vls en = BU_VLS_INIT_ZERO;
	    VSET(V, 0, 0, g->ext_center ? -g->ext_height / 2 : 0);
	    VSET(h, 0, 0, g->ext_height);
	    VSET(u, 1, 0, 0);
	    VSET(v, 0, 1, 0);
	    uniq_name(st, &en, "linext", ".s");
	    if (mk_extrusion(st->wdbp, bu_vls_cstr(&en), bu_vls_cstr(&skt), V, h, u, v, 0) >= 0)
		(void)mk_addmember(bu_vls_cstr(&en), &head->l, (matp_t)M, op);
	    bu_vls_free(&en);
	} else {
	    emit_extrude_bot(st, g, &ll, head, M, op);
	}
    } else {	/* G_ROTEXT */
	double minx = INFINITY;
	for (i = 0; i < ll.n; i++)
	    for (j = 0; j < ll.l[i].n; j++)
		if (ll.l[i].xy[j * 2] < minx) minx = ll.l[i].xy[j * 2];
	if (minx >= -VUNITIZE_TOL && build_profile_sketch(st, &ll, &skt))
	    emit_revolve(st, g, bu_vls_cstr(&skt), head, M, op);
	else
	    emit_extrude_bot(st, g, &ll, head, M, op);
    }

    looplist_free(&ll);
    bu_vls_free(&skt);
}

/* append node g as one-or-more members of head, under matrix M and op */
static void
emit_node(struct scad_state *st, struct scad_geom *g, struct wmember *head, const mat_t M, int op)
{
    if (!g) return;
    switch (g->kind) {
	case G_PRIM: {
	    struct bu_vls name = BU_VLS_INIT_ZERO;
	    if (emit_prim(st, g, &name))
		(void)mk_addmember(bu_vls_cstr(&name), &head->l, (matp_t)M, op);
	    bu_vls_free(&name);
	    return;
	}
	case G_TRANSFORM: {
	    mat_t M2;
	    bn_mat_mul(M2, M, g->mat);
	    emit_union_children(st, g, head, M2, op);
	    return;
	}
	case G_GROUP:
	case G_RENDER:
	    emit_union_children(st, g, head, M, op);
	    return;
	case G_COLOR: {
	    /* materialize a comb so the color attribute is preserved */
	    struct wmember h;
	    struct bu_vls name = BU_VLS_INIT_ZERO;
	    unsigned char rgb[3];
	    mat_t idn;
	    MAT_IDN(idn);
	    BU_LIST_INIT(&h.l);
	    emit_union_children(st, g, &h, idn, WMOP_UNION);
	    if (BU_LIST_NON_EMPTY(&h.l)) {
		rgb[0] = (unsigned char)(g->rgba[0] * 255.0 + 0.5);
		rgb[1] = (unsigned char)(g->rgba[1] * 255.0 + 0.5);
		rgb[2] = (unsigned char)(g->rgba[2] * 255.0 + 0.5);
		uniq_name(st, &name, "color", ".c");
		mk_lcomb(st->wdbp, bu_vls_cstr(&name), &h, 0, NULL, NULL, rgb, 0);
		(void)mk_addmember(bu_vls_cstr(&name), &head->l, (matp_t)M, op);
	    }
	    bu_vls_free(&name);
	    return;
	}
	case G_DIFFERENCE:
	case G_INTERSECTION:
	    emit_boolean(st, g, head, M, op);
	    return;
	case G_HULL:
	    emit_hull(st, g, head, M, op);
	    return;
	case G_LINEXT:
	case G_ROTEXT:
	    emit_extrude(st, g, head, M, op);
	    return;
	default:
	    /* G_MINKOWSKI never reaches here (rewritten during eval) */
	    return;
    }
}

/* ------------------------------------------------------------------ */
/* top-level emission: each top object -> a region under group "all"   */
/* ------------------------------------------------------------------ */

static void
emit_region(struct scad_state *st, struct scad_geom *node, struct wmember *all_head)
{
    struct wmember h;
    struct bu_vls name = BU_VLS_INIT_ZERO;
    mat_t idn;
    unsigned char rgb[3];
    int have_color = 0;

    MAT_IDN(idn);
    BU_LIST_INIT(&h.l);
    if (node->kind == G_COLOR) {
	/* apply the color to the region itself and emit its children
	 * directly, so there is no redundant inner colored comb whose
	 * override the raytracer would warn about and ignore. */
	rgb[0] = (unsigned char)(node->rgba[0] * 255.0 + 0.5);
	rgb[1] = (unsigned char)(node->rgba[1] * 255.0 + 0.5);
	rgb[2] = (unsigned char)(node->rgba[2] * 255.0 + 0.5);
	have_color = 1;
	emit_union_children(st, node, &h, idn, WMOP_UNION);
    } else {
	emit_node(st, node, &h, idn, WMOP_UNION);
    }
    if (BU_LIST_IS_EMPTY(&h.l)) { bu_vls_free(&name); return; }

    uniq_name(st, &name, "part", ".r");
    mk_lrcomb(st->wdbp, bu_vls_cstr(&name), &h, 1, NULL, NULL,
	      have_color ? rgb : NULL, 0, 0, 0, 100, 0);
    (void)mk_addmember(bu_vls_cstr(&name), &all_head->l, NULL, WMOP_UNION);
    bu_vls_free(&name);
}

int
scad_emit(struct scad_state *st, struct scad_geom *root)
{
    struct wmember all_head;
    size_t i;
    int created;

    if (!root) return 0;
    BU_LIST_INIT(&all_head.l);

    if (root->kind == G_GROUP || root->kind == G_RENDER) {
	for (i = 0; i < root->nkids; i++)
	    emit_region(st, root->kids[i], &all_head);
    } else {
	emit_region(st, root, &all_head);
    }

    created = BU_LIST_NON_EMPTY(&all_head.l);
    if (created)
	mk_lcomb(st->wdbp, "all", &all_head, 0, NULL, NULL, NULL, 0);
    else
	bu_log("scad: no geometry was produced\n");

    return created;
}

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
