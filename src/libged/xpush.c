/*                         X P U S H . C
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
/** @file libged/xpush.c
 *
 * The xpush command.
 *
 */

#include "common.h"

#include <string.h>
#include "bio.h"

#include "cmd.h"

#include "./ged_private.h"


struct object_use
{
    struct bu_list l;
    struct directory *dp;
    mat_t xform;
    int used;
};


/**
 *
 *
 */
static void
increment_uses(struct db_i *UNUSED(db_ip), struct directory *dp, genptr_t UNUSED(ptr))
{
    RT_CK_DIR(dp);

    dp->d_uses++;
}


/**
 *
 *
 */
static void
increment_nrefs(struct db_i *UNUSED(db_ip), struct directory *dp, genptr_t UNUSED(ptr))
{
    RT_CK_DIR(dp);

    dp->d_nref++;
}


/**
 *
 *
 */
static void
Free_uses(struct db_i *dbip)
{
    int i;

    for (i=0; i<RT_DBNHASH; i++) {
	struct directory *dp;
	struct object_use *use;

	for (dp=dbip->dbi_Head[i]; dp!=RT_DIR_NULL; dp=dp->d_forw) {
	    if (!(dp->d_flags & (RT_DIR_SOLID | RT_DIR_COMB)))
		continue;

	    while (BU_LIST_NON_EMPTY(&dp->d_use_hd)) {
		use = BU_LIST_FIRST(object_use, &dp->d_use_hd);
		if (!use->used) {
		    if (use->dp->d_un.file_offset >= 0) {
			/* was written to disk */
			db_delete(dbip, use->dp);
		    }
		    db_dirdelete(dbip, use->dp);
		}
		BU_LIST_DEQUEUE(&use->l);
		bu_free((genptr_t)use, "Free_uses: use");
	    }

	}
    }

}


/**
 *
 *
 */
static void
Make_new_name(struct db_i *dbip,
	      struct directory *dp,
	      genptr_t ptr)
{
    struct object_use *use;
    int use_no;
    int digits;
    int suffix_start;
    int name_length;
    int j;
    char format_v4[50], format_v5[50];
    struct bu_vls name_v5;
    char name_v4[NAMESIZE+1];
    char *name;
    struct ged *gedp;

    /* only one use and not referenced elsewhere, nothing to do */
    if (dp->d_uses < 2 && dp->d_uses == dp->d_nref)
	return;

    /* check if already done */
    if (BU_LIST_NON_EMPTY(&dp->d_use_hd))
	return;

    gedp = (struct ged *)ptr;

    digits = log10((double)dp->d_uses) + 2.0;
    snprintf(format_v5, 50, "%%s_%%0%dd", digits);
    snprintf(format_v4, 50, "_%%0%dd", digits);

    name_length = (int)strlen(dp->d_namep);
    if (name_length + digits + 1 > NAMESIZE - 1)
	suffix_start = NAMESIZE - digits - 2;
    else
	suffix_start = name_length;

    bu_vls_init(&name_v5);

    j = 0;
    for (use_no=0; use_no<dp->d_uses; use_no++) {
	j++;
	use = (struct object_use *)bu_malloc(sizeof(struct object_use), "Make_new_name: use");

	/* set xform for this object_use to all zeros */
	MAT_ZERO(use->xform);
	use->used = 0;
	if (db_version(dbip) < 5) {
	    NAMEMOVE(dp->d_namep, name_v4);
	    name_v4[NAMESIZE] = '\0';                /* ensure null termination */
	}

	/* Add an entry for the original at the end of the list
	 * This insures that the original will be last to be modified
	 * If original were modified earlier, copies would be screwed-up
	 */
	if (use_no == dp->d_uses-1 && dp->d_uses == dp->d_nref)
	    use->dp = dp;
	else {
	    if (db_version(dbip) < 5) {
		snprintf(&name_v4[suffix_start], NAMESIZE-suffix_start, format_v4, j);
		name = name_v4;
	    } else {
		bu_vls_trunc(&name_v5, 0);
		bu_vls_printf(&name_v5, format_v5, dp->d_namep, j);
		name = bu_vls_addr(&name_v5);
	    }

	    /* Insure that new name is unique */
	    while (db_lookup(dbip, name, 0) != RT_DIR_NULL) {
		j++;
		if (db_version(dbip) < 5) {
		    snprintf(&name_v4[suffix_start], NAMESIZE-suffix_start, format_v4, j);
		    name = name_v4;
		} else {
		    bu_vls_trunc(&name_v5, 0);
		    bu_vls_printf(&name_v5, format_v5, dp->d_namep, j);
		    name = bu_vls_addr(&name_v5);
		}
	    }

	    /* Add new name to directory */
	    use->dp = db_diradd(dbip, name, RT_DIR_PHONY_ADDR, 0, dp->d_flags, (genptr_t)&dp->d_minor_type);
	    if (use->dp == RT_DIR_NULL) {
		bu_vls_printf(gedp->ged_result_str, "\nAn error has occured while adding a new object to the database.\n"); \
		return;
	    }
	}

	/* Add new directory pointer to use list for this object */
	BU_LIST_INSERT(&dp->d_use_hd, &use->l);
    }

    bu_vls_free(&name_v5);
}


