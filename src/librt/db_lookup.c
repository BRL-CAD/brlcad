/*                     D B _ L O O K U P . C
 * BRL-CAD
 *
 * Copyright (c) 1988-2022 United States Government as represented by
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
/** @file librt/db_lookup.c
 *
 * v4/v5 database directory routines
 *
 */

#include "common.h"

#include <string.h>
#include "bio.h"

#include "vmath.h"
#include "bu/vls.h"
#include "rt/db4.h"
#include "raytrace.h"
#include "librt_private.h"

int
db_is_directory_non_empty(const struct db_i *dbip)
{
    int i;

    RT_CK_DBI(dbip);

    for (i = 0; i < RT_DBNHASH; i++) {
	if (dbip->dbi_Head[i] != RT_DIR_NULL)
	    return 1;
    }
    return 0;
}


size_t
db_directory_size(const struct db_i *dbip)
{
    struct directory *dp;
    size_t count = 0;
    size_t i;

    RT_CK_DBI(dbip);

    for (i = 0; i < RT_DBNHASH; i++) {
	for (dp = dbip->dbi_Head[i]; dp != RT_DIR_NULL; dp = dp->d_forw)
	    count++;
    }
    return count;
}


void
db_ck_directory(const struct db_i *dbip)
{
    struct directory *dp;
    int i;

    RT_CK_DBI(dbip);

    for (i = 0; i < RT_DBNHASH; i++) {
	for (dp = dbip->dbi_Head[i]; dp != RT_DIR_NULL; dp = dp->d_forw)
	    RT_CK_DIR(dp);
    }
}


int
db_dirhash(const char *str)
{
    const unsigned char *s = (unsigned char *)str;
    size_t sum = 0;
    int i = 1;

    /* sanity */
    if (!str)
	return 0;

    /* BSD name hashing starts i=0, discarding first char.  why? */
    while (*s)
	sum += (size_t)*s++ * i++;

    return RT_DBHASH(sum);
}


int
db_dircheck(struct db_i *dbip,
	    struct bu_vls *ret_name,
	    int noisy,
	    struct directory ***headp)
{
    struct directory *dp;
    char *cp = bu_vls_addr(ret_name);
    char n0 = cp[0];
    char n1 = cp[1];

    /* Compute hash only once (almost always the case) */
    *headp = &(dbip->dbi_Head[db_dirhash(cp)]);

    for (dp = **headp; dp != RT_DIR_NULL; dp=dp->d_forw) {
	char *this_obj;
	if (n0 == *(this_obj=dp->d_namep)  &&	/* speed */
	    n1 == this_obj[1]  &&			/* speed */
	    BU_STR_EQUAL(cp, this_obj)) {
	    /* Name exists in directory already */
	    int c;

	    bu_vls_strcpy(ret_name, "A_");
	    bu_vls_strcat(ret_name, this_obj);
	    cp = bu_vls_addr(ret_name);

	    for (c = 'A'; c <= 'Z'; c++) {
		*cp = c;
		if (db_lookup(dbip, cp, noisy) == RT_DIR_NULL)
		    break;
	    }
	    if (c > 'Z') {
		bu_log("db_dircheck: Duplicate of name '%s', ignored\n",
		       cp);
		return -1;	/* fail */
	    }
	    bu_log("db_dircheck: Duplicate of '%s', given temporary name '%s'\n",
		   cp+2, cp);

	    /* no need to recurse, simply recompute the hash and break */
	    *headp = &(dbip->dbi_Head[db_dirhash(cp)]);
	    break;
	}
    }

    return 0;	/* success */
}


