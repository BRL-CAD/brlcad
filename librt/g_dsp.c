/*
 *			G _ D S P . C
 *
 *  Purpose -
 *	Intersect a ray with a displacement map
 *
 * Adding a new solid type:
 *
 *
 *  The bounding box planes (in dsp coordinates) are numbered 0 .. 5
 *
 *  For purposes of the "struct hit" surface number, the "non-elevation" 
 *  surfaces are numbered -1 .. -6 where:
 *  
 *
 *	     Plane #	Name  plane dist     BBSURF #
 *	--------------------------------------------------
 *		0	XMIN (dist = 0)		-1
 *		1	XMAX (dist = xsiz)	-2
 *		2	YMIN (dist = 0)		-3
 *		3	YMAX (dist = ysiz)	-4
 *		4	ZMIN (dist = 0)		-5
 *		5	ZMAX (dsp_max)		-6
 *
 *		6	ZMID (dsp_min)		-7	(upward normal)
 *
 *  if the "struct hit" surfno surface is positive, then 
 *  	hit_vpriv holds the cell that was hit.
 *	surfno is either  TRI1 or TRI2
 *	
 *	The top surfaces of the dsp are numbered 0 .. wid*len*2
 *	(2 triangles per ``cell'')
 *
 *	The BBSURF() macro maps plane # into surface number (or vice versa)
 *
 *  Authors -
 *  	Lee A. Butler
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
static char RCSdsp[] = "@(#)$Header$ (BRL)";
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
#include "plot3.h"

#define BBSURF(_s) (-((_s)+1))	/* bounding box surface */
#define BBOX_PLANES	7	/* 2 tops & 5 other sides */
#define XMIN 0
#define XMAX 1
#define YMIN 2
#define YMAX 3
#define ZMIN 4
#define ZMAX 5
#define ZMID 6

#define TRI1	10
#define TRI2	20

/* access to the array */
#define DSP(_p,_x,_y) ( \
	((unsigned short *)(((struct rt_dsp_internal *)_p)->dsp_mp->buf))[ \
		(_y) * ((struct rt_dsp_internal *)_p)->dsp_xcnt + (_x) ] )

#define XCNT(_p) (((struct rt_dsp_internal *)_p)->dsp_xcnt)
#define YCNT(_p) (((struct rt_dsp_internal *)_p)->dsp_ycnt)

#define XSIZ(_p) (((struct dsp_specific *)_p)->xsiz)
#define YSIZ(_p) (((struct dsp_specific *)_p)->ysiz)


/* per-solid ray tracing form of solid, including precomputed terms
 *
 * The dsp_i element MUST BE FIRST so that we can cast a pointer to 
 *  a dsp_specific to a rt_dsp_intermal.
 */
struct dsp_specific {
	struct rt_dsp_internal dsp_i;	/* MUST BE FIRST */
	double		dsp_pl_dist[BBOX_PLANES];
	int		xsiz;
	int		ysiz;
};

struct bbox_isect {
	double	in_dist;
	double	out_dist;
	int	in_surf;
	int	out_surf;
};


/* per-ray ray tracing information
 *
 */
struct isect_stuff {
	struct dsp_specific	*dsp;
	struct bu_list		seglist;
	struct xray		r;
	struct application	*ap;
	struct soltab		*stp;
	struct bn_tol		*tol;

	struct bbox_isect bbox;
	struct bbox_isect minbox;

	struct seg *segp;	/* the current segment being filled */
};



/* plane equations (minus offset distances) for bounding RPP */
static CONST vect_t	dsp_pl[BBOX_PLANES] = {
	{-1.0, 0.0, 0.0},
	{ 1.0, 0.0, 0.0},

	{0.0, -1.0, 0.0},
	{0.0,  1.0, 0.0},

	{0.0, 0.0, -1.0},
	{0.0, 0.0,  1.0},
	{0.0, 0.0,  1.0},
};



#define DSP_O(m) offsetof(struct rt_dsp_internal, m)
#define DSP_AO(a) offsetofarray(struct rt_dsp_internal, a)

struct bu_structparse rt_dsp_parse[] = {
	{"%s",	DSP_NAME_LEN, "file", DSP_AO(dsp_file), FUNC_NULL },
	{"%d",	1, "w", DSP_O(dsp_xcnt), FUNC_NULL },
	{"%d",	1, "n", DSP_O(dsp_ycnt), FUNC_NULL },
	{"%f",	1, "xs", DSP_O(dsp_xs), FUNC_NULL },
	{"%f",	1, "ys", DSP_O(dsp_ys), FUNC_NULL },
	{"%f",	1, "zs", DSP_O(dsp_zs), FUNC_NULL },
	{"%f", 16, "stom", DSP_AO(dsp_stom), FUNC_NULL },
	{"",	0, (char *)0, 0,			FUNC_NULL }
};

struct bu_structparse rt_dsp_ptab[] = {
	{"%s",	DSP_NAME_LEN, "file", DSP_AO(dsp_file), FUNC_NULL },
	{"%d",	1, "w", DSP_O(dsp_xcnt), FUNC_NULL },
	{"%d",	1, "n", DSP_O(dsp_ycnt), FUNC_NULL },
	{"%f", 16, "stom", DSP_AO(dsp_stom), FUNC_NULL },
	{"",	0, (char *)0, 0,			FUNC_NULL }
};


static void
dsp_print(vls, dsp_ip)
struct bu_vls *vls;
CONST struct rt_dsp_internal *dsp_ip;
{
	point_t pt, v;
	RT_DSP_CK_MAGIC(dsp_ip);

	bu_vls_init( vls );
	bu_vls_printf( vls, "Displacement Map\n  file='%s' xc=%d yc=%d\n",
		dsp_ip->dsp_file, dsp_ip->dsp_xcnt, dsp_ip->dsp_ycnt);

	VSETALL(pt, 0.0);
	MAT4X3PNT(v, dsp_ip->dsp_stom, pt);
	bu_vls_printf( vls, "  V=(%g %g %g)\n", V3ARGS(pt));

	bu_vls_printf( vls, "  stom=\n");
	bu_vls_printf( vls, "  %8.3f %8.3f %8.3f %8.3f\n",
		V4ARGS(dsp_ip->dsp_stom) );

	bu_vls_printf( vls, "  %8.3f %8.3f %8.3f %8.3f\n",
		V4ARGS( &dsp_ip->dsp_stom[4]) );

	bu_vls_printf( vls, "  %8.3f %8.3f %8.3f %8.3f\n",
		V4ARGS( &dsp_ip->dsp_stom[8]) );

	bu_vls_printf( vls, "  %8.3f %8.3f %8.3f %8.3f\n",
		V4ARGS( &dsp_ip->dsp_stom[12]) );

}

/*
 *			R T _ D S P _ P R I N T
 */
void
rt_dsp_print( stp )
register CONST struct soltab *stp;
{
	register CONST struct dsp_specific *dsp =
		(struct dsp_specific *)stp->st_specific;
	struct bu_vls vls;
 
	RT_DSP_CK_MAGIC(dsp);

	dsp_print(&vls, &(dsp->dsp_i) );

	bu_log("%s", bu_vls_addr( &vls));
	bu_vls_free( &vls );

}


/*
 *  			R T _ D S P _ P R E P
 *  
 *  Given a pointer to a GED database record, and a transformation matrix,
 *  determine if this is a valid DSP, and if so, precompute various
 *  terms of the formula.
 *  
 *  Returns -
 *  	0	DSP is OK
 *  	!0	Error in description
 *  
 *  Implicit return -
 *  	A struct dsp_specific is created, and it's address is stored in
 *  	stp->st_specific for use by dsp_shot().
 */
int
rt_dsp_prep( stp, ip, rtip )
struct soltab		*stp;
struct rt_db_internal	*ip;
struct rt_i		*rtip;
{
	struct rt_dsp_internal		*dsp_ip;
	register struct dsp_specific	*dsp;
	int x, y;
	unsigned short dsp_min, dsp_max;
	point_t pt, bbpt;
	vect_t work;
	fastf_t f;

	if (rt_g.debug & DEBUG_HF)
		bu_log("rt_dsp_prep()\n");

	RT_CK_DB_INTERNAL(ip);
	dsp_ip = (struct rt_dsp_internal *)ip->idb_ptr;
	RT_DSP_CK_MAGIC(dsp_ip);
	BU_CK_MAPPED_FILE(dsp_ip->dsp_mp);

	GETSTRUCT( dsp, dsp_specific );
	stp->st_specific = (genptr_t) dsp;
	dsp->dsp_i = *dsp_ip;		/* struct copy */

	RES_ACQUIRE( &rt_g.res_model);
	++dsp_ip->dsp_mp->uses;
	RES_RELEASE( &rt_g.res_model);

	dsp->xsiz = dsp_ip->dsp_xcnt-1;	/* size is # cells or values-1 */
	dsp->ysiz = dsp_ip->dsp_ycnt-1;	/* size is # cells or values-1 */

	dsp_min = 0xffff;
	dsp_max = 0;

	for (y=0 ; y < YSIZ(dsp) ; y++) {

		for (x=1 ; x < XCNT(dsp) ; x++) {

			if (DSP(dsp, x, y) < dsp_min)
				dsp_min = DSP(dsp, x, y);

			if (DSP(dsp, x, y) > dsp_max)
				dsp_max = DSP(dsp, x, y);
		}
	}

	if (rt_g.debug & DEBUG_HF)
		bu_log("min %d max %d\n", dsp_min, dsp_max);

