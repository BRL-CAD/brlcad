/*
 *			G _ E X T R U D E . C
 *
 *  Purpose -
 *	Provide support for solids of extrusion
 *
 * Adding a new solid type:
 *	Design disk record
 *
 *	define rt_extrude_internal --- parameters for solid
 *	define extrude_specific --- raytracing form, possibly w/precomuted terms
 *
 *	code import/export/describe/print/ifree/plot/prep/shot/curve/uv/tess
 *
 *	edit db.h add solidrec s_type define
 *	edit rtgeom.h to add rt_extrude_internal
 *	edit table.c:
 *		RT_DECLARE_INTERFACE()
 *		struct rt_functab entry
 *		rt_id_solid()
 *	edit raytrace.h to make ID_EXTRUDE, increment ID_MAXIMUM
 *	edit db_scan.c to add support for new solid type
 *	edit Cakefile to add g_extrude.c to compile
 *
 *	Then:
 *	go to /cad/libwdb and create mk_extrude() routine
 *	go to /cad/mged and create the edit support
 *
 *  Authors -
 *  	John R. Anderson
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 *  
 *  Copyright Notice -
 *	This software is Copyright (C) 1990 by the United States Army.
 *	All rights reserved.
 */
#ifndef lint
static char RCSextrude[] = "@(#)$Header$ (BRL)";
#endif

#include "conf.h"

#include <stdio.h>
#include <math.h>
#include "machine.h"
#include "vmath.h"
#include "db.h"
#include "nmg.h"
#include "raytrace.h"
#include "rtgeom.h"
#include "./debug.h"

RT_EXTERN( struct rt_sketch_internal *copy_sketch, (struct rt_sketch_internal *sketch_ip ) );

struct extrude_specific {
	fastf_t mag_h;
};

/*
 *			R T _ E X T R _ R O T A T E
 *
 *			(copied from rt_tgc_rotate)
 *
 *  To rotate vectors  A  and  B  ( where  A  is perpendicular to  B )
 *  to the X and Y axes respectively, create a rotation matrix
 *
 *	    | A' |
 *	R = | B' |
 *	    | C' |
 *
 *  where  A',  B'  and  C'  are vectors such that
 *
 *	A' = A/|A|	B' = B/|B|	C' = C/|C|
 *
 *  where    C = H - ( H.A' )A' - ( H.B' )B'
 *
 *  The last operation ( Gram Schmidt method ) finds the component
 *  of the vector  H  perpendicular  A  and to  B.  This is, therefore
 *  the normal for the base and top of the extrusion.
 */
static void
rt_extr_rotate( eip, Rot, Inv  )
CONST struct rt_extrude_internal *eip;
mat_t		Rot, Inv;
{
	LOCAL vect_t	uA, uB, uC;	/*  unit vectors		*/
	LOCAL fastf_t	mag_ha,		/*  magnitude of H in the	*/
			mag_hb;		/*    A and B directions	*/
	LOCAL mat_t	tmp_mat1, tmp_mat2;
	LOCAL vect_t	tmp_h;

	/* copy A and B, then 'unitize' the results			*/
	VMOVE( uA, eip->u_vec );
	VUNITIZE( uA );
	VMOVE( uB, eip->v_vec );
	VUNITIZE( uB );

	/*  Find component of H in the A direction			*/
	mag_ha = VDOT( eip->h, uA );
	/*  Find component of H in the B direction			*/
	mag_hb = VDOT( eip->h, uB );

	/*  Subtract the A and B components of H to find the component
	 *  perpendicular to both, then 'unitize' the result.
	 */
	VJOIN2( uC, eip->h, -mag_ha, uA, -mag_hb, uB );
	VUNITIZE( uC );

	bn_mat_idn( Rot );
	bn_mat_idn( Inv );
	bn_mat_idn( tmp_mat1 );
	bn_mat_idn( tmp_mat2 );

	tmp_mat1[0] = uA[X];
	tmp_mat1[1] = uA[Y];
	tmp_mat1[2] = uA[Z];

	tmp_mat1[4] = uB[X];
	tmp_mat1[5] = uB[Y];
	tmp_mat1[6] = uB[Z];

	tmp_mat1[8]  = uC[X];
	tmp_mat1[9]  = uC[Y];
	tmp_mat1[10] = uC[Z];

	MAT_DELTAS_VEC_NEG( Rot, eip->V )
	bn_bn_mat_mul2( tmp_mat1, Rot );

	MAT4X3VEC( tmp_h, Rot, eip->h );

	tmp_mat2[2] = tmp_h[X]/tmp_h[Z];
	tmp_mat2[6] = tmp_h[Y]/tmp_h[Z];

	bn_bn_mat_mul2( tmp_mat2, Rot );

}

