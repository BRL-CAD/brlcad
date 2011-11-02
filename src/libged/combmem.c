/*                         C O M B M E M . C
 * BRL-CAD
 *
 * Copyright (c) 2008-2011 United States Government as represented by
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

#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include "bio.h"

#include "./ged_private.h"

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


/**
 * C O M B M E M _ M A T _ A E T
 *
 * Given the azimuth, elevation and twist angles, calculate the
 * rotation part of a 4x4 matrix.
 */
HIDDEN void
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
 * C O M B M E M _ D I S A S S E M B L E _ R M A T
 *
 * Disassemble the given rotation matrix into az, el, tw.
 */
HIDDEN void
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
 * C O M B M E M _ D I S A S S E M B L E _ M A T
 *
 * Disassemble the given matrix into az, el, tw, tx, ty, tz, sa, sx, sy and sz.
 */
HIDDEN int
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
 * C O M B M E M _ A S S E M B L E _ M A T
 *
 * Assemble the given aetvec, tvec and svec into a 4x4 matrix using key_pt for rotations and scale.
 */
HIDDEN void
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
    bn_mat_xform_about_pt(mat_aet_about_pt, mat_aet, key_pt);

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


HIDDEN void
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
			      az * bn_radtodeg,
			      el * bn_radtodeg,
			      tw * bn_radtodeg,
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
	if ((_dp = db_lookup((_gedp)->ged_wdbp->dbip, (_name), LOOKUP_NOISY)) == RT_DIR_NULL) { \
	    bu_vls_printf(gedp->ged_result_str, "%s: Warning - %s not found in database.\n", (_cmd), (_name)); \
	    return GED_ERROR; \
	} \
	\
	if (!(_dp->d_flags & RT_DIR_COMB)) { \
	    bu_vls_printf((_gedp)->ged_result_str, "%s: Warning - %s not a combination\n", (_cmd), (_name)); \
	    return GED_ERROR; \
	} \
	\
	if (rt_db_get_internal(&(_intern), _dp, (_gedp)->ged_wdbp->dbip, (matp_t)NULL, &rt_uniresource) < 0) { \
	    bu_vls_printf((_gedp)->ged_result_str, "Database read error, aborting"); \
	    return GED_ERROR; \
	} \
	\
	_comb = (struct rt_comb_internal *)(_intern).idb_ptr;	\
	RT_CK_COMB(_comb); \
	if (_comb->tree) {							\
	    (_ntp) = db_dup_subtree(_comb->tree, &rt_uniresource);	\
	    RT_CK_TREE(_ntp);						\
	    \
	    /* Convert to "v4 / GIFT style", so that the flatten makes sense. */ \
	    if (db_ck_v4gift_tree(_ntp) < 0)					\
		db_non_union_push((_ntp), &rt_uniresource);			\
	    RT_CK_TREE(_ntp);							\
	    \
	    (_node_count) = db_tree_nleaves(_ntp);				\
	    (_rt_tree_array) = (struct rt_tree_array *)bu_calloc((_node_count), sizeof(struct rt_tree_array), "rt_tree_array"); \
	    \
	    /*							 \
	     * free=0 means that the tree won't have any leaf nodes freed. \
	     */								\
	    (void)db_flatten_tree((_rt_tree_array), (_ntp), OP_UNION, 0, &rt_uniresource); \
	} else { \
	    (_ntp) = TREE_NULL; \
	    (_node_count) = 0; \
	    (_rt_tree_array) = (struct rt_tree_array *)0; \
	} \
    }


HIDDEN int
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
	return GED_ERROR;
    }

    COMBMEM_GETCOMBTREE(gedp, argv[0], argv[1], dp, intern, ntp, rt_tree_array, node_count);

    for (i=0; i<node_count; i++) {
	union tree *itp = rt_tree_array[i].tl_tree;
	char op = '\0';

	RT_CK_TREE(itp);
	BU_ASSERT_LONG(itp->tr_op, ==, OP_DB_LEAF);
	BU_ASSERT_PTR(itp->tr_l.tl_name, !=, NULL);

	switch (rt_tree_array[i].tl_op) {
	    case OP_INTERSECT:
		op = '+';
		break;
	    case OP_SUBTRACT:
		op = '-';
		break;
	    case OP_UNION:
		op = 'u';
		break;
	    default:
		bu_bomb("combmem_get() corrupt rt_tree_array");
	}

	combmem_vls_print_member_info(gedp, op, itp, etype);
	bu_vls_printf(gedp->ged_result_str, "\n");
    }

    rt_db_free_internal(&intern);
    if (rt_tree_array) bu_free((genptr_t)rt_tree_array, "rt_tree_array");
    db_free_tree(ntp, &rt_uniresource);
    return GED_OK;
}


