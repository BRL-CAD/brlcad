/*                     D B 5 _ A L L O C . C
 * BRL-CAD
 *
 * Copyright (C) 2004-2005 United States Government as represented by
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

/** \defgroup db5 Database5
 * \ingroup librt
 */

/*@{*/
/** @file db5_alloc.c
 *  Handle disk space allocation in the BRL-CAD v5 database.
 *
 *  Author -
 *	Michael John Muuss
 *  
 *  Source -
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5068  USA
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
#include "bu.h"
#include "vmath.h"
#include "db5.h"
#include "raytrace.h"

#include "./debug.h"

/**
 *			D B 5 _ W R I T E _ F R E E
 *
 *  Create a v5 database "free" object of the specified size,
 *  and place it at the indicated location in the database.
 *
 *  There are two interesting cases:
 *	1)  The free object is "small".  Just write it all at once.
 *	2)  The free object is "large".  Write header and trailer
 *	    separately
 *
 *  Returns -
 *	0	OK
 *	-1	Fail.  This is a horrible error.
 */
int
db5_write_free( struct db_i *dbip, struct directory *dp, long length )
{
	struct bu_external	ext;

	RT_CK_DBI(dbip);
	RT_CK_DIR(dp);

 	if( length <= 8192 )  {

		BU_INIT_EXTERNAL( &ext );
		db5_make_free_object( &ext, length );

		if( dp->d_flags & RT_DIR_INMEM )  {
			bcopy( (char *)ext.ext_buf, dp->d_un.ptr, ext.ext_nbytes );
			bu_free_external( &ext );
			return 0;
		}

		if( db_write( dbip, (char *)ext.ext_buf, ext.ext_nbytes, dp->d_addr ) < 0 )  {
			bu_free_external( &ext );
			return -1;
		}
		bu_free_external( &ext );
		return 0;
	}

	/* Free object is "large", only write the header and trailer bytes. */

	BU_INIT_EXTERNAL( &ext );
	db5_make_free_object_hdr( &ext, length );

	if( dp->d_flags & RT_DIR_INMEM )  {
		bcopy( (char *)ext.ext_buf, dp->d_un.ptr, ext.ext_nbytes );
		((char *)ext.ext_buf)[length-1] = DB5HDR_MAGIC2;
		bu_free_external( &ext );
		return 0;
	}

	/* Write header */
	if( db_write( dbip, (char *)ext.ext_buf, ext.ext_nbytes, dp->d_addr ) < 0 )  {
		bu_free_external( &ext );
		return -1;
	}

	/* Write trailer byte */
	*((char *)ext.ext_buf) = DB5HDR_MAGIC2;
	if( db_write( dbip, (char *)ext.ext_buf, 1, dp->d_addr+length-1 ) < 0 )  {
		bu_free_external( &ext );
		return -1;
	}
	bu_free_external( &ext );
	return 0;
}

/**
 *			D B 5 _ R E A L L O C
 *
 *  Change the size of a v5 database object.
 *
 *  If the object is getting smaller, break it into two pieces,
 *  and write out free objects for both.
 *  The caller is expected to re-write new data on the first one.
 *
 *  If the object is getting larger, seek a suitable "hole" large enough
 *  to hold it, throwing back any surplus, properly marked.
 *
 *  If the object is getting larger and there is no suitable "hole"
 *  in the database, extend the file, write a free object in the
 *  new space, and write a free object in the old space.
 *
 *  There is no point to trying to extend in place, that would require
 *  two searches through the memory map, and doesn't save any disk I/O.
 *
 *  Returns -
 *	0	OK
 *	-1	Failure
 */
