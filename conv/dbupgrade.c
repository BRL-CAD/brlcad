/******conv/dbupgrade.c
 *
 *	NAME
 *		dbupgrade: program to upgrade database files to the current version
 *
 *      COPYRIGHT
 *               Re-distribution of this software is restricted, as described in your
 *               "Statement of Terms and Conditions for the Release of The BRL-CAD
 *               Package" agreement.
 *
 *               Portions of this software and the associated databases and images are
 *               Copyright (C) 1985, 1986, 1987, 1988, 1989, 1991, 1994, 1995, 2000, 2001
 *               by the United States Army in all countries except the USA. Unauthorized
 *               redistribution in any form is prohibited. Recipients of this
 *               distribution are authorized unlimited use within their corporation,
 *               organization, or agency.
 *
 *               The mark "BRL-CAD" is a Registered Trademark of the United States Army.
 *               All rights reserved.
 *
 *               There is no warranty or other guarantee of fitness for this software,
 *               it is provided solely "as is".  Bug reports or fixes must be sent
 *               to ARL, which may or may not act on them.
 *
 *               This software was developed by and is proprietary to the United States
 *               Government, and any use of the software without the written permission
 *               of the US Army Research Laboratory is strictly prohibited. Any
 *               unauthorized use of the software will be construed as an infringement of
 *               the Government's rights and will subject the unauthorized user to
 *               action(s) for infringement damages. (BRL MFR 380-15, Feb-88)
 *
 *               The software in the "contributed" directory is Public Domain, Distribution
 *               Unlimited.  In addition, certain other modules in the BRL-CAD package
 *               bear this designation.  Files so marked are available for unlimited use.
 *	USAGE
 *		dbupgrade input.g output.g
 *
 *	DESCRIPTION
 *		Dbupgrade takes one input file and one output file name.
 *		It recognizes the version of the BRL-CAD database provided
 *		as input and converts it to the current database version.
 *		This code is intended to be upgraded as new database versions
 *		are created. Currently, only db version 4 can be upgraded.
 */


#include "conf.h"

#ifdef USE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif

#include <stdio.h>

#include "machine.h"
#include "bu.h"
#include "vmath.h"
#include "bn.h"
#include "raytrace.h"
#include "rtgeom.h"
#include "mater.h"


int
main(argc, argv)
int	argc;
char	**argv;
{
	struct rt_wdb	*fp;
	struct db_i	*dbip;
	struct db_i	*dbip4;
	struct directory	*dp;
	struct bn_tol tol;
	struct mater *mp;
	struct bu_vls colortab;
	char name[17];
	int reverse=0;
	int in_arg=1;
	int out_arg=2;
	long	errors = 0, skipped = 0;
	name[16] = '\0';

	/* this tolerance structure is only used for converting polysolids to BOT's
	 * use zero distance to avoid losing any polysolid facets
	 */
        tol.magic = BN_TOL_MAGIC;
        tol.dist = 0.0;
        tol.dist_sq = tol.dist * tol.dist;
        tol.perp = 1e-6;
        tol.para = 1 - tol.perp;

	bu_debug = BU_DEBUG_COREDUMP;

	rt_init_resource( &rt_uniresource, 0, NULL );

	if( argc != 3 && argc != 4 )  {
		fprintf(stderr, "Usage: %s input.g output.g\n", argv[0]);
		return 1;
	}

	if( argc == 4 ) {
		/* undocumented option to revert to an old db version
		 * currently, can only revert to db version 4
		 */
		if( strcmp( argv[1], "-r" ) ) {
			fprintf(stderr, "Usage: %s input.g output.g\n", argv[0]);
			return 1;
		} else {
			reverse = 1;
			in_arg = 2;
			out_arg = 3;
		}
	}

	if( !reverse ) {
		if( (dbip = db_open( argv[in_arg], "r" )) == DBI_NULL )  {
			perror( argv[in_arg] );
			return 2;
		}

		if( (fp = wdb_fopen( argv[out_arg] )) == NULL )  {
			perror( argv[out_arg] );
			return 3;
		}
	} else {
		if( (dbip = db_open( argv[in_arg], "r" )) == DBI_NULL )  {
			perror( argv[in_arg] );
			return 2;
		}
		if( (dbip4 = db_create( argv[out_arg], 4 )) == DBI_NULL ) {
			bu_log( "Failed to create output database (%s)\n", argv[out_arg] );
			return 3;
		}

		if( (fp = wdb_dbopen( dbip4, RT_WDB_TYPE_DB_DISK )) == RT_WDB_NULL ) {
			bu_log( "db_dbopen() failed for %s\n", argv[out_arg] );
			return 4;
		}
		
	}

	if( !reverse ) {
		if( dbip->dbi_version == 5 ) {
			bu_log( "This database (%s) is already at the current version\n",
				argv[in_arg] );
			return 5;
		}
		if( dbip->dbi_version != 4 ) {
			bu_log( "Input database version not recognized!!!!\n" );
			return 4;
		}
	} else if( reverse ) {
		if( dbip->dbi_version != 5 ) {
			bu_log( "Can only revert from db version 5\n" );
			return 6;
		}
	}


	RT_CK_DBI(dbip);
	db_dirbuild( dbip );

        if( (strcmp( dbip->dbi_title, "Untitled v4 BRL-CAD Database" )==0) && (dbip->dbi_version == 4) ) {
		dbip->dbi_title=bu_strdup( "Untitled BRL-CAD Database" );
        }
	db_update_ident( fp->dbip, dbip->dbi_title, dbip->dbi_local2base );

	/* set regionid color table */
	if( rt_material_head != MATER_NULL ) {
		bu_vls_init( &colortab );
		for( mp = rt_material_head; mp != MATER_NULL; mp = mp->mt_forw )  {
			bu_vls_printf( &colortab, "{%d %d %d %d %d} ", mp->mt_low, mp->mt_high,
				       mp->mt_r, mp->mt_g, mp->mt_b);
		}
		db5_update_attribute("_GLOBAL", "regionid_colortable", bu_vls_addr(&colortab), fp->dbip);
		bu_vls_free( &colortab );
	}
	

	/* Retrieve every item in the input database */
	FOR_ALL_DIRECTORY_START(dp, dbip)  {
		struct rt_db_internal	intern;
		int id;
		int ret;

		if( reverse && dp->d_major_type != DB5_MAJORTYPE_BRLCAD ) {
			bu_log( "\t%s not supported in version4 databases, not converted\n",
				dp->d_namep);
			skipped++;
			continue;
		}
		id = rt_db_get_internal( &intern, dp, dbip, NULL, &rt_uniresource );
		if( id < 0 )  {
			fprintf(stderr,
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
				fprintf(stderr,
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
				fprintf( stderr, "%s: Conversion from polysolid to BOT failed for solid %s\n",
					argv[0], dp->d_namep );
				errors++;
				continue;
			}
		}

		/* to insure null termination */
		strncpy( name, dp->d_namep, 16 );
		ret = wdb_put_internal( fp, name, &intern, 1.0 );
		if( ret < 0 )  {
			fprintf(stderr,
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

	fprintf(stderr, "%ld objects failed to convert\n", errors);
	return 0;
}
