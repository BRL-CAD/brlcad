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
#include "/m/cad/librt/debug.h"

/* --- Begin John's pretty-printer --- */

char
*Recurse_tree( tree )
union tree *tree;
{
	char *left,*right;
	char *return_str;
	char op;
	int return_length;

	if( tree == NULL )
		return( (char *)NULL );
	if( tree->tr_op == OP_UNION || tree->tr_op == OP_SUBTRACT || tree->tr_op == OP_INTERSECT )
	{
		left = Recurse_tree( tree->tr_b.tb_left );
		right = Recurse_tree( tree->tr_b.tb_right );
		switch( tree->tr_op )
		{
			case OP_UNION:
				op = 'u';
				break;
			case OP_SUBTRACT:
				op = '-';
				break;
			case OP_INTERSECT:
				op = '+';
				break;
		}
		return_length = strlen( left ) + strlen( right ) + 4;
		if( op == 'u' )
			return_length += 4;
		return_str = (char *)rt_malloc( return_length , "recurse_tree: return string" );
		if( op == 'u' )
		{
			char *blankl,*blankr;

			blankl = strchr( left , ' ' );
			blankr = strchr( right , ' ' );
			if( blankl && blankr )
				sprintf( return_str , "(%s) %c (%s)" , left , op , right );
			else if( blankl && !blankr )
				sprintf( return_str , "(%s) %c %s" , left , op , right );
			else if( !blankl && blankr )
				sprintf( return_str , "%s %c (%s)" , left , op , right );
			else
				sprintf( return_str , "%s %c %s" , left , op , right );
		}
		else
			sprintf( return_str , "%s %c %s" , left , op , right );
		if( tree->tr_b.tb_left->tr_op != OP_DB_LEAF )
			rt_free( left , "Recurse_tree: left string" );
		if( tree->tr_b.tb_right->tr_op != OP_DB_LEAF )
			rt_free( right , "Recurse_tree: right string" );
		return( return_str );
	}
	else if( tree->tr_op == OP_DB_LEAF ) {
		return bu_strdup(tree->tr_l.tl_name) ;
	}
}

void
Print_tree( tree )
union tree *tree;
{
	char *str;

	str = Recurse_tree( tree );
	if( str != NULL )
	{
		printf( "%s\n" , str );
		rt_free( str , "Print_tree" );
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
	struct rt_external	ep;
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

	mat_idn( identity_mat );
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
			rt_log( "db_get_external failed for %s\n" , argv[i] );
			continue;
		}
		if( rt_comb_v4_import( &ip , &ep , NULL ) < 0 )  {
			rt_log("import of %s failed\n", dp->d_namep);
			continue;
		}

		RT_CK_DB_INTERNAL( &ip );
		bu_mem_barriercheck();

		if( ip.idb_type != ID_COMBINATION )
		{
			rt_log( "idb_type = %d\n" , ip.idb_type );
			continue;
		}

		comb = (struct rt_comb_internal *)ip.idb_ptr;
		RT_CK_COMB(comb);
		if( comb->region_flag )
		{
			rt_log( "\tRegion id = %d, aircode = %d GIFTmater = %d, los = %d\n",
				comb->region_id, comb->aircode, comb->GIFTmater, comb->los );
		}
		rt_log( "\trgb_valid = %d, color = %d/%d/%d\n" , comb->rgb_valid , V3ARGS( comb->rgb ) );
		rt_log( "\tshader = %s (%s)\n" ,
				rt_vls_addr( &comb->shader ),
				rt_vls_addr( &comb->material )
		);

		/* John's way */
		Print_tree( comb->tree );

		/* Standard way */
		rt_pr_tree( comb->tree, 1 );

		/* Compact way */
		{
			struct rt_vls	str;
			rt_vls_init( &str );
			rt_pr_tree_vls( &str, comb->tree );
			rt_log("%s\n", rt_vls_addr(&str) );
			rt_vls_free(&str );
		}

		/* Test the support routines */
		if( db_ck_v4gift_tree( comb->tree ) < 0 )
			rt_log("ERROR: db_ck_v4gift_tree is unhappy\n");

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

		rt_v4_export( &ep, &ip, 1.0, dp );
		bu_mem_barriercheck();

		if( (fp = fopen(bu_vls_addr(&file), "w")) != NULL )  {
			fwrite( ep.ext_buf, ep.ext_nbytes, 1, fp );
			fclose(fp);
		}
		db_free_external( &ep );
		bu_vls_free( &file );

		/* Test the lumberjacks */
		rt_comb_ifree( &ip );

	}
}
