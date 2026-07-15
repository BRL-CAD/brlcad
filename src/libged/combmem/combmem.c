/*                         C O M B M E M . C
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
/** @file libged/combmem.c
 *
 * The combmem command.
 *
 */

#include "common.h"

#include <errno.h>
#include <float.h>
#include <math.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include "bu/cmdschema.h"
#include "../ged_private.h"

enum etypes {
    ETYPES_NULL = -1,
    ETYPES_ABS,
    ETYPES_REL,
    ETYPES_ROT_AET,
    ETYPES_ROT_XYZ,
    ETYPES_ROT_ARBITRARY_AXIS,
    ETYPES_TRA,
    ETYPES_SCA
};

struct combmem_args {
    int input_type;
    int replacement_type;
};

static const char * const combmem_operations[] = {"u", "-", "+", NULL};

static int
combmem_type_validate(struct bu_vls *msg, const char *arg)
{
    char *end = NULL;
    long value;

    errno = 0;
    value = strtol(arg, &end, 0);
    if (!arg || !arg[0] || errno == ERANGE || !end || *end ||
	value < ETYPES_ABS || value > ETYPES_SCA) {
	if (msg)
	    bu_vls_printf(msg, "combmem representation type must be from %d through %d",
		ETYPES_ABS, ETYPES_SCA);
	return -1;
    }
    return 0;
}

static int
combmem_number_valid(const char *arg)
{
    char *end = NULL;
    double value;

    errno = 0;
    value = strtod(arg, &end);
    return arg && arg[0] && errno != ERANGE && end && !*end && isfinite(value) &&
	!(sizeof(fastf_t) == sizeof(float) && (value > FLT_MAX || value < -FLT_MAX));
}

static void
combmem_operation_candidates(struct bu_cmd_validate_result *result, const char *prefix)
{
    size_t count = 0;

    for (size_t i = 0; combmem_operations[i]; i++)
	if (!prefix || !prefix[0] || bu_strncmp(combmem_operations[i], prefix, strlen(prefix)) == 0)
	    count++;
    if (!count)
	return;
    result->completion_candidates = (const char **)bu_calloc(count + 1,
	sizeof(char *), "combmem operation candidates");
    for (size_t i = 0, oi = 0; combmem_operations[i]; i++)
	if (!prefix || !prefix[0] || bu_strncmp(combmem_operations[i], prefix, strlen(prefix)) == 0)
	    result->completion_candidates[oi++] = bu_strdup(combmem_operations[i]);
    result->completion_count = count;
}

static int
combmem_validation_result(struct bu_cmd_validate_result *result,
	bu_cmd_validate_state_t state, size_t token, bu_cmd_value_t type,
	const char *hint, const char *provider)
{
    bu_cmd_validate_result_clear(result);
    result->state = state;
    result->token_start = token;
    result->token_end = token;
    result->expected = BU_CMD_EXPECT_OPERAND;
    result->completion_type = type;
    result->hint = hint;
    result->semantic_provider = provider;
    return 0;
}

static size_t
combmem_first_operand(size_t argc, const char **argv, int *replacement_type,
	int *have_replacement)
{
    size_t first = 0;

    if (replacement_type)
	*replacement_type = ETYPES_NULL;
    if (have_replacement)
	*have_replacement = 0;
    while (first < argc) {
	if (BU_STR_EQUAL(argv[first], "--")) {
	    first++;
	    break;
	}
	if ((!BU_STR_EQUAL(argv[first], "-i") && !BU_STR_EQUAL(argv[first], "-r")) ||
	    first + 1 >= argc)
	    break;
	if (BU_STR_EQUAL(argv[first], "-r")) {
	    if (replacement_type)
		*replacement_type = (int)strtol(argv[first + 1], NULL, 0);
	    if (have_replacement)
		*have_replacement = 1;
	}
	first += 2;
    }
    return first;
}

static int
combmem_schema_validate(const struct bu_cmd_schema *cmd, size_t argc,
	const char **argv, size_t cursor_arg, struct bu_cmd_validate_result *result)
{
    struct bu_cmd_schema flat = *cmd;
    size_t first;
    size_t members;
    size_t width = 15;
    int replacement_type;
    int have_replacement;
    int ret;

    flat.validation.custom_validate = NULL;
    ret = bu_cmd_schema_validate(&flat, argc, argv, cursor_arg, result);
    if (ret || result->state == BU_CMD_VALIDATE_INVALID)
	return ret;
    if (cursor_arg < argc && argv[cursor_arg] && argv[cursor_arg][0] == '-' &&
	argv[cursor_arg][1] && (result->expected & BU_CMD_EXPECT_OPTION))
	return 0;

    first = combmem_first_operand(argc, argv, &replacement_type, &have_replacement);
    if (first >= argc)
	return 0;
    if (replacement_type == ETYPES_ROT_AET || replacement_type == ETYPES_ROT_XYZ)
	width = 8;
    if (replacement_type == ETYPES_ROT_ARBITRARY_AXIS || replacement_type == ETYPES_SCA)
	width = 9;
    if (replacement_type == ETYPES_TRA)
	width = 5;
    members = argc - first - 1;

    for (size_t i = 0; i < argc; i++) {
	size_t ri;
	if (i == cursor_arg || i <= first)
	    continue;
	ri = (i - first - 1) % width;
	if (ri == 0 && !BU_STR_EQUAL(argv[i], "u") &&
	    !BU_STR_EQUAL(argv[i], "-") && !BU_STR_EQUAL(argv[i], "+"))
	    return combmem_validation_result(result, BU_CMD_VALIDATE_INVALID, i,
		BU_CMD_VALUE_KEYWORD, "invalid boolean operation", NULL);
	if (ri > 1 && !combmem_number_valid(argv[i]))
	    return combmem_validation_result(result, BU_CMD_VALIDATE_INVALID, i,
		BU_CMD_VALUE_NUMBER, "invalid transform value", NULL);
    }

    if (cursor_arg > first && cursor_arg < argc) {
	size_t ri = (cursor_arg - first - 1) % width;
	if (ri == 0) {
	    bu_cmd_validate_state_t state =
		(BU_STR_EQUAL(argv[cursor_arg], "u") || BU_STR_EQUAL(argv[cursor_arg], "-") ||
		 BU_STR_EQUAL(argv[cursor_arg], "+")) ? BU_CMD_VALIDATE_VALID : BU_CMD_VALIDATE_INCOMPLETE;
	    combmem_validation_result(result, state, cursor_arg, BU_CMD_VALUE_KEYWORD,
		state == BU_CMD_VALIDATE_VALID ? "boolean operation" : "boolean operation expected", NULL);
	    combmem_operation_candidates(result, argv[cursor_arg]);
	} else if (ri == 1) {
	    combmem_validation_result(result, BU_CMD_VALIDATE_VALID, cursor_arg,
		BU_CMD_VALUE_DB_OBJECT, "member object", "ged.db_object");
	} else {
	    bu_cmd_validate_state_t state = combmem_number_valid(argv[cursor_arg]) ?
		BU_CMD_VALIDATE_VALID : BU_CMD_VALIDATE_INVALID;
	    combmem_validation_result(result, state, cursor_arg, BU_CMD_VALUE_NUMBER,
		state == BU_CMD_VALIDATE_VALID ? "transform value" : "invalid transform value", NULL);
	}
	return 0;
    }

    if (cursor_arg >= argc && members && (members % width)) {
	size_t ri = members % width;
	bu_cmd_value_t type = ri == 1 ? BU_CMD_VALUE_DB_OBJECT :
	    (ri == 0 ? BU_CMD_VALUE_KEYWORD : BU_CMD_VALUE_NUMBER);
	const char *hint = ri == 1 ? "member object expected" :
	    (ri == 0 ? "boolean operation expected" : "transform value expected");
	combmem_validation_result(result, BU_CMD_VALIDATE_INCOMPLETE, argc, type, hint,
	    ri == 1 ? "ged.db_object" : NULL);
	if (ri == 0)
	    combmem_operation_candidates(result, "");
    }
    if (!have_replacement && members && members < width)
	result->state = BU_CMD_VALIDATE_INCOMPLETE;
    return 0;
}

