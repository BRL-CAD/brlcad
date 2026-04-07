/*                       G - S C A D . C
 * BRL-CAD
 *
 * Copyright (c) 2025 United States Government as represented by
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
/** @file csg/g-scad.c
 *
 * Convert BRL-CAD .g format to OpenSCAD .scad format.
 *
 * Walks the CSG tree preserving boolean structure, transforms, and
 * primitive geometry.  Primitives with direct OpenSCAD equivalents
 * (sphere, cylinder, cube-like ARBs) are emitted analytically.
 * Others are tessellated to polyhedron().
 *
 */

#include "common.h"

#include <stdlib.h>
#include <math.h>
#include <string.h>
#include "bio.h"

#include "bu/app.h"
#include "bu/getopt.h"
#include "bu/log.h"
#include "bu/malloc.h"
#include "bu/str.h"
#include "bu/vls.h"
#include "vmath.h"
#include "bn/tol.h"
#include "nmg.h"
#include "rt/geom.h"
#include "rt/nmg_conv.h"
#include "raytrace.h"
#include "wdb.h"


static struct db_i *dbip;		/* database instance */
static FILE *fd_out;			/* output file */
static int debug = 0;			/* debug flag */
static int indent_level = 0;		/* current output indentation */
static struct bn_tol tol;		/* tolerance */
static struct bg_tess_tol ttol;		/* tessellation tolerance */
static int solid_count = 0;		/* statistics */
static int comb_count = 0;


/*
 * Print indentation for the current nesting level.
 */
static void
print_indent(void)
{
    int i;
    for (i = 0; i < indent_level; i++)
	fprintf(fd_out, "\t");
}


/*
 * Emit a 4x4 transformation matrix as multmatrix().
 * BRL-CAD stores column-major; OpenSCAD expects row-major.
 */
static void
emit_matrix_open(matp_t m)
{
    if (!m || bn_mat_is_identity(m))
	return;

    print_indent();
    fprintf(fd_out, "multmatrix([\n");
    indent_level++;
    print_indent();
    fprintf(fd_out, "[%g, %g, %g, %g],\n",
	    m[0], m[4], m[8], m[12]);
    print_indent();
    fprintf(fd_out, "[%g, %g, %g, %g],\n",
	    m[1], m[5], m[9], m[13]);
    print_indent();
    fprintf(fd_out, "[%g, %g, %g, %g],\n",
	    m[2], m[6], m[10], m[14]);
    print_indent();
    fprintf(fd_out, "[%g, %g, %g, %g]\n",
	    m[3], m[7], m[11], m[15]);
    indent_level--;
    print_indent();
    fprintf(fd_out, "]) {\n");
    indent_level++;
}


static void
emit_matrix_close(matp_t m)
{
    if (!m || bn_mat_is_identity(m))
	return;

    indent_level--;
    print_indent();
    fprintf(fd_out, "}\n");
}


/*
 * Check if an ellipsoid is actually a sphere (all axes equal length).
 */
static int
ell_is_sphere(struct rt_ell_internal *ell, double *radius)
{
    double la = MAGNITUDE(ell->a);
    double lb = MAGNITUDE(ell->b);
    double lc = MAGNITUDE(ell->c);

    if (NEAR_EQUAL(la, lb, tol.dist) && NEAR_EQUAL(lb, lc, tol.dist)) {
	*radius = la;
	return 1;
    }
    return 0;
}


/*
 * Check if a TGC is a right circular cylinder or cone (circular cross
 * sections, height perpendicular to base, A/B and C/D equal length).
 */
