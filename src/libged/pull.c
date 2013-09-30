/*                         P U L L . C
 * BRL-CAD
 *
 * Copyright (c) 2013 United States Government as represented by
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
/** @file libged/push.c
 * The pull command.
 * This is the Pull command which pulls the matrix transformations of an object
 * up the CSG Tree.
 */


#include "common.h"

#include <stdlib.h>
#include <string.h>
#include "bio.h"

#include "bu.h"
#include "bn.h"
#include "cmd.h"

#include "./ged_private.h"



void
pull_comb(struct db_i *dbip,
	  struct directory *dp,
	  genptr_t mp);


/* This restores the matrix transformation at a combination by taking leaf matrix transformations, inverting
 * and storing the changes at the combinations.
 */
static void
pull_comb_mat(struct db_i *dbip, struct rt_comb_internal *UNUSED(comb), union tree *comb_leaf, matp_t mp, genptr_t UNUSED(usr_ptr2),
	      genptr_t UNUSED(usr_ptr3), genptr_t UNUSED(usr_ptr4))
{
    struct directory *dp;
    mat_t inv_mat;
    matp_t mat = mp;

    RT_CK_DBI(dbip);
    RT_CK_TREE(comb_leaf);

    if ((dp = db_lookup(dbip, comb_leaf->tr_l.tl_name, LOOKUP_NOISY)) == RT_DIR_NULL)
	return;

    if (!comb_leaf->tr_l.tl_mat) {
	comb_leaf->tr_l.tl_mat = (matp_t)bu_malloc(sizeof(mat_t), "tl_mat");
	MAT_COPY(comb_leaf->tr_l.tl_mat, mat);

	return;
    }

    /* invert the matrix transformation of the leaf and store new matrix  at comb */
    bn_mat_inverse(inv_mat, comb_leaf->tr_l.tl_mat );

    /* multiply inverse and store at combination */
    bn_mat_mul2(inv_mat,mat);
    MAT_COPY(comb_leaf->tr_l.tl_mat, mat);
    pull_comb(dbip, dp, mp);
}


/* pull_comb(): This routine enters a comb restoring the matrix transformation at the combination
 *              calls and calls pull_comb_mat() which updates current matrix transformation and moves up tree.
 * Note:        the generic pointer points to a matrix if successful or a string if unsuccessful.
 */
void
pull_comb(struct db_i *dbip,
	  struct directory *dp,
	  genptr_t mp)
{
    struct rt_db_internal intern;
    struct rt_comb_internal *comb;
    matp_t mat = (matp_t)mp;
    mat_t m;
    mat_t invMat;

    if (dp->d_flags & RT_DIR_SOLID)
	return;
    if (rt_db_get_internal(&intern, dp, dbip, m, &rt_uniresource) < 0) {
	bu_log("Database read error, aborting\n");
	return;
    }

    comb = (struct rt_comb_internal *)intern.idb_ptr;

    /* checks if matrix pointer is valid */
    if (mat == NULL) {
	mat = (matp_t)bu_malloc(sizeof(mat_t), "cur_mat");
	MAT_IDN(mat);
    }

    bn_mat_inverse(invMat, mat);
    bn_mat_mul2(mat,m);
    MAT_COPY(mat, m);/* updates current matrix pointer */

    if (comb->tree) {
	db_tree_funcleaf(dbip, comb, comb->tree, pull_comb_mat,
			 &m, (genptr_t)NULL, (genptr_t)NULL, (genptr_t)NULL);

	if (rt_db_put_internal(dp, dbip, &intern, &rt_uniresource) < 0) {
	    bu_log("Cannot write modified combination (%s) to database\n", dp->d_namep);
	    return;
	}
    }
}


/* routine takes the maximum and minimum points from the AABB and determines the translation matrix
 * which moves the centrepoint to the origin and moves the primitive by the its inverse
 * before pulling the translation.
 */
