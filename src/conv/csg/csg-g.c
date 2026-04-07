/*                         C S G - G . C
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
/** @file csg-g.c
 *
 * Convert OpenSCAD .csg format files to BRL-CAD .g binary format.
 *
 * The .csg format is OpenSCAD's evaluated CSG tree output, produced
 * by running: openscad -o file.csg input.scad
 *
 * All variables, modules, loops, and conditionals are already resolved
 * in this format, leaving only primitives, boolean operations, and
 * transformations.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>

#include "bu/app.h"
#include "bu/getopt.h"
#include "bu/log.h"
#include "bu/malloc.h"
#include "bu/str.h"
#include "bu/vls.h"
#include "vmath.h"
#include "rt/geom.h"
#include "raytrace.h"
#include "wdb.h"


static char *input_file;	/* name of the input file */
static char *brlcad_file;	/* name of output file */
static FILE *fd_in;		/* input file */
static struct rt_wdb *fd_out;	/* output BRL-CAD file */
static int debug = 0;		/* debug flag */
static int solid_count = 0;	/* count of solids converted */
static int comb_count = 0;	/* count of combinations created */
static struct wmember all_head;	/* top-level group */

/* Input buffer */
static char *buf = NULL;	/* file contents */
static size_t buf_len = 0;	/* buffer length */
static size_t pos = 0;		/* current parse position */


/*
 * Generate a unique solid name like s.1, s.2, ...
 */
static void
make_solid_name(struct bu_vls *name)
{
    bu_vls_sprintf(name, "s.%d", ++solid_count);
}


/*
 * Generate a unique combination name like c.1, c.2, ...
 */
static void
make_comb_name(struct bu_vls *name)
{
    bu_vls_sprintf(name, "c.%d", ++comb_count);
}


/*
 * Skip whitespace and comments in the input buffer.
 */
static void
skip_ws(void)
{
    while (pos < buf_len) {
	if (isspace((int)buf[pos])) {
	    pos++;
	} else if (pos + 1 < buf_len && buf[pos] == '/' && buf[pos+1] == '/') {
	    /* line comment */
	    while (pos < buf_len && buf[pos] != '\n')
		pos++;
	} else if (pos + 1 < buf_len && buf[pos] == '/' && buf[pos+1] == '*') {
	    /* block comment */
	    pos += 2;
	    while (pos + 1 < buf_len && !(buf[pos] == '*' && buf[pos+1] == '/'))
		pos++;
	    if (pos + 1 < buf_len)
		pos += 2;
	} else {
	    break;
	}
    }
}


/*
 * Check if the current position matches a keyword, followed by '(' or '{'.
 * Does not advance the position.
 */
static int
looking_at(const char *kw)
{
    size_t len = strlen(kw);
    if (pos + len > buf_len)
	return 0;
    if (bu_strncmp(&buf[pos], kw, len) != 0)
	return 0;
    /* keyword must be followed by '(' or whitespace then '(' */
    size_t p = pos + len;
    while (p < buf_len && isspace((int)buf[p]))
	p++;
    if (p < buf_len && buf[p] == '(')
	return 1;
    return 0;
}


/*
 * Consume a keyword and advance past it.
 */
static void
consume(const char *kw)
{
    pos += strlen(kw);
}


/*
 * Expect and consume a specific character.
 */
static int
expect_char(char c)
{
    skip_ws();
    if (pos < buf_len && buf[pos] == c) {
	pos++;
	return 1;
    }
    bu_log("csg-g: expected '%c' at position %zu, got '%c'\n",
	   c, pos, (pos < buf_len) ? buf[pos] : '?');
    return 0;
}


/*
 * Parse a floating point number from the buffer.
 */
static double
parse_double(void)
{
    skip_ws();
    char *end;
    double val = strtod(&buf[pos], &end);
    pos = end - buf;
    return val;
}


