/*
 *			G _ S K E T C H . C
 *
 *  Purpose -
 *	Provide support for 2D sketches
 *
 * Adding a new solid type:
 *	Design disk record
 *
 *	define rt_sketch_internal --- parameters for solid
 *	define sketch_specific --- raytracing form, possibly w/precomuted terms
 *
 *	code import/export/describe/print/ifree/plot/prep/shot/curve/uv/tess
 *
 *	edit db.h add solidrec s_type define
 *	edit rtgeom.h to add rt_sketch_internal
 *	edit table.c:
 *		RT_DECLARE_INTERFACE()
 *		struct rt_functab entry
 *		rt_id_solid()
 *	edit raytrace.h to make ID_SKETCH, increment ID_MAXIMUM
 *	edit db_scan.c to add support for new solid type
 *	edit Cakefile to add g_sketch.c to compile
 *
 *	Then:
 *	go to /cad/libwdb and create mk_sketch() routine
 *	go to /cad/mged and create the edit support
 *
 *  Authors -
 *  
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
static char RCSsketch[] = "@(#)$Header$ (BRL)";
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

/*
 *  			R T _ S K E T C H _ P R E P
 *  
 *  Given a pointer to a GED database record, and a transformation matrix,
 *  determine if this is a valid SKETCH, and if so, precompute various
 *  terms of the formula.
 *  
 *  Returns -
 *  	0	SKETCH is OK
 *  	!0	Error in description
 *  
 *  Implicit return -
 *  	A struct sketch_specific is created, and it's address is stored in
 *  	stp->st_specific for use by sketch_shot().
 */
int
rt_sketch_prep( stp, ip, rtip )
struct soltab		*stp;
struct rt_db_internal	*ip;
struct rt_i		*rtip;
{
	stp->st_specific = (genptr_t)NULL;
	return( 0 );
}

/*
 *			R T _ S K E T C H _ P R I N T
 */
void
rt_sketch_print( stp )
register CONST struct soltab *stp;
{
	register CONST struct sketch_specific *sketch =
		(struct sketch_specific *)stp->st_specific;
}

/*
 *  			R T _ S K E T C H _ S H O T
 *  
 *  Intersect a ray with a sketch.
 *  If an intersection occurs, a struct seg will be acquired
 *  and filled in.
 *  
 *  Returns -
 *  	0	MISS
 *	>0	HIT
 */
int
rt_sketch_shot( stp, rp, ap, seghead )
struct soltab		*stp;
register struct xray	*rp;
struct application	*ap;
struct seg		*seghead;
{
	return(0);			/* MISS */
}

#define RT_SKETCH_SEG_MISS(SEG)	(SEG).seg_stp=RT_SOLTAB_NULL

/*
 *			R T _ S K E T C H _ V S H O T
 *
 *  Vectorized version.
 */
void
rt_sketch_vshot( stp, rp, segp, n, ap )
struct soltab	       *stp[]; /* An array of solid pointers */
struct xray		*rp[]; /* An array of ray pointers */
struct  seg            segp[]; /* array of segs (results returned) */
int		  	    n; /* Number of ray/object pairs */
struct application	*ap;
{
	rt_vstub( stp, rp, segp, n, ap );
}

/*
 *  			R T _ S K E T C H _ N O R M
 *  
 *  Given ONE ray distance, return the normal and entry/exit point.
 */
void
rt_sketch_norm( hitp, stp, rp )
register struct hit	*hitp;
struct soltab		*stp;
register struct xray	*rp;
{
	register struct sketch_specific *sketch =
		(struct sketch_specific *)stp->st_specific;

	VJOIN1( hitp->hit_point, rp->r_pt, hitp->hit_dist, rp->r_dir );
}

/*
 *			R T _ S K E T C H _ C U R V E
 *
 *  Return the curvature of the sketch.
 */
