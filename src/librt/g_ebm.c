/*                         G _ E B M . C
 * BRL-CAD
 *
 * Copyright (C) 1988-2005 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */

/** \addtogroup g */

/*@{*/
/** @file g_ebm.c
 *	Intersect a ray with an Extruded Bitmap,
 *	where the bitmap is taken from a bw(5) file.
 *
 *  Author -
 *	Michael John Muuss
 *
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005
 *
 */
/*@}*/

#ifndef lint
static const char RCSebm[] = "@(#)$Header$ (BRL)";
#endif

#include "common.h"



#include <stdio.h>
#include <ctype.h>
#include <math.h>
#ifdef HAVE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif
#include <ctype.h>
#include "tcl.h"
#include "machine.h"
#include "vmath.h"
#include "db.h"
#include "nmg.h"
#include "rtgeom.h"
#include "raytrace.h"
#include "./debug.h"
#include "./fixpt.h"

/*
NOTES:
	Changed small to small11 for win32 compatibility
*/


struct rt_ebm_specific {
	struct rt_ebm_internal	ebm_i;
	vect_t		ebm_xnorm;	/* local +X norm in model coords */
	vect_t		ebm_ynorm;
	vect_t		ebm_znorm;
	vect_t		ebm_cellsize;	/* ideal coords: size of each cell */
	vect_t		ebm_origin;	/* local coords of grid origin (0,0,0) for now */
	vect_t		ebm_large;	/* local coords of XYZ max */
	mat_t		ebm_mat;	/* model to ideal space */
};
#define RT_EBM_NULL	((struct rt_ebm_specific *)0)

#define RT_EBM_O(m)	offsetof(struct rt_ebm_internal, m)

const struct bu_structparse rt_ebm_parse[] = {
#if CRAY && !__STDC__
	{"%s",	RT_EBM_NAME_LEN, "file",	1,	BU_STRUCTPARSE_FUNC_NULL },
#else
	{"%s",	RT_EBM_NAME_LEN, "file", bu_offsetofarray(struct rt_ebm_internal, file), BU_STRUCTPARSE_FUNC_NULL },
#endif
	{"%d",	1, "w",		RT_EBM_O(xdim),		BU_STRUCTPARSE_FUNC_NULL },
	{"%d",	1, "n",		RT_EBM_O(ydim),		BU_STRUCTPARSE_FUNC_NULL },
	{"%f",	1, "d",		RT_EBM_O(tallness),	BU_STRUCTPARSE_FUNC_NULL },
	{"%f",	16, "mat", bu_offsetofarray(struct rt_ebm_internal, mat), BU_STRUCTPARSE_FUNC_NULL },
	{"",	0, (char *)0, 0,			BU_STRUCTPARSE_FUNC_NULL }
};

struct ebm_hit_private {
	int	x_cell;
	int	y_cell;
};


BU_EXTERN(int rt_ebm_dda,(struct xray *rp, struct soltab *stp,
	struct application *ap, struct seg *seghead));
BU_EXTERN(int rt_seg_planeclip,(struct seg *out_hd, struct seg *in_hd,
	vect_t out_norm, fastf_t in, fastf_t out,
	struct xray *rp, struct application *ap));
BU_EXTERN( void rt_ebm_plate, ( int x1, int y1, int x2, int y2,
	double t, mat_t mat, struct bu_list *vhead ) );

/*
 *  Codes to represent surface normals.
 *  In a bitmap, there are only 4 possible normals.
 *  With this code, reverse the sign to reverse the direction.
 *  As always, the normal is expected to point outwards.
 */
#define NORM_ZPOS	3
#define NORM_YPOS	2
#define NORM_XPOS	1
#define NORM_XNEG	(-1)
#define NORM_YNEG	(-2)
#define NORM_ZNEG	(-3)

/*
 *  Regular bit addressing is used:  (0..W-1, 0..N-1),
 *  but the bitmap is stored with two cells of zeros all around,
 *  so permissible subscripts run (-2..W+1, -2..N+1).
 *  This eliminates special-case code for the boundary conditions.
 */
#define	BIT_XWIDEN	2
#define	BIT_YWIDEN	2
#define BIT(_eip,_xx,_yy)	\
	((unsigned char *)((_eip)->mp->apbuf))[ \
		((_yy)+BIT_YWIDEN)*((_eip)->xdim + \
		BIT_XWIDEN*2)+(_xx)+BIT_XWIDEN ]

/**
 *			R T _ S E G _ P L A N E C L I P
 *
 *  Take a segment chain, in sorted order (ascending hit_dist),
 *  and clip to the range (in, out) along the normal "out_norm".
 *  For the particular ray "rp", find the parametric distances:
 *	kmin is the minimum permissible parameter, "in" units away
 *	kmax is the maximum permissible parameter, "out" units away
 *
 *  Returns -
 *	1	OK: trimmed segment chain, still in sorted order
 *	0	ERROR
 */
int
rt_seg_planeclip(struct seg *out_hd, struct seg *in_hd, fastf_t *out_norm, fastf_t in, fastf_t out, struct xray *rp, struct application *ap)
{
	fastf_t		norm_dist_min, norm_dist_max;
	fastf_t		slant_factor;
	fastf_t		kmin, kmax;
	vect_t		in_norm;
	register struct seg	*curr;
	int		out_norm_code;
	int		count;

	norm_dist_min = in - VDOT( rp->r_pt, out_norm );
	slant_factor = VDOT( rp->r_dir, out_norm );	/* always abs < 1 */
	if( NEAR_ZERO( slant_factor, SQRT_SMALL_FASTF ) )  {
		if( norm_dist_min < 0.0 )  {
			bu_log("rt_seg_planeclip ERROR -- ray parallel to baseplane, outside \n");
			/* XXX Free segp chain */
			return(0);
		}
		kmin = -INFINITY;
	} else
		kmin =  norm_dist_min / slant_factor;

	VREVERSE( in_norm, out_norm );
	norm_dist_max = out - VDOT( rp->r_pt, in_norm );
	slant_factor = VDOT( rp->r_dir, in_norm );	/* always abs < 1 */
	if( NEAR_ZERO( slant_factor, SQRT_SMALL_FASTF ) )  {
		if( norm_dist_max < 0.0 )  {
			bu_log("rt_seg_planeclip ERROR -- ray parallel to baseplane, outside \n");
			/* XXX Free segp chain */
			return(0);
		}
		kmax = INFINITY;
	} else
		kmax =  norm_dist_max / slant_factor;

	if( kmin > kmax )  {
		/* If r_dir[Z] < 0, will need to swap min & max */
		slant_factor = kmax;
		kmax = kmin;
		kmin = slant_factor;
		out_norm_code = NORM_ZPOS;
	} else {
		out_norm_code = NORM_ZNEG;
	}
	if(RT_G_DEBUG&DEBUG_EBM)bu_log("kmin=%g, kmax=%g, out_norm_code=%d\n", kmin, kmax, out_norm_code );

	count = 0;
	while( BU_LIST_WHILE( curr, seg, &(in_hd->l) ) )  {
		BU_LIST_DEQUEUE( &(curr->l) );
		if(RT_G_DEBUG&DEBUG_EBM)bu_log(" rt_seg_planeclip seg( %g, %g )\n", curr->seg_in.hit_dist, curr->seg_out.hit_dist );
		if( curr->seg_out.hit_dist <= kmin )  {
			if(RT_G_DEBUG&DEBUG_EBM)bu_log("seg_out %g <= kmin %g, freeing\n", curr->seg_out.hit_dist, kmin );
			RT_FREE_SEG(curr, ap->a_resource);
			continue;
		}
		if( curr->seg_in.hit_dist >= kmax )  {
			if(RT_G_DEBUG&DEBUG_EBM)bu_log("seg_in  %g >= kmax %g, freeing\n", curr->seg_in.hit_dist, kmax );
			RT_FREE_SEG(curr, ap->a_resource);
			continue;
		}
		if( curr->seg_in.hit_dist <= kmin )  {
			if(RT_G_DEBUG&DEBUG_EBM)bu_log("seg_in = kmin %g\n", kmin );
			curr->seg_in.hit_dist = kmin;
			curr->seg_in.hit_surfno = out_norm_code;
		}
		if( curr->seg_out.hit_dist >= kmax )  {
			if(RT_G_DEBUG&DEBUG_EBM)bu_log("seg_out= kmax %g\n", kmax );
			curr->seg_out.hit_dist = kmax;
			curr->seg_out.hit_surfno = (-out_norm_code);
		}
		BU_LIST_INSERT( &(out_hd->l), &(curr->l) );
		count += 2;
	}
	return( count );
}

static int rt_ebm_normtab[3] = { NORM_XPOS, NORM_YPOS, NORM_ZPOS };


/**
 *			R T _ E B M _ D D A
 *
 *  Step through the 2-D array, in local coordinates ("ideal space").
 */
