/*
 *			G _ V O L . C
 *
 *  Purpose -
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
 *  Copyright Notice -
 *	This software is Copyright (C) 1989 by the United States Army.
 *	All rights reserved.
 */
#ifndef lint
static char RCSvol[] = "@(#)$Header$ (BRL)";
#endif

#include <stdio.h>
#include <ctype.h>
#include <math.h>
#include "machine.h"
#include "vmath.h"
#include "db.h"
#include "raytrace.h"
#include "./debug.h"
#include "./fixpt.h"

#define VOL_NAME_LEN 128
struct vol_specific {
	char		vol_file[VOL_NAME_LEN];
	unsigned char	*vol_map;
	int		vol_xdim;	/* X dimension */
	int		vol_ydim;	/* Y dimension */
	int		vol_zdim;	/* Z dimension */
	vect_t		vol_xnorm;	/* local +X norm in model coords */
	vect_t		vol_ynorm;
	vect_t		vol_znorm;
	vect_t		vol_cellsize;	/* ideal coords: size of each cell */
	mat_t		vol_mat;	/* model to ideal space */
	vect_t		vol_origin;	/* local coords of grid origin (0,0,0) for now */
	vect_t		vol_large;	/* local coords of XYZ max */
	int		vol_lo;		/* Low threshold */
	int		vol_hi;		/* High threshold */
};
#define VOL_NULL	((struct vol_specific *)0)
#define VOL_O(m)	offsetof(struct vol_specific, m)

struct structparse rt_vol_parse[] = {
#if CRAY && !__STDC__
	"%s",	VOL_NAME_LEN, "file",	0,		FUNC_NULL,
#else
	"%s",	VOL_NAME_LEN, "file",	offsetofarray(struct vol_specific, vol_file), FUNC_NULL,
#endif
	"%d",	1, "w",		VOL_O(vol_xdim),	FUNC_NULL,
	"%d",	1, "n",		VOL_O(vol_ydim),	FUNC_NULL,
	"%d",	1, "d",		VOL_O(vol_zdim),	FUNC_NULL,
	"%d",	1, "lo",	VOL_O(vol_lo),		FUNC_NULL,
	"%d",	1, "hi",	VOL_O(vol_hi),		FUNC_NULL,
	"%f",	ELEMENTS_PER_VECT, "size",offsetofarray(struct vol_specific, vol_cellsize), FUNC_NULL,
	/* XXX might have option for vol_origin */
	"",	0, (char *)0,	0,			FUNC_NULL
};

RT_EXTERN(void rt_vol_plate,(point_t a, point_t b, point_t c, point_t d,
	mat_t mat, struct vlhead *vhead, struct vol_specific *volp));
RT_EXTERN(struct vol_specific *rt_vol_import, (union record *rp));

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
#define VOL(xx,yy,zz)	volp->vol_map[ \
	(((zz)+VOL_ZWIDEN) * (volp->vol_ydim + VOL_YWIDEN*2)+ \
	 ((yy)+VOL_YWIDEN))* (volp->vol_xdim + VOL_XWIDEN*2)+ \
	  (xx)+VOL_XWIDEN ]

#define OK(v)	( (v) >= volp->vol_lo && (v) <= volp->vol_hi )

static int rt_vol_normtab[3] = { NORM_XPOS, NORM_YPOS, NORM_ZPOS };


/*
 *			R T _ V O L _ S H O T
 *
 *  Transform the ray into local coordinates of the volume ("ideal space").
 *  Step through the 3-D array, in local coordinates.
 *  Return intersection segments.
 *
 */
int
rt_vol_shot( stp, rp, ap, seghead )
struct soltab		*stp;
register struct xray	*rp;
struct application	*ap;
struct seg		*seghead;
{
	register struct vol_specific *volp =
		(struct vol_specific *)stp->st_specific;
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
if(rt_g.debug&DEBUG_VOL)VPRINT("vol_origin", volp->vol_origin);
if(rt_g.debug&DEBUG_VOL)VPRINT("r_pt", rp->r_pt);
if(rt_g.debug&DEBUG_VOL)VPRINT("P", P);
if(rt_g.debug&DEBUG_VOL)VPRINT("cellsize", volp->vol_cellsize);
	t0 = rp->r_min;
	tmax = rp->r_max;
if(rt_g.debug&DEBUG_VOL)rt_log("[shoot: r_min=%g, r_max=%g]\n", rp->r_min, rp->r_max);