void
rt_sketch_curve( cvp, hitp, stp )
register struct curvature *cvp;
register struct hit	*hitp;
struct soltab		*stp;
{
	register struct sketch_specific *sketch =
		(struct sketch_specific *)stp->st_specific;

 	cvp->crv_c1 = cvp->crv_c2 = 0;

	/* any tangent direction */
 	vec_ortho( cvp->crv_pdir, hitp->hit_normal );
}

/*
 *  			R T _ S K E T C H _ U V
 *  
 *  For a hit on the surface of an sketch, return the (u,v) coordinates
 *  of the hit point, 0 <= u,v <= 1.
 *  u = azimuth
 *  v = elevation
 */
void
rt_sketch_uv( ap, stp, hitp, uvp )
struct application	*ap;
struct soltab		*stp;
register struct hit	*hitp;
register struct uvcoord	*uvp;
{
	register struct sketch_specific *sketch =
		(struct sketch_specific *)stp->st_specific;
}

/*
 *		R T _ S K E T C H _ F R E E
 */
void
rt_sketch_free( stp )
register struct soltab *stp;
{
	register struct sketch_specific *sketch =
		(struct sketch_specific *)stp->st_specific;

	bu_free( (char *)sketch, "sketch_specific" );
}

/*
 *			R T _ S K E T C H _ C L A S S
 */
int
rt_sketch_class()
{
	return(0);
}

/*
 *			R T _ S K E T C H _ P L O T
 */
int
rt_sketch_plot( vhead, ip, ttol, tol )
struct bu_list		*vhead;
struct rt_db_internal	*ip;
CONST struct rt_tess_tol *ttol;
CONST struct bn_tol		*tol;
{
	LOCAL struct rt_sketch_internal	*sketch_ip;
	struct curve			*crv;
	int				curve_no;

	RT_CK_DB_INTERNAL(ip);
	sketch_ip = (struct rt_sketch_internal *)ip->idb_ptr;
	RT_SKETCH_CK_MAGIC(sketch_ip);

	for( curve_no=0 ; curve_no < sketch_ip->curve_count ; curve_no++ )
	{
		long *lng;
		int seg_no;
		struct line_seg *lsg;
		int first=1;
		int start, end, prev=-1;
		point_t pt;

		crv = &sketch_ip->curves[curve_no];

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
						VJOIN2( pt, sketch_ip->V, sketch_ip->verts[start][0], sketch_ip->u_vec, sketch_ip->verts[start][1], sketch_ip->v_vec);
						RT_ADD_VLIST( vhead, pt, BN_VLIST_LINE_MOVE )
					}
					VJOIN2( pt, sketch_ip->V, sketch_ip->verts[end][0], sketch_ip->u_vec, sketch_ip->verts[end][1], sketch_ip->v_vec);
					RT_ADD_VLIST( vhead, pt, BN_VLIST_LINE_DRAW );
					prev = end;
					break;
				default:
				{
					bu_log( "rt_sketch_plot: ERROR: unrecognized segment type!!!!\n" );
					return( -1 );
				}
			}
			first = 0;
		}
	}

	return(0);
}

/*
 *			R T _ S K E T C H _ T E S S
 *
 *  Returns -
 *	-1	failure
 *	 0	OK.  *r points to nmgregion that holds this tessellation.
 */
int
rt_sketch_tess( r, m, ip, ttol, tol )
struct nmgregion	**r;
struct model		*m;
struct rt_db_internal	*ip;
CONST struct rt_tess_tol *ttol;
CONST struct bn_tol		*tol;
{
	return(-1);
}

/*
 *			R T _ S K E T C H _ I M P O R T
 *
 *  Import an SKETCH from the database format to the internal format.
 *  Apply modeling transformations as well.
 */