int
rt_ebm_dda(register struct xray *rp, struct soltab *stp, struct application *ap, struct seg *seghead)
{
	register struct rt_ebm_specific *ebmp =
		(struct rt_ebm_specific *)stp->st_specific;
	vect_t	invdir;
	double	t0;	/* in point of cell */
	double	t1;	/* out point of cell */
	double	tmax;	/* out point of entire grid */
	vect_t	t;	/* next t value for XYZ cell plane intersect */
	vect_t	delta;	/* spacing of XYZ cell planes along ray */
	int	igrid[3];/* Grid cell coordinates of cell (integerized) */
	vect_t	P;	/* hit point */
	int	inside;	/* inside/outside a solid flag */
	int	in_index;
	int	out_index;
	int	j;

	/* Compute the inverse of the direction cosines */
	if( !NEAR_ZERO( rp->r_dir[X], SQRT_SMALL_FASTF ) )  {
		invdir[X]=1.0/rp->r_dir[X];
	} else {
		invdir[X] = INFINITY;
		rp->r_dir[X] = 0.0;
	}
	if( !NEAR_ZERO( rp->r_dir[Y], SQRT_SMALL_FASTF ) )  {
		invdir[Y]=1.0/rp->r_dir[Y];
	} else {
		invdir[Y] = INFINITY;
		rp->r_dir[Y] = 0.0;
	}
	if( !NEAR_ZERO( rp->r_dir[Z], SQRT_SMALL_FASTF ) )  {
		invdir[Z]=1.0/rp->r_dir[Z];
	} else {
		invdir[Z] = INFINITY;
		rp->r_dir[Z] = 0.0;
	}

	/* intersect ray with ideal grid rpp */
	VSETALL( P, 0 );
	if( ! rt_in_rpp(rp, invdir, P, ebmp->ebm_large ) )
		return(0);	/* MISS */

	VJOIN1( P, rp->r_pt, rp->r_min, rp->r_dir );
	/* P is hit point (on RPP?) */

if(RT_G_DEBUG&DEBUG_EBM)VPRINT("ebm_origin", ebmp->ebm_origin);
if(RT_G_DEBUG&DEBUG_EBM)VPRINT("r_pt", rp->r_pt);
if(RT_G_DEBUG&DEBUG_EBM)VPRINT("P", P);
if(RT_G_DEBUG&DEBUG_EBM)VPRINT("cellsize", ebmp->ebm_cellsize);
	t0 = rp->r_min;
	tmax = rp->r_max;
if(RT_G_DEBUG&DEBUG_EBM)bu_log("[shoot: r_min=%g, r_max=%g]\n", rp->r_min, rp->r_max);

	/* find grid cell where ray first hits ideal space bounding RPP */
	igrid[X] = (P[X] - ebmp->ebm_origin[X]) / ebmp->ebm_cellsize[X];
	igrid[Y] = (P[Y] - ebmp->ebm_origin[Y]) / ebmp->ebm_cellsize[Y];
	if( igrid[X] < 0 )  {
		igrid[X] = 0;
	} else if( igrid[X] >= ebmp->ebm_i.xdim ) {
		igrid[X] = ebmp->ebm_i.xdim-1;
	}
	if( igrid[Y] < 0 )  {
		igrid[Y] = 0;
	} else if( igrid[Y] >= ebmp->ebm_i.ydim ) {
		igrid[Y] = ebmp->ebm_i.ydim-1;
	}
if(RT_G_DEBUG&DEBUG_EBM)bu_log("g[X] = %d, g[Y] = %d\n", igrid[X], igrid[Y]);

	if( rp->r_dir[X] == 0.0 && rp->r_dir[Y] == 0.0 )  {
		register struct seg	*segp;

		/*  Ray is traveling exactly along Z axis.
		 *  Just check the one cell hit.
		 *  Depend on higher level to clip ray to Z extent.
		 */
if(RT_G_DEBUG&DEBUG_EBM)bu_log("ray on local Z axis\n");
		if( BIT( &ebmp->ebm_i, igrid[X], igrid[Y] ) == 0 )
			return(0);	/* MISS */
		RT_GET_SEG(segp, ap->a_resource);
		segp->seg_stp = stp;
		segp->seg_in.hit_dist = 0;
		segp->seg_out.hit_dist = INFINITY;

		segp->seg_in.hit_vpriv[X] =
			(double) igrid[X] / ebmp->ebm_i.xdim;
		segp->seg_in.hit_vpriv[Y] =
			(double) igrid[Y] / ebmp->ebm_i.ydim;

		segp->seg_out.hit_vpriv[X] =
			(double) igrid[X] / ebmp->ebm_i.xdim;
		segp->seg_out.hit_vpriv[Y] =
			(double) igrid[Y] / ebmp->ebm_i.ydim;

		if( rp->r_dir[Z] < 0 )  {
			segp->seg_in.hit_surfno = NORM_ZPOS;
			segp->seg_out.hit_surfno = NORM_ZNEG;
		} else {
			segp->seg_in.hit_surfno = NORM_ZNEG;
			segp->seg_out.hit_surfno = NORM_ZPOS;
		}
		BU_LIST_INSERT( &(seghead->l), &(segp->l) );
		return(2);			/* HIT */
	}

	/* X setup */
	if( rp->r_dir[X] == 0.0 )  {
		t[X] = INFINITY;
		delta[X] = 0;
	} else {
		j = igrid[X];
		if( rp->r_dir[X] < 0 ) j++;
		t[X] = (ebmp->ebm_origin[X] + j*ebmp->ebm_cellsize[X] -
			rp->r_pt[X]) * invdir[X];
		delta[X] = ebmp->ebm_cellsize[X] * fabs(invdir[X]);
	}
	/* Y setup */
	if( rp->r_dir[Y] == 0.0 )  {
		t[Y] = INFINITY;
		delta[Y] = 0;
	} else {
		j = igrid[Y];
		if( rp->r_dir[Y] < 0 ) j++;
		t[Y] = (ebmp->ebm_origin[Y] + j*ebmp->ebm_cellsize[Y] -
			rp->r_pt[Y]) * invdir[Y];
		delta[Y] = ebmp->ebm_cellsize[Y] * fabs(invdir[Y]);
	}
#if 0
	/* Z setup */
	if( rp->r_dir[Z] == 0.0 )  {
		t[Z] = INFINITY;
	} else {
		/* Consider igrid[Z] to be either 0 or 1 */
		if( rp->r_dir[Z] < 0 )  {
			t[Z] = (ebmp->ebm_origin[Z] + ebmp->ebm_cellsize[Z] -
				rp->r_pt[Z]) * invdir[Z];
		} else {
			t[Z] = (ebmp->ebm_origin[Z] - rp->r_pt[Z]) * invdir[Z];
		}
	}
#endif

	/* The delta[] elements *must* be positive, as t must increase */
if(RT_G_DEBUG&DEBUG_EBM)bu_log("t[X] = %g, delta[X] = %g\n", t[X], delta[X] );
if(RT_G_DEBUG&DEBUG_EBM)bu_log("t[Y] = %g, delta[Y] = %g\n", t[Y], delta[Y] );

	/* Find face of entry into first cell -- max initial t value */
	if( t[X] == INFINITY ) {
		in_index = Y;
		t0 = t[Y];
	}
	else if( t[Y] == INFINITY ) {
		in_index = X;
		t0 = t[X];
	}
	else if( t[X] >= t[Y]  )  {
		in_index = X;
		t0 = t[X];
	} else {
		in_index = Y;
		t0 = t[Y];
	}
if(RT_G_DEBUG&DEBUG_EBM)bu_log("Entry index is %s, t0=%g\n", in_index==X ? "X" : "Y", t0);

	/* Advance to next exits */
	t[X] += delta[X];
	t[Y] += delta[Y];

	/* Ensure that next exit is after first entrance */
	if( t[X] < t0 )  {
		bu_log("*** advancing t[X]\n");
		t[X] += delta[X];
	}
	if( t[Y] < t0 )  {
		bu_log("*** advancing t[Y]\n");
		t[Y] += delta[Y];
	}
if(RT_G_DEBUG&DEBUG_EBM)bu_log("Exit t[X]=%g, t[Y]=%g\n", t[X], t[Y] );

	inside = 0;

	while( t0 < tmax ) {
		int	val;
		struct seg	*segp;

		/* find minimum exit t value */
		out_index = t[X] < t[Y] ? X : Y;

		t1 = t[out_index];

		/* Ray passes through cell igrid[XY] from t0 to t1 */
		val = BIT( &ebmp->ebm_i, igrid[X], igrid[Y] );
if(RT_G_DEBUG&DEBUG_EBM)bu_log("igrid [%d %d] from %g to %g, val=%d\n",
			igrid[X], igrid[Y],
			t0, t1, val );
if(RT_G_DEBUG&DEBUG_EBM)bu_log("Exit index is %s, t[X]=%g, t[Y]=%g\n",
			out_index==X ? "X" : "Y", t[X], t[Y] );


		if( t1 <= t0 )  bu_log("ERROR ebm t1=%g < t0=%g\n", t1, t0 );
		if( !inside )  {
			if( val > 0 )  {
				/* Handle the transition from vacuum to solid */
				/* Start of segment (entering a full voxel) */
				inside = 1;

				RT_GET_SEG(segp, ap->a_resource);
				segp->seg_stp = stp;
				segp->seg_in.hit_dist = t0;

				segp->seg_in.hit_vpriv[X] =
					(double) igrid[X] / ebmp->ebm_i.xdim;
				segp->seg_in.hit_vpriv[Y] =
					(double) igrid[Y] / ebmp->ebm_i.ydim;

				/* Compute entry normal */
				if( rp->r_dir[in_index] < 0 )  {
					/* Go left, entry norm goes right */
					segp->seg_in.hit_surfno =
						rt_ebm_normtab[in_index];
				}  else  {
					/* go right, entry norm goes left */
					segp->seg_in.hit_surfno =
						(-rt_ebm_normtab[in_index]);
				}
				BU_LIST_INSERT( &(seghead->l), &(segp->l) );

				if(RT_G_DEBUG&DEBUG_EBM) bu_log("START t=%g, surfno=%d\n",
					t0, segp->seg_in.hit_surfno);
			} else {
				/* Do nothing, marching through void */
			}
		} else {
			register struct seg	*tail;
			if( val > 0 )  {
				/* Do nothing, marching through solid */
			} else {
				/* End of segment (now in an empty voxel) */
				/* Handle transition from solid to vacuum */
				inside = 0;

				tail = BU_LIST_LAST( seg, &(seghead->l) );
				tail->seg_out.hit_dist = t0;

				tail->seg_out.hit_vpriv[X] =
					(double) igrid[X] / ebmp->ebm_i.xdim;
				tail->seg_out.hit_vpriv[Y] =
					(double) igrid[Y] / ebmp->ebm_i.ydim;

				/* Compute exit normal */
				if( rp->r_dir[in_index] < 0 )  {
					/* Go left, exit normal goes left */
					tail->seg_out.hit_surfno =
						(-rt_ebm_normtab[in_index]);
				}  else  {
					/* go right, exit norm goes right */
					tail->seg_out.hit_surfno =
						rt_ebm_normtab[in_index];
				}
				if(RT_G_DEBUG&DEBUG_EBM) bu_log("END t=%g, surfno=%d\n",
					t0, tail->seg_out.hit_surfno );
			}
		}

		/* Take next step */
		t0 = t1;
		in_index = out_index;
		t[out_index] += delta[out_index];
		if( rp->r_dir[out_index] > 0 ) {
			igrid[out_index]++;
		} else {
			igrid[out_index]--;
		}
	}

	if( inside )  {
		register struct seg	*tail;

		/* Close off the final segment */
		tail = BU_LIST_LAST( seg, &(seghead->l) );
		tail->seg_out.hit_dist = tmax;
		VSETALL(tail->seg_out.hit_vpriv, 0.0);

		/* Compute exit normal.  Previous out_index is now in_index */
		if( rp->r_dir[in_index] < 0 )  {
			/* Go left, exit normal goes left */
			tail->seg_out.hit_surfno = (-rt_ebm_normtab[in_index]);
		}  else  {
			/* go right, exit norm goes right */
			tail->seg_out.hit_surfno = rt_ebm_normtab[in_index];
		}
		if(RT_G_DEBUG&DEBUG_EBM) bu_log("closed END t=%g, surfno=%d\n",
			tmax, tail->seg_out.hit_surfno );
	}

	if( BU_LIST_IS_EMPTY( &(seghead->l) ) )
		return(0);
	return(2);
}

