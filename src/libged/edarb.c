/*                         E D A R B . C
 * BRL-CAD
 *
 * Copyright (c) 1985-2014 United States Government as represented by
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
 */
/** @file libged/edarb.c
 *
 */

#include "common.h"

#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include "bu/cmd.h"
#include "rt/arb_edit.h"
#include "ged_private.h"

/*
 * An ARB edge is moved by finding the direction of the line
 * containing the edge and the 2 "bounding" planes.  The new edge is
 * found by intersecting the new line location with the bounding
 * planes.  The two "new" planes thus defined are calculated and the
 * affected points are calculated by intersecting planes.  This keeps
 * ALL faces planar.
 *
 */
static int
editarb(struct ged *gedp, struct rt_arb_internal *arb, int type, int edge, vect_t pos_model, int newedge)
{
    int ret;
    fastf_t peqn[7][4];
    struct bu_vls error_msg = BU_VLS_INIT_ZERO;

    if (rt_arb_calc_planes(&error_msg, arb, type, peqn, &gedp->ged_wdbp->wdb_tol)) {
	bu_vls_printf(gedp->ged_result_str, "%s. Cannot calculate plane equations for faces\n", bu_vls_addr(&error_msg));
	bu_vls_free(&error_msg);
	return GED_ERROR;
    }
    bu_vls_free(&error_msg);
    ret = arb_edit(arb, peqn, edge, newedge, pos_model, &gedp->ged_wdbp->wdb_tol);
    if (!ret) {
	return GED_OK;
    } else {
	/* Error handling */
	bu_vls_printf(gedp->ged_result_str, "Error editing arb\n");
	return GED_ERROR;
    }
}


/* Extrude command - project an arb face */
/* Format: extrude face distance */
static int
edarb_extrude(void *data, int argc, const char *argv[])
{
    struct ged *gedp = (struct ged *)data;
    int type;
    struct directory *dp;
    struct rt_db_internal intern;
    struct rt_arb_internal *arb;
    int face;
    fastf_t dist;
    fastf_t peqn[7][4];
    static const char *usage = "arb face distance";

    /* must be wanting help */
    if (argc == 2) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s %s", argv[0], argv[1], usage);
	return GED_HELP;
    }

    if (argc != 5) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s %s", argv[0], argv[1], usage);
	return GED_ERROR;
    }

    GED_DB_LOOKUP(gedp, dp, (char *)argv[2], LOOKUP_QUIET, GED_ERROR);
    GED_DB_GET_INTERNAL(gedp, &intern, dp, (matp_t)NULL, &rt_uniresource, GED_ERROR);

    if (intern.idb_type != ID_ARB8) {
	bu_vls_printf(gedp->ged_result_str, "%s %s: solid type must be ARB\n", argv[0], argv[1]);
	rt_db_free_internal(&intern);
	return GED_ERROR;
    }

    type = rt_arb_std_type(&intern, &gedp->ged_wdbp->wdb_tol);
    if (type != 8 && type != 6 && type != 4) {
	bu_vls_printf(gedp->ged_result_str, "ARB%d: extrusion of faces not allowed\n", type);
	rt_db_free_internal(&intern);
	return GED_ERROR;
    }

    arb = (struct rt_arb_internal *)intern.idb_ptr;
    RT_ARB_CK_MAGIC(arb);

    face = atoi(argv[3]);

    /* get distance to project face */
    dist = atof(argv[4]);

    /* convert from the local unit (as input) to the base unit */
    dist = dist * gedp->ged_wdbp->dbip->dbi_local2base;

    if (arb_extrude(arb, face, dist, &gedp->ged_wdbp->wdb_tol, peqn)) {
	bu_vls_printf(gedp->ged_result_str, "ARB%d: error extruding face\n", type);
	rt_db_free_internal(&intern);
	return GED_ERROR;
    }

    GED_DB_PUT_INTERNAL(gedp, dp, &intern, &rt_uniresource, GED_ERROR);
    rt_db_free_internal(&intern);

    return GED_OK;
}


