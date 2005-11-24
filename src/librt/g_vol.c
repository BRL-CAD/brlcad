/*                         G _ V O L . C
 * BRL-CAD
 *
 * Copyright (C) 1989-2005 United States Government as represented by
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
/** @file g_vol.c
 *	Intersect a ray with a 3-D volume.
 *	The volume is described as a concatenation of
 *	bw(5) files.
 *
 *  Authors -
 *	Michael John Muuss
 *	Phil Dykstra
 *
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005
 *
 */
/*@}*/

#ifndef lint
static const char RCSvol[] = "@(#)$Header$ (BRL)";
#endif

#include "common.h"



#include <stdio.h>
#include <ctype.h>
#include <math.h>
#include <string.h>
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
	Changed small to small1 for win32 compatibility
*/


struct rt_vol_specific {
	struct rt_vol_internal vol_i;
	vect_t		vol_xnorm;	/* local +X norm in model coords */
	vect_t		vol_ynorm;
	vect_t		vol_znorm;
	mat_t		vol_mat;	/* model to ideal space */
	vect_t		vol_origin;	/* local coords of grid origin (0,0,0) for now */
	vect_t		vol_large;	/* local coords of XYZ max */
};
#define VOL_NULL	((struct rt_vol_specific *)0)

#define VOL_O(m)	offsetof(struct rt_vol_internal, m)

const struct bu_structparse rt_vol_parse[] = {
#if CRAY && !__STDC__
	{"%s",	RT_VOL_NAME_LEN, "file",	1,		BU_STRUCTPARSE_FUNC_NULL },
#else
	{"%s",	RT_VOL_NAME_LEN, "file",	bu_offsetofarray(struct rt_vol_internal, file), BU_STRUCTPARSE_FUNC_NULL },
#endif
	{"%d",	1, "w",		VOL_O(xdim),	BU_STRUCTPARSE_FUNC_NULL },
	{"%d",	1, "n",		VOL_O(ydim),	BU_STRUCTPARSE_FUNC_NULL },
	{"%d",	1, "d",		VOL_O(zdim),	BU_STRUCTPARSE_FUNC_NULL },
	{"%d",	1, "lo",	VOL_O(lo),		BU_STRUCTPARSE_FUNC_NULL },
	{"%d",	1, "hi",	VOL_O(hi),		BU_STRUCTPARSE_FUNC_NULL },
	{"%f",	ELEMENTS_PER_VECT, "size",bu_offsetofarray(struct rt_vol_internal, cellsize), BU_STRUCTPARSE_FUNC_NULL },
	{"%f",	16, "mat", bu_offsetofarray(struct rt_vol_internal,mat), BU_STRUCTPARSE_FUNC_NULL },
	{"",	0, (char *)0,	0,			BU_STRUCTPARSE_FUNC_NULL }
};

BU_EXTERN(void rt_vol_plate,(point_t a, point_t b, point_t c, point_t d,
	mat_t mat, struct bu_list *vhead, struct rt_vol_internal *vip));

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
#define	VOL_XWIDEN	2
#define	VOL_YWIDEN	2
#define	VOL_ZWIDEN	2
#define VOL(_vip,_xx,_yy,_zz)	(_vip)->map[ \
	(((_zz)+VOL_ZWIDEN) * ((_vip)->ydim + VOL_YWIDEN*2)+ \
	 ((_yy)+VOL_YWIDEN))* ((_vip)->xdim + VOL_XWIDEN*2)+ \
	  (_xx)+VOL_XWIDEN ]

#define OK(_vip,_v)	( (int)(_v) >= (_vip)->lo && (int)(_v) <= (_vip)->hi )

static int rt_vol_normtab[3] = { NORM_XPOS, NORM_YPOS, NORM_ZPOS };


/**
 *			R T _ V O L _ S H O T
 *
 *  Transform the ray into local coordinates of the volume ("ideal space").
 *  Step through the 3-D array, in local coordinates.
 *  Return intersection segments.
 *
 */