	/* record the distance to each of the bounding planes */
	dsp->dsp_pl_dist[XMIN] = 0.0;
	dsp->dsp_pl_dist[XMAX] = (fastf_t)dsp->xsiz;
	dsp->dsp_pl_dist[YMIN] = 0.0;
	dsp->dsp_pl_dist[YMAX] = (fastf_t)dsp->ysiz;
	dsp->dsp_pl_dist[ZMIN] = 0.0;
	dsp->dsp_pl_dist[ZMAX] = (fastf_t)dsp_max;
	dsp->dsp_pl_dist[ZMID] = (fastf_t)dsp_min;

	/* compute bounding box and spere */

#define BBOX_PT(_x, _y, _z) \
	VSET(pt, (fastf_t)_x, (fastf_t)_y, (fastf_t)_z); \
	MAT4X3PNT(bbpt, dsp_ip->dsp_stom, pt); \
	VMINMAX( stp->st_min, stp->st_max, bbpt)

	BBOX_PT(-.1,		    -.1,		        -.1);
	BBOX_PT(dsp_ip->dsp_xcnt+.1, -.1,		        -.1);
	BBOX_PT(dsp_ip->dsp_xcnt+.1, dsp_ip->dsp_ycnt+.1, -1);
	BBOX_PT(-.1,		    dsp_ip->dsp_ycnt+.1, -1);
	BBOX_PT(-.1,		    -.1,		        dsp_max+.1);
	BBOX_PT(dsp_ip->dsp_xcnt+.1, -.1,		        dsp_max+.1);
	BBOX_PT(dsp_ip->dsp_xcnt+.1, dsp_ip->dsp_ycnt+1, dsp_max+.1);
	BBOX_PT(-.1,		    dsp_ip->dsp_ycnt+1, dsp_max+.1);

#undef BBOX_PT

	VADD2SCALE( stp->st_center, stp->st_min, stp->st_max, 0.5 );
	VSUB2SCALE( work, stp->st_max, stp->st_min, 0.5 );

	f = work[X];
	if( work[Y] > f )  f = work[Y];
	if( work[Z] > f )  f = work[Z];
	stp->st_aradius = f;
	stp->st_bradius = MAGNITUDE(work);

	if (rt_g.debug & DEBUG_HF) {
		bu_log("model space bbox (%g %g %g) (%g %g %g)\n",
			V3ARGS(stp->st_min),
			V3ARGS(stp->st_max));
	}

	BU_CK_MAPPED_FILE(dsp_ip->dsp_mp);

	return 0;
}


/*
 *  Intersect the ray with the bounding box of the dsp solid.
 */
static int
isect_ray_bbox(isect)
register struct isect_stuff *isect;
{
	register double NdotD;
	register double NdotPt;
	double dist;
	register int i;

	isect->bbox.in_dist = - (isect->bbox.out_dist = MAX_FASTF);
	isect->bbox.in_surf = isect->bbox.out_surf = -1;

	/* intersect the ray with the bounding box */
	for (i=0 ; i < BBOX_PLANES ; i++) {
		NdotD = VDOT(dsp_pl[i], isect->r.r_dir);
		NdotPt = VDOT(dsp_pl[i], isect->r.r_pt);

		/* if N/ray vectors are perp, ray misses plane */
		if (BN_VECT_ARE_PERP(NdotD, isect->tol)) {
			if ( (NdotPt - isect->dsp->dsp_pl_dist[i]) >
			    SQRT_SMALL_FASTF ) {
				if (rt_g.debug & DEBUG_HF)
					bu_log("ray parallel and above bbox surf %d\n", i);

				return 0; /* MISS */
			    }
		}

		dist = - ( NdotPt - isect->dsp->dsp_pl_dist[i] ) / NdotD;

		if (rt_g.debug & DEBUG_HF)
			bu_log("surf[%d](%g %g %g %g) dot:%g dist:%g\n",
				i, V3ARGS(dsp_pl[i]),
				isect->dsp->dsp_pl_dist[i],
				NdotD, dist);

		if ( i > 5) /* dsp_min elevation not valid bbox limit */
			continue;

		if (NdotD < 0.0) {
			/* entering */
			if (dist > isect->bbox.in_dist) {
				isect->bbox.in_dist = dist;
				isect->bbox.in_surf = i;
			}
		} else { /*  (NdotD > 0.0) */
			/* leaving */
			if (dist < isect->bbox.out_dist) {
				isect->bbox.out_dist = dist;
				isect->bbox.out_surf = i;
			}
		}
	}

	if (rt_g.debug & DEBUG_HF) {
		vect_t pt;
		VJOIN1(pt, isect->r.r_pt, isect->bbox.in_dist, isect->r.r_dir);
		bu_log("solid rpp in  surf[%d] dist:%g (%g %g %g)\n",
			isect->bbox.in_surf, isect->bbox.in_dist, V3ARGS(pt));
		VJOIN1(pt, isect->r.r_pt, isect->bbox.out_dist, isect->r.r_dir);
		bu_log("solid rpp out surf[%d] dist:%g  (%g %g %g)\n",
			isect->bbox.out_surf, isect->bbox.out_dist, V3ARGS(pt));
	}

	/* validate */
	if (isect->bbox.in_dist >= isect->bbox.out_dist ||
	    isect->bbox.out_dist >= MAX_FASTF ) 
		return 0; /* MISS */


	/* copmute "minbox" the bounding box of the rpp under the lowest
	 * displacement height
	 */

	isect->minbox.in_dist = isect->bbox.in_dist;
	isect->minbox.in_surf = isect->bbox.in_surf;
	isect->minbox.out_dist = isect->bbox.out_dist;
	isect->minbox.out_surf = isect->bbox.out_surf;

	if (NdotD < 0.0) {
		/* entering */
		if (dist > isect->bbox.in_dist) {
			isect->minbox.in_dist = dist;
			isect->minbox.in_surf = 6;
		}
	} else { /*  (NdotD > 0.0) */
		/* leaving */
		if (dist < isect->bbox.out_dist) {
			isect->minbox.out_dist = dist;
			isect->minbox.out_surf = 6;
		}
	}

	if (rt_g.debug & DEBUG_HF) {
		vect_t pt;
		VJOIN1(pt, isect->r.r_pt, isect->minbox.in_dist, isect->r.r_dir);

		bu_log("solid minbox rpp in  surf[%d] dist:%g (%g %g %g)\n",
			isect->minbox.in_surf, isect->minbox.in_dist, V3ARGS(pt));

		VJOIN1(pt, isect->r.r_pt, isect->minbox.out_dist, isect->r.r_dir);
		bu_log("solid minbox rpp out surf[%d] dist:%g (%g %g %g)\n",
			isect->minbox.out_surf, isect->minbox.out_dist, V3ARGS(pt));
	}

	return 1;
}




#if 1
#define CELL_MIN(_c) c_min(isect, _c)
#define CELL_MAX(_c) c_max(isect, _c)


static short
c_min(isect, cell)
struct isect_stuff *isect;
CONST register int *cell;
{
	register struct dsp_specific *dsp = isect->dsp;
	register short v, i;
	

	v=DSP(dsp, cell[X], cell[Y]);

	if (  (i=DSP(dsp, cell[X]+1, cell[Y])) < v) 
		v = i;

	if ( (i=DSP(isect->dsp, cell[X], cell[Y]+1)) < v)
		v = i;

	if ( (i=DSP(isect->dsp, cell[X]+1, cell[Y]+1)) < v)
		v = i;

	return v;
}
static short
c_max(isect, cell)
struct isect_stuff *isect;
CONST register int *cell;
{
	register struct dsp_specific *dsp = isect->dsp;
	register short v, i;
	int j;

	j = cell[Y] * dsp->dsp_i.dsp_xcnt + cell[X];

	v = ((unsigned short *)dsp->dsp_i.dsp_mp->buf)[ j ];


	v = DSP(dsp, cell[X], cell[Y]);

	if (  (i=DSP(dsp, cell[X]+1, cell[Y])) > v )
		v = i;

	if ( (i=DSP(isect->dsp, cell[X], cell[Y]+1)) > v)
		v = i;

	if ( (i=DSP(isect->dsp, cell[X]+1, cell[Y]+1)) > v)
		v = i;

	return v;
}
#else
#define CELL_MIN(_c)                                                       \
((h=DSP(isect->dsp, _c[X], _c[Y])) < (i=DSP(isect->dsp, _c[X], _c[Y]+1)) ? \
	(h < (i=DSP(isect->dsp, _c[X], _c[Y]+1)) ?                         \
		(h < (i=DSP(isect->dsp, _c[X]+1, _c[Y]+1)) ?		   \
			h : i) :					   \
		(i < (h=DSP(isect->dsp, _c[X]+1, _c[Y]+1)) ?		   \
			i : h) )  :					   \
	(i < (h=DSP(isect->dsp, _c[X], _c[Y]+1)) ?			   \
 		(i < (h=DSP(isect->dsp, _c[X]+1, _c[Y]+1)) ?		   \
			i : h) :					   \
		(h < (i=DSP(isect->dsp, _c[X]+1, _c[Y]+1)) ?		   \
			h : i) ))