int
rt_sketch_import( ip, ep, mat, dbip )
struct rt_db_internal		*ip;
CONST struct bu_external	*ep;
register CONST mat_t		mat;
CONST struct db_i		*dbip;
{
	LOCAL struct rt_sketch_internal	*sketch_ip;
	union record			*rp;
	vect_t				v;
	int				seg_count;
	int				vert_no;
	int				seg_no;
	int				curve_no;
	unsigned char			*ptr;
	genptr_t			*segs;

	BU_CK_EXTERNAL( ep );
	rp = (union record *)ep->ext_buf;
	/* Check record type */
	if( rp->u_id != DBID_SKETCH )  {
		bu_log("rt_sketch_import: defective record\n");
		return(-1);
	}

	if( bu_debug&BU_DEBUG_MEM_CHECK )
	{
		bu_log( "Barrier check at start of sketch_import():\n" );
		bu_mem_barriercheck();
	}

	RT_INIT_DB_INTERNAL( ip );
	ip->idb_type = ID_SKETCH;
	ip->idb_meth = &rt_functab[ID_SKETCH];
	ip->idb_ptr = bu_malloc( sizeof(struct rt_sketch_internal), "rt_sketch_internal");
	sketch_ip = (struct rt_sketch_internal *)ip->idb_ptr;
	sketch_ip->magic = RT_SKETCH_INTERNAL_MAGIC;

	ntohd( (unsigned char *)v, rp->skt.skt_V, 3 );
	MAT4X3PNT( sketch_ip->V, mat, v );
	ntohd( (unsigned char *)v, rp->skt.skt_uvec, 3 );
	MAT4X3VEC( sketch_ip->u_vec, mat, v );
	ntohd( (unsigned char *)v, rp->skt.skt_vvec, 3 );
	MAT4X3VEC( sketch_ip->v_vec, mat, v );
	sketch_ip->vert_count = bu_glong( rp->skt.skt_vert_count );
	sketch_ip->curve_count = bu_glong( rp->skt.skt_curve_count );
	seg_count = bu_glong( rp->skt.skt_seg_count );

	ptr = (unsigned char *)rp;
	ptr += sizeof( struct sketch_rec );
	sketch_ip->verts = (point2d_t *)bu_calloc( sketch_ip->vert_count, sizeof( point2d_t ), "sketch_ip->vert" );
	for( vert_no=0 ; vert_no < sketch_ip->vert_count ; vert_no++ )
	{
		ntohd( (unsigned char *)&sketch_ip->verts[vert_no][0], ptr, 2 );
		ptr += 16;
	}

	segs = (genptr_t *)bu_calloc( seg_count, sizeof( genptr_t ), "segs" );
	for( seg_no=0 ; seg_no < seg_count ; seg_no++ )
	{
		long magic;
		struct line_seg *lsg;
		struct carc_seg *csg;

		magic = bu_glong( ptr );
		ptr += 4;
		switch( magic )
		{
			case CURVE_LSEG_MAGIC:
				lsg = (struct line_seg *)bu_malloc( sizeof( struct line_seg ), "lsg" );
				lsg->magic = magic;
				lsg->start = bu_glong( ptr );
				ptr += 4;
				lsg->end = bu_glong( ptr );
				ptr += 4;
				segs[seg_no] = (genptr_t)lsg;
				break;
			case CURVE_CARC_MAGIC:
				csg = (struct carc_seg *)bu_malloc( sizeof( struct carc_seg ), "csg" );
				csg->magic = magic;
				csg->start = bu_glong( ptr );
				ptr += 4;
				csg->end = bu_glong( ptr );
				ptr += 4;
				csg->orientation = bu_glong( ptr );
				ptr += 4;
				ntohd( (unsigned char *)&csg->radius, ptr, 1 );
				ptr += 8;
				segs[seg_no] = (genptr_t)csg;
				break;
			default:
				bu_bomb( "rt_sketch_import: ERROR: unrecognized segment type!!!\n" );
				break;
		}
	}

	sketch_ip->curves = (struct curve *)bu_calloc( sketch_ip->curve_count, sizeof( struct curve ), "sketch_ip->curves" );
	for( curve_no=0 ; curve_no < sketch_ip->curve_count ; curve_no++ )
	{
		struct curve *crv;
		int i;

		crv = &sketch_ip->curves[curve_no];
		NAMEMOVE( (char *)ptr, crv->crv_name );
		ptr += NAMESIZE;
		crv->seg_count = bu_glong( ptr );
		ptr += 4;

		crv->reverse = (int *)bu_calloc( crv->seg_count, sizeof(int), "crv->reverse" );
		crv->segments = (genptr_t *)bu_calloc( crv->seg_count, sizeof(genptr_t), "crv->segemnts" );
		for( i=0 ; i<crv->seg_count ; i++ )
		{
			int seg_index;

			crv->reverse[i] = bu_glong( ptr );
			ptr += 4;
			seg_index = bu_glong( ptr );
			ptr += 4;
			crv->segments[i] = segs[seg_index];
		}
	}

	bu_free( (char *)segs, "segs" );

	if( bu_debug&BU_DEBUG_MEM_CHECK )
	{
		bu_log( "Barrier check at end of sketch_import():\n" );
		bu_mem_barriercheck();
	}

	return(0);			/* OK */
}