#define COMBMEM_SET_PART_I(_gedp, _argc, _cmd, _name, _num_params, _intern, _dp, _comb, _node_count, _rt_tree_array) { \
	if (rt_db_get_internal(&(_intern), (_dp), (_gedp)->ged_wdbp->dbip, (matp_t)NULL, &rt_uniresource) < 0) { \
	    bu_vls_printf((_gedp)->ged_result_str, "Database read error, aborting"); \
	    return GED_ERROR;							\
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


#define COMBMEM_SET_PART_II(_gedp, _argv, _op, _i, _rt_tree_array, _tree_index, _mat) { \
	(_op) = (_argv)[(_i)][0]; \
	\
	/* Add it to the combination */ \
	switch ((_op)) { \
	    case '+': \
		(_rt_tree_array)[(_tree_index)].tl_op = OP_INTERSECT; \
		break; \
	    case '-': \
		(_rt_tree_array)[(_tree_index)].tl_op = OP_SUBTRACT; \
		break; \
	    default: \
		bu_vls_printf((_gedp)->ged_result_str, "combmem_set: unrecognized relation (assume UNION)\n"); \
	    case 'u': \
		(_rt_tree_array)[(_tree_index)].tl_op = OP_UNION; \
		break; \
	} \
	\
	MAT_IDN((_mat));				\
    }


#define COMBMEM_SET_PART_III(_tp, _tree, _rt_tree_array, _tree_index, _name) \
    (_rt_tree_array)[(_tree_index)].tl_tree = (_tp); \
		    (_tp)->tr_l.tl_op = OP_DB_LEAF; \
		    (_tp)->tr_l.tl_name = bu_strdup(_name); \
			 (_tp)->tr_l.tl_mat = (matp_t)bu_calloc(16, sizeof(fastf_t), "combmem_set: mat");


#define COMBMEM_SET_PART_IV(_gedp, _comb, _tree_index, _intern, _final_tree, _node_count, _dp, _rt_tree_array, _old_intern, _old_rt_tree_array, _old_ntp) \
    db_free_tree((_comb)->tree, &rt_uniresource); \
    (_comb)->tree = NULL; \
    \
    if ((_tree_index)) \
	(_final_tree) = (union tree *)db_mkgift_tree((_rt_tree_array), (_node_count), &rt_uniresource); \
    else \
	(_final_tree) = TREE_NULL; \
    \
    RT_DB_INTERNAL_INIT(&(_intern)); \
    (_intern).idb_major_type = DB5_MAJORTYPE_BRLCAD; \
    (_intern).idb_type = ID_COMBINATION; \
	     (_intern).idb_meth = &rt_functab[ID_COMBINATION]; \
		      (_intern).idb_ptr = (genptr_t)(_comb); \
		      (_comb)->tree = (_final_tree); \
		      \
		      if (rt_db_put_internal((_dp), (_gedp)->ged_wdbp->dbip, &(_intern), &rt_uniresource) < 0) { \
			  bu_vls_printf((_gedp)->ged_result_str, "Unable to write new combination into database.\n"); \
			  \
			  rt_db_free_internal(&(_old_intern)); \
			  if (_old_rt_tree_array) \
			      bu_free((genptr_t)(_old_rt_tree_array), "rt_tree_array"); \
			  db_free_tree((_old_ntp), &rt_uniresource); \
			  \
			  return GED_ERROR; \
		      } \
		      \
		      rt_db_free_internal(&(_old_intern)); \
		      if (_old_rt_tree_array) \
			  bu_free((genptr_t)(_old_rt_tree_array), "rt_tree_array"); \
		      db_free_tree((_old_ntp), &rt_uniresource);


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