int
rt_vol_shot(struct soltab *stp, register struct xray *rp, struct application *ap, struct seg *seghead)
{
	register struct rt_vol_specific *volp =
		(struct rt_vol_specific *)stp->st_specific;
	vect_t	invdir;
	double	t0;	/* in point of cell */
	double	t1;	/* out point of cell */
	double	tmax;	/* out point of entire grid */
	vect_t	t;	/* next t value for XYZ cell plane intersect */
	vect_t	delta;	/* spacing of XYZ cell planes along ray */
	int	igrid[3];/* Grid cell coordinates of cell (integerized) */
	vect_t	P;	/* hit point */
	int	inside;	/* inside/outside a solid flag */
	int	in_axis;
	int	out_axis;
	int	j;
	struct xray	ideal_ray;

	/* Transform actual ray into ideal space at origin in X-Y plane */
	MAT4X3PNT( ideal_ray.r_pt, volp->vol_mat, rp->r_pt );
	MAT4X3VEC( ideal_ray.r_dir, volp->vol_mat, rp->r_dir );
	rp = &ideal_ray;	/* XXX */

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
	if( ! rt_in_rpp(rp, invdir, P, volp->vol_large ) )
		return	0;	/* MISS */
	VJOIN1( P, rp->r_pt, rp->r_min, rp->r_dir );	/* P is hit point */
if(RT_G_DEBUG&DEBUG_VOL)VPRINT("vol_large", volp->vol_large);
if(RT_G_DEBUG&DEBUG_VOL)VPRINT("vol_origin", volp->vol_origin);
if(RT_G_DEBUG&DEBUG_VOL)VPRINT("r_pt", rp->r_pt);
if(RT_G_DEBUG&DEBUG_VOL)VPRINT("P", P);
if(RT_G_DEBUG&DEBUG_VOL)VPRINT("cellsize", volp->vol_i.cellsize);
	t0 = rp->r_min;
	tmax = rp->r_max;
if(RT_G_DEBUG&DEBUG_VOL)bu_log("[shoot: r_min=%g, r_max=%g]\n", rp->r_min, rp->r_max);

	/* find grid cell where ray first hits ideal space bounding RPP */
	igrid[X] = (P[X] - volp->vol_origin[X]) / volp->vol_i.cellsize[X];
	igrid[Y] = (P[Y] - volp->vol_origin[Y]) / volp->vol_i.cellsize[Y];
	igrid[Z] = (P[Z] - volp->vol_origin[Z]) / volp->vol_i.cellsize[Z];
	if( igrid[X] < 0 )  {
		igrid[X] = 0;
	} else if( igrid[X] >= volp->vol_i.xdim ) {
		igrid[X] = volp->vol_i.xdim-1;
	}
	if( igrid[Y] < 0 )  {
		igrid[Y] = 0;
	} else if( igrid[Y] >= volp->vol_i.ydim ) {
		igrid[Y] = volp->vol_i.ydim-1;
	}
	if( igrid[Z] < 0 )  {
		igrid[Z] = 0;
	} else if( igrid[Z] >= volp->vol_i.zdim ) {
		igrid[Z] = volp->vol_i.zdim-1;
	}
if(RT_G_DEBUG&DEBUG_VOL)bu_log("igrid=(%d, %d, %d)\n", igrid[X], igrid[Y], igrid[Z]);

	/* X setup */
	if( rp->r_dir[X] == 0.0 )  {
		t[X] = INFINITY;
		delta[X] = 0;
	} else {
		j = igrid[X];
		if( rp->r_dir[X] < 0 ) j++;
		t[X] = (volp->vol_origin[X] + j*volp->vol_i.cellsize[X] -
			rp->r_pt[X]) * invdir[X];
		delta[X] = volp->vol_i.cellsize[X] * fabs(invdir[X]);
	}
	/* Y setup */
	if( rp->r_dir[Y] == 0.0 )  {
		t[Y] = INFINITY;
		delta[Y] = 0;
	} else {
		j = igrid[Y];
		if( rp->r_dir[Y] < 0 ) j++;
		t[Y] = (volp->vol_origin[Y] + j*volp->vol_i.cellsize[Y] -
			rp->r_pt[Y]) * invdir[Y];
		delta[Y] = volp->vol_i.cellsize[Y] * fabs(invdir[Y]);
	}
	/* Z setup */
	if( rp->r_dir[Z] == 0.0 )  {
		t[Z] = INFINITY;
		delta[Z] = 0;
	} else {
		j = igrid[Z];
		if( rp->r_dir[Z] < 0 ) j++;
		t[Z] = (volp->vol_origin[Z] + j*volp->vol_i.cellsize[Z] -
			rp->r_pt[Z]) * invdir[Z];
		delta[Z] = volp->vol_i.cellsize[Z] * fabs(invdir[Z]);
	}

	/* The delta[] elements *must* be positive, as t must increase */
if(RT_G_DEBUG&DEBUG_VOL)bu_log("t[X] = %g, delta[X] = %g\n", t[X], delta[X] );
if(RT_G_DEBUG&DEBUG_VOL)bu_log("t[Y] = %g, delta[Y] = %g\n", t[Y], delta[Y] );
if(RT_G_DEBUG&DEBUG_VOL)bu_log("t[Z] = %g, delta[Z] = %g\n", t[Z], delta[Z] );

	/* Find face of entry into first cell -- max initial t value */
	if( t[X] >= t[Y] )  {
		in_axis = X;
		t0 = t[X];
	} else {
		in_axis = Y;
		t0 = t[Y];
	}
	if( t[Z] > t0 )  {
		in_axis = Z;
		t0 = t[Z];
	}
if(RT_G_DEBUG&DEBUG_VOL)bu_log("Entry axis is %s, t0=%g\n", in_axis==X ? "X" : (in_axis==Y?"Y":"Z"), t0);

	/* Advance to next exits */
	t[X] += delta[X];
	t[Y] += delta[Y];
	t[Z] += delta[Z];

	/* Ensure that next exit is after first entrance */
	if( t[X] < t0 )  {
		bu_log("*** advancing t[X]\n");
		t[X] += delta[X];
	}
	if( t[Y] < t0 )  {
		bu_log("*** advancing t[Y]\n");
		t[Y] += delta[Y];
	}
	if( t[Z] < t0 )  {
		bu_log("*** advancing t[Z]\n");
		t[Z] += delta[Z];
	}
if(RT_G_DEBUG&DEBUG_VOL) VPRINT("Exit t[]", t);

	inside = 0;

	while( t0 < tmax ) {
		int	val;
		struct seg	*segp;

		/* find minimum exit t value */
		if( t[X] < t[Y] )  {
			if( t[Z] < t[X] )  {
				out_axis = Z;
				t1 = t[Z];
			} else {
				out_axis = X;
				t1 = t[X];
			}
		} else {
			if( t[Z] < t[Y] )  {
				out_axis = Z;
				t1 = t[Z];
			} else {
				out_axis = Y;
				t1 = t[Y];
			}
		}

		/* Ray passes through cell igrid[XY] from t0 to t1 */
		val = VOL( &volp->vol_i, igrid[X], igrid[Y], igrid[Z] );
if(RT_G_DEBUG&DEBUG_VOL)bu_log("igrid [%d %d %d] from %g to %g, val=%d\n",
			igrid[X], igrid[Y], igrid[Z],
			t0, t1, val );
if(RT_G_DEBUG&DEBUG_VOL)bu_log("Exit axis is %s, t[]=(%g, %g, %g)\n",
			out_axis==X ? "X" : (out_axis==Y?"Y":"Z"),
			t[X], t[Y], t[Z] );

		if( t1 <= t0 )  bu_log("ERROR vol t1=%g < t0=%g\n", t1, t0 );
		if( !inside )  {
			if( OK( &volp->vol_i, val ) )  {
				/* Handle the transition from vacuum to solid */
				/* Start of segment (entering a full voxel) */
				inside = 1;

				RT_GET_SEG(segp, ap->a_resource);
				segp->seg_stp = stp;
				segp->seg_in.hit_dist = t0;

				/* Compute entry normal */
				if( rp->r_dir[in_axis] < 0 )  {
					/* Go left, entry norm goes right */
					segp->seg_in.hit_surfno =
						rt_vol_normtab[in_axis];
				}  else  {
					/* go right, entry norm goes left */
					segp->seg_in.hit_surfno =
						(-rt_vol_normtab[in_axis]);
				}
				BU_LIST_INSERT( &(seghead->l), &(segp->l) );
				if(RT_G_DEBUG&DEBUG_VOL) bu_log("START t=%g, surfno=%d\n",
					t0, segp->seg_in.hit_surfno);
			} else {
				/* Do nothing, marching through void */
			}
		} else {
			if( OK( &volp->vol_i, val ) )  {
				/* Do nothing, marching through solid */
			} else {
				register struct seg	*tail;
				/* End of segment (now in an empty voxel) */
				/* Handle transition from solid to vacuum */
				inside = 0;

				tail = BU_LIST_LAST( seg, &(seghead->l) );
				tail->seg_out.hit_dist = t0;

				/* Compute exit normal */
				if( rp->r_dir[in_axis] < 0 )  {
					/* Go left, exit normal goes left */
					tail->seg_out.hit_surfno =
						(-rt_vol_normtab[in_axis]);
				}  else  {
					/* go right, exit norm goes right */
					tail->seg_out.hit_surfno =
						rt_vol_normtab[in_axis];
				}
				if(RT_G_DEBUG&DEBUG_VOL) bu_log("END t=%g, surfno=%d\n",
					t0, tail->seg_out.hit_surfno );
			}
		}

		/* Take next step */
		t0 = t1;
		in_axis = out_axis;
		t[out_axis] += delta[out_axis];
		if( rp->r_dir[out_axis] > 0 ) {
			igrid[out_axis]++;
		} else {
			igrid[out_axis]--;
		}
	}

	if( inside )  {
		register struct seg	*tail;

		/* Close off the final segment */
		tail = BU_LIST_LAST( seg, &(seghead->l) );
		tail->seg_out.hit_dist = tmax;

		/* Compute exit normal.  Previous out_axis is now in_axis */
		if( rp->r_dir[in_axis] < 0 )  {
			/* Go left, exit normal goes left */
			tail->seg_out.hit_surfno = (-rt_vol_normtab[in_axis]);
		}  else  {
			/* go right, exit norm goes right */
			tail->seg_out.hit_surfno = rt_vol_normtab[in_axis];
		}
		if(RT_G_DEBUG&DEBUG_VOL) bu_log("closed END t=%g, surfno=%d\n",
			tmax, tail->seg_out.hit_surfno );
	}

	if( BU_LIST_IS_EMPTY( &(seghead->l) ) )
		return(0);
	return(2);
}

