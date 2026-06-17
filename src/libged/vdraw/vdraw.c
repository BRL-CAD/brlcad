/*                         V D R A W . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @addtogroup libged */
/** @{ */
/** @file libged/vdraw.c
 *
 * Edit vector lists and display them as pseudosolids.
 *
 * OPEN COMMAND
 * vdraw	open			- with no argument, asks if there is
 * an open vlist (1 yes, 0 no)
 *
 * name		- opens the specified vlist
 * returns 1 if creating new vlist
 * 0 if opening an existing vlist
 *
 * EDITING COMMANDS - no return value
 *
 * vdraw write i  c x y z	- write params into i-th vector
 * next	c x y z	- write params to end of vector list
 *
 * vdraw insert i	c x y z	- insert params in front of i-th vector
 *
 * vdraw delete i		- delete i-th vector
 * last		- delete last vector on list
 * all		- delete all vectors on list
 *
 * PARAMETER SETTING COMMAND - no return value
 * vdraw params color		- set the current color with 6 hex digits
 * representing rrggbb
 * name		- change the name of the current vlist
 *
 * QUERY COMMAND
 * vdraw read i	- returns contents of i-th vector "c x y z"
 * color	- return the current color in hex
 * length	- return number of vectors in list
 * name		- return name of current vlist
 *
 * DISPLAY COMMAND -
 * vdraw send		- send the current vlist to the display
 * returns 0 on success, -1 if the name
 * conflicts with an existing true solid
 *
 * CURVE COMMANDS
 * vdraw vlist list	- return list of all existing vlists
 * delete name		- delete the named vlist
 *
 * All textual arguments can be replaced by their first letter.
 * (e.g. "vdraw d a" instead of "vdraw delete all"
 *
 * In the above listing:
 * "i" refers to an integer
 * "c" is an integer representing one of the following vdraw commands:
 *
 * VDRAW_CMD_LINE_MOVE     0 / begin new line /
 * VDRAW_CMD_LINE_DRAW     1 / draw line /
 * VDRAW_CMD_POLY_START    2 / pt[] has surface normal /
 * VDRAW_CMD_POLY_MOVE     3 / move to first poly vertex /
 * VDRAW_CMD_POLY_DRAW     4 / subsequent poly vertex /
 * VDRAW_CMD_POLY_END      5 / last vert (repeats 1st), draw poly /
 * VDRAW_CMD_POLY_VERTNORM 6 / per-vertex normal, for interpolation /
 *
 * "x y z" refer to floating point values which represent a point or
 * normal vector. For commands 0, 1, 3, 4, and 5, they represent a
 * point, while for commands 2 and 6 they represent normal vectors
 *
 * Example Use -
 * vdraw open rays
 * vdraw delete all
 * foreach partition $ray {
 * ...stuff...
 * vdraw write next 0 $inpt
 * vdraw write next 1 $outpt
 * }
 * vdraw send
 *
 */
/** @} */

#include "common.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "bu/cmd.h"
#include "bu/interrupt.h"
#include "bu/malloc.h"
#include "bn.h"
#include "vmath.h"
#include "bsg/geometry.h"
#include "raytrace.h"
#include "nmg.h"

#include "../ged_private.h"

enum vdraw_command {
    VDRAW_CMD_LINE_MOVE = 0,
    VDRAW_CMD_LINE_DRAW = 1,
    VDRAW_CMD_POLY_START = 2,
    VDRAW_CMD_POLY_MOVE = 3,
    VDRAW_CMD_POLY_DRAW = 4,
    VDRAW_CMD_POLY_END = 5,
    VDRAW_CMD_POLY_VERTNORM = 6,
    VDRAW_CMD_TRI_START = 7,
    VDRAW_CMD_TRI_MOVE = 8,
    VDRAW_CMD_TRI_DRAW = 9,
    VDRAW_CMD_TRI_END = 10,
    VDRAW_CMD_TRI_VERTNORM = 11,
    VDRAW_CMD_POINT_DRAW = 12
};

static void
vdraw_curve_clear(struct vd_curve *curve)
{
    if (!curve)
	return;
    curve->vdc_count = 0;
}

static void
vdraw_curve_free(struct vd_curve *curve)
{
    if (!curve)
	return;
    if (curve->vdc_points)
	bu_free(curve->vdc_points, "vdraw points");
    if (curve->vdc_commands)
	bu_free(curve->vdc_commands, "vdraw commands");
    curve->vdc_points = NULL;
    curve->vdc_commands = NULL;
    curve->vdc_count = 0;
    curve->vdc_capacity = 0;
}