HIDDEN int
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
    char op;

    switch (etype) {
	case ETYPES_ABS:
	case ETYPES_REL:
	    break;
	default:
	    return GED_ERROR;
    }

    COMBMEM_GETCOMBTREE(gedp, argv[0], argv[1], dp, old_intern, old_ntp, old_rt_tree_array, old_node_count);
    COMBMEM_SET_PART_I(gedp, argc, argv[0], argv[1], 15, intern, dp, comb, node_count, rt_tree_array);

    tree_index = 0;
    for (i = 2; i < (size_t)argc; i += 15) {
	mat_t mat;
	fastf_t az, el, tw;
	fastf_t tx, ty, tz;
	fastf_t sa, sx, sy, sz;
	fastf_t kx, ky, kz;
	vect_t aetvec = VINIT_ZERO;
	vect_t tvec = VINIT_ZERO;
	point_t key_pt = VINIT_ZERO;
	hvect_t svec = HINIT_ZERO;

	COMBMEM_SET_PART_II(gedp, argv, op, i, rt_tree_array, tree_index, mat);

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
	    VSCALE(aetvec, aetvec, bn_degtorad);
	    VSET(tvec, tx, ty, tz);
	    VSCALE(tvec, tvec, gedp->ged_wdbp->dbip->dbi_local2base);
	    VSET(key_pt, kx, ky, kz);
	    VSCALE(key_pt, key_pt, gedp->ged_wdbp->dbip->dbi_local2base);
	    QSET(svec, sx, sy, sz, sa);

	    combmem_assemble_mat(mat, aetvec, tvec, svec, key_pt, 1);

	    /* If bn_mat_ck fails, it's because scaleX, scaleY and/or scaleZ has been
	     * been applied along with rotations. This screws up perpendicularity.
	     */
	    if (bn_mat_ck("combmem_set", mat)) {
		combmem_assemble_mat(mat, aetvec, tvec, svec, key_pt, 0);
	    }
	}

	BU_GETUNION(tp, tree);
	RT_TREE_INIT(tp);
	COMBMEM_SET_PART_III(tp, tree, rt_tree_array, tree_index, argv[i+1]);

	if (etype == ETYPES_REL && tree_index < old_node_count && old_rt_tree_array[tree_index].tl_tree->tr_l.tl_mat &&
	    BU_STR_EQUAL(old_rt_tree_array[tree_index].tl_tree->tr_l.tl_name, tp->tr_l.tl_name)) {

	    COMBMEM_CHECK_MAT(tp, tree_index, old_rt_tree_array, mat, aetvec, tvec, svec, key_pt, az, el, tw, tx, ty, tz, sa, sx, sy, sz);
	} else {
	    MAT_COPY(tp->tr_l.tl_mat, mat);
	}

	tree_index++;
    }

    COMBMEM_SET_PART_IV(gedp, comb, tree_index, intern, final_tree, node_count, dp, rt_tree_array, old_intern, old_rt_tree_array, old_ntp);

    return GED_OK;
}