/*
 * Look for a named parameter in the parameter list.
 * Returns 1 and sets *val if found, 0 otherwise.
 * Call before consuming the parameter list.
 */
static int
find_param_double(const char *name, double *val)
{
    size_t save = pos;
    size_t namelen = strlen(name);

    while (pos < buf_len && buf[pos] != ')') {
	skip_ws();
	if (pos + namelen < buf_len && bu_strncmp(&buf[pos], name, namelen) == 0) {
	    size_t p = pos + namelen;
	    while (p < buf_len && isspace((int)buf[p])) p++;
	    if (p < buf_len && buf[p] == '=') {
		p++;
		pos = p;
		*val = parse_double();
		pos = save;
		return 1;
	    }
	}
	/* advance to next comma or end */
	while (pos < buf_len && buf[pos] != ',' && buf[pos] != ')')
	    pos++;
	if (pos < buf_len && buf[pos] == ',')
	    pos++;
    }
    pos = save;
    return 0;
}


/*
 * Look for a named array parameter like size = [x, y, z].
 * Returns 1 and fills vals (up to maxn entries) if found.
 */
static int
find_param_array(const char *name, double *vals, int maxn, int *countp)
{
    size_t save = pos;
    size_t namelen = strlen(name);
    int count = 0;

    while (pos < buf_len && buf[pos] != ')') {
	skip_ws();
	if (pos + namelen < buf_len && bu_strncmp(&buf[pos], name, namelen) == 0) {
	    size_t p = pos + namelen;
	    while (p < buf_len && isspace((int)buf[p])) p++;
	    if (p < buf_len && buf[p] == '=') {
		p++;
		while (p < buf_len && isspace((int)buf[p])) p++;
		if (p < buf_len && buf[p] == '[') {
		    p++;
		    pos = p;
		    while (count < maxn) {
			skip_ws();
			if (pos < buf_len && buf[pos] == ']')
			    break;
			vals[count++] = parse_double();
			skip_ws();
			if (pos < buf_len && buf[pos] == ',')
			    pos++;
		    }
		    if (countp) *countp = count;
		    pos = save;
		    return 1;
		}
	    }
	}
	while (pos < buf_len && buf[pos] != ',' && buf[pos] != ')')
	    pos++;
	if (pos < buf_len && buf[pos] == ',')
	    pos++;
    }
    pos = save;
    return 0;
}


/*
 * Look for a boolean parameter like center = true/false.
 */
static int
find_param_bool(const char *name, int *val)
{
    size_t save = pos;
    size_t namelen = strlen(name);

    while (pos < buf_len && buf[pos] != ')') {
	skip_ws();
	if (pos + namelen < buf_len && bu_strncmp(&buf[pos], name, namelen) == 0) {
	    size_t p = pos + namelen;
	    while (p < buf_len && isspace((int)buf[p])) p++;
	    if (p < buf_len && buf[p] == '=') {
		p++;
		while (p < buf_len && isspace((int)buf[p])) p++;
		if (p + 4 <= buf_len && bu_strncmp(&buf[p], "true", 4) == 0) {
		    *val = 1;
		    pos = save;
		    return 1;
		} else if (p + 5 <= buf_len && bu_strncmp(&buf[p], "false", 5) == 0) {
		    *val = 0;
		    pos = save;
		    return 1;
		}
	    }
	}
	while (pos < buf_len && buf[pos] != ',' && buf[pos] != ')')
	    pos++;
	if (pos < buf_len && buf[pos] == ',')
	    pos++;
    }
    pos = save;
    return 0;
}


/*
 * Skip everything inside parentheses (the parameter list).
 */
static void
skip_params(void)
{
    skip_ws();
    if (pos < buf_len && buf[pos] == '(') {
	int depth = 1;
	pos++;
	while (pos < buf_len && depth > 0) {
	    if (buf[pos] == '(') depth++;
	    else if (buf[pos] == ')') depth--;
	    pos++;
	}
    }
}