/**
 *			R T _ V O L _ I M P O R T
 *
 *  Read in the information from the string solid record.
 *  Then, as a service to the application, read in the bitmap
 *  and set up some of the associated internal variables.
 */
int
rt_vol_import(struct rt_db_internal *ip, const struct bu_external *ep, const fastf_t *mat, const struct db_i *dbip)
{
	union record	*rp;
	register struct rt_vol_internal *vip;
	struct bu_vls	str;
	FILE		*fp;
	int		nbytes;
	register int	y;
	register int	z;
	mat_t		tmat;
	int		ret;

	BU_CK_EXTERNAL( ep );
	rp = (union record *)ep->ext_buf;
	if( rp->u_id != DBID_STRSOL )  {
		bu_log("rt_ebm_import: defective strsol record\n");
		return(-1);
	}

	RT_CK_DB_INTERNAL( ip );
	ip->idb_major_type = DB5_MAJORTYPE_BRLCAD;
	ip->idb_type = ID_VOL;
	ip->idb_meth = &rt_functab[ID_VOL];
	ip->idb_ptr = bu_calloc(1, sizeof(struct rt_vol_internal), "rt_vol_internal");
	vip = (struct rt_vol_internal *)ip->idb_ptr;
	vip->magic = RT_VOL_INTERNAL_MAGIC;

	/* Establish defaults */
	MAT_IDN( vip->mat );
	vip->lo = 0;
	vip->hi = 255;

	/* Default VOL cell size in ideal coordinates is one unit/cell */
	VSETALL( vip->cellsize, 1 );

	bu_vls_init( &str );
	bu_vls_strcpy( &str, rp->ss.ss_args );
	if( bu_struct_parse( &str, rt_vol_parse, (char *)vip ) < 0 )  {
		bu_vls_free( &str );
		return -2;
	}
	bu_vls_free( &str );

	/* Check for reasonable values */
	if( vip->file[0] == '\0' || vip->xdim < 1 ||
	    vip->ydim < 1 || vip->zdim < 1 || vip->mat[15] <= 0.0 ||
	    vip->lo < 0 || vip->hi > 255 )  {
	    	bu_struct_print("Unreasonable VOL parameters", rt_vol_parse,
			(char *)vip );
	    	return(-1);
	}

	/* Apply any modeling transforms to get final matrix */
	bn_mat_mul( tmat, mat, vip->mat );
	MAT_COPY( vip->mat, tmat );

	/* Get bit map from .bw(5) file */
	nbytes = (vip->xdim+VOL_XWIDEN*2)*
		(vip->ydim+VOL_YWIDEN*2)*
		(vip->zdim+VOL_ZWIDEN*2);
	vip->map = (unsigned char *)bu_calloc( 1, nbytes, "vol_import bitmap" );

	bu_semaphore_acquire( BU_SEM_SYSCALL );		/* lock */
	if( (fp = fopen(vip->file, "r")) == NULL )  {
		perror(vip->file);
		bu_semaphore_release( BU_SEM_SYSCALL );		/* unlock */
		return(-1);
	}
	bu_semaphore_release( BU_SEM_SYSCALL );		/* unlock */

	/* Because of in-memory padding, read each scanline separately */
	for( z=0; z < vip->zdim; z++ )  {
		for( y=0; y < vip->ydim; y++ )  {
			bu_semaphore_acquire( BU_SEM_SYSCALL );		/* lock */
			ret = fread( &VOL(vip, 0, y, z), vip->xdim, 1, fp ); /* res_syscall */
			bu_semaphore_release( BU_SEM_SYSCALL );		/* unlock */
			if( ret < 1 )  {
				bu_log("rt_vol_import(%s): Unable to read whole VOL, y=%d, z=%d\n",
					vip->file, y, z);
				bu_semaphore_acquire( BU_SEM_SYSCALL );		/* lock */
				fclose(fp);
				bu_semaphore_release( BU_SEM_SYSCALL );		/* unlock */
				return -1;
			}
		}
	}
	bu_semaphore_acquire( BU_SEM_SYSCALL );		/* lock */
	fclose(fp);
	bu_semaphore_release( BU_SEM_SYSCALL );		/* unlock */
	return( 0 );
}

