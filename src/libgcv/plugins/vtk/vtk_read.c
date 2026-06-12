/*                      V T K _ R E A D . C
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
/** @file vtk_read.c
 *
 * Import filter for "legacy" (ASCII) VTK files.
 *
 * Supports the legacy VTK file format (the "# vtk DataFile Version"
 * header) in ASCII encoding.  Binary legacy VTK and the newer XML
 * formats (.vtp/.vtu/...) are recognized but politely rejected.
 *
 * Dataset mappings, in priority order:
 *
 *   POLYDATA (POINTS only)         -> a single pnts object (mk_pnts)
 *   POLYDATA (VERTICES)            -> pnts/sph/datum (--vertices)
 *   POLYDATA (LINES)               -> datum/pipe/rcc (--lines)
 *   POLYDATA (POLYGONS/STRIPS)     -> a single bot (mk_bot)
 *   UNSTRUCTURED_GRID              -> arbN cells and/or a bot (--cells)
 *   STRUCTURED_POINTS              -> vol, or dsp/ebm for 2D (--grid)
 *   RECTILINEAR_GRID               -> rpp cells or a bot (--grid/--cell-max)
 *   STRUCTURED_GRID                -> arb8 hex cells (--cell-max)
 *
 * Everything created is unioned under a top level comb named "all".
 *
 */

#include "common.h"

#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <ctype.h>

#include "bio.h"

#include "bu/avs.h"
#include "bu/malloc.h"
#include "bu/opt.h"
#include "bu/str.h"
#include "bu/units.h"
#include "bu/vls.h"
#include "gcv/api.h"
#include "vmath.h"
#include "rt/db5.h"
#include "rt/geom.h"
#include "raytrace.h"
#include "wdb.h"


/* VTK cell type codes (subset we map) */
#define VTK_TRIANGLE	5
#define VTK_QUAD	9
#define VTK_TETRA	10
#define VTK_VOXEL	11
#define VTK_HEXAHEDRON	12
#define VTK_WEDGE	13
#define VTK_PYRAMID	14


struct vtk_read_options
{
    char *scalar_array;		/* POINT_DATA scalar to use for scale/color */
    char *vector_array;		/* POINT_DATA vector to use for normal */
    fastf_t point_scale;	/* multiplier applied to pnts scale */
    char *units;		/* model units */

    char *vertices;		/* POLYDATA VERTICES: datum|pnts|sph (default pnts) */
    fastf_t vertex_radius;	/* radius for --vertices sph (0 -> default) */
    int vertex_max;		/* cap on per-vertex solids before fallback */

    char *lines;		/* POLYDATA LINES: datum|pipe|rcc (default datum) */
    fastf_t line_radius;	/* radius for pipe/rcc lines (0 -> default) */

    char *cells;		/* solid cells: arb|bot (default arb) */
    int cell_max;		/* cap on per-cell solids before fallback */

    char *grid;			/* structured/rectilinear: auto|vol|dsp|ebm|cells */
};


/* A named array of doubles parsed from the file (POINTS, SCALARS, ...) */
struct vtk_darray
{
    char *name;
    int ncomp;			/* components per tuple (1 for scalar) */
    size_t ntuple;		/* number of tuples */
    fastf_t *v;			/* ntuple*ncomp values */
};


struct vtk_state
{
    const struct gcv_opts *gcv_options;
    const struct vtk_read_options *opts;

    const char *input_file;
    FILE *fp;
    struct rt_wdb *wdbp;

    struct wmember all_head;

    struct bu_vls title;	/* line 2 of the file */
    struct bu_vls dataset;	/* DATASET <type> */

    /* attributes to set on "all" once it has been created (FIELD data
     * such as TIME/CYCLE may be parsed before any geometry exists) */
    struct bu_attribute_value_set pending_attrs;
    int pending_init;

    /* lookahead line buffer (one line of pushback) */
    struct bu_vls line;
    int have_line;

    /* token stream over the current line; unconsumed tokens persist
     * across calls so "count then values on one line" layouts parse
     * correctly (VTK CELLS/POLYGONS connectivity) */
    struct bu_vls tok_line;
    char *tok_pos;

    /* dropped sections / arrays, for the consolidated warning */
    int dropped;
};


/*
 * Line input with a single line of pushback.  Returns 1 on success
 * (line content in state->line, trailing newline removed), 0 on EOF.
 */
static int
vtk_getline(struct vtk_state *s)
{
    if (s->have_line) {
	s->have_line = 0;
	s->tok_pos = NULL;
	return 1;
    }
    bu_vls_trunc(&s->line, 0);
    if (bu_vls_gets(&s->line, s->fp) < 0)
	return 0;
    s->tok_pos = NULL;
    return 1;
}


static void
vtk_ungetline(struct vtk_state *s)
{
    s->have_line = 1;
}


/* return 1 if the current line is blank or whitespace-only */
static int
vtk_line_blank(struct vtk_state *s)
{
    const char *c = bu_vls_cstr(&s->line);
    while (*c) {
	if (!isspace((int)*c))
	    return 0;
	c++;
    }
    return 1;
}


/*
 * Return the next whitespace-delimited token from the input, crossing
 * line boundaries as needed.  Tokens left unconsumed on a line are
 * preserved for the following call, so a "count then values on the same
 * line" layout (VTK CELLS/POLYGONS connectivity) is read correctly.
 * Returns NULL at end of file.
 */
static char *
vtk_next_token(struct vtk_state *s)
{
    for (;;) {
	char *p, *start;

	if (!s->tok_pos) {
	    if (!vtk_getline(s))
		return NULL;
	    bu_vls_sprintf(&s->tok_line, "%s", bu_vls_cstr(&s->line));
	    s->tok_pos = bu_vls_addr(&s->tok_line);
	}

	p = s->tok_pos;
	while (*p && isspace((int)*p))
	    p++;
	if (!*p) {
	    /* current line exhausted; fetch the next one */
	    s->tok_pos = NULL;
	    continue;
	}

	start = p;
	while (*p && !isspace((int)*p))
	    p++;
	if (*p) {
	    *p = '\0';
	    s->tok_pos = p + 1;
	} else {
	    s->tok_pos = p;	/* at end of line; next call refills */
	}
	return start;
    }
}


/*
 * Read exactly 'n' doubles from the file, crossing line boundaries as
 * needed (VTK does not guarantee one tuple per line).  Values are
 * stored into out[].  Returns the count actually read.
 */
static size_t
vtk_read_n_doubles(struct vtk_state *s, fastf_t *out, size_t n)
{
    size_t got = 0;
    char *tok;

    while (got < n && (tok = vtk_next_token(s)) != NULL)
	out[got++] = strtod(tok, NULL);

    return got;
}


/*
 * Read exactly 'n' ints from the file, crossing line boundaries.
 * Returns the count actually read.
 */
static size_t
vtk_read_n_ints(struct vtk_state *s, int *out, size_t n)
{
    size_t got = 0;
    char *tok;

    while (got < n && (tok = vtk_next_token(s)) != NULL)
	out[got++] = (int)strtol(tok, NULL, 10);

    return got;
}


static void
vtk_darray_free(struct vtk_darray *a)
{
    if (!a)
	return;
    if (a->name)
	bu_free(a->name, "vtk darray name");
    if (a->v)
	bu_free(a->v, "vtk darray v");
    a->name = NULL;
    a->v = NULL;
}


/* quantize v in [min,max] to a uint8 0..255 */
static unsigned char
vtk_quantize(double v, double min, double max)
{
    double t;
    if (max <= min)
	return 0;
    t = (v - min) / (max - min);
    if (t < 0.0)
	t = 0.0;
    if (t > 1.0)
	t = 1.0;
    return (unsigned char)(t * 255.0 + 0.5);
}


/* map a normalized [0,1] value to a simple blue->red color ramp */
static void
vtk_ramp(double t, unsigned char rgb[3])
{
    if (t < 0.0)
	t = 0.0;
    if (t > 1.0)
	t = 1.0;
    rgb[0] = (unsigned char)(t * 255.0 + 0.5);
    rgb[1] = (unsigned char)((1.0 - fabs(2.0 * t - 1.0)) * 255.0 + 0.5);
    rgb[2] = (unsigned char)((1.0 - t) * 255.0 + 0.5);
}


/* return option string or a default when the option is NULL/empty */
static const char *
vtk_optstr(const char *opt, const char *def)
{
    return (opt && opt[0]) ? opt : def;
}


/*
 * Union a freshly-created solid into "all".  When a scalar value (with
 * min/max) is supplied, wrap the solid in a region colored by the value
 * via the blue->red ramp (mk_region1); otherwise add the bare solid.
 * On color path the region (not the solid) is unioned into "all".
 */
static void
vtk_add_cell(struct vtk_state *s, const char *solid, int have_scalar,
	     double val, double min, double max)
{
    if (have_scalar) {
	unsigned char rgb[3];
	double t = (max > min) ? (val - min) / (max - min) : 0.0;
	struct bu_vls rn = BU_VLS_INIT_ZERO;
	vtk_ramp(t, rgb);
	bu_vls_sprintf(&rn, "%s.r", solid);
	if (mk_region1(s->wdbp, bu_vls_cstr(&rn), solid, NULL, NULL, rgb) == 0)
	    (void)mk_addmember(bu_vls_cstr(&rn), &s->all_head.l, NULL, WMOP_UNION);
	else
	    (void)mk_addmember(solid, &s->all_head.l, NULL, WMOP_UNION);
	bu_vls_free(&rn);
    } else {
	(void)mk_addmember(solid, &s->all_head.l, NULL, WMOP_UNION);
    }
}


/*
 * Buffer a key/value attribute for the top-level "all" comb.  The comb
 * may not exist yet at parse time (FIELD data often precedes geometry),
 * so attributes are accumulated and flushed by vtk_flush_attrs().
 */
static void
vtk_set_attr(struct vtk_state *s, const char *obj, const char *key, const char *val)
{
    /* this importer only ever annotates "all" */
    (void)obj;
    if (!s->pending_init) {
	bu_avs_init_empty(&s->pending_attrs);
	s->pending_init = 1;
    }
    (void)bu_avs_add(&s->pending_attrs, key, val);
}


/*
 * Apply all buffered attributes to the (now existing) "all" comb.
 */
static void
vtk_flush_attrs(struct vtk_state *s)
{
    struct bu_attribute_value_pair *avpp;
    if (!s->pending_init)
	return;
    for (BU_AVS_FOR(avpp, &s->pending_attrs)) {
	if (db5_update_attribute("all", avpp->name, avpp->value, s->wdbp->dbip))
	    bu_log("vtk_read: warning, failed to set attribute %s on all\n", avpp->name);
    }
    bu_avs_free(&s->pending_attrs);
    s->pending_init = 0;
}


/*
 * Parse a FIELD construct: "FIELD <name> <narrays>" has already been
 * consumed; read 'narr' arrays, each preceded by a
 * "name ncomp ntuple type" header.  Single-valued entries (e.g. TIME,
 * CYCLE) are exposed as db5 attributes on "all".
 */
static void
vtk_read_field(struct vtk_state *s, int narr)
{
    int k;
    for (k = 0; k < narr; k++) {
	char *fargv[16];
	struct bu_vls fwork = BU_VLS_INIT_ZERO;
	size_t fac;
	int fcomp, ftup;
	fastf_t *tmp;

	if (!vtk_getline(s)) {
	    bu_vls_free(&fwork);
	    break;
	}
	bu_vls_sprintf(&fwork, "%s", bu_vls_cstr(&s->line));
	fac = bu_argv_from_string(fargv, 16, bu_vls_addr(&fwork));
	if (fac < 4) {
	    bu_vls_free(&fwork);
	    continue;
	}
	fcomp = (int)strtol(fargv[1], NULL, 10);
	ftup = (int)strtol(fargv[2], NULL, 10);
	if (fcomp < 1)
	    fcomp = 1;
	if (ftup < 0)
	    ftup = 0;
	tmp = (fastf_t *)bu_malloc(sizeof(fastf_t) * (fcomp * ftup + 1), "field vals");
	vtk_read_n_doubles(s, tmp, (size_t)fcomp * ftup);
	/* expose single-valued field entries (TIME, CYCLE) as attrs */
	if (fcomp * ftup >= 1) {
	    struct bu_vls aval = BU_VLS_INIT_ZERO;
	    bu_vls_sprintf(&aval, "%.17g", tmp[0]);
	    vtk_set_attr(s, "all", fargv[0], bu_vls_cstr(&aval));
	    bu_vls_free(&aval);
	}
	bu_free(tmp, "field vals");
	bu_vls_free(&fwork);
    }
}


