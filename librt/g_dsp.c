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
 *  	hit_vpriv[X,Y] holds the cell that was hit.
 *	hit_vpriv[Z] is 0 if this was an in-hit.  1 if an out-hit
 *	surfno is either  TRI1 or TRI2  (XXX TRI3/TRI4?)
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
static const char RCSdsp[] = "@(#)$Header$ (BRL)";
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
#include <setjmp.h>

extern int rt_retrieve_binunif(struct rt_db_internal *intern,
			       const struct db_i	*dbip,
			       const char *name);

extern void rt_binunif_ifree( struct rt_db_internal	*ip );


#define dlog if (rt_g.debug & DEBUG_HF) bu_log	


#define BBSURF(_s) (-((_s)+1))	/* bounding box surface */
#define BBOX_PLANES	7	/* 2 tops & 5 other sides */
#define XMIN 0
#define XMAX 1
#define YMIN 2
#define YMAX 3
#define ZMIN 4
#define ZMAX 5
#define ZMID 6

#define XMIN_SURF -1
#define XMAX_SURF -2
#define YMIN_SURF -3
#define YMAX_SURF -4
#define ZMIN_SURF -5
#define ZMAX_SURF -6
#define ZMID_SURF -7

#define TRI1	10
#define TRI2	20
#define TRI3	30
#define TRI4	40

