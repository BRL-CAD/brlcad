/*                           W D B . C
 * BRL-CAD
 *
 * Copyright (c) 2000-2006 United States Government as represented by
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

/** \addtogroup wdb */
/*@{*/
/** @file wdb.c
 *  Routines to allow libwdb to use librt's import/export interface,
 *  rather than having to know about the database formats directly.
 *
 *  Author -
 *	Michael John Muuss
 *
 *  Source -
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5068  USA
 */
/*@}*/

#ifndef lint
static const char RCSid[] = "@(#)$Header$ (ARL)";
#endif

#include "common.h"

#include <stdio.h>
#include <math.h>

#include "machine.h"
#include "bu.h"
#include "vmath.h"
#include "bn.h"
#include "rtgeom.h"
#include "raytrace.h"
#include "wdb.h"

/*
 *			W D B _ F O P E N
 *
 *  Create a libwdb output stream destined for a disk file.
 *  This will destroy any existing file by this name, and start fresh.
 *  The file is then opened in the normal "update" mode and
 *  an in-memory directory is built along the way,
 *  allowing retrievals and object replacements as needed.
 *
 *  Users can change the database title by calling: ???
 */
struct rt_wdb *
wdb_fopen_v( const char *filename, int version )
{
	struct db_i	*dbip;

	if( rt_uniresource.re_magic != RESOURCE_MAGIC )
		rt_init_resource( &rt_uniresource, 0, NULL );

	if( (dbip = db_create( filename, version )) == DBI_NULL )
		return RT_WDB_NULL;

	return wdb_dbopen( dbip, RT_WDB_TYPE_DB_DISK );
}