static int
tgc_is_cylinder(struct rt_tgc_internal *tgc,
		double *r1, double *r2, double *h)
{
    double la = MAGNITUDE(tgc->a);
    double lb = MAGNITUDE(tgc->b);
    double lc = MAGNITUDE(tgc->c);
    double ld = MAGNITUDE(tgc->d);
    vect_t h_unit, a_unit, b_unit;

    /* A and B must be equal length (circular base) */
    if (!NEAR_EQUAL(la, lb, tol.dist))
	return 0;
    /* C and D must be equal length (circular top) */
    if (!NEAR_EQUAL(lc, ld, tol.dist))
	return 0;

    /* A and B must be perpendicular */
    if (!NEAR_ZERO(VDOT(tgc->a, tgc->b), tol.dist))
	return 0;

    /* H must be perpendicular to both A and B */
    *h = MAGNITUDE(tgc->h);
    if (*h < tol.dist)
	return 0;

    VSCALE(h_unit, tgc->h, 1.0 / *h);
    VSCALE(a_unit, tgc->a, 1.0 / la);
    VSCALE(b_unit, tgc->b, 1.0 / lb);

    if (!NEAR_ZERO(VDOT(h_unit, a_unit), tol.perp))
	return 0;
    if (!NEAR_ZERO(VDOT(h_unit, b_unit), tol.perp))
	return 0;

    *r1 = la;
    *r2 = lc;
    return 1;
}


/*
 * Emit a sphere primitive.
 */
static void
emit_sphere(struct rt_ell_internal *ell)
{
    double r;

    if (ell_is_sphere(ell, &r)) {
	/* Pure sphere: emit translate + sphere */
	print_indent();
	fprintf(fd_out, "translate([%g, %g, %g])\n",
		V3ARGS(ell->v));
	indent_level++;
	print_indent();
	fprintf(fd_out, "sphere(r = %g);\n", r);
	indent_level--;
    } else {
	/* General ellipsoid: emit translate + multmatrix + sphere(1) */
	double la = MAGNITUDE(ell->a);
	double lb = MAGNITUDE(ell->b);
	double lc = MAGNITUDE(ell->c);
	mat_t m;

	MAT_ZERO(m);
	/* Build matrix from ellipsoid axes */
	m[0]  = ell->a[0]; m[4]  = ell->b[0]; m[8]  = ell->c[0]; m[12] = ell->v[0];
	m[1]  = ell->a[1]; m[5]  = ell->b[1]; m[9]  = ell->c[1]; m[13] = ell->v[1];
	m[2]  = ell->a[2]; m[6]  = ell->b[2]; m[10] = ell->c[2]; m[14] = ell->v[2];
	m[15] = 1.0;

	emit_matrix_open(m);
	print_indent();
	fprintf(fd_out, "sphere(r = 1);\n");
	emit_matrix_close(m);

	if (debug)
	    bu_log("  ellipsoid axes: %g %g %g\n", la, lb, lc);
    }
    solid_count++;
}


/*
 * Emit a TGC/cylinder primitive.
 */
static void
emit_tgc(struct rt_tgc_internal *tgc)
{
    double r1, r2, h;

    if (tgc_is_cylinder(tgc, &r1, &r2, &h)) {
	/* Right circular cylinder/cone: translate + cylinder */
	mat_t m;
	vect_t h_unit;

	VSCALE(h_unit, tgc->h, 1.0 / h);

	/* If H is along Z axis, just translate */
	if (NEAR_ZERO(h_unit[X], tol.perp) &&
	    NEAR_ZERO(h_unit[Y], tol.perp) &&
	    h_unit[Z] > 0) {
	    print_indent();
	    fprintf(fd_out, "translate([%g, %g, %g])\n",
		    V3ARGS(tgc->v));
	    indent_level++;
	    print_indent();
	    fprintf(fd_out, "cylinder(h = %g, r1 = %g, r2 = %g);\n",
		    h, r1, r2);
	    indent_level--;
	} else {
	    /* Need full transform */
	    vect_t a_unit, b_unit;
	    VSCALE(a_unit, tgc->a, 1.0 / r1);
	    VSCALE(b_unit, tgc->b, 1.0 / r1);

	    MAT_ZERO(m);
	    m[0]  = a_unit[0]; m[4]  = b_unit[0]; m[8]  = h_unit[0]; m[12] = tgc->v[0];
	    m[1]  = a_unit[1]; m[5]  = b_unit[1]; m[9]  = h_unit[1]; m[13] = tgc->v[1];
	    m[2]  = a_unit[2]; m[6]  = b_unit[2]; m[10] = h_unit[2]; m[14] = tgc->v[2];
	    m[15] = 1.0;

	    emit_matrix_open(m);
	    print_indent();
	    fprintf(fd_out, "cylinder(h = %g, r1 = %g, r2 = %g);\n",
		    h, r1, r2);
	    emit_matrix_close(m);
	}
    } else {
	/* General TGC: emit as transformed geometry using full axis matrix */
	mat_t m;

	MAT_ZERO(m);
	m[0]  = tgc->a[0]; m[4]  = tgc->b[0]; m[8]  = tgc->h[0]; m[12] = tgc->v[0];
	m[1]  = tgc->a[1]; m[5]  = tgc->b[1]; m[9]  = tgc->h[1]; m[13] = tgc->v[1];
	m[2]  = tgc->a[2]; m[6]  = tgc->b[2]; m[10] = tgc->h[2]; m[14] = tgc->v[2];
	m[15] = 1.0;

	emit_matrix_open(m);
	print_indent();
	/* Unit cylinder scaled by matrix gives general TGC */
	fprintf(fd_out, "cylinder(h = 1, r1 = 1, r2 = %g);\n",
		MAGNITUDE(tgc->c) / MAGNITUDE(tgc->a));
	emit_matrix_close(m);
    }
    solid_count++;
}