/**
 *			R T _ V O L _ E X P O R T
 *
 *  The name will be added by the caller.
 */
int
rt_vol_export(struct bu_external *ep, const struct rt_db_internal *ip, double local2mm, const struct db_i *dbip)
{
	struct rt_vol_internal	*vip;
	struct rt_vol_internal	vol;	/* scaled version */
	union record		*rec;
	struct bu_vls		str;

	RT_CK_DB_INTERNAL(ip);
	if( ip->idb_type != ID_VOL )  return(-1);
	vip = (struct rt_vol_internal *)ip->idb_ptr;
	RT_VOL_CK_MAGIC(vip);
	vol = *vip;			/* struct copy */

	/* Apply scale factor */
	vol.mat[15] /= local2mm;

	BU_CK_EXTERNAL(ep);
	ep->ext_nbytes = sizeof(union record)*DB_SS_NGRAN;
	ep->ext_buf = (genptr_t)bu_calloc( 1, ep->ext_nbytes, "vol external");
	rec = (union record *)ep->ext_buf;

	bu_vls_init( &str );
	bu_vls_struct_print( &str, rt_vol_parse, (char *)&vol );

	rec->ss.ss_id = DBID_STRSOL;
	strncpy( rec->ss.ss_keyword, "vol", NAMESIZE-1 );
	strncpy( rec->ss.ss_args, bu_vls_addr(&str), DB_SS_LEN-1 );
	bu_vls_free( &str );

	return(0);
}

/**
 *			R T _ V O L _ I M P O R T 5
 *
 *  Read in the information from the string solid record.
 *  Then, as a service to the application, read in the bitmap
 *  and set up some of the associated internal variables.
 */
int
rt_vol_import5(struct rt_db_internal *ip, const struct bu_external *ep, const fastf_t *mat, const struct db_i *dbip)
{
	register struct rt_vol_internal *vip;
	struct bu_vls	str;
	FILE		*fp;
	int		nbytes;
	register int	y;
	register int	z;
	mat_t		tmat;
	int		ret;

	BU_CK_EXTERNAL( ep );

	RT_CK_DB_INTERNAL( ip );
	ip->idb_major_type = DB5_MAJORTYPE_BRLCAD;
	ip->idb_type = ID_VOL;
	ip->idb_meth = &rt_functab[ID_VOL];
	ip->idb_ptr = bu_calloc(1, sizeof(struct rt_vol_internal), "rt_vol_internal");
	vip = (struct rt_vol_internal *)ip->idb_ptr;
	vip->magic = RT_VOL_INTERNAL_MAGIC;

	/* Establish defaults */
	MAT_IDN( vip->mat );
	vip->lo = 0;
	vip->hi = 255;

	/* Default VOL cell size in ideal coordinates is one unit/cell */
	VSETALL( vip->cellsize, 1 );

	bu_vls_init( &str );
	bu_vls_strncpy( &str, ep->ext_buf, ep->ext_nbytes );
	if( bu_struct_parse( &str, rt_vol_parse, (char *)vip ) < 0 )  {
		bu_vls_free( &str );
		return -2;
	}
	bu_vls_free( &str );

	/* Check for reasonable values */
	if( vip->file[0] == '\0' || vip->xdim < 1 ||
	    vip->ydim < 1 || vip->zdim < 1 || vip->mat[15] <= 0.0 ||
	    vip->lo < 0 || vip->hi > 255 )  {
	    	bu_struct_print("Unreasonable VOL parameters", rt_vol_parse,
			(char *)vip );
	    	return(-1);
	}

	/* Apply any modeling transforms to get final matrix */
	bn_mat_mul( tmat, mat, vip->mat );
	MAT_COPY( vip->mat, tmat );

	/* Get bit map from .bw(5) file */
	nbytes = (vip->xdim+VOL_XWIDEN*2)*
		(vip->ydim+VOL_YWIDEN*2)*
		(vip->zdim+VOL_ZWIDEN*2);
	vip->map = (unsigned char *)bu_calloc( 1, nbytes, "vol_import bitmap" );

	bu_semaphore_acquire( BU_SEM_SYSCALL );		/* lock */
	if( (fp = fopen(vip->file, "r")) == NULL )  {
		perror(vip->file);
		bu_semaphore_release( BU_SEM_SYSCALL );		/* unlock */
		return(-1);
	}
	bu_semaphore_release( BU_SEM_SYSCALL );		/* unlock */

	/* Because of in-memory padding, read each scanline separately */
	for( z=0; z < vip->zdim; z++ )  {
		for( y=0; y < vip->ydim; y++ )  {
			bu_semaphore_acquire( BU_SEM_SYSCALL );		/* lock */
			ret = fread( &VOL(vip, 0, y, z), vip->xdim, 1, fp ); /* res_syscall */
			bu_semaphore_release( BU_SEM_SYSCALL );		/* unlock */
			if( ret < 1 )  {
				bu_log("rt_vol_import(%s): Unable to read whole VOL, y=%d, z=%d\n",
					vip->file, y, z);
				bu_semaphore_acquire( BU_SEM_SYSCALL );		/* lock */
				fclose(fp);
				bu_semaphore_release( BU_SEM_SYSCALL );		/* unlock */
				return -1;
			}
		}
	}
	bu_semaphore_acquire( BU_SEM_SYSCALL );		/* lock */
	fclose(fp);
	bu_semaphore_release( BU_SEM_SYSCALL );		/* unlock */
	return( 0 );
}

/**
 *			R T _ V O L _ E X P O R T 5
 *
 *  The name will be added by the caller.
 */
