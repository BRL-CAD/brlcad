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
 *	This software is Copyright (C) 1994 by the United States Army
 *	in all countries except the USA. All rights reserved.
 */

/*	This routine loops through all the directory entries and calls
	appropriate routines to convert trimmed surface entities to BRLCAD
	NMG TNURBS	*/

#ifndef lint
static char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "conf.h"

#include <stdio.h>
#include "machine.h"
#include "vmath.h"
#include "nmg.h"
#include "raytrace.h"
#include "nurb.h"
#include "./iges_struct.h"
#include "./iges_extern.h"
#include "wdb.h"

/* translations to get knot vectors in first quadrant */
static fastf_t u_translation=0.0;
static fastf_t v_translation=0.0;

RT_EXTERN( struct cnurb *rt_arc2d_to_cnurb, ( point_t center, point_t start, point_t end, int pt_type, struct rt_tol *tol ) );

#define CTL_INDEX(_i,_j)	((_i * n_cols + _j) * ncoords)

struct snurb *
Get_nurb_surf( entityno )
int entityno;
{
	struct snurb *srf;
	int entity_type;
	int i,j;
	int num_knots;
	int num_pts;
	int rational;
	int ncoords;
	int n_rows,n_cols,u_order,v_order;
	int n_u_knots,n_v_knots;
	int pt_type;
	fastf_t u_min,u_max;
	fastf_t v_min,v_max;
	double a;
rt_log( "Get_nurb_surf( %d )\n" , entityno );

	/* Acquiring Data */

	if( dir[entityno]->param <= pstart )
	{
		rt_log( "Illegal parameter pointer for entity D%07d (%s)\n" ,
				dir[entityno]->direct , dir[entityno]->name );
		return( (struct snurb *)NULL );
	}

	Readrec( dir[entityno]->param );
	Readint( &entity_type , "" );
	if( entity_type != 128 )
	{
		rt_log( "Only B-Spline surfaces allowed for faces (found type %d)\n", entity_type );
		return( (struct snurb *)NULL );
	}
	Readint( &i , "" );
	n_rows = i+1;
	Readint( &i , "" );
	n_cols = i+1;
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

	n_u_knots = n_cols+u_order;
	n_v_knots = n_rows+v_order;
	srf = rt_nurb_new_snurb( u_order, v_order, n_u_knots, n_v_knots, n_rows, n_cols, pt_type );

	/* Read knot vectors */
	for( i=0 ; i<n_v_knots ; i++ )
	{
		Readdbl( &a , "" );
		srf->v_knots.knots[i] = a;
	}
	for( i=0 ; i<n_u_knots ; i++ )
	{
		Readdbl( &a , "" );
		srf->u_knots.knots[i] = a;
	}

	/* Insure that knot values are non-negative */
	if( srf->v_knots.knots[0] < 0.0 )
	{
		v_translation = (-srf->v_knots.knots[0]);
		for( i=0 ; i<n_v_knots ; i++ )
			srf->v_knots.knots[i] += v_translation;
	}
	else
		v_translation = 0.0;

	if( srf->u_knots.knots[0] < 0.0 )
	{
		u_translation = (-srf->u_knots.knots[0]);
		for( i=0 ; i<n_u_knots ; i++ )
			srf->u_knots.knots[i] += u_translation;
	}
	else
		u_translation = 0.0;

	for( j=0 ; j<n_cols ; j++ )
		for( i=0 ; i<n_rows ; i++ )
		{
			Readdbl( &a , "" );
			if( rational )
				srf->ctl_points[CTL_INDEX(i,j)+3] = a;
		}
	for( j=0 ; j<n_cols ; j++ )
		for( i=0 ; i<n_rows ; i++ )
		{
			Readcnv( &a , "" );
			srf->ctl_points[CTL_INDEX(i,j)] = a;
			Readcnv( &a , "" );
			srf->ctl_points[CTL_INDEX(i,j)+1] = a;
			Readcnv( &a , "" );
			srf->ctl_points[CTL_INDEX(i,j)+2] = a;
		}
	Readdbl( &a , "" );
	u_min = a;
	Readdbl( &a , "" );
	u_max = a;
	Readdbl( &a , "" );
	v_min = a;
	Readdbl( &a , "" );
	v_max = a;

	return( srf );
}

