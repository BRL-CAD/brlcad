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

struct iges_edge_list *
Get_edge_list( edge )
struct iges_edge_use *edge;
{
	struct iges_edge_list *e_list;

	if( edge_root == NULL )
	{
		edge_root = Read_edge_list( edge );
		if( edge_root != NULL )
		{
			edge_root->next = NULL;
			return( edge_root );
		}
		else
			return( (struct iges_edge_list *)NULL );
	}
	else
	{
		e_list = edge_root;
		while( e_list->next != NULL && e_list->edge_de != edge->edge_de )
			e_list = e_list->next;
	}

	if( e_list->edge_de == edge->edge_de )
		return( e_list );

	e_list->next = Read_edge_list( edge );
	if( e_list->next == NULL )
		return( (struct iges_edge_list *)NULL );
	else
		return( e_list->next );
	
}