/*
 * Parse a POINT_DATA / CELL_DATA section.  We collect SCALARS and
 * VECTORS arrays (and skip FIELD/other constructs gracefully).  The
 * caller passes the announced tuple count.
 *
 * Returns via *scalars/*nscalars and *vectors/*nvectors growable
 * arrays the caller is responsible for freeing.
 */
static void
vtk_read_data_arrays(struct vtk_state *s, size_t ntuple,
		     struct vtk_darray **scalars, int *nscalars,
		     struct vtk_darray **vectors, int *nvectors)
{
    *scalars = NULL;
    *vectors = NULL;
    *nscalars = 0;
    *nvectors = 0;

    while (vtk_getline(s)) {
	char *argv[16];
	struct bu_vls work = BU_VLS_INIT_ZERO;
	size_t ac;

	if (vtk_line_blank(s)) {
	    bu_vls_free(&work);
	    continue;
	}

	bu_vls_sprintf(&work, "%s", bu_vls_cstr(&s->line));
	ac = bu_argv_from_string(argv, 16, bu_vls_addr(&work));
	if (!ac) {
	    bu_vls_free(&work);
	    continue;
	}

	if (BU_STR_EQUAL(argv[0], "SCALARS") && ac >= 3) {
	    struct vtk_darray a;
	    int ncomp = (ac >= 4) ? (int)strtol(argv[3], NULL, 10) : 1;
	    if (ncomp < 1)
		ncomp = 1;
	    a.name = bu_strdup(argv[1]);
	    a.ncomp = ncomp;
	    a.ntuple = ntuple;
	    a.v = (fastf_t *)bu_malloc(sizeof(fastf_t) * ntuple * ncomp, "scalar vals");

	    /* an optional LOOKUP_TABLE line precedes the data */
	    if (vtk_getline(s)) {
		struct bu_vls lt = BU_VLS_INIT_ZERO;
		bu_vls_sprintf(&lt, "%s", bu_vls_cstr(&s->line));
		bu_vls_trimspace(&lt);
		if (bu_strncmp(bu_vls_cstr(&lt), "LOOKUP_TABLE", 12) != 0)
		    vtk_ungetline(s);
		bu_vls_free(&lt);
	    }

	    vtk_read_n_doubles(s, a.v, ntuple * ncomp);
	    *scalars = (struct vtk_darray *)bu_realloc(*scalars,
		    sizeof(struct vtk_darray) * (*nscalars + 1), "scalars");
	    (*scalars)[(*nscalars)++] = a;

	} else if (BU_STR_EQUAL(argv[0], "VECTORS") && ac >= 2) {
	    struct vtk_darray a;
	    a.name = bu_strdup(argv[1]);
	    a.ncomp = 3;
	    a.ntuple = ntuple;
	    a.v = (fastf_t *)bu_malloc(sizeof(fastf_t) * ntuple * 3, "vector vals");
	    vtk_read_n_doubles(s, a.v, ntuple * 3);
	    *vectors = (struct vtk_darray *)bu_realloc(*vectors,
		    sizeof(struct vtk_darray) * (*nvectors + 1), "vectors");
	    (*vectors)[(*nvectors)++] = a;

	} else if (BU_STR_EQUAL(argv[0], "FIELD") && ac >= 3) {
	    /* FIELD <name> <narrays> */
	    vtk_read_field(s, (int)strtol(argv[2], NULL, 10));

	} else if (BU_STR_EQUAL(argv[0], "POINT_DATA")
		   || BU_STR_EQUAL(argv[0], "CELL_DATA")) {
	    /* a new data block begins; hand it back to the caller */
	    vtk_ungetline(s);
	    bu_vls_free(&work);
	    break;
	} else if (BU_STR_EQUAL(argv[0], "LOOKUP_TABLE")
		   || BU_STR_EQUAL(argv[0], "COLOR_SCALARS")
		   || BU_STR_EQUAL(argv[0], "NORMALS")
		   || BU_STR_EQUAL(argv[0], "TEXTURE_COORDINATES")
		   || BU_STR_EQUAL(argv[0], "TENSORS")) {
	    /* recognized but unhandled; we cannot reliably know the
	     * length of all of these, so we stop collecting here. */
	    bu_log("vtk_read: skipping unhandled data array '%s'\n", argv[0]);
	    s->dropped++;
	    vtk_ungetline(s);
	    bu_vls_free(&work);
	    break;
	} else {
	    /* unknown keyword - stop, let the caller decide */
	    vtk_ungetline(s);
	    bu_vls_free(&work);
	    break;
	}

	bu_vls_free(&work);
    }
}


/* pick a scalar array by name, else the first; -1 if none */
static int
vtk_pick(struct vtk_darray *arr, int n, const char *name)
{
    int i;
    if (n <= 0)
	return -1;
    if (name && name[0]) {
	for (i = 0; i < n; i++)
	    if (arr[i].name && BU_STR_EQUAL(arr[i].name, name))
		return i;
    }
    return 0;
}


/*
 * MAPPING 1: POLYDATA POINTS -> pnts object.
 *
 * pts holds count*3 coordinates already scaled.  Scalar/vector arrays
 * drive optional per-point scale and normal.
 */
static int
vtk_emit_pnts(struct vtk_state *s, const char *name, fastf_t *pts, size_t count,
	      struct vtk_darray *scalars, int nscalars,
	      struct vtk_darray *vectors, int nvectors)
{
    rt_pnt_type type = RT_PNT_TYPE_PNT;
    fastf_t *scales = NULL;
    fastf_t *normals = NULL;
    unsigned char *colors = NULL;
    int si, vi;
    int have_sca = 0, have_nrm = 0;
    size_t i;
    int ret;

    si = vtk_pick(scalars, nscalars, s->opts->scalar_array);
    vi = vtk_pick(vectors, nvectors, s->opts->vector_array);

    if (si >= 0 && scalars[si].ntuple >= count) {
	double min = scalars[si].v[0], max = scalars[si].v[0];
	have_sca = 1;
	scales = (fastf_t *)bu_malloc(sizeof(fastf_t) * count, "pnt scales");
	colors = (unsigned char *)bu_malloc(sizeof(unsigned char) * count * 3, "pnt colors");
	for (i = 0; i < count; i++) {
	    double val = scalars[si].v[i];
	    if (val < min) min = val;
	    if (val > max) max = val;
	}
	for (i = 0; i < count; i++) {
	    double val = scalars[si].v[i];
	    double t = (max > min) ? (val - min) / (max - min) : 0.0;
	    scales[i] = (fastf_t)(val * s->opts->point_scale);
	    vtk_ramp(t, &colors[i * 3]);
	}
	bu_log("vtk_read: per-point scale/color from SCALARS '%s'\n", scalars[si].name);
    }

    if (vi >= 0 && vectors[vi].ntuple >= count) {
	have_nrm = 1;
	normals = (fastf_t *)bu_malloc(sizeof(fastf_t) * count * 3, "pnt normals");
	for (i = 0; i < count * 3; i++)
	    normals[i] = vectors[vi].v[i];
	bu_log("vtk_read: per-point normal from VECTORS '%s'\n", vectors[vi].name);
    }

    if (have_sca && have_nrm)
	type = RT_PNT_TYPE_COL_SCA_NRM;
    else if (have_sca)
	type = RT_PNT_TYPE_COL_SCA;
    else if (have_nrm)
	type = RT_PNT_TYPE_NRM;
    else
	type = RT_PNT_TYPE_PNT;

    ret = mk_pnts(s->wdbp, name, type, s->opts->point_scale, count,
		  pts, colors, scales, normals);

    if (scales)
	bu_free(scales, "pnt scales");
    if (normals)
	bu_free(normals, "pnt normals");
    if (colors)
	bu_free(colors, "pnt colors");

    if (ret) {
	bu_log("vtk_read: mk_pnts(%s) failed\n", name);
	return 0;
    }

    (void)mk_addmember(name, &s->all_head.l, NULL, WMOP_UNION);
    return 1;
}


/*
 * Read the POINTS section body (count*3 doubles), scaling by the gcv
 * scale factor.  Returns the allocated array (caller frees) and sets
 * *count.  NULL on failure.
 */
static fastf_t *
vtk_read_points(struct vtk_state *s, size_t count)
{
    fastf_t *pts = (fastf_t *)bu_malloc(sizeof(fastf_t) * count * 3, "vtk points");
    size_t got = vtk_read_n_doubles(s, pts, count * 3);
    size_t i;
    fastf_t sf = s->gcv_options->scale_factor;
    if (sf <= 0.0)
	sf = 1.0;
    if (got < count * 3) {
	bu_log("vtk_read: expected %zu point coords, got %zu\n", count * 3, got);
	bu_free(pts, "vtk points");
	return NULL;
    }
    for (i = 0; i < count * 3; i++)
	pts[i] *= sf;
    return pts;
}


/*
 * Emit POLYDATA VERTICES per --vertices.  conn is the raw connectivity
 * stream (size ints): each cell is "cnt idx idx ...".  Honors sph/datum,
 * falling back to a no-op (pnts already emitted) for "pnts".
 */
