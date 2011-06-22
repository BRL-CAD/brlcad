/* BRL-CAD	G 4 - G 5 . C
 *
 *
 * Copyright (c) 2004-2011 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 *
 */
/** @file conv/g4-g5.c
 *               g4-g5: program to convert version 4 databases to version 5
 *
 *               Imports version 4 database objects and writes out the equivalent v5 database
 *               objects as best it can.
 *
 *       USAGE
 *               g4-g5 v4_input_database v5_output_database
 *
 *
 *       AUTHOR
 *               John R. Anderson
 *
 *       EXAMPLE
 *               g4-g5 model_v4.g model_v5.g
 */

#include "common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bu.h"
#include "vmath.h"
#include "bn.h"
#include "raytrace.h"
#include "rtgeom.h"


int
main(int argc, char **argv)
{
    struct rt_wdb	*fp;
    struct db_i	*dbip;
    struct directory	*dp;
    long	errors = 0;
    struct bn_tol tol;
    char name[17];

    /* FIXME: These need to be improved */
    tol.magic = BN_TOL_MAGIC;
    tol.dist = 0.0005;
    tol.dist_sq = tol.dist * tol.dist;
    tol.perp = 1e-6;
    tol.para = 1 - tol.perp;

    bu_debug = BU_DEBUG_COREDUMP;

    rt_init_resource( &rt_uniresource, 0, NULL );

    if ( argc != 3 )  {
	fprintf(stderr, "Usage: %s v4.g v5.g\n", argv[0]);
	return 1;
    }

    if ( (dbip = db_open( argv[1], "r" )) == DBI_NULL )  {
	perror( argv[1] );
	return 2;
    }

    if ( (fp = wdb_fopen( argv[2] )) == NULL )  {
	perror( argv[2] );
	return 3;
    }

    if ( db_version(dbip) != 4 ) {
	bu_log( "Input database must be a version 4 datbase!!!!\n" );
	return 4;
    }

    RT_CK_DBI(dbip);
    if ( db_dirbuild( dbip ) )
	bu_exit(1, "db_dirbuild failed\n" );

    db_update_ident( fp->dbip, dbip->dbi_title, dbip->dbi_local2base );

    /* Retrieve every item in the input database */
    FOR_ALL_DIRECTORY_START(dp, dbip)  {
	struct rt_db_internal	intern;
	int id;
	int ret;

	fprintf(stderr, "%.16s\n", dp->d_namep );

	id = rt_db_get_internal( &intern, dp, dbip, NULL, &rt_uniresource );
	if ( id < 0 )  {
	    fprintf(stderr,
		    "%s: rt_db_get_internal(%s) failure, skipping\n",
		    argv[0], dp->d_namep);
	    errors++;
	    continue;
	}
	if ( id == ID_COMBINATION ) {
	    struct rt_comb_internal *comb;
	    char *ptr;

	    comb = (struct rt_comb_internal *)intern.idb_ptr;
	    RT_CK_COMB( comb );

	    /* Convert "plastic" to "phong" in the shader string */
	    while ( (ptr=strstr( bu_vls_addr( &comb->shader), "plastic" )) != NULL ) {
		ptr[0] = 'p'; /* p */
		ptr[1] = 'h'; /* l */
		ptr[2] = 'o'; /* a */
		ptr[3] = 'n'; /* s */
		ptr[4] = 'g'; /* t */
		ptr[5] = ' '; /* i */
		ptr[6] = ' '; /* c */
	    }
	}
	if ( id == ID_HF ) {
	    if (rt_hf_to_dsp( &intern )) {
		fprintf(stderr,
			"%s: Conversion from HF to DSP failed for solid %s\n",
			argv[0], dp->d_namep );
		errors++;
		continue;
	    }
	}
	if ( id == ID_POLY)
	{
	    if ( rt_pg_to_bot( &intern, &tol, &rt_uniresource ) )
	    {
		fprintf( stderr, "%s: Conversion from polysolid to BOT failed for solid %s\n",
			 argv[0], dp->d_namep );
		errors++;
		continue;
	    }
	}

	/* to insure null termination */
	bu_strlcpy( name, dp->d_namep, sizeof(name) );
	ret = wdb_put_internal( fp, name, &intern, 1.0 );
	if ( ret < 0 )  {
	    fprintf(stderr,
		    "%s: wdb_put_internal(%s) failure, skipping\n",
		    argv[0], dp->d_namep);
	    rt_db_free_internal(&intern);
	    errors++;
	    continue;
	}
	rt_db_free_internal(&intern);
    } FOR_ALL_DIRECTORY_END

	  wdb_close( fp );
    db_close( dbip );

    fprintf(stderr, "%ld objects failed to convert\n", errors);
    return 0;
}

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