int
rt_vol_export5(struct bu_external *ep, const struct rt_db_internal *ip, double local2mm, const struct db_i *dbip)
{
	struct rt_vol_internal	*vip;
	struct rt_vol_internal	vol;	/* scaled version */
	struct bu_vls		str;

	RT_CK_DB_INTERNAL(ip);
	if( ip->idb_type != ID_VOL )  return(-1);
	vip = (struct rt_vol_internal *)ip->idb_ptr;
	RT_VOL_CK_MAGIC(vip);
	vol = *vip;			/* struct copy */

	/* Apply scale factor */
	vol.mat[15] /= local2mm;

	BU_CK_EXTERNAL(ep);

	bu_vls_init( &str );
	bu_vls_struct_print( &str, rt_vol_parse, (char *)&vol );
	ep->ext_nbytes = bu_vls_strlen( &str );
	ep->ext_buf = (genptr_t)bu_calloc( 1, ep->ext_nbytes, "vol external");

	strcpy( ep->ext_buf, bu_vls_addr(&str) );
	bu_vls_free( &str );

	return(0);
}

/**
 *			R T _ V O L _ D E S C R I B E
 *
 *  Make human-readable formatted presentation of this solid.
 *  First line describes type of solid.
 *  Additional lines are indented one tab, and give parameter values.
 */
int
rt_vol_describe(struct bu_vls *str, const struct rt_db_internal *ip, int verbose, double mm2local)
{
	register struct rt_vol_internal	*vip =
		(struct rt_vol_internal *)ip->idb_ptr;
	register int i;
	struct bu_vls substr;
	vect_t local;

	RT_VOL_CK_MAGIC(vip);

	VSCALE( local, vip->cellsize, mm2local )
	bu_vls_strcat( str, "thresholded volumetric solid (VOL)\n\t");

/*	bu_vls_struct_print( str, rt_vol_parse, (char *)vip );
	bu_vls_strcat( str, "\n" ); */

	bu_vls_init( &substr );
	bu_vls_printf( &substr, "  file=\"%s\" w=%d n=%d d=%d lo=%d hi=%d size=%g %g %g\n   mat=",
		vip->file, vip->xdim, vip->ydim, vip->zdim, vip->lo, vip->hi,
		V3INTCLAMPARGS( local ) );
	bu_vls_vlscat( str, &substr );
	for( i=0 ; i<15 ; i++ )
	{
		bu_vls_trunc2( &substr, 0 );
		bu_vls_printf( &substr, "%g,", INTCLAMP(vip->mat[i]) );
		bu_vls_vlscat( str, &substr );
	}
	bu_vls_trunc2( &substr, 0 );
	bu_vls_printf( &substr, "%g\n", INTCLAMP(vip->mat[i]) );
	bu_vls_vlscat( str, &substr );

	bu_vls_free( &substr );

	return(0);
}

/**
 *			R T _ V O L _ I F R E E
 *
 *  Free the storage associated with the rt_db_internal version of this solid.
 */
void
rt_vol_ifree(struct rt_db_internal *ip)
{
	register struct rt_vol_internal	*vip;

	RT_CK_DB_INTERNAL(ip);
	vip = (struct rt_vol_internal *)ip->idb_ptr;
	RT_VOL_CK_MAGIC(vip);

	if( vip->map) bu_free( (char *)vip->map, "vol bitmap" );

	vip->magic = 0;			/* sanity */
	vip->map = (unsigned char *)0;
	bu_free( (char *)vip, "vol ifree" );
	ip->idb_ptr = GENPTR_NULL;	/* sanity */
}

/**
 *			R T _ V O L _ P R E P
 *
 *  Returns -
 *	0	OK
 *	!0	Failure
 *
 *  Implicit return -
 *	A struct rt_vol_specific is created, and it's address is stored
 *	in stp->st_specific for use by rt_vol_shot().
 */
int
rt_vol_prep(struct soltab *stp, struct rt_db_internal *ip, struct rt_i *rtip)
{
	struct rt_vol_internal	*vip;
	register struct rt_vol_specific *volp;
	vect_t	norm;
	vect_t	radvec;
	vect_t	diam;
	vect_t	small1;


	vip = (struct rt_vol_internal *)ip->idb_ptr;
	RT_VOL_CK_MAGIC(vip);

	BU_GETSTRUCT( volp, rt_vol_specific );
	volp->vol_i = *vip;		/* struct copy */
	vip->map = (unsigned char *)0;	/* "steal" the bitmap storage */

	/* build Xform matrix from model(world) to ideal(local) space */
	bn_mat_inv( volp->vol_mat, vip->mat );

	/* Pre-compute the necessary normals.  Rotate only. */
	VSET( norm, 1, 0 , 0 );
	MAT3X3VEC( volp->vol_xnorm, vip->mat, norm );
	VSET( norm, 0, 1, 0 );
	MAT3X3VEC( volp->vol_ynorm, vip->mat, norm );
	VSET( norm, 0, 0, 1 );
	MAT3X3VEC( volp->vol_znorm, vip->mat, norm );

	stp->st_specific = (genptr_t)volp;

	/* Find bounding RPP of rotated local RPP */
	VSETALL( small1, 0 );
	VSET( volp->vol_large,
		volp->vol_i.xdim*vip->cellsize[0], volp->vol_i.ydim*vip->cellsize[1], volp->vol_i.zdim*vip->cellsize[2] );/* type conversion */
	bn_rotate_bbox( stp->st_min, stp->st_max, vip->mat,
		small1, volp->vol_large );

	/* for now, VOL origin in ideal coordinates is at origin */
	VSETALL( volp->vol_origin, 0 );
	VADD2( volp->vol_large, volp->vol_large, volp->vol_origin );

	VSUB2( diam, stp->st_max, stp->st_min );
	VADD2SCALE( stp->st_center, stp->st_min, stp->st_max, 0.5 );
	VSCALE( radvec, diam, 0.5 );
	stp->st_aradius = stp->st_bradius = MAGNITUDE( radvec );

	return(0);		/* OK */
}

/**
 *			R T _ V O L _ P R I N T
 */
void
rt_vol_print(register const struct soltab *stp)
{
	register const struct rt_vol_specific *volp =
		(struct rt_vol_specific *)stp->st_specific;

	bu_log("vol file = %s\n", volp->vol_i.file );
	bu_log("dimensions = (%d, %d, %d)\n",
		volp->vol_i.xdim, volp->vol_i.ydim,
		volp->vol_i.zdim );
	VPRINT("model cellsize", volp->vol_i.cellsize);
	VPRINT("model grid origin", volp->vol_origin);
}