/*
 *  			R T _ E X T R U D E _ P R E P
 *  
 *  Given a pointer to a GED database record, and a transformation matrix,
 *  determine if this is a valid EXTRUDE, and if so, precompute various
 *  terms of the formula.
 *  
 *  Returns -
 *  	0	EXTRUDE is OK
 *  	!0	Error in description
 *  
 *  Implicit return -
 *  	A struct extrude_specific is created, and it's address is stored in
 *  	stp->st_specific for use by extrude_shot().
 */
int
rt_extrude_prep( stp, ip, rtip )
struct soltab		*stp;
struct rt_db_internal	*ip;
struct rt_i		*rtip;
{
	struct rt_extrude_internal *eip;
	register struct extrude_specific *extr;
	LOCAL mat_t	Rot, iRot;
	LOCAL point_t	tmp_pt;
	
	eip = (struct rt_extrude_internal *)ip->idb_ptr;
	RT_EXTRUDE_CK_MAGIC( eip );

	BU_GETSTRUCT( extr, extrude_specific );
	stp->st_specific = (genptr_t)extr;

	rt_extr_rotate( eip, Rot, iRot );
	bn_mat_print( "extrude rotation", Rot );

	MAT4X3PNT( tmp_pt, Rot, eip->V );
	VPRINT( "original Vertex:", eip->V );
	VPRINT( "rotated vertex:", tmp_pt );
	MAT4X3VEC( tmp_pt, Rot, eip->h );
	VPRINT( "orinal height:", eip->h );
	VPRINT( "rotated height:", tmp_pt );
	MAT4X3VEC( tmp_pt, Rot, eip->u_vec );
	VPRINT( "rotated u_vec:", tmp_pt );
	MAT4X3VEC( tmp_pt, Rot, eip->v_vec );
	VPRINT( "rotated v_vec:", tmp_pt );

	return(0);              /* OK */
}

/*
 *			R T _ E X T R U D E _ P R I N T
 */
void
rt_extrude_print( stp )
register CONST struct soltab *stp;
{
	register CONST struct extrude_specific *extrude =
		(struct extrude_specific *)stp->st_specific;
}

/*
 *  			R T _ E X T R U D E _ S H O T
 *  
 *  Intersect a ray with a extrude.
 *  If an intersection occurs, a struct seg will be acquired
 *  and filled in.
 *  
 *  Returns -
 *  	0	MISS
 *	>0	HIT
 */
int
rt_extrude_shot( stp, rp, ap, seghead )
struct soltab		*stp;
register struct xray	*rp;
struct application	*ap;
struct seg		*seghead;
{
	return(0);			/* MISS */
}

#define RT_EXTRUDE_SEG_MISS(SEG)	(SEG).seg_stp=RT_SOLTAB_NULL

/*
 *			R T _ E X T R U D E _ V S H O T
 *
 *  Vectorized version.
 */
void
rt_extrude_vshot( stp, rp, segp, n, ap )
struct soltab	       *stp[]; /* An array of solid pointers */
struct xray		*rp[]; /* An array of ray pointers */
struct  seg            segp[]; /* array of segs (results returned) */
int		  	    n; /* Number of ray/object pairs */
struct application	*ap;
{
	rt_vstub( stp, rp, segp, n, ap );
}

/*
 *  			R T _ E X T R U D E _ N O R M
 *  
 *  Given ONE ray distance, return the normal and entry/exit point.
 */
void
rt_extrude_norm( hitp, stp, rp )
register struct hit	*hitp;
struct soltab		*stp;
register struct xray	*rp;
{
	register struct extrude_specific *extrude =
		(struct extrude_specific *)stp->st_specific;

	VJOIN1( hitp->hit_point, rp->r_pt, hitp->hit_dist, rp->r_dir );
}

/*
 *			R T _ E X T R U D E _ C U R V E
 *
 *  Return the curvature of the extrude.
 */
void
rt_extrude_curve( cvp, hitp, stp )
register struct curvature *cvp;
register struct hit	*hitp;
struct soltab		*stp;
{
	register struct extrude_specific *extrude =
		(struct extrude_specific *)stp->st_specific;

