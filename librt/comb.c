/*
 *			C O M B . C
 *
 * XXX Move to db_tree.c when complete.
 *
 *  Authors -
 *	John R. Anderson
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


#include <stdio.h>
#include <math.h>
#include <string.h>
#include "machine.h"
#include "db.h"
#include "externs.h"
#include "bu.h"
#include "vmath.h"
#include "bn.h"
#include "nmg.h"
#include "rtgeom.h"
#include "raytrace.h"
#include "../librt/debug.h"

/* --- Begin John's pretty-printer --- */

void
Print_tree( tree )
union tree *tree;
{
	char *str;

	str = (char *)rt_pr_tree_str( tree );
	if( str != NULL )
	{
		printf( "%s\n" , str );
		bu_free( str , "Print_tree" );
	}
	else
		printf( "NULL Tree\n" );
}

main( argc , argv )
int argc;
char *argv[];
{
	struct db_i		*dbip;
	struct directory	*dp;
	struct bu_external	ep;
	struct rt_db_internal	ip;
	struct rt_comb_internal	*comb;
	mat_t			identity_mat;
	int			i;
	struct bu_vls		file;
	FILE			*fp;

	bu_debug = BU_DEBUG_MEM_CHECK | BU_DEBUG_COREDUMP;

	if( argc < 3 )
	{
		fprintf( stderr , "Usage:\n\t%s db_file object1 object2 ...\n" , argv[0] );
		exit( 1 );
	}

	bn_mat_idn( identity_mat );
	bu_vls_init( &file );

	if( (dbip = db_open( argv[1] , "r" ) ) == NULL )
	{
		fprintf( stderr , "Cannot open %s\n" , argv[1] );
		perror( "test" );
		exit( 1 );
	}

	/* Scan the database */
	db_scan(dbip, (int (*)())db_diradd, 1);

	for( i=2 ; i<argc ; i++ )
	{
		int j;

		printf( "%s\n" , argv[i] );

		dp = db_lookup( dbip , argv[i] , 1 );
		if( db_get_external( &ep , dp , dbip ) )
		{
			bu_log( "db_get_external failed for %s\n" , argv[i] );
			continue;
		}
		if( rt_comb_v4_import( &ip , &ep , NULL ) < 0 )  {
			bu_log("import of %s failed\n", dp->d_namep);
			continue;
		}

		RT_CK_DB_INTERNAL( &ip );
		bu_mem_barriercheck();

		if( ip.idb_type != ID_COMBINATION )
		{
			bu_log( "idb_type = %d\n" , ip.idb_type );
			continue;
		}

		comb = (struct rt_comb_internal *)ip.idb_ptr;
		RT_CK_COMB(comb);
		if( comb->region_flag )
		{
			bu_log( "\tRegion id = %d, aircode = %d GIFTmater = %d, los = %d\n",
				comb->region_id, comb->aircode, comb->GIFTmater, comb->los );
		}
		bu_log( "\trgb_valid = %d, color = %d/%d/%d\n" , comb->rgb_valid , V3ARGS( comb->rgb ) );
		bu_log( "\tshader = %s (%s)\n" ,
				bu_vls_addr( &comb->shader ),
				bu_vls_addr( &comb->material )
		);

		/* John's way */
		Print_tree( comb->tree );

		/* Standard way */
		rt_pr_tree( comb->tree, 1 );

		/* Compact way */
		{
			struct bu_vls	str;
			bu_vls_init( &str );
			rt_pr_tree_vls( &str, comb->tree );
			bu_log("%s\n", bu_vls_addr(&str) );
			bu_vls_free(&str );
		}

		/* Test the support routines */
		if( db_ck_v4gift_tree( comb->tree ) < 0 )
			bu_log("ERROR: db_ck_v4gift_tree is unhappy\n");

		/* write original external form into a file */
		bu_vls_printf( &file, "/tmp/%s.a", argv[i] );
		if( (fp = fopen(bu_vls_addr(&file), "w")) != NULL )  {
			fwrite( ep.ext_buf, ep.ext_nbytes, 1, fp );
			fclose(fp);
		}
		db_free_external( &ep );
		bu_vls_free( &file );

		/* Convert internal back to external, and write both to files */
		bu_vls_printf( &file, "/tmp/%s.b", argv[i] );
		bu_mem_barriercheck();

		rt_db_put_internal( dp, dbip, &ip );
		bu_mem_barriercheck();

		if( (fp = fopen(bu_vls_addr(&file), "w")) != NULL )  {
			fwrite( ep.ext_buf, ep.ext_nbytes, 1, fp );
			fclose(fp);
		}
		db_free_external( &ep );
		bu_vls_free( &file );

		/* Test the lumberjacks */
		ip.idb_meth->ft_ifree( &ip );

	}
}