/**
 *			R T _ V O L _ N O R M
 *
 *  Given one ray distance, return the normal and
 *  entry/exit point.
 *  This is mostly a matter of translating the stored
 *  code into the proper normal.
 */
void
rt_vol_norm(register struct hit *hitp, struct soltab *stp, register struct xray *rp)
{
	register struct rt_vol_specific *volp =
		(struct rt_vol_specific *)stp->st_specific;

	VJOIN1( hitp->hit_point, rp->r_pt, hitp->hit_dist, rp->r_dir );

	switch( hitp->hit_surfno )  {
	case NORM_XPOS:
		VMOVE( hitp->hit_normal, volp->vol_xnorm );
		break;
	case NORM_XNEG:
		VREVERSE( hitp->hit_normal, volp->vol_xnorm );
		break;

	case NORM_YPOS:
		VMOVE( hitp->hit_normal, volp->vol_ynorm );
		break;
	case NORM_YNEG:
		VREVERSE( hitp->hit_normal, volp->vol_ynorm );
		break;

	case NORM_ZPOS:
		VMOVE( hitp->hit_normal, volp->vol_znorm );
		break;
	case NORM_ZNEG:
		VREVERSE( hitp->hit_normal, volp->vol_znorm );
		break;

	default:
		bu_log("rt_vol_norm(%s): surfno=%d bad\n",
			stp->st_name, hitp->hit_surfno );
		VSETALL( hitp->hit_normal, 0 );
		break;
	}
}

/**
 *			R T _ V O L _ C U R V E
 *
 *  Everything has sharp edges.  This makes things easy.
 */
void
rt_vol_curve(register struct curvature *cvp, register struct hit *hitp, struct soltab *stp)
{
/*	register struct rt_vol_specific *volp =
		(struct rt_vol_specific *)stp->st_specific; */

	bn_vec_ortho( cvp->crv_pdir, hitp->hit_normal );
	cvp->crv_c1 = cvp->crv_c2 = 0;
}

/**
 *			R T _ V O L _ U V
 *
 *  Map the hit point in 2-D into the range 0..1
 *  untransformed X becomes U, and Y becomes V.
 */
void
rt_vol_uv(struct application *ap, struct soltab *stp, register struct hit *hitp, register struct uvcoord *uvp)
{
/*	register struct rt_vol_specific *volp =
		(struct rt_vol_specific *)stp->st_specific;*/

	/* XXX uv should be xy in ideal space */
}

/**
 * 			R T _ V O L _ F R E E
 */
void
rt_vol_free(struct soltab *stp)
{
	register struct rt_vol_specific *volp =
		(struct rt_vol_specific *)stp->st_specific;

	bu_free( (char *)volp->vol_i.map, "vol_map" );
	bu_free( (char *)volp, "rt_vol_specific" );
}

int
rt_vol_class(void)
{
	return(0);
}

/**
 *			R T _ V O L _ P L O T
 */
int
rt_vol_plot(struct bu_list *vhead, struct rt_db_internal *ip, const struct rt_tess_tol *ttol, const struct bn_tol *tol)
{
	register struct rt_vol_internal *vip;
	register short	x,y,z;
	register short	v1,v2;
	point_t		a,b,c,d;

	RT_CK_DB_INTERNAL(ip);
	vip = (struct rt_vol_internal *)ip->idb_ptr;
	RT_VOL_CK_MAGIC(vip);

	/*
	 *  Scan across in Z & X.  For each X position, scan down Y,
	 *  looking for the longest run of edge.
	 */
	for( z=-1; z<=vip->zdim; z++ )  {
		for( x=-1; x<=vip->xdim; x++ )  {
			for( y=-1; y<=vip->ydim; y++ )  {
				v1 = VOL(vip, x,y,z);
				v2 = VOL(vip, x+1,y,z);
				if( OK(vip, v1) == OK(vip, v2) )  continue;
				/* Note start point, continue scan */
				VSET( a, x+0.5, y-0.5, z-0.5 );
				VSET( b, x+0.5, y-0.5, z+0.5 );
				for( ++y; y<=vip->ydim; y++ )  {
					v1 = VOL(vip, x,y,z);
					v2 = VOL(vip, x+1,y,z);
					if( OK(vip, v1) == OK(vip, v2) )
						break;
				}
				/* End of run of edge.  One cell beyond. */
				VSET( c, x+0.5, y-0.5, z+0.5 );
				VSET( d, x+0.5, y-0.5, z-0.5 );
				rt_vol_plate( a,b,c,d, vip->mat, vhead, vip );
			}
		}
	}

	/*
	 *  Scan up in Z & Y.  For each Y position, scan across X
	 */
	for( z=-1; z<=vip->zdim; z++ )  {
		for( y=-1; y<=vip->ydim; y++ )  {
			for( x=-1; x<=vip->xdim; x++ )  {
				v1 = VOL(vip, x,y,z);
				v2 = VOL(vip, x,y+1,z);
				if( OK(vip, v1) == OK(vip, v2) )  continue;
				/* Note start point, continue scan */
				VSET( a, x-0.5, y+0.5, z-0.5 );
				VSET( b, x-0.5, y+0.5, z+0.5 );
				for( ++x; x<=vip->xdim; x++ )  {
					v1 = VOL(vip, x,y,z);
					v2 = VOL(vip, x,y+1,z);
					if( OK(vip, v1) == OK(vip, v2) )
						break;
				}
				/* End of run of edge.  One cell beyond */
				VSET( c, (x-0.5), (y+0.5), (z+0.5) );
				VSET( d, (x-0.5), (y+0.5), (z-0.5) );
				rt_vol_plate( a,b,c,d, vip->mat, vhead, vip );
			}
		}
	}

	/*
	 * Scan across in Y & X.  For each X position pair edge, scan up Z.
	 */
	for( x=-1; x<=vip->xdim; x++ )  {
		for( z=-1; z<=vip->zdim; z++ )  {
			for( y=-1; y<=vip->ydim; y++ )  {
				v1 = VOL(vip, x,y,z);
				v2 = VOL(vip, x,y,z+1);
				if( OK(vip, v1) == OK(vip, v2) )  continue;
				/* Note start point, continue scan */
				VSET( a, (x-0.5), (y-0.5), (z+0.5) );
				VSET( b, (x+0.5), (y-0.5), (z+0.5) );
				for( ++y; y<=vip->ydim; y++ )  {
					v1 = VOL(vip, x,y,z);
					v2 = VOL(vip, x,y,z+1);
					if( OK(vip, v1) == OK(vip, v2) )
						break;
				}
				/* End of run of edge.  One cell beyond */
				VSET( c, (x+0.5), (y-0.5), (z+0.5) );
				VSET( d, (x-0.5), (y-0.5), (z+0.5) );
				rt_vol_plate( a,b,c,d, vip->mat, vhead, vip );
			}
		}
	}
	return(0);
}

