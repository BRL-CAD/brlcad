/*
 *			D B _ A L L O C . C
 *
 * Functions -
 *	db_alloc	Find a contiguous block of database storage
 *	db_grow		Increase size of existing database entry
 *	db_trunc	Decrease size of existing entry, from it's end
 *	db_delete	Delete storage associated w/entry, zap records
 *	db_zapper	Zap region of file into ID_FREE records
 *	db_flags_internal	Construct flags word
 *
 *
 *  Authors -
 *	Michael John Muuss
 *	Robert Jon Reschly Jr.
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

#include "conf.h"

#include <stdio.h>
#ifdef USE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif

#include "machine.h"
#include "vmath.h"
#include "db.h"
#include "raytrace.h"

#include "./debug.h"

/*
 *  			D B _ A L L O C
 *  
 *  Find a block of database storage of "count" granules.
 *
 *  Returns -
 *	 0	OK
 *	-1	failure
 */
int
db_alloc( dbip, dp, count )
register struct db_i	*dbip;
register struct directory *dp;
int		count;
{
	unsigned long	addr;
	union record	rec;

	RT_CK_DBI(dbip);
	RT_CK_DIR(dp);
	if(rt_g.debug&DEBUG_DB) bu_log("db_alloc(%s) x%x, x%x, count=%d\n",
		dp->d_namep, dbip, dp, count );
	if( count <= 0 )  {
		bu_log("db_alloc(0)\n");
		return(-1);
	}

	if( dp->d_flags & RT_DIR_INMEM )  {
		if( dp->d_un.ptr )  {
			dp->d_un.ptr = bu_realloc( dp->d_un.ptr,
				count * sizeof(union record), "db_alloc() d_un.ptr" );
		} else {
			dp->d_un.ptr = bu_malloc( count * sizeof(union record), "db_alloc() d_un.ptr" );
		}
		dp->d_len = count;
		return 0;
	}

	if( dbip->dbi_read_only )  {
		bu_log("db_alloc on READ-ONLY file\n");
		return(-1);
	}
	while(1)  {
		if( (addr = rt_memalloc( &(dbip->dbi_freep), (unsigned)count )) == 0L )  {
			/* No contiguous free block, append to file */
			if( (dp->d_addr = dbip->dbi_eof) < 0 )  {
				bu_log("db_alloc: bad EOF\n");
				return(-1);
			}
			dp->d_len = count;
			dbip->dbi_eof += count * sizeof(union record);
			dbip->dbi_nrec += count;
			break;
		}
		dp->d_addr = addr * sizeof(union record);
		dp->d_len = count;
		if( db_get( dbip, dp, &rec, 0, 1 ) < 0 )
			return(-1);
		if( rec.u_id != ID_FREE )  {
			bu_log("db_alloc():  addr %ld non-FREE (id %d), skipping\n",
				addr, rec.u_id );
			continue;
		}
	}

	/* Clear out ALL the granules, for safety */
	return( db_zapper( dbip, dp, 0 ) );
}

/*
 *  			D B _ G R O W
 *  
 *  Increase the database size of an object by "count",
 *  by duplicating in a new area if necessary.
 *  XXX deprecated.  Only used by mged/mover.c
 *
 *  Returns:
 *	-1	on error
 *	0	on success
 */
int
db_grow( dbip, dp, count )
struct db_i	*dbip;
register struct directory *dp;
int		count;
{
	register int	i;
	union record	rec;
	union record	*rp;
	struct directory olddir;
	int		extra_start;
	int		old_len;

	RT_CK_DBI(dbip);
	RT_CK_DIR(dp);
	if(rt_g.debug&DEBUG_DB) bu_log("db_grow(%s) x%x, x%x, count=%d\n",
		dp->d_namep, dbip, dp, count );
	if( dp->d_flags & RT_DIR_INMEM )  bu_bomb("db_grow() called on RT_DIR_INMEM object\n");

	if( dbip->dbi_read_only )  {
		bu_log("db_grow on READ-ONLY file\n");
		return(-1);
	}

	if( count <= 0 )  {
		bu_log("db_grow(%d)\n", count );
		return(-1);
	}

	/* Easy case -- see if at end-of-file */
	old_len = dp->d_len;
	extra_start = dp->d_addr + dp->d_len * sizeof(union record);
	if( extra_start == dbip->dbi_eof )  {
		dbip->dbi_eof += count * sizeof(union record);
		dbip->dbi_nrec += count;
		dp->d_len += count;
		return( db_zapper( dbip, dp, old_len ) );

	}

	/* Try to extend into free space immediately following current obj */
	if( rt_memget( &(dbip->dbi_freep), (unsigned)count, (unsigned long)(dp->d_addr/sizeof(union record)) ) == 0L )
		goto hard;

	dp->d_len += count;
	/* Check to see if granules are all really availible (sanity check) */
	for( i=old_len; i < dp->d_len; i++ )  {
		if( db_get( dbip, dp, &rec, i, 1 ) < 0 ||
		     rec.u_id != ID_FREE )  {
			bu_log("db_grow:  FREE record wasn't?! (id%d)\n",
				rec.u_id);
		     	dp->d_len -= count;
			goto hard;
		}
	}
	return( db_zapper( dbip, dp, old_len ) );

hard:
	/*
	 *  Duplicate the records into a new area that is large enough.
	 */
	olddir = *dp;				/* struct copy */

	if( (rp = db_getmrec( dbip, dp )) == (union record *)0 )
		return(-1);
	db_alloc( dbip, dp, dp->d_len + count );	/* fixes addr & len */
	db_put( dbip, dp, rp, 0, old_len );
	bu_free( (char *)rp, "db_grow temp records");

	/* Release space that original copy consumed */
	db_delete( dbip, &olddir );
	return(0);
}