/*
 *			R T _ S K E T C H _ E X P O R T
 *
 *  The name is added by the caller, in the usual place.
 */
int
rt_sketch_export( ep, ip, local2mm, dbip )
struct bu_external		*ep;
CONST struct rt_db_internal	*ip;
double				local2mm;
CONST struct db_i		*dbip;
{
	struct rt_sketch_internal	*sketch_ip;
	union record		*rec;
	int	curve_no, seg_no, vert_no, nbytes=0, ngran;
	struct bu_ptbl segs;
	vect_t tmp_vec;
	unsigned char *ptr;
	double tmp[2];

	RT_CK_DB_INTERNAL(ip);
	if( ip->idb_type != ID_SKETCH )  return(-1);
	sketch_ip = (struct rt_sketch_internal *)ip->idb_ptr;
	RT_SKETCH_CK_MAGIC(sketch_ip);

	if( bu_debug&BU_DEBUG_MEM_CHECK )
	{
		bu_log( "Barrier check at start of sketch_export():\n" );
		bu_mem_barriercheck();
	}

	BU_INIT_EXTERNAL(ep);

	bu_ptbl_init( &segs, 64, "rt_sketch_export: segs" );
	nbytes = sizeof(union record);		/* base record */
	nbytes += sketch_ip->vert_count*(8*2);	/* vertex list */
	nbytes += sketch_ip->curve_count*(NAMESIZE+4);	/* curve name and seg_count */
	for( curve_no=0 ; curve_no < sketch_ip->curve_count ; curve_no++ )
	{
		struct curve *crv;

		crv = &sketch_ip->curves[curve_no];
		nbytes += crv->seg_count*8;	/* reverse flags and indices into segments array */

		for( seg_no=0 ; seg_no < crv->seg_count ; seg_no++ )
			bu_ptbl_ins_unique( &segs, (long *)crv->segments[seg_no] );
	}

	for( seg_no=0 ; seg_no < BU_PTBL_END( &segs ) ; seg_no++ )
	{
		long *lng;

		lng = BU_PTBL_GET( &segs, seg_no );
		switch( *lng )
		{
			case CURVE_LSEG_MAGIC:
				nbytes += 12;
				break;
			case CURVE_CARC_MAGIC:
				nbytes += 24;
				break;
			default:
				bu_log( "rt_sketch_export: unsupported segement type (x%x)\n", *lng );
				bu_bomb( "rt_sketch_export: unsupported segement type\n" );
		}
	}

	ngran = ceil((double)(nbytes + sizeof(union record)) / sizeof(union record));
	ep->ext_nbytes = ngran * sizeof(union record);	
	ep->ext_buf = (genptr_t)bu_calloc( 1, ep->ext_nbytes, "sketch external");

	rec = (union record *)ep->ext_buf;

	rec->skt.skt_id = DBID_SKETCH;

	/* convert from local editing units to mm and export
	 * to database record format
	 */
	VSCALE( tmp_vec, sketch_ip->V, local2mm );
	htond( rec->skt.skt_V, (unsigned char *)tmp_vec, 3 );
	VSCALE( tmp_vec, sketch_ip->u_vec, local2mm );
	htond( rec->skt.skt_uvec, (unsigned char *)tmp_vec, 3 );
	VSCALE( tmp_vec, sketch_ip->v_vec, local2mm );
	htond( rec->skt.skt_vvec, (unsigned char *)tmp_vec, 3 );
	(void)bu_plong( rec->skt.skt_vert_count, sketch_ip->vert_count );
	(void)bu_plong( rec->skt.skt_curve_count, sketch_ip->curve_count );
	(void)bu_plong( rec->skt.skt_seg_count, BU_PTBL_END( &segs) );
	(void)bu_plong( rec->skt.skt_count, ngran-1 );

	ptr = (unsigned char *)rec;
	ptr += sizeof( struct sketch_rec );
	for( vert_no=0 ; vert_no < sketch_ip->vert_count ; vert_no++ )
	{
		/* write 2D point coordinates */
		V2MOVE( tmp, sketch_ip->verts[vert_no] )
		htond( ptr, (unsigned char *)tmp, 2 );
		ptr += 16;
	}

	for( seg_no=0 ; seg_no < BU_PTBL_END( &segs) ; seg_no++ )
	{
		struct line_seg *lseg;
		struct carc_seg *cseg;
		long *lng;

		/* write segment type ID, and segement parameters */
		lng = BU_PTBL_GET( &segs, seg_no );
		switch (*lng )
		{
			case CURVE_LSEG_MAGIC:
				lseg = (struct line_seg *)lng;
				(void)bu_plong( ptr, CURVE_LSEG_MAGIC );
				ptr += 4;
				(void)bu_plong( ptr, lseg->start );
				ptr +=4;
				(void)bu_plong( ptr, lseg->end );
				ptr += 4;
				break;
			case CURVE_CARC_MAGIC:
				cseg = (struct carc_seg *)lng;
				(void)bu_plong( ptr, CURVE_CARC_MAGIC );
				ptr += 4;
				(void)bu_plong( ptr, cseg->start );
				ptr += 4;
				(void)bu_plong( ptr, cseg->end );
				ptr += 4;
				(void) bu_plong( ptr, cseg->orientation );
				ptr += 4;
				htond( ptr, (unsigned char *)&cseg->radius, 1 );
				ptr += 8;
				break;
			default:
				bu_bomb( "rt_sketch_export: ERROR: unrecognized curve type!!!!\n" );
				break;
				
		}
	}

	for( curve_no=0 ; curve_no < sketch_ip->curve_count ; curve_no++ )
	{
		struct curve *crv;
		int i;

		crv = &sketch_ip->curves[curve_no];

		/* write curve parameters */
		NAMEMOVE( crv->crv_name, (char *)ptr );
		ptr += NAMESIZE;
		(void)bu_plong( ptr,  crv->seg_count );
		ptr += 4;
		for( i=0 ; i<crv->seg_count ; i++ )
		{
			int seg_index;

			(void)bu_plong( ptr, crv->reverse[i] );
			ptr += 4;
			seg_index = bu_ptbl_locate( &segs, (long *)crv->segments[i] );
			if( seg_index < 0 )
				bu_bomb( "rt_sketch_export: ERROR: unlisted segement encountered!!!\n" );

			(void)bu_plong( ptr, seg_index );
			ptr += 4;
		}
	}
	if( bu_debug&BU_DEBUG_MEM_CHECK )
	{
		bu_log( "Barrier check at end of sketch_export():\n" );
		bu_mem_barriercheck();
	}

	return(0);
}