#define CELL_MAX(_c)							   \
((h=DSP(isect->dsp, _c[X], _c[Y])) > (i=DSP(isect->dsp, _c[X], _c[Y]+1)) ? \
	(h > (i=DSP(isect->dsp, _c[X], _c[Y]+1)) ?			   \
		(h > (i=DSP(isect->dsp, _c[X]+1, _c[Y]+1)) ?		   \
			h : i) :					   \
		(i > (h=DSP(isect->dsp, _c[X]+1, _c[Y]+1)) ?		   \
			i : h) )  :					   \
	(i > (h=DSP(isect->dsp, _c[X], _c[Y]+1)) ?			   \
 		(i > (h=DSP(isect->dsp, _c[X]+1, _c[Y]+1)) ?		   \
			i : h) :					   \
		(h > (i=DSP(isect->dsp, _c[X]+1, _c[Y]+1)) ?		   \
			h : i) ))
#endif

/*
 *  Once we've determined that the ray hits the bounding box for
 *  the triangles, this routine takes care of the actual triangle intersection
 *  and follow-up.
 *
 *
 */
static void
isect_ray_triangles(isect, cell, 
	curr_pt, next_pt, curr_surf, next_surf, inside)
struct isect_stuff *isect;
int cell[3];
point_t curr_pt, next_pt;
int curr_surf;
int next_surf;
int *inside;
{
	point_t A, B, C, D;
	vect_t AB, AC, AD;
	vect_t N;
	vect_t PA;
	vect_t PAxD;
	double alpha;
	double beta;
	double NdotD;
	double abs_NdotD;
	double NdotPt;
	double	dist;

	/*  C	 D
	 *   *--*
	 *   | /|
	 *   |/ |
	 *   *--*
	 *  A    B
	 */
	VSET(A, cell[X],   cell[Y],   DSP(isect->dsp, cell[X],   cell[Y])  );
	VSET(B, cell[X]+1, cell[Y],   DSP(isect->dsp, cell[X]+1, cell[Y])  );
	VSET(C, cell[X],   cell[Y]+1, DSP(isect->dsp, cell[X],   cell[Y]+1));
	VSET(D, cell[X]+1, cell[Y]+1, DSP(isect->dsp, cell[X]+1, cell[Y]+1));

	VSUB2(AB, B, A);
	VSUB2(AC, C, A);
	VSUB2(AD, D, A);

	VCROSS(N, AB, AD);
	NdotD = VDOT( N, isect->r.r_dir);
	NdotPt = VDOT(N, isect->r.r_pt);

	if (rt_g.debug & DEBUG_HF) {
		FILE *fd;
		static int n=0;
		short h, i;
		char buf[32];
		short cell_min = CELL_MIN(cell);
		short cell_max = CELL_MAX(cell);

		/* plot out the scenario */
		RES_ACQUIRE( &rt_g.res_model);

		sprintf(buf, "dsp%d_1.pl", n++);
		if ((fd=fopen(buf, "w")) != (FILE *)NULL) {
			/* plot the box */
			pl_color(fd, 180, 180, 255);
			pl_3move(fd, cell[X], cell[Y], cell_min);
			pl_3cont(fd, cell[X]+1, cell[Y], cell_min);
			pl_3cont(fd, cell[X]+1, cell[Y]+1, cell_min);
			pl_3cont(fd, cell[X], cell[Y]+1, cell_min);
			pl_3cont(fd, cell[X], cell[Y], cell_min);
			pl_3cont(fd, cell[X], cell[Y], cell_max);
			pl_3cont(fd, cell[X]+1, cell[Y], cell_max);
			pl_3cont(fd, cell[X]+1, cell[Y]+1, cell_max);
			pl_3cont(fd, cell[X], cell[Y]+1, cell_max);
			pl_3cont(fd, cell[X], cell[Y], cell_max);
			pl_3line(fd, cell[X]+1, cell[Y], cell_min,
				     cell[X]+1, cell[Y], cell_max);
			pl_3line(fd, cell[X]+1, cell[Y]+1, cell_min,
				     cell[X]+1, cell[Y]+1, cell_max);
			pl_3line(fd, cell[X], cell[Y]+1, cell_min,
				     cell[X], cell[Y]+1, cell_max);

			/* plot the ray */
			pl_color(fd, 255, 140, 140);
			pdv_3line(fd, curr_pt, next_pt);

			/* plot the triangles */
			pl_color(fd, 140, 255, 140);
			pl_3move(fd, cell[X], cell[Y], DSP(isect->dsp,
				     cell[X], cell[Y]) );
			pl_3cont(fd, cell[X]+1, cell[Y], DSP(isect->dsp,
				     cell[X]+1, cell[Y]) );
			pl_3cont(fd, cell[X]+1, cell[Y]+1, DSP(isect->dsp,
				     cell[X]+1, cell[Y]+1) );
			pl_3cont(fd, cell[X], cell[Y]+1, DSP(isect->dsp,
				     cell[X], cell[Y]+1) );
			pl_3cont(fd, cell[X], cell[Y], DSP(isect->dsp,
				     cell[X], cell[Y]) );
			pl_3cont(fd, cell[X]+1, cell[Y]+1, DSP(isect->dsp,
				     cell[X]+1, cell[Y]+1) );


			fclose(fd);
		}
		RES_RELEASE( &rt_g.res_model);
	}

	abs_NdotD = NdotD >= 0.0 ? NdotD : (-NdotD);
	if (BN_VECT_ARE_PERP(NdotD, isect->tol)) {
		if (rt_g.debug & DEBUG_HF)
			bu_log("miss tri1 (perp)\n");

		goto tri2;
	}

	VSUB2(PA, A, isect->r.r_pt);

	VCROSS(PAxD, PA, isect->r.r_dir); /* perpendicular to PA, D */

	/* project PAxD onto AB */
	alpha = VDOT( AB, PAxD );

	/* because PAxD is perpendicular to PA, we have to 
	 * reverse the projected direction along AB when the ray is 
	 * entering the triangle from the normalward direction
	 */
	if (NdotD < 0.0) alpha = -alpha;
	if (alpha < 0.0) {
		if (rt_g.debug & DEBUG_HF)
			bu_log("miss tri1 (alpha < 0.0)\n");

		goto tri2;
	}
	if (alpha > abs_NdotD ) {
		/* miss triangle */
		if (rt_g.debug & DEBUG_HF)
			bu_log("miss tri1 (alpha > NdotD)\n");

		goto tri2;
	}

	beta = VDOT( AD, PAxD );
	if ( NdotD > 0.0 ) beta = -beta;
	if( beta < 0.0) {
		/* miss triangle */
		if (rt_g.debug & DEBUG_HF)
			bu_log("miss tri1 (beta < 0.0)\n");
		goto tri2;
	}
	if (beta > abs_NdotD ) {
		/* miss triangle */
		if (rt_g.debug & DEBUG_HF)
			bu_log("miss tri1 (beta > NdotD)\n");
		goto tri2;
	}

	if ( alpha+beta > abs_NdotD ) {
		/* miss triangle */
		if (rt_g.debug & DEBUG_HF)
			bu_log("miss tri1 (alpha+beta > NdotD)\n");
		goto tri2;
	}

	dist = VDOT( PA, N );
	if (NdotD < 0.0) {
		/* Entering Solid */
		if (*inside) 
			bu_bomb("Can't enter top of DSP from inside\n");

		if (rt_g.debug & DEBUG_HF)
			bu_log("hit tri1 entering\n");

		RT_GET_SEG( isect->segp, isect->ap->a_resource );
		isect->segp->seg_stp = isect->stp;
		isect->segp->seg_in.hit_dist = dist;

		isect->segp->seg_in.hit_surfno = TRI1;
		isect->segp->seg_in.hit_vpriv[X] = cell[X];
		isect->segp->seg_in.hit_vpriv[Y] = cell[Y];
		*inside = 1;

	} else {
		/* Leaving Solid */
		if (! *inside) 
			bu_bomb("Can't leave top of DSP from outside\n");

		if (rt_g.debug & DEBUG_HF)
			bu_log("hit tri1 leaving\n");

		isect->segp->seg_out.hit_dist = dist;
		isect->segp->seg_out.hit_surfno = TRI1;
		isect->segp->seg_out.hit_vpriv[X] = cell[X];
		isect->segp->seg_out.hit_vpriv[Y] = cell[Y];
		BU_LIST_INSERT( &isect->seglist, &( isect->segp->l) );
		*inside = 0;
	}
tri2:

	VCROSS(N, AD, AC);
	NdotD = VDOT( N, isect->r.r_dir);
	NdotPt = VDOT(N, isect->r.r_pt);

	abs_NdotD = NdotD >= 0.0 ? NdotD : (-NdotD);
	if (BN_VECT_ARE_PERP(NdotD, isect->tol)) {
		if (rt_g.debug & DEBUG_HF)
			bu_log("miss tri2 (perp)\n");
		return;
	}

	alpha = VDOT( AD, PAxD );
	/* because PAxD is perpendicular to PA, we have to 
	 * reverse the projected direction along AB when the ray is 
	 * entering the triangle from the normalward direction
	 */
	if (NdotD < 0.0) alpha = -alpha;
	if (alpha < 0.0) {
		/* miss triangle */
		if (rt_g.debug & DEBUG_HF)
			bu_log("miss tri2 (alpha < 0.0)\n");
		return;
	}
	if (alpha > abs_NdotD ) {
		/* miss triangle */
		if (rt_g.debug & DEBUG_HF)
			bu_log("miss tri2 (alpha > NdotD)\n");
		return;
	}

	beta = VDOT( AC, PAxD );
	if ( NdotD > 0.0 ) beta = -beta;
	if (beta < 0.0) {
		/* miss triangle */
		if (rt_g.debug & DEBUG_HF)
			bu_log("miss tri2 (beta < 0.0)\n");

		return;
	}
	if (beta > abs_NdotD ) {
		/* miss triangle */
		if (rt_g.debug & DEBUG_HF)
			bu_log("miss tri2 (beta > NdotD)\n");
		return;
	}

	dist = VDOT( PA, N );
	if (NdotD < 0.0) {
		/* Entering Solid */
		if (*inside) 
			bu_bomb("Can't enter top of DSP from inside\n");

		if (rt_g.debug & DEBUG_HF)
			bu_log("hit tri2 entering\n");

		RT_GET_SEG( isect->segp, isect->ap->a_resource );
		isect->segp->seg_stp = isect->stp;
		isect->segp->seg_in.hit_dist = dist;

		isect->segp->seg_in.hit_surfno = TRI2;
		isect->segp->seg_in.hit_vpriv[X] = cell[X];
		isect->segp->seg_in.hit_vpriv[Y] = cell[Y];
		*inside = 1;

	} else {
		/* Leaving Solid */
		if (! *inside) 
			bu_bomb("Can't leave top of DSP from outside\n");

		if (rt_g.debug & DEBUG_HF)
			bu_log("hit tri2 leaving\n");
		isect->segp->seg_out.hit_dist = dist;
		isect->segp->seg_out.hit_surfno = TRI2;
		isect->segp->seg_out.hit_vpriv[X] = cell[X];
		isect->segp->seg_out.hit_vpriv[Y] = cell[Y];
		BU_LIST_INSERT( &isect->seglist, &( isect->segp->l) );
		*inside = 0;
	}

}