void
Assign_cnurb_to_eu( eu, crv )
struct edgeuse *eu;
struct cnurb *crv;
{
	fastf_t *ctl_points;
	fastf_t *knots;
	int ncoords;
	int i;

	NMG_CK_EDGEUSE( eu );
	NMG_CK_CNURB( crv );

	ncoords = RT_NURB_EXTRACT_COORDS( crv->pt_type );

	ctl_points = (fastf_t *)rt_calloc( ncoords*crv->c_size, sizeof( fastf_t ),
			"Assign_cnurb_to_eu: ctl_points" );

	for( i=0 ; i<crv->c_size ; i++ )
		VMOVEN( &ctl_points[i*ncoords], &crv->ctl_points[i*ncoords], ncoords )

	knots = (fastf_t *)rt_calloc( crv->knot.k_size, sizeof( fastf_t ),
			"Assign_cnurb_to_eu: knots" );

	for( i=0 ; i<crv->knot.k_size; i++ )
		knots[i] = crv->knot.knots[i];

	nmg_edge_g_cnurb( eu, crv->order, crv->knot.k_size, knots, crv->c_size,
			crv->pt_type, ctl_points );
}

struct cnurb *
Get_cnurb( entity_no )
int entity_no;
{
	struct cnurb *crv;
	int entity_type;
	int num_pts;
	int degree;
	int i,j;
	int planar;
	int rational;
	int pt_type;
	int ncoords;
	double a;
	double x,y,z;
rt_log( "Get_cnurb( %d )\n" ,  entity_no );

	if( dir[entity_no]->param <= pstart )
	{
		rt_log( "Get_cnurb: Illegal parameter pointer for entity D%07d (%s)\n" ,
				dir[entity_no]->direct , dir[entity_no]->name );
		return( (struct cnurb *)NULL );
	}

	Readrec( dir[entity_no]->param );
	Readint( &entity_type , "" );

	if( entity_type != 126 )
	{
		rt_log( "Get_cnurb: Was expecting spline curve, got type %d\n" , entity_type );
		return( (struct cnurb *)NULL );
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
		crv->knot.knots[i] = a;
	}

	/* weights */
	for( i=0 ; i<num_pts ; i++ )
	{
		Readdbl( &a , "" );
		if( rational )
			crv->ctl_points[i*ncoords+2] = a;
	}

	/* vertices */
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
		crv->ctl_points[i*ncoords] = y + u_translation;
		crv->ctl_points[i*ncoords+1] = x + v_translation;
	}

	return( crv );
}

void
Assign_vu_geom( vu, u, v, srf )
struct vertexuse *vu;
fastf_t u,v;
struct snurb *srf;
{
	point_t uvw;
	point_t pt_on_srf;
	struct vertexuse *vu1;

	NMG_CK_VERTEXUSE( vu );
	NMG_CK_SNURB( srf );

	rt_nurb_s_eval( srf, u, v, pt_on_srf );
	nmg_vertex_gv( vu->v_p, pt_on_srf );
	VSET( uvw, u, v, 1.0 );

	for( RT_LIST_FOR( vu1, vertexuse, &vu->v_p->vu_hd ) )
		nmg_vertexuse_a_cnurb( vu1, uvw );
}

void
Add_trim_curve( entity_no, lu, srf )
int entity_no;
struct loopuse *lu;
struct snurb *srf;
{
	int entity_type;
	struct cnurb *crv;
	struct edgeuse *eu;
	struct edgeuse *new_eu;
	struct vertex *vp;
	double x,y,z;
	fastf_t u,v;
	int ncoords;
	int i;

rt_log( "in Add_trim_curve, entity_no = %d\n", entity_no );

	NMG_CK_LOOPUSE( lu );
	NMG_CK_SNURB( srf );

