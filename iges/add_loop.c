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
	int			vert_count=0;	/* Actual number of vertices used to make loop */
	struct iges_edge_use	*edge_list;	/* list of edges from iges entity */
	struct vertex		**verts;	/* list of vertices */
	struct faceuse		*fu1;
	vect_t			pl_lu;		/* Plane equation for plane of loop */
	vect_t			norm;		/* Normal for plane of face */
	int			loop_orient;
	int			done;
	int			i,j,k;

	NMG_CK_SHELL( s );
	NMG_CK_FACEUSE( fu );

	NMG_GET_FU_NORMAL( norm , fu );

	/* Acquiring Data */
	if( dir[entityno]->param <= pstart )
	{
		printf( "Illegal parameter pointer for entity D%07d (%s)\n" ,
				dir[entityno]->direct , dir[entityno]->name );
		return(0);
	}

	Readrec( dir[entityno]->param );
	Readint( &sol_num , "" );
	if( sol_num != 508 )
	{
		rt_log( "Entity #%d is not a loop (it's a type %d)\n" , entityno , sol_num );
		rt_bomb( "Fatal error\n" );
	}

	Readint( &no_of_edges , "" );
	edge_list = (struct iges_edge_use *)rt_calloc( no_of_edges , sizeof( struct iges_edge_use ) ,
			"Add_loop_to_face (edge_list)" );
printf( "ADD LOOP:\n" );
	for( i=0 ; i<no_of_edges ; i++ )
	{
		Readint( &edge_list[i].edge_is_vertex , "\tEDGE IS VERTEX: " );
		Readint( &edge_list[i].edge_de , "\tEDGE LIST DE: " );
		Readint( &edge_list[i].index , "\tEDGE LIST INDEX: " );
		Readint( &edge_list[i].orient , "\tEDGE ORIENT: " );
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

	/* list vertices */
	for( i=0 ; i<no_of_edges ; i++ )
	{
		struct iges_vertex_list *vl;
		point_t pt1,pt2,start;
		vect_t v1,v2,cross;
		int found;

		vl = vertex_root;
		found = 0;
printf( "verts[%d] = x%x" , i , verts[i] );
		while( vl && !found )
		{
			for( j=0 ; j<vl->no_of_verts ; j++ )
			{
				if( vl->i_verts[j].v == verts[i] )
				{
					found = 1;
printf( "\t( %g %g %g)\n" , V3ARGS( vl->i_verts[j].pt ) );
					break;
				}
			}
			if( !found )
				vl = vl->next;
		}
	}

	/* eliminate zero length edges */
	vert_count = no_of_edges;
	done = 0;
	while( !done )
	{
		done = 1;
		for( i=0 ; i<vert_count ; i++ )
		{
			k = i + 1;
			if( k == vert_count )
				k = 0;

			if( verts[i] == verts[k] )
			{
				printf( "Ignoring zero length edge\n" );
				done = 0;
				vert_count--;
				for( j=i ; j<vert_count ; j++ )
					verts[j] = verts[j+1];
			}
		}
	}

	if( !vert_count )
		printf( "\tNO VERTICES LEFT IN LOOP (Addloop)\n" );
	else
		printf( "LOOP AFTER ELIMINATING ZERO LENGTH EDGES\n" );

	VSET( pl_lu , 0.0 , 0.0 , 0.0 );
printf( "Calculating plane for loop:\n" );
	for( i=0 ; i<vert_count ; i++ )
	{
		struct iges_vertex_list *vl;
		point_t pt1,pt2,start;
		vect_t v1,v2,cross;
		int found;

		vl = vertex_root;
		found = 0;
		while( vl && !found )
		{
			for( j=0 ; j<vl->no_of_verts ; j++ )
			{
				if( vl->i_verts[j].v == verts[i] )
				{
					found = 1;
printf( "\t( %g %g %g)\n" , V3ARGS( vl->i_verts[j].pt ) );
					break;
				}
			}
			if( !found )
				vl = vl->next;
		}

		if( !found )
			rt_bomb( "Cannot find vertex for face\n" );

		if( i == 0 )
		{
			VMOVE( start , vl->i_verts[j].pt );
			VMOVE( pt2 , start );
		}
		else
		{
			VMOVE( pt1 , pt2 );
			VMOVE( pt2 , vl->i_verts[j].pt );
			VSUB2( v1 , pt1 , start );
			VSUB2( v2 , pt2 , start );
			VCROSS( cross , v1 , v2 );
			VADD2( pl_lu , pl_lu , cross );
		}
	}

	if( VDOT( pl_lu , norm ) < 0.0 )
	{
		/* reverse loop */
		for( i=0 ; i<vert_count/2 ; i++ )
		{
			struct vertex *v_tmp;

			v_tmp = verts[i];
			verts[i] = verts[vert_count-i-1];
			verts[vert_count-i-1] = v_tmp;
		}
	}

	if( vert_count )
		fu1 = nmg_add_loop_to_face( s , fu , verts , vert_count , OT_SAME );

	/* put newly created nmg vertices into vertex list */
	for( i=0 ; i<vert_count ; i++ )
	{
		struct vertex **vt;

		vt = Get_vertex( &edge_list[i] );
		if( face_orient )
			(*vt) = verts[i];
		else
			(*vt) = verts[vert_count - 1 - i];
	}

	printf( "LOOP:\n" );
	for( i=0 ; i<vert_count ; i++ )
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
	}

	rt_free( (char *)edge_list , "Add_loop_to_face (edge_list)" );
	rt_free( (char *)verts , "Add_loop_to_face (vertexlist)" );
	return( 1 );

  err:
	rt_free( (char *)edge_list , "Add_loop_to_face (edge_list)" );
	rt_free( (char *)verts , "Add_loop_to_face (vertexlist)" );
	return( 0 );
}