/* These macros are for the isect_ray_dsp routine.  These exist as macros
 * only because we can't do inline subroutines in C, and we don't want to
 * pay the overhead of the subroutine call.
 */

#define IN_BASEX(_i, _inside)                                               \
	if (!_inside) {							    \
        /* if (grid_cell[X] != 0 && grid_cell[X] != XCNT) bu_log("Oops, inside DSP without crossing boundary\n"); */ \
		RT_GET_SEG( (_i)->segp, (_i)->ap->a_resource );		    \
		(_i)->segp->seg_stp = (_i)->stp;			    \
		(_i)->segp->seg_in.hit_dist = tX;			    \
                                                                            \
		(_i)->segp->seg_in.hit_surfno = insurfX;		    \
		(_i)->segp->seg_in.hit_vpriv[X] = grid_cell[X];		    \
		(_i)->segp->seg_in.hit_vpriv[Y] = grid_cell[Y];		    \
		_inside = 1;						    \
	}								    \
	(_i)->segp->seg_out.hit_dist = tX + tDX;			    \
	(_i)->segp->seg_out.hit_surfno = outsurfX;			    \
	(_i)->segp->seg_in.hit_vpriv[X] = grid_cell[X];			    \
	(_i)->segp->seg_in.hit_vpriv[Y] = grid_cell[Y]

#define IN_BASEY(_i, _inside)						    \
	if (!_inside) {							    \
									    \
/* bu_log("Oops, inside DSP without crossing boundary\n"); */		    \
		RT_GET_SEG( (_i)->segp, (_i)->ap->a_resource );             \
		(_i)->segp->seg_stp = (_i)->stp;                            \
		(_i)->segp->seg_in.hit_dist = tY;                           \
									    \
		(_i)->segp->seg_in.hit_surfno = insurfY;                    \
		(_i)->segp->seg_in.hit_vpriv[X] = grid_cell[X];             \
		(_i)->segp->seg_in.hit_vpriv[Y] = grid_cell[Y];             \
		_inside = 1;						    \
	}								    \
	(_i)->segp->seg_out.hit_dist = tY + tDY;			    \
	(_i)->segp->seg_out.hit_surfno = outsurfY;                          \
	(_i)->segp->seg_in.hit_vpriv[X] = grid_cell[X];                     \
	(_i)->segp->seg_in.hit_vpriv[Y] = grid_cell[Y]

#define MISS_ABOVE(_i, _inside)                                     	    \
	if (_inside) {                                     		    \
		BU_LIST_INSERT( &(_i)->seglist, &( (_i)->segp->l) );        \
		_inside = 0;                                     	    \
	}

#define ISECT_ENTRY_WALL()                                     		    \
{	register double slope;                                     	    \
	/* determine if we are starting out "under/inside" the surface      \
	 * or "above/outside" the surface.                                  \
	 */                                     			    \
	switch (curr_pt_surf) {                                     	    \
	case BBSURF(XMIN):                                     		    \
		h = grid_cell[X];                                     	    \
                                     					    \
		i = DSP(isect->dsp, h, grid_cell[Y]);                       \
		slope = (next_pt[Z] - i) / (next_pt[Y] - grid_cell[Y]);     \
                                     					    \
		i = DSP(isect->dsp, h, grid_cell[Y]+1) - i;                 \
                                     					    \
		if (slope < i) {                                     	    \
			/* Entering cell under DSP top */                   \
			if (!inside) {                                      \
				if (rt_g.debug & DEBUG_HF) \
					bu_log("entering XMIN wall\n"); \
				RT_GET_SEG( isect->segp, isect->ap->a_resource ); \
				isect->segp->seg_stp = isect->stp; \
				isect->segp->seg_in.hit_dist = curr_pt_dist; \
					                                     \
				isect->segp->seg_in.hit_surfno = BBSURF(XMIN); \
				isect->segp->seg_in.hit_vpriv[X] = grid_cell[X]; \
				isect->segp->seg_in.hit_vpriv[Y] = grid_cell[Y]; \
				inside = 1;                                 \
			}                           		            \
		} else { \
			if (rt_g.debug & DEBUG_HF) \
				bu_log("entering cell above DSP at XMIN wall\n"); \
		}			                                    \
		break;                  		                    \
	case BBSURF(XMAX):                                		    \
		h = grid_cell[X]+1;                                         \
					                                    \
		i = DSP(isect->dsp, h, grid_cell[Y]);                       \
		slope = (next_pt[Z] - i) / (next_pt[Y] - grid_cell[Y]);     \
		i = (DSP(isect->dsp, h, grid_cell[Y]+1) - i) /* / 1.0 */;   \
		if (slope < i ) {                                 	    \
			/* Entering cell under DSP top */                   \
			if (!inside) {                                      \
				if (rt_g.debug & DEBUG_HF) \
					bu_log("entering XMAX wall\n"); \
				RT_GET_SEG( isect->segp, isect->ap->a_resource ); \
				isect->segp->seg_stp = isect->stp; \
				isect->segp->seg_in.hit_dist = curr_pt_dist; \
                                					    \
				isect->segp->seg_in.hit_surfno = BBSURF(XMAX); \
				isect->segp->seg_in.hit_vpriv[X] = grid_cell[X]; \
				isect->segp->seg_in.hit_vpriv[Y] = grid_cell[Y]; \
				inside = 1;                    		    \
			}                                 		    \
		} else {                               			    \
			if (rt_g.debug & DEBUG_HF) \
				bu_log("entering cell above DSP at XMAX wall\n"); \
		}			                                    \
		break;                               			    \
	case BBSURF(YMIN):                                		    \
		h = grid_cell[Y];                                	    \
					                                    \
		i = DSP(isect->dsp, grid_cell[X], h);                       \
		slope = (next_pt[Z] - i) / (next_pt[X] - grid_cell[X]);     \
		i = (DSP(isect->dsp, grid_cell[X]+1, h) - i) /* / 1.0 */;   \
		if (slope < i ) {                                 	    \
			/* Entering cell under DSP top */                   \
			if (!inside) {                                      \
				if (rt_g.debug & DEBUG_HF) \
					bu_log("entering @ YMIN wall\n"); \
				RT_GET_SEG( isect->segp, isect->ap->a_resource ); \
				isect->segp->seg_stp = isect->stp;          \
				isect->segp->seg_in.hit_dist = curr_pt_dist; \
					                                    \
				isect->segp->seg_in.hit_surfno = BBSURF(YMIN); \
				isect->segp->seg_in.hit_vpriv[X] = grid_cell[X]; \
				isect->segp->seg_in.hit_vpriv[Y] = grid_cell[Y]; \
				inside = 1;				    \
			}                                  		    \
		} else { \
			if (rt_g.debug & DEBUG_HF) \
				bu_log("entering cell above DSP @ YMIN wall\n"); \
		}                                			    \
		break;                                 			    \
	case BBSURF(YMAX):                                		    \
		h = grid_cell[Y]+1;                                 	    \
					                                    \
		i = DSP(isect->dsp, grid_cell[X], h);			    \
		slope = (next_pt[Z] - i) / (next_pt[X] - grid_cell[X]);     \
		i = DSP(isect->dsp, grid_cell[X]+1, h) - i;                 \
		if (slope < i ) {                                 	    \
			/* Entering cell under DSP top */                   \
			if (!inside) {                                      \
				if (rt_g.debug & DEBUG_HF) \
					bu_log("entering @ YMIN wall\n"); \
				RT_GET_SEG( isect->segp, isect->ap->a_resource );                                     \
				isect->segp->seg_stp = isect->stp;          \
				isect->segp->seg_in.hit_dist = curr_pt_dist;\
                                 					    \
				isect->segp->seg_in.hit_surfno = BBSURF(YMAX);  \
				isect->segp->seg_in.hit_vpriv[X] = grid_cell[X]; \
				isect->segp->seg_in.hit_vpriv[Y] = grid_cell[Y]; \
				inside = 1;                                 \
			}                                 		    \
		} else { \
			if (rt_g.debug & DEBUG_HF) \
				bu_log("entering cell above DSP @ YMIN wall\n"); \
		}                                 			    \
		break;                                			    \
	case BBSURF(ZMIN):                                 		    \
		/* Entering cell under DSP top if rising */                 \
		if (!rising)                                 		    \
			bu_bomb("How can I be sinking and entering a cell at ZMIN?\n"); \
                                					    \
		if (!inside) {                                		    \
			if (rt_g.debug & DEBUG_HF) \
				bu_log("hit bottom risinging\n"); \
			RT_GET_SEG( isect->segp, isect->ap->a_resource ); \
			isect->segp->seg_stp = isect->stp;          \
			isect->segp->seg_in.hit_dist = curr_pt_dist;\
                                					    \
			isect->segp->seg_in.hit_surfno = BBSURF(ZMIN); \
			isect->segp->seg_in.hit_vpriv[X] = grid_cell[X]; \
			isect->segp->seg_in.hit_vpriv[Y] = grid_cell[Y]; \
			inside = 1;				    \
		}                                 			    \
                                  					    \
		break;                                			    \
	case BBSURF(ZMAX):                                 		    \
		/* entering from above */                                   \
		if (rising)                                 		    \
			bu_bomb("How can I be rising and entering a cell at ZMAX?\n"); \
\
		if (rt_g.debug & DEBUG_HF) \
			bu_log("hit top descending\n"); \
                                					    \
		/* do nothing because we haven't entered through a          \
		 * real surface                                 	    \
		 */                                 			    \
		break;                                			    \
	}                                 				    \
}


