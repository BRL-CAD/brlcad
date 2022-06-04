/*                       D B _ F P . C P P
 * BRL-CAD
 *
 * Copyright (c) 1990-2022 United States Government as represented by
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
/** @addtogroup dbio */
/** @{ */
/** @file librt/db_fp.cpp
 *
 * Routines to manipulate geometry database hierarchical path description
 * structures.
 */

#include "common.h"

#include <vector>
#include <set>
#include <stack>
#include <map>
#include <utility>

#include <limits.h>
#include <math.h>
#include <string.h>
#include "bio.h"

extern "C" {
#include "vmath.h"
#include "bu/opt.h"
#include "bn/mat.h"
#include "rt/db_fp.h"
#include "raytrace.h"
}


static int
_db_comb_instance(matp_t m, int *icnt, int *bval, int bool_val, const struct db_i *dbip, union tree *tp, const char *cp, int itarget)
{
    int ret;
    if (!tp) return 0;

    RT_CHECK_DBI(dbip);
    RT_CK_TREE(tp);

    switch (tp->tr_op) {


	case OP_DB_LEAF:
	    if (!BU_STR_EQUAL(cp, tp->tr_l.tl_name))
		return 0;               /* NO-OP */
	    // Have a name match, is this the right instance
	    if (itarget == *icnt) {
		(*bval) = bool_val;
		if (tp->tr_l.tl_mat && m)
		    MAT_COPY(m, tp->tr_l.tl_mat);
		return 1;
	    } else {
		(*icnt)++;
		return 0;
	    }
	case OP_UNION:
	case OP_INTERSECT:
	case OP_SUBTRACT:
	case OP_XOR:
	    ret = _db_comb_instance(m, icnt, bval, OP_UNION, dbip, tp->tr_b.tb_left, cp, itarget);
	    if (ret)
		return ret;
	    return _db_comb_instance(m, icnt, bval, tp->tr_op, dbip, tp->tr_b.tb_right, cp, itarget);
	default:
	    bu_log("_db_comb_instance: bad op %d\n", tp->tr_op);
	    bu_bomb("_db_comb_instance\n");
    }
}

static int
db_comb_has_instance(int *bval, const struct db_i *dbip, struct directory *cdp, struct directory *dp, int itarget)
{
    RT_CK_DBI(dbip);

    if (!(cdp->d_flags & RT_DIR_COMB))
	return 0;

    struct rt_db_internal in;
    struct rt_comb_internal *comb;
    if (rt_db_get_internal(&in, cdp, dbip, NULL, &rt_uniresource) < 0)
	return 0;
    comb = (struct rt_comb_internal *)in.idb_ptr;
    RT_CK_COMB(comb);

    int icnt = 0;
    int has_inst = _db_comb_instance(NULL, &icnt, bval, OP_UNION, dbip, comb->tree, dp->d_namep, itarget);
    rt_db_free_internal(&in);
    return has_inst;
}

static struct directory *
dp_instance(int *comb_instance_index, const struct db_i *dbip, const char *cp)
{
    struct directory *dp = RT_DIR_NULL;
    char *lcp = bu_strdup(cp);
    char *atp = strchr(lcp, '@');
    if (atp) {
	*atp = '\0';
	atp++;
	if (bu_opt_int(NULL, 1, (const char **)&atp, (void *)comb_instance_index) == -1) {
	    bu_log("invalid comb instance index: %s\n", atp);
	    bu_free(lcp, "lcp");
	    return dp;
	}
	dp = db_lookup(dbip, lcp, LOOKUP_NOISY);
    }
    bu_free(lcp, "lcp");
    return dp;
}

