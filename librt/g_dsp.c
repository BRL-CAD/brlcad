/*
 *			G _ D S P . C
 *
 *  Purpose -
 *	Intersect a ray with a displacement map
 *
 *  The bounding box planes (in dsp coordinates) are numbered 0 .. 5
 *
 *  For purposes of the "struct hit" surface number, the "non-elevation" 
 *  surfaces are numbered 0 .. 7 where:
 *
 *	     Plane #	Name  plane dist
 *	--------------------------------------------------
 *		0	XMIN (dist = 0)
 *		1	XMAX (dist = xsiz)
 *		2	YMIN (dist = 0)
 *		3	YMAX (dist = ysiz)
 *		4	ZMIN (dist = 0)
 *		5	ZMAX (dsp_max)
 *
 *		6	ZMID (dsp_min)
 *		7	ZTOP (computed)
 *
 *  if the "struct hit" surfno surface is ZMAX, then 
 *  	hit_vpriv[X,Y] holds the cell that was hit.
 * 	hit_vpriv[Z] is 0 if this was an in-hit.  1 if an out-hit
 *
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

int old_way = 0;

#define DIM_BB_CHILDREN 4
#define NUM_BB_CHILDREN (DIM_BB_CHILDREN*DIM_BB_CHILDREN)

struct dsp_rpp {
	unsigned short dsp_min[3];
	unsigned short dsp_max[3];
};

/* This structure contains a bounding box for a portion of the DSP
 * along with information about sub-bounding boxes, and what layer
 * (resolution) of the DSP this box bounds
 */
struct dsp_bb {
	struct dsp_rpp	dspb_rpp;	/* our bounding box */
	/*
	 * the next two elements indicate the number and locations of
	 * sub-bounding rpps.
	 *
	 * dsp_b_ch_dim is typically DIM_BB_CHILDREN,DIM_BB_CHILDREN
	 * except for "border" areas of the array
	 */
	int			dspb_ch_dim[2];	/* dimensions of children[] */
	struct dsp_bb		*dspb_children[NUM_BB_CHILDREN];
};

/* 
 * This structure provides a handle to all of the bounding boxes for the DSP
 * at a particular resolution.
 */
#define LAYER(l, x,y) l->p[l->dim[1]*y+x]
struct dsp_bb_layer {
    /*	int	cell_size;	XXX Do we actually use this? */
	int	dim[2];		/* the dimensions of the array at element p */
	struct dsp_bb *p;	/* array of dsp_bb's for this level */
};



extern int rt_retrieve_binunif(struct rt_db_internal *intern,
			       const struct db_i	*dbip,
			       const char *name);

extern void rt_binunif_ifree( struct rt_db_internal	*ip );


#define dlog if (RT_G_DEBUG & DEBUG_HF) bu_log	


#define BBOX_PLANES	7	/* 2 tops & 5 other sides */
#define XMIN 0
#define XMAX 1
#define YMIN 2
#define YMAX 3
#define ZMIN 4
#define ZMAX 5
#define ZMID 6
#define ZTOP 7

/* access to the array */
#define DSP(_p,_x,_y) ( \
	((unsigned short *)(((struct rt_dsp_internal *)_p)->dsp_buf))[ \
		(_y) * ((struct rt_dsp_internal *)_p)->dsp_xcnt + (_x) ] )

#define XCNT(_p) (((struct rt_dsp_internal *)_p)->dsp_xcnt)
#define YCNT(_p) (((struct rt_dsp_internal *)_p)->dsp_ycnt)

#define XSIZ(_p) (_p->dsp_i.dsp_xcnt - 1)

#define YSIZ(_p) (_p->dsp_i.dsp_ycnt - 1)


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
	int		layers;
	struct dsp_bb_layer *layer;
	struct dsp_bb *bb_array;
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
    struct bu_list	seglist;	/* list of segments */
    struct xray		r;		/* solid space ray */
    vect_t 		inv_dir;	/* inverses of ray direction */
    struct application	*ap;
    struct soltab	*stp;
    struct bn_tol	*tol;

    struct bbox_isect	bbox;
    struct bbox_isect	minbox;

    struct seg		*sp;		/* the current segment being filled */
    int			sp_is_valid;	/* boolean: sp allocated & (inhit) set */
    int			sp_is_done;	/* boolean: sp has (outhit) content */
    jmp_buf		env;		/* for setjmp()/longjmp() */

    int			dmin, dmax;	/* for dsp_in_rpp , {X,Y,Z}MIN/MAX */
};




/* plane equations (minus offset distances) for bounding RPP */
static const vect_t	dsp_pl[BBOX_PLANES] = {
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
	dsp_ip->dsp_bip = (struct rt_db_internal *)NULL;
}

#define DSP_O(m) offsetof(struct rt_dsp_internal, m)
#define DSP_AO(a) bu_offsetofarray(struct rt_dsp_internal, a)

/*
 *	These are only used when editing a v4 database
 */
const struct bu_structparse rt_dsp_parse[] = {
	{"%S",	1, "file", DSP_O(dsp_name), hook_file },
	{"%d",  1, "sm", DSP_O(dsp_smooth), BU_STRUCTPARSE_FUNC_NULL },
	{"%d",	1, "w", DSP_O(dsp_xcnt), BU_STRUCTPARSE_FUNC_NULL },
	{"%d",	1, "n", DSP_O(dsp_ycnt), BU_STRUCTPARSE_FUNC_NULL },
	{"%f", 16, "stom", DSP_AO(dsp_stom), hook_mtos_from_stom },
	{"",	0, (char *)0, 0,	BU_STRUCTPARSE_FUNC_NULL }
};

const struct bu_structparse rt_dsp_ptab[] = {
	{"%S",	1, "file", DSP_O(dsp_name), BU_STRUCTPARSE_FUNC_NULL },
	{"%d",  1, "sm", DSP_O(dsp_smooth), BU_STRUCTPARSE_FUNC_NULL },
	{"%d",	1, "w", DSP_O(dsp_xcnt), BU_STRUCTPARSE_FUNC_NULL },
	{"%d",	1, "n", DSP_O(dsp_ycnt), BU_STRUCTPARSE_FUNC_NULL },
	{"%f", 16, "stom", DSP_AO(dsp_stom), BU_STRUCTPARSE_FUNC_NULL },
	{"",	0, (char *)0, 0,	BU_STRUCTPARSE_FUNC_NULL }
};

static int plot_file_num=0;
#if 0
static int plot_em=1;
#endif

/*	P L O T _ R P P
 *
 * Plot an RPP to a file in the given color
 */
static void
plot_rpp(FILE *fp, struct bound_rpp *rpp, int r, int g, int b)
{
	pl_color(fp, r, g, b);

	pd_3move(fp, rpp->min[X], rpp->min[Y], rpp->min[Z]);
	pd_3cont(fp, rpp->max[X], rpp->min[Y], rpp->min[Z]);
	pd_3cont(fp, rpp->max[X], rpp->max[Y], rpp->min[Z]);
	pd_3cont(fp, rpp->min[X], rpp->max[Y], rpp->min[Z]);
	pd_3cont(fp, rpp->min[X], rpp->min[Y], rpp->min[Z]);

	pd_3cont(fp, rpp->min[X], rpp->min[Y], rpp->max[Z]);
	pd_3cont(fp, rpp->max[X], rpp->min[Y], rpp->max[Z]);
	pd_3cont(fp, rpp->max[X], rpp->max[Y], rpp->max[Z]);
	pd_3cont(fp, rpp->min[X], rpp->max[Y], rpp->max[Z]);
	pd_3cont(fp, rpp->min[X], rpp->min[Y], rpp->max[Z]);
}
/*	P L O T _ D S P _ B B
 *
 *  Plot a dsp_bb structure
 */
static void
plot_dsp_bb(FILE *fp, struct dsp_bb *dsp_bb,
	    struct dsp_specific *dsp,
	    int r, int g, int b)
{
	fastf_t *stom = &dsp->dsp_i.dsp_stom[0];
	struct bound_rpp rpp;
	point_t pt;

	VMOVE(pt, dsp_bb->dspb_rpp.dsp_min); /* int->float conversion */
	MAT4X3PNT(rpp.min, stom, pt);

	VMOVE(pt, dsp_bb->dspb_rpp.dsp_max); /* int->float conversion */
	MAT4X3PNT(rpp.max, stom, pt);

	bu_log("dsp_bb (%g %g %g)  (%g %g %g)\n",
	     V3ARGS(rpp.min), V3ARGS(rpp.max) );

	plot_rpp(fp, &rpp, r, g, b);
}