static int
vtk_emit_vertices(struct vtk_state *s, const int *conn, size_t connlen,
		  fastf_t *pts, size_t npts, int have_pnts)
{
    const char *mode = vtk_optstr(s->opts->vertices, "pnts");
    fastf_t sf = s->gcv_options->scale_factor;
    size_t i;
    int nref = 0;
    int created = 0;
    fastf_t rad;

    if (sf <= 0.0)
	sf = 1.0;

    /* count referenced vertices */
    for (i = 0; i < connlen;) {
	int cnt = conn[i++];
	if (cnt < 0)
	    break;
	nref += cnt;
	i += (size_t)cnt;
    }
    if (nref <= 0)
	return 0;

    /* default radius: --vertex-radius if >0, else a small fraction of model */
    rad = (s->opts->vertex_radius > 0.0) ? s->opts->vertex_radius * sf : 1.0 * sf;

    if (BU_STR_EQUAL(mode, "pnts")) {
	if (have_pnts) {
	    bu_log("vtk_read: POLYDATA VERTICES reference existing pnts (no-op)\n");
	    return 0;
	}
	mode = "sph";	/* nothing emitted yet; fall through to sph below */
    }

    if (BU_STR_EQUAL(mode, "sph")) {
	if (nref > s->opts->vertex_max) {
	    bu_log("vtk_read: %d VERTICES exceed --vertex-max %d; "
		   "falling back to a single pnts\n", nref, s->opts->vertex_max);
	    mode = "pnts-fallback";
	} else {
	    int vid = 0;
	    for (i = 0; i < connlen;) {
		int cnt = conn[i++];
		int k;
		if (cnt < 0)
		    break;
		for (k = 0; k < cnt; k++) {
		    int p = conn[i + k];
		    struct bu_vls vn = BU_VLS_INIT_ZERO;
		    if (p < 0 || (size_t)p >= npts)
			continue;
		    bu_vls_sprintf(&vn, "vtk.vert%d.sph", vid++);
		    if (mk_sph(s->wdbp, bu_vls_cstr(&vn), &pts[p * 3], rad) == 0) {
			(void)mk_addmember(bu_vls_cstr(&vn), &s->all_head.l, NULL, WMOP_UNION);
			created++;
		    }
		    bu_vls_free(&vn);
		}
		i += (size_t)cnt;
	    }
	    bu_log("vtk_read: POLYDATA VERTICES -> %d sph (radius %g)\n", created, rad);
	    return created;
	}
    }

    if (BU_STR_EQUAL(mode, "datum")) {
	struct rt_datum_internal *head = NULL, *tail = NULL;
	int nd = 0;
	for (i = 0; i < connlen;) {
	    int cnt = conn[i++];
	    int k;
	    if (cnt < 0)
		break;
	    for (k = 0; k < cnt; k++) {
		int p = conn[i + k];
		struct rt_datum_internal *d;
		if (p < 0 || (size_t)p >= npts)
		    continue;
		BU_ALLOC(d, struct rt_datum_internal);
		d->magic = RT_DATUM_INTERNAL_MAGIC;
		VMOVE(d->pnt, &pts[p * 3]);
		VSETALL(d->dir, 0.0);
		d->w = 0.0;
		d->next = NULL;
		if (tail)
		    tail->next = d;
		else
		    head = d;
		tail = d;
		nd++;
	    }
	    i += (size_t)cnt;
	}
	if (head && mk_datums(s->wdbp, "vtk.verts.datum", head) == 0) {
	    (void)mk_addmember("vtk.verts.datum", &s->all_head.l, NULL, WMOP_UNION);
	    bu_log("vtk_read: POLYDATA VERTICES -> %d point datums\n", nd);
	    created++;
	}
	/* mk_datums duplicates the chain; we own and free our nodes */
	while (head) {
	    struct rt_datum_internal *n = head->next;
	    bu_free(head, "vtk vert datum");
	    head = n;
	}
	return created;
    }

    /* "pnts-fallback": build a plain pnts from the referenced points */
    {
	fastf_t *vp = (fastf_t *)bu_malloc(sizeof(fastf_t) * nref * 3, "vert pnts");
	size_t np = 0;
	for (i = 0; i < connlen;) {
	    int cnt = conn[i++];
	    int k;
	    if (cnt < 0)
		break;
	    for (k = 0; k < cnt; k++) {
		int p = conn[i + k];
		if (p < 0 || (size_t)p >= npts)
		    continue;
		VMOVE(&vp[np * 3], &pts[p * 3]);
		np++;
	    }
	    i += (size_t)cnt;
	}
	if (np > 0 && mk_pnts(s->wdbp, "vtk.verts.pnts", RT_PNT_TYPE_PNT,
			      s->opts->point_scale, np, vp, NULL, NULL, NULL) == 0) {
	    (void)mk_addmember("vtk.verts.pnts", &s->all_head.l, NULL, WMOP_UNION);
	    bu_log("vtk_read: POLYDATA VERTICES -> pnts (%zu points)\n", np);
	    created++;
	}
	bu_free(vp, "vert pnts");
    }
    return created;
}


/*
 * Emit POLYDATA LINES per --lines.  conn is the raw connectivity stream:
 * each polyline is "cnt idx idx ...".  Honors datum/pipe/rcc.
 */
static int
vtk_emit_lines(struct vtk_state *s, const int *conn, size_t connlen,
	       fastf_t *pts, size_t npts)
{
    const char *mode = vtk_optstr(s->opts->lines, "datum");
    fastf_t sf = s->gcv_options->scale_factor;
    fastf_t rad;
    size_t i;
    int created = 0;
    int lid = 0;

    if (sf <= 0.0)
	sf = 1.0;
    rad = (s->opts->line_radius > 0.0) ? s->opts->line_radius * sf : 1.0 * sf;

    if (BU_STR_EQUAL(mode, "datum")) {
	struct rt_datum_internal *head = NULL, *tail = NULL;
	int nd = 0;
	for (i = 0; i < connlen;) {
	    int cnt = conn[i++];
	    int k;
	    if (cnt < 0)
		break;
	    /* one line datum per segment (pnt -> dir to next) */
	    for (k = 0; k + 1 < cnt; k++) {
		int p0 = conn[i + k];
		int p1 = conn[i + k + 1];
		struct rt_datum_internal *d;
		if (p0 < 0 || (size_t)p0 >= npts || p1 < 0 || (size_t)p1 >= npts)
		    continue;
		BU_ALLOC(d, struct rt_datum_internal);
		d->magic = RT_DATUM_INTERNAL_MAGIC;
		VMOVE(d->pnt, &pts[p0 * 3]);
		VSUB2(d->dir, &pts[p1 * 3], &pts[p0 * 3]);
		d->w = 0.0;
		d->next = NULL;
		if (tail)
		    tail->next = d;
		else
		    head = d;
		tail = d;
		nd++;
	    }
	    i += (size_t)cnt;
	}
	if (head && mk_datums(s->wdbp, "vtk.lines.datum", head) == 0) {
	    (void)mk_addmember("vtk.lines.datum", &s->all_head.l, NULL, WMOP_UNION);
	    bu_log("vtk_read: POLYDATA LINES -> %d line datums\n", nd);
	    created++;
	}
	while (head) {
	    struct rt_datum_internal *n = head->next;
	    bu_free(head, "vtk line datum");
	    head = n;
	}
	return created;
    }

    if (BU_STR_EQUAL(mode, "pipe")) {
	for (i = 0; i < connlen;) {
	    int cnt = conn[i++];
	    int k;
	    struct bu_list phead;
	    int added = 0;
	    if (cnt < 0)
		break;
	    if (cnt < 2) {
		i += (size_t)cnt;
		continue;
	    }
	    mk_pipe_init(&phead);
	    for (k = 0; k < cnt; k++) {
		int p = conn[i + k];
		if (p < 0 || (size_t)p >= npts)
		    continue;
		/* od = 2*radius, id = 0, bendradius >= od */
		mk_add_pipe_pnt(&phead, &pts[p * 3], 2.0 * rad, 0.0, 2.0 * rad);
		added++;
	    }
	    if (added >= 2) {
		struct bu_vls pn = BU_VLS_INIT_ZERO;
		bu_vls_sprintf(&pn, "vtk.line%d.pipe", lid++);
		if (mk_pipe(s->wdbp, bu_vls_cstr(&pn), &phead) == 0) {
		    (void)mk_addmember(bu_vls_cstr(&pn), &s->all_head.l, NULL, WMOP_UNION);
		    created++;
		}
		bu_vls_free(&pn);
	    }
	    mk_pipe_free(&phead);
	    i += (size_t)cnt;
	}
	bu_log("vtk_read: POLYDATA LINES -> %d pipe(s)\n", created);
	return created;
    }

    if (BU_STR_EQUAL(mode, "rcc")) {
	for (i = 0; i < connlen;) {
	    int cnt = conn[i++];
	    int k;
	    if (cnt < 0)
		break;
	    for (k = 0; k + 1 < cnt; k++) {
		int p0 = conn[i + k];
		int p1 = conn[i + k + 1];
		vect_t h;
		struct bu_vls cn = BU_VLS_INIT_ZERO;
		if (p0 < 0 || (size_t)p0 >= npts || p1 < 0 || (size_t)p1 >= npts) {
		    continue;
		}
		VSUB2(h, &pts[p1 * 3], &pts[p0 * 3]);
		if (MAGNITUDE(h) <= SMALL_FASTF) {
		    bu_vls_free(&cn);
		    continue;
		}
		bu_vls_sprintf(&cn, "vtk.line%d.rcc", lid++);
		if (mk_rcc(s->wdbp, bu_vls_cstr(&cn), &pts[p0 * 3], h, rad) == 0) {
		    (void)mk_addmember(bu_vls_cstr(&cn), &s->all_head.l, NULL, WMOP_UNION);
		    created++;
		}
		bu_vls_free(&cn);
	    }
	    i += (size_t)cnt;
	}
	bu_log("vtk_read: POLYDATA LINES -> %d rcc(s)\n", created);
	return created;
    }

    bu_log("vtk_read: unknown --lines mode '%s'; skipping LINES\n", mode);
    s->dropped++;
    return 0;
}


/*
 * MAPPING 1/2: DATASET POLYDATA.
 */