static const struct bu_cmd_option combmem_schema_options[] = {
    BU_CMD_INTEGER_VALIDATE("i", NULL, struct combmem_args, input_type,
	combmem_type_validate, "type", "Input matrix representation type"),
    BU_CMD_INTEGER_VALIDATE("r", NULL, struct combmem_args, replacement_type,
	combmem_type_validate, "type", "Replacement matrix representation type"),
    BU_CMD_OPTION_NULL
};
static const struct bu_cmd_operand combmem_schema_operands[] = {
    BU_CMD_OPERAND("combination", BU_CMD_VALUE_DB_OBJECT, 1, 1,
	"Combination to inspect or replace", "ged.db_object"),
    BU_CMD_OPERAND("member_records", BU_CMD_VALUE_RAW, 0,
	BU_CMD_COUNT_UNLIMITED, "Operation, member, and transform records", NULL),
    BU_CMD_OPERAND_NULL
};
static const struct bu_cmd_schema combmem_cmd_schema = {
    "combmem", "Inspect or replace combination members", combmem_schema_options,
    combmem_schema_operands, BU_CMD_PARSE_OPTIONS_FIRST, {combmem_schema_validate}
};


/**
 * Given the azimuth, elevation and twist angles, calculate the
 * rotation part of a 4x4 matrix.
 */
static void
combmem_mat_aet(matp_t matp, fastf_t az, fastf_t el, fastf_t tw)
{
    fastf_t cos_az, sin_az;
    fastf_t cos_el, sin_el;
    fastf_t cos_tw, sin_tw;

    if (!matp)
	return;

    cos_az = cos(az);
    sin_az = sin(az);
    cos_el = cos(el);
    sin_el = sin(el);
    cos_tw = cos(tw);
    sin_tw = sin(tw);

    matp[0] = cos_az*cos_el;
    matp[1] = -cos_el*sin_az;
    matp[2] = -sin_el;
    matp[3] = 0.0;
    matp[4] = cos_tw*sin_az - cos_az*sin_el*sin_tw;
    matp[5] = cos_az*cos_tw + sin_az*sin_el*sin_tw;
    matp[6] = -cos_el*sin_tw;
    matp[7] = 0.0;
    matp[8] = cos_az*cos_tw*sin_el + sin_az*sin_tw;
    matp[9] = -cos_tw*sin_az*sin_el + cos_az*sin_tw;
    matp[10] = cos_el*cos_tw;
    matp[11] = 0.0;

    /* no perspective */
    matp[12] = matp[13] = matp[14] = 0.0;

    /* overall scale of 1 */
    matp[15] = 1.0;
}


/**
 * Disassemble the given rotation matrix into az, el, tw.
 */
static void
combmem_disassemble_rmat(matp_t matp, fastf_t *az, fastf_t *el, fastf_t *tw)
{
    fastf_t cos_az, sin_az;
    fastf_t cos_el, sin_el;
    fastf_t cos_tw, sin_tw;

    sin_el = -matp[2];
    cos_el = sqrt(1.0 - (sin_el*sin_el));

    if (NEAR_ZERO(cos_el, 0.000001)) {
	cos_az = matp[5];
	cos_tw = 1.0;
	sin_az = matp[4];
	sin_tw = 0.0;
    } else {
	cos_az = matp[0] / cos_el;
	cos_tw = matp[10] / cos_el;
	sin_az = -matp[1] / cos_el;
	sin_tw = -matp[6] / cos_el;
    }

    if (NEAR_EQUAL(cos_az, 1.0, 0.000001) ||
	NEAR_EQUAL(cos_az, -1.0, 0.000001)) {
	*az= 0.0;
    } else
	*az = acos(cos_az);

    if (NEAR_EQUAL(cos_el, 1.0, 0.000001) ||
	NEAR_EQUAL(cos_el, -1.0, 0.000001)) {
	*el= 0.0;
    } else
	*el = acos(cos_el);

    if (NEAR_EQUAL(cos_tw, 1.0, 0.000001) ||
	NEAR_EQUAL(cos_tw, -1.0, 0.000001)) {
	*tw= 0.0;
    } else
	*tw = acos(cos_tw);

    if (sin_az < 0.0)
	*az *= -1.0;
    if (sin_el < 0.0)
	*el *= -1.0;
    if (sin_tw < 0.0)
	*tw *= -1.0;
}


/**
 * Disassemble the given matrix into az, el, tw, tx, ty, tz, sa, sx, sy and sz.
 */
