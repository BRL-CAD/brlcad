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

brep( entityno )
int entityno;
{ 

	int		sol_num;		/* IGES solid type number */
	int		shell_de;		/* Directory sequence number for a shell */
	int		orient;			/* Orientation of shell */
	int		*void_shell_de;		/* Directory sequence number for an void shell */
	int		*void_orient;		/* Orientation of void shell */
	int		num_of_voids;		/* Number of inner void shells */
	struct model	*m;			/* NMG model */
	struct nmgregion *r;			/* NMG region */
	struct iges_vertex_list *v_list;
	struct iges_edge_list	*e_list;
	int		i;

	/* Acquiring Data */

	if( dir[entityno]->param <= pstart )
	{
		printf( "Illegal parameter pointer for entity D%07d (%s)\n" ,
				dir[entityno]->direct , dir[entityno]->name );
		return(0);
	}

	Readrec( dir[entityno]->param );
	Readint( &sol_num , "" );
	Readint( &shell_de , "" );
	Readint( &orient , "" );
	Readint( &num_of_voids , "" );
	if( num_of_voids )
	{
		void_shell_de = (int *)rt_calloc( num_of_voids , sizeof( int ) , "BREP: void shell DE's" );
		void_orient = (int *)rt_calloc( num_of_voids , sizeof( int ) , "BREP: void shell orients" );
		for( i=0 ; i<num_of_voids ; i++ )
		{
			Readint( &void_shell_de[i] , "" );
			Readint( &void_orient[i] , "" );
		}
	}

	/* start building */
	m = nmg_mmr();
	r = RT_LIST_FIRST( nmgregion, &m->r_hd );

	/* Put outer shell in region */
	if( !Get_outer_shell( r , (shell_de - 1)/2 , orient ) )
		goto err;

	/* Put voids in */
	for( i=0 ; i<num_of_voids ; i++ )
	{
		if( !Add_inner_shell( r , (void_shell_de[i] - 1)/2 , void_orient[i] ) )
			goto err;
	}

	v_list = vertex_root;
	while( v_list != NULL )
	{
		for( i=0 ; i < v_list->no_of_verts ; i++ )	
		{
			if( v_list->i_verts[i].v )
				nmg_vertex_gv( v_list->i_verts[i].v ,
					v_list->i_verts[i].pt );
		}
		v_list = v_list->next;
	}

	/* Compute "geometry" for region and shell */
	nmg_region_a( r );

	if( mk_nmg( fdout , dir[entityno]->name , m ) )
		goto err;

	if( num_of_voids )
	{
		rt_free( (char *)void_shell_de , "BREP: void shell DE's" );
		rt_free( (char *)void_orient , "BREP: void shell orients" );
	}
	nmg_km( m );

	v_list = vertex_root;
	while( v_list != NULL )
	{
		rt_free( (char *)v_list->i_verts , "brep: iges_vertex" );
		rt_free( (char *)v_list , "brep: vertex list" );
		v_list = v_list->next;
	}
	vertex_root = NULL;

	e_list = edge_root;
	while( e_list != NULL )
	{
		rt_free( (char *)e_list->i_edge , "brep:iges_edge" );
		rt_free( (char *)e_list , "brep: edge list" );
		e_list = e_list->next;
	}
	edge_root = NULL;
	return( 1 );

 err :
	if( num_of_voids )
	{
		rt_free( (char *)void_shell_de , "BREP: void shell DE's" );
		rt_free( (char *)void_orient , "BREP: void shell orients" );
	}
	nmg_km( m );
	return( 0 );
}