	if( dir[entity_no]->param <= pstart )
	{
		rt_log( "Illegal parameter pointer for entity D%07d (%s)\n" ,
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
rt_log( "\tComposite curve\n" );

				Readint( &curve_count , "Curve Count: " );
				curve_list = (int *)rt_calloc( curve_count , sizeof( int ),
						"Add_trim_curve: curve_list" );

				for( i=0 ; i<curve_count ; i++ )
					Readint( &curve_list[i] , "\t Curve: " );

				for( i=0 ; i<curve_count ; i++ )
					Add_trim_curve( (curve_list[i]-1)/2, lu, srf );

				rt_free( (char *)curve_list , "Add_trim_curve: curve_list" );
			}
			break;
		case 110:	/* line */
rt_log( "\tLines\n" );
			/* get start point */
			Readdbl( &x , "X: " );
			Readdbl( &y , "Y: " );
			Readdbl( &z , "Z: " );

			/* Split last edge in loop */
			eu = RT_LIST_LAST( edgeuse, &lu->down_hd );
			new_eu = nmg_eusplit( (struct vertex *)NULL, eu, 0 );
			vp = eu->vu_p->v_p;

			/* if old edge doesn't have vertex geometry, assign some */
			if( !vp->vg_p )
			{
				u = y + u_translation;
				v = x + v_translation;
rt_log( "\t\tstart point at ( %g %g ) translated to ( %g %g )\n", x, y, u, v );
				Assign_vu_geom( eu->vu_p, u, v, srf );
			}

			/* read terminate point */
			Readdbl( &x , "X: " );
			Readdbl( &y , "Y: " );
			Readdbl( &z , "Z: " );

			/* assign geometry to vertex of new edgeuse */
			u = y + u_translation;
			v = x + v_translation;
rt_log( "\t\tend point at ( %g %g ) translated to ( %g %g )\n", x, y, u, v );
			Assign_vu_geom( new_eu->vu_p, u, v, srf );

			/* assign edge geometry */
			nmg_edge_g_cnurb( eu, 2, 0, (fastf_t *)NULL, 2,
				RT_NURB_MAKE_PT_TYPE( 2, RT_NURB_PT_UV, RT_NURB_PT_NONRAT),
				 (fastf_t *)NULL );
			break;
		case 100:	/* circular arc */
			{
				struct cnurb *crv;
				point_t center, start, end;
rt_log( "In Add_trim_curve; ARC:\n" );

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

				/* build cnurb arc */
				crv = rt_arc2d_to_cnurb( center, start, end, RT_NURB_PT_UV, &tol );
rt_log( "\t ARC:\n" );
rt_nurb_c_print( crv );

				/* add a new edge to loop */
rt_log( "split eu\n" );
				eu = RT_LIST_LAST( edgeuse, &lu->down_hd );
				new_eu = nmg_eusplit( (struct vertex *)NULL, eu, 0 );
				vp = eu->vu_p->v_p;

				/* if old edge doesn't have vertex geometry, assign some */
				if( !vp->vg_p )
				{
rt_log( "Assign geometry to start of eu\n" );
					u = start[X];
					v = start[Y];
					Assign_vu_geom( eu->vu_p, u, v, srf );
				}

				/* Assign geometry to new vertex */
rt_log( "Assign geometry to end of eu\n" );
				u = end[X];
				v = end[Y];
				Assign_vu_geom( new_eu->vu_p, u, v, srf );

				/* Assign edge geometry */
rt_log( "Assign crv geometry to eu\n" );
				Assign_cnurb_to_eu( eu, crv );

				rt_nurb_free_cnurb( crv );
			}
			break;

		case 104:	/* conic arc */
			rt_log( "Conic Arc not yet handled as a trimming curve\n" );
			break;

		case 126:	/* spline curve */
			/* Get spline curve */
rt_log( "\tSpline curve\n" );
			crv = Get_cnurb( entity_no );
rt_nurb_c_print( crv );

			ncoords = RT_NURB_EXTRACT_COORDS( crv->pt_type );
rt_log( "split eu \n" );
			/* add a new edge to loop */
			eu = RT_LIST_LAST( edgeuse, &lu->down_hd );
			new_eu = nmg_eusplit( (struct vertex *)NULL, eu, 0 );
			vp = eu->vu_p->v_p;

			/* if old edge doesn't have vertex geometry, assign some */
			if( !vp->vg_p )
			{
rt_log( "Assign geometry to start of eu\n" );
				Assign_vu_geom( eu->vu_p, crv->ctl_points[0],
					crv->ctl_points[1], srf );
			}

			/* Assign geometry to new vertex */
rt_log( "Assign geometry to end of eu\n" );
			Assign_vu_geom( new_eu->vu_p, crv->ctl_points[(crv->c_size-1)*ncoords],
					crv->ctl_points[(crv->c_size-1)*ncoords+1], srf );

			/* Assign edge geometry */
rt_log( "Assign crv geometry to eu\n" );
			Assign_cnurb_to_eu( eu, crv );

			rt_nurb_free_cnurb( crv );

			break;
		default:
			rt_log( "Curves of type %d are not yet handled for trimmed surfaces\n", entity_type );
			break;
	}
}