	/* find grid cell where ray first hits ideal space bounding RPP */
	igrid[X] = (P[X] - volp->vol_origin[X]) / volp->vol_cellsize[X];
	igrid[Y] = (P[Y] - volp->vol_origin[Y]) / volp->vol_cellsize[Y];
	igrid[Z] = (P[Z] - volp->vol_origin[Z]) / volp->vol_cellsize[Z];
	if( igrid[X] < 0 )  {
		igrid[X] = 0;
	} else if( igrid[X] >= volp->vol_xdim ) {
		igrid[X] = volp->vol_xdim-1;
	}
	if( igrid[Y] < 0 )  {
		igrid[Y] = 0;
	} else if( igrid[Y] >= volp->vol_ydim ) {
		igrid[Y] = volp->vol_ydim-1;
	}
	if( igrid[Z] < 0 )  {
		igrid[Z] = 0;
	} else if( igrid[Z] >= volp->vol_zdim ) {
		igrid[Z] = volp->vol_zdim-1;
	}
if(rt_g.debug&DEBUG_VOL)rt_log("igrid=(%d, %d, %d)\n", igrid[X], igrid[Y], igrid[Z]);

	/* X setup */
	if( rp->r_dir[X] == 0.0 )  {
		t[X] = INFINITY;
		delta[X] = 0;
	} else {
		j = igrid[X];
		if( rp->r_dir[X] < 0 ) j++;
		t[X] = (volp->vol_origin[X] + j*volp->vol_cellsize[X] -
			rp->r_pt[X]) * invdir[X];
		delta[X] = volp->vol_cellsize[X] * fabs(invdir[X]);
	}
	/* Y setup */
	if( rp->r_dir[Y] == 0.0 )  {
		t[Y] = INFINITY;
		delta[Y] = 0;
	} else {
		j = igrid[Y];
		if( rp->r_dir[Y] < 0 ) j++;
		t[Y] = (volp->vol_origin[Y] + j*volp->vol_cellsize[Y] -
			rp->r_pt[Y]) * invdir[Y];
		delta[Y] = volp->vol_cellsize[Y] * fabs(invdir[Y]);
	}
	/* Z setup */
	if( rp->r_dir[Z] == 0.0 )  {
		t[Z] = INFINITY;
		delta[Z] = 0;
	} else {
		j = igrid[Z];
		if( rp->r_dir[Z] < 0 ) j++;
		t[Z] = (volp->vol_origin[Z] + j*volp->vol_cellsize[Z] -
			rp->r_pt[Z]) * invdir[Z];
		delta[Z] = volp->vol_cellsize[Z] * fabs(invdir[Z]);
	}

	/* The delta[] elements *must* be positive, as t must increase */
if(rt_g.debug&DEBUG_VOL)rt_log("t[X] = %g, delta[X] = %g\n", t[X], delta[X] );
if(rt_g.debug&DEBUG_VOL)rt_log("t[Y] = %g, delta[Y] = %g\n", t[Y], delta[Y] );
if(rt_g.debug&DEBUG_VOL)rt_log("t[Z] = %g, delta[Z] = %g\n", t[Z], delta[Z] );

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
if(rt_g.debug&DEBUG_VOL)rt_log("Entry axis is %s, t0=%g\n", in_axis==X ? "X" : (in_axis==Y?"Y":"Z"), t0);

	/* Advance to next exits */
	t[X] += delta[X];
	t[Y] += delta[Y];
	t[Z] += delta[Z];

	/* Ensure that next exit is after first entrance */
	if( t[X] < t0 )  {
		rt_log("*** advancing t[X]\n");
		t[X] += delta[X];
	}
	if( t[Y] < t0 )  {
		rt_log("*** advancing t[Y]\n");
		t[Y] += delta[Y];
	}
	if( t[Z] < t0 )  {
		rt_log("*** advancing t[Z]\n");
		t[Z] += delta[Z];
	}
if(rt_g.debug&DEBUG_VOL) VPRINT("Exit t[]", t);

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
		val = VOL( igrid[X], igrid[Y], igrid[Z] );
if(rt_g.debug&DEBUG_VOL)rt_log("igrid [%d %d %d] from %g to %g, val=%d\n",
			igrid[X], igrid[Y], igrid[Z],
			t0, t1, val );
if(rt_g.debug&DEBUG_VOL)rt_log("Exit axis is %s, t[]=(%g, %g, %g)\n",
			out_axis==X ? "X" : (out_axis==Y?"Y":"Z"),
			t[X], t[Y], t[Z] );