/**
 *			R T _ V O L _ P L A T E
 */
void
rt_vol_plate(fastf_t *a, fastf_t *b, fastf_t *c, fastf_t *d, register fastf_t *mat, register struct bu_list *vhead, register struct rt_vol_internal *vip)
{
	LOCAL point_t	s;		/* scaled original point */
	LOCAL point_t	arot, prot;

	VELMUL( s, vip->cellsize, a );
	MAT4X3PNT( arot, mat, s );
	RT_ADD_VLIST( vhead, arot, BN_VLIST_LINE_MOVE );

	VELMUL( s, vip->cellsize, b );
	MAT4X3PNT( prot, mat, s );
	RT_ADD_VLIST( vhead, prot, BN_VLIST_LINE_DRAW );

	VELMUL( s, vip->cellsize, c );
	MAT4X3PNT( prot, mat, s );
	RT_ADD_VLIST( vhead, prot, BN_VLIST_LINE_DRAW );

	VELMUL( s, vip->cellsize, d );
	MAT4X3PNT( prot, mat, s );
	RT_ADD_VLIST( vhead, prot, BN_VLIST_LINE_DRAW );

	RT_ADD_VLIST( vhead, arot, BN_VLIST_LINE_DRAW );
}

/**
 *			R T _ V O L _ T E S S
 */