struct loopuse *
Make_trim_loop( entity_no, orientation, srf, fu )
int entity_no;
int orientation;
struct snurb *srf;
struct faceuse *fu;
{
	struct cnurb *crv;
	struct loopuse *lu;
	struct edgeuse *eu;
	struct edgeuse *new_eu;
	struct vertexuse *vu;
	struct vertex *vp;
	point_t pt_on_srf;
	vect_t uvw;
	int entity_type;
	int ncoords;
	double x,y,z;
	fastf_t u,v;
	int i;

rt_log( "In Make_trim_loop, entity_no = %d\n", entity_no );

	NMG_CK_SNURB( srf );
	NMG_CK_FACEUSE( fu );

	if( dir[entity_no]->param <= pstart )
	{
		rt_log( "Illegal parameter pointer for entity D%07d (%s)\n" ,
				dir[entity_no]->direct , dir[entity_no]->name );
		return( (struct loopuse *)NULL );
	}

	Readrec( dir[entity_no]->param );
	Readint( &entity_type , "" );

	lu = nmg_mlv( &fu->l.magic, (struct vertex *)NULL, orientation );
	vu = RT_LIST_FIRST(vertexuse, &lu->down_hd);
	eu = nmg_meonvu(vu);

	switch( entity_type )
	{
		case 102:	/* composite curve */
			{
				int curve_count;
				int *curve_list;
rt_log( "Composite curve\n" );

				Readint( &curve_count , "Curve Count: " );
				curve_list = (int *)rt_calloc( curve_count , sizeof( int ),
						"Make_trim_loop: curve_list" );

				for( i=0 ; i<curve_count ; i++ )
					Readint( &curve_list[i] , "\tCurve: " );

				for( i=0 ; i<curve_count ; i++ )
					Add_trim_curve( (curve_list[i]-1)/2, lu, srf );

				rt_free( (char *)curve_list , "Make_trim_loop: curve_list" );
			}
			break;
		case 110:	/* line */

			/* get sart point */
			Readdbl( &x , "" );
			Readdbl( &y , "" );
			Readdbl( &z , "" );

			/* Split last edge in loop */
			eu = RT_LIST_LAST( edgeuse, &lu->down_hd );
			new_eu = nmg_eusplit( (struct vertex *)NULL, eu, 0 );
			vp = eu->vu_p->v_p;

			/* if old edge doesn't have vertex geometry, assign some */
			if( !vp->vg_p )
			{
				u = y + u_translation;
				v = x + v_translation;
				Assign_vu_geom( eu->vu_p, u, v, srf );
			}

			/* read terminate point */
			Readdbl( &x , "" );
			Readdbl( &y , "" );
			Readdbl( &z , "" );

			/* assign geometry to vertex of new edgeuse */
			u = y + u_translation;
			v = x + v_translation;
			Assign_vu_geom( new_eu->vu_p, u, v, srf );

			/* assign edge geometry */
			nmg_edge_g_cnurb( eu, 2, 0, (fastf_t *)NULL, 2,
				RT_NURB_MAKE_PT_TYPE( 3, RT_NURB_PT_UV, RT_NURB_PT_RATIONAL),
				 (fastf_t *)NULL );
			break;
		case 100:	/* circular arc */
			{
				struct cnurb *crv;
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

				/* build cnurb arc */
				crv = rt_arc2d_to_cnurb( center, start, end, RT_NURB_PT_UV, &tol );

				/* add a new edge to loop */
				eu = RT_LIST_LAST( edgeuse, &lu->down_hd );
				new_eu = nmg_eusplit( (struct vertex *)NULL, eu, 0 );
				vp = eu->vu_p->v_p;

				/* if old edge doesn't have vertex geometry, assign some */
				if( !vp->vg_p )
				{
					u = start[X];
					v = start[Y];
					Assign_vu_geom( eu->vu_p, u, v, srf );
				}

				/* Assign geometry to new vertex */
				u = end[X];
				v = end[Y];
				Assign_vu_geom( new_eu->vu_p, u, v, srf );

				/* Assign edge geometry */
				Assign_cnurb_to_eu( eu, crv );

				rt_nurb_free_cnurb( crv );
			}
			break;

		case 104:	/* conic arc */
			rt_log( "Conic Arc not yet handled as a trimming curve\n" );
			break;

		case 126:	/* spline curve */
			/* Get spline curve */
			crv = Get_cnurb( entity_no );

			ncoords = RT_NURB_EXTRACT_COORDS( crv->pt_type );

			/* add a new edge to loop */
			eu = RT_LIST_LAST( edgeuse, &lu->down_hd );
			new_eu = nmg_eusplit( (struct vertex *)NULL, eu, 0 );
			vp = eu->vu_p->v_p;

			/* if old edge doesn't have vertex geometry, assign some */
			if( !vp->vg_p )
				Assign_vu_geom( eu->vu_p, crv->ctl_points[0],
					crv->ctl_points[1], v, srf );

			/* Assign geometry to new vertex */
			Assign_vu_geom( new_eu->vu_p, crv->ctl_points[(crv->c_size-1)*ncoords],
					crv->ctl_points[(crv->c_size-1)*ncoords+1], srf );

			/* Assign edge geometry */
			Assign_cnurb_to_eu( eu, crv );

			rt_nurb_free_cnurb( crv );

			break;
		default:
			rt_log( "Curves of type %d are not yet handled for trimmed surfaces\n", entity_type );
			break;
	}
	eu = RT_LIST_LAST( edgeuse, &lu->down_hd );
	nmg_keu( eu );
	return( lu );
}