#define ISECT_X_EXIT_WALL(isect, grid_cell)                                 \
{	register short h, i;                                 		    \
	register double slope;                                  	    \
                                 					    \
	if (next_pt_surf == BBSURF(XMIN) ) h = grid_cell[X];                \
	else h = grid_cell[X]+1;                                	    \
                                					    \
	i = DSP(isect->dsp, h, grid_cell[Y]);                               \
	slope = (next_pt[Z] - i) / (next_pt[Y] - grid_cell[Y]);             \
	if (slope < DSP(isect->dsp, h, grid_cell[Y]+1) - i) {               \
	    	/* exiting cell while inside solid */                       \
		if (inside) {                                 		    \
			if (rt_g.debug & DEBUG_HF) \
				bu_log("exiting cell X dir inside DSP\n"); \
			isect->segp->seg_out.hit_dist = tX;                 \
			isect->segp->seg_out.hit_surfno = outsurfX;         \
			isect->segp->seg_in.hit_vpriv[X] = grid_cell[X];    \
			isect->segp->seg_in.hit_vpriv[Y] = grid_cell[Y];    \
		} else {                                 		    \
		    bu_log("exiting DSP cell in solid without existing seg\n"); \
		}                                 			    \
	}                                 				    \
}



#define ISECT_Y_EXIT_WALL(isect, grid_cell)                                 \
{	register short h, i;                                 		    \
	register double slope;                                 		    \
                                 					    \
	if (next_pt_surf == BBSURF(YMIN) ) h = grid_cell[Y];                \
	else h = grid_cell[Y]+1;                                 	    \
                                 					    \
	i = DSP(isect->dsp, grid_cell[X], h);                               \
	slope = (next_pt[Z] - i) / (next_pt[X] - grid_cell[X]);             \
	if (slope < DSP(isect->dsp, grid_cell[X]+1, h) - i) {               \
	    	/* exiting cell while inside solid */                       \
		if (inside) {                                  		    \
			if (rt_g.debug & DEBUG_HF) \
				bu_log("exiting cell Y dir inside DSP\n"); \
			isect->segp->seg_out.hit_dist = tY;                 \
			isect->segp->seg_out.hit_surfno = outsurfY;         \
			isect->segp->seg_in.hit_vpriv[X] = grid_cell[X];    \
			isect->segp->seg_in.hit_vpriv[Y] = grid_cell[Y];    \
		} else {                                  		    \
		    bu_log("exiting DSP cell in solid without existing seg\n"); \
		}                                 			    \
	}                                 				    \
}




static void
isect_ray_dsp(isect)
struct isect_stuff *isect;
{
	point_t curr_pt;
	int	curr_pt_surf;
	double	curr_pt_dist;
	point_t next_pt;
	int	next_pt_surf;
	int	inside = 0;	/* currently have a valid seg we're building */
	int	grid_cell[3];	/* grid cell of current point */

	point_t out_pt;
	int	out_cell[3];	/* grid cell of last point in bbox */
	int	rising;		/* boolean:  Ray Z dir sign is positive */
	int	stepX, stepY;	/* signed step delta for grid cell marching */
	int	outX, outY;	/* cell index which idicates we're outside */
	int	insurfX, outsurfX;
	int	insurfY, outsurfY;
	vect_t	pDX, pDY;	/* vector for 1 cell change in x, y */
	double	tDX;		/* dist along ray to span 1 cell in X dir */
	double	tDY;		/* dist along ray to span 1 cell in Y dir */

	/* tmp values for various macros including:
	 *	CELL_MIN, CELL_MAX
	 *	ISECT_ENTRY_WALL()
	 *	ISECT_{XY}_EXIT_WALL()
	 */
	short	h, i;		

	double	out_dist = isect->bbox.out_dist;
	double	dsp_max = isect->dsp->dsp_pl_dist[ZMAX];
				
	double	tX, tY;	/* dist along ray from hit pt. to next cell boundary */
	/* since we're probably starting in the middle of a cell, we need
	 * to compute the distance along the ray to the initial
	 * X and Y boundaries.
	 */


	/* compute BBox entry point and starting grid cell */
	curr_pt_dist = isect->bbox.in_dist;
	VJOIN1(curr_pt, isect->r.r_pt, curr_pt_dist, isect->r.r_dir);
	curr_pt_surf = isect->bbox.in_surf;

	VMOVE(grid_cell, curr_pt);	/* int/float conversion */

	VJOIN1(out_pt, isect->r.r_pt, isect->bbox.out_dist, isect->r.r_dir);
	VMOVE(out_cell, out_pt);	/* int/float conversion */

	rising = (isect->r.r_dir[Z] > 0.); /* compute Z direction */

	if (isect->r.r_dir[X] < 0.0) {
		stepX = -1;	/* cell delta for stepping X dir on ray */
		outX = -1;	/* cell beyond last valid in X direction */
		insurfX = BBSURF(XMAX);
		outsurfX = BBSURF(XMIN);


		/* tDX is the distance along the ray we have to travel
		 * to traverse a cell (travel a unit distance) along the
		 * X axis of the grid
		 */
		tDX = -1.0 / isect->r.r_dir[X];

		/* tX is the distance along the ray from the initial hit point
		 * to the first cell boundary in the X direction
		 */
		tX = (grid_cell[X] - curr_pt[X]) / isect->r.r_dir[X];
/*		if (tX == 0.0) tX = tDX; */
	} else {
		stepX = 1;
		outX = isect->dsp->dsp_i.dsp_xcnt;
		insurfX = BBSURF(XMIN);
		outsurfX = BBSURF(XMAX);

		tDX = 1.0 / isect->r.r_dir[X];

		if (isect->r.r_dir[X] > 0.0) {
			tX = ((grid_cell[X]+1) - curr_pt[X]) / isect->r.r_dir[X];
/*			if (tX == 0.0) tX = tDX; */
		} else
			tX = MAX_FASTF;
	}
	if (isect->r.r_dir[Y] < 0) {
		stepY = -1;
		outY = -1;
		insurfY = BBSURF(YMAX);
		outsurfY = BBSURF(YMIN);

		tDY = -1.0 / isect->r.r_dir[Y];

		tY = (grid_cell[Y] - curr_pt[Y]) / isect->r.r_dir[Y];
/*		if (tY == 0.0) tX = tDY; */

	} else {
		stepY = 1;
		outY = isect->dsp->dsp_i.dsp_ycnt;
		insurfY = BBSURF(YMIN);
		outsurfY = BBSURF(YMAX);

		tDY = 1.0 / isect->r.r_dir[Y];

		if (isect->r.r_dir[Y] > 0.0) {
			tY = ((grid_cell[Y]+1) - curr_pt[Y]) / isect->r.r_dir[Y];
/*			if (tY == 0.0) tX = tDY; */
		} else
			tY = MAX_FASTF;
	}

	VSCALE(pDX, isect->r.r_dir, tDX);	/* vect for going tDX dist */
	VSCALE(pDY, isect->r.r_dir, tDY);

	
	if (rt_g.debug & DEBUG_HF) {
		bu_log(" in cell(%d,%d)  pt(%g %g %g)\n",
			grid_cell[X], grid_cell[Y], V3ARGS(curr_pt));
		bu_log("out cell(%d,%d)  pt(%g %g %g)\n",
			out_cell[X], out_cell[Y], V3ARGS(out_pt));

		bu_log("stepX:%d outX:%d  tDX:%g tX:%g\n",
			stepX, outX, tDX, tX);
		bu_log("stepY:%d outY:%d  tDY:%g tY:%g\n",
			stepY, outY, tDY, tY);
	}