/**
 *			R T _ E B M _ I M P O R T
 *
 *  Read in the information from the string solid record.
 *  Then, as a service to the application, read in the bitmap
 *  and set up some of the associated internal variables.
 */
int
rt_ebm_import(struct rt_db_internal *ip, const struct bu_external *ep, const fastf_t *mat, const struct db_i *dbip)
{
	union record	*rp;
	register struct rt_ebm_internal *eip;
	struct bu_vls	str;
	int		nbytes;
	mat_t		tmat;
	struct bu_mapped_file	*mp;

	BU_CK_EXTERNAL( ep );
	rp = (union record *)ep->ext_buf;
	if( rp->u_id != DBID_STRSOL )  {
		bu_log("rt_ebm_import: defective strsol record\n");
		return(-1);
	}

	RT_CK_DB_INTERNAL( ip );
	ip->idb_major_type = DB5_MAJORTYPE_BRLCAD;
	ip->idb_type = ID_EBM;
	ip->idb_meth = &rt_functab[ID_EBM];
	ip->idb_ptr = bu_calloc(1, sizeof(struct rt_ebm_internal), "rt_ebm_internal");
	eip = (struct rt_ebm_internal *)ip->idb_ptr;
	eip->magic = RT_EBM_INTERNAL_MAGIC;

	/* Provide default orientation info */
	MAT_IDN( eip->mat );

	bu_vls_init( &str );
	bu_vls_strcpy( &str, rp->ss.ss_args );
	if( bu_struct_parse( &str, rt_ebm_parse, (char *)eip ) < 0 )  {
		bu_vls_free( &str );
		bu_free( (char *)eip , "rt_ebm_import: eip" );
		ip->idb_type = ID_NULL;
		ip->idb_ptr = (genptr_t)NULL;
		return -2;
	}
	bu_vls_free( &str );

	/* Check for reasonable values */
	if( eip->file[0] == '\0' || eip->xdim < 1 ||
	    eip->ydim < 1 || eip->mat[15] <= 0.0 ||
	    eip->tallness <= 0.0 )  {
	    	bu_struct_print( "Unreasonable EBM parameters", rt_ebm_parse,
	    		(char *)eip );
	    	bu_free( (char *)eip , "rt_ebm_import: eip" );
	    	ip->idb_type = ID_NULL;
	    	ip->idb_ptr = (genptr_t)NULL;
		return -1;
	}

	/* Apply any modeling transforms to get final matrix */
	bn_mat_mul( tmat, mat, eip->mat );
	MAT_COPY( eip->mat, tmat );

	/* Get bit map from .bw(5) file */
	if( !(mp = bu_open_mapped_file_with_path( dbip->dbi_filepath, eip->file, "ebm" )) )  {
		bu_log("rt_ebm_import() unable to open '%s'\n", eip->file);
		bu_free( (char *)eip , "rt_ebm_import: eip" );
fail:
		ip->idb_type = ID_NULL;
		ip->idb_ptr = (genptr_t)NULL;
		return -1;
	}
	eip->mp = mp;
	if( mp->buflen < eip->xdim*eip->ydim )  {
		bu_log("rt_ebm_import() file '%s' is too short %d < %d\n",
			eip->file, mp->buflen, eip->xdim*eip->ydim );
		goto fail;
	}

	nbytes = (eip->xdim+BIT_XWIDEN*2)*(eip->ydim+BIT_YWIDEN*2);

	/* If first use of this file, prepare in-memory buffer */
	if( !mp->apbuf )  {
		register int	y;
		unsigned char	*cp;

		/* Prevent a multi-processor race */
		bu_semaphore_acquire(RT_SEM_MODEL);
		if( mp->apbuf )  {
			/* someone else beat us, nothing more to do */
			bu_semaphore_release(RT_SEM_MODEL);
			return 0;
		}
		mp->apbuf = (genptr_t)bu_calloc(
			1, nbytes, "rt_ebm_import bitmap" );
		mp->apbuflen = nbytes;

		bu_semaphore_release(RT_SEM_MODEL);

		/* Because of in-memory padding, read each scanline separately */
		cp = (unsigned char *)mp->buf;
		for( y=0; y < eip->ydim; y++ )  {
			/* BIT() addresses into mp->apbuf */
			bcopy( cp, &BIT( eip, 0, y), eip->xdim );
			cp += eip->xdim;
		}
	}
	return( 0 );
}

/**
 *			R T _ E B M _ E X P O R T
 *
 *  The name will be added by the caller.
 */
int
rt_ebm_export(struct bu_external *ep, const struct rt_db_internal *ip, double local2mm, const struct db_i *dbip)
{
	struct rt_ebm_internal	*eip;
	struct rt_ebm_internal	ebm;	/* scaled version */
	union record		*rec;
	struct bu_vls		str;

	RT_CK_DB_INTERNAL(ip);
	if( ip->idb_type != ID_EBM )  return(-1);
	eip = (struct rt_ebm_internal *)ip->idb_ptr;
	RT_EBM_CK_MAGIC(eip);
	ebm = *eip;			/* struct copy */

	/* Apply scale factor */
	ebm.mat[15] /= local2mm;

	BU_CK_EXTERNAL(ep);
	ep->ext_nbytes = sizeof(union record)*DB_SS_NGRAN;
	ep->ext_buf = (genptr_t)bu_calloc( 1, ep->ext_nbytes, "ebm external");
	rec = (union record *)ep->ext_buf;

	bu_vls_init( &str );
	bu_vls_struct_print( &str, rt_ebm_parse, (char *)&ebm );

	rec->ss.ss_id = DBID_STRSOL;
	strncpy( rec->ss.ss_keyword, "ebm", NAMESIZE-1 );
	strncpy( rec->ss.ss_args, bu_vls_addr(&str), DB_SS_LEN-1 );
	bu_vls_free( &str );

	return(0);
}


/**
 *			R T _ E B M _ I M P O R T 5
 *
 *  Read in the information from the string solid record.
 *  Then, as a service to the application, read in the bitmap
 *  and set up some of the associated internal variables.
 */
