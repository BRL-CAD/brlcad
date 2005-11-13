/*                M A K E _ N U R B _ F A C E . C
 * BRL-CAD
 *
 * Copyright (C) 1995-2005 United States Government as represented by
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
/** @file make_nurb_face.c
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

int
Add_nurb_loop_to_face( s, fu, loop_entityno , face_orient )
struct shell *s;
struct faceuse *fu;
int loop_entityno;
int face_orient;
{
	int i,j,k;
	int entity_type;
	int no_of_edges;
	int no_of_param_curves;
	int vert_no;
	struct face *f;
	struct face_g_snurb *srf;
	struct faceuse *fu_ret;
	struct loopuse *lu;
	struct edgeuse *eu;
	struct vertex **verts;
	struct iges_edge_use *edge_uses;

	NMG_CK_SHELL( s );
	NMG_CK_FACEUSE( fu );
	f = fu->f_p;
	NMG_CK_FACE( f );
	srf = f->g.snurb_p;
	NMG_CK_FACE_G_SNURB( srf );

	if( dir[loop_entityno]->param <= pstart )
	{
		bu_log( "Illegal parameter pointer for entity D%07d (%s), loop ignored\n" ,
				dir[loop_entityno]->direct , dir[loop_entityno]->name );
		return( 0 );
	}

	if( dir[loop_entityno]->type != 508 )
	{
		bu_log( "Entity #%d is not a loop (it's a %s)\n" , loop_entityno , iges_type(dir[loop_entityno]->type) );
		rt_bomb( "Fatal error\n" );
	}

	Readrec( dir[loop_entityno]->param );
	Readint( &entity_type , "" );
	if( entity_type != 508 )
	{
		bu_log( "Add_nurb_loop_to_face: Entity #%d is not a loop (it's a %s)\n",
				loop_entityno , iges_type(entity_type) );
		rt_bomb( "Add_nurb_loop_to_face: Fatal error\n" );
	}

	Readint( &no_of_edges , "" );

	edge_uses = (struct iges_edge_use *)bu_calloc( no_of_edges , sizeof( struct iges_edge_use ) ,
			"Add_nurb_loop_to_face (edge_uses)" );
	for( i=0 ; i<no_of_edges ; i++ )
	{
		Readint( &edge_uses[i].edge_is_vertex , "" );
		Readint( &edge_uses[i].edge_de , "" );
		Readint( &edge_uses[i].index , "" );
		Readint( &edge_uses[i].orient , "" );
		edge_uses[i].root = (struct iges_param_curve *)NULL;
		Readint( &no_of_param_curves , "" );
		for( j=0 ; j<no_of_param_curves ; j++ )
		{
			struct iges_param_curve *new_crv;
			struct iges_param_curve *crv;

			Readint( &k , "" );	/* ignore iso-parametric flag */
			new_crv = (struct iges_param_curve *)bu_malloc( sizeof( struct iges_param_curve ),
				"Add_nurb_loop_to_face: new_crv" );
			if( edge_uses[i].root == (struct iges_param_curve *)NULL )
				edge_uses[i].root = new_crv;
			else
			{
				crv = edge_uses[i].root;
				while( crv->next != (struct iges_param_curve *)NULL )
					crv = crv->next;
				crv->next = new_crv;
			}
			Readint( &new_crv->curve_de, "" );
			new_crv->next = (struct iges_param_curve *)NULL;
		}
	}

	verts = (struct vertex **)bu_calloc( no_of_edges , sizeof( struct vertex *) ,
		"Add_nurb_loop_to_face: vertex_list **" );

	for( i=0 ; i<no_of_edges ; i++ )
	{
		struct vertex **v;

		v = Get_vertex( &edge_uses[i] );
		if( *v )
			verts[i] = (*v);
		else
			verts[i] = (struct vertex *)NULL;
	}

	fu_ret = nmg_add_loop_to_face( s, fu, verts, no_of_edges, OT_SAME );
	for( i=0 ; i<no_of_edges ; i++ )
	{
		struct vertex **v;

		v = Get_vertex( &edge_uses[i] );
		if( !(*v) )
		{
			if( !Put_vertex( verts[i], &edge_uses[i] ) )
			{
				bu_log( "Cannot put vertex x%x\n", verts[i] );
				rt_bomb( "Cannot put vertex\n" );
			}
		}
	}

	lu = BU_LIST_LAST( loopuse, &fu_ret->lu_hd );
	NMG_CK_LOOPUSE( lu );
	i = no_of_edges - 1;
	for( BU_LIST_FOR( eu, edgeuse, &lu->down_hd ) )
	{
		struct vertex **v;

		v = Get_vertex( &edge_uses[i] );

		if( *v != eu->vu_p->v_p )
		{
			nmg_jv( *v, eu->vu_p->v_p );
			verts[i] = *v;
		}
		i++;
		if( i == no_of_edges )
			i = 0;
	}

	/* assign geometry to vertices */
	for( vert_no=0 ; vert_no < no_of_edges ; vert_no++ )
	{
		struct iges_vertex *ivert;

		ivert = Get_iges_vertex( verts[vert_no] );
		if( !ivert )
		{
			bu_log( "Vertex x%x not in vertex list\n" , verts[vert_no] );
			rt_bomb( "Can't get geometry for vertex" );
		}
		nmg_vertex_gv( ivert->v, ivert->pt );
	}

	/* assign geometry to edges */
	i = no_of_edges - 1;
	for( BU_LIST_FOR( eu, edgeuse, &lu->down_hd ) )
	{
		int next_edge_no;
		struct iges_param_curve *param;
		struct vertex *ivert,*jvert;

		next_edge_no = i + 1;
		if( next_edge_no == no_of_edges )
			next_edge_no = 0;

		ivert = (*Get_vertex( &edge_uses[i] ) );
		if( !ivert )
			rt_bomb( "Cannot get vertex for edge_use!\n" );
		jvert = (*Get_vertex( &edge_uses[next_edge_no] ) );
		if( !jvert )
			rt_bomb( "Cannot get vertex for edge_use!\n" );

		if( ivert != eu->vu_p->v_p || jvert != eu->eumate_p->vu_p->v_p )
		{
			bu_log( "ivert=x%x, jvert=x%x, eu->vu_p->v_p=x%x, eu->eumate_p->vu_p->v_p=x%x\n",
				ivert,jvert,eu->vu_p->v_p,eu->eumate_p->vu_p->v_p );
			rt_bomb( "Add_nurb_loop_to_face: Edgeuse/vertex mixup!\n" );
		}

		param = edge_uses[i].root;
		if( !param )
		{
			bu_log( "No parameter curve for eu x%x\n", eu );
			continue;
		}

		while( param )
		{
			int linear;
			int coords;
			struct edge_g_cnurb *crv;
			struct vertex *v2;
			struct edgeuse *new_eu;
			point_t start_uv;
			point_t end_uv;
			hpoint_t pt_on_srf;

			v2 = (struct vertex *)NULL;

			/* Get NURB curve in parameter space for This edgeuse */
			crv = Get_cnurb_curve( param->curve_de, &linear );

			coords = RT_NURB_EXTRACT_COORDS( crv->pt_type );
			VMOVE( start_uv, crv->ctl_points );
			VMOVE( end_uv, &crv->ctl_points[(crv->c_size-1)*coords] );
			if( coords == 2 )
			{
				start_uv[2] = 1.0;
				end_uv[2] = 1.0;
			}

			if( param->next )
			{
				/* need to split this edge to agree with parameter curves */
				new_eu = nmg_esplit( v2, eu, 0 );

				/* evaluate srf at the parameter values for v2 to get geometry */
				if( RT_NURB_EXTRACT_PT_TYPE( crv->pt_type ) == RT_NURB_PT_UV &&
					RT_NURB_IS_PT_RATIONAL( crv->pt_type ) )
				{
					rt_nurb_s_eval( srf, end_uv[0]/end_uv[2],
						end_uv[1]/end_uv[2], pt_on_srf );
				}
				else if( coords == 2 )
					rt_nurb_s_eval( srf, end_uv[0], end_uv[1], pt_on_srf );

				if( RT_NURB_IS_PT_RATIONAL( srf->pt_type ) )
				{
					fastf_t scale;

					scale = 1.0/pt_on_srf[3];
					VSCALE( pt_on_srf, pt_on_srf, scale );
				}
				nmg_vertex_gv( v2, pt_on_srf );
			}
			else
				new_eu = (struct edgeuse *)NULL;

			if( eu->vu_p->v_p == eu->eumate_p->vu_p->v_p )
			{
				/* this edge runs to/from one vertex, need to split */
				struct bu_list split_hd;
				struct edge_g_cnurb *crv1,*crv2;
				point_t start_uv, end_uv;
				hpoint_t pt_on_srf;

				BU_LIST_INIT( &split_hd );

				/* split the edge */
				v2 = (struct vertex *)NULL;
				new_eu = nmg_esplit( v2, eu, 0 );

				/* split the curve */
				rt_nurb_c_split( &split_hd, crv );
				crv1 = BU_LIST_FIRST( edge_g_cnurb, &split_hd );
				crv2 = BU_LIST_LAST( edge_g_cnurb, &split_hd );

				/* get geometry for new vertex */
				coords = RT_NURB_EXTRACT_COORDS( crv1->pt_type );
				VMOVE( start_uv, crv1->ctl_points );
				VMOVE( end_uv, &crv1->ctl_points[(crv1->c_size-1)*coords] );
				if( RT_NURB_EXTRACT_PT_TYPE( crv1->pt_type ) == RT_NURB_PT_UV &&
					RT_NURB_IS_PT_RATIONAL( crv1->pt_type ) )
				{
					rt_nurb_s_eval( srf, end_uv[0]/end_uv[2],
						end_uv[1]/end_uv[2], pt_on_srf );
				}
				else
				{
					start_uv[2] = 1.0;
					end_uv[2] = 1.0;
					rt_nurb_s_eval( srf, end_uv[0], end_uv[1], pt_on_srf );
				}

				if( RT_NURB_IS_PT_RATIONAL( srf->pt_type ) )
				{
					fastf_t scale;

					scale = 1.0/pt_on_srf[3];
					VSCALE( pt_on_srf, pt_on_srf, scale );
				}

				/* assign geometry */
				nmg_vertex_gv( new_eu->vu_p->v_p, pt_on_srf );
				nmg_vertexuse_a_cnurb( eu->vu_p, start_uv );
				nmg_vertexuse_a_cnurb( eu->eumate_p->vu_p, end_uv );
				if( linear )
					nmg_edge_g_cnurb_plinear( eu );
				else
					Assign_cnurb_to_eu( eu, crv1 );

				/* now the second section */
				coords = RT_NURB_EXTRACT_COORDS( crv2->pt_type );
				VMOVE( start_uv, crv2->ctl_points );
				VMOVE( end_uv, &crv2->ctl_points[(crv2->c_size-1)*coords] );
				if( RT_NURB_EXTRACT_PT_TYPE( crv2->pt_type ) == RT_NURB_PT_UV &&
					RT_NURB_IS_PT_RATIONAL( crv2->pt_type ) )
				{
					rt_nurb_s_eval( srf, start_uv[0]/start_uv[2],
						start_uv[1]/start_uv[2], pt_on_srf );
				}
				else
				{
					start_uv[2] = 1.0;
					end_uv[2] = 1.0;
					rt_nurb_s_eval( srf, start_uv[0], start_uv[1], pt_on_srf );
				}

				if( RT_NURB_IS_PT_RATIONAL( srf->pt_type ) )
				{
					fastf_t scale;

					scale = 1.0/pt_on_srf[3];
					VSCALE( pt_on_srf, pt_on_srf, scale );
				}

				/* assign geometry */
				nmg_vertexuse_a_cnurb( new_eu->vu_p, start_uv );
				nmg_vertexuse_a_cnurb( new_eu->eumate_p->vu_p, end_uv );
				if( linear )
					nmg_edge_g_cnurb_plinear( new_eu );
				else
					Assign_cnurb_to_eu( new_eu, crv2 );

				/* free memory */
				while( BU_LIST_NON_EMPTY( &split_hd ) )
				{
					struct edge_g_cnurb *tmp_crv;

					tmp_crv = BU_LIST_FIRST( edge_g_cnurb, &split_hd );
					BU_LIST_DEQUEUE( &tmp_crv->l );
					bu_free( (char *)tmp_crv, "Add_nurb_loop_to_face: tmp_crv" );
				}
			}
			else
			{
				nmg_vertexuse_a_cnurb( eu->vu_p, start_uv );
				nmg_vertexuse_a_cnurb( eu->eumate_p->vu_p, end_uv );

				if( linear )
					nmg_edge_g_cnurb_plinear( eu );
				else
					Assign_cnurb_to_eu( eu, crv );
			}

			bu_free( (char *)crv->k.knots, "Add_nurb_loop_to_face: crv->k.knots" );
			bu_free( (char *)crv->ctl_points, "Add_nurb_loop_to_face: crv->ctl_points" );
			bu_free( (char *)crv, "Add_nurb_loop_to_face: crv" );

			if( new_eu )
				eu = new_eu;

			param = param->next;
		}
		i++;
		if( i == no_of_edges )
			i = 0;
	}

	return( 1 );
