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
#include <string.h>
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
#define TRI3	30
#define TRI4	40

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

	struct seg *sp;	/* the current segment being filled */
	int sp_is_valid;
};

#define INHIT(isp, dist, surf, cell) {\
	if (isp->sp_is_valid) {\
		bu_log("%s:%d pixel(%d,%d) ", __FILE__, __LINE__, \
			isp->ap->a_x, isp->ap->a_y); \
		bu_bomb("trying to enter solid when inside\n"); \
	} \
 \
	RT_GET_SEG( isp->sp, isp->ap->a_resource ); \
	isp->sp->seg_stp = isp->stp; \
	isp->sp->seg_in.hit_dist = dist; \
	isp->sp->seg_in.hit_surfno = surf; \
	isp->sp->seg_in.hit_vpriv[X] = cell[X]; \
	isp->sp->seg_in.hit_vpriv[Y] = cell[Y]; \
	isp->sp_is_valid = 1; \
	if (rt_g.debug & DEBUG_HF) { \
		point_t in, t; \
		VJOIN1(t, isp->r.r_pt, dist, isp->r.r_dir); \
		MAT4X3PNT(in, isp->dsp->dsp_i.dsp_stom, t); \
		bu_log("line %d New in pt(%g %g %g) ss_dist %g surf: %d\n", \
			__LINE__, V3ARGS(in), dist, surf); \
	} \
}

#define OUTHIT(isp, dist, surf, cell) {\
	if (! isp->sp_is_valid) {\
		bu_log("%s:%d pixel(%d,%d) ", __FILE__, __LINE__, \
			isp->ap->a_x, isp->ap->a_y); \
		bu_bomb("Trying to set outpoint when not inside\n"); \
	} \
	isp->sp->seg_out.hit_dist = dist; \
	isp->sp->seg_out.hit_surfno = surf; \
	isp->sp->seg_out.hit_vpriv[X] = cell[X]; \
	isp->sp->seg_out.hit_vpriv[Y] = cell[Y]; \
	if (rt_g.debug & DEBUG_HF) { \
		point_t out, t; \
		VJOIN1(t, isp->r.r_pt, dist, isp->r.r_dir); \
		MAT4X3PNT(out, isp->dsp->dsp_i.dsp_stom, t); \
		bu_log("line %d New out pt(%g %g %g) ss_dist %g surf:%d\n", \
			__LINE__, V3ARGS(out), dist, surf); \
	} \
}

#define HIT_COMMIT(isp) { \
	if ( ! (isp)->sp_is_valid) { \
		bu_log("%s:%d pixel(%d,%d) ", __FILE__, __LINE__, \
			isp->ap->a_x, isp->ap->a_y); \
		bu_bomb("attempt to commit an invalid seg\n"); \
	} \
 \
	if (rt_g.debug & DEBUG_HF) { \
		bu_log("line %d Committing seg %g -> %g\n", __LINE__,\
			(isp)->sp->seg_in.hit_dist, \
			(isp)->sp->seg_out.hit_dist); \
	} \
 \
	BU_LIST_INSERT( &(isp)->seglist, &( isp->sp->l) ); \
	(isp)->sp = (struct seg *)NULL; \
	(isp)->sp_is_valid = 0; \
}

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
	{"%d",  1, "sm", DSP_O(dsp_smooth), FUNC_NULL },
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
	{"%d",  1, "sm", DSP_O(dsp_smooth), FUNC_NULL },
	{"%d",	1, "w", DSP_O(dsp_xcnt), FUNC_NULL },
	{"%d",	1, "n", DSP_O(dsp_ycnt), FUNC_NULL },
	{"%f", 16, "stom", DSP_AO(dsp_stom), FUNC_NULL },
	{"",	0, (char *)0, 0,			FUNC_NULL }
};

static int plot_file_num=0;
static int plot_em=1;