int
rt_ebm_import5(struct rt_db_internal *ip, const struct bu_external *ep, const fastf_t *mat, const struct db_i *dbip)
{
	register struct rt_ebm_internal *eip;
	struct bu_vls	str;
	int		nbytes;
	mat_t		tmat;
	struct bu_mapped_file	*mp;

	BU_CK_EXTERNAL( ep );
	RT_CK_DB_INTERNAL( ip );

	ip->idb_major_type = DB5_MAJORTYPE_BRLCAD;
	ip->idb_type = ID_EBM;
	ip->idb_meth = &rt_functab[ID_EBM];
	ip->idb_ptr = bu_calloc(1, sizeof(struct rt_ebm_internal), "rt_ebm_internal");
	eip = (struct rt_ebm_internal *)ip->idb_ptr;
	eip->magic = RT_EBM_INTERNAL_MAGIC;

	/* Provide default orientation info */
	MAT_IDN( eip->mat );

	bu_vls_init( &str );
	bu_vls_strcpy( &str, ep->ext_buf );
	if( bu_struct_parse( &str, rt_ebm_parse, (char *)eip ) < 0 )  {
		bu_vls_free( &str );
		bu_free( (char *)eip , "rt_ebm_import: eip" );
		ip->idb_type = ID_NULL;
		ip->idb_ptr = (genptr_t)NULL;
		return -2;
	}
	bu_vls_free( &str );

	/* Check for reasonable values */
	if( eip->file[0] == '\0' || eip->xdim < 1 ||
	    eip->ydim < 1 || eip->mat[15] <= 0.0 ||
	    eip->tallness <= 0.0 )  {
	    	bu_struct_print( "Unreasonable EBM parameters", rt_ebm_parse,
	    		(char *)eip );
	    	bu_free( (char *)eip , "rt_ebm_import: eip" );
	    	ip->idb_type = ID_NULL;
	    	ip->idb_ptr = (genptr_t)NULL;
		return -1;
	}

	/* Apply any modeling transforms to get final matrix */
	bn_mat_mul( tmat, mat, eip->mat );
	MAT_COPY( eip->mat, tmat );

	/* Get bit map from .bw(5) file */
	if( !(mp = bu_open_mapped_file_with_path( dbip->dbi_filepath, eip->file, "ebm" )) )  {
		bu_log("rt_ebm_import() unable to open '%s'\n", eip->file);
		bu_free( (char *)eip , "rt_ebm_import: eip" );
fail:
		ip->idb_type = ID_NULL;
		ip->idb_ptr = (genptr_t)NULL;
		return -1;
	}
	eip->mp = mp;
	if( mp->buflen < eip->xdim*eip->ydim )  {
		bu_log("rt_ebm_import() file '%s' is too short %d < %d\n",
			eip->file, mp->buflen, eip->xdim*eip->ydim );
		goto fail;
	}

	nbytes = (eip->xdim+BIT_XWIDEN*2)*(eip->ydim+BIT_YWIDEN*2);

	/* If first use of this file, prepare in-memory buffer */
	if( !mp->apbuf )  {
		register int	y;
		unsigned char	*cp;

		/* Prevent a multi-processor race */
		bu_semaphore_acquire(RT_SEM_MODEL);
		if( mp->apbuf )  {
			/* someone else beat us, nothing more to do */
			bu_semaphore_release(RT_SEM_MODEL);
			return 0;
		}
		mp->apbuf = (genptr_t)bu_calloc(
			1, nbytes, "rt_ebm_import bitmap" );
		mp->apbuflen = nbytes;

		bu_semaphore_release(RT_SEM_MODEL);

		/* Because of in-memory padding, read each scanline separately */
		cp = (unsigned char *)mp->buf;
		for( y=0; y < eip->ydim; y++ )  {
			/* BIT() addresses into mp->apbuf */
			bcopy( cp, &BIT( eip, 0, y), eip->xdim );
			cp += eip->xdim;
		}
	}
	return( 0 );
}

/**
 *			R T _ E B M _ E X P O R T 5
 *
 *  The name will be added by the caller.
 */
int
rt_ebm_export5(struct bu_external *ep, const struct rt_db_internal *ip, double local2mm, const struct db_i *dbip)
{
	struct rt_ebm_internal	*eip;
	struct rt_ebm_internal	ebm;	/* scaled version */
	struct bu_vls		str;

	RT_CK_DB_INTERNAL(ip);
	if( ip->idb_type != ID_EBM )  return(-1);
	eip = (struct rt_ebm_internal *)ip->idb_ptr;
	RT_EBM_CK_MAGIC(eip);
	ebm = *eip;			/* struct copy */

	/* Apply scale factor */
	ebm.mat[15] /= local2mm;

	BU_CK_EXTERNAL(ep);

	bu_vls_init( &str );
	bu_vls_struct_print( &str, rt_ebm_parse, (char *)&ebm );

	ep->ext_nbytes = bu_vls_strlen( &str ) + 1;
	ep->ext_buf = (genptr_t)bu_calloc( 1, ep->ext_nbytes, "ebm external");

	strcpy( ep->ext_buf , bu_vls_addr(&str) );
	bu_vls_free( &str );

	return(0);
}

/**
 *			R T _ E B M _ D E S C R I B E
 *
 *  Make human-readable formatted presentation of this solid.
 *  First line describes type of solid.
 *  Additional lines are indented one tab, and give parameter values.
 */
int
rt_ebm_describe(struct bu_vls *str, const struct rt_db_internal *ip, int verbose, double mm2local)
{
	register struct rt_ebm_internal	*eip =
		(struct rt_ebm_internal *)ip->idb_ptr;
	int i;
	struct bu_vls substr;

	RT_EBM_CK_MAGIC(eip);

	bu_vls_init( &substr );
	bu_vls_strcat( str, "extruded bitmap (EBM)\n\t");

/*	bu_vls_struct_print( str, rt_ebm_parse, (char *)eip );
	bu_vls_strcat( str, "\n" );	*/

	bu_vls_printf( &substr, "  file=\"%s\" w=%d n=%d depth=%g\n   mat=",
		eip->file, eip->xdim, eip->ydim, INTCLAMP(eip->tallness*mm2local) );
	bu_vls_vlscat( str, &substr );
	for( i=0 ; i<15 ; i++ )
	{
		bu_vls_trunc2( &substr, 0 );
		bu_vls_printf( &substr, "%g,", INTCLAMP(eip->mat[i]) );
		bu_vls_vlscat( str, &substr );
	}
	bu_vls_trunc2( &substr, 0 );
	bu_vls_printf( &substr, "%g\n", INTCLAMP(eip->mat[15]) );
	bu_vls_vlscat( str, &substr );

	bu_vls_free( &substr );

	return(0);
}

/**
 *			R T _ E B M _ I F R E E
 *
 *  Free the storage associated with the rt_db_internal version of this solid.
 */
void
rt_ebm_ifree(struct rt_db_internal *ip)
{
	register struct rt_ebm_internal	*eip;

	RT_CK_DB_INTERNAL(ip);
	eip = (struct rt_ebm_internal *)ip->idb_ptr;
	RT_EBM_CK_MAGIC(eip);

	if(eip->mp)  {
		BU_CK_MAPPED_FILE(eip->mp);
		bu_close_mapped_file(eip->mp);
	}

	eip->magic = 0;			/* sanity */
	eip->mp = (struct bu_mapped_file *)0;
	bu_free( (char *)eip, "ebm ifree" );
	ip->idb_ptr = GENPTR_NULL;	/* sanity */
	ip->idb_type = ID_NULL;		/* sanity */
}

/**
 *			R T _ E B M _ P R E P
 *
 *  Returns -
 *	0	OK
 *	!0	Failure
 *
 *  Implicit return -
 *	A struct rt_ebm_specific is created, and it's address is stored
 *	in stp->st_specific for use by rt_ebm_shot().
 */
int
rt_ebm_prep(struct soltab *stp, struct rt_db_internal *ip, struct rt_i *rtip)
{
	struct rt_ebm_internal	*eip;
	register struct rt_ebm_specific *ebmp;
	vect_t	norm;
	vect_t	radvec;
	vect_t	diam;
	vect_t	small1;

	eip = (struct rt_ebm_internal *)ip->idb_ptr;
	RT_EBM_CK_MAGIC(eip);

	BU_GETSTRUCT( ebmp, rt_ebm_specific );
	ebmp->ebm_i = *eip;		/* struct copy */

	/* "steal" the bitmap storage */
	eip->mp = (struct bu_mapped_file *)0;	/* "steal" the mapped file */

	/* build Xform matrix from model(world) to ideal(local) space */
	bn_mat_inv( ebmp->ebm_mat, eip->mat );

	/* Pre-compute the necessary normals.  Rotate only. */
	VSET( norm, 1, 0 , 0 );
	MAT3X3VEC( ebmp->ebm_xnorm, eip->mat, norm );
	VSET( norm, 0, 1, 0 );
	MAT3X3VEC( ebmp->ebm_ynorm, eip->mat, norm );
	VSET( norm, 0, 0, 1 );
	MAT3X3VEC( ebmp->ebm_znorm, eip->mat, norm );

	stp->st_specific = (genptr_t)ebmp;

	/* Find bounding RPP of rotated local RPP */
	VSETALL( small1, 0 );
	VSET( ebmp->ebm_large, ebmp->ebm_i.xdim, ebmp->ebm_i.ydim, ebmp->ebm_i.tallness );
	bn_rotate_bbox( stp->st_min, stp->st_max, eip->mat,
		small1, ebmp->ebm_large );

	/* for now, EBM origin in ideal coordinates is at origin */
	VSETALL( ebmp->ebm_origin, 0 );
	VADD2( ebmp->ebm_large, ebmp->ebm_large, ebmp->ebm_origin );

	/* for now, EBM cell size in ideal coordinates is one unit/cell */
	VSETALL( ebmp->ebm_cellsize, 1 );

	VSUB2( diam, stp->st_max, stp->st_min );
	VADD2SCALE( stp->st_center, stp->st_min, stp->st_max, 0.5 );
	VSCALE( radvec, diam, 0.5 );
	stp->st_aradius = stp->st_bradius = MAGNITUDE( radvec );

	return(0);		/* OK */
}

