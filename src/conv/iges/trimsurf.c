/*                      T R I M S U R F . C
 * BRL-CAD
 *
 * Copyright (c) 1994-2006 United States Government as represented by
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
/** @file trimsurf.c
 *  Authors -
 *	John R. Anderson
 *
 *  Source -
 *	SLAD/BVLD/VMB
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005
 *
 */

/*	This routine loops through all the directory entries and calls
	appropriate routines to convert trimmed surface entities to BRLCAD
	NMG TNURBS	*/

#ifndef lint
static const char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "./iges_struct.h"
#include "./iges_extern.h"
#include "../../librt/debug.h"

/* translations to get knot vectors in first quadrant */
static fastf_t u_translation=0.0;
static fastf_t v_translation=0.0;

#define UV_TOL  1.0e-6

#define CTL_INDEX(_i,_j)	((_i * n_cols + _j) * ncoords)

struct snurb_hit
{
	struct bu_list l;
	fastf_t dist;
	point_t pt;
	vect_t norm;
	struct face *f;
};

struct face_g_snurb *
Get_nurb_surf( entityno, m )
int entityno;
struct model *m;
{
	struct face_g_snurb *srf;
	point_t pt;
	int entity_type;
	int i;
	int rational;
	int ncoords;
	int n_rows,n_cols,u_order,v_order;
	int n_u,n_v;
	int pt_type;
	double a;

	/* Acquiring Data */

	if( dir[entityno]->param <= pstart )
	{
		bu_log( "Illegal parameter pointer for entity D%07d (%s)\n" ,
				dir[entityno]->direct , dir[entityno]->name );
		return( (struct face_g_snurb *)NULL );
	}

	Readrec( dir[entityno]->param );
	Readint( &entity_type , "" );
	if( entity_type != 128 )
	{
		bu_log( "Only B-Spline surfaces allowed for faces (found type %d)\n", entity_type );
		return( (struct face_g_snurb *)NULL );
	}
	Readint( &i , "" );
	n_cols = i+1;
	Readint( &i , "" );
	n_rows = i+1;
	Readint( &i , "" );
	u_order = i+1;
	Readint ( &i , "" );
	v_order = i+1;
	Readint( &i , "" );
	Readint( &i , "" );
	Readint( &i , "" );
	rational = !i;

	ncoords = 3+rational;
	pt_type = RT_NURB_MAKE_PT_TYPE( ncoords , RT_NURB_PT_XYZ , rational );
	Readint( &i , "" );
	Readint( &i , "" );

	n_u = n_cols+u_order;
	n_v = n_rows+v_order;
	if( !m )
		srf = rt_nurb_new_snurb( u_order, v_order, n_u, n_v, n_rows, n_cols, pt_type, (struct resource *)NULL );
	else
	{
		int pnum;

		GET_FACE_G_SNURB( srf, m );
		BU_LIST_INIT( &srf->l );
		BU_LIST_INIT( &srf->f_hd );
		srf->l.magic = NMG_FACE_G_SNURB_MAGIC;
		srf->order[0] = u_order;
		srf->order[1] = v_order;
		srf->dir = RT_NURB_SPLIT_ROW;
		srf->u.magic = NMG_KNOT_VECTOR_MAGIC;
		srf->v.magic = NMG_KNOT_VECTOR_MAGIC;
		srf->u.k_size = n_u;
		srf->v.k_size = n_v;
		srf->u.knots = (fastf_t *) bu_malloc (
			n_u * sizeof (fastf_t ), "Get_nurb_surf: u kv knot values");
		srf->v.knots = (fastf_t *) bu_malloc (
			n_v * sizeof (fastf_t ), "Get_nurb_surf: v kv knot values");

		srf->s_size[0] = n_rows;
		srf->s_size[1] = n_cols;
		srf->pt_type = pt_type;

		pnum = sizeof (fastf_t) * n_rows * n_cols * RT_NURB_EXTRACT_COORDS(pt_type);
		srf->ctl_points = ( fastf_t *) bu_malloc(
			pnum, "Get_nurb_surf: control mesh points");

	}
	NMG_CK_FACE_G_SNURB( srf );

	/* Read knot vectors */
	for( i=0 ; i<n_u ; i++ )
	{
		Readdbl( &a , "" );
		srf->u.knots[i] = a;
	}
	for( i=0 ; i<n_v ; i++ )
	{
		Readdbl( &a , "" );
		srf->v.knots[i] = a;
	}

	/* Insure that knot values are non-negative */
	if( srf->v.knots[0] < 0.0 )
	{
		v_translation = (-srf->v.knots[0]);
		for( i=0 ; i<n_v ; i++ )
			srf->v.knots[i] += v_translation;
	}
	else
		v_translation = 0.0;

	if( srf->u.knots[0] < 0.0 )
	{
		u_translation = (-srf->u.knots[0]);
		for( i=0 ; i<n_u ; i++ )
			srf->u.knots[i] += u_translation;
	}
	else
		u_translation = 0.0;

	/* Read weights */
	for( i=0 ; i<n_cols*n_rows ; i++ )
	{
		Readdbl( &a , "" );
		if( rational )
			srf->ctl_points[i*ncoords + 3] = a;
	}

	/* Read control points */
	for( i=0 ; i<n_cols*n_rows ; i++ )
	{
			Readcnv( &a , "" );
			if( rational )
				pt[X] = a*srf->ctl_points[i*ncoords+3];
			else
				pt[X] = a;
			Readcnv( &a , "" );
			if( rational )
				pt[Y] = a*srf->ctl_points[i*ncoords+3];
			else
				pt[Y] = a;
			Readcnv( &a , "" );
			if( rational )
				pt[Z] = a*srf->ctl_points[i*ncoords+3];
			else
				pt[Z] = a;

			/* apply transformation */
			MAT4X3PNT( &srf->ctl_points[i*ncoords], *dir[entityno]->rot, pt );
	}

	Readdbl( &a , "" );
	Readdbl( &a , "" );
	Readdbl( &a , "" );
	Readdbl( &a , "" );

	return( srf );
}

void
Assign_cnurb_to_eu( eu, crv )
struct edgeuse *eu;
struct edge_g_cnurb *crv;
{
	fastf_t *ctl_points;
	fastf_t *knots;
	int ncoords;
	int i;

	NMG_CK_EDGEUSE( eu );
	NMG_CK_CNURB( crv );

	ncoords = RT_NURB_EXTRACT_COORDS( crv->pt_type );