 	cvp->crv_c1 = cvp->crv_c2 = 0;

	/* any tangent direction */
 	vec_ortho( cvp->crv_pdir, hitp->hit_normal );
}

/*
 *  			R T _ E X T R U D E _ U V
 *  
 *  For a hit on the surface of an extrude, return the (u,v) coordinates
 *  of the hit point, 0 <= u,v <= 1.
 *  u = azimuth
 *  v = elevation
 */
void
rt_extrude_uv( ap, stp, hitp, uvp )
struct application	*ap;
struct soltab		*stp;
register struct hit	*hitp;
register struct uvcoord	*uvp;
{
	register struct extrude_specific *extrude =
		(struct extrude_specific *)stp->st_specific;
}

/*
 *		R T _ E X T R U D E _ F R E E
 */
void
rt_extrude_free( stp )
register struct soltab *stp;
{
	register struct extrude_specific *extrude =
		(struct extrude_specific *)stp->st_specific;

	bu_free( (char *)extrude, "extrude_specific" );
}

/*
 *			R T _ E X T R U D E _ C L A S S
 */
int
rt_extrude_class()
{
	return(0);
}

/*
 *			R T _ E X T R U D E _ P L O T
 */
int
rt_extrude_plot( vhead, ip, ttol, tol )
struct bu_list		*vhead;
struct rt_db_internal	*ip;
CONST struct rt_tess_tol *ttol;
struct bn_tol		*tol;
{
	LOCAL struct rt_extrude_internal	*extrude_ip;
	struct curve			*crv=(struct curve *)NULL;
	int				curve_no;
	struct directory		*dp;
	struct rt_sketch_internal	*sketch_ip;
	long				*lng;
	int				seg_no;
	struct line_seg			*lsg;
	int				first=1;
	int				start, end, prev=-1;
	point_t				pt;
	point_t				end_of_h;

	RT_CK_DB_INTERNAL(ip);
	extrude_ip = (struct rt_extrude_internal *)ip->idb_ptr;
	RT_EXTRUDE_CK_MAGIC(extrude_ip);

	sketch_ip = extrude_ip->skt;
	RT_SKETCH_CK_MAGIC( sketch_ip );

	/* find the curve for this extrusion */
	for( curve_no=0 ; curve_no < sketch_ip->curve_count ; curve_no++ )
	{
		if( !strncmp( extrude_ip->curve_name, sketch_ip->curves[curve_no].crv_name, SKETCH_NAME_LEN ) )
		{
			crv = &sketch_ip->curves[curve_no];
			break;
		}
	}

	if( !crv )
	{
		bu_log( "rt_extrude_plot: ERROR: cannot find curve named '%.16s' in sketch named '%.16s'\n",
			extrude_ip->curve_name, extrude_ip->sketch_name );
		return( -1 );
	}

	/* plot bottom curve */
	for( seg_no=0 ; seg_no < crv->seg_count ; seg_no++ )
	{
		lng = (long *)crv->segments[seg_no];
		switch( *lng )
		{
			case CURVE_LSEG_MAGIC:
				lsg = (struct line_seg *)lng;
				if( crv->reverse[seg_no] )
				{
					end = lsg->start;
					start = lsg->end;
				}
				else
				{
					start = lsg->start;
					end = lsg->end;
				}
				if( first || start != prev )
				{
					VJOIN2( pt, extrude_ip->V, sketch_ip->verts[start][0], extrude_ip->u_vec, sketch_ip->verts[start][1], extrude_ip->v_vec);
					RT_ADD_VLIST( vhead, pt, BN_VLIST_LINE_MOVE )
				}
				VJOIN2( pt, extrude_ip->V, sketch_ip->verts[end][0], extrude_ip->u_vec, sketch_ip->verts[end][1], extrude_ip->v_vec);
				RT_ADD_VLIST( vhead, pt, BN_VLIST_LINE_DRAW );
				prev = end;
				break;
			default:
			{
				bu_log( "rt_extrude_plot: ERROR: unrecognized segment type!!!!\n" );
				return( -1 );
			}
		}
		first = 0;
	}