static int
vdraw_curve_reserve(struct vd_curve *curve, size_t capacity)
{
    if (!curve)
	return 0;
    if (capacity <= curve->vdc_capacity)
	return 1;

    size_t new_capacity = curve->vdc_capacity ? curve->vdc_capacity : 64;
    while (new_capacity < capacity) {
	if (new_capacity > ((size_t)-1) / 2)
	    return 0;
	new_capacity *= 2;
    }

    curve->vdc_points = (point_t *)bu_realloc(curve->vdc_points,
	    new_capacity * sizeof(point_t), "vdraw points");
    curve->vdc_commands = (int *)bu_realloc(curve->vdc_commands,
	    new_capacity * sizeof(int), "vdraw commands");
    curve->vdc_capacity = new_capacity;
    return 1;
}

static int
vdraw_curve_set(struct vd_curve *curve, size_t idx, int command, const point_t point)
{
    if (!curve || idx > curve->vdc_count)
	return 0;
    if (!vdraw_curve_reserve(curve, idx + 1))
	return 0;
    curve->vdc_commands[idx] = command;
    VMOVE(curve->vdc_points[idx], point);
    if (idx == curve->vdc_count)
	curve->vdc_count++;
    return 1;
}

static int
vdraw_curve_insert(struct vd_curve *curve, size_t idx, int command, const point_t point)
{
    if (!curve || idx > curve->vdc_count)
	return 0;
    if (!vdraw_curve_reserve(curve, curve->vdc_count + 1))
	return 0;
    if (idx < curve->vdc_count) {
	memmove(&curve->vdc_commands[idx + 1], &curve->vdc_commands[idx],
		(curve->vdc_count - idx) * sizeof(int));
	memmove(&curve->vdc_points[idx + 1], &curve->vdc_points[idx],
		(curve->vdc_count - idx) * sizeof(point_t));
    }
    curve->vdc_commands[idx] = command;
    VMOVE(curve->vdc_points[idx], point);
    curve->vdc_count++;
    return 1;
}

static int
vdraw_curve_delete(struct vd_curve *curve, size_t idx)
{
    if (!curve || idx >= curve->vdc_count)
	return 0;
    if (idx + 1 < curve->vdc_count) {
	memmove(&curve->vdc_commands[idx], &curve->vdc_commands[idx + 1],
		(curve->vdc_count - idx - 1) * sizeof(int));
	memmove(&curve->vdc_points[idx], &curve->vdc_points[idx + 1],
		(curve->vdc_count - idx - 1) * sizeof(point_t));
    }
    curve->vdc_count--;
    return 1;
}


/*
 * Usage:
 *        vdraw write i|next c x y z
 */
static int
vdraw_write(void *data, int argc, const char *argv[])
{
    struct ged *gedp = (struct ged *)data;
    size_t idx;
    unsigned long uind = 0;
    static const char *usage = "i|next c x y z";
    point_t point = VINIT_ZERO;
    int command = 0;

    /* must be wanting help */
    if (argc == 2) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s %s", argv[0], argv[1], usage);
	return GED_HELP;
    }

    if (!gedp->i->ged_gdp->gd_currVHead) {
	bu_vls_printf(gedp->ged_result_str, "vdraw write: no vlist is currently open.");
	return BRLCAD_ERROR;
    }
    if (argc < 5) {
	bu_vls_printf(gedp->ged_result_str, "vdraw write: not enough args\n");
	return BRLCAD_ERROR;
    }
    if (argv[2][0] == 'n') {
	idx = gedp->i->ged_gdp->gd_currVHead->vdc_count;
    } else if (sscanf(argv[2], "%lu", &uind) < 1) {
	bu_vls_printf(gedp->ged_result_str, "vdraw: write index not an integer\n");
	return BRLCAD_ERROR;
    } else {
	idx = (size_t)uind;
	if (idx > gedp->i->ged_gdp->gd_currVHead->vdc_count) {
	    bu_vls_printf(gedp->ged_result_str, "vdraw: write out of range\n");
	    return BRLCAD_ERROR;
	}
    }

    if (sscanf(argv[3], "%d", &command) < 1) {
	bu_vls_printf(gedp->ged_result_str, "vdraw: cmd not an integer\n");
	return BRLCAD_ERROR;
    }
    if (argc == 7) {
	point[0] = atof(argv[4]);
	point[1] = atof(argv[5]);
	point[2] = atof(argv[6]);
    } else {
	if (argc != 5 ||
	    bn_decode_vect(point, argv[4]) != 3) {
	    bu_vls_printf(gedp->ged_result_str, "vdraw write: wrong # args, need either x y z or {x y z}\n");
	    return BRLCAD_ERROR;
	}
    }

    if (!vdraw_curve_set(gedp->i->ged_gdp->gd_currVHead, idx, command, point)) {
	bu_vls_printf(gedp->ged_result_str, "vdraw: write failed\n");
	return BRLCAD_ERROR;
    }

    return BRLCAD_OK;
}