static int
combmem_disassemble_mat(matp_t matp, fastf_t *az, fastf_t *el, fastf_t *tw, fastf_t *tx, fastf_t *ty, fastf_t *tz, fastf_t *sa, fastf_t *sx, fastf_t *sy, fastf_t *sz)
{
    mat_t m;

    if (!matp)
	return 1;

    MAT_COPY(m, matp);

    *sa = 1.0 / m[MSA];

    *sx = sqrt(m[0]*m[0] + m[4]*m[4] + m[8]*m[8]);
    *sy = sqrt(m[1]*m[1] + m[5]*m[5] + m[9]*m[9]);
    *sz = sqrt(m[2]*m[2] + m[6]*m[6] + m[10]*m[10]);

    if (!NEAR_EQUAL(*sx, 1.0, VUNITIZE_TOL)) {
	m[0] /= *sx;
	m[4] /= *sx;
	m[8] /= *sx;
    }

    if (!NEAR_EQUAL(*sy, 1.0, VUNITIZE_TOL)) {
	m[1] /= *sy;
	m[5] /= *sy;
	m[9] /= *sy;
    }

    if (!NEAR_EQUAL(*sz, 1.0, VUNITIZE_TOL)) {
	m[2] /= *sz;
	m[6] /= *sz;
	m[10] /= *sz;
    }

    if (bn_mat_ck("combmem_disassemble_mat", m)) {
	*az = 0.0;
	*el = 0.0;
	*tw = 0.0;
	*tx = 0.0;
	*ty = 0.0;
	*tz = 0.0;
	*sa = 1.0;
	*sx = 1.0;
	*sy = 1.0;
	*sz = 1.0;

	/* The upper left 3x3 is not a rotation matrix */
	return 1;
    }

    *tx = m[MDX];
    *ty = m[MDY];
    *tz = m[MDZ];

    combmem_disassemble_rmat(m, az, el, tw);

    return 0; /* OK */
}


/**
 * Assemble the given aetvec, tvec and svec into a 4x4 matrix using key_pt for rotations and scale.
 */
static void
combmem_assemble_mat(matp_t matp, vect_t aetvec, vect_t tvec, hvect_t svec, point_t key_pt, int sflag)
{
    mat_t mat_aet_about_pt;
    mat_t mat_aet;
    mat_t mat_scale;
    mat_t xlate;
    mat_t tmp;

    if (!matp)
	return;

    if (ZERO(svec[X]) ||
	ZERO(svec[Y]) ||
	ZERO(svec[Z]) ||
	ZERO(svec[W])) {
	return;
    }

    combmem_mat_aet(mat_aet, aetvec[X], aetvec[Y], aetvec[Z]);
    bn_mat_xform_about_pnt(mat_aet_about_pt, mat_aet, key_pt);

    MAT_IDN(mat_scale);

    if (sflag) {
	if (!NEAR_EQUAL(svec[X], 1.0, VUNITIZE_TOL)) {
	    mat_scale[0] *= svec[X];
	    mat_scale[4] *= svec[X];
	    mat_scale[8] *= svec[X];
	}

	if (!NEAR_EQUAL(svec[Y], 1.0, VUNITIZE_TOL)) {
	    mat_scale[1] *= svec[Y];
	    mat_scale[5] *= svec[Y];
	    mat_scale[9] *= svec[Y];
	}

	if (!NEAR_EQUAL(svec[Z], 1.0, VUNITIZE_TOL)) {
	    mat_scale[2] *= svec[Z];
	    mat_scale[6] *= svec[Z];
	    mat_scale[10] *= svec[Z];
	}
    }

    if (!NEAR_EQUAL(svec[W], 1.0, VUNITIZE_TOL)) {
	mat_scale[MSA] *= 1.0 / svec[W];
    }

    MAT_IDN(xlate);
    MAT_DELTAS_VEC_NEG(xlate, key_pt);

    bn_mat_mul(tmp, mat_scale, xlate);
    MAT_DELTAS_VEC(xlate, key_pt);
    bn_mat_mul(mat_scale, xlate, tmp);

    MAT_IDN(xlate);
    MAT_DELTAS_VEC(xlate, tvec);

    bn_mat_mul(tmp, mat_scale, mat_aet_about_pt);
    bn_mat_mul(matp, xlate, tmp);
}


static void
combmem_vls_print_member_info(struct ged *gedp, char op, union tree *itp, enum etypes etype)
{
    fastf_t az, el, tw;
    fastf_t tx, ty, tz;
    fastf_t sa, sx, sy, sz;

    switch (etype) {
	case ETYPES_ABS:
	    if (!itp->tr_l.tl_mat) {
		bu_vls_printf(gedp->ged_result_str, "%c {%s} 0.0 0.0 0.0 0.0 0.0 0.0 1.0 1.0 1.0 1.0 0.0 0.0 0.0",
			      op, itp->tr_l.tl_name);
	    } else {
		if (combmem_disassemble_mat(itp->tr_l.tl_mat, &az, &el, &tw, &tx, &ty, &tz, &sa, &sx, &sy, &sz))
		    bu_log("Found bad matrix for %s\n", itp->tr_l.tl_name);

		bu_vls_printf(gedp->ged_result_str, "%c {%s} %lf %lf %lf %lf %lf %lf %lf %lf %lf %lf 0.0 0.0 0.0",
			      op, itp->tr_l.tl_name,
			      az * RAD2DEG,
			      el * RAD2DEG,
			      tw * RAD2DEG,
			      tx, ty, tz,
			      sa, sx, sy, sz);
	    }

	    break;
	case ETYPES_REL:
	    bu_vls_printf(gedp->ged_result_str, "%c {%s} 0.0 0.0 0.0 0.0 0.0 0.0 1.0 1.0 1.0 1.0 0.0 0.0 0.0",
			  op, itp->tr_l.tl_name);
	    break;
	case ETYPES_ROT_AET:
	    bu_vls_printf(gedp->ged_result_str, "%c {%s} 0.0 0.0 0.0 0.0 0.0 0.0",
			  op, itp->tr_l.tl_name);
	    break;
	case ETYPES_ROT_XYZ:
	    bu_vls_printf(gedp->ged_result_str, "%c {%s} 0.0 0.0 0.0 0.0 0.0 0.0",
			  op, itp->tr_l.tl_name);
	    break;
	case ETYPES_ROT_ARBITRARY_AXIS:
	    bu_vls_printf(gedp->ged_result_str, "%c {%s} 0.0 0.0 0.0 0.0 0.0 0.0 0.0",
			  op, itp->tr_l.tl_name);
	    break;
	case ETYPES_TRA:
	    bu_vls_printf(gedp->ged_result_str, "%c {%s} 0.0 0.0 0.0",
			  op, itp->tr_l.tl_name);
	    break;
	case ETYPES_SCA:
	    bu_vls_printf(gedp->ged_result_str, "%c {%s} 1.0 1.0 1.0 1.0 0.0 0.0 0.0",
			  op, itp->tr_l.tl_name);
	default:
	    break;
    }

}