	ctl_points = (fastf_t *)bu_calloc( ncoords*crv->c_size, sizeof( fastf_t ),
			"Assign_cnurb_to_eu: ctl_points" );

	for( i=0 ; i<crv->c_size ; i++ )
		VMOVEN( &ctl_points[i*ncoords], &crv->ctl_points[i*ncoords], ncoords )

	knots = (fastf_t *)bu_calloc( crv->k.k_size, sizeof( fastf_t ),
			"Assign_cnurb_to_eu: knots" );

	for( i=0 ; i<crv->k.k_size; i++ )
		knots[i] = crv->k.knots[i];

	nmg_edge_g_cnurb( eu, crv->order, crv->k.k_size, knots, crv->c_size,
			crv->pt_type, ctl_points );
}

struct edge_g_cnurb *
Get_cnurb( entity_no )
int entity_no;
{
	struct edge_g_cnurb *crv;
	point_t pt,pt2;
	int entity_type;
	int num_pts;
	int degree;
	int i;
	int planar;
	int rational;
	int pt_type;
	int ncoords;
	double a;
	double x,y,z;

	if( dir[entity_no]->param <= pstart )
	{
		bu_log( "Get_cnurb: Illegal parameter pointer for entity D%07d (%s)\n" ,
				dir[entity_no]->direct , dir[entity_no]->name );
		return( (struct edge_g_cnurb *)NULL );
	}

	Readrec( dir[entity_no]->param );
	Readint( &entity_type , "" );

	if( entity_type != 126 )
	{
		bu_log( "Get_cnurb: Was expecting spline curve, got type %d\n" , entity_type );
		return( (struct edge_g_cnurb *)NULL );
	}

	Readint( &i , "" );
	num_pts = i+1;
	Readint( &degree , "" );

	/* properties */
	Readint( &planar , "" );
	Readint( &i, "" );	/* open or closed */
	Readint( &i, "" );	/* polynomial */
	rational = !i;
	Readint( &i, "" );	/* periodic */

	if( rational )
	{
		ncoords = 3;
		pt_type = RT_NURB_MAKE_PT_TYPE( ncoords, RT_NURB_PT_UV, RT_NURB_PT_RATIONAL );
	}
	else
	{
		ncoords = 2;
		pt_type = RT_NURB_MAKE_PT_TYPE( ncoords, RT_NURB_PT_UV, RT_NURB_PT_NONRAT );
	}

	crv = rt_nurb_new_cnurb( degree+1, num_pts+degree+1, num_pts, pt_type );
	/* knot vector */
	for( i=0 ; i<num_pts+degree+1 ; i++ )
	{
		Readdbl( &a , "" );
		crv->k.knots[i] = a;
	}

	/* weights */
	for( i=0 ; i<num_pts ; i++ )
	{
		Readdbl( &a , "" );
		if( rational )
			crv->ctl_points[i*ncoords+2] = a;
	}

	/* control points */
	for( i=0 ; i<num_pts ; i++ )
	{
		if( dir[entity_no]->status & 500 )
		{
			Readdbl( &x , "" );
			Readdbl( &y , "" );
			Readdbl( &z , "" );
		}
		else
		{
			Readcnv( &x , "" );
			Readcnv( &y , "" );
			Readcnv( &z , "" );
		}
		if( rational )
		{
			pt[X] = (x + u_translation) * crv->ctl_points[i*ncoords+2];
			pt[Y] = (y + v_translation) * crv->ctl_points[i*ncoords+2];
		}
		else
		{
			pt[X] = x + u_translation;
			pt[Y] = y + v_translation;
		}
		pt[Z] = z;

		/* apply transformation */
		MAT4X3PNT( pt2, *dir[entity_no]->rot, pt );
		crv->ctl_points[i*ncoords] = pt2[X];
		crv->ctl_points[i*ncoords+1] = pt2[Y];
	}
	return( crv );
}

void
Assign_vu_geom( vu, u, v, srf )
struct vertexuse *vu;
fastf_t u,v;
struct face_g_snurb *srf;
{
	point_t uvw;
	hpoint_t pt_on_srf;
	struct vertexuse *vu1;
	int moved=0;

	NMG_CK_VERTEXUSE( vu );
	NMG_CK_SNURB( srf );

	VSETALLN( pt_on_srf, 0.0, 4 )

	if( u < srf->u.knots[0] || v < srf->v.knots[0] ||
		u > srf->u.knots[srf->u.k_size-1] || v > srf->v.knots[srf->v.k_size-1] )
	{
		bu_log( "WARNING: UV point outside of domain of surface!!!:\n" );
		bu_log( "\tUV = (%g %g)\n", u, v );
		bu_log( "\tsrf domain: (%g %g) <-> (%g %g)\n",
			srf->u.knots[0], srf->v.knots[0],
			srf->u.knots[srf->u.k_size-1], srf->v.knots[srf->v.k_size-1] );

		if( u < srf->u.knots[0] )
			u = srf->u.knots[0];
		if( v < srf->v.knots[0] )
			v = srf->v.knots[0];
		if( u > srf->u.knots[srf->u.k_size-1] )
			u = srf->u.knots[srf->u.k_size-1];
		if( v > srf->v.knots[srf->v.k_size-1] )
			v = srf->v.knots[srf->v.k_size-1];

		moved = 1;
	}

	rt_nurb_s_eval( srf, u, v, pt_on_srf );
	if( RT_NURB_IS_PT_RATIONAL( srf->pt_type ) )
	{
		fastf_t scale;

		scale = 1.0/pt_on_srf[3];
		VSCALE( pt_on_srf, pt_on_srf, scale );
	}
	if( moved )
	{
		bu_log( "\tMoving VU geom to (%g %g %g) new UV = (%g %g)\n",
			V3ARGS( pt_on_srf ), u, v );
	}

	nmg_vertex_gv( vu->v_p, pt_on_srf );
	/* XXXXX Need w coord!!!!! */
	VSET( uvw, u, v, 1.0 );

	for( BU_LIST_FOR( vu1, vertexuse, &vu->v_p->vu_hd ) )
		nmg_vertexuse_a_cnurb( vu1, uvw );
}

void
Add_trim_curve( entity_no, lu, srf )
int entity_no;
struct loopuse *lu;
struct face_g_snurb *srf;
{
	int entity_type;
	struct edge_g_cnurb *crv;
	struct edgeuse *eu;
	struct edgeuse *new_eu;
	struct vertex *vp;
	double x,y,z;
	point_t pt, pt2;
	int ncoords;
	int i;

