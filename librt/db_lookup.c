/*
 *			D B _ L O O K U P . C
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
 *  Copyright Notice -
 *	This software is Copyright (C) 1988 by the United States Army.
 *	All rights reserved.
 */
#ifndef lint
static char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include <stdio.h>
#ifdef BSD
#include <strings.h>
#else
#include <string.h>
#endif

#include "machine.h"
#include "vmath.h"
#include "raytrace.h"
#include "db.h"

#include "./debug.h"



/*
 *			D B _ D I R H A S H
 *  
 *  Internal function to return pointer to head of hash chain
 *  corresponding to the given string.
 */
static int
db_dirhash(str)
char *str;
{
	register unsigned char *s = (unsigned char *)str;
	register long sum;
	register int i;

	sum = 0;
	/* BSD namei hashing starts i=0, discarding first char.  why? */
	for( i=1; *s; )
		sum += *s++ * i++;

	return( RT_DBHASH(sum) );
}


/*
 *			D B _ L O O K U P
 *
 * This routine takes a name, trims to NAMESIZE, and looks it up in the
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
db_lookup( dbip, name, noisy )
register struct db_i	*dbip;
register char		*name;
int			noisy;
{
	register struct directory *dp;
	static char local[NAMESIZE+2];

	if( dbip->dbi_magic != DBI_MAGIC )  rt_bomb("db_lookup:  bad dbip\n");

	if( strlen(name) > NAMESIZE )  {
		(void)strncpy( local, name, NAMESIZE );	/* Trim the name */
		local[NAMESIZE] = '\0';			/* ensure null termination */
		name = local;
	}
	dp = dbip->dbi_Head[db_dirhash(name)];
	for(; dp != DIR_NULL; dp=dp->d_forw )  {
		if(
			name[0] == dp->d_namep[0]  &&	/* speed */
			name[1] == dp->d_namep[1]  &&	/* speed */
			strcmp( name, dp->d_namep ) == 0
		)  {
			if(rt_g.debug&DEBUG_DB) rt_log("db_lookup(%s) x%x\n", name, dp);
			return(dp);
		}
	}

	if( noisy )
		rt_log("db_lookup:  could not find '%s'\n", name );
	if(rt_g.debug&DEBUG_DB) rt_log("db_lookup(%s) failed\n", name);
	return( DIR_NULL );
}

/*
 *			D B _ D I R A D D
 *
 * Add an entry to the directory
 */
struct directory *
db_diradd( dbip, name, laddr, len, flags )
register struct db_i	*dbip;
register char		*name;
long			laddr;
int			len;
int			flags;
{
	register struct directory **headp;
	register struct directory *dp;
	struct directory	*dupdp;
	char			local[NAMESIZE+2];

	if( dbip->dbi_magic != DBI_MAGIC )  rt_bomb("db_diradd:  bad dbip\n");

	if(rt_g.debug&DEBUG_DB) rt_log("db_diradd( x%x, %s, addr=x%x, len=%d, flags=x%x)\n", dbip, name, laddr, len, flags);

	if( (dupdp = db_lookup( dbip, name, 0 )) != DIR_NULL )  {
		rt_log("db_diradd(dbip=x%x, name='%s', addr=x%x, len=%d, flags=x%x)\n",
			dbip, name, laddr, len, flags );
		rt_log("Attempt to duplicate existing entry x%x d_addr=x%x with same name, ignored\n",
			dupdp, dupdp->d_addr );
		return( DIR_NULL );
	}

	GETSTRUCT( dp, directory );
	if( dp == DIR_NULL )
		return( DIR_NULL );
	(void)strncpy( local, name, NAMESIZE );	/* Trim the name */
	local[NAMESIZE] = '\0';			/* ensure null termination */
	dp->d_namep = rt_strdup( local );
	dp->d_addr = laddr;
	dp->d_flags = flags;
	dp->d_len = len;
	headp = &(dbip->dbi_Head[db_dirhash(local)]);
	dp->d_forw = *headp;
	*headp = dp;
	return( dp );
}

/*
 *  			D B _ D I R D E L E T E
 *
 *  Given a pointer to a directory entry, remove it from the
 *  linked list, and free the associated memory.
 *
 *  Returns -
 *	 0	on success
 *	-1	on failure
 */
int
db_dirdelete( dbip, dp )
register struct db_i		*dbip;
register struct directory	*dp;
{
	register struct directory *findp;
	register struct directory **headp;

	if( dbip->dbi_magic != DBI_MAGIC )  rt_bomb("db_dirdelete:  bad dbip\n");

	headp = &(dbip->dbi_Head[db_dirhash(dp->d_namep)]);
	if( *headp == dp )  {
		rt_free( dp->d_namep, "dir name" );
		*headp = dp->d_forw;
		rt_free( (char *)dp, "struct directory" );
		return(0);
	}
	for( findp = *headp; findp != DIR_NULL; findp = findp->d_forw )  {
		if( findp->d_forw != dp )
			continue;
		rt_free( dp->d_namep, "dir name" );
		findp->d_forw = dp->d_forw;
		rt_free( (char *)dp, "struct directory" );
		return(0);
	}
	return(-1);
}

/*
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
db_rename( dbip, dp, newname )
register struct db_i		*dbip;
register struct directory	*dp;
char				*newname;
{
	register struct directory *findp;
	register struct directory **headp;

	if( dbip->dbi_magic != DBI_MAGIC )  rt_bomb("db_rename:  bad dbip\n");

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
	rt_free( dp->d_namep, "d_namep" );
	dp->d_namep = rt_strdup( newname );

	/* Add to new linked list */
	headp = &(dbip->dbi_Head[db_dirhash(newname)]);
	dp->d_forw = *headp;
	*headp = dp;
	return(0);
}

/*
 *			D B _ P R _ D I R
 *
 *  For debugging, print the entire contents of the database directory.
 */
void
db_pr_dir( dbip )
register struct db_i	*dbip;
{
	register struct directory *dp;
	register char		*flags;
	register int		i;

	if( dbip->dbi_magic != DBI_MAGIC )  rt_bomb("db_pr_dir:  bad dbip\n");

	rt_log("db_pr_dir(x%x):  Dump of directory for file %s [%s]\n",
		dbip, dbip->dbi_filename,
		dbip->dbi_read_only ? "READ-ONLY" : "Read/Write" );

	rt_log("Title = %s\n", dbip->dbi_title);
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
			rt_log("%.8x %.16s %s addr=%.6x use=%.2d len=%.3d nref=%.2d",
				dp, dp->d_namep,
				flags,
				dp->d_addr,
				dp->d_uses,
				dp->d_len,
				dp->d_nref );
			if( dp->d_animate )
				rt_log(" anim=x%x\n", dp->d_animate );
			else
				rt_log("\n");
		}
	}
}
