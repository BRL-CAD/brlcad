/* Routine to convert all the NMG solids in a BRL-CAD model to polysolids
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
 *	This software is Copyright (C) 1998 by the United States Army
 *	in all countries except the USA.  All rights reserved.
 */


#ifndef lint
static const char RCSid[] = "@(#)$Header$ (BRL)";
#endif
 
#include "conf.h"

#include <stdio.h>
#include <ctype.h>
#include "machine.h"
#include "vmath.h"
#include "externs.h"
#include "db.h"
#include "wdb.h"
#include "rtlist.h"
#include "nmg.h"
#include "raytrace.h"
#include "rtgeom.h"

static union record record;
static struct db_i *dbip;
static int verbose=0;
static FILE *fdout=NULL;
static long out_offset=0;

static void
nmg_conv()
{
	struct directory *dp;
	struct rt_db_internal intern;
	struct model *m;
	struct nmgregion *r;
	struct shell *s;
	int id;

	if( (dp=db_lookup( dbip, record.nmg.N_name, LOOKUP_NOISY)) == DIR_NULL )
	{
		bu_log( "Cannot find NMG solid named %s (skipping)\n", record.nmg.N_name );
		return;
	}

	id = rt_db_get_internal( &intern, dp, dbip, bn_mat_identity );
	if( id != ID_NMG )
	{
		bu_log( "ERROR: rt_db_get_internal returned %d (was expecting %d)\n", id, ID_NMG);
		bu_bomb( "ERROR: rt_db_get_internal returned wrong type\n" );
	}

	if( verbose )
		bu_log( "%s\n", record.nmg.N_name );

	m = (struct model *)intern.idb_ptr;
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
		bu_log( "Failed to convert %s\n", record.nmg.N_name );
		(void)fseek( fdout, out_offset, SEEK_SET );
		rt_db_free_internal( &intern );
		return;
	}
	write_shell_as_polysolid( fdout, record.nmg.N_name, s);
	BU_UNSETJUMP;
	rt_db_free_internal( &intern );
}

main( argc, argv )
int argc;
char *argv[];
{

	long offset=0;
	long granules;

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

	dbip = db_open( argv[argc-2], "r" );
	if( dbip == DBI_NULL )
	{
		bu_log( "Cannot open file (%s)\n", argv[argc-2] );
		perror( argv[0] );	
		bu_bomb( "Cannot open database file\n" );
	}

	if( (fdout=fopen( argv[argc-1], "w")) == NULL )
	{
		bu_log( "Cannot open file (%s)\n", argv[argc-1] );
		perror( argv[0] );	
		bu_bomb( "Cannot open output file\n" );
	}

	db_scan(dbip, (int (*)())db_diradd, 1, NULL);

	fseek( dbip->dbi_fp, 0, SEEK_SET );
	while( fread( (char *)&record, sizeof record, 1, dbip->dbi_fp ) == 1  &&
		!feof(stdin) )
	{
		switch( record.u_id )
		{
		    	case ID_FREE:
				break;
		    	case DBID_NMG:
				offset = ftell( dbip->dbi_fp );
				out_offset = ftell( fdout );
				granules = bu_glong(record.nmg.N_count);
				offset += granules * sizeof( union record );
		    		nmg_conv();
				fseek( dbip->dbi_fp, offset, SEEK_SET );
		    		break;
		    	default:
				fwrite( (char *)&record, sizeof( union record ), 1, fdout );
		    		break;
		}
	}
}
