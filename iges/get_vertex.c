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
#include <strings.h>
#endif
#include "machine.h"
#include "vmath.h"
#include "raytrace.h"
#include "wdb.h"
#include "./iges_struct.h"
#include "./iges_extern.h"

RT_EXTERN( struct iges_edge_list *Get_edge_list , (struct iges_edge_use *edge ) );
RT_EXTERN( struct iges_vertex_list *Get_vertex_list , (int vert_de ) );

struct vertex **
Get_vertex( edge )
struct iges_edge_use *edge;
{
	struct iges_edge_list		*e_list;
	struct iges_vertex_list		*v_list;
	int				edge_index;
	int				vert_index;
	int				vert_de;

	if( (e_list = Get_edge_list( edge )) == NULL )
		return( (struct vertex **)NULL );

	edge_index = edge->index-1;
	if( edge->orient )
	{
		vert_de = e_list->i_edge[edge_index].start_vert_de;
		vert_index = e_list->i_edge[edge->index-1].start_vert_index - 1;
	}
	else
	{
		vert_de = e_list->i_edge[edge_index].end_vert_de;
		vert_index = e_list->i_edge[edge->index-1].end_vert_index - 1;
	}

	if( (v_list = Get_vertex_list( vert_de )) == NULL )
		return( (struct vertex **)NULL );

	return( &v_list->i_verts[vert_index].v );
}