/* Forward declaration */
static int parse_node(struct bu_vls *out_name, mat_t xform);


/*
 * Parse the children inside a { } block, collecting their names and
 * applying a boolean operation.  Returns the name of the resulting
 * combination in out_name.
 */
static int
parse_boolean(const char *op_name, int wmop, struct bu_vls *out_name, mat_t parent_xform)
{
    struct wmember head;
    struct bu_vls child_name = BU_VLS_INIT_ZERO;
    struct bu_vls cname = BU_VLS_INIT_ZERO;
    int child_count = 0;
    int first = 1;

    BU_LIST_INIT(&head.l);

    skip_params();
    skip_ws();

    if (!expect_char('{'))
	return 0;

    while (1) {
	skip_ws();
	if (pos >= buf_len || buf[pos] == '}')
	    break;

	bu_vls_trunc(&child_name, 0);
	if (parse_node(&child_name, parent_xform)) {
	    int op;
	    if (first) {
		op = WMOP_UNION;
		first = 0;
	    } else {
		op = wmop;
	    }
	    mk_addmember(bu_vls_cstr(&child_name), &head.l, NULL, op);
	    child_count++;
	}
    }

    expect_char('}');

    if (child_count == 0) {
	bu_vls_free(&child_name);
	bu_vls_free(&cname);
	return 0;
    }

    /* If only one child in a group/union, just pass through its name */
    if (child_count == 1 && wmop == WMOP_UNION) {
	bu_vls_vlscat(out_name, &child_name);
	bu_vls_free(&child_name);
	bu_vls_free(&cname);
	return 1;
    }

    make_comb_name(&cname);
    mk_lcomb(fd_out, bu_vls_cstr(&cname), &head, 0,
	     (char *)NULL, (char *)NULL, (unsigned char *)NULL, 0);

    if (debug)
	bu_log("  %s: %s (%d children)\n", op_name, bu_vls_cstr(&cname), child_count);

    bu_vls_vlscat(out_name, &cname);
    bu_vls_free(&child_name);
    bu_vls_free(&cname);
    return 1;
}


/*
 * Parse a multmatrix node.  The matrix is a 4x4 array of doubles.
 * Applies the matrix to the child node's transform.
 */
static int
parse_multmatrix(struct bu_vls *out_name, mat_t parent_xform)
{
    mat_t local_mat;
    mat_t combined;
    double vals[16];
    int i;

    skip_ws();
    if (!expect_char('('))
	return 0;

    /* Parse [[a,b,c,d],[e,f,g,h],[i,j,k,l],[m,n,o,p]] */
    skip_ws();
    if (!expect_char('['))
	return 0;

    for (i = 0; i < 4; i++) {
	skip_ws();
	if (i > 0) {
	    skip_ws();
	    if (pos < buf_len && buf[pos] == ',')
		pos++;
	}
	skip_ws();
	if (!expect_char('['))
	    return 0;
	vals[i*4+0] = parse_double(); skip_ws(); expect_char(',');
	vals[i*4+1] = parse_double(); skip_ws(); expect_char(',');
	vals[i*4+2] = parse_double(); skip_ws(); expect_char(',');
	vals[i*4+3] = parse_double(); skip_ws();
	if (!expect_char(']'))
	    return 0;
    }

    skip_ws();
    expect_char(']');
    skip_ws();
    expect_char(')');

    /*
     * OpenSCAD uses row-major order:
     *   [[r00, r01, r02, tx],
     *    [r10, r11, r12, ty],
     *    [r20, r21, r22, tz],
     *    [  0,   0,   0,  1]]
     *
     * BRL-CAD uses a transposed column-major layout.
     */
    MAT_ZERO(local_mat);
    local_mat[ 0] = vals[ 0]; local_mat[ 1] = vals[ 4]; local_mat[ 2] = vals[ 8]; local_mat[ 3] = vals[12];
    local_mat[ 4] = vals[ 1]; local_mat[ 5] = vals[ 5]; local_mat[ 6] = vals[ 9]; local_mat[ 7] = vals[13];
    local_mat[ 8] = vals[ 2]; local_mat[ 9] = vals[ 6]; local_mat[10] = vals[10]; local_mat[11] = vals[14];
    local_mat[12] = vals[ 3]; local_mat[13] = vals[ 7]; local_mat[14] = vals[11]; local_mat[15] = vals[15];

    bn_mat_mul(combined, parent_xform, local_mat);

    skip_ws();
    if (!expect_char('{'))
	return 0;

    /* Parse the single child */
    skip_ws();
    if (!parse_node(out_name, combined)) {
	/* skip to closing brace */
	while (pos < buf_len && buf[pos] != '}')
	    pos++;
    }

    expect_char('}');
    return (bu_vls_strlen(out_name) > 0) ? 1 : 0;
}