struct loopuse *
Make_loop( entity_no, orientation, on_surf_de, srf, fu )
int entity_no;
int orientation;
int on_surf_de;
struct snurb *srf;
struct faceuse *fu;
{
	struct loopuse *lu;
	int entity_type;
	int surf_de,param_curve_de,model_curve_de;
	int i;

rt_log( "In Make_loop, entity_no = %d:\n", entity_no );

	NMG_CK_SNURB( srf );
	NMG_CK_FACEUSE( fu );

	/* Acquiring Data */
	if( dir[entity_no]->param <= pstart )
	{
		rt_log( "Illegal parameter pointer for entity D%07d (%s)\n" ,
				dir[entity_no]->direct , dir[entity_no]->name );
		return(0);
	}

	Readrec( dir[entity_no]->param );
	Readint( &entity_type , "" );
	if( entity_type != 142 )
	{
		rt_log( "Expected Curve on a Parametric Surface, found entity type %d\n" , entity_type );
		return( 0 );
	}
	Readint( &i , "" );
	Readint( &surf_de , "" );
	if( surf_de != on_surf_de )
		rt_log( "Curve is on surface at DE %d, should be on surface at DE %d\n", surf_de, on_surf_de );

	Readint( &param_curve_de , "" );
	Readint( &model_curve_de , "" );

	lu = Make_trim_loop( (param_curve_de-1)/2, orientation, srf, fu );
nmg_pr_lu_briefly( lu, " " );
	return( lu );
}

struct loopuse *
Make_default_loop( srf, fu )
struct snurb *srf;
struct faceuse *fu;
{
	struct shell *s;
	struct loopuse *lu;
	struct edgeuse *eu;
	struct vertexuse *vu;
	struct vertex *v[4];
	fastf_t knots[4];
	fastf_t ctl_points[4];
	int edge_no=0;
	int pt_type;
	int ncoords;
	int planar=0;
	int i;

	NMG_CK_FACEUSE( fu );
	NMG_CK_SNURB( srf );

	pt_type = RT_NURB_MAKE_PT_TYPE( 2, RT_NURB_PT_UV, RT_NURB_PT_NONRAT );
	ncoords = RT_NURB_EXTRACT_COORDS( srf->pt_type );