	NMG_CK_LOOPUSE( lu );
	NMG_CK_SNURB( srf );

	if( dir[entity_no]->param <= pstart )
	{
		bu_log( "Illegal parameter pointer for entity D%07d (%s)\n" ,
				dir[entity_no]->direct , dir[entity_no]->name );
		return;
	}

	Readrec( dir[entity_no]->param );
	Readint( &entity_type , "" );

	switch( entity_type )
	{
		case 102:	/* composite curve */
			{
				int curve_count;
				int *curve_list;

				Readint( &curve_count , "" );
				curve_list = (int *)bu_calloc( curve_count , sizeof( int ),
						"Add_trim_curve: curve_list" );

				for( i=0 ; i<curve_count ; i++ )
					Readint( &curve_list[i] , "" );

				for( i=0 ; i<curve_count ; i++ )
					Add_trim_curve( (curve_list[i]-1)/2, lu, srf );

				bu_free( (char *)curve_list , "Add_trim_curve: curve_list" );
			}
			break;
		case 110:	/* line */
			/* get start point */
			Readdbl( &x , "" );
			Readdbl( &y , "" );
			Readdbl( &z , "" );
			VSET( pt, x + u_translation, y + v_translation, z )

			/* apply transformation */
			MAT4X3PNT( pt2, *dir[entity_no]->rot, pt )

			/* Split last edge in loop */
			eu = BU_LIST_LAST( edgeuse, &lu->down_hd );
			new_eu = nmg_eusplit( (struct vertex *)NULL, eu, 0 );
			vp = eu->vu_p->v_p;

			/* if old edge doesn't have vertex geometry, assign some */
			if( !vp->vg_p )
				Assign_vu_geom( eu->vu_p, pt2[X], pt2[Y], srf );

			/* read terminate point */
			Readdbl( &x , "" );
			Readdbl( &y , "" );
			Readdbl( &z , "" );
			VSET( pt, x + u_translation, y + v_translation, z )

			/* apply transformation */
			MAT4X3PNT( pt2, *dir[entity_no]->rot, pt )

			Assign_vu_geom( new_eu->vu_p, pt2[X], pt2[Y], srf );

			/* assign edge geometry */
			nmg_edge_g_cnurb_plinear( eu );
			break;
		case 100:	/* circular arc */
			{
				struct edge_g_cnurb *crv;
				point_t center, start, end;

				/* read Arc center start and end points */
				Readcnv( &z , "" );	/* common Z-coord */
				Readcnv( &x , "" );	/* center */
				Readcnv( &y , "" );	/* center */
				VSET( center, y+u_translation, x+v_translation, z )

				Readcnv( &x , "" );	/* start */
				Readcnv( &y , "" );	/* start */
				VSET( start, y+u_translation, x+v_translation, z )

				Readcnv( &x , "" );	/* end */
				Readcnv( &y , "" );	/* end */
				VSET( end, y+u_translation, x+v_translation, z )

				/* build edge_g_cnurb arc */
				crv = rt_arc2d_to_cnurb( center, start, end, RT_NURB_PT_UV, &tol );

				/* apply transformation to control points */
				for( i=0 ; i<crv->c_size ; i++ )
				{
					V2MOVE( pt2, &crv->ctl_points[i*3] )
					pt2[Z] = z;
					MAT4X3PNT( pt, *dir[entity_no]->rot, pt2 )
					V2MOVE( &crv->ctl_points[i*3], pt );
				}

				/* add a new edge to loop */
				eu = BU_LIST_LAST( edgeuse, &lu->down_hd );
				new_eu = nmg_eusplit( (struct vertex *)NULL, eu, 0 );
				vp = eu->vu_p->v_p;

				/* if old edge doesn't have vertex geometry, assign some */
				if( !vp->vg_p )
					Assign_vu_geom( eu->vu_p,
						crv->ctl_points[0], crv->ctl_points[1], srf );

				/* Assign geometry to new vertex */
				Assign_vu_geom( new_eu->vu_p,
						crv->ctl_points[(crv->c_size-1)*3],
						crv->ctl_points[(crv->c_size-1)*3 + 1],
						srf );

				/* Assign edge geometry */
				Assign_cnurb_to_eu( eu, crv );

				rt_nurb_free_cnurb( crv );
			}
			break;

		case 104:	/* conic arc */
			bu_log( "Conic Arc not yet handled as a trimming curve\n" );
			break;

		case 126:	/* spline curve */
			/* Get spline curve */
			crv = Get_cnurb( entity_no );

			ncoords = RT_NURB_EXTRACT_COORDS( crv->pt_type );

			/* add a new edge to loop */
			eu = BU_LIST_LAST( edgeuse, &lu->down_hd );
			new_eu = nmg_eusplit( (struct vertex *)NULL, eu, 0 );
			vp = eu->vu_p->v_p;

			/* if old edge doesn't have vertex geometry, assign some */
			if( !vp->vg_p )
			{
				Assign_vu_geom( eu->vu_p, crv->ctl_points[0],
					crv->ctl_points[1], srf );
			}

			/* Assign geometry to new vertex */
			Assign_vu_geom( new_eu->vu_p, crv->ctl_points[(crv->c_size-1)*ncoords],
					crv->ctl_points[(crv->c_size-1)*ncoords+1], srf );

			/* Assign edge geometry */
			Assign_cnurb_to_eu( eu, crv );

			rt_nurb_free_cnurb( crv );
			break;
		default:
			bu_log( "Curves of type %d are not yet handled for trimmed surfaces\n", entity_type );
			break;
	}
}

struct loopuse *
Make_trim_loop( entity_no, orientation, srf, fu )
int entity_no;
int orientation;
struct face_g_snurb *srf;
struct faceuse *fu;
{
	struct loopuse *lu;
	struct edgeuse *eu;
	struct edgeuse *new_eu;
	struct vertexuse *vu;
	struct vertex *vp;
	int entity_type;
	int ncoords;
	double x,y,z;
	fastf_t u,v;
	int i;

	NMG_CK_SNURB( srf );
	NMG_CK_FACEUSE( fu );

	if( dir[entity_no]->param <= pstart )
	{
		bu_log( "Illegal parameter pointer for entity D%07d (%s)\n" ,
				dir[entity_no]->direct , dir[entity_no]->name );
		return( (struct loopuse *)NULL );
	}