/* Do_copy_membs() needs the forward declaration due to a cyclic dependency */
static struct directory *Copy_object(struct ged *gedp, struct directory *dp, mat_t xform);

/**
 *
 *
 */
static void
Do_copy_membs(struct db_i *dbip, struct rt_comb_internal *UNUSED(comb), union tree *comb_leaf, genptr_t user_ptr1, genptr_t user_ptr2, genptr_t UNUSED(user_ptr3))
{
    struct directory *dp;
    struct directory *dp_new;
    mat_t new_xform;
    matp_t xform;
    struct ged *gedp;

    RT_CK_DBI(dbip);
    RT_CK_TREE(comb_leaf);

    if ((dp=db_lookup(dbip, comb_leaf->tr_l.tl_name, LOOKUP_QUIET)) == RT_DIR_NULL)
	return;

    xform = (matp_t)user_ptr1;
    gedp = (struct ged *)user_ptr2;

    /* apply transform matrix for this arc */
    if (comb_leaf->tr_l.tl_mat) {
	bn_mat_mul(new_xform, xform, comb_leaf->tr_l.tl_mat);
    } else {
	MAT_COPY(new_xform, xform);
    }

    /* Copy member with current tranform matrix */
    if ((dp_new=Copy_object(gedp, dp, new_xform)) == RT_DIR_NULL) {
	bu_vls_printf(gedp->ged_result_str, "Failed to copy object %s", dp->d_namep);
	return;
    }

    /* replace member name with new copy */
    bu_free(comb_leaf->tr_l.tl_name, "comb_leaf->tr_l.tl_name");
    comb_leaf->tr_l.tl_name = bu_strdup(dp_new->d_namep);

    /* make transform for this arc the identity matrix */
    if (!comb_leaf->tr_l.tl_mat) {
	comb_leaf->tr_l.tl_mat = (matp_t)bu_malloc(sizeof(mat_t), "tl_mat");
    }
    MAT_IDN(comb_leaf->tr_l.tl_mat);
}


/**
 *
 *
 */
static struct directory *
Copy_solid(struct ged *gedp,
	   struct directory *dp,
	   mat_t xform)
{
    struct directory *found;
    struct rt_db_internal sol_int;
    struct object_use *use;

    RT_CK_DIR(dp);

    if (!(dp->d_flags & RT_DIR_SOLID)) {
	bu_vls_printf(gedp->ged_result_str, "Copy_solid: %s is not a solid!!!!\n", dp->d_namep);
	return RT_DIR_NULL;
    }

    /* If no transformation is to be applied, just use the original */
    if (bn_mat_is_identity(xform)) {
	/* find original in the list */
	for (BU_LIST_FOR (use, object_use, &dp->d_use_hd)) {
	    if (use->dp == dp && use->used == 0) {
		use->used = 1;
		return dp;
	    }
	}
    }

    /* Look for a copy that already has this transform matrix */
    for (BU_LIST_FOR (use, object_use, &dp->d_use_hd)) {
	if (bn_mat_is_equal(xform, use->xform, &gedp->ged_wdbp->wdb_tol)) {
	    /* found a match, no need to make another copy */
	    use->used = 1;
	    return use->dp;
	}
    }

    /* get a fresh use */
    found = RT_DIR_NULL;
    for (BU_LIST_FOR (use, object_use, &dp->d_use_hd)) {
	if (use->used)
	    continue;

	found = use->dp;
	use->used = 1;
	MAT_COPY(use->xform, xform);
	break;
    }

    if (found == RT_DIR_NULL && dp->d_nref == 1 && dp->d_uses == 1) {
	/* only one use, take it */
	found = dp;
    }

    if (found == RT_DIR_NULL) {
	bu_vls_printf(gedp->ged_result_str, "Ran out of uses for solid %s\n", dp->d_namep);
	return RT_DIR_NULL;
    }

    if (rt_db_get_internal(&sol_int, dp, gedp->ged_wdbp->dbip, xform, &rt_uniresource) < 0) {
	bu_vls_printf(gedp->ged_result_str, "Cannot import solid %s\n", dp->d_namep);
	return RT_DIR_NULL;
    }

    RT_CK_DB_INTERNAL(&sol_int);
    if (rt_db_put_internal(found, gedp->ged_wdbp->dbip, &sol_int, &rt_uniresource) < 0) {
	bu_vls_printf(gedp->ged_result_str, "Cannot write copy solid (%s) to database\n", found->d_namep);
	return RT_DIR_NULL;
    }

    return found;
}