/*
 *			R T _ S K E T C H _ D E S C R I B E
 *
 *  Make human-readable formatted presentation of this solid.
 *  First line describes type of solid.
 *  Additional lines are indented one tab, and give parameter values.
 */
int
rt_sketch_describe( str, ip, verbose, mm2local )
struct bu_vls		*str;
CONST struct rt_db_internal	*ip;
int			verbose;
double			mm2local;
{
	register struct rt_sketch_internal	*sketch_ip =
		(struct rt_sketch_internal *)ip->idb_ptr;
	int curve_no;
	int seg_no;
	char	buf[256];

	RT_SKETCH_CK_MAGIC(sketch_ip);
	bu_vls_strcat( str, "2D sketch (SKETCH)\n");

	sprintf(buf, "\tV = (%g %g %g)\n\t%d vertices, %d curves\n",
		V3ARGS( sketch_ip->V ),
		sketch_ip->vert_count,
		sketch_ip->curve_count );
	bu_vls_strcat( str, buf );

	if( !verbose )
		return( 0 );

	for( curve_no=0 ; curve_no < sketch_ip->curve_count ; curve_no++ )
	{
		sprintf(buf, "\tCurve: %-16.16s\n", sketch_ip->curves[curve_no].crv_name );
		bu_vls_strcat( str, buf );
		for( seg_no=0 ; seg_no < sketch_ip->curves[curve_no].seg_count ; seg_no++ )
		{
			struct line_seg *lsg;
			struct carc_seg *csg;
			struct nurb_seg *nsg;

			lsg = (struct line_seg *)sketch_ip->curves[curve_no].segments[seg_no];
			switch( lsg->magic )
			{
				case CURVE_LSEG_MAGIC:
					lsg = (struct line_seg *)sketch_ip->curves[curve_no].segments[seg_no];
					sprintf( buf, "\t\tLine segment (%g %g) <-> (%g %g)\n",
						V2ARGS( sketch_ip->verts[lsg->start] ),
						V2ARGS( sketch_ip->verts[lsg->end] ) );
					bu_vls_strcat( str, buf );
					break;
				case CURVE_CARC_MAGIC:
					csg = (struct carc_seg *)sketch_ip->curves[curve_no].segments[seg_no];
					sprintf( buf, "\t\tCircular Arc:\n" );
					bu_vls_strcat( str, buf );
					sprintf( buf, "\t\t\tstart: (%g, %g)\n",
						V2ARGS( sketch_ip->verts[csg->start] ) );
					bu_vls_strcat( str, buf );
					sprintf( buf, "\t\t\tend: (%g, %g)\n",
						V2ARGS( sketch_ip->verts[csg->end] ) );
					bu_vls_strcat( str, buf );
					sprintf( buf, "\t\t\tradius: %g\n", csg->radius );
					bu_vls_strcat( str, buf );
					sprintf( buf, "\t\t\torientation: %d\n", csg->orientation );
					bu_vls_strcat( str, buf );
					break;
				default:
					bu_bomb( "rt_sketch_describe: ERROR: unrecognized segment type\n" );
			}
		}
	}

	return(0);
}