struct rt_wdb *
wdb_fopen( const char *filename)
{
    return wdb_fopen_v(filename, 5);
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
wdb_dbopen( struct db_i *dbip, int mode )
{
	struct rt_wdb	*wdbp;

	RT_CK_DBI(dbip);

	if (mode != RT_WDB_TYPE_DB_DISK	 && mode != RT_WDB_TYPE_DB_DISK_APPEND_ONLY &&
	    mode != RT_WDB_TYPE_DB_INMEM && mode != RT_WDB_TYPE_DB_INMEM_APPEND_ONLY) {
		bu_log("wdb_dbopen(%s) mode %d unknown\n",
			dbip->dbi_filename, mode );
		return RT_WDB_NULL;
	}

#if 0
	if( (mode == RT_WDB_TYPE_DB_DISK || mode == RT_WDB_TYPE_DB_DISK_APPEND_ONLY ) &&
	    dbip->dbi_read_only )  {
		/* In-mem updates happen regardless of disk read-only flag */
		bu_log("wdb_dbopen(%s): read-only\n",
			dbip->dbi_filename );
	}
#endif

	if( rt_uniresource.re_magic != RESOURCE_MAGIC )
		rt_init_resource( &rt_uniresource, 0, NULL );

	BU_GETSTRUCT(wdbp, rt_wdb);
	wdbp->l.magic = RT_WDB_MAGIC;
	wdbp->type = mode;
	wdbp->dbip = dbip;
	wdbp->dbip->dbi_wdbp = wdbp;

	/* Provide the same default tolerance that librt/prep.c does */
	wdbp->wdb_tol.magic = BN_TOL_MAGIC;
	wdbp->wdb_tol.dist = 0.005;
	wdbp->wdb_tol.dist_sq = wdbp->wdb_tol.dist * wdbp->wdb_tol.dist;
	wdbp->wdb_tol.perp = 1e-6;
	wdbp->wdb_tol.para = 1 - wdbp->wdb_tol.perp;

	wdbp->wdb_ttol.magic = RT_TESS_TOL_MAGIC;
	wdbp->wdb_ttol.abs = 0.0;
	wdbp->wdb_ttol.rel = 0.01;
	wdbp->wdb_ttol.norm = 0;
	bu_vls_init( &wdbp->wdb_prestr );

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
 *
 *  NON-PARALLEL because of rt_uniresource
 */
int
wdb_import(struct rt_wdb *wdbp,	struct rt_db_internal *internp,	const char *name, const mat_t mat )
{
	struct directory	*dp;

	if( (dp = db_lookup( wdbp->dbip, name, LOOKUP_QUIET )) == DIR_NULL )
		return -4;

	return rt_db_get_internal( internp, dp, wdbp->dbip, mat, &rt_uniresource );
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
wdb_export_external(
	struct rt_wdb *wdbp,
	struct bu_external *ep,
	const char *name,
	int flags,
	unsigned char type)
{
	struct directory	*dp;

	RT_CK_WDB(wdbp);
	BU_CK_EXTERNAL(ep);

	/* Stash name into external representation */
	if( wdbp->dbip->dbi_version <= 4 )  {
		db_wrap_v4_external( ep, name );
	} else if( wdbp->dbip->dbi_version == 5 )  {
		if( db_wrap_v5_external( ep, name ) < 0 )  {
			bu_log("wdb_export_external(%s): db_wrap_v5_external error\n",
				name );
			return -4;
		}
	} else {
		bu_log("wdb_export_external(%s): version %d unsupported\n",
				name, wdbp->dbip->dbi_version );
		return -4;
	}

	switch( wdbp->type )  {

	case RT_WDB_TYPE_DB_DISK:
		if( wdbp->dbip->dbi_read_only )  {
			bu_log("wdb_export_external(%s): read-only database, write aborted\n");
			return -5;
		}
		/* If name already exists, that object will be updated. */
		dp = db_lookup( wdbp->dbip, name, LOOKUP_QUIET );
		if( dp == DIR_NULL ) {
			if( (dp = db_diradd( wdbp->dbip, name, -1L, 0, flags,
					   (genptr_t)&type )) == DIR_NULL )  {
			bu_log("wdb_export_external(%s): db_diradd error\n",
			       name );
			return -3;
			}
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
		if( (dp = db_diradd( wdbp->dbip, name, -1L, 0, flags,
				    (genptr_t)&type )) == DIR_NULL )  {
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
		if( (dp = db_diradd( wdbp->dbip, name, -1L, 0, flags,
				(genptr_t)&type )) == DIR_NULL )  {
			bu_log("wdb_export_external(%s): db_diradd error\n",
			       name );
			return -3;
		}

		db_inmem( dp, ep, flags, wdbp->dbip );
		/* ep->buf has been stolen, replaced with null. */
		break;

	case RT_WDB_TYPE_DB_INMEM:
		if( (dp = db_lookup( wdbp->dbip, name, 0 )) == DIR_NULL )  {
			if( (dp = db_diradd( wdbp->dbip, name, -1L, 0, flags,
					(genptr_t)&type )) == DIR_NULL )  {
				bu_log("wdb_export_external(%s): db_diradd error\n",
				       name );
				bu_free_external( ep );
				return -3;
			}
		} else {
			dp->d_flags = (dp->d_flags & ~7) | flags;
		}

		db_inmem( dp, ep, flags, wdbp->dbip );
		/* ep->buf has been stolen, replaced with null. */
		break;
	}

	return 0;
}

/*
 *			W D B _ P U T _ I N T E R N A L
 *
 *  Convert the internal representation of a solid to the external one,
 *  and write it into the database.
 *  The internal representation is always freed.
 *  This is the analog of rt_db_put_internal() for rt_wdb objects.
 *
 *  Use this routine in preference to wdb_export() whenever the
 *  caller already has an rt_db_internal structure handy.
 *
 *  NON-PARALLEL because of rt_uniresource
 *
 *  Returns -
 *	 0	OK
 *	<0	error
 */
int
wdb_put_internal(
	struct rt_wdb *wdbp,
	const char *name,
	struct rt_db_internal *ip,
	double local2mm )
{
	struct bu_external	ext;
	int			ret;
	int			flags;

	RT_CK_WDB(wdbp);
	RT_CK_DB_INTERNAL(ip);

	if( wdbp->dbip->dbi_version <= 4 )  {
		BU_INIT_EXTERNAL( &ext );
		ret = ip->idb_meth->ft_export( &ext, ip, local2mm, wdbp->dbip, &rt_uniresource );
		if( ret < 0 )  {
			bu_log("rt_db_put_internal(%s):  solid export failure\n",
				name);
			ret = -1;
			goto out;
		}
		db_wrap_v4_external( &ext, name );
	} else {
		if( rt_db_cvt_to_external5( &ext, name, ip, local2mm, wdbp->dbip, &rt_uniresource, ip->idb_major_type ) < 0 )  {
			bu_log("wdb_export(%s): solid export failure\n",
				name );
			ret = -2;
			goto out;
		}
	}
	BU_CK_EXTERNAL( &ext );

	flags = db_flags_internal( ip );
	ret = wdb_export_external( wdbp, &ext, name, flags, ip->idb_type );
out:
	bu_free_external( &ext );
	rt_db_free_internal( ip, &rt_uniresource );
	return ret;
}

/*
 *			W D B _ E X P O R T
 *
 *  Export an in-memory representation of an object,
 *  as described in the file h/rtgeom.h, into the indicated database.
 *
 *  The internal representation (gp) is always freed.
 *
 *  WARNING: The caller must be careful not to double-free gp,
 *  particularly if it's been extracted from an rt_db_internal,
 *  e.g. by passing intern.idb_ptr for gp.
 *
 *  If the caller has an rt_db_internal structure handy already,
 *  they should call wdb_put_internal() directly -- this is a
 *  convenience routine intended primarily for internal use in LIBWDB.
 *
 *  Returns -
 *	 0	OK
 *	<0	error
 */
int
wdb_export(
	struct rt_wdb *wdbp,
	const char *name,
	genptr_t gp,
	int id,
	double local2mm )
{
	struct rt_db_internal	intern;

	RT_CK_WDB(wdbp);

	if( (id <= 0 || id > ID_MAX_SOLID) && id != ID_COMBINATION )  {
		bu_log("wdb_export(%s): id=%d bad\n",
			name, id );
		return(-1);
	}

	RT_INIT_DB_INTERNAL( &intern );
	intern.idb_major_type = DB5_MAJORTYPE_BRLCAD;
	intern.idb_type = id;
	intern.idb_ptr = gp;
	intern.idb_meth = &rt_functab[id];

	return wdb_put_internal( wdbp, name, &intern, local2mm );
}

/*
 *			W D B _ C L O S E
 *
 *  Release from associated database "file", destroy dynamic data structure.
 */
void
wdb_close( struct rt_wdb *wdbp )
{

	RT_CK_WDB(wdbp);

	/* XXX Flush any unwritten "struct matter" records here */

	db_close( wdbp->dbip );

	bu_vls_free( &wdbp->wdb_prestr );
	bu_free( (genptr_t)wdbp, "struct rt_wdb");
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
