/*
 *			W D B . C
 *
 *  Routines to allow libwdb to use librt's import/export interface,
 *  rather than having to know about the database formats directly.
 *
 *  Author -
 *	Michael John Muuss
 *  
 *  Source -
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5068  USA
 *  
 *  Distribution Notice -
 *	Re-distribution of this software is restricted, as described in
 *	your "Statement of Terms and Conditions for the Release of
 *	The BRL-CAD Package" agreement.
 *
 *  Copyright Notice -
 *	This software is Copyright (C) 1996 by the United States Army
 *	in all countries except the USA.  All rights reserved.
 */
#ifndef lint
static char RCSid[] = "@(#)$Header$ (ARL)";
#endif

#include "conf.h"

#include <stdio.h>
#include <math.h>
#include "machine.h"
#include "bu.h"
#include "db.h"
#include "vmath.h"
#include "bn.h"
#include "rtgeom.h"
#include "raytrace.h"
#include "wdb.h"

/*
 *			W D B _ F O P E N
 *
 *  Create a libwdb output stream destined for a disk file.
 */
struct rt_wdb *
wdb_fopen( filename )
CONST char *filename;
{
	struct rt_wdb	*wdbp;
	FILE		*fp;

	if( (fp = fopen( filename, "wb" )) == NULL )
		return RT_WDB_NULL;

	GETSTRUCT(wdbp, rt_wdb);
	wdbp->magic = RT_WDB_MAGIC;
	wdbp->type = RT_WDB_TYPE_FILE;
	wdbp->fp = fp;
	return wdbp;
}

/*
 *			W D B _ D B O P E N
 *
 *  Create a libwdb output stream destined for an existing BRL-CAD database,
 *  already opened via a db_open() call.
 *	RT_WDB_TYPE_DB_DISK		Add to on-disk database
 *	RT_WDB_TYPE_DB_INMEM		Add to in-memory database only
 *	RT_WDB_TYPE_DB_INMEM_APPEND_ONLY	Ditto, but give errors if name in use.
 */
struct rt_wdb *
wdb_dbopen( dbip, mode )
struct db_i	*dbip;
int		mode;
{
	struct rt_wdb	*wdbp;

	RT_CK_DBI(dbip);

	if( mode != RT_WDB_TYPE_DB_DISK && mode != RT_WDB_TYPE_DB_INMEM && mode != RT_WDB_TYPE_DB_DISK )  {
		bu_log("wdb_dbopen(%s) mode %d unknown\n",
			dbip->dbi_filename, mode );
		return RT_WDB_NULL;
	}

	if( mode == RT_WDB_TYPE_DB_DISK && dbip->dbi_read_only )  {
		/* In-mem updates happen regardless of disk read-only flag */
		bu_log("wdb_dbopen(%s) database is read-only, aborting\n",
			dbip->dbi_filename );
		return RT_WDB_NULL;
	}

	GETSTRUCT(wdbp, rt_wdb);
	wdbp->magic = RT_WDB_MAGIC;
	wdbp->type = mode;
	wdbp->dbip = dbip;
	return wdbp;
}

/* XXX move to another module.  db_alloc(), probably. */
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

/*
 *			W D B _ E X P O R T
 *
 *  The caller must free "gp".
 *
 *  Returns -
 *	 0	OK
 *	<0	error
 */
int
wdb_export( wdbp, name, gp, id, local2mm )
struct rt_wdb	*wdbp;
char		*name;
genptr_t	gp;
int		id;
double		local2mm;
{
	struct rt_db_internal	intern;
	struct rt_external	ext;
	struct directory	*dp;
	int			flags;

	if( (id <= 0 || id > ID_MAXIMUM) && id != ID_COMBINATION )  {
		rt_log("wdb_export(%s): id=%d bad\n",
			name, id );
		return(-1);
	}

	RT_INIT_DB_INTERNAL( &intern );
	intern.idb_type = id;
	intern.idb_ptr = gp;

	if( rt_functab[id].ft_export( &ext, &intern, local2mm ) < 0 )  {
		bu_log("wdb_export(%s): solid export failure\n",
			name );
		db_free_external( &ext );
		return(-2);				/* FAIL */
	}
	RT_CK_EXTERNAL( &ext );

	switch( wdbp->type )  {

	case RT_WDB_TYPE_FILE:
		{
			union record	*rec;

			/* v4: Depends on solid names always being in the same place */
			rec = (union record *)ext.ext_buf;
			NAMEMOVE( name, rec->s.s_name );
		}

		if( fwrite( ext.ext_buf, ext.ext_nbytes, 1, wdbp->fp ) != 1 )  {
			bu_log("wdb_export(%s): fwrite error\n",
				name );
			db_free_external( &ext );
			return(-3);
		}
		break;

	case RT_WDB_TYPE_DB_DISK:
		flags = db_flags_internal( &intern );
		/* If name already exists, temporary name will be generated */
		if( (dp = db_diradd( wdbp->dbip, name, -1L, 0, flags )) == DIR_NULL )  {
			bu_log("wdb_export(%s): db_diradd error\n",
				name );
			db_free_external( &ext );
			return -3;
		}
		if( db_put_external( &ext, dp, wdbp->dbip ) < 0 )  {
			bu_log("wdb_export(%s): db_put_external error\n",
				name );
			db_free_external( &ext );
			return -3;
		}
		break;

	case RT_WDB_TYPE_DB_INMEM_APPEND_ONLY:
		if( (dp = db_lookup( wdbp->dbip, name, 0 )) != DIR_NULL )  {
			bu_log("wdb_export(%s): ERROR, that name is already in use, and APPEND_ONLY mode has been specified.\n",
				name );
			db_free_external( &ext );
			return -3;
		}
		if( (dp = db_diradd( wdbp->dbip, name, -1L, 0, 0 )) == DIR_NULL )  {
			bu_log("wdb_export(%s): db_diradd error\n",
				name );
			db_free_external( &ext );
			return -3;
		}

		/* Stash name into external representation */
		if( db_wrap_v4_external( &ext, &ext, dp ) < 0 )  {
			bu_log("wdb_export(%s): db_wrap_v4_external error\n",
				name );
			db_free_external( &ext );
			return -4;
		}

		db_inmem( dp, &ext, db_flags_internal(&intern) );
		/* ext->buf has been taken; extra free call is harmless */
		break;

	case RT_WDB_TYPE_DB_INMEM:
		if( (dp = db_lookup( wdbp->dbip, name, 0 )) == DIR_NULL )  {
			if( (dp = db_diradd( wdbp->dbip, name, -1L, 0, 0 )) == DIR_NULL )  {
				bu_log("wdb_export(%s): db_diradd error\n",
					name );
				db_free_external( &ext );
				return -3;
			}
		}

		/* Stash name into external representation */
		if( db_wrap_v4_external( &ext, &ext, dp ) < 0 )  {
			bu_log("wdb_export(%s): db_wrap_v4_external error\n",
				name );
			db_free_external( &ext );
			return -4;
		}

		db_inmem( dp, &ext, db_flags_internal(&intern) );
		/* ext->buf has been taken; extra free call is harmless */
		break;
	}
	db_free_external( &ext );
	return(0);
}