#define COMBMEM_GETCOMBTREE(_gedp, _cmd, _name, _dp, _intern, _ntp, _rt_tree_array, _node_count) { \
	struct rt_comb_internal *_comb; \
	\
	if ((_dp = db_lookup((_gedp)->dbip, (_name), LOOKUP_NOISY)) == RT_DIR_NULL) { \
	    bu_vls_printf(gedp->ged_result_str, "%s: Warning - %s not found in database.\n", (_cmd), (_name)); \
	    return BRLCAD_ERROR; \
	} \
	\
	if (!(_dp->d_flags & RT_DIR_COMB)) { \
	    bu_vls_printf((_gedp)->ged_result_str, "%s: Warning - %s not a combination\n", (_cmd), (_name)); \
	    return BRLCAD_ERROR; \
	} \
	\
	if (rt_db_get_internal(&(_intern), _dp, (_gedp)->dbip, (matp_t)NULL) < 0) { \
	    bu_vls_printf((_gedp)->ged_result_str, "Database read error, aborting"); \
	    return BRLCAD_ERROR; \
	} \
	\
	_comb = (struct rt_comb_internal *)(_intern).idb_ptr;	\
	RT_CK_COMB(_comb); \
	if (_comb->tree) {							\
	    (_ntp) = db_dup_subtree(_comb->tree);	\
	    RT_CK_TREE(_ntp);						\
	    \
	    /* Convert to "v4 / GIFT style", so that the flatten makes sense. */ \
	    if (db_ck_v4gift_tree(_ntp) < 0)					\
		db_non_union_push((_ntp));			\
	    RT_CK_TREE(_ntp);							\
	    \
	    (_node_count) = db_tree_nleaves(_ntp);				\
	    (_rt_tree_array) = (struct rt_tree_array *)bu_calloc((_node_count), sizeof(struct rt_tree_array), "rt_tree_array"); \
	    \
	    /*							 \
	     * free=0 means that the tree won't have any leaf nodes freed. \
	     */								\
	    (void)db_flatten_tree((_rt_tree_array), (_ntp), OP_UNION, 0); \
	} else { \
	    (_ntp) = TREE_NULL; \
	    (_node_count) = 0; \
	    (_rt_tree_array) = (struct rt_tree_array *)0; \
	} \
    }


static int
combmem_get(struct ged *gedp, int argc, const char *argv[], enum etypes etype)
{
    struct directory *dp;
    struct rt_db_internal intern;
    union tree *ntp;
    struct rt_tree_array *rt_tree_array;
    size_t i;
    size_t node_count;

    if (argc < 2) {
	bu_vls_printf(gedp->ged_result_str, "ERROR argument missing after [%s]\n", argv[0]);
	return BRLCAD_ERROR;
    }

    COMBMEM_GETCOMBTREE(gedp, argv[0], argv[1], dp, intern, ntp, rt_tree_array, node_count);

    for (i = 0; i < node_count; i++) {
	union tree *itp = rt_tree_array[i].tl_tree;
	char op = '\0';

	RT_CK_TREE(itp);
	BU_ASSERT(itp->tr_op == OP_DB_LEAF);
	BU_ASSERT(itp->tr_l.tl_name != NULL);

	switch (rt_tree_array[i].tl_op) {
	    case OP_INTERSECT:
		op = DB_OP_INTERSECT;
		break;
	    case OP_SUBTRACT:
		op = DB_OP_SUBTRACT;
		break;
	    case OP_UNION:
		op = DB_OP_UNION;
		break;
	    default:
		bu_bomb("combmem_get() corrupt rt_tree_array");
	}

	combmem_vls_print_member_info(gedp, op, itp, etype);
	bu_vls_printf(gedp->ged_result_str, "\n");
    }

    rt_db_free_internal(&intern);
    if (rt_tree_array) bu_free((void *)rt_tree_array, "rt_tree_array");
    db_free_tree(ntp);
    return BRLCAD_OK;
}


#define COMBMEM_SET_PART_I(_gedp, _argc, _cmd, _name, _num_params, _intern, _dp, _comb, _node_count, _rt_tree_array) { \
	if (rt_db_get_internal(&(_intern), (_dp), (_gedp)->dbip, (matp_t)NULL) < 0) { \
	    bu_vls_printf((_gedp)->ged_result_str, "Database read error, aborting"); \
	    return BRLCAD_ERROR;							\
	}									\
	\
	(_comb) = (struct rt_comb_internal *)(_intern).idb_ptr;		\
	RT_CK_COMB((_comb));							\
	(_node_count) = ((_argc) - 2) / (_num_params); /* integer division */ \
	if (_node_count) {							\
	    (_rt_tree_array) = (struct rt_tree_array *)bu_calloc((_node_count), sizeof(struct rt_tree_array), "tree list"); \
	} else { \
	    (_rt_tree_array) = (struct rt_tree_array *)0; \
	}									\
    }


#define COMBMEM_SET_PART_II(_gedp, _opstr, _rt_tree_array_index, _mat) { \
	db_op_t combmem_set_part_ii_op = db_str2op((_opstr)); \
	\
	/* Add it to the combination */ \
	switch (combmem_set_part_ii_op) { \
	    case DB_OP_INTERSECT: \
		(_rt_tree_array_index).tl_op = OP_INTERSECT; \
		break; \
	    case DB_OP_SUBTRACT: \
		(_rt_tree_array_index).tl_op = OP_SUBTRACT; \
		break; \
	    default: \
		bu_vls_printf((_gedp)->ged_result_str, "combmem_set: unrecognized relation %c (assuming UNION)\n", (_opstr)[0]); \
	    /* fall through */ \
	    case DB_OP_UNION: \
		(_rt_tree_array_index).tl_op = OP_UNION; \
		break; \
	} \
	\
	MAT_IDN((_mat));				\
    }


#define COMBMEM_SET_PART_III(_tp, _tree, _rt_tree_array_index, _name) \
    (_rt_tree_array_index).tl_tree = (_tp); \
		    (_tp)->tr_l.tl_op = OP_DB_LEAF; \
		    (_tp)->tr_l.tl_name = bu_strdup(_name); \
			 (_tp)->tr_l.tl_mat = (matp_t)bu_calloc(1, sizeof(mat_t), "combmem_set: mat");