/* Mirface command - mirror an arb face */
/* Format: mirror face axis */
int
edarb_mirface(void *data, int argc, const char *argv[])
{
    struct ged *gedp = (struct ged *)data;
    struct directory *dp;
    struct rt_db_internal intern;
    struct rt_arb_internal *arb;
    static int face;
    fastf_t peqn[7][4];
    static const char *usage = "arb face axis";

    /* must be wanting help */
    if (argc == 2) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s %s", argv[0], argv[1], usage);
	return GED_HELP;
    }

    if (argc != 5) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s %s", argv[0], argv[1], usage);
	return GED_ERROR;
    }

    GED_DB_LOOKUP(gedp, dp, (char *)argv[2], LOOKUP_QUIET, GED_ERROR);
    GED_DB_GET_INTERNAL(gedp, &intern, dp, (matp_t)NULL, &rt_uniresource, GED_ERROR);

    if (intern.idb_type != ID_ARB8) {
	bu_vls_printf(gedp->ged_result_str, "%s %s: solid type must be ARB\n", argv[0], argv[1]);
	rt_db_free_internal(&intern);
	return GED_ERROR;
    }

    arb = (struct rt_arb_internal *)intern.idb_ptr;
    RT_ARB_CK_MAGIC(arb);

    face = atoi(argv[3]);

    if (arb_mirror_face_axis(arb, peqn, face, argv[4], &gedp->ged_wdbp->wdb_tol)) {
	bu_vls_printf(gedp->ged_result_str, "ERROR: mirror operation failed\n");
	rt_db_free_internal(&intern);
	return GED_ERROR;
    }

    GED_DB_PUT_INTERNAL(gedp, dp, &intern, &rt_uniresource, GED_ERROR);
    rt_db_free_internal(&intern);

    return GED_OK;
}


/* Edgedir command: define the direction of an arb edge being moved
 * Format: edgedir deltax deltay deltaz OR edgedir rot fb
*/
static int
edarb_edgedir(void *data, int argc, const char *argv[])
{
    struct ged *gedp = (struct ged *)data;
    int type;
    struct directory *dp;
    struct rt_db_internal intern;
    struct rt_arb_internal *arb;
    int ret;
    int i;
    int edge;
    vect_t slope;
    fastf_t rot, fb;
    static const char *usage = "arb edge [delta_x delta_y delta_z] | [rot fb]";

    /* must be wanting help */
    if (argc == 2) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s %s", argv[0], argv[1], usage);
	return GED_HELP;
    }

    if (argc < 6 || 7 < argc) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s %s", argv[0], argv[1], usage);
	return GED_ERROR;
    }

    if (sscanf(argv[3], "%d", &edge) != 1) {
	bu_vls_printf(gedp->ged_result_str, "bad edge - %s", argv[3]);
	return GED_ERROR;
    }
    edge -= 1;

    GED_DB_LOOKUP(gedp, dp, (char *)argv[2], LOOKUP_QUIET, GED_ERROR);
    GED_DB_GET_INTERNAL(gedp, &intern, dp, (matp_t)NULL, &rt_uniresource, GED_ERROR);

    if (intern.idb_type != ID_ARB8) {
	bu_vls_printf(gedp->ged_result_str, "%s %s: solid type must be ARB\n", argv[0], argv[1]);
	rt_db_free_internal(&intern);
	return GED_ERROR;
    }

    type = rt_arb_std_type(&intern, &gedp->ged_wdbp->wdb_tol);
    arb = (struct rt_arb_internal *)intern.idb_ptr;
    RT_ARB_CK_MAGIC(arb);

    /* set up slope -
     * if 2 values input assume rot, fb used
     * else assume delta_x, delta_y, delta_z
     */
    if (argc == 6) {
	rot = atof(argv[1]) * DEG2RAD;
	fb = atof(argv[2]) * DEG2RAD;
	slope[0] = cos(fb) * cos(rot);
	slope[1] = cos(fb) * sin(rot);
	slope[2] = sin(fb);
    } else {
	for (i=0; i<3; i++) {
	    /* put edge slope in slope[] array */
	    slope[i] = atof(argv[i+4]);
	}
    }

    if (ZERO(MAGNITUDE(slope))) {
	bu_vls_printf(gedp->ged_result_str, "%s %s: BAD slope\n", argv[0], argv[1]);
	rt_db_free_internal(&intern);
	return GED_ERROR;
    }

    ret = editarb(gedp, arb, type, edge, slope, 1);
    if (ret == GED_OK) {
	GED_DB_PUT_INTERNAL(gedp, dp, &intern, &rt_uniresource, GED_ERROR);
    }

    rt_db_free_internal(&intern);

    return ret;
}


