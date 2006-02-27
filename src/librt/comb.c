/*                          C O M B . C
 * BRL-CAD
 *
 * Copyright (c) 1996-2006 United States Government as represented by
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

/** \addtogroup librt */
/*@{*/
/** @file comb.c
 * XXX Move to db_tree.c when complete.
 *
 *  Authors -
 *	John R. Anderson
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

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>

#include "machine.h"
#include "bu.h"
#include "vmath.h"
#include "bn.h"
#include "nmg.h"
#include "rtgeom.h"
#include "raytrace.h"
#include "../librt/debug.h"


/* --- Begin John's pretty-printer --- */

void
Print_tree(union tree *tree)
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

int
main(int argc, char *argv[])
{
	struct db_i		*dbip;
	struct directory	*dp;
	struct rt_db_internal	ip;
	struct rt_comb_internal	*comb;
	mat_t			identity_mat;
	int			i;
	struct bu_vls		file;

	bu_debug = BU_DEBUG_MEM_CHECK | BU_DEBUG_COREDUMP;

	if( argc < 3 )
	{
		fprintf( stderr , "Usage:\n\t%s db_file object1 object2 ...\n" , argv[0] );
		return 1;
	}

	MAT_IDN( identity_mat );
	bu_vls_init( &file );

	if( (dbip = db_open( argv[1] , "r" ) ) == NULL )
	{
		fprintf( stderr , "Cannot open %s\n" , argv[1] );
		perror( "test" );
		return 1;
	}

	rt_init_resource( &rt_uniresource, 0, NULL );

	/* Scan the database */
	db_dirbuild( dbip );

	for( i=2 ; i<argc ; i++ )
	{
		printf( "%s\n" , argv[i] );

		dp = db_lookup( dbip , argv[i] , 1 );

		if( rt_db_get_internal( &ip, dp, dbip, NULL, &rt_uniresource ) < 0 )  {
			bu_log("import of %s failed\n", dp->d_namep);
			continue;
		}

		RT_CK_DB_INTERNAL( &ip );
		bu_mem_barriercheck();

		if( ip.idb_type != ID_COMBINATION )
		{
			bu_log( "idb_type = %d\n" , ip.idb_type );
			rt_db_free_internal( &ip, &rt_uniresource );
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
		bu_log( "Pretty print:\n" );
		Print_tree( comb->tree );

		/* Standard way */
		bu_log( "Standard print:\n" );
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

		/* Test the lumberjacks */
		rt_db_free_internal( &ip, &rt_uniresource );

	}

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