#define COMBMEM_SET_PART_IV(_gedp, _comb, _tree_index, _intern, _final_tree, _node_count, _dp, _rt_tree_array, _old_intern, _old_rt_tree_array, _old_ntp) \
    db_free_tree((_comb)->tree); \
    (_comb)->tree = NULL; \
    \
    if ((_tree_index)) \
	(_final_tree) = (union tree *)db_mkgift_tree((_rt_tree_array), (_node_count)); \
    else \
	(_final_tree) = TREE_NULL; \
    \
    RT_DB_INTERNAL_INIT(&(_intern)); \
    (_intern).idb_major_type = DB5_MAJORTYPE_BRLCAD; \
    (_intern).idb_type = ID_COMBINATION; \
	     (_intern).idb_meth = &OBJ[ID_COMBINATION]; \
		      (_intern).idb_ptr = (void *)(_comb); \
		      (_comb)->tree = (_final_tree); \
		      \
		      if (rt_db_put_internal((_dp), (_gedp)->dbip, &(_intern)) < 0) { \
			  bu_vls_printf((_gedp)->ged_result_str, "Unable to write new combination into database.\n"); \
			  \
			  rt_db_free_internal(&(_old_intern)); \
			  if (_old_rt_tree_array) \
			      bu_free((void *)(_old_rt_tree_array), "rt_tree_array"); \
			  db_free_tree((_old_ntp)); \
			  \
			  return BRLCAD_ERROR; \
		      } \
		      \
		      rt_db_free_internal(&(_old_intern)); \
		      if (_old_rt_tree_array) \
			  bu_free((void *)(_old_rt_tree_array), "rt_tree_array"); \
		      db_free_tree((_old_ntp));


#define COMBMEM_CHECK_MAT(_tp, _tree_index, _old_rt_tree_array, _mat, _aetvec, _tvec, _svec, _key_pt, _az, _el, _tw, _tx, _ty, _tz, _sa, _sx, _sy, _sz) \
    bn_mat_mul((_tp)->tr_l.tl_mat, (_mat), (_old_rt_tree_array)[(_tree_index)].tl_tree->tr_l.tl_mat); \
    \
/* If bn_mat_ck fails, it's because scaleX, scaleY and/or scaleZ has been \
 * been applied along with rotations. This screws up perpendicularity. \
 */ \
    if (bn_mat_ck("combmem_set", (_tp)->tr_l.tl_mat)) { \
	combmem_assemble_mat((_mat), (_aetvec), (_tvec), (_svec), (_key_pt), 0); \
	bn_mat_mul((_tp)->tr_l.tl_mat, (_mat), (_old_rt_tree_array)[(_tree_index)].tl_tree->tr_l.tl_mat); \
	\
	if (bn_mat_ck("combmem_set", (_tp)->tr_l.tl_mat)) { \
	    MAT_COPY((_tp)->tr_l.tl_mat, (_old_rt_tree_array)[(_tree_index)].tl_tree->tr_l.tl_mat); \
	} \
    } else { \
	/* This will check if the 3x3 rotation part is bad after factoring out scaleX, scaleY and scaleZ. */ \
	if (combmem_disassemble_mat((_tp)->tr_l.tl_mat, &(_az), &(_el), &(_tw), &(_tx), &(_ty), &(_tz), &(_sa), &(_sx), &(_sy), &(_sz))) { \
	    MAT_COPY((_tp)->tr_l.tl_mat, (_old_rt_tree_array)[(_tree_index)].tl_tree->tr_l.tl_mat); \
	} \
    }


static int
combmem_set(struct ged *gedp, int argc, const char *argv[], enum etypes etype)
{
    struct rt_db_internal old_intern;
    union tree *old_ntp;
    struct rt_tree_array *old_rt_tree_array;
    size_t old_node_count;

    struct directory *dp;
    struct rt_db_internal intern;
    struct rt_comb_internal *comb;
    struct rt_tree_array *rt_tree_array;
    size_t i;
    size_t node_count;
    size_t tree_index;
    union tree *tp;
    union tree *final_tree;

    switch (etype) {
	case ETYPES_ABS:
	case ETYPES_REL:
	    break;
	default:
	    return BRLCAD_ERROR;
    }

    COMBMEM_GETCOMBTREE(gedp, argv[0], argv[1], dp, old_intern, old_ntp, old_rt_tree_array, old_node_count);
    COMBMEM_SET_PART_I(gedp, argc, argv[0], argv[1], 15, intern, dp, comb, node_count, rt_tree_array);

    tree_index = 0;
    for (i = 2; i < (size_t)argc; i += 15) {
	mat_t mat;
	double az, el, tw;
	double tx, ty, tz;
	double sa, sx, sy, sz;
	double kx, ky, kz;
	hvect_t svec = HINIT_ZERO;
	point_t key_pt = VINIT_ZERO;
	vect_t aetvec = VINIT_ZERO;
	vect_t tvec = VINIT_ZERO;

	COMBMEM_SET_PART_II(gedp, argv[i], rt_tree_array[tree_index], mat);

	if (sscanf(argv[i+2], "%lf", &az) == 1 &&
	    sscanf(argv[i+3], "%lf", &el) == 1 &&
	    sscanf(argv[i+4], "%lf", &tw) == 1 &&
	    sscanf(argv[i+5], "%lf", &tx) == 1 &&
	    sscanf(argv[i+6], "%lf", &ty) == 1 &&
	    sscanf(argv[i+7], "%lf", &tz) == 1 &&
	    sscanf(argv[i+8], "%lf", &sa) == 1 &&
	    sscanf(argv[i+9], "%lf", &sx) == 1 &&
	    sscanf(argv[i+10], "%lf", &sy) == 1 &&
	    sscanf(argv[i+11], "%lf", &sz) == 1 &&
	    sscanf(argv[i+12], "%lf", &kx) == 1 &&
	    sscanf(argv[i+13], "%lf", &ky) == 1 &&
	    sscanf(argv[i+14], "%lf", &kz) == 1) {

	    VSET(aetvec, az, el, tw);
	    VSCALE(aetvec, aetvec, DEG2RAD);
	    VSET(tvec, tx, ty, tz);
	    VSCALE(tvec, tvec, gedp->dbip->dbi_local2base);
	    VSET(key_pt, kx, ky, kz);
	    VSCALE(key_pt, key_pt, gedp->dbip->dbi_local2base);
	    QSET(svec, sx, sy, sz, sa);

	    combmem_assemble_mat(mat, aetvec, tvec, svec, key_pt, 1);

	    /* If bn_mat_ck fails, it's because scaleX, scaleY and/or scaleZ has been
	     * been applied along with rotations. This screws up perpendicularity.
	     */
	    if (bn_mat_ck("combmem_set", mat)) {
		combmem_assemble_mat(mat, aetvec, tvec, svec, key_pt, 0);
	    }
	}

	BU_ALLOC(tp, union tree);
	RT_TREE_INIT(tp);
	COMBMEM_SET_PART_III(tp, tree, rt_tree_array[tree_index], argv[i+1]);

	if (etype == ETYPES_REL
	    && tree_index < old_node_count
	    && old_rt_tree_array[tree_index].tl_tree->tr_l.tl_mat
	    && BU_STR_EQUAL(old_rt_tree_array[tree_index].tl_tree->tr_l.tl_name, tp->tr_l.tl_name))
	{
	    fastf_t azf = az, elf = el, twf = tw;
	    fastf_t txf = tx, tyf = ty, tzf = tz;
	    fastf_t saf = sa, sxf = sx, syf = sy, szf = sz;

	    COMBMEM_CHECK_MAT(tp, tree_index, old_rt_tree_array, mat, aetvec, tvec, svec, key_pt, azf, elf, twf, txf, tyf, tzf, saf, sxf, syf, szf);
	} else {
	    MAT_COPY(tp->tr_l.tl_mat, mat);
	}

	tree_index++;
    }

    COMBMEM_SET_PART_IV(gedp, comb, tree_index, intern, final_tree, node_count, dp, rt_tree_array, old_intern, old_rt_tree_array, old_ntp);

    return BRLCAD_OK;
}