#ifdef PLOT_LAYERS
/*	P L O T _ L A Y E R S
 *
 *
 *  Plot the bounding box layers for a dsp
 */
static void
plot_layers(struct dsp_specific *dsp_sp)
{
	FILE *fp;
	int l, x, y, n, tot;
	char buf[32];
	static int colors[7][3] = {
		{255, 0, 0},
		{0, 255, 0},
		{0, 0, 255},
		{255, 255, 0},
		{255, 0, 255},
		{0, 255, 255},
		{255, 255, 255}
	};
	int r, g, b, c;
	struct dsp_bb *d_bb;

	for (l=0 ; l < dsp_sp->layers ; l++) {
		sprintf(buf, "Dsp_layer%d.pl", l);
		fp=fopen(buf, "w");
		if ( fp == (FILE *)NULL ) {
			bu_log("%s:%d\n", __FILE__, __LINE__);
			return;
		}
		
		c = l % 6;
		r = colors[c][0];
		g = colors[c][1];
		b = colors[c][2];

#if 0
		if (rt_g.debug & DEBUG_HF)
			bu_log("plot layer %d dim:%d,%d\n", l, 
			       dsp_sp->layer[l].dim[X],
			       dsp_sp->layer[l].dim[Y]);
#endif
		tot = dsp_sp->layer[l].dim[Y] *  dsp_sp->layer[l].dim[X];
		for (y=1 ; y < dsp_sp->layer[l].dim[Y] ; y+= 2 ) {
			for (x=1 ; x < dsp_sp->layer[l].dim[X] ; x+= 2 ) {
				n = y * dsp_sp->layer[l].dim[X] + x;
				d_bb = &dsp_sp->layer[l].p[n];
				plot_dsp_bb(fp, d_bb, dsp_sp,
					    r, g, b);

#if 0
				if (rt_g.debug & DEBUG_HF)
					bu_log("\t%d,%d ch_dim:%d,%d  min:(%d %d %d)  max:(%d %d %d)\n",
					       x, y,
					       d_bb->dspb_ch_dim[X],
					       d_bb->dspb_ch_dim[Y],
					       V3ARGS(d_bb->dspb_rpp.dsp_min),
					       V3ARGS(d_bb->dspb_rpp.dsp_max));
#endif
			}
		}
	}
}
#endif

/*	P L O T _ C E L L _ T O P 
 *
 *	Plot the results of intersecting a ray with the top of a cell
 *
 */
static void
plot_cell_top(struct isect_stuff *isect, 
	      struct dsp_bb *dsp_bb,
	      point_t A,
	      point_t B,
	      point_t C,
	      point_t D,
	      struct hit hitlist[],
	      int hitflags, int style)
{
    fastf_t *stom = &isect->dsp->dsp_i.dsp_stom[0];
    char buf[64];
    static int plotcnt = 0;
    static int cnt = 0;
    FILE *fp;
    point_t p1, p2, p3, p4;
    int i;
    int in_seg;
    static unsigned char colors[4][3] = {
	{255, 255, 128},
	{255, 128, 255},
	{128, 255, 128},
	{128, 255, 255},
    };


    if (style)
	sprintf(buf, "dsp_cell_isect%04d.pl", cnt++);
    else
	sprintf(buf, "dsp_cell_top%04d.pl", plotcnt++);

    if ( (fp=fopen(buf, "w")) == (FILE *)NULL) {
	bu_log("error opening \"%s\"\n", buf);
	return;
    } else {
	bu_log("plotting %s flags 0x%x\n\t", buf, hitflags);
    }

    plot_dsp_bb(fp, dsp_bb, isect->dsp, 128, 128, 128);

    /* plot the triangulation */
    pl_color(fp, 255, 255, 255);
    MAT4X3PNT(p1, stom, A);
    MAT4X3PNT(p2, stom, B);
    MAT4X3PNT(p3, stom, C);
    MAT4X3PNT(p4, stom, D);

    pdv_3move(fp, p1);
    pdv_3cont(fp, p2);
    pdv_3cont(fp, p4);
    if (!style) pl_color(fp, 96, 96, 96);

    pdv_3cont(fp, p1);

    if (!style) pl_color(fp, 255, 255, 255);
    pdv_3cont(fp, p3);
    pdv_3cont(fp, p4);


    /* plot the hit points */

    for (in_seg = 0, i = 0 ; i < 4 ; i++ ) {
	if (hitflags & (1<<i)) {
	    if (in_seg) {
		in_seg = 0;
		MAT4X3PNT(p1, stom, hitlist[i].hit_point);
		pdv_3cont(fp, p1);
		bu_log("\t%d outseg %g %g %g\n", i, V3ARGS(p1));
	    } else {
		in_seg = 1;
		pl_color(fp, colors[i][0], colors[i][1], colors[i][2]);
		MAT4X3PNT(p1, stom, hitlist[i].hit_point);
		pdv_3move(fp, p1);
		bu_log("\t%d inseg %g %g %g\n", i, V3ARGS(p1));
	    }
	}
    }
    fclose(fp);
}

/*	D S P _ P R I N T _ V 4
 *
 *
 */
static void
dsp_print_v4(vls, dsp_ip)
struct bu_vls *vls;
const struct rt_dsp_internal *dsp_ip;
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

/*	D S P _ P R I N T _ V 5
 *
 *
 */
static void
dsp_print_v5(vls, dsp_ip)
struct bu_vls *vls;
const struct rt_dsp_internal *dsp_ip;
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

	switch (dsp_ip->dsp_cuttype) {
	case DSP_CUT_DIR_ADAPT:
	    bu_vls_printf( vls, "cut=ad" ); break;
	case DSP_CUT_DIR_llUR:
	    bu_vls_printf( vls, "cut=lR" ); break;
	case DSP_CUT_DIR_ULlr:
	    bu_vls_printf( vls, "cut=Lr" ); break;
	}


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

/*
 *			R T _ D S P _ P R I N T
 */