/*
 * Usage:
 *        vdraw insert i c x y z
 */
int
vdraw_insert(void *data, int argc, const char *argv[])
{
    struct ged *gedp = (struct ged *)data;
    size_t idx;
    unsigned long uind = 0;
    static const char *usage = "i c x y z";
    point_t point = VINIT_ZERO;
    int command = 0;

    /* must be wanting help */
    if (argc == 2) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s %s", argv[0], argv[1], usage);
	return GED_HELP;
    }

    if (!gedp->i->ged_gdp->gd_currVHead) {
	bu_vls_printf(gedp->ged_result_str, "vdraw: no vlist is currently open.");
	return BRLCAD_ERROR;
    }
    if (argc < 7) {
	bu_vls_printf(gedp->ged_result_str, "vdraw: not enough args");
	return BRLCAD_ERROR;
    }
    if (sscanf(argv[2], "%lu", &uind) < 1) {
	bu_vls_printf(gedp->ged_result_str, "vdraw: insert index not an integer\n");
	return BRLCAD_ERROR;
    }

    idx = (size_t)uind;
    if (idx > gedp->i->ged_gdp->gd_currVHead->vdc_count) {
	bu_vls_printf(gedp->ged_result_str, "vdraw: insert out of range\n");
	return BRLCAD_ERROR;
    }

    if (sscanf(argv[3], "%d", &command) < 1) {
	bu_vls_printf(gedp->ged_result_str, "vdraw: cmd not an integer\n");
	return BRLCAD_ERROR;
    }
    point[0] = atof(argv[4]);
    point[1] = atof(argv[5]);
    point[2] = atof(argv[6]);

    if (!vdraw_curve_insert(gedp->i->ged_gdp->gd_currVHead, idx, command, point)) {
	bu_vls_printf(gedp->ged_result_str, "vdraw: insert failed\n");
	return BRLCAD_ERROR;
    }

    return BRLCAD_OK;
}


/*
 * Usage:
 *        vdraw delete i|last|all
 */
int
vdraw_delete(void *data, int argc, const char *argv[])
{
    struct ged *gedp = (struct ged *)data;
    unsigned long uind = 0;
    static const char *usage = "i|last|all";

    /* must be wanting help */
    if (argc == 2) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s %s", argv[0], argv[1], usage);
	return GED_HELP;
    }

    if (!gedp->i->ged_gdp->gd_currVHead) {
	bu_vls_printf(gedp->ged_result_str, "%s %s: no vlist is currently open.", argv[0], argv[1]);
	return BRLCAD_ERROR;
    }
    if (argc < 3) {
	bu_vls_printf(gedp->ged_result_str, "%s %s: not enough args\n", argv[0], argv[1]);
	return BRLCAD_ERROR;
    }
    if (argv[2][0] == 'a') {
	vdraw_curve_clear(gedp->i->ged_gdp->gd_currVHead);
	return BRLCAD_OK;
    }
    if (argv[2][0] == 'l') {
	if (gedp->i->ged_gdp->gd_currVHead->vdc_count)
	    gedp->i->ged_gdp->gd_currVHead->vdc_count--;
	return BRLCAD_OK;
    }
    if (sscanf(argv[2], "%lu", &uind) < 1) {
	bu_vls_printf(gedp->ged_result_str, "%s %s: delete index not an integer\n", argv[0], argv[1]);
	return BRLCAD_ERROR;
    }

    if ((size_t)uind >= gedp->i->ged_gdp->gd_currVHead->vdc_count) {
	bu_vls_printf(gedp->ged_result_str, "%s %s: delete out of range\n", argv[0], argv[1]);
	return BRLCAD_ERROR;
    }

    if (!vdraw_curve_delete(gedp->i->ged_gdp->gd_currVHead, (size_t)uind)) {
	bu_vls_printf(gedp->ged_result_str, "%s %s: delete failed", argv[0], argv[1]);
	return BRLCAD_ERROR;
    }

    return BRLCAD_OK;
}