static int
combmem_set_rot(struct ged *gedp, int argc, const char *argv[], enum etypes etype)
{
    struct rt_db_internal old_intern;
    union tree *old_ntp;
    struct rt_tree_array *old_rt_tree_array;
    size_t old_node_count;

    struct directory *dp;
    struct rt_db_internal intern;
    struct rt_comb_internal *comb;
    struct rt_tree_array *rt_tree_array;
    size_t i;
    size_t node_count;
    size_t tree_index;
    union tree *tp;
    union tree *final_tree;

    switch (etype) {
	case ETYPES_ROT_AET:
	case ETYPES_ROT_XYZ:
	    break;
	default:
	    return BRLCAD_ERROR;
    }

    COMBMEM_GETCOMBTREE(gedp, argv[0], argv[1], dp, old_intern, old_ntp, old_rt_tree_array, old_node_count);
    COMBMEM_SET_PART_I(gedp, argc, argv[0], argv[1], 8, intern, dp, comb, node_count, rt_tree_array);

    tree_index = 0;
    for (i = 2; i < (size_t)argc; i += 8) {
	mat_t mat = MAT_INIT_ZERO;
	double az = 0.0;
	double el = 0.0;
	double tw = 0.0;
	double kx = 0.0;
	double ky = 0.0;
	double kz = 0.0;
	point_t key_pt = VINIT_ZERO;

	COMBMEM_SET_PART_II(gedp, argv[i], rt_tree_array[tree_index], mat);

	if (sscanf(argv[i+2], "%lf", &az) == 1 &&
	    sscanf(argv[i+3], "%lf", &el) == 1 &&
	    sscanf(argv[i+4], "%lf", &tw) == 1 &&
	    sscanf(argv[i+5], "%lf", &kx) == 1 &&
	    sscanf(argv[i+6], "%lf", &ky) == 1 &&
	    sscanf(argv[i+7], "%lf", &kz) == 1) {

	    mat_t mat_rot;

	    VSET(key_pt, kx, ky, kz);
	    VSCALE(key_pt, key_pt, gedp->dbip->dbi_local2base);

	    if (etype == ETYPES_ROT_AET) {
		az *= DEG2RAD;
		el *= DEG2RAD;
		tw *= DEG2RAD;
		combmem_mat_aet(mat_rot, az, el, tw);
	    } else
		bn_mat_angles(mat_rot, az, el, tw);

	    bn_mat_xform_about_pnt(mat, mat_rot, key_pt);
	}

	BU_ALLOC(tp, union tree);
	RT_TREE_INIT(tp);
	COMBMEM_SET_PART_III(tp, tree, rt_tree_array[tree_index], argv[i+1]);

	if (tree_index < old_node_count
	    && old_rt_tree_array[tree_index].tl_tree->tr_l.tl_mat
	    && BU_STR_EQUAL(old_rt_tree_array[tree_index].tl_tree->tr_l.tl_name, tp->tr_l.tl_name))
	{
	    fastf_t azf = az, elf = el, twf = tw;
	    fastf_t tx, ty, tz;
	    fastf_t sa, sx, sy, sz;
	    hvect_t svec = HINIT_ZERO;
	    vect_t aetvec = VINIT_ZERO;
	    vect_t tvec = VINIT_ZERO;

	    COMBMEM_CHECK_MAT(tp, tree_index, old_rt_tree_array, mat, aetvec, tvec, svec, key_pt, azf, elf, twf, tx, ty, tz, sa, sx, sy, sz);
	} else {
	    MAT_COPY(tp->tr_l.tl_mat, mat);
	}

	tree_index++;
    }

    COMBMEM_SET_PART_IV(gedp, comb, tree_index, intern, final_tree, node_count, dp, rt_tree_array, old_intern, old_rt_tree_array, old_ntp);

    return BRLCAD_OK;
}


