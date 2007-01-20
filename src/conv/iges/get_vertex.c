/*                    G E T _ V E R T E X . C
 * BRL-CAD
 *
 * Copyright (c) 1993-2007 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this file; see the file named COPYING for more
 * information.
 */
/** @file get_vertex.c
 *  Authors -
 *	John R. Anderson
 *
 *  Source -
 *	SLAD/BVLD/VMB
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005
 *
 */

#include "./iges_struct.h"
#include "./iges_extern.h"

struct vertex **
Get_vertex( edge_use )
struct iges_edge_use *edge_use;
{
	struct iges_edge_list		*e_list;
	struct iges_vertex_list		*v_list;
	int				edge_index;
	int				vert_index;
	int				vert_de;

	if( (e_list = Get_edge_list( edge_use )) == NULL )
		return( (struct vertex **)NULL );

	edge_index = edge_use->index-1;
	if( edge_use->orient )
	{
		vert_de = e_list->i_edge[edge_index].start_vert_de;
		vert_index = e_list->i_edge[edge_index].start_vert_index - 1;
	}
	else
	{
		vert_de = e_list->i_edge[edge_index].end_vert_de;
		vert_index = e_list->i_edge[edge_index].end_vert_index - 1;
	}

	if( (v_list = Get_vertex_list( vert_de )) == NULL )
		return( (struct vertex **)NULL );

	return( &v_list->i_verts[vert_index].v );
}

int
Put_vertex( v, edge )
struct vertex *v;
struct iges_edge_use *edge;
{
	struct iges_edge_list		*e_list;
	struct iges_edge_list		*el;
	struct iges_vertex_list		*v_list;
	int				vert_index;
	int				vert_de;

	if( (e_list = Get_edge_list( edge )) == NULL )
		return( 0 );


	el = e_list;
	while( el && el->edge_de != edge->edge_de )
		el = el->next;

	if( !el )
	{
		bu_log( "Cannot find an edge list with edge_de = %d\n" , edge->edge_de );
		rt_bomb( "Cannot find correct edge list\n" );
	}

	if( edge->orient )
	{
		vert_de = el->i_edge[edge->index-1].start_vert_de;
		vert_index = el->i_edge[edge->index-1].start_vert_index-1;
	}
	else
	{
		vert_de = el->i_edge[edge->index-1].end_vert_de;
		vert_index = el->i_edge[edge->index-1].end_vert_index-1;
	}


	if( (v_list = Get_vertex_list( vert_de )) == NULL )
		return( 0 );

	if( v_list->i_verts[vert_index].v )
	{
		bu_log( "vertex already assigned x%x, trying to assign x%x\n", v_list->i_verts[vert_index].v, v );
		rt_bomb( "Multiple vertex assignments\n" );
	}

	v_list->i_verts[vert_index].v = v;
	return( 1 );
}

struct iges_edge *
Get_edge( e_use )
struct iges_edge_use *e_use;
{
	struct iges_edge_list	*e_list;

	if( (e_list = Get_edge_list( e_use )) == NULL )
		return( (struct iges_edge *)NULL );

	return( &e_list->i_edge[e_use->index-1] );
}

struct vertex *
Get_edge_start_vertex( edge )
struct iges_edge *edge;
{
	struct iges_vertex_list *v_list;

	if( (v_list = Get_vertex_list( edge->start_vert_de )) == NULL )
		return( (struct vertex *)NULL );

	return( v_list->i_verts[edge->start_vert_index-1].v );
}


struct vertex *
Get_edge_end_vertex( edge )
struct iges_edge *edge;
{
	struct iges_vertex_list *v_list;

	if( (v_list = Get_vertex_list( edge->end_vert_de )) == NULL )
		return( (struct vertex *)NULL );

	return( v_list->i_verts[edge->end_vert_index-1].v );
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