/*
 * Usage:
 *        vdraw read i|color|length|name
 */
static int
vdraw_read(void *data, int argc, const char *argv[])
{
    struct ged *gedp = (struct ged *)data;
    unsigned long uind = 0;
    static const char *usage = "read i|color|length|name";

    /* must be wanting help */
    if (argc == 2) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s %s", argv[0], argv[1], usage);
	return GED_HELP;
    }

    if (!gedp->i->ged_gdp->gd_currVHead) {
	bu_vls_printf(gedp->ged_result_str, "%s %s: no vlist is currently open.", argv[0], argv[1]);
	return BRLCAD_ERROR;
    }
    if (argc < 3) {
	bu_vls_printf(gedp->ged_result_str, "%s %s: need index to read\n", argv[0], argv[1]);
	return BRLCAD_ERROR;
    }
    if (argv[2][0] == 'c') {
	/* read color of current solid */
	bu_vls_printf(gedp->ged_result_str, "%.6lx", gedp->i->ged_gdp->gd_currVHead->vdc_rgb);
	return BRLCAD_OK;
    }
    if (argv[2][0] == 'n') {
	/*read name of currently open solid*/
	bu_vls_printf(gedp->ged_result_str, "%.89s", gedp->i->ged_gdp->gd_currVHead->vdc_name);
	return BRLCAD_OK;
    }
    if (argv[2][0] == 'l') {
	bu_vls_printf(gedp->ged_result_str, "%zu",
		gedp->i->ged_gdp->gd_currVHead->vdc_count);
	return BRLCAD_OK;
    }
    if (sscanf(argv[2], "%lu", &uind) < 1) {
	bu_vls_printf(gedp->ged_result_str, "%s %s: read index not an integer\n", argv[0], argv[1]);
	return BRLCAD_ERROR;
    }

    if ((size_t)uind >= gedp->i->ged_gdp->gd_currVHead->vdc_count) {
	bu_vls_printf(gedp->ged_result_str, "%s %s: read out of range\n", argv[0], argv[1]);
	return BRLCAD_ERROR;
    }

    bu_vls_printf(gedp->ged_result_str, "%d %.12e %.12e %.12e",
		  gedp->i->ged_gdp->gd_currVHead->vdc_commands[uind],
		  gedp->i->ged_gdp->gd_currVHead->vdc_points[uind][0],
		  gedp->i->ged_gdp->gd_currVHead->vdc_points[uind][1],
		  gedp->i->ged_gdp->gd_currVHead->vdc_points[uind][2]);

    return BRLCAD_OK;
}


struct vdraw_overlay_publish_geometry {
    struct ged_draw_overlay_geometry geometry;
    point_t *points;
    int *commands;
    vect_t *normals;
    int *indices;
};


static void
vdraw_overlay_publish_geometry_free(struct vdraw_overlay_publish_geometry *pg)
{
    if (!pg)
	return;
    if (pg->points)
	bu_free(pg->points, "vdraw overlay points");
    if (pg->commands)
	bu_free(pg->commands, "vdraw overlay commands");
    if (pg->normals)
	bu_free(pg->normals, "vdraw overlay normals");
    if (pg->indices)
	bu_free(pg->indices, "vdraw overlay indices");
    memset(pg, 0, sizeof(*pg));
}


static int
vdraw_cmd_to_line_geometry(int command, int *geometry_cmd)
{
    if (!geometry_cmd)
	return 0;
    switch (command) {
	case VDRAW_CMD_LINE_MOVE:
	    *geometry_cmd = BSG_GEOMETRY_LINE_MOVE;
	    return 1;
	case VDRAW_CMD_LINE_DRAW:
	    *geometry_cmd = BSG_GEOMETRY_LINE_DRAW;
	    return 1;
	default:
	    return 0;
    }
}