/*
 * Parse a cube primitive: cube(size = [x, y, z], center = false)
 */
static int
parse_cube(struct bu_vls *out_name, mat_t xform)
{
    struct bu_vls sname = BU_VLS_INIT_ZERO;
    double size[3] = {1.0, 1.0, 1.0};
    int centered = 0;
    int n = 0;
    point_t min_pt, max_pt;

    skip_ws();
    if (!expect_char('('))
	return 0;

    find_param_array("size", size, 3, &n);
    find_param_bool("center", &centered);
    skip_params(); /* consume rest - pos is past ')' since skip_params
		    * was called but we already consumed '(' */

    /* Rewind: skip_params expects to see '(' but we already consumed it.
     * Instead, just skip to closing ')'. */
    /* Actually, we need to skip to ')' */
    while (pos < buf_len && buf[pos] != ')')
	pos++;
    if (pos < buf_len) pos++;

    if (centered) {
	VSET(min_pt, -size[0]/2.0, -size[1]/2.0, -size[2]/2.0);
	VSET(max_pt,  size[0]/2.0,  size[1]/2.0,  size[2]/2.0);
    } else {
	VSET(min_pt, 0.0, 0.0, 0.0);
	VSET(max_pt, size[0], size[1], size[2]);
    }

    /* Apply transform to the RPP by creating it with a matrix */
    make_solid_name(&sname);

    if (bn_mat_is_identity(xform)) {
	mk_rpp(fd_out, bu_vls_cstr(&sname), min_pt, max_pt);
    } else {
	/* Create the RPP at identity, then wrap in a combination with the matrix */
	struct bu_vls raw = BU_VLS_INIT_ZERO;
	struct wmember head;

	bu_vls_sprintf(&raw, "s.raw.%d", solid_count);
	mk_rpp(fd_out, bu_vls_cstr(&raw), min_pt, max_pt);

	BU_LIST_INIT(&head.l);
	mk_addmember(bu_vls_cstr(&raw), &head.l, xform, WMOP_UNION);
	mk_lcomb(fd_out, bu_vls_cstr(&sname), &head, 0,
		 (char *)NULL, (char *)NULL, (unsigned char *)NULL, 0);
	bu_vls_free(&raw);
    }

    if (debug)
	bu_log("  cube: %s [%.3f, %.3f, %.3f]\n",
	       bu_vls_cstr(&sname), size[0], size[1], size[2]);

    bu_vls_vlscat(out_name, &sname);
    bu_vls_free(&sname);

    skip_ws();
    if (pos < buf_len && buf[pos] == ';')
	pos++;

    return 1;
}


/*
 * Parse a sphere primitive: sphere($fn = N, $fa = N, $fs = N, r = R)
 */
