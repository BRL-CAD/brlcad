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
#include "rtlist.h"
#include "rtstring.h"
#include "nmg.h"
#include "raytrace.h"
#include "wdb.h"
#include "./iges_struct.h"
#include "./iges_extern.h"

RT_EXTERN( struct vertex **Get_vertex , (struct iges_edge_use *edge ) );

int
Add_loop_to_face( s , fu , entityno , face_orient )
struct shell *s;
struct faceuse *fu;
int entityno;
int face_orient;
{ 
	int			sol_num;	/* IGES solid type number */
	int			no_of_edges;	/* edge count for this loop */
	int			no_of_param_curves;
	struct iges_edge_use	*edge_list;	/* list of edges from iges entity */
	struct vertex		**verts;	/* list of vertices */
	struct faceuse		*fu1;
	int			loop_orient;
	int			i,j,k;

	/* Acquiring Data */

	if( dir[entityno]->param <= pstart )
	{
		printf( "Illegal parameter pointer for entity D%07d (%s)\n" ,
				dir[entityno]->direct , dir[entityno]->name );
		return(0);
	}

	Readrec( dir[entityno]->param );
	Readint( &sol_num , "" );
	Readint( &no_of_edges , "" );
	edge_list = (struct iges_edge_use *)rt_calloc( no_of_edges , sizeof( struct iges_edge_use ) ,
			"Add_loop_to_face (edge_list)" );
	for( i=0 ; i<no_of_edges ; i++ )
	{
		Readint( &edge_list[i].edge_is_vertex , "" );
		Readint( &edge_list[i].edge_de , "" );
		Readint( &edge_list[i].index , "" );
		Readint( &edge_list[i].orient , "" );
		Readint( &no_of_param_curves , "" );
		for( j=0 ; j<2*no_of_param_curves ; j++ )
			Readint( &k , "" );
	}

	verts = (struct vertex **)rt_calloc( no_of_edges , sizeof( struct vertex *) ,
		"Make_face: vertex_list **" );

	/* If nmg vertices already exist, put them in the list */
	for( i=0 ; i<no_of_edges ; i++ )
	{
		if( face_orient )
			verts[i] = (*Get_vertex( &edge_list[i] ));
		else
			verts[no_of_edges - 1 - i] = (*Get_vertex( &edge_list[i] ));
	}

	fu1 = nmg_add_loop_to_face( s , fu , verts , no_of_edges , OT_NONE );

	/* put newly created nmg vertices into vertex list */
	for( i=0 ; i<no_of_edges ; i++ )
	{
		struct vertex **vt;

		vt = Get_vertex( &edge_list[i] );
		if( face_orient )
			(*vt) = verts[i];
		else
			(*vt) = verts[no_of_edges - 1 - i];
	}

	printf( "LOOP:\n" );
	for( i=0 ; i<no_of_edges ; i++ )
	{
		struct iges_vertex_list *ivlist;
		int found;

		ivlist = vertex_root;
		found = 0;
		while( !found && ivlist != NULL )
		{
			for( j=0 ; j<ivlist->no_of_verts ; j++ )
			{
				if( ivlist->i_verts[j].v == verts[i] )
				{
					printf( "\t\t\t( %g , %g , %g )\n" ,
						ivlist->i_verts[j].pt[X],
						ivlist->i_verts[j].pt[Y],
						ivlist->i_verts[j].pt[Z] );
					found = 1;
					break;
				}
			}
			ivlist = ivlist->next;
		}
		k = i + 1;
		if( k == no_of_edges )
			k = 0;
		if( verts[i] == verts[k] )
			printf( "******************BAD LOOP (zero length edge)\n" );
	}

	rt_free( (char *)edge_list , "Add_loop_to_face (edge_list)" );
	rt_free( (char *)verts , "Add_loop_to_face (vertexlist)" );
	return( 1 );

  err:
	rt_free( (char *)edge_list , "Add_loop_to_face (edge_list)" );
	rt_free( (char *)verts , "Add_loop_to_face (vertexlist)" );
	return( 0 );
}
