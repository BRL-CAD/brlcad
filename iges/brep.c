/*
 *  Authors -
 *	John R. Anderson
 *
 *  Source -
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5068  USA
 *  
 *  Distribution Notice -
 *	Re-distribution of this software is restricted, as described in
 *	your "Statement of Terms and Conditions for the Release of
 *	The BRL-CAD Package" agreement.
 *
 *  Copyright Notice -
 *	This software is Copyright (C) 1994 by the United States Army
 *	in all countries except the USA.  All rights reserved.
 */
#ifndef lint
static char RCSid[] = "@(#)$Header$ (ARL)";
#endif

#include "./iges_struct.h"
#include "./iges_extern.h"

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
	struct shell	**void_shells;		/* List of void shells */
	struct shell	*s_outer;		/* Outer shell */
	struct iges_vertex_list *v_list;
	struct iges_edge_list	*e_list;
	int		i;

	/* Acquiring Data */

	if( dir[entityno]->param <= pstart )
	{
		bu_log( "Illegal parameter pointer for entity D%07d (%s)\n" ,
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
		void_shell_de = (int *)bu_calloc( num_of_voids , sizeof( int ) , "BREP: void shell DE's" );
		void_orient = (int *)bu_calloc( num_of_voids , sizeof( int ) , "BREP: void shell orients" );
		void_shells = (struct shell **)bu_calloc( num_of_voids , sizeof( struct shell *) , "BREP: void shell pointers" );
		for( i=0 ; i<num_of_voids ; i++ )
		{
			Readint( &void_shell_de[i] , "" );
			Readint( &void_orient[i] , "" );
		}
	}

	/* start building */
	m = nmg_mmr();
	r = BU_LIST_FIRST( nmgregion, &m->r_hd );

	/* Put outer shell in region */
	if( (s_outer=Get_outer_shell( r , (shell_de - 1)/2 , orient )) == (struct shell *)NULL )
		goto err;

	/* Put voids in */
	for( i=0 ; i<num_of_voids ; i++ )
	{
		if( (void_shells[i]=Add_inner_shell( r, (void_shell_de[i] - 1)/2, void_orient[i] ))
			== (struct shell *)NULL )
				goto err;
	}

	/* orient loops */
	Orient_loops( r );

	/* orient shells */
	nmg_fix_normals( s_outer , &tol );
	for( i=0 ; i<num_of_voids ; i++ )
	{
		nmg_fix_normals( void_shells[i] , &tol );
		nmg_invert_shell( void_shells[i] , &tol );
	}

	if( do_polysolids )
	{
		/* Merge all shells into one */
		for( i=0 ; i<num_of_voids ; i++ )
			nmg_js( s_outer, void_shells[i], &tol );

		/* write out polysolid */
		write_shell_as_polysolid( fdout, dir[entityno]->name, s_outer );
	}
	else
	{
		/* Compute "geometry" for region and shell */
		nmg_region_a( r , &tol );

		/* Write NMG solid */
		if( mk_nmg( fdout , dir[entityno]->name , m ) )
			goto err;
	}

	if( num_of_voids )
	{
		bu_free( (char *)void_shell_de , "BREP: void shell DE's" );
		bu_free( (char *)void_orient , "BREP: void shell orients" );
		bu_free( (char *)void_shells , "brep: void shell list" );
	}
	nmg_km( m );

	v_list = vertex_root;
	while( v_list != NULL )
	{
		bu_free( (char *)v_list->i_verts , "brep: iges_vertex" );
		bu_free( (char *)v_list , "brep: vertex list" );
		v_list = v_list->next;
	}
	vertex_root = NULL;

	e_list = edge_root;
	while( e_list != NULL )
	{
		bu_free( (char *)e_list->i_edge , "brep:iges_edge" );
		bu_free( (char *)e_list , "brep: edge list" );
		e_list = e_list->next;
	}
	edge_root = NULL;
	return( 1 );

 err :
	if( num_of_voids )
	{
		bu_free( (char *)void_shell_de , "BREP: void shell DE's" );
		bu_free( (char *)void_orient , "BREP: void shell orients" );
		bu_free( (char *)void_shells , "brep: void shell list" );
	}
	nmg_km( m );
	return( 0 );
}
