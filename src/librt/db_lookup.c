/*                     D B _ L O O K U P . C
 * BRL-CAD
 *
 * Copyright (C) 1988-2005 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */

/** \addtogroup db */

/*@{*/
/** @file db_lookup.c
 *
 * Functions -
 *	db_dirhash	Compute hashing function
 *	db_lookup	Convert an object name into directory pointer
 *	db_diradd	Add entry to the directory
 *	db_dirdelete	Delete entry from directory
 *	db_rename	Change name string of a directory entry
 *	db_pr_dir	Print contents of database directory
 *
 *
 *  Authors -
 *	Michael John Muuss
 *
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 *
 */
/*@}*/

#ifndef lint
static const char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "common.h"

#include <stdio.h>
#ifdef HAVE_STRING_H
#  include <string.h>
#else
#  include <strings.h>
#endif

#include "machine.h"
#include "vmath.h"
#include "db.h"
#include "raytrace.h"

#include "./debug.h"


/**
 *			D B _ I S _ D I R E C T O R Y _ N O N _ E M P T Y
 *
 *  Returns -
 *	0	if the in-memory directory is empty
 *	1	if the in-memory directory has entries,
 *		which implies that a db_scan() has already been performed.
 */
int
db_is_directory_non_empty(const struct db_i	*dbip)
{
    register int	i;

    RT_CK_DBI(dbip);

    for (i = 0; i < RT_DBNHASH; i++)  {
	if( dbip->dbi_Head[i] != DIR_NULL )
	    return 1;
    }
    return 0;
}

/**
 *			D B _ G E T _ D I R E C T O R Y _ S I Z E
 *
 *  Return the number of "struct directory" nodes in the given database.
 */
int
db_get_directory_size(const struct db_i *dbip)
{
    register struct directory *dp;
    register int	count = 0;
    int		i;

    RT_CK_DBI(dbip);

    for (i = 0; i < RT_DBNHASH; i++)  {
	for (dp = dbip->dbi_Head[i]; dp != DIR_NULL; dp = dp->d_forw)
	    count++;
    }
    return count;
}

/**
 *			D B _ C K _ D I R E C T O R Y
 *
 *  For debugging, ensure that all the linked-lists for the directory
 *  structure are intact.
 */
void
db_ck_directory(const struct db_i *dbip)
{
    register struct directory *dp;
    int		i;

    RT_CK_DBI(dbip);

    for (i = 0; i < RT_DBNHASH; i++)  {
	for (dp = dbip->dbi_Head[i]; dp != DIR_NULL; dp = dp->d_forw)
	    RT_CK_DIR(dp);
    }
}

/**
 *			D B _ D I R H A S H
 *
 *  Internal function to return pointer to head of hash chain
 *  corresponding to the given string.
 */
int
db_dirhash(const char *str)
{
    register const unsigned char *s = (unsigned char *)str;
    register long sum;
    register int i;

    sum = 0;
    /* BSD namei hashing starts i=0, discarding first char.  why? */
    for( i=1; *s; )
	sum += *s++ * i++;

    return( RT_DBHASH(sum) );
}

/**
 *  Name -
 *	D B _ D I R C H E C K
 *
 *  Description -
 *	This routine ensures that ret_name is not already in the
 *	directory. If it is, it tries a fixed number of times to
 *	modify ret_name before giving up. Note - most of the time,
 *	the hash for ret_name is computed once.
 *
 *  Inputs -
 *	dbip		database instance pointer
 *	ret_name	the original name
 *	noisy		to blather or not
 *
 *  Outputs -
 *	ret_name	the name to use
 *	headp		pointer to the first (struct directory *) in the bucket
 *
 *  Returns -
 *	 0	success
 *	<0	fail
 */
int
db_dircheck(struct db_i		*dbip,
	    struct bu_vls	*ret_name,
	    int			noisy,
	    struct directory	***headp)
{
    register struct directory	*dp;
    register char			*cp = bu_vls_addr(ret_name);
    register char			n0 = cp[0];
    register char			n1 = cp[1];

    /* Compute hash only once (almost always the case) */
    *headp = &(dbip->dbi_Head[db_dirhash(cp)]);

