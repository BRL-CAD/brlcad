/*
 *  Authors -
 *	John R. Anderson
 *
 *  Source -
 *	SLAD/BVLD/VMB
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005
 *  
 *  Copyright Notice -
 *	This software is Copyright (C) 1993 by the United States Army.
 *	All rights reserved.
 */
#include <stdio.h>
#include "machine.h"
#include "vmath.h"
#ifdef BSD
#include <strings.h>
#else
#include <string.h>
#endif
#include "./iges_struct.h"
#include "./iges_extern.h"
#include "rtlist.h"
#include "rtstring.h"
#include "nmg.h"
#include "raytrace.h"
#include "wdb.h"

int
Add_loop_to_face( fu , entityno , loop_orient )
struct faceuse *fu;
int entityno;
int loop_orient;
{ 
	int			sol_num;	/* IGES solid type number */
	int			no_of_edges;	/* edge count for this loop */
	int			no_of_param_curves;
	struct iges_edge	*edge_list;	/* list of edges from iges entity */
	int			i,j,k;

	/* Acquiring Data */

	if( dir[entityno]->param <= pstart )
	{
		printf( "Illegal parameter pointer for entity D%07d (%s)\n" ,
				dir[entityno]->direct , dir[entityno]->name );
		return(0);
	}

	Readrec( dir[entityno]->param );
	Readint( &sol_num , "LOOP: " );
	Readint( &no_of_edges , "No. of edges: " );
	edge_list = (struct iges_edge *)rt_calloc( no_of_edges , sizeof( struct iges_edge ) ,
			"Add_loop_to_face (edge_list)" );
/*	for( i=0 ; i<no_of_edges ; i++ )
	{
		Readint( &edge_list[i].edge_is_vertex , "\tEdge is vertex: " );
		Readint( &edge_list[i].edge_de , "\tEdge DE: " );
		Readint( &edge_list[i].index , "\tIndex: " );
		Readint( &edge_list[i].orient , "\tOrient: " );
		Readint( &no_of_param_curves , "\tNo of curves: " );
		for( j=0 ; j<no_of_param_curves ; j++ )
			Readint( &k , "\t\tCurve: " );
	}
*/
	rt_free( (char *)edge_list , "Add_loop_to_face (edge_list)" );
	return( 1 );

  err:
	rt_free( (char *)edge_list , "Add_loop_to_face (edge_list)" );
	return( 0 );
}