static int
vdraw_geometry_line_set_compatible(const struct vd_curve *curve)
{
    if (!curve)
	return 1;

    for (size_t i = 0; i < curve->vdc_count; i++) {
	switch (curve->vdc_commands[i]) {
	    case VDRAW_CMD_LINE_MOVE:
	    case VDRAW_CMD_LINE_DRAW:
		break;
	    default:
		return 0;
	}
    }

    return 1;
}


static int
vdraw_geometry_point_set_compatible(const struct vd_curve *curve)
{
    if (!curve)
	return 1;

    for (size_t i = 0; i < curve->vdc_count; i++) {
	if (curve->vdc_commands[i] != VDRAW_CMD_POINT_DRAW)
	    return 0;
    }

    return 1;
}


static int
vdraw_geometry_face_set_stats(const struct vd_curve *curve,
			      size_t *point_count,
			      size_t *index_count)
{
    size_t points = 0;
    size_t indices = 0;
    size_t face_vertices = 0;
    int in_face = 0;

    if (point_count)
	*point_count = 0;
    if (index_count)
	*index_count = 0;
    if (!curve)
	return 0;

    for (size_t i = 0; i < curve->vdc_count; i++) {
	switch (curve->vdc_commands[i]) {
	    case VDRAW_CMD_POLY_START:
	    case VDRAW_CMD_TRI_START:
	    case VDRAW_CMD_POLY_VERTNORM:
	    case VDRAW_CMD_TRI_VERTNORM:
		break;
	    case VDRAW_CMD_POLY_MOVE:
	    case VDRAW_CMD_TRI_MOVE:
		if (in_face && face_vertices)
		    return 0;
		in_face = 1;
		face_vertices = 1;
		points++;
		indices++;
		break;
	    case VDRAW_CMD_POLY_DRAW:
	    case VDRAW_CMD_TRI_DRAW:
		if (!in_face)
		    return 0;
		face_vertices++;
		points++;
		indices++;
		break;
	    case VDRAW_CMD_POLY_END:
	    case VDRAW_CMD_TRI_END:
		if (!in_face || face_vertices < 3)
		    return 0;
		indices++;
		in_face = 0;
		face_vertices = 0;
		break;
	    default:
		return 0;
	}
    }

    if (in_face || !points || !indices)
	return 0;

    if (point_count)
	*point_count = points;
    if (index_count)
	*index_count = indices;
    return 1;
}


static int
vdraw_overlay_line_geometry_from_curve(const struct vd_curve *curve,
				       struct vdraw_overlay_publish_geometry *pg,
				       size_t count)
{
    size_t idx = 0;

    pg->points = (point_t *)bu_calloc(count ? count : 1, sizeof(point_t),
	    "vdraw overlay line points");
    pg->commands = (int *)bu_calloc(count ? count : 1, sizeof(int),
	    "vdraw overlay line commands");

    for (size_t i = 0; curve && i < curve->vdc_count && idx < count; i++) {
	int geometry_cmd = BSG_GEOMETRY_LINE_MOVE;
	if (!vdraw_cmd_to_line_geometry(curve->vdc_commands[i], &geometry_cmd))
	    continue;
	VMOVE(pg->points[idx], curve->vdc_points[i]);
	pg->commands[idx] = geometry_cmd;
	idx++;
    }

    pg->geometry.kind = BSG_GEOMETRY_NODE_LINE_SET;
    pg->geometry.points = (const point_t *)pg->points;
    pg->geometry.point_count = idx;
    pg->geometry.commands = pg->commands;
    pg->geometry.command_count = idx;
    return 1;
}


static int
vdraw_overlay_point_geometry_from_curve(const struct vd_curve *curve,
					struct vdraw_overlay_publish_geometry *pg,
					size_t count)
{
    size_t idx = 0;

    pg->points = (point_t *)bu_calloc(count ? count : 1, sizeof(point_t),
	    "vdraw overlay point points");

    for (size_t i = 0; curve && i < curve->vdc_count && idx < count; i++, idx++)
	VMOVE(pg->points[idx], curve->vdc_points[i]);

    pg->geometry.kind = BSG_GEOMETRY_NODE_POINT_SET;
    pg->geometry.points = (const point_t *)pg->points;
    pg->geometry.point_count = idx;
    return 1;
}