/* access to the array */
#define DSP(_p,_x,_y) ( \
	((unsigned short *)(((struct rt_dsp_internal *)_p)->dsp_buf))[ \
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
	struct bu_list		seglist;	/* list of segments */
	struct xray		r;		/* solid space ray */
	struct application	*ap;
	struct soltab		*stp;
	struct bn_tol		*tol;

	struct bbox_isect bbox;
	struct bbox_isect minbox;

	struct seg *sp;		/* the current segment being filled */
	int sp_is_valid;	/* boolean: sp allocated & (inhit) set */
	int sp_is_done;		/* boolean: sp has (outhit) content */
	jmp_buf env;		/* for setjmp()/longjmp() */
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

/*
 * This function computes the DSP mtos matrix from the stom matrix
 * whenever the stom matrix is parsed using a bu_structparse.
 */
static void
hook_mtos_from_stom(
		    const struct bu_structparse	*ip,
		    const char 			*sp_name,
		    genptr_t			base,		    
		    char			*p)
{
	struct rt_dsp_internal *dsp_ip = (struct rt_dsp_internal *)base;

	bn_mat_inv(dsp_ip->dsp_mtos, dsp_ip->dsp_stom);
}
static void
hook_file(
		    const struct bu_structparse	*ip,
		    const char 			*sp_name,
		    genptr_t			base,		    
		    char			*p)
{
	struct rt_dsp_internal *dsp_ip = (struct rt_dsp_internal *)base;

	dsp_ip->dsp_datasrc = RT_DSP_SRC_V4_FILE;
}


#define DSP_O(m) offsetof(struct rt_dsp_internal, m)
#define DSP_AO(a) bu_offsetofarray(struct rt_dsp_internal, a)

CONST struct bu_structparse rt_dsp_parse[] = {
	{"%S",	1, "file", DSP_O(dsp_name), hook_file },
	{"%d",  1, "sm", DSP_O(dsp_smooth), BU_STRUCTPARSE_FUNC_NULL },
	{"%d",	1, "w", DSP_O(dsp_xcnt), BU_STRUCTPARSE_FUNC_NULL },
	{"%d",	1, "n", DSP_O(dsp_ycnt), BU_STRUCTPARSE_FUNC_NULL },
	{"%f", 16, "stom", DSP_AO(dsp_stom), hook_mtos_from_stom },
	{"",	0, (char *)0, 0,	BU_STRUCTPARSE_FUNC_NULL }
};

CONST struct bu_structparse rt_dsp_ptab[] = {
	{"%S",	1, "file", DSP_O(dsp_name), BU_STRUCTPARSE_FUNC_NULL },
	{"%d",  1, "sm", DSP_O(dsp_smooth), BU_STRUCTPARSE_FUNC_NULL },
	{"%d",	1, "w", DSP_O(dsp_xcnt), BU_STRUCTPARSE_FUNC_NULL },
	{"%d",	1, "n", DSP_O(dsp_ycnt), BU_STRUCTPARSE_FUNC_NULL },
	{"%f", 16, "stom", DSP_AO(dsp_stom), BU_STRUCTPARSE_FUNC_NULL },
	{"",	0, (char *)0, 0,	BU_STRUCTPARSE_FUNC_NULL }
};


static int plot_file_num=0;
static int plot_em=1;

static void
dsp_print_v4(vls, dsp_ip)
struct bu_vls *vls;
CONST struct rt_dsp_internal *dsp_ip;
{
	point_t pt, v;
	RT_DSP_CK_MAGIC(dsp_ip);
	BU_CK_VLS(&dsp_ip->dsp_name);

	bu_vls_init( vls );

	bu_vls_printf( vls, "Displacement Map\n  file='%s' w=%d n=%d sm=%d ",
		       bu_vls_addr(&dsp_ip->dsp_name),
		       dsp_ip->dsp_xcnt,
		       dsp_ip->dsp_ycnt,
		       dsp_ip->dsp_smooth);

	VSETALL(pt, 0.0);

	MAT4X3PNT(v, dsp_ip->dsp_stom, pt);

	bu_vls_printf( vls, " (origin at %g %g %g)mm\n", V3ARGS(v));

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

static void
dsp_print_v5(vls, dsp_ip)
struct bu_vls *vls;
CONST struct rt_dsp_internal *dsp_ip;
{
	point_t pt, v;
	RT_DSP_CK_MAGIC(dsp_ip);
	BU_CK_VLS(&dsp_ip->dsp_name);

	bu_vls_init( vls );

	bu_vls_printf( vls, "Displacement Map\n" );

	switch (dsp_ip->dsp_datasrc) {
	case RT_DSP_SRC_FILE:
		bu_vls_printf( vls, "  file");
		break;
	case RT_DSP_SRC_OBJ:
		bu_vls_printf( vls, "  obj");
		break;
	default:
		bu_vls_printf( vls, "unk src type'%c'", dsp_ip->dsp_datasrc);
		break;
	}

	bu_vls_printf( vls, "='%s'\n  w=%d n=%d sm=%d ",
		       bu_vls_addr(&dsp_ip->dsp_name),
		       dsp_ip->dsp_xcnt,
		       dsp_ip->dsp_ycnt,
		       dsp_ip->dsp_smooth);

	VSETALL(pt, 0.0);

	MAT4X3PNT(v, dsp_ip->dsp_stom, pt);

	bu_vls_printf( vls, " (origin at %g %g %g)mm\n", V3ARGS(v));

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

void
dsp_dump(struct rt_dsp_internal *dsp)
{
	struct bu_vls vls;

	dsp_print_v5(&vls, dsp);

	bu_log("%s", bu_vls_addr(&vls));

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
	BU_CK_VLS(&dsp->dsp_i.dsp_name);

	switch (stp->st_rtip->rti_dbip->dbi_version) {
	case 4: 
		dsp_print_v4(&vls, &(dsp->dsp_i) );
		break;
	case 5:
		dsp_print_v5(&vls, &(dsp->dsp_i) );
		break;
	}

	bu_log("%s", bu_vls_addr( &vls));

	if (BU_VLS_IS_INITIALIZED( &vls )) bu_vls_free( &vls );

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
	BU_CK_VLS(&dsp_ip->dsp_name);

	BU_CK_MAPPED_FILE(dsp_ip->dsp_mp);

	BU_GETSTRUCT( dsp, dsp_specific );
	stp->st_specific = (genptr_t) dsp;
	dsp->dsp_i = *dsp_ip;		/* struct copy */

	bu_semaphore_acquire( RT_SEM_MODEL);
	++dsp_ip->dsp_mp->uses;
	bu_semaphore_release( RT_SEM_MODEL);

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
		bu_log("  x:%d y:%d min %d max %d\n", XCNT(dsp), YCNT(dsp), dsp_min, dsp_max);

	/* record the distance to each of the bounding planes */
	dsp->dsp_pl_dist[XMIN] = 0.0;
	dsp->dsp_pl_dist[XMAX] = (fastf_t)dsp->xsiz;
	dsp->dsp_pl_dist[YMIN] = 0.0;
	dsp->dsp_pl_dist[YMAX] = (fastf_t)dsp->ysiz;
	dsp->dsp_pl_dist[ZMIN] = 0.0;
	dsp->dsp_pl_dist[ZMAX] = (fastf_t)dsp_max;
	dsp->dsp_pl_dist[ZMID] = (fastf_t)dsp_min;

	/* compute enlarged bounding box and spere */

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
	BBOX_PT(dsp_ip->dsp_xcnt+.1, dsp_ip->dsp_ycnt+.1, dsp_max+.1);
	BBOX_PT(-.1,		    dsp_ip->dsp_ycnt+.1, dsp_max+.1);

#undef BBOX_PT

	VADD2SCALE( stp->st_center, stp->st_min, stp->st_max, 0.5 );
	VSUB2SCALE( work, stp->st_max, stp->st_min, 0.5 );

	f = work[X];
	if (work[Y] > f )  f = work[Y];
	if (work[Z] > f )  f = work[Z];
	stp->st_aradius = f;
	stp->st_bradius = MAGNITUDE(work);

	if (rt_g.debug & DEBUG_HF) {
		bu_log("  model space bbox (%g %g %g) (%g %g %g)\n",
			V3ARGS(stp->st_min),
			V3ARGS(stp->st_max));
	}

	BU_CK_MAPPED_FILE(dsp_ip->dsp_mp);

	return 0;
}


/*
 *	Register an in-bound hit on a cell
 */
#define INHIT(isp, dist, surf, cell, norm) \
	inhit(isp, dist, surf, cell, norm, __FILE__, __LINE__)
void
inhit(isp, dist, surf, cell, norm, file, line)
struct isect_stuff *isp;
double dist;
int surf;
int cell[2];
vect_t norm;
char *file;
int line;
{

	if (isp->sp_is_valid) {

		/* if we have a hit point exactly on the top of 2 cells
		 * at the exact border, we can get 2 hits on the same point.
		 * We just ignore the second hit in this case.
		 */
		if (fabs(dist - isp->sp->seg_in.hit_dist) < isp->tol->dist)
			return;


		bu_log("%s:%d pixel(%d,%d) ", file, line,
			isp->ap->a_x, isp->ap->a_y);
		bu_log("trying to enter solid when inside.\n");
		bu_log("\tnew dist %g\n\told dist %g difference: %g\n",
			dist, isp->sp->seg_in.hit_dist,
			dist - isp->sp->seg_in.hit_dist);
		longjmp(isp->env, 1);
	}

	RT_GET_SEG( isp->sp, isp->ap->a_resource );
	isp->sp->seg_stp = isp->stp;
	isp->sp->seg_in.hit_dist = dist;
	isp->sp->seg_in.hit_surfno = surf;
	isp->sp->seg_in.hit_vpriv[X] = cell[X];
	isp->sp->seg_in.hit_vpriv[Y] = cell[Y];
	isp->sp->seg_in.hit_vpriv[Z] = 0.0;
	VMOVE(isp->sp->seg_in.hit_normal, norm);
	isp->sp_is_valid = 1;

	if (rt_g.debug & DEBUG_HF) {
		point_t in, t;
		VJOIN1(t, isp->r.r_pt, dist, isp->r.r_dir);
		MAT4X3PNT(in, isp->dsp->dsp_i.dsp_stom, t);
		bu_log("line %d(%d) New in pt(%g %g %g) ss_dist %g surf: %d\n",
			__LINE__, line, V3ARGS(in), dist, surf);
		bu_log("\tNormal: %g %g %g\n", V3ARGS(norm));
	}
}




/*
 *	Register an out-bound hit on a cell
 */
#define OUTHIT(isp, dist, surf, cell, norm) \
	outhit(isp, dist, surf, cell, norm, __FILE__, __LINE__)
void
outhit(isp, dist, surf, cell, norm, file, line)
struct isect_stuff *isp;
double dist;
int surf;
int cell[2];
vect_t norm;
char *file;
int line;
{
	point_t out, t;

	if (! isp->sp_is_valid) {
		bu_log("%s:%d pixel(%d,%d) ", file, line, 
			isp->ap->a_x, isp->ap->a_y);
		bu_log("Trying to set outpoint when not inside\n");
		longjmp(isp->env, 1);
	}

	isp->sp->seg_out.hit_dist = dist;
	isp->sp->seg_out.hit_surfno = surf;
	isp->sp->seg_out.hit_vpriv[X] = cell[X];
	isp->sp->seg_out.hit_vpriv[Y] = cell[Y];
	isp->sp->seg_out.hit_vpriv[Z] = 1.0;
	VMOVE(isp->sp->seg_out.hit_normal, norm);
	isp->sp_is_done = 1;

	if (rt_g.debug & DEBUG_HF) {

		VJOIN1(t, isp->r.r_pt, dist, isp->r.r_dir);
		MAT4X3PNT(out, isp->dsp->dsp_i.dsp_stom, t);
		bu_log("line %d(%d) New out pt(%g %g %g) ss_dist %g surf:%d\n",
			__LINE__, line, V3ARGS(out), dist, surf);
		bu_log("\tNormal: %g %g %g\n", V3ARGS(norm));
	}
}





/*
 *  Commit a completed segment (in-hit, out-hit) to the intersection result
 */
#define HIT_COMMIT(isp) hit_commit(isp, __FILE__, __LINE__)
void
hit_commit(isp, file, line)
struct isect_stuff *isp;
char *file;
int line;
{
	if ( ! (isp)->sp_is_valid) {
		bu_log("%s:%d pixel(%d,%d) ", file, line,
			isp->ap->a_x, isp->ap->a_y);
		bu_log("attempt to commit an invalid seg\n");
		longjmp(isp->env, 1);
	}
	if ( ! (isp)->sp_is_done) {
		bu_log("%s:%d pixel(%d,%d) ", file, line,
			isp->ap->a_x, isp->ap->a_y);
		bu_log("attempt to commit an incomplete seg\n");
		longjmp(isp->env, 1);
	}

	if (rt_g.debug & DEBUG_HF) {
		bu_log("line %d Committing seg %g -> %g\n", line,
			(isp)->sp->seg_in.hit_dist,
			(isp)->sp->seg_out.hit_dist);
	}

	BU_LIST_INSERT( &(isp)->seglist, &( isp->sp->l) );
	(isp)->sp = (struct seg *)NULL;
	(isp)->sp_is_valid = 0;
	(isp)->sp_is_done = 0;
}



/*
 *  Intersect the ray with the real bounding box of the dsp solid.
 */
static int
isect_ray_bbox(isect)
register struct isect_stuff *isect;
{
	register double NdotD = -42.0;
	register double NdotPt;
	double dist;
	register int plane;

	if (rt_g.debug & DEBUG_HF)
		bu_log("isect_ray_bbox(tol dist:%g perp:%g para:%g)\n",
			isect->tol->dist, isect->tol->perp, isect->tol->para);

	isect->bbox.in_dist = - (isect->bbox.out_dist = MAX_FASTF);
	isect->bbox.in_surf = isect->bbox.out_surf = -1;

	/* intersect the ray with the bounding box */
	for (plane=0 ; plane < BBOX_PLANES ; plane++) {
		NdotD = VDOT(dsp_pl[plane], isect->r.r_dir);
		NdotPt = VDOT(dsp_pl[plane], isect->r.r_pt);

		if (rt_g.debug & DEBUG_HF)
			bu_log("\nplane %d NdotD:%g\n", plane, NdotD);

		/* if N/ray vectors are perp, ray misses plane */
		if (plane != ZMID && fabs(NdotD) < isect->tol->perp) {

			if ( (NdotPt - isect->dsp->dsp_pl_dist[plane]) >
			    SQRT_SMALL_FASTF ) {
				if (rt_g.debug & DEBUG_HF)
					bu_log("ray parallel and above bbox plane %d\n", plane);

				return 0; /* MISS */
			}
		}
		dist = - ( NdotPt - isect->dsp->dsp_pl_dist[plane] ) / NdotD;


		if (rt_g.debug & DEBUG_HF)
			bu_log("plane[%d](%g %g %g %g) dot:%g ss_dist:%g\n",
				plane, V3ARGS(dsp_pl[plane]),
				isect->dsp->dsp_pl_dist[plane],
				NdotD, dist);

		if ( plane == ZMID) /* dsp_min elevation not valid bbox limit */
			continue;

		if (NdotD < 0.0) { 
			/* ray dir and normal oppose,  entering */
			if (dist > isect->bbox.in_dist) {
				dlog("new in dist %g\n", dist);
				isect->bbox.in_dist = dist;
				isect->bbox.in_surf = plane;
			}
		} else if (NdotD > 0.0) {
			/* leaving */
			if (dist < isect->bbox.out_dist) {
				dlog("new out dist %g\n", dist);
				isect->bbox.out_dist = dist;
				isect->bbox.out_surf = plane;
			}
		} else {
			dlog("NdotD == 0.0\n");
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
			isect->bbox.out_surf, isect->bbox.out_dist, 
			V3ARGS(pt));
	}

	/* validate */
	if (isect->bbox.in_dist >= isect->bbox.out_dist ||
	    isect->bbox.out_dist >= MAX_FASTF ) {
		dlog("apparent miss, in dist: %g out dist:%g\n",
			isect->bbox.in_dist, isect->bbox.out_dist);
		return 0; /* MISS */
	}

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
unsigned short *cell_min;
unsigned short *cell_max;
{
	register unsigned short cmin, cmax, v;

	RT_DSP_CK_MAGIC(dsp);

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





/* 
 * produce a plot file of the ray and the cell being intersected 
 */
static void
plot_cell_ray(isect, cell,
	curr_pt, next_pt, hit1, dist1, hit2, dist2)
struct isect_stuff *isect;
int cell[3];		  /* 2D cell # (coordinates) */
vect_t curr_pt, next_pt;  /* in and out points on cell bbox */
int hit1;		  /* Boolean: dist1 valid */
double dist1;		  /* distance to first cell hit */
int hit2;		  /* Boolean: dist2 valid */
double dist2;		  /* distance to second cell hit */
{
	FILE	*fd;
	char	buf[132];
	point_t	A, B, C, D;
	point_t tmp, pt, in_pt, out_pt;
	unsigned short	min_val;
	unsigned short	max_val;
	int	outside;


	VSET(A, cell[X],   cell[Y],   DSP(isect->dsp, cell[X],   cell[Y])  );
	VSET(B, cell[X]+1, cell[Y],   DSP(isect->dsp, cell[X]+1, cell[Y])  );
	VSET(C, cell[X],   cell[Y]+1, DSP(isect->dsp, cell[X],   cell[Y]+1));
	VSET(D, cell[X]+1, cell[Y]+1, DSP(isect->dsp, cell[X]+1, cell[Y]+1));

	bu_semaphore_acquire( BU_SEM_SYSCALL);
	sprintf(buf, "dsp%02d.pl", plot_file_num++);
	bu_semaphore_release( BU_SEM_SYSCALL);
	bu_log("%s\n", buf);

	bu_semaphore_acquire( BU_SEM_SYSCALL);
	if ((fd=fopen(buf, "w")) == (FILE *)NULL) {
		bu_semaphore_release( BU_SEM_SYSCALL);
		return;
	}




	/* we don't plot the bounds of the box if it's a
	 * zero thickness axis aligned plane
	 */
	if (A[Z] != B[Z] || A[Z] != C[Z] || A[Z] != D[Z]) {
		RT_DSP_CK_MAGIC(isect->dsp);

		cell_minmax(isect->dsp, cell[X], cell[Y], &min_val, &max_val);


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
	} else 
		min_val = max_val = A[Z];

	/* 
	 * plot the ray on the bottom of the box if the ray
	 * is in/below it.  Otherwise, plot the ray on the top
	 * of the box.  This helps us see the overlap of the ray
	 * when it's far away.
	 */
	VMOVE(tmp, curr_pt);
	if (curr_pt[Z] < max_val) tmp[Z] = (double)min_val;
	else			  tmp[Z] = (double)max_val;
	MAT4X3PNT(in_pt, isect->dsp->dsp_i.dsp_stom, tmp);

	VMOVE(tmp, next_pt);
	if (curr_pt[Z] < max_val) tmp[Z] = (double)min_val;
	else			  tmp[Z] = (double)max_val;
	MAT4X3PNT(out_pt, isect->dsp->dsp_i.dsp_stom, tmp);
	pl_color(fd, 120, 120, 240);
	pdv_3move(fd, in_pt);
	pdv_3cont(fd, out_pt);



	/* 
	 * plot the triangles which make up the top of the cell in GREEN
	 */
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
	outside = !isect->sp_is_valid;

	if (outside)	pl_color(fd, 255, 40, 40);	/* RED */
	else		pl_color(fd, 255, 255, 100);	/* YELLOW */


	MAT4X3PNT(in_pt, isect->dsp->dsp_i.dsp_stom, curr_pt);
	pdv_3move(fd, in_pt);

	if (hit1 && hit2) {
		if (dist1 < dist2) {
			VJOIN1(tmp, isect->r.r_pt, dist1, isect->r.r_dir);
			MAT4X3PNT(pt, isect->dsp->dsp_i.dsp_stom, tmp);
			pdv_3cont(fd, pt);

			if (outside) pl_color(fd, 255, 255, 100); /* YELLOW */
			else	     pl_color(fd, 255, 40, 40);   /* RED */
			pdv_3move(fd, pt);

			outside = !outside;
			VJOIN1(tmp, isect->r.r_pt, dist2, isect->r.r_dir);
			MAT4X3PNT(pt, isect->dsp->dsp_i.dsp_stom, tmp);
			pdv_3cont(fd, pt);

			if (outside) pl_color(fd, 255, 255, 100); /* YELLOW */
			else	     pl_color(fd, 255, 40, 40);   /* RED */

			pdv_3move(fd, pt);
			outside = !outside;
		} else {
			VJOIN1(tmp, isect->r.r_pt, dist2, isect->r.r_dir);
			MAT4X3PNT(pt, isect->dsp->dsp_i.dsp_stom, tmp);
			pdv_3cont(fd, pt);

			if (outside) pl_color(fd, 255, 255, 100);
			else	     pl_color(fd, 255, 40, 40);
			pdv_3move(fd, pt);
			outside = !outside;
			VJOIN1(tmp, isect->r.r_pt, dist1, isect->r.r_dir);
			MAT4X3PNT(pt, isect->dsp->dsp_i.dsp_stom, tmp);
			pdv_3cont(fd, pt);

			if (outside) pl_color(fd, 255, 255, 100);
			else	     pl_color(fd, 255, 40, 40);
			pdv_3move(fd, pt);
			outside = !outside;
		}
	} else if (hit1) {
		VJOIN1(tmp, isect->r.r_pt, dist1, isect->r.r_dir);
		MAT4X3PNT(pt, isect->dsp->dsp_i.dsp_stom, tmp);
		pdv_3cont(fd, pt);

		if (outside)	pl_color(fd, 255, 255, 100);
		else		pl_color(fd, 255, 40, 40);
		pdv_3move(fd, pt);

	} else if (hit2) {
		VJOIN1(tmp, isect->r.r_pt, dist2, isect->r.r_dir);
		MAT4X3PNT(pt, isect->dsp->dsp_i.dsp_stom, tmp);
		pdv_3cont(fd, pt);

		if (outside)	pl_color(fd, 255, 255, 100);
		else		pl_color(fd, 255, 40, 40);
		pdv_3move(fd, pt);
	}

		
	MAT4X3PNT(pt, isect->dsp->dsp_i.dsp_stom, next_pt);
	pdv_3cont(fd, pt);


	fclose(fd);
	bu_semaphore_release( BU_SEM_SYSCALL);
}

/* 
 *  This is a support routine for isect_ray_triangles()
 *  The basic idea is to get the coordinates of the four corners of the cell.
 *  The cell top will be cut from point A to point D by isect_ray_triangles()
 *  So we re-order the points so that the cut is optimally placed.  Basically
 *  we decide if the cut will be A-D or B-C.
 */
static void
set_and_permute(isect, cell, A, B, C, D, tri1, tri2)
struct isect_stuff *isect;
int cell[3];
point_t A, B, C, D;
int *tri1;
int *tri2;
{
	int lo[2], hi[2];
	point_t tmp;
	double c1, c2;
	double h1, h2, h3, h4;

	/* Load the corner points of the cell */

	VSET(A, cell[X],   cell[Y],   DSP(isect->dsp, cell[X],   cell[Y])  );
	VSET(B, cell[X]+1, cell[Y],   DSP(isect->dsp, cell[X]+1, cell[Y])  );
	VSET(C, cell[X],   cell[Y]+1, DSP(isect->dsp, cell[X],   cell[Y]+1));
	VSET(D, cell[X]+1, cell[Y]+1, DSP(isect->dsp, cell[X]+1, cell[Y]+1));


	/*
	 *  We look at the points in the diagonal next cells to determine
	 *  the curvature along each diagonal of this cell.  This cell is
	 *  divided into two triangles by cutting across the cell in the
	 *  direction of least curvature.
	 *
	 *	*  *  *	 *
	 *	 \      /
	 *	  \C  D/
	 *	*  *--*  *
	 *	   |\/|
	 *	   |/\|
	 *	*  *--*  *
	 *	  /A  B\
	 *	 /	\
	 *	*  *  *	 *
	 */

	lo[X] = cell[X] - 1;
	lo[Y] = cell[Y] - 1;
	hi[X] = cell[X] + 1;
	hi[Y] = cell[Y] + 1;

	if (lo[X] < 0) lo[X] = 0;
	if (lo[Y] < 0) lo[Y] = 0;

	if (hi[X] > XSIZ(isect->dsp)) hi[X] = XSIZ(isect->dsp);
	if (hi[Y] > YSIZ(isect->dsp)) hi[Y] = YSIZ(isect->dsp);


	/* compute curvature along the A->D direction */
	h1 = DSP(isect->dsp, lo[X], lo[Y]);
	h2 = A[Z];
	h3 = D[Z];
	h4 = DSP(isect->dsp, hi[X], hi[Y]);

	c1 = fabs(h3 + h1 - 2*h2 ) + fabs( h4 + h2 - 2*h3 );


	/* compute curvature along the B->C direction */
	h1 = DSP(isect->dsp, hi[X], lo[Y]);
	h2 = B[Z];
	h3 = C[Z];
	h4 = DSP(isect->dsp, lo[X], hi[Y]);

	c2 = fabs(h3 + h1 - 2*h2 ) + fabs( h4 + h2 - 2*h3 );
	
	if ( c1 < c2 ) {
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
 */
static void
isect_ray_triangles(isect, cell, 
	curr_pt, next_pt)
struct isect_stuff *isect;
int cell[3];
point_t curr_pt, next_pt;
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
	double	dist1 = -42.0;
	double	dist2 = -42.0;
	int hit1;
	int hit2;
	char *reason1;
	char *reason2;
	int tria1, tria2;


	set_and_permute(isect, cell, A, B, C, D, &tria1, &tria2);

	if (rt_g.debug & DEBUG_HF) {
		point_t t;
		bu_log("isect_ray_triangles(inside=%d)\n", isect->sp_is_valid);

		MAT4X3PNT(t, isect->dsp->dsp_i.dsp_stom, A);
		VPRINT("\tA", t);
		MAT4X3PNT(t, isect->dsp->dsp_i.dsp_stom, B);
		VPRINT("\tB", t);
		MAT4X3PNT(t, isect->dsp->dsp_i.dsp_stom, C);
		VPRINT("\tC", t);
		MAT4X3PNT(t, isect->dsp->dsp_i.dsp_stom, D);
		VPRINT("\tD", t);
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
		dlog("\tmiss tri1 %s\n", reason1="perpendicular");
		goto tri2;
	}

	/* project PAxD onto AB giving scaled distance along *AD* */
	alpha = VDOT( AB, PAxD );

	if (NdotD1 > 0.0) alpha = -alpha;

	abs_NdotD = NdotD1 >= 0.0 ? NdotD1 : (-NdotD1);

	if (alpha < 0.0 || alpha > abs_NdotD ) {
		dlog("\tmiss tri1 %s\n", reason1 = "(alpha)");
		goto tri2;
	}

	/* project PAxD onto AD giving scaled distance along *AB* */
	beta = VDOT( AD, PAxD );

	if ( NdotD1 < 0.0 ) beta = -beta;

	if (beta < 0.0 || beta > abs_NdotD ) {
		dlog("\tmiss tri1 %s\n", reason1 = "(beta)");
		goto tri2;
	}
	if ( alpha+beta > abs_NdotD ) {
		dlog("miss tri1 %s\n", reason1 = "(alpha+beta > NdotD)");
		goto tri2;
	}
	hit1 = 1;
	dist1 = VDOT( PA, N1 ) / NdotD1;

tri2:
	if (rt_g.debug & DEBUG_HF) {
		if (hit1)  bu_log("hit tri1 dist %g\n", dist1);
		else bu_log("missed tria1 %s\n", reason1);
	}
	VCROSS(N2, AD, AC);
	NdotD2 = VDOT( N2, isect->r.r_dir);

	hit2 = 0;
	reason2 = (char *)NULL;
	if (BN_VECT_ARE_PERP(NdotD2, isect->tol)) {
		dlog("miss tri2 %s\n", reason2 = "perpendicular");
		goto done;
	}

	/* project PAxD onto AD giving scaled distance along *AC* */
	alpha = VDOT( AD, PAxD );
	if (NdotD2 > 0.0) alpha = -alpha;
	abs_NdotD = NdotD2 >= 0.0 ? NdotD2 : (-NdotD2);
	if (alpha < 0.0 || alpha > abs_NdotD ) {
		dlog("miss tri2 %s\n", reason2 = "(alpha)");
		goto done;
	}

	/* project PAxD onto AC giving scaled distance along *AD* */
	beta = VDOT( AC, PAxD );
	if ( NdotD2 < 0.0 ) beta = -beta;
	if (beta < 0.0 || beta > abs_NdotD ) {
		dlog("miss tri2 %s\n", reason2 = "(beta)");
		goto done;
	}

	if ( alpha+beta > abs_NdotD ) {
		dlog("miss tri2 %s\n", reason2 = "(alpha+beta > NdotD)");
		goto done;
	}

	hit2 = 1;
	dist2 = VDOT( PA, N2 ) / NdotD2;

done:
	if (rt_g.debug & DEBUG_HF) {
		if (hit2)  bu_log("hit tri2 dist %g\n", dist2);
		else bu_log("missed tri2 %s\n", reason2);
	}

	/* plot some diagnostic overlays */
	if (rt_g.debug & DEBUG_HF && plot_em)
		plot_cell_ray(isect, cell, curr_pt, next_pt, 
				hit1, dist1, hit2, dist2);

	if (hit1 && hit2 && ((NdotD1 < 0) == (NdotD2 < 0)) &&
	    BN_APPROXEQUAL(dist1, dist2, isect->tol) ) {
		/* hit a seam, eg. between two triangles of a flat plate. */
		if (rt_g.debug & DEBUG_HF) {
			bu_log("ss_dist1:%g N1:%g %d\n", dist1);
			bu_log("ss_dist2:%g N2:%g %d\n", dist2);
			bu_log("hit seam, skipping hit2\n");
		}

		hit2 = 0;
	}


	/* sort the hit distances and add the hits to the segment list */
	if (hit1 && hit2) {
		int first_tri, second_tri;
		double first, second;
		double firstND, secondND;
		double *firstN, *secondN;

		/* hit both */

		if (dist1 < dist2) {
			if (rt_g.debug & DEBUG_HF)
				bu_log("hit triangles: ss_dist1:%g ss_dist2:%g line:%d\n", dist1, dist2, __LINE__);
			first_tri = tria1;
			firstND = NdotD1;
			first = dist1;
			firstN = N1;
			second_tri = tria2;
			secondND = NdotD2;
			second = dist2;
			secondN = N2;
		} else {
			if (rt_g.debug & DEBUG_HF)
				bu_log("hit triangles: ss_dist2:%g ss_dist1:%g line:%d\n", dist2, dist1, __LINE__);
			first_tri = tria2;
			firstND = NdotD2;
			first = dist2;
			firstN = N2;
			second_tri = tria1;
			secondND = NdotD1;
			second = dist1;
			secondN = N1;
		}

		if (firstND < 0) {
			/* entering, according to ray/normal comparison */
			INHIT(isect, first, first_tri, cell, firstN);

			if (secondND < 0) {
				/* will fail if not same point */
				INHIT(isect, second, second_tri, 
					cell, secondN);
			}
			OUTHIT(isect, second, second_tri, cell, secondN);
			HIT_COMMIT(isect);

		} else {
			/* leaving */
			OUTHIT(isect, first, first_tri, cell, firstN);
			HIT_COMMIT(isect);

			if (secondND > 0) {
				bu_log("%s:%d ray leaving while outside dsp pixel(%d,%d)",
					__FILE__, __LINE__,
					isect->ap->a_x, isect->ap->a_y);
				longjmp(isect->env, 1);
			}
			INHIT(isect, second, second_tri, cell, secondN);
		}
	} else if (hit1) {
		/* only hit tri 1 */

		if (NdotD1 < 0) {
			/* entering */
			if (rt_g.debug & DEBUG_HF)
				bu_log("hit triangle 1 entering ss_dist:%g\n",
					dist1);

			INHIT(isect, dist1, tria1, cell, N1);
		} else {
			/* leaving */
			if (rt_g.debug & DEBUG_HF)
				bu_log("hit triangle 1 leaving ss_dist:%g\n",
					dist1);

			OUTHIT(isect, dist1, tria1, cell, N1);
			HIT_COMMIT(isect);
		}
	} else if (hit2) {
		/* only hit 2 */

		if (NdotD2 < 0) {
			/* entering */

			if (rt_g.debug & DEBUG_HF)
				bu_log("hit triangle 2 entering ss_dist:%g\n",
					dist2);

			INHIT(isect, dist2, tria2, cell, N2);
		} else {
			/* leaving */

			if (rt_g.debug & DEBUG_HF)
				bu_log("hit triangle 2 leaving ss_dist:%g\n",
					dist2);

			OUTHIT(isect, dist2, tria2, cell, N2);
			HIT_COMMIT(isect);
		}
	} else {
		/* missed everything */
	}



}


/*
 *	Intersect the ray with a wall of a "cell" of the DSP.  In particular,
 *	a wall perpendicular to the X axis.
 */
static int
isect_cell_x_wall(isect, cell, surf, dist, pt)
struct isect_stuff *isect;
int cell[3];	/* cell to intersect */
int surf;	/* wall of cell to intersect */
double dist;	/* in-hit distance to cell along ray */
point_t pt;	/* point on cell wall */
{
	unsigned short a, b;	/* Elevation values on cell wall */
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
		if (fabs(pt[X] - cell[X]) > isect->tol->dist) {
			bu_log("%s:%d pixel %d,%d point (%g %g %g)\n",
				__FILE__, __LINE__, 
				isect->ap->a_x, isect->ap->a_y,
				V3ARGS(pt) );
			bu_log("\tNot on plane X=%d\n", cell[X]);
			longjmp(isect->env, 1);

		}

		a = DSP(isect->dsp, cell[X],   cell[Y]);
		b = DSP(isect->dsp, cell[X],   cell[Y]+1);
	} else if (surf == BBSURF(XMAX)) {
		if (fabs(pt[X] - (cell[X]+1)) > isect->tol->dist) {
			bu_log("%s:%d pixel %d,%d point (%g %g %g)\n",
				__FILE__, __LINE__, 
				isect->ap->a_x, isect->ap->a_y,
				V3ARGS(pt) );
			bu_log("\tNot on plane Y=%d\n", cell[X] + 1);
			longjmp(isect->env, 1);

		}

		a = DSP(isect->dsp, cell[X]+1, cell[Y]);
		b = DSP(isect->dsp, cell[X]+1, cell[Y]+1);
	} else {
		bu_log("%s:%d pixel(%d,%d) ", __FILE__, __LINE__,
			isect->ap->a_x, isect->ap->a_y);
		bu_log("bad surface %d, isect_cell_x_entry_wall()\n", surf);
		longjmp(isect->env, 1);
	}

	wall_top_slope = b - a;
	pt_dy = pt[Y] - cell[Y];

	/* compute the height of the wall top at the Y location of pt */
	wall_top = a + pt_dy * wall_top_slope;

	if (rt_g.debug & DEBUG_HF)
		bu_log("\ta:%d b:%d wall slope:%g top:%g dy:%g pt[Z]:%g\n",
			a, b, wall_top_slope, wall_top, pt_dy, pt[Z]);

	return (wall_top > pt[Z]);
}




/*
 *	Intersect the ray with a wall of a "cell" of the DSP.  In particular,
 *	a wall perpendicular to the Y axis.
 */
static int
isect_cell_y_wall(isect, cell, surf, dist, pt)
struct isect_stuff *isect;
int cell[3];
int surf;
double dist;
point_t pt;
{
	unsigned short a, b;	/* points on cell wall */
	double wall_top_slope;
	double wall_top;	/* wall height at Y of pt */
	double pt_dx;

	if (rt_g.debug & DEBUG_HF) {
		vect_t t;

		MAT4X3PNT(t, isect->dsp->dsp_i.dsp_stom, pt);
		bu_log("isect_cell_y_wall() cell(%d,%d) surf:%d ss_dist:%g pt(%g %g %g)\n",
			V2ARGS(cell), surf, dist, V3ARGS(t));
	}



	if (surf == BBSURF(YMIN)) {
		if ( fabs(pt[Y] - cell[Y]) > isect->tol->dist) {
			bu_log("%s:%d pixel %d,%d point (%g %g %g)\n",
				__FILE__, __LINE__, 
				isect->ap->a_x, isect->ap->a_y,
				V3ARGS(pt) );
			bu_log("\tNot on plane Y=%d\n", cell[Y]);
			longjmp(isect->env, 1);
		}
		a = DSP(isect->dsp, cell[X],   cell[Y]);
		b = DSP(isect->dsp, cell[X]+1, cell[Y]);
	} else if (surf == BBSURF(YMAX)) {
		if ( fabs(pt[Y] - (cell[Y] + 1)) > isect->tol->dist) {
			bu_log("%s:%d pixel %d,%d point (%g %g %g)\n",
				__FILE__, __LINE__, 
				isect->ap->a_x, isect->ap->a_y,
				V3ARGS(pt) );
			bu_log("\tNot on plane Y=%d\n", cell[Y] + 1);
			longjmp(isect->env, 1);
		}
		a = DSP(isect->dsp, cell[X],   cell[Y]+1);
		b = DSP(isect->dsp, cell[X]+1, cell[Y]+1);
	} else {
		bu_log("%s:%d pixel(%d,%d) ", __FILE__, __LINE__,
			isect->ap->a_x, isect->ap->a_y);
		bu_log("bad surface %d, isect_cell_y_entry_wall()\n", surf);
		longjmp(isect->env, 1);
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

/*
 *	Intersect the ray with a wall of a "cell" of the DSP.  In particular,
 *	a wall perpendicular to the Z axis.
 */
static int
isect_cell_z_wall(isect, cell, surf, dist, pt)
struct isect_stuff *isect;
int cell[3];
int surf;
double dist;
point_t pt;
{
	/* make sure the point is really on the bottom of the solid */
	if ( fabs(pt[Z]) > isect->tol->dist) {
		bu_log("%s:%d pixel %d,%d point (%g %g %g)\n",
			__FILE__, __LINE__, 
			isect->ap->a_x, isect->ap->a_y,
			V3ARGS(pt) );
		bu_log("\tNot on plane Z=0\n");
		longjmp(isect->env, 1);
	}


	/* make sure the point is inside the bottom of the cell */
	if (cell[X] <= pt[X] && (cell[X]+1) >= pt[X] &&
	    cell[Y] <= pt[Y] && (cell[Y]+1) >= pt[Y] )
		return 1;

	bu_log("%s:%d pixel %d,%d point (%g %g %g)\n",
		__FILE__, __LINE__, 
		isect->ap->a_x, isect->ap->a_y,
		V3ARGS(pt) );
	bu_log("\tNot inside cell\n");
	longjmp(isect->env, 1);
	/* NOTREACHED */
	return 0;
}



/*
 * A bag of pointers and info that cell_isect needs.  We package it
 * up for convenient parameter passing.
 */
struct cell_stuff {

	int	*grid_cell;	/* grid coordinates for current cell */
	double	 curr_dist;	/* distance along ray to curr_pt */
	pointp_t curr_pt;  /* entry point on grid_cell */
	pointp_t next_pt;  /* exit point on grid_cell */
	double	 next_dist;	/* distance along ray to next_pt */

	int rising;	   /* boolean: ray Z is positive */
	int cell_insurf;   /* surface through which we   entered  grid_cell */
	int cell_outsurf;  /* surface through which we will leave grid_cell */
};



/*
 * determine if a point on a plane is on the wall of a cell in that plane
 *
 */
int
isect_pt_wall(isect, cell, surf, dist, pt, s)
struct isect_stuff *isect;
int cell[2];
int surf;
double dist;
point_t pt;
char *s;
{
	int hit;

	switch (surf) {
	case BBSURF(XMIN) :	/* Intersect X wall of cell */
	case BBSURF(XMAX) :
		hit = isect_cell_x_wall(isect, cell, surf, dist, pt);
		if (rt_g.debug & DEBUG_HF) {
			if (hit)bu_log("\thit X %s-wall\n", s);
			else	bu_log("\tmiss X %s-wall\n", s);
		}
		break;
	case BBSURF(YMIN) :	/* Intersect Y wall of cell */
	case BBSURF(YMAX) :
		hit = isect_cell_y_wall(isect, cell, surf, dist, pt);
		if (rt_g.debug & DEBUG_HF) {
			if (hit)bu_log("\thit Y %s-wall\n", s);
			else	bu_log("\tmiss Y %swall\n", s);
		}
		break;

	case BBSURF(ZMIN) :
		hit = isect_cell_z_wall(isect, cell, surf, dist, pt);
		if (rt_g.debug & DEBUG_HF) {
			if (hit)bu_log("\thit ZMIN %s-wall\n", s);
			else	bu_log("\tmiss ZMIN %s-wall\n", s);
		}
		break;
	case BBSURF(ZMAX) :
		hit = 0; /* can't actually hit the top of the bbox */
		break;
	default :
		bu_log("\t%s:%d pixel(%d,%d) surface %d ", __FILE__, __LINE__,
			isect->ap->a_x, isect->ap->a_y, surf);
		bu_log("\tbad surface to intersect\n");
		longjmp(isect->env, 1);
	}
	return hit;
}



/*	Intersect a ray with a single "cell" (bounded by 4 values) of 
 *	the DSP.  This consists of 2 triangles for the top and 5 quadrilateral
 *	plates for the sides and bottom.
 *
 *       Cell		  Miss above zone
 *
 *	*--__       ----*
 *	|\   --*	|
 *	| \  /  \	| interect zone
 *	|  *__   \	|
 *	|  |  --__* ----*
 *	|  |      |
 *	|  |      |	   Under zone
 *	|  |      |
 */
static void
isect_ray_cell(isect, cs)
struct isect_stuff *isect;
struct cell_stuff *cs;
{
	unsigned short	cell_min;
	unsigned short	cell_max;
	int hit;

	RT_DSP_CK_MAGIC(isect->dsp);

	cell_minmax(isect->dsp, cs->grid_cell[X], cs->grid_cell[Y],
		&cell_min, &cell_max);

	if (rt_g.debug & DEBUG_HF) {
		plot_cell_ray(isect, cs->grid_cell, cs->curr_pt, cs->next_pt,
				0, 0.0, 0, 0.0);
	}

	/* see if we can just skip ahead 1 cell */
	if (( cs->rising && cs->curr_pt[Z] > cell_max) ||
	    (!cs->rising && cs->next_pt[Z] > cell_max) ) {
		/* miss above zone */
		if (rt_g.debug & DEBUG_HF) bu_log("\tmiss above\n");

		return;
	}

	/* if the ray passes completely under the the intersection zone
	 * we can just make note of that and move on.
	 */
	if ( (!cs->rising && cs->curr_pt[Z] < cell_min) ||
	     (cs->rising && cs->next_pt[Z] < cell_min) ) {
		/* in base */
		if (!isect->sp_is_valid) {
			/* entering in base */
			INHIT(isect, cs->curr_dist, cs->cell_insurf,
				cs->grid_cell, 
				dsp_pl[BBSURF(cs->cell_insurf)]);
		}

		/* extend segment out-point to the end of this cell */
		OUTHIT(isect, cs->next_dist, cs->cell_outsurf, 
			cs->grid_cell,
			dsp_pl[BBSURF(cs->cell_outsurf)]);

	     	if (BBSURF(cs->cell_outsurf) == ZMIN ||
		    BBSURF(cs->cell_outsurf) == XMIN ||
		    BBSURF(cs->cell_outsurf) == XMAX ||
		    BBSURF(cs->cell_outsurf) == YMIN ||
		    BBSURF(cs->cell_outsurf) == YMAX ) {
	     		/* we're leaving the DSP */
	     		HIT_COMMIT(isect);
	     	}

		return;
	}

	/* At this point, we know that the ray has some componenet
	 * in the intersection zone, so we must actually do the ray/cell
	 * intersection
	 */



	/* intersect */
	if (rt_g.debug & DEBUG_HF) 
		bu_log("\tintersect %d %d X %s\n", cell_min, cell_max, 
				(cs->rising?"rising":"falling") );



	/* find out if we hit the inbound cell wall */
	if (!isect->sp_is_valid) {
		hit = isect_pt_wall(isect, cs->grid_cell, 
			cs->cell_insurf, cs->curr_dist, cs->curr_pt, "in");
		if (hit) {
			INHIT(isect, cs->curr_dist, cs->cell_insurf,
				cs->grid_cell, 
				dsp_pl[BBSURF(cs->cell_insurf)]);
			if (rt_g.debug & DEBUG_HF) 
				bu_log("\thit inbound wall at %g (%g %g %g)\n",
						cs->curr_dist,
						V3ARGS(cs->curr_pt));
		} else
			if (rt_g.debug & DEBUG_HF) 
				bu_log("\tmissed inbound wall %g (%g %g %g)\n",
						cs->curr_dist,
						V3ARGS(cs->curr_pt));
	}

	/*
	 * This is where we actually intersect the ray with the top of
	 * the cell.
	 */
	isect_ray_triangles(isect, cs->grid_cell, cs->curr_pt, cs->next_pt);



	if (isect->sp_is_valid) {
		hit = isect_pt_wall(isect, cs->grid_cell, 
			cs->cell_outsurf, cs->next_dist, cs->next_pt, "out");
		if (hit) {
			OUTHIT(isect, cs->next_dist, cs->cell_outsurf, 
				cs->grid_cell,
				dsp_pl[BBSURF(cs->cell_outsurf)]);
		}
	}
}


/*
 *	Intersect a ray with the whole DSP
 *
 */
static void
isect_ray_dsp(isect)
struct isect_stuff *isect;
{
	point_t	bbin_pt;	/* DSP Bounding Box entry point */
	double	bbin_dist;
	point_t	bbout_pt;	/* DSP Bounding Box exit point */
	int	bbout_cell[3];	/* grid cell of last point in bbox */
	point_t curr_pt;	/* entry pt into a cell */
	double	curr_dist;	/* dist along ray to curr_pt */
	point_t	next_pt;	/* The out point of the current cell */


	int	curr_cell[3];	/* grid cell of current point */
	int	rising;		/* boolean:  Ray Z dir sign is positive */
	int	stepX, stepY;	/* signed step delta for grid cell marching */
	int	insurfX, outsurfX;
	int	insurfY, outsurfY;

	double	tDX;		/* dist along ray to span 1 cell in X dir */
	double	tDY;		/* dist along ray to span 1 cell in Y dir */

	double	out_dist;

	double	tX, tY;	/* dist along ray from hit pt. to next cell boundary */
	struct cell_stuff cs;

	if (rt_g.debug & DEBUG_HF) {
		bu_log("isect_ray_dsp()\n");
	}


	/* compute BBox entry point and starting grid cell */
	bbin_dist = isect->bbox.in_dist;
	VJOIN1(bbin_pt, isect->r.r_pt, bbin_dist, isect->r.r_dir);

	out_dist = isect->bbox.out_dist;
	VJOIN1(bbout_pt, isect->r.r_pt, out_dist, isect->r.r_dir);

	if (rt_g.debug & DEBUG_HF) {
		bu_log("  r_pt: %g %g %g  dir: %g %g %g\n",
		       V3ARGS(isect->r.r_pt),
		       V3ARGS(isect->r.r_dir));
		bu_log(" in_pt: %g %g %g  dist: %g\n",
		       V3ARGS(bbin_pt), bbin_dist);
		bu_log("out_pt: %g %g %g  dist: %g\n",
		       V3ARGS(bbout_pt), out_dist);
	}



	VMOVE(curr_cell, bbin_pt);	/* int/float conversion */
	if (curr_cell[X] >= XSIZ(isect->dsp)) curr_cell[X]--;
	if (curr_cell[Y] >= YSIZ(isect->dsp)) curr_cell[Y]--;


	VMOVE(bbout_cell, bbout_pt);	/* int/float conversion */
	if (bbout_cell[X] >= XSIZ(isect->dsp)) bbout_cell[X]--;
	if (bbout_cell[Y] >= YSIZ(isect->dsp)) bbout_cell[Y]--;

	if (rt_g.debug & DEBUG_HF) {
		vect_t t;

		MAT4X3PNT(t, isect->dsp->dsp_i.dsp_stom, bbin_pt);
		bu_log(" in cell(%4d,%4d)  pt(%g %g %g) ss_dist:%g\n",
			V2ARGS(curr_cell), V3ARGS(t), bbin_dist);

		MAT4X3PNT(t, isect->dsp->dsp_i.dsp_stom, bbout_pt);
		bu_log("out cell(%4d,%4d)  pt(%g %g %g) ss_dist:%g\n",
			V2ARGS(	bbout_cell), V3ARGS(t), out_dist);
	}

	rising = (isect->r.r_dir[Z] > 0.); /* compute Z direction */



	/* Compute stepping directions and distances for both
	 * X and Y axes
	 */
	tX = tY = bbin_dist;
	if (isect->r.r_dir[X] < 0.0) {
		stepX = -1;	/* cell delta for stepping X dir on ray */
		insurfX = BBSURF(XMAX);
		outsurfX = BBSURF(XMIN);

		/* tDX is the distance along the ray we have to travel
		 * to traverse a cell (travel a unit distance) along the
		 * X axis of the grid
		 */
		tDX = -1.0 / isect->r.r_dir[X];

		/* tX is the distance along the ray to the first cell 
 		 * boundary in the X direction beyond bbin_pt
		 */
		tX += (curr_cell[X] - bbin_pt[X]) / isect->r.r_dir[X];

	} else {
		stepX = 1;
		insurfX = BBSURF(XMIN);
		outsurfX = BBSURF(XMAX);

		tDX = 1.0 / isect->r.r_dir[X];

		if (isect->r.r_dir[X] > 0.0) {
			tX += ((curr_cell[X]+1) - bbin_pt[X])
					/
				 isect->r.r_dir[X];
		} else
			tX = MAX_FASTF;
	}

	if (isect->r.r_dir[Y] < 0) {
		stepY = -1;
		insurfY = BBSURF(YMAX);
		outsurfY = BBSURF(YMIN);

		tDY = -1.0 / isect->r.r_dir[Y];

		tY += (curr_cell[Y] - bbin_pt[Y]) / isect->r.r_dir[Y];

	} else {
		stepY = 1;
		insurfY = BBSURF(YMIN);
		outsurfY = BBSURF(YMAX);

		tDY = 1.0 / isect->r.r_dir[Y];

		if (isect->r.r_dir[Y] > 0.0) {
			tY += ((curr_cell[Y]+1) - bbin_pt[Y])
					/
				isect->r.r_dir[Y];
		} else
			tY = MAX_FASTF;
	}


	if (rt_g.debug & DEBUG_HF) {
		point_t t;

		VJOIN1(t, isect->r.r_pt, tX, isect->r.r_dir);
		bu_log("stepX:%d tDX:%g tX:%g next: %g %g %g\n", 
			stepX, tDX, tX, V3ARGS(t));

		VJOIN1(t, isect->r.r_pt, tY, isect->r.r_dir);
		bu_log("stepY:%d tDY:%g tY:%g next: %g %g %g\n",
			stepY,  tDY, tY, V3ARGS(t));
	}
/*	if (tX > out_dist) tX = out_dist; */
/*	if (tY > out_dist) tY = out_dist; */


	VMOVE(curr_pt, bbin_pt);
	curr_dist = bbin_dist;


	/* precompute some addresses for parameters we're going to
	 * pass frequently to cell_isect();
	 */
	cs.grid_cell = curr_cell;
	cs.curr_dist = curr_dist;
	cs.curr_pt = curr_pt;
	cs.rising = rising;
	cs.next_pt = next_pt;
	cs.cell_insurf = BBSURF(isect->bbox.in_surf);


	while ( (out_dist - cs.curr_dist) > isect->tol->dist) {
		if (rt_g.debug & DEBUG_HF) {
			bu_log("Step to cell %d,%d curr_dist %g out_dist %g tX:%g tY:%g\n",
				cs.grid_cell[X], cs.grid_cell[Y],
				cs.curr_dist, out_dist, tX, tY);
		}


		if ( tX > out_dist && tY > out_dist ) {
			VJOIN1(cs.next_pt, isect->r.r_pt,
				out_dist, isect->r.r_dir);

			cs.cell_outsurf = BBSURF(isect->bbox.out_surf);
			cs.next_dist = out_dist;

			isect_ray_cell(isect, &cs);

			cs.curr_dist = out_dist;



		} else if (tX < tY) {
			VJOIN1(cs.next_pt, isect->r.r_pt, tX, isect->r.r_dir);
			cs.cell_outsurf = outsurfX;
			cs.next_dist = tX;

			isect_ray_cell(isect, &cs);

			cs.curr_dist = cs.next_dist;
			cs.grid_cell[X] += stepX;
			cs.cell_insurf = insurfX;
			tX += tDX;
		} else {
			VJOIN1(cs.next_pt, isect->r.r_pt, tY, isect->r.r_dir);
			cs.cell_outsurf = outsurfY;
			cs.next_dist = tY;

			isect_ray_cell(isect, &cs);

			cs.curr_dist = cs.next_dist;
			cs.grid_cell[Y] += stepY;
			cs.cell_insurf = insurfY;
			tY += tDY;
		}
		VMOVE(cs.curr_pt, cs.next_pt);
	}

	if (isect->sp_is_valid && !isect->sp_is_done) {
		OUTHIT( isect, 
		isect->bbox.out_dist,
		isect->bbox.out_surf,
		bbout_cell,
		dsp_pl[BBSURF(isect->bbox.out_surf)]);


		HIT_COMMIT( isect );
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
	vect_t	dir;	/* temp storage */
	vect_t	v;
	struct isect_stuff isect;
	static CONST int junk[2] = { 0, 0 };


	if (setjmp(isect.env)) {
		if (rt_g.debug & DEBUG_HF)
			bu_bomb("");

		rt_g.debug |= DEBUG_HF; 

	}

	if (rt_g.debug & DEBUG_HF) {
		bu_log("rt_dsp_shot(pt:(%g %g %g)\n\tdir[%g]:(%g %g %g))\n    pixel(%d,%d)\n",
			V3ARGS(rp->r_pt),
			MAGNITUDE(rp->r_dir),
			V3ARGS(rp->r_dir),
			ap->a_x, ap->a_y);
	}
	RT_DSP_CK_MAGIC(dsp);
	BU_CK_VLS(&dsp->dsp_i.dsp_name);
	BU_CK_MAPPED_FILE(dsp->dsp_i.dsp_mp);


	/* 
	 * map ray into the coordinate system of the dsp 
	 */
	MAT4X3PNT(isect.r.r_pt, dsp->dsp_i.dsp_mtos, rp->r_pt);
	MAT4X3VEC(dir, dsp->dsp_i.dsp_mtos, rp->r_dir);
	VMOVE(isect.r.r_dir, dir);
	VUNITIZE(isect.r.r_dir);

	if (rt_g.debug & DEBUG_HF) {
		bn_mat_print("mtos", dsp->dsp_i.dsp_mtos);
		bu_log("Solid space ray pt:(%g %g %g)\n", V3ARGS(isect.r.r_pt));
		bu_log("\tdir[%g]: [%g %g %g]\n\tunit_dir(%g %g %g)\n",
			MAGNITUDE(dir),
			V3ARGS(dir),
			V3ARGS(isect.r.r_dir));
	}

	isect.ap = ap;
	isect.stp = stp;
	isect.dsp = (struct dsp_specific *)stp->st_specific;
	isect.tol = &ap->a_rt_i->rti_tol;
	isect.sp_is_valid = 0;
	isect.sp_is_done = 0;

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

		inhit( &isect, isect.minbox.in_dist,
			BBSURF(isect.minbox.in_surf),
			junk, 
			dsp_pl[isect.minbox.in_surf], __FILE__, __LINE__);

		OUTHIT( (&isect), isect.minbox.out_dist,
			BBSURF(isect.minbox.out_surf), junk,
			dsp_pl[isect.minbox.out_surf]);

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
		if (VDOT(v, rp->r_dir) < 0.0) segp->seg_in.hit_dist *= -1.0;

		VSCALE(dir, isect.r.r_dir, segp->seg_out.hit_dist);
		MAT4X3VEC(v, dsp->dsp_i.dsp_stom, dir)
		segp->seg_out.hit_dist = MAGNITUDE(v);
		if (VDOT(v, rp->r_dir) < 0.0) segp->seg_out.hit_dist *= -1.0;

		if (segp->seg_in.hit_dist > segp->seg_out.hit_dist) {
			bu_log("Pixel %d %d seg inside out %g %g\n",
				ap->a_x, ap->a_y,
				segp->seg_in.hit_dist,
				segp->seg_out.hit_dist);
		}

		MAT4X3VEC(v, dsp->dsp_i.dsp_mtos, segp->seg_in.hit_normal);
		VMOVE(segp->seg_in.hit_normal, v);
		VUNITIZE( segp->seg_in.hit_normal );

		MAT4X3VEC(v, dsp->dsp_i.dsp_mtos, segp->seg_out.hit_normal);
		VMOVE(segp->seg_out.hit_normal, v);
		VUNITIZE( segp->seg_out.hit_normal );

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

/***********************************************************************
 *
 * Compute the model-space normal at a gridpoint
 *
 */
static void
compute_normal_at_gridpoint(N, dsp, x, y, fd, bool)
vect_t N;
struct dsp_specific *dsp;
int x, y;
FILE *fd;
int bool;
{
	/*  Gridpoint specified is "B" we compute normal by taking the
	 *  cross product of the vectors  A->C, D->E
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
	
	point_t A, C, D, E, tmp;
	vect_t Vac, Vde;

	if (x == 0) {	VSET(tmp, x, y, DSP(dsp, x, y) );	}
	else {		VSET(tmp, x-1, y, DSP(dsp, x-1, y) );	}
	MAT4X3PNT(A, dsp->dsp_i.dsp_stom, tmp);

	if (x >= XSIZ(dsp)) {	VSET(tmp, x, y,  DSP(dsp, x, y) ); } 
	else {			VSET(tmp, x+1, y,  DSP(dsp, x+1, y) );	}
	MAT4X3PNT(C, dsp->dsp_i.dsp_stom, tmp);


	if (y == 0) {	VSET(tmp, x, y, DSP(dsp, x, y) ); }
	else {		VSET(tmp, x, y-1, DSP(dsp, x, y-1) );	}
	MAT4X3PNT(D, dsp->dsp_i.dsp_stom, tmp);

	if (y >= YSIZ(dsp)) {	VSET(tmp, x, y, DSP(dsp, x, y) ); }
	else {			VSET(tmp, x, y+1, DSP(dsp, x, y+1) );	}
	MAT4X3PNT(E, dsp->dsp_i.dsp_stom, tmp);

	if (fd && bool) {
		pl_color(fd, 220, 220, 90);
		pdv_3line(fd, A, C);
		pdv_3line(fd, D, E);
	}

	VSUB2(Vac, C, A);
	VSUB2(Vde, E, D);

	VCROSS(N, Vac, Vde);

	VUNITIZE(N);
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
	vect_t N, t, tmp, A, B, C, D, N_orig;
	char buf[32];
	struct dsp_specific *dsp = (struct dsp_specific *)stp->st_specific;
	vect_t Anorm, Bnorm, Dnorm, Cnorm, ABnorm, CDnorm;
	double Xfrac, Yfrac;
	int x, y;
	point_t pt;
	double dot;
	double len;
	FILE *fd = (FILE *)NULL;


	RT_DSP_CK_MAGIC(dsp);
	BU_CK_VLS(&dsp->dsp_i.dsp_name);
	BU_CK_MAPPED_FILE(dsp->dsp_i.dsp_mp);


 	if (rt_g.debug & DEBUG_HF)
		bu_log("rt_dsp_norm(%g %g %g)\n", V3ARGS(hitp->hit_normal));

	VMOVE(N_orig, hitp->hit_normal);

	VJOIN1( hitp->hit_point, rp->r_pt, hitp->hit_dist, rp->r_dir );
 	if (rt_g.debug & DEBUG_HF)
		VPRINT("hit point", hitp->hit_point);

	if (hitp->hit_surfno < 0 || !dsp->dsp_i.dsp_smooth ) {
		/* we've hit one of the sides or bottom, or the user didn't
		 * ask for smoothing of the elevation data,
		 * so there's no interpolation to do
		 */

		if (rt_g.debug & DEBUG_HF)
			bu_log("no Interpolation needed\n",
			       V3ARGS(hitp->hit_normal));
		return;
	}

	if ( hitp->hit_surfno != TRI2 &&  hitp->hit_surfno != TRI1 &&
	     hitp->hit_surfno != TRI3 && hitp->hit_surfno != TRI4 ) {
		bu_log("%s:%d bogus surface of DSP %d\n",
		       __FILE__, __LINE__, hitp->hit_surfno);
		bu_bomb("");
	}

	if (rt_g.debug & DEBUG_HF)
		bu_log("Interpolation: %d\n", dsp->dsp_i.dsp_smooth);

	/* compute the distance between grid points in model space */
	VSET(tmp, 1.0, 0.0, 0.0);
	MAT4X3VEC(t, dsp->dsp_i.dsp_stom, tmp);
	len = MAGNITUDE(t);

	if (rt_g.debug & DEBUG_HF) {
		bu_semaphore_acquire( BU_SEM_SYSCALL);
		sprintf(buf, "dsp%02d.pl", plot_file_num++);
		bu_semaphore_release( BU_SEM_SYSCALL);

		bu_log("plotting normals in %s\n", buf);
		bu_semaphore_acquire( BU_SEM_SYSCALL);
		if ((fd=fopen(buf, "w")) == (FILE *)NULL) {
			bu_semaphore_release( BU_SEM_SYSCALL);
			bu_bomb("Couldn't open plot file\n");
		}
	}

	/* get the cell we hit */
	x = hitp->hit_vpriv[X];
	y = hitp->hit_vpriv[Y];

	compute_normal_at_gridpoint(Anorm, dsp, x, y, fd, 1);
	compute_normal_at_gridpoint(Bnorm, dsp, x+1, y, fd, 0);
	compute_normal_at_gridpoint(Dnorm, dsp, x+1, y+1, fd, 0);
	compute_normal_at_gridpoint(Cnorm, dsp, x, y+1, fd, 0);

	if (rt_g.debug & DEBUG_HF) {

		/* plot the ray */
		pl_color(fd, 255, 0, 0);
		pdv_3line(fd, rp->r_pt, hitp->hit_point);

		/* plot the normal we started with */
		pl_color(fd, 0, 255, 0);
		VJOIN1(tmp, hitp->hit_point, len, N_orig);
		pdv_3line(fd, hitp->hit_point, tmp);


		/* Plot the normals we just got */
		pl_color(fd, 220, 220, 90);

		VSET(tmp, x,   y,   DSP(dsp, x,   y));
		MAT4X3PNT(A, dsp->dsp_i.dsp_stom, tmp);
		VJOIN1(tmp, A, len, Anorm);
		pdv_3line(fd, A, tmp);

		VSET(tmp, x+1, y,   DSP(dsp, x+1, y));
		MAT4X3PNT(B, dsp->dsp_i.dsp_stom, tmp);
		VJOIN1(tmp, B, len, Anorm);
		pdv_3line(fd, B, tmp);

		VSET(tmp, x+1, y+1, DSP(dsp, x+1, y+1));
		MAT4X3PNT(D, dsp->dsp_i.dsp_stom, tmp);
		VJOIN1(tmp, D, len, Anorm);
		pdv_3line(fd, D, tmp);

		VSET(tmp, x,   y+1, DSP(dsp, x,   y+1));
		MAT4X3PNT(C, dsp->dsp_i.dsp_stom, tmp);
		VJOIN1(tmp, C, len, Anorm);
		pdv_3line(fd, C, tmp);

		bu_semaphore_release( BU_SEM_SYSCALL);
	}

	MAT4X3PNT(pt, dsp->dsp_i.dsp_mtos, hitp->hit_point);

	Xfrac = (pt[X] - x);
	Yfrac = (pt[Y] - y);
	if (rt_g.debug & DEBUG_HF)
		bu_log("Xfract:%g Yfract:%g\n", Xfrac, Yfrac);

	if (Xfrac < 0.0) Xfrac = 0.0;
	else if (Xfrac > 1.0) Xfrac = 1.0;

	if (Yfrac < 0.0) Yfrac = 0.0;
	else if (Yfrac > 1.0) Yfrac = 1.0;


     	if (dsp->dsp_i.dsp_smooth == 2) {
		/* This is an experiment to "flatten" the curvature 
		 * of the dsp near the grid points
		 */
#define SMOOTHSTEP(x)  ((x)*(x)*(3 - 2*(x)))
		Xfrac = SMOOTHSTEP( Xfrac );
		Yfrac = SMOOTHSTEP( Yfrac );
#undef SMOOTHSTEP
     	}

	/* we compute the normal along the "X edges" of the cell */
	VSCALE(Anorm, Anorm, (1.0-Xfrac) );
	VSCALE(Bnorm, Bnorm,      Xfrac  );
	VADD2(ABnorm, Anorm, Bnorm);
	VUNITIZE(ABnorm);

	VSCALE(Cnorm, Cnorm, (1.0-Xfrac) );
	VSCALE(Dnorm, Dnorm,      Xfrac  );
	VADD2(CDnorm, Dnorm, Cnorm);
	VUNITIZE(CDnorm);

	/* now we interpolate the two X edge normals to get the final one */
	VSCALE(ABnorm, ABnorm, (1.0-Yfrac) );
	VSCALE(CDnorm, CDnorm, Yfrac );
	VADD2(N, ABnorm, CDnorm);

	VUNITIZE(N);

	dot = VDOT(N, rp->r_dir);
	if (rt_g.debug & DEBUG_HF)
		bu_log("interpolated %g %g %g  dot:%g\n", 
		       V3ARGS(N), dot);

	if ( (hitp->hit_vpriv[Z] == 0.0 && dot > 0.0)/* in-hit needs fix */ ||
	     (hitp->hit_vpriv[Z] == 1.0 && dot < 0.0)/* out-hit needs fix */){
		/* bring the normal back to being perpindicular 
		 * to the ray to avoid "flipped normal" warnings
		 */
		VCROSS(A, rp->r_dir, N);
		VCROSS(N, A, rp->r_dir);
		VUNITIZE(N);

		dot = VDOT(N, rp->r_dir);


		if (rt_g.debug & DEBUG_HF)
			bu_log("corrected: %g %g %g dot:%g\n", V3ARGS(N), dot);
	}
	VMOVE(hitp->hit_normal, N);

	if (rt_g.debug & DEBUG_HF) {


		if (fd) {

			bu_semaphore_acquire( BU_SEM_SYSCALL);
			pl_color(fd, 255, 255, 255);
			VJOIN1(tmp, hitp->hit_point, len, hitp->hit_normal);
			pdv_3line(fd, hitp->hit_point, tmp);

			fclose(fd);
			bu_semaphore_release( BU_SEM_SYSCALL);
		}
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

	if (rt_g.debug & DEBUG_HF)
		bu_log("rt_dsp_curve()\n");

 	cvp->crv_c1 = cvp->crv_c2 = 0;

	/* any tangent direction */
 	bn_vec_ortho( cvp->crv_pdir, hitp->hit_normal );
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
	vect_t tmp;
	double r;
	fastf_t min_r_U, min_r_V;
	vect_t norm;
	vect_t rev_dir;
	fastf_t dot_N;
	vect_t UV_dir;
	vect_t U_dir, V_dir;
	fastf_t U_len, V_len;
	double one_over_len;

	MAT4X3PNT(pt, dsp->dsp_i.dsp_mtos, hitp->hit_point);

	/* compute U, V */
	uvp->uv_u = pt[X] / (double)XSIZ(dsp);
	CLAMP(uvp->uv_u, 0.0, 1.0);

	uvp->uv_v = pt[Y] / (double)YSIZ(dsp);
	CLAMP(uvp->uv_v, 0.0, 1.0);


	/* du, dv indicate the extent of the ray radius in UV coordinates.
	 * To compute this, transform unit vectors from solid space to model
	 * space.  We remember the length of the resultant vectors and then
	 * unitize them to get u,v directions in model coordinate space.
	 * 

	 */
	VSET( tmp, XSIZ(dsp), 0.0, 0.0 )
	MAT4X3VEC( U_dir,  dsp->dsp_i.dsp_stom, tmp )
	U_len = MAGNITUDE( U_dir );
	one_over_len = 1.0/U_len;
	VSCALE( U_dir, U_dir, one_over_len )

	VSET( tmp, 0.0, YSIZ(dsp), 0.0 )
	MAT4X3VEC( V_dir,  dsp->dsp_i.dsp_stom, tmp )
	V_len = MAGNITUDE( V_dir );
	one_over_len = 1.0/V_len;
	VSCALE( V_dir, V_dir, one_over_len )

	/* divide the hit-point radius by the U/V unit length distance */
	r = ap->a_rbeam + ap->a_diverge * hitp->hit_dist;
	min_r_U = r / U_len;
	min_r_V = r / V_len;

	/* compute UV_dir, a vector in the plane of the hit point (surface)
	 * which points in the anti-rayward direction
	 */
	VREVERSE( rev_dir, ap->a_ray.r_dir )
	VMOVE( norm, hitp->hit_normal )
	dot_N = VDOT( rev_dir, norm );
	VJOIN1( UV_dir, rev_dir, -dot_N, norm )
	VUNITIZE( UV_dir )

	if (NEAR_ZERO( dot_N, SMALL_FASTF ) ) {
		/* ray almost perfectly 90 degrees to surface */
		uvp->uv_du = 0.5;
		uvp->uv_dv = 0.5;
	} else {
		/* somehow this computes the extent of U and V in the radius */
		uvp->uv_du = (r / U_len) * VDOT( UV_dir, U_dir ) / dot_N;
		uvp->uv_dv = (r / V_len) * VDOT( UV_dir, V_dir ) / dot_N;
	}

	if (uvp->uv_du < 0.0 )
		uvp->uv_du = -uvp->uv_du;
	if (uvp->uv_du < min_r_U )
		uvp->uv_du = min_r_U;

	if (uvp->uv_dv < 0.0 )
		uvp->uv_dv = -uvp->uv_dv;
	if (uvp->uv_dv < min_r_V )
		uvp->uv_dv = min_r_V;

	if (rt_g.debug & DEBUG_HF)
		bu_log("rt_dsp_uv(pt:%g,%g siz:%d,%d)\n U_len=%g V_len=%g\n r=%g rbeam=%g diverge=%g dist=%g\n u=%g v=%g du=%g dv=%g\n",
			pt[X], pt[Y], XSIZ(dsp), YSIZ(dsp), 
			U_len, V_len,
			r, ap->a_rbeam, ap->a_diverge, hitp->hit_dist,
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

	BU_CK_MAPPED_FILE(dsp->dsp_i.dsp_mp);
	bu_close_mapped_file(dsp->dsp_i.dsp_mp);

	bu_free( (char *)dsp, "dsp_specific" );
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
struct bu_list		*vhead;
struct rt_db_internal	*ip;
CONST struct rt_tess_tol *ttol;
CONST struct bn_tol	*tol;
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
	RT_ADD_VLIST( vhead, m_pt, BN_VLIST_LINE_MOVE )

#define DRAW() \
	MAT4X3PNT(m_pt, dsp_ip->dsp_stom, s_pt); \
	RT_ADD_VLIST( vhead, m_pt, BN_VLIST_LINE_DRAW )


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
	if (ttol->rel )  {
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
	if (step < 1 )  step = 1;


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
CONST struct bn_tol	*tol;
{
	LOCAL struct rt_dsp_internal	*dsp_ip;

	if (rt_g.debug & DEBUG_HF)
		bu_log("rt_dsp_tess()\n");

	RT_CK_DB_INTERNAL(ip);
	dsp_ip = (struct rt_dsp_internal *)ip->idb_ptr;
	RT_DSP_CK_MAGIC(dsp_ip);

	return(-1);
}


/* 	G E T _ F I L E _ D A T A
 *
 *	Retrieve data for DSP from external file.
 *	Returns:
 *		0 Success
 *		!0 Failuer
 */
static int
get_file_data(struct rt_dsp_internal	*dsp_ip,
	     struct rt_db_internal	*ip,
	     const struct bu_external	*ep,
	     register const mat_t	mat,
	     const struct db_i		*dbip)
{
	struct bu_mapped_file		*mf;
	int				count, in_cookie, out_cookie;


	/* get file */
	mf = dsp_ip->dsp_mp = 
		bu_open_mapped_file_with_path(dbip->dbi_filepath,
			bu_vls_addr(&dsp_ip->dsp_name), "dsp");
	if (!mf) {
		bu_log("mapped file open failed\n");
		return -1;
	}

	if (dsp_ip->dsp_mp->buflen != dsp_ip->dsp_xcnt*dsp_ip->dsp_ycnt*2) {
		bu_log("DSP buffer wrong size");
		return -1;
	}

	in_cookie = bu_cv_cookie("nus"); /* data is network unsigned short */
	out_cookie = bu_cv_cookie("hus");

	if ( bu_cv_optimize(in_cookie) != bu_cv_optimize(out_cookie) ) {
		int got;
		/* if we're on a little-endian machine we convert the
		 * input file from network to host format
		 */
		count = dsp_ip->dsp_xcnt * dsp_ip->dsp_ycnt;
		mf->apbuflen = count * sizeof(unsigned short);
		mf->apbuf = bu_malloc(mf->apbuflen, "apbuf");

		got = bu_cv_w_cookie(mf->apbuf, out_cookie, mf->apbuflen,
				     mf->buf,    in_cookie, count);
		if (got != count) {
			bu_log("got %d != count %d", got, count);
			bu_bomb("\n");
		}
		dsp_ip->dsp_buf = dsp_ip->dsp_mp->apbuf;
	} else {
		dsp_ip->dsp_buf = dsp_ip->dsp_mp->buf;
	}
	return 0;
}

/*	G E T _ O B J _ D A T A
 *
 *	Retrieve data for DSP from a database object.
 */
static int
get_obj_data(struct rt_dsp_internal	*dsp_ip,
	     struct rt_db_internal	*ip,
	     const struct bu_external	*ep,
	     register const mat_t	mat,
	     const struct db_i		*dbip)
{
	struct rt_binunif_internal	*bip;

	BU_GETSTRUCT(dsp_ip->dsp_bip, rt_db_internal);

	if (rt_retrieve_binunif(dsp_ip->dsp_bip, dbip,
				bu_vls_addr( &dsp_ip->dsp_name) ))
		return -1;

	if (rt_g.debug & DEBUG_HF)
		bu_log("db_internal magic: 0x%08x  major: %d minor:%d\n",
		       dsp_ip->dsp_bip->idb_magic,
		       dsp_ip->dsp_bip->idb_major_type,
		       dsp_ip->dsp_bip->idb_minor_type);

	bip = dsp_ip->dsp_bip->idb_ptr;

	if (rt_g.debug & DEBUG_HF)
		bu_log("binunif magic: 0x%08x  type: %d count:%d data[0]:%u\n",
		       bip->magic, bip->type, bip->count, bip->u.uint16[0]);

	dsp_ip->dsp_buf = bip->u.uint16;
	return 0;
}

/*
 *	D S P _ G E T _ D A T A
 *
 *  Handle things common to both the v4 and v5 database.
 *
 *  This include applying the modelling transform, and fetching the
 *  actual data.
 */
static int
dsp_get_data(struct rt_dsp_internal	*dsp_ip,
	     struct rt_db_internal	*ip,
	     const struct bu_external	*ep,
	     register const mat_t	mat,
	     const struct db_i		*dbip)
{
	mat_t				tmp;

	/* Apply Modeling transform */
	bn_mat_copy(tmp, dsp_ip->dsp_stom);
	bn_mat_mul(dsp_ip->dsp_stom, mat, tmp);
	
	bn_mat_inv(dsp_ip->dsp_mtos, dsp_ip->dsp_stom);

	switch (dsp_ip->dsp_datasrc) {
	case RT_DSP_SRC_FILE:
		if (rt_g.debug & DEBUG_HF)
			bu_log("getting data from file\n");
		return get_file_data(dsp_ip, ip, ep, mat, dbip);
		break;
	case RT_DSP_SRC_OBJ:
		if (rt_g.debug & DEBUG_HF)
			bu_log("getting data from object\n");
		return get_obj_data(dsp_ip, ip, ep, mat, dbip);
		break;
	default:
		bu_log("%s:%d Odd dsp data src '%c' s/b '%c' or '%c'\n", 
		       __FILE__, __LINE__, dsp_ip->dsp_datasrc,
		       RT_DSP_SRC_FILE, RT_DSP_SRC_OBJ);
		return -1;
	}
}

/*
 *			R T _ D S P _ I M P O R T
 *
 *  Import an DSP from the database format to the internal format.
 *  Apply modeling transformations as well.
 */
int
rt_dsp_import( ip, ep, mat, dbip )
struct rt_db_internal		*ip;
CONST struct bu_external	*ep;
register CONST mat_t		mat;
CONST struct db_i		*dbip;
{
	LOCAL struct rt_dsp_internal	*dsp_ip;
	union record			*rp;
	struct bu_vls			str;





#define IMPORT_FAIL(_s) \
	bu_log("rt_dsp_import(%d) '%s' %s\n", __LINE__, \
               bu_vls_addr(&dsp_ip->dsp_name), _s);\
	bu_free( (char *)dsp_ip , "rt_dsp_import: dsp_ip" ); \
	ip->idb_type = ID_NULL; \
	ip->idb_ptr = (genptr_t)NULL; \
	return -2

	BU_CK_EXTERNAL( ep );
	rp = (union record *)ep->ext_buf;

	if (rt_g.debug & DEBUG_HF)
		bu_log("rt_dsp_import(%s)\n", rp->ss.ss_args);
/*----------------------------------------------------------------------*/



	/* Check record type */
	if (rp->u_id != DBID_STRSOL )  {
		bu_log("rt_dsp_import: defective record\n");
		return(-1);
	}

	RT_CK_DB_INTERNAL( ip );
	ip->idb_type = ID_DSP;
	ip->idb_meth = &rt_functab[ID_DSP];
	ip->idb_ptr = bu_malloc( sizeof(struct rt_dsp_internal), "rt_dsp_internal");
	dsp_ip = (struct rt_dsp_internal *)ip->idb_ptr;
	dsp_ip->magic = RT_DSP_INTERNAL_MAGIC;

	/* set defaults */
	/* XXX bu_struct_parse does not set the null?
	 * memset(&dsp_ip->dsp_name[0], 0, DSP_NAME_LEN); 
	 */
	dsp_ip->dsp_xcnt = dsp_ip->dsp_ycnt = 0;

	dsp_ip->dsp_smooth = 1;
	MAT_IDN(dsp_ip->dsp_stom);
	MAT_IDN(dsp_ip->dsp_mtos);

	bu_vls_init( &str );
	bu_vls_strcpy( &str, rp->ss.ss_args );
	if (bu_struct_parse( &str, rt_dsp_parse, (char *)dsp_ip ) < 0) {
		if (BU_VLS_IS_INITIALIZED( &str )) bu_vls_free( &str );

		IMPORT_FAIL("parse error");
	}


	/* Validate parameters */
	if (dsp_ip->dsp_xcnt == 0 || dsp_ip->dsp_ycnt == 0) {
		IMPORT_FAIL("zero dimension on map");
	}
	
	if (dsp_get_data(dsp_ip, ip, ep, mat, dbip)) {
		IMPORT_FAIL("DSP data");
	}

	if (rt_g.debug & DEBUG_HF) {
		bu_vls_trunc(&str, 0);
		bu_vls_struct_print( &str, rt_dsp_ptab, (char *)dsp_ip);
		bu_log("  imported as(%s)\n", bu_vls_addr(&str));

	}

	if (BU_VLS_IS_INITIALIZED( &str )) bu_vls_free( &str );
	return(0);			/* OK */
}


/*
 *			R T _ D S P _ E X P O R T
 *
 *  The name is added by the caller, in the usual place.
 */
int
rt_dsp_export( ep, ip, local2mm, dbip )
struct bu_external		*ep;
CONST struct rt_db_internal	*ip;
double				local2mm;
CONST struct db_i		*dbip;
{
	struct rt_dsp_internal	*dsp_ip;
	struct rt_dsp_internal	dsp;
	union record		*rec;
	struct bu_vls		str;


	RT_CK_DB_INTERNAL(ip);
	if (ip->idb_type != ID_DSP )  return(-1);
	dsp_ip = (struct rt_dsp_internal *)ip->idb_ptr;
	RT_DSP_CK_MAGIC(dsp_ip);
	BU_CK_VLS(&dsp_ip->dsp_name);


	BU_CK_EXTERNAL(ep);
	ep->ext_nbytes = sizeof(union record)*DB_SS_NGRAN;
	ep->ext_buf = bu_calloc( 1, ep->ext_nbytes, "dsp external");
	rec = (union record *)ep->ext_buf;

	dsp = *dsp_ip;	/* struct copy */

	/* Since libwdb users may want to operate in units other
	 * than mm, we offer the opportunity to scale the solid
	 * (to get it into mm) on the way out.
	 */
	dsp.dsp_stom[15] *= local2mm;

	bu_vls_init( &str );
	bu_vls_struct_print( &str, rt_dsp_ptab, (char *)&dsp);
	if (rt_g.debug & DEBUG_HF)	
		bu_log("rt_dsp_export(%s)\n", bu_vls_addr(&str) );

	rec->ss.ss_id = DBID_STRSOL;
	strncpy( rec->ss.ss_keyword, "dsp", NAMESIZE-1 );
	strncpy( rec->ss.ss_args, bu_vls_addr(&str), DB_SS_LEN-1 );


	if (BU_VLS_IS_INITIALIZED( &str )) bu_vls_free( &str );

	return(0);
}




/*
 *			R T _ D S P _ I M P O R T 5
 *
 *  Import an DSP from the database format to the internal format.
 *  Apply modeling transformations as well.
 */
int
rt_dsp_import5( ip, ep, mat, dbip )
struct rt_db_internal		*ip;
CONST struct bu_external	*ep;
register CONST mat_t		mat;
CONST struct db_i		*dbip;
{
	struct rt_dsp_internal	*dsp_ip;
	unsigned char		*cp;



	BU_CK_EXTERNAL( ep );

	BU_ASSERT_LONG( ep->ext_nbytes, >, 141 );

	RT_CK_DB_INTERNAL( ip );

	ip->idb_type = ID_DSP;
	ip->idb_meth = &rt_functab[ID_DSP];
	dsp_ip = ip->idb_ptr = 
		bu_malloc( sizeof(struct rt_dsp_internal), "rt_dsp_internal");
	memset(dsp_ip, 0, sizeof(*dsp_ip));

	dsp_ip->magic = RT_DSP_INTERNAL_MAGIC;

	/* get x, y counts */
	cp = (unsigned char *)ep->ext_buf;

	dsp_ip->dsp_xcnt = (unsigned) bu_glong( cp );
	cp += SIZEOF_NETWORK_LONG;

	dsp_ip->dsp_ycnt = (unsigned) bu_glong( cp );
	cp += SIZEOF_NETWORK_LONG;

	/* convert matrix */
	ntohd((unsigned char *)dsp_ip->dsp_stom, cp, 16);
	cp += SIZEOF_NETWORK_DOUBLE * 16;
	bn_mat_inv(dsp_ip->dsp_mtos, dsp_ip->dsp_stom);

	/* convert smooth flag */
	dsp_ip->dsp_smooth = bu_gshort( cp );
	cp += SIZEOF_NETWORK_SHORT;

	dsp_ip->dsp_datasrc = *cp;
	cp++;

	/* convert name of data location */
	bu_vls_init( &dsp_ip->dsp_name );
	bu_vls_strcpy( &dsp_ip->dsp_name, (char *)cp );

	if (dsp_get_data(dsp_ip, ip, ep, mat, dbip)) {
		IMPORT_FAIL("unable to read DSP data");
	}

	return 0; /* OK */
}

/*
 *			R T _ D S P _ E X P O R T 5
 *
 *  The name is added by the caller, in the usual place.
 */
int
rt_dsp_export5( ep, ip, local2mm, dbip )
struct bu_external		*ep;
CONST struct rt_db_internal	*ip;
double				local2mm;
CONST struct db_i		*dbip;
{
	struct rt_dsp_internal	*dsp_ip;
	unsigned long		name_len;
	unsigned char		*cp;

	RT_CK_DB_INTERNAL(ip);
	if (ip->idb_type != ID_DSP )  return(-1);
	dsp_ip = (struct rt_dsp_internal *)ip->idb_ptr;
	RT_DSP_CK_MAGIC(dsp_ip);

#if 0
	bu_log("export5");
	dsp_dump(dsp_ip); 
#endif

	name_len = bu_vls_strlen(&dsp_ip->dsp_name) + 1;

	BU_CK_EXTERNAL(ep);
	ep->ext_nbytes = 139 + name_len;
	ep->ext_buf = bu_malloc( ep->ext_nbytes, "dsp external");
	cp = (unsigned char *)ep->ext_buf;

	memset(ep->ext_buf, 0, ep->ext_nbytes);


	/* Now we fill the buffer with the data, making sure everything is
	 * converted to Big-Endian IEEE
	 */

	bu_plong( cp, (unsigned long)dsp_ip->dsp_xcnt );
	cp += SIZEOF_NETWORK_LONG;

	bu_plong( cp, (unsigned long)dsp_ip->dsp_ycnt );
	cp += SIZEOF_NETWORK_LONG;

	/* Since libwdb users may want to operate in units other
	 * than mm, we offer the opportunity to scale the solid
	 * (to get it into mm) on the way out.
	 */
	dsp_ip->dsp_stom[15] *= local2mm;

	htond(cp, (unsigned char *)dsp_ip->dsp_stom, 16);
	cp += SIZEOF_NETWORK_DOUBLE * 16;

	bu_pshort( cp, (int)dsp_ip->dsp_smooth );
	cp += SIZEOF_NETWORK_SHORT;

	*cp = dsp_ip->dsp_datasrc;
	cp++;

	strncpy((char *)cp, bu_vls_addr(&dsp_ip->dsp_name), name_len);

	return 0; /* OK */
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
struct bu_vls		*str;
CONST struct rt_db_internal *ip;
int			verbose;
double			mm2local;
{
	register struct rt_dsp_internal	*dsp_ip =
		(struct rt_dsp_internal *)ip->idb_ptr;
	struct bu_vls vls;


	if (rt_g.debug & DEBUG_HF)
		bu_log("rt_dsp_describe()\n");

	RT_DSP_CK_MAGIC(dsp_ip);

	dsp_print_v5(&vls, dsp_ip);
	bu_vls_vlscat( str, &vls );

	if (BU_VLS_IS_INITIALIZED( &vls )) bu_vls_free( &vls );

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
		BU_CK_MAPPED_FILE(dsp_ip->dsp_mp);
		bu_close_mapped_file(dsp_ip->dsp_mp);
	}

	if (dsp_ip->dsp_bip) {
		rt_binunif_ifree( (struct rt_db_internal *) dsp_ip->dsp_bip);
	}

	dsp_ip->magic = 0;			/* sanity */
	dsp_ip->dsp_mp = (struct bu_mapped_file *)0;

	if (BU_VLS_IS_INITIALIZED(&dsp_ip->dsp_name)) 
		bu_vls_free(  &dsp_ip->dsp_name );
	else
		bu_log("Freeing Bogus DSP, VLS string not initialized\n");


	bu_free( (char *)dsp_ip, "dsp ifree" );
	ip->idb_ptr = GENPTR_NULL;	/* sanity */
}

/* Important when concatenating source files together */
#undef dlog
#undef XMIN
#undef XMAX
#undef YMIN
#undef YMAX
#undef ZMIN
#undef ZMAX
#undef ZMID
#undef DSP
#undef XCNT
#undef YCNT
#undef XSIZ
#undef YSIZ