static int
combmem_set_arb_rot(struct ged *gedp, int argc, const char *argv[], enum etypes etype)
{
    struct rt_db_internal old_intern;
    union tree *old_ntp;
    struct rt_tree_array *old_rt_tree_array;
    size_t old_node_count;

    struct directory *dp;
    struct rt_db_internal intern;
    struct rt_comb_internal *comb;
    struct rt_tree_array *rt_tree_array;
    size_t i;
    size_t node_count;
    size_t tree_index;
    union tree *tp;
    union tree *final_tree;

    if (etype != ETYPES_ROT_ARBITRARY_AXIS)
	return BRLCAD_ERROR;

    COMBMEM_GETCOMBTREE(gedp, argv[0], argv[1], dp, old_intern, old_ntp, old_rt_tree_array, old_node_count);
    COMBMEM_SET_PART_I(gedp, argc, argv[0], argv[1], 9, intern, dp, comb, node_count, rt_tree_array);

    tree_index = 0;
    for (i = 2; i < (size_t)argc; i += 9) {
	mat_t mat;
	double px, py, pz;
	double dx, dy, dz;
	double ang;
	vect_t dir;
	point_t pt;

	COMBMEM_SET_PART_II(gedp, argv[i], rt_tree_array[tree_index], mat);

	if (sscanf(argv[i+2], "%lf", &px) == 1 &&
	    sscanf(argv[i+3], "%lf", &py) == 1 &&
	    sscanf(argv[i+4], "%lf", &pz) == 1 &&
	    sscanf(argv[i+5], "%lf", &dx) == 1 &&
	    sscanf(argv[i+6], "%lf", &dy) == 1 &&
	    sscanf(argv[i+7], "%lf", &dz) == 1 &&
	    sscanf(argv[i+8], "%lf", &ang) == 1) {

	    VSET(pt, px, py, pz);
	    VSCALE(pt, pt, gedp->dbip->dbi_local2base);
	    VSET(dir, dx, dy, dz);
	    VUNITIZE(dir);
	    ang *= DEG2RAD;
	    bn_mat_arb_rot(mat, pt, dir, ang);
	}

	BU_ALLOC(tp, union tree);
	RT_TREE_INIT(tp);
	COMBMEM_SET_PART_III(tp, tree, rt_tree_array[tree_index], argv[i+1]);

	if (tree_index < old_node_count && old_rt_tree_array[tree_index].tl_tree->tr_l.tl_mat &&
	    BU_STR_EQUAL(old_rt_tree_array[tree_index].tl_tree->tr_l.tl_name, tp->tr_l.tl_name)) {
	    fastf_t az, el, tw;
	    fastf_t tx, ty, tz;
	    fastf_t sa, sx, sy, sz;
	    hvect_t svec = HINIT_ZERO;
	    point_t key_pt = VINIT_ZERO;
	    vect_t aetvec = VINIT_ZERO;
	    vect_t tvec = VINIT_ZERO;

	    COMBMEM_CHECK_MAT(tp, tree_index, old_rt_tree_array, mat, aetvec, tvec, svec, key_pt, az, el, tw, tx, ty, tz, sa, sx, sy, sz);
	} else {
	    MAT_COPY(tp->tr_l.tl_mat, mat);
	}

	tree_index++;
    }

    COMBMEM_SET_PART_IV(gedp, comb, tree_index, intern, final_tree, node_count, dp, rt_tree_array, old_intern, old_rt_tree_array, old_ntp);

    return BRLCAD_OK;
}


static int
combmem_set_tra(struct ged *gedp, int argc, const char *argv[], enum etypes etype)
{
    struct rt_db_internal old_intern;
    union tree *old_ntp;
    struct rt_tree_array *old_rt_tree_array;
    size_t old_node_count;

    struct directory *dp;
    struct rt_db_internal intern;
    struct rt_comb_internal *comb;
    struct rt_tree_array *rt_tree_array;
    size_t i;
    size_t node_count;
    size_t tree_index;
    union tree *tp;
    union tree *final_tree;

    if (etype != ETYPES_TRA)
	return BRLCAD_ERROR;

    COMBMEM_GETCOMBTREE(gedp, argv[0], argv[1], dp, old_intern, old_ntp, old_rt_tree_array, old_node_count);
    COMBMEM_SET_PART_I(gedp, argc, argv[0], argv[1], 5, intern, dp, comb, node_count, rt_tree_array);

    tree_index = 0;
    for (i = 2; i < (size_t)argc; i += 5) {
	mat_t mat;
	double tx, ty, tz;
	vect_t tvec = VINIT_ZERO;

	COMBMEM_SET_PART_II(gedp, argv[i], rt_tree_array[tree_index], mat);

	if (sscanf(argv[i+2], "%lf", &tx) == 1 &&
	    sscanf(argv[i+3], "%lf", &ty) == 1 &&
	    sscanf(argv[i+4], "%lf", &tz) == 1) {

	    VSET(tvec, tx, ty, tz);
	    VSCALE(tvec, tvec, gedp->dbip->dbi_local2base);
	    MAT_DELTAS_VEC(mat, tvec);
	}

	BU_ALLOC(tp, union tree);
	RT_TREE_INIT(tp);
	COMBMEM_SET_PART_III(tp, tree, rt_tree_array[tree_index], argv[i+1]);

	if (tree_index < old_node_count && old_rt_tree_array[tree_index].tl_tree->tr_l.tl_mat &&
	    BU_STR_EQUAL(old_rt_tree_array[tree_index].tl_tree->tr_l.tl_name, tp->tr_l.tl_name)) {

	    bn_mat_mul(tp->tr_l.tl_mat, mat, old_rt_tree_array[tree_index].tl_tree->tr_l.tl_mat);
	} else {
	    MAT_COPY(tp->tr_l.tl_mat, mat);
	}

	tree_index++;
    }

    COMBMEM_SET_PART_IV(gedp, comb, tree_index, intern, final_tree, node_count, dp, rt_tree_array, old_intern, old_rt_tree_array, old_ntp);

    return BRLCAD_OK;
}


static int
combmem_set_sca(struct ged *gedp, int argc, const char *argv[], enum etypes etype)
{
    struct rt_db_internal old_intern;
    union tree *old_ntp;
    struct rt_tree_array *old_rt_tree_array;
    size_t old_node_count;

    struct directory *dp;
    struct rt_db_internal intern;
    struct rt_comb_internal *comb;
    struct rt_tree_array *rt_tree_array;
    size_t i;
    size_t node_count;
    size_t tree_index;
    union tree *tp;
    union tree *final_tree;

    if (etype != ETYPES_SCA)
	return BRLCAD_ERROR;

    COMBMEM_GETCOMBTREE(gedp, argv[0], argv[1], dp, old_intern, old_ntp, old_rt_tree_array, old_node_count);
    COMBMEM_SET_PART_I(gedp, argc, argv[0], argv[1], 9, intern, dp, comb, node_count, rt_tree_array);

    tree_index = 0;
    for (i = 2; i < (size_t)argc; i += 9) {
	mat_t mat = MAT_INIT_ZERO;
	double sa = 0.0;
	double sx = 0.0;
	double sy = 0.0;
	double sz = 0.0;
	double kx = 0.0;
	double ky = 0.0;
	double kz = 0.0;
	hvect_t svec = HINIT_ZERO;
	point_t key_pt = VINIT_ZERO;
	vect_t aetvec = VINIT_ZERO;
	vect_t tvec = VINIT_ZERO;

	COMBMEM_SET_PART_II(gedp, argv[i], rt_tree_array[tree_index], mat);

	if (sscanf(argv[i+2], "%lf", &sa) == 1 &&
	    sscanf(argv[i+3], "%lf", &sx) == 1 &&
	    sscanf(argv[i+4], "%lf", &sy) == 1 &&
	    sscanf(argv[i+5], "%lf", &sz) == 1 &&
	    sscanf(argv[i+6], "%lf", &kx) == 1 &&
	    sscanf(argv[i+7], "%lf", &ky) == 1 &&
	    sscanf(argv[i+8], "%lf", &kz) == 1) {

	    VSETALL(aetvec, 0.0);
	    VSETALL(tvec, 0.0);
	    VSET(key_pt, kx, ky, kz);
	    VSCALE(key_pt, key_pt, gedp->dbip->dbi_local2base);
	    QSET(svec, sx, sy, sz, sa);

	    combmem_assemble_mat(mat, aetvec, tvec, svec, key_pt, 1);
	}

	BU_ALLOC(tp, union tree);
	RT_TREE_INIT(tp);
	COMBMEM_SET_PART_III(tp, tree, rt_tree_array[tree_index], argv[i+1]);

	if (tree_index < old_node_count
	    && old_rt_tree_array[tree_index].tl_tree->tr_l.tl_mat
	    && BU_STR_EQUAL(old_rt_tree_array[tree_index].tl_tree->tr_l.tl_name, tp->tr_l.tl_name))
	{
	    fastf_t az, el, tw;
	    fastf_t tx, ty, tz;
	    fastf_t saf = sa, sxf = sx, syf = sy, szf = sz;

	    COMBMEM_CHECK_MAT(tp, tree_index, old_rt_tree_array, mat, aetvec, tvec, svec, key_pt, az, el, tw, tx, ty, tz, saf, sxf, syf, szf);
	} else {
	    MAT_COPY(tp->tr_l.tl_mat, mat);
	}

	tree_index++;
    }

    COMBMEM_SET_PART_IV(gedp, comb, tree_index, intern, final_tree, node_count, dp, rt_tree_array, old_intern, old_rt_tree_array, old_ntp);

    return BRLCAD_OK;
}


