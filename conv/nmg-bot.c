/* Routine to convert all the NMG solids in a BRL-CAD model to BoTs
 *
 *  Author -
 *      John R. Anderson
 *  
 *  Source -
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5068  USA
 *  
 *  Distribution Notice -
 *	Re-distribution of this software is restricted, as described in
 *	your "Statement of Terms and Conditions for the Release of
 *	The BRL-CAD Pacakge" agreement.
 *
 *  Copyright Notice -
 *	This software is Copyright (C) 1998-2004 by the United States Army
 *	in all countries except the USA.  All rights reserved.
 */


#ifndef lint
static const char RCSid[] = "@(#)$Header$ (BRL)";
#endif
 
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif



#include <stdio.h>

#ifdef USE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif

#include <ctype.h>
#include "machine.h"
#include "vmath.h"
#include "externs.h"
#include "db.h"
#include "bu.h"
#include "nmg.h"
#include "raytrace.h"
#include "rtgeom.h"
#include "wdb.h"

static struct db_i *dbip;
static int verbose=0;
static struct rt_wdb *fdout=NULL;

static void
nmg_conv(struct rt_db_internal *intern, const char *name )
{
	struct model *m;
	struct nmgregion *r;
	struct shell *s;

	RT_CK_DB_INTERNAL(intern);
	m = (struct model *)intern->idb_ptr;
	NMG_CK_MODEL( m );
	r = BU_LIST_FIRST( nmgregion, &m->r_hd );
	if( BU_LIST_NEXT( nmgregion, &r->l ) !=  (struct nmgregion *)&m->r_hd )
		bu_bomb( "ERROR: this code works only for NMG models with one region!!!\n" );

	s = BU_LIST_FIRST( shell, &r->s_hd );
	if( BU_LIST_NEXT( shell, &s->l) != (struct shell *)&r->s_hd )
		bu_bomb( "ERROR: this code works only for NMG models with one shell!!!\n" );

	if( BU_SETJUMP )
	{
		BU_UNSETJUMP;
		bu_log( "Failed to convert %s\n", name );
		rt_db_free_internal( intern, &rt_uniresource );
		return;
	}
	mk_bot_from_nmg( fdout, name, s);
	BU_UNSETJUMP;
	if(verbose) bu_log("Converted %s to a Bot solid\n", name);
	rt_db_free_internal( intern, &rt_uniresource );
}

int
main(int argc, char **argv)
{
	struct directory *dp;

	if( argc != 3 && argc != 4 )
	{
		bu_log( "Usage:\n\t%s [-v] input.g output.g\n", argv[0] );
		exit( 1 );
	}

	if( argc == 4 )
	{
		if( !strcmp( argv[1], "-v" ) )
			verbose = 1;
		else
		{
			bu_log( "Illegal option: %s\n", argv[1] );
			bu_log( "Usage:\n\t%s [-v] input.g output.g\n", argv[0] );
			exit( 1 );
		}
	}

	rt_init_resource( &rt_uniresource, 0, NULL );

	dbip = db_open( argv[argc-2], "r" );
	if( dbip == DBI_NULL )
	{
		bu_log( "Cannot open file (%s)\n", argv[argc-2] );
		perror( argv[0] );	
		bu_bomb( "Cannot open database file\n" );
	}

	if( (fdout=wdb_fopen( argv[argc-1] )) == NULL )
	{
		bu_log( "Cannot open file (%s)\n", argv[argc-1] );
		perror( argv[0] );	
		bu_bomb( "Cannot open output file\n" );
	}
	db_dirbuild( dbip );

	/* Visit all records in input database, and spew them out,
	 * modifying NMG objects into BoTs.
	 */
	FOR_ALL_DIRECTORY_START(dp, dbip)  {
		struct rt_db_internal	intern;
		int id;
		int ret;
		id = rt_db_get_internal( &intern, dp, dbip, NULL, &rt_uniresource );
		if( id < 0 )  {
			fprintf(stderr,
				"%s: rt_db_get_internal(%s) failure, skipping\n",
				argv[0], dp->d_namep);
			continue;
		}
		if ( id == ID_NMG ) {
	    		nmg_conv( &intern, dp->d_namep );
		} else {
			ret = wdb_put_internal( fdout, dp->d_namep, &intern, 1.0 );
			if( ret < 0 )  {
				fprintf(stderr,
					"%s: wdb_put_internal(%s) failure, skipping\n",
					argv[0], dp->d_namep);
				rt_db_free_internal( &intern, &rt_uniresource );
				continue;
			}
			rt_db_free_internal( &intern, &rt_uniresource );
		}
	} FOR_ALL_DIRECTORY_END
	wdb_close(fdout);
	return 0;
}