	/* plot top curve */
	first = 1;
	VADD2( end_of_h, extrude_ip->V, extrude_ip->h );
	for( seg_no=0 ; seg_no < crv->seg_count ; seg_no++ )
	{
		lng = (long *)crv->segments[seg_no];
		switch( *lng )
		{
			case CURVE_LSEG_MAGIC:
				lsg = (struct line_seg *)lng;
				if( crv->reverse[seg_no] )
				{
					end = lsg->start;
					start = lsg->end;
				}
				else
				{
					start = lsg->start;
					end = lsg->end;
				}
				if( first || start != prev )
				{
					VJOIN2( pt, end_of_h, sketch_ip->verts[start][0], extrude_ip->u_vec, sketch_ip->verts[start][1], extrude_ip->v_vec);
					RT_ADD_VLIST( vhead, pt, BN_VLIST_LINE_MOVE )
				}
				VJOIN2( pt, end_of_h, sketch_ip->verts[end][0], extrude_ip->u_vec, sketch_ip->verts[end][1], extrude_ip->v_vec);
				RT_ADD_VLIST( vhead, pt, BN_VLIST_LINE_DRAW );
				prev = end;
				break;
			default:
			{
				bu_log( "rt_extrude_plot: ERROR: unrecognized segment type!!!!\n" );
				return( -1 );
			}
		}
		first = 0;
	}

	/* plot connecting lines */
	for( seg_no=0 ; seg_no < crv->seg_count ; seg_no++ )
	{
		point_t pt2;

		lng = (long *)crv->segments[seg_no];
		switch( *lng )
		{
			case CURVE_LSEG_MAGIC:
				lsg = (struct line_seg *)lng;
				if( crv->reverse[seg_no] )
					end = lsg->start;
				else
					end = lsg->end;
				VJOIN2( pt, extrude_ip->V, sketch_ip->verts[end][0], extrude_ip->u_vec, sketch_ip->verts[end][1], extrude_ip->v_vec);
				VJOIN2( pt2, end_of_h, sketch_ip->verts[end][0], extrude_ip->u_vec, sketch_ip->verts[end][1], extrude_ip->v_vec);
				RT_ADD_VLIST( vhead, pt, BN_VLIST_LINE_MOVE )
				RT_ADD_VLIST( vhead, pt2, BN_VLIST_LINE_DRAW );
				prev = end;
				break;
			default:
			{
				bu_log( "rt_extrude_plot: ERROR: unrecognized segment type!!!!\n" );
				return( -1 );
			}
		}
		first = 0;
	}

	return(0);
}

/*
 *			R T _ E X T R U D E _ T E S S
 *
 *  Returns -
 *	-1	failure
 *	 0	OK.  *r points to nmgregion that holds this tessellation.
 */
int
rt_extrude_tess( r, m, ip, ttol, tol )
struct nmgregion	**r;
struct model		*m;
struct rt_db_internal	*ip;
CONST struct rt_tess_tol *ttol;
struct bn_tol		*tol;
{
	return(-1);
}

/*
 *			R T _ E X T R U D E _ I M P O R T
 *
 *  Import an EXTRUDE from the database format to the internal format.
 *  Apply modeling transformations as well.
 */
int
rt_extrude_import( ip, ep, mat, dbip )
struct rt_db_internal		*ip;
CONST struct bu_external	*ep;
register CONST mat_t		mat;
struct db_i		*dbip;
{
	LOCAL struct rt_extrude_internal	*extrude_ip;
	struct rt_sketch_internal		*sketch_ip;
	struct rt_db_internal			tmp_ip;
	struct directory			*dp;
	char					*sketch_name;
	union record				*rp;
	char					*ptr;
	point_t					tmp_vec;

	BU_CK_EXTERNAL( ep );
	rp = (union record *)ep->ext_buf;
	/* Check record type */
	if( rp->u_id != DBID_EXTR )  {
		bu_log("rt_extrude_import: defective record\n");
		return(-1);
	}

	sketch_name = (char *)rp + sizeof( struct extr_rec );
	if( (dp=db_lookup( dbip, sketch_name, LOOKUP_NOISY)) == DIR_NULL )
	{
		bu_log( "rt_extrude_import: ERROR: Cannot find sketch (%.16s) for extrusion (%.16s)\n",
			sketch_name, rp->extr.ex_name );
		return( -1 );
	}

	if( rt_db_get_internal( &tmp_ip, dp, dbip, bn_mat_identity ) != ID_SKETCH )
	{
		bu_log( "rt_extrude_import: ERROR: Cannot import sketch (%.16s) for extrusion (%.16s)\n",
			sketch_name, rp->extr.ex_name );
		return( -1 );
	}