/**
 *			R T _ E B M _ P R I N T
 */
void
rt_ebm_print(register const struct soltab *stp)
{
	register const struct rt_ebm_specific *ebmp =
		(struct rt_ebm_specific *)stp->st_specific;

	bu_log("ebm file = %s\n", ebmp->ebm_i.file );
	bu_log("dimensions = (%d, %d, %g)\n",
		ebmp->ebm_i.xdim, ebmp->ebm_i.ydim,
		ebmp->ebm_i.tallness );
	VPRINT("model cellsize", ebmp->ebm_cellsize);
	VPRINT("model grid origin", ebmp->ebm_origin);
}

/**
 *			R T _ E B M _ S H O T
 *
 *  Intersect a ray with an extruded bitmap.
 *  If intersection occurs, a pointer to a sorted linked list of
 *  "struct seg"s will be returned.
 *
 *  Returns -
 *	0	MISS
 *	>0	HIT
 */
int
rt_ebm_shot(struct soltab *stp, register struct xray *rp, struct application *ap, struct seg *seghead)
{
	register struct rt_ebm_specific *ebmp =
		(struct rt_ebm_specific *)stp->st_specific;
	vect_t		norm;
	struct xray	ideal_ray;
	struct seg	myhead;
	int		i;

	BU_LIST_INIT( &(myhead.l) );

	/* Transform actual ray into ideal space at origin in X-Y plane */
	MAT4X3PNT( ideal_ray.r_pt, ebmp->ebm_mat, rp->r_pt );
	MAT4X3VEC( ideal_ray.r_dir, ebmp->ebm_mat, rp->r_dir );

#if 0
bu_log("%g %g %g %g %g %g\n",
ideal_ray.r_pt[X], ideal_ray.r_pt[Y], ideal_ray.r_pt[Z],
ideal_ray.r_dir[X], ideal_ray.r_dir[Y], ideal_ray.r_dir[Z] );
#endif
	if( rt_ebm_dda( &ideal_ray, stp, ap, &myhead ) <= 0 )
		return(0);

	VSET( norm, 0, 0, -1 );		/* letters grow in +z, which is "inside" the halfspace */
	i = rt_seg_planeclip( seghead, &myhead, norm, 0.0, ebmp->ebm_i.tallness,
		&ideal_ray, ap );
#if 0
	if( segp )  {
		vect_t	a, b;
		/* Plot where WE think the ray goes */
		VJOIN1( a, rp->r_pt, segp->seg_in.hit_dist, rp->r_dir );
		VJOIN1( b, rp->r_pt, segp->seg_out.hit_dist, rp->r_dir );
		pl_color( stdout, 0, 0, 255 );	/* B */
		pdv_3line( stdout, a, b );
	}
#endif
	return(i);
}

/**
 *			R T _ E B M _ N O R M
 *
 *  Given one ray distance, return the normal and
 *  entry/exit point.
 *  This is mostly a matter of translating the stored
 *  code into the proper normal.
 */
void
rt_ebm_norm(register struct hit *hitp, struct soltab *stp, register struct xray *rp)
{
	register struct rt_ebm_specific *ebmp =
		(struct rt_ebm_specific *)stp->st_specific;

	VJOIN1( hitp->hit_point, rp->r_pt, hitp->hit_dist, rp->r_dir );

	switch( hitp->hit_surfno )  {
	case NORM_XPOS:
		VMOVE( hitp->hit_normal, ebmp->ebm_xnorm );
		break;
	case NORM_XNEG:
		VREVERSE( hitp->hit_normal, ebmp->ebm_xnorm );
		break;

	case NORM_YPOS:
		VMOVE( hitp->hit_normal, ebmp->ebm_ynorm );
		break;
	case NORM_YNEG:
		VREVERSE( hitp->hit_normal, ebmp->ebm_ynorm );
		break;

	case NORM_ZPOS:
		VMOVE( hitp->hit_normal, ebmp->ebm_znorm );
		break;
	case NORM_ZNEG:
		VREVERSE( hitp->hit_normal, ebmp->ebm_znorm );
		break;

	default:
		bu_log("ebm_norm(%s): surfno=%d bad\n",
			stp->st_name, hitp->hit_surfno );
		VSETALL( hitp->hit_normal, 0 );
		break;
	}
}

/**
 *			R T _ E B M _ C U R V E
 *
 *  Everything has sharp edges.  This makes things easy.
 */
void
rt_ebm_curve(register struct curvature *cvp, register struct hit *hitp, struct soltab *stp)
{
/*	register struct rt_ebm_specific *ebmp =
		(struct rt_ebm_specific *)stp->st_specific; */

	bn_vec_ortho( cvp->crv_pdir, hitp->hit_normal );
	cvp->crv_c1 = cvp->crv_c2 = 0;
}

/**
 *			R T _ E B M _ U V
 *
 *  Map the hit point in 2-D into the range 0..1
 *  untransformed X becomes U, and Y becomes V.
 */
void
rt_ebm_uv(struct application *ap, struct soltab *stp, register struct hit *hitp, register struct uvcoord *uvp)
{
	uvp->uv_u = hitp->hit_vpriv[X];
	uvp->uv_v = hitp->hit_vpriv[Y];

	/* XXX should compute this based upon footprint of ray in ebm space */
	uvp->uv_du = 0.0;
	uvp->uv_dv = 0.0;
}

/**
 * 			R T _ E B M _ F R E E
 */
void
rt_ebm_free(struct soltab *stp)
{
	register struct rt_ebm_specific *ebmp =
		(struct rt_ebm_specific *)stp->st_specific;

	BU_CK_MAPPED_FILE(ebmp->ebm_i.mp);
	bu_close_mapped_file(ebmp->ebm_i.mp);

	bu_free( (char *)ebmp, "rt_ebm_specific" );
}

int
rt_ebm_class(void)
{
	return(0);
}

/**
 *			R T _ E B M _ P L O T
 */
int
rt_ebm_plot(struct bu_list *vhead, struct rt_db_internal *ip, const struct rt_tess_tol *ttol, const struct bn_tol *tol)
{
	register struct rt_ebm_internal *eip;
	register int	x,y;
	register int	following;
	register int	base;
/*	int		i; */

	RT_CK_DB_INTERNAL(ip);
	eip = (struct rt_ebm_internal *)ip->idb_ptr;
	RT_EBM_CK_MAGIC(eip);

	/* Find vertical lines */
	base = 0;	/* lint */
	for( x=0; x <= eip->xdim; x++ )  {
		following = 0;
		for( y=0; y <= eip->ydim; y++ )  {
			if( following )  {
				if( (BIT( eip, x-1, y )==0) != (BIT( eip, x, y )==0) )
					continue;
				rt_ebm_plate( x, base, x, y, eip->tallness,
					eip->mat, vhead );
				following = 0;
			} else {
				if( (BIT( eip, x-1, y )==0) == (BIT( eip, x, y )==0) )
					continue;
				following = 1;
				base = y;
			}
		}
	}

	/* Find horizontal lines */
	for( y=0; y <= eip->ydim; y++ )  {
		following = 0;
		for( x=0; x <= eip->xdim; x++ )  {
			if( following )  {
				if( (BIT( eip, x, y-1 )==0) != (BIT( eip, x, y )==0) )
					continue;
				rt_ebm_plate( base, y, x, y, eip->tallness,
					eip->mat, vhead );
				following = 0;
			} else {
				if( (BIT( eip, x, y-1 )==0) == (BIT( eip, x, y )==0) )
					continue;
				following = 1;
				base = x;
			}
		}
	}
	return(0);
}

/* either x1==x2, or y1==y2 */
void
rt_ebm_plate(int x1, int y1, int x2, int y2, double t, register fastf_t *mat, register struct bu_list *vhead)
{
	LOCAL point_t	s, p;
	LOCAL point_t	srot, prot;

	VSET( s, x1, y1, 0.0 );
	MAT4X3PNT( srot, mat, s );
	RT_ADD_VLIST( vhead, srot, BN_VLIST_LINE_MOVE );

	VSET( p, x1, y1, t );
	MAT4X3PNT( prot, mat, p );
	RT_ADD_VLIST( vhead, prot, BN_VLIST_LINE_DRAW );

	VSET( p, x2, y2, t );
	MAT4X3PNT( prot, mat, p );
	RT_ADD_VLIST( vhead, prot, BN_VLIST_LINE_DRAW );

	p[Z] = 0;
	MAT4X3PNT( prot, mat, p );
	RT_ADD_VLIST( vhead, prot, BN_VLIST_LINE_DRAW );

	RT_ADD_VLIST( vhead, srot, BN_VLIST_LINE_DRAW );
}

struct ebm_edge
{
	struct bu_list	l;
	int		x1,y1;
	int		x2,y2;
	int		left;	/* 1=>material to left, 0=>material to right */
	struct vertex	*v;	/* vertex at x1,y1 */
};

/* either x1==x2, or y1==y2 */
static void
rt_ebm_edge(int x1, int y1, int x2, int y2, int left, struct ebm_edge *edges)
{
	struct ebm_edge *new_edge;

	new_edge = (struct ebm_edge *)bu_malloc( sizeof( struct ebm_edge ) , "rt_ebm_tess: new_edge" );

	/* make all edges go from lower values to larger */
	if( y1 < y2 || x1 < x2 )
	{
		new_edge->x1 = x1;
		new_edge->y1 = y1;
		new_edge->x2 = x2;
		new_edge->y2 = y2;
		new_edge->left = left;
	}
	else
	{
		new_edge->x1 = x2;
		new_edge->y1 = y2;
		new_edge->x2 = x1;
		new_edge->y2 = y1;
		new_edge->left = (!left);
	}
	new_edge->v = (struct vertex *)NULL;
	BU_LIST_APPEND( &edges->l , &new_edge->l );
}

