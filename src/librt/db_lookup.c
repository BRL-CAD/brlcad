/*                     D B _ L O O K U P . C
 * BRL-CAD
 *
 * Copyright (c) 1988-2011 United States Government as represented by
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

#include <stdio.h>
#include <string.h>
#include "bio.h"

#include "vmath.h"
#include "db.h"
#include "raytrace.h"


/**
 * D B _ I S _ D I R E C T O R Y _ N O N _ E M P T Y
 *
 * Returns -
 * 0 if the in-memory directory is empty
 * 1 if the in-memory directory has entries,
 * which implies that a db_scan() has already been performed.
 */
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


/**
 * D B _ C K _ D I R E C T O R Y
 *
 * For debugging, ensure that all the linked-lists for the directory
 * structure are intact.
 */
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


/**
 * D B _ D I R H A S H
 *
 * Returns a hash index for a given string that corresponds with the
 * head of that string's hash chain.
 */
int
db_dirhash(const char *str)
{
    const unsigned char *s = (unsigned char *)str;
    long sum;
    int i;

    /* sanity */
    if (!str) {
	return 0;
    }

    sum = 0;
    /* BSD name hashing starts i=0, discarding first char.  why? */
    for (i=1; *s;)
	sum += *s++ * i++;

    return RT_DBHASH(sum);
}


/**
 * Name -
 * D B _ D I R C H E C K
 *
 * Description -
 * This routine ensures that ret_name is not already in the
 * directory. If it is, it tries a fixed number of times to
 * modify ret_name before giving up. Note - most of the time,
 * the hash for ret_name is computed once.
 *
 * Inputs -
 * dbip database instance pointer
 * ret_name the original name
 * noisy to blather or not
 *
 * Outputs -
 * ret_name the name to use
 * headp pointer to the first (struct directory *) in the bucket
 *
 * Returns -
 * 0 success
 * <0 fail
 */
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
	char *this;
	if (n0 == *(this=dp->d_namep)  &&	/* speed */
	    n1 == this[1]  &&			/* speed */
	    BU_STR_EQUAL(cp, this)) {
	    /* Name exists in directory already */
	    int c;

	    bu_vls_strcpy(ret_name, "A_");
	    bu_vls_strcat(ret_name, this);

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


/**
 * D B _ L O O K U P
 *
 * This routine takes a name and looks it up in the directory table.
 * If the name is present, a pointer to the directory struct element
 * is returned, otherwise NULL is returned.
 *
 * If noisy is non-zero, a print occurs, else only the return code
 * indicates failure.
 *
 * Returns -
 * struct directory if name is found
 * RT_DIR_NULL on failure
 */
struct directory *
db_lookup(const struct db_i *dbip, const char *name, int noisy)
{
    struct directory *dp;
    char n0;
    char n1;

    if (!name || name[0] == '\0') {
	if (noisy || RT_G_DEBUG&DEBUG_DB)
	    bu_log("db_lookup received NULL or empty name\n");
	return RT_DIR_NULL;
    }

    n0 = name[0];
    n1 = name[1];

    RT_CK_DBI(dbip);

    dp = dbip->dbi_Head[db_dirhash(name)];
    for (; dp != RT_DIR_NULL; dp=dp->d_forw) {
	char *this;

	/* first two checks are for speed */
	if ((n0 == *(this=dp->d_namep)) && (n1 == this[1]) && (BU_STR_EQUAL(name, this))) {
	    if (RT_G_DEBUG&DEBUG_DB)
		bu_log("db_lookup(%s) x%x\n", name, dp);
	    return dp;
	}
    }

    if (noisy || RT_G_DEBUG&DEBUG_DB)
	bu_log("db_lookup(%s) failed: %s does not exist\n", name, name);

    return RT_DIR_NULL;
}


/**
 * D B _ D I R A D D
 *
 * Add an entry to the directory.  Try to make the regular path
 * through the code as fast as possible, to speed up building the
 * table of contents.
 *
 * dbip is a pointer to a valid/opened database instance
 *
 * name is the string name of the object being added
 *
 * laddr is the offset into the file to the object
 *
 * len is the length of the object, number of db granules used
 *
 * flags are defined in raytrace.h (RT_DIR_SOLID, RT_DIR_COMB, RT_DIR_REGION,
 * RT_DIR_INMEM, etc) for db version 5, ptr is the minor_type
 * (non-null pointer to valid unsigned char code)
 *
 * an laddr of RT_DIR_PHONY_ADDR means that database storage has not
 * been allocated yet.
 */
struct directory *
db_diradd(struct db_i *dbip, const char *name, off_t laddr, size_t len, int flags, genptr_t ptr)
{
    struct directory **headp;
    struct directory *dp;
    char *tmp_ptr;
    struct bu_vls local;

    RT_CK_DBI(dbip);

    if (RT_G_DEBUG&DEBUG_DB) {
	bu_log("db_diradd(dbip=0x%x, name='%s', addr=0x%x, len=%zu, flags=0x%x, ptr=0x%x)\n",
	       dbip, name, laddr, len, flags, ptr);
    }

    if ((tmp_ptr=strchr(name, '/')) != NULL) {
	/* if this is a version 4 database and the offending char is beyond NAMESIZE
	 * then it is not really a problem
	 */
	if (db_version(dbip) < 5 && (tmp_ptr - name) < NAMESIZE) {
	    bu_log("db_diradd() object named '%s' is illegal, ignored\n", name);
	    return RT_DIR_NULL;
	}
    }

    bu_vls_init(&local);
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
    return dp;
}


/**
 * D B _ D I R D E L E T E
 *
 * Given a pointer to a directory entry, remove it from the linked
 * list, and free the associated memory.
 *
 * It is the responsibility of the caller to have released whatever
 * structures have been hung on the d_use_hd bu_list, first.
 *
 * Returns -
 * 0 on success
 * non-0 on failure
 */
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
	RT_DIR_FREE_NAMEP(dp);	/* frees d_namep */
	findp->d_forw = dp->d_forw;

	/* Put 'dp' back on the freelist */
	dp->d_forw = rt_uniresource.re_directory_hd;
	rt_uniresource.re_directory_hd = dp;
	return 0;
    }
    return -1;
}