void
translate(matp_t matrix, mat_t tm, point_t min, point_t max)
{
    vect_t c_pt = VINIT_ZERO; /* centre point */
    mat_t t_mat = MAT_INIT_ZERO; /* temp matrix */
    vect_t t_vec = VINIT_ZERO; /* translation vector */

    /* computes the centrepoint from both minimum and maximum point
     * following this algorithm: cen[i] = bbx_min[i] + bbx_max[i]) / 2.0
     */
    VADD2(c_pt, max, min);
    VSCALE(c_pt, c_pt, 1/2);

    /* translates the centrepoint to the origin. and computes the inverse of this transformation which will be applied to the primitive */
    VREVERSE(t_vec, c_pt);
    MAT_DELTAS_VEC(t_mat, t_vec);
    MAT_DELTAS_VEC(matrix, t_vec);/* pulls translation component of primitive */

    /* copies transformation matrix whose inverse restores primitive and bbox */
    MAT_COPY(tm, t_mat);

    return;
}


/**
 * P U L L _ L E A F
 *
 * @brief
 * This routine takes the internal database representation of a leaf node or
 * primitive object and builds  a matrix transformation, closest to this node's,
 * sets these values to default and returns matrix.
 * Fixme: This routine only pulls translations up. So additional support for rotations/scale
 *        would be needed for further purposes.
 */
static void
pull_leaf(struct db_i *dbip, struct directory *dp, genptr_t mp)
{
    struct rt_db_internal intern;
    struct bn_tol tol;
    struct resource *resp;
    point_t min;             /* minimum point of bbox */
    point_t max;             /* maximum point of bbox */
    matp_t mat = (matp_t)mp; /* current transformation matrix */
    mat_t matrix, invXform;

    BN_TOL_INIT(&tol); /* initializes the tolerance */
    resp = &rt_uniresource;
    rt_init_resource( &rt_uniresource, 0, NULL );

    if (mat == NULL) {
	mat = (matp_t)bu_malloc(sizeof(mat_t), "cur_mat");
        MAT_IDN(mat);
    }

    if (!(dp->d_flags & RT_DIR_SOLID))
        return;
    if (rt_db_get_internal(&intern, dp, dbip, mat, &rt_uniresource) < 0) {
        bu_vls_printf(mp, "Database read error, aborting\n");
        return;
    }

    MAT_IDN(mat);
    MAT_IDN(matrix);

    /* this computes the AABB bounding box of the leaf object
     * using the bbox call back routine in its rt_functab table.
     * it then passes the minimum and maximum point to translate()
     * which then extracts the translation.
     */
    (intern.idb_meth)->ft_bbox(&intern, &min, &max, &tol);

    /* pulls primitive translation matrix copying inverse transformation to restore primitive and bbox */
    translate(mat,matrix, min, max);
    bn_mat_inverse(invXform, matrix);/* computes the inverse of transformation matrix. */

    (intern.idb_meth)->ft_xform(&intern, invXform, &intern, 0, dbip, resp);/* restores the primitive */

    return;
}


int
ged_pull(struct ged *gedp, int argc, const char *argv[])
{
    struct directory *dp;
    struct resource *resp;
    mat_t mat;
    int c;
    static const char *usage = "object";

    resp = &rt_uniresource;
    rt_init_resource( &rt_uniresource, 0, NULL );

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_READ_ONLY(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    if (argc != 2) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    /* get directory pointer for arg */
    if ((dp = db_lookup(gedp->ged_wdbp->dbip,  argv[1], LOOKUP_NOISY)) == RT_DIR_NULL)
	return GED_ERROR;

    /* Checks whether the object is a primitive.*/
    if (dp->d_flags & RT_DIR_SOLID) {
	bu_log("Attempt to pull primitive, aborting.\n");
	return GED_ERROR;
    }

    /* Parse options */
    bu_optind = 1;	/* re-init bu_getopt() */
    while ((c = bu_getopt(argc, (char * const *)argv, "d")) != -1) {
	switch (c) {
	   case 'd':
		RTG.debug |= DEBUG_TREEWALK;
		break;
	  case '?':
	  default:
		bu_vls_printf(gedp->ged_result_str, "ged_pull: usage pull [-d] root \n");
		break;
	}
    }

    /*
     * uses a no frills walk routine recursively moving up the tree
     * from the leaves performing the necessary matrix transformations moving up
     * right to the the head of the tree pulling objects.
     * All new changes are immediately written to database
     */
    db_functree(gedp->ged_wdbp->dbip, dp, pull_comb, pull_leaf, resp, &mat);

   return  GED_OK;
}


/** @} */
/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