/*
 * Emit an ARB as polyhedron().
 */
static void
emit_arb(struct rt_arb_internal *arb)
{
    int i;
    int unique[8];
    int nunique = 0;
    int map[8]; /* maps original index to unique index */

    /* Find unique vertices (ARBs with < 8 verts have duplicates) */
    for (i = 0; i < 8; i++) {
	int j, found = 0;
	for (j = 0; j < nunique; j++) {
	    if (VNEAR_EQUAL(arb->pt[i], arb->pt[unique[j]], tol.dist)) {
		map[i] = j;
		found = 1;
		break;
	    }
	}
	if (!found) {
	    map[i] = nunique;
	    unique[nunique] = i;
	    nunique++;
	}
    }

    print_indent();
    fprintf(fd_out, "polyhedron(\n");
    indent_level++;

    /* Points */
    print_indent();
    fprintf(fd_out, "points = [\n");
    indent_level++;
    for (i = 0; i < nunique; i++) {
	print_indent();
	fprintf(fd_out, "[%g, %g, %g]%s\n",
		V3ARGS(arb->pt[unique[i]]),
		(i < nunique - 1) ? "," : "");
    }
    indent_level--;
    print_indent();
    fprintf(fd_out, "],\n");

    /* Faces — use the standard ARB8 face definitions, mapping to unique verts */
    /* ARB8 faces: 0123, 7654, 0347, 1562, 0451, 3267 */
    {
	static const int arb8_faces[6][4] = {
	    {0, 1, 2, 3}, {7, 6, 5, 4},
	    {0, 3, 7, 4}, {1, 5, 6, 2},
	    {0, 4, 5, 1}, {3, 2, 6, 7}
	};

	print_indent();
	fprintf(fd_out, "faces = [\n");
	indent_level++;

	int nfaces = 0;
	for (i = 0; i < 6; i++) {
	    int fv[4], nfv = 0, j;

	    /* Map and deduplicate face vertices */
	    for (j = 0; j < 4; j++) {
		int mv = map[arb8_faces[i][j]];
		int dup = 0, k;
		for (k = 0; k < nfv; k++) {
		    if (fv[k] == mv) { dup = 1; break; }
		}
		if (!dup)
		    fv[nfv++] = mv;
	    }

	    /* Skip degenerate faces (< 3 unique verts) */
	    if (nfv < 3)
		continue;

	    if (nfaces > 0) {
		fprintf(fd_out, ",\n");
	    }
	    print_indent();
	    fprintf(fd_out, "[");
	    for (j = 0; j < nfv; j++) {
		fprintf(fd_out, "%d%s", fv[j], (j < nfv-1) ? ", " : "");
	    }
	    fprintf(fd_out, "]");
	    nfaces++;
	}
	fprintf(fd_out, "\n");

	indent_level--;
	print_indent();
	fprintf(fd_out, "]\n");
    }

    indent_level--;
    print_indent();
    fprintf(fd_out, ");\n");
    solid_count++;
}