static int
vdraw_overlay_face_geometry_from_curve(const struct vd_curve *curve,
				       struct vdraw_overlay_publish_geometry *pg,
				       size_t point_count,
				       size_t index_count)
{
    size_t point_idx = 0;
    size_t index_idx = 0;
    vect_t face_normal = VINIT_ZERO;
    vect_t current_normal = VINIT_ZERO;

    pg->points = (point_t *)bu_calloc(point_count, sizeof(point_t),
	    "vdraw overlay face points");
    pg->normals = (vect_t *)bu_calloc(point_count, sizeof(vect_t),
	    "vdraw overlay face normals");
    pg->indices = (int *)bu_calloc(index_count, sizeof(int),
	    "vdraw overlay face indices");

    for (size_t i = 0; curve && i < curve->vdc_count; i++) {
	switch (curve->vdc_commands[i]) {
	    case VDRAW_CMD_POLY_START:
	    case VDRAW_CMD_TRI_START:
		VMOVE(face_normal, curve->vdc_points[i]);
		VMOVE(current_normal, face_normal);
		break;
	    case VDRAW_CMD_POLY_VERTNORM:
	    case VDRAW_CMD_TRI_VERTNORM:
		VMOVE(current_normal, curve->vdc_points[i]);
		break;
	    case VDRAW_CMD_POLY_MOVE:
	    case VDRAW_CMD_POLY_DRAW:
	    case VDRAW_CMD_TRI_MOVE:
	    case VDRAW_CMD_TRI_DRAW:
		VMOVE(pg->points[point_idx], curve->vdc_points[i]);
		VMOVE(pg->normals[point_idx], current_normal);
		pg->indices[index_idx++] = (int)point_idx++;
		VMOVE(current_normal, face_normal);
		break;
	    case VDRAW_CMD_POLY_END:
	    case VDRAW_CMD_TRI_END:
		pg->indices[index_idx++] = -1;
		break;
	    default:
		break;
	}
    }

    pg->geometry.kind = BSG_GEOMETRY_NODE_INDEXED_FACE_SET;
    pg->geometry.points = (const point_t *)pg->points;
    pg->geometry.point_count = point_idx;
    pg->geometry.normals = (const vect_t *)pg->normals;
    pg->geometry.normal_count = point_idx;
    pg->geometry.indices = pg->indices;
    pg->geometry.index_count = index_idx;
    return 1;
}


static int
vdraw_overlay_geometry_from_curve(const struct vd_curve *curve,
				  struct vdraw_overlay_publish_geometry *pg)
{
    size_t count = curve ? curve->vdc_count : 0;
    size_t face_points = 0;
    size_t face_indices = 0;

    if (!pg)
	return 0;
    memset(pg, 0, sizeof(*pg));

    if (vdraw_geometry_line_set_compatible(curve))
	return vdraw_overlay_line_geometry_from_curve(curve, pg, count);
    if (vdraw_geometry_point_set_compatible(curve))
	return vdraw_overlay_point_geometry_from_curve(curve, pg, count);
    if (vdraw_geometry_face_set_stats(curve, &face_points, &face_indices))
	return vdraw_overlay_face_geometry_from_curve(curve, pg,
		face_points, face_indices);

    return 0;
}


/*
 * Usage:
 *        vdraw send
 */
static int
vdraw_send(void *data, int argc, const char *argv[])
{
    struct ged *gedp = (struct ged *)data;
    struct directory *dp;
    char solid_name [RT_VDRW_MAXNAME+RT_VDRW_PREFIX_LEN+1];
    struct vdraw_overlay_publish_geometry pg;
    int idx;

    if (argc < 2) {
	bu_vls_printf(gedp->ged_result_str, "ERROR: missing parameter after [%s]", argv[0]);
	return BRLCAD_ERROR;
    }

    if (!gedp->i->ged_gdp->gd_currVHead) {
	bu_vls_printf(gedp->ged_result_str, "%s %s: no vlist is currently open.", argv[0], argv[1]);
	return BRLCAD_ERROR;
    }

    snprintf(solid_name, RT_VDRW_MAXNAME+RT_VDRW_PREFIX_LEN+1, "%s%s", RT_VDRW_PREFIX, gedp->i->ged_gdp->gd_currVHead->vdc_name);

    /* Overlays no longer create phony db entries; refuse only if the name
     * collides with an actual database object. */
    if ((dp = db_lookup(gedp->dbip, solid_name, LOOKUP_QUIET)) != RT_DIR_NULL) {
	/* solid name matches a real database entry - don't overwrite */
	bu_vls_printf(gedp->ged_result_str, "-1");
	return BRLCAD_OK;
    }

    /* 0 means OK, -1 means conflict with real solid name */
    ged_draw_shape_ref ref = GED_DRAW_SHAPE_REF_NULL;
    if (!vdraw_overlay_geometry_from_curve(gedp->i->ged_gdp->gd_currVHead, &pg)) {
	bu_vls_printf(gedp->ged_result_str, "-1");
	return BRLCAD_OK;
    }
    idx = ged_draw_overlay_geometry_insert(gedp, solid_name, &pg.geometry,
	    gedp->i->ged_gdp->gd_currVHead->vdc_rgb, 1.0, 0, 0, &ref);
    vdraw_overlay_publish_geometry_free(&pg);

    bu_vls_printf(gedp->ged_result_str, "%d", idx);

    return BRLCAD_OK;
}