static int
combmem_set_empty(struct ged *gedp, int argc, const char *argv[])
{
    struct directory *dp;
    struct rt_comb_internal *comb;
    struct rt_db_internal intern;

    if (argc < 2) {
	bu_vls_printf(gedp->ged_result_str, "ERROR argument missing after [%s]\n", argv[0]);
	return BRLCAD_ERROR;
    }

    if ((dp = db_lookup(gedp->dbip, argv[1], LOOKUP_NOISY)) == RT_DIR_NULL) {
	bu_vls_printf(gedp->ged_result_str, "%s: Warning - %s not found in database.\n", argv[0], argv[1]);
	return BRLCAD_ERROR;
    }

    if (!(dp->d_flags & RT_DIR_COMB)) {
	bu_vls_printf(gedp->ged_result_str, "%s: Warning - %s not a combination\n", argv[0], argv[1]);
	return BRLCAD_ERROR;
    }									\

    if (rt_db_get_internal(&intern, dp, gedp->dbip, (matp_t)NULL) < 0) {
	bu_vls_printf(gedp->ged_result_str, "Database read error, aborting");
	return BRLCAD_ERROR;
    }
    comb = (struct rt_comb_internal *)intern.idb_ptr;
    RT_CK_COMB(comb);

    db_free_tree(comb->tree);
    comb->tree = NULL;

    if (rt_db_put_internal(dp, gedp->dbip, &intern) < 0) {
	bu_vls_printf(gedp->ged_result_str, "Unable to write combination into database - %s\n", argv[1]);
	return BRLCAD_ERROR;
    }

    return BRLCAD_OK;
}


/*
 * Set/get a combinations members.
 */
int
ged_combmem_core(struct ged *gedp, int argc, const char *argv[])
{
    struct combmem_args args = {ETYPES_ABS, ETYPES_NULL};
    enum etypes iflag;
    enum etypes rflag;
    const char *cmd_name = argv[0];
    static const char *usage = "[-i type] [-r type] comb [op1 name1 az1 el1 tw1 x1 y1 z1 sa1 sx1 sy1 sz1 ...]";
    int operand_index;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", cmd_name, usage);
	return GED_HELP;
    }

    operand_index = bu_cmd_schema_parse_complete(&combmem_cmd_schema, &args,
	gedp->ged_result_str, argc - 1, argv + 1);
    if (operand_index < 0)
	goto bad;

    /* The longstanding helper routines expect argv[1] to be the target
     * combination.  Moving to argv + operand_index preserves that contract:
     * with options, argv[0] is the final option argument; without them it is
     * the command name. */
    argc -= operand_index;
    argv += operand_index;
    iflag = (enum etypes)args.input_type;
    rflag = (enum etypes)args.replacement_type;

    if (argc == 2) {
	if (rflag == ETYPES_NULL)
	    return combmem_get(gedp, argc, argv, iflag);

	/* Remove all members */
	return combmem_set_empty(gedp, argc, argv);
    }

    /* Check number of args */
    switch (rflag) {
	case ETYPES_ABS:
	case ETYPES_REL:
	    if (argc > 16 && !((argc-2)%15)) {
		return combmem_set(gedp, argc, argv, rflag);
	    }
	    break;
	case ETYPES_ROT_AET:
	case ETYPES_ROT_XYZ:
	    if (argc > 9 && !((argc-2)%8)) {
		return combmem_set_rot(gedp, argc, argv, rflag);
	    }
	    break;
	case ETYPES_ROT_ARBITRARY_AXIS:
	    if (argc > 10 && !((argc-2)%9)) {
		return combmem_set_arb_rot(gedp, argc, argv, rflag);
	    }
	    break;
	case ETYPES_TRA:
	    if (argc > 6 && !((argc-2)%5)) {
		return combmem_set_tra(gedp, argc, argv, rflag);
	    }
	    break;
	case ETYPES_SCA:
	    if (argc > 10 && !((argc-2)%9)) {
		return combmem_set_sca(gedp, argc, argv, rflag);
	    }
	    break;
	case ETYPES_NULL:
	default:
	    if (argc > 16 && !((argc-2)%15)) {
		return combmem_set(gedp, argc, argv, ETYPES_ABS);
	    }
	    break;
    }

bad:
    bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", cmd_name, usage);
    return BRLCAD_ERROR;
}

#include "../include/plugin.h"

#define GED_COMBMEM_COMMANDS(X, XID) \
    X(combmem, ged_combmem_core, GED_CMD_DEFAULT, &combmem_cmd_schema) \

GED_DECLARE_COMMAND_SET_WITH_NATIVE_SCHEMA(GED_COMBMEM_COMMANDS)
GED_DECLARE_PLUGIN_MANIFEST_WITH_NATIVE_SCHEMA("libged_combmem", 1, GED_COMBMEM_COMMANDS)

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