	do {
		if (rt_g.debug & DEBUG_HF)
			bu_log("cell(%d,%d) tX:%g tY:%g\n\t  curr_pt(%g %g %g)\n\t",
				grid_cell[X], grid_cell[Y], tX, tY, V3ARGS(curr_pt));

		if (tX < tY) {
			/* dist along ray to next X cell boundary is closer
			 * than dist to next Y cell boundary
			 */
			VJOIN1(next_pt, curr_pt, tX, isect->r.r_dir);
			next_pt_surf = outsurfX;


			if (rt_g.debug & DEBUG_HF) {
				VPRINT("X next_pt", next_pt);
			}
			if (rising) {
				if (next_pt[Z] < CELL_MIN(grid_cell)) {
					/* in base */
					if (rt_g.debug & DEBUG_HF) bu_log("in base\n");

					IN_BASEX(isect, inside);

				} else if (curr_pt[Z] > CELL_MAX(grid_cell)) {
					/* miss above */
					if (rt_g.debug & DEBUG_HF) bu_log("miss above\n");
					MISS_ABOVE(isect, inside);
				} else {
					/* intersect */
					if (rt_g.debug & DEBUG_HF) bu_log("intersect %d %d\n", CELL_MIN(grid_cell), CELL_MAX(grid_cell));

					ISECT_ENTRY_WALL();
					isect_ray_triangles(isect, grid_cell,
						curr_pt, next_pt,
						curr_pt_surf, next_pt_surf,
						&inside);

					ISECT_X_EXIT_WALL(isect, grid_cell);
				}
			} else {
				if (next_pt[Z] > CELL_MAX(grid_cell)) {
					/* miss above */
					if (rt_g.debug & DEBUG_HF) bu_log("miss above\n");
					MISS_ABOVE(isect, inside);
				} else if (curr_pt[Z] < CELL_MIN(grid_cell)){
					/* in base */
					if (rt_g.debug & DEBUG_HF) bu_log("in base\n");
					IN_BASEX(isect, inside);
				} else {
					/* intersect */
					if (rt_g.debug & DEBUG_HF) bu_log("intersect %d %d\n", CELL_MIN(grid_cell), CELL_MAX(grid_cell));

					ISECT_ENTRY_WALL();
					isect_ray_triangles(isect, grid_cell,
						curr_pt, next_pt,
						curr_pt_surf, next_pt_surf,
						&inside);

					ISECT_X_EXIT_WALL(isect, grid_cell);
				}
			}

			/* step to next cell in X direction */
			VMOVE(curr_pt, next_pt);
			grid_cell[X] += stepX;
			curr_pt_surf = insurfX;
			curr_pt_dist = tX;
			
			/* update dist along ray to next X cell boundary */
			tX += tDX;
		} else {
			/* dist along ray to next Y cell boundary is closer
			 * than dist to next X cell boundary
			 */
			VJOIN1(next_pt, curr_pt, tY, isect->r.r_dir);
			next_pt_surf = outsurfY;

			if (rt_g.debug & DEBUG_HF) {
				VPRINT("Y next_pt", next_pt);
			}
			if (rising) {
				if (next_pt[Z] < CELL_MIN(grid_cell)) {
					/* in base */
					if (rt_g.debug & DEBUG_HF) bu_log("in base\n");
					IN_BASEY(isect, inside);

				} else if (curr_pt[Z] > CELL_MAX(grid_cell)) {
					/* miss above */
					if (rt_g.debug & DEBUG_HF) bu_log("miss above\n");
					MISS_ABOVE(isect, inside);
				} else {
					/* intersect */
					if (rt_g.debug & DEBUG_HF) bu_log("intersect %d %d\n", CELL_MIN(grid_cell), CELL_MAX(grid_cell));

					ISECT_ENTRY_WALL();
					isect_ray_triangles(isect, grid_cell,
						curr_pt, next_pt,
						curr_pt_surf, next_pt_surf,
						&inside);
					ISECT_Y_EXIT_WALL(isect, grid_cell);
				}
			} else {
				if (next_pt[Z] > CELL_MAX(grid_cell)) {
					/* miss above */
					if (rt_g.debug & DEBUG_HF) bu_log("miss above\n");
					MISS_ABOVE(isect, inside);
				} else if (curr_pt[Z] < CELL_MIN(grid_cell)){
					/* in base */
					if (rt_g.debug & DEBUG_HF) bu_log("in base\n");
					IN_BASEY(isect, inside);
				} else {
					/* intersect */
					if (rt_g.debug & DEBUG_HF)
						bu_log("intersect %d %d\n",
							CELL_MIN(grid_cell),
							CELL_MAX(grid_cell));

					ISECT_ENTRY_WALL();
					isect_ray_triangles(isect, grid_cell,
						curr_pt, next_pt,
						curr_pt_surf, next_pt_surf,
						&inside);
					ISECT_Y_EXIT_WALL(isect, grid_cell);
				}
			}

			/* step to next cell in Y direction */
			VMOVE(curr_pt, next_pt);
			grid_cell[Y] += stepY;
			curr_pt_surf = insurfY;
			curr_pt_dist = tY;
			
			/* update dist along ray to next Y cell boundary */
			tY += tDY;
		}


	} while ( grid_cell[X] != outX && grid_cell[Y] != outY &&
		  (tX < out_dist+tDX || tY < out_dist+tDY) ||
		  (rising && curr_pt[Z] < dsp_max) ||
		 (!rising && curr_pt[Z] > 0.0) );
		

	if (inside) {
		BU_LIST_INSERT( &isect->seglist, &( isect->segp->l) );
		inside = 0;
	}

}






/*
 *  			R T _ D S P _ S H O T
 *  
 *  Intersect a ray with a dsp.
 *  If an intersection occurs, a struct seg will be acquired
 *  and filled in.
 *  
 *  Returns -
 *  	0	MISS
 *	>0	HIT
 */
int
rt_dsp_shot( stp, rp, ap, seghead )
struct soltab		*stp;
register struct xray	*rp;
struct application	*ap;
struct seg		*seghead;
{
	register struct dsp_specific *dsp =
		(struct dsp_specific *)stp->st_specific;
	register struct seg *segp;
	int	i;
	vect_t	dir, v;
	struct isect_stuff isect;

	if (rt_g.debug & DEBUG_HF)
		bu_log("rt_dsp_shot(pt:(%g %g %g)\n\tdir[%g]:(%g %g %g))\n",
			V3ARGS(rp->r_pt),
			MAGNITUDE(rp->r_dir),
			V3ARGS(rp->r_dir));

	RT_DSP_CK_MAGIC(dsp);
	BU_CK_MAPPED_FILE(dsp->dsp_i.dsp_mp);

	/* map the ray into the coordinate system of the dsp */
	MAT4X3PNT(isect.r.r_pt, dsp->dsp_i.dsp_mtos, rp->r_pt);
	MAT4X3VEC(dir, dsp->dsp_i.dsp_mtos, rp->r_dir);
	VMOVE(isect.r.r_dir, dir);
	VUNITIZE(isect.r.r_dir);

	if (rt_g.debug & DEBUG_HF) {
		bn_mat_print("mtos", dsp->dsp_i.dsp_mtos);
		bu_log("Solid space ray pt:(%g %g %g)\n\tdir[%g]:[%g %g %g]\n\tu_dir(%g %g %g)\n",
			V3ARGS(isect.r.r_pt),
			MAGNITUDE(dir),
			V3ARGS(dir),
			V3ARGS(isect.r.r_dir));

	}

	isect.ap = ap;
	isect.stp = stp;
	isect.dsp = (struct dsp_specific *)stp->st_specific;
	isect.tol = &ap->a_rt_i->rti_tol;

	/* intersect ray with bounding cube */
	if ( isect_ray_bbox(&isect) == 0)
		return 0;


	BU_LIST_INIT(&isect.seglist);

	if (isect.minbox.in_surf < 5 && isect.minbox.out_surf < 5 ) {

		/*  We actually hit and exit the dsp BELOW the min value.
		 *  For example:
		 *			     o ray origin
		 *	     /\dsp	    /
		 *	   _/  \__   /\_   /
		 *	_ |       \_/   | / ray
		 *	| |		|/
		 * Minbox |		*in point
		 *	| |	       /|
		 *	- +-----------*-+
		 *		     / out point
		 */

		RT_GET_SEG( isect.segp, isect.ap->a_resource );
		isect.segp->seg_stp = isect.stp;
		isect.segp->seg_in.hit_dist = isect.minbox.in_dist;
		isect.segp->seg_in.hit_surfno = BBSURF(isect.minbox.in_surf);

		isect.segp->seg_out.hit_dist = isect.minbox.out_dist;
		isect.segp->seg_out.hit_surfno = BBSURF(isect.minbox.out_surf);

		BU_LIST_INSERT( &isect.seglist, &(isect.segp->l) );
	} else {

		/* intersect ray with dsp data */
		isect_ray_dsp(&isect);
	}

	/* if we missed it all, give up now */
	if (BU_LIST_IS_EMPTY(&isect.seglist))
		return 0;

	/* map hit distances back to model space */
	i = 0;
	for (BU_LIST_FOR(segp, seg, &isect.seglist)) {
		i += 2;
		if (rt_g.debug & DEBUG_HF) {
			bu_log("solid in:%6g out:%6g\n",
				segp->seg_in.hit_dist,
				segp->seg_out.hit_dist);
		}

		VSCALE(dir, isect.r.r_dir, segp->seg_in.hit_dist);
		MAT4X3VEC(v, dsp->dsp_i.dsp_stom, dir)
		segp->seg_in.hit_dist = MAGNITUDE(v);

		VSCALE(dir, isect.r.r_dir, segp->seg_out.hit_dist);
		MAT4X3VEC(v, dsp->dsp_i.dsp_stom, dir)
		segp->seg_out.hit_dist = MAGNITUDE(v);

		if (rt_g.debug & DEBUG_HF) {
			bu_log("model in:%6g out:%6g\n",
			segp->seg_in.hit_dist,
			segp->seg_out.hit_dist);
		}
	}