/*
 * Usage:
 *        vdraw params color|name
 */
static int
vdraw_params(void *data, int argc, const char *argv[])
{
    struct ged *gedp = (struct ged *)data;
    struct vd_curve *rcp;
    unsigned long rgb;
    static const char *usage = "color|name args";

    /* must be wanting help */
    if (argc == 2) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s %s", argv[0], argv[1], usage);
	return GED_HELP;
    }

    if (!gedp->i->ged_gdp->gd_currVHead) {
	bu_vls_printf(gedp->ged_result_str, "%s %s: no vlist is currently open.", argv[0], argv[1]);
	return BRLCAD_ERROR;
    }
    if (argc < 4) {
	bu_vls_printf(gedp->ged_result_str, "%s %s: need params to set\n", argv[0], argv[1]);
	return BRLCAD_ERROR;
    }
    if (argv[2][0] == 'c') {
	if (sscanf(argv[3], "%lx", &rgb)>0)
	    gedp->i->ged_gdp->gd_currVHead->vdc_rgb = rgb;
	return BRLCAD_OK;
    }
    if (argv[2][0] == 'n') {
	/* check for conflicts with existing vlists*/
	for (BU_LIST_FOR(rcp, vd_curve, gedp->i->ged_gdp->gd_headVDraw)) {
	    if (!bu_strncmp(rcp->vdc_name, argv[2], RT_VDRW_MAXNAME)) {
		bu_vls_printf(gedp->ged_result_str, "%s %s: name %.40s is already in use\n", argv[0], argv[1], argv[2]);
		return BRLCAD_ERROR;
	    }
	}
	/* otherwise name not yet used */
	bu_strlcpy(gedp->i->ged_gdp->gd_currVHead->vdc_name, argv[2], RT_VDRW_MAXNAME);

	bu_vls_printf(gedp->ged_result_str, "0");
	return BRLCAD_OK;
    }

    return BRLCAD_OK;
}


/*
 * Usage:
 *        vdraw open [name]
 */
static int
vdraw_open(void *data, int argc, const char *argv[])
{
    struct ged *gedp = (struct ged *)data;
    struct vd_curve *rcp;
    char temp_name[RT_VDRW_MAXNAME+1];
    static const char *usage = "[name]";

    if (argc == 2) {
	if (gedp->i->ged_gdp->gd_currVHead) {
	    bu_vls_printf(gedp->ged_result_str, "1");
	    return BRLCAD_OK;
	} else {
	    bu_vls_printf(gedp->ged_result_str, "0");
	    return BRLCAD_OK;
	}
    }

    if (3 < argc) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s %s", argv[0], argv[1], usage);
	return BRLCAD_ERROR;
    }

    bu_strlcpy(temp_name, argv[2], RT_VDRW_MAXNAME);

    gedp->i->ged_gdp->gd_currVHead = (struct vd_curve *) NULL;
    for (BU_LIST_FOR(rcp, vd_curve, gedp->i->ged_gdp->gd_headVDraw)) {
	if (!bu_strncmp(rcp->vdc_name, temp_name, RT_VDRW_MAXNAME)) {
	    gedp->i->ged_gdp->gd_currVHead = rcp;
	    break;
	}
    }

    if (!gedp->i->ged_gdp->gd_currVHead) {
	/* create new entry */
	BU_GET(rcp, struct vd_curve);
	memset(rcp, 0, sizeof(*rcp));
	BU_LIST_APPEND(gedp->i->ged_gdp->gd_headVDraw, &(rcp->l));

	bu_strlcpy(rcp->vdc_name, temp_name, RT_VDRW_MAXNAME);

	rcp->vdc_rgb = RT_VDRW_DEF_COLOR;
	gedp->i->ged_gdp->gd_currVHead = rcp;
	/* 1 means new entry */
	bu_vls_printf(gedp->ged_result_str, "1");
	return BRLCAD_OK;
    } else {
	/* entry already existed */
	gedp->i->ged_gdp->gd_currVHead->vdc_name[RT_VDRW_MAXNAME] = '\0'; /*safety*/
	/* 0 means entry already existed*/
	bu_vls_printf(gedp->ged_result_str, "0");
	return BRLCAD_OK;
    }
}