/*
 * Emit a BOT (Bag of Triangles) as polyhedron().
 */
static void
emit_bot(struct rt_bot_internal *bot)
{
    size_t i;

    print_indent();
    fprintf(fd_out, "polyhedron(\n");
    indent_level++;

    print_indent();
    fprintf(fd_out, "points = [\n");
    indent_level++;
    for (i = 0; i < bot->num_vertices; i++) {
	print_indent();
	fprintf(fd_out, "[%g, %g, %g]%s\n",
		bot->vertices[i*3+0],
		bot->vertices[i*3+1],
		bot->vertices[i*3+2],
		(i < bot->num_vertices - 1) ? "," : "");
    }
    indent_level--;
    print_indent();
    fprintf(fd_out, "],\n");

    print_indent();
    fprintf(fd_out, "faces = [\n");
    indent_level++;
    for (i = 0; i < bot->num_faces; i++) {
	print_indent();
	fprintf(fd_out, "[%d, %d, %d]%s\n",
		bot->faces[i*3+0],
		bot->faces[i*3+1],
		bot->faces[i*3+2],
		(i < bot->num_faces - 1) ? "," : "");
    }
    indent_level--;
    print_indent();
    fprintf(fd_out, "]\n");

    indent_level--;
    print_indent();
    fprintf(fd_out, ");\n");
    solid_count++;
}


/*
 * Emit an unsupported primitive by tessellating it to polyhedron().
 * Uses the primitive's ft_tessellate to get NMG, then nmg_bot() to
 * convert to triangles.
 */
static void
emit_tessellated(const char *name, struct rt_db_internal *ip)
{
    struct model *m;
    struct nmgregion *r;
    struct shell *s;
    struct rt_bot_internal *bot;
    const struct rt_functab *ftp;

    ftp = ip->idb_meth;
    if (!ftp || !ftp->ft_tessellate) {
	bu_log("WARNING: no tessellation method for %s, skipping\n", name);
	return;
    }

    m = nmg_mm();
    if (ftp->ft_tessellate(&r, m, ip, &ttol, &tol) < 0) {
	bu_log("WARNING: tessellation failed for %s, skipping\n", name);
	nmg_km(m);
	return;
    }

    /* Convert the first shell to BOT */
    s = BU_LIST_FIRST(shell, &r->s_hd);
    {
	struct bu_list vlfree;
	BU_LIST_INIT(&vlfree);
	bot = nmg_bot(s, &vlfree, &tol);
    }
    if (!bot || bot->num_faces == 0) {
	bu_log("WARNING: empty tessellation for %s, skipping\n", name);
	nmg_km(m);
	return;
    }

    if (debug)
	bu_log("  tessellated %s: %zu verts, %zu faces\n",
	       name, bot->num_vertices, bot->num_faces);

    emit_bot(bot);
    nmg_km(m);
}


/* Forward declaration */
static void emit_object(const char *name);


/*
 * Emit a boolean tree node as OpenSCAD CSG operations.
 */
static void
emit_tree(union tree *tree)
{
    if (!tree)
	return;

    switch (tree->tr_op) {
	case OP_DB_LEAF:
	    emit_matrix_open(tree->tr_l.tl_mat);
	    emit_object(tree->tr_l.tl_name);
	    emit_matrix_close(tree->tr_l.tl_mat);
	    break;

	case OP_UNION:
	    print_indent();
	    fprintf(fd_out, "union() {\n");
	    indent_level++;
	    emit_tree(tree->tr_b.tb_left);
	    emit_tree(tree->tr_b.tb_right);
	    indent_level--;
	    print_indent();
	    fprintf(fd_out, "}\n");
	    break;

	case OP_SUBTRACT:
	    print_indent();
	    fprintf(fd_out, "difference() {\n");
	    indent_level++;
	    emit_tree(tree->tr_b.tb_left);
	    emit_tree(tree->tr_b.tb_right);
	    indent_level--;
	    print_indent();
	    fprintf(fd_out, "}\n");
	    break;

	case OP_INTERSECT:
	    print_indent();
	    fprintf(fd_out, "intersection() {\n");
	    indent_level++;
	    emit_tree(tree->tr_b.tb_left);
	    emit_tree(tree->tr_b.tb_right);
	    indent_level--;
	    print_indent();
	    fprintf(fd_out, "}\n");
	    break;

	default:
	    bu_log("WARNING: unhandled boolean op %d\n", tree->tr_op);
	    break;
    }
}


