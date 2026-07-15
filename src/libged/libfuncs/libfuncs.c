/*                        L I B F U N C S . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2026 United States Government as represented by
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
/** @file libged/libfuncs.c
 *
 * BRL-CAD's libged wrappers for various libraries.
 *
 */

#include "common.h"

#include "bu/cmdschema.h"
#include "ged.h"


static int
libfuncs_matrix_validate(struct bu_vls *msg, const char *arg)
{
    mat_t matrix;

    if (arg && bn_decode_mat(matrix, arg) >= 16)
	return 0;
    if (msg)
	bu_vls_printf(msg, "a packed 4x4 matrix is required");
    return -1;
}


static int
libfuncs_vector_validate(struct bu_vls *msg, const char *arg)
{
    vect_t vector;

    if (arg && bn_decode_vect(vector, arg) >= 3)
	return 0;
    if (msg)
	bu_vls_printf(msg, "a packed XYZ vector is required");
    return -1;
}


static const struct bu_cmd_operand mat_ae_operands[] = {
    BU_CMD_OPERAND("angles", BU_CMD_VALUE_NUMBER, 2, 2,
	"Azimuth and elevation", NULL),
    BU_CMD_OPERAND_NULL
};
static const struct bu_cmd_operand mat_mul_operands[] = {
    BU_CMD_OPERAND_VALIDATE("matrices", BU_CMD_VALUE_MATRIX, 2, 2,
	libfuncs_matrix_validate, "Two packed 4x4 matrices", NULL),
    BU_CMD_OPERAND_NULL
};
static const struct bu_cmd_operand mat_point_operands[] = {
    BU_CMD_OPERAND_VALIDATE("matrix", BU_CMD_VALUE_MATRIX, 1, 1,
	libfuncs_matrix_validate, "Packed 4x4 matrix", NULL),
    BU_CMD_OPERAND_VALIDATE("point", BU_CMD_VALUE_VECTOR, 1, 1,
	libfuncs_vector_validate, "Packed XYZ point", NULL),
    BU_CMD_OPERAND_NULL
};
static const struct bu_cmd_operand point_scale_operands[] = {
    BU_CMD_OPERAND_VALIDATE("point", BU_CMD_VALUE_VECTOR, 1, 1,
	libfuncs_vector_validate, "Packed XYZ point", NULL),
    BU_CMD_OPERAND("scale", BU_CMD_VALUE_NUMBER, 1, 1,
	"Scale factor", NULL),
    BU_CMD_OPERAND_NULL
};
#define LIB_SCHEMA(_id, _name, _help, _ops) \
    static const struct bu_cmd_schema _id##_cmd_schema = { \
	_name, _help, NULL, _ops, BU_CMD_PARSE_STOP_AT_FIRST_OPERAND, \
	BU_CMD_SCHEMA_CONSTRAINTS(NULL, NULL)}
LIB_SCHEMA(mat4x3pnt, "mat4x3pnt", "Transform a point by a matrix", mat_point_operands);
LIB_SCHEMA(mat_ae, "mat_ae", "Create a matrix from azimuth and elevation", mat_ae_operands);
LIB_SCHEMA(mat_mul, "mat_mul", "Multiply two matrices", mat_mul_operands);
LIB_SCHEMA(mat_scale_about_pnt, "mat_scale_about_pnt", "Create a scale-about-point matrix", point_scale_operands);
#undef LIB_SCHEMA

int
ged_mat_ae_core(struct ged *gedp,
	   int argc,
	   const char *argv[])
{
    mat_t o;
    fastf_t az, el;
    static const char *usage = "azimuth elevation";

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    if (bu_cmd_schema_parse_complete(&mat_ae_cmd_schema, NULL, NULL,
	argc - 1, argv + 1) < 0 ||
	!bu_cmd_number_from_str(&az, argv[1]) ||
	!bu_cmd_number_from_str(&el, argv[2])) {
	bu_vls_printf(gedp->ged_result_str, "usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    bn_mat_ae(o, az, el);
    bn_encode_mat(gedp->ged_result_str, o, 1);

    return BRLCAD_OK;
}


int
ged_mat_mul_core(struct ged *gedp,
	    int argc,
	    const char *argv[])
{
    mat_t o, a, b;
    static const char *usage = "matA matB";

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    if (bu_cmd_schema_parse_complete(&mat_mul_cmd_schema, NULL, NULL,
	argc - 1, argv + 1) < 0 ||
	bn_decode_mat(a, argv[1]) < 16 || bn_decode_mat(b, argv[2]) < 16) {
	bu_vls_printf(gedp->ged_result_str, "usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    bn_mat_mul(o, a, b);
    bn_encode_mat(gedp->ged_result_str, o, 1);

    return BRLCAD_OK;
}


int
ged_mat4x3pnt_core(struct ged *gedp,
	      int argc,
	      const char *argv[])
{
    mat_t m = MAT_INIT_ZERO;
    point_t i, o;
    static const char *usage = "matrix point";

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

	if (bu_cmd_schema_parse_complete(&mat4x3pnt_cmd_schema, NULL, NULL,
	argc - 1, argv + 1) < 0 || bn_decode_mat(m, argv[1]) < 16 ||
	bn_decode_vect(i, argv[2]) < 3) {
	bu_vls_printf(gedp->ged_result_str, "usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    MAT4X3PNT(o, m, i);
    bn_encode_vect(gedp->ged_result_str, o, 1);

    return BRLCAD_OK;
}


int
ged_mat_scale_about_pnt_core(struct ged *gedp,
		       int argc,
		       const char *argv[])
{
    mat_t o;
    vect_t v;
    fastf_t scale;
    static const char *usage = "point scale";

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

	if (bu_cmd_schema_parse_complete(&mat_scale_about_pnt_cmd_schema, NULL, NULL,
	argc - 1, argv + 1) < 0 || bn_decode_vect(v, argv[1]) < 3 ||
	!bu_cmd_number_from_str(&scale, argv[2])) {
	bu_vls_printf(gedp->ged_result_str, "usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    if (bn_mat_scale_about_pnt(o, v, scale) != 0) {
	bu_vls_printf(gedp->ged_result_str, "error performing calculation");
	return BRLCAD_ERROR;
    }

    bn_encode_mat(gedp->ged_result_str, o, 1);

    return BRLCAD_OK;
}

#include "../include/plugin.h"

#define GED_LIBFUNCS_COMMANDS(X, XID) \
    X(mat4x3pnt, ged_mat4x3pnt_core, GED_CMD_DEFAULT, &mat4x3pnt_cmd_schema) \
    X(mat_ae, ged_mat_ae_core, GED_CMD_DEFAULT, &mat_ae_cmd_schema) \
    X(mat_mul, ged_mat_mul_core, GED_CMD_DEFAULT, &mat_mul_cmd_schema) \
    X(mat_scale_about_pnt, ged_mat_scale_about_pnt_core, GED_CMD_DEFAULT, &mat_scale_about_pnt_cmd_schema) \

GED_DECLARE_COMMAND_SET_WITH_NATIVE_SCHEMA(GED_LIBFUNCS_COMMANDS)
GED_DECLARE_PLUGIN_MANIFEST_WITH_NATIVE_SCHEMA("libged_libfuncs", 1, GED_LIBFUNCS_COMMANDS)

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
