/*                  R O T A T E _ A R B _ F A C E . C
 * BRL-CAD
 *
 * Copyright (c) 2008-2026 United States Government as represented by
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
/** @file libged/rotate_arb_face.c
 *
 * The rotate_arb_face command.
 *
 */

#include "common.h"

#include <string.h>

#include "bu/cmd.h"
#include "bu/cmdschema.h"
#include "rt/geom.h"

#include "../ged_private.h"
#include "ged_arb.h"


static const short int arb_vertices[5][24] = {
    { 1, 2, 3, 0, 1, 2, 4, 0, 2, 3, 4, 0, 1, 3, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0 },	/* arb4 */
    { 1, 2, 3, 4, 1, 2, 5, 0, 2, 3, 5, 0, 3, 4, 5, 0, 1, 4, 5, 0, 0, 0, 0, 0 },	/* arb5 */
    { 1, 2, 3, 4, 2, 3, 6, 5, 1, 5, 6, 4, 1, 2, 5, 0, 3, 4, 6, 0, 0, 0, 0, 0 },	/* arb6 */
    { 1, 2, 3, 4, 5, 6, 7, 0, 1, 4, 5, 0, 2, 3, 7, 6, 1, 2, 6, 5, 4, 3, 7, 5 },	/* arb7 */
    { 1, 2, 3, 4, 5, 6, 7, 8, 1, 5, 8, 4, 2, 3, 7, 6, 1, 2, 6, 5, 4, 3, 7, 8 }	/* arb8 */
};


static int
rotate_arb_face_count(int arb_type)
{
    switch (arb_type) {
	case ARB4: return 4;
	case ARB5:
	case ARB6: return 5;
	case ARB7:
	case ARB8: return 6;
	default: return 0;
    }
}


static int
rotate_arb_face_index_validate(struct bu_vls *msg, const char *arg)
{
    int value;

    if (bu_cmd_integer_from_str(&value, arg) && value >= 1 && value <= 6)
	return 0;
    if (msg)
	bu_vls_printf(msg, "ARB face must be an integer from 1 through 6: %s\n", arg ? arg : "");
    return -1;
}


static int
rotate_arb_vertex_index_validate(struct bu_vls *msg, const char *arg)
{
    int value;

    if (bu_cmd_integer_from_str(&value, arg) && value >= 1 && value <= 8)
	return 0;
    if (msg)
	bu_vls_printf(msg, "ARB vertex must be an integer from 1 through 8: %s\n", arg ? arg : "");
    return -1;
}


static int
rotate_arb_face_schema_validate(const struct bu_cmd_schema *schema, size_t argc,
	const char **argv, size_t cursor_arg, struct bu_cmd_validate_result *result)
{
    struct bu_cmd_schema flat = *schema;
    fastf_t rotation[3] = {0.0, 0.0, 0.0};
    int ret;

    flat.validation.custom_validate = NULL;
    ret = bu_cmd_schema_validate(&flat, argc, argv, cursor_arg, result);
    if (ret || result->state == BU_CMD_VALIDATE_INVALID || argc < 4)
	return ret;
    if (bu_cmd_vector3_from_argv(rotation, 1, (const char * const *)argv + 3) != 1) {
	bu_cmd_validate_result_clear(result);
	result->state = BU_CMD_VALIDATE_INVALID;
	result->token_start = 3;
	result->token_end = 3;
	result->expected = BU_CMD_EXPECT_OPERAND;
	result->completion_type = BU_CMD_VALUE_VECTOR;
	result->semantic_provider = "ged.vector_group";
	result->hint = "packed finite XYZ rotation required";
    } else {
	/* The schema owns this packed vector as a whole. */
	result->semantic_provider = "ged.vector_group";
    }
    return 0;
}


static const struct bu_cmd_operand rotate_arb_face_operands[] = {
    BU_CMD_OPERAND("arb", BU_CMD_VALUE_DB_PATH, 1, 1,
	"ARB object or path", "ged.db_path"),
    BU_CMD_OPERAND_VALIDATE("face", BU_CMD_VALUE_INTEGER, 1, 1,
	rotate_arb_face_index_validate, "Face number (1 through 6)", NULL),
    BU_CMD_OPERAND_VALIDATE("vertex", BU_CMD_VALUE_INTEGER, 1, 1,
	rotate_arb_vertex_index_validate, "Vertex number (1 through 8)", NULL),
    BU_CMD_OPERAND("rotation", BU_CMD_VALUE_VECTOR, 1, 1,
	"Packed XYZ rotation angles", "ged.vector_group"),
    BU_CMD_OPERAND_NULL
};
extern const struct bu_cmd_schema ged_rotate_arb_face_schema = {
    "rotate_arb_face", "Rotate an ARB face about a vertex", NULL,
    rotate_arb_face_operands, BU_CMD_PARSE_STOP_AT_FIRST_OPERAND,
    BU_CMD_SCHEMA_CONSTRAINTS(rotate_arb_face_schema_validate, NULL)
};