		if( t1 <= t0 )  rt_log("ERROR vol t1=%g < t0=%g\n", t1, t0 );
		if( !inside )  {
			if( OK( val ) )  {
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
				RT_LIST_INSERT( &(seghead->l), &(segp->l) );
				if(rt_g.debug&DEBUG_VOL) rt_log("START t=%g, surfno=%d\n",
					t0, segp->seg_in.hit_surfno);
			} else {
				/* Do nothing, marching through void */
			}
		} else {
			if( OK( val ) )  {
				/* Do nothing, marching through solid */
			} else {
				register struct seg	*tail;
				/* End of segment (now in an empty voxel) */
				/* Handle transition from solid to vacuum */
				inside = 0;

				tail = RT_LIST_LAST( seg, &(seghead->l) );
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
				if(rt_g.debug&DEBUG_VOL) rt_log("END t=%g, surfno=%d\n",
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
		tail = RT_LIST_LAST( seg, &(seghead->l) );
		tail->seg_out.hit_dist = tmax;

		/* Compute exit normal.  Previous out_axis is now in_axis */
		if( rp->r_dir[in_axis] < 0 )  {
			/* Go left, exit normal goes left */
			tail->seg_out.hit_surfno = (-rt_vol_normtab[in_axis]);
		}  else  {
			/* go right, exit norm goes right */
			tail->seg_out.hit_surfno = rt_vol_normtab[in_axis];
		}
		if(rt_g.debug&DEBUG_VOL) rt_log("closed END t=%g, surfno=%d\n",
			tmax, tail->seg_out.hit_surfno );
	}

	if( RT_LIST_IS_EMPTY( &(seghead->l) ) )
		return(0);
	return(2);
}

/*
 *			R T _ V O L _ I M P O R T
 */
HIDDEN struct vol_specific *
rt_vol_import( rp )
union record	*rp;
{
	register struct vol_specific *volp;
	FILE	*fp;
	int	nbytes;
	register int	y;
	register int	z;
	char	*str;
	char	*cp;
	struct rt_vls vls;

	GETSTRUCT( volp, vol_specific );

	rt_vls_init( &vls );

	cp = rp->ss.ss_str;
	while( *cp && !isspace(*cp) )  cp++;
	/* Skip all white space */
	while( *cp && isspace(*cp) )  cp++;

	/* Establish defaults */
	volp->vol_lo = 0;
	volp->vol_hi = 255;

	/* Default VOL cell size in ideal coordinates is one unit/cell */
	VSETALL( volp->vol_cellsize, 1 );

	rt_vls_strcpy( &vls, cp);
	rt_structparse( &vls, rt_vol_parse, (char *)volp );
	rt_vls_free( &vls );

	/* Check for reasonable values */
	if( volp->vol_file[0] == '\0' || volp->vol_xdim < 1 ||
	    volp->vol_ydim < 1 || volp->vol_zdim < 1 ||
	    volp->vol_lo < 0 || volp->vol_hi > 255 )  {
	    	rt_log("Unreasonable VOL parameters\n");
	    	rt_structprint("unreasonable", rt_vol_parse, (char *)volp );
		rt_free( (char *)volp, "vol_specific" );
		return( VOL_NULL );
	}

	/* Get bit map from .bw(5) file */
	nbytes = (volp->vol_xdim+VOL_XWIDEN*2)*
		(volp->vol_ydim+VOL_YWIDEN*2)*
		(volp->vol_zdim+VOL_ZWIDEN*2);
	volp->vol_map = (unsigned char *)rt_malloc( nbytes, "vol_import bitmap" );
#ifdef SYSV
	memset( volp->vol_map, 0, nbytes );
#else
	bzero( volp->vol_map, nbytes );
#endif
	if( (fp = fopen(volp->vol_file, "r")) == NULL )  {
		perror(volp->vol_file);
err:
		rt_free( (char *)volp->vol_map, "vol_map" );
		rt_free( (char *)volp, "vol_specific" );
		return( VOL_NULL );
	}

	/* Because of in-memory padding, read each scanline separately */
	for( z=0; z < volp->vol_zdim; z++ )  {
		for( y=0; y < volp->vol_ydim; y++ )  {
			if( fread( &VOL(0, y, z), volp->vol_xdim, 1, fp ) < 1 )  {
				rt_log("Unable to read whole VOL\n");
				fclose(fp);
				goto err;
			}
		}
	}
	fclose(fp);
	return( volp );
}

/*
 *			R T _ V O L _ P R E P
 *
 *  Returns -
 *	0	OK
 *	!0	Failure
 *
 *  Implicit return -
 *	A struct vol_specific is created, and it's address is stored
 *	in stp->st_specific for use by rt_vol_shot().
 */
int
rt_vol_prep( stp, rp, rtip )
struct soltab	*stp;
union record	*rp;
struct rt_i	*rtip;
{
	register struct vol_specific *volp;
	vect_t	norm;
	vect_t	radvec;
	vect_t	diam;
	vect_t	small;

	if( (volp = rt_vol_import( rp )) == VOL_NULL )
		return(-1);	/* ERROR */

	/* build Xform matrix from model(world) to ideal(local) space */
	mat_inv( volp->vol_mat, stp->st_pathmat );

	/* Pre-compute the necessary normals */
	VSET( norm, 1, 0 , 0 );
	MAT4X3VEC( volp->vol_xnorm, stp->st_pathmat, norm );
	VSET( norm, 0, 1, 0 );
	MAT4X3VEC( volp->vol_ynorm, stp->st_pathmat, norm );
	VSET( norm, 0, 0, 1 );
	MAT4X3VEC( volp->vol_znorm, stp->st_pathmat, norm );

	stp->st_specific = (genptr_t)volp;

	/* Find bounding RPP of rotated local RPP */
	VSETALL( small, 0 );
	VSET( volp->vol_large,
		volp->vol_xdim, volp->vol_ydim, volp->vol_zdim );/* type conversion */
	rt_rot_bound_rpp( stp->st_min, stp->st_max, stp->st_pathmat,
		small, volp->vol_large );

	/* for now, VOL origin in ideal coordinates is at origin */
	VSETALL( volp->vol_origin, 0 );
	VADD2( volp->vol_large, volp->vol_large, volp->vol_origin );

	VSUB2( diam, stp->st_max, stp->st_min );
	VADD2SCALE( stp->st_center, stp->st_min, stp->st_max, 0.5 );
	VSCALE( radvec, diam, 0.5 );
	stp->st_aradius = stp->st_bradius = MAGNITUDE( radvec );

	return(0);		/* OK */
}

/*
 *			R T _ V O L _ P R I N T
 */
void
rt_vol_print( stp )
register struct soltab	*stp;
{
	register struct vol_specific *volp =
		(struct vol_specific *)stp->st_specific;

	rt_log("vol file = %s\n", volp->vol_file );
	rt_log("dimensions = (%d, %d, %d)\n",
		volp->vol_xdim, volp->vol_ydim,
		volp->vol_zdim );
	VPRINT("model cellsize", volp->vol_cellsize);
	VPRINT("model grid origin", volp->vol_origin);
}

/*
 *			R T _ V O L _ N O R M
 *
 *  Given one ray distance, return the normal and
 *  entry/exit point.
 *  This is mostly a matter of translating the stored
 *  code into the proper normal.
 */
void
rt_vol_norm( hitp, stp, rp )
register struct hit	*hitp;
struct soltab		*stp;
register struct xray	*rp;
{
	register struct vol_specific *volp =
		(struct vol_specific *)stp->st_specific;

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
		rt_log("rt_vol_norm(%s): surfno=%d bad\n",
			stp->st_name, hitp->hit_surfno );
		VSETALL( hitp->hit_normal, 0 );
		break;
	}
}

/*
 *			R T _ V O L _ C U R V E
 *
 *  Everything has sharp edges.  This makes things easy.
 */
void
rt_vol_curve( cvp, hitp, stp )
register struct curvature	*cvp;
register struct hit		*hitp;
struct soltab			*stp;
{
	register struct vol_specific *volp =
		(struct vol_specific *)stp->st_specific;