	RT_INIT_DB_INTERNAL( ip );
	ip->idb_type = ID_EXTRUDE;
	ip->idb_ptr = bu_malloc( sizeof(struct rt_extrude_internal), "rt_extrude_internal");
	extrude_ip = (struct rt_extrude_internal *)ip->idb_ptr;
	extrude_ip->magic = RT_EXTRUDE_INTERNAL_MAGIC;
	extrude_ip->skt = (struct rt_sketch_internal *)tmp_ip.idb_ptr;

	ntohd( (unsigned char *)tmp_vec, rp->extr.ex_V, 3 );
	MAT4X3PNT( extrude_ip->V, mat, tmp_vec );
	ntohd( (unsigned char *)tmp_vec, rp->extr.ex_h, 3 );
	MAT4X3VEC( extrude_ip->h, mat, tmp_vec );
	ntohd( (unsigned char *)tmp_vec, rp->extr.ex_uvec, 3 );
	MAT4X3VEC( extrude_ip->u_vec, mat, tmp_vec );
	ntohd( (unsigned char *)tmp_vec, rp->extr.ex_vvec, 3 );
	MAT4X3VEC( extrude_ip->v_vec, mat, tmp_vec );
	extrude_ip->keypoint = bu_glong( rp->extr.ex_key );

	ptr = (char *)rp;
	ptr += sizeof( struct extr_rec );
	strncpy( extrude_ip->sketch_name, ptr, 16 );
	ptr += 16;
	strncpy( extrude_ip->curve_name, ptr, 16 );

	return(0);			/* OK */
}

/*
 *			R T _ E X T R U D E _ E X P O R T
 *
 *  The name is added by the caller, in the usual place.
 */
int
rt_extrude_export( ep, ip, local2mm, dbip )
struct bu_external		*ep;
CONST struct rt_db_internal	*ip;
double				local2mm;
CONST struct db_i		*dbip;
{
	struct rt_extrude_internal	*extrude_ip;
	vect_t				tmp_vec;
	union record			*rec;
	unsigned char			*ptr;

	RT_CK_DB_INTERNAL(ip);
	if( ip->idb_type != ID_EXTRUDE )  return(-1);
	extrude_ip = (struct rt_extrude_internal *)ip->idb_ptr;
	RT_EXTRUDE_CK_MAGIC(extrude_ip);

	BU_INIT_EXTERNAL(ep);
	ep->ext_nbytes = 2*sizeof( union record );
	ep->ext_buf = (genptr_t)bu_calloc( 1, ep->ext_nbytes, "extrusion external");
	rec = (union record *)ep->ext_buf;

	rec->extr.ex_id = DBID_EXTR;

	VSCALE( tmp_vec, extrude_ip->V, local2mm );
	htond( rec->extr.ex_V, (unsigned char *)tmp_vec, 3 );
	VSCALE( tmp_vec, extrude_ip->h, local2mm );
	htond( rec->extr.ex_h, (unsigned char *)tmp_vec, 3 );
	VSCALE( tmp_vec, extrude_ip->u_vec, local2mm );
	htond( rec->extr.ex_uvec, (unsigned char *)tmp_vec, 3 );
	VSCALE( tmp_vec, extrude_ip->v_vec, local2mm );
	htond( rec->extr.ex_vvec, (unsigned char *)tmp_vec, 3 );
	bu_plong( rec->extr.ex_key, extrude_ip->keypoint );
	bu_plong( rec->extr.ex_count, 1 );

	ptr = (unsigned char *)rec;
	ptr += sizeof( struct extr_rec );

	strncpy( (char *)ptr, extrude_ip->sketch_name, 16 );
	ptr += 16;
	strncpy( (char *)ptr, extrude_ip->curve_name, 16 );

	return(0);
}

/*
 *			R T _ E X T R U D E _ D E S C R I B E
 *
 *  Make human-readable formatted presentation of this solid.
 *  First line describes type of solid.
 *  Additional lines are indented one tab, and give parameter values.
 */