	Readrec( dir[entity_no]->param );
	Readint( &entity_type , "" );

	lu = nmg_mlv( &fu->l.magic, (struct vertex *)NULL, orientation );
	vu = BU_LIST_FIRST(vertexuse, &lu->down_hd);
	eu = nmg_meonvu(vu);

	switch( entity_type )
	{
		case 102:	/* composite curve */
			{
				int curve_count;
				int *curve_list;

				Readint( &curve_count , "" );
				curve_list = (int *)bu_calloc( curve_count , sizeof( int ),
						"Make_trim_loop: curve_list" );

				for( i=0 ; i<curve_count ; i++ )
					Readint( &curve_list[i] , "" );

				for( i=0 ; i<curve_count ; i++ )
					Add_trim_curve( (curve_list[i]-1)/2, lu, srf );

				bu_free( (char *)curve_list , "Make_trim_loop: curve_list" );

				/* if last EU is zero length, kill it */
				eu = BU_LIST_LAST( edgeuse, &lu->down_hd );
				if( bn_dist_pt3_pt3( eu->vu_p->v_p->vg_p->coord, eu->eumate_p->vu_p->v_p->vg_p->coord ) < tol.dist )
					nmg_keu( eu );
				else
				{
					bu_log( "ERROR: composite trimming curve is not closed!!!!\n" );
					bu_log( "\ttrim curve is entity #%d, parameters at line #%d\n",
						entity_no, dir[entity_no]->param );
					bu_log( "\tThis is likely to result in failure to convert (core dump)\n" );
				}
			}
			break;
		case 100:	/* circular arc (must be full cirle here) */
			{
				struct edge_g_cnurb *crv;
				struct edge_g_cnurb *crv1,*crv2;
				point_t center, start, end;
				struct bu_list curv_hd;

				/* read Arc center start and end points */
				Readcnv( &z , "" );	/* common Z-coord */
				Readcnv( &x , "" );	/* center */
				Readcnv( &y , "" );	/* center */
				VSET( center, x+u_translation, y+v_translation, z )

				Readcnv( &x , "" );	/* start */
				Readcnv( &y , "" );	/* start */
				VSET( start, x+u_translation, y+v_translation, z )

				Readcnv( &x , "" );	/* end */
				Readcnv( &y , "" );	/* end */
				VSET( end, x+u_translation, y+v_translation, z )

				/* build edge_g_cnurb circle */
				crv = rt_arc2d_to_cnurb( center, start, end, RT_NURB_PT_UV, &tol );

				/* split circle into two pieces */
				BU_LIST_INIT( &curv_hd );
				rt_nurb_c_split( &curv_hd, crv );
				crv1 = BU_LIST_FIRST( edge_g_cnurb, &curv_hd );
				crv2 = BU_LIST_LAST( edge_g_cnurb, &curv_hd );

				/* Split last edge in loop */
				eu = BU_LIST_LAST( edgeuse, &lu->down_hd );
				new_eu = nmg_eusplit( (struct vertex *)NULL, eu, 0 );
				vp = eu->vu_p->v_p;

				/* if old edge doesn't have vertex geometry, assign some */
				if( !vp->vg_p )
				{
					u = crv1->ctl_points[0]/crv1->ctl_points[2];
					v = crv1->ctl_points[1]/crv1->ctl_points[2];
					Assign_vu_geom( eu->vu_p, u, v, srf );
				}

				vp = new_eu->vu_p->v_p;
				if( !vp->vg_p )
				{
					/* Assign geometry to new_eu vertex */
					u = crv2->ctl_points[0]/crv2->ctl_points[2];
					v = crv2->ctl_points[1]/crv2->ctl_points[2];
					Assign_vu_geom( new_eu->vu_p, u, v, srf );
				}

				/* Assign edge geometry */
				Assign_cnurb_to_eu( eu, crv1 );
				Assign_cnurb_to_eu( new_eu, crv2 );

				rt_nurb_free_cnurb( crv );
				rt_nurb_free_cnurb( crv1 );
				rt_nurb_free_cnurb( crv2 );
			}
			break;

		case 104:	/* conic arc */
			bu_log( "Conic Arc not yet handled as a trimming curve\n" );
			break;

		case 126:	/* spline curve (must be closed loop) */
			{
				struct edge_g_cnurb *crv;
				struct edge_g_cnurb *crv1,*crv2;
				struct bu_list curv_hd;

				/* Get spline curve */
				crv = Get_cnurb( entity_no );

				ncoords = RT_NURB_EXTRACT_COORDS( crv->pt_type );

				/* split circle into two pieces */
				BU_LIST_INIT( &curv_hd );
				rt_nurb_c_split( &curv_hd, crv );
				crv1 = BU_LIST_FIRST( edge_g_cnurb, &curv_hd );
				crv2 = BU_LIST_LAST( edge_g_cnurb, &curv_hd );
				/* Split last edge in loop */
				eu = BU_LIST_LAST( edgeuse, &lu->down_hd );
				new_eu = nmg_eusplit( (struct vertex *)NULL, eu, 0 );
				vp = eu->vu_p->v_p;

				/* if old edge doesn't have vertex geometry, assign some */
				if( !vp->vg_p )
				{
					/* XXX Don't divied out rational coord */
					if( RT_NURB_IS_PT_RATIONAL( crv1->pt_type ) )
					{
						u = crv1->ctl_points[0]/crv1->ctl_points[ncoords-1];
						v = crv1->ctl_points[1]/crv1->ctl_points[ncoords-1];
					}
					else
					{
						u = crv1->ctl_points[0];
						v = crv1->ctl_points[1];
					}
					Assign_vu_geom( eu->vu_p, u, v, srf );
				}

				/* Assign geometry to new_eu vertex */
				vp = new_eu->vu_p->v_p;
				if( !vp->vg_p )
				{
					if( RT_NURB_IS_PT_RATIONAL( crv2->pt_type ) )
					{
						u = crv2->ctl_points[0]/crv2->ctl_points[ncoords-1];
						v = crv2->ctl_points[1]/crv2->ctl_points[ncoords-1];
					}
					else
					{
						u = crv2->ctl_points[0];
						v = crv2->ctl_points[1];
					}
					Assign_vu_geom( new_eu->vu_p, u, v, srf );
				}

				/* Assign edge geometry */
				Assign_cnurb_to_eu( eu, crv1 );
				Assign_cnurb_to_eu( new_eu, crv2 );

				rt_nurb_free_cnurb( crv );
				rt_nurb_free_cnurb( crv1 );
				rt_nurb_free_cnurb( crv2 );
			}
			break;
		default:
			bu_log( "Curves of type %d are not yet handled for trimmed surfaces\n", entity_type );
			break;
	}
	return( lu );
}

