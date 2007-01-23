/*               G E T _ I G E S _ V E R T E X . C
 * BRL-CAD
 *
 * Copyright (c) 1995-2007 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file get_iges_vertex.c
 *  Authors -
 *	John R. Anderson
 *
 *  Source -
 *	SLAD/BVLD/VMB
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005
 *
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

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
