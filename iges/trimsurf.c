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

#include "./iges_struct.h"
#include "./iges_extern.h"
#include "../librt/debug.h"

/* translations to get knot vectors in first quadrant */
static fastf_t u_translation=0.0;
static fastf_t v_translation=0.0;

#define CTL_INDEX(_i,_j)	((_i * n_cols + _j) * ncoords)

struct face_g_snurb *
Get_nurb_surf( entityno, m )
int entityno;
struct model *m;
{
	struct face_g_snurb *srf;
	point_t pt;
	int entity_type;
	int i,j;
	int num_knots;
	int num_pts;
	int rational;
	int ncoords;
	int n_rows,n_cols,u_order,v_order;
	int n_u,n_v;
	int pt_type;
	fastf_t u_min,u_max;
	fastf_t v_min,v_max;
	double a;

	/* Acquiring Data */

	if( dir[entityno]->param <= pstart )
	{
		rt_log( "Illegal parameter pointer for entity D%07d (%s)\n" ,
				dir[entityno]->direct , dir[entityno]->name );
		return( (struct face_g_snurb *)NULL );
	}

	Readrec( dir[entityno]->param );
	Readint( &entity_type , "" );
	if( entity_type != 128 )
	{
		rt_log( "Only B-Spline surfaces allowed for faces (found type %d)\n", entity_type );
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
		srf = rt_nurb_new_snurb( u_order, v_order, n_u, n_v, n_rows, n_cols, pt_type );
	else
	{
		int pnum;

		GET_FACE_G_SNURB( srf, m );
		RT_LIST_INIT( &srf->l );
		RT_LIST_INIT( &srf->f_hd );
		srf->l.magic = NMG_FACE_G_SNURB_MAGIC;
		srf->order[0] = u_order;
		srf->order[1] = v_order;
		srf->dir = RT_NURB_SPLIT_ROW;
		srf->u.magic = NMG_KNOT_VECTOR_MAGIC;
		srf->v.magic = NMG_KNOT_VECTOR_MAGIC;
		srf->u.k_size = n_u;
		srf->v.k_size = n_v;
		srf->u.knots = (fastf_t *) rt_malloc ( 
			n_u * sizeof (fastf_t ), "Get_nurb_surf: u kv knot values");
		srf->v.knots = (fastf_t *) rt_malloc ( 
			n_v * sizeof (fastf_t ), "Get_nurb_surf: v kv knot values");

		srf->s_size[0] = n_rows;
		srf->s_size[1] = n_cols;
		srf->pt_type = pt_type;

		pnum = sizeof (fastf_t) * n_rows * n_cols * RT_NURB_EXTRACT_COORDS(pt_type);
		srf->ctl_points = ( fastf_t *) rt_malloc( 
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
struct edge_g_cnurb *crv;
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

	knots = (fastf_t *)rt_calloc( crv->k.k_size, sizeof( fastf_t ),
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
	int i,j;
	int planar;
	int rational;
	int pt_type;
	int ncoords;
	double a;
	double x,y,z;

	if( dir[entity_no]->param <= pstart )
	{
		rt_log( "Get_cnurb: Illegal parameter pointer for entity D%07d (%s)\n" ,
				dir[entity_no]->direct , dir[entity_no]->name );
		return( (struct edge_g_cnurb *)NULL );
	}

	Readrec( dir[entity_no]->param );
	Readint( &entity_type , "" );

	if( entity_type != 126 )
	{
		rt_log( "Get_cnurb: Was expecting spline curve, got type %d\n" , entity_type );
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

	NMG_CK_VERTEXUSE( vu );
	NMG_CK_SNURB( srf );

	VSETALLN( pt_on_srf, 0.0, 4 )

	rt_nurb_s_eval( srf, u, v, pt_on_srf );
	if( RT_NURB_IS_PT_RATIONAL( srf->pt_type ) )
	{
		fastf_t scale;

		scale = 1.0/pt_on_srf[3];
		VSCALE( pt_on_srf, pt_on_srf, scale );
	}

	nmg_vertex_gv( vu->v_p, pt_on_srf );
	/* XXXXX Need w coord!!!!! */
	VSET( uvw, u, v, 1.0 );

	for( RT_LIST_FOR( vu1, vertexuse, &vu->v_p->vu_hd ) )
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
	fastf_t u,v;
	point_t pt, pt2;
	int ncoords;
	int i;

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

				Readint( &curve_count , "" );
				curve_list = (int *)rt_calloc( curve_count , sizeof( int ),
						"Add_trim_curve: curve_list" );

				for( i=0 ; i<curve_count ; i++ )
					Readint( &curve_list[i] , "" );

				for( i=0 ; i<curve_count ; i++ )
					Add_trim_curve( (curve_list[i]-1)/2, lu, srf );

				rt_free( (char *)curve_list , "Add_trim_curve: curve_list" );
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
			eu = RT_LIST_LAST( edgeuse, &lu->down_hd );
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
				eu = RT_LIST_LAST( edgeuse, &lu->down_hd );
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
			rt_log( "Curves of type %d are not yet handled for trimmed surfaces\n", entity_type );
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
	struct edge_g_cnurb *crv;
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

				Readint( &curve_count , "" );
				curve_list = (int *)rt_calloc( curve_count , sizeof( int ),
						"Make_trim_loop: curve_list" );

				for( i=0 ; i<curve_count ; i++ )
					Readint( &curve_list[i] , "" );

				for( i=0 ; i<curve_count ; i++ )
					Add_trim_curve( (curve_list[i]-1)/2, lu, srf );

				rt_free( (char *)curve_list , "Make_trim_loop: curve_list" );
				eu = RT_LIST_LAST( edgeuse, &lu->down_hd );
				nmg_keu( eu );
			}
			break;
		case 100:	/* circular arc (must be full cirle here) */
			{
				struct edge_g_cnurb *crv;
				struct edge_g_cnurb *crv1,*crv2;
				point_t center, start, end;
				struct rt_list curv_hd;
rt_log( "Full circle\n" );

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
				RT_LIST_INIT( &curv_hd );
				rt_nurb_c_split( &curv_hd, crv );
				crv1 = RT_LIST_FIRST( edge_g_cnurb, &curv_hd );
				crv2 = RT_LIST_LAST( edge_g_cnurb, &curv_hd );

				/* Split last edge in loop */
				eu = RT_LIST_LAST( edgeuse, &lu->down_hd );
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
			rt_log( "Conic Arc not yet handled as a trimming curve\n" );
			break;

		case 126:	/* spline curve (must be closed loop) */
			{
				struct edge_g_cnurb *crv;
				struct edge_g_cnurb *crv1,*crv2;
				struct rt_list curv_hd;

				/* Get spline curve */
rt_log( "CLosed loop spline curve\n" );
				crv = Get_cnurb( entity_no );

				ncoords = RT_NURB_EXTRACT_COORDS( crv->pt_type );

				/* split circle into two pieces */
				RT_LIST_INIT( &curv_hd );
				rt_nurb_c_split( &curv_hd, crv );
				crv1 = RT_LIST_FIRST( edge_g_cnurb, &curv_hd );
				crv2 = RT_LIST_LAST( edge_g_cnurb, &curv_hd );
				/* Split last edge in loop */
				eu = RT_LIST_LAST( edgeuse, &lu->down_hd );
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
			rt_log( "Curves of type %d are not yet handled for trimmed surfaces\n", entity_type );
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
		rt_log( "Illegal parameter pointer for entity D%07d (%s)\n" ,
				dir[entity_no]->direct , dir[entity_no]->name );
		return(0);
	}

	Readrec( dir[entity_no]->param );
	Readint( &entity_type , "" );
	if( entity_type != 142 )
	{
		rt_log( "Expected Curve on a Parametric Surface, found %s\n" , iges_type( entity_type ) );
		return( 0 );
	}
	Readint( &i , "" );
	Readint( &surf_de , "" );
	if( surf_de != on_surf_de )
		rt_log( "Curve is on surface at DE %d, should be on surface at DE %d\n", surf_de, on_surf_de );

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

	if( srf->order[0] < 3 && srf->order[1] < 3 )
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
	RT_LIST_APPEND( &srf->f_hd, &f->l );
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
	struct loopuse *lumate;
	struct loopuse *kill_lu;
	struct nurb_verts *nurb_v;
	struct vertex *verts[3];
	int entity_type;
	int surf_de;
	int nverts;
	int u_order,v_order,n_u,n_v,n_rows,n_cols,pt_type;
	fastf_t *ukv,*vkv,*mesh,u_min,u_max,v_min,v_max;
	int coords;
	int has_outer_boundary,inner_loop_count,outer_loop;
	int *inner_loop;
	int i;
	int lu_uv_orient;

	NMG_CK_SHELL( s );

	m = nmg_find_model( &s->l.magic );
	NMG_CK_MODEL( m );

	/* Acquiring Data */
	if( dir[entityno]->param <= pstart )
	{
		rt_log( "Illegal parameter pointer for entity D%07d (%s)\n" ,
				dir[entityno]->direct , dir[entityno]->name );
		return(0);
	}

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

	if( (srf=Get_nurb_surf( (surf_de-1)/2, m )) == (struct face_g_snurb *)NULL )
	{
		rt_free( (char *)inner_loop , "trim_surf: inner_loop" );
		return( (struct faceuse *)NULL );
	}
	coords = RT_NURB_EXTRACT_COORDS( srf->pt_type );

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
		lu = Make_default_loop( srf, fu);
	else
		lu = Make_loop( (outer_loop-1)/2, OT_SAME, surf_de, srf, fu );

	(void)nmg_klu( kill_lu );

	/* first loop is an outer loop, orientation must be OT_SAME */
	lu_uv_orient = nmg_snurb_calc_lu_uv_orient( lu );
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
		RT_LIST_DEQUEUE( &lu->l );
		RT_LIST_DEQUEUE( &lu->lumate_p->l );
		RT_LIST_APPEND( &fu->lu_hd , &lu->lumate_p->l );
		lu->lumate_p->up.fu_p = fu;
		RT_LIST_APPEND( &fu->fumate_p->lu_hd , &lu->l );
		lu->up.fu_p = fu->fumate_p;
	}

	if( inner_loop_count )	
		rt_free( (char *)inner_loop , "trim_surf: inner_loop" );

	NMG_CK_FACE_G_SNURB( fu->f_p->g.snurb_p );

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

	if( rt_g.debug & DEBUG_MEM_FULL )
		rt_mem_barriercheck();

	m = nmg_mm();
	r = nmg_mrsv( m );
	s = RT_LIST_FIRST( shell , &r->s_hd );

	for( i=0 ; i<totentities ; i++ )
	{
		if( dir[i]->type == 144 )
		{
			if( rt_g.debug & DEBUG_MEM_FULL )
				rt_mem_barriercheck();

			totsurfs++;
			fu = trim_surf( i , s );
			if( fu )
			{
				nmg_face_bb( fu->f_p , &tol );
				convsurf++;
			}
			if( rt_g.debug & DEBUG_MEM_FULL )
				rt_mem_barriercheck();

		}
	}

	rt_log( "Converted %d Trimmed Sufaces successfully out of %d total Trimmed Sufaces\n" , convsurf , totsurfs );

	if( rt_g.debug & DEBUG_MEM_FULL )
		rt_mem_barriercheck();

	if( convsurf )
	{
		(void)nmg_model_vertex_fuse( m, &tol );

		if( curr_file->obj_name )
			mk_nmg( fdout , curr_file->obj_name , m );
		else
			mk_nmg( fdout , "Trimmed_surf" , m );
	}
	if( rt_g.debug & DEBUG_MEM_FULL )
		rt_mem_barriercheck();


	nmg_km( m );

	if( rt_g.debug & DEBUG_MEM_FULL )
		rt_mem_barriercheck();

}