struct loopuse *
Make_loop( entity_no, orientation, on_surf_de, srf, fu )
int entity_no;
int orientation;
int on_surf_de;
struct face_g_snurb *srf;
struct faceuse *fu;
{
	struct loopuse *lu;
	int entity_type;
	int surf_de,param_curve_de,model_curve_de;
	int i;

	NMG_CK_SNURB( srf );
	NMG_CK_FACEUSE( fu );

	/* Acquiring Data */
	if( dir[entity_no]->param <= pstart )
	{
		bu_log( "Illegal parameter pointer for entity D%07d (%s)\n" ,
				dir[entity_no]->direct , dir[entity_no]->name );
		return(0);
	}

	Readrec( dir[entity_no]->param );
	Readint( &entity_type , "" );
	if( entity_type != 142 )
	{
		bu_log( "Expected Curve on a Parametric Surface, found %s\n" , iges_type( entity_type ) );
		return( 0 );
	}
	Readint( &i , "" );
	Readint( &surf_de , "" );
	if( surf_de != on_surf_de )
		bu_log( "Curve is on surface at DE %d, should be on surface at DE %d\n", surf_de, on_surf_de );

	Readint( &param_curve_de , "" );
	Readint( &model_curve_de , "" );

	lu = Make_trim_loop( (param_curve_de-1)/2, orientation, srf, fu );

	return( lu );
}

struct loopuse *
Make_default_loop( srf, fu )
struct face_g_snurb *srf;
struct faceuse *fu;
{
	struct loopuse *lu;
	struct edgeuse *eu;
	struct vertexuse *vu;
	fastf_t *knots;
	fastf_t u=0,v=0;
	fastf_t *ctl_points;
	int edge_no=0;
	int pt_type;
	int ncoords;
	int planar=0;
	int i;

	NMG_CK_FACEUSE( fu );
	NMG_CK_SNURB( srf );

	pt_type = RT_NURB_MAKE_PT_TYPE( 2, RT_NURB_PT_UV, RT_NURB_PT_NONRAT );
	ncoords = RT_NURB_EXTRACT_COORDS( srf->pt_type );

	if( srf->order[0] < 3 && srf->order[1] < 3 )
		planar = 1;

	lu = nmg_mlv( &fu->l.magic, (struct vertex *)NULL, OT_SAME );
	vu = BU_LIST_FIRST(vertexuse, &lu->down_hd);
	eu = nmg_meonvu(vu);
	for( i=0 ; i<3 ; i++ )
		(void)nmg_eusplit( (struct vertex *)NULL, eu, 0 );


	/* assign vertex geometry */
	for( BU_LIST_FOR( eu, edgeuse, &lu->down_hd ) )
	{
		int u_index=0,v_index=0;

		NMG_CK_EDGEUSE( eu );
		vu = eu->vu_p;
		NMG_CK_VERTEXUSE( vu );
		switch( edge_no )
		{
			case 0:
				u_index = 0;
				v_index = 0;
				u = srf->u.knots[0];
				v = srf->v.knots[0];
				break;
			case 1:
				u_index = srf->s_size[0] - 1;
				v_index = 0;
				u = srf->u.knots[srf->u.k_size - 1];
				v = srf->v.knots[0];
				break;
			case 2:
				u_index = srf->s_size[0] - 1;
				v_index = srf->s_size[1] - 1;
				u = srf->u.knots[srf->u.k_size - 1];
				v = srf->v.knots[srf->v.k_size - 1];
				break;
			case 3:
				u_index = 0;
				v_index = srf->s_size[0] - 1;
				u = srf->u.knots[0];
				v = srf->v.knots[srf->v.k_size - 1];
				break;
		}
		nmg_vertex_gv( vu->v_p, &srf->ctl_points[(u_index*srf->s_size[1] + v_index)*ncoords] );
		Assign_vu_geom( vu, u, v, srf );
		edge_no++;
	}

	/* assign edge geometry */
	edge_no = 0;
	for( BU_LIST_FOR( eu, edgeuse, &lu->down_hd ) )
	{
		NMG_CK_EDGEUSE( eu );
		vu = eu->vu_p;
		NMG_CK_VERTEXUSE( vu );
		if( planar )
			nmg_edge_g( eu );
		else
		{
			ctl_points = (fastf_t *)bu_calloc( sizeof( fastf_t ), 4, "ctl_points" );
			switch( edge_no )
			{
				case 0:
					ctl_points[0] = 0.0;
					ctl_points[1] = 0.0;
					ctl_points[2] = 1.0;
					ctl_points[3] = 0.0;
					break;
				case 1:
					ctl_points[0] = 1.0;
					ctl_points[1] = 0.0;
					ctl_points[2] = 1.0;
					ctl_points[3] = 1.0;
					break;
				case 2:
					ctl_points[0] = 1.0;
					ctl_points[1] = 1.0;
					ctl_points[2] = 0.0;
					ctl_points[3] = 1.0;
					break;
				case 3:
					ctl_points[0] = 0.0;
					ctl_points[1] = 1.0;
					ctl_points[2] = 0.0;
					ctl_points[3] = 0.0;
					break;
			}
			knots = (fastf_t *)bu_calloc( sizeof( fastf_t ), 4, "knots" );
			knots[0] = 0.0;
			knots[1] = 0.0;
			knots[2] = 1.0;
			knots[3] = 1.0;
			nmg_edge_g_cnurb( eu, 2, 4, knots, 2, pt_type, ctl_points );
		}
		edge_no++;
	}

	return( lu );
}

void
Assign_surface_to_fu( fu, srf )
struct faceuse *fu;
struct face_g_snurb *srf;
{
	struct face *f;

	NMG_CK_FACEUSE( fu );
	NMG_CK_SNURB( srf );

	f = fu->f_p;
	NMG_CK_FACE( f );

	if( f->g.snurb_p )
		rt_bomb( "Assign_surface_to_fu: fu already has geometry\n" );

	fu->orientation = OT_SAME;
	fu->fumate_p->orientation = OT_OPPOSITE;

	f->g.snurb_p = srf;
	f->flip = 0;
	BU_LIST_APPEND( &srf->f_hd, &f->l );
}