struct directory *
db_lookup(const struct db_i *dbip, const char *name, int noisy)
{
    int is_path = 0;
    const char *pc = name;
    struct directory *dp = RT_DIR_NULL;
    char n0;
    char n1;

    /* No string, no lookup */
    if (UNLIKELY(!name || name[0] == '\0')) {
	if (UNLIKELY(noisy || RT_G_DEBUG&RT_DEBUG_DB)) {
	    bu_log("db_lookup received NULL or empty name\n");
	}
	return RT_DIR_NULL;
    }
    if (UNLIKELY(!dbip)) {
	return RT_DIR_NULL;
    }


    n0 = name[0];
    n1 = name[1];

    RT_CK_DBI(dbip);

    dp = dbip->dbi_Head[db_dirhash(name)];
    for (; dp != RT_DIR_NULL; dp=dp->d_forw) {
	char *this_obj;

	/* first two checks are for speed */
	if ((n0 == *(this_obj=dp->d_namep)) && (n1 == this_obj[1]) && (BU_STR_EQUAL(name, this_obj))) {
	    if (UNLIKELY(RT_G_DEBUG&RT_DEBUG_DB)) {
		bu_log("db_lookup(%s) %p\n", name, (void *)dp);
	    }
	    return dp;
	}
    }

    /* Anything with a forward slash is potentially a path, rather than an object
     * name.  The full path lookup is potentially more expensive, so only do it
     * when we need to.
     *
     * TODO - could we ultimately consolidate db_lookup and db_string_to_path
     * somehow - maybe have db_lookup take an optional parameter for a full
     * path, and always return the directory pointer?
     */
    while(*pc != '\0' && !is_path) {
	is_path = (*pc == '/');
	pc++;
    }

    if (is_path) {

	/* If we have a valid path, return the dp of the current directory
	 * (which is the leaf node in the path */
	struct db_full_path fp;
	dp = RT_DIR_NULL;

	/* db_string_to_path does the hard work */
	db_full_path_init(&fp);
	if (!db_string_to_path(&fp, dbip, name)) {
	    dp = DB_FULL_PATH_CUR_DIR(&fp);
	}
	db_free_full_path(&fp);

	/* If we succeeded as a full path return.  Otherwise, let the normal
	 * lookup proceed to find out if we have a diabolical case where a
	 * forward slash ended up in a name.  It's not supposed to, but it has
	 * been observed in the wild. */
	if (dp != RT_DIR_NULL) {
	    return dp;
	}
    }

    if (noisy || RT_G_DEBUG&RT_DEBUG_DB) {
	bu_log("db_lookup(%s) failed: %s does not exist\n", name, name);
    }

    return RT_DIR_NULL;

}


struct directory *
db_diradd(struct db_i *dbip, const char *name, b_off_t laddr, size_t len, int flags, void *ptr)
{
    struct directory **headp;
    struct directory *dp;
    const char *tmp_ptr;
    struct bu_vls local = BU_VLS_INIT_ZERO;

    RT_CK_DBI(dbip);

    if (RT_G_DEBUG&RT_DEBUG_DB) {
	bu_log("db_diradd(dbip=%p, name='%s', addr=%jd, len=%zu, flags=0x%x, ptr=%p)\n",
	       (void *)dbip, name, (intmax_t)laddr, len, flags, ptr);
    }

    if (BU_STR_EMPTY(name)) {
	bu_log("db_diradd() object with empty name is illegal, ignored\n");
	return RT_DIR_NULL;
    }

    tmp_ptr = strchr(name, '/');
    if (tmp_ptr != NULL) {
	/* if this is a version 4 database and the offending char is beyond NAMESIZE
	 * then it is not really a problem
	 */
	if (db_version(dbip) < 5 && (tmp_ptr - name) < NAMESIZE) {
	    bu_log("db_diradd() object named '%s' is illegal, ignored\n", name);
	    return RT_DIR_NULL;
	}
    }

    if (db_version(dbip) < 5) {
	bu_vls_strncpy(&local, name, NAMESIZE);
    } else {
	/* must provide a valid minor type */
	if (!ptr) {
	    bu_log("WARNING: db_diradd() called with a null minor type pointer for object %s\nIgnoring %s\n", name, name);
	    bu_vls_free(&local);
	    return RT_DIR_NULL;
	}
	bu_vls_strcpy(&local, name);
    }
    if (db_dircheck(dbip, &local, 0, &headp) < 0) {
	bu_vls_free(&local);
	return RT_DIR_NULL;
    }

    /* 'name' not found in directory, add it */
    RT_GET_DIRECTORY(dp, &rt_uniresource);
    RT_CK_DIR(dp);
    RT_DIR_SET_NAMEP(dp, bu_vls_addr(&local));	/* sets d_namep */
    dp->d_addr = laddr;

    /* TODO: investigate removing exclusion of INMEM flag, added by
     * mike in c13285 when in-mem support was originally added
     */
    dp->d_flags = flags & ~(RT_DIR_INMEM);
    dp->d_len = len;
    dp->d_forw = *headp;
    BU_LIST_INIT(&dp->d_use_hd);
    *headp = dp;
    dp->d_animate = NULL;
    dp->d_nref = 0;
    dp->d_uses = 0;

    /* v4 geometry databases do not use d_major/minor_type */
    if (db_version(dbip) > 4) {
	dp->d_major_type = DB5_MAJORTYPE_BRLCAD;
	if (ptr)
	    dp->d_minor_type = *(unsigned char *)ptr;
	else
	    dp->d_minor_type = 0;
    } else {
	dp->d_major_type = 0;
	dp->d_minor_type = 0;
    }

    bu_vls_free(&local);

    if (BU_PTBL_IS_INITIALIZED(&dbip->dbi_changed_clbks)) {
	for (size_t i = 0; i < BU_PTBL_LEN(&dbip->dbi_changed_clbks); i++) {
	    struct dbi_changed_clbk *cb = (struct dbi_changed_clbk *)BU_PTBL_GET(&dbip->dbi_changed_clbks, i);
	    (*cb->f)(dbip, dp, 1, cb->u_data);
	}
    }

    return dp;
}