#if 0
err:
	for( i=0 ; i<no_of_edges ; i++ )
	{
		struct iges_param_curve *crv;

		crv = edge_uses[i].root;
		while( crv )
		{
			struct iges_param_curve *tmp_crv;

			tmp_crv = crv;
			crv = crv->next;
			bu_free( (char *)tmp_crv, "Add_nurb_loop_to_face: tmp_crv" );
		}
	}
	bu_free( (char *)edge_uses, "Add_nurb_loop_to_face: (edge list)" );
	bu_free( (char *)verts, "Add_nurb_loop_to_face: (vertex list)" );

	return( 0 );
#endif
}

struct faceuse *
Make_nurb_face( s, surf_entityno )
struct shell *s;
int surf_entityno;
{
	struct vertex *verts[1];
	struct loopuse *lu;
	struct faceuse *fu;
	struct face_g_snurb *srf;
	struct model *m;

	if( dir[surf_entityno]->type != 128 )
	{
		bu_log( "Make_nurb_face: Called with surface entity (%d) of type %s\n", surf_entityno,
			iges_type( dir[surf_entityno]->type ) );
		bu_log( "Make_nurb_face: Can only handle surfaces of %s, ignoring face\n", iges_type( 128 ) );
		return( (struct faceuse *)NULL );
	}

	m = nmg_find_model( &s->l.magic );

	if( (srf = Get_nurb_surf( surf_entityno, m )) == (struct face_g_snurb *)NULL )
	{
		bu_log( "Make_nurb_face: Get_nurb_surf failed for surface entity (%d), face ignored\n",	 surf_entityno );
		return( (struct faceuse *)NULL );
	}

	verts[0] = (struct vertex *)NULL;

	fu = nmg_cface( s, verts, 1 );
	Assign_surface_to_fu( fu, srf );

	lu = BU_LIST_FIRST( loopuse, &fu->lu_hd );
	(void)nmg_klu( lu );

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