HIDDEN int
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
    char op;

    switch (etype) {
	case ETYPES_ROT_AET:
	case ETYPES_ROT_XYZ:
	    break;
	default:
	    return GED_ERROR;
    }

    COMBMEM_GETCOMBTREE(gedp, argv[0], argv[1], dp, old_intern, old_ntp, old_rt_tree_array, old_node_count);
    COMBMEM_SET_PART_I(gedp, argc, argv[0], argv[1], 8, intern, dp, comb, node_count, rt_tree_array);

    tree_index = 0;
    for (i = 2; i < (size_t)argc; i += 8) {
	mat_t mat;
	fastf_t az, el, tw;
	fastf_t kx, ky, kz;
	point_t key_pt;

	COMBMEM_SET_PART_II(gedp, argv, op, i, rt_tree_array, tree_index, mat);

	if (sscanf(argv[i+2], "%lf", &az) == 1 &&
	    sscanf(argv[i+3], "%lf", &el) == 1 &&
	    sscanf(argv[i+4], "%lf", &tw) == 1 &&
	    sscanf(argv[i+5], "%lf", &kx) == 1 &&
	    sscanf(argv[i+6], "%lf", &ky) == 1 &&
	    sscanf(argv[i+7], "%lf", &kz) == 1) {

	    mat_t mat_rot;

	    VSET(key_pt, kx, ky, kz);
	    VSCALE(key_pt, key_pt, gedp->ged_wdbp->dbip->dbi_local2base);

	    if (etype == ETYPES_ROT_AET) {
		az *= bn_degtorad;
		el *= bn_degtorad;
		tw *= bn_degtorad;
		combmem_mat_aet(mat_rot, az, el, tw);
	    } else
		bn_mat_angles(mat_rot, az, el, tw);

	    bn_mat_xform_about_pt(mat, mat_rot, key_pt);
	}

	BU_GETUNION(tp, tree);
	RT_TREE_INIT(tp);
	COMBMEM_SET_PART_III(tp, tree, rt_tree_array, tree_index, argv[i+1]);

	if (tree_index < old_node_count && old_rt_tree_array[tree_index].tl_tree->tr_l.tl_mat &&
	    BU_STR_EQUAL(old_rt_tree_array[tree_index].tl_tree->tr_l.tl_name, tp->tr_l.tl_name)) {
	    fastf_t tx, ty, tz;
	    fastf_t sa, sx, sy, sz;
	    vect_t aetvec = VINIT_ZERO;
	    vect_t tvec = VINIT_ZERO;
	    vect_t svec;

	    COMBMEM_CHECK_MAT(tp, tree_index, old_rt_tree_array, mat, aetvec, tvec, svec, key_pt, az, el, tw, tx, ty, tz, sa, sx, sy, sz);
	} else {
	    MAT_COPY(tp->tr_l.tl_mat, mat);
	}

	tree_index++;
    }

    COMBMEM_SET_PART_IV(gedp, comb, tree_index, intern, final_tree, node_count, dp, rt_tree_array, old_intern, old_rt_tree_array, old_ntp);

    return GED_OK;
}


HIDDEN int
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
    char op;

    if (etype != ETYPES_ROT_ARBITRARY_AXIS)
	return GED_ERROR;

    COMBMEM_GETCOMBTREE(gedp, argv[0], argv[1], dp, old_intern, old_ntp, old_rt_tree_array, old_node_count);
    COMBMEM_SET_PART_I(gedp, argc, argv[0], argv[1], 9, intern, dp, comb, node_count, rt_tree_array);

    tree_index = 0;
    for (i = 2; i < (size_t)argc; i += 9) {
	mat_t mat;
	fastf_t px, py, pz;
	fastf_t dx, dy, dz;
	fastf_t ang;
	vect_t dir;
	point_t pt;

	COMBMEM_SET_PART_II(gedp, argv, op, i, rt_tree_array, tree_index, mat);

	if (sscanf(argv[i+2], "%lf", &px) == 1 &&
	    sscanf(argv[i+3], "%lf", &py) == 1 &&
	    sscanf(argv[i+4], "%lf", &pz) == 1 &&
	    sscanf(argv[i+5], "%lf", &dx) == 1 &&
	    sscanf(argv[i+6], "%lf", &dy) == 1 &&
	    sscanf(argv[i+7], "%lf", &dz) == 1 &&
	    sscanf(argv[i+8], "%lf", &ang) == 1) {

	    VSET(pt, px, py, pz);
	    VSCALE(pt, pt, gedp->ged_wdbp->dbip->dbi_local2base);
	    VSET(dir, dx, dy, dz);
	    VUNITIZE(dir);
	    ang *= bn_degtorad;
	    bn_mat_arb_rot(mat, pt, dir, ang);
	}

	BU_GETUNION(tp, tree);
	RT_TREE_INIT(tp);
	COMBMEM_SET_PART_III(tp, tree, rt_tree_array, tree_index, argv[i+1]);

	if (tree_index < old_node_count && old_rt_tree_array[tree_index].tl_tree->tr_l.tl_mat &&
	    BU_STR_EQUAL(old_rt_tree_array[tree_index].tl_tree->tr_l.tl_name, tp->tr_l.tl_name)) {
	    fastf_t az, el, tw;
	    fastf_t tx, ty, tz;
	    fastf_t sa, sx, sy, sz;
	    vect_t aetvec = VINIT_ZERO;
	    vect_t tvec = VINIT_ZERO;
	    vect_t svec;
	    point_t key_pt;

	    COMBMEM_CHECK_MAT(tp, tree_index, old_rt_tree_array, mat, aetvec, tvec, svec, key_pt, az, el, tw, tx, ty, tz, sa, sx, sy, sz);
	} else {
	    MAT_COPY(tp->tr_l.tl_mat, mat);
	}

	tree_index++;
    }

    COMBMEM_SET_PART_IV(gedp, comb, tree_index, intern, final_tree, node_count, dp, rt_tree_array, old_intern, old_rt_tree_array, old_ntp);

    return GED_OK;
}