/*
 *			R T _ S K E T C H _ I F R E E
 *
 *  Free the storage associated with the rt_db_internal version of this solid.
 */
void
rt_sketch_ifree( ip )
struct rt_db_internal	*ip;
{
	register struct rt_sketch_internal	*sketch_ip;
	struct curve				*crv;
	int					curve_no;
	genptr_t				*seg_ptr;
	int					seg_no;

	RT_CK_DB_INTERNAL(ip);
	sketch_ip = (struct rt_sketch_internal *)ip->idb_ptr;
	RT_SKETCH_CK_MAGIC(sketch_ip);
	sketch_ip->magic = 0;			/* sanity */

	if( bu_debug&BU_DEBUG_MEM_CHECK )
	{
		bu_log( "Barrier check at start of sketch_ifree():\n" );
		bu_mem_barriercheck();
	}

	bu_free( (char *)sketch_ip->verts, "sketch_ip->verts" );
	for( curve_no=0 ; curve_no < sketch_ip->curve_count ; curve_no++ )
	{
		long *lng;

		crv = &sketch_ip->curves[curve_no];

		for( seg_no=0 ; seg_no < crv->seg_count ; seg_no++ )
		{
			lng = (long *)crv->segments[seg_no];
			if( *lng > 0 )
			{
				*lng = 0;
				bu_free( (char *)lng, "lng" );
			}
		}

		bu_free( (char *)crv->reverse, "crv->reverse" );
		bu_free( (char *)crv->segments, "crv->segments" );
	}
	bu_free( (char *)sketch_ip->curves, "sketch_ip->curves" );
	bu_free( (char *)sketch_ip, "sketch ifree" );
	ip->idb_ptr = GENPTR_NULL;	/* sanity */