int
db5_realloc( struct db_i *dbip, struct directory *dp, struct bu_external *ep )
{
	long	baseaddr;
	long	baselen;

	RT_CK_DBI(dbip);
	RT_CK_DIR(dp);
	BU_CK_EXTERNAL(ep);
	if(RT_G_DEBUG&DEBUG_DB) bu_log("db5_realloc(%s) dbip=x%x, dp=x%x, ext_nbytes=%ld\n",
		dp->d_namep, dbip, dp, ep->ext_nbytes );

	BU_ASSERT_LONG( ep->ext_nbytes&7, ==, 0 );

	if( dp->d_addr != -1L && ep->ext_nbytes == dp->d_len )  {
		if(RT_G_DEBUG&DEBUG_DB) bu_log("db5_realloc(%s) current allocation is exactly right.\n", dp->d_namep);
		return 0;
	}
	if( dp->d_addr == -1L )  BU_ASSERT_LONG( dp->d_len, ==, 0 );

	baseaddr = dp->d_addr;
	baselen = dp->d_len;

	if( dp->d_flags & RT_DIR_INMEM )  {
		if( dp->d_un.ptr )  {
			if(RT_G_DEBUG&DEBUG_DB) bu_log("db5_realloc(%s) bu_realloc()ing memory resident object\n", dp->d_namep);
			dp->d_un.ptr = bu_realloc( dp->d_un.ptr,
				ep->ext_nbytes, "db5_realloc() d_un.ptr" );
		} else {
			if(RT_G_DEBUG&DEBUG_DB) bu_log("db5_realloc(%s) bu_malloc()ing memory resident object\n", dp->d_namep);
			dp->d_un.ptr = bu_malloc( ep->ext_nbytes, "db5_realloc() d_un.ptr" );
		}
		dp->d_len = ep->ext_nbytes;
		return 0;
	}

	if( dbip->dbi_read_only )  {
		bu_log("db5_realloc(%s) on READ-ONLY file\n", dp->d_namep);
		return(-1);
	}

	/* If the object is getting smaller... */
	if( ep->ext_nbytes < dp->d_len )  {
		if(RT_G_DEBUG&DEBUG_DB) bu_log("db5_realloc(%s) object is getting smaller\n", dp->d_namep);

		/* First, erase front half of storage to desired size. */
		dp->d_len = ep->ext_nbytes;
		if( db5_write_free( dbip, dp, dp->d_len ) < 0 )  return -1;

		/* Second, erase back half of storage to remainder. */
		dp->d_addr = baseaddr + ep->ext_nbytes;
		dp->d_len = baselen - ep->ext_nbytes;
		if( db5_write_free( dbip, dp, dp->d_len ) < 0 )  return -1;

		/* Finally, update tables */
		rt_memfree( &(dbip->dbi_freep), dp->d_len, dp->d_addr );
		dp->d_addr = baseaddr;
		dp->d_len = ep->ext_nbytes;
		return 0;
	}

	/* The object is getting larger... */

	/* Start by zapping existing database object into a free object */
	if( dp->d_addr != -1L )  {
		if(RT_G_DEBUG&DEBUG_DB) bu_log("db5_realloc(%s) releasing storage at x%x, len=%d\n", dp->d_namep, dp->d_addr, dp->d_len);

		rt_memfree( &(dbip->dbi_freep), dp->d_len, dp->d_addr );
		if( db5_write_free( dbip, dp, dp->d_len ) < 0 )  return -1;
		baseaddr = dp->d_addr = -1L;	/* sanity */
	}

	/*
	 *  Can we obtain a free block somewhere else?
	 *  Keep in mind that free blocks may be very large (e.g. 50 MBytes).
	 */
	{
		struct mem_map	*mmp;
		long		newaddr;

		if( (mmp = rt_memalloc_nosplit( &(dbip->dbi_freep), ep->ext_nbytes )) != MAP_NULL )  {
			if(RT_G_DEBUG&DEBUG_DB) bu_log("db5_realloc(%s) obtained free block at x%x, len=%d\n", dp->d_namep, mmp->m_addr, mmp->m_size );
			BU_ASSERT_LONG( mmp->m_size, >=, ep->ext_nbytes );
			if( mmp->m_size == ep->ext_nbytes )  {
				/* No need to reformat, existing free object is perfect */
				dp->d_addr = mmp->m_addr;
				dp->d_len = ep->ext_nbytes;
				return 0;
			}
			newaddr = mmp->m_addr;
			if( mmp->m_size > ep->ext_nbytes )  {
				/* Reformat and free the surplus */
				dp->d_addr = mmp->m_addr + ep->ext_nbytes;
				dp->d_len = mmp->m_size - ep->ext_nbytes;
				if(RT_G_DEBUG&DEBUG_DB) bu_log("db5_realloc(%s) returning surplus at x%x, len=%d\n", dp->d_namep, dp->d_addr, dp->d_len );
				if( db5_write_free( dbip, dp, dp->d_len ) < 0 )  return -1;
				rt_memfree( &(dbip->dbi_freep), dp->d_len, dp->d_addr );
				/* mmp is invalid beyond here! */
			}
			dp->d_addr = newaddr;
			dp->d_len = ep->ext_nbytes;
			/* Erase the new place */
			if(RT_G_DEBUG&DEBUG_DB) bu_log("db5_realloc(%s) utilizing free block at addr=x%x, len=%d\n", dp->d_namep, dp->d_addr, dp->d_len);
			if( db5_write_free( dbip, dp, dp->d_len ) < 0 )  return -1;
			return 0;
		}
	}

	/* No free storage of the desired size, extend the database */
	dp->d_addr = dbip->dbi_eof;
	dbip->dbi_eof += ep->ext_nbytes;
	dp->d_len = ep->ext_nbytes;
	if(RT_G_DEBUG&DEBUG_DB) bu_log("db5_realloc(%s) extending database addr=x%x, len=%d\n", dp->d_namep, dp->d_addr, dp->d_len);
#if 0
	/* Extending db with free record isn't necessary even to
	 * provide "stable-store" capability.
	 * If program or system aborts before caller write new object,
	 * there is no problem.
	 */
	if( db5_write_free( dbip, dp, dp->d_len ) < 0 )  return -1;
#endif
	return 0;
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