HIDDEN int
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
    char op;

    if (etype != ETYPES_TRA)
	return GED_ERROR;

    COMBMEM_GETCOMBTREE(gedp, argv[0], argv[1], dp, old_intern, old_ntp, old_rt_tree_array, old_node_count);
    COMBMEM_SET_PART_I(gedp, argc, argv[0], argv[1], 5, intern, dp, comb, node_count, rt_tree_array);

    tree_index = 0;
    for (i = 2; i < (size_t)argc; i += 5) {
	mat_t mat;
	fastf_t tx, ty, tz;
	vect_t tvec = VINIT_ZERO;

	COMBMEM_SET_PART_II(gedp, argv, op, i, rt_tree_array, tree_index, mat);

	if (sscanf(argv[i+2], "%lf", &tx) == 1 &&
	    sscanf(argv[i+3], "%lf", &ty) == 1 &&
	    sscanf(argv[i+4], "%lf", &tz) == 1) {

	    VSET(tvec, tx, ty, tz);
	    VSCALE(tvec, tvec, gedp->ged_wdbp->dbip->dbi_local2base);
	    MAT_DELTAS_VEC(mat, tvec);
	}

	BU_GETUNION(tp, tree);
	RT_TREE_INIT(tp);
	COMBMEM_SET_PART_III(tp, tree, rt_tree_array, tree_index, argv[i+1]);

	if (tree_index < old_node_count && old_rt_tree_array[tree_index].tl_tree->tr_l.tl_mat &&
	    BU_STR_EQUAL(old_rt_tree_array[tree_index].tl_tree->tr_l.tl_name, tp->tr_l.tl_name)) {

	    bn_mat_mul(tp->tr_l.tl_mat, mat, old_rt_tree_array[tree_index].tl_tree->tr_l.tl_mat);
	} else {
	    MAT_COPY(tp->tr_l.tl_mat, mat);
	}

	tree_index++;
    }

    COMBMEM_SET_PART_IV(gedp, comb, tree_index, intern, final_tree, node_count, dp, rt_tree_array, old_intern, old_rt_tree_array, old_ntp);

    return GED_OK;
}


HIDDEN int
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
    char op;

    if (etype != ETYPES_SCA)
	return GED_ERROR;

    COMBMEM_GETCOMBTREE(gedp, argv[0], argv[1], dp, old_intern, old_ntp, old_rt_tree_array, old_node_count);
    COMBMEM_SET_PART_I(gedp, argc, argv[0], argv[1], 9, intern, dp, comb, node_count, rt_tree_array);

    tree_index = 0;
    for (i = 2; i < (size_t)argc; i += 9) {
	mat_t mat;
	fastf_t sa, sx, sy, sz;
	fastf_t kx, ky, kz;
	vect_t aetvec = VINIT_ZERO;
	vect_t tvec = VINIT_ZERO;
	point_t key_pt;
	hvect_t svec;

	HSETALL(svec, 0);

	COMBMEM_SET_PART_II(gedp, argv, op, i, rt_tree_array, tree_index, mat);

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
	    VSCALE(key_pt, key_pt, gedp->ged_wdbp->dbip->dbi_local2base);
	    QSET(svec, sx, sy, sz, sa);

	    combmem_assemble_mat(mat, aetvec, tvec, svec, key_pt, 1);
	}

	BU_GETUNION(tp, tree);
	RT_TREE_INIT(tp);
	COMBMEM_SET_PART_III(tp, tree, rt_tree_array, tree_index, argv[i+1]);

	if (tree_index < old_node_count && old_rt_tree_array[tree_index].tl_tree->tr_l.tl_mat &&
	    BU_STR_EQUAL(old_rt_tree_array[tree_index].tl_tree->tr_l.tl_name, tp->tr_l.tl_name)) {
	    fastf_t az, el, tw;
	    fastf_t tx, ty, tz;

	    COMBMEM_CHECK_MAT(tp, tree_index, old_rt_tree_array, mat, aetvec, tvec, svec, key_pt, az, el, tw, tx, ty, tz, sa, sx, sy, sz);
	} else {
	    MAT_COPY(tp->tr_l.tl_mat, mat);
	}

	tree_index++;
    }

    COMBMEM_SET_PART_IV(gedp, comb, tree_index, intern, final_tree, node_count, dp, rt_tree_array, old_intern, old_rt_tree_array, old_ntp);

    return GED_OK;
}


