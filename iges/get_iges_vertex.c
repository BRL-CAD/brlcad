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
 *	This software is Copyright (C) 1995 by the United States Army.
 *	All rights reserved.
 */
#ifndef lint
static const char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "./iges_struct.h"
#include "./iges_extern.h"

struct iges_vertex *
Get_iges_vertex( v )
struct vertex *v;
{
	struct iges_vertex_list *vert_list;

	NMG_CK_VERTEX( v );

	vert_list = vertex_root;

	while( vert_list )
	{
		int vert_no;

		for( vert_no=0 ; vert_no < vert_list->no_of_verts ; vert_no++ )
		{
			if( vert_list->i_verts[vert_no].v == v )
				return( &(vert_list->i_verts[vert_no]) );
		}
		vert_list = vert_list->next;
	}

	return( (struct iges_vertex *)NULL );
}
