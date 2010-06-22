/*                         C O M B M E M . C
 * BRL-CAD
 *
 * Copyright (c) 2008-2010 United States Government as represented by
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
/** @file combmem.c
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

static void ged_mat_aet(matp_t matp, fastf_t az, fastf_t el, fastf_t tw);


/**
 * G E D _ M A T _ A E T
 *
 * Given the azimuth, elevation and twist angles,
 * calculate the rotation part of a 4x4 matrix.
 */
static void
ged_mat_aet(matp_t matp, fastf_t az, fastf_t el, fastf_t tw)
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
 * G E D _ D I S A S S E M B L E _ R M A T
 *
 * Disassemble the given rotation matrix into az, el, tw.
 */
static void
ged_disassemble_rmat(matp_t matp, fastf_t *az, fastf_t *el, fastf_t *tw)
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

    if (NEAR_ZERO(cos_az-1.0, 0.000001) ||
	NEAR_ZERO(cos_az+1.0, 0.000001)) {
	*az= 0.0;
    } else
	*az = acos(cos_az);

    if (NEAR_ZERO(cos_el-1.0, 0.000001) ||
	NEAR_ZERO(cos_el+1.0, 0.000001)) {
	*el= 0.0;
    } else
	*el = acos(cos_el);

    if (NEAR_ZERO(cos_tw-1.0, 0.000001) ||
	NEAR_ZERO(cos_tw+1.0, 0.000001)) {
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
 * G E D _ D I S A S S E M B L E _ M A T
 *
 * Disassemble the given matrix into az, el, tw, tx, ty, tz, sa, sx, sy and sz.
 */
static int
ged_disassemble_mat(matp_t matp, fastf_t *az, fastf_t *el, fastf_t *tw, fastf_t *tx, fastf_t *ty, fastf_t *tz, fastf_t *sa, fastf_t *sx, fastf_t *sy, fastf_t *sz)
{
    mat_t m;

    if (!matp)
	return 1;

    MAT_COPY(m, matp);

    *sa = 1.0 / m[MSA];

    *sx = sqrt(m[0]*m[0] + m[4]*m[4] + m[8]*m[8]);
    *sy = sqrt(m[1]*m[1] + m[5]*m[5] + m[9]*m[9]);
    *sz = sqrt(m[2]*m[2] + m[6]*m[6] + m[10]*m[10]);

    if (!NEAR_ZERO(*sx-1.0,VUNITIZE_TOL)) {
	m[0] /= *sx;
	m[4] /= *sx;
	m[8] /= *sx;
    }

    if (!NEAR_ZERO(*sy-1.0,VUNITIZE_TOL)) {
	m[1] /= *sy;
	m[5] /= *sy;
	m[9] /= *sy;
    }

    if (!NEAR_ZERO(*sz-1.0,VUNITIZE_TOL)) {
	m[2] /= *sz;
	m[6] /= *sz;
	m[10] /= *sz;
    }

    if (bn_mat_ck("ged_disassemble_mat", m)) {
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

    ged_disassemble_rmat(m, az, el, tw);

    return 0; /* OK */
}

/**
 * G E D _ A S S E M B L E _ M A T
 *
 * Assemble the given aetvec, tvec and svec into a 4x4 matrix using key_pt for rotations and scale.
 */
static void
ged_assemble_mat(matp_t matp, vect_t aetvec, vect_t tvec, hvect_t svec, point_t key_pt, int sflag)
{
    mat_t mat_aet_about_pt;
    mat_t mat_aet;
    mat_t mat_scale;
    mat_t xlate;
    mat_t tmp;

    if (!matp)
	return;

    if (NEAR_ZERO(svec[X], SMALL) ||
	NEAR_ZERO(svec[Y], SMALL) ||
	NEAR_ZERO(svec[Z], SMALL) ||
	NEAR_ZERO(svec[W], SMALL)) {
	return;
    }

    ged_mat_aet(mat_aet, aetvec[X], aetvec[Y], aetvec[Z]);
    bn_mat_xform_about_pt(mat_aet_about_pt, mat_aet, key_pt);

    MAT_IDN(mat_scale);

    if (sflag) {
	if (!NEAR_ZERO(svec[X]-1.0,VUNITIZE_TOL)) {
	    mat_scale[0] *= svec[X];
	    mat_scale[4] *= svec[X];
	    mat_scale[8] *= svec[X];
	}

	if (!NEAR_ZERO(svec[Y]-1.0,VUNITIZE_TOL)) {
	    mat_scale[1] *= svec[Y];
	    mat_scale[5] *= svec[Y];
	    mat_scale[9] *= svec[Y];
	}

	if (!NEAR_ZERO(svec[Z]-1.0,VUNITIZE_TOL)) {
	    mat_scale[2] *= svec[Z];
	    mat_scale[6] *= svec[Z];
	    mat_scale[10] *= svec[Z];
	}
    }

    if (!NEAR_ZERO(svec[W]-1.0,VUNITIZE_TOL)) {
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
ged_vls_print_member_info(struct ged *gedp, char op, union tree *itp, int iflag)
{
    fastf_t az, el, tw;
    fastf_t tx, ty, tz;
    fastf_t sa, sx, sy, sz;

    if (!itp->tr_l.tl_mat || iflag)
	bu_vls_printf(&gedp->ged_result_str, "%c %s 0.0 0.0 0.0 0.0 0.0 0.0 1.0 1.0 1.0 1.0 0.0 0.0 0.0",
		      op, itp->tr_l.tl_name);
    else {
	if (ged_disassemble_mat(itp->tr_l.tl_mat, &az, &el, &tw, &tx, &ty, &tz, &sa, &sx, &sy, &sz))
	    bu_log("Found bad matrix for %s\n", itp->tr_l.tl_name);

	bu_vls_printf(&gedp->ged_result_str, "%c %s %lf %lf %lf %lf %lf %lf %lf %lf %lf %lf 0.0 0.0 0.0",
		      op, itp->tr_l.tl_name,
		      az * bn_radtodeg,
		      el * bn_radtodeg,
		      tw * bn_radtodeg,
		      tx, ty, tz,
		      sa, sx, sy, sz);
    }
}

#define GED_GETCOMBTREE(_gedp,_cmd,_name,_intern,_ntp,_rt_tree_array,_node_count) { \
  struct directory *_dp; \
  struct rt_comb_internal *_comb; \
\
  if ((_dp = db_lookup((_gedp)->ged_wdbp->dbip, (_name), LOOKUP_NOISY)) == DIR_NULL) { \
    bu_vls_printf(&gedp->ged_result_str, "%s: Warning - %s not found in database.\n", (_cmd), (_name)); \
    return GED_ERROR; \
  } \
\
  if (!(_dp->d_flags & DIR_COMB)) { \
    bu_vls_printf(&(_gedp)->ged_result_str, "%s: Warning - %s not a combination\n", (_cmd), (_name)); \
    return GED_ERROR; \
  } \
\
  if (rt_db_get_internal(&(_intern), _dp, (_gedp)->ged_wdbp->dbip, (matp_t)NULL, &rt_uniresource) < 0) { \
    bu_vls_printf(&(_gedp)->ged_result_str, "Database read error, aborting"); \
    return GED_ERROR; \
  } \
\
  _comb = (struct rt_comb_internal *)(_intern).idb_ptr;	\
  RT_CK_COMB(_comb); \
  if (!_comb->tree) {							\
    bu_vls_printf(&(_gedp)->ged_result_str, "%s: empty combination", _dp->d_namep); \
    rt_db_free_internal(&(_intern)); \
    return GED_ERROR; \
  } \
\
  (_ntp) = db_dup_subtree(_comb->tree, &rt_uniresource); \
  RT_CK_TREE(_ntp); \
\
  /* Convert to "v4 / GIFT style", so that the flatten makes sense. */ \
  if (db_ck_v4gift_tree(_ntp) < 0) \
    db_non_union_push((_ntp), &rt_uniresource); \
  RT_CK_TREE(_ntp); \
\
  (_node_count) = db_tree_nleaves(_ntp); \
  (_rt_tree_array) = (struct rt_tree_array *)bu_calloc((_node_count), sizeof(struct rt_tree_array), "rt_tree_array"); \
\
  /* \
   * free=0 means that the tree won't have any leaf nodes freed. \
   */ \
  (void)db_flatten_tree((_rt_tree_array), (_ntp), OP_UNION, 0, &rt_uniresource); \
}

static int
ged_getcombmem(struct ged *gedp, int argc, const char *argv[], int iflag)
{
    struct rt_db_internal intern;
    union tree *ntp;
    struct rt_tree_array *rt_tree_array;
    size_t i;
    size_t node_count;

    GED_GETCOMBTREE(gedp,argv[0],argv[1],intern,ntp,rt_tree_array,node_count);

    for (i=0; i<node_count; i++) {
	union tree *itp = rt_tree_array[i].tl_tree;
	char op;
	register int j;

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
		bu_bomb("ged_getcombmem() corrupt rt_tree_array");
	}

	ged_vls_print_member_info(gedp, op, itp, iflag);
	bu_vls_printf(&gedp->ged_result_str, "\n");
    }

    rt_db_free_internal(&intern);
    if (rt_tree_array) bu_free((genptr_t)rt_tree_array, "rt_tree_array");
    db_free_tree(ntp, &rt_uniresource);
    return GED_OK;
}

static int
ged_setcombmem_abs(struct ged *gedp, int argc, const char *argv[])
{
    struct directory *dp;
    struct rt_db_internal intern;
    struct rt_comb_internal *comb;
    union tree *ntp;
    struct rt_tree_array *rt_tree_array;
    size_t i;
    size_t node_count;
    size_t tree_index;
    union tree *tp;
    union tree *final_tree;
    char op;

    if ((dp = db_lookup(gedp->ged_wdbp->dbip, argv[1], LOOKUP_NOISY)) == DIR_NULL) {
	bu_vls_printf(&gedp->ged_result_str, "%s: Warning - %s not found in database.\n", argv[0], argv[1]);
	return GED_ERROR;
    }

    if (!(dp->d_flags & DIR_COMB)) {
	bu_vls_printf(&gedp->ged_result_str, "%s: Warning - %s not a combination\n", argv[0], argv[1]);
	return GED_ERROR;
    }

    if (rt_db_get_internal(&intern, dp, gedp->ged_wdbp->dbip, (matp_t)NULL, &rt_uniresource) < 0) {
	bu_vls_printf(&gedp->ged_result_str, "Database read error, aborting");
	return GED_ERROR;
    }

    comb = (struct rt_comb_internal *)intern.idb_ptr;
    RT_CK_COMB(comb);
    if (!comb->tree) {
	bu_vls_printf(&gedp->ged_result_str, "%s: empty combination", dp->d_namep);
	rt_db_free_internal(&intern);
	return GED_ERROR;
    }

    db_free_tree(comb->tree, &rt_uniresource);
    comb->tree = NULL;
    node_count = (argc - 2) / 15; /* integer division */
    rt_tree_array = (struct rt_tree_array *)bu_calloc(node_count, sizeof(struct rt_tree_array), "tree list");

    tree_index = 0;
    for (i = 2; i < argc; i += 15) {
	matp_t matp;
	fastf_t az, el, tw;
	fastf_t tx, ty, tz;
	fastf_t sa, sx, sy, sz;
	fastf_t kx, ky, kz;

	op = argv[i][0];

      	/* Add it to the combination */
      	switch (op) {
      	case '+':
	    rt_tree_array[tree_index].tl_op = OP_INTERSECT;
	    break;
	case '-':
	    rt_tree_array[tree_index].tl_op = OP_SUBTRACT;
	    break;
	default:
	    bu_vls_printf(&gedp->ged_result_str, "ged_setcombmem_abs: unrecognized relation (assume UNION)\n");
	case 'u':
	    rt_tree_array[tree_index].tl_op = OP_UNION;
	    break;
	}

	matp = (matp_t)bu_calloc(16, sizeof(fastf_t), "ged_setcombmem_abs: mat");
	MAT_IDN(matp);

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

	    vect_t aetvec, tvec;
	    point_t key_pt;
	    hvect_t svec;

	    VSET(aetvec, az, el, tw);
	    VSCALE(aetvec, aetvec, bn_degtorad);
	    VSET(tvec, tx, ty, tz);
	    VSCALE(tvec, tvec, gedp->ged_wdbp->dbip->dbi_local2base);
	    VSET(key_pt, kx, ky, kz);
	    VSCALE(key_pt, key_pt, gedp->ged_wdbp->dbip->dbi_local2base);
	    QSET(svec, sx, sy, sz, sa);

	    ged_assemble_mat(matp, aetvec, tvec, svec, key_pt, 1);
	}

	BU_GETUNION(tp, tree);
	rt_tree_array[tree_index].tl_tree = tp;
	tp->tr_l.magic = RT_TREE_MAGIC;
	tp->tr_l.tl_op = OP_DB_LEAF;
	tp->tr_l.tl_name = bu_strdup(argv[i+1]);
	tp->tr_l.tl_mat = matp;
	tree_index++;
    }

    if (tree_index)
	final_tree = (union tree *)db_mkgift_tree(rt_tree_array, node_count, &rt_uniresource);
    else
	final_tree = (union tree *)NULL;

    RT_INIT_DB_INTERNAL(&intern);
    intern.idb_major_type = DB5_MAJORTYPE_BRLCAD;
    intern.idb_type = ID_COMBINATION;
    intern.idb_meth = &rt_functab[ID_COMBINATION];
    intern.idb_ptr = (genptr_t)comb;
    comb->tree = final_tree;

    if (rt_db_put_internal(dp, gedp->ged_wdbp->dbip, &intern, &rt_uniresource) < 0) {
	bu_vls_printf(&gedp->ged_result_str, "_ged_make_tree: Unable to write new combination into database.\n");
	return GED_ERROR;
    }

    return GED_OK;
}

static int
ged_setcombmem_rel(struct ged *gedp, int argc, const char *argv[])
{
    struct rt_db_internal old_intern;
    union tree *old_ntp;
    struct rt_tree_array *old_rt_tree_array;
    size_t old_node_count;

    struct directory *dp;
    struct rt_db_internal intern;
    struct rt_comb_internal *comb;
    union tree *ntp;
    struct rt_tree_array *rt_tree_array;
    size_t i;
    size_t node_count;
    size_t tree_index;
    union tree *tp;
    union tree *final_tree;
    char op;

    GED_GETCOMBTREE(gedp,argv[0],argv[1],old_intern,old_ntp,old_rt_tree_array,old_node_count);

    if ((dp = db_lookup(gedp->ged_wdbp->dbip, argv[1], LOOKUP_NOISY)) == DIR_NULL) {
	bu_vls_printf(&gedp->ged_result_str, "%s: Warning - %s not found in database.\n", argv[0], argv[1]);
	return GED_ERROR;
    }

    if (!(dp->d_flags & DIR_COMB)) {
	bu_vls_printf(&gedp->ged_result_str, "%s: Warning - %s not a combination\n", argv[0], argv[1]);
	return GED_ERROR;
    }

    if (rt_db_get_internal(&intern, dp, gedp->ged_wdbp->dbip, (matp_t)NULL, &rt_uniresource) < 0) {
	bu_vls_printf(&gedp->ged_result_str, "Database read error, aborting");
	return GED_ERROR;
    }

    comb = (struct rt_comb_internal *)intern.idb_ptr;
    RT_CK_COMB(comb);
    if (!comb->tree) {
	bu_vls_printf(&gedp->ged_result_str, "%s: empty combination", dp->d_namep);
	rt_db_free_internal(&intern);
	return GED_ERROR;
    }

    node_count = (argc - 2) / 15; /* integer division */
    rt_tree_array = (struct rt_tree_array *)bu_calloc(node_count, sizeof(struct rt_tree_array), "tree list");

    tree_index = 0;
    for (i = 2; i < argc; i += 15) {
	mat_t mat;
	fastf_t az, el, tw;
	fastf_t tx, ty, tz;
	fastf_t sa, sx, sy, sz;
	fastf_t kx, ky, kz;
	vect_t aetvec, tvec;
	point_t key_pt;
	hvect_t svec;

	op = argv[i][0];

      	/* Add it to the combination */
      	switch (op) {
      	case '+':
	    rt_tree_array[tree_index].tl_op = OP_INTERSECT;
	    break;
	case '-':
	    rt_tree_array[tree_index].tl_op = OP_SUBTRACT;
	    break;
	default:
	    bu_vls_printf(&gedp->ged_result_str, "ged_setcombmem_rel: unrecognized relation (assume UNION)\n");
	case 'u':
	    rt_tree_array[tree_index].tl_op = OP_UNION;
	    break;
	}

	MAT_IDN(mat);

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
	    VSET(key_pt, kx, ky, kz);
	    QSET(svec, sx, sy, sz, sa);

	    ged_assemble_mat(mat, aetvec, tvec, svec, key_pt, 1);

	    /* If bn_mat_ck fails, it's because scaleX, scaleY and/or scaleZ has been
	     * been applied along with rotations. This screws up perpendicularity.
	     */
	    if (bn_mat_ck("ged_setcombmem_rel", mat)) {
		ged_assemble_mat(mat, aetvec, tvec, svec, key_pt, 0);
	    }
	}

	BU_GETUNION(tp, tree);
	rt_tree_array[tree_index].tl_tree = tp;
	tp->tr_l.magic = RT_TREE_MAGIC;
	tp->tr_l.tl_op = OP_DB_LEAF;
	tp->tr_l.tl_name = bu_strdup(argv[i+1]);
	tp->tr_l.tl_mat = (matp_t)bu_calloc(16, sizeof(fastf_t), "ged_setcombmem_rel: mat");

	if (tree_index < old_node_count && old_rt_tree_array[tree_index].tl_tree->tr_l.tl_mat &&
	    !strcmp(old_rt_tree_array[tree_index].tl_tree->tr_l.tl_name, tp->tr_l.tl_name)) {
	    bn_mat_mul(tp->tr_l.tl_mat, mat, old_rt_tree_array[tree_index].tl_tree->tr_l.tl_mat);

	    /* If bn_mat_ck fails, it's because scaleX, scaleY and/or scaleZ has been
	     * been applied along with rotations. This screws up perpendicularity.
	     */
	    if (bn_mat_ck("ged_setcombmem_rel", tp->tr_l.tl_mat)) {
		ged_assemble_mat(mat, aetvec, tvec, svec, key_pt, 0);
		bn_mat_mul(tp->tr_l.tl_mat, mat, old_rt_tree_array[tree_index].tl_tree->tr_l.tl_mat);

		if (bn_mat_ck("ged_setcombmem_rel", tp->tr_l.tl_mat)) {
		    MAT_COPY(tp->tr_l.tl_mat, old_rt_tree_array[tree_index].tl_tree->tr_l.tl_mat);
		}
	    } else {
		fastf_t az, el, tw;
		fastf_t tx, ty, tz;
		fastf_t sa, sx, sy, sz;

		/* This will check if the 3x3 rotation part is bad after factoring out scaleX, scaleY and scaleZ. */
		if (ged_disassemble_mat(tp->tr_l.tl_mat, &az, &el, &tw, &tx, &ty, &tz, &sa, &sx, &sy, &sz)) {
		    MAT_COPY(tp->tr_l.tl_mat, old_rt_tree_array[tree_index].tl_tree->tr_l.tl_mat);
		}
	    }
	} else {
	    MAT_COPY(tp->tr_l.tl_mat, mat);
	}

	tree_index++;
    }

    db_free_tree(comb->tree, &rt_uniresource);
    comb->tree = NULL;

    if (tree_index)
	final_tree = (union tree *)db_mkgift_tree(rt_tree_array, node_count, &rt_uniresource);
    else
	final_tree = (union tree *)NULL;

    RT_INIT_DB_INTERNAL(&intern);
    intern.idb_major_type = DB5_MAJORTYPE_BRLCAD;
    intern.idb_type = ID_COMBINATION;
    intern.idb_meth = &rt_functab[ID_COMBINATION];
    intern.idb_ptr = (genptr_t)comb;
    comb->tree = final_tree;

    if (rt_db_put_internal(dp, gedp->ged_wdbp->dbip, &intern, &rt_uniresource) < 0) {
	bu_vls_printf(&gedp->ged_result_str, "_ged_make_tree: Unable to write new combination into database.\n");

	rt_db_free_internal(&old_intern);
	if (old_rt_tree_array)
	    bu_free((genptr_t)old_rt_tree_array, "rt_tree_array");
	db_free_tree(old_ntp, &rt_uniresource);

	return GED_ERROR;
    }

    rt_db_free_internal(&old_intern);
    if (old_rt_tree_array)
	bu_free((genptr_t)old_rt_tree_array, "rt_tree_array");
    db_free_tree(old_ntp, &rt_uniresource);

    return GED_OK;
}

/*
 * Set/get a combinations members.
 */
int
ged_combmem(struct ged *gedp, int argc, const char *argv[])
{
    int iflag = 0;
    int rflag = 0;
    static const char *usage = "[-r] comb [op1 name1 az1 el1 tw1 x1 y1 z1 sa1 sx1 sy1 sz1 ...]";

    /* initialize result */
    bu_vls_trunc(&gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    if (argv[1][0] == '-' && argv[1][1] == 'i' && argv[1][2] == '\0') {
	iflag = 1;
	--argc;
	++argv;
    }

    if (argc == 2)
	return ged_getcombmem(gedp, argc, argv, iflag);

    if (argv[1][0] == '-' && argv[1][1] == 'r' && argv[1][2] == '\0') {
	rflag = 1;
	--argc;
	++argv;
    }

    if (argc > 16 && !((argc-2)%15)) {
	if (rflag)
	    return ged_setcombmem_rel(gedp, argc, argv);
	else
	    return ged_setcombmem_abs(gedp, argc, argv);
    }

    bu_vls_printf(&gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
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