/**
 * D B _ R E N A M E
 *
 * Change the name string of a directory entry.  Because of the
 * hashing function, this takes some extra work.
 *
 * Returns -
 * 0 on success
 * non-0 on failure
 */
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


/**
 * D B _ P R _ D I R
 *
 * For debugging, print the entire contents of the database directory.
 */
void
db_pr_dir(const struct db_i *dbip)
{
    const struct directory *dp;
    char *flags;
    int i;

    RT_CK_DBI(dbip);

    bu_log("db_pr_dir(x%x):  Dump of directory for file %s [%s]\n",
	   dbip, dbip->dbi_filename,
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
	    bu_log("x%.8x %s %s=x%.8x len=%.5zu use=%.2d nref=%.2d %s",
		   dp,
		   flags,
		   dp->d_flags & RT_DIR_INMEM ? "  ptr " : "d_addr",
		   dp->d_addr,
		   dp->d_len,
		   dp->d_uses,
		   dp->d_nref,
		   dp->d_namep);
	    if (dp->d_animate)
		bu_log(" anim=x%x\n", dp->d_animate);
	    else
		bu_log("\n");
	}
    }
}


/**
 * D B _ L O O K U P _ B Y _ A T T R
 *
 * lookup directory entries based on directory flags (dp->d_flags) and
 * attributes the "dir_flags" arg is a mask for the directory flags
 * the *"avs" is an attribute value set used to select from the
 * objects that *pass the flags mask. if "op" is 1, then the object
 * must have all the *attributes and values that appear in "avs" in
 * order to be *selected. If "op" is 2, then the object must have at
 * least one of *the attribute/value pairs from "avs".
 *
 * dir_flags are in the form used in struct directory (d_flags)
 *
 * for op:
 * 1 -> all attribute name/value pairs must be present and match
 * 2 -> at least one of the name/value pairs must be present and match
 *
 * returns a ptbl list of selected directory pointers an empty list
 * means nothing met the requirements a NULL return means something
 * went wrong.
 */
struct bu_ptbl *
db_lookup_by_attr(struct db_i *dbip, int dir_flags, struct bu_attribute_value_set *avs, int op)
{
    struct bu_attribute_value_set obj_avs;
    struct directory *dp;
    struct bu_ptbl *tbl;
    int match_count=0;
    int attr_count;
    int i, j;
    int draw;

    RT_CK_DBI(dbip);

    if (avs) {
	BU_CK_AVS(avs);
	attr_count = avs->count;
    } else {
	attr_count = 0;
    }

    tbl = (struct bu_ptbl *)bu_calloc(sizeof(struct bu_ptbl), 1, "wdb_get_by_attr ptbl");
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
	    for (i=0; (size_t)i<(size_t)avs->count; i++) {
		for (j=0; (size_t)j<(size_t)obj_avs.count; j++) {
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