struct faceuse *
trim_surf( entityno , s )
int entityno;
struct shell *s;
{
	struct model *m;
	struct face_g_snurb *srf;
	struct faceuse *fu;
	struct loopuse *lu;
	struct loopuse *kill_lu;
	struct vertex *verts[3];
	int entity_type;
	int surf_de;
	int has_outer_boundary,inner_loop_count,outer_loop;
	int *inner_loop=NULL;
	int i;
	int lu_uv_orient;

	NMG_CK_SHELL( s );

	m = nmg_find_model( &s->l.magic );
	NMG_CK_MODEL( m );

	if( bu_debug & BU_DEBUG_MEM_CHECK )
	{
		bu_log( "barriercheck at start of trim_surf():\n" );
		bu_mem_barriercheck();
	}

	/* Acquiring Data */
	if( dir[entityno]->param <= pstart )
	{
		bu_log( "Illegal parameter pointer for entity D%07d (%s)\n" ,
				dir[entityno]->direct , dir[entityno]->name );
		return(0);
	}

	Readrec( dir[entityno]->param );
	Readint( &entity_type , "" );
	if( entity_type != 144 )
	{
		bu_log( "Expected Trimmed Surface Entity found type %d\n" );
		return( (struct faceuse *)NULL );
	}
	Readint( &surf_de , "" );
	Readint( &has_outer_boundary , "" );
	Readint( &inner_loop_count , "" );
	Readint( &outer_loop , "" );
	if( inner_loop_count )
	{
		inner_loop = (int *)bu_calloc( inner_loop_count , sizeof( int ) , "trim_surf: innerloop" );
		for( i=0 ; i<inner_loop_count ; i++ )
			Readint( &inner_loop[i] , "" );
	}

	if( (srf=Get_nurb_surf( (surf_de-1)/2, m )) == (struct face_g_snurb *)NULL )
	{
		if( inner_loop_count )
			bu_free( (char *)inner_loop , "trim_surf: inner_loop" );
		return( (struct faceuse *)NULL );
	}

	/* Make a face (with a loop to be destroted later)
	 * because loop routines insist that face and face geometry
	 * must already be assigned
	 */
	for( i=0 ; i<3 ; i++ )
		verts[i] = (struct vertex *)NULL;

	if( bu_debug & BU_DEBUG_MEM_CHECK )
	{
		bu_log( "barriercheck before making face:\n" );
		bu_mem_barriercheck();
	}

	fu = nmg_cface( s, verts, 3 );
	Assign_surface_to_fu( fu, srf );

	kill_lu = BU_LIST_FIRST( loopuse, &fu->lu_hd );

	if( !has_outer_boundary )
	{
		if( bu_debug & BU_DEBUG_MEM_CHECK )
		{
			bu_log( "barriercheck before making default loop():\n" );
			bu_mem_barriercheck();
		}

		lu = Make_default_loop( srf, fu);
	}
	else
	{
		if( bu_debug & BU_DEBUG_MEM_CHECK )
		{
			bu_log( "barriercheck before Make_loop():\n" );
			bu_mem_barriercheck();
		}

		lu = Make_loop( (outer_loop-1)/2, OT_SAME, surf_de, srf, fu );
	}

	if( bu_debug & BU_DEBUG_MEM_CHECK )
	{
		bu_log( "barriercheck after making loop:\n" );
		bu_mem_barriercheck();
	}


	(void)nmg_klu( kill_lu );

	/* first loop is an outer loop, orientation must be OT_SAME */
	if( bu_debug & BU_DEBUG_MEM_CHECK )
	{
		bu_log( "barriercheck before nmg_snurb_calc_lu_uv_orient(():\n" );
		bu_mem_barriercheck();
		bu_log( "check complete!!!\n" );
	}

	lu_uv_orient = nmg_snurb_calc_lu_uv_orient( lu );
	if( bu_debug & BU_DEBUG_MEM_CHECK )
	{
		bu_log( "barriercheck after nmg_snurb_calc_lu_uv_orient(():\n" );
		bu_mem_barriercheck();
	}
	if( lu_uv_orient == OT_SAME )
		nmg_set_lu_orientation( lu, 0 );
	else
	{
		nmg_reverse_face( fu );
		nmg_set_lu_orientation( lu, 0 );
	}

	for( i=0 ; i<inner_loop_count ; i++ )
	{
		lu = Make_loop( (inner_loop[i]-1)/2, OT_OPPOSITE, surf_de, srf, fu );

		/* These loops must all be OT_OPPOSITE */
		lu_uv_orient = nmg_snurb_calc_lu_uv_orient( lu );
		if( (lu_uv_orient == OT_OPPOSITE && !fu->f_p->flip) ||
		    (lu_uv_orient == OT_SAME && fu->f_p->flip) )
				continue;

		/* loop is in wrong direction, exchange lu and lu_mate */
		BU_LIST_DEQUEUE( &lu->l );
		BU_LIST_DEQUEUE( &lu->lumate_p->l );
		BU_LIST_APPEND( &fu->lu_hd , &lu->lumate_p->l );
		lu->lumate_p->up.fu_p = fu;
		BU_LIST_APPEND( &fu->fumate_p->lu_hd , &lu->l );
		lu->up.fu_p = fu->fumate_p;
	}

	if( inner_loop_count )
		bu_free( (char *)inner_loop , "trim_surf: inner_loop" );

	NMG_CK_FACE_G_SNURB( fu->f_p->g.snurb_p );

	return( fu );
}

int
uv_in_fu( u, v, fu )
fastf_t u, v;
struct faceuse *fu;
{
	int ot_sames, ot_opps;
	struct loopuse *lu;

	/* check if point is in face (trimming curves) */
	ot_sames = 0;
	ot_opps = 0;

	for( BU_LIST_FOR( lu, loopuse, &fu->lu_hd ) )
	{
		if( BU_LIST_FIRST_MAGIC( &lu->down_hd ) != NMG_EDGEUSE_MAGIC )
			continue;

		if( lu->orientation == OT_SAME )
			ot_sames += nmg_uv_in_lu( u, v, lu );
		else if( lu->orientation == OT_OPPOSITE )
			ot_opps += nmg_uv_in_lu( u, v, lu );
		else
		{
			bu_log( "isect_ray_snurb_face: lu orientation = %s!!\n",
				nmg_orientation( lu->orientation ) );
			bu_bomb( "isect_ray_snurb_face: bad lu orientation\n" );
		}
	}