	if (rt_g.debug & DEBUG_HF) {
		double NdotD;
		double d;
		static CONST plane_t plane = {0.0, 0.0, -1.0, 0.0};

		NdotD = VDOT(plane, rp->r_dir);
		d = - ( (VDOT(plane, rp->r_pt) - plane[H]) / NdotD);	
		bu_log("rp -> Z=0 dist: %g\n", d);
	}

	/* transfer list of hitpoints */
	BU_LIST_APPEND_LIST( &(seghead->l), &isect.seglist);

	return i;
}

#define RT_DSP_SEG_MISS(SEG)	(SEG).seg_stp=RT_SOLTAB_NULL

/*
 *			R T _ D S P _ V S H O T
 *
 *  Vectorized version.
 */
void
rt_dsp_vshot( stp, rp, segp, n, ap )
struct soltab	       *stp[]; /* An array of solid pointers */
struct xray		*rp[]; /* An array of ray pointers */
struct  seg            segp[]; /* array of segs (results returned) */
int		  	    n; /* Number of ray/object pairs */
struct application	*ap;
{
	if (rt_g.debug & DEBUG_HF)
		bu_log("rt_dsp_vshot()\n");

	rt_vstub( stp, rp, segp, n, ap );
}

/*
 *  			R T _ D S P _ N O R M
 *  
 *  Given ONE ray distance, return the normal and entry/exit point.
 */
void
rt_dsp_norm( hitp, stp, rp )
register struct hit	*hitp;
struct soltab		*stp;
register struct xray	*rp;
{
	register struct dsp_specific *dsp =
		(struct dsp_specific *)stp->st_specific;
	vect_t N, T;
	int cell[2];

	if (rt_g.debug & DEBUG_HF)
		bu_log("rt_dsp_norm()\n");

	VJOIN1( hitp->hit_point, rp->r_pt, hitp->hit_dist, rp->r_dir );
	if (hitp->hit_surfno < 0) {
		MAT4X3VEC( N, dsp->dsp_i.dsp_stom, 
			dsp_pl[ BBSURF(hitp->hit_surfno) ] );
	} else if ( hitp->hit_surfno == TRI1 ) {
		cell[X] = hitp->hit_vpriv[X];
		cell[Y] = hitp->hit_vpriv[Y];
		
		VSET(T, .5, 0.0, 1.0);
		MAT4X3VEC(N, dsp->dsp_i.dsp_stom, T);

	} else if ( hitp->hit_surfno == TRI2 ) {
		cell[X] = hitp->hit_vpriv[X];
		cell[Y] = hitp->hit_vpriv[Y];

		VSET(T, 0.0, .5, 1.0);
		MAT4X3VEC(N, dsp->dsp_i.dsp_stom, T);

	} else {
		bu_bomb("bogus surface of DSP\n");
	}

	VUNITIZE(N);
	VMOVE(hitp->hit_normal, N);

	
	if (rt_g.debug & DEBUG_HF)
		bu_log("surf[%d] pt(%g %g %g) Norm[%g %g %g](%g %g %g)\n",
			hitp->hit_surfno,
			V3ARGS(hitp->hit_point),
			V3ARGS(dsp_pl[ hitp->hit_surfno ]),
			V3ARGS(N));
}

/*
 *			R T _ D S P _ C U R V E
 *
 *  Return the curvature of the dsp.
 */
void
rt_dsp_curve( cvp, hitp, stp )
register struct curvature *cvp;
register struct hit	*hitp;
struct soltab		*stp;
{
	register struct dsp_specific *dsp =
		(struct dsp_specific *)stp->st_specific;

	if (rt_g.debug & DEBUG_HF)
		bu_log("rt_dsp_curve()\n");

 	cvp->crv_c1 = cvp->crv_c2 = 0;

	/* any tangent direction */
 	vec_ortho( cvp->crv_pdir, hitp->hit_normal );
}

/*
 *  			R T _ D S P _ U V
 *  
 *  For a hit on the surface of an dsp, return the (u,v) coordinates
 *  of the hit point, 0 <= u,v <= 1.
 *  u = azimuth
 *  v = elevation
 */
void
rt_dsp_uv( ap, stp, hitp, uvp )
struct application	*ap;
struct soltab		*stp;
register struct hit	*hitp;
register struct uvcoord	*uvp;
{
	register struct dsp_specific *dsp =
		(struct dsp_specific *)stp->st_specific;

	if (rt_g.debug & DEBUG_HF)
		bu_log("rt_dsp_uv()\n");

	uvp->uv_u = 0.0;
	uvp->uv_v = 0.0;
	uvp->uv_du = 0.0;
	uvp->uv_dv = 0.0;
}

/*
 *		R T _ D S P _ F R E E
 */
void
rt_dsp_free( stp )
register struct soltab *stp;
{
	register struct dsp_specific *dsp =
		(struct dsp_specific *)stp->st_specific;

	if (rt_g.debug & DEBUG_HF)
		bu_log("rt_dsp_free()\n");

	RT_CK_MAPPED_FILE(dsp->dsp_i.dsp_mp);
	rt_close_mapped_file(dsp->dsp_i.dsp_mp);

	rt_free( (char *)dsp, "dsp_specific" );
}

/*
 *			R T _ D S P _ C L A S S
 */
int
rt_dsp_class()
{
	if (rt_g.debug & DEBUG_HF)
		bu_log("rt_dsp_class()\n");

	return(0);
}

/*
 *			R T _ D S P _ P L O T
 */
int
rt_dsp_plot( vhead, ip, ttol, tol )
struct rt_list		*vhead;
struct rt_db_internal	*ip;
CONST struct rt_tess_tol *ttol;
struct rt_tol		*tol;
{
	struct rt_dsp_internal	*dsp_ip =
		(struct rt_dsp_internal *)ip->idb_ptr;
	point_t m_pt;
	point_t s_pt;
	int x, y;
	int step;
	int xlim = dsp_ip->dsp_xcnt - 1;
	int ylim = dsp_ip->dsp_ycnt - 1;
	int xfudge, yfudge;

	if (rt_g.debug & DEBUG_HF)
		bu_log("rt_dsp_plot()\n");

	RT_CK_DB_INTERNAL(ip);
	RT_DSP_CK_MAGIC(dsp_ip);


#define MOVE() \
	MAT4X3PNT(m_pt, dsp_ip->dsp_stom, s_pt); \
	RT_ADD_VLIST( vhead, m_pt, RT_VLIST_LINE_MOVE )

#define DRAW() \
	MAT4X3PNT(m_pt, dsp_ip->dsp_stom, s_pt); \
	RT_ADD_VLIST( vhead, m_pt, RT_VLIST_LINE_DRAW )


	/* Draw the Bottom */
	VSETALL(s_pt, 0.0);
	MOVE();

	s_pt[X] = xlim;
	DRAW();

	s_pt[Y] = ylim;
	DRAW();

	s_pt[X] = 0.0;
	DRAW();

	s_pt[Y] = 0.0;
	DRAW();


	/* Draw the corners */
	s_pt[Z] = DSP(dsp_ip, 0, 0);
	DRAW();

	VSET(s_pt, xlim, 0.0, 0.0);
	MOVE();
	s_pt[Z] = DSP(dsp_ip, xlim, 0);
	DRAW();


	VSET(s_pt, xlim, ylim, 0.0);
	MOVE();
	s_pt[Z] = DSP(dsp_ip, xlim, ylim);
	DRAW();

	VSET(s_pt, 0.0, ylim, 0.0);
	MOVE();
	s_pt[Z] = DSP(dsp_ip, 0, ylim);
	DRAW();


	/* Draw the outside line of the top 
	 * We draw the four sides of the top at full resolution.
	 * This helps edge matching.
	 * The inside of the top, we draw somewhat coarser
	 */
	for (y=0 ; y < dsp_ip->dsp_ycnt ; y += ylim ) {
		VSET(s_pt, 0.0, y, DSP(dsp_ip, 0, y));
		MOVE();

		for (x=0 ; x < dsp_ip->dsp_xcnt ; x++) {
			s_pt[X] = x;
			s_pt[Z] = DSP(dsp_ip, x, y);
			DRAW();
		}
	}


	for (x=0 ; x < dsp_ip->dsp_xcnt ; x += xlim ) {
		VSET(s_pt, x, 0.0, DSP(dsp_ip, x, 0));
		MOVE();

		for (y=0 ; y < dsp_ip->dsp_ycnt ; y++) {
			s_pt[Y] = y;
			s_pt[Z] = DSP(dsp_ip, x, y);
			DRAW();
		}
	}

	/* now draw the body of the top */
	if( ttol->rel )  {
		int	rstep;
		rstep = dsp_ip->dsp_xcnt;
		V_MAX( rstep, dsp_ip->dsp_ycnt );
		step = (int)(ttol->rel * rstep);
	} else { 
		int goal = 10000;
		goal -= 5;
		goal -= 8 + 2 * (dsp_ip->dsp_xcnt+dsp_ip->dsp_ycnt);

		if (goal <= 0) return 0;

		/* Compute data stride based upon producing no more than 'goal' vectors */
		step = ceil(
			sqrt( 2*(xlim)*(ylim) /
				(double)goal )
			);
	}
	if( step < 1 )  step = 1;


	xfudge = (dsp_ip->dsp_xcnt % step + step) / 2 ;
	yfudge = (dsp_ip->dsp_ycnt % step + step) / 2 ;