static int
vtk_do_polydata(struct vtk_state *s)
{
    fastf_t *pts = NULL;
    size_t npts = 0;
    int created = 0;

    /* growable bot face list */
    int *faces = NULL;
    size_t nfaces = 0, fcap = 0;

    /* deferred VERTICES/LINES connectivity (processed after POINTS) */
    int *vconn = NULL;
    size_t vconnlen = 0;
    int *lconn = NULL;
    size_t lconnlen = 0;

    struct vtk_darray *scalars = NULL, *vectors = NULL;
    int nscalars = 0, nvectors = 0;

    while (vtk_getline(s)) {
	char *argv[16];
	struct bu_vls work = BU_VLS_INIT_ZERO;
	size_t ac;

	if (vtk_line_blank(s)) {
	    bu_vls_free(&work);
	    continue;
	}

	bu_vls_sprintf(&work, "%s", bu_vls_cstr(&s->line));
	ac = bu_argv_from_string(argv, 16, bu_vls_addr(&work));
	if (!ac) {
	    bu_vls_free(&work);
	    continue;
	}

	if (BU_STR_EQUAL(argv[0], "POINTS") && ac >= 2) {
	    npts = (size_t)strtol(argv[1], NULL, 10);
	    pts = vtk_read_points(s, npts);
	    if (!pts)
		npts = 0;

	} else if ((BU_STR_EQUAL(argv[0], "POLYGONS")
		    || BU_STR_EQUAL(argv[0], "TRIANGLE_STRIPS")) && ac >= 3) {
	    int strips = BU_STR_EQUAL(argv[0], "TRIANGLE_STRIPS");
	    int ncells = (int)strtol(argv[1], NULL, 10);
	    int c;
	    for (c = 0; c < ncells; c++) {
		int nidx;
		int *idx;
		int j;
		if (vtk_read_n_ints(s, &nidx, 1) != 1 || nidx < 1)
		    break;
		idx = (int *)bu_malloc(sizeof(int) * nidx, "cell idx");
		if (vtk_read_n_ints(s, idx, nidx) != (size_t)nidx) {
		    bu_free(idx, "cell idx");
		    break;
		}
		if (strips) {
		    /* strip of nidx verts -> nidx-2 triangles */
		    for (j = 0; j + 2 < nidx; j++) {
			int a, b, cc;
			if (j & 1) {
			    a = idx[j + 1]; b = idx[j]; cc = idx[j + 2];
			} else {
			    a = idx[j]; b = idx[j + 1]; cc = idx[j + 2];
			}
			if (nfaces * 3 + 3 > fcap) {
			    fcap = fcap ? fcap * 2 : 128;
			    faces = (int *)bu_realloc(faces, sizeof(int) * fcap * 3, "faces");
			}
			faces[nfaces * 3 + 0] = a;
			faces[nfaces * 3 + 1] = b;
			faces[nfaces * 3 + 2] = cc;
			nfaces++;
		    }
		} else {
		    /* fan-triangulate the n-gon */
		    for (j = 1; j + 1 < nidx; j++) {
			if (nfaces * 3 + 3 > fcap) {
			    fcap = fcap ? fcap * 2 : 128;
			    faces = (int *)bu_realloc(faces, sizeof(int) * fcap * 3, "faces");
			}
			faces[nfaces * 3 + 0] = idx[0];
			faces[nfaces * 3 + 1] = idx[j];
			faces[nfaces * 3 + 2] = idx[j + 1];
			nfaces++;
		    }
		}
		bu_free(idx, "cell idx");
	    }

	} else if ((BU_STR_EQUAL(argv[0], "VERTICES")
		    || BU_STR_EQUAL(argv[0], "LINES")) && ac >= 3) {
	    /* "VERTICES/LINES n size" then size connectivity ints */
	    int total = (int)strtol(argv[2], NULL, 10);
	    int is_lines = BU_STR_EQUAL(argv[0], "LINES");
	    if (total > 0) {
		int *conn = (int *)bu_malloc(sizeof(int) * total, "conn");
		size_t got = vtk_read_n_ints(s, conn, (size_t)total);
		if (is_lines) {
		    lconn = conn;
		    lconnlen = got;
		} else {
		    vconn = conn;
		    vconnlen = got;
		}
	    }

	} else if (BU_STR_EQUAL(argv[0], "FIELD") && ac >= 3) {
	    vtk_read_field(s, (int)strtol(argv[2], NULL, 10));

	} else if (BU_STR_EQUAL(argv[0], "POINT_DATA") && ac >= 2) {
	    size_t nt = (size_t)strtol(argv[1], NULL, 10);
	    vtk_read_data_arrays(s, nt, &scalars, &nscalars, &vectors, &nvectors);

	} else if (BU_STR_EQUAL(argv[0], "CELL_DATA") && ac >= 2) {
	    /* collected but unused for polydata point/bot emission */
	    struct vtk_darray *cs = NULL, *cv = NULL;
	    int ncs = 0, ncv = 0;
	    size_t nt = (size_t)strtol(argv[1], NULL, 10);
	    vtk_read_data_arrays(s, nt, &cs, &ncs, &cv, &ncv);
	    {
		int k;
		for (k = 0; k < ncs; k++) vtk_darray_free(&cs[k]);
		for (k = 0; k < ncv; k++) vtk_darray_free(&cv[k]);
	    }
	    if (cs) bu_free(cs, "cs");
	    if (cv) bu_free(cv, "cv");

	} else {
	    bu_log("vtk_read: skipping POLYDATA section '%s'\n", argv[0]);
	    s->dropped++;
	}

	bu_vls_free(&work);
    }

    /* emit a bot if we built faces */
    if (pts && nfaces > 0) {
	struct bu_vls bn = BU_VLS_INIT_ZERO;
	bu_vls_sprintf(&bn, "vtk.bot");
	if (mk_bot(s->wdbp, bu_vls_cstr(&bn), RT_BOT_SURFACE, RT_BOT_UNORIENTED, 0,
		   npts, nfaces, pts, faces, NULL, NULL) == 0) {
	    (void)mk_addmember(bu_vls_cstr(&bn), &s->all_head.l, NULL, WMOP_UNION);
	    bu_log("vtk_read: POLYDATA -> bot (%zu verts, %zu faces)\n", npts, nfaces);
	    created++;
	}
	bu_vls_free(&bn);
    }

    {
	/* bare points -> pnts (only when no surface was built) */
	int have_pnts = 0;
	if (pts && npts > 0 && nfaces == 0) {
	    struct bu_vls pn = BU_VLS_INIT_ZERO;
	    bu_vls_sprintf(&pn, "vtk.pnts");
	    if (vtk_emit_pnts(s, bu_vls_cstr(&pn), pts, npts,
			      scalars, nscalars, vectors, nvectors)) {
		bu_log("vtk_read: POLYDATA -> pnts (%zu points)\n", npts);
		created++;
		have_pnts = 1;
	    }
	    bu_vls_free(&pn);
	}

	/* deferred VERTICES / LINES connectivity */
	if (pts && vconn && vconnlen > 0)
	    created += vtk_emit_vertices(s, vconn, vconnlen, pts, npts, have_pnts);
	if (pts && lconn && lconnlen > 0)
	    created += vtk_emit_lines(s, lconn, lconnlen, pts, npts);
    }

    if (pts)
	bu_free(pts, "vtk points");
    if (faces)
	bu_free(faces, "faces");
    if (vconn)
	bu_free(vconn, "conn");
    if (lconn)
	bu_free(lconn, "conn");
    {
	int k;
	for (k = 0; k < nscalars; k++) vtk_darray_free(&scalars[k]);
	for (k = 0; k < nvectors; k++) vtk_darray_free(&vectors[k]);
    }
    if (scalars) bu_free(scalars, "scalars");
    if (vectors) bu_free(vectors, "vectors");

    return created;
}


/*
 * MAPPING 3: DATASET UNSTRUCTURED_GRID.
 *
 * POINTS, then CELLS <n> <size> (each: npts i0 i1 ...), then
 * CELL_TYPES <n>.  Solid cells become arbN; surface cells become a bot.
 */
static int
vtk_do_unstructured(struct vtk_state *s)
{
    fastf_t *pts = NULL;
    size_t npts = 0;
    int **cells = NULL;		/* cells[i][0] = count, then indices */
    int *ctypes = NULL;
    int ncells = 0;
    int i;
    int created = 0;
    int *faces = NULL;
    size_t nfaces = 0, fcap = 0;
    const char *cellmode = vtk_optstr(s->opts->cells, "arb");
    int force_bot = BU_STR_EQUAL(cellmode, "bot");

    /* selected CELL_DATA scalar for per-cell coloring */
    fastf_t *cellscalar = NULL;
    size_t ncellscalar = 0;
    double csmin = 0.0, csmax = 0.0;
    int have_cellscalar = 0;

    while (vtk_getline(s)) {
	char *argv[16];
	struct bu_vls work = BU_VLS_INIT_ZERO;
	size_t ac;

	if (vtk_line_blank(s)) {
	    bu_vls_free(&work);
	    continue;
	}

	bu_vls_sprintf(&work, "%s", bu_vls_cstr(&s->line));
	ac = bu_argv_from_string(argv, 16, bu_vls_addr(&work));
	if (!ac) {
	    bu_vls_free(&work);
	    continue;
	}

	if (BU_STR_EQUAL(argv[0], "POINTS") && ac >= 2) {
	    npts = (size_t)strtol(argv[1], NULL, 10);
	    pts = vtk_read_points(s, npts);
	    if (!pts)
		npts = 0;

	} else if (BU_STR_EQUAL(argv[0], "CELLS") && ac >= 3) {
	    int c;
	    ncells = (int)strtol(argv[1], NULL, 10);
	    cells = (int **)bu_calloc(ncells > 0 ? ncells : 1, sizeof(int *), "cells");
	    for (c = 0; c < ncells; c++) {
		int n;
		if (vtk_read_n_ints(s, &n, 1) != 1 || n < 0)
		    break;
		cells[c] = (int *)bu_malloc(sizeof(int) * (n + 1), "cell");
		cells[c][0] = n;
		if (vtk_read_n_ints(s, &cells[c][1], n) != (size_t)n)
		    break;
	    }

	} else if (BU_STR_EQUAL(argv[0], "CELL_TYPES") && ac >= 2) {
	    int n = (int)strtol(argv[1], NULL, 10);
	    ctypes = (int *)bu_malloc(sizeof(int) * (n > 0 ? n : 1), "ctypes");
	    vtk_read_n_ints(s, ctypes, n > 0 ? n : 0);

	} else if (BU_STR_EQUAL(argv[0], "FIELD") && ac >= 3) {
	    vtk_read_field(s, (int)strtol(argv[2], NULL, 10));

	} else if (BU_STR_EQUAL(argv[0], "POINT_DATA")
		   || BU_STR_EQUAL(argv[0], "CELL_DATA")) {
	    int is_cell = BU_STR_EQUAL(argv[0], "CELL_DATA");
	    struct vtk_darray *cs = NULL, *cv = NULL;
	    int ncs = 0, ncv = 0;
	    size_t nt = (ac >= 2) ? (size_t)strtol(argv[1], NULL, 10) : 0;
	    vtk_read_data_arrays(s, nt, &cs, &ncs, &cv, &ncv);
	    /* retain a CELL_DATA scalar for per-cell coloring */
	    if (is_cell && !have_cellscalar) {
		int si = vtk_pick(cs, ncs, s->opts->scalar_array);
		if (si >= 0) {
		    size_t k;
		    ncellscalar = cs[si].ntuple;
		    cellscalar = (fastf_t *)bu_malloc(sizeof(fastf_t) * (ncellscalar + 1),
						      "cell scalar");
		    for (k = 0; k < ncellscalar; k++)
			cellscalar[k] = cs[si].v[k];
		    if (ncellscalar > 0) {
			csmin = csmax = cellscalar[0];
			for (k = 0; k < ncellscalar; k++) {
			    if (cellscalar[k] < csmin) csmin = cellscalar[k];
			    if (cellscalar[k] > csmax) csmax = cellscalar[k];
			}
			have_cellscalar = 1;
			bu_log("vtk_read: per-cell color from CELL_DATA SCALARS '%s'\n",
			       cs[si].name);
		    }
		}
	    }
	    {
		int k;
		for (k = 0; k < ncs; k++) vtk_darray_free(&cs[k]);
		for (k = 0; k < ncv; k++) vtk_darray_free(&cv[k]);
	    }
	    if (cs) bu_free(cs, "cs");
	    if (cv) bu_free(cv, "cv");

	} else {
	    bu_log("vtk_read: skipping UNSTRUCTURED_GRID section '%s'\n", argv[0]);
	    s->dropped++;
	}

	bu_vls_free(&work);
    }

    if (!pts || ncells <= 0 || !ctypes) {
	bu_log("vtk_read: UNSTRUCTURED_GRID missing points/cells/types\n");
	goto cleanup;
    }

    /* cell-max gate: if too many solid cells, fall back to a single bot */
    if (!force_bot) {
	int nsolid = 0;
	for (i = 0; i < ncells; i++) {
	    int t = ctypes[i];
	    if (t == VTK_TETRA || t == VTK_PYRAMID || t == VTK_WEDGE
		|| t == VTK_HEXAHEDRON || t == VTK_VOXEL)
		nsolid++;
	}
	if (nsolid > s->opts->cell_max) {
	    bu_log("vtk_read: %d solid cells exceed --cell-max %d; "
		   "falling back to a single bot\n", nsolid, s->opts->cell_max);
	    force_bot = 1;
	}
    }

    for (i = 0; i < ncells; i++) {
	int *cell = cells[i];
	int type = ctypes[i];
	int n;
	fastf_t apts[8 * 3];
	int j;
	int is_solid;
	struct bu_vls cn = BU_VLS_INIT_ZERO;

	if (!cell)
	    continue;
	n = cell[0];

	is_solid = (type == VTK_TETRA || type == VTK_PYRAMID || type == VTK_WEDGE
		    || type == VTK_HEXAHEDRON || type == VTK_VOXEL);

	/* gather point coords for solid cell makers */
	if (is_solid) {
	    int valid = 1;
	    for (j = 0; j < n && j < 8; j++) {
		int p = cell[1 + j];
		if (p < 0 || (size_t)p >= npts) {
		    valid = 0;
		    break;
		}
		VMOVE(&apts[j * 3], &pts[p * 3]);
	    }
	    if (!valid) {
		bu_vls_free(&cn);
		continue;
	    }
	}

	/* --cells bot (or cell-max fallback): triangulate solid cell
	 * boundary faces into the shared bot instead of an arbN */
	if (is_solid && force_bot) {
	    /* boundary triangles per VTK cell type, in local-vertex order */
	    static const int tet_f[4][3] = {{0,1,3},{1,2,3},{2,0,3},{0,2,1}};
	    static const int pyr_f[6][3] = {{0,1,4},{1,2,4},{2,3,4},{3,0,4},{0,3,1},{1,3,2}};
	    static const int wed_f[8][3] = {{0,1,2},{3,5,4},{0,3,4},{0,4,1},
					    {1,4,5},{1,5,2},{2,5,3},{2,3,0}};
	    static const int hex_f[12][3] = {{0,1,2},{0,2,3},{4,6,5},{4,7,6},
					     {0,4,5},{0,5,1},{1,5,6},{1,6,2},
					     {2,6,7},{2,7,3},{3,7,4},{3,4,0}};
	    const int (*ft)[3] = NULL;
	    int nf = 0;
	    int local[8];
	    int t;
	    /* voxel -> hexahedron local order before face lookup */
	    static const int vox_order[8] = {0, 1, 3, 2, 4, 5, 7, 6};
	    for (j = 0; j < n && j < 8; j++)
		local[j] = cell[1 + j];
	    if (type == VTK_VOXEL) {
		int tmp[8];
		for (j = 0; j < 8; j++)
		    tmp[j] = local[vox_order[j]];
		for (j = 0; j < 8; j++)
		    local[j] = tmp[j];
		type = VTK_HEXAHEDRON;	/* now in hex order */
	    }
	    switch (type) {
		case VTK_TETRA:	   ft = tet_f; nf = 4;  break;
		case VTK_PYRAMID:  ft = pyr_f; nf = 6;  break;
		case VTK_WEDGE:	   ft = wed_f; nf = 8;  break;
		case VTK_HEXAHEDRON: ft = hex_f; nf = 12; break;
		default: break;
	    }
	    for (t = 0; t < nf; t++) {
		if (nfaces * 3 + 3 > fcap) {
		    fcap = fcap ? fcap * 2 : 128;
		    faces = (int *)bu_realloc(faces, sizeof(int) * fcap * 3, "faces");
		}
		faces[nfaces * 3 + 0] = local[ft[t][0]];
		faces[nfaces * 3 + 1] = local[ft[t][1]];
		faces[nfaces * 3 + 2] = local[ft[t][2]];
		nfaces++;
	    }
	    bu_vls_free(&cn);
	    continue;
	}

	switch (type) {
	    case VTK_TETRA:
		bu_vls_sprintf(&cn, "vtk.cell%d.arb4", i);
		if (mk_arb4(s->wdbp, bu_vls_cstr(&cn), apts) == 0) {
		    vtk_add_cell(s, bu_vls_cstr(&cn), have_cellscalar && (size_t)i < ncellscalar,
				 have_cellscalar && (size_t)i < ncellscalar ? cellscalar[i] : 0.0,
				 csmin, csmax);
		    created++;
		}
		break;
	    case VTK_PYRAMID:
		bu_vls_sprintf(&cn, "vtk.cell%d.arb5", i);
		if (mk_arb5(s->wdbp, bu_vls_cstr(&cn), apts) == 0) {
		    vtk_add_cell(s, bu_vls_cstr(&cn), have_cellscalar && (size_t)i < ncellscalar,
				 have_cellscalar && (size_t)i < ncellscalar ? cellscalar[i] : 0.0,
				 csmin, csmax);
		    created++;
		}
		break;
	    case VTK_WEDGE:
		bu_vls_sprintf(&cn, "vtk.cell%d.arb6", i);
		if (mk_arb6(s->wdbp, bu_vls_cstr(&cn), apts) == 0) {
		    vtk_add_cell(s, bu_vls_cstr(&cn), have_cellscalar && (size_t)i < ncellscalar,
				 have_cellscalar && (size_t)i < ncellscalar ? cellscalar[i] : 0.0,
				 csmin, csmax);
		    created++;
		}
		break;
	    case VTK_HEXAHEDRON:
		bu_vls_sprintf(&cn, "vtk.cell%d.arb8", i);
		if (mk_arb8(s->wdbp, bu_vls_cstr(&cn), apts) == 0) {
		    vtk_add_cell(s, bu_vls_cstr(&cn), have_cellscalar && (size_t)i < ncellscalar,
				 have_cellscalar && (size_t)i < ncellscalar ? cellscalar[i] : 0.0,
				 csmin, csmax);
		    created++;
		}
		break;
	    case VTK_VOXEL: {
		/* VTK voxel index order differs from arb8; reorder
		 * 0,1,3,2,4,5,7,6 to a consistent hull */
		fastf_t vpts[8 * 3];
		static const int order[8] = {0, 1, 3, 2, 4, 5, 7, 6};
		for (j = 0; j < 8; j++)
		    VMOVE(&vpts[j * 3], &apts[order[j] * 3]);
		bu_vls_sprintf(&cn, "vtk.cell%d.arb8", i);
		if (mk_arb8(s->wdbp, bu_vls_cstr(&cn), vpts) == 0) {
		    vtk_add_cell(s, bu_vls_cstr(&cn), have_cellscalar && (size_t)i < ncellscalar,
				 have_cellscalar && (size_t)i < ncellscalar ? cellscalar[i] : 0.0,
				 csmin, csmax);
		    created++;
		}
		break;
	    }
	    case VTK_TRIANGLE:
		if (n >= 3) {
		    if (nfaces * 3 + 3 > fcap) {
			fcap = fcap ? fcap * 2 : 128;
			faces = (int *)bu_realloc(faces, sizeof(int) * fcap * 3, "faces");
		    }
		    faces[nfaces * 3 + 0] = cell[1];
		    faces[nfaces * 3 + 1] = cell[2];
		    faces[nfaces * 3 + 2] = cell[3];
		    nfaces++;
		}
		break;
	    case VTK_QUAD:
		if (n >= 4) {
		    int k;
		    for (k = 1; k + 1 < n; k++) {
			if (nfaces * 3 + 3 > fcap) {
			    fcap = fcap ? fcap * 2 : 128;
			    faces = (int *)bu_realloc(faces, sizeof(int) * fcap * 3, "faces");
			}
			faces[nfaces * 3 + 0] = cell[1];
			faces[nfaces * 3 + 1] = cell[1 + k];
			faces[nfaces * 3 + 2] = cell[1 + k + 1];
			nfaces++;
		    }
		}
		break;
	    default:
		bu_log("vtk_read: skipping unsupported cell type %d\n", type);
		s->dropped++;
		break;
	}
	bu_vls_free(&cn);
    }

    if (faces && nfaces > 0) {
	if (mk_bot(s->wdbp, "vtk.cells.bot", RT_BOT_SURFACE, RT_BOT_UNORIENTED, 0,
		   npts, nfaces, pts, faces, NULL, NULL) == 0) {
	    (void)mk_addmember("vtk.cells.bot", &s->all_head.l, NULL, WMOP_UNION);
	    bu_log("vtk_read: UNSTRUCTURED_GRID surface cells -> bot (%zu faces)\n", nfaces);
	    created++;
	}
    }

cleanup:
    if (pts)
	bu_free(pts, "vtk points");
    if (faces)
	bu_free(faces, "faces");
    if (cells) {
	for (i = 0; i < ncells; i++)
	    if (cells[i])
		bu_free(cells[i], "cell");
	bu_free(cells, "cells");
    }
    if (ctypes)
	bu_free(ctypes, "ctypes");
    if (cellscalar)
	bu_free(cellscalar, "cell scalar");

    return created;
}


