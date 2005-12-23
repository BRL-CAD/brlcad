/*                     M A K E _ F A C E . C
 * BRL-CAD
 *
 * Copyright (C) 1993-2005 United States Government as represented by
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
/** @file make_face.c
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

struct faceuse *
Make_planar_face( s , entityno , face_orient )
struct shell *s;
int entityno;
int face_orient;
{

	int			sol_num;	/* IGES solid type number */
	int			no_of_edges;	/* edge count for this loop */
	int			no_of_param_curves;
	int			vert_count=0;	/* Actual number of vertices used to make face */
	struct iges_edge_use	*edge_list;	/* list of edgeuses from iges loop entity */
	struct faceuse		*fu=NULL;	/* NMG face use */
	struct loopuse		*lu;		/* NMG loop use */
	struct vertex		***verts;	/* list of vertices */
	struct iges_vertex_list	*v_list;
	int			done;
	int			i,j,k;

	/* Acquiring Data */

	if( dir[entityno]->param <= pstart )
	{
		bu_log( "Illegal parameter pointer for entity D%07d (%s)\n" ,
				dir[entityno]->direct , dir[entityno]->name );
		return(0);
	}

	Readrec( dir[entityno]->param );
	Readint( &sol_num , "" );
	if( sol_num != 508 )
	{
		bu_log( "Entity #%d is not a loop (it's a %s)\n" , entityno , iges_type(sol_num) );
		rt_bomb( "Fatal error\n" );
	}

	Readint( &no_of_edges , "" );
	edge_list = (struct iges_edge_use *)bu_calloc( no_of_edges , sizeof( struct iges_edge_use ) ,
			"Make_face (edge_list)" );
	for( i=0 ; i<no_of_edges ; i++ )
	{
		Readint( &edge_list[i].edge_is_vertex , "" );
		Readint( &edge_list[i].edge_de , "" );
		Readint( &edge_list[i].index , "" );
		Readint( &edge_list[i].orient , "" );
		if( !face_orient ) /* need opposite orientation of edge */
		{
			if( edge_list[i].orient )
				edge_list[i].orient = 0;
			else
				edge_list[i].orient = 1;
		}
		edge_list[i].root = (struct iges_param_curve *)NULL;
		Readint( &no_of_param_curves , "" );
		for( j=0 ; j<no_of_param_curves ; j++ )
		{
			struct iges_param_curve *new_crv;
			struct iges_param_curve *crv;

			Readint( &k , "" );	/* ignore iso-parametric flag */
			new_crv = (struct iges_param_curve *)bu_malloc( sizeof( struct iges_param_curve ),
				"Make_planar_face: new_crv" );
			if( edge_list[i].root == (struct iges_param_curve *)NULL )
				edge_list[i].root = new_crv;
			else
			{
				crv = edge_list[i].root;
				while( crv->next != (struct iges_param_curve *)NULL )
					crv = crv->next;
				crv->next = new_crv;
			}
			Readint( &new_crv->curve_de, "" );
			new_crv->next = (struct iges_param_curve *)NULL;
		}
	}

	verts = (struct vertex ***)bu_calloc( no_of_edges , sizeof( struct vertex **) ,
		"Make_face: vertex_list **" );

	for( i=0 ; i<no_of_edges ; i++ )
	{
		if( face_orient )
			verts[i] = Get_vertex( &edge_list[i] );
		else
			verts[no_of_edges-1-i] = Get_vertex( &edge_list[i] );
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
				bu_log( "Ignoring zero length edge\n" );
				done = 0;
				vert_count--;
				for( j=i ; j<vert_count ; j++ )
					verts[j] = verts[j+1];
			}
		}
	}

	if( vert_count )
	{
		plane_t			pl;		/* Plane equation for face */
		fastf_t			area;		/* area of loop */
		fastf_t dist;
		vect_t min2max;
		point_t outside_pt;

		fu = nmg_cmface( s, verts, vert_count );

		/* associate geometry */
		v_list = vertex_root;
		while( v_list != NULL )
		{
			for( i=0 ; i < v_list->no_of_verts ; i++ )
			{
				if( v_list->i_verts[i].v != NULL && v_list->i_verts[i].v->vg_p == NULL )
				{
					NMG_CK_VERTEX( v_list->i_verts[i].v );
					nmg_vertex_gv( v_list->i_verts[i].v ,
						v_list->i_verts[i].pt );
				}
			}
			v_list = v_list->next;
		}

		lu = BU_LIST_FIRST( loopuse , &fu->lu_hd );
		NMG_CK_LOOPUSE( lu );

		area = nmg_loop_plane_area( lu , pl );
		if( area < 0.0 )
		{
			bu_log( "Could not calculate area for face (entityno = %d)\n", entityno );
			nmg_pr_fu_briefly( fu, "" );
			nmg_kfu( fu );
			fu = (struct faceuse *)NULL;
			goto err;
		}

		nmg_face_g( fu , pl );
		nmg_face_bb( fu->f_p , &tol );

		/* find a point that is surely outside the loop */
		VSUB2( min2max , fu->f_p->max_pt , fu->f_p->min_pt );
		VADD2( outside_pt , fu->f_p->max_pt , min2max );

		/* move it to the plane of the face */
		dist = DIST_PT_PLANE( outside_pt , pl );
		VJOIN1( outside_pt , outside_pt , -dist , pl );

		if( nmg_class_pt_lu_except( outside_pt , lu, (struct edge *)NULL , &tol ) != NMG_CLASS_AoutB )
		{
			nmg_reverse_face( fu );
			if( fu->orientation != OT_SAME )
			{
				fu = fu->fumate_p;
				if( fu->orientation != OT_SAME )
					rt_bomb( "Make_face: no OT_SAME use for a face!!!\n" );
			}
		}
	}
	else
		bu_log( "No edges left!!\n" );

  err:
	bu_free( (char *)edge_list , "Make_face (edge_list)" );
	bu_free( (char *)verts , "Make_face (vertexlist)" );
	return( fu );
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
