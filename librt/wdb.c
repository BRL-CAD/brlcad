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
 *  The stream is established in "append-only" mode.
 */
struct rt_wdb *
wdb_fopen( filename )
CONST char *filename;
{
	struct rt_wdb	*wdbp;
	FILE		*fp;
	struct db_i	*dbip;

	if( (dbip = db_create( filename )) == DBI_NULL )
		return RT_WDB_NULL;
	db_close(dbip);

	if( (fp = fopen( filename, "ab" )) == NULL )
		return RT_WDB_NULL;

	BU_GETSTRUCT(wdbp, rt_wdb);
	wdbp->l.magic = RT_WDB_MAGIC;
	wdbp->type = RT_WDB_TYPE_FILE;
	wdbp->fp = fp;
	return wdbp;
}

/*
 *			W D B _ D B O P E N
 *
 *  Create a libwdb output stream destined for an existing BRL-CAD database,
 *  already opened via a db_open() call.
 *	RT_WDB_TYPE_DB_DISK			Add to on-disk database
 *	RT_WDB_TYPE_DB_DISK_APPEND_ONLY		Add to on-disk database, don't clobber existing names, use prefix
 *	RT_WDB_TYPE_DB_INMEM			Add to in-memory database only
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

	if( (mode == RT_WDB_TYPE_DB_DISK || mode == RT_WDB_TYPE_DB_DISK_APPEND_ONLY ) &&
	    dbip->dbi_read_only )  {
		/* In-mem updates happen regardless of disk read-only flag */
		bu_log("wdb_dbopen(%s): read-only\n",
			dbip->dbi_filename );
	}

	BU_GETSTRUCT(wdbp, rt_wdb);
	wdbp->l.magic = RT_WDB_MAGIC;
	wdbp->type = mode;
	wdbp->dbip = dbip;

	dbip->dbi_uses++;

	return wdbp;
}

/* 
 *			W D B _ I M P O R T
 *
 *  Returns -
 *	0	and modified *internp;
 *	-1	ft_import failure (from rt_db_get_internal)
 *	-2	db_get_external failure (from rt_db_get_internal)
 *	-3	Attempt to import from write-only (stream) file.
 *	-4	Name not found in database TOC.
 */
int
wdb_import( wdbp, internp, name, mat )
struct rt_wdb			*wdbp;
struct rt_db_internal		*internp;
CONST char			*name;
CONST mat_t			mat;
{
	struct directory	*dp;

	if( wdbp->type == RT_WDB_TYPE_FILE )
		return -3;	/* No table of contents, file is write-only */

	if( (dp = db_lookup( wdbp->dbip, name, LOOKUP_QUIET )) == DIR_NULL )
		return -4;

	return rt_db_get_internal( internp, dp, wdbp->dbip, mat );
}

/*
 *			W D B _ E X P O R T _ E X T E R N A L
 *
 *  The caller must free "ep".
 *
 *  Returns -
 *	 0	OK
 *	<0	error
 */