/*
 * MAPPING 4: DATASET STRUCTURED_POINTS.
 *
 * DIMENSIONS nx ny nz; ORIGIN ox oy oz; SPACING sx sy sz; then
 * POINT_DATA with a SCALARS array -> quantize to uint8, mk_binunif +
 * mk_vol.
 */
static int
vtk_do_structured_points(struct vtk_state *s)
{
    int dim[3] = {0, 0, 0};
    vect_t spacing = {1.0, 1.0, 1.0};
    vect_t origin = {0.0, 0.0, 0.0};
    struct vtk_darray *scalars = NULL, *vectors = NULL;
    int nscalars = 0, nvectors = 0;
    int created = 0;
    size_t total;

    while (vtk_getline(s)) {
	char *argv[16];
	struct bu_vls work = BU_VLS_INIT_ZERO;
	size_t ac;

	if (vtk_line_blank(s)) {
	    bu_vls_free(&work);
	    continue;
	}

	bu_vls_sprintf(&work, "%s", bu_vls_cstr(&s->line));
	ac = bu_argv_from_string(argv, 16, bu_vls_addr(&work));
	if (!ac) {
	    bu_vls_free(&work);
	    continue;
	}

	if (BU_STR_EQUAL(argv[0], "DIMENSIONS") && ac >= 4) {
	    dim[0] = (int)strtol(argv[1], NULL, 10);
	    dim[1] = (int)strtol(argv[2], NULL, 10);
	    dim[2] = (int)strtol(argv[3], NULL, 10);
	} else if ((BU_STR_EQUAL(argv[0], "SPACING")
		    || BU_STR_EQUAL(argv[0], "ASPECT_RATIO")) && ac >= 4) {
	    spacing[0] = strtod(argv[1], NULL);
	    spacing[1] = strtod(argv[2], NULL);
	    spacing[2] = strtod(argv[3], NULL);
	} else if (BU_STR_EQUAL(argv[0], "ORIGIN") && ac >= 4) {
	    origin[0] = strtod(argv[1], NULL);
	    origin[1] = strtod(argv[2], NULL);
	    origin[2] = strtod(argv[3], NULL);
	} else if (BU_STR_EQUAL(argv[0], "FIELD") && ac >= 3) {
	    vtk_read_field(s, (int)strtol(argv[2], NULL, 10));
	} else if (BU_STR_EQUAL(argv[0], "POINT_DATA")
		   || BU_STR_EQUAL(argv[0], "CELL_DATA")) {
	    size_t nt = (ac >= 2) ? (size_t)strtol(argv[1], NULL, 10) : 0;
	    vtk_read_data_arrays(s, nt, &scalars, &nscalars, &vectors, &nvectors);
	} else {
	    bu_log("vtk_read: skipping STRUCTURED_POINTS section '%s'\n", argv[0]);
	    s->dropped++;
	}

	bu_vls_free(&work);
    }

    total = (size_t)dim[0] * dim[1] * dim[2];

    {
	const char *gridmode = vtk_optstr(s->opts->grid, "auto");
	int want_dsp = BU_STR_EQUAL(gridmode, "dsp");
	int want_ebm = BU_STR_EQUAL(gridmode, "ebm");
	/* a field is "2D" when exactly one dimension is 1 */
	int is2d = (dim[0] == 1 || dim[1] == 1 || dim[2] == 1);

	if ((want_dsp || want_ebm) && is2d
	    && nscalars > 0 && total > 0 && scalars[0].ntuple >= total) {
	    /* in-plane dimensions xcnt*ycnt (the two non-unit dims) */
	    int xcnt, ycnt;
	    double min = scalars[0].v[0], max = scalars[0].v[0];
	    size_t i;
	    fastf_t sf = s->gcv_options->scale_factor;
	    mat_t mat;
	    if (sf <= 0.0)
		sf = 1.0;
	    if (dim[2] == 1) {
		xcnt = dim[0]; ycnt = dim[1];
	    } else if (dim[1] == 1) {
		xcnt = dim[0]; ycnt = dim[2];
	    } else {
		xcnt = dim[1]; ycnt = dim[2];
	    }
	    for (i = 0; i < total; i++) {
		if (scalars[0].v[i] < min) min = scalars[0].v[i];
		if (scalars[0].v[i] > max) max = scalars[0].v[i];
	    }

	    if (want_dsp) {
		/* quantize to uint16 elevation, mk_binunif + mk_dsp_obj */
		uint16_t *hdata = (uint16_t *)bu_malloc(sizeof(uint16_t) * total, "dsp data");
		for (i = 0; i < total; i++) {
		    double t = (max > min) ? (scalars[0].v[i] - min) / (max - min) : 0.0;
		    if (t < 0.0) t = 0.0;
		    if (t > 1.0) t = 1.0;
		    hdata[i] = (uint16_t)(t * 65535.0 + 0.5);
		}
		if (mk_binunif(s->wdbp, "vtk.dsp.data", hdata, WDB_BINUNIF_UINT16,
			       (long)total) == 0) {
		    /* stom: map cell (i,j,height) to model space.  Identity
		     * with in-plane spacing on x/y and a vertical scale that
		     * maps the uint16 range to ~max spacing extent. */
		    fastf_t vscale;
		    MAT_IDN(mat);
		    mat[0]  = spacing[0] * sf;	/* x step */
		    mat[5]  = spacing[1] * sf;	/* y step */
		    /* full-height (65535) should rise about the larger of the
		     * in-plane extents so the relief reads visually */
		    vscale = (xcnt > ycnt ? xcnt : ycnt) * spacing[0] * sf / 65535.0;
		    if (vscale <= 0.0)
			vscale = sf / 65535.0;
		    mat[10] = vscale;
		    MAT_DELTAS(mat, origin[0] * sf, origin[1] * sf, origin[2] * sf);
		    if (mk_dsp_obj(s->wdbp, "vtk.dsp", "vtk.dsp.data",
				   (size_t)xcnt, (size_t)ycnt, mat) == 0) {
			(void)mk_addmember("vtk.dsp", &s->all_head.l, NULL, WMOP_UNION);
			bu_log("vtk_read: STRUCTURED_POINTS -> dsp (%dx%d)\n", xcnt, ycnt);
			created++;
		    }
		}
		bu_free(hdata, "dsp data");
	    } else {
		/* ebm: threshold to a uint8 bitmap, mk_binunif + mk_ebm_obj */
		unsigned char *bdata = (unsigned char *)bu_malloc(total, "ebm data");
		double mid = (min + max) * 0.5;
		fastf_t tallness = (xcnt > ycnt ? xcnt : ycnt) * spacing[0] * sf;
		for (i = 0; i < total; i++)
		    bdata[i] = (scalars[0].v[i] >= mid) ? 1 : 0;
		if (tallness <= 0.0)
		    tallness = sf;
		if (mk_binunif(s->wdbp, "vtk.ebm.data", bdata, WDB_BINUNIF_UINT8,
			       (long)total) == 0) {
		    MAT_IDN(mat);
		    /* mk_ebm_obj MAT_COPYs the matrix; never pass NULL */
		    MAT_DELTAS(mat, origin[0] * sf, origin[1] * sf, origin[2] * sf);
		    if (mk_ebm_obj(s->wdbp, "vtk.ebm", "vtk.ebm.data",
				   (size_t)xcnt, (size_t)ycnt, tallness, mat) == 0) {
			(void)mk_addmember("vtk.ebm", &s->all_head.l, NULL, WMOP_UNION);
			bu_log("vtk_read: STRUCTURED_POINTS -> ebm (%dx%d, tallness %g)\n",
			       xcnt, ycnt, tallness);
			created++;
		    }
		}
		bu_free(bdata, "ebm data");
	    }
	    goto sp_done;
	}

	if ((want_dsp || want_ebm) && !is2d)
	    bu_log("vtk_read: --grid=%s requires a 2D field; using vol instead\n", gridmode);
    }

    if (nscalars > 0 && total > 0 && scalars[0].ntuple >= total) {
	unsigned char *data = (unsigned char *)bu_malloc(total, "vol data");
	double min = scalars[0].v[0], max = scalars[0].v[0];
	size_t i;
	vect_t cellsize;
	mat_t vmat;
	fastf_t sf = s->gcv_options->scale_factor;
	if (sf <= 0.0)
	    sf = 1.0;
	for (i = 0; i < total; i++) {
	    if (scalars[0].v[i] < min) min = scalars[0].v[i];
	    if (scalars[0].v[i] > max) max = scalars[0].v[i];
	}
	for (i = 0; i < total; i++)
	    data[i] = vtk_quantize(scalars[0].v[i], min, max);

	if (mk_binunif(s->wdbp, "vtk.vol.data", data, WDB_BINUNIF_UINT8, (long)total) == 0) {
	    VSCALE(cellsize, spacing, sf);
	    /* place the volume at ORIGIN (mk_vol dereferences the matrix,
	     * so it must never be NULL) */
	    MAT_IDN(vmat);
	    MAT_DELTAS(vmat, origin[0] * sf, origin[1] * sf, origin[2] * sf);
	    /* threshold: voxel "on" for the upper half of the range */
	    if (mk_vol(s->wdbp, "vtk.vol", 'o', "vtk.vol.data",
		       (size_t)dim[0], (size_t)dim[1], (size_t)dim[2],
		       128, 255, cellsize, vmat) == 0) {
		(void)mk_addmember("vtk.vol", &s->all_head.l, NULL, WMOP_UNION);
		bu_log("vtk_read: STRUCTURED_POINTS -> vol (%dx%dx%d, threshold 128..255)\n",
		       dim[0], dim[1], dim[2]);
		created++;
	    }
	}
	bu_free(data, "vol data");
    } else {
	bu_log("vtk_read: STRUCTURED_POINTS missing usable scalar data\n");
    }

sp_done:
    {
	int k;
	for (k = 0; k < nscalars; k++) vtk_darray_free(&scalars[k]);
	for (k = 0; k < nvectors; k++) vtk_darray_free(&vectors[k]);
    }
    if (scalars) bu_free(scalars, "scalars");
    if (vectors) bu_free(vectors, "vectors");

    return created;
}