int
rt_vol_tess(struct nmgregion **r, struct model *m, struct rt_db_internal *ip, const struct rt_tess_tol *ttol, const struct bn_tol *tol)
{
	struct rt_vol_internal	*vip;
	register int	x,y,z;
	int		i;
	struct shell	*s;
	struct vertex	*verts[4];
	struct faceuse	*fu;
	struct model	*m_tmp;
	struct nmgregion *r_tmp;

	NMG_CK_MODEL( m );
	BN_CK_TOL( tol );
	RT_CK_TESS_TOL( ttol );

	RT_CK_DB_INTERNAL(ip);
	vip = (struct rt_vol_internal *)ip->idb_ptr;
	RT_VOL_CK_MAGIC(vip);

	/* make region, shell, vertex */
	m_tmp = nmg_mm();
	r_tmp = nmg_mrsv( m_tmp );
	s = BU_LIST_FIRST(shell, &r_tmp->s_hd);

	for( x=0 ; x<vip->xdim ; x++ )
	{
		for( y=0 ; y<vip->ydim ; y++ )
		{
			for( z=0 ; z<vip->zdim ; z++ )
			{
				point_t pt,pt1;

				/* skip empty cells */
				if( !OK( vip , VOL( vip , x , y , z ) ) )
					continue;

				/* check neighboring cells, make a face where needed */

				/* check z+1 */
				if( !OK( vip , VOL( vip , x , y , z+1 ) ) )
				{
					for( i=0 ; i<4 ; i++ )
						verts[i] = (struct vertex *)NULL;

					fu = nmg_cface( s , verts , 4 );

					VSET( pt , x+.5 , y-.5 , z+.5 );
					VELMUL( pt1 , vip->cellsize , pt );
					MAT4X3PNT( pt , vip->mat , pt1 );
					nmg_vertex_gv( verts[0] , pt );
					VSET( pt , x+.5 , y+.5 , z+.5 );
					VELMUL( pt1 , vip->cellsize , pt );
					MAT4X3PNT( pt , vip->mat , pt1 );
					nmg_vertex_gv( verts[1] , pt );
					VSET( pt , x-.5 , y+.5 , z+.5 );
					VELMUL( pt1 , vip->cellsize , pt );
					MAT4X3PNT( pt , vip->mat , pt1 );
					nmg_vertex_gv( verts[2] , pt );
					VSET( pt , x-.5 , y-.5 , z+.5 );
					VELMUL( pt1 , vip->cellsize , pt );
					MAT4X3PNT( pt , vip->mat , pt1 );
					nmg_vertex_gv( verts[3] , pt );

					if( nmg_fu_planeeqn( fu , tol ) )
						goto fail;
				}

				/* check z-1 */
				if( !OK( vip , VOL( vip , x , y , z-1 ) ) )
				{
					for( i=0 ; i<4 ; i++ )
						verts[i] = (struct vertex *)NULL;

					fu = nmg_cface( s , verts , 4 );

					VSET( pt , x+.5 , y-.5 , z-.5 );
					VELMUL( pt1 , vip->cellsize , pt );
					MAT4X3PNT( pt , vip->mat , pt1 );
					nmg_vertex_gv( verts[3] , pt );
					VSET( pt , x+.5 , y+.5 , z-.5 );
					VELMUL( pt1 , vip->cellsize , pt );
					MAT4X3PNT( pt , vip->mat , pt1 );
					nmg_vertex_gv( verts[2] , pt );
					VSET( pt , x-.5 , y+.5 , z-.5 );
					VELMUL( pt1 , vip->cellsize , pt );
					MAT4X3PNT( pt , vip->mat , pt1 );
					nmg_vertex_gv( verts[1] , pt );
					VSET( pt , x-.5 , y-.5 , z-.5 );
					VELMUL( pt1 , vip->cellsize , pt );
					MAT4X3PNT( pt , vip->mat , pt1 );
					nmg_vertex_gv( verts[0] , pt );

					if( nmg_fu_planeeqn( fu , tol ) )
						goto fail;
				}

				/* check y+1 */
				if( !OK( vip , VOL( vip , x , y+1 , z ) ) )
				{
					for( i=0 ; i<4 ; i++ )
						verts[i] = (struct vertex *)NULL;

					fu = nmg_cface( s , verts , 4 );

					VSET( pt , x+.5 , y+.5 , z+.5 );
					VELMUL( pt1 , vip->cellsize , pt );
					MAT4X3PNT( pt , vip->mat , pt1 );
					nmg_vertex_gv( verts[0] , pt );
					VSET( pt , x+.5 , y+.5 , z-.5 );
					VELMUL( pt1 , vip->cellsize , pt );
					MAT4X3PNT( pt , vip->mat , pt1 );
					nmg_vertex_gv( verts[1] , pt );
					VSET( pt , x-.5 , y+.5 , z-.5 );
					VELMUL( pt1 , vip->cellsize , pt );
					MAT4X3PNT( pt , vip->mat , pt1 );
					nmg_vertex_gv( verts[2] , pt );
					VSET( pt , x-.5 , y+.5 , z+.5 );
					VELMUL( pt1 , vip->cellsize , pt );
					MAT4X3PNT( pt , vip->mat , pt1 );
					nmg_vertex_gv( verts[3] , pt );

					if( nmg_fu_planeeqn( fu , tol ) )
						goto fail;
				}

				/* check y-1 */
				if( !OK( vip , VOL( vip , x , y-1 , z ) ) )
				{
					for( i=0 ; i<4 ; i++ )
						verts[i] = (struct vertex *)NULL;

					fu = nmg_cface( s , verts , 4 );

					VSET( pt , x+.5 , y-.5 , z+.5 );
					VELMUL( pt1 , vip->cellsize , pt );
					MAT4X3PNT( pt , vip->mat , pt1 );
					nmg_vertex_gv( verts[3] , pt );
					VSET( pt , x+.5 , y-.5 , z-.5 );
					VELMUL( pt1 , vip->cellsize , pt );
					MAT4X3PNT( pt , vip->mat , pt1 );
					nmg_vertex_gv( verts[2] , pt );
					VSET( pt , x-.5 , y-.5 , z-.5 );
					VELMUL( pt1 , vip->cellsize , pt );
					MAT4X3PNT( pt , vip->mat , pt1 );
					nmg_vertex_gv( verts[1] , pt );
					VSET( pt , x-.5 , y-.5 , z+.5 );
					VELMUL( pt1 , vip->cellsize , pt );
					MAT4X3PNT( pt , vip->mat , pt1 );
					nmg_vertex_gv( verts[0] , pt );

					if( nmg_fu_planeeqn( fu , tol ) )
						goto fail;
				}

				/* check x+1 */
				if( !OK( vip , VOL( vip , x+1 , y , z ) ) )
				{
					for( i=0 ; i<4 ; i++ )
						verts[i] = (struct vertex *)NULL;

					fu = nmg_cface( s , verts , 4 );

					VSET( pt , x+.5 , y-.5 , z-.5 );
					VELMUL( pt1 , vip->cellsize , pt );
					MAT4X3PNT( pt , vip->mat , pt1 );
					nmg_vertex_gv( verts[0] , pt );
					VSET( pt , x+.5 , y+.5 , z-.5 );
					VELMUL( pt1 , vip->cellsize , pt );
					MAT4X3PNT( pt , vip->mat , pt1 );
					nmg_vertex_gv( verts[1] , pt );
					VSET( pt , x+.5 , y+.5 , z+.5 );
					VELMUL( pt1 , vip->cellsize , pt );
					MAT4X3PNT( pt , vip->mat , pt1 );
					nmg_vertex_gv( verts[2] , pt );
					VSET( pt , x+.5 , y-.5 , z+.5 );
					VELMUL( pt1 , vip->cellsize , pt );
					MAT4X3PNT( pt , vip->mat , pt1 );
					nmg_vertex_gv( verts[3] , pt );

					if( nmg_fu_planeeqn( fu , tol ) )
						goto fail;
				}

				/* check x-1 */
				if( !OK( vip , VOL( vip , x-1 , y , z ) ) )
				{
					for( i=0 ; i<4 ; i++ )
						verts[i] = (struct vertex *)NULL;

					fu = nmg_cface( s , verts , 4 );

					VSET( pt , x-.5 , y-.5 , z-.5 );
					VELMUL( pt1 , vip->cellsize , pt );
					MAT4X3PNT( pt , vip->mat , pt1 );
					nmg_vertex_gv( verts[3] , pt );
					VSET( pt , x-.5 , y+.5 , z-.5 );
					VELMUL( pt1 , vip->cellsize , pt );
					MAT4X3PNT( pt , vip->mat , pt1 );
					nmg_vertex_gv( verts[2] , pt );
					VSET( pt , x-.5 , y+.5 , z+.5 );
					VELMUL( pt1 , vip->cellsize , pt );
					MAT4X3PNT( pt , vip->mat , pt1 );
					nmg_vertex_gv( verts[1] , pt );
					VSET( pt , x-.5 , y-.5 , z+.5 );
					VELMUL( pt1 , vip->cellsize , pt );
					MAT4X3PNT( pt , vip->mat , pt1 );
					nmg_vertex_gv( verts[0] , pt );

					if( nmg_fu_planeeqn( fu , tol ) )
						goto fail;
				}
			}
		}
	}

	nmg_region_a( r_tmp , tol );

	/* fuse model */
	nmg_model_fuse( m_tmp , tol );

	/* simplify shell */
	nmg_shell_coplanar_face_merge( s, tol, 1 );

	/* kill snakes */
	for( BU_LIST_FOR( fu , faceuse , &s->fu_hd ) )
	{
		struct loopuse *lu;

		NMG_CK_FACEUSE( fu );

		if( fu->orientation != OT_SAME )
			continue;

		for( BU_LIST_FOR( lu , loopuse , &fu->lu_hd ) )
			(void)nmg_kill_snakes( lu );
	}

	(void)nmg_unbreak_region_edges( (long *)(&s->l) );

	(void)nmg_mark_edges_real( (long *)&s->l );

	nmg_merge_models( m , m_tmp );
	*r = r_tmp;

	return( 0 );

fail:
	nmg_km( m_tmp );
	*r = (struct nmgregion *)NULL;

	return( -1 );
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