/*
 * Emit a single database object (primitive or combination).
 */
static void
emit_object(const char *name)
{
    struct directory *dp;
    struct rt_db_internal intern;

    dp = db_lookup(dbip, name, LOOKUP_QUIET);
    if (dp == RT_DIR_NULL) {
	bu_log("WARNING: object '%s' not found\n", name);
	return;
    }

    if (rt_db_get_internal(&intern, dp, dbip, NULL, &rt_uniresource) < 0) {
	bu_log("WARNING: failed to read '%s'\n", name);
	return;
    }

    if (dp->d_flags & RT_DIR_COMB) {
	/* Combination: emit boolean tree */
	struct rt_comb_internal *comb = (struct rt_comb_internal *)intern.idb_ptr;
	RT_CK_COMB(comb);

	if (debug)
	    bu_log("  combination: %s%s\n", name,
		   comb->region_flag ? " (region)" : "");

	/* Emit color if present */
	if (comb->rgb_valid) {
	    print_indent();
	    fprintf(fd_out, "color([%g, %g, %g]) {\n",
		    comb->rgb[0] / 255.0,
		    comb->rgb[1] / 255.0,
		    comb->rgb[2] / 255.0);
	    indent_level++;
	}

	/* Add a comment with the combination name */
	print_indent();
	fprintf(fd_out, "/* %s */\n", name);

	emit_tree(comb->tree);
	comb_count++;

	if (comb->rgb_valid) {
	    indent_level--;
	    print_indent();
	    fprintf(fd_out, "}\n");
	}
    } else {
	/* Primitive solid */
	if (debug)
	    bu_log("  solid: %s (type %d)\n", name, intern.idb_type);

	print_indent();
	fprintf(fd_out, "/* %s */\n", name);

	switch (intern.idb_type) {
	    case ID_ELL:
	    case ID_SPH:
		emit_sphere((struct rt_ell_internal *)intern.idb_ptr);
		break;

	    case ID_TGC:
	    case ID_REC:
		emit_tgc((struct rt_tgc_internal *)intern.idb_ptr);
		break;

	    case ID_ARB8:
		emit_arb((struct rt_arb_internal *)intern.idb_ptr);
		break;

	    case ID_BOT:
		emit_bot((struct rt_bot_internal *)intern.idb_ptr);
		break;

	    case ID_TOR:
		{
		    struct rt_tor_internal *tor = (struct rt_tor_internal *)intern.idb_ptr;
		    mat_t m;
		    vect_t h_unit, a_unit, b_unit;

		    VMOVE(h_unit, tor->h);
		    VUNITIZE(h_unit);
		    bn_vec_ortho(a_unit, h_unit);
		    VCROSS(b_unit, h_unit, a_unit);

		    MAT_ZERO(m);
		    m[0]  = a_unit[0]; m[4]  = b_unit[0]; m[8]  = h_unit[0]; m[12] = tor->v[0];
		    m[1]  = a_unit[1]; m[5]  = b_unit[1]; m[9]  = h_unit[1]; m[13] = tor->v[1];
		    m[2]  = a_unit[2]; m[6]  = b_unit[2]; m[10] = h_unit[2]; m[14] = tor->v[2];
		    m[15] = 1.0;

		    emit_matrix_open(m);
		    print_indent();
		    fprintf(fd_out, "rotate_extrude()\n");
		    indent_level++;
		    print_indent();
		    fprintf(fd_out, "translate([%g, 0, 0])\n", tor->r_a);
		    indent_level++;
		    print_indent();
		    fprintf(fd_out, "circle(r = %g);\n", tor->r_h);
		    indent_level -= 2;
		    emit_matrix_close(m);
		    solid_count++;
		    break;
		}

	    case ID_EXTRUDE:
		{
		    struct rt_extrude_internal *extr = (struct rt_extrude_internal *)intern.idb_ptr;
		    struct rt_sketch_internal *skt = NULL;
		    struct directory *skt_dp;
		    struct rt_db_internal skt_intern;
		    mat_t m;
		    vect_t h_unit;
		    double h_len;

		    /* Load the referenced sketch */
		    skt_dp = db_lookup(dbip, extr->sketch_name, LOOKUP_QUIET);
		    if (skt_dp == RT_DIR_NULL) {
			bu_log("WARNING: sketch '%s' not found for extrude %s\n",
			       extr->sketch_name, name);
			emit_tessellated(name, &intern);
			break;
		    }
		    if (rt_db_get_internal(&skt_intern, skt_dp, dbip, NULL, &rt_uniresource) < 0) {
			bu_log("WARNING: failed to read sketch '%s'\n", extr->sketch_name);
			emit_tessellated(name, &intern);
			break;
		    }
		    skt = (struct rt_sketch_internal *)skt_intern.idb_ptr;

		    h_len = MAGNITUDE(extr->h);

		    /* Build transform from sketch plane to world */
		    MAT_ZERO(m);
		    m[0]  = extr->u_vec[0]; m[4]  = extr->v_vec[0]; m[12] = extr->V[0];
		    m[1]  = extr->u_vec[1]; m[5]  = extr->v_vec[1]; m[13] = extr->V[1];
		    m[2]  = extr->u_vec[2]; m[6]  = extr->v_vec[2]; m[14] = extr->V[2];
		    /* H direction as Z of the local frame */
		    if (h_len > tol.dist) {
			VSCALE(h_unit, extr->h, 1.0 / h_len);
			m[8]  = h_unit[0];
			m[9]  = h_unit[1];
			m[10] = h_unit[2];
		    }
		    m[15] = 1.0;

		    emit_matrix_open(m);

		    print_indent();
		    fprintf(fd_out, "linear_extrude(height = %g) {\n", h_len);
		    indent_level++;

		    /* Emit polygon from sketch vertices and line segments */
		    {
			size_t vi;
			size_t si;
			int has_lines = 0;

			print_indent();
			fprintf(fd_out, "polygon(\n");
			indent_level++;

			/* Points */
			print_indent();
			fprintf(fd_out, "points = [\n");
			indent_level++;
			for (vi = 0; vi < skt->vert_count; vi++) {
			    print_indent();
			    fprintf(fd_out, "[%g, %g]%s\n",
				    skt->verts[vi][0], skt->verts[vi][1],
				    (vi < skt->vert_count - 1) ? "," : "");
			}
			indent_level--;
			print_indent();
			fprintf(fd_out, "],\n");

			/* Paths — collect line segment chains */
			/* For simplicity, emit one path per closed loop.
			 * Walk line segments to find connected chains. */
			print_indent();
			fprintf(fd_out, "paths = [\n");
			indent_level++;

			/* Simple approach: emit vertex indices from line
			 * segments in order. */
			for (si = 0; si < skt->curve.count; si++) {
			    uint32_t *magic_p = (uint32_t *)skt->curve.segment[si];
			    if (*magic_p == CURVE_LSEG_MAGIC) {
				has_lines = 1;
				break;
			    }
			}

			if (has_lines) {
			    /* Collect all start vertices from line segments as a single path */
			    print_indent();
			    fprintf(fd_out, "[");
			    {
				int first = 1;
				for (si = 0; si < skt->curve.count; si++) {
				    uint32_t *magic_p = (uint32_t *)skt->curve.segment[si];
				    if (*magic_p == CURVE_LSEG_MAGIC) {
					struct line_seg *lseg = (struct line_seg *)skt->curve.segment[si];
					if (!first) fprintf(fd_out, ", ");
					fprintf(fd_out, "%d", lseg->start);
					first = 0;
				    }
				}
			    }
			    fprintf(fd_out, "]\n");
			} else {
			    /* No line segments — emit default path */
			    print_indent();
			    fprintf(fd_out, "[");
			    for (vi = 0; vi < skt->vert_count; vi++) {
				fprintf(fd_out, "%zu%s", vi,
					(vi < skt->vert_count - 1) ? ", " : "");
			    }
			    fprintf(fd_out, "]\n");
			}

			indent_level--;
			print_indent();
			fprintf(fd_out, "]\n");

			indent_level--;
			print_indent();
			fprintf(fd_out, ");\n");
		    }

		    indent_level--;
		    print_indent();
		    fprintf(fd_out, "}\n");

		    emit_matrix_close(m);
		    rt_db_free_internal(&skt_intern);
		    solid_count++;
		    break;
		}

	    case ID_SKETCH:
		/* Sketches are referenced by extrusions, not standalone geometry.
		 * If encountered alone, skip silently. */
		break;

	    default:
		/* Try tessellation for unsupported types */
		emit_tessellated(name, &intern);
		break;
	}
    }

    rt_db_free_internal(&intern);
}