int
db_dirdelete(struct db_i *dbip, struct directory *dp)
{
    struct directory *findp;
    struct directory **headp;

    RT_CK_DBI(dbip);
    RT_CK_DIR(dp);

    headp = &(dbip->dbi_Head[db_dirhash(dp->d_namep)]);

    if (dp->d_flags & RT_DIR_INMEM) {
	if (dp->d_un.ptr != NULL)
	    bu_free(dp->d_un.ptr, "db_dirdelete() inmem ptr");
    }

    if (*headp == dp) {

	// If we've gotten this far, the dp is on its way out - call the change
	// callback first if defined, so the app can get information from the
	// dp before it is cleared.
	if (BU_PTBL_IS_INITIALIZED(&dbip->dbi_changed_clbks)) {
	    for (size_t i = 0; i < BU_PTBL_LEN(&dbip->dbi_changed_clbks); i++) {
		struct dbi_changed_clbk *cb = (struct dbi_changed_clbk *)BU_PTBL_GET(&dbip->dbi_changed_clbks, i);
		(*cb->f)(dbip, dp, 2, cb->u_data);
	    }
	}

	RT_DIR_FREE_NAMEP(dp);	/* frees d_namep */
	*headp = dp->d_forw;

	/* Put 'dp' back on the freelist */
	dp->d_forw = rt_uniresource.re_directory_hd;
	rt_uniresource.re_directory_hd = dp;
	return 0;
    }
    for (findp = *headp; findp != RT_DIR_NULL; findp = findp->d_forw) {
	if (findp->d_forw != dp)
	    continue;

	// If we've gotten this far, the dp is on its way out - call the change
	// callback first if defined, so the app can get information from the
	// dp before it is cleared.
	if (BU_PTBL_IS_INITIALIZED(&dbip->dbi_changed_clbks)) {
	    for (size_t i = 0; i < BU_PTBL_LEN(&dbip->dbi_changed_clbks); i++) {
		struct dbi_changed_clbk *cb = (struct dbi_changed_clbk *)BU_PTBL_GET(&dbip->dbi_changed_clbks, i);
		(*cb->f)(dbip, dp, 2, cb->u_data);
	    }
	}

	RT_DIR_FREE_NAMEP(dp);	/* frees d_namep */
	findp->d_forw = dp->d_forw;

	/* Put 'dp' back on the freelist */
	dp->d_forw = rt_uniresource.re_directory_hd;
	rt_uniresource.re_directory_hd = dp;
	return 0;
    }
    return -1;
}


int
db_rename(struct db_i *dbip, struct directory *dp, const char *newname)
{
    struct directory *findp;
    struct directory **headp;

    RT_CK_DBI(dbip);
    RT_CK_DIR(dp);

    /* Remove from linked list */
    headp = &(dbip->dbi_Head[db_dirhash(dp->d_namep)]);
    if (*headp == dp) {
	/* Was first on list, dequeue */
	*headp = dp->d_forw;
    } else {
	for (findp = *headp; findp != RT_DIR_NULL; findp = findp->d_forw) {
	    if (findp->d_forw != dp)
		continue;
	    /* Dequeue */
	    findp->d_forw = dp->d_forw;
	    goto out;
	}
	return -1;		/* ERROR: can't find */
    }

out:
    /* Effect new name */
    RT_DIR_FREE_NAMEP(dp);			/* frees d_namep */
    RT_DIR_SET_NAMEP(dp, newname);	/* sets d_namep */

    /* Add to new linked list */
    headp = &(dbip->dbi_Head[db_dirhash(newname)]);
    dp->d_forw = *headp;
    *headp = dp;
    return 0;
}