/*
 * MAPPING 5: DATASET RECTILINEAR_GRID.
 *
 * DIMENSIONS nx ny nz; X_COORDINATES/Y_COORDINATES/Z_COORDINATES (each
 * "n type" then n values); then POINT_DATA/CELL_DATA.  Each cell becomes
 * an axis-aligned rpp; over --cell-max it falls back to a single bot of
 * cell boundaries.  A selected CELL_DATA scalar colors each cell.
 */
static int
vtk_do_rectilinear(struct vtk_state *s)
{
    int dim[3] = {0, 0, 0};
    fastf_t *xc = NULL, *yc = NULL, *zc = NULL;
    size_t nx = 0, ny = 0, nz = 0;
    int created = 0;
    fastf_t sf = s->gcv_options->scale_factor;
    const char *gridmode;
    int want_bot;

    fastf_t *cellscalar = NULL;
    size_t ncellscalar = 0;
    double csmin = 0.0, csmax = 0.0;
    int have_cellscalar = 0;

    size_t cx, cy, cz, cellidx;
    size_t ncellx, ncelly, ncellz, ncell;

    if (sf <= 0.0)
	sf = 1.0;

    while (vtk_getline(s)) {
	char *argv[16];
	struct bu_vls work = BU_VLS_INIT_ZERO;
	size_t ac;

	if (vtk_line_blank(s)) {
	    bu_vls_free(&work);
	    continue;
	}

	bu_vls_sprintf(&work, "%s", bu_vls_cstr(&s->line));
	ac = bu_argv_from_string(argv, 16, bu_vls_addr(&work));
	if (!ac) {
	    bu_vls_free(&work);
	    continue;
	}

	if (BU_STR_EQUAL(argv[0], "DIMENSIONS") && ac >= 4) {
	    dim[0] = (int)strtol(argv[1], NULL, 10);
	    dim[1] = (int)strtol(argv[2], NULL, 10);
	    dim[2] = (int)strtol(argv[3], NULL, 10);
	} else if (BU_STR_EQUAL(argv[0], "X_COORDINATES") && ac >= 2) {
	    nx = (size_t)strtol(argv[1], NULL, 10);
	    xc = (fastf_t *)bu_malloc(sizeof(fastf_t) * (nx ? nx : 1), "xc");
	    vtk_read_n_doubles(s, xc, nx);
	} else if (BU_STR_EQUAL(argv[0], "Y_COORDINATES") && ac >= 2) {
	    ny = (size_t)strtol(argv[1], NULL, 10);
	    yc = (fastf_t *)bu_malloc(sizeof(fastf_t) * (ny ? ny : 1), "yc");
	    vtk_read_n_doubles(s, yc, ny);
	} else if (BU_STR_EQUAL(argv[0], "Z_COORDINATES") && ac >= 2) {
	    nz = (size_t)strtol(argv[1], NULL, 10);
	    zc = (fastf_t *)bu_malloc(sizeof(fastf_t) * (nz ? nz : 1), "zc");
	    vtk_read_n_doubles(s, zc, nz);
	} else if (BU_STR_EQUAL(argv[0], "FIELD") && ac >= 3) {
	    vtk_read_field(s, (int)strtol(argv[2], NULL, 10));
	} else if (BU_STR_EQUAL(argv[0], "POINT_DATA")
		   || BU_STR_EQUAL(argv[0], "CELL_DATA")) {
	    int is_cell = BU_STR_EQUAL(argv[0], "CELL_DATA");
	    struct vtk_darray *cs = NULL, *cv = NULL;
	    int ncs = 0, ncv = 0;
	    size_t nt = (ac >= 2) ? (size_t)strtol(argv[1], NULL, 10) : 0;
	    vtk_read_data_arrays(s, nt, &cs, &ncs, &cv, &ncv);
	    if (is_cell && !have_cellscalar) {
		int si = vtk_pick(cs, ncs, s->opts->scalar_array);
		if (si >= 0 && cs[si].ntuple > 0) {
		    size_t k;
		    ncellscalar = cs[si].ntuple;
		    cellscalar = (fastf_t *)bu_malloc(sizeof(fastf_t) * ncellscalar, "rg cell scalar");
		    csmin = csmax = cs[si].v[0];
		    for (k = 0; k < ncellscalar; k++) {
			cellscalar[k] = cs[si].v[k];
			if (cellscalar[k] < csmin) csmin = cellscalar[k];
			if (cellscalar[k] > csmax) csmax = cellscalar[k];
		    }
		    have_cellscalar = 1;
		    bu_log("vtk_read: per-cell color from CELL_DATA SCALARS '%s'\n", cs[si].name);
		}
	    }
	    {
		int k;
		for (k = 0; k < ncs; k++) vtk_darray_free(&cs[k]);
		for (k = 0; k < ncv; k++) vtk_darray_free(&cv[k]);
	    }
	    if (cs) bu_free(cs, "cs");
	    if (cv) bu_free(cv, "cv");
	} else {
	    bu_log("vtk_read: skipping RECTILINEAR_GRID section '%s'\n", argv[0]);
	    s->dropped++;
	}

	bu_vls_free(&work);
    }

    if (!xc || !yc || !zc || dim[0] < 1 || dim[1] < 1 || dim[2] < 1
	|| nx < (size_t)dim[0] || ny < (size_t)dim[1] || nz < (size_t)dim[2]) {
	bu_log("vtk_read: RECTILINEAR_GRID missing dimensions/coordinates\n");
	goto rg_cleanup;
    }

    ncellx = (dim[0] > 1) ? (size_t)dim[0] - 1 : 0;
    ncelly = (dim[1] > 1) ? (size_t)dim[1] - 1 : 0;
    ncellz = (dim[2] > 1) ? (size_t)dim[2] - 1 : 0;
    ncell = ncellx * ncelly * ncellz;

    gridmode = vtk_optstr(s->opts->grid, "auto");
    /* auto/cells -> per-cell rpp; over cell-max -> bot fallback */
    want_bot = 0;
    if (BU_STR_EQUAL(gridmode, "vol") || BU_STR_EQUAL(gridmode, "dsp")
	|| BU_STR_EQUAL(gridmode, "ebm"))
	bu_log("vtk_read: --grid=%s not supported for RECTILINEAR_GRID; using cells\n", gridmode);
    if (ncell == 0) {
	bu_log("vtk_read: RECTILINEAR_GRID has no cells\n");
	goto rg_cleanup;
    }
    if ((long)ncell > (long)s->opts->cell_max) {
	bu_log("vtk_read: %zu rectilinear cells exceed --cell-max %d; "
	       "falling back to a single bot\n", ncell, s->opts->cell_max);
	want_bot = 1;
    }

    if (!want_bot) {
	cellidx = 0;
	for (cz = 0; cz < (ncellz ? ncellz : 1); cz++) {
	    for (cy = 0; cy < (ncelly ? ncelly : 1); cy++) {
		for (cx = 0; cx < (ncellx ? ncellx : 1); cx++) {
		    point_t mn, mx;
		    struct bu_vls cn = BU_VLS_INIT_ZERO;
		    size_t x1 = (ncellx ? cx + 1 : cx);
		    size_t y1 = (ncelly ? cy + 1 : cy);
		    size_t z1 = (ncellz ? cz + 1 : cz);
		    VSET(mn, xc[cx] * sf, yc[cy] * sf, zc[cz] * sf);
		    VSET(mx, xc[x1] * sf, yc[y1] * sf, zc[z1] * sf);
		    bu_vls_sprintf(&cn, "vtk.rcell%zu.rpp", cellidx);
		    if (mk_rpp(s->wdbp, bu_vls_cstr(&cn), mn, mx) == 0) {
			vtk_add_cell(s, bu_vls_cstr(&cn),
				     have_cellscalar && cellidx < ncellscalar,
				     (have_cellscalar && cellidx < ncellscalar) ? cellscalar[cellidx] : 0.0,
				     csmin, csmax);
			created++;
		    }
		    bu_vls_free(&cn);
		    cellidx++;
		}
	    }
	}
	bu_log("vtk_read: RECTILINEAR_GRID -> %d rpp cell(s)\n", created);
    } else {
	/* fallback: a single bot of the outer cell boundaries (axis box
	 * faces of each cell), sharing a deduplicated vertex grid */
	size_t i, j, k;
	size_t gv = (size_t)dim[0] * dim[1] * dim[2];
	fastf_t *verts = (fastf_t *)bu_malloc(sizeof(fastf_t) * gv * 3, "rg verts");
	int *faces = NULL;
	size_t nfaces = 0, fcap = 0;
#define RG_VID(ii,jj,kk) (((kk) * (size_t)dim[1] + (jj)) * (size_t)dim[0] + (ii))
	for (k = 0; k < (size_t)dim[2]; k++)
	    for (j = 0; j < (size_t)dim[1]; j++)
		for (i = 0; i < (size_t)dim[0]; i++) {
		    size_t v = RG_VID(i, j, k);
		    verts[v * 3 + 0] = xc[i] * sf;
		    verts[v * 3 + 1] = yc[j] * sf;
		    verts[v * 3 + 2] = zc[k] * sf;
		}
	/* emit two triangles per cell face (all 6 faces of every cell) */
	for (cz = 0; cz < (ncellz ? ncellz : 1); cz++)
	    for (cy = 0; cy < (ncelly ? ncelly : 1); cy++)
		for (cx = 0; cx < (ncellx ? ncellx : 1); cx++) {
		    size_t x1 = (ncellx ? cx + 1 : cx);
		    size_t y1 = (ncelly ? cy + 1 : cy);
		    size_t z1 = (ncellz ? cz + 1 : cz);
		    size_t v[8];
		    static const int quads[6][4] = {
			{0,2,3,1}, {4,5,7,6},	/* -z, +z */
			{0,1,5,4}, {2,6,7,3},	/* -y, +y */
			{0,4,6,2}, {1,3,7,5}	/* -x, +x */
		    };
		    int q;
		    v[0] = RG_VID(cx, cy, cz);   v[1] = RG_VID(x1, cy, cz);
		    v[2] = RG_VID(cx, y1, cz);   v[3] = RG_VID(x1, y1, cz);
		    v[4] = RG_VID(cx, cy, z1);   v[5] = RG_VID(x1, cy, z1);
		    v[6] = RG_VID(cx, y1, z1);   v[7] = RG_VID(x1, y1, z1);
		    for (q = 0; q < 6; q++) {
			int tri[2][3] = {
			    {quads[q][0], quads[q][1], quads[q][2]},
			    {quads[q][0], quads[q][2], quads[q][3]}
			};
			int tt;
			for (tt = 0; tt < 2; tt++) {
			    if (nfaces * 3 + 3 > fcap) {
				fcap = fcap ? fcap * 2 : 256;
				faces = (int *)bu_realloc(faces, sizeof(int) * fcap * 3, "rg faces");
			    }
			    faces[nfaces * 3 + 0] = (int)v[tri[tt][0]];
			    faces[nfaces * 3 + 1] = (int)v[tri[tt][1]];
			    faces[nfaces * 3 + 2] = (int)v[tri[tt][2]];
			    nfaces++;
			}
		    }
		}
#undef RG_VID
	if (faces && nfaces > 0
	    && mk_bot(s->wdbp, "vtk.rgrid.bot", RT_BOT_SURFACE, RT_BOT_UNORIENTED, 0,
		      gv, nfaces, verts, faces, NULL, NULL) == 0) {
	    (void)mk_addmember("vtk.rgrid.bot", &s->all_head.l, NULL, WMOP_UNION);
	    bu_log("vtk_read: RECTILINEAR_GRID -> bot (%zu faces)\n", nfaces);
	    created++;
	}
	if (faces)
	    bu_free(faces, "rg faces");
	bu_free(verts, "rg verts");
    }

rg_cleanup:
    if (xc) bu_free(xc, "xc");
    if (yc) bu_free(yc, "yc");
    if (zc) bu_free(zc, "zc");
    if (cellscalar) bu_free(cellscalar, "rg cell scalar");

    return created;
}