void
rt_dsp_print( stp )
register const struct soltab *stp;
{
	register const struct dsp_specific *dsp =
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
 *	compute bounding boxes for each cell, then compute bounding boxes
 *	for collections of bounding boxes
 */
static void
dsp_layers(struct dsp_specific *dsp, unsigned short *d_min, unsigned short *d_max)
{
	int idx, i, j, k, curr_layer, x, y, xs, ys, xv, yv, tot;
	unsigned short dsp_min, dsp_max, cell_min, cell_max;
	unsigned short elev;
	struct dsp_bb *dsp_bb;
	struct dsp_rpp *t;
	struct dsp_bb_layer *curr, *prev;

	/* First we compute the total number of struct dsp_bb's we will need */
	xs = dsp->xsiz;
	ys = dsp->ysiz;
	tot = xs * ys;
	dsp->layers = 1;
	while ( xs > 1 || ys > 1 ) {
		xv = xs / DIM_BB_CHILDREN;
		yv = ys / DIM_BB_CHILDREN;
		if (xs % DIM_BB_CHILDREN) xv++;
		if (ys % DIM_BB_CHILDREN) yv++;

		if (rt_g.debug & DEBUG_HF)
			bu_log("layer %d   %dx%d\n", dsp->layers, xv, yv);

		tot += xv * yv;

		if (xv > 0) xs = xv;
		else xs = 1;

		if (yv > 0) ys = yv;
		else ys = 1;
		dsp->layers++;
	}


	if (rt_g.debug & DEBUG_HF)
		bu_log("%d layers total\n", dsp->layers);


	/* allocate the struct dsp_bb's we will need */
	dsp->layer = bu_malloc(dsp->layers * sizeof(struct dsp_bb_layer),
			       "dsp_bb_layers array");
	dsp->bb_array = bu_malloc(tot * sizeof(struct dsp_bb), "dsp_bb array");

	/* now we fill in the "lowest" layer of struct dsp_bb's from the
	 * raw data
	 */
	dsp->layer[0].dim[X] = dsp->xsiz;
	dsp->layer[0].dim[Y] = dsp->ysiz;
	dsp->layer[0].p = dsp->bb_array;

	xs = dsp->xsiz;
	ys = dsp->ysiz;

	dsp_min = 0xffff;
	dsp_max = 0;

	for (y=0 ; y < YSIZ(dsp) ; y++) {

		cell_min = 0xffff;
		cell_max = 0;

		for (x=0 ; x < XSIZ(dsp) ; x++) {

#if 0
			if (rt_g.debug & DEBUG_HF)
				bu_log("filling %d,%d\n", x, y);
#endif
			elev = DSP(dsp, x, y);
			cell_min = cell_max = elev;

			elev = DSP(dsp, x+1, y);
			V_MIN(cell_min, elev);
			V_MAX(cell_max, elev);

			elev = DSP(dsp, x, y+1);
			V_MIN(cell_min, elev);
			V_MAX(cell_max, elev);

			elev = DSP(dsp, x+1, y+1);
			V_MIN(cell_min, elev);
			V_MAX(cell_max, elev);

			/* factor the cell min/max into the overall min/max */
			V_MIN(dsp_min, cell_min);
			V_MAX(dsp_max, cell_max);

			/* fill in the dsp_rpp cell min/max */
			i = y*XSIZ(dsp) + x;
			dsp_bb = &dsp->layer[0].p[i];
			VSET(dsp_bb->dspb_rpp.dsp_min, x, y, cell_min);
			VSET(dsp_bb->dspb_rpp.dsp_max, x+1, y+1, cell_max);

			/* There are no "children" of a layer 0 element */
			dsp_bb->dspb_ch_dim[X] = 0;
			dsp_bb->dspb_ch_dim[Y] = 0;
			for (k=0 ; k < NUM_BB_CHILDREN ; k++ ) {
				dsp_bb->dspb_children[k] = 
					(struct dsp_bb *)NULL;
			}
#if 0
			dlog("cell %d,%d min: %d,%d,%d  max: %d,%d,%d\n",
			     x, y, V3ARGS(dsp_bb->dspb_rpp.dsp_min),
			     V3ARGS(dsp_bb->dspb_rpp.dsp_max) );
#endif

			/* XXX should we compute the triangle orientation and
			 * save it here too?
			 */
		}
	}

	*d_min = dsp_min;
	*d_max = dsp_max;


	if (rt_g.debug & DEBUG_HF)
			bu_log("filled first layer\n");


	/* now we compute successive layers from the initial layer */
	for (curr_layer = 1 ; curr_layer < dsp->layers ; curr_layer++ ) {
		/* compute the number of cells in each direction for this layer */
		xs = dsp->layer[curr_layer-1].dim[X];
		if (xs % DIM_BB_CHILDREN)  
			dsp->layer[curr_layer].dim[X] =
				xs / DIM_BB_CHILDREN + 1;
		else			  
			dsp->layer[curr_layer].dim[X] =
				xs / DIM_BB_CHILDREN;

		ys = dsp->layer[curr_layer-1].dim[Y];
		if (ys % DIM_BB_CHILDREN) 
			dsp->layer[curr_layer].dim[Y] = 
				ys / DIM_BB_CHILDREN + 1;
		else			  
			dsp->layer[curr_layer].dim[Y] = 
				ys / DIM_BB_CHILDREN;
		
		/* set the start of the array for this layer */
		dsp->layer[curr_layer].p = &dsp->layer[curr_layer-1].p[
				    dsp->layer[curr_layer-1].dim[X] *
				    dsp->layer[curr_layer-1].dim[Y] ];

		curr = &dsp->layer[curr_layer];
		prev = &dsp->layer[curr_layer-1];

		if (rt_g.debug & DEBUG_HF)
			bu_log("layer %d\n", curr_layer);

		/* walk the grid and fill in the values for this layer */
		for (y=0 ; y < curr->dim[Y] ; y++ ) {
			for (x=0 ; x < curr->dim[X] ; x++ ) {
				int n, xp, yp;
				/* x,y are in the coordinates in the current
				 * layer.  xp,yp are the coordinates of the
				 * same area in the previous (lower) layer.
				 */
				xp = x * DIM_BB_CHILDREN;
				yp = y * DIM_BB_CHILDREN;

				/* initialize the current dsp_bb cell */
				dsp_bb = &curr->p[y*curr->dim[X]+x];
				n = (int)pow( (double)DIM_BB_CHILDREN, (double)curr_layer);
				VSET(dsp_bb->dspb_rpp.dsp_min,
				     x * n, y * n, 0x0ffff);
				VSET(dsp_bb->dspb_rpp.dsp_max,
				     x * n, y * n, 0);

				tot = 0;
#if 0
				dlog("\tcell %d,%d  (%d,%d-%d,%d)\n", x, y, 
					       dsp_bb->dspb_rpp.dsp_min[X],
					       dsp_bb->dspb_rpp.dsp_min[Y],
					       (x+1) * n, (y+1)*n);
#endif
				i=0;
				for (j=0 ; j < DIM_BB_CHILDREN && (yp+j) < prev->dim[Y] ; j++) {
					for (i=0 ; i < DIM_BB_CHILDREN && (xp+i) < prev->dim[X]; i++) {

						idx = (yp+j) * prev->dim[X] + xp+i;

						t = &prev->p[ idx ].dspb_rpp;

						VMINMAX(dsp_bb->dspb_rpp.dsp_min, dsp_bb->dspb_rpp.dsp_max, t->dsp_min);
						VMINMAX(dsp_bb->dspb_rpp.dsp_min, dsp_bb->dspb_rpp.dsp_max, t->dsp_max);

						dsp_bb->dspb_children[tot++] = &prev->p[ idx ];

#if 0
						dlog("\t\tsubcell %d,%d min: %d,%d,%d  max: %d,%d,%d\n",
						     xp+i, yp+j, V3ARGS(t->dsp_min), V3ARGS(t->dsp_max) );

						if (rt_g.debug & DEBUG_HF) 
						    if (i+1 >= DIM_BB_CHILDREN || xp+i+1 >= prev->dim[X])
							bu_log("\n");
#endif
					}
				}
#if 0
				dlog("\t\txy: %d,%d, ij:%d,%d min:%d,%d,%d max:%d,%d,%d\n",
				     x, y, i, j,
				     V3ARGS(dsp_bb->dspb_rpp.dsp_min),
				     V3ARGS(dsp_bb->dspb_rpp.dsp_max) );
#endif

				dsp_bb->dspb_ch_dim[X] = i;
				dsp_bb->dspb_ch_dim[Y] = j;
			}
		}
	}

#ifdef PLOT_LAYERS
	if (rt_g.debug & DEBUG_HF) {
		plot_layers(dsp);
		bu_log("_  x:%d y:%d min %d max %d\n",
		       XCNT(dsp), YCNT(dsp), dsp_min, dsp_max);
	}
#endif
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
	unsigned short dsp_min, dsp_max;
	point_t pt, bbpt;
	vect_t work;
	fastf_t f;

	if (RT_G_DEBUG & DEBUG_HF)
		bu_log("rt_dsp_prep()\n");

	RT_CK_DB_INTERNAL(ip);
	dsp_ip = (struct rt_dsp_internal *)ip->idb_ptr;
	RT_DSP_CK_MAGIC(dsp_ip);
	BU_CK_VLS(&dsp_ip->dsp_name);

	switch (dsp_ip->dsp_datasrc) {
	case RT_DSP_SRC_V4_FILE: 
	case RT_DSP_SRC_FILE:
	    BU_CK_MAPPED_FILE(dsp_ip->dsp_mp);
	    break;
	case RT_DSP_SRC_OBJ:
	    break;
	}


	BU_GETSTRUCT( dsp, dsp_specific );
	stp->st_specific = (genptr_t) dsp;
	dsp->dsp_i = *dsp_ip;		/* struct copy */

	bu_semaphore_acquire( RT_SEM_MODEL);
	++dsp_ip->dsp_mp->uses;
	bu_semaphore_release( RT_SEM_MODEL);

	dsp->xsiz = dsp_ip->dsp_xcnt-1;	/* size is # cells or values-1 */
	dsp->ysiz = dsp_ip->dsp_ycnt-1;	/* size is # cells or values-1 */


	/* compute the multi-resolution bounding boxes */
	dsp_layers(dsp, &dsp_min, &dsp_max);


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

	if (RT_G_DEBUG & DEBUG_HF) {
		bu_log("  model space bbox (%g %g %g) (%g %g %g)\n",
			V3ARGS(stp->st_min),
			V3ARGS(stp->st_max));
	}


	switch (dsp_ip->dsp_datasrc) {
	case RT_DSP_SRC_V4_FILE: 
	case RT_DSP_SRC_FILE:
	    BU_CK_MAPPED_FILE(dsp_ip->dsp_mp);
	    break;
	case RT_DSP_SRC_OBJ:
	    break;
	}

	return 0;
}



static void
add_seg(struct isect_stuff *isect,
	struct hit *in_hit,
	struct hit *out_hit,
	const point_t bbmin,/* The bounding box of what you are adding ... */
	const point_t bbmax,/* ... */
	int r, int g, int b)/* ... this is strictly for debug plot purposes */

{
    struct dsp_specific *dsp = isect->dsp;
    fastf_t *stom = &dsp->dsp_i.dsp_stom[0];
    FILE *fp;
    char fname[32];
    static int segnum = 0;
    struct seg *seg;
    struct seg *seg_p;
    struct bound_rpp rpp;

    RT_GET_SEG(seg, isect->ap->a_resource);

    seg->seg_in = *in_hit; /* struct copy */
    seg->seg_in.hit_magic = RT_HIT_MAGIC;
    seg->seg_in.hit_vpriv[Z] = 0; /* flag as in-hit */
    /* XXX */
    if (seg->seg_in.hit_surfno < 0 || seg->seg_in.hit_surfno > ZTOP) {
	bu_log("%s:%d bogus surfno %d  dist:%g\n", __FILE__, __LINE__,
	       seg->seg_in.hit_surfno, seg->seg_in.hit_dist);
	bu_bomb("");
    }


    seg->seg_out = *out_hit; /* struct copy */
    seg->seg_out.hit_magic = RT_HIT_MAGIC;
    seg->seg_out.hit_vpriv[Z] = 1; /* flag as out-hit */
    /* XXX */
    if (seg->seg_out.hit_surfno < 0 || seg->seg_out.hit_surfno > ZTOP) {
	bu_log("%s:%d bogus surfno %d  dist:%g\n", __FILE__, __LINE__,
	       seg->seg_in.hit_surfno, seg->seg_out.hit_dist);
	bu_bomb("");
    }

    seg->seg_stp = isect->stp;

    /* insert the segment in the list by in-hit distance */
    for ( BU_LIST_FOR(seg_p, seg, &isect->seglist) ) {
	fp = (FILE *)NULL;
	if (seg->seg_in.hit_dist < seg_p->seg_in.hit_dist) {
	    /* found the spot for this one */
	    BU_LIST_INSERT(&(seg_p->l), &seg->l);
	    goto done;
	}
    }
    /* this one goes in the end of the list */
    BU_LIST_INSERT(&isect->seglist, &seg->l);
 done:


    if (seg->seg_in.hit_dist > seg->seg_out.hit_dist) {
	bu_log("DSP:  Adding inside-out seg\n");
    }

    if (rt_g.debug & DEBUG_HF) {
	/* plot the bounding box and the seg */
	sprintf(fname, "dsp_seg%04d.pl", segnum++);

	if ((fp=fopen(fname, "w")) != (FILE *)NULL) {
	    bu_log("plotting %s\n", fname);

	    MAT4X3PNT(rpp.min, stom, bbmin);
	    MAT4X3PNT(rpp.max, stom, bbmax);
	    plot_rpp(fp, &rpp, r/2, g/2, b/2);

	    /* re-use the rpp for the points for the segment */
	    MAT4X3PNT(rpp.min, stom, in_hit->hit_point);
	    MAT4X3PNT(rpp.max, stom, out_hit->hit_point);

	    pl_color(fp, r, g, b);
	    pdv_3line(fp, rpp.min, rpp.max);

	    fclose(fp);
	}

    }
}




/*	I S E C T _ R A Y _ T R I A N G L E
 *
 * Side Effects:
 *	dist and P may be set
 *
 * Return:
 *	1	Ray intersects triangle 
 *	0	Ray misses triangle
 *	-1	Ray/plane parallel
 */
static int
isect_ray_triangle(struct isect_stuff *isect, 
		   point_t A,
		   point_t B,
		   point_t C,
		   struct hit *hitp,
		   double alphabbeta[])

{
    point_t P;
    vect_t AB, AC;
    plane_t N;	/* Normal for plane of triangle */
    double NdotDir, alpha, beta, u0, u1, u2, v0, v1, v2;

    dlog("isect_ray_triangle\n");

    VSUB2(AB, B, A);
    VSUB2(AC, C, A);

    /* Compute the plane equation of the triangle */
    VCROSS(N, AB, AC);
    VUNITIZE(N);
    VMOVE(hitp->hit_normal, N);
    N[H] = VDOT(N, A);


    /* intersect ray with plane */
    NdotDir = VDOT(N, isect->r.r_dir);
    if ( BN_VECT_ARE_PERP(NdotDir, isect->tol) ) {
	/* Ray perpendicular to plane of triangle */
	return -1;
    }

    /* dist to plane icept */
    hitp->hit_dist = (N[H]-VDOT(N, isect->r.r_pt)) / NdotDir;

    VJOIN1(P, isect->r.r_pt, hitp->hit_dist, isect->r.r_dir);
    VMOVE(hitp->hit_point, P);

    if (rt_g.debug & DEBUG_HF) {
	vect_t tmp;
	fastf_t *stom = &isect->dsp->dsp_i.dsp_stom[0];

	MAT4X3PNT(tmp, stom, hitp->hit_point);

	bu_log("\tdist:%g plane point: %g %g %g\n", hitp->hit_dist, V3ARGS(tmp));
    }
    /* We project the system into the XY plane at this point to determine
     * if the ray_plane_isect_pt is within the bounds of the triangle
     *
     * The general idea here is to project the vector AP onto both sides of the
     * triangle.  The distances along each side can be used to determine if
     * we are inside/outside the triangle.
     *
     *    VSUB2(AP, P, A);
     *    alpha = VDOT(AP, AB);
     *    beta = VDOT(AP, AC);
     *
     *		  C
     *		  |
     *		  |
     *		  |
     *----	  |--- P
     *	|	  |   /
     *	|	  |  / |
     * alpha	  | /  | 
     *	|	  |/   |
     *----	  A---------B
     *
     *		    b
     *		  |-e--|
     *		    t
     *		    a
     *
     *	To save on computation, we do this calculation in 2D.
     *
     * See: "Graphics Gems",  Andrew S. Glassner ed.  PP390-393 for details
     */

    u0 = P[0] - A[0];	/* delta X */
    v0 = P[1] - A[1];	/* delta Y */

    u1 = AB[0];    /*   u1 = B[0] - A[0]; always 0 or 1 */
    u2 = AC[0];    /*	u2 = C[0] - A[0]; always 0 or 1 */

    v1 = AB[1];    /* v1 = B[1] - A[1]; always 0 or 1 */
    v2 = AC[1];    /* v2 = C[1] - A[1]; always 0 or 1 */

    if ( u1 == 0 ) { 
	beta = u0/u2;
	if ( 0.0 <= beta && beta <= 1.0 ) {
	    alpha = (v0 - beta * v2) / v1;
	} else 
	    alpha = 0;
    } else {
	beta = (v0 * u1 - u0 * v1) / (v2 * u1 - u2 * v1);
	if ( 0.0 <= beta && beta <= 1.0 ) {
	    alpha = (u0 - beta * u2)/ u1;
	} else
	    alpha = 0;
    }

    /* return 1 if we hit the triangle */
    alphabbeta[0] = alpha;
    alphabbeta[1] = beta;

    if (alpha >= 0.0 && beta >= 0.0 && (alpha+beta) < 1.0)
	return 1;

    return 0;
}

/*	P E R M U T E _ C E L L
 *
 *	For adaptive diagonal selection or for Upper-Left to lower right
 *	cell cut, we must permute the verticies of the cell before handing
 *	them to the intersection algorithm.  That's what this function does.
 */
static int
permute_cell(point_t A, 
	point_t B, 
	point_t C, 
	point_t D, 
	struct isect_stuff *isect, 
	struct dsp_bb *dsp_bb)
{
    int x, y;

	if (rt_g.debug & DEBUG_HF) {
	    VPRINT("\tA", A);
	    VPRINT("\tB", B);
	    VPRINT("\tC", C);
	    VPRINT("\tD", D);
	}


    switch (isect->dsp->dsp_i.dsp_cuttype) {
    case DSP_CUT_DIR_llUR:
	return DSP_CUT_DIR_llUR;
	break;

    case DSP_CUT_DIR_ADAPT: {
	int lo[2], hi[2];
	point_t tmp;
	double h1, h2, h3, h4;
	double cAD, cBC;  /* curvature in direction AD, and BC */

	if (rt_g.debug & DEBUG_HF)
	    bu_log("cell %d,%d adaptive triangulation... ",
		   dsp_bb->dspb_rpp.dsp_min[X],
		   dsp_bb->dspb_rpp.dsp_min[Y]);

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

	lo[X] = dsp_bb->dspb_rpp.dsp_min[X] - 1;
	lo[Y] = dsp_bb->dspb_rpp.dsp_min[Y] - 1;
	hi[X] = dsp_bb->dspb_rpp.dsp_max[X] + 1;
	hi[Y] = dsp_bb->dspb_rpp.dsp_max[Y] + 1;

	/* compute curvature along the A->D direction */
	h1 = DSP(&isect->dsp->dsp_i, lo[X], lo[Y]);
	h2 = A[Z];
	h3 = D[Z];
	h4 = DSP(isect->dsp, hi[X], hi[Y]);

	cAD = fabs(h3 + h1 - 2*h2 ) + fabs( h4 + h2 - 2*h3 );


	/* compute curvature along the B->C direction */
	h1 = DSP(isect->dsp, hi[X], lo[Y]);
	h2 = B[Z];
	h3 = C[Z];
	h4 = DSP(isect->dsp, lo[X], hi[Y]);

	cBC = fabs(h3 + h1 - 2*h2 ) + fabs( h4 + h2 - 2*h3 );

	if ( cAD < cBC ) {
	    /* A-D cut is fine, no need to permute */
	    if (rt_g.debug & DEBUG_HF)
		bu_log("A-D cut\n");

	    return DSP_CUT_DIR_llUR;
	}

	/* prefer the B-C cut */
	VMOVE(tmp, A);
	VMOVE(A, B);
	VMOVE(B, D);
	VMOVE(D, C);
	VMOVE(C, tmp);
	if (rt_g.debug & DEBUG_HF)
		bu_log("B-C cut\n");

	return DSP_CUT_DIR_ULlr;

	break;
    }
    case DSP_CUT_DIR_ULlr:
	/* assign the values for the corner points
	 *
	 *  D----C
	 *  |    |
	 *  |    |
	 *  |    |
	 *  B----A
	 */
	x = dsp_bb->dspb_rpp.dsp_min[X];
	y = dsp_bb->dspb_rpp.dsp_min[Y];
	VSET(B, x, y, DSP(isect->dsp, x, y) );

	x = dsp_bb->dspb_rpp.dsp_max[X];
	VSET(A, x, y, DSP(isect->dsp, x, y) );

	y = dsp_bb->dspb_rpp.dsp_max[Y];
	VSET(C, x, y, DSP(isect->dsp, x, y) );

	x = dsp_bb->dspb_rpp.dsp_min[X];
	VSET(D, x, y, DSP(isect->dsp, x, y) );

	return DSP_CUT_DIR_ULlr;
	break;
    }
    bu_log("%s:%d Unknown DSP cut direction: %d\n",
	       __FILE__, __LINE__, isect->dsp->dsp_i.dsp_cuttype);
    bu_bomb("");
    /* not reached */
    return -1;
}

/*
 *	C H E C K _ B B _ E L E V A T I O N
 *
 *	determine if a point P is above/below the slope line on the
 *	bounding box.  eg:
 *
 *	Bounding box side view:
 *
 *	+-------+
 *	|	|
 *	|	*	Determine if P ( or Q ) is above the
 *	|      /|	diagonal from the two * points at the corners
 *	| P.  /	|	of the bounding box.
 *	|    /	|
 *	|   /	|
 *	|  /  .	|
 *	| /   Q	|
 *	|/	|
 *	*	|
 *	|	|
 *	+-------+
 *
 *	Return
 *		0 if pt above line (such as P in diagram)
 *		1 if pt at/below line (such as Q in diagram)
 */
static int
check_bbpt_hit_elev(int i,	/* indicates face of cell */
		   point_t A,
		   point_t B,
		   point_t C,
		   point_t D,
		   point_t P)
{
    double slope = 0.0;
    double delta = 0.0;
    double origin = 0.0;

    dlog("check_bbpt_hit_elev(");

    switch (i) {
    case XMIN:
	/* the minimal YZ plane.  Top view:	*   *
	 *					|    
	 *					*   *		
	 */
	dlog("XMIN)\n");
	slope = C[Z] - A[Z];
	delta = P[Y] - A[Y];
	origin = A[Z];
	break;
    case XMAX:
	/* the maximal YZ plane.   Top view:	*   *
	 *					    |    
	 *					*   *
	 */
	dlog("XMAX)\n");
	slope = D[Z] - B[Z];
	delta = P[Y] - B[Y];
	origin = B[Z];
	break;
    case YMIN:
	/* the minimal XZ plane.   Top view:	*   *
	 *
	 *					* - *
	 */
	dlog("YMIN)\n");
	slope = B[Z] - A[Z];
	delta = P[X] - A[X];
	origin = A[Z];
	break;
    case YMAX:
	/* the maximal XZ plane.   Top view:	* - *
	 *	    
	 *					*   *
	 */
	dlog("YMAX)\n");
	slope = D[Z] - C[Z];
	delta = P[X] - C[X];
	origin = C[Z];
	break;
    case ZMIN:
	dlog("ZMIN)\n");
	return 1;
	break;
    case ZMAX:
	dlog("ZMAX)\n");
	return 0;
	break;
    default:
	bu_log("%s:%d Coding error, bad face\n", __FILE__, __LINE__);
	bu_bomb("");
	break;
    }

    if ( (origin + slope * delta) < P[Z] ) return 0;

    return 1;
}

/*
 *	I S E C T _ R A Y _ C E L L _ T O P 
 *
 *
 */
static void
isect_ray_cell_top(struct isect_stuff *isect, struct dsp_bb *dsp_bb)
{
    point_t A, B, C, D, P;
    int x, y;
    int direction;
    double ab_first[2], ab_second[2];
    struct hit hits[4];	/* list of hits that are valid */
    struct hit *hitp;
    int hitf = 0;	/* bit flags for valid hits in hits */
    int cond, i;
    point_t bbmin, bbmax;


    dlog("isect_ray_cell_top\n");

    /* assign the values for the corner points
     *
     *  C----D
     *  |    |
     *  |    |
     *  |    |
     *  A----B
     */
    x = dsp_bb->dspb_rpp.dsp_min[X];
    y = dsp_bb->dspb_rpp.dsp_min[Y];
    VSET(A, x, y, DSP(isect->dsp, x, y) );
    
    x = dsp_bb->dspb_rpp.dsp_max[X];
    VSET(B, x, y, DSP(isect->dsp, x, y) );

    y = dsp_bb->dspb_rpp.dsp_max[Y];
    VSET(D, x, y, DSP(isect->dsp, x, y) );

    x = dsp_bb->dspb_rpp.dsp_min[X];
    VSET(C, x, y, DSP(isect->dsp, x, y) );


    if (rt_g.debug & DEBUG_HF) {
	point_t p1, p2;

	VJOIN1(p1, isect->r.r_pt, isect->r.r_min, isect->r.r_dir);
	VMOVE(hits[0].hit_point, p1);
	hits[0].hit_dist = isect->r.r_min;

	VJOIN1(p2, isect->r.r_pt, isect->r.r_max, isect->r.r_dir);
	VMOVE(hits[1].hit_point, p2);
	hits[1].hit_dist = isect->r.r_max;

	plot_cell_top(isect, dsp_bb, A, B, C, D, hits, 3, 0);
    }



    /* first order of business is to discard any "fake" hits on the 
     * bounding box, and fill in any "real" hits in our list
     */
    VJOIN1(P, isect->r.r_pt, isect->r.r_min, isect->r.r_dir);
    if ( check_bbpt_hit_elev(isect->dmin, A, B, C, D, P) ) {
	hits[0].hit_dist = isect->r.r_min;
	VMOVE(hits[0].hit_point, P);
	VMOVE(hits[0].hit_normal, dsp_pl[isect->dmin]);
	/* vpriv */
	/* private */
	hits[0].hit_surfno = isect->dmin;

	/* XXX need to fill in rest of hit struct */
	hitf = 1;
	dlog("hit ray/bb min\n");
    } else {
	dlog("miss ray/bb min\n");
    }


    VJOIN1(P, isect->r.r_pt, isect->r.r_max, isect->r.r_dir);
    if (check_bbpt_hit_elev(isect->dmax, A, B, C, D, P) ) {
	hits[3].hit_dist = isect->r.r_max;
	VMOVE(hits[3].hit_point, P);
	VMOVE(hits[3].hit_normal, dsp_pl[isect->dmax]);
	/* vpriv */
	/* private */
	hits[3].hit_surfno = isect->dmax;

	/* XXX need to fill in rest of hit struct */
	hitf |= 8;
	dlog("hit ray/bb max\n");
    } else {
	dlog("miss ray/bb max\n");
    }



    direction = permute_cell(A, B, C, D, isect, dsp_bb);

    if ((cond=isect_ray_triangle(isect, B, D, A, &hits[1], ab_first)) > 0.0){
	/* hit triangle */

	/* record cell */
	hits[1].hit_vpriv[X] = dsp_bb->dspb_rpp.dsp_min[X];
	hits[1].hit_vpriv[Y] = dsp_bb->dspb_rpp.dsp_min[Y];
	hits[1].hit_surfno = ZTOP; /* indicate we hit the top */

	hitf |= 2; 
	dlog("\t hit triangle 1 (alpha: %g beta:%g alpha+beta: %g)\n",
	     ab_first[0], ab_first[1], ab_first[0] + ab_first[1]);
    } else {
	dlog("\tmiss triangle 1 (alpha: %g beta:%g a+b: %g) cond:%d\n",
	     ab_first[0], ab_first[1], ab_first[0] + ab_first[1], cond);
    }
    if ((cond=isect_ray_triangle(isect, C, A, D, &hits[2], ab_second)) > 0.0) {
	/* hit triangle */

	/* record cell */
	hits[2].hit_vpriv[X] = dsp_bb->dspb_rpp.dsp_min[X];
	hits[2].hit_vpriv[Y] = dsp_bb->dspb_rpp.dsp_min[Y];
	hits[2].hit_surfno = ZTOP; /* indicate we hit the top */

	hitf |= 4;
	dlog("\t hit triangle 2 (alpha: %g beta:%g alpha+beta: %g)\n",
	     ab_second[0], ab_second[1], ab_second[0] + ab_second[1]);
    } else {
	dlog("\tmiss triangle 2 (alpha: %g beta:%g alpha+beta: %g) cond:%d\n",
	     ab_second[0], ab_second[1], ab_second[0] + ab_second[1], cond);
    }


    if (rt_g.debug & DEBUG_HF) {
	plot_cell_top(isect, dsp_bb, A, B, C, D, hits, hitf, 1);
    }

    /* fill out the segment structures */
    hitp = 0;
    for (i = 0 ; i < 4 ; i++ ) {
	if (hitf & (1<<i)) {
	    if (hitp) {
		/* create seg with hits[i].hit_point); as in point */
		if (rt_g.debug & DEBUG_HF) {
		    /* int/float conv */
		    VMOVE(bbmin, dsp_bb->dspb_rpp.dsp_min);
		    VMOVE(bbmax, dsp_bb->dspb_rpp.dsp_max);
		    bu_log("out-hit at dist %g\n", hits[i].hit_dist);
		}

		add_seg(isect, hitp, &hits[i], bbmin, bbmax, 255, 255, 255);
		hitp = 0;
	    } else {
		/* finish seg with hits[i].hit_point); as out point */
		if (rt_g.debug & DEBUG_HF) {
		    bu_log("in-hit at dist %g\n", hits[i].hit_dist);
		}
		hitp = &hits[i];
	    }
	}
    }

    if (hitp) {
	point_t p1, p2;

	bu_log("----------------ERROR incomplete segment-------------\n");
	bu_log("  pixel %d %d\n", isect->ap->a_x, isect->ap->a_y);

	VJOIN1(p1, isect->r.r_pt, isect->r.r_min, isect->r.r_dir);
	VMOVE(hits[0].hit_point, p1);
	hits[0].hit_dist = isect->r.r_min;

	VJOIN1(p2, isect->r.r_pt, isect->r.r_max, isect->r.r_dir);
	VMOVE(hits[1].hit_point, p2);
	hits[1].hit_dist = isect->r.r_max;

	plot_cell_top(isect, dsp_bb, A, B, C, D, hits, 3, 0);
    }

}

/*
 *			D S P _ I N _ R P P
 *
 *  Compute the intersections of a ray with a rectangular parallelpiped (RPP)
 *  that has faces parallel to the coordinate planes
 *
 *  The algorithm here was developed by Gary Kuehl for GIFT.
 *  A good description of the approach used can be found in
 *  "??" by XYZZY and Barsky,
 *  ACM Transactions on Graphics, Vol 3 No 1, January 1984.
 *
 * Note -
 *  The computation of entry and exit distance is mandatory, as the final
 *  test catches the majority of misses.
 *
 * Note -
 *  A hit is returned if the intersect is behind the start point.
 *
 *  Returns -
 *	 0  if ray does not hit RPP,
 *	!0  if ray hits RPP.
 *
 *  Implicit return -
 *	rp->r_min = dist from start of ray to point at which ray ENTERS solid
 *	rp->r_max = dist from start of ray to point at which ray LEAVES solid
 *
 * XXX need to return surface number for in/out
 */
int
dsp_in_rpp(struct isect_stuff *isect,
	  register const fastf_t *min,
	  register const fastf_t *max)
{
    struct xray		*rp = &isect->r;    
    register const fastf_t *invdir = isect->inv_dir;	/* inverses of rp->r_dir[] */
    register const fastf_t	*pt = &rp->r_pt[0];
    register fastf_t	sv;
    register fastf_t	rmin = -INFINITY;
    register fastf_t	rmax =  INFINITY;
    int dmin = -1;
    int dmax = -1;

    /* Start with infinite ray, and trim it down */

    /* X axis */
    if( *invdir < 0.0 )  {
	/* Heading towards smaller numbers */
	/* if( *min > *pt )  miss */
	if(rmax > (sv = (*min - *pt) * *invdir) ) {
	    rmax = sv;
	    dmax = XMIN;
	}
	if( rmin < (sv = (*max - *pt) * *invdir) ) {
	    rmin = sv;
	    dmin = XMAX;
	}
    }  else if( *invdir > 0.0 )  {
	/* Heading towards larger numbers */
	/* if( *max < *pt )  miss */
	if (rmax > (sv = (*max - *pt) * *invdir) ) {
	    rmax = sv;
	    dmax = XMAX;
	}
	if( rmin < ((sv = (*min - *pt) * *invdir)) ) {
	    rmin = sv;
	    dmin = XMIN;
	}
    }  else  {
	/*
	 *  Direction cosines along this axis is NEAR 0,
	 *  which implies that the ray is perpendicular to the axis,
	 *  so merely check position against the boundaries.
	 */
	if( (*min > *pt) || (*max < *pt) )
	    return(0);	/* MISS */
    }

    /* Y axis */
    pt++; invdir++; max++; min++;
    if( *invdir < 0.0 )  {
	if (rmax > (sv = (*min - *pt) * *invdir) ) {
	    /* towards smaller */
	    rmax = sv;
	    dmax = YMIN;
	}
	if (rmin < (sv = (*max - *pt) * *invdir) ) {
	    rmin = sv;
	    dmin = YMAX;
	}
    }  else if( *invdir > 0.0 )  {
	/* towards larger */
	if(rmax > (sv = (*max - *pt) * *invdir) ) {
	    rmax = sv;
	    dmax = YMAX;
	}
	if( rmin < ((sv = (*min - *pt) * *invdir)) ) {
	    rmin = sv;
	    dmin = YMIN;
	}
    }  else  {
	if( (*min > *pt) || (*max < *pt) )
	    return(0);	/* MISS */
    }

    /* Z axis */
    pt++; invdir++; max++; min++;
    if( *invdir < 0.0 )  {
	/* towards smaller */
	if(rmax > (sv = (*min - *pt) * *invdir) ) {
	    rmax = sv;
	    dmax = ZMIN;
	} 
	if( rmin < (sv = (*max - *pt) * *invdir) ) {
	    rmin = sv;
	    dmin = ZMAX;
	}
    }  else if( *invdir > 0.0 )  {
	/* towards larger */
	if(rmax > (sv = (*max - *pt) * *invdir) ) {
	    rmax = sv;
	    dmax = ZMAX;
	}
	if( rmin < ((sv = (*min - *pt) * *invdir)) ) {
	    rmin = sv;
	    dmin = ZMIN;
	}
    }  else  {
	if( (*min > *pt) || (*max < *pt) )
	    return(0);	/* MISS */
    }

    /* If equal, RPP is actually a plane */
    if( rmin > rmax )
	return(0);	/* MISS */

    /* HIT.  Only now do rp->r_min and rp->r_max have to be written */
    rp->r_min = rmin;
    rp->r_max = rmax;
    
    isect->dmin = dmin;
    isect->dmax = dmax;
    return(1);		/* HIT */
}




/*
 *	I S E C T _ R A Y _ D S P _ B B
 *
 *  Intersect a ray with a DSP bounding box.  This is the primary child of
 *  rt_dsp_shot()
 */
static void 
isect_ray_dsp_bb(struct isect_stuff *isect, struct dsp_bb *dsp_bb)
{
    int i;
    point_t bbmin, bbmax;
    point_t minpt, maxpt;
    double min_z = dsp_bb->dspb_rpp.dsp_min[Z];

    if (rt_g.debug & DEBUG_HF) {

	bu_log("isect_ray_dsp_bb( (%d,%d,%d) (%d,%d,%d))\n",
	       V3ARGS(dsp_bb->dspb_rpp.dsp_min),
	       V3ARGS(dsp_bb->dspb_rpp.dsp_min));
    }

    /* check to see if we miss the RPP for this area entirely */
    VMOVE(bbmax, dsp_bb->dspb_rpp.dsp_max);
    VSET(bbmin,
	 dsp_bb->dspb_rpp.dsp_min[X], 
	 dsp_bb->dspb_rpp.dsp_min[Y], 0.0);

    if (!dsp_in_rpp(isect, bbmin, bbmax) ) {
	/* missed it all, just return */
	return;
    }

    /* At this point we know that we've hit the overall bounding box */

    /* if both hits are UNDER the top of the "foundation" pillar, we can
     * just add a segment for that range and return
     */
    VJOIN1(minpt, isect->r.r_pt, isect->r.r_min, isect->r.r_dir);
    VJOIN1(maxpt, isect->r.r_pt, isect->r.r_max, isect->r.r_dir);
    if (minpt[Z] < min_z && maxpt[Z] < min_z) {
	/* add hit segment */
	struct hit in_hit, out_hit;

	in_hit.hit_dist = isect->r.r_min;
	VMOVE(in_hit.hit_point, minpt);
	VMOVE(in_hit.hit_normal, dsp_pl[isect->dmin]);
	/* hit_priv   */
	/* hit_private */
	in_hit.hit_surfno = isect->dmin;
	/* hit_rayp */

	out_hit.hit_dist = isect->r.r_max;
	VMOVE(out_hit.hit_point, minpt);
	VMOVE(out_hit.hit_normal, dsp_pl[isect->dmax]);
	/* hit_priv   */
	/* hit_private */
	out_hit.hit_surfno = isect->dmax;
	/* hit_rayp */

	if (rt_g.debug & DEBUG_HF) {
	    /* we need these for debug output */
	    VMOVE(in_hit.hit_point, minpt);
	    VMOVE(out_hit.hit_point, maxpt);

	    /* create a special bounding box for plotting purposes */
	    VMOVE(bbmax,  dsp_bb->dspb_rpp.dsp_max);
	    VMOVE(bbmin,  dsp_bb->dspb_rpp.dsp_min);
	    bbmax[Z] = bbmin[Z];
	    bbmin[Z] = 0.0;
	}

	add_seg(isect, &in_hit, &out_hit, bbmin, bbmax, 0, 255, 255);

	/* outta here */
	return;
    }


    /* We've hit something where we might be going through the
     * boundary.  We've got to intersect the children
     */
    if (dsp_bb->dspb_ch_dim[0]) {
	/* there are children, so we recurse */
	i = dsp_bb->dspb_ch_dim[0] * dsp_bb->dspb_ch_dim[1] - 1;
	for ( ; i >= 0 ; i--)
	    isect_ray_dsp_bb(isect, dsp_bb->dspb_children[i]);

	return;
    }

    /***********************************************************************
     *
     *	 This section is for level 0 intersections only
     *
     ***********************************************************************/

    /* intersect the DSP grid surface geometry */

    /* Check for a hit on the triangulated zone on top.
     * This gives us intersections on the triangulated top, and the sides
     * and bottom of the bounding box for the triangles.
     *
     * We do this first because we already know that the ray does NOT
     * just pass through the "foundation " pillar underneath (see test above)
     */
    bbmin[Z] = dsp_bb->dspb_rpp.dsp_min[Z];
    if (dsp_in_rpp(isect, bbmin, bbmax) ) {
	/* hit rpp */
	
	isect_ray_cell_top(isect, dsp_bb);
    }


    /* check for hits on the "foundation" pillar under the top.
     * The ray may have entered through the top of the pillar, possibly
     * after having come down through the triangles above
     */
    bbmax[Z] = dsp_bb->dspb_rpp.dsp_min[Z];
    bbmin[Z] = 0.0;
    if (dsp_in_rpp(isect, bbmin, bbmax) ) {
	/* hit rpp */
	struct hit in_hit, out_hit;

	VJOIN1(minpt, isect->r.r_pt, isect->r.r_min, isect->r.r_dir);
	VJOIN1(maxpt, isect->r.r_pt, isect->r.r_max, isect->r.r_dir);

	in_hit.hit_dist = isect->r.r_min;
	in_hit.hit_surfno = isect->dmin;
	VMOVE(in_hit.hit_point, minpt);

	out_hit.hit_dist = isect->r.r_max;
	out_hit.hit_surfno = isect->dmax;
	VMOVE(out_hit.hit_point, maxpt);

	/* add a segment to the list */
	add_seg(isect, &in_hit, &out_hit, bbmin, bbmax, 255, 255, 0);
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

	RT_DSP_CK_MAGIC(dsp);
	BU_CK_VLS(&dsp->dsp_i.dsp_name);

	switch (dsp->dsp_i.dsp_datasrc) {
	case RT_DSP_SRC_V4_FILE: 
	case RT_DSP_SRC_FILE:
	    BU_CK_MAPPED_FILE(dsp->dsp_i.dsp_mp);
	    break;
	case RT_DSP_SRC_OBJ:
	    break;
	}

	/* if we run into an error, we can jump here and re-start the
	 * ray with debugging turned on.
	 */
	if (setjmp(isect.env)) {
		if (RT_G_DEBUG & DEBUG_HF)
			bu_bomb("");

		rt_g.debug |= DEBUG_HF; 
	}

	/* 
	 * map ray into the coordinate system of the dsp 
	 */
	MAT4X3PNT(isect.r.r_pt, dsp->dsp_i.dsp_mtos, rp->r_pt);
	MAT4X3VEC(dir, dsp->dsp_i.dsp_mtos, rp->r_dir);
	VMOVE(isect.r.r_dir, dir);
	VUNITIZE(isect.r.r_dir);

	/* wrap a bunch of things together */
	isect.ap = ap;
	isect.stp = stp;
	isect.dsp = (struct dsp_specific *)stp->st_specific;
	isect.tol = &ap->a_rt_i->rti_tol;
	isect.sp_is_valid = 0;
	isect.sp_is_done = 0;
	VINVDIR(isect.inv_dir, isect.r.r_dir);
	BU_LIST_INIT(&isect.seglist);
	isect.sp = (struct seg *)NULL;


	if (rt_g.debug & DEBUG_HF) {
	    bu_log("rt_dsp_shot(pt:(%g %g %g)\n\tdir[%g]:(%g %g %g))\n    pixel(%d,%d)\n",
			V3ARGS(rp->r_pt),
			MAGNITUDE(rp->r_dir),
			V3ARGS(rp->r_dir),
			ap->a_x, ap->a_y);

	    bn_mat_print("mtos", dsp->dsp_i.dsp_mtos);
	    bu_log("Solid space ray pt:(%g %g %g)\n", V3ARGS(isect.r.r_pt));
	    bu_log("\tdir[%g]: [%g %g %g]\n\tunit_dir(%g %g %g)\n",
			MAGNITUDE(dir),
			V3ARGS(dir),
			V3ARGS(isect.r.r_dir));
	}

	/* We look at the topmost layer of the bounding-box tree and
	 * make sure that it has dimension 1.  Otherwise, something is wrong
	 */
	if (isect.dsp->layer[isect.dsp->layers-1].dim[X] != 1 ||
	    isect.dsp->layer[isect.dsp->layers-1].dim[Y] != 1) {
	    bu_log("%s:%d how do i find the topmost layer?\n",
		   __FILE__, __LINE__);
	    bu_bomb("");
	}


	/* intersect the ray with the bounding rpps */
	isect_ray_dsp_bb(&isect, isect.dsp->layer[isect.dsp->layers-1].p);


	/* if we missed it all, give up now */
	if (BU_LIST_IS_EMPTY(&isect.seglist))
		return 0;

	/* map hit distances back to model space */
	i = 0;
	for (BU_LIST_FOR(segp, seg, &isect.seglist)) {
		i += 2;
		if (rt_g.debug & DEBUG_HF) {
			bu_log("solid in:%6g out:%6g\t",
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

		if (RT_G_DEBUG & DEBUG_HF) {
			bu_log("model in:%6g out:%6g\n",
			segp->seg_in.hit_dist,
			segp->seg_out.hit_dist);
		}
	}

	if (RT_G_DEBUG & DEBUG_HF) {
		double NdotD;
		double d;
		static const plane_t plane = {0.0, 0.0, -1.0, 0.0};

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
	if (RT_G_DEBUG & DEBUG_HF)
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
    vect_t N, t, tmp, A, B, C, D;
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

    switch (dsp->dsp_i.dsp_datasrc) {
    case RT_DSP_SRC_V4_FILE: 
    case RT_DSP_SRC_FILE:
	BU_CK_MAPPED_FILE(dsp->dsp_i.dsp_mp);
	break;
    case RT_DSP_SRC_OBJ:
	break;
    }

    if (RT_G_DEBUG & DEBUG_HF) {
	bu_log("rt_dsp_norm(%g %g %g)\n", V3ARGS(hitp->hit_normal));
	VJOIN1( hitp->hit_point, rp->r_pt, hitp->hit_dist, rp->r_dir );
	VPRINT("\thit point", hitp->hit_point);
    }

    if ( hitp->hit_surfno < XMIN || hitp->hit_surfno > ZTOP ) {
	bu_log("%s:%d bogus surface of DSP %d\n",
	       __FILE__, __LINE__, hitp->hit_surfno);
	bu_bomb("");
    }

    if ( hitp->hit_surfno < ZTOP || !dsp->dsp_i.dsp_smooth ) {
	/* We've hit one of the sides or bottom, or the user didn't
	 * ask for smoothing of the elevation data,
	 * so there's no interpolation to do
	 */

	if (RT_G_DEBUG & DEBUG_HF)
	    bu_log("\tno Interpolation needed.  Normal: %g,%g,%g\n", 
		   V3ARGS(hitp->hit_normal));
	return;
    }

    if (RT_G_DEBUG & DEBUG_HF)
	bu_log("\tNormal Interpolation flag: %d\n", dsp->dsp_i.dsp_smooth);


    /* compute the distance between grid points in model space */
    VSET(tmp, 1.0, 0.0, 0.0);
    MAT4X3VEC(t, dsp->dsp_i.dsp_stom, tmp);
    len = MAGNITUDE(t);

    if (RT_G_DEBUG & DEBUG_HF) {
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

    if (RT_G_DEBUG & DEBUG_HF) {
	
	/* plot the ray */
	pl_color(fd, 255, 0, 0);
	pdv_3line(fd, rp->r_pt, hitp->hit_point);

	/* plot the normal we started with */
	pl_color(fd, 0, 255, 0);
	VJOIN1(tmp, hitp->hit_point, len, hitp->hit_normal);
	pdv_3line(fd, hitp->hit_point, tmp);


	/* Plot the normals we just got */
	pl_color(fd, 220, 220, 90);

	VSET(tmp, x,   y,   DSP(dsp, x,   y));
	MAT4X3PNT(A, dsp->dsp_i.dsp_stom, tmp);
	VJOIN1(tmp, A, len, Anorm);
	pdv_3line(fd, A, tmp);

	VSET(tmp, x+1, y,   DSP(dsp, x+1, y));
	MAT4X3PNT(B, dsp->dsp_i.dsp_stom, tmp);
	VJOIN1(tmp, B, len, Bnorm);
	pdv_3line(fd, B, tmp);

	VSET(tmp, x+1, y+1, DSP(dsp, x+1, y+1));
	MAT4X3PNT(D, dsp->dsp_i.dsp_stom, tmp);
	VJOIN1(tmp, D, len, Dnorm);
	pdv_3line(fd, D, tmp);

	VSET(tmp, x,   y+1, DSP(dsp, x,   y+1));
	MAT4X3PNT(C, dsp->dsp_i.dsp_stom, tmp);
	VJOIN1(tmp, C, len, Cnorm);
	pdv_3line(fd, C, tmp);

	bu_semaphore_release( BU_SEM_SYSCALL);
    }

    /* transform the hit point into DSP space for determining interpolation */
    MAT4X3PNT(pt, dsp->dsp_i.dsp_mtos, hitp->hit_point);
    
    Xfrac = (pt[X] - x);
    Yfrac = (pt[Y] - y);
    if (RT_G_DEBUG & DEBUG_HF)
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
    if (RT_G_DEBUG & DEBUG_HF)
	bu_log("interpolated %g %g %g  dot:%g\n", V3ARGS(N), dot);
    
    if ( (hitp->hit_vpriv[Z] == 0.0 && dot > 0.0)/* in-hit needs fix */ ||
	 (hitp->hit_vpriv[Z] == 1.0 && dot < 0.0)/* out-hit needs fix */){
	/* bring the normal back to being perpindicular 
	 * to the ray to avoid "flipped normal" warnings
	 */
	VCROSS(A, rp->r_dir, N);
	VCROSS(N, A, rp->r_dir);
	VUNITIZE(N);

	dot = VDOT(N, rp->r_dir);


	if (RT_G_DEBUG & DEBUG_HF)
	    bu_log("corrected: %g %g %g dot:%g\n", V3ARGS(N), dot);
    }
    VMOVE(hitp->hit_normal, N);

    if (RT_G_DEBUG & DEBUG_HF) {
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

	if (RT_G_DEBUG & DEBUG_HF)
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

	if (RT_G_DEBUG & DEBUG_HF)
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

	if (RT_G_DEBUG & DEBUG_HF)
		bu_log("rt_dsp_free()\n");

	switch (dsp->dsp_i.dsp_datasrc) {
	case RT_DSP_SRC_V4_FILE: 
	case RT_DSP_SRC_FILE:
	    BU_CK_MAPPED_FILE(dsp->dsp_i.dsp_mp);
	    bu_close_mapped_file(dsp->dsp_i.dsp_mp);
	    break;
	case RT_DSP_SRC_OBJ:
	    break;
	}


	bu_free( (char *)dsp, "dsp_specific" );
}

/*
 *			R T _ D S P _ C L A S S
 */
int
rt_dsp_class()
{
	if (RT_G_DEBUG & DEBUG_HF)
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
const struct rt_tess_tol *ttol;
const struct bn_tol	*tol;
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

	if (RT_G_DEBUG & DEBUG_HF)
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
const struct rt_tess_tol *ttol;
const struct bn_tol	*tol;
{
	LOCAL struct rt_dsp_internal	*dsp_ip;

	if (RT_G_DEBUG & DEBUG_HF)
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

	if (RT_G_DEBUG & DEBUG_HF)
		bu_log("db_internal magic: 0x%08x  major: %d minor:%d\n",
		       dsp_ip->dsp_bip->idb_magic,
		       dsp_ip->dsp_bip->idb_major_type,
		       dsp_ip->dsp_bip->idb_minor_type);

	bip = dsp_ip->dsp_bip->idb_ptr;

	if (RT_G_DEBUG & DEBUG_HF)
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
	MAT_COPY(tmp, dsp_ip->dsp_stom);
	bn_mat_mul(dsp_ip->dsp_stom, mat, tmp);
	
	bn_mat_inv(dsp_ip->dsp_mtos, dsp_ip->dsp_stom);

	switch (dsp_ip->dsp_datasrc) {
	case RT_DSP_SRC_FILE:
	case RT_DSP_SRC_V4_FILE:
		if (RT_G_DEBUG & DEBUG_HF)
			bu_log("getting data from file\n");
		return get_file_data(dsp_ip, ip, ep, mat, dbip);
		break;
	case RT_DSP_SRC_OBJ:
		if (RT_G_DEBUG & DEBUG_HF)
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
const struct bu_external	*ep;
register const mat_t		mat;
const struct db_i		*dbip;
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

	if (RT_G_DEBUG & DEBUG_HF)
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

	if (RT_G_DEBUG & DEBUG_HF) {
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
const struct rt_db_internal	*ip;
double				local2mm;
const struct db_i		*dbip;
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
	if (RT_G_DEBUG & DEBUG_HF)	
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
const struct bu_external	*ep;
register const mat_t		mat;
const struct db_i		*dbip;
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
const struct rt_db_internal	*ip;
double				local2mm;
const struct db_i		*dbip;
{
	struct rt_dsp_internal	*dsp_ip;
	unsigned long		name_len;
	unsigned char		*cp;

	RT_CK_DB_INTERNAL(ip);
	if (ip->idb_type != ID_DSP )  return(-1);
	dsp_ip = (struct rt_dsp_internal *)ip->idb_ptr;
	RT_DSP_CK_MAGIC(dsp_ip);

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
const struct rt_db_internal *ip;
int			verbose;
double			mm2local;
{
	register struct rt_dsp_internal	*dsp_ip =
		(struct rt_dsp_internal *)ip->idb_ptr;
	struct bu_vls vls;


	if (RT_G_DEBUG & DEBUG_HF)
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

	if (RT_G_DEBUG & DEBUG_HF)
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