static void
dsp_print(vls, dsp_ip)
struct bu_vls *vls;
CONST struct rt_dsp_internal *dsp_ip;
{
	point_t pt, v;
	RT_DSP_CK_MAGIC(dsp_ip);

	bu_vls_init( vls );
	bu_vls_printf( vls, "Displacement Map\n  file='%s' xc=%d yc=%d sm=%d\n",
		dsp_ip->dsp_file, dsp_ip->dsp_xcnt, dsp_ip->dsp_ycnt, dsp_ip->dsp_smooth);

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

	for (y=0 ; y < YCNT(dsp) ; y++) {

		for (x=0 ; x < XCNT(dsp) ; x++) {

			if (DSP(dsp, x, y) < dsp_min)
				dsp_min = DSP(dsp, x, y);

			if (DSP(dsp, x, y) > dsp_max)
				dsp_max = DSP(dsp, x, y);
		}
	}

	if (rt_g.debug & DEBUG_HF)
		bu_log("x:%d y:%d min %d max %d\n", XCNT(dsp), YCNT(dsp), dsp_min, dsp_max);

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
			bu_log("surf[%d](%g %g %g %g) dot:%g ss_dist:%g\n",
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
		vect_t pt, t;
		VJOIN1(t, isect->r.r_pt, isect->bbox.in_dist, isect->r.r_dir);
		MAT4X3PNT(pt, isect->dsp->dsp_i.dsp_stom, t);
		bu_log("solid rpp in  surf[%d] ss_dist:%g (%g %g %g)\n",
			isect->bbox.in_surf, isect->bbox.in_dist, V3ARGS(pt));
		VJOIN1(t, isect->r.r_pt, isect->bbox.out_dist, isect->r.r_dir);
		MAT4X3PNT(pt, isect->dsp->dsp_i.dsp_stom, t);
		bu_log("solid rpp out surf[%d] ss_dist:%g  (%g %g %g)\n",
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
		vect_t pt, t;
		VJOIN1(t, isect->r.r_pt, isect->minbox.in_dist, isect->r.r_dir);

		MAT4X3PNT(pt, isect->dsp->dsp_i.dsp_stom, t);

		bu_log("solid minbox rpp in  surf[%d] ss_dist:%g (%g %g %g)\n",
			isect->minbox.in_surf, isect->minbox.in_dist, V3ARGS(pt));

		VJOIN1(t, isect->r.r_pt, isect->minbox.out_dist, isect->r.r_dir);
		MAT4X3PNT(pt, isect->dsp->dsp_i.dsp_stom, t);

		bu_log("solid minbox rpp out surf[%d] ss_dist:%g (%g %g %g)\n",
			isect->minbox.out_surf, isect->minbox.out_dist, V3ARGS(pt));
	}

	return 1;
}


/*
 *	Finds the minimum and maximum values in a cell
 */
void
cell_minmax(dsp, x, y, cell_min, cell_max)
register struct dsp_specific *dsp;
register int x, y;
short *cell_min;
short *cell_max;
{
	register short cmin, cmax, v;

	cmin = cmax = DSP(dsp, x, y);

	v = DSP(dsp, x+1, y);
	if (v < cmin) cmin = v;
	if (v > cmax) cmax = v;
	
	v = DSP(dsp, x+1, y+1);
	if (v < cmin) cmin = v;
	if (v > cmax) cmax = v;
	
	v = DSP(dsp, x, y+1);
	if (v < cmin) cmin = v;
	if (v > cmax) cmax = v;

	*cell_min = cmin;
	*cell_max = cmax;
}






static void
plot_cell_ray(isect, cell,
	curr_pt, next_pt, hit1, dist1, hit2, dist2, inside)
struct isect_stuff *isect;
int cell[3];
vect_t curr_pt, next_pt;
int hit1;
double dist1;
int hit2;
double dist2;
int inside;
{
	FILE *fd;
	char buf[132];
	point_t A, B, C, D, tmp, pt;
	vect_t A_B;
	vect_t A_C;
	short min_val;
	short max_val;
	int outside;


	VSET(A, cell[X],   cell[Y],   DSP(isect->dsp, cell[X],   cell[Y])  );
	VSET(B, cell[X]+1, cell[Y],   DSP(isect->dsp, cell[X]+1, cell[Y])  );
	VSET(C, cell[X],   cell[Y]+1, DSP(isect->dsp, cell[X],   cell[Y]+1));
	VSET(D, cell[X]+1, cell[Y]+1, DSP(isect->dsp, cell[X]+1, cell[Y]+1));

	RES_ACQUIRE( &rt_g.res_model);
	sprintf(buf, "dsp%d.pl", plot_file_num++);
	RES_RELEASE( &rt_g.res_model);
	bu_log("%s\n", buf);

	RES_ACQUIRE( &rt_g.res_syscall);
	if ((fd=fopen(buf, "w")) != (FILE *)NULL) {

		VSUB2(A_B, B, A);
		VSUB2(A_C, C, A);

		if (A[Z] != B[Z] || A[Z] != C[Z] || A[Z] != D[Z]) {
			cell_minmax(isect->dsp, cell[X], cell[Y],
				&min_val, &max_val);


			/* plot cell top bounding box */
			pl_color(fd, 60, 60, 190);

			tmp[Z] = (double)min_val;

			VMOVEN(tmp, A, 2);
			MAT4X3PNT(pt, isect->dsp->dsp_i.dsp_stom, tmp);
			pdv_3move(fd, pt);

			VMOVEN(tmp, B, 2);
			MAT4X3PNT(pt, isect->dsp->dsp_i.dsp_stom, tmp);
			pdv_3cont(fd, pt);

			VMOVEN(tmp, D, 2);
			MAT4X3PNT(pt, isect->dsp->dsp_i.dsp_stom, tmp);
			pdv_3cont(fd, pt);

			VMOVEN(tmp, C, 2);
			MAT4X3PNT(pt, isect->dsp->dsp_i.dsp_stom, tmp);
			pdv_3cont(fd, pt);

			VMOVEN(tmp, A, 2);
			MAT4X3PNT(pt, isect->dsp->dsp_i.dsp_stom, tmp);
			pdv_3cont(fd, pt);

			tmp[Z] = (double)max_val;
			MAT4X3PNT(pt, isect->dsp->dsp_i.dsp_stom, tmp);
			pdv_3cont(fd, pt);

			VMOVEN(tmp, B, 2);
			MAT4X3PNT(pt, isect->dsp->dsp_i.dsp_stom, tmp);
			pdv_3cont(fd, pt);

			VMOVEN(tmp, D, 2);
			MAT4X3PNT(pt, isect->dsp->dsp_i.dsp_stom, tmp);
			pdv_3cont(fd, pt);

			VMOVEN(tmp, C, 2);
			MAT4X3PNT(pt, isect->dsp->dsp_i.dsp_stom, tmp);
			pdv_3cont(fd, pt);

			VMOVEN(tmp, A, 2);
			MAT4X3PNT(pt, isect->dsp->dsp_i.dsp_stom, tmp);
			pdv_3cont(fd, pt);
		}



		/* plot the triangles */
		pl_color(fd, 90, 220, 90);

		VMOVE(tmp, A);
		MAT4X3PNT(pt, isect->dsp->dsp_i.dsp_stom, tmp);
		pdv_3move(fd, pt);

		VMOVE(tmp, B);
		MAT4X3PNT(pt, isect->dsp->dsp_i.dsp_stom, tmp);
		pdv_3cont(fd, pt);

		VMOVE(tmp, D);
		MAT4X3PNT(pt, isect->dsp->dsp_i.dsp_stom, tmp);
		pdv_3cont(fd, pt);

		VMOVE(tmp, A);
		MAT4X3PNT(pt, isect->dsp->dsp_i.dsp_stom, tmp);
		pdv_3cont(fd, pt);

		VMOVE(tmp, C);
		MAT4X3PNT(pt, isect->dsp->dsp_i.dsp_stom, tmp);
		pdv_3cont(fd, pt);

		VMOVE(tmp, D);
		MAT4X3PNT(pt, isect->dsp->dsp_i.dsp_stom, tmp);
		pdv_3cont(fd, pt);


		/* plot the ray */
		outside = !inside;

		if (outside)
			pl_color(fd, 255, 40, 40);
		else
			pl_color(fd, 255, 255, 100);


		MAT4X3PNT(pt, isect->dsp->dsp_i.dsp_stom, curr_pt);
		pdv_3move(fd, pt);

		if (hit1 && hit2) {
			if (dist1 < dist2) {
				VJOIN1(tmp, isect->r.r_pt, dist1, isect->r.r_dir);
				MAT4X3PNT(pt, isect->dsp->dsp_i.dsp_stom, tmp);
				pdv_3cont(fd, pt);

				if (outside)
					pl_color(fd, 255, 255, 100);
				else
					pl_color(fd, 255, 40, 40);

				pdv_3move(fd, pt);

				outside = !outside;
				VJOIN1(tmp, isect->r.r_pt, dist2, isect->r.r_dir);
				MAT4X3PNT(pt, isect->dsp->dsp_i.dsp_stom, tmp);
				pdv_3cont(fd, pt);

				if (outside)
					pl_color(fd, 255, 255, 100);
				else
					pl_color(fd, 255, 40, 40);

				pdv_3move(fd, pt);
				outside = !outside;
			} else {
				VJOIN1(tmp, isect->r.r_pt, dist2, isect->r.r_dir);
				MAT4X3PNT(pt, isect->dsp->dsp_i.dsp_stom, tmp);
				pdv_3cont(fd, pt);

				if (outside)
					pl_color(fd, 255, 255, 100);
				else
					pl_color(fd, 255, 40, 40);
				pdv_3move(fd, pt);
				outside = !outside;
				VJOIN1(tmp, isect->r.r_pt, dist1, isect->r.r_dir);
				MAT4X3PNT(pt, isect->dsp->dsp_i.dsp_stom, tmp);
				pdv_3cont(fd, pt);

				if (outside)
					pl_color(fd, 255, 255, 100);
				else
					pl_color(fd, 255, 40, 40);
				pdv_3move(fd, pt);
				outside = !outside;
			}
		} else if (hit1) {
			VJOIN1(tmp, isect->r.r_pt, dist1, isect->r.r_dir);
			MAT4X3PNT(pt, isect->dsp->dsp_i.dsp_stom, tmp);
			pdv_3cont(fd, pt);

			if (outside)
				pl_color(fd, 255, 255, 100);
			else
				pl_color(fd, 255, 40, 40);
			pdv_3move(fd, pt);

		} else if (hit2) {
			VJOIN1(tmp, isect->r.r_pt, dist2, isect->r.r_dir);
			MAT4X3PNT(pt, isect->dsp->dsp_i.dsp_stom, tmp);
			pdv_3cont(fd, pt);

			if (outside)
				pl_color(fd, 255, 255, 100);
			else
				pl_color(fd, 255, 40, 40);
			pdv_3move(fd, pt);
		}

		
		MAT4X3PNT(pt, isect->dsp->dsp_i.dsp_stom, next_pt);
		pdv_3cont(fd, pt);


		fclose(fd);
		RES_RELEASE( &rt_g.res_syscall);
	}
}

static void
set_and_permute(isect, cell, A, B, C, D, tri1, tri2)
struct isect_stuff *isect;
int cell[3];
point_t A, B, C, D;
int *tri1;
int *tri2;
{
	int sub[2], sup[2];
	point_t tmp;
	vect_t Aline, Bline, Cline, Dline;
	double dot1, dot2;

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

	sub[X] = cell[X] - 1;
	sub[Y] = cell[Y] - 1;
	sup[X] = cell[X] + 1;
	sup[Y] = cell[Y] + 1;

	if (sub[X] < 0) sub[X] = 0;
	if (sub[Y] < 0) sub[Y] = 0;

	if (sup[X] > XSIZ(isect->dsp)) sup[X] = XSIZ(isect->dsp);
	if (sup[Y] > YSIZ(isect->dsp)) sup[Y] = YSIZ(isect->dsp);

	VSET(tmp,	 sub[X], sub[Y],
	 DSP(isect->dsp, sub[X], sub[Y]));
	VSUB2(Aline, A, tmp);

	VSET(tmp,	 sup[X], sup[Y],
	 DSP(isect->dsp, sup[X], sup[Y]));
	VSUB2(Dline, tmp, D);

	VSET(tmp,	 sub[X], sup[Y],
	 DSP(isect->dsp, sub[X], sup[Y]));
	VSUB2(Cline, tmp, C);

	VSET(tmp, sup[X], sub[Y], DSP(isect->dsp, sup[X], sub[Y]));
	VSUB2(Bline, B, tmp);

	dot1 = VDOT(Aline, Dline);
	dot2 = VDOT(Bline, Cline);

	if ( dot1 > dot2 ) {
		*tri1 = TRI1;
		*tri2 = TRI2;

		return;
	}

	
	/* prefer the B-C cut rather than the A-D cut */
	*tri1 = TRI3;
	*tri2 = TRI4;


	VMOVE(tmp, A);
	VMOVE(A, B);
	VMOVE(B, D);
	VMOVE(D, C);
	VMOVE(C, tmp);

}

/*
 *  Once we've determined that the ray hits the bounding box for
 *  the triangles, this routine takes care of the actual triangle intersection
 *  and follow-up.
 *
 *
 */
static void
isect_ray_triangles(isect, cell, 
	curr_pt, next_pt, inside)
struct isect_stuff *isect;
int cell[3];
point_t curr_pt, next_pt;
int *inside;
{
	point_t A, B, C, D;
	vect_t AB, AC, AD;
	vect_t N1, N2;
	vect_t PA;
	vect_t PAxD;
	double alpha;
	double beta;
	double NdotD1;
	double NdotD2;
	double abs_NdotD;
	double	dist1;
	double	dist2;
	int hit1;
	int hit2;
	char *reason1;
	char *reason2;
	int tri1, tri2;

	set_and_permute(isect, cell, A, B, C, D, &tri1, &tri2);

	if (rt_g.debug & DEBUG_HF) {
		point_t t;
		bu_log("isect_ray_triangles(inside=%d)\n", *inside);

		MAT4X3PNT(t, isect->dsp->dsp_i.dsp_stom, A);
		VPRINT("A", t);
		MAT4X3PNT(t, isect->dsp->dsp_i.dsp_stom, B);
		VPRINT("B", t);
		MAT4X3PNT(t, isect->dsp->dsp_i.dsp_stom, C);
		VPRINT("C", t);
		MAT4X3PNT(t, isect->dsp->dsp_i.dsp_stom, D);
		VPRINT("D", t);
	}


	VSUB2(AB, B, A);
	VSUB2(AC, C, A);
	VSUB2(AD, D, A);

	VSUB2(PA, A, isect->r.r_pt);

	/* get perpendicular to PA, D */
	VCROSS(PAxD, PA, isect->r.r_dir);

	VCROSS(N1, AB, AD);
	NdotD1 = VDOT( N1, isect->r.r_dir);

	hit1 = 0;
	reason1 = (char *)NULL;
	if ( BN_VECT_ARE_PERP(NdotD1, isect->tol)) {
		reason1 = "perpendicular";
		goto tri2;
	}

	/* project PAxD onto AB giving scaled distance along *AD* */
	alpha = VDOT( AB, PAxD );

	if (NdotD1 > 0.0) alpha = -alpha;

	abs_NdotD = NdotD1 >= 0.0 ? NdotD1 : (-NdotD1);

	if (alpha < 0.0 || alpha > abs_NdotD ) {
		reason1 = "miss tri1 (alpha)\n";
		goto tri2;
	}

	/* project PAxD onto AD giving scaled distance along *AB* */
	beta = VDOT( AD, PAxD );

	if ( NdotD1 < 0.0 ) beta = -beta;

	if( beta < 0.0 || beta > abs_NdotD ) {
		reason1 = "miss tri1 (beta)\n";
		goto tri2;
	}
	if ( alpha+beta > abs_NdotD ) {
		reason1 = "miss tri1 (alpha+beta > NdotD)\n";
		goto tri2;
	}
	hit1 = 1;
	dist1 = VDOT( PA, N1 ) / NdotD1;

tri2:
	if (rt_g.debug & DEBUG_HF && reason1 != (char *)NULL)
		bu_log("missed tri1 %s\n", reason1);
		


	VCROSS(N2, AD, AC);
	NdotD2 = VDOT( N2, isect->r.r_dir);

	hit2 = 0;
	reason2 = (char *)NULL;
	if (BN_VECT_ARE_PERP(NdotD2, isect->tol)) {
		reason2 = "perpendicular";
		goto done;
	}

	/* project PAxD onto AD giving scaled distance along *AC* */
	alpha = VDOT( AD, PAxD );
	if (NdotD2 > 0.0) alpha = -alpha;
	abs_NdotD = NdotD2 >= 0.0 ? NdotD2 : (-NdotD2);
	if (alpha < 0.0 || alpha > abs_NdotD ) {
		reason2 = "miss tri2 (alpha)\n";
		goto done;
	}

	/* project PAxD onto AC giving scaled distance along *AD* */
	beta = VDOT( AC, PAxD );
	if ( NdotD2 < 0.0 ) beta = -beta;
	if( beta < 0.0 || beta > abs_NdotD ) {
		reason2 = "miss tri2 (beta)\n";
		goto done;
	}

	if ( alpha+beta > abs_NdotD ) {
		reason2 = "miss tri2 (alpha+beta > NdotD)\n";
		goto done;
	}

	hit2 = 1;
	dist2 = VDOT( PA, N2 ) / NdotD2;

done:
	if (rt_g.debug & DEBUG_HF && reason2 != (char *)NULL)
		bu_log("missed tri2 %s\n", reason2);

	/* plot some diagnostic overlays */
	if (rt_g.debug & DEBUG_HF && plot_em)
		plot_cell_ray(isect, cell, curr_pt, next_pt, hit1, dist1, hit2, dist2, *inside);



	/* sort the hit distances and add the hits to the segment list */
	if (hit1 && hit2) {
		int first_tri, second_tri;
		double first, second;
		double firstND, secondND;
		double *firstN, *secondN;

		/* hit both */
		if (dist1 < dist2) {
			if (rt_g.debug & DEBUG_HF)
				bu_log("hit triangles: ss_dist1:%g ss_dist2:%g \n", dist1, dist2);
			first_tri = tri1;
			firstND = NdotD1;
			first = dist1;
			firstN = N1;
			second_tri = tri2;
			secondND = NdotD2;
			second = dist2;
			secondN = N2;
		} else {
			if (rt_g.debug & DEBUG_HF)
				bu_log("hit triangles: ss_dist2:%g ss_dist1:%g \n", dist2, dist1);
			first_tri = tri2;
			firstND = NdotD2;
			first = dist2;
			firstN = N2;
			second_tri = tri1;
			secondND = NdotD1;
			second = dist1;
			secondN = N1;
		}

		if (firstND < 0) {
			/* entering */
			if (*inside) {
				bu_log("%s:%d ray entering while inside dsp pixel(%d,%d)",
					__FILE__, __LINE__,
					isect->ap->a_x, isect->ap->a_y);
				bu_bomb("");
			}
			INHIT(isect, first, first_tri, cell);

			if (secondND < 0) {
				bu_log("%s:%d ray entering while inside dsp pixel(%d,%d)",
					__FILE__, __LINE__,
					isect->ap->a_x, isect->ap->a_y);
				bu_bomb("");
			}
			OUTHIT(isect, second, second_tri, cell);
			HIT_COMMIT(isect);

		} else {
			/* leaving */
			if (! *inside) {
				bu_log("%s:%d ray leaving while outside dsp pixel(%d,%d)",
					__FILE__, __LINE__,
					isect->ap->a_x, isect->ap->a_y);
				bu_bomb("");
			}
			OUTHIT(isect, first, first_tri, cell);
			HIT_COMMIT(isect);
			*inside = 0;

			if (secondND > 0) {
				bu_log("%s:%d ray leaving while outside dsp pixel(%d,%d)",
					__FILE__, __LINE__,
					isect->ap->a_x, isect->ap->a_y);
				bu_bomb("");
			}
			INHIT(isect, second, second_tri, cell);
			*inside = 1;
		}
	} else if (hit1) {
		/* only hit tri 1 */

		if (NdotD1 < 0) {
			/* entering */
			if (rt_g.debug & DEBUG_HF)
				bu_log("hit triangle 1 entering ss_dist:%g\n", dist1);

			if (*inside) {
				bu_log("%s:%d ray entering while inside dsp pixel(%d,%d)",
					__FILE__, __LINE__,
					isect->ap->a_x, isect->ap->a_y);
				bu_bomb("");
			}

			INHIT(isect, dist1, tri1, cell);
			*inside = 1;
		} else {
			/* leaving */
			if (rt_g.debug & DEBUG_HF)
				bu_log("hit triangle 1 leaving ss_dist:%g\n", dist1);

			if (! *inside) {
				bu_log("%s:%d ray leaving while outside dsp pixel(%d,%d)",
					__FILE__, __LINE__,
					isect->ap->a_x, isect->ap->a_y);
				bu_bomb("");
			}
			OUTHIT(isect, dist1, tri1, cell);
			HIT_COMMIT(isect);
			*inside = 0;
		}
	} else if (hit2) {
		/* only hit 2 */

		if (NdotD2 < 0) {
			/* entering */

			if (rt_g.debug & DEBUG_HF)
				bu_log("hit triangle 2 entering ss_dist:%g\n", dist2);
			if (*inside) {
				bu_log("%s:%d ray entering while inside dsp pixel(%d,%d)",
					__FILE__, __LINE__,
					isect->ap->a_x, isect->ap->a_y);
				bu_bomb("");
			}

			INHIT(isect, dist2, tri2, cell);
			*inside = 1;
		} else {
			/* leaving */

			if (rt_g.debug & DEBUG_HF)
				bu_log("hit triangle 2 leaving ss_dist:%g\n", dist2);
			if (! *inside) {
				bu_log("%s:%d ray leaving while outside dsp pixel(%d,%d)",
					__FILE__, __LINE__,
					isect->ap->a_x, isect->ap->a_y);
				bu_bomb("");
			}
			OUTHIT(isect, dist2, tri2, cell);
			HIT_COMMIT(isect);
			*inside = 0;
		}
	} else {
		/* missed everything */
	}



}



static int
isect_cell_x_wall(isect, cell, surf, dist, pt)
struct isect_stuff *isect;
int cell[3];
int surf;
double dist;
point_t pt;
{
	short a, b;	/* points on cell wall */
	double wall_top_slope;
	double wall_top;	/* wall height at Y of curr_pt */
	double pt_dy;

	if (rt_g.debug & DEBUG_HF) {
		vect_t t;
		MAT4X3PNT(t, isect->dsp->dsp_i.dsp_stom, pt);

		bu_log("isect_cell_x_wall() cell(%d,%d) surf:%d ss_dist:%g pt(%g %g %g)\n",
			V2ARGS(cell), surf, dist, V3ARGS(t));
	}
	if (surf == BBSURF(XMIN)) {
		a = DSP(isect->dsp, cell[X],   cell[Y]);
		b = DSP(isect->dsp, cell[X],   cell[Y]+1);
	} else if (surf == BBSURF(XMAX)) {
		a = DSP(isect->dsp, cell[X]+1, cell[Y]);
		b = DSP(isect->dsp, cell[X]+1, cell[Y]+1);
	} else {
		bu_log("%s:%d pixel(%d,%d) ", __FILE__, __LINE__,
			isect->ap->a_x, isect->ap->a_y);
		bu_log("bad surface %d, isect_cell_x_entry_wall()\n", surf);
		bu_bomb("");
	}

	wall_top_slope = b - a;
	pt_dy = pt[Y] - cell[Y];

	/* compute the height of the wall top at the Y location of curr_pt */
	wall_top = a + pt_dy * wall_top_slope;

	if (rt_g.debug & DEBUG_HF)
		bu_log("\ta:%d b:%d wall slope:%g top:%g dy:%g pt[Z]:%g\n",
			a, b, wall_top_slope, wall_top, pt_dy, pt[Z]);

	return (wall_top > pt[Z]);
}




static int
isect_cell_y_wall(isect, cell, surf, dist, pt)
struct isect_stuff *isect;
int cell[3];
int surf;
double dist;
point_t pt;
{
	short a, b;	/* points on cell wall */
	double wall_top_slope;
	double wall_top;	/* wall height at Y of pt */
	double pt_dx;

	if (rt_g.debug & DEBUG_HF) {
		vect_t t;

		MAT4X3PNT(t, isect->dsp->dsp_i.dsp_stom, pt);
		bu_log("isect_cell_y_wall() cell(%d,%d) surf:%d ss_dist:%d pt(%g %g %g)\n",
			V2ARGS(cell), surf, dist, V3ARGS(t));
	}
	if (surf == BBSURF(YMIN)) {
		a = DSP(isect->dsp, cell[X],   cell[Y]);
		b = DSP(isect->dsp, cell[X]+1, cell[Y]);
	} else if (surf == BBSURF(YMAX)) {
		a = DSP(isect->dsp, cell[X],   cell[Y]+1);
		b = DSP(isect->dsp, cell[X]+1, cell[Y]+1);
	} else {
		bu_log("%s:%d pixel(%d,%d) ", __FILE__, __LINE__,
			isect->ap->a_x, isect->ap->a_y);
		bu_log("bad surface %d, isect_cell_y_entry_wall()\n", surf);
		bu_bomb("");
	}

	wall_top_slope = b - a;
	pt_dx = pt[X] - cell[X];

	/* compute the height of the wall top at the X location of pt */
	wall_top = a + pt_dx * wall_top_slope;

	if (rt_g.debug & DEBUG_HF)
		bu_log("\ta:%d b:%d wall slope:%g top:%g dx:%g pt[Z]:%g\n",
			a, b, wall_top_slope, wall_top, pt_dx, pt[Z]);


	return (wall_top > pt[Z]);
}



struct cell_stuff {
	pointp_t bbin_pt;
	double bbin_dist;
	int *grid_cell;
	double *curr_dist;
	int *curr_surf;
	pointp_t curr_pt;
};

static void
cell_isect(isect, dt, next_surf, cs, inside, rising, isect_wall)
struct isect_stuff *isect;
double dt;
int next_surf;
struct cell_stuff *cs;
int *inside;
int rising;
int (*isect_wall)();
{
	point_t next_pt;
	short	cell_min;
	short	cell_max;
	int	hit;
	
	VJOIN1(next_pt, cs->bbin_pt, dt, isect->r.r_dir);


	if (rt_g.debug & DEBUG_HF) {
		vect_t t;
		MAT4X3PNT(t, isect->dsp->dsp_i.dsp_stom, next_pt);
		bu_log("next_pt(%g %g %g) surf:%d  ss_dist:%g\n",
			V3ARGS(t), next_surf, dt);
	}
	cell_minmax(isect->dsp, cs->grid_cell[X], cs->grid_cell[Y],
		&cell_min, &cell_max);

	if (( rising && next_pt[Z] < cell_min) ||
	    (!rising && cs->curr_pt[Z] < cell_min) ) {
		/* in base */
		if (rt_g.debug & DEBUG_HF) bu_log("in base\n");
	    	if (!*inside) {
	    		INHIT(isect, *cs->curr_dist, *cs->curr_surf, cs->grid_cell);
	    		*inside = 1;
	    	}
	    	OUTHIT(isect, (cs->bbin_dist+dt), next_surf, cs->grid_cell);

	    	if (rt_g.debug & DEBUG_HF)
		    	plot_cell_ray(isect, cs->grid_cell, 
		    		cs->curr_pt, next_pt, 0, 0.0, 0, 0.0, *inside);


	} else if (
	    ( rising && cs->curr_pt[Z] > cell_max) ||
	    (!rising && next_pt[Z] > cell_max) ) {
		/* miss above */
		if (rt_g.debug & DEBUG_HF) bu_log("miss above\n");
	    	if (*inside) {
	    		HIT_COMMIT(isect);
	    		*inside = 0;
	    	}
	    	if (rt_g.debug & DEBUG_HF)
		    	plot_cell_ray(isect, cs->grid_cell, 
		    		cs->curr_pt, next_pt, 0, 0.0, 0, 0.0, *inside);


	} else {
		/* intersect */
		if (rt_g.debug & DEBUG_HF)
			bu_log("intersect %d %d X %s\n",
				cell_min, cell_max, 
				(rising?"rising":"falling") );

		switch (*cs->curr_surf) {
		case BBSURF(XMIN) :
		case BBSURF(XMAX) :
			hit = isect_cell_x_wall(
				isect, cs->grid_cell, *cs->curr_surf,
				*cs->curr_dist, cs->curr_pt);
			if (rt_g.debug & DEBUG_HF) {
				if (hit)
					bu_log("hit X in-wall\n");
				else
					bu_log("miss X in-wall\n");
			}
			break;
		case BBSURF(YMIN) :
		case BBSURF(YMAX) :
			hit = isect_cell_y_wall(
				isect, cs->grid_cell, *cs->curr_surf,
				*cs->curr_dist, cs->curr_pt);
			if (rt_g.debug & DEBUG_HF) {
				if (hit)
					bu_log("hit Y in-wall\n");
				else
					bu_log("miss Y in-wall\n");
			}
			break;
		case BBSURF(ZMIN) :
			if (*cs->curr_dist != cs->bbin_dist){
				bu_log("%s:%d hitting bottom of dsp for entry while past bottom?  pixel(%d,%d)\n",
					__FILE__, __LINE__,
					isect->ap->a_x, isect->ap->a_y);
				bu_bomb("");
			}
			hit = 1;
			break;
		case BBSURF(ZMAX) :
			hit = 0;
			break;
		default:
			bu_log("%s:%d pixel(%d,%d) surface %d ", __FILE__, __LINE__, isect->ap->a_x, isect->ap->a_y, *cs->curr_surf);
			bu_bomb("bad surface to intersect\n");
		}

		if (hit && !*inside) {
			INHIT(isect, *cs->curr_dist,
				*cs->curr_surf, cs->grid_cell);
			*inside = 1;
		}

		isect_ray_triangles(isect, cs->grid_cell,
			cs->curr_pt, next_pt,
			inside);


		hit = isect_wall(isect, cs->grid_cell, next_surf, dt, next_pt);

		if (hit) {
			if (!*inside) {
				bu_log("%s:%d pixel(%d,%d) ", __FILE__, __LINE__, isect->ap->a_x, isect->ap->a_y);
				bu_log("hit dsp and not inside g_dsp.c line:%d", __LINE__);
				bu_bomb("");
			}
			if (rt_g.debug & DEBUG_HF)
				bu_log("hit X out-wall\n");
			OUTHIT(isect, (cs->bbin_dist+dt), next_surf, cs->grid_cell);
			*inside = 1;
		} else {
			if (rt_g.debug & DEBUG_HF)
				bu_log("miss X out-wall\n");
		}
	} 

	/* step to next cell */
	VMOVE(cs->curr_pt, next_pt);

	*cs->curr_dist = dt;
}

static void
isect_ray_dsp(isect)
struct isect_stuff *isect;
{
	point_t	bbin_pt;	/* DSP Bounding Box entry point */
	int	bbin_surf;
	double	bbin_dist;
	point_t	bbout_pt;	/* DSP Bounding Box exit point */
	int	bbout_cell[3];	/* grid cell of last point in bbox */
	int	cell_bbox[4];

	point_t curr_pt;	/* entry pt into a cell */
	int	curr_surf;	/* surface of cell bbox for curr_pt */
	double	curr_dist;	/* dist along ray to curr_pt */

	int	grid_cell[3];	/* grid cell of current point */
	int	inside = 0;
	int	rising;		/* boolean:  Ray Z dir sign is positive */
	int	stepX, stepY;	/* signed step delta for grid cell marching */
	int	insurfX, outsurfX;
	int	insurfY, outsurfY;

	double	tDX;		/* dist along ray to span 1 cell in X dir */
	double	tDY;		/* dist along ray to span 1 cell in Y dir */

	double	out_dist;

	double	tX, tY;	/* dist along ray from hit pt. to next cell boundary */
	struct cell_stuff cs;

	/* since we're probably starting in the middle of a cell, we need
	 * to compute the distance along the ray to the initial
	 * X and Y boundaries.
	 */

	if (rt_g.debug & DEBUG_HF) {
		bu_log("isect_ray_dsp()\n");
	}

	/* compute BBox entry point and starting grid cell */
	bbin_dist = isect->bbox.in_dist;
	VJOIN1(bbin_pt, isect->r.r_pt, bbin_dist, isect->r.r_dir);
	bbin_surf = isect->bbox.in_surf;

	VMOVE(grid_cell, bbin_pt);	/* int/float conversion */
	if (grid_cell[X] >= XSIZ(isect->dsp)) grid_cell[X]--;
	if (grid_cell[Y] >= YSIZ(isect->dsp)) grid_cell[Y]--;
	
	out_dist = isect->bbox.out_dist;
	VJOIN1(bbout_pt, isect->r.r_pt, out_dist, isect->r.r_dir);

	VMOVE(bbout_cell, bbout_pt);	/* int/float conversion */
	if (bbout_cell[X] >= XSIZ(isect->dsp)) bbout_cell[X]--;
	if (bbout_cell[Y] >= YSIZ(isect->dsp)) bbout_cell[Y]--;


	/* compute min/max cell values in X and Y for extent of 
	 * ray overlap with bounding box
	 */
	if (bbout_cell[X] < grid_cell[X]) {
		cell_bbox[XMIN] = bbout_cell[X];
		cell_bbox[XMAX] = grid_cell[X];
	} else {
		cell_bbox[XMIN] = grid_cell[X];
		cell_bbox[XMAX] = bbout_cell[X];
	}
	if (bbout_cell[Y] < grid_cell[Y]) {
		cell_bbox[YMIN] = bbout_cell[Y];
		cell_bbox[YMAX] = grid_cell[Y];
	} else {
		cell_bbox[YMIN] = grid_cell[Y];
		cell_bbox[YMAX] = bbout_cell[Y];
	}


	VMOVE(curr_pt, bbin_pt);
	curr_dist = bbin_dist;
	curr_surf = BBSURF(bbin_surf);

	if (rt_g.debug & DEBUG_HF) {
		vect_t t;

		MAT4X3PNT(t, isect->dsp->dsp_i.dsp_stom, curr_pt);
		bu_log(" in cell(%d,%d)  pt(%g %g %g) ss_dist:%g\n",
			grid_cell[X], grid_cell[Y], V3ARGS(t),
			bbin_dist);

		MAT4X3PNT(t, isect->dsp->dsp_i.dsp_stom, bbout_pt);
		bu_log("out cell(%d,%d)  pt(%g %g %g) ss_dist:%g\n",
			bbout_cell[X], bbout_cell[Y], V3ARGS(t),
			out_dist);
	}

	rising = (isect->r.r_dir[Z] > 0.); /* compute Z direction */


	if (isect->r.r_dir[X] < 0.0) {
		stepX = -1;	/* cell delta for stepping X dir on ray */
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

	} else {
		stepX = 1;
		insurfX = BBSURF(XMIN);
		outsurfX = BBSURF(XMAX);

		tDX = 1.0 / isect->r.r_dir[X];

		if (isect->r.r_dir[X] > 0.0) {
			tX = ((grid_cell[X]+1) - curr_pt[X]) / isect->r.r_dir[X];
		} else
			tX = MAX_FASTF;
	}
	if (isect->r.r_dir[Y] < 0) {
		stepY = -1;
		insurfY = BBSURF(YMAX);
		outsurfY = BBSURF(YMIN);

		tDY = -1.0 / isect->r.r_dir[Y];

		tY = (grid_cell[Y] - curr_pt[Y]) / isect->r.r_dir[Y];
	} else {
		stepY = 1;
		insurfY = BBSURF(YMIN);
		outsurfY = BBSURF(YMAX);

		tDY = 1.0 / isect->r.r_dir[Y];

		if (isect->r.r_dir[Y] > 0.0) {
			tY = ((grid_cell[Y]+1) - curr_pt[Y]) / isect->r.r_dir[Y];
		} else
			tY = MAX_FASTF;
	}


	/* precompute some addresses for parameters we're going to
	 * pass frequently to cell_isect();
	 */
	cs.bbin_pt = bbin_pt;
	cs.bbin_dist = bbin_dist;
	cs.grid_cell = grid_cell;
	cs.curr_dist = &curr_dist;
	cs.curr_surf = &curr_surf;
	cs.curr_pt = curr_pt;


	if (rt_g.debug & DEBUG_HF) {
		bu_log("stepX:%d tDX:%g tX:%g\n",
			stepX, tDX, tX);
		bu_log("stepY:%d tDY:%g tY:%g\n",
			stepY,  tDY, tY);
	}


	do {
		if (rt_g.debug & DEBUG_HF) {
			vect_t t;
			MAT4X3PNT(t, isect->dsp->dsp_i.dsp_stom, curr_pt);
bu_log("cell(%d,%d) tX:%g tY:%g  inside=%d\n\t  curr_pt (%g %g %g) surf:%d  ss_dist:%g\n\t",
grid_cell[X], grid_cell[Y], tX, tY, inside, V3ARGS(t), curr_surf, curr_dist);
		}


		if (tX < tY) {

			cell_isect(isect, tX, outsurfX, &cs, &inside, rising,
				&isect_cell_x_wall);

			curr_surf = insurfX;
			grid_cell[X] += stepX;

			/* update dist along ray to next X cell boundary */
			tX += tDX;
		} else {

			cell_isect(isect, tY, outsurfY, &cs, &inside, rising,
				&isect_cell_y_wall);

			curr_surf = insurfY;
			grid_cell[Y] += stepY;

			tY += tDY;
		}


	} while ( grid_cell[X] >= cell_bbox[XMIN] &&
		grid_cell[X] <= cell_bbox[XMAX] &&
		grid_cell[Y] >= cell_bbox[YMIN] &&
		grid_cell[Y] <= cell_bbox[YMAX] );


	if (inside) {
		HIT_COMMIT( isect );
		inside = 0;
	}
/* rt_g.debug & DEBUG_HF */
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
	static CONST point_t junk = { 0.0, 0.0, 0.0 };

	if (rt_g.debug & DEBUG_HF)
		bu_log("rt_dsp_shot(pt:(%g %g %g)\n\tdir[%g]:(%g %g %g))\n    pixel(%d,%d)\n",
			V3ARGS(rp->r_pt),
			MAGNITUDE(rp->r_dir),
			V3ARGS(rp->r_dir),
			ap->a_x, ap->a_y);

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
	isect.sp_is_valid = 0;

	/* intersect ray with bounding cube */
	if ( isect_ray_bbox(&isect) == 0)
		return 0;


	BU_LIST_INIT(&isect.seglist);
	isect.sp = (struct seg *)NULL;

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

		INHIT( (&isect), isect.minbox.in_dist,
			BBSURF(isect.minbox.in_surf), junk);

		OUTHIT( (&isect), isect.minbox.out_dist,
			BBSURF(isect.minbox.out_surf), junk);

		HIT_COMMIT( (&isect) );

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

	(void)rt_vstub( stp, rp, segp, n, ap );
}

static void
compute_normal_at_gridpoint(N, dsp, x, y)
vect_t N;
struct dsp_specific *dsp;
int x, y;
{
	/*  Gridpoint specified is "B" we compute normal based upon
	 *  A/C, D/E
	 *
	 * 		E
	 *
	 *		|
	 *
	 *	A   -	B   -	C
	 *
	 *		|
	 *
	 *		D
	 */
	
	point_t A, B, C, D, E, tmp;
	vect_t Vac, Vde;

	if (x < 1) {
		VSET(tmp, x, y, DSP(dsp, x, y) );
	} else {
		VSET(tmp, x-1, y, DSP(dsp, x-1, y) );
	}
	MAT4X3PNT(A, dsp->dsp_i.dsp_stom, tmp);

	if (x >= XSIZ(dsp)) {
		VSET(tmp, x, y,  DSP(dsp, x, y) );
	} else {
		VSET(tmp, x+1, y,  DSP(dsp, x+1, y) );
	}
	MAT4X3PNT(C, dsp->dsp_i.dsp_stom, tmp);


	if (y < 1) {
		VSET(tmp, x, y, DSP(dsp, x, y) );
	} else {
		VSET(tmp, x, y-1, DSP(dsp, x, y-1) );
	}
	MAT4X3PNT(D, dsp->dsp_i.dsp_stom, tmp);

	if (y >= YSIZ(dsp)) {
		VSET(tmp, x, y, DSP(dsp, x, y) );
	} else {
		VSET(tmp, x, y+1, DSP(dsp, x, y+1) );
	}

	MAT4X3PNT(E, dsp->dsp_i.dsp_stom, tmp);

	VSUB2(Vac, A, C);
	VSUB2(Vde, D, E);

	VCROSS(N, Vac, Vde);

	VUNITIZE(N);

	if (rt_g.debug & DEBUG_HF) {
		char buf[32];
		FILE *fd;

		VSET(tmp, x, y, DSP(dsp, x, y));
		MAT4X3PNT(B, dsp->dsp_i.dsp_stom, tmp);

		RES_ACQUIRE( &rt_g.res_model);
		sprintf(buf, "dsp%d.pl", plot_file_num++);
		RES_RELEASE( &rt_g.res_model);
		bu_log("%s\n", buf);

		RES_ACQUIRE( &rt_g.res_syscall);
		if ((fd=fopen(buf, "w")) != (FILE *)NULL) {
			pl_color(fd, 255, 255, 0);
			VJOIN1(tmp, B, 1.0, N);
			pdv_3line(fd, B, tmp);

			pl_color(fd, 255, 140, 0);
			VUNITIZE(Vac);
			VUNITIZE(Vde);

			VJOIN1(tmp, B, 1.0, Vac);
			pdv_3line(fd, B, tmp);

			VJOIN1(tmp, B, 1.0, Vde);
			pdv_3line(fd, B, tmp);

			fclose(fd);
		}
		RES_RELEASE( &rt_g.res_syscall);
	}






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
	vect_t N, tmp, A, B, C, D, AB, AC, AD;
	int cell[2];
	char buf[32];

 	if (rt_g.debug & DEBUG_HF)
		bu_log("rt_dsp_norm()\n");

	VJOIN1( hitp->hit_point, rp->r_pt, hitp->hit_dist, rp->r_dir );
	if (hitp->hit_surfno < 0) {
		MAT4X3VEC( N, dsp->dsp_i.dsp_stom, 
			dsp_pl[ BBSURF(hitp->hit_surfno) ] );
	} else if (dsp->dsp_i.dsp_smooth) {
		if ( hitp->hit_surfno == TRI2 ||  hitp->hit_surfno == TRI1 ||
		     hitp->hit_surfno == TRI3 ||  hitp->hit_surfno == TRI4 ) {
			vect_t Anorm, Bnorm, Dnorm, Cnorm, ABnorm, CDnorm;
			double Xfrac, Yfrac;
			int x, y;
			point_t pt;
	
			x = hitp->hit_vpriv[X];
			y = hitp->hit_vpriv[Y];
	
			compute_normal_at_gridpoint(Anorm, dsp, x, y);
			compute_normal_at_gridpoint(Bnorm, dsp, x+1, y);
			compute_normal_at_gridpoint(Dnorm, dsp, x+1, y+1);
			compute_normal_at_gridpoint(Cnorm, dsp, x, y+1);
	
			VSET(A, x,   y, DSP(dsp, x, y)  );
	
			VSET(D, x+1, y+1, DSP(dsp, x+1, y+1));
	
			MAT4X3PNT(pt, dsp->dsp_i.dsp_mtos, hitp->hit_point);
	

			Xfrac = (pt[X] - A[X]) / (D[X] - A[X]);
			Yfrac = (pt[Y] - A[Y]) / (D[Y] - A[Y]);

#if 1
#define SMOOTHSTEP(x)  ((x)*(x)*(3 - 2*(x)))
			if (Xfrac < 0.0) Xfrac = 0.0;
			if (Xfrac > 1.0) Xfrac = 1.0;
			Xfrac = SMOOTHSTEP( Xfrac );

			if (Yfrac < 0.0) Yfrac = 0.0;
			if (Yfrac > 1.0) Yfrac = 1.0;
			Yfrac = SMOOTHSTEP( Yfrac );
#endif
			VSCALE(Anorm, Anorm, (1.0-Xfrac) );
			VSCALE(Bnorm, Bnorm,      Xfrac  );
			VADD2(ABnorm, Anorm, Bnorm);
			VUNITIZE(ABnorm);

			VSCALE(Cnorm, Cnorm, (1.0-Xfrac) );
			VSCALE(Dnorm, Dnorm,      Xfrac  );
			VADD2(CDnorm, Dnorm, Cnorm);
			VUNITIZE(CDnorm);

			VSCALE(ABnorm, ABnorm, (1.0-Yfrac) );
			VSCALE(CDnorm, CDnorm, Yfrac );
	
			VADD2(N, ABnorm, CDnorm);
			if (rt_g.debug & DEBUG_HF) {
				FILE *fd;

				VPRINT("A", A);
				VPRINT("D", D);
				VPRINT("pt", pt);
				bu_log("Xfrac:%g Yfrac:%g\n", Xfrac, Yfrac);

				RES_ACQUIRE( &rt_g.res_model);
				sprintf(buf, "dsp%d.pl", plot_file_num++);
				RES_RELEASE( &rt_g.res_model);
				bu_log("plotting normal in %s\n", buf);

				RES_ACQUIRE( &rt_g.res_syscall);
				if ((fd=fopen(buf, "w")) != (FILE *)NULL) {
					pl_color(fd, 220, 220, 90);

					VJOIN1(tmp, hitp->hit_point, 1.0, Anorm);
					pdv_3line(fd, hitp->hit_point, tmp);

					VJOIN1(tmp, hitp->hit_point, 1.0, Bnorm);
					pdv_3line(fd, hitp->hit_point, tmp);

					VJOIN1(tmp, hitp->hit_point, 1.0, Dnorm);
					pdv_3line(fd, hitp->hit_point, tmp);

					VJOIN1(tmp, hitp->hit_point, 1.0, Cnorm);
					pdv_3line(fd, hitp->hit_point, tmp);

					fclose(fd);
				}
				RES_RELEASE( &rt_g.res_syscall);
			}
		} else {
			bu_log("%s:%d ", __FILE__, __LINE__);
			bu_log("bogus surface of DSP %d\n", hitp->hit_surfno);
			bu_bomb("");
		}
	} else {
		/* XXX this section should be replaced with code in
		 * isect_ray_triangles() to save the normal
		 * the only thing we should do here is transform the normal
		 * from solid to model space
		 */

		cell[X] = hitp->hit_vpriv[X];
		cell[Y] = hitp->hit_vpriv[Y];
		if ( hitp->hit_surfno == TRI1 ) {
		
			VSET(tmp, cell[X],   cell[Y],   DSP(dsp, cell[X],   cell[Y])  );
			MAT4X3PNT(A, dsp->dsp_i.dsp_stom, tmp);

			VSET(tmp, cell[X]+1, cell[Y],   DSP(dsp, cell[X]+1, cell[Y])  );
			MAT4X3PNT(B, dsp->dsp_i.dsp_stom, tmp);

			VSET(tmp, cell[X]+1, cell[Y]+1, DSP(dsp, cell[X]+1, cell[Y]+1));
			MAT4X3PNT(D, dsp->dsp_i.dsp_stom, tmp);


			VSUB2(AB, B, A);
			VSUB2(AD, D, A);

			VCROSS(N, AB, AD); 
		} else if ( hitp->hit_surfno == TRI2 ) {

			VSET(tmp, cell[X],   cell[Y],   DSP(dsp, cell[X],   cell[Y])  );
			MAT4X3PNT(A, dsp->dsp_i.dsp_stom, tmp);

			VSET(tmp, cell[X],   cell[Y]+1, DSP(dsp, cell[X],   cell[Y]+1));
			MAT4X3PNT(C, dsp->dsp_i.dsp_stom, tmp);

			VSET(tmp, cell[X]+1, cell[Y]+1, DSP(dsp, cell[X]+1, cell[Y]+1));
			MAT4X3PNT(D, dsp->dsp_i.dsp_stom, tmp);

			VSUB2(AD, D, A);
			VSUB2(AC, C, A);


			VCROSS(N, AD, AC); 
		} else if ( hitp->hit_surfno == TRI3 ) {
			/* alternate cut cell */
			
			VSET(tmp, cell[X]+1,   cell[Y],
			 DSP(dsp, cell[X]+1,   cell[Y])  );
			MAT4X3PNT(A, dsp->dsp_i.dsp_stom, tmp);

			VSET(tmp, cell[X]+1, cell[Y]+1,  
			 DSP(dsp, cell[X]+1, cell[Y]+1)  );
			MAT4X3PNT(B, dsp->dsp_i.dsp_stom, tmp);

			VSET(tmp, cell[X], cell[Y]+1,
			 DSP(dsp, cell[X], cell[Y]+1) );
			MAT4X3PNT(D, dsp->dsp_i.dsp_stom, tmp);

			VSUB2(AB, B, A);
			VSUB2(AD, D, A);

			VCROSS(N, AB, AD); 

		} else if ( hitp->hit_surfno == TRI4 ) {
			/* alternate cut cell */

			VSET(tmp, cell[X]+1,   cell[Y],  
			 DSP(dsp, cell[X]+1,   cell[Y])  );
			MAT4X3PNT(A, dsp->dsp_i.dsp_stom, tmp);

			VSET(tmp, cell[X],   cell[Y],
			 DSP(dsp, cell[X],   cell[Y]));
			MAT4X3PNT(C, dsp->dsp_i.dsp_stom, tmp);

			VSET(tmp, cell[X], cell[Y]+1,
			 DSP(dsp, cell[X], cell[Y]+1));
			MAT4X3PNT(D, dsp->dsp_i.dsp_stom, tmp);

			VSUB2(AD, D, A);
			VSUB2(AC, C, A);

			VCROSS(N, AD, AC); 
		} else {
			bu_log("%s:%d ", __FILE__, __LINE__);
			bu_log("bogus surface of DSP %d\n", hitp->hit_surfno);
			bu_bomb("");
		}
	}

	VUNITIZE(N);
	VMOVE(hitp->hit_normal, N);

	
	if (rt_g.debug & DEBUG_HF) {
		vect_t tmp;
		char buf[32];
		FILE *fd;

		if ( hitp->hit_surfno == TRI1 || hitp->hit_surfno == TRI2 ) {
			bu_log("surf[%d] cell(%d,%d) pt(%g %g %g) Norm[%g %g %g](%g %g %g)\n",
				hitp->hit_surfno,
				cell[X], cell[Y],
				V3ARGS(hitp->hit_point),
				V3ARGS(dsp_pl[ hitp->hit_surfno ]),
				V3ARGS(N));
		} else {
			bu_log("surf[%d] cell(?,?) pt(%g %g %g) Norm[%g %g %g](%g %g %g)\n",
				hitp->hit_surfno,
				V3ARGS(hitp->hit_point),
				V3ARGS(dsp_pl[ hitp->hit_surfno ]),
				V3ARGS(N));
		}
		RES_ACQUIRE( &rt_g.res_model);
		sprintf(buf, "dsp%d.pl", plot_file_num++);
		RES_RELEASE( &rt_g.res_model);
		bu_log("plotting normal in %s\n", buf);

		RES_ACQUIRE( &rt_g.res_syscall);
		if ((fd=fopen(buf, "w")) != (FILE *)NULL) {
			pl_color(fd, 90, 220, 90);

			pdv_3move(fd, hitp->hit_point);
			VJOIN1(tmp, hitp->hit_point, 1.0, N);
			pdv_3cont(fd, tmp);
			fclose(fd);
		}
		RES_RELEASE( &rt_g.res_syscall);
	}
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
 *  For a hit on the surface of a dsp, return the (u,v) coordinates
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
	point_t pt;
	int x, y;
	vect_t v_m, v_s, tmp;
	double u, v, r;

	MAT4X3PNT(pt, dsp->dsp_i.dsp_mtos, hitp->hit_point);

	x = pt[X];	/* float/int conv */
	y = pt[Y];	/* float/int conv */

	if (x >= XSIZ(dsp)) x = XSIZ(dsp)-1;
	if (y >= YSIZ(dsp)) y = YSIZ(dsp)-1;

	u = x / (XSIZ(dsp)-1.0);
	v = y / (YSIZ(dsp)-1.0);

	uvp->uv_u = u;
	uvp->uv_v = v;

#ifdef DUDV
	r = ap->a_rbeam + ap->a_diverge * hitp->hit_dist;

	VSETALL(v_m, r);
	VUNITIZE(v_m);

	MAT4X3VEC(v_s, dsp->dsp_i.dsp_mtos, v_m);

	/* XXX this is a gross approximation */
	VSET(tmp, v_s[X], 0.0, 0.0);
	uvp->uv_du = MAGNITUDE(tmp);

	VSET(tmp, 0.0, v_s[Y], 0.0);
	uvp->uv_dv = MAGNITUDE(tmp);
#else
	/* XXX The texture anti-aliasing stuff doesn't actually work */
	uvp->uv_du = 0.0;
	uvp->uv_dv = 0.0;
#endif
	if (rt_g.debug & DEBUG_HF)
		bu_log("rt_dsp_uv(pt:%g,%g siz:%d,%d) u=%g v=%g du=%g dv=%g\n",
			pt[X], pt[Y], XSIZ(dsp), YSIZ(dsp), 
			uvp->uv_u, uvp->uv_v,
			uvp->uv_du, uvp->uv_dv);
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
/*----------------------------------------------------------------------*/



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
	dsp_ip->dsp_smooth = 1;
	mat_idn(dsp_ip->dsp_stom);
	mat_idn(dsp_ip->dsp_mtos);

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
