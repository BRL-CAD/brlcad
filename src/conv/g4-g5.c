#include "common.h"



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



int
main(int argc, char **argv)
{
	struct rt_wdb	*fp;
	struct db_i	*dbip;
	struct directory	*dp;
	long	errors = 0;
	struct bn_tol tol;
	char name[17];

	name[16] = '\0';

        /* XXX These need to be improved */
        tol.magic = BN_TOL_MAGIC;
        tol.dist = 0.005;
        tol.dist_sq = tol.dist * tol.dist;
        tol.perp = 1e-6;
        tol.para = 1 - tol.perp;

	bu_debug = BU_DEBUG_COREDUMP;

	rt_init_resource( &rt_uniresource, 0, NULL );

	if( argc != 3 )  {
		fprintf(stderr, "Usage: %s v4.g v5.g\n", argv[0]);
		return 1;
	}

	if( (dbip = db_open( argv[1], "r" )) == DBI_NULL )  {
		perror( argv[1] );
		return 2;
	}

	if( (fp = wdb_fopen( argv[2] )) == NULL )  {
		perror( argv[2] );
		return 3;
	}

	if( dbip->dbi_version != 4 ) {
		bu_log( "Input database must be a version 4 datbase!!!!\n" );
		return 4;
	}

	RT_CK_DBI(dbip);
	db_dirbuild( dbip );

	db_update_ident( fp->dbip, dbip->dbi_title, dbip->dbi_local2base );

	/* Retrieve every item in the input database */
	FOR_ALL_DIRECTORY_START(dp, dbip)  {
		struct rt_db_internal	intern;
		int id;
		int ret;

		fprintf(stderr, "%.16s\n", dp->d_namep );

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

























































