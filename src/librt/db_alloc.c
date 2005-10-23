/*                      D B _ A L L O C . C
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

/** \defgroup db Database
 * \ingroup librt
 */

/*@{*/
/** @file db_alloc.c
 *
 * Functions -
 *	- db_alloc	Find a contiguous block of database storage
 *	- db_delete	Delete storage associated w/entry, zap records
 *	- db_zapper	Zap region of file into ID_FREE records
 *	- db_flags_internal	Construct flags word
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
 */
/*@}*/

#ifndef lint
static const char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "common.h"



#include <stdio.h>
#ifdef HAVE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif

#include "machine.h"
#include "vmath.h"
#include "db.h"
#include "raytrace.h"

#include "./debug.h"

/**
 *  			D B _ A L L O C
 *
 *  Find a block of database storage of "count" granules.
 *
 *  Returns -
 *	 0	OK
 *	-1	failure
 */
int
db_alloc(register struct db_i *dbip, register struct directory *dp, int count)
{
	unsigned long	addr;
	union record	rec;

	RT_CK_DBI(dbip);
	RT_CK_DIR(dp);
	if(RT_G_DEBUG&DEBUG_DB) bu_log("db_alloc(%s) x%x, x%x, count=%d\n",
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

/**
 *			D B _ D E L R E C
 *
 *  Delete a specific record from database entry
 *  No longer supported.
 */
int
db_delrec(struct db_i *dbip, register struct directory *dp, int recnum)
{

	RT_CK_DBI(dbip);
	RT_CK_DIR(dp);
	if(RT_G_DEBUG&DEBUG_DB) bu_log("db_delrec(%s) x%x, x%x, recnum=%d\n",
		dp->d_namep, dbip, dp, recnum );

	bu_log("ERROR db_delrec() is no longer supported.  Use combination import/export routines.\n");
	return -1;
}

/**
 *  			D B _ D E L E T E
 *
 *  Delete the indicated database record(s).
 *  Arrange to write "free storage" database markers in it's place,
 *  positively erasing what had been there before.
 */
int
db_delete(struct db_i *dbip, struct directory *dp)
{
	register int i = -1;

	RT_CK_DBI(dbip);
	RT_CK_DIR(dp);
	if(RT_G_DEBUG&DEBUG_DB) bu_log("db_delete(%s) x%x, x%x\n",
		dp->d_namep, dbip, dp );

	if( dp->d_flags & RT_DIR_INMEM )  {
		bu_free( dp->d_un.ptr, "db_delete d_un.ptr");
		dp->d_un.ptr = NULL;
		dp->d_len = 0;
		return 0;
	}

	if( dbip->dbi_version == 4 )  {
		i = db_zapper( dbip, dp, 0 );
		rt_memfree( &(dbip->dbi_freep), (unsigned)dp->d_len,
			dp->d_addr/(sizeof(union record)) );
	} else if( dbip->dbi_version == 5 )  {
		i = db5_write_free( dbip, dp, dp->d_len );
		rt_memfree( &(dbip->dbi_freep), dp->d_len,
			dp->d_addr );
	} else {
		bu_bomb("db_delete() unsupported database version\n");
	}

	dp->d_len = 0;
	dp->d_addr = -1;
	return i;
}

/**
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
db_zapper(struct db_i *dbip, struct directory *dp, int start)
{
	register union record	*rp;
	register int		i;
	int			todo;

	RT_CK_DBI(dbip);
	RT_CK_DIR(dp);
	if(RT_G_DEBUG&DEBUG_DB) bu_log("db_zapper(%s) x%x, x%x, start=%d\n",
		dp->d_namep, dbip, dp, start );

	if( dp->d_flags & RT_DIR_INMEM )  bu_bomb("db_zapper() called on RT_DIR_INMEM object\n");

	if( dbip->dbi_read_only )
		return(-1);

	BU_ASSERT_LONG( dbip->dbi_version, ==, 4 );

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

/**
 *			D B _ F L A G S _ I N T E R N A L
 *
 *  Given the internal form of a database object,
 *  return the appropriate 'flags' word for stashing in the
 *  in-memory directory of objects.
 */
int
db_flags_internal(const struct rt_db_internal *intern)
{
	const struct rt_comb_internal	*comb;

	RT_CK_DB_INTERNAL(intern);

	if( intern->idb_type != ID_COMBINATION )
		return DIR_SOLID;

	comb = (struct rt_comb_internal *)intern->idb_ptr;
	RT_CK_COMB(comb);

	if( comb->region_flag )
		return DIR_COMB | DIR_REGION;
	return DIR_COMB;
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