/*
 * Usage:
 *        vdraw vlist list
 *        vdraw vlist delete name
 */
static int
vdraw_vlist(void *data, int argc, const char *argv[])
{
    struct ged *gedp = (struct ged *)data;
    struct vd_curve *rcp, *rcp2;
    static const char *usage = "list\n\tdelete name";

    /* must be needing help */
    if (argc < 3 || argc > 4) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s %s", argc>0?argv[0]:"vdraw", argc>1?argv[1]:"vlist", usage);
	return GED_HELP;
    }

    switch  (argv[2][0]) {
	case 'l':
	    for (BU_LIST_FOR(rcp, vd_curve, gedp->i->ged_gdp->gd_headVDraw)) {
		bu_vls_strcat(gedp->ged_result_str, rcp->vdc_name);
		bu_vls_strcat(gedp->ged_result_str, " ");
	    }

	    return BRLCAD_OK;
	case 'd':
	    rcp2 = (struct vd_curve *)NULL;
	    for (BU_LIST_FOR(rcp, vd_curve, gedp->i->ged_gdp->gd_headVDraw)) {
		if (!bu_strncmp(rcp->vdc_name, argv[3], RT_VDRW_MAXNAME)) {
		    rcp2 = rcp;
		    break;
		}
	    }
	    if (!rcp2) {
		bu_vls_printf(gedp->ged_result_str, "%s %s: vlist %.40s not found", argv[0], argv[1], argv[3]);
		return BRLCAD_ERROR;
	    }
	    BU_LIST_DEQUEUE(&(rcp2->l));
	    if (gedp->i->ged_gdp->gd_currVHead == rcp2) {
		if (BU_LIST_IS_EMPTY(gedp->i->ged_gdp->gd_headVDraw)) {
		    gedp->i->ged_gdp->gd_currVHead = (struct vd_curve *)NULL;
		} else {
		    gedp->i->ged_gdp->gd_currVHead = BU_LIST_LAST(vd_curve, gedp->i->ged_gdp->gd_headVDraw);
		}
	    }
	    vdraw_curve_free(rcp2);
	    BU_PUT(rcp2, struct vd_curve);
	    return BRLCAD_OK;
	default:
	    bu_vls_printf(gedp->ged_result_str, "%s %s: unknown option to vdraw vlist", argv[0], argv[1]);
	    return BRLCAD_ERROR;
    }
}


static int
vdraw_cmd(struct ged *gedp, int argc, const char *argv[])
{
    int ret;

    /**
     * view draw command table
     */
    static struct bu_cmdtab vdraw_cmds[] = {
	{"write",		vdraw_write},
	{"insert",		vdraw_insert},
	{"delete",		vdraw_delete},
	{"read",		vdraw_read},
	{"send",		vdraw_send},
	{"params",		vdraw_params},
	{"open",		vdraw_open},
	{"vlist",		vdraw_vlist},
	{(const char *)NULL, BU_CMD_NULL}
    };

    static const char *usage = "write|insert|delete|read|send|params|open|vlist [args]";

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_DRAWABLE(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }


    if (bu_cmd(vdraw_cmds, argc, argv, 1, gedp, &ret) == BRLCAD_OK)
	return ret;

    bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);

    return BRLCAD_ERROR;
}


int
ged_vdraw_core(struct ged *gedp, int argc, const char *argv[])
{
    return vdraw_cmd(gedp, argc, argv);
}


#include "../include/plugin.h"

#define GED_VDRAW_COMMANDS(X, XID) \
    X(vdraw, ged_vdraw_core, GED_CMD_DEFAULT) \

GED_DECLARE_COMMAND_SET(GED_VDRAW_COMMANDS)
GED_DECLARE_PLUGIN_MANIFEST("libged_vdraw", 1, GED_VDRAW_COMMANDS)

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