static int
rt_ebm_sort_edges(struct ebm_edge *edges)
{
	struct ebm_edge loops;
	int vertical;
	int done;
	int from_x,from_y,to_x,to_y;
	int start_x,start_y;
	int max_loop_length=0;
	int loop_length;

	/* create another list to hold the edges as they are sorted */
	BU_LIST_INIT( &loops.l );

	while( BU_LIST_NON_EMPTY( &edges->l ) )
	{
		struct ebm_edge *start,*next;

		/* look for a vertical edge starting in lower left (smallest x and y ) */
		start = (struct ebm_edge *)NULL;
		next = BU_LIST_FIRST( ebm_edge , &edges->l );
		while( BU_LIST_NOT_HEAD( &next->l , &edges->l ) )
		{
			if( next->x1 != next->x2 )
			{
				next = BU_LIST_PNEXT( ebm_edge , &next->l );
				continue;	/* not a vertical edge */
			}

			if( !start )
				start = next;
			else if( next->x1 < start->x1 || next->y1 < start->y1 )
				start = next;

			next = BU_LIST_PNEXT( ebm_edge , &next->l );
		}

		if( !start )
			rt_bomb( "rt_ebm_tess: rt_ebm_sort_edges: no vertical edges left!\n" );

		/* put starting edge on the loop list */
		BU_LIST_DEQUEUE( &start->l );
		BU_LIST_INSERT( &loops.l , &start->l );

		next = (struct ebm_edge *)NULL;
		vertical = 0; 	/* look for horizontal edge */
		done = 0;
		to_x = start->x2;
		to_y = start->y2;
		from_x = start->x1;
		from_y = start->y1;
		start_x = from_x;
		start_y = from_y;
		loop_length = 1;
		while( !done )
		{
			struct ebm_edge *e,*e_poss[2];
			int poss;

			/* now find an edge that starts where this one stops (at to_x,to_y) */
			poss = 0;
			for( BU_LIST_FOR( e , ebm_edge , &edges->l ) )
			{
				if( (vertical && e->y1 == e->y2) ||
				   (!vertical && e->x1 == e->x2) )
					continue;

				if( (e->x1 == to_x && e->y1 == to_y) ||
				    (e->x2 == to_x && e->y2 == to_y) )
					e_poss[poss++] = e;
				if( poss > 2 )
					rt_bomb( "rt_ebm_tess: rt_ebm_sort_edges: too many edges at one point\n" );
			}

			if( poss == 0 )
				rt_bomb( "rt_ebm_tess: rt_ebm_sort_edges: no edge to continue loop\n" );
			if( poss == 1 )
			{
				next = e_poss[0];
			}
			else
			{
				/* must choose between two possibilities */
				if( vertical )
				{
					if( to_x < from_x )
					{
						if( e_poss[0]->y1 > to_y || e_poss[0]->y2 > to_y )
							next = e_poss[0];
						else
							next = e_poss[1];
					}
					else
					{
						if( e_poss[0]->y1 < to_y || e_poss[0]->y2 < to_y )
							next = e_poss[0];
						else
							next = e_poss[1];
					}
				}
				else
				{
					if( to_y < from_y )
					{
						if( e_poss[0]->x1 < to_x || e_poss[0]->x2 < to_x )
							next = e_poss[0];
						else
							next = e_poss[1];
					}
					else
					{
						if( e_poss[0]->x1 > to_x || e_poss[0]->x2 > to_x )
							next = e_poss[0];
						else
							next = e_poss[1];
					}
				}
			}

			if( next->x2 == to_x && next->y2 == to_y )
			{
				/* reverse direction of edge */
				next->x2 = next->x1;
				next->y2 = next->y1;
				next->x1 = to_x;
				next->y1 = to_y;
				next->left = (!next->left);
			}
			to_x = next->x2;
			to_y = next->y2;
			from_x = next->x1;
			from_y = next->y1;
			loop_length++;

			BU_LIST_DEQUEUE( &next->l );
			BU_LIST_INSERT( &loops.l , &next->l );

			if( to_x == start_x && to_y == start_y )
			{
				if( loop_length > max_loop_length )
					max_loop_length = loop_length;
				done = 1;	/* complete loop */
			}

			if( vertical )
				vertical = 0;
			else
				vertical = 1;
		}
	}

	/* move sorted list back to "edges" */
	BU_LIST_INSERT_LIST( &edges->l , &loops.l );

	return( max_loop_length );
}

/**
 *			R T _ E B M _ T E S S
 */
int
rt_ebm_tess(struct nmgregion **r, struct model *m, struct rt_db_internal *ip, const struct rt_tess_tol *ttol, const struct bn_tol *tol)
{
	struct rt_ebm_internal	*eip;
	struct shell	*s;
	struct faceuse	*fu=(struct faceuse*)NULL;
	register int	i;
	struct vertex	***vertp;	/* dynam array of ptrs to pointers */
	struct vertex	**loop_verts;
	struct ebm_edge	edges;		/* list of edges */
	struct ebm_edge *e,*start_loop;
	int		start,x,y,left;
	int		max_loop_length;
	int		loop_length;
	vect_t		height,h;

	BN_CK_TOL( tol );
	NMG_CK_MODEL( m );

	RT_CK_DB_INTERNAL(ip);
	eip = (struct rt_ebm_internal *)ip->idb_ptr;
	RT_EBM_CK_MAGIC(eip);

	BU_LIST_INIT( &edges.l );

	x = 0;
	while( x <= eip->xdim )
	{
		y = 0;
		while( y <= eip->ydim )
		{
			if( (BIT( eip , x-1 , y ) == 0 ) != (BIT( eip , x , y ) == 0 ) )
			{
				/* a vertical edge starts here */
				start = y;
				left = (BIT( eip , x , y ) != 0 );

				/* find other end */
				while( (BIT( eip , x-1 , y ) == 0 ) != (BIT( eip , x , y ) == 0 ) &&
					(BIT( eip , x , y ) != 0 ) == left )
						y++;
				rt_ebm_edge( x , start , x , y , left , &edges );
			}
			else
				y++;
		}
		x++;
	}

	y = 0;
	while( y <= eip->ydim )
	{
		x = 0;
		while( x <= eip->xdim )
		{
			if( (BIT( eip, x, y-1 )==0) != (BIT( eip, x, y )==0) )
			{
				/* a horizontal edge starts here */
				start = x;
				left = (BIT( eip , x , y-1 ) != 0 );

				/* find other end */
				while( (BIT( eip, x, y-1 )==0) != (BIT( eip, x, y )==0) &&
					(BIT( eip , x , y-1 ) != 0 ) == left )
						x++;
				rt_ebm_edge( start , y , x , y , left , &edges );
			}
			else
				x++;
		}
		y++;
	}

	/* Sort edges into loops */
	max_loop_length = rt_ebm_sort_edges( &edges );

	/* Make NMG structures */

	/* make region, shell, vertex */
	*r = nmg_mrsv( m );
	s = BU_LIST_FIRST(shell, &(*r)->s_hd);


	vertp = (struct vertex ***)bu_calloc( max_loop_length , sizeof( struct vertex **) ,
		"rt_ebm_tess: vertp" );
	loop_verts = (struct vertex **)bu_calloc( max_loop_length , sizeof( struct vertex *),
		"rt_ebm_tess: loop_verts" );

	e = BU_LIST_FIRST( ebm_edge , &edges.l );
	while( BU_LIST_NOT_HEAD( &e->l , &edges.l ) )
	{
		start_loop = e;
		loop_length = 0;
		vertp[loop_length++] = &start_loop->v;

		e = BU_LIST_PNEXT( ebm_edge , &start_loop->l );
		while( BU_LIST_NOT_HEAD( &e->l , &edges.l ) )
		{
			vertp[loop_length++] = &e->v;
			if( e->x2 == start_loop->x1 && e->y2 == start_loop->y1 )
			{
				struct faceuse *fu1;
				struct ebm_edge *e1;
				point_t pt_ebm,pt_model;
				int done=0;

				if( e->left )
				{
					/* make a face */
					fu1 = nmg_cmface( s , vertp , loop_length );
					NMG_CK_FACEUSE( fu1 );

					/* assign geometry to the vertices used in this face */
					e1 = start_loop;
					while( !done )
					{
						if( e1->v )
						{
							VSET( pt_ebm , e1->x1 , e1->y1 , 0.0 );
							MAT4X3PNT( pt_model , eip->mat , pt_ebm );
							nmg_vertex_gv( e1->v , pt_model );
						}
						if( e1 == e )
							done = 1;
						else
							e1 = BU_LIST_PNEXT( ebm_edge , &e1->l );
					}

					/* assign face geometry */
					if( nmg_fu_planeeqn( fu1 , tol ) )
						goto fail;

					if( !fu )
						fu = fu1;
				}
				else
				{
					/* make a hole */
					for( i=0 ; i<loop_length ; i++ )
					{
						if( *vertp[loop_length-i-1] )
							loop_verts[i] = (*vertp[loop_length-i-1]);
						else
							loop_verts[i] = (struct vertex *)NULL;
					}

					(void) nmg_add_loop_to_face( s , fu , loop_verts ,
							loop_length , OT_OPPOSITE );

					/* Assign geometry to new vertices */
					done = 0;
					e1 = start_loop;
					i = loop_length - 1;
					while( !done )
					{
						if( !loop_verts[i]->vg_p )
						{
							VSET( pt_ebm , e1->x1 , e1->y1 , 0.0 );
							MAT4X3PNT( pt_model , eip->mat , pt_ebm );
							nmg_vertex_gv( loop_verts[i] , pt_model );
							e1->v = loop_verts[i];
						}
						if( e1 == e )
							done = 1;
						else
						{
							e1 = BU_LIST_PNEXT( ebm_edge , &e1->l );
							i--;
						}
					}
				}
				break;
			}
			e = BU_LIST_PNEXT( ebm_edge , &e->l );
		}
		e = BU_LIST_PNEXT( ebm_edge , &e->l );
	}

	/* all faces should merge into one */
	nmg_shell_coplanar_face_merge( s , tol , 1 );

	fu = BU_LIST_FIRST( faceuse , &s->fu_hd );
	NMG_CK_FACEUSE( fu );

	VSET( h , 0.0 , 0.0 , eip->tallness );
	MAT4X3VEC( height , eip->mat , h );

	nmg_extrude_face( fu , height , tol );

	nmg_region_a( *r , tol );

	(void)nmg_mark_edges_real( &s->l.magic );

	bu_free( (char *)vertp , "rt_ebm_tess: vertp" );
	bu_free( (char *)loop_verts , "rt_ebm_tess: loop_verts" );
	while( BU_LIST_NON_EMPTY( &edges.l ) )
	{
		e = BU_LIST_FIRST( ebm_edge , &edges.l );
		BU_LIST_DEQUEUE( &e->l );
		bu_free( (char *)e , "rt_ebm_tess: e" );
	}
	return( 0 );

fail:
	bu_free( (char *)vertp , "rt_ebm_tess: vertp" );
	bu_free( (char *)loop_verts , "rt_ebm_tess: loop_verts" );
	while( BU_LIST_NON_EMPTY( &edges.l ) )
	{
		e = BU_LIST_FIRST( ebm_edge , &edges.l );
		BU_LIST_DEQUEUE( &e->l );
		bu_free( (char *)e , "rt_ebm_tess: e" );
	}

	return(-1);
}