/* Permute command - permute the vertex labels of an ARB
 * Format: permute tuple */

/*
 *	     Minimum and maximum tuple lengths
 *     ------------------------------------------------
 *	Solid	# vertices needed	# vertices
 *	type	 to disambiguate	in THE face
 *     ------------------------------------------------
 *	ARB4		3		    3
 *	ARB5		2		    4
 *	ARB6		2		    4
 *	ARB7		1		    4
 *	ARB8		3		    4
 *     ------------------------------------------------
 */
static int
edarb_permute(void *data, int argc, const char *argv[])
{
    struct ged *gedp = (struct ged *)data;
    struct directory *dp;
    struct rt_db_internal intern;
    struct rt_arb_internal *arb;
    static const char *usage = "arb tuple";

    /* must be wanting help */
    if (argc == 2) {
	bu_vls_printf(gedp->ged_result_str, "Usage: edarb %s %s %s", argv[0], argv[1], usage);
	return GED_HELP;
    }

    if (argc != 4) {
	bu_vls_printf(gedp->ged_result_str, "Usage: edarb %s %s %s", argv[0], argv[1], usage);
	return GED_ERROR;
    }

    GED_DB_LOOKUP(gedp, dp, (char *)argv[2], LOOKUP_QUIET, GED_ERROR);
    GED_DB_GET_INTERNAL(gedp, &intern, dp, (matp_t)NULL, &rt_uniresource, GED_ERROR);

    if (intern.idb_type != ID_ARB8) {
	bu_vls_printf(gedp->ged_result_str, "%s %s: solid type must be ARB\n", argv[0], argv[1]);
	rt_db_free_internal(&intern);
	return GED_ERROR;
    }

    arb = (struct rt_arb_internal *)intern.idb_ptr;
    RT_ARB_CK_MAGIC(arb);

    if (arb_permute(arb, argv[3], &gedp->ged_wdbp->wdb_tol)) {
	bu_vls_printf(gedp->ged_result_str, "Permute failed\n");
	rt_db_free_internal(&intern);
	return GED_ERROR;
    }

    GED_DB_PUT_INTERNAL(gedp, dp, &intern, &rt_uniresource, GED_ERROR);
    rt_db_free_internal(&intern);

    return GED_OK;
}


int
ged_edarb(struct ged *gedp, int argc, const char *argv[])
{
    int ret;
    static struct bu_cmdtab arb_cmds[] = {
	{"edgedir",		edarb_edgedir},
	{"extrude",		edarb_extrude},
	{"facedef",		edarb_facedef},
	{"mirface",		edarb_mirface},
	{"permute",		edarb_permute},
	{(const char *)NULL, BU_CMD_NULL}
    };
    static const char *usage = "edgedir|extrude|mirror|permute arb [args]";

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_DRAWABLE(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }


    if (bu_cmd(arb_cmds, argc, argv, 1, gedp, &ret) == BRLCAD_OK)
	return ret;

    bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);

    return GED_ERROR;
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