	if (xfudge < 1) xfudge = 1;
	if (yfudge < 1) yfudge = 1;

	for (y=yfudge ; y < ylim ; y += step ) {
		VSET(s_pt, 0.0, y, DSP(dsp_ip, 0, y));
		MOVE();

		for (x=xfudge ; x < xlim ; x+=step ) {
			s_pt[X] = x;
			s_pt[Z] = DSP(dsp_ip, x, y);
			DRAW();
		}
		
		s_pt[X] = xlim;
		s_pt[Z] = DSP(dsp_ip, xlim, y);
		DRAW();

	}

	for (x=xfudge ; x < xlim ; x += step ) {
		VSET(s_pt, x, 0.0, DSP(dsp_ip, x, 0));
		MOVE();
		
		for (y=yfudge ; y < ylim ; y+=step) {
			s_pt[Y] = y;
			s_pt[Z] = DSP(dsp_ip, x, y);
			DRAW();
		}
		
		s_pt[Y] = ylim;
		s_pt[Z] = DSP(dsp_ip, x, ylim);
		DRAW();
	}

#undef MOVE
#undef DRAW
	return(0);
}

/*
 *			R T _ D S P _ T E S S
 *
 *  Returns -
 *	-1	failure
 *	 0	OK.  *r points to nmgregion that holds this tessellation.
 */
int
rt_dsp_tess( r, m, ip, ttol, tol )
struct nmgregion	**r;
struct model		*m;
struct rt_db_internal	*ip;
CONST struct rt_tess_tol *ttol;
struct rt_tol		*tol;
{
	LOCAL struct rt_dsp_internal	*dsp_ip;

	if (rt_g.debug & DEBUG_HF)
		bu_log("rt_dsp_tess()\n");

	RT_CK_DB_INTERNAL(ip);
	dsp_ip = (struct rt_dsp_internal *)ip->idb_ptr;
	RT_DSP_CK_MAGIC(dsp_ip);

	return(-1);
}

/*
 *			R T _ D S P _ I M P O R T
 *
 *  Import an DSP from the database format to the internal format.
 *  Apply modeling transformations as well.
 */
int
rt_dsp_import( ip, ep, mat )
struct rt_db_internal		*ip;
CONST struct rt_external	*ep;
register CONST mat_t		mat;
{
	LOCAL struct rt_dsp_internal	*dsp_ip;
	union record			*rp;
	struct bu_vls			str;
	mat_t tmp;

#define IMPORT_FAIL(_s) \
	bu_log("rt_ebm_import(%d) '%s' %s\n", __LINE__, dsp_ip->dsp_file,_s);\
	bu_free( (char *)dsp_ip , "rt_dsp_import: dsp_ip" ); \
	ip->idb_type = ID_NULL; \
	ip->idb_ptr = (genptr_t)NULL; \
	return -2

	RT_CK_EXTERNAL( ep );
	rp = (union record *)ep->ext_buf;

	if (rt_g.debug & DEBUG_HF)
		bu_log("rt_dsp_import(%s)\n", rp->ss.ss_args);




	/* Check record type */
	if( rp->u_id != DBID_STRSOL )  {
		rt_log("rt_dsp_import: defective record\n");
		return(-1);
	}

	RT_INIT_DB_INTERNAL( ip );
	ip->idb_type = ID_DSP;
	ip->idb_ptr = rt_malloc( sizeof(struct rt_dsp_internal), "rt_dsp_internal");
	dsp_ip = (struct rt_dsp_internal *)ip->idb_ptr;
	dsp_ip->magic = RT_DSP_INTERNAL_MAGIC;

	/* set defaults */
	/* XXX bu_struct_parse does not set the null? */
	memset(&dsp_ip->dsp_file[0], 0, DSP_NAME_LEN); 
	dsp_ip->dsp_xcnt = dsp_ip->dsp_ycnt = 0;
	dsp_ip->dsp_xs = dsp_ip->dsp_ys = dsp_ip->dsp_zs = 0.0;
	mat_idn(&dsp_ip->dsp_stom);
	mat_idn(&dsp_ip->dsp_mtos);

	bu_vls_init( &str );
	bu_vls_strcpy( &str, rp->ss.ss_args );
	if (bu_struct_parse( &str, rt_dsp_parse, (char *)dsp_ip ) < 0) {
		bu_vls_free( &str );
		IMPORT_FAIL("parse error");
	}

	
	/* Validate parameters */
	if (dsp_ip->dsp_xcnt == 0 || dsp_ip->dsp_ycnt == 0) {
		IMPORT_FAIL("zero dimension on map");
	}
	
	/* Apply Modeling transoform */
	mat_copy(tmp, dsp_ip->dsp_stom);
	mat_mul(dsp_ip->dsp_stom, mat, tmp);
	
	mat_inv(dsp_ip->dsp_mtos, dsp_ip->dsp_stom);

	/* get file */
	if( !(dsp_ip->dsp_mp = rt_open_mapped_file( dsp_ip->dsp_file, "dsp" )) )  {
		IMPORT_FAIL("unable to open");
	}
	if (dsp_ip->dsp_mp->buflen != dsp_ip->dsp_xcnt*dsp_ip->dsp_ycnt*2) {
		IMPORT_FAIL("buffer wrong size");
	}

	if (rt_g.debug & DEBUG_HF) {
		bu_vls_trunc(&str, 0);
		bu_vls_struct_print( &str, rt_dsp_ptab, (char *)dsp_ip);
		bu_log("  imported as(%s)\n", bu_vls_addr(&str));

	}
	bu_vls_free( &str );
	return(0);			/* OK */
}

/*
 *			R T _ D S P _ E X P O R T
 *
 *  The name is added by the caller, in the usual place.
 */
int
rt_dsp_export( ep, ip, local2mm )
struct rt_external		*ep;
CONST struct rt_db_internal	*ip;
double				local2mm;
{
	struct rt_dsp_internal	*dsp_ip;
	struct rt_dsp_internal	dsp;
	union record		*rec;
	struct bu_vls		str;


	RT_CK_DB_INTERNAL(ip);
	if( ip->idb_type != ID_DSP )  return(-1);
	dsp_ip = (struct rt_dsp_internal *)ip->idb_ptr;
	RT_DSP_CK_MAGIC(dsp_ip);

	RT_INIT_EXTERNAL(ep);
	ep->ext_nbytes = sizeof(union record)*DB_SS_NGRAN;
	ep->ext_buf = (genptr_t)rt_calloc( 1, ep->ext_nbytes, "dsp external");
	rec = (union record *)ep->ext_buf;

	dsp = *dsp_ip;	/* struct copy */

	/* Since libwdb users may want to operate in units other
	 * than mm, we offer the opportunity to scale the solid
	 * (to get it into mm) on the way out.
	 */
	dsp.dsp_mtos[15] /= local2mm;

	bu_vls_init( &str );
	bu_vls_struct_print( &str, rt_dsp_ptab, (char *)&dsp);
	if (rt_g.debug & DEBUG_HF)	
		bu_log("rt_dsp_export(%s)\n", bu_vls_addr(&str) );

	rec->ss.ss_id = DBID_STRSOL;
	strncpy( rec->ss.ss_keyword, "dsp", NAMESIZE-1 );
	strncpy( rec->ss.ss_args, bu_vls_addr(&str), DB_SS_LEN-1 );


	bu_vls_free( &str );


	return(0);
}

/*
 *			R T _ D S P _ D E S C R I B E
 *
 *  Make human-readable formatted presentation of this solid.
 *  First line describes type of solid.
 *  Additional lines are indented one tab, and give parameter values.
 */
int
rt_dsp_describe( str, ip, verbose, mm2local )
struct rt_vls		*str;
struct rt_db_internal	*ip;
int			verbose;
double			mm2local;
{
	register struct rt_dsp_internal	*dsp_ip =
		(struct rt_dsp_internal *)ip->idb_ptr;
	struct bu_vls vls;


	if (rt_g.debug & DEBUG_HF)
		bu_log("rt_dsp_describe()\n");

	RT_DSP_CK_MAGIC(dsp_ip);

	dsp_print(&vls, dsp_ip);
	bu_vls_vlscat( str, &vls );
	bu_vls_free( &vls );

#if 0
	sprintf(buf, "\tV (%g, %g, %g)\n",
		dsp_ip->v[X] * mm2local,
		dsp_ip->v[Y] * mm2local,
		dsp_ip->v[Z] * mm2local );
	rt_vls_strcat( str, buf );
#endif

	return(0);
}

/*
 *			R T _ D S P _ I F R E E
 *
 *  Free the storage associated with the rt_db_internal version of this solid.
 */
void
rt_dsp_ifree( ip )
struct rt_db_internal	*ip;
{
	register struct rt_dsp_internal	*dsp_ip;

	if (rt_g.debug & DEBUG_HF)
		bu_log("rt_dsp_ifree()\n");

	RT_CK_DB_INTERNAL(ip);
	dsp_ip = (struct rt_dsp_internal *)ip->idb_ptr;
	RT_DSP_CK_MAGIC(dsp_ip);

	if (dsp_ip->dsp_mp) {
		RT_CK_MAPPED_FILE(dsp_ip->dsp_mp);
		rt_close_mapped_file(dsp_ip->dsp_mp);
	}

	dsp_ip->magic = 0;			/* sanity */
	dsp_ip->dsp_mp = (struct rt_mapped_file *)0;

	rt_free( (char *)dsp_ip, "dsp ifree" );
	ip->idb_ptr = GENPTR_NULL;	/* sanity */
}
