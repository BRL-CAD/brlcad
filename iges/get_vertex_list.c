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

#include "./iges_struct.h"
#include "./iges_extern.h"

struct iges_vertex_list *
Get_vertex_list( vert_de )
int vert_de;
{
	struct iges_vertex_list *v_list;

	if( vertex_root == NULL )
	{
		vertex_root = Read_vertex_list( vert_de );
		if( vertex_root != NULL )
		{
			vertex_root->next = NULL;
			return( vertex_root );
		}
		else
			return( (struct iges_vertex_list *)NULL );
	}
	else
	{
		v_list = vertex_root;
		while( v_list->next != NULL && v_list->vert_de != vert_de )
			v_list = v_list->next;
	}

	if( v_list->vert_de == vert_de )
		return( v_list );

	v_list->next = Read_vertex_list( vert_de );
	if( v_list->next == NULL )
		return( (struct iges_vertex_list *)NULL );
	else
		return( v_list->next );
	
}