void
db_pr_dir(const struct db_i *dbip)
{
    const struct directory *dp;
    char *flags;
    int i;

    RT_CK_DBI(dbip);

    bu_log("db_pr_dir(%p):  Dump of directory for file %s [%s]\n",
	   (void *)dbip, dbip->dbi_filename,
	   dbip->dbi_read_only ? "READ-ONLY" : "Read/Write");

    bu_log("Title = %s\n", dbip->dbi_title);
    /* units ? */

    for (i = 0; i < RT_DBNHASH; i++) {
	for (dp = dbip->dbi_Head[i]; dp != RT_DIR_NULL; dp=dp->d_forw) {
	    if (dp->d_flags & RT_DIR_SOLID)
		flags = "SOL";
	    else if ((dp->d_flags & (RT_DIR_COMB|RT_DIR_REGION)) ==
		     (RT_DIR_COMB|RT_DIR_REGION))
		flags = "REG";
	    else if ((dp->d_flags & (RT_DIR_COMB|RT_DIR_REGION)) ==
		     RT_DIR_COMB)
		flags = "COM";
	    else
		flags = "Bad";
	    bu_log("%p %s %s=%jd len=%.5zu use=%.2ld nref=%.2ld %s",
		   (void *)dp,
		   flags,
		   dp->d_flags & RT_DIR_INMEM ? "  ptr " : "d_addr",
		   (intmax_t)dp->d_addr,
		   dp->d_len,
		   dp->d_uses,
		   dp->d_nref,
		   dp->d_namep);
	    if (dp->d_animate)
		bu_log(" anim=%p\n", (void *)dp->d_animate);
	    else
		bu_log("\n");
	}
    }
}


struct bu_ptbl *
db_lookup_by_attr(struct db_i *dbip, int dir_flags, struct bu_attribute_value_set *avs, int op)
{
    struct bu_attribute_value_set obj_avs;
    struct directory *dp;
    struct bu_ptbl *tbl;
    size_t match_count = 0;
    size_t attr_count;
    int i, j;
    int draw;

    RT_CK_DBI(dbip);

    if (avs) {
	BU_CK_AVS(avs);
	attr_count = avs->count;
    } else {
	attr_count = 0;
    }

    BU_ALLOC(tbl, struct bu_ptbl);
    bu_ptbl_init(tbl, 128, "wdb_get_by_attr ptbl_init");

    FOR_ALL_DIRECTORY_START(dp, dbip) {

	if ((dp->d_flags & dir_flags) == 0) continue;

	/* Skip phony entries */
	if (dp->d_addr == RT_DIR_PHONY_ADDR) continue;

	if (attr_count) {
	    bu_avs_init_empty(&obj_avs);
	    if (db5_get_attributes(dbip, &obj_avs, dp) < 0) {
		bu_log("ERROR: failed to get attributes for %s\n", dp->d_namep);
		return (struct bu_ptbl *)NULL;
	    }

	    draw = 0;
	    match_count = 0;
	    for (i = 0; (size_t)i < (size_t)avs->count; i++) {
		for (j = 0; (size_t)j < (size_t)obj_avs.count; j++) {
		    if (BU_STR_EQUAL(avs->avp[i].name, obj_avs.avp[j].name)) {
			if (BU_STR_EQUAL(avs->avp[i].value, obj_avs.avp[j].value)) {
			    if (op == 2) {
				draw = 1;
				break;
			    } else {
				match_count++;
			    }
			}
		    }
		}
		if (draw) break;
	    }

	    bu_avs_free(&obj_avs);
	} else {
	    draw = 1;
	}
	if (draw || match_count == attr_count) {
	    bu_ptbl_ins(tbl, (long *)dp);
	}
    } FOR_ALL_DIRECTORY_END;

    return tbl;
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