static int
parse_sphere(struct bu_vls *out_name, mat_t xform)
{
    struct bu_vls sname = BU_VLS_INIT_ZERO;
    double r = 1.0;
    point_t center;

    skip_ws();
    if (!expect_char('('))
	return 0;

    find_param_double("r", &r);

    while (pos < buf_len && buf[pos] != ')')
	pos++;
    if (pos < buf_len) pos++;

    VSET(center, 0.0, 0.0, 0.0);

    make_solid_name(&sname);

    if (bn_mat_is_identity(xform)) {
	mk_sph(fd_out, bu_vls_cstr(&sname), center, r);
    } else {
	struct bu_vls raw = BU_VLS_INIT_ZERO;
	struct wmember head;

	bu_vls_sprintf(&raw, "s.raw.%d", solid_count);
	mk_sph(fd_out, bu_vls_cstr(&raw), center, r);

	BU_LIST_INIT(&head.l);
	mk_addmember(bu_vls_cstr(&raw), &head.l, xform, WMOP_UNION);
	mk_lcomb(fd_out, bu_vls_cstr(&sname), &head, 0,
		 (char *)NULL, (char *)NULL, (unsigned char *)NULL, 0);
	bu_vls_free(&raw);
    }

    if (debug)
	bu_log("  sphere: %s r=%.3f\n", bu_vls_cstr(&sname), r);

    bu_vls_vlscat(out_name, &sname);
    bu_vls_free(&sname);

    skip_ws();
    if (pos < buf_len && buf[pos] == ';')
	pos++;

    return 1;
}


/*
 * Parse a cylinder primitive:
 *   cylinder($fn=N, $fa=N, $fs=N, h=H, r1=R1, r2=R2, center=false)
 */
static int
parse_cylinder(struct bu_vls *out_name, mat_t xform)
{
    struct bu_vls sname = BU_VLS_INIT_ZERO;
    double h = 1.0, r1 = 1.0, r2 = 1.0;
    int centered = 0;
    point_t base;
    vect_t height_vec, a_vec, b_vec;

    skip_ws();
    if (!expect_char('('))
	return 0;

    find_param_double("h", &h);
    find_param_double("r1", &r1);
    find_param_double("r2", &r2);
    find_param_bool("center", &centered);

    while (pos < buf_len && buf[pos] != ')')
	pos++;
    if (pos < buf_len) pos++;

    if (centered) {
	VSET(base, 0.0, 0.0, -h/2.0);
    } else {
	VSET(base, 0.0, 0.0, 0.0);
    }
    VSET(height_vec, 0.0, 0.0, h);

    make_solid_name(&sname);

    if (bn_mat_is_identity(xform)) {
	/* TGC: base, height, A, B, C, D vectors */
	VSET(a_vec, r1, 0.0, 0.0);
	VSET(b_vec, 0.0, r1, 0.0);
	vect_t c_vec, d_vec;
	VSET(c_vec, r2, 0.0, 0.0);
	VSET(d_vec, 0.0, r2, 0.0);
	mk_tgc(fd_out, bu_vls_cstr(&sname), base, height_vec,
		a_vec, b_vec, c_vec, d_vec);
    } else {
	struct bu_vls raw = BU_VLS_INIT_ZERO;
	struct wmember head;
	vect_t c_vec, d_vec;

	bu_vls_sprintf(&raw, "s.raw.%d", solid_count);
	VSET(a_vec, r1, 0.0, 0.0);
	VSET(b_vec, 0.0, r1, 0.0);
	VSET(c_vec, r2, 0.0, 0.0);
	VSET(d_vec, 0.0, r2, 0.0);
	mk_tgc(fd_out, bu_vls_cstr(&raw), base, height_vec,
		a_vec, b_vec, c_vec, d_vec);

	BU_LIST_INIT(&head.l);
	mk_addmember(bu_vls_cstr(&raw), &head.l, xform, WMOP_UNION);
	mk_lcomb(fd_out, bu_vls_cstr(&sname), &head, 0,
		 (char *)NULL, (char *)NULL, (unsigned char *)NULL, 0);
	bu_vls_free(&raw);
    }

    if (debug)
	bu_log("  cylinder: %s h=%.3f r1=%.3f r2=%.3f\n",
	       bu_vls_cstr(&sname), h, r1, r2);

    bu_vls_vlscat(out_name, &sname);
    bu_vls_free(&sname);

    skip_ws();
    if (pos < buf_len && buf[pos] == ';')
	pos++;

    return 1;
}