static void
usage(const char *argv0)
{
    bu_log("Usage: %s [-d] [-o output.scad] input.g object [object ...]\n", argv0);
    bu_log("	where input.g is a BRL-CAD database\n");
    bu_log("	and object(s) are the top-level objects to export.\n");
    bu_log("	The -d option prints additional debugging information.\n");
    bu_log("	The -o option specifies the output file (default: stdout).\n");
}


int
main(int argc, char *argv[])
{
    int c, i;
    const char *output_file = NULL;

    bu_setprogname(argv[0]);

    tol.magic = BN_TOL_MAGIC;
    tol.dist = 0.0005;
    tol.dist_sq = tol.dist * tol.dist;
    tol.perp = 1e-6;
    tol.para = 1 - tol.perp;

    ttol.magic = BG_TESS_TOL_MAGIC;
    ttol.abs = 0.0;
    ttol.rel = 0.01;
    ttol.norm = 0.0;

    if (argc < 3) {
	usage(argv[0]);
	bu_exit(1, NULL);
    }

    while ((c = bu_getopt(argc, argv, "do:")) != -1) {
	switch (c) {
	    case 'd':
		debug = 1;
		break;
	    case 'o':
		output_file = bu_optarg;
		break;
	    default:
		usage(argv[0]);
		bu_exit(1, NULL);
		break;
	}
    }

    if (bu_optind >= argc) {
	usage(argv[0]);
	bu_exit(1, "g-scad: need input file and object names\n");
    }

    /* Open BRL-CAD database */
    dbip = db_open(argv[bu_optind], DB_OPEN_READONLY);
    if (dbip == DBI_NULL) {
	bu_log("Cannot open BRL-CAD file (%s)\n", argv[bu_optind]);
	bu_exit(1, NULL);
    }
    if (db_dirbuild(dbip)) {
	bu_log("db_dirbuild failed on %s\n", argv[bu_optind]);
	bu_exit(1, NULL);
    }
    bu_optind++;

    if (bu_optind >= argc) {
	usage(argv[0]);
	bu_exit(1, "g-scad: need object name(s)\n");
    }

    /* Open output */
    if (output_file) {
	fd_out = fopen(output_file, "w");
	if (!fd_out) {
	    bu_log("Cannot open output file (%s)\n", output_file);
	    perror("g-scad");
	    bu_exit(1, NULL);
	}
    } else {
	fd_out = stdout;
    }

    fprintf(fd_out, "// Converted from BRL-CAD by g-scad\n");
    fprintf(fd_out, "// Source: %s\n\n", argv[bu_optind - 1]);

    /* Process each named object */
    for (i = bu_optind; i < argc; i++) {
	bu_log("Exporting: %s\n", argv[i]);
	emit_object(argv[i]);
    }

    bu_log("Export complete: %d solids, %d combinations\n",
	   solid_count, comb_count);

    if (output_file)
	fclose(fd_out);

    db_close(dbip);
    return 0;
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
