/*               G E T _ V E R T E X _ L I S T . C
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
/** @file get_vertex_list.c
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

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
