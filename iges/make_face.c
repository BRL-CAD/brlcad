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
#include "./iges_struct.h"
#include "./iges_extern.h"
#include "rtlist.h"
#include "rtstring.h"
#include "nmg.h"
#include "raytrace.h"
#include "wdb.h"

RT_EXTERN( struct vertex **Get_vertex , (struct iges_edge_use *edge ) );

struct faceuse *
Make_face( s , entityno , loop_orient )
struct shell *s;
int entityno;
int loop_orient;
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
	Readint( &sol_num , "" );
	Readint( &no_of_edges , "" );
	edge_list = (struct iges_edge_use *)rt_calloc( no_of_edges , sizeof( struct iges_edge_use ) ,
			"Make_face (edge_list)" );
	for( i=0 ; i<no_of_edges ; i++ )
	{
		Readint( &edge_list[i].edge_is_vertex , "" );
		Readint( &edge_list[i].edge_de , "" );
		Readint( &edge_list[i].index , "" );
		Readint( &edge_list[i].orient , "" );
		Readint( &no_of_param_curves , "" );
		for( j=0 ; j<no_of_param_curves ; j++ )
			Readint( &k , "" );
	}

	verts = (struct vertex ***)rt_calloc( no_of_edges , sizeof( struct vertex **) ,
		"Make_face: vertex_list **" );

	for( i=0 ; i<no_of_edges ; i++ )
		verts[i] = Get_vertex( &edge_list[i] );

	fu = nmg_cmface( s, verts, no_of_edges );

	rt_free( (char *)edge_list , "Make_face (edge_list)" );
	rt_free( (char *)verts , "Make_face (vertexlist)" );
	return( fu );

  err:
	rt_free( (char *)edge_list , "Make_face (edge_list)" );
	rt_free( (char *)verts , "Make_face (vertexlist)" );
	return( fu );
}
