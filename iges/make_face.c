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

RT_EXTERN( struct vertex **Get_vertex , (struct iges_edge_use *edge ) );
RT_EXTERN( struct faceuse *nmg_cmface , ( struct shell *s, struct vertex ***verts, int no_of_edges ) );

struct faceuse *
Make_face( s , entityno , face_orient )
struct shell *s;
int entityno;
int face_orient;
{ 

	int			sol_num;	/* IGES solid type number */
	int			no_of_edges;	/* edge count for this loop */
	int			no_of_param_curves;
	struct iges_edge_use	*edge_list;	/* list of edgeuses from iges loop entity */
	struct faceuse		*fu=NULL;	/* NMG face use */
	struct vertex		***verts;	/* list of vertices */
	int			i,j,k;

	/* Acquiring Data */

	if( dir[entityno]->param <= pstart )
	{
		printf( "Illegal parameter pointer for entity D%07d (%s)\n" ,
				dir[entityno]->direct , dir[entityno]->name );
		return(0);
	}

	Readrec( dir[entityno]->param );
	printf( "Make Face (loop at %d), passed orient = %d\n" , entityno*2+1 , face_orient );
	Readint( &sol_num , "LOOP: " );
	if( sol_num != 508 )
	{
		rt_log( "Entity #%d is not a loop (it's a type %d)\n" , entityno , sol_num );
		rt_bomb( "Fatal error\n" );
	}

	Readint( &no_of_edges , "no of edges: " );
	edge_list = (struct iges_edge_use *)rt_calloc( no_of_edges , sizeof( struct iges_edge_use ) ,
			"Make_face (edge_list)" );
	for( i=0 ; i<no_of_edges ; i++ )
	{
		Readint( &edge_list[i].edge_is_vertex , "\n\tEdge is Vertex: " );
		Readint( &edge_list[i].edge_de , "\tEdge DE: " );
		Readint( &edge_list[i].index , "\tEdge index: " );
		Readint( &edge_list[i].orient , "\tEdge orient: " );
		if( !face_orient ) /* need opposite orientation of edge */
		{
printf( "Face orientation is %d, so switch edge (%d) orientation from %d to" , face_orient , i , edge_list[i].orient );
			if( edge_list[i].orient )
				edge_list[i].orient = 0;
			else
				edge_list[i].orient = 1;
printf( " %d\n" , edge_list[i].orient );
		}
		Readint( &no_of_param_curves , "" );
		for( j=0 ; j<2*no_of_param_curves ; j++ )
			Readint( &k , "" );
	}

	verts = (struct vertex ***)rt_calloc( no_of_edges , sizeof( struct vertex **) ,
		"Make_face: vertex_list **" );

	printf( "\t\tLOOP:\n" );
	for( i=0 ; i<no_of_edges ; i++ )
	{
		if( face_orient )
			verts[i] = Get_vertex( &edge_list[i] );
		else
			verts[no_of_edges-1-i] = Get_vertex( &edge_list[i] );
	}

	for( i=0 ; i<no_of_edges ; i++ )
	{
		struct iges_vertex_list *vl;
		struct iges_vertex *iv;
		int found;

		found = 0;
		vl  = vertex_root;
		while( vl != NULL && !found )
		{
			for( j=0 ; j<vl->no_of_verts ; j++ )
			{
				if( &vl->i_verts[j].v == verts[i] )
				{
					printf( "\t\t\t( %g , %g , %g )\n" ,
						vl->i_verts[j].pt[X],
						vl->i_verts[j].pt[Y],
						vl->i_verts[j].pt[Z] );
					found = 1;
					break;
				}
			}
			vl = vl->next;
		}

		k = i + 1;
		if( k == no_of_edges )
			k = 0;
		if( verts[i] == verts[k] )
			printf( "******************BAD LOOP (zero length edge)\n" );
	}

	fu = nmg_cmface( s, verts, no_of_edges );

	rt_free( (char *)edge_list , "Make_face (edge_list)" );
	rt_free( (char *)verts , "Make_face (vertexlist)" );
	return( fu );

  err:
	rt_free( (char *)edge_list , "Make_face (edge_list)" );
	rt_free( (char *)verts , "Make_face (vertexlist)" );
	return( fu );
}