	if( ot_sames == 0 || ot_opps == ot_sames )
	{
		/* not a hit */
		return( 0 );
	}
	else
	{
		/* this is a hit */
		return( 1 );
	}
}

/* find all the intersections of fu along line through "mid_pt",
 * in direction "ray_dir". Place hits on "hit_list"
 */
void
find_intersections( fu, mid_pt, ray_dir, hit_list )
struct faceuse *fu;
point_t mid_pt;
vect_t ray_dir;
struct bu_list *hit_list;
{
	plane_t pl1, pl2;
	struct bu_list bezier;
	struct face *f;
	struct face_g_snurb *fg;

	NMG_CK_FACEUSE( fu );

	f = fu->f_p;

	if( *f->g.magic_p != NMG_FACE_G_SNURB_MAGIC )
		bu_bomb( "ERROR: find_intersections(): face is not a TNURB surface!!!!\n" );

	fg = f->g.snurb_p;

	bn_vec_ortho( pl2, ray_dir );
	VCROSS( pl1, pl2, ray_dir );
	pl1[3] = VDOT( mid_pt, pl1 );
	pl2[3] = VDOT( mid_pt, pl2 );

	BU_LIST_INIT( &bezier );

	rt_nurb_bezier( &bezier, fg, (struct resource *)NULL );
	while( BU_LIST_NON_EMPTY( &bezier ) )
	{
		struct face_g_snurb *srf;
		struct rt_nurb_uv_hit *hp;

		srf = BU_LIST_FIRST( face_g_snurb,  &bezier );
		BU_LIST_DEQUEUE( &srf->l );

		hp = rt_nurb_intersect( srf, pl1, pl2, UV_TOL, (struct resource *)NULL );
		/* process each hit point */
		while( hp != (struct rt_nurb_uv_hit *)NULL )
		{
			struct rt_nurb_uv_hit *next;
			struct snurb_hit *myhit;
			vect_t to_hit;
			fastf_t homo_hit[4];

			next = hp->next;

			if (rt_g.NMG_debug & DEBUG_RT_ISECT)
				bu_log( "\tintersect snurb surface at uv=(%g %g)\n", hp->u, hp->v );

			/* check if point is in face (trimming curves) */
			if( !uv_in_fu( hp->u, hp->v, fu ) )
			{
				/* not a hit */

				if (rt_g.NMG_debug & DEBUG_RT_ISECT)
					bu_log( "\tNot a hit\n" );

				bu_free( (char *)hp, "nurb_uv_hit" );
				hp = next;
				continue;
			}

			myhit = (struct snurb_hit *)bu_malloc( sizeof( struct snurb_hit ), "myhit" );
			myhit->f = f;

			/* calculate actual hit point (x y z) */
			rt_nurb_s_eval( srf, hp->u, hp->v, homo_hit );
			if( RT_NURB_IS_PT_RATIONAL( srf->pt_type ) )
			{
				fastf_t inv_homo;

				inv_homo = 1.0/homo_hit[3];
				VSCALE( myhit->pt, homo_hit, inv_homo )
			}
			else
				VMOVE( myhit->pt, homo_hit )

			VSUB2( to_hit, myhit->pt, mid_pt );
			myhit->dist = VDOT( to_hit, ray_dir );

			/* get surface normal */
			rt_nurb_s_norm( srf, hp->u, hp->v, myhit->norm );

			/* may need to reverse it */
			if( f->flip )
				VREVERSE( myhit->norm, myhit->norm )

			/* add hit to list */
			if( BU_LIST_IS_EMPTY( hit_list ) )
				BU_LIST_APPEND( hit_list, &myhit->l )
			else
			{
				struct snurb_hit *tmp;

				for( BU_LIST_FOR( tmp, snurb_hit, hit_list ) )
				{
					if( tmp->dist >= myhit->dist )
					{
						BU_LIST_INSERT( &tmp->l, &myhit->l );
						break;
					}
				}
				if( myhit->l.forw == (struct bu_list *)0 )
					BU_LIST_INSERT( hit_list, &myhit->l )
			}

			bu_free( (char *)hp, "nurb_uv_hit" );
			hp = next;
		}
		rt_nurb_free_snurb( srf, (struct resource *)NULL );
	}
}

/* adjust flip flag on faces using hit list data */
void
adjust_flips( hit_list, ray_dir )
struct bu_list *hit_list;
vect_t ray_dir;
{
	struct snurb_hit *hit;
	int enter=0;
	fastf_t prev_dist=(-MAX_FASTF);

	for( BU_LIST_FOR( hit, snurb_hit, hit_list ) )
	{
		fastf_t dot;

		if( !NEAR_ZERO( hit->dist - prev_dist, tol.dist ) )
			enter = !enter;

		dot = VDOT( hit->norm, ray_dir );

		if( (enter && dot > 0.0) || (!enter && dot < 0.0) )
		{
			struct snurb_hit *tmp;

			/* reverse this face */
			hit->f->flip = !(hit->f->flip);
			for( BU_LIST_FOR( tmp, snurb_hit, hit_list ) )
			{
				if( tmp->f == hit->f )
				{
					VREVERSE( tmp->norm, tmp->norm )
				}
			}
		}

		prev_dist = hit->dist;
	}
}

/* Find a uv point that is actually in the face */
int
Find_uv_in_fu( u_in, v_in, fu )
fastf_t *u_in, *v_in;
struct faceuse *fu;
{
	struct face *f;
	struct face_g_snurb *fg;
	struct loopuse *lu;
	fastf_t umin, umax, vmin, vmax;
	fastf_t u, v;
	int i;

	NMG_CK_FACEUSE( fu );

	f = fu->f_p;

	if( *f->g.magic_p != NMG_FACE_G_SNURB_MAGIC )
		return( 1 );

	fg = f->g.snurb_p;

	umin = fg->u.knots[0];
	umax = fg->u.knots[fg->u.k_size-1];
	vmin = fg->v.knots[0];
	vmax = fg->v.knots[fg->v.k_size-1];

	/* first try the center of the uv-plane */
	u = (umin + umax)/2.0;
	v = (vmin + vmax)/2.0;

	if( uv_in_fu( u, v, fu ) )
	{
		*u_in = u;
		*v_in = v;
		return( 0 );
	}

	/* no luck, try a few points along the diagonals of the UV plane */
	for( i=0 ; i<10 ; i++ )
	{
		u = umin + (umax - umin)*(double)(i + 1)/11.0;
		v = vmin + (vmax - vmin)*(double)(i + 1)/11.0;

		if( uv_in_fu( u, v, fu ) )
		{
			*u_in = u;
			*v_in = v;
			return( 0 );
		}
	}