int
ged_rotate_arb_face_core(struct ged *gedp, int argc, const char *argv[])
{
    struct rt_db_internal intern;
    struct rt_arb_internal *arb;
    struct directory *dp;
    fastf_t planes[7][4];		/* ARBs defining plane equations */
    int arb_type;
    int face;
    int vi;
    mat_t mat;
    int i;
    int pnt5;		/* special arb7 case */
    char *last;

    fastf_t pt[3];

    static const char *usage = "arb face pt rvec";

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_READ_ONLY(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    if (bu_cmd_schema_parse_complete(&ged_rotate_arb_face_schema, NULL,
	gedp->ged_result_str, argc - 1, argv + 1) < 0) {
	if (!bu_vls_strlen(gedp->ged_result_str))
	    bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    if ((last = strrchr(argv[1], '/')) == NULL)
	last = (char *)argv[1];
    else
	++last;

    if (last[0] == '\0') {
	bu_vls_printf(gedp->ged_result_str, "illegal input - %s", argv[1]);
	return BRLCAD_ERROR;
    }

    if ((dp = db_lookup(gedp->dbip, last, LOOKUP_QUIET)) == RT_DIR_NULL) {
	bu_vls_printf(gedp->ged_result_str, "%s not found", argv[1]);
	return BRLCAD_ERROR;
    }

    struct rt_wdb *wdbp = wdb_dbopen(gedp->dbip, RT_WDB_TYPE_DB_DEFAULT);
    if (wdb_import_from_path2(gedp->ged_result_str, &intern, argv[1], wdbp, mat) & BRLCAD_ERROR) {
	return BRLCAD_ERROR;
    }

    if (intern.idb_major_type != DB5_MAJORTYPE_BRLCAD ||
	intern.idb_minor_type != DB5_MINORTYPE_BRLCAD_ARB8) {
	bu_vls_printf(gedp->ged_result_str, "Object not an ARB");
	rt_db_free_internal(&intern);

	return BRLCAD_ERROR;
    }

    if (!bu_cmd_integer_from_str(&face, argv[2])) {
	bu_vls_printf(gedp->ged_result_str, "bad face - %s", argv[2]);
	rt_db_free_internal(&intern);

	return BRLCAD_ERROR;
    }

    face -= 1;
    if (face < 0 || 5 < face) {
	bu_vls_printf(gedp->ged_result_str, "bad face - %s", argv[2]);
	rt_db_free_internal(&intern);

	return BRLCAD_ERROR;
    }

    if (!bu_cmd_integer_from_str(&vi, argv[3])) {
	bu_vls_printf(gedp->ged_result_str, "bad vertex index - %s", argv[3]);
	rt_db_free_internal(&intern);

	return BRLCAD_ERROR;
    }


    vi -= 1;
    if (vi < 0 || 7 < vi) {
	bu_vls_printf(gedp->ged_result_str, "bad vertex - %s", argv[2]);
	rt_db_free_internal(&intern);

	return BRLCAD_ERROR;
    }

    if (bu_cmd_vector3_from_argv(pt, 1, (const char * const *)argv + 4) != 1) {
	bu_vls_printf(gedp->ged_result_str, "bad rotation vector - %s", argv[4]);
	rt_db_free_internal(&intern);

	return BRLCAD_ERROR;
    }

    arb = (struct rt_arb_internal *)intern.idb_ptr;
    RT_ARB_CK_MAGIC(arb);

    arb_type = rt_arb_std_type(&intern, &wdbp->wdb_tol);

    if (face >= rotate_arb_face_count(arb_type)) {
	bu_vls_printf(gedp->ged_result_str,
	    "bad face - %s is not valid for this ARB type", argv[2]);
	rt_db_free_internal(&intern);
	return BRLCAD_ERROR;
    }

    if (rt_arb_calc_planes(gedp->ged_result_str, arb, arb_type, planes, &wdbp->wdb_tol)) {
	rt_db_free_internal(&intern);

	return BRLCAD_ERROR;
    }

    /* special case for arb4 */
    if (arb_type == ARB4 && vi >= 3)
	vi = 4;

    /* special case for arb6 */
    if (arb_type == ARB6 && vi >= 5)
	vi = 6;

    /* special case for arb7 */
    if (arb_type == ARB7) {
	/* check if point 5 is in the face */
	pnt5 = 0;
	for (i = 0; i < 4; i++) {
	    if (arb_vertices[arb_type-4][face*4+i]==5)
		pnt5=1;
	}

	if (pnt5)
	    vi = 4;
    }

    {
	/* Apply incremental changes */
	vect_t tempvec;
	vect_t work;
	fastf_t *plane;
	mat_t rmat;

	bn_mat_angles(rmat, pt[X], pt[Y], pt[Z]);

	plane = &planes[face][0];
	VMOVE(work, plane);
	MAT4X3VEC(plane, rmat, work);

	/* point notation of fixed vertex */
	VMOVE(tempvec, arb->pt[vi]);

	/* set D of planar equation to anchor at fixed vertex */
	planes[face][3]=VDOT(plane, tempvec);
    }

    /* calculate new points for the arb */
    (void)rt_arb_calc_points(arb, arb_type, (const plane_t *)planes, &wdbp->wdb_tol);

    {
	mat_t invmat;

	bn_mat_inv(invmat, mat);

	for (i = 0; i < 8; ++i) {
	    point_t arb_pt;

	    MAT4X3PNT(arb_pt, invmat, arb->pt[i]);
	    VMOVE(arb->pt[i], arb_pt);
	}

	GED_DB_PUT_INTERN(gedp, dp, &intern, BRLCAD_ERROR);
    }

    rt_db_free_internal(&intern);
    return BRLCAD_OK;
}


/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