int
db_string_to_path(struct db_full_path *pp, const struct db_i *dbip, const char *str)
{
    char *cp;
    char *slashp;
    struct directory *dp;
    char *copy;
    size_t nslash = 0;
    int ret = 0;
    size_t len;

    /* assume NULL str is '/' */
    if (!str) {
	db_full_path_init(pp);
	return 0;
    }

    while (*str == '/') str++; /* strip off leading slashes */
    if (*str == '\0') {
	/* Path of a lone slash */
	db_full_path_init(pp);
	return 0;
    }

    copy = bu_strdup(str);

    /* eliminate all trailing slashes */
    len = strlen(copy);
    while (copy[len - 1] == '/') {
	copy[len - 1] = '\0';
	--len;
    }

    cp = copy;
    while (*cp) {
	if ((slashp = strchr(cp, '/')) == NULL) break;
	nslash++;
	cp = slashp+1;
    }

    /* Make a path structure just big enough */
    pp->magic = DB_FULL_PATH_MAGIC;
    pp->fp_maxlen = pp->fp_len = nslash+1;
    pp->fp_names = (struct directory **)bu_malloc(
	    pp->fp_maxlen * sizeof(struct directory *),
	    "db_string_to_path path array");
    pp->fp_bool = (int *)bu_calloc(
	    pp->fp_maxlen, sizeof(int),
	    "db_string_to_path bool array");
    pp->fp_cinst = (int *)bu_calloc(
	    pp->fp_maxlen, sizeof(int),
	    "db_string_to_path cinst array");


    RT_CK_DBI(dbip);
    /* Build up path array */
    cp = copy;
    nslash = 0;
    while (*cp) {
	if ((slashp = strchr(cp, '/')) == NULL) {
	    /* Last element of string, has no trailing slash */
	    slashp = cp + strlen(cp) - 1;
	} else {
	    *slashp = '\0';
	}
	int bool_op = 2; // default to union
	int comb_instance_index = 0;
	dp = db_lookup(dbip, cp, LOOKUP_QUIET);
	if (dp == RT_DIR_NULL)
	    dp = dp_instance(&comb_instance_index, dbip, cp);
	if (dp == RT_DIR_NULL) {
	    bu_log("db_string_to_path() of '%s' failed on '%s'\n", str, cp);
	    ret = -1; /* FAILED */
	    /* Fall through, storing null dp in this location */
	} else {
	    // db_lookup isn't enough by itself - we also need to check that
	    // the parent comb (if there is one) does in fact contain the specified instance
	    // of the dp
	    if (nslash > 0 && pp->fp_names[nslash -1] != RT_DIR_NULL) {
		if (!db_comb_has_instance(&bool_op, dbip, pp->fp_names[nslash -1], dp, comb_instance_index)) {
		    // NOT falling through here, since we do have a dp but it's
		    // not under the parent
		    bu_log("db_string_to_path() failed: '%s@%d' is not a child of '%s'\n", dp->d_namep, comb_instance_index, pp->fp_names[nslash -1]->d_namep);
		    pp->fp_names[nslash++] = RT_DIR_NULL;
		    cp = slashp+1;
		    ret = -1; /* FAILED */
		    continue;
		}
	    }
	}
	DB_FULL_PATH_SET_BOOL(pp, nslash, bool_op);
	DB_FULL_PATH_SET_COMB_INST(pp, nslash, comb_instance_index);
	pp->fp_names[nslash++] = dp;
	cp = slashp+1;
    }
    BU_ASSERT(nslash == pp->fp_len);
    bu_free(copy, "db_string_to_path() duplicate string");
    return ret;
}

int
db_argv_to_path(struct db_full_path *pp, struct db_i *dbip, int argc, const char *const *argv)
{
    struct directory *dp;
    int ret = 0;
    int i;

    if (!pp)
	return -1;

    /* Make a path structure just big enough */
    pp->magic = DB_FULL_PATH_MAGIC;
    pp->fp_maxlen = pp->fp_len = argc;
    pp->fp_names = (struct directory **)bu_malloc(
	    pp->fp_maxlen * sizeof(struct directory *),
	    "db_argv_to_path path array");
    pp->fp_bool = (int *)bu_calloc(
	    pp->fp_maxlen, sizeof(int),
	    "db_argv_to_path bool array");
    pp->fp_cinst = (int *)bu_calloc(
	    pp->fp_maxlen, sizeof(int),
	    "db_argv_to_path cinst array");

    RT_CK_DBI(dbip);

    for (i = 0; i<argc; i++) {
	int bool_op = 2; // default to union
	int comb_instance_index = 0;
	dp = db_lookup(dbip, argv[i], LOOKUP_QUIET);
	if (dp == RT_DIR_NULL)
	    dp = dp_instance(&comb_instance_index, dbip, argv[i]);
	if (dp == RT_DIR_NULL) {
	    bu_log("db_fp_from_argv() failed on element %d='%s'\n", i, argv[i]);
	    ret = -1; /* FAILED */
	    /* Fall through, storing null dp in this location */
	} else {
	    // db_lookup isn't enough by itself - we also need to check that
	    // the parent comb (if there is one) does in fact contain the specified instance
	    // of the dp
	    if (i > 0 && pp->fp_names[i -1] != RT_DIR_NULL) {
		if (!db_comb_has_instance(&bool_op, dbip, pp->fp_names[i-1], dp, comb_instance_index)) {
		    // NOT falling through here, since we do have a dp but it's
		    // not under the parent
		    bu_log("db_fp_from_argv() failed: '%s@%d' is not a child of '%s'\n", dp->d_namep, comb_instance_index, pp->fp_names[i-1]->d_namep);
		    pp->fp_names[i] = RT_DIR_NULL;
		    ret = -1; /* FAILED */
		    continue;
		}
	    }
	}
	DB_FULL_PATH_SET_BOOL(pp, i, bool_op);
	DB_FULL_PATH_SET_COMB_INST(pp, i, comb_instance_index);
	pp->fp_names[i] = dp;
    }
    return ret;
}