/*
 * MAPPING 6: DATASET STRUCTURED_GRID.
 *
 * DIMENSIONS nx ny nz; POINTS nx*ny*nz (curvilinear).  Each hex cell ->
 * an arb8 from its 8 ijk corner points; over --cell-max -> warn + skip.
 */
static int
vtk_do_structured_grid(struct vtk_state *s)
{
    int dim[3] = {0, 0, 0};
    fastf_t *pts = NULL;
    size_t npts = 0;
    int created = 0;

    fastf_t *cellscalar = NULL;
    size_t ncellscalar = 0;
    double csmin = 0.0, csmax = 0.0;
    int have_cellscalar = 0;

    size_t cx, cy, cz, cellidx;
    size_t ncellx, ncelly, ncellz, ncell;

    while (vtk_getline(s)) {
	char *argv[16];
	struct bu_vls work = BU_VLS_INIT_ZERO;
	size_t ac;

	if (vtk_line_blank(s)) {
	    bu_vls_free(&work);
	    continue;
	}

	bu_vls_sprintf(&work, "%s", bu_vls_cstr(&s->line));
	ac = bu_argv_from_string(argv, 16, bu_vls_addr(&work));
	if (!ac) {
	    bu_vls_free(&work);
	    continue;
	}

	if (BU_STR_EQUAL(argv[0], "DIMENSIONS") && ac >= 4) {
	    dim[0] = (int)strtol(argv[1], NULL, 10);
	    dim[1] = (int)strtol(argv[2], NULL, 10);
	    dim[2] = (int)strtol(argv[3], NULL, 10);
	} else if (BU_STR_EQUAL(argv[0], "POINTS") && ac >= 2) {
	    npts = (size_t)strtol(argv[1], NULL, 10);
	    pts = vtk_read_points(s, npts);
	    if (!pts)
		npts = 0;
	} else if (BU_STR_EQUAL(argv[0], "FIELD") && ac >= 3) {
	    vtk_read_field(s, (int)strtol(argv[2], NULL, 10));
	} else if (BU_STR_EQUAL(argv[0], "POINT_DATA")
		   || BU_STR_EQUAL(argv[0], "CELL_DATA")) {
	    int is_cell = BU_STR_EQUAL(argv[0], "CELL_DATA");
	    struct vtk_darray *cs = NULL, *cv = NULL;
	    int ncs = 0, ncv = 0;
	    size_t nt = (ac >= 2) ? (size_t)strtol(argv[1], NULL, 10) : 0;
	    vtk_read_data_arrays(s, nt, &cs, &ncs, &cv, &ncv);
	    if (is_cell && !have_cellscalar) {
		int si = vtk_pick(cs, ncs, s->opts->scalar_array);
		if (si >= 0 && cs[si].ntuple > 0) {
		    size_t k;
		    ncellscalar = cs[si].ntuple;
		    cellscalar = (fastf_t *)bu_malloc(sizeof(fastf_t) * ncellscalar, "sg cell scalar");
		    csmin = csmax = cs[si].v[0];
		    for (k = 0; k < ncellscalar; k++) {
			cellscalar[k] = cs[si].v[k];
			if (cellscalar[k] < csmin) csmin = cellscalar[k];
			if (cellscalar[k] > csmax) csmax = cellscalar[k];
		    }
		    have_cellscalar = 1;
		    bu_log("vtk_read: per-cell color from CELL_DATA SCALARS '%s'\n", cs[si].name);
		}
	    }
	    {
		int k;
		for (k = 0; k < ncs; k++) vtk_darray_free(&cs[k]);
		for (k = 0; k < ncv; k++) vtk_darray_free(&cv[k]);
	    }
	    if (cs) bu_free(cs, "cs");
	    if (cv) bu_free(cv, "cv");
	} else {
	    bu_log("vtk_read: skipping STRUCTURED_GRID section '%s'\n", argv[0]);
	    s->dropped++;
	}

	bu_vls_free(&work);
    }

    if (!pts || dim[0] < 1 || dim[1] < 1 || dim[2] < 1
	|| npts < (size_t)dim[0] * dim[1] * dim[2]) {
	bu_log("vtk_read: STRUCTURED_GRID missing dimensions/points\n");
	goto sg_cleanup;
    }

    ncellx = (dim[0] > 1) ? (size_t)dim[0] - 1 : 0;
    ncelly = (dim[1] > 1) ? (size_t)dim[1] - 1 : 0;
    ncellz = (dim[2] > 1) ? (size_t)dim[2] - 1 : 0;
    ncell = (ncellx ? ncellx : 1) * (ncelly ? ncelly : 1) * (ncellz ? ncellz : 1);
    if (ncellx == 0 && ncelly == 0 && ncellz == 0)
	ncell = 0;

    if (ncell == 0) {
	bu_log("vtk_read: STRUCTURED_GRID has no 3D cells\n");
	goto sg_cleanup;
    }
    if ((long)ncell > (long)s->opts->cell_max) {
	bu_log("vtk_read: %zu structured cells exceed --cell-max %d; skipping\n",
	       ncell, s->opts->cell_max);
	s->dropped++;
	goto sg_cleanup;
    }

#define SG_VID(ii,jj,kk) (((kk) * (size_t)dim[1] + (jj)) * (size_t)dim[0] + (ii))
    cellidx = 0;
    for (cz = 0; cz < (ncellz ? ncellz : 1); cz++) {
	for (cy = 0; cy < (ncelly ? ncelly : 1); cy++) {
	    for (cx = 0; cx < (ncellx ? ncellx : 1); cx++) {
		size_t x1 = (ncellx ? cx + 1 : cx);
		size_t y1 = (ncelly ? cy + 1 : cy);
		size_t z1 = (ncellz ? cz + 1 : cz);
		/* arb8 corner order: bottom face 0-1-2-3 CCW, top 4-5-6-7 */
		size_t c[8];
		fastf_t apts[8 * 3];
		struct bu_vls cn = BU_VLS_INIT_ZERO;
		int m;
		c[0] = SG_VID(cx, cy, cz);  c[1] = SG_VID(x1, cy, cz);
		c[2] = SG_VID(x1, y1, cz);  c[3] = SG_VID(cx, y1, cz);
		c[4] = SG_VID(cx, cy, z1);  c[5] = SG_VID(x1, cy, z1);
		c[6] = SG_VID(x1, y1, z1);  c[7] = SG_VID(cx, y1, z1);
		for (m = 0; m < 8; m++)
		    VMOVE(&apts[m * 3], &pts[c[m] * 3]);
		bu_vls_sprintf(&cn, "vtk.scell%zu.arb8", cellidx);
		if (mk_arb8(s->wdbp, bu_vls_cstr(&cn), apts) == 0) {
		    vtk_add_cell(s, bu_vls_cstr(&cn),
				 have_cellscalar && cellidx < ncellscalar,
				 (have_cellscalar && cellidx < ncellscalar) ? cellscalar[cellidx] : 0.0,
				 csmin, csmax);
		    created++;
		}
		bu_vls_free(&cn);
		cellidx++;
	    }
	}
    }
#undef SG_VID
    bu_log("vtk_read: STRUCTURED_GRID -> %d arb8 cell(s)\n", created);

sg_cleanup:
    if (pts) bu_free(pts, "vtk points");
    if (cellscalar) bu_free(cellscalar, "sg cell scalar");

    return created;
}


