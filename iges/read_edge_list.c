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
 *	This software is Copyright (C) 1993-2004 by the United States Army.
 *	All rights reserved.
 */

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
		bu_log( "Illegal parameter pointer for entity D%07d (%s)\n" ,
				dir[entityno]->direct , dir[entityno]->name );
		return( (struct iges_edge_list *)NULL );
	}

	Readrec( dir[entityno]->param );
	Readint( &sol_num , "" );
	if( sol_num != 504 )
	{
		/* this is not an edge list entity */
		bu_log( "Read_edge_list: entity at DE %d is not an edge list entity\n" , edge->edge_de );
		return( (struct iges_edge_list *)NULL );
	}

	edge_list = (struct iges_edge_list *)bu_malloc( sizeof( struct iges_edge_list )  ,
			"Read_edge_list: iges_edge_list" );

	edge_list->edge_de = edge->edge_de;
	edge_list->next = NULL;
	Readint( &edge_list->no_of_edges , "" );
	edge_list->i_edge = (struct iges_edge *)bu_calloc( edge_list->no_of_edges , sizeof( struct iges_edge ) ,
			"Read_edge_list: iges_edge" );

	for( i=0 ; i<edge_list->no_of_edges ; i++ )
	{
		Readint( &edge_list->i_edge[i].curve_de , "" );
		Readint( &edge_list->i_edge[i].start_vert_de , "" );
		Readint( &edge_list->i_edge[i].start_vert_index , "" );
		Readint( &edge_list->i_edge[i].end_vert_de , "" );
		Readint( &edge_list->i_edge[i].end_vert_index , "" );
	}

	return( edge_list );
}