	if( srf->order[0] < 3 && srf->order[2] < 3 )
		planar = 1;

	lu = nmg_mlv( &fu->l.magic, (struct vertex *)NULL, OT_SAME );
	vu = RT_LIST_FIRST(vertexuse, &lu->down_hd);
	eu = nmg_meonvu(vu);
	for( i=0 ; i<3 ; i++ )
		(void)nmg_eusplit( (struct vertex *)NULL, eu, 0 );

	knots[0] = 0.0;
	knots[1] = 0.0;
	knots[2] = 1.0;
	knots[3] = 1.0;
	for( RT_LIST_FOR( eu, edgeuse, &lu->down_hd ) )
	{
		int u_index,v_index;

		NMG_CK_EDGEUSE( eu );
		vu = eu->vu_p;
		NMG_CK_VERTEXUSE( vu );
		switch( edge_no )
		{
			case 0:
				u_index = 0;
				v_index = 0;
				ctl_points[0] = 0.0;
				ctl_points[1] = 0.0;
				ctl_points[2] = 1.0;
				ctl_points[3] = 0.0;
				break;
			case 1:
				u_index = srf->s_size[0] - 1;
				v_index = 0;
				ctl_points[0] = 0.0;
				ctl_points[1] = 1.0;
				ctl_points[2] = 1.0;
				ctl_points[3] = 1.0;
				break;
			case 2:
				u_index = srf->s_size[0] - 1;
				v_index = srf->s_size[1] - 1;
				ctl_points[0] = 1.0;
				ctl_points[1] = 1.0;
				ctl_points[2] = 0.0;
				ctl_points[3] = 1.0;
				break;
			case 3:
				u_index = 0;
				v_index = srf->s_size[0] - 1;
				ctl_points[0] = 0.0;
				ctl_points[1] = 1.0;
				ctl_points[2] = 0.0;
				ctl_points[3] = 0.0;
				break;
		}
		nmg_vertex_gv( vu->v_p, &srf->ctl_points[(u_index*srf->s_size[1] + v_index)*ncoords] );
		if( planar )
			nmg_edge_g( eu );
		else
			nmg_edge_g_cnurb( eu, 2, 4, knots, 2, pt_type, ctl_points );
		edge_no++;
	}

	return( lu );
}

void
Assign_surface_to_fu( fu, srf )
struct faceuse *fu;
struct snurb *srf;
{
	fastf_t *ukv;
	fastf_t *vkv;
	fastf_t *mesh;
	int ncoords;
	int i;

	NMG_CK_FACEUSE( fu );
	NMG_CK_SNURB( srf );

	ncoords = RT_NURB_EXTRACT_COORDS( srf->pt_type );

	ukv = (fastf_t *)rt_calloc( srf->u_knots.k_size, sizeof( fastf_t ), "Assign_surface_to_fu: ukv" );
	for( i=0 ; i<srf->u_knots.k_size ; i++ )
		ukv[i] = srf->u_knots.knots[i];

	vkv = (fastf_t *)rt_calloc( srf->v_knots.k_size, sizeof( fastf_t ), "Assign_surface_to_fu: vkv" );
	for( i=0 ; i<srf->v_knots.k_size ; i++ )
		vkv[i] = srf->v_knots.knots[i];

	mesh = (fastf_t *)rt_calloc( srf->s_size[0]*srf->s_size[1]*ncoords, sizeof( fastf_t ),
		"Assign_surface_to_fu: mesh" );

	for( i=0 ; i<srf->s_size[0]*srf->s_size[1]*ncoords ; i++ )
		mesh[i] = srf->ctl_points[i];

	nmg_face_g_snurb( fu, srf->order[0], srf->order[1], srf->u_knots.k_size, srf->v_knots.k_size,
			ukv, vkv, srf->s_size[0], srf->s_size[1], srf->pt_type, mesh );
}