	/* try other diagonal */
	for( i=0 ; i<10 ; i++ )
	{
		u = umin + (umax - umin)*(double)(i + 1)/11.0;
		v = vmax - (vmax - vmin)*(double)(i + 1)/11.0;

		if( uv_in_fu( u, v, fu ) )
		{
			*u_in = u;
			*v_in = v;
			return( 0 );
		}
	}

	/* last resort, look at loops */
	for( BU_LIST_FOR( lu, loopuse, &fu->lu_hd ) )
	{
		struct edgeuse *eu;

		if( BU_LIST_FIRST_MAGIC( &lu->down_hd ) != NMG_EDGEUSE_MAGIC )
			continue;

		for( BU_LIST_FOR( eu, edgeuse, &lu->down_hd ) )
		{
			struct edge_g_cnurb *eg;

			if( *eu->g.magic_p != NMG_EDGE_G_CNURB_MAGIC )
				continue;

			eg = eu->g.cnurb_p;

			if(RT_NURB_IS_PT_RATIONAL( eg->pt_type ) )
			{
				u = eu->vu_p->a.cnurb_p->param[0] / eu->vu_p->a.cnurb_p->param[2];
				v = eu->vu_p->a.cnurb_p->param[1] / eu->vu_p->a.cnurb_p->param[2];
			}
			else
			{
				u = eu->vu_p->a.cnurb_p->param[0];
				v = eu->vu_p->a.cnurb_p->param[1];
			}

			if( uv_in_fu( u, v, fu ) )
			{
				*u_in = u;
				*v_in = v;
				return( 0 );
			}
		}
	}

	return( 1 );
}

/* find an xyz pt that s actually in the face
 * and the surface normal at that point
 */
int
Find_pt_in_fu( fu, pt, norm, hit_list )
struct faceuse *fu;
point_t pt;
vect_t norm;
struct bu_list *hit_list;
{
	struct face *f;
	struct face_g_snurb *fg;
	fastf_t u, v;
	fastf_t homo_hit[4];

	NMG_CK_FACEUSE( fu );

	f = fu->f_p;

	if( *f->g.magic_p != NMG_FACE_G_SNURB_MAGIC )
		return( 1 );

	fg = f->g.snurb_p;

	if( Find_uv_in_fu( &u, &v, fu ) )
		return( 1 );

	/* calculate actual hit point (x y z) */
	rt_nurb_s_eval( fg, u, v, homo_hit );
	if( RT_NURB_IS_PT_RATIONAL( fg->pt_type ) )
	{
		fastf_t inv_homo;

		inv_homo = 1.0/homo_hit[3];
		VSCALE( pt, homo_hit, inv_homo )
	}
	else
		VMOVE( pt, homo_hit )

	/* get surface normal */
	rt_nurb_s_norm( fg, u, v, norm );

	/* may need to reverse it */
	if( f->flip )
		VREVERSE( norm, norm )

	return( 0 );
}

void
Convtrimsurfs()
{

	int i,convsurf=0,totsurfs=0;
	struct model *m;
	struct nmgregion *r;
	struct shell *s;
	struct faceuse *fu;
	struct bu_list hit_list;

	bu_log( "\n\nConverting Trimmed Surface entities:\n" );

	if( RT_G_DEBUG & DEBUG_MEM_FULL )
		bu_mem_barriercheck();

	m = nmg_mm();
	r = nmg_mrsv( m );
	s = BU_LIST_FIRST( shell , &r->s_hd );

	for( i=0 ; i<totentities ; i++ )
	{
		if( dir[i]->type == 144 )
		{
			if( RT_G_DEBUG & DEBUG_MEM_FULL )
				bu_mem_barriercheck();

			totsurfs++;
			fu = trim_surf( i , s );
			if( fu )
			{
				nmg_face_bb( fu->f_p , &tol );
				convsurf++;
			}
			if( RT_G_DEBUG & DEBUG_MEM_FULL )
				bu_mem_barriercheck();

		}
	}

	nmg_rebound( m, &tol );

	bu_log( "\n\t%d surfaces converted, adusting surface normals....\n", convsurf );

	/* do some raytracing to get face orientations correct */
	for( BU_LIST_FOR( fu, faceuse, &s->fu_hd ) )
	{
		struct faceuse *fu2;
		point_t mid_pt;
		vect_t ray_dir;

		if( fu->orientation != OT_SAME )
			continue;

		BU_LIST_INIT( &hit_list );
		if( Find_pt_in_fu( fu, mid_pt, ray_dir/* !!! fourth param missing */ ) )
		{
			bu_log( "Convtrimsurfs: Cannot find a point in fu (x%x)\n", fu );
			nmg_pr_fu( fu, " " );
			bu_bomb( "Convtrimsurfs: Cannot find a point in fu\n" );
		}

		/* find intersections with all the faces
		 * must include current fu since there
		 * may be more than one intersection
		 */
		for( BU_LIST_FOR( fu2, faceuse, &s->fu_hd ) )
		{
			if( fu2->orientation != OT_SAME )
				continue;

			find_intersections( fu2, mid_pt, ray_dir, &hit_list );
		}

		adjust_flips( &hit_list, ray_dir );

		while( BU_LIST_NON_EMPTY( &hit_list ) )
		{
			struct snurb_hit *myhit;

			myhit = BU_LIST_FIRST( snurb_hit, &hit_list );
			BU_LIST_DEQUEUE( &myhit->l );

			bu_free( (char *)myhit, "myhit" );
		}

	}

	bu_log( "Converted %d Trimmed Sufaces successfully out of %d total Trimmed Sufaces\n" , convsurf , totsurfs );

	if( RT_G_DEBUG & DEBUG_MEM_FULL )
		bu_mem_barriercheck();

	if( convsurf )
	{
		(void)nmg_model_vertex_fuse( m, &tol );

		if( curr_file->obj_name )
			mk_nmg( fdout , curr_file->obj_name , m );
		else
			mk_nmg( fdout , "Trimmed_surf" , m );
	}
	if( RT_G_DEBUG & DEBUG_MEM_FULL )
		bu_mem_barriercheck();

	if( !convsurf ) {
	    nmg_km( m );
	}

	if( RT_G_DEBUG & DEBUG_MEM_FULL )
		bu_mem_barriercheck();

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