static void
vtk_read_create_opts(struct bu_opt_desc **options_desc, void **dest_options_data)
{
    struct vtk_read_options *options_data;

    BU_ALLOC(options_data, struct vtk_read_options);
    *dest_options_data = options_data;
    *options_desc = (struct bu_opt_desc *)bu_malloc(13 * sizeof(struct bu_opt_desc), "options_desc");

    options_data->scalar_array = NULL;
    options_data->vector_array = NULL;
    options_data->point_scale = 1.0;
    options_data->units = NULL;	/* NULL -> default "mm" at use */

    options_data->vertices = NULL;	/* NULL -> "pnts" at use */
    options_data->vertex_radius = 0.0;
    options_data->vertex_max = 5000;
    options_data->lines = NULL;		/* NULL -> "datum" at use */
    options_data->line_radius = 0.0;
    options_data->cells = NULL;		/* NULL -> "arb" at use */
    options_data->cell_max = 50000;
    options_data->grid = NULL;		/* NULL -> "auto" at use */

    BU_OPT((*options_desc)[0], NULL, "scalar-array", "name",
	    bu_opt_str, &options_data->scalar_array,
	    "name of the POINT_DATA SCALARS array to drive per-point scale/color");

    BU_OPT((*options_desc)[1], NULL, "vector-array", "name",
	    bu_opt_str, &options_data->vector_array,
	    "name of the POINT_DATA VECTORS array to drive per-point normals");

    BU_OPT((*options_desc)[2], NULL, "point-scale", "factor",
	    bu_opt_fastf_t, &options_data->point_scale,
	    "multiplier applied to per-point scale values (default 1.0)");

    BU_OPT((*options_desc)[3], NULL, "units", "unit",
	    bu_opt_str, &options_data->units,
	    "units of the input model (default mm)");

    BU_OPT((*options_desc)[4], NULL, "vertices", "datum|pnts|sph",
	    bu_opt_str, &options_data->vertices,
	    "how POLYDATA VERTICES map: datum, pnts (default), or sph");

    BU_OPT((*options_desc)[5], NULL, "vertex-radius", "factor",
	    bu_opt_fastf_t, &options_data->vertex_radius,
	    "sphere radius for --vertices sph (default 0 -> auto)");

    BU_OPT((*options_desc)[6], NULL, "vertex-max", "n",
	    bu_opt_int, &options_data->vertex_max,
	    "max per-vertex solids before falling back to pnts (default 5000)");

    BU_OPT((*options_desc)[7], NULL, "lines", "datum|pipe|rcc",
	    bu_opt_str, &options_data->lines,
	    "how POLYDATA LINES map: datum (default), pipe, or rcc");

    BU_OPT((*options_desc)[8], NULL, "line-radius", "factor",
	    bu_opt_fastf_t, &options_data->line_radius,
	    "pipe/rcc radius for --lines (default 0 -> auto)");

    BU_OPT((*options_desc)[9], NULL, "cells", "arb|bot",
	    bu_opt_str, &options_data->cells,
	    "how solid cells map: arb (default) or bot");

    BU_OPT((*options_desc)[10], NULL, "cell-max", "n",
	    bu_opt_int, &options_data->cell_max,
	    "max per-cell solids before falling back to a single bot (default 50000)");

    BU_OPT((*options_desc)[11], NULL, "grid", "auto|vol|dsp|ebm|cells",
	    bu_opt_str, &options_data->grid,
	    "how structured/rectilinear grids map: auto (default), vol, dsp, ebm, or cells");

    BU_OPT_NULL((*options_desc)[12]);
}


static void
vtk_read_free_opts(void *options_data)
{
    /* scalar_array/vector_array/units are non-owning pointers into the
     * caller's argv strings (bu_opt_str does not copy), so only the
     * options struct itself is freed here. */
    bu_free(options_data, "options_data");
}


/*
 * Recognition: legacy ASCII/binary header, or XML.  Returns 1 if this
 * looks like a VTK file we should claim (so vtk_read can emit a precise
 * message even for the formats we do not yet handle).
 */
static int
vtk_can_read(const char *data)
{
    FILE *fp;
    char buf[64];
    int ret = 0;

    if (!data)
	return 0;

    fp = fopen(data, "rb");
    if (!fp)
	return 0;

    if (bu_fgets(buf, sizeof(buf), fp) != NULL) {
	if (bu_strncmp(buf, "# vtk DataFile Version", 22) == 0)
	    ret = 1;
	else if (bu_strncmp(buf, "<?xml", 5) == 0)
	    ret = 1;
	else if (bu_strncmp(buf, "<VTKFile", 8) == 0)
	    ret = 1;
    }

    fclose(fp);
    return ret;
}


static int
vtk_read(struct gcv_context *context, const struct gcv_opts *gcv_options,
	 const void *options_data, const char *source_path)
{
    struct vtk_state s;
    struct vtk_read_options *opts = (struct vtk_read_options *)options_data;
    struct bu_vls dtype = BU_VLS_INIT_ZERO;
    int created = 0;

    memset(&s, 0, sizeof(s));
    s.gcv_options = gcv_options;
    s.opts = opts;
    s.input_file = source_path;
    bu_vls_init(&s.title);
    bu_vls_init(&s.dataset);
    bu_vls_init(&s.line);
    bu_vls_init(&s.tok_line);

    s.wdbp = wdb_dbopen(context->dbip, RT_WDB_TYPE_DB_INMEM);

    if ((s.fp = fopen(source_path, "rb")) == NULL) {
	bu_log("vtk_read: cannot open input file (%s)\n", source_path);
	goto fail;
    }

    /* XML reject (check first line raw) */
    {
	char hdr[64];
	long pos = ftell(s.fp);
	if (bu_fgets(hdr, sizeof(hdr), s.fp) != NULL) {
	    if (bu_strncmp(hdr, "<?xml", 5) == 0 || bu_strncmp(hdr, "<VTKFile", 8) == 0) {
		bu_log("vtk_read: XML VTK (.vtp/.vtu/...) is not yet supported; "
		       "only legacy ASCII VTK.\n");
		goto fail;
	    }
	}
	fseek(s.fp, pos, SEEK_SET);
    }

    /* line 1: header (already validated by can_read; consume) */
    if (!vtk_getline(&s)) {
	bu_log("vtk_read: empty file (%s)\n", source_path);
	goto fail;
    }
    if (bu_strncmp(bu_vls_cstr(&s.line), "# vtk DataFile Version", 22) != 0) {
	bu_log("vtk_read: not a legacy VTK file (%s)\n", source_path);
	goto fail;
    }

    /* line 2: title */
    if (!vtk_getline(&s))
	goto fail;
    bu_vls_sprintf(&s.title, "%s", bu_vls_cstr(&s.line));

    /* line 3: ASCII | BINARY */
    if (!vtk_getline(&s))
	goto fail;
    bu_vls_trimspace(&s.line);
    if (BU_STR_EQUAL(bu_vls_cstr(&s.line), "BINARY")) {
	bu_log("vtk_read: Binary legacy VTK is not yet supported. Re-export as ASCII "
	       "(ParaView: Save Data -> ASCII; or vtkDataSetWriter::SetFileTypeToASCII). "
	       "File: %s\n", source_path);
	goto fail;
    }
    if (!BU_STR_EQUAL(bu_vls_cstr(&s.line), "ASCII")) {
	bu_log("vtk_read: expected 'ASCII' on line 3, got '%s'\n", bu_vls_cstr(&s.line));
	goto fail;
    }

    /* line 4: DATASET <type> */
    if (!vtk_getline(&s))
	goto fail;
    {
	char *argv[8];
	struct bu_vls work = BU_VLS_INIT_ZERO;
	size_t ac;
	bu_vls_sprintf(&work, "%s", bu_vls_cstr(&s.line));
	ac = bu_argv_from_string(argv, 8, bu_vls_addr(&work));
	if (ac < 2 || !BU_STR_EQUAL(argv[0], "DATASET")) {
	    bu_log("vtk_read: expected 'DATASET <type>' on line 4\n");
	    bu_vls_free(&work);
	    goto fail;
	}
	bu_vls_sprintf(&dtype, "%s", argv[1]);
	bu_vls_sprintf(&s.dataset, "%s", argv[1]);
	bu_vls_free(&work);
    }

    BU_LIST_INIT(&s.all_head.l);

    mk_id_units(s.wdbp, bu_vls_cstr(&s.title),
		(opts && opts->units) ? opts->units : "mm");

    bu_log("vtk_read: legacy ASCII VTK, DATASET %s\n", bu_vls_cstr(&dtype));

    if (BU_STR_EQUAL(bu_vls_cstr(&dtype), "POLYDATA")) {
	created = vtk_do_polydata(&s);
    } else if (BU_STR_EQUAL(bu_vls_cstr(&dtype), "UNSTRUCTURED_GRID")) {
	created = vtk_do_unstructured(&s);
    } else if (BU_STR_EQUAL(bu_vls_cstr(&dtype), "STRUCTURED_POINTS")) {
	created = vtk_do_structured_points(&s);
    } else if (BU_STR_EQUAL(bu_vls_cstr(&dtype), "RECTILINEAR_GRID")) {
	created = vtk_do_rectilinear(&s);
    } else if (BU_STR_EQUAL(bu_vls_cstr(&dtype), "STRUCTURED_GRID")) {
	created = vtk_do_structured_grid(&s);
    } else {
	bu_log("vtk_read: DATASET %s is not yet supported.\n", bu_vls_cstr(&dtype));
	goto fail;
    }

    /* top-level group */
    mk_lcomb(s.wdbp, "all", &s.all_head, 0, (char *)NULL, (char *)NULL,
	     (unsigned char *)NULL, 0);

    /* record provenance */
    vtk_set_attr(&s, "all", "importer", "gcv-vtk");
    vtk_set_attr(&s, "all", "vtk_dataset", bu_vls_cstr(&dtype));
    if (bu_vls_strlen(&s.title))
	vtk_set_attr(&s, "all", "vtk_header", bu_vls_cstr(&s.title));

    vtk_flush_attrs(&s);

    if (s.dropped)
	bu_log("vtk_read: %d section(s)/array(s) were not handled and were skipped.\n",
	       s.dropped);

    if (!created)
	bu_log("vtk_read: warning, no geometry was created from %s\n", source_path);

    fclose(s.fp);
    bu_vls_free(&dtype);
    bu_vls_free(&s.title);
    bu_vls_free(&s.dataset);
    bu_vls_free(&s.line);
    bu_vls_free(&s.tok_line);
    return created ? 1 : 0;

fail:
    if (s.fp)
	fclose(s.fp);
    if (s.pending_init)
	bu_avs_free(&s.pending_attrs);
    bu_vls_free(&dtype);
    bu_vls_free(&s.title);
    bu_vls_free(&s.dataset);
    bu_vls_free(&s.line);
    bu_vls_free(&s.tok_line);
    return 0;
}


static const struct gcv_filter gcv_conv_vtk_read = {
    "VTK Reader", GCV_FILTER_READ, BU_MIME_MODEL_VND_VTK, vtk_can_read,
    vtk_read_create_opts, vtk_read_free_opts, vtk_read
};

static const struct gcv_filter gcv_conv_vtk_read_auto = {
    "VTK Reader (auto)", GCV_FILTER_READ, BU_MIME_MODEL_AUTO, vtk_can_read,
    vtk_read_create_opts, vtk_read_free_opts, vtk_read
};


static const struct gcv_filter * const filters[] = {
    &gcv_conv_vtk_read, &gcv_conv_vtk_read_auto, NULL
};

const struct gcv_plugin gcv_plugin_info_s = { filters };

COMPILER_DLLEXPORT const struct gcv_plugin *
gcv_plugin_info(void){ return &gcv_plugin_info_s; }

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