	vec_ortho( cvp->crv_pdir, hitp->hit_normal );
	cvp->crv_c1 = cvp->crv_c2 = 0;
}

/*
 *			R T _ V O L _ U V
 *
 *  Map the hit point in 2-D into the range 0..1
 *  untransformed X becomes U, and Y becomes V.
 */
void
rt_vol_uv( ap, stp, hitp, uvp )
struct application	*ap;
struct soltab		*stp;
register struct hit	*hitp;
register struct uvcoord	*uvp;
{
	register struct vol_specific *volp =
		(struct vol_specific *)stp->st_specific;

	/* XXX uv should be xy in ideal space */
}

/*
 * 			R T _ V O L _ F R E E
 */
void
rt_vol_free( stp )
struct soltab	*stp;
{
	register struct vol_specific *volp =
		(struct vol_specific *)stp->st_specific;

	rt_free( (char *)volp->vol_map, "vol_map" );
	rt_free( (char *)volp, "vol_specific" );
}

int
rt_vol_class()
{
	return(0);
}

/*
 *			R T _ V O L _ P L O T
 */
int
rt_vol_plot( rp, matp, vhead, dp )
union record	*rp;
mat_t		matp;
struct vlhead	*vhead;
struct directory *dp;
{
	register struct vol_specific *volp;
	register short	x,y,z;
	register short	v1,v2;
	point_t		a,b,c,d;