    for (dp = **headp; dp != DIR_NULL; dp=dp->d_forw) {
	register char	*this;
	if (n0 == *(this=dp->d_namep)  &&	/* speed */
	    n1 == this[1]  &&			/* speed */
	    strcmp(cp, this) == 0) {
	    /* Name exists in directory already */
	    register int	c;

	    bu_vls_strcpy(ret_name, "A_");
	    bu_vls_strcat(ret_name, this);

	    for (c = 'A'; c <= 'Z'; c++) {
		*cp = c;
		if (db_lookup(dbip, cp, noisy) == DIR_NULL)
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
 *			D B _ L O O K U P
 *
 * This routine takes a name and looks it up in the
 * directory table.  If the name is present, a pointer to
 * the directory struct element is returned, otherwise
 * NULL is returned.
 *
 * If noisy is non-zero, a print occurs, else only
 * the return code indicates failure.
 *
 *  Returns -
 *	struct directory	if name is found
 *	DIR_NULL		on failure
 */
struct directory *
db_lookup(const struct db_i *dbip, register const char *name, int noisy)
{
    register struct directory *dp;
    register char	n0;
    register char	n1;

    if (!name) {
	bu_log("db_lookup received NULL name\n");
	return (DIR_NULL);
    }

    n0 = name[0];
    n1 = name[1];

    RT_CK_DBI(dbip);

    dp = dbip->dbi_Head[db_dirhash(name)];
    for(; dp != DIR_NULL; dp=dp->d_forw )  {
	register char	*this;
	if(
	   n0 == *(this=dp->d_namep)  &&	/* speed */
	   n1 == this[1]  &&	/* speed */
	   strcmp( name, this ) == 0
	   )  {
	    if(RT_G_DEBUG&DEBUG_DB) bu_log("db_lookup(%s) x%x\n", name, dp);
	    return(dp);
	}
    }

    if(noisy || RT_G_DEBUG&DEBUG_DB) bu_log("db_lookup(%s) failed: %s does not exist\n", name, name);
    return( DIR_NULL );
}

/**
 *			D B _ D I R A D D
 *
 * Add an entry to the directory.
 * Try to make the regular path through the code as fast as possible,
 * to speed up building the table of contents.
 *
 * dbip is a pointer to a valid/opened database instance
 * name is the string name of the object being added
 * laddr is the long offset into the file to the object
 * len is the length of the object, number of db granules used
 * flags are defined in raytrace.h (DIR_SOLID, DIR_COMB, DIR_REGION, RT_DIR_INMEM, etc)
 * for db version 5, ptr is the minor_type (non-null pointer to valid unsigned char code)
 *
 * an laddr of RT_DIR_PHONY_ADDR (-1L) means that database storage has
 * not been allocated yet.
 */
struct directory *
db_diradd(register struct db_i *dbip, register const char *name, long int laddr, int len, int flags, genptr_t ptr)
{
    struct directory **headp;
    register struct directory *dp;
    char *tmp_ptr;
    struct bu_vls	local;

    RT_CK_DBI(dbip);

    if(RT_G_DEBUG&DEBUG_DB)  {
	bu_log("db_diradd(dbip=0x%x, name='%s', addr=0x%x, len=%d, flags=0x%x, ptr=0x%x)\n",
	       dbip, name, laddr, len, flags, ptr );
    }

    if( (tmp_ptr=strchr( name, '/' )) != NULL )  {
	/* if this is a version 4 database and the offending char is beyond NAMESIZE
	 * then it is not really a problem
	 */
	if( dbip->dbi_version < 5 && (tmp_ptr - name) < 16 ) {
	    bu_log("db_diradd() object named '%s' is illegal, ignored\n", name );
	    return DIR_NULL;
	}
    }

    bu_vls_init(&local);
    if( dbip->dbi_version < 5 ) {
	bu_vls_strncpy(&local, name, NAMESIZE);
    } else {
	/* must provide a valid minor type */
	if (!ptr) {
	    bu_log("WARNING: db_diradd() called with a null minor type pointer for object %s\nIgnoring %s\n", name, name);
	    bu_vls_free(&local);
	    return DIR_NULL;
	}
	bu_vls_strcpy(&local, name);
    }
    if (db_dircheck(dbip, &local, 0, &headp) < 0) {
	bu_vls_free(&local);
	return DIR_NULL;
    }

    /* 'name' not found in directory, add it */
    RT_GET_DIRECTORY(dp, &rt_uniresource);
    RT_CK_DIR(dp);
    RT_DIR_SET_NAMEP(dp, bu_vls_addr(&local));	/* sets d_namep */
    dp->d_un.file_offset = laddr;
    dp->d_flags = flags & ~(RT_DIR_INMEM);
    dp->d_len = len;
    dp->d_forw = *headp;
    BU_LIST_INIT( &dp->d_use_hd );
    *headp = dp;
    dp->d_animate = NULL;
    dp->d_nref = 0;
    dp->d_uses = 0;
    if( dbip->dbi_version > 4 ) {
	dp->d_major_type = DB5_MAJORTYPE_BRLCAD;
	dp->d_minor_type = *(unsigned char *)ptr;
    }
    bu_vls_free(&local);
    return( dp );
}


/**
 *  			D B _ D I R D E L E T E
 *
 *  Given a pointer to a directory entry, remove it from the
 *  linked list, and free the associated memory.
 *
 *  It is the responsibility of the caller to have released whatever
 *  structures have been hung on the d_use_hd bu_list, first.
 *
 *  Returns -
 *	 0	on success
 *	-1	on failure
 */
int
db_dirdelete(register struct db_i *dbip, register struct directory *dp)
{
    register struct directory *findp;
    register struct directory **headp;

    RT_CK_DBI(dbip);
    RT_CK_DIR(dp);

    headp = &(dbip->dbi_Head[db_dirhash(dp->d_namep)]);

    if( dp->d_flags & RT_DIR_INMEM )
	{
	    if( dp->d_un.ptr != NULL )
		bu_free( dp->d_un.ptr, "db_dirdelete() inmem ptr" );
	}

    if( *headp == dp )  {
	RT_DIR_FREE_NAMEP(dp);	/* frees d_namep */
	*headp = dp->d_forw;

	/* Put 'dp' back on the freelist */
	dp->d_forw = rt_uniresource.re_directory_hd;
	rt_uniresource.re_directory_hd = dp;
	return(0);
    }
    for( findp = *headp; findp != DIR_NULL; findp = findp->d_forw )  {
	if( findp->d_forw != dp )
	    continue;
	RT_DIR_FREE_NAMEP(dp);	/* frees d_namep */
	findp->d_forw = dp->d_forw;

	/* Put 'dp' back on the freelist */
	dp->d_forw = rt_uniresource.re_directory_hd;
	rt_uniresource.re_directory_hd = dp;
	return(0);
    }
    return(-1);
}

/**
 *			D B _ R E N A M E
 *
 *  Change the name string of a directory entry.
 *  Because of the hashing function, this takes some extra work.
 *
 *  Returns -
 *	 0	on success
 *	-1	on failure
 */
int
db_rename(register struct db_i *dbip, register struct directory *dp, const char *newname)
{
    register struct directory *findp;
    register struct directory **headp;

    RT_CK_DBI(dbip);
    RT_CK_DIR(dp);

    /* Remove from linked list */
    headp = &(dbip->dbi_Head[db_dirhash(dp->d_namep)]);
    if( *headp == dp )  {
	/* Was first on list, dequeue */
	*headp = dp->d_forw;
    } else {
	for( findp = *headp; findp != DIR_NULL; findp = findp->d_forw )  {
	    if( findp->d_forw != dp )
		continue;
	    /* Dequeue */
	    findp->d_forw = dp->d_forw;
	    goto out;
	}
	return(-1);		/* ERROR: can't find */
    }

 out:
    /* Effect new name */
    RT_DIR_FREE_NAMEP(dp);			/* frees d_namep */
    RT_DIR_SET_NAMEP( dp, newname );	/* sets d_namep */

    /* Add to new linked list */
    headp = &(dbip->dbi_Head[db_dirhash(newname)]);
    dp->d_forw = *headp;
    *headp = dp;
    return(0);
}

/**
 *			D B _ P R _ D I R
 *
 *  For debugging, print the entire contents of the database directory.
 */
void
db_pr_dir(register const struct db_i *dbip)
{
    register const struct directory *dp;
    register char		*flags;
    register int		i;

    RT_CK_DBI(dbip);

    bu_log("db_pr_dir(x%x):  Dump of directory for file %s [%s]\n",
	   dbip, dbip->dbi_filename,
	   dbip->dbi_read_only ? "READ-ONLY" : "Read/Write" );

    bu_log("Title = %s\n", dbip->dbi_title);
    /* units ? */

    for( i = 0; i < RT_DBNHASH; i++ )  {
	for( dp = dbip->dbi_Head[i]; dp != DIR_NULL; dp=dp->d_forw )  {
	    if( dp->d_flags & DIR_SOLID )
		flags = "SOL";
	    else if( (dp->d_flags & (DIR_COMB|DIR_REGION)) ==
		     (DIR_COMB|DIR_REGION) )
		flags = "REG";
	    else if( (dp->d_flags & (DIR_COMB|DIR_REGION)) ==
		     DIR_COMB )
		flags = "COM";
	    else
		flags = "Bad";
	    bu_log("x%.8x %s %s=x%.8x len=%.5d use=%.2d nref=%.2d %s",
		   dp,
		   flags,
		   dp->d_flags & RT_DIR_INMEM ? "  ptr " : "d_addr",
		   dp->d_addr,
		   dp->d_len,
		   dp->d_uses,
		   dp->d_nref,
		   dp->d_namep );
	    if( dp->d_animate )
		bu_log(" anim=x%x\n", dp->d_animate );
	    else
		bu_log("\n");
	}
    }
}


/**
 *  			D B _ G E T _ D I R E C T O R Y
 *
 *  This routine is called by the RT_GET_DIRECTORY macro when the freelist
 *  is exhausted.  Rather than simply getting one additional structure,
 *  we get a whole batch, saving overhead.
 */
void
db_get_directory(register struct resource *resp)
{
    register struct directory	*dp;
    register int		bytes;

    RT_RESOURCE_CHECK(resp);
    BU_CK_PTBL( &resp->re_directory_blocks );

    BU_ASSERT_PTR( resp->re_directory_hd, ==, NULL );

    /* Get a BIG block */
    bytes = bu_malloc_len_roundup(1024*sizeof(struct directory));
    dp = (struct directory *)bu_malloc(bytes, "db_get_directory()");

    /* Record storage for later */
    bu_ptbl_ins( &resp->re_directory_blocks, (long *)dp );

    while( bytes >= sizeof(struct directory) )  {
	dp->d_magic = RT_DIR_MAGIC;
	dp->d_forw = resp->re_directory_hd;
	resp->re_directory_hd = dp;
	dp++;
	bytes -= sizeof(struct directory);
    }
}

/**
 *			D B _ L O O K U P _ B Y _ A T T R
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
    int i,j;
    int draw;

    RT_CK_DBI(dbip);

    if( avs ) {
	BU_CK_AVS( avs );
	attr_count = avs->count;
    } else {
	attr_count = 0;
    }
    tbl = (struct bu_ptbl *)bu_malloc( sizeof( struct bu_ptbl ), "wdb_get_by_attr ptbl" );
    bu_ptbl_init( tbl, 128, "wdb_get_by_attr ptbl_init" );
    FOR_ALL_DIRECTORY_START(dp,dbip)
	if( (dp->d_flags & dir_flags) == 0 ) continue;
    if(attr_count ) {
	if( db5_get_attributes( dbip, &obj_avs, dp ) < 0 ) {
	    bu_log( "ERROR: failed to get attributes for %s\n", dp->d_namep );
	    return( (struct bu_ptbl *)NULL );
	}

	draw = 0;
	match_count = 0;
	for( i=0 ; i<avs->count ; i++ ) {
	    for( j=0 ; j<obj_avs.count ; j++ ) {
		if( !strcmp( avs->avp[i].name, obj_avs.avp[j].name ) ) {
		    if( !strcmp( avs->avp[i].value, obj_avs.avp[j].value ) ) {
			if( op == 2 ) {
			    draw = 1;
			    break;
			} else {
			    match_count++;
			}
		    }
		}
	    }
	    if( draw ) break;
	}
	bu_avs_free( &obj_avs );
    } else {
	draw = 1;
    }
    if( draw || match_count == attr_count ) {
	bu_ptbl_ins( tbl , (long *)dp );
    }
    FOR_ALL_DIRECTORY_END

	return( tbl );
}

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