/*
 * Skip an unrecognized node: consume identifier, params, and
 * optional { children } block.
 */
static void
skip_node(void)
{
    /* skip identifier */
    while (pos < buf_len && !isspace((int)buf[pos]) && buf[pos] != '(' && buf[pos] != '{')
	pos++;
    skip_params();
    skip_ws();
    if (pos < buf_len && buf[pos] == '{') {
	int depth = 1;
	pos++;
	while (pos < buf_len && depth > 0) {
	    if (buf[pos] == '{') depth++;
	    else if (buf[pos] == '}') depth--;
	    pos++;
	}
    }
    skip_ws();
    if (pos < buf_len && buf[pos] == ';')
	pos++;
}


/*
 * Parse a color() wrapper: color([r, g, b, a]) { child }
 * We currently ignore the color and just parse the child.
 */
static int
parse_color(struct bu_vls *out_name, mat_t xform)
{
    skip_params();
    skip_ws();
    if (!expect_char('{'))
	return 0;

    skip_ws();
    if (!parse_node(out_name, xform)) {
	while (pos < buf_len && buf[pos] != '}')
	    pos++;
    }

    expect_char('}');
    return (bu_vls_strlen(out_name) > 0) ? 1 : 0;
}


/*
 * Parse a single CSG node.  This is the main recursive dispatch.
 * Returns the name of the created object in out_name.
 */
static int
parse_node(struct bu_vls *out_name, mat_t xform)
{
    skip_ws();

    if (pos >= buf_len)
	return 0;

    /* Boolean operations */
    if (looking_at("difference")) {
	consume("difference");
	return parse_boolean("difference", WMOP_SUBTRACT, out_name, xform);
    }
    if (looking_at("union")) {
	consume("union");
	return parse_boolean("union", WMOP_UNION, out_name, xform);
    }
    if (looking_at("intersection")) {
	consume("intersection");
	return parse_boolean("intersection", WMOP_INTERSECT, out_name, xform);
    }
    if (looking_at("group")) {
	consume("group");
	return parse_boolean("group", WMOP_UNION, out_name, xform);
    }

    /* Transform */
    if (looking_at("multmatrix")) {
	consume("multmatrix");
	return parse_multmatrix(out_name, xform);
    }

    /* Color (pass through to child) */
    if (looking_at("color")) {
	consume("color");
	return parse_color(out_name, xform);
    }

    /* Primitives */
    if (looking_at("cube")) {
	consume("cube");
	return parse_cube(out_name, xform);
    }
    if (looking_at("sphere")) {
	consume("sphere");
	return parse_sphere(out_name, xform);
    }
    if (looking_at("cylinder")) {
	consume("cylinder");
	return parse_cylinder(out_name, xform);
    }

    /* Unsupported nodes: warn and skip */
    if (looking_at("linear_extrude") || looking_at("rotate_extrude") ||
	looking_at("hull") || looking_at("minkowski") ||
	looking_at("polyhedron") || looking_at("polygon") ||
	looking_at("square") || looking_at("circle") ||
	looking_at("render") || looking_at("import")) {
	size_t start = pos;
	size_t end = pos;
	while (end < buf_len && buf[end] != '(' && !isspace((int)buf[end]))
	    end++;
	bu_log("WARNING: unsupported node '%.*s' at position %zu, skipping\n",
	       (int)(end - start), &buf[start], pos);
	skip_node();
	return 0;
    }

    /* Unknown node */
    if (isalpha((int)buf[pos]) || buf[pos] == '_') {
	size_t start = pos;
	size_t end = pos;
	while (end < buf_len && buf[end] != '(' && !isspace((int)buf[end]))
	    end++;
	if (debug)
	    bu_log("  skipping unknown node '%.*s'\n",
		   (int)(end - start), &buf[start]);
	skip_node();
	return 0;
    }

    /* If we get here, something unexpected */
    bu_log("csg-g: unexpected character '%c' at position %zu\n",
	   buf[pos], pos);
    pos++;
    return 0;
}