/******** Test Driver *********/
#ifdef TEST_DRIVER

FILE			*plotfp;

struct soltab		Tsolid;
struct directory	Tdir;
struct rt_ebm_specific	*bmsp;
struct resource		resource;
mat_t			Tmat;
struct application	Tappl;

main( int argc, char * *argv )
{
	point_t	pt1;
	point_t	pt2;
	register int	x, y;
	fastf_t		xx, yy;
	mat_t		mat;
	register struct rt_ebm_specific	*ebmp;
	int		arg;
	FILE		*fp;
	union record	rec;


	if( argc > 1 )  {
		rt_g.debug |= DEBUG_EBM;
		arg = atoi(argv[1]);
	}

	plotfp = fopen( "ebm.pl", "w" );

	RT_DIR_SET_NAMEP(&Tdir, "Tsolid");
	Tsolid.st_dp = &Tdir;

	RT_APPLICATION_INIT(&Tappl);

	Tappl.a_purpose = "testing";
	Tappl.a_resource = &resource;
	Tsolid.st_matp = &Tmat;
	MAT_IDN( Tsolid.st_matp );

	strcpy( rec.ss.ss_keyword, "ebm" );
	strcpy( rec.ss.ss_args, "file=bm.bw w=6 n=6 d=6.0" );

	if( rt_ebm_prep( &Tsolid, &rec, 0 ) != 0 )  {
		printf("prep failed\n");
		exit(1);
	}
	rt_ebm_print( &Tsolid );
	ebmp = bmsp = (struct rt_ebm_specific *)Tsolid.st_specific;

	outline( Tsolid.st_matp, &rec );

#if 1
	if( (fp = fopen("ebm.rays", "r")) == NULL )  {
		perror("ebm.rays");
		exit(1);
	}
	for(;;)  {
		x = fscanf( fp, "%lf %lf %lf %lf %lf %lf\n",
			&pt1[X], &pt1[Y], &pt1[Z],
			&pt2[X], &pt2[Y], &pt2[Z] );
		if( x < 6 )  break;
		VADD2( pt2, pt2, pt1 );
		trial( pt1, pt2 );
	}
#endif
#if 0
	y = arg;
	for( x=0; x<=ebmp->ebm_i.xdim; x++ )  {
		VSET( pt1, 0, y, 1 );
		VSET( pt2, x, 0, 1 );
		trial( pt1, pt2 );
	}
#endif
#if 0
	y = arg;
	for( x=0; x<=ebmp->ebm_i.xdim; x++ )  {
		VSET( pt1, 0, y, 2 );
		VSET( pt2, x, 0, 4 );
		trial( pt1, pt2 );
	}
#endif
#if 0
	for( y=0; y<=ebmp->ebm_i.ydim; y++ )  {
		for( x=0; x<=ebmp->ebm_i.xdim; x++ )  {
			VSET( pt1, 0, y, 2 );
			VSET( pt2, x, 0, 4 );
			trial( pt1, pt2 );
		}
	}
#endif
#if 0
	for( y= -1; y<=ebmp->ebm_i.ydim; y++ )  {
		for( x= -1; x<=ebmp->ebm_i.xdim; x++ )  {
			VSET( pt1, x, y, 10 );
			VSET( pt2, x+2, y+3, -4 );
			trial( pt1, pt2 );
		}
	}
#endif
#if 0
	for( y=0; y<=ebmp->ebm_i.ydim; y++ )  {
		for( x=0; x<=ebmp->ebm_i.xdim; x++ )  {
			VSET( pt1, ebmp->ebm_i.xdim, y, 2 );
			VSET( pt2, x, ebmp->ebm_i.ydim, 4 );
			trial( pt1, pt2 );
		}
	}
#endif

#if 0
	for( yy = 2.0; yy < 6.0; yy += 0.3 )  {
		VSET( pt1, 0, yy, 2 );
		VSET( pt2, 6, 0, 4 );
		trial( pt1, pt2 );
	}
#endif
#if 0
	for( yy=0; yy<=ebmp->ebm_i.ydim; yy += 0.3 )  {
		for( xx=0; xx<=ebmp->ebm_i.xdim; xx += 0.3 )  {
			VSET( pt1, 0, yy, 2 );
			VSET( pt2, xx, 0, 4 );
			trial( pt1, pt2 );
		}
	}
#endif
#if 0
	for( yy=0; yy<=ebmp->ebm_i.ydim; yy += 0.3 )  {
		for( xx=0; xx<=ebmp->ebm_i.xdim; xx += 0.3 )  {
			VSET( pt1, ebmp->ebm_i.xdim, yy, 2 );
			VSET( pt2, xx, ebmp->ebm_i.ydim, 4 );
			trial( pt1, pt2 );
		}
	}
#endif

#if 0
	/* (6,5) (2,5) (3,4) (1.2,1.2)
	 * were especially troublesome */
	xx=6;
	yy=1.5;
	VSET( pt1, 0, yy, 2 );
	VSET( pt2, xx, 0, 4 );
	trial( pt1, pt2 );
#endif

#if 0
	/* (1,2) (3,2) (0,2) (0,0.3)
	 * were especially troublesome */
	xx=0;
	yy=0.3;
	VSET( pt1, ebmp->ebm_i.xdim, yy, 2 );
	VSET( pt2, xx, ebmp->ebm_i.ydim, 4 );
	trial( pt1, pt2 );
#endif

#if 0
	VSET( pt1, 0, 1.5, 2 );
	VSET( pt2, 4.75, 6, 4 );
	trial( pt1, pt2 );

	VSET( pt1, 0, 2.5, 2 );
	VSET( pt2, 4.5, 0, 4 );
	trial( pt1, pt2 );
#endif


#if 0
	/* With Z=-10, it works, but looks like trouble, due to
	 * the Z-clipping causing lots of green vectors on the 2d proj.
	 * VSET( pt1, 0.75, 1.1, -10 );
	 */
	VSET( pt1, 0.75, 1.1, 0 );
	{
		for( yy=0; yy<=ebmp->ebm_i.ydim; yy += 0.3 )  {
			for( xx=0; xx<=ebmp->ebm_i.xdim; xx += 0.3 )  {
				VSET( pt2, xx, yy, 4 );
				trial( pt1, pt2 );
			}
		}
	}
#endif
#if 0
	for( x=0; x<ebmp->ebm_i.xdim; x++ )  {
		VSET( pt1, x+0.75, 1.1, -10 );
		VSET( pt2, x+0.75, 1.1, 4 );
		trial( pt1, pt2 );
	}
#endif

	exit(0);
}