	if( (volp = rt_vol_import( rp )) == VOL_NULL )
		return(-1);

	/*
	 *  Scan across in Z & X.  For each X position, scan down Y,
	 *  looking for the longest run of edge.
	 */
	for( z=0; z<volp->vol_zdim; z++ )  {
		for( x=0; x<volp->vol_xdim; x++ )  {
			for( y=0; y<volp->vol_ydim; y++ )  {
				v1 = VOL(x,y,z);
				v2 = VOL(x+1,y,z);
				if( OK(v1) == OK(v2) )  continue;
				/* Note start point, continue scan */
				VSET( a, x+0.5, y-0.5, z-0.5 );
				VSET( b, x+0.5, y-0.5, z+0.5 );
				for( ++y; y<volp->vol_ydim; y++ )  {
					v1 = VOL(x,y,z);
					v2 = VOL(x+1,y,z);
					if( OK(v1) == OK(v2) )
						break;
				}
				/* End of run of edge.  One cell beyond. */
				VSET( c, x+0.5, y-0.5, z+0.5 );
				VSET( d, x+0.5, y-0.5, z-0.5 );
				rt_vol_plate( a,b,c,d, matp, vhead, volp );
			}
		}
	}

	/*
	 *  Scan up in Z & Y.  For each Y position, scan across X
	 */
	for( z=0; z<volp->vol_zdim; z++ )  {
		for( y=0; y<volp->vol_ydim; y++ )  {
			for( x=0; x<volp->vol_xdim; x++ )  {
				v1 = VOL(x,y,z);
				v2 = VOL(x,y+1,z);
				if( OK(v1) == OK(v2) )  continue;
				/* Note start point, continue scan */
				VSET( a, x-0.5, y+0.5, z-0.5 );
				VSET( b, x-0.5, y+0.5, z+0.5 );
				for( ++x; x<volp->vol_xdim; x++ )  {
					v1 = VOL(x,y,z);
					v2 = VOL(x,y+1,z);
					if( OK(v1) == OK(v2) )
						break;
				}
				/* End of run of edge.  One cell beyond */
				VSET( c, (x-0.5), (y+0.5), (z+0.5) );
				VSET( d, (x-0.5), (y+0.5), (z-0.5) );
				rt_vol_plate( a,b,c,d, matp, vhead, volp );
			}
		}
	}

	/*
	 * Scan across in Y & X.  For each X position pair edge, scan up Z.
	 */
	for( y=0; y<volp->vol_ydim; y++ )  {
		for( x=0; x<volp->vol_xdim; x++ )  {
			for( z=0; z<volp->vol_zdim; z++ )  {
				v1 = VOL(x,y,z);
				v2 = VOL(x+1,y,z);
				if( OK(v1) == OK(v2) )  continue;
				/* Note start point, continue scan */
				VSET( a, (x+0.5), (y-0.5), (z-0.5) );
				VSET( b, (x+0.5), (y+0.5), (z-0.5) );
				for( ++z; z<volp->vol_zdim; z++ )  {
					v1 = VOL(x,y,z);
					v2 = VOL(x+1,y,z);
					if( OK(v1) == OK(v2) )
						break;
				}
				/* End of run of edge.  One cell beyond */
				VSET( c, (x+0.5), (y+0.5), (z-0.5) );
				VSET( d, (x+0.5), (y-0.5), (z-0.5) );
				rt_vol_plate( a,b,c,d, matp, vhead, volp );
			}
		}
	}
	return(0);
}

/*
 *			R T _ V O L _ P L A T E
 */
void
rt_vol_plate( a,b,c,d, mat, vhead, volp )
point_t			a,b,c,d;
register mat_t		mat;
register struct vlhead	*vhead;
register struct vol_specific	*volp;
{
	LOCAL point_t	s;		/* scaled original point */
	LOCAL point_t	arot, prot;

	VELMUL( s, volp->vol_cellsize, a );
	MAT4X3PNT( arot, mat, s );
	ADD_VL( vhead, arot, 0 );

	VELMUL( s, volp->vol_cellsize, b );
	MAT4X3PNT( prot, mat, s );
	ADD_VL( vhead, prot, 1 );

	VELMUL( s, volp->vol_cellsize, c );
	MAT4X3PNT( prot, mat, s );
	ADD_VL( vhead, prot, 1 );

	VELMUL( s, volp->vol_cellsize, d );
	MAT4X3PNT( prot, mat, s );
	ADD_VL( vhead, prot, 1 );

	ADD_VL( vhead, arot, 1 );
}