int
wdb_export_external( wdbp, ep, name, flags )
struct rt_wdb		*wdbp;
struct bu_external	*ep;
CONST char		*name;
int			flags;
{
	struct directory	*dp;

	RT_CK_WDB(wdbp);
	BU_CK_EXTERNAL(ep);
	switch( wdbp->type )  {

	case RT_WDB_TYPE_FILE:
		{
			union record	*rec;

			/* v4: Depends on solid names always being in the same place */
			rec = (union record *)ep->ext_buf;
			NAMEMOVE( name, rec->s.s_name );
		}

		if( fwrite( ep->ext_buf, ep->ext_nbytes, 1, wdbp->fp ) != 1 )  {
			bu_log("wdb_export_external(%s): fwrite error\n",
				name );
			return(-3);
		}
		break;

	case RT_WDB_TYPE_DB_DISK:
		if( wdbp->dbip->dbi_read_only )  {
			bu_log("wdb_export_external(%s): read-only database, write aborted\n");
			return -5;
		}
		/* If name already exists, that object will be updated. */
		if( (dp = db_lookup( wdbp->dbip, name, LOOKUP_QUIET )) == DIR_NULL &&
		    (dp = db_diradd( wdbp->dbip, name, -1L, 0, flags, NULL )) == DIR_NULL )  {
			bu_log("wdb_export_external(%s): db_diradd error\n",
				name );
			return -3;
		}
		dp->d_flags = (dp->d_flags & ~7) | flags;
		if( db_put_external( ep, dp, wdbp->dbip ) < 0 )  {
			bu_log("wdb_export_external(%s): db_put_external error\n",
				name );
			return -3;
		}
		break;

	case RT_WDB_TYPE_DB_DISK_APPEND_ONLY:
		if( wdbp->dbip->dbi_read_only )  {
			bu_log("wdb_export_external(%s): read-only database, write aborted\n");
			return -5;
		}
		/* If name already exists, new non-conflicting name will be generated */
		if( (dp = db_diradd( wdbp->dbip, name, -1L, 0, flags, NULL )) == DIR_NULL )  {
			bu_log("wdb_export_external(%s): db_diradd error\n",
				name );
			return -3;
		}
		if( db_put_external( ep, dp, wdbp->dbip ) < 0 )  {
			bu_log("wdb_export_external(%s): db_put_external error\n",
				name );
			return -3;
		}
		break;

	case RT_WDB_TYPE_DB_INMEM_APPEND_ONLY:
		if( (dp = db_lookup( wdbp->dbip, name, 0 )) != DIR_NULL )  {
			bu_log("wdb_export_external(%s): ERROR, that name is already in use, and APPEND_ONLY mode has been specified.\n",
				name );
			return -3;
		}
		if( (dp = db_diradd( wdbp->dbip, name, -1L, 0, flags, NULL )) == DIR_NULL )  {
			bu_log("wdb_export_external(%s): db_diradd error\n",
				name );
			return -3;
		}

		/* Stash name into external representation */
		if( db_wrap_v4_external( ep, ep, dp ) < 0 )  {
			bu_log("wdb_export_external(%s): db_wrap_v4_external error\n",
				name );
			return -4;
		}

		db_inmem( dp, ep, flags );
		/* ep->buf has been stolen, replaced with null. */
		break;

	case RT_WDB_TYPE_DB_INMEM:
		if( (dp = db_lookup( wdbp->dbip, name, 0 )) == DIR_NULL )  {
			if( (dp = db_diradd( wdbp->dbip, name, -1L, 0, flags, NULL )) == DIR_NULL )  {
				bu_log("wdb_export_external(%s): db_diradd error\n",
					name );
				bu_free_external( ep );
				return -3;
			}
		} else {
			dp->d_flags = (dp->d_flags & ~7) | flags;
		}

		/* Stash name into external representation */
		if( db_wrap_v4_external( ep, ep, dp ) < 0 )  {
			bu_log("wdb_export_external(%s): db_wrap_v4_external error\n",
				name );
			bu_free_external( ep );
			return -4;
		}

		db_inmem( dp, ep, flags );
		/* ep->buf has been stolen, replaced with null. */
		break;
	}
	return 0;
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
CONST char	*name;
genptr_t	gp;
int		id;
double		local2mm;
{
	struct rt_db_internal	intern;
	struct bu_external	ext;
	int			ret;

	if( (id <= 0 || id > ID_MAXIMUM) && id != ID_COMBINATION )  {
		bu_log("wdb_export(%s): id=%d bad\n",
			name, id );
		return(-1);
	}

	RT_INIT_DB_INTERNAL( &intern );
	intern.idb_type = id;
	intern.idb_ptr = gp;

	if( rt_functab[id].ft_export( &ext, &intern, local2mm, wdbp->dbip ) < 0 )  {
		bu_log("wdb_export(%s): solid export failure\n",
			name );
		bu_free_external( &ext );
		return(-2);				/* FAIL */
	}
	BU_CK_EXTERNAL( &ext );

	ret = wdb_export_external( wdbp, &ext, name, db_flags_internal( &intern ) );
	bu_free_external( &ext );
	return ret;
}

/*
 *			W D B _ C L O S E
 *
 *  Release from associated database "file", destroy dyanmic data structure.
 */
void
wdb_close( wdbp )
struct rt_wdb	*wdbp;
{

	RT_CK_WDB(wdbp);
	if( wdbp->type == RT_WDB_TYPE_FILE )  {
		fclose( wdbp->fp );
	} else {
		/* db_i is use counted */
		db_close( wdbp->dbip );
	}
	bu_free( (genptr_t)wdbp, "struct rt_wdb");
}
