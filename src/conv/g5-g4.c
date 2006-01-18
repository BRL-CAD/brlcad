/* BRL-CAD	G 5 - G 4 . C
 *
 * Copyright (c) 2004-2006 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this file; see the file named COPYING for more
 * information.
 *
 */
/** @file conv/g5-g4.c
 *               g5-g4: program to convert version 5 databases to version 4
 *
 * USAGE
 * 	g5-g4 v5_input_database v4_output_database
 *
 * DESCRIPTION
 *	Imports version 5 database objects and writes out the equivalent v4 database
 *	objects as best it can. Note that some v5 objects cannot be represented in a
 *	version 4 database.
 *
 * AUTHOR
 *	John R. Anderson
 *
 * EXAMPLE
 *	g5-g4 model_v5.g model_v4.g
 */

#include "common.h"

#ifdef HAVE_STRING_H
#  include <string.h>
#else
#  include <strings.h>
#endif

#include <stdio.h>

#include "machine.h"
#include "bu.h"
#include "vmath.h"
#include "bn.h"
#include "raytrace.h"
#include "rtgeom.h"
#include "wdb.h"


int
main(int argc, char **argv)
{
	struct rt_wdb	*fp;
	struct db_i	*dbip, *dbip4;
	struct directory	*dp;
	long	errors = 0, skipped = 0;
	struct bn_tol tol;

        /* XXX These need to be improved */
        tol.magic = BN_TOL_MAGIC;
        tol.dist = 0.005;
        tol.dist_sq = tol.dist * tol.dist;
        tol.perp = 1e-6;
        tol.para = 1 - tol.perp;

	bu_debug = BU_DEBUG_COREDUMP;

	rt_init_resource( &rt_uniresource, 0, NULL );

	if( argc != 3 )  {
		bu_log( "Usage: %s v5.g v4.g\n", argv[0]);
		return 1;
	}

	if( (dbip = db_open( argv[1], "r" )) == DBI_NULL )  {
		perror( argv[1] );
		return 2;
	}

	if( rt_uniresource.re_magic != RESOURCE_MAGIC )
		rt_init_resource( &rt_uniresource, 0, NULL );

	if( (dbip4 = db_create( argv[2], 4 )) == DBI_NULL ) {
		bu_log( "Failed to create output database (%s)\n", argv[2] );
		return 3;
	}

	if( (fp = wdb_dbopen( dbip4, RT_WDB_TYPE_DB_DISK )) == RT_WDB_NULL ) {
		bu_log( "db_dbopen() failed for %s\n", argv[2] );
		return 4;
	}

	if( dbip->dbi_version != 5 ) {
		bu_log( "Input database must be a version 5 database!!!!\n" );
		return 5;
	}

	RT_CK_DBI(dbip);
	db_dirbuild( dbip );

	mk_id_units( fp, dbip->dbi_title, bu_units_string( dbip->dbi_local2base ) );

	/* Retrieve every item in the input database */
	FOR_ALL_DIRECTORY_START(dp, dbip)  {
		struct rt_db_internal	intern;
		int id;
		int ret;

		bu_log( "%s\n", dp->d_namep );
		if( dp->d_major_type != DB5_MAJORTYPE_BRLCAD ) {
			bu_log( "\tThis object not supported in version4 databases, not converted\n" );
			skipped++;
			continue;
		}

		id = rt_db_get_internal( &intern, dp, dbip, NULL, &rt_uniresource );
		if( id < 0 )  {
			bu_log(
				"%s: rt_db_get_internal(%s) failure, skipping\n",
				argv[0], dp->d_namep);
			errors++;
			continue;
		}
		if( id == ID_COMBINATION ) {
			struct rt_comb_internal *comb;
			char *ptr;

			comb = (struct rt_comb_internal *)intern.idb_ptr;
			RT_CK_COMB( comb );

			/* Convert "plastic" to "phong" in the shader string */
			while( (ptr=strstr( bu_vls_addr( &comb->shader), "plastic" )) != NULL ) {
				strncpy( ptr, "phong  ", 7 );
			}
		}
		if ( id == ID_HF ) {
			if (rt_hf_to_dsp( &intern, &rt_uniresource )) {
				bu_log(
					"%s: Conversion from HF to DSP failed for solid %s\n",
					argv[0], dp->d_namep );
				errors++;
				continue;
			}
		}
		if( id == ID_POLY)
		{
			if( rt_pg_to_bot( &intern, &tol, &rt_uniresource ) )
			{
				bu_log( "%s: Conversion from polysolid to BOT failed for solid %s\n",
					argv[0], dp->d_namep );
				errors++;
				continue;
			}
		}
		ret = wdb_put_internal( fp, dp->d_namep, &intern, 1.0 );
		if( ret < 0 )  {
			bu_log(
				"%s: wdb_put_internal(%s) failure, skipping\n",
				argv[0], dp->d_namep);
			rt_db_free_internal( &intern, &rt_uniresource );
			errors++;
			continue;
		}
		rt_db_free_internal( &intern, &rt_uniresource );
	} FOR_ALL_DIRECTORY_END

	wdb_close( fp );
	db_close( dbip );

	bu_log( "%ld database version 5 specific objects not attempted to convert\n", skipped );
	bu_log( "%ld attempted objects failed to convert\n", errors);
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