trial(p1, p2)
vect_t	p1, p2;
{
	struct seg	*segp, *next;
	fastf_t		lastk;
	struct xray	ray;
	register struct rt_ebm_specific *ebmp = bmsp;

	VMOVE( ray.r_pt, p1 );
	VSUB2( ray.r_dir, p2, p1 );
	if( MAGNITUDE( ray.r_dir ) < 1.0e-10 )
		return;
	VUNITIZE( ray.r_dir );

	printf("------- (%g, %g, %g) to (%g, %g, %g), dir=(%g, %g, %g)\n",
		ray.r_pt[X], ray.r_pt[Y], ray.r_pt[Z],
		p2[X], p2[Y], p2[Z],
		ray.r_dir[X], ray.r_dir[Y], ray.r_dir[Z] );


	segp = rt_ebm_shot( &Tsolid, &ray, &Tappl );

	lastk = 0;
	while( segp != SEG_NULL )  {
		/* Draw 2-D segments */
		draw2seg( ray.r_pt, ray.r_dir, lastk, segp->seg_in.hit_dist, 0 );
		draw2seg( ray.r_pt, ray.r_dir, segp->seg_in.hit_dist, segp->seg_out.hit_dist, 1 );
		lastk = segp->seg_out.hit_dist;

		draw3seg( segp, ray.r_pt, ray.r_dir );

		next = segp->seg_next;
		FREE_SEG(segp, Tappl.a_resource);
		segp = next;
	}
}

outline(mat, rp)
mat_t	mat;
union record	*rp;
{
	register struct rt_ebm_specific *ebmp = bmsp;
	register struct vlist	*vp;
	struct vlhead	vhead;

	vhead.vh_first = vhead.vh_last = VL_NULL;

	pl_3space( plotfp, -BIT_XWIDEN,-BIT_YWIDEN,-BIT_XWIDEN,
		 ebmp->ebm_i.xdim+BIT_XWIDEN, ebmp->ebm_i.ydim+BIT_YWIDEN, (int)(ebmp->ebm_i.tallness+1.99) );
	pl_3box( plotfp, -BIT_XWIDEN,-BIT_YWIDEN,-BIT_XWIDEN,
		 ebmp->ebm_i.xdim+BIT_XWIDEN, ebmp->ebm_i.ydim+BIT_YWIDEN, (int)(ebmp->ebm_i.tallness+1.99) );

	/* Get vlist, then just draw the vlist */
	rt_ebm_plot( rp, mat, &vhead, 0 );

	for( vp = vhead.vh_first; vp != VL_NULL; vp = vp->vl_forw )  {
		if( vp->vl_draw == 0 )
			pdv_3move( plotfp, vp->vl_pnt );
		else
			pdv_3cont( plotfp, vp->vl_pnt );
	}
	FREE_VL( vhead.vh_first );
}


draw2seg( pt, dir, k1, k2, inside )
vect_t	pt, dir;
fastf_t	k1, k2;
int	inside;
{
	vect_t	a, b;

	a[0] = pt[0] + k1 * dir[0];
	a[1] = pt[1] + k1 * dir[1];
	a[2] = 0;
	b[0] = pt[0] + k2 * dir[0];
	b[1] = pt[1] + k2 * dir[1];
	b[2] = 0;

	if( inside )
		pl_color( plotfp, 255, 0, 0 );	/* R */
	else
		pl_color( plotfp, 0, 255, 0 );	/* G */
	pdv_3line( plotfp, a, b );
}

draw3seg( segp, pt, dir )
register struct seg	*segp;
vect_t	pt, dir;
{
	vect_t	a, b;

	VJOIN1( a, pt, segp->seg_in.hit_dist, dir );
	VJOIN1( b, pt, segp->seg_out.hit_dist, dir );
	pl_color( plotfp, 0, 0, 255 );	/* B */
	pdv_3line( plotfp, a, b );
}
#endif /* test driver */

/**
 *		R T _ E B M _ T C L G E T
 *
 *	Routine to format the parameters of an EBM for "db get"
 *
 *	Legal requested parameters are:
 *		"F" - bitmap file to extrude
 *		"W" - number of cells in X direction
 *		"N" - number of cells in Y direction
 *		"H" - height of each cell (mm)
 *		"M" - matrix to transform EBM solid into model coordinates
 *
 *	no paramaters requested returns all
 */
int
rt_ebm_tclget(Tcl_Interp *interp, const struct rt_db_internal *intern, const char *attr)
{
	register struct rt_ebm_internal *ebm=(struct rt_ebm_internal *)intern->idb_ptr;
	Tcl_DString	ds;
	struct bu_vls	vls;
	int		i;

	RT_EBM_CK_MAGIC( ebm );

	Tcl_DStringInit( &ds );
	bu_vls_init( &vls );

	if( attr == (char *)NULL ) {
		bu_vls_strcpy( &vls, "ebm" );
		bu_vls_printf( &vls, " F %s W %d N %d H %.25g",
			       ebm->file, ebm->xdim, ebm->ydim, ebm->tallness );
		bu_vls_printf( &vls, " M {" );
		for( i=0 ; i<16 ; i++ )
			bu_vls_printf( &vls, " %.25g", ebm->mat[i] );
		bu_vls_printf( &vls, " }" );
	}
	else if( !strcmp( attr, "F" ) )
		bu_vls_printf( &vls, "%s", ebm->file );
	else if( !strcmp( attr, "W" ) )
		bu_vls_printf( &vls, "%d", ebm->xdim );
	else if( !strcmp( attr, "N" ) )
		bu_vls_printf( &vls, "%d", ebm->ydim );
	else if( !strcmp( attr, "H" ) )
		bu_vls_printf( &vls, "%.25g", ebm->tallness );
	else if( !strcmp( attr, "M" ) ) {
		for( i=0 ; i<16 ; i++ )
			bu_vls_printf( &vls, "%.25g ", ebm->mat[i] );
	}
	else {
		Tcl_SetResult( interp,"ERROR: Unknown attribute, choices are F, W, N, or H\n",
		TCL_STATIC );
		bu_vls_free( &vls );
		return( TCL_ERROR );
	}

	Tcl_DStringAppend( &ds, bu_vls_addr( &vls ), -1 );
	Tcl_DStringResult( interp, &ds );
	Tcl_DStringFree( &ds );
	bu_vls_free( &vls );
	return( TCL_OK );
}


/**
 *		R T _ E B M _ T C L A D J U S T
 *
 *	Routine to adjust the parameters of an EBM
 *
 *	Legal parameters are:
 *		"F" - bitmap file to extrude
 *		"W" - number of cells in X direction
 *		"N" - number of cells in Y direction
 *		"H" - height of each cell (mm)
 *		"M" - matrix to transform EBM solid into model coordinates
 */

int
rt_ebm_tcladjust(Tcl_Interp *interp, struct rt_db_internal *intern, int argc, char **argv)
{
	struct rt_ebm_internal *ebm;

	RT_CK_DB_INTERNAL( intern );

	ebm = (struct rt_ebm_internal *)intern->idb_ptr;
	RT_EBM_CK_MAGIC( ebm );

	while( argc >= 2 ) {
		if( !strcmp( argv[0], "F" ) ) {
			if( strlen( argv[1] ) >= RT_EBM_NAME_LEN ) {
				Tcl_SetResult( interp,
					       "ERROR: File name too long",
					       TCL_STATIC );
				return( TCL_ERROR );
			}
			strcpy( ebm->file, argv[1] );
		}
		else if( !strcmp( argv[0], "W" ) ) {
			ebm->xdim = atoi( argv[1] );
		}
		else if( !strcmp( argv[0], "N" ) ) {
			ebm->ydim = atoi( argv[1] );
		}
		else if( !strcmp( argv[0], "H" ) ) {
			ebm->tallness = atof( argv[1] );
		}
		else if( !strcmp( argv[0], "M" ) ) {
			int len=16;
			fastf_t array[16];
			fastf_t *ar_ptr;

			ar_ptr = array;

			if( tcl_list_to_fastf_array( interp, argv[1], &ar_ptr, &len) !=
			    len ) {
				Tcl_SetResult( interp,
				      "ERROR: incorrect number of coefficents for matrix\n",
				      TCL_STATIC );
				return( TCL_ERROR );
			}
			MAT_COPY( ebm->mat, array )
		}
		else {
			Tcl_SetResult( interp,
			      "ERROR: illegal argument, choices are F, W, N, or H\n",
			      TCL_STATIC );
			return( TCL_ERROR );
		}
		argc -= 2;
		argv += 2;
	}
	return( TCL_OK );
}

int
rt_ebm_tclform( const struct rt_functab *ftp, Tcl_Interp *interp )
{
        RT_CK_FUNCTAB(ftp);

        Tcl_AppendResult( interp,
			  "F %s W %d N %d H %f M { %f %f %f %f %f %f %f %f %f %f %f %f %f %f %f %f", (char *)NULL );

        return TCL_OK;

}


/**
 *		R T _ E B M _ M A K E
 *
 *	Routine to make a new EBM solid. The only purpose of this routine is
 *	to initialize the matrix and height to legal values.
 */
void
rt_ebm_make(const struct rt_functab *ftp, struct rt_db_internal *intern, double diameter)
{
	struct rt_ebm_internal *ebm;

	intern->idb_major_type = DB5_MAJORTYPE_BRLCAD;
	intern->idb_type = ID_EBM;
	BU_ASSERT(&rt_functab[intern->idb_type] == ftp);

	intern->idb_meth = ftp;
	ebm = (struct rt_ebm_internal *)bu_calloc( sizeof( struct rt_ebm_internal ), 1,
				     "rt_ebm_internal");
	intern->idb_ptr = (genptr_t)ebm;
	ebm->magic = RT_EBM_INTERNAL_MAGIC;
	MAT_IDN( ebm->mat );
	ebm->tallness = 1.0;
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