struct faceuse *
trim_surf( entityno , s )
int entityno;
struct shell *s;
{
	struct snurb *srf;
	struct faceuse *fu;
	struct loopuse *lu;
	struct loopuse *lumate;
	struct loopuse *kill_lu;
	struct nurb_verts *nurb_v;
	struct vertex *verts[3];
	int entity_type;
	int surf_de;
	int nverts;
	int u_order,v_order,n_u_knots,n_v_knots,n_rows,n_cols,pt_type;
	fastf_t *ukv,*vkv,*mesh,u_min,u_max,v_min,v_max;
	int coords;
	int has_outer_boundary,inner_loop_count,outer_loop;
	int *inner_loop;
	int i;
rt_log( "trim_surf( %d )\n", entityno );

	NMG_CK_SHELL( s );

	/* Acquiring Data */
	if( dir[entityno]->param <= pstart )
	{
		rt_log( "Illegal parameter pointer for entity D%07d (%s)\n" ,
				dir[entityno]->direct , dir[entityno]->name );
		return(0);
	}

rt_log( "in trim_surf\n" );
	Readrec( dir[entityno]->param );
	Readint( &entity_type , "" );
	if( entity_type != 144 )
	{
		rt_log( "Expected Trimmed Surface Entity found type %d\n" );
		return( (struct faceuse *)NULL );
	}
	Readint( &surf_de , "" );
	Readint( &has_outer_boundary , "" );
	Readint( &inner_loop_count , "" );
	Readint( &outer_loop , "" );
	if( inner_loop_count )
	{
		inner_loop = (int *)rt_calloc( inner_loop_count , sizeof( int ) , "trim_surf: innerloop" );
		for( i=0 ; i<inner_loop_count ; i++ )
			Readint( &inner_loop[i] , "" );
	}

	if( (srf=Get_nurb_surf( (surf_de-1)/2 )) == (struct snurb *)NULL )
	{
		rt_free( (char *)inner_loop , "trim_surf: inner_loop" );
		return( (struct faceuse *)NULL );
	}
	coords = RT_NURB_EXTRACT_COORDS( srf->pt_type );
rt_log( "In Trim_surf:\n" );
rt_nurb_s_print( " " , srf );

	/* Make a face (with a loop to be destroted later)
	 * because loop routines insist that face and face geometry
	 * must already be assigned
	 */
	for( i=0 ; i<3 ; i++ )
		verts[i] = (struct vertex *)NULL;
	fu = nmg_cface( s, verts, 3 );
	Assign_surface_to_fu( fu, srf );
	kill_lu = RT_LIST_FIRST( loopuse, &fu->lu_hd );

	if( !has_outer_boundary )
	{
rt_log( "\tCalling Make_default_loop\n" );
		lu = Make_default_loop( srf, fu);
	}
	else
	{
rt_log( "\tCalling Make_loop\n" );
		lu = Make_loop( (outer_loop-1)/2, OT_SAME, surf_de, srf, fu );
	}

	(void)nmg_klu( kill_lu );

	for( i=0 ; i<inner_loop_count ; i++ )
		lu = Make_loop( (inner_loop[i]-1)/2, OT_OPPOSITE, surf_de, srf, fu );

	rt_nurb_free_snurb( srf );

	if( inner_loop_count )	
		rt_free( (char *)inner_loop , "trim_surf: inner_loop" );

	return( fu );
}

void
Convtrimsurfs()
{

	int i,convsurf=0,totsurfs=0;
	struct model *m;
	struct nmgregion *r;
	struct shell *s;
	struct faceuse *fu;

	rt_log( "\n\nConverting Trimmed Surface entities:\n" );

rt_g.NMG_debug = 1;

	m = nmg_mm();
	r = nmg_mrsv( m );
	s = RT_LIST_FIRST( shell , &r->s_hd );

	for( i=0 ; i<totentities ; i++ )
	{
		if( dir[i]->type == 144 )
		{
			totsurfs++;
			fu = trim_surf( i , s );
			if( fu )
			{
				nmg_face_bb( fu->f_p , &tol );
				convsurf++;
			}
		}
	}
	rt_log( "Converted %d Trimmed Sufaces successfully out of %d total Trimmed Sufaces\n" , convsurf , totsurfs );

	(void)nmg_model_fuse( m , &tol );

nmg_vmodel( m );
nmg_pr_m( m );

	mk_nmg( fdout , "Trimmed_surf" , m );
	nmg_km( m );
}