void
db_path_to_vls(struct bu_vls *str, const struct db_full_path *pp)
{
    size_t i;

    if (!pp || !str)
	return;

    RT_CK_FULL_PATH(pp);

    for (i = 0; i < pp->fp_len; i++) {
	bu_vls_putc(str, '/');
	if (pp->fp_names[i]) {
	    if (pp->fp_cinst[i]) {
		bu_vls_printf(str, "%s@%d", pp->fp_names[i]->d_namep, pp->fp_cinst[i]);
	    } else {
		bu_vls_strcat(str, pp->fp_names[i]->d_namep);
	    }
	} else {
	    bu_vls_strcat(str, "**NULL**");
	}
    }
}

char *
db_path_to_string(const struct db_full_path *pp)
{
    char *buf = NULL;
    struct bu_vls vls = BU_VLS_INIT_ZERO;
    db_path_to_vls(&vls, pp);
    if (bu_vls_strlen(&vls))
	buf = bu_strdup(bu_vls_cstr(&vls));
    bu_vls_free(&vls);
    return buf;
}

/* If an instance other than the default was specified, we need
 * to walk the comb tree to get to that specific matrix.  Reuse
 * the _db_comb_instance logic. */
static int
_comb_instance_matrix(matp_t m, const struct db_i *dbip, struct directory *cdp, struct directory *dp, struct resource *resp, int itarget)
{
    RT_CK_DBI(dbip);

    if (!(cdp->d_flags & RT_DIR_COMB))
	return 0;

    struct rt_db_internal in;
    struct rt_comb_internal *comb;
    if (rt_db_get_internal(&in, cdp, dbip, NULL, resp) < 0)
	return 0;
    comb = (struct rt_comb_internal *)in.idb_ptr;
    RT_CK_COMB(comb);

    int bval = 0;
    int icnt = 0;
    int ret = _db_comb_instance(m, &icnt, &bval, OP_UNION, dbip, comb->tree, dp->d_namep, itarget);
    rt_db_free_internal(&in);
    return ret;
}

int
db_path_to_mat(
	struct db_i *dbip,
	struct db_full_path *pp,
	mat_t mat,
	int depth,
	struct resource *resp
	)
{
    if (!pp || !dbip || depth < 0 || !resp)
	return -1;

    mat_t all_m = MAT_INIT_IDN;
    mat_t cur_m = MAT_INIT_IDN;
    mat_t mtmp = MAT_INIT_IDN;
    for (size_t i = 1; i < pp->fp_len; i++) {
	if (depth && i + 1 > (size_t)depth)
	    break;
	struct directory *cdp = pp->fp_names[i-1];
	struct directory *dp = pp->fp_names[i];
	if (!cdp || !dp)
	    return -1;
	if (!_comb_instance_matrix(cur_m, dbip, cdp, dp, resp, pp->fp_cinst[i]))
	    return -1;
	bn_mat_mul(mtmp, all_m, cur_m);
	MAT_COPY(all_m, mtmp);
    }

    MAT_COPY(mat, all_m);
    return 0;
}


/* If an instance other than the default was specified, we need
 * to walk the comb tree to get to that specific matrix.  Reuse
 * the _db_comb_instance logic. */
static int
_comb_instance_bool_op(int *bval, const struct db_i *dbip, struct directory *cdp, struct directory *dp, struct resource *resp, int itarget)
{
    RT_CK_DBI(dbip);

    if (!(cdp->d_flags & RT_DIR_COMB))
	return 0;

    struct rt_db_internal in;
    struct rt_comb_internal *comb;
    if (rt_db_get_internal(&in, cdp, dbip, NULL, resp) < 0)
	return 0;
    comb = (struct rt_comb_internal *)in.idb_ptr;
    RT_CK_COMB(comb);

    int icnt = 0;
    int ret = _db_comb_instance(NULL, &icnt, bval, OP_UNION, dbip, comb->tree, dp->d_namep, itarget);
    rt_db_free_internal(&in);
    return ret;
}

// Eventually, this should replace the explicit fp_bool storage - that
// container is not automatically updated if the comb tree changes.  Doing
// it this way will result in either the current answer or the exposure of
// an out-of-date db_full_path.
int
db_fp_op(const struct db_full_path *pp,
	struct db_i *dbip,
	int depth,
	struct resource *resp)
{
    if (!pp || !dbip || depth < 0 || !resp)
	return OP_NOP;

    int r_op = OP_UNION;
    for (size_t i = 1; i < pp->fp_len; i++) {
	if (depth && i + 1 > (size_t)depth)
	    break;
	struct directory *cdp = pp->fp_names[i-1];
	struct directory *dp = pp->fp_names[i];
	if (!cdp || !dp)
	    return OP_NOP;
	int c_op;
	if (!_comb_instance_bool_op(&c_op, dbip, cdp, dp, resp, pp->fp_cinst[i]))
	    return OP_NOP;
	if (c_op == OP_INTERSECT && r_op != OP_SUBTRACT)
	    r_op = OP_INTERSECT;
	if (c_op == OP_SUBTRACT)
	    r_op = OP_SUBTRACT;
    }

    return r_op;
}

/** @} */
// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8