static void
Convert_input(void)
{
    mat_t identity;

    MAT_IDN(identity);

    while (pos < buf_len) {
	struct bu_vls name = BU_VLS_INIT_ZERO;

	skip_ws();
	if (pos >= buf_len)
	    break;

	if (parse_node(&name, identity)) {
	    mk_addmember(bu_vls_cstr(&name), &all_head.l, NULL, WMOP_UNION);
	}

	bu_vls_free(&name);
    }
}


static void
usage(const char *argv0)
{
    bu_log("Usage: %s [-d] input.csg output.g\n", argv0);
    bu_log("	where input.csg is an OpenSCAD CSG export file\n");
    bu_log("	(produced by: openscad -o file.csg input.scad)\n");
    bu_log("	and output.g is the name of a BRL-CAD database file to receive the conversion.\n");
    bu_log("	The -d option prints additional debugging information.\n");
}


int
main(int argc, char *argv[])
{
    int c;
    long file_size;

    bu_setprogname(argv[0]);

    if (argc < 2) {
	usage(argv[0]);
	bu_exit(1, NULL);
    }

    while ((c = bu_getopt(argc, argv, "d")) != -1) {
	switch (c) {
	    case 'd':
		debug = 1;
		break;
	    default:
		usage(argv[0]);
		bu_exit(1, NULL);
		break;
	}
    }

    if (bu_optind + 1 >= argc) {
	usage(argv[0]);
	bu_exit(1, "csg-g: need input and output file names\n");
    }

    input_file = argv[bu_optind];
    brlcad_file = argv[bu_optind + 1];

    /* Open input file and read entire contents */
    if ((fd_in = fopen(input_file, "r")) == NULL) {
	bu_log("Cannot open input file (%s)\n", input_file);
	perror("csg-g");
	bu_exit(1, NULL);
    }

    fseek(fd_in, 0, SEEK_END);
    file_size = ftell(fd_in);
    fseek(fd_in, 0, SEEK_SET);

    buf = (char *)bu_malloc(file_size + 1, "input buffer");
    if (fread(buf, 1, file_size, fd_in) != (size_t)file_size) {
	bu_log("Error reading input file (%s)\n", input_file);
	perror("csg-g");
	bu_exit(1, NULL);
    }
    buf[file_size] = '\0';
    buf_len = file_size;
    fclose(fd_in);

    /* Open output */
    if ((fd_out = wdb_fopen(brlcad_file)) == NULL) {
	bu_log("Cannot open BRL-CAD file (%s)\n", brlcad_file);
	perror("csg-g");
	bu_exit(1, NULL);
    }

    mk_id_units(fd_out, "Conversion from OpenSCAD CSG format", "mm");

    BU_LIST_INIT(&all_head.l);

    bu_log("Converting OpenSCAD CSG file: %s\n", input_file);

    Convert_input();

    /* Make a top level group */
    if (BU_LIST_NON_EMPTY(&all_head.l)) {
	mk_lcomb(fd_out, "all", &all_head, 0,
		 (char *)NULL, (char *)NULL, (unsigned char *)NULL, 0);
    }

    bu_log("Conversion complete: %d solids, %d combinations\n",
	   solid_count, comb_count);

    bu_free(buf, "input buffer");
    db_close(fd_out->dbip);

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