/*
 *  			D B _ T R U N C
 *  
 *  Remove "count" granules from the indicated database entry.
 *  Stomp on them with ID_FREE's.
 *  Add them to the freelist.
 */
int
db_trunc( dbip, dp, count )
struct db_i		*dbip;
register struct directory *dp;
int			 count;
{
	register int i;

	RT_CK_DBI(dbip);
	RT_CK_DIR(dp);
	if(rt_g.debug&DEBUG_DB) bu_log("db_trunc(%s) x%x, x%x, count=%d\n",
		dp->d_namep, dbip, dp, count );

	if( dp->d_flags & RT_DIR_INMEM )  {
		/* Decrease d_len, but don't bother with bu_realloc() */
		dp->d_len -= count;
		return 0;
	}
	if( dbip->dbi_read_only )  {
		bu_log("db_trunc on READ-ONLY file\n");
		return(-1);
	}
	i = db_zapper( dbip, dp, dp->d_len - count );
	rt_memfree( &(dbip->dbi_freep), (unsigned)count,
		(dp->d_addr/(sizeof(union record)))+dp->d_len-count );
	dp->d_len -= count;
	return(i);
}

/*
 *			D B _ D E L R E C
 *
 *  Delete a specific record from database entry
 *  No longer supported.
 */
int
db_delrec( dbip, dp, recnum )
struct db_i		*dbip;
register struct directory *dp;
int			recnum;
{

	RT_CK_DBI(dbip);
	RT_CK_DIR(dp);
	if(rt_g.debug&DEBUG_DB) bu_log("db_delrec(%s) x%x, x%x, recnum=%d\n",
		dp->d_namep, dbip, dp, recnum );

	bu_log("ERROR db_delrec() is no longer supported.  Use combination import/export routines.\n");
	return -1;
}

/*
 *  			D B _ D E L E T E
 *  
 *  Delete the indicated database record(s).
 *  Mark all records with ID_FREE.
 */
int
db_delete( dbip, dp )
struct db_i	*dbip;
struct directory *dp;
{
	register int i;

	RT_CK_DBI(dbip);
	RT_CK_DIR(dp);
	if(rt_g.debug&DEBUG_DB) bu_log("db_delete(%s) x%x, x%x\n",
		dp->d_namep, dbip, dp );

	if( dp->d_flags & RT_DIR_INMEM )  {
		bu_free( dp->d_un.ptr, "db_delete d_un.ptr");
		dp->d_un.ptr = NULL;
		dp->d_len = 0;
		return 0;
	}

	i = db_zapper( dbip, dp, 0 );

	rt_memfree( &(dbip->dbi_freep), (unsigned)dp->d_len,
		dp->d_addr/(sizeof(union record)) );

	dp->d_len = 0;
	dp->d_addr = -1;
	return(i);
}

/*
 *			D B _ Z A P P E R
 *
 *  Using a single call to db_put(), write multiple zeroed records out,
 *  all with u_id field set to ID_FREE.
 *  This will zap all records from "start" to the end of this entry.
 *
 *  Returns:
 *	-1	on error
 *	0	on success (from db_put())
 */
int
db_zapper( dbip, dp, start )
struct db_i	*dbip;
struct directory *dp;
int		start;
{
	register union record	*rp;
	register int		i;
	int			todo;

	RT_CK_DBI(dbip);
	RT_CK_DIR(dp);
	if(rt_g.debug&DEBUG_DB) bu_log("db_zapper(%s) x%x, x%x, start=%d\n",
		dp->d_namep, dbip, dp, start );

	if( dp->d_flags & RT_DIR_INMEM )  bu_bomb("db_zapper() called on RT_DIR_INMEM object\n");

	if( dbip->dbi_read_only )
		return(-1);

	if( (todo = dp->d_len - start) == 0 )
		return(0);		/* OK -- trivial */
	if( todo < 0 )
		return(-1);

	rp = (union record *)bu_malloc( todo * sizeof(union record), "db_zapper buf");
	bzero( (char *)rp, todo * sizeof(union record) );
	for( i=0; i < todo; i++ )
		rp[i].u_id = ID_FREE;
	i = db_put( dbip, dp, rp, start, todo );
	bu_free( (char *)rp, "db_zapper buf" );
	return i;
}

/*
 *			D B _ F L A G S _ I N T E R N A L
 *
 *  Given the internal form of a database object,
 *  return the appropriate 'flags' word for stashing in the
 *  in-memory directory of objects.
 */
int
db_flags_internal( intern )
CONST struct rt_db_internal	*intern;
{
	CONST struct rt_comb_internal	*comb;

	RT_CK_DB_INTERNAL(intern);

	if( intern->idb_type != ID_COMBINATION )
		return DIR_SOLID;

	comb = (struct rt_comb_internal *)intern->idb_ptr;
	RT_CK_COMB(comb);

	if( comb->region_flag )
		return DIR_COMB | DIR_REGION;
	return DIR_COMB;
}