	if( bu_debug&BU_DEBUG_MEM_CHECK )
	{
		bu_log( "Barrier check at end of sketch_ifree():\n" );
		bu_mem_barriercheck();
	}

}

struct rt_sketch_internal *
rt_copy_sketch( sketch_ip )
CONST struct rt_sketch_internal *sketch_ip;
{
	struct rt_sketch_internal *out;
	int i;

	RT_SKETCH_CK_MAGIC( sketch_ip );

	if( bu_debug&BU_DEBUG_MEM_CHECK )
	{
		bu_log( "Barrier check at start of rt_copy_sketch():\n" );
		bu_mem_barriercheck();
	}

	out = (struct rt_sketch_internal *) bu_malloc( sizeof( struct rt_sketch_internal ), "rt_sketch_internal" );
	*out = *sketch_ip;	/* struct copy */

	out->verts = (point2d_t *)bu_calloc( out->vert_count, sizeof( point2d_t ), "out->verts" );
	for( i=0 ; i<out->vert_count ; i++ )
		V2MOVE( out->verts[i], sketch_ip->verts[i] );

	out->curves = (struct curve *)bu_calloc( out->curve_count, sizeof( struct curve ), "out->curves" );
	for( i=0 ; i<out->curve_count ; i++ )
	{
		struct curve *crv_out, *crv_in;
		int j;

		crv_out = &out->curves[i];
		crv_in = &sketch_ip->curves[i];
		strncpy( crv_out->crv_name, crv_in->crv_name, SKETCH_NAME_LEN );
		crv_out->seg_count = crv_in->seg_count;
		crv_out->reverse = (int *)bu_calloc( crv_out->seg_count, sizeof( int ), "crv->reverse" );
		crv_out->segments = (genptr_t *)bu_calloc( crv_out->seg_count, sizeof( genptr_t ), "crv->segments" );
		for( j=0 ; j<crv_out->seg_count ; j++ )
		{
			long *lng;
			struct line_seg *lsg_out, *lsg_in;
			struct carc_seg *csg_out, *csg_in;

			crv_out->reverse[j] = crv_in->reverse[j];
			lng = (long *)crv_in->segments[j];
			switch( *lng )
			{
				case CURVE_LSEG_MAGIC:
					lsg_in = (struct line_seg *)lng;
					lsg_out = (struct line_seg *)bu_malloc( sizeof( struct line_seg ), "line_seg" );
					crv_out->segments[j] = (genptr_t)lsg_out;
					*lsg_out = *lsg_in;
					break;
				case CURVE_CARC_MAGIC:
					csg_in = (struct carc_seg *)lng;
					csg_out = (struct carc_seg *)bu_malloc( sizeof( struct carc_seg ), "carc_seg" );
					crv_out->segments[j] = (genptr_t)csg_out;
					*csg_out = *csg_in;
					break;
				default:
					bu_log( "rt_copy_sketch: ERROR: unrecognized segment type!!!!\n" );
					return( (struct rt_sketch_internal *)NULL );
			}
		}
	}

	if( bu_debug&BU_DEBUG_MEM_CHECK )
	{
		bu_log( "Barrier check at end of rt_copy_sketch():\n" );
		bu_mem_barriercheck();
	}

	return( out );
}
