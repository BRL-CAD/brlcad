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

#include "conf.h"

#include <stdio.h>
#ifdef USE_STRING_H
#include <string.h>
#else
#include <string.h>
#endif

#include "machine.h"
#include "vmath.h"
#include "raytrace.h"
#include "wdb.h"
#include "./iges_struct.h"
#include "./iges_extern.h"

struct iges_edge_list *
Read_edge_list( edge )
struct iges_edge_use *edge;
{
	struct iges_edge_list	*edge_list;
	int			entityno;
	int			sol_num;
	int			i;

	entityno = (edge->edge_de - 1)/2;

	/* Acquiring Data */

	if( dir[entityno]->param <= pstart )
	{
		printf( "Illegal parameter pointer for entity D%07d (%s)\n" ,
				dir[entityno]->direct , dir[entityno]->name );
		return( (struct iges_edge_list *)NULL );
	}

	Readrec( dir[entityno]->param );
	Readint( &sol_num , "EDGE LIST: " );
	if( sol_num != 504 )
	{
		/* this is not an edge list entity */
		rt_log( "Read_edge_list: entity at DE %d is not an edge list entity\n" , edge->edge_de );
		return( (struct iges_edge_list *)NULL );
	}

	edge_list = (struct iges_edge_list *)rt_malloc( sizeof( struct iges_edge_list )  ,
			"Read_edge_list: iges_edge_list" );

	edge_list->edge_de = edge->edge_de;
	edge_list->next = NULL;
	Readint( &edge_list->no_of_edges , "\tNo of edges: " );
	edge_list->i_edge = (struct iges_edge *)rt_calloc( edge_list->no_of_edges , sizeof( struct iges_edge ) ,
			"Read_edge_list: iges_edge" );

	for( i=0 ; i<edge_list->no_of_edges ; i++ )
	{
		printf( "--- Edge index %d:\n" , i+1 );
		Readint( &edge_list->i_edge[i].curve_de , "\t\tCurve DE: " );
		Readint( &edge_list->i_edge[i].start_vert_de , "\t\t\tStart vertex DE: " );
		Readint( &edge_list->i_edge[i].start_vert_index , "\t\t\tStart index: " );
		Readint( &edge_list->i_edge[i].end_vert_de , "\t\t\tEnd vertex DE: " );
		Readint( &edge_list->i_edge[i].end_vert_index , "\t\t\tEnd index: " );
	}

	return( edge_list );
}