HIDDEN int
combmem_set_empty(struct ged *gedp, int argc, const char *argv[])
{
    struct directory *dp;
    struct rt_comb_internal *comb;
    struct rt_db_internal intern;

    if (argc < 2) {
	bu_vls_printf(gedp->ged_result_str, "ERROR argument missing after [%s]\n", argv[0]);
	return GED_ERROR;
    }

    if ((dp = db_lookup(gedp->ged_wdbp->dbip, argv[1], LOOKUP_NOISY)) == RT_DIR_NULL) {
	bu_vls_printf(gedp->ged_result_str, "%s: Warning - %s not found in database.\n", argv[0], argv[1]);
	return GED_ERROR;
    }

    if (!(dp->d_flags & RT_DIR_COMB)) {
	bu_vls_printf(gedp->ged_result_str, "%s: Warning - %s not a combination\n", argv[0], argv[1]);
	return GED_ERROR;
    }									\

    if (rt_db_get_internal(&intern, dp, gedp->ged_wdbp->dbip, (matp_t)NULL, &rt_uniresource) < 0) {
	bu_vls_printf(gedp->ged_result_str, "Database read error, aborting");
	return GED_ERROR;
    }
    comb = (struct rt_comb_internal *)intern.idb_ptr;
    RT_CK_COMB(comb);

    db_free_tree(comb->tree, &rt_uniresource);
    comb->tree = NULL;

    if (rt_db_put_internal(dp, gedp->ged_wdbp->dbip, &intern, &rt_uniresource) < 0) {
	bu_vls_printf(gedp->ged_result_str, "Unable to write combination into database - %s\n", argv[1]);
	return GED_ERROR;
    }

    return GED_OK;
}


/*
 * Set/get a combinations members.
 */
int
ged_combmem(struct ged *gedp, int argc, const char *argv[])
{
    int c;
    enum etypes iflag = ETYPES_ABS;
    enum etypes rflag = ETYPES_NULL;
    const char *cmd_name = argv[0];
    static const char *usage = "[-i type] [-r type] comb [op1 name1 az1 el1 tw1 x1 y1 z1 sa1 sx1 sy1 sz1 ...]";

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", cmd_name, usage);
	return GED_HELP;
    }

    bu_optind = 1;

    /* Get command line options. */
    while ((c = bu_getopt(argc, (char * const *)argv, "i:r:")) != -1) {
	switch (c) {
	    case 'i':
		{
		    int d;

		    if (sscanf(bu_optarg, "%d", &d) != 1 || d < ETYPES_ABS || d > ETYPES_SCA) {
			goto bad;
		    }

		    iflag = (enum etypes)d;
		}
		break;
	    case 'r':
		{
		    int d;

		    if (sscanf(bu_optarg, "%d", &d) != 1 || d < ETYPES_ABS || d > ETYPES_SCA) {
			goto bad;
		    }

		    rflag = (enum etypes)d;
		}
		break;
	    default:
		break;
	}
    }
    argc -= (bu_optind - 1);
    argv += (bu_optind - 1);

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
	case ETYPES_SCA:
	    if (argc > 10 && !((argc-2)%9)) {
		return combmem_set_sca(gedp, argc, argv, rflag);
	    }
	case ETYPES_NULL:
	default:
	    if (argc > 16 && !((argc-2)%15)) {
		return combmem_set(gedp, argc, argv, ETYPES_ABS);
	    }
	    break;
    }

bad:
    bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", cmd_name, usage);
    return GED_ERROR;
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