int
rt_extrude_describe( str, ip, verbose, mm2local )
struct bu_vls		*str;
struct rt_db_internal	*ip;
int			verbose;
double			mm2local;
{
	register struct rt_extrude_internal	*extrude_ip =
		(struct rt_extrude_internal *)ip->idb_ptr;
	int curve_no;
	int seg_no;
	char	buf[256];

	RT_EXTRUDE_CK_MAGIC(extrude_ip);
	bu_vls_strcat( str, "2D extrude (EXTRUDE)\n");
	sprintf( buf, "\tV = (%g %g %g)\n\tH = (%g %g %g)\n\tu_dir = (%g %g %g)\n\tv_dir = (%g %g %g)\n",
		V3ARGS( extrude_ip->V ),
		V3ARGS( extrude_ip->h ),
		V3ARGS( extrude_ip->u_vec ),
		V3ARGS( extrude_ip->v_vec ) );
	bu_vls_strcat( str, buf );
	sprintf( buf, "\tsketch name: %.16s\n\tcurve name: %.16s\n",
		extrude_ip->sketch_name,
		extrude_ip->curve_name );
	bu_vls_strcat( str, buf );
	

	return(0);
}

/*
 *			R T _ E X T R U D E _ I F R E E
 *
 *  Free the storage associated with the rt_db_internal version of this solid.
 */
void
rt_extrude_ifree( ip )
struct rt_db_internal	*ip;
{
	register struct rt_extrude_internal	*extrude_ip;
	struct rt_db_internal			tmp_ip;
	struct curve				*crv;
	int					curve_no;
	genptr_t				*seg_ptr;
	int					seg_no;

	RT_CK_DB_INTERNAL(ip);
	extrude_ip = (struct rt_extrude_internal *)ip->idb_ptr;
	RT_EXTRUDE_CK_MAGIC(extrude_ip);
	if( extrude_ip->skt )
	{
		RT_INIT_DB_INTERNAL( &tmp_ip );
		tmp_ip.idb_type = ID_SKETCH;
		tmp_ip.idb_ptr = (genptr_t)extrude_ip->skt;
		rt_sketch_ifree( &tmp_ip );
	}
	extrude_ip->magic = 0;			/* sanity */

	bu_free( (char *)extrude_ip, "extrude ifree" );
	ip->idb_ptr = GENPTR_NULL;	/* sanity */
}

int
rt_extrude_xform( op, mat, ip, free )
struct rt_db_internal *op;
CONST mat_t mat;
struct rt_db_internal *ip;
int free;
{
	struct rt_extrude_internal	*eip, *eop;
	point_t tmp_vec;

	RT_CK_DB_INTERNAL( ip );
	eip = (struct rt_extrude_internal *)ip->idb_ptr;
	RT_EXTRUDE_CK_MAGIC( eip );

	if( bu_debug&BU_DEBUG_MEM_CHECK )
	{
		bu_log( "Barrier check at start of extrude_xform():\n" );
		bu_mem_barriercheck();
	}

	if( op != ip )
	{
		RT_INIT_DB_INTERNAL( op );
		eop = (struct rt_extrude_internal *)bu_malloc( sizeof( struct rt_extrude_internal ), "eop" );
		eop->magic = RT_EXTRUDE_INTERNAL_MAGIC;
		op->idb_ptr = (genptr_t)eop;
	}
	else
		eop = (struct rt_extrude_internal *)ip->idb_ptr;

	MAT4X3PNT( tmp_vec, mat, eip->V );
	VMOVE( eop->V, tmp_vec );
	MAT4X3VEC( tmp_vec, mat, eip->h );
	VMOVE( eop->h, tmp_vec );
	MAT4X3VEC( tmp_vec, mat, eip->u_vec );
	VMOVE( eop->u_vec, tmp_vec );
	MAT4X3VEC( tmp_vec, mat, eip->v_vec );
	VMOVE( eop->v_vec, tmp_vec );
	eop->keypoint = eip->keypoint;
	strncpy( eop->sketch_name, eip->sketch_name, 16 );
	strncpy( eop->curve_name, eip->curve_name, 16 );

	if( free && ip != op )
	{
		eop->skt = eip->skt;
		eip->skt = (struct rt_sketch_internal *)NULL;
		rt_functab[ip->idb_type].ft_ifree( ip );
		ip->idb_ptr = (genptr_t) 0;
	}
	else
		eop->skt = copy_sketch( eip->skt );

	if( bu_debug&BU_DEBUG_MEM_CHECK )
	{
		bu_log( "Barrier check at end of extrude_xform():\n" );
		bu_mem_barriercheck();
	}

	return( 0 );		
}