/**
 *
 *
 */
static struct directory *
Copy_comb(struct ged *gedp,
	  struct directory *dp,
	  mat_t xform)
{
    struct object_use *use;
    struct directory *found;
    struct rt_db_internal intern;
    struct rt_comb_internal *comb;

    RT_CK_DIR(dp);

    /* Look for a copy that already has this transform matrix */
    for (BU_LIST_FOR (use, object_use, &dp->d_use_hd)) {
	if (bn_mat_is_equal(xform, use->xform, &gedp->ged_wdbp->wdb_tol)) {
	    /* found a match, no need to make another copy */
	    use->used = 1;
	    return use->dp;
	}
    }

    /* if we can't get records for this combination, just leave it alone */
    if (rt_db_get_internal(&intern, dp, gedp->ged_wdbp->dbip, (fastf_t *)NULL, &rt_uniresource) < 0)
	return dp;
    comb = (struct rt_comb_internal *)intern.idb_ptr;

    /* copy members */
    if (comb->tree)
	db_tree_funcleaf(gedp->ged_wdbp->dbip, comb, comb->tree, Do_copy_membs,
			 (genptr_t)xform, (genptr_t)gedp, (genptr_t)0);

    /* Get a use of this object */
    found = RT_DIR_NULL;
    for (BU_LIST_FOR (use, object_use, &dp->d_use_hd)) {
	/* Get a fresh use of this object */
	if (use->used)
	    continue;	/* already used */
	found = use->dp;
	use->used = 1;
	MAT_COPY(use->xform, xform);
	break;
    }

    if (found == RT_DIR_NULL && dp->d_nref == 1 && dp->d_uses == 1) {
	/* only one use, so take original */
	found = dp;
    }

    if (found == RT_DIR_NULL) {
	bu_vls_printf(gedp->ged_result_str, "Ran out of uses for combination %s\n", dp->d_namep);
	return RT_DIR_NULL;
    }

    if (rt_db_put_internal(found, gedp->ged_wdbp->dbip, &intern, &rt_uniresource) < 0) {
	bu_vls_printf(gedp->ged_result_str, "rt_db_put_internal failed for %s\n", dp->d_namep);
	rt_db_free_internal(&intern);
	return RT_DIR_NULL;
    }

    return found;
}


/**
 *
 *
 */
static struct directory *
Copy_object(struct ged *gedp,
	    struct directory *dp,
	    mat_t xform)
{
    RT_CK_DIR(dp);

    if (dp->d_flags & RT_DIR_SOLID)
	return Copy_solid(gedp, dp, xform);
    else
	return Copy_comb(gedp, dp, xform);
}


/**
 *
 *
 */
static void
Do_ref_incr(struct db_i *dbip, struct rt_comb_internal *UNUSED(comb), union tree *comb_leaf, genptr_t UNUSED(user_ptr1), genptr_t UNUSED(user_ptr2), genptr_t UNUSED(user_ptr3))
{
    struct directory *dp;

    RT_CK_DBI(dbip);
    RT_CK_TREE(comb_leaf);

    if ((dp = db_lookup(dbip, comb_leaf->tr_l.tl_name, LOOKUP_QUIET)) == RT_DIR_NULL)
	return;

    dp->d_nref++;
}


static struct directory *Copy_object(struct ged *gedp, struct directory *dp, fastf_t *xform);

static void Do_ref_incr(struct db_i *dbip, struct rt_comb_internal *comb, union tree *comb_leaf, genptr_t user_ptr1, genptr_t user_ptr2, genptr_t user_ptr3);


