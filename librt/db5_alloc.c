/*
 *			D B _ A L L O C 5 . C
 *
 *  Handle disk space allocation in the BRL-CAD v5 database.
 *
 *  Author -
 *	Michael John Muuss
 *  
 *  Source -
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5068  USA
 *  
 *  Distribution Status -
 *	Public Domain, Distribution Unlimited.
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
#include "bu.h"
#include "vmath.h"
#include "db5.h"
#include "raytrace.h"

#include "./debug.h"

/*
 *			D B 5 _ W R I T E _ F R E E
 *
 *  Create a v5 database "free" object of the specified size,
 *  and place it at the indicated location in the database.
 *
 *  Returns -
 *	0	OK
 *	-1	Fail
 */
int
db5_write_free( struct db_i *dbip, struct directory *dp, long length )
{
	struct bu_external	ext;

	RT_CK_DBI(dbip);
	RT_CK_DIR(dp);

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

/*
 *			D B 5 _ R E A L L O C
 *
 *  Change the size of a v5 database object.
 *
 *  If the object is getting smaller, break it into two pieces,
 *  and write out free objects for both.
 *  The caller is expected to re-write new data on the first one.
 *
 *  If the object is getting larger, see if it can be extended in place.
 *  If yes, write a free object that fits the new size,
 *  and a new free object for any remaining space.
 *
 *  If the ojbect is getting larger and there is no suitable "hole"
 *  in the database, extend the file, write a free object in the
 *  new space, and write a free object in the old space.
 *
 *  Returns -
 *	0	OK
 *	-1	Failure
 */
int
db5_realloc( struct db_i *dbip, struct directory *dp, struct bu_external *ep )
{
	RT_CK_DBI(dbip);
	RT_CK_DIR(dp);
	BU_CK_EXTERNAL(ep);
	if(rt_g.debug&DEBUG_DB) bu_log("db5_realloc(%s) dbip=x%x, dp=x%x, ext_nbytes=%ld\n",
		dp->d_namep, dbip, dp, ep->ext_nbytes );

	BU_ASSERT_LONG( ep->ext_nbytes&7, ==, 0 );

	if( dp->d_flags & RT_DIR_INMEM )  {
		if( dp->d_un.ptr )  {
			dp->d_un.ptr = bu_realloc( dp->d_un.ptr,
				ep->ext_nbytes, "db5_realloc() d_un.ptr" );
		} else {
			dp->d_un.ptr = bu_malloc( ep->ext_nbytes, "db5_realloc() d_un.ptr" );
		}
		dp->d_len = ep->ext_nbytes;
		return 0;
	}

	if( dbip->dbi_read_only )  {
		bu_log("db5_realloc on READ-ONLY file\n");
		return(-1);
	}

	/* Simple algorithm -- zap old copy, extend file for new copy */
	if( dp->d_addr != -1L )  {
		rt_memfree( &(dbip->dbi_freep), dp->d_len, dp->d_addr );
		db5_write_free( dbip, dp, dp->d_len );
		dp->d_addr = -1L;	/* sanity */
	}
	/* extend */
	dp->d_addr = dbip->dbi_eof;
	dbip->dbi_eof += ep->ext_nbytes;
	dp->d_len = ep->ext_nbytes;
	db5_write_free( dbip, dp, dp->d_len );
	return 0;

#if 0
	bu_bomb("db5_realloc() not fully implemented\n");
#endif
}