int
ged_xpush(struct ged *gedp, int argc, const char *argv[])
{
    struct directory *old_dp;
    struct rt_db_internal intern;
    struct rt_comb_internal *comb;
    struct bu_ptbl tops;
    mat_t xform;
    int i;
    static const char *usage = "object";

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
    if ((old_dp = db_lookup(gedp->ged_wdbp->dbip,  argv[1], LOOKUP_NOISY)) == RT_DIR_NULL)
	return GED_ERROR;

    if (old_dp->d_flags & RT_DIR_SOLID) {
	bu_log("Attempt to xpush a primitive, aborting.\n");
	return GED_ERROR;
    }

    /* Initialize use and reference counts of all directory entries */
    for (i=0; i<RT_DBNHASH; i++) {
	struct directory *dp;

	for (dp=gedp->ged_wdbp->dbip->dbi_Head[i]; dp!=RT_DIR_NULL; dp=dp->d_forw) {
	    if (!(dp->d_flags & (RT_DIR_SOLID | RT_DIR_COMB)))
		continue;

	    dp->d_uses = 0;
	    dp->d_nref = 0;
	}
    }

    /* Count uses in the tree being pushed (updates dp->d_uses) */
    db_functree(gedp->ged_wdbp->dbip, old_dp, increment_uses, increment_uses, &rt_uniresource, NULL);

    /* Do a simple reference count to find top level objects */
    for (i=0; i<RT_DBNHASH; i++) {
	struct directory *dp;

	for (dp=gedp->ged_wdbp->dbip->dbi_Head[i]; dp!=RT_DIR_NULL; dp=dp->d_forw) {
	    if (dp->d_flags & RT_DIR_SOLID)
		continue;

	    if (!(dp->d_flags & (RT_DIR_SOLID | RT_DIR_COMB)))
		continue;

	    if (rt_db_get_internal(&intern, dp, gedp->ged_wdbp->dbip, (fastf_t *)NULL, &rt_uniresource) < 0) {
		bu_vls_printf(gedp->ged_result_str, "Database read error, aborting.\n");
		return GED_ERROR;
	    }
	    comb = (struct rt_comb_internal *)intern.idb_ptr;
	    if (comb->tree)
		db_tree_funcleaf(gedp->ged_wdbp->dbip, comb, comb->tree, Do_ref_incr,
				 (genptr_t)NULL, (genptr_t)NULL, (genptr_t)NULL);
	    rt_db_free_internal(&intern);
	}
    }

    /* anything with zero references is a tree top */
    bu_ptbl_init(&tops, 0, "tops for xpush");
    for (i=0; i<RT_DBNHASH; i++) {
	struct directory *dp;

	for (dp=gedp->ged_wdbp->dbip->dbi_Head[i]; dp!=RT_DIR_NULL; dp=dp->d_forw) {
	    if (dp->d_flags & RT_DIR_SOLID)
		continue;

	    if (!(dp->d_flags & (RT_DIR_SOLID | RT_DIR_COMB)))
		continue;

	    if (dp->d_nref == 0)
		bu_ptbl_ins(&tops, (long *)dp);
	}
    }

    /* now re-zero the reference counts */
    for (i=0; i<RT_DBNHASH; i++) {
	struct directory *dp;

	for (dp=gedp->ged_wdbp->dbip->dbi_Head[i]; dp!=RT_DIR_NULL; dp=dp->d_forw) {
	    if (!(dp->d_flags & (RT_DIR_SOLID | RT_DIR_COMB)))
		continue;

	    dp->d_nref = 0;
	}
    }

    /* accurately count references in entire model */
    for (i=0; i<BU_PTBL_END(&tops); i++) {
	struct directory *dp;

	dp = (struct directory *)BU_PTBL_GET(&tops, i);
	db_functree(gedp->ged_wdbp->dbip, dp, increment_nrefs, increment_nrefs, &rt_uniresource, NULL);
    }

    /* Free list of tree-tops */
    bu_ptbl_free(&tops);

    /* Make new names */
    db_functree(gedp->ged_wdbp->dbip, old_dp, Make_new_name, Make_new_name, &rt_uniresource, (genptr_t)gedp);

    MAT_IDN(xform);

    /* Make new objects */
    if (rt_db_get_internal(&intern, old_dp, gedp->ged_wdbp->dbip, (fastf_t *)NULL, &rt_uniresource) < 0) {
	bu_vls_printf(gedp->ged_result_str, "ERROR: cannot load %s feom the database!!!\n", old_dp->d_namep);
	bu_vls_printf(gedp->ged_result_str, "\tNothing has been changed!!\n");
	Free_uses(gedp->ged_wdbp->dbip);
	return GED_ERROR;
    }

    comb = (struct rt_comb_internal *)intern.idb_ptr;
    if (!comb->tree) {
	Free_uses(gedp->ged_wdbp->dbip);
	return GED_OK;
    }

    db_tree_funcleaf(gedp->ged_wdbp->dbip, comb, comb->tree, Do_copy_membs,
		     (genptr_t)xform, (genptr_t)gedp, (genptr_t)0);

    if (rt_db_put_internal(old_dp, gedp->ged_wdbp->dbip, &intern, &rt_uniresource) < 0) {
	bu_vls_printf(gedp->ged_result_str, "rt_db_put_internal failed for %s\n", old_dp->d_namep);
	rt_db_free_internal(&intern);
	Free_uses(gedp->ged_wdbp->dbip);
	return GED_ERROR;
    }

    /* Free use lists and delete unused directory entries */
    Free_uses(gedp->ged_wdbp->dbip);
    return GED_OK;
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
