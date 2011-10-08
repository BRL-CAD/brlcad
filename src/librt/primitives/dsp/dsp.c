/*                           D S P . C
 * BRL-CAD
 *
 * Copyright (c) 1999-2011 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @addtogroup primitives */
/** @{ */
/** @file primitives/dsp/dsp.c
 *
 * Intersect a ray with a displacement map.
 *
 * The bounding box planes (in dsp coordinates) are numbered 0 .. 5
 *
 * For purposes of the "struct hit" surface number, the
 * "non-elevation" surfaces are numbered 0 .. 7 where:
 *
 *     Plane #	Name  plane dist
 * --------------------------------------------------
 *	0	XMIN (dist = 0)
 *	1	XMAX (dist = xsiz)
 *	2	YMIN (dist = 0)
 *	3	YMAX (dist = ysiz)
 *	4	ZMIN (dist = 0)
 *	5	ZMAX (dsp_max)
 *
 *	6	ZMID (dsp_min)
 *	7	ZTOP (computed)
 *
 * if the "struct hit" surfno surface is ZMAX, then hit_vpriv[X, Y]
 * holds the cell that was hit.  hit_vpriv[Z] is 0 if this was an
 * in-hit.  1 if an out-hit.
 *
 */

#include "common.h"

#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <setjmp.h>
#include "bin.h"

#include "vmath.h"
#include "raytrace.h"
#include "rtgeom.h"
#include "db.h"
#include "plot3.h"

/* private header */
#include "./dsp.h"


#define FULL_DSP_DEBUGGING 1

#define ORDERED_ISECT 1

#define DIM_BB_CHILDREN 4
#define NUM_BB_CHILDREN (DIM_BB_CHILDREN*DIM_BB_CHILDREN)

struct dsp_rpp {
    unsigned short dsp_min[3];
    unsigned short dsp_max[3];
};


/**
 * This structure contains a bounding box for a portion of the DSP
 * along with information about sub-bounding boxes, and what layer
 * (resolution) of the DSP this box bounds
 */
struct dsp_bb {
    uint32_t magic;
    struct dsp_rpp dspb_rpp;	/* our bounding box */
    /*
     * the next two elements indicate the number and locations of
     * sub-bounding rpps.
     *
     * dsp_b_ch_dim is typically DIM_BB_CHILDREN, DIM_BB_CHILDREN
     * except for "border" areas of the array
     */
    unsigned short dspb_subcell_size;/* XXX This is not yet computed */
    unsigned short dspb_ch_dim[2];	/* dimensions of children[] */
    struct dsp_bb *dspb_children[NUM_BB_CHILDREN];
};


#define MAGIC_dsp_bb 234
#define DSP_BB_CK(_p) BU_CKMAG(_p, MAGIC_dsp_bb, "dsp_bb")
/*
 * This structure provides a handle to all of the bounding boxes for
 * the DSP at a particular resolution.
 */
#define LAYER(l, x, y) l->p[l->dim[1]*y+x]
struct dsp_bb_layer {
    unsigned int dim[2]; /* the dimensions of the array at element p */
    struct dsp_bb *p; /* array of dsp_bb's for this level */
};

# define XCNT(_p) (((struct rt_dsp_internal *)_p)->dsp_xcnt)
# define YCNT(_p) (((struct rt_dsp_internal *)_p)->dsp_ycnt)
# define XSIZ(_p) (_p->dsp_i.dsp_xcnt - 1)
# define YSIZ(_p) (_p->dsp_i.dsp_ycnt - 1)



/* FIXME: rename? */
extern int rt_retrieve_binunif(struct rt_db_internal *intern,
			       const struct db_i *dbip,
			       const char *name);


#define dlog if (RT_G_DEBUG & DEBUG_HF) bu_log


#define BBOX_PLANES 7 /* 2 tops & 5 other sides */
#define XMIN 0
#define XMAX 1
#define YMIN 2
#define YMAX 3
#define ZMIN 4
#define ZMAX 5
#define ZMID 6
#define ZTOP 7


/**
 * per-solid ray tracing form of solid, including precomputed terms
 *
 * The dsp_i element MUST BE FIRST so that we can cast a pointer to a
 * dsp_specific to a rt_dsp_intermal.
 */
struct dsp_specific {
    struct rt_dsp_internal dsp_i;	/* MUST BE FIRST */
    double dsp_pl_dist[BBOX_PLANES];
    int xsiz;
    int ysiz;
    int layers;
    struct dsp_bb_layer *layer;
    struct dsp_bb *bb_array;
};


struct bbox_isect {
    double in_dist;
    double out_dist;
    int in_surf;
    int out_surf;
};


/**
 * per-ray ray tracing information
 */
struct isect_stuff {
    struct dsp_specific *dsp;
    struct bu_list seglist;	/**< list of segments */
    struct xray r;		/**< solid space ray */
    vect_t inv_dir;		/**< inverses of ray direction */
    struct application *ap;
    struct soltab *stp;
    struct bn_tol *tol;

    struct bbox_isect bbox;
    struct bbox_isect minbox;

    int num_segs;
    int dmin, dmax;	/* for dsp_in_rpp, {X, Y, Z}MIN/MAX */
};


/**
 * plane equations (minus offset distances) for bounding RPP
 */
static const vect_t dsp_pl[BBOX_PLANES] = {
    {-1.0, 0.0, 0.0},
    { 1.0, 0.0, 0.0},

    {0.0, -1.0, 0.0},
    {0.0,  1.0, 0.0},

    {0.0, 0.0, -1.0},
    {0.0, 0.0,  1.0},
    {0.0, 0.0,  1.0},
};


/**
 * This function computes the DSP mtos matrix from the stom matrix
 * whenever the stom matrix is parsed using a bu_structparse.
 */
HIDDEN void
hook_mtos_from_stom(
    const struct bu_structparse *sp,
    const char *sp_name,
    genptr_t base,
    char *UNUSED(p))
{
    struct rt_dsp_internal *dsp_ip = (struct rt_dsp_internal *)base;

    if (!sp) return;
    if (!sp_name) return;
    if (!base) return;

    bn_mat_inv(dsp_ip->dsp_mtos, dsp_ip->dsp_stom);
}


HIDDEN void
hook_file(
    const struct bu_structparse *sp,
    const char *sp_name,
    genptr_t base,
    char *UNUSED(p))
{
    struct rt_dsp_internal *dsp_ip = (struct rt_dsp_internal *)base;

    if (!sp) return;
    if (!sp_name) return;
    if (!base) return;

    dsp_ip->dsp_datasrc = RT_DSP_SRC_V4_FILE;
    dsp_ip->dsp_bip = (struct rt_db_internal *)NULL;
}


#define DSP_O(m) bu_offsetof(struct rt_dsp_internal, m)
#define DSP_AO(a) bu_offsetofarray(struct rt_dsp_internal, a)

/** only used when editing a v4 database */
const struct bu_structparse rt_dsp_parse[] = {
    {"%V",	1, "file", DSP_O(dsp_name), hook_file, NULL, NULL },
    {"%i",	1, "sm", DSP_O(dsp_smooth), BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    {"%d",	1, "w", DSP_O(dsp_xcnt), BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    {"%d",	1, "n", DSP_O(dsp_ycnt), BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    {"%f",     16, "stom", DSP_AO(dsp_stom), hook_mtos_from_stom, NULL, NULL },
    {"",	0, (char *)0, 0,	BU_STRUCTPARSE_FUNC_NULL, NULL, NULL }
};


/** only used when editing a v4 database */
const struct bu_structparse rt_dsp_ptab[] = {
    {"%V",	1, "file", DSP_O(dsp_name), BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    {"%i",	1, "sm", DSP_O(dsp_smooth), BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    {"%d",	1, "w", DSP_O(dsp_xcnt), BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    {"%d",	1, "n", DSP_O(dsp_ycnt), BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    {"%f",     16, "stom", DSP_AO(dsp_stom), BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    {"",	0, (char *)0, 0,	BU_STRUCTPARSE_FUNC_NULL, NULL, NULL }
};


/*
 * This one is used by the rt_dsp_tclget()
 */


static int plot_file_num=0;


/**
 * P L O T _ R P P
 *
 * Plot an RPP to a file in the given color
 */
HIDDEN void
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


/**
 * P L O T _ D S P _ B B
 *
 * Plot a dsp_bb structure
 */
HIDDEN void
plot_dsp_bb(FILE *fp, struct dsp_bb *dsp_bb,
	    struct dsp_specific *dsp,
	    int r, int g, int b, int blather)
{
    fastf_t *stom = &dsp->dsp_i.dsp_stom[0];
    struct bound_rpp rpp;
    point_t pt;

    DSP_BB_CK(dsp_bb);

    VMOVE(pt, dsp_bb->dspb_rpp.dsp_min); /* int->float conversion */
    MAT4X3PNT(rpp.min, stom, pt);

    VMOVE(pt, dsp_bb->dspb_rpp.dsp_max); /* int->float conversion */
    MAT4X3PNT(rpp.max, stom, pt);

    if (blather)
	bu_log(" (%g %g %g) (%g %g %g)\n",
	       V3ARGS(rpp.min), V3ARGS(rpp.max));

    plot_rpp(fp, &rpp, r, g, b);
}


/*
 * drawing support for isect_ray_dsp_bb()
 */
HIDDEN FILE *
draw_dsp_bb(int *plotnum,
	    struct dsp_bb *dsp_bb,
	    struct dsp_specific *dsp,
	    int r, int g, int b)
{
    char buf[64];
    FILE *fp;
    struct dsp_bb bb;

    sprintf(buf, "dsp_bb%03d.pl", (*plotnum)++);
    if ((fp=fopen(buf, "wb")) == (FILE *)NULL) {
	perror(buf);
	bu_bomb("");
    }

    bu_log("plotting %s", buf);
    bb = *dsp_bb; /* struct copy */
    bb.dspb_rpp.dsp_min[Z] = 0.0;
    plot_dsp_bb(fp, &bb, dsp, r, g, b, 1);

    return fp;
}


#define PLOT_LAYERS
#ifdef PLOT_LAYERS


/**
 * P L O T _ L A Y E R S
 *
 * Plot the bounding box layers for a dsp
 */
HIDDEN void
plot_layers(struct dsp_specific *dsp_sp)
{
    FILE *fp;
    int l, n;
    unsigned int x, y;
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

    for (l=0; l < dsp_sp->layers; l++) {
	bu_semaphore_acquire(BU_SEM_SYSCALL);
	sprintf(buf, "Dsp_layer%d.pl", l);
	fp=fopen(buf, "wb");
	bu_semaphore_release(BU_SEM_SYSCALL);
	if (fp == (FILE *)NULL) {
	    bu_log("%s:%d error opening %s\n", __FILE__, __LINE__,
		   buf);
	    return;
	} else
	    bu_log("plotting \"%s\" dim:%d, %d\n", buf,
		   dsp_sp->layer[l].dim[X],
		   dsp_sp->layer[l].dim[Y]);
	c = l % 6;
	r = colors[c][0];
	g = colors[c][1];
	b = colors[c][2];

	for (y=0; y < dsp_sp->layer[l].dim[Y]; y+= 2) {
	    for (x=0; x < dsp_sp->layer[l].dim[X]; x+= 2) {
		n = y * dsp_sp->layer[l].dim[X] + x;
		d_bb = &dsp_sp->layer[l].p[n];
		plot_dsp_bb(fp, d_bb, dsp_sp, r, g, b, 0);

	    }
	}
	fclose(fp);
    }
}


#endif


/**
 * P L O T _ C E L L _ T O P
 *
 * Plot the results of intersecting a ray with the top of a cell
 */
HIDDEN void
plot_cell_top(struct isect_stuff *isect,
	      struct dsp_bb *dsp_bb,
	      point_t A,
	      point_t B,
	      point_t C,
	      point_t D,
	      struct hit hitlist[],
	      int hitflags,
	      int style)	/* plot diagonal */
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

    DSP_BB_CK(dsp_bb);

    bu_semaphore_acquire(BU_SEM_SYSCALL);
    if (style)
	sprintf(buf, "dsp_cell_isect%04d.pl", cnt++);
    else
	sprintf(buf, "dsp_cell_top%04d.pl", plotcnt++);

    fp=fopen(buf, "wb");

    bu_semaphore_release(BU_SEM_SYSCALL);

    if (fp == (FILE *)NULL) {
	bu_log("error opening \"%s\"\n", buf);
	return;
    } else {
	bu_log("plotting %s flags 0x%x\n\t", buf, hitflags);
    }

    plot_dsp_bb(fp, dsp_bb, isect->dsp, 128, 128, 128, 1);

    /* plot the triangulation */
    pl_color(fp, 255, 255, 255);
    MAT4X3PNT(p1, stom, A);
    MAT4X3PNT(p2, stom, B);
    MAT4X3PNT(p3, stom, C);
    MAT4X3PNT(p4, stom, D);

    pdv_3move(fp, p1);
    if (style) {
	pdv_3cont(fp, p2);
	pdv_3cont(fp, p4);
	pdv_3cont(fp, p1);
	pdv_3cont(fp, p3);
	pdv_3cont(fp, p4);
    } else {
	pdv_3cont(fp, p2);
	pdv_3cont(fp, p4);
	pdv_3cont(fp, p3);
	pdv_3cont(fp, p1);
    }

    /* plot the hit points */

    for (in_seg = 0, i = 0; i < 4; i++) {
	if (hitflags & (1<<i)) {
	    if (in_seg) {
		in_seg = 0;
		MAT4X3PNT(p1, stom, hitlist[i].hit_point);
		pdv_3cont(fp, p1);
	    } else {
		in_seg = 1;
		pl_color(fp, colors[i][0], colors[i][1], colors[i][2]);
		MAT4X3PNT(p1, stom, hitlist[i].hit_point);
		pdv_3move(fp, p1);
	    }
	}
    }
    fclose(fp);
}


/**
 * D S P _ P R I N T _ V 4
 */
HIDDEN void
dsp_print_v4(struct bu_vls *vls, const struct rt_dsp_internal *dsp_ip)
{
    point_t pt, v;
    RT_DSP_CK_MAGIC(dsp_ip);
    BU_CK_VLS(&dsp_ip->dsp_name);
    BU_CK_VLS(vls);

    bu_vls_printf(vls, "Displacement Map\n  file='%s' w=%zu n=%zu sm=%d",
		  bu_vls_addr(&dsp_ip->dsp_name),
		  dsp_ip->dsp_xcnt,
		  dsp_ip->dsp_ycnt,
		  dsp_ip->dsp_smooth);

    VSETALL(pt, 0.0);

    MAT4X3PNT(v, dsp_ip->dsp_stom, pt);

    bu_vls_printf(vls, " (origin at %g %g %g)mm\n", V3INTCLAMPARGS(v));

    bu_vls_printf(vls, "  stom=\n");
    bu_vls_printf(vls, "  %8.3f %8.3f %8.3f %8.3f\n",
		  V4INTCLAMPARGS(dsp_ip->dsp_stom));

    bu_vls_printf(vls, "  %8.3f %8.3f %8.3f %8.3f\n",
		  V4INTCLAMPARGS(&dsp_ip->dsp_stom[4]));

    bu_vls_printf(vls, "  %8.3f %8.3f %8.3f %8.3f\n",
		  V4INTCLAMPARGS(&dsp_ip->dsp_stom[8]));

    bu_vls_printf(vls, "  %8.3f %8.3f %8.3f %8.3f\n",
		  V4INTCLAMPARGS(&dsp_ip->dsp_stom[12]));
}


/**
 * D S P _ P R I N T _ V 5
 */
HIDDEN void
dsp_print_v5(struct bu_vls *vls,
	     const struct rt_dsp_internal *dsp_ip)
{
    point_t pt, v;

    BU_CK_VLS(vls);

    RT_DSP_CK_MAGIC(dsp_ip);
    BU_CK_VLS(&dsp_ip->dsp_name);


    bu_vls_printf(vls, "Displacement Map\n");

    switch (dsp_ip->dsp_datasrc) {
	case RT_DSP_SRC_V4_FILE:
	    bu_vls_printf(vls, "  Error Error Error");
	    break;
	case RT_DSP_SRC_FILE:
	    bu_vls_printf(vls, "  file");
	    break;
	case RT_DSP_SRC_OBJ:
	    bu_vls_printf(vls, "  obj");
	    break;
	default:
	    bu_vls_printf(vls, "unk src type'%c'", dsp_ip->dsp_datasrc);
	    break;
    }

    bu_vls_printf(vls, "='%s'\n  w=%zu n=%zu sm=%d ",
		  bu_vls_addr(&dsp_ip->dsp_name),
		  dsp_ip->dsp_xcnt,
		  dsp_ip->dsp_ycnt,
		  dsp_ip->dsp_smooth);

    switch (dsp_ip->dsp_cuttype) {
	case DSP_CUT_DIR_ADAPT:
	    bu_vls_printf(vls, "cut=ad"); break;
	case DSP_CUT_DIR_llUR:
	    bu_vls_printf(vls, "cut=lR"); break;
	case DSP_CUT_DIR_ULlr:
	    bu_vls_printf(vls, "cut=Lr"); break;
	default:
	    bu_vls_printf(vls, "cut bogus('%c'/%d)",
			  dsp_ip->dsp_cuttype,
			  dsp_ip->dsp_cuttype); break;
    }


    VSETALL(pt, 0.0);

    MAT4X3PNT(v, dsp_ip->dsp_stom, pt);

    bu_vls_printf(vls, " (origin at %g %g %g)mm\n", V3INTCLAMPARGS(v));

    bu_vls_printf(vls, "  stom=\n");
    bu_vls_printf(vls, "  %8.3f %8.3f %8.3f %8.3f\n", V4INTCLAMPARGS(dsp_ip->dsp_stom));
    bu_vls_printf(vls, "  %8.3f %8.3f %8.3f %8.3f\n", V4INTCLAMPARGS(&dsp_ip->dsp_stom[4]));
    bu_vls_printf(vls, "  %8.3f %8.3f %8.3f %8.3f\n", V4INTCLAMPARGS(&dsp_ip->dsp_stom[8]));
    bu_vls_printf(vls, "  %8.3f %8.3f %8.3f %8.3f\n", V4INTCLAMPARGS(&dsp_ip->dsp_stom[12]));
}


/**
 * R T _ D S P _ P R I N T
 */
void
rt_dsp_print(register const struct soltab *stp)
{
    register const struct dsp_specific *dsp =
	(struct dsp_specific *)stp->st_specific;
    struct bu_vls vls;


    RT_DSP_CK_MAGIC(dsp);
    BU_CK_VLS(&dsp->dsp_i.dsp_name);

    bu_vls_init(&vls);
    BU_CK_VLS(&vls);

    bu_vls_printf(&vls, "\n---------db version: %d----------\n",
		  db_version(stp->st_rtip->rti_dbip));

    switch (db_version(stp->st_rtip->rti_dbip)) {
	case 4:
	    BU_CK_VLS(&vls);
	    dsp_print_v4(&vls, &(dsp->dsp_i));
	    break;
	case 5:
	    BU_CK_VLS(&vls);
	    dsp_print_v5(&vls, &(dsp->dsp_i));
	    break;
    }

    bu_log("%s", bu_vls_addr(&vls));

    if (BU_VLS_IS_INITIALIZED(&vls)) bu_vls_free(&vls);

}


/**
 * compute bounding boxes for each cell, then compute bounding boxes
 * for collections of bounding boxes
 */
HIDDEN void
dsp_layers(struct dsp_specific *dsp, unsigned short *d_min, unsigned short *d_max)
{
    int idx, curr_layer, xs, ys, xv, yv, tot;
    unsigned int x, y, i, j, k;
    unsigned short dsp_min, dsp_max, cell_min, cell_max;
    unsigned short elev;
    struct dsp_bb *dsp_bb;
    struct dsp_rpp *t;
    struct dsp_bb_layer *curr, *prev;
    unsigned short subcell_size;

    /* First we compute the total number of struct dsp_bb's we will need */
    xs = dsp->xsiz;
    ys = dsp->ysiz;
    tot = xs * ys;
    /*    bu_log("layer %d   %dx%d\n", 0, xs, ys); */
    dsp->layers = 1;
    while (xs > 1 || ys > 1) {
	xv = xs / DIM_BB_CHILDREN;
	yv = ys / DIM_BB_CHILDREN;
	if (xs % DIM_BB_CHILDREN) xv++;
	if (ys % DIM_BB_CHILDREN) yv++;

#ifdef FULL_DSP_DEBUGGING
	if (RT_G_DEBUG & DEBUG_HF)
	    bu_log("layer %d   %dx%d\n", dsp->layers, xv, yv);
#endif
	tot += xv * yv;

	if (xv > 0) xs = xv;
	else xs = 1;

	if (yv > 0) ys = yv;
	else ys = 1;
	dsp->layers++;
    }


#ifdef FULL_DSP_DEBUGGING
    if (RT_G_DEBUG & DEBUG_HF)
	bu_log("%d layers total\n", dsp->layers);
#endif

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

    for (y=0; y < YSIZ(dsp); y++) {

	cell_min = 0xffff;
	cell_max = 0;

	for (x=0; x < XSIZ(dsp); x++) {

	    elev = DSP(&dsp->dsp_i, x, y);
	    cell_min = cell_max = elev;

	    elev = DSP(&dsp->dsp_i, x+1, y);
	    V_MIN(cell_min, elev);
	    V_MAX(cell_max, elev);

	    elev = DSP(&dsp->dsp_i, x, y+1);
	    V_MIN(cell_min, elev);
	    V_MAX(cell_max, elev);

	    elev = DSP(&dsp->dsp_i, x+1, y+1);
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

	    dsp_bb->dspb_subcell_size = 0;

	    /* There are no "children" of a layer 0 element */
	    dsp_bb->dspb_ch_dim[X] = 0;
	    dsp_bb->dspb_ch_dim[Y] = 0;
	    for (k=0; k < NUM_BB_CHILDREN; k++) {
		dsp_bb->dspb_children[k] =
		    (struct dsp_bb *)NULL;
	    }
	    dsp_bb->magic = MAGIC_dsp_bb;

	    /* XXX should we compute the triangle orientation and
	     * save it here too?
	     */
	}
    }

    *d_min = dsp_min;
    *d_max = dsp_max;


    if (RT_G_DEBUG & DEBUG_HF)
	bu_log("layer 0 filled\n");

    subcell_size = 1;

    /* now we compute successive layers from the initial layer */
    for (curr_layer = 1; curr_layer < dsp->layers; curr_layer++) {
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
	dsp->layer[curr_layer].p =
	    &dsp->layer[curr_layer-1].p[dsp->layer[curr_layer-1].dim[X] * dsp->layer[curr_layer-1].dim[Y] ];

	curr = &dsp->layer[curr_layer];
	prev = &dsp->layer[curr_layer-1];

	if (RT_G_DEBUG & DEBUG_HF)
	    bu_log("layer %d  subcell size %d\n", curr_layer, subcell_size);

	/* walk the grid and fill in the values for this layer */
	for (y=0; y < curr->dim[Y]; y++) {
	    for (x=0; x < curr->dim[X]; x++) {
		int n, xp, yp;
		/* x, y are in the coordinates in the current
		 * layer.  xp, yp are the coordinates of the
		 * same area in the previous (lower) layer.
		 */
		xp = x * DIM_BB_CHILDREN;
		yp = y * DIM_BB_CHILDREN;

		/* initialize the current dsp_bb cell */
		dsp_bb = &curr->p[y*curr->dim[X]+x];
		dsp_bb->magic = MAGIC_dsp_bb;
		n = (int)pow((double)DIM_BB_CHILDREN, (double)curr_layer);
		VSET(dsp_bb->dspb_rpp.dsp_min,
		     x * n, y * n, 0x0ffff);
		VSET(dsp_bb->dspb_rpp.dsp_max,
		     x * n, y * n, 0);

		/* record the dimensions of our children */
		dsp_bb->dspb_subcell_size = subcell_size;


		tot = 0;
		i=0;
		for (j=0; j<DIM_BB_CHILDREN && (yp+j)<prev->dim[Y]; j++) {
		    for (i=0; i<DIM_BB_CHILDREN && (xp+i)<prev->dim[X]; i++) {

			idx = (yp+j) * prev->dim[X] + xp+i;

			t = &prev->p[ idx ].dspb_rpp;

			VMINMAX(dsp_bb->dspb_rpp.dsp_min,
				dsp_bb->dspb_rpp.dsp_max, t->dsp_min);
			VMINMAX(dsp_bb->dspb_rpp.dsp_min,
				dsp_bb->dspb_rpp.dsp_max, t->dsp_max);

			dsp_bb->dspb_children[tot++] = &prev->p[ idx ];

		    }
		}

		dsp_bb->dspb_ch_dim[X] = i;
		dsp_bb->dspb_ch_dim[Y] = j;
	    }
	}
	subcell_size *= DIM_BB_CHILDREN;
    }

#ifdef PLOT_LAYERS
    if (RT_G_DEBUG & DEBUG_HF) {
	plot_layers(dsp);
	bu_log("_  x:%zu y:%zu min %d max %d\n",
	       XCNT(dsp), YCNT(dsp), dsp_min, dsp_max);
    }
#endif
}

/**
 * R T _ D S P _ B B O X
 *
 * Calculate the bounding box for a dsp.
 */
int
rt_dsp_bbox(struct rt_db_internal *ip, point_t *min, point_t *max) {
    struct rt_dsp_internal *dsp_ip;
    register struct dsp_specific *dsp;
    unsigned short dsp_min, dsp_max;
    point_t pt, bbpt;

    RT_CK_DB_INTERNAL(ip);
    dsp_ip = (struct rt_dsp_internal *)ip->idb_ptr;
    RT_DSP_CK_MAGIC(dsp_ip);
    BU_CK_VLS(&dsp_ip->dsp_name);

    switch (dsp_ip->dsp_datasrc) {
	case RT_DSP_SRC_V4_FILE:
	case RT_DSP_SRC_FILE:
	    if (!dsp_ip->dsp_mp) {
		bu_log("dsp(%s): no data file or data file empty\n", bu_vls_addr(&dsp_ip->dsp_name));
		return 1; /* BAD */
	    }
	    BU_CK_MAPPED_FILE(dsp_ip->dsp_mp);

	    /* we do this here and now because we will need it for the
	     * dsp_specific structure in a few lines
	     */
	    bu_semaphore_acquire(RT_SEM_MODEL);
	    ++dsp_ip->dsp_mp->uses;
	    bu_semaphore_release(RT_SEM_MODEL);
	    break;
	case RT_DSP_SRC_OBJ:
	    if (!dsp_ip->dsp_bip) {
		bu_log("dsp(%s): no data\n", bu_vls_addr(&dsp_ip->dsp_name));
		return 1; /* BAD */
	    }
	    RT_CK_DB_INTERNAL(dsp_ip->dsp_bip);
	    RT_CK_BINUNIF(dsp_ip->dsp_bip->idb_ptr);
	    break;
    }


    BU_GETSTRUCT(dsp, dsp_specific);

    /* this works ok, because the mapped file keeps track of the
     * number of uses.  However, the binunif interface does not.
     * We'll have to copy the data for that one.
     */
    dsp->dsp_i = *dsp_ip;		/* struct copy */

    /* this keeps the binary internal object from being freed */
    dsp_ip->dsp_bip = (struct rt_db_internal *)NULL;


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
	VMINMAX((*min), (*max), bbpt)

    BBOX_PT(-.1,		 -.1,		      -.1);
    BBOX_PT(dsp_ip->dsp_xcnt+.1, -.1,		      -.1);
    BBOX_PT(dsp_ip->dsp_xcnt+.1, dsp_ip->dsp_ycnt+.1, -1);
    BBOX_PT(-.1,		 dsp_ip->dsp_ycnt+.1, -1);
    BBOX_PT(-.1,		 -.1,		      dsp_max+.1);
    BBOX_PT(dsp_ip->dsp_xcnt+.1, -.1,		      dsp_max+.1);
    BBOX_PT(dsp_ip->dsp_xcnt+.1, dsp_ip->dsp_ycnt+.1, dsp_max+.1);
    BBOX_PT(-.1,		 dsp_ip->dsp_ycnt+.1, dsp_max+.1);

#undef BBOX_PT

    switch (dsp->dsp_i.dsp_datasrc) {
	case RT_DSP_SRC_V4_FILE:
	case RT_DSP_SRC_FILE:
	    if (dsp->dsp_i.dsp_mp) {
		BU_CK_MAPPED_FILE(dsp->dsp_i.dsp_mp);
		bu_close_mapped_file(dsp->dsp_i.dsp_mp);
	    } else if (dsp->dsp_i.dsp_buf) {
		bu_free(dsp->dsp_i.dsp_buf, "dsp fake data");
	    }
	    break;
	case RT_DSP_SRC_OBJ:
	    break;
    }

    bu_free((char *)dsp, "bbox dsp");

    return 0;
}

/**
 * R T _ D S P _ P R E P
 *
 * Given a pointer to a GED database record, and a transformation
 * matrix, determine if this is a valid DSP, and if so, precompute
 * various terms of the formula.
 *
 * Returns -
 * 0 DSP is OK
 * !0 Error in description
 *
 * Implicit return -
 * A struct dsp_specific is created, and its address is stored in
 * stp->st_specific for use by dsp_shot().
 *
 * Note:  because the stand-along bbox calculation requires much
 * of the prep logic, the in-prep bbox calculations are left
 * in to avoid dupliation rather than calling rt_dsp_bbox.
 */
int
rt_dsp_prep(struct soltab *stp, struct rt_db_internal *ip, struct rt_i *rtip)
{
    struct rt_dsp_internal *dsp_ip;
    register struct dsp_specific *dsp;
    unsigned short dsp_min, dsp_max;
    point_t pt, bbpt;
    vect_t work;
    fastf_t f;

    if (RT_G_DEBUG & DEBUG_HF)
	bu_log("rt_dsp_prep()\n");

    if (rtip) RT_CK_RTI(rtip);

    RT_CK_DB_INTERNAL(ip);
    dsp_ip = (struct rt_dsp_internal *)ip->idb_ptr;
    RT_DSP_CK_MAGIC(dsp_ip);
    BU_CK_VLS(&dsp_ip->dsp_name);

    switch (dsp_ip->dsp_datasrc) {
	case RT_DSP_SRC_V4_FILE:
	case RT_DSP_SRC_FILE:
	    if (!dsp_ip->dsp_mp) {
		bu_log("dsp(%s): no data file or data file empty\n", bu_vls_addr(&dsp_ip->dsp_name));
		return 1; /* BAD */
	    }
	    BU_CK_MAPPED_FILE(dsp_ip->dsp_mp);

	    /* we do this here and now because we will need it for the
	     * dsp_specific structure in a few lines
	     */
	    bu_semaphore_acquire(RT_SEM_MODEL);
	    ++dsp_ip->dsp_mp->uses;
	    bu_semaphore_release(RT_SEM_MODEL);
	    break;
	case RT_DSP_SRC_OBJ:
	    if (!dsp_ip->dsp_bip) {
		bu_log("dsp(%s): no data\n", bu_vls_addr(&dsp_ip->dsp_name));
		return 1; /* BAD */
	    }
	    RT_CK_DB_INTERNAL(dsp_ip->dsp_bip);
	    RT_CK_BINUNIF(dsp_ip->dsp_bip->idb_ptr);
	    break;
    }


    BU_GETSTRUCT(dsp, dsp_specific);
    stp->st_specific = (genptr_t) dsp;

    /* this works ok, because the mapped file keeps track of the
     * number of uses.  However, the binunif interface does not.
     * We'll have to copy the data for that one.
     */
    dsp->dsp_i = *dsp_ip;		/* struct copy */

    /* this keeps the binary internal object from being freed */
    dsp_ip->dsp_bip = (struct rt_db_internal *)NULL;


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
	VMINMAX(stp->st_min, stp->st_max, bbpt)

    BBOX_PT(-.1,		 -.1,		      -.1);
    BBOX_PT(dsp_ip->dsp_xcnt+.1, -.1,		      -.1);
    BBOX_PT(dsp_ip->dsp_xcnt+.1, dsp_ip->dsp_ycnt+.1, -1);
    BBOX_PT(-.1,		 dsp_ip->dsp_ycnt+.1, -1);
    BBOX_PT(-.1,		 -.1,		      dsp_max+.1);
    BBOX_PT(dsp_ip->dsp_xcnt+.1, -.1,		      dsp_max+.1);
    BBOX_PT(dsp_ip->dsp_xcnt+.1, dsp_ip->dsp_ycnt+.1, dsp_max+.1);
    BBOX_PT(-.1,		 dsp_ip->dsp_ycnt+.1, dsp_max+.1);

#undef BBOX_PT

    VADD2SCALE(stp->st_center, stp->st_min, stp->st_max, 0.5);
    VSUB2SCALE(work, stp->st_max, stp->st_min, 0.5);

    f = work[X];
    if (work[Y] > f) f = work[Y];
    if (work[Z] > f) f = work[Z];
    stp->st_aradius = f;
    stp->st_bradius = MAGNITUDE(work);

    if (RT_G_DEBUG & DEBUG_HF) {
	bu_log("  model space bbox (%g %g %g) (%g %g %g)\n",
	       V3ARGS(stp->st_min),
	       V3ARGS(stp->st_max));
    }

    return 0;
}


HIDDEN void
plot_seg(struct isect_stuff *isect,
	 struct hit *in_hit,
	 struct hit *out_hit,
	 const point_t bbmin, /* The bounding box of what you are adding ... */
	 const point_t bbmax, /* ... */
	 int r, int g, int b)/* ... this is strictly for debug plot purposes */
{
    fastf_t *stom = &isect->dsp->dsp_i.dsp_stom[0];
    struct bound_rpp rpp;
    char fname[32];
    FILE *fp;
    static int segnum =0;

    /* plot the bounding box and the seg */
    bu_semaphore_acquire(BU_SEM_SYSCALL);
    sprintf(fname, "dsp_seg%04d.pl", segnum++);
    fp=fopen(fname, "wb");
    bu_semaphore_release(BU_SEM_SYSCALL);

    if (fp != (FILE *)NULL) {
	bu_log("plotting %s\n", fname);

	MAT4X3PNT(rpp.min, stom, bbmin);
	MAT4X3PNT(rpp.max, stom, bbmax);
	plot_rpp(fp, &rpp, r/2, g/2, b/2);

	/* re-use the rpp for the points for the segment */
	MAT4X3PNT(rpp.min, stom, in_hit->hit_point);
	MAT4X3PNT(rpp.max, stom, out_hit->hit_point);

	pl_color(fp, r, g, b);
	pdv_3line(fp, rpp.min, rpp.max);

	bu_semaphore_acquire(BU_SEM_SYSCALL);
	fclose(fp);
	bu_semaphore_release(BU_SEM_SYSCALL);
    }
}


#define ADD_SEG(isect, in, out, min, max, r, g, b) \
	add_seg(isect, in, out, min, max, r, g, b, __LINE__)


/**
 * A D D _ S E G
 *
 * Add a segment to the list of intersections in DSP space
 *
 * Return:
 * 0 continue to intersect
 * 1 All intersections computed, terminate intersection processing
 */
HIDDEN int
add_seg(struct isect_stuff *isect,
	struct hit *in_hit,
	struct hit *out_hit,
	const point_t bbmin, /* The bounding box of what you are adding ... */
	const point_t bbmax, /* ... */
	int r, int g, int b, /* ... this is strictly for debug plot purposes */
	int line)

{
    struct seg *seg;
    double tt = isect->tol->dist;
    double delta;
#ifndef ORDERED_ISECT
    struct bu_list *spot;
#endif

    dlog("add_seg %g %g line %d vpriv:%g %g\n", in_hit->hit_dist, out_hit->hit_dist, line, in_hit->hit_vpriv[X], in_hit->hit_vpriv[Y]);

    tt *= tt;

#ifdef ORDERED_ISECT
    if (BU_LIST_NON_EMPTY(&isect->seglist)) {
	/* if the new in-distance equals the old out distance
	 * we just extend the old segment
	 */
	seg = BU_LIST_LAST(seg, &isect->seglist);
	if (fabs(seg->seg_out.hit_dist - in_hit->hit_dist) <= tt) {

	    seg->seg_out.hit_dist = out_hit->hit_dist;
	    seg->seg_out.hit_surfno = out_hit->hit_surfno;
	    VMOVE(seg->seg_out.hit_normal, out_hit->hit_normal);
	    if (out_hit->hit_surfno == ZTOP) {
		seg->seg_out.hit_vpriv[X] = out_hit->hit_vpriv[X];
		seg->seg_out.hit_vpriv[Y] = out_hit->hit_vpriv[Y];
	    }
	    seg->seg_out.hit_vpriv[Z] = 1.0; /* flag as out-hit */

	    if (RT_G_DEBUG & DEBUG_HF) {
		bu_log("extending previous seg to %g\n", out_hit->hit_dist);
		plot_seg(isect, in_hit, out_hit, bbmin, bbmax, r, g, b);
	    }
	    return 0;
	}
    }
#else
    /* insert the segment in the list by in-hit distance */
    dlog("searching for insertion point for seg w in/out dist %g %g\n",
	 in_hit->hit_dist, out_hit->hit_dist);

    for (BU_LIST_FOR(seg, seg, &isect->seglist)) {
	dlog("checking %g->%g seg\n", seg->seg_in.hit_dist,
	     seg->seg_out.hit_dist);
	/* found the spot for this one */
	if (fabs(seg->seg_out.hit_dist - in_hit->hit_dist) <= tt) {
	    seg->seg_out = *out_hit; /* struct copy */
	    seg->seg_out.hit_magic = RT_HIT_MAGIC;
	    seg->seg_out.hit_vpriv[Z] = 1.0; /* flag as out-hit */

	    if (RT_G_DEBUG & DEBUG_HF) {
		bu_log("extending previous seg to %g\n",
		       out_hit->hit_dist);
		plot_seg(isect, in_hit, out_hit, bbmin, bbmax, r, g, b);
	    }
	    return 0;
	}
	if (in_hit->hit_dist < seg->seg_in.hit_dist) {
	    spot = &seg->l;
	    dlog("insert before this one\n");
	    goto found_spot;
	}
    }
    spot = &isect->seglist;
    seg = BU_LIST_LAST(seg, &isect->seglist);
    dlog("insert at end\n");
 found_spot:
#endif


    /* if both points are on the "floor" of the DSP, then we
     * don't have a hit segment
     */
    if (NEAR_ZERO(in_hit->hit_point[Z], isect->tol->dist) &&
	NEAR_ZERO(out_hit->hit_point[Z], isect->tol->dist)) {
	return 0;
    }

    /* throw away any zero length segments,
     * mostly to avoid seeing inside-out segments
     */
    delta = out_hit->hit_dist - in_hit->hit_dist;

    if (ZERO(delta)) {
	return 0;
    }

    /* if the segment is inside-out, we need to say something about it */
    if (delta < 0.0 && !NEAR_ZERO(delta, isect->tol->dist)) {
	bu_log(" %s:%dDSP:  Adding inside-out seg in:%g out:%g\n",
	       __FILE__, __LINE__,
	       in_hit->hit_dist, out_hit->hit_dist);

	VPRINT("\tin_pt", in_hit->hit_point);
	VPRINT("\tout_pt", out_hit->hit_point);
    }


    RT_GET_SEG(seg, isect->ap->a_resource);

    seg->seg_in.hit_dist    = in_hit->hit_dist;
    seg->seg_out.hit_dist   = out_hit->hit_dist;

    seg->seg_in.hit_surfno  = in_hit->hit_surfno;
    seg->seg_out.hit_surfno = out_hit->hit_surfno;

    VMOVE(seg->seg_in.hit_normal, in_hit->hit_normal);
    VMOVE(seg->seg_out.hit_normal, out_hit->hit_normal);

    if (in_hit->hit_surfno >= ZMAX) {
	seg->seg_in.hit_vpriv[X] = in_hit->hit_vpriv[X];
	seg->seg_in.hit_vpriv[Y] = in_hit->hit_vpriv[Y];
    }

    if (out_hit->hit_surfno >= ZMAX) {
	seg->seg_out.hit_vpriv[X] = out_hit->hit_vpriv[X];
	seg->seg_out.hit_vpriv[Y] = out_hit->hit_vpriv[Y];
    }

    seg->seg_in.hit_vpriv[Z] = 0.0; /* flag as in-hit */
    seg->seg_out.hit_vpriv[Z] = 1.0; /* flag as out-hit */

    seg->seg_stp = isect->stp;

#ifdef FULL_DSP_DEBUGGING
    if (VDOT(seg->seg_in.hit_normal, isect->r.r_dir) > 0) {
	bu_log("----------------------------------------------------------\n");
	bu_log("pixel %d, %d bogus seg in, inward normal\nN: %g %g %g\nd: %g %g %g\n",
	       isect->ap->a_x, isect->ap->a_y,
	       V3ARGS(seg->seg_in.hit_normal), V3ARGS(isect->r.r_dir));
	bu_bomb("");
    }
    if (VDOT(seg->seg_out.hit_normal, isect->r.r_dir) < 0) {
	bu_log("----------------------------------------------------------\n");
	bu_log("pixel %d, %d bogus seg out, inward normal\nN: %g %g %g\nd: %g %g %g\n",
	       isect->ap->a_x, isect->ap->a_y,
	       V3ARGS(seg->seg_out.hit_normal), V3ARGS(isect->r.r_dir));
	bu_bomb("");
    }
#endif

#ifdef ORDERED_ISECT
    BU_LIST_INSERT(&isect->seglist, &seg->l);
#else
    BU_LIST_INSERT(spot, &seg->l);
#endif


    if (RT_G_DEBUG & DEBUG_HF)
	plot_seg(isect, in_hit, out_hit, bbmin, bbmax, r, g, b);


    if (seg->seg_in.hit_dist > 0.0 || seg->seg_out.hit_dist > 0.0) {
	return ++isect->num_segs > isect->ap->a_onehit;
    }
    return 0;
}


/**
 * I S E C T _ R A Y _ T R I A N G L E
 *
 * Side Effects:
 * dist and P may be set
 *
 * Return:
 * 1 Ray intersects triangle
 * 0 Ray misses triangle
 * -1 Ray/plane parallel
 */
HIDDEN int
isect_ray_triangle(struct isect_stuff *isect,
		   point_t A,
		   point_t B,
		   point_t C,
		   struct hit *hitp,
		   double alphabbeta[])
{
    point_t P;		/* plane intercept point */
    vect_t AB, AC, AP;
    plane_t N;		/* Normal for plane of triangle */
    double NdotDir;
    double alpha, beta;	/* barycentric distances */
    double hitdist;	/* distance to ray/trianlge intercept */
    double toldist;	/* distance tolerance from isect->tol */

#ifdef FULL_DSP_DEBUGGING
    if (RT_G_DEBUG & DEBUG_HF) {
	fastf_t *stom = &isect->dsp->dsp_i.dsp_stom[0];

	MAT4X3PNT(P, stom, A);
	bu_log("isect_ray_triangle...\n  A %g %g %g  (%g %g %g)\n",
	       V3ARGS(A), V3ARGS(P));

	MAT4X3PNT(P, stom, B);
	bu_log("  B %g %g %g  (%g %g %g)\n",
	       V3ARGS(B), V3ARGS(P));

	MAT4X3PNT(P, stom, C);
	bu_log("  C %g %g %g  (%g %g %g)\n",
	       V3ARGS(C), V3ARGS(P));

    }
#endif
    VSUB2(AB, B, A);
    VSUB2(AC, C, A);

    /* Compute the plane equation of the triangle */
    VCROSS(N, AB, AC);
    VUNITIZE(N);
    N[H] = VDOT(N, A);


    /* intersect ray with plane */
    NdotDir = VDOT(N, isect->r.r_dir);
    if (BN_VECT_ARE_PERP(NdotDir, isect->tol)) {
	/* Ray perpendicular to plane of triangle */
	return -1;
    }

    /* dist to plane icept */
    hitdist = (N[H]-VDOT(N, isect->r.r_pt)) / NdotDir;

    VJOIN1(P, isect->r.r_pt, hitdist, isect->r.r_dir);

#ifdef FULL_DSP_DEBUGGING
    if (RT_G_DEBUG & DEBUG_HF) {
	FILE *fp;
	char buf[32];
	static int plotnum = 0;
	fastf_t *stom = &isect->dsp->dsp_i.dsp_stom[0];
	point_t p1, p2;

	bu_semaphore_acquire(BU_SEM_SYSCALL);
	sprintf(buf, "dsp_tri%03d.pl", plotnum++);
	fp=fopen(buf, "wb");
	bu_semaphore_release(BU_SEM_SYSCALL);

	if (fp == (FILE *)NULL) {
	    bu_log("%s:%d error opening \"%s\"\n", __FILE__, __LINE__, buf);
	    bu_bomb("");
	} else
	    bu_log("  plotting %s\n", buf);


	pl_color(fp, 255, 255, 255);
	MAT4X3PNT(p1, stom, A); pdv_3move(fp, p1);
	MAT4X3PNT(p2, stom, B);	pdv_3cont(fp, p2);
	MAT4X3PNT(p2, stom, C);	pdv_3cont(fp, p2);
	pdv_3cont(fp, p1);

	/* plot the ray */
	pl_color(fp, 255, 255, 0);
	MAT4X3PNT(p1, stom, P);
	MAT4X3PNT(p2, stom, isect->r.r_pt);
	pdv_3line(fp, p1, p2);

	bu_semaphore_acquire(BU_SEM_SYSCALL);
	fclose(fp);
	bu_semaphore_release(BU_SEM_SYSCALL);

	bu_log("  dist:%g plane point: %g %g %g\n", hitdist, V3ARGS(p2));
    }
#endif
    /* We project the system into the XY plane at this point to
     * determine if the ray_plane_isect_pt is within the bounds of the
     * triangle
     *
     * The general idea here is to project the vector AP onto both
     * sides of the triangle.  The distances along each side can be
     * used to determine if we are inside/outside the triangle.
     *
     * VSUB2(AP, P, A);
     * alpha = VDOT(AP, AB);
     * beta = VDOT(AP, AC);
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
     * To save on computation, we do this calculation in 2D.
     *
     * See: "Graphics Gems",  Andrew S. Glassner ed.  PP390-393 for details
     */

    toldist = isect->tol->dist;


    VSUB2(AP, P, A);
#ifdef FULL_DSP_DEBUGGING
    if (RT_G_DEBUG & DEBUG_HF) {
	VPRINT("  AP", AP);
	VPRINT("  AB", AB);
	VPRINT("  AC", AC);
    }
#endif

    /* Oridnarily, in 2D we would say:
     *
     * beta = AB[X] * AP[X] + AB[Y] * AP[Y];
     * alpha = AC[X] * AP[X] + AC[Y] * AP[Y];
     *
     * However, in this case, we know that AB and AC will always be
     * axis-aligned, so we can short-cut.  XXX consider: we know that
     * AB and AC will be unit length, so only the sign counts.
     */

    if (ZERO(AB[X])) {
	beta = AB[Y] * AP[Y];
    } else {
	beta = AB[X] * AP[X];
    }

    if (ZERO(AC[X])) {
	alpha = AC[Y] * AP[Y];
    } else {
	alpha = AC[X] * AP[X];
    }

    /* return 1 if we hit the triangle */
    alphabbeta[0] = alpha;
    alphabbeta[1] = beta;

#ifdef FULL_DSP_DEBUGGING
    if (alpha < -toldist) {
	dlog("alpha < 0\n");
	return 0;
    }
    if (beta < -toldist) {
	dlog("beta < 0\n");
	return 0;
    }
    if ((alpha+beta) > (1.0 + toldist)) {
	dlog("alpha+beta > 1\n");
	return 0;
    }
#else
    if (alpha < -toldist || beta < -toldist || (alpha+beta) > (1.0 + toldist))
	return 0;
#endif

    hitp->hit_dist = hitdist;
    VMOVE(hitp->hit_normal, N);
    VMOVE(hitp->hit_point, P);
    return 1;
}


/**
 * P E R M U T E _ C E L L
 *
 * For adaptive diagonal selection or for Upper-Left to lower right
 * cell cut, we must permute the verticies of the cell before handing
 * them to the intersection algorithm.  That's what this function
 * does.
 */
HIDDEN int
permute_cell(point_t A,
	     point_t B,
	     point_t C,
	     point_t D,
	     struct dsp_specific *dsp,
	     struct dsp_rpp *dsp_rpp)
{
    int x, y;


#ifdef FULL_DSP_DEBUGGING
    if (RT_G_DEBUG & DEBUG_HF) {
	VPRINT("\tA", A);
	VPRINT("\tB", B);
	VPRINT("\tC", C);
	VPRINT("\tD", D);
    }
#endif

    switch (dsp->dsp_i.dsp_cuttype) {
	case DSP_CUT_DIR_llUR:
	    return DSP_CUT_DIR_llUR;
	    break;

	case DSP_CUT_DIR_ADAPT: {
	    int lo[2], hi[2];
	    point_t tmp;
	    double h1, h2, h3, h4;
	    double cAD, cBC;  /* curvature in direction AD, and BC */

	    if (RT_G_DEBUG & DEBUG_HF)
		bu_log("cell %d, %d adaptive triangulation... ",
		       dsp_rpp->dsp_min[X],
		       dsp_rpp->dsp_min[Y]);

	    /*
	     * We look at the points in the diagonal next cells to
	     * determine the curvature along each diagonal of this
	     * cell.  This cell is divided into two triangles by
	     * cutting across the cell in the direction of least
	     * curvature.
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

	    lo[X] = dsp_rpp->dsp_min[X] - 1;
	    lo[Y] = dsp_rpp->dsp_min[Y] - 1;
	    hi[X] = dsp_rpp->dsp_max[X] + 1;
	    hi[Y] = dsp_rpp->dsp_max[Y] + 1;

	    /* a little bounds checking */
	    if (lo[X] < 0) lo[X] = 0;
	    if (lo[Y] < 0) lo[Y] = 0;
	    if (hi[X] > dsp->xsiz)
		hi[X] = dsp->xsiz;

	    if (hi[Y] > dsp->ysiz)
		hi[Y] = dsp->ysiz;

	    /* compute curvature along the A->D direction */
	    h1 = DSP(&dsp->dsp_i, lo[X], lo[Y]);
	    h2 = A[Z];
	    h3 = D[Z];
	    h4 = DSP(&dsp->dsp_i, hi[X], hi[Y]);

	    cAD = fabs(h3 + h1 - 2*h2) + fabs(h4 + h2 - 2*h3);


	    /* compute curvature along the B->C direction */
	    h1 = DSP(&dsp->dsp_i, hi[X], lo[Y]);
	    h2 = B[Z];
	    h3 = C[Z];
	    h4 = DSP(&dsp->dsp_i, lo[X], hi[Y]);

	    cBC = fabs(h3 + h1 - 2*h2) + fabs(h4 + h2 - 2*h3);

	    if (cAD < cBC) {
		/* A-D cut is fine, no need to permute */
		if (RT_G_DEBUG & DEBUG_HF)
		    bu_log("A-D cut\n");

		return DSP_CUT_DIR_llUR;

	    }

	    /* prefer the B-C cut */
	    VMOVE(tmp, A);
	    VMOVE(A, B);
	    VMOVE(B, D);
	    VMOVE(D, C);
	    VMOVE(C, tmp);
	    if (RT_G_DEBUG & DEBUG_HF)
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
	    x = dsp_rpp->dsp_min[X];
	    y = dsp_rpp->dsp_min[Y];
	    VSET(B, x, y, DSP(&dsp->dsp_i, x, y));

	    x = dsp_rpp->dsp_max[X];
	    VSET(A, x, y, DSP(&dsp->dsp_i, x, y));

	    y = dsp_rpp->dsp_max[Y];
	    VSET(C, x, y, DSP(&dsp->dsp_i, x, y));

	    x = dsp_rpp->dsp_min[X];
	    VSET(D, x, y, DSP(&dsp->dsp_i, x, y));

	    return DSP_CUT_DIR_ULlr;
	    break;
    }
    bu_log("%s:%d Unknown DSP cut direction: %d\n",
	   __FILE__, __LINE__, dsp->dsp_i.dsp_cuttype);
    bu_bomb("");
    /* not reached */
    return -1;
}


/**
 * C H E C K _ B B _ E L E V A T I O N
 *
 * determine if a point P is above/below the slope line on the
 * bounding box.  eg:
 *
 * Bounding box side view:
 *
 *	+-------+
 *	|	|
 *	|	*	Determine if P (or Q) is above the
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
 * Return
 * 0 if pt above line (such as P in diagram)
 * 1 if pt at/below line (such as Q in diagram)
 */
HIDDEN int
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

#ifdef FULL_DSP_DEBUGGING
    dlog("check_bbpt_hit_elev(");
#endif
    switch (i) {
	case XMIN:
	    /* the minimal YZ plane.  Top view:	*   *
	     *					|
	     *					*   *
	     */
#ifdef FULL_DSP_DEBUGGING
	    dlog("XMIN)\n");
#endif
	    slope = C[Z] - A[Z];
	    delta = P[Y] - A[Y];
	    origin = A[Z];
	    break;
	case XMAX:
	    /* the maximal YZ plane.   Top view:	*   *
	     *						    |
	     *						*   *
	     */
#ifdef FULL_DSP_DEBUGGING
	    dlog("XMAX)\n");
#endif
	    slope = D[Z] - B[Z];
	    delta = P[Y] - B[Y];
	    origin = B[Z];
	    break;
	case YMIN:
	    /* the minimal XZ plane.   Top view:	*   *
	     *
	     *						* - *
	     */
#ifdef FULL_DSP_DEBUGGING
	    dlog("YMIN)\n");
#endif
	    slope = B[Z] - A[Z];
	    delta = P[X] - A[X];
	    origin = A[Z];
	    break;
	case YMAX:
	    /* the maximal XZ plane.   Top view:	* - *
	     *
	     *						*   *
	     */
#ifdef FULL_DSP_DEBUGGING
	    dlog("YMAX)\n");
#endif
	    slope = D[Z] - C[Z];
	    delta = P[X] - C[X];
	    origin = C[Z];
	    break;
	case ZMIN:
#ifdef FULL_DSP_DEBUGGING
	    dlog("ZMIN)\n");
#endif
	    return 1;
	    break;
	case ZMAX:
#ifdef FULL_DSP_DEBUGGING
	    dlog("ZMAX)\n");
#endif
	    return 0;
	    break;
	default:
	    bu_log("%s:%d Coding error, bad face %d\n", __FILE__, __LINE__, i);
	    bu_bomb("");
	    break;
    }

    if ((origin + slope * delta) < P[Z]) return 0;

    return 1;
}


/*
 * I S E C T _ R A Y _ C E L L _ T O P
 *
 * Return
 * 0 continue intesection calculations
 * 1 Terminate intesection computation
 */
HIDDEN int
isect_ray_cell_top(struct isect_stuff *isect, struct dsp_bb *dsp_bb)
{
    point_t A, B, C, D, P;
    int x, y;
    double ab_first[2] = {0.0,0.0}, ab_second[2] = {0.0,0.0};
    struct hit hits[4];	/* list of hits that are valid */
    struct hit *hitp;
    int hitf = 0;	/* bit flags for valid hits in hits */
    int cond, i;
    int hitcount = 0;
    point_t bbmin, bbmax;
    double dot, dot2;


    dlog("isect_ray_cell_top\n");
    DSP_BB_CK(dsp_bb);

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
    VSET(A, x, y, DSP(&isect->dsp->dsp_i, x, y));

    x = dsp_bb->dspb_rpp.dsp_max[X];
    VSET(B, x, y, DSP(&isect->dsp->dsp_i, x, y));

    y = dsp_bb->dspb_rpp.dsp_max[Y];
    VSET(D, x, y, DSP(&isect->dsp->dsp_i, x, y));

    x = dsp_bb->dspb_rpp.dsp_min[X];
    VSET(C, x, y, DSP(&isect->dsp->dsp_i, x, y));


#ifdef DEBUG_FULL
    if (RT_G_DEBUG & DEBUG_HF) {
	point_t p1, p2;

	VJOIN1(p1, isect->r.r_pt, isect->r.r_min, isect->r.r_dir);
	VMOVE(hits[0].hit_point, p1);
	hits[0].hit_dist = isect->r.r_min;

	VJOIN1(p2, isect->r.r_pt, isect->r.r_max, isect->r.r_dir);
	VMOVE(hits[1].hit_point, p2);
	hits[1].hit_dist = isect->r.r_max;

	plot_cell_top(isect, dsp_bb, A, B, C, D, hits, 3, 0);
    }
#endif


    /* first order of business is to discard any "fake" hits on the
     * bounding box, and fill in any "real" hits in our list
     */
    VJOIN1(P, isect->r.r_pt, isect->r.r_min, isect->r.r_dir);
    if (check_bbpt_hit_elev(isect->dmin, A, B, C, D, P)) {
	hits[0].hit_dist = isect->r.r_min;
	VMOVE(hits[0].hit_point, P);
	VMOVE(hits[0].hit_normal, dsp_pl[isect->dmin]);
	/* vpriv */
	hits[0].hit_vpriv[X] = dsp_bb->dspb_rpp.dsp_min[X];
	hits[0].hit_vpriv[Y] = dsp_bb->dspb_rpp.dsp_min[Y];
	/* private */
	hits[0].hit_surfno = isect->dmin;

	hitcount++;

	hitf = 1;
	if (RT_G_DEBUG & DEBUG_HF) {
	    dot = VDOT(hits[0].hit_normal, isect->r.r_dir);
	    bu_log("hit ray/bb min  Normal: %g %g %g %s\n",
		   V3ARGS(hits[0].hit_normal),
		   ((dot > 0.0) ? "outbound" : "inbound"));
	}
    } else {
	dlog("miss ray/bb min\n");
    }


    /* make sure the point P is below the cell top */
    VJOIN1(P, isect->r.r_pt, isect->r.r_max, isect->r.r_dir);
    if (check_bbpt_hit_elev(isect->dmax, A, B, C, D, P)) {
	/* P is at or below the top surface */
	hits[3].hit_dist = isect->r.r_max;
	VMOVE(hits[3].hit_point, P);
	VMOVE(hits[3].hit_normal, dsp_pl[isect->dmax]);
	/* vpriv */
	hits[3].hit_vpriv[X] = dsp_bb->dspb_rpp.dsp_min[X];
	hits[3].hit_vpriv[Y] = dsp_bb->dspb_rpp.dsp_min[Y];
	/* private */
	hits[3].hit_surfno = isect->dmax;

	hitcount++;

	hitf |= 8;
	if (RT_G_DEBUG & DEBUG_HF) {
	    dot = VDOT(hits[3].hit_normal, isect->r.r_dir);
	    bu_log("hit ray/bb max  Normal: %g %g %g  %s\n",
		   V3ARGS(hits[3].hit_normal),
		   ((dot > 0.0) ? "outbound" : "inbound"));
	}
    } else {
	dlog("miss ray/bb max\n");
    }


    (void)permute_cell(A, B, C, D, isect->dsp, &dsp_bb->dspb_rpp);

    if ((cond=isect_ray_triangle(isect, B, D, A, &hits[1], ab_first)) > 0.0) {
	/* hit triangle */

	/* record cell */
	hits[1].hit_vpriv[X] = dsp_bb->dspb_rpp.dsp_min[X];
	hits[1].hit_vpriv[Y] = dsp_bb->dspb_rpp.dsp_min[Y];
	hits[1].hit_surfno = ZTOP; /* indicate we hit the top */

	hitcount++;
	hitf |= 2;
	dlog("  hit triangle 1 (alpha: %g beta:%g alpha+beta: %g) vpriv %g %g\n",
	     ab_first[0], ab_first[1], ab_first[0] + ab_first[1],
	     hits[1].hit_vpriv[X], hits[1].hit_vpriv[Y]);
    } else {
	dlog("  miss triangle 1 (alpha: %g beta:%g a+b: %g) cond:%d\n",
	     ab_first[0], ab_first[1], ab_first[0] + ab_first[1], cond);
    }
    if ((cond=isect_ray_triangle(isect, C, A, D, &hits[2], ab_second)) > 0.0) {
	/* hit triangle */

	/* record cell */
	hits[2].hit_vpriv[X] = dsp_bb->dspb_rpp.dsp_min[X];
	hits[2].hit_vpriv[Y] = dsp_bb->dspb_rpp.dsp_min[Y];
	hits[2].hit_surfno = ZTOP; /* indicate we hit the top */

	hitcount++;

	hitf |= 4;
	dlog("  hit triangle 2 (alpha: %g beta:%g alpha+beta: %g) vpriv %g %g\n",
	     ab_second[0], ab_second[1], ab_second[0] + ab_second[1],
	     hits[2].hit_vpriv[X], hits[2].hit_vpriv[Y]);

	if (hitf & 2) {
	    /* if this hit occurs before the hit on the other triangle
	     * swap the order
	     */
	    if (hits[1].hit_dist > hits[2].hit_dist) {
		struct hit tmp;
		tmp = hits[1]; /* struct copy */
		hits[1] = hits[2]; /* struct copy */
		hits[2] = tmp; /* struct copy */
		dlog("re-ordered triangle hits\n");

	    } else
		dlog("triangle hits in order\n");
	}


    } else {
	dlog("  miss triangle 2 (alpha: %g beta:%g alpha+beta: %g) cond:%d\n",
	     ab_second[0], ab_second[1], ab_second[0] + ab_second[1], cond);
    }

    if (RT_G_DEBUG & DEBUG_HF) {
	bu_log("hitcount: %d flags: 0x%0x\n", hitcount, hitf);

	plot_cell_top(isect, dsp_bb, A, B, C, D, hits, hitf, 1);
	for (i=0; i < 4; i++) {
	    if (hitf & (1<<i)) {
		double v = VDOT(isect->r.r_dir, hits[i].hit_normal);

		bu_log("%d dist:%g N:%g %g %g ",
		       i, hits[i].hit_dist, V3ARGS(hits[i].hit_normal));

		if (v > 0.0) bu_log("outbound\n");
		else if (v < 0.0) bu_log("inbound\n");
		else bu_log("perp\n");
	    }
	}
	bu_log("assembling segs\n");
    }


    /* fill out the segment structures */

    hitp = 0;
    for (i = 0; i < 4; i++) {
	if (hitf & (1<<i)) {
	    if (hitp) {

		dot2 = VDOT(isect->r.r_dir, hits[i].hit_normal);

		/* if we have two entry points then pick the first one */
		if (dot2 < 0.0) {
		    dlog("dot2(%g) < 0.0\n", dot2);
		    if (hitp->hit_dist > hits[i].hit_dist) {
			dlog("skipping duplicate entry point at dist %g\n",
			     hitp->hit_dist);

			hitp = &hits[i];
		    } else {
			dlog("skipping duplicate entry point at dist %g\n",
			     hits[i].hit_dist);
		    }

		    continue;
		}

		/* create seg with hits[i].hit_point); as out point */
		if (RT_G_DEBUG & DEBUG_HF) {
		    /* int/float conv */
		    VMOVE(bbmin, dsp_bb->dspb_rpp.dsp_min);
		    VMOVE(bbmax, dsp_bb->dspb_rpp.dsp_max);
		}

		if (ADD_SEG(isect, hitp, &hits[i], bbmin, bbmax, 255, 255, 255))
		    return 1;

		hitp = 0;
	    } else {
		dot = VDOT(isect->r.r_dir, hits[i].hit_normal);
		if (dot >= 0.0)
		    continue;

		/* remember hits[i].hit_point); as in point */
		if (RT_G_DEBUG & DEBUG_HF) {
		    bu_log("in-hit at dist %g\n", hits[i].hit_dist);
		}
		hitp = &hits[i];
	    }
	}
    }

    if (hitp && hitcount > 1) {
	point_t p1, p2;

	bu_log("----------------ERROR incomplete segment-------------\n");
	bu_log("  pixel %d %d\n", isect->ap->a_x, isect->ap->a_y);

	VJOIN1(p1, isect->r.r_pt, isect->r.r_min, isect->r.r_dir);
	VMOVE(hits[0].hit_point, p1);
	hits[0].hit_dist = isect->r.r_min;

	VJOIN1(p2, isect->r.r_pt, isect->r.r_max, isect->r.r_dir);
	VMOVE(hits[1].hit_point, p2);
	hits[1].hit_dist = isect->r.r_max;

	if (RT_G_DEBUG & DEBUG_HF)
	    plot_cell_top(isect, dsp_bb, A, B, C, D, hits, 3, 0);
    }
    return 0;
}


/**
 * D S P _ I N _ R P P
 *
 * Compute the intersections of a ray with a rectangular parallelpiped
 * (RPP) that has faces parallel to the coordinate planes
 *
 * The algorithm here was developed by Gary Kuehl for GIFT.  A good
 * description of the approach used can be found in "??" by XYZZY and
 * Barsky, ACM Transactions on Graphics, Vol 3 No 1, January 1984.
 *
 * Note - The computation of entry and exit distance is mandatory, as
 * the final test catches the majority of misses.
 *
 * Note - A hit is returned if the intersect is behind the start
 * point.
 *
 * Returns -
 * 0 if ray does not hit RPP,
 * !0 if ray hits RPP.
 *
 * Implicit return -
 * rp->r_min = dist from start of ray to point at which ray ENTERS solid
 * rp->r_max = dist from start of ray to point at which ray LEAVES solid
 * isect->dmin
 * isect->dmax
 */
int
dsp_in_rpp(struct isect_stuff *isect,
	   register const fastf_t *min,
	   register const fastf_t *max)
{
    struct xray *rp = &isect->r;
    /* inverses of rp->r_dir[] */
    register const fastf_t *invdir = isect->inv_dir;
    register const fastf_t *pt = &rp->r_pt[0];
    register fastf_t sv;
    register fastf_t rmin = -INFINITY;
    register fastf_t rmax =  INFINITY;
    int dmin = -1;
    int dmax = -1;

    /* Start with infinite ray, and trim it down */

    /* X axis */
    if (*invdir < 0.0) {
	/* Heading towards smaller numbers */
	/* if (*min > *pt) miss */
	if (rmax > (sv = (*min - *pt) * *invdir)) {
	    rmax = sv;
	    dmax = XMIN;
	}
	if (rmin < (sv = (*max - *pt) * *invdir)) {
	    rmin = sv;
	    dmin = XMAX;
	}
    }  else if (*invdir > 0.0) {
	/* Heading towards larger numbers */
	/* if (*max < *pt) miss */
	if (rmax > (sv = (*max - *pt) * *invdir)) {
	    rmax = sv;
	    dmax = XMAX;
	}
	if (rmin < ((sv = (*min - *pt) * *invdir))) {
	    rmin = sv;
	    dmin = XMIN;
	}
    } else {
	/*
	 * Direction cosines along this axis is NEAR 0, which implies
	 * that the ray is perpendicular to the axis, so merely check
	 * position against the boundaries.
	 */
	if ((*min > *pt) || (*max < *pt))
	    return 0;	/* MISS */
    }

    /* Y axis */
    pt++; invdir++; max++; min++;
    if (*invdir < 0.0) {
	if (rmax > (sv = (*min - *pt) * *invdir)) {
	    /* towards smaller */
	    rmax = sv;
	    dmax = YMIN;
	}
	if (rmin < (sv = (*max - *pt) * *invdir)) {
	    rmin = sv;
	    dmin = YMAX;
	}
    }  else if (*invdir > 0.0) {
	/* towards larger */
	if (rmax > (sv = (*max - *pt) * *invdir)) {
	    rmax = sv;
	    dmax = YMAX;
	}
	if (rmin < ((sv = (*min - *pt) * *invdir))) {
	    rmin = sv;
	    dmin = YMIN;
	}
    } else {
	if ((*min > *pt) || (*max < *pt))
	    return 0;	/* MISS */
    }

    /* Z axis */
    pt++; invdir++; max++; min++;
    if (*invdir < 0.0) {
	/* towards smaller */
	if (rmax > (sv = (*min - *pt) * *invdir)) {
	    rmax = sv;
	    dmax = ZMIN;
	}
	if (rmin < (sv = (*max - *pt) * *invdir)) {
	    rmin = sv;
	    dmin = ZMAX;
	}
    }  else if (*invdir > 0.0) {
	/* towards larger */
	if (rmax > (sv = (*max - *pt) * *invdir)) {
	    rmax = sv;
	    dmax = ZMAX;
	}
	if (rmin < ((sv = (*min - *pt) * *invdir))) {
	    rmin = sv;
	    dmin = ZMIN;
	}
    } else {
	if ((*min > *pt) || (*max < *pt))
	    return 0;	/* MISS */
    }

    /* If equal, RPP is actually a plane */
    if (rmin > rmax)
	return 0;	/* MISS */

    /* HIT.  Only now do rp->r_min and rp->r_max have to be written */
    rp->r_min = rmin;
    rp->r_max = rmax;

    isect->dmin = dmin;
    isect->dmax = dmax;
    return 1;		/* HIT */
}


#ifdef ORDERED_ISECT
HIDDEN int
isect_ray_dsp_bb(struct isect_stuff *isect, struct dsp_bb *dsp_bb);


/**
 * R E C U R S E _ D S P _ B B
 *
 * Return
 * 0 continue intesection calculations
 * 1 Terminate intesection computation
 */
HIDDEN int
recurse_dsp_bb(struct isect_stuff *isect,
	       struct dsp_bb *dsp_bb,
	       point_t minpt, /* entry point of dsp_bb */
	       point_t UNUSED(maxpt), /* exit point of dsp_bb */
	       point_t bbmin, /* min point of bb (Z=0) */
	       point_t UNUSED(bbmax)) /* max point of bb */
{
    double tDX;		/* dist along ray to span 1 cell in X dir */
    double tDY;		/* dist along ray to span 1 cell in Y dir */
    double tX, tY;	/* dist from hit pt. to next cell boundary */
    double curr_dist;
    short cX, cY;	/* coordinates of current cell */
    short cs;		/* cell X, Y dimension */
    short stepX, stepY;	/* dist to step in child array for each dir */
    short stepPX, stepPY;
    double out_dist;
    struct dsp_bb **p;
    fastf_t *stom = &isect->dsp->dsp_i.dsp_stom[0];
    point_t pt, v;
    int loop = 0;

    DSP_BB_CK(dsp_bb);

    /* compute the size of a cell in each direction */
    cs = dsp_bb->dspb_subcell_size;

    /* compute current cell */
    cX = (minpt[X] - bbmin[X]) / cs;
    cY = (minpt[Y] - bbmin[Y]) / cs;

    /* a little bounds checking because a hit on XMAX or YMAX looks
     * like it should be in the next cell outside the box
     */
    if (cX >= dsp_bb->dspb_ch_dim[X]) cX = dsp_bb->dspb_ch_dim[X] - 1;
    if (cY >= dsp_bb->dspb_ch_dim[Y]) cY = dsp_bb->dspb_ch_dim[Y] - 1;

#ifdef FULL_DSP_DEBUGGING
    dlog("recurse_dsp_bb  cell size: %d  current cell: %d %d\n",
	 cs, cX, cY);
    dlog("dspb_ch_dim x:%d  y:%d\n",
	 dsp_bb->dspb_ch_dim[X], dsp_bb->dspb_ch_dim[Y]);
#endif

    tX = tY = curr_dist = isect->r.r_min;

    if (isect->r.r_dir[X] < 0.0) {
	stepPX = stepX = -1;
	/* tDX is the distance along the ray we have to travel to
	 * traverse a cell (travel a unit distance) along the X axis
	 * of the grid
	 */
	tDX = -cs / isect->r.r_dir[X];

	/* tX is the distance along the ray to the first cell boundary
	 * in the X direction beyond our hit point (minpt)
	 */
	tX += ((bbmin[X] + (cX * cs)) - minpt[X]) / isect->r.r_dir[X];
    } else {
	stepPX = stepX = 1;
	tDX = cs / isect->r.r_dir[X];

	if (isect->r.r_dir[X] > 0.0)
	    tX += ((bbmin[X] + ((cX+1) * cs)) - minpt[X]) / isect->r.r_dir[X];
	else
	    tX = MAX_FASTF; /* infinite distance to next X boundary */
    }

    if (isect->r.r_dir[Y] < 0) {
	/* distance in dspb_children we have to move to step in Y dir
	 */
	stepY = -1;
	stepPY = -dsp_bb->dspb_ch_dim[X];
	tDY = -cs / isect->r.r_dir[Y];
	tY += ((bbmin[Y] + (cY * cs)) - minpt[Y]) / isect->r.r_dir[Y];
    } else {
	stepY = 1;
	stepPY = dsp_bb->dspb_ch_dim[X];
	tDY = cs / isect->r.r_dir[Y];

	if (isect->r.r_dir[Y] > 0.0)
	    tY += ((bbmin[Y] + ((cY+1) * cs)) - minpt[Y]) / isect->r.r_dir[Y];
	else
	    tY = MAX_FASTF;
    }

    /* factor in the tolerance to the out-distance */
    out_dist = isect->r.r_max - isect->tol->dist;

    p = &dsp_bb->dspb_children[dsp_bb->dspb_ch_dim[X] * cY + cX];
#ifdef FULL_DSP_DEBUGGING
    dlog("tX:%g tY:%g\n", tX, tY);

#endif

    do {
	/* intersect with the current cell */
	if (RT_G_DEBUG & DEBUG_HF) {
	    if (loop)
		bu_log("\nisect sub-cell %d %d  curr_dist:%g out_dist: %g",
		       cX, cY, curr_dist, out_dist);
	    else {
		bu_log("isect sub-cell %d %d  curr_dist:%g out_dist %g",
		       cX, cY, curr_dist, out_dist);
		loop = 1;
	    }
	    VJOIN1(pt, isect->r.r_pt, curr_dist, isect->r.r_dir);
	    MAT4X3PNT(v, stom, pt);
	    bu_log("pt %g %g %g\n", V3ARGS(v));
	}

	/* get pointer to the current child cell.  Note the extra
	 * level of indirection.  We want to march p through
	 * dspb_children using pointer addition, but dspb_children
	 * contains pointers.  We need to be sure to increment the
	 * offset in the array, not the child pointer.
	 */

	if (RT_G_DEBUG & DEBUG_HF)
	    bu_log_indent_delta(4);

	if (isect_ray_dsp_bb(isect, *p)) return 1;

	if (RT_G_DEBUG & DEBUG_HF)
	    bu_log_indent_delta(-4);

	/* figure out which cell is next */
	if (tX < tY) {
	    cX += stepX;  /* track cell offset for debugging */
	    p += stepPX;
#ifdef FULL_DSP_DEBUGGING
	    dlog("stepping X to %d because %g < %g\n", cX, tX, tY);
#endif
	    curr_dist = tX;
	    tX += tDX;
	} else {
	    cY += stepY;  /* track cell offset for debugging */
	    p += stepPY;
#ifdef FULL_DSP_DEBUGGING
	    dlog("stepping Y to %d because %g >= %g\n", cY, tX, tY);
#endif
	    curr_dist = tY;
	    tY += tDY;
	}
#ifdef FULL_DSP_DEBUGGING
	dlog("curr_dist %g, out_dist %g\n", curr_dist, out_dist);
#endif
    } while (curr_dist < out_dist &&
	     cX < dsp_bb->dspb_ch_dim[X] && cX >= 0 &&
	     cY < dsp_bb->dspb_ch_dim[Y] && cY >= 0);

    return 0;
}


#endif

/**
 * I S E C T _ R A Y _ D S P _ B B
 *
 * Intersect a ray with a DSP bounding box.  This is the primary child
 * of rt_dsp_shot()
 *
 * Return
 * 0 continue intesection calculations
 * 1 Terminate intesection computation
 */
HIDDEN int
isect_ray_dsp_bb(struct isect_stuff *isect, struct dsp_bb *dsp_bb)
{
    point_t bbmin, bbmax;
    point_t minpt, maxpt;
    double min_z;
    /* the rest of these support debugging output */
    FILE *fp;
    static int plotnum;
    fastf_t *stom;
    point_t pt;
    struct xray *r = &isect->r; /* Does this buy us anything? */

    if (dsp_bb) {
	DSP_BB_CK(dsp_bb);
    } else {
	bu_log("%s:%d null ptr pixel %d %d\n",
	       __FILE__, __LINE__, isect->ap->a_x,  isect->ap->a_y);
	bu_bomb("");
    }

    if (RT_G_DEBUG & DEBUG_HF) {
	bu_log("\nisect_ray_dsp_bb((%d, %d, %d) (%d, %d, %d))\n",
	       V3ARGS(dsp_bb->dspb_rpp.dsp_min),
	       V3ARGS(dsp_bb->dspb_rpp.dsp_max));
    }

    /* check to see if we miss the RPP for this area entirely */
    VMOVE(bbmax, dsp_bb->dspb_rpp.dsp_max);
    VSET(bbmin,
	 dsp_bb->dspb_rpp.dsp_min[X],
	 dsp_bb->dspb_rpp.dsp_min[Y], 0.0);


    if (! dsp_in_rpp(isect, bbmin, bbmax)) {
	/* missed it all, just return */

	if (RT_G_DEBUG & DEBUG_HF) {
	    bu_log("missed... ");
	    fclose(draw_dsp_bb(&plotnum, dsp_bb, isect->dsp, 0, 150, 0));
	}

	return 0;
    }

    /* At this point we know that we've hit the overall bounding box
     */

    VJOIN1(minpt, r->r_pt, r->r_min, r->r_dir);
    VJOIN1(maxpt, r->r_pt, r->r_max, r->r_dir);
    /* We could do the following:
     *
     * however, we only need the Z values now, and benchmarking show
     * that this is an expensive
     */
    if (RT_G_DEBUG & DEBUG_HF) {

	stom = &isect->dsp->dsp_i.dsp_stom[0];

	bu_log("hit b-box ");
	fp = draw_dsp_bb(&plotnum, dsp_bb, isect->dsp, 200, 200, 100);

	pl_color(fp, 150, 150, 255);
	MAT4X3PNT(pt, stom, minpt);
	pdv_3move(fp, pt);
	MAT4X3PNT(pt, stom, maxpt);
	pdv_3cont(fp, pt);

	fclose(fp);
    }


    /* if both hits are UNDER the top of the "foundation" pillar, we
     * can just add a segment for that range and return
     */
    min_z = dsp_bb->dspb_rpp.dsp_min[Z];

    if (minpt[Z] < min_z && maxpt[Z] < min_z) {
	/* add hit segment */
	struct hit seg_in, seg_out;

	seg_in.hit_magic = RT_HIT_MAGIC;
	seg_in.hit_dist = r->r_min;
	VMOVE(seg_in.hit_point, minpt);
	VMOVE(seg_in.hit_normal, dsp_pl[isect->dmin]);
	/* hit_priv */
	/* hit_private */
	seg_in.hit_surfno = isect->dmin;
	/* hit_rayp */

	seg_out.hit_dist = r->r_max;

	VMOVE(seg_out.hit_point, maxpt);
	VMOVE(seg_out.hit_normal, dsp_pl[isect->dmax]);
	/* hit_priv */
	/* hit_private */
	seg_out.hit_surfno = isect->dmax;
	/* hit_rayp */

	if (RT_G_DEBUG & DEBUG_HF) {
	    /* we need these for debug output
	     * VMOVE(seg_in.hit_point, minpt);
	     * VMOVE(seg_out.hit_point, maxpt);
	     */
	    /* create a special bounding box for plotting purposes */
	    VMOVE(bbmax,  dsp_bb->dspb_rpp.dsp_max);
	    VMOVE(bbmin,  dsp_bb->dspb_rpp.dsp_min);
	    bbmax[Z] = bbmin[Z];
	    bbmin[Z] = 0.0;
	}

	/* outta here */
	return ADD_SEG(isect, &seg_in, &seg_out, bbmin, bbmax, 0, 255, 255);
    }


    /* We've hit something where we might be going through the
     * boundary.  We've got to intersect the children
     */
    if (dsp_bb->dspb_ch_dim[0]) {
#ifdef ORDERED_ISECT
	return recurse_dsp_bb(isect, dsp_bb, minpt, maxpt, bbmin, bbmax);
#else
	int i;
	/* there are children, so we recurse */
	i = dsp_bb->dspb_ch_dim[X] * dsp_bb->dspb_ch_dim[Y] - 1;
	if (RT_G_DEBUG & DEBUG_HF)
	    bu_log_indent_delta(4);

	for (; i >= 0; i--)
	    isect_ray_dsp_bb(isect, dsp_bb->dspb_children[i]);

	if (RT_G_DEBUG & DEBUG_HF)
	    bu_log_indent_delta(-4);

	return 0;

#endif
    }

    /***********************************************************************
     *
     * This section is for level 0 intersections only
     *
     ***********************************************************************/

    /* intersect the DSP grid surface geometry */

    /* Check for a hit on the triangulated zone on top.  This gives us
     * intersections on the triangulated top, and the sides and bottom
     * of the bounding box for the triangles.
     *
     * We do this first because we already know that the ray does NOT
     * just pass through the "foundation " pillar underneath (see test
     * above)
     */
    bbmin[Z] = dsp_bb->dspb_rpp.dsp_min[Z];
    if (dsp_in_rpp(isect, bbmin, bbmax)) {
	/* hit rpp */

	isect_ray_cell_top(isect, dsp_bb);
    }


    /* check for hits on the "foundation" pillar under the top.  The
     * ray may have entered through the top of the pillar, possibly
     * after having come down through the triangles above
     */
    bbmax[Z] = dsp_bb->dspb_rpp.dsp_min[Z];
    bbmin[Z] = 0.0;
    if (dsp_in_rpp(isect, bbmin, bbmax)) {
	/* hit rpp */
	struct hit in_hit, out_hit;

	VJOIN1(minpt, r->r_pt, r->r_min, r->r_dir);
	VJOIN1(maxpt, r->r_pt, r->r_max, r->r_dir);

	in_hit.hit_dist = r->r_min;
	in_hit.hit_surfno = isect->dmin;
	VMOVE(in_hit.hit_point, minpt);
	VMOVE(in_hit.hit_normal, dsp_pl[isect->dmin]);

	out_hit.hit_dist = r->r_max;
	out_hit.hit_surfno = isect->dmax;
	VMOVE(out_hit.hit_point, maxpt);
	VMOVE(out_hit.hit_normal, dsp_pl[isect->dmax]);

	/* add a segment to the list */
	return ADD_SEG(isect, &in_hit, &out_hit, bbmin, bbmax, 255, 255, 0);
    }

    return 0;
}


/**
 * R T _ D S P _ S H O T
 *
 * Intersect a ray with a dsp.
 * If an intersection occurs, a struct seg will be acquired
 * and filled in.
 *
 * Returns -
 * 0 MISS
 * >0 HIT
 */
int
rt_dsp_shot(struct soltab *stp, register struct xray *rp, struct application *ap, struct seg *seghead)
{
    register struct dsp_specific *dsp =
	(struct dsp_specific *)stp->st_specific;
    register struct seg *segp;
    int i;
    vect_t dir;	/* temp storage */
    vect_t v;
    struct isect_stuff isect;
    double delta;

    RT_DSP_CK_MAGIC(dsp);
    BU_CK_VLS(&dsp->dsp_i.dsp_name);

    switch (dsp->dsp_i.dsp_datasrc) {
	case RT_DSP_SRC_V4_FILE:
	case RT_DSP_SRC_FILE:
	    BU_CK_MAPPED_FILE(dsp->dsp_i.dsp_mp);
	    break;
	case RT_DSP_SRC_OBJ:
	    RT_CK_DB_INTERNAL(dsp->dsp_i.dsp_bip);
	    RT_CK_BINUNIF(dsp->dsp_i.dsp_bip->idb_ptr);
	    break;
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
    isect.num_segs = 0;

    VINVDIR(isect.inv_dir, isect.r.r_dir);
    BU_LIST_INIT(&isect.seglist);


    if (RT_G_DEBUG & DEBUG_HF) {
	bu_log("rt_dsp_shot(pt:(%g %g %g)\n\tdir[%g]:(%g %g %g))\n    pixel(%d, %d)\n",
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

    /* We look at the topmost layer of the bounding-box tree and make
     * sure that it has dimension 1.  Otherwise, something is wrong
     */
    if (isect.dsp->layer[isect.dsp->layers-1].dim[X] != 1 ||
	isect.dsp->layer[isect.dsp->layers-1].dim[Y] != 1) {
	bu_log("%s:%d how do i find the topmost layer?\n",
	       __FILE__, __LINE__);
	bu_bomb("");
    }


    /* intersect the ray with the bounding rpps */
    (void)isect_ray_dsp_bb(&isect, isect.dsp->layer[isect.dsp->layers-1].p);

    /* if we missed it all, give up now */
    if (BU_LIST_IS_EMPTY(&isect.seglist))
	return 0;

    /* map hit distances back to model space */
    i = 0;
    for (BU_LIST_FOR(segp, seg, &isect.seglist)) {
	i += 2;
	if (RT_G_DEBUG & DEBUG_HF) {
	    bu_log("\nsolid in:%6g out:%6g\t",
		   segp->seg_in.hit_dist,
		   segp->seg_out.hit_dist);
	}

	/* form vector for hit point from start in solid space */
	VSCALE(dir, isect.r.r_dir, segp->seg_in.hit_dist);
	/* transform vector into model space */
	MAT4X3VEC(v, dsp->dsp_i.dsp_stom, dir);
	/* get magnitude */
	segp->seg_in.hit_dist = MAGNITUDE(v);
	/* XXX why is this necessary? */
	if (VDOT(v, rp->r_dir) < 0.0) segp->seg_in.hit_dist *= -1.0;

	VSCALE(dir, isect.r.r_dir, segp->seg_out.hit_dist);
	MAT4X3VEC(v, dsp->dsp_i.dsp_stom, dir);
	segp->seg_out.hit_dist = MAGNITUDE(v);
	if (VDOT(v, rp->r_dir) < 0.0) segp->seg_out.hit_dist *= -1.0;


	delta = segp->seg_out.hit_dist - segp->seg_in.hit_dist;

	if (delta < 0.0 && !NEAR_ZERO(delta, ap->a_rt_i->rti_tol.dist)) {
	    bu_log("Pixel %d %d seg inside out in:%g out:%g seg_len:%g\n",
		   ap->a_x, ap->a_y, segp->seg_in.hit_dist, segp->seg_out.hit_dist,
		   delta);
	}

	if (RT_G_DEBUG & DEBUG_HF) {
	    bu_log("model in:%6g out:%6g\t",
		   segp->seg_in.hit_dist,
		   segp->seg_out.hit_dist);
	}
    }

    if (RT_G_DEBUG & DEBUG_HF) {
	double NdotD;
	double d;
	static const plane_t plane = {0.0, 0.0, -1.0, 0.0};

	NdotD = VDOT(plane, rp->r_dir);
	d = - ((VDOT(plane, rp->r_pt) - plane[H]) / NdotD);
	bu_log("rp -> Z=0 dist: %g\n", d);
    }

    /* transfer list of hitpoints */
    BU_LIST_APPEND_LIST(&(seghead->l), &isect.seglist);

    return i;
}


/***********************************************************************
 *
 * Compute the model-space normal at a gridpoint
 *
 */
HIDDEN void
compute_normal_at_gridpoint(vect_t N,
			    struct dsp_specific *dsp,
			    unsigned int x,
			    unsigned int y,
			    FILE *fd,
			    double len)
{
    /* Gridpoint specified is "B" we compute normal by taking the
     * cross product of the vectors  A->C, D->E
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

    point_t A, C, D, E, tmp, pt, endpt;
    vect_t Vac, Vde;

    if (RT_G_DEBUG & DEBUG_HF) {
	bu_log("normal at %d %d\n", x, y);
	bn_mat_print("\tstom", dsp->dsp_i.dsp_stom);
    }
    VSET(tmp, x, y, DSP(&dsp->dsp_i, x, y));

    if (x == 0) {
	VMOVE(A, tmp);
    } else {
	VSET(A, x-1, y, DSP(&dsp->dsp_i, x-1, y));
    }

    if (x >= XSIZ(dsp)) {
	VMOVE(C, tmp);
    } else {
	VSET(C, x+1, y,  DSP(&dsp->dsp_i, x+1, y));
    }

    if (y == 0) {
	VMOVE(D, tmp);
    } else {
	VSET(D, x, y-1, DSP(&dsp->dsp_i, x, y-1));
    }

    if (y >= YSIZ(dsp)) {
	VMOVE(E, tmp);
    } else {
	VSET(E, x, y+1, DSP(&dsp->dsp_i, x, y+1));
    }

    MAT4X3PNT(pt, dsp->dsp_i.dsp_stom, tmp);


    /* Computing in world coordinates */
    VMOVE(tmp, A);
    MAT4X3PNT(A, dsp->dsp_i.dsp_stom, tmp);

    VMOVE(tmp, C);
    MAT4X3PNT(C, dsp->dsp_i.dsp_stom, tmp);

    VMOVE(tmp, D);
    MAT4X3PNT(D, dsp->dsp_i.dsp_stom, tmp);

    VMOVE(tmp, E);
    MAT4X3PNT(E, dsp->dsp_i.dsp_stom, tmp);

    VSUB2(Vac, C, A);
    VSUB2(Vde, E, D);

    VUNITIZE(Vac);
    VUNITIZE(Vde);
    VCROSS(N, Vac, Vde);

    if (RT_G_DEBUG & DEBUG_HF) {
	VPRINT("\tA", A);
	VPRINT("\tC", C);
	VPRINT("\tD", D);
	VPRINT("\tE", E);
	VPRINT("\tVac", Vac);
	VPRINT("\tVde", Vde);
	VPRINT("\tModel Cross N", N);
    }
    VUNITIZE(N);

    if (RT_G_DEBUG & DEBUG_HF) {
	VPRINT("\tModel Unit N", N);
    }
    if (fd) {
	VJOIN1(endpt, pt, len, N);

	pl_color(fd, 220, 220, 90);
	pdv_3line(fd, A, C);
	pdv_3line(fd, D, E);

	pdv_3line(fd, pt, endpt);
    }


}


/**
 * R T _ D S P _ N O R M
 *
 * Given ONE ray distance, return the normal and entry/exit point.
 */
void
rt_dsp_norm(register struct hit *hitp, struct soltab *stp, register struct xray *rp)
{
    vect_t N, t, tmp, A;
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
	    RT_CK_DB_INTERNAL(dsp->dsp_i.dsp_bip);
	    RT_CK_BINUNIF(dsp->dsp_i.dsp_bip->idb_ptr);
	    break;
    }

    if (RT_G_DEBUG & DEBUG_HF) {
	bu_log("rt_dsp_norm(%g %g %g)\n", V3ARGS(hitp->hit_normal));
	VJOIN1(hitp->hit_point, rp->r_pt, hitp->hit_dist, rp->r_dir);
	VPRINT("\thit point", hitp->hit_point);
	bu_log("hit dist: %g\n", hitp->hit_dist);
	bu_log("%s:%d vpriv: %g, %g %g\n",
	       __FILE__, __LINE__, V3ARGS(hitp->hit_vpriv));
    }

    if (hitp->hit_surfno < XMIN || hitp->hit_surfno > ZTOP) {
	bu_log("%s:%d bogus surface of DSP %d\n",
	       __FILE__, __LINE__, hitp->hit_surfno);
	bu_bomb("");
    }

    /* compute hit point */
    VJOIN1(hitp->hit_point, rp->r_pt, hitp->hit_dist, rp->r_dir);


    if (hitp->hit_surfno != ZTOP || !dsp->dsp_i.dsp_smooth) {
	/* We've hit one of the sides or bottom, or the user didn't
	 * ask for smoothing of the elevation data, so there's no
	 * interpolation to do.  Just transform the normal to model
	 * space, and comput the actual hit point
	 */

	/* transform normal into model space */
	MAT4X3VEC(tmp, dsp->dsp_i.dsp_mtos, hitp->hit_normal);
	VUNITIZE(tmp);
	VMOVE(hitp->hit_normal, tmp);

	if (RT_G_DEBUG & DEBUG_HF)
	    bu_log("\tno Interpolation needed.  Normal: %g, %g, %g\n",
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
	struct bu_vls str;

	bu_vls_init(&str);
	bu_vls_printf(&str, "dsp_gourand%02d.pl", plot_file_num++);
	bu_log("plotting normals in %s", bu_vls_addr(&str));
	fd = fopen(bu_vls_addr(&str), "w");
	bu_vls_free(&str);

	/* plot the ray */
	pl_color(fd, 255, 0, 0);
	pdv_3line(fd, rp->r_pt, hitp->hit_point);

	/* plot the normal we started with */
	pl_color(fd, 0, 255, 0);
	VJOIN1(tmp, hitp->hit_point, len, hitp->hit_normal);
	pdv_3line(fd, hitp->hit_point, tmp);

    }

    /* get the cell we hit */
    x = hitp->hit_vpriv[X];
    y = hitp->hit_vpriv[Y];

    compute_normal_at_gridpoint(Anorm, dsp, x, y, fd, len);
    compute_normal_at_gridpoint(Bnorm, dsp, x+1, y, fd, len);
    compute_normal_at_gridpoint(Dnorm, dsp, x+1, y+1, fd, len);
    compute_normal_at_gridpoint(Cnorm, dsp, x, y+1, fd, len);

    /* transform the hit point into DSP space for determining
     * interpolation
     */
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
	/* This is an experiment to "flatten" the curvature of the dsp
	 * near the grid points
	 */
#define SMOOTHSTEP(x)  ((x)*(x)*(3 - 2*(x)))
	Xfrac = SMOOTHSTEP(Xfrac);
	Yfrac = SMOOTHSTEP(Yfrac);
#undef SMOOTHSTEP
    }

    /* we compute the normal along the "X edges" of the cell */
    VSCALE(Anorm, Anorm, (1.0-Xfrac));
    VSCALE(Bnorm, Bnorm,      Xfrac);
    VADD2(ABnorm, Anorm, Bnorm);
    VUNITIZE(ABnorm);

    VSCALE(Cnorm, Cnorm, (1.0-Xfrac));
    VSCALE(Dnorm, Dnorm,      Xfrac);
    VADD2(CDnorm, Dnorm, Cnorm);
    VUNITIZE(CDnorm);

    /* now we interpolate the two X edge normals to get the final one
     */
    VSCALE(ABnorm, ABnorm, (1.0-Yfrac));
    VSCALE(CDnorm, CDnorm, Yfrac);
    VADD2(N, ABnorm, CDnorm);

    VUNITIZE(N);

    dot = VDOT(N, rp->r_dir);
    if (RT_G_DEBUG & DEBUG_HF)
	bu_log("interpolated %g %g %g  dot:%g\n", V3ARGS(N), dot);

    if ((ZERO(hitp->hit_vpriv[Z]) && dot > 0.0)/* in-hit needs fix */ ||
	(ZERO(hitp->hit_vpriv[Z] - 1.0) && dot < 0.0)/* out-hit needs fix */) {
	/* bring the normal back to being perpindicular to the ray to
	 * avoid "flipped normal" warnings
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
	pl_color(fd, 255, 255, 255);
	VJOIN1(tmp, hitp->hit_point, len, hitp->hit_normal);
	pdv_3line(fd, hitp->hit_point, tmp);

	bu_semaphore_acquire(BU_SEM_SYSCALL);
	fclose(fd);
	bu_semaphore_release(BU_SEM_SYSCALL);
    }
}


/**
 * R T _ D S P _ C U R V E
 *
 * Return the curvature of the dsp.
 */
void
rt_dsp_curve(register struct curvature *cvp, register struct hit *hitp, struct soltab *stp)
{
    if (stp) RT_CK_SOLTAB(stp);

    if (RT_G_DEBUG & DEBUG_HF)
	bu_log("rt_dsp_curve()\n");

    cvp->crv_c1 = cvp->crv_c2 = 0;

    /* any tangent direction */
    bn_vec_ortho(cvp->crv_pdir, hitp->hit_normal);
}


/**
 * R T _ D S P _ U V
 *
 * For a hit on the surface of a dsp, return the (u, v) coordinates
 * of the hit point, 0 <= u, v <= 1.
 * u = azimuth
 * v = elevation
 */
void
rt_dsp_uv(struct application *ap, struct soltab *stp, register struct hit *hitp, register struct uvcoord *uvp)
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
     * To compute this, transform unit vectors from solid space to
     * model space.  We remember the length of the resultant vectors
     * and then unitize them to get u, v directions in model
     * coordinate space.
     */
    VSET(tmp, XSIZ(dsp), 0.0, 0.0);	/* X direction vector */
    MAT4X3VEC(U_dir,  dsp->dsp_i.dsp_stom, tmp); /* into model space */
    U_len = MAGNITUDE(U_dir);		/* remember model space length */
    one_over_len = 1.0/U_len;
    VSCALE(U_dir, U_dir, one_over_len); /* scale to unit length in model space */

    VSET(tmp, 0.0, YSIZ(dsp), 0.0);	/* Y direction vector */
    MAT4X3VEC(V_dir,  dsp->dsp_i.dsp_stom, tmp);
    V_len = MAGNITUDE(V_dir);
    one_over_len = 1.0/V_len;
    VSCALE(V_dir, V_dir, one_over_len); /* scale to unit length in model space */

    /* divide the hit-point radius by the U/V unit length distance */
    r = ap->a_rbeam + ap->a_diverge * hitp->hit_dist;
    min_r_U = r / U_len;
    min_r_V = r / V_len;

    /* compute UV_dir, a vector in the plane of the hit point
     * (surface) which points in the anti-rayward direction
     */
    VREVERSE(rev_dir, ap->a_ray.r_dir);
    VMOVE(norm, hitp->hit_normal);
    dot_N = VDOT(rev_dir, norm);
    VJOIN1(UV_dir, rev_dir, -dot_N, norm);
    VUNITIZE(UV_dir);

    if (ZERO(dot_N)) {
	/* ray almost perfectly 90 degrees to surface */
	uvp->uv_du = min_r_U;
	uvp->uv_dv = min_r_V;
    } else {
	/* somehow this computes the extent of U and V in the radius */
	uvp->uv_du = (r / U_len) * VDOT(UV_dir, U_dir) / dot_N;
	uvp->uv_dv = (r / V_len) * VDOT(UV_dir, V_dir) / dot_N;
    }


    if (uvp->uv_du < 0.0)
	uvp->uv_du = -uvp->uv_du;
    if (uvp->uv_du < min_r_U)
	uvp->uv_du = min_r_U;

    if (uvp->uv_dv < 0.0)
	uvp->uv_dv = -uvp->uv_dv;
    if (uvp->uv_dv < min_r_V)
	uvp->uv_dv = min_r_V;

    if (RT_G_DEBUG & DEBUG_HF)
	bu_log("rt_dsp_uv(pt:%g, %g siz:%zu, %zu)\n U_len=%g V_len=%g\n r=%g rbeam=%g diverge=%g dist=%g\n u=%g v=%g du=%g dv=%g\n",
	       pt[X], pt[Y], XSIZ(dsp), YSIZ(dsp),
	       U_len, V_len,
	       r, ap->a_rbeam, ap->a_diverge, hitp->hit_dist,
	       uvp->uv_u, uvp->uv_v,
	       uvp->uv_du, uvp->uv_dv);
}


/**
 * R T _ D S P _ F R E E
 */
void
rt_dsp_free(register struct soltab *stp)
{
    register struct dsp_specific *dsp =
	(struct dsp_specific *)stp->st_specific;

    if (RT_G_DEBUG & DEBUG_HF)
	bu_log("rt_dsp_free()\n");

    switch (dsp->dsp_i.dsp_datasrc) {
	case RT_DSP_SRC_V4_FILE:
	case RT_DSP_SRC_FILE:
	    if (dsp->dsp_i.dsp_mp) {
		BU_CK_MAPPED_FILE(dsp->dsp_i.dsp_mp);
		bu_close_mapped_file(dsp->dsp_i.dsp_mp);
	    } else if (dsp->dsp_i.dsp_buf) {
		bu_free(dsp->dsp_i.dsp_buf, "dsp fake data");
	    }
	    break;
	case RT_DSP_SRC_OBJ:
	    break;
    }

    bu_free((char *)dsp, "dsp_specific");
}


/**
 * R T _ D S P _ C L A S S
 */
int
rt_dsp_class(void)
{
    if (RT_G_DEBUG & DEBUG_HF)
	bu_log("rt_dsp_class()\n");

    return 0;
}


/**
 * R T _ D S P _ P L O T
 */
int
rt_dsp_plot(struct bu_list *vhead, struct rt_db_internal *ip, const struct rt_tess_tol *ttol, const struct bn_tol *UNUSED(tol), const struct rt_view_info *UNUSED(info))
{
    struct rt_dsp_internal *dsp_ip =
	(struct rt_dsp_internal *)ip->idb_ptr;
    point_t m_pt;
    point_t s_pt;
    point_t o_pt;
    unsigned int x, y;
    int step;
    unsigned int xlim = dsp_ip->dsp_xcnt - 1;
    unsigned int ylim = dsp_ip->dsp_ycnt - 1;
    int xfudge, yfudge;
    int drawing;

    if (RT_G_DEBUG & DEBUG_HF)
	bu_log("rt_dsp_plot()\n");

    BU_CK_LIST_HEAD(vhead);
    RT_CK_DB_INTERNAL(ip);
    RT_DSP_CK_MAGIC(dsp_ip);

    switch (dsp_ip->dsp_datasrc) {
	case RT_DSP_SRC_V4_FILE:
	case RT_DSP_SRC_FILE:
	    if (!dsp_ip->dsp_mp) {
		bu_log("WARNING: Cannot find data file for displacement map (DSP)\n");
		if (bu_vls_addr(&dsp_ip->dsp_name)) {
		    bu_log("         DSP data file [%s] not found or empty\n", bu_vls_addr(&dsp_ip->dsp_name));
		} else {
		    bu_log("         DSP data file not found or not specified\n");
		}
		return 0;
	    }
	    BU_CK_MAPPED_FILE(dsp_ip->dsp_mp);
	    break;
	case RT_DSP_SRC_OBJ:
	    if (!dsp_ip->dsp_bip) {
		bu_log("WARNING: Cannot find data object for displacement map (DSP)\n");
		if (bu_vls_addr(&dsp_ip->dsp_name)) {
		    bu_log("         DSP data object [%s] not found or empty\n", bu_vls_addr(&dsp_ip->dsp_name));
		} else {
		    bu_log("         DSP data object not found or not specified\n");
		}
		return 0;
	    }
	    RT_CK_DB_INTERNAL(dsp_ip->dsp_bip);
	    RT_CK_BINUNIF(dsp_ip->dsp_bip->idb_ptr);
	    break;
    }


#define MOVE(_pt) \
	MAT4X3PNT(m_pt, dsp_ip->dsp_stom, _pt); \
	RT_ADD_VLIST(vhead, m_pt, BN_VLIST_LINE_MOVE)

#define DRAW(_pt) \
	MAT4X3PNT(m_pt, dsp_ip->dsp_stom, _pt); \
	RT_ADD_VLIST(vhead, m_pt, BN_VLIST_LINE_DRAW)


    /* Draw the Bottom */
    VSETALL(s_pt, 0.0);
    MOVE(s_pt);

    s_pt[X] = xlim;
    DRAW(s_pt);

    s_pt[Y] = ylim;
    DRAW(s_pt);

    s_pt[X] = 0.0;
    DRAW(s_pt);

    s_pt[Y] = 0.0;
    DRAW(s_pt);


    /* Draw the corners */
    s_pt[Z] = DSP(dsp_ip, 0, 0);
    DRAW(s_pt);

    VSET(s_pt, xlim, 0.0, 0.0);
    MOVE(s_pt);
    s_pt[Z] = DSP(dsp_ip, xlim, 0);
    DRAW(s_pt);


    VSET(s_pt, xlim, ylim, 0.0);
    MOVE(s_pt);
    s_pt[Z] = DSP(dsp_ip, xlim, ylim);
    DRAW(s_pt);

    VSET(s_pt, 0.0, ylim, 0.0);
    MOVE(s_pt);
    s_pt[Z] = DSP(dsp_ip, 0, ylim);
    DRAW(s_pt);


    /* Draw the outside line of the top We draw the four sides of the
     * top at full resolution.  This helps edge matching.  The inside
     * of the top, we draw somewhat coarser
     */
    for (y=0; y < dsp_ip->dsp_ycnt; y += ylim) {
	VSET(s_pt, 0.0, y, DSP(dsp_ip, 0, y));
	MOVE(s_pt);

	for (x=0; x < dsp_ip->dsp_xcnt; x++) {
	    s_pt[X] = x;
	    s_pt[Z] = DSP(dsp_ip, x, y);
	    DRAW(s_pt);
	}
    }


    for (x=0; x < dsp_ip->dsp_xcnt; x += xlim) {
	VSET(s_pt, x, 0.0, DSP(dsp_ip, x, 0));
	MOVE(s_pt);

	for (y=0; y < dsp_ip->dsp_ycnt; y++) {
	    s_pt[Y] = y;
	    s_pt[Z] = DSP(dsp_ip, x, y);
	    DRAW(s_pt);
	}
    }

    /* now draw the body of the top */
    if (!ZERO(ttol->rel)) {
	unsigned int rstep;
	rstep = dsp_ip->dsp_xcnt;
	V_MAX(rstep, dsp_ip->dsp_ycnt);
	step = (int)(ttol->rel * rstep);
    } else {
	int goal = 10000;
	goal -= 5;
	goal -= 8 + 2 * (dsp_ip->dsp_xcnt+dsp_ip->dsp_ycnt);

	if (goal <= 0) return 0;

	/* Compute data stride based upon producing no more than 'goal' vectors */
	step = ceil(
	    sqrt(2*(xlim)*(ylim) /
		 (double)goal)
	    );
    }
    if (step < 1) step = 1;


    xfudge = (dsp_ip->dsp_xcnt % step + step) / 2;
    yfudge = (dsp_ip->dsp_ycnt % step + step) / 2;

    if (xfudge < 1) xfudge = 1;
    if (yfudge < 1) yfudge = 1;

    /* draw the horizontal (y==const) lines */
    for (y=yfudge; y < ylim; y += step) {
	VSET(s_pt, 0.0, y, DSP(dsp_ip, 0, y));
	VMOVE(o_pt, s_pt);
	if (!ZERO(o_pt[Z])) {
	    drawing = 1;
	    MOVE(o_pt);
	} else {
	    drawing = 0;
	}

	for (x=xfudge; x < xlim; x+=step) {
	    s_pt[X] = x;

	    s_pt[Z] = DSP(dsp_ip, x, y);
	    if (!ZERO(s_pt[Z])) {
		if (drawing) {
		    DRAW(s_pt);
		} else {
		    MOVE(o_pt);
		    DRAW(s_pt);
		    drawing = 1;
		}
	    } else {
		if (drawing) {
		    DRAW(s_pt);
		    drawing = 0;
		}
	    }

	    VMOVE(o_pt, s_pt);
	}

	s_pt[X] = xlim;
	s_pt[Z] = DSP(dsp_ip, xlim, y);
	if (!ZERO(s_pt[Z])) {
	    if (drawing) {
		DRAW(s_pt);
	    } else {
		MOVE(o_pt);
		DRAW(s_pt);
		drawing = 1;
	    }
	} else {
	    if (drawing) {
		DRAW(s_pt);
		drawing = 0;
	    }
	}

    }

    for (x=xfudge; x < xlim; x += step) {
	VSET(s_pt, x, 0.0, DSP(dsp_ip, x, 0));
	VMOVE(o_pt, s_pt);
	if (!ZERO(o_pt[Z])) {
	    drawing = 1;
	    MOVE(o_pt);
	} else {
	    drawing = 0;
	}


	for (y=yfudge; y < ylim; y+=step) {
	    s_pt[Y] = y;

	    s_pt[Z] = DSP(dsp_ip, x, y);
	    if (!ZERO(s_pt[Z])) {
		if (drawing) {
		    DRAW(s_pt);
		} else {
		    MOVE(o_pt);
		    DRAW(s_pt);
		    drawing = 1;
		}
	    } else {
		if (drawing) {
		    DRAW(s_pt);
		    drawing = 0;
		}
	    }

	    VMOVE(o_pt, s_pt);
	}

	s_pt[Y] = ylim;
	s_pt[Z] = DSP(dsp_ip, x, ylim);
	if (!ZERO(s_pt[Z])) {
	    if (drawing) {
		DRAW(s_pt);
	    } else {
		MOVE(o_pt);
		DRAW(s_pt);
		drawing = 1;
	    }
	} else {
	    if (drawing) {
		DRAW(s_pt);
		drawing = 0;
	    }
	}
    }

#undef MOVE
#undef DRAW
    return 0;
}


/*
 * G E T _ C U T _ D I R
 *
 * Determine the cut direction for a DSP cell. This routine is used by
 * rt_dsp_tess(). It somewhat duplicates code from permute_cell(),
 * which is a bad thing, but permute_cell() had other side effects not
 * desired here.
 *
 * inputs:
 * dsp_ip - pointer to the rt_dsp_internal struct fro thi DSP
 * x - the DSP cell x-coord of the lower left corner of the cell
 * y - the DSP cell y-coord of the lower left corner of the cell
 * xlim - the maximum value of the DSP x coordinates
 * ylim - the maximum value of the DSP y coordinates
 * return:
 * the direction for cutting this cell.
 * DSP_CUT_DIR_llUR - lower left to upper right
 * DSP_CUT_DIR_ULlr - upper right to lower left
 */
HIDDEN int
get_cut_dir(struct rt_dsp_internal *dsp_ip, int x, int y, int xlim, int ylim)
{
/*
 * height array contains DSP values:
 * height[0] is at (x, y)
 * height[1] is at (x+1, y)
 * height[2] is at (x+1, y+1)
 * height[3] is at (x, y+1)
 * height[4] is at (x-1, y-1)
 * height[5] is at (x+2, y-1)
 * height[6] is at (x+2, y+2)
 * height[7] is at (x-1, y+2)
 *
 * 7         6
 *  \       /
 *   \     /
 *    3---2
 *    |   |
 *    |   |
 *    0---1
 *   /     \
 *  /       \
 * 4         5
 *
 * (0, 1, 2, 3) is the cell of interest.
 */
    int height[8];
    int xx, yy;
    double c02, c13;  /* curvature in direction 0<->2, and 1<->3 */

    if (dsp_ip->dsp_cuttype != DSP_CUT_DIR_ADAPT) {
	/* not using adpative cut type, so just return the cut type */
	return dsp_ip->dsp_cuttype;
    }

    /* fill in the height array */
    xx = x;
    yy = y;
    height[0] = DSP(dsp_ip, xx, yy);
    xx = x+1;
    if (xx > xlim) xx = xlim;
    height[1] = DSP(dsp_ip, xx, yy);
    yy = y+1;
    if (yy > ylim) yy = ylim;
    height[2] = DSP(dsp_ip, xx, yy);
    xx = x;
    height[3] = DSP(dsp_ip, xx, yy);
    xx = x-1;
    if (xx < 0) xx = 0;
    yy = y-1;
    if (yy < 0) yy = 0;
    height[4] = DSP(dsp_ip, xx, yy);
    xx = x+2;
    if (xx > xlim) xx = xlim;
    height[5] = DSP(dsp_ip, xx, yy);
    yy = y+2;
    if (yy > ylim) yy = ylim;
    height[6] = DSP(dsp_ip, xx, yy);
    xx = x-1;
    if (xx < 0) xx = 0;
    height[7] = DSP(dsp_ip, xx, yy);

    /* compute curvature along the 0<->2 direction */
    c02 = fabs(height[2] + height[4] - 2*height[0]) + fabs(height[6] + height[0] - 2*height[2]);


    /* compute curvature along the 1<->3 direction */
    c13 = fabs(height[3] + height[5] - 2*height[1]) + fabs(height[7] + height[1] - 2*height[3]);

    if (c02 < c13) {
	/* choose the 0<->2 direction */
	return DSP_CUT_DIR_llUR;
    } else {
	/* choose the other direction */
	return DSP_CUT_DIR_ULlr;
    }
}


/* a macro to check if these vertices can produce a valid face */
#define CHECK_VERTS(_v) \
     ((*_v[0] != *_v[1]) || (*_v[0] == NULL)) && \
     ((*_v[0] != *_v[2]) || (*_v[2] == NULL)) && \
     ((*_v[1] != *_v[2]) || (*_v[1] == NULL))

/**
 * R T _ D S P _ T E S S
 *
 * Returns -
 * -1 failure
 * 0 OK.  *r points to nmgregion that holds this tessellation.
 */
int
rt_dsp_tess(struct nmgregion **r, struct model *m, struct rt_db_internal *ip, const struct rt_tess_tol *UNUSED(ttol), const struct bn_tol *tol)
{
    struct rt_dsp_internal *dsp_ip;
    struct shell *s;
    int xlim;
    int ylim;
    int x, y;
    point_t pt[4];
    point_t tmp_pt;
    int base_vert_count;
    struct vertex **base_verts;
    struct vertex **verts[3];
    struct vertex *hole_verts[3];
    struct faceuse *fu;
    struct faceuse *base_fu;
    struct vertex **strip1Verts;
    struct vertex **strip2Verts;
    int base_vert_no1;
    int base_vert_no2;
    int has_holes=0;

    if (RT_G_DEBUG & DEBUG_HF)
	bu_log("rt_dsp_tess()\n");

    /* do a bunch of checks to make sure all is well */

    RT_CK_DB_INTERNAL(ip);
    dsp_ip = (struct rt_dsp_internal *)ip->idb_ptr;
    RT_DSP_CK_MAGIC(dsp_ip);

    switch(dsp_ip->dsp_datasrc) {
	case RT_DSP_SRC_FILE:
	case RT_DSP_SRC_V4_FILE:
	    if (!dsp_ip->dsp_mp) {
		bu_log("WARNING: Cannot find data file for displacement map (DSP)\n");
		if (bu_vls_addr(&dsp_ip->dsp_name)) {
		    bu_log("         DSP data file [%s] not found or empty\n", bu_vls_addr(&dsp_ip->dsp_name));
		} else {
		    bu_log("         DSP data file not found or not specified\n");
		}
		return -1;
	    }
	    BU_CK_MAPPED_FILE(dsp_ip->dsp_mp);
	    break;
	case RT_DSP_SRC_OBJ:
	    if (!dsp_ip->dsp_bip) {
		bu_log("WARNING: Cannot find data object for displacement map (DSP)\n");
		if (bu_vls_addr(&dsp_ip->dsp_name)) {
		    bu_log("         DSP data object [%s] not found or empty\n", bu_vls_addr(&dsp_ip->dsp_name));
		} else {
		    bu_log("         DSP data object not found or not specified\n");
		}
		return -1;
	    }
	    RT_CK_DB_INTERNAL(dsp_ip->dsp_bip);
	    RT_CK_BINUNIF(dsp_ip->dsp_bip->idb_ptr);
	    break;
    }


    xlim = dsp_ip->dsp_xcnt - 1;
    ylim = dsp_ip->dsp_ycnt - 1;

    /* Base_verts will contain the vertices for the base face ordered
     * correctly to create the base face.
     * Cannot simply use the four corners, because that would not
     * create a valid NMG.
     * base_verts[0] is at (0, 0)
     * base_verts[ylim] is at (0, ylim)
     * base_verts[ylim+xlim] is at (xlim, ylim)
     * base_verts[2*ylim+xlim] is at (xlim, 0)
     * base_verts[2*ylim+2*xlim-x] is at (x, 0)
     *
     * strip1Verts and strip2Verts are temporary storage for vertices
     * along the top of the dsp. For each strip of triangles at a
     * given x value, strip1Verts[y] is the vertex at (x, y, h) and
     * strip2Verts[y] is the vertex at (x+1, y, h), where h is the DSP
     * value at that point.  After each strip of faces is created,
     * strip2Verts is copied to strip1Verts and strip2Verts is set to
     * all NULLs.
     */

    /* malloc space for the vertices */
    base_vert_count = 2*xlim + 2*ylim;
    base_verts = bu_calloc(base_vert_count, sizeof(struct vertex *), "base verts");
    strip1Verts = bu_calloc(ylim+1, sizeof(struct vertex *), "strip1Verts");
    strip2Verts = bu_calloc(ylim+1, sizeof(struct vertex *), "strip2Verts");

    /* Make region, empty shell, vertex */
    *r = nmg_mrsv(m);
    s = BU_LIST_FIRST(shell, &(*r)->s_hd);

    /* make the base face */
    base_fu = nmg_cface(s, base_verts, base_vert_count);

    /* assign geometry to the base_verts */
    /* start with x=0 edge */
    for (y=0 ; y<=ylim ; y++) {
	VSET(tmp_pt, 0, y, 0);
	MAT4X3PNT(pt[0], dsp_ip->dsp_stom, tmp_pt);
	nmg_vertex_gv(base_verts[y], pt[0]);
    }
    /* now do y=ylim edge */
    for (x=1 ; x<=xlim ; x++) {
	VSET(tmp_pt, x, ylim, 0);
	MAT4X3PNT(pt[0], dsp_ip->dsp_stom, tmp_pt);
	nmg_vertex_gv(base_verts[ylim+x], pt[0]);
    }
    /* now do x=xlim edge */
    for (y=0 ; y<ylim ; y++) {
	VSET(tmp_pt, xlim, y, 0);
	MAT4X3PNT(pt[0], dsp_ip->dsp_stom, tmp_pt);
	nmg_vertex_gv(base_verts[2*ylim+xlim-y], pt[0]);
    }
    /* now do y=0 edge */
    for (x=1 ; x<xlim ; x++) {
	VSET(tmp_pt, x, 0, 0);
	MAT4X3PNT(pt[0], dsp_ip->dsp_stom, tmp_pt);
	nmg_vertex_gv(base_verts[2*(xlim+ylim)-x], pt[0]);
    }
    if (nmg_fu_planeeqn(base_fu, tol) < 0) {
	bu_log("Failed to make base face\n");
	bu_free(base_verts, "base verts");
	bu_free(strip1Verts, "strip 1 verts");
	bu_free(strip2Verts, "strip 2 verts");
	return -1; /* FAIL */
    }

    /* if a displacement on the edge (x=0) is zero then strip1Verts at
     * that point is the corresponding base_vert
     */
    for (y=0 ; y<=ylim ; y++) {
	if (DSP(dsp_ip, 0, y) == 0) {
	    strip1Verts[y] = base_verts[y];
	}
    }

    /* make faces along x=0 plane */
    for (y=1 ; y<=ylim ; y++) {
	verts[0] = &base_verts[y-1];
	verts[1] = &strip1Verts[y-1];
	verts[2] = &strip1Verts[y];
	if (CHECK_VERTS(verts)) {
	    fu = nmg_cmface(s, verts, 3);
	    if (y == 1 && strip1Verts[0]->vg_p == NULL) {
		VSET(tmp_pt, 0, 0, DSP(dsp_ip, 0, 0));
		MAT4X3PNT(pt[0], dsp_ip->dsp_stom, tmp_pt);
		nmg_vertex_gv(strip1Verts[0], pt[0]);
	    }
	    if (strip1Verts[y]->vg_p == NULL) {
		VSET(tmp_pt, 0, y, DSP(dsp_ip, 0, y));
		MAT4X3PNT(pt[0], dsp_ip->dsp_stom, tmp_pt);
		nmg_vertex_gv(strip1Verts[y], pt[0]);
	    }
	    if (nmg_fu_planeeqn(fu, tol) < 0) {
		bu_log("Failed to make x=0 face at y=%d\n", y);
		bu_free(base_verts, "base verts");
		bu_free(strip1Verts, "strip 1 verts");
		bu_free(strip2Verts, "strip 2 verts");
		return -1; /* FAIL */
	    }
	}
	verts[0] = &base_verts[y-1];
	verts[1] = &strip1Verts[y];
	verts[2] = &base_verts[y];
	if (CHECK_VERTS(verts)) {
	    fu = nmg_cmface(s, verts, 3);
	    if (strip1Verts[y]->vg_p == NULL) {
		VSET(tmp_pt, 0, y, DSP(dsp_ip, 0, y));
		MAT4X3PNT(pt[0], dsp_ip->dsp_stom, tmp_pt);
		nmg_vertex_gv(strip1Verts[y], pt[0]);
	    }
	    if (nmg_fu_planeeqn(fu, tol) < 0) {
		bu_log("Failed to make x=0 face at y=%d\n", y);
		bu_free(base_verts, "base verts");
		bu_free(strip1Verts, "strip 1 verts");
		bu_free(strip2Verts, "strip 2 verts");
		return -1; /* FAIL */
	    }
	}
    }

    /* make each strip of triangles. Make two triangles for each cell
     * (x, y)<->(x+1, y+1). Also make the vertical faces at y=0 and
     * y=ylim.
     */
    for (x=0 ; x<xlim ; x++) {
	/* set strip2Verts to base_verts values where the strip2Vert
	 * is above a base_vert and DSP value is 0
	 */
	if ((x+1) == xlim) {
	    for (y=0 ; y<=ylim ; y++) {
		if (DSP(dsp_ip, xlim, y) == 0) {
		    strip2Verts[y] = base_verts[2*ylim + xlim - y];
		}
	    }
	} else {
	    if (DSP(dsp_ip, x+1, 0) == 0) {
		strip2Verts[0] = base_verts[2*(ylim+xlim)-(x+1)];
	    }
	    if (DSP(dsp_ip, x+1, ylim) == 0) {
		strip2Verts[ylim] = base_verts[ylim+x+1];
	    }
	}

	/* make the faces at y=0 for this strip */
	if (x == 0) {
	    base_vert_no1 = 0;
	    base_vert_no2 = 2*(ylim+xlim)-1;
	} else {
	    base_vert_no1 = 2*(ylim + xlim) - x;
	    base_vert_no2 = base_vert_no1 - 1;
	}

	verts[0] = &base_verts[base_vert_no1];
	verts[1] = &strip2Verts[0];
	verts[2] = &strip1Verts[0];
	if (CHECK_VERTS(verts)) {
	    fu = nmg_cmface(s, verts, 3);
	    VSET(tmp_pt, x+1, 0, DSP(dsp_ip, x+1, 0));
	    MAT4X3PNT(pt[0], dsp_ip->dsp_stom, tmp_pt);
	    nmg_vertex_gv(strip2Verts[0], pt[0]);
	    if (nmg_fu_planeeqn(fu, tol) < 0) {
		bu_log("Failed to make first face at x=%d, y=%d\n", x, 0);
		bu_free(base_verts, "base verts");
		bu_free(strip1Verts, "strip 1 verts");
		bu_free(strip2Verts, "strip 2 verts");
		return -1; /* FAIL */
	    }
	}

	verts[0] = &base_verts[base_vert_no1];
	verts[1] = &base_verts[base_vert_no2];
	verts[2] = &strip2Verts[0];
	if (CHECK_VERTS(verts)) {
	    fu = nmg_cmface(s, verts, 3);
	    if (nmg_fu_planeeqn(fu, tol) < 0) {
		bu_log("Failed to make second face at x=%d, y=%d\n", x, 0);
		bu_free(base_verts, "base verts");
		bu_free(strip1Verts, "strip 1 verts");
		bu_free(strip2Verts, "strip 2 verts");
		return -1; /* FAIL */
	    }
	    if (strip2Verts[0]->vg_p == NULL) {
		VSET(tmp_pt, x+1, 0, DSP(dsp_ip, x+1, 0));
		MAT4X3PNT(pt[0], dsp_ip->dsp_stom, tmp_pt);
		nmg_vertex_gv(strip2Verts[0], pt[0]);
	    }
	}

	/* make the top faces for this strip */
	for (y=0 ; y<ylim ; y++) {
	    int dir;
	    /* get the cut direction for this cell */
	    dir = get_cut_dir(dsp_ip, x, y, xlim, ylim);

	    if (dir == DSP_CUT_DIR_llUR) {
		verts[0] = &strip1Verts[y];
		verts[1] = &strip2Verts[y];
		verts[2] = &strip2Verts[y+1];
		if (CHECK_VERTS(verts)) {
		    if (DSP(dsp_ip, x, y) == 0 && DSP(dsp_ip, x+1, y) == 0 && DSP(dsp_ip, x+1, y+1) == 0) {
			/* make a hole, instead of a face */
			hole_verts[0] = strip1Verts[y];
			hole_verts[1] = strip2Verts[y];
			hole_verts[2] = strip2Verts[y+1];
			nmg_add_loop_to_face(s, base_fu, hole_verts, 3, OT_OPPOSITE);
			has_holes = 1;
			VSET(tmp_pt, x+1, y+1, DSP(dsp_ip, x+1, y+1));
			MAT4X3PNT(pt[0], dsp_ip->dsp_stom, tmp_pt);
			nmg_vertex_gv(hole_verts[2], pt[0]);
			strip2Verts[y+1] = hole_verts[2];
		    } else {
			fu = nmg_cmface(s, verts, 3);
			VSET(tmp_pt, x+1, y+1, DSP(dsp_ip, x+1, y+1));
			MAT4X3PNT(pt[0], dsp_ip->dsp_stom, tmp_pt);
			nmg_vertex_gv(strip2Verts[y+1], pt[0]);
			if (nmg_fu_planeeqn(fu, tol) < 0) {
			    bu_log("Failed to make first top face at x=%d, y=%d\n", x, y);
			    bu_free(base_verts, "base verts");
			    bu_free(strip1Verts, "strip 1 verts");
			    bu_free(strip2Verts, "strip 2 verts");
			    return -1; /* FAIL */
			}
		    }
		}
	    } else {
		verts[0] = &strip1Verts[y+1];
		verts[1] = &strip1Verts[y];
		verts[2] = &strip2Verts[y];
		if (CHECK_VERTS(verts)) {
		    if (DSP(dsp_ip, x, y+1) == 0 && DSP(dsp_ip, x, y) == 0 && DSP(dsp_ip, x+1, y) == 0) {
			/* make a hole, instead of a face */
			hole_verts[0] = strip1Verts[y+1];
			hole_verts[1] = strip1Verts[y];
			hole_verts[2] = strip2Verts[y];
			nmg_add_loop_to_face(s, base_fu, hole_verts, 3, OT_OPPOSITE);
			has_holes = 1;
		    } else {
			fu = nmg_cmface(s, verts, 3);
			if (nmg_fu_planeeqn(fu, tol) < 0) {
			    bu_log("Failed to make first top face at x=%d, y=%d\n", x, y);
			    bu_free(base_verts, "base verts");
			    bu_free(strip1Verts, "strip 1 verts");
			    bu_free(strip2Verts, "strip 2 verts");
			    return -1; /* FAIL */
			}
		    }
		}
	    }


	    if (dir == DSP_CUT_DIR_llUR) {
		verts[0] = &strip1Verts[y];
		verts[1] = &strip2Verts[y+1];
		verts[2] = &strip1Verts[y+1];
		if (CHECK_VERTS(verts)) {
		    if (DSP(dsp_ip, x, y) == 0 && DSP(dsp_ip, x+1, y+1) == 0 && DSP(dsp_ip, x, y+1) == 0) {
			/* make a hole, instead of a face */
			hole_verts[0] = strip1Verts[y];
			hole_verts[1] = strip2Verts[y+1];
			hole_verts[2] = strip1Verts[y+1];
			nmg_add_loop_to_face(s, base_fu, hole_verts, 3, OT_OPPOSITE);
			has_holes = 1;
			VSET(tmp_pt, x+1, y+1, DSP(dsp_ip, x+1, y+1));
			MAT4X3PNT(pt[0], dsp_ip->dsp_stom, tmp_pt);
			nmg_vertex_gv(hole_verts[1], pt[0]);
			strip2Verts[y+1] = hole_verts[1];
		    } else {
			fu = nmg_cmface(s, verts, 3);
			VSET(tmp_pt, x+1, y+1, DSP(dsp_ip, x+1, y+1));
			MAT4X3PNT(pt[0], dsp_ip->dsp_stom, tmp_pt);
			nmg_vertex_gv(strip2Verts[y+1], pt[0]);
			if (nmg_fu_planeeqn(fu, tol) < 0) {
			    bu_log("Failed to make second top face at x=%d, y=%d\n", x, y);
			    bu_free(base_verts, "base verts");
			    bu_free(strip1Verts, "strip 1 verts");
			    bu_free(strip2Verts, "strip 2 verts");
			    return -1; /* FAIL */
			}
		    }
		}
	    } else {
		verts[0] = &strip2Verts[y];
		verts[1] = &strip2Verts[y+1];
		verts[2] = &strip1Verts[y+1];
		if (CHECK_VERTS(verts)) {
		    if (DSP(dsp_ip, x+1, y) == 0 && DSP(dsp_ip, x+1, y+1) == 0 && DSP(dsp_ip, x, y+1) == 0) {
			/* make a hole, instead of a face */
			hole_verts[0] = strip2Verts[y];
			hole_verts[1] = strip2Verts[y+1];
			hole_verts[2] = strip1Verts[y+1];
			nmg_add_loop_to_face(s, base_fu, hole_verts, 3, OT_OPPOSITE);
			has_holes = 1;
			VSET(tmp_pt, x+1, y+1, DSP(dsp_ip, x+1, y+1));
			MAT4X3PNT(pt[0], dsp_ip->dsp_stom, tmp_pt);
			nmg_vertex_gv(hole_verts[1], pt[0]);
			strip2Verts[y+1] = hole_verts[1];
		    } else {
			fu = nmg_cmface(s, verts, 3);
			VSET(tmp_pt, x+1, y+1, DSP(dsp_ip, x+1, y+1));
			MAT4X3PNT(pt[0], dsp_ip->dsp_stom, tmp_pt);
			nmg_vertex_gv(strip2Verts[y+1], pt[0]);
			if (nmg_fu_planeeqn(fu, tol) < 0) {
			    bu_log("Failed to make second top face at x=%d, y=%d\n", x, y);
			    bu_free(base_verts, "base verts");
			    bu_free(strip1Verts, "strip 1 verts");
			    bu_free(strip2Verts, "strip 2 verts");
			    return -1; /* FAIL */
			}
		    }
		}
	    }
	}

	/* make the faces at the y=ylim plane for this strip */
	verts[0] = &strip1Verts[ylim];
	verts[1] = &strip2Verts[ylim];
	verts[2] = &base_verts[ylim+x+1];
	if (CHECK_VERTS(verts)) {
	    fu = nmg_cmface(s, verts, 3);
	    if (nmg_fu_planeeqn(fu, tol) < 0) {
		bu_log("Failed to make first face at x=%d, y=ylim\n", x);
		bu_free(base_verts, "base verts");
		bu_free(strip1Verts, "strip 1 verts");
		bu_free(strip2Verts, "strip 2 verts");
		return -1; /* FAIL */
	    }
	}

	verts[0] = &base_verts[ylim+x+1];
	verts[1] = &base_verts[ylim+x];
	verts[2] = &strip1Verts[ylim];
	if (CHECK_VERTS(verts)) {
	    fu = nmg_cmface(s, verts, 3);
	    if (nmg_fu_planeeqn(fu, tol) < 0) {
		bu_log("Failed to make second face at x=%d, y=ylim\n", x);
		bu_free(base_verts, "base verts");
		bu_free(strip1Verts, "strip 1 verts");
		bu_free(strip2Verts, "strip 2 verts");
		return -1; /* FAIL */
	    }
	}

	/* copy strip2 to strip1, set strip2 to all NULLs */
	for (y=0 ; y<=ylim ; y++) {
	    strip1Verts[y] = strip2Verts[y];
	    strip2Verts[y] = (struct vertex *)NULL;
	}
    }

    /* make faces at x=xlim plane */
    for (y=0 ; y<ylim ; y++) {
	base_vert_no1 = 2*ylim+xlim-y;
	verts[0] = &base_verts[base_vert_no1];
	verts[1] = &base_verts[base_vert_no1-1];
	verts[2] = &strip1Verts[y];
	if (CHECK_VERTS(verts)) {
	    fu = nmg_cmface(s, verts, 3);
	    if (nmg_fu_planeeqn(fu, tol) < 0) {
		bu_log("Failed to make first face at x=xlim, y=%d\n", y);
		bu_free(base_verts, "base verts");
		bu_free(strip1Verts, "strip 1 verts");
		bu_free(strip2Verts, "strip 2 verts");
		return -1; /* FAIL */
	    }
	}

	verts[0] = &strip1Verts[y];
	verts[1] = &base_verts[base_vert_no1-1];
	verts[2] = &strip1Verts[y+1];
	if (CHECK_VERTS(verts)) {
	    fu = nmg_cmface(s, verts, 3);
	    if (nmg_fu_planeeqn(fu, tol) < 0) {
		bu_log("Failed to make second face at x=xlim, y=%d\n", y);
		bu_free(base_verts, "base verts");
		bu_free(strip1Verts, "strip 1 verts");
		bu_free(strip2Verts, "strip 2 verts");
		return -1; /* FAIL */
	    }
	}
    }


    bu_free(base_verts, "base verts");
    bu_free(strip1Verts, "strip 1 verts");
    bu_free(strip2Verts, "strip 2 verts");

    if (has_holes) {
	/* do a bunch of joining and splitting of touching loops to
	 * get the base face in a state that the triangulator can
	 * handle
	 */
	struct loopuse *lu;
	for (BU_LIST_FOR(lu, loopuse, &base_fu->lu_hd)) {
	    nmg_join_touchingloops(lu);
	}
	for (BU_LIST_FOR(lu, loopuse, &base_fu->lu_hd)) {
	    if (lu->orientation != OT_UNSPEC) continue;
	    nmg_lu_reorient(lu);
	}
	nmg_kill_cracks(s);
	for (BU_LIST_FOR(lu, loopuse, &base_fu->lu_hd)) {
	    nmg_split_touchingloops(lu, tol);
	}
	for (BU_LIST_FOR(lu, loopuse, &base_fu->lu_hd)) {
	    if (lu->orientation != OT_UNSPEC) continue;
	    nmg_lu_reorient(lu);
	}
	nmg_kill_cracks(s);
	for (BU_LIST_FOR(lu, loopuse, &base_fu->lu_hd)) {
	    nmg_join_touchingloops(lu);
	}
	for (BU_LIST_FOR(lu, loopuse, &base_fu->lu_hd)) {
	    if (lu->orientation != OT_UNSPEC) continue;
	    nmg_lu_reorient(lu);
	}
	nmg_kill_cracks(s);
    }

    /* Mark edges as real */
    (void)nmg_mark_edges_real(&s->l.magic);

    /* Compute "geometry" for region and shell */
    nmg_region_a(*r, tol);

    /* sanity check */
    nmg_make_faces_within_tol(s, tol);

    return 0;
}


/**
 * G E T _ F I L E _ D A T A
 *
 * Retrieve data for DSP from external file.
 * Returns:
 * 0 Success
 * !0 Failure
 */
HIDDEN int
get_file_data(struct rt_dsp_internal *dsp_ip, const struct db_i *dbip)
{
    struct bu_mapped_file *mf;
    int count, in_cookie, out_cookie;

    RT_DSP_CK_MAGIC(dsp_ip);
    RT_CK_DBI(dbip);

    /* get file */
    mf = dsp_ip->dsp_mp =
	bu_open_mapped_file_with_path(dbip->dbi_filepath,
				      bu_vls_addr(&dsp_ip->dsp_name), "dsp");
    if (!mf) {
	bu_log("mapped file open failed\n");
	return 0;
    }

    if ((size_t)dsp_ip->dsp_mp->buflen != (size_t)(dsp_ip->dsp_xcnt*dsp_ip->dsp_ycnt*2)) {
	bu_log("DSP buffer wrong size: %zu s/b %zu ",
	       dsp_ip->dsp_mp->buflen, dsp_ip->dsp_xcnt*dsp_ip->dsp_ycnt*2);
	return -1;
    }

    in_cookie = bu_cv_cookie("nus"); /* data is network unsigned short */
    out_cookie = bu_cv_cookie("hus");

    if (bu_cv_optimize(in_cookie) != bu_cv_optimize(out_cookie)) {
	size_t got;
	/* if we're on a little-endian machine we convert the input
	 * file from network to host format
	 */
	count = dsp_ip->dsp_xcnt * dsp_ip->dsp_ycnt;
	mf->apbuflen = count * sizeof(unsigned short);
	mf->apbuf = bu_malloc(mf->apbuflen, "apbuf");

	got = bu_cv_w_cookie(mf->apbuf, out_cookie, mf->apbuflen,
			     mf->buf,    in_cookie, count);
	if (got != (size_t)count) {
	    bu_log("got %zu != count %d", got, count);
	    bu_bomb("\n");
	}
	dsp_ip->dsp_buf = dsp_ip->dsp_mp->apbuf;
    } else {
	dsp_ip->dsp_buf = dsp_ip->dsp_mp->buf;
    }
    return 0;
}


/**
 * G E T _ O B J _ D A T A
 *
 * Retrieve data for DSP from a database object.
 */
HIDDEN int
get_obj_data(struct rt_dsp_internal *dsp_ip, const struct db_i *dbip)
{
    struct rt_binunif_internal *bip;
    int in_cookie, out_cookie;
    size_t got;
    int ret;

    BU_GETSTRUCT(dsp_ip->dsp_bip, rt_db_internal);

    ret = rt_retrieve_binunif (dsp_ip->dsp_bip, dbip, bu_vls_addr(&dsp_ip->dsp_name));
    if (ret)
	return -1;

    if (RT_G_DEBUG & DEBUG_HF) {
	bu_log("db_internal magic: 0x%08x  major: %d  minor: %d\n",
	       dsp_ip->dsp_bip->idb_magic,
	       dsp_ip->dsp_bip->idb_major_type,
	       dsp_ip->dsp_bip->idb_minor_type);
    }

    bip = dsp_ip->dsp_bip->idb_ptr;

    if (RT_G_DEBUG & DEBUG_HF)
	bu_log("binunif magic: 0x%08x  type: %d count:%zu data[0]:%u\n",
	       bip->magic, bip->type, bip->count, bip->u.uint16[0]);


    in_cookie = bu_cv_cookie("nus"); /* data is network unsigned short */
    out_cookie = bu_cv_cookie("hus");

    if (bu_cv_optimize(in_cookie) != bu_cv_optimize(out_cookie)) {
	/* if we're on a little-endian machine we convert the input
	 * file from network to host format
	 */

	got = bu_cv_w_cookie(bip->u.uint16, out_cookie,
			     bip->count * sizeof(unsigned short),
			     bip->u.uint16, in_cookie, bip->count);

	if (got != bip->count) {
	    bu_log("got %zu != count %zu", got, bip->count);
	    bu_bomb("\n");
	}
    }

    dsp_ip->dsp_buf = bip->u.uint16;
    return 0;
}


/**
 * D S P _ G E T _ D A T A
 *
 * Handle things common to both the v4 and v5 database.
 *
 * This include applying the modelling transform, and fetching the
 * actual data.
 *
 * Return:
 * 0 success
 * !0 failure
 */
HIDDEN int
dsp_get_data(struct rt_dsp_internal *dsp_ip, const mat_t mat, const struct db_i *dbip)
{
    mat_t tmp;
    char *p;

    /* Apply Modeling transform */
    MAT_COPY(tmp, dsp_ip->dsp_stom);
    bn_mat_mul(dsp_ip->dsp_stom, mat, tmp);

    bn_mat_inv(dsp_ip->dsp_mtos, dsp_ip->dsp_stom);

    p = bu_vls_addr(&dsp_ip->dsp_name);

    switch (dsp_ip->dsp_datasrc) {
	case RT_DSP_SRC_FILE:
	case RT_DSP_SRC_V4_FILE:
	    /* Retrieve the data from an external file */
	    if (RT_G_DEBUG & DEBUG_HF)
		bu_log("getting data from file \"%s\"\n", p);

	    if (get_file_data(dsp_ip, dbip) != 0) {
		p = "file";
	    } else {
		return 0;
	    }

	    break;
	case RT_DSP_SRC_OBJ:
	    /* Retrieve the data from an internal db object */
	    if (RT_G_DEBUG & DEBUG_HF)
		bu_log("getting data from object \"%s\"\n", p);

	    if (get_obj_data(dsp_ip, dbip) != 0) {
		p = "object";
	    } else {
		RT_CK_DB_INTERNAL(dsp_ip->dsp_bip);
		RT_CK_BINUNIF(dsp_ip->dsp_bip->idb_ptr);
		return 0;
	    }
	    break;
	default:
	    bu_log("%s:%d Odd dsp data src '%c' s/b '%c' or '%c'\n",
		   __FILE__, __LINE__, dsp_ip->dsp_datasrc,
		   RT_DSP_SRC_FILE, RT_DSP_SRC_OBJ);
	    return -1;
    }

    bu_log("Cannot retrieve DSP data from %s \"%s\"\n", p,
	   bu_vls_addr(&dsp_ip->dsp_name));

    dsp_ip->dsp_mp = (struct bu_mapped_file *)NULL;
    dsp_ip->dsp_buf = bu_calloc(sizeof(short),
				dsp_ip->dsp_xcnt*dsp_ip->dsp_ycnt,
				"dsp fake data");

    return 1;
}


/**
 * R T _ D S P _ I M P O R T
 *
 * Import an DSP from the database format to the internal format.
 * Apply modeling transformations as well.
 */
int
rt_dsp_import4(struct rt_db_internal *ip, const struct bu_external *ep, register const fastf_t *mat, const struct db_i *dbip)
{
    struct rt_dsp_internal *dsp_ip;
    union record *rp;
    struct bu_vls str;


    if (RT_G_DEBUG & DEBUG_HF)
	bu_log("rt_dsp_import4_v4()\n");


#define IMPORT_FAIL(_s) \
	bu_log("rt_dsp_import4(%d) '%s' %s\n", __LINE__, \
	       bu_vls_addr(&dsp_ip->dsp_name), _s);\
	bu_free((char *)dsp_ip, "rt_dsp_import4: dsp_ip"); \
	ip->idb_type = ID_NULL; \
	ip->idb_ptr = (genptr_t)NULL; \
	return -2

    BU_CK_EXTERNAL(ep);
    rp = (union record *)ep->ext_buf;

    if (RT_G_DEBUG & DEBUG_HF)
	bu_log("rt_dsp_import4(%s)\n", rp->ss.ss_args);
    /*----------------------------------------------------------------------*/


    /* Check record type */
    if (rp->u_id != DBID_STRSOL) {
	bu_log("rt_dsp_import4: defective record\n");
	return -1;
    }

    RT_CK_DB_INTERNAL(ip);
    ip->idb_major_type = DB5_MAJORTYPE_BRLCAD;
    ip->idb_type = ID_DSP;
    ip->idb_meth = &rt_functab[ID_DSP];
    ip->idb_ptr = bu_malloc(sizeof(struct rt_dsp_internal), "rt_dsp_internal");
    dsp_ip = (struct rt_dsp_internal *)ip->idb_ptr;
    dsp_ip->magic = RT_DSP_INTERNAL_MAGIC;

    /* set defaults */
    /* XXX bu_struct_parse does not set the null?
     * memset(&dsp_ip->dsp_name[0], 0, DSP_NAME_LEN);
     */
    dsp_ip->dsp_xcnt = dsp_ip->dsp_ycnt = 0;

    dsp_ip->dsp_cuttype = DSP_CUT_DIR_ADAPT;
    dsp_ip->dsp_smooth = 1;
    MAT_IDN(dsp_ip->dsp_stom);
    MAT_IDN(dsp_ip->dsp_mtos);

    bu_vls_init(&str);
    bu_vls_strcpy(&str, rp->ss.ss_args);
    if (bu_struct_parse(&str, rt_dsp_parse, (char *)dsp_ip) < 0) {
	if (BU_VLS_IS_INITIALIZED(&str)) bu_vls_free(&str);
	IMPORT_FAIL("parse error");
    }


    /* Validate parameters */
    if (dsp_ip->dsp_xcnt == 0 || dsp_ip->dsp_ycnt == 0) {
	IMPORT_FAIL("zero dimension on map");
    }

    if (mat == NULL) mat = bn_mat_identity;
    if (dsp_get_data(dsp_ip, mat, dbip)!=0) {
	IMPORT_FAIL("unable to load displacement map data");
    }

    if (RT_G_DEBUG & DEBUG_HF) {
	bu_vls_trunc(&str, 0);
	bu_vls_struct_print(&str, rt_dsp_ptab, (char *)dsp_ip);
	bu_log("  imported as(%s)\n", bu_vls_addr(&str));

    }

    if (BU_VLS_IS_INITIALIZED(&str)) bu_vls_free(&str);

    RT_CK_DB_INTERNAL(dsp_ip->dsp_bip);
    RT_CK_BINUNIF(dsp_ip->dsp_bip->idb_ptr);

    return 0;			/* OK */
}


/**
 * R T _ D S P _ E X P O R T
 *
 * The name is added by the caller, in the usual place.
 */
int
rt_dsp_export4(struct bu_external *ep, const struct rt_db_internal *ip, double local2mm, const struct db_i *dbip)
{
    struct rt_dsp_internal *dsp_ip;
    struct rt_dsp_internal dsp;
    union record *rec;
    struct bu_vls str;

    if (dbip) RT_CK_DBI(dbip);

    RT_CK_DB_INTERNAL(ip);
    if (ip->idb_type != ID_DSP) return -1;
    dsp_ip = (struct rt_dsp_internal *)ip->idb_ptr;
    RT_DSP_CK_MAGIC(dsp_ip);
    BU_CK_VLS(&dsp_ip->dsp_name);

    BU_CK_EXTERNAL(ep);
    ep->ext_nbytes = sizeof(union record)*DB_SS_NGRAN;
    ep->ext_buf = bu_calloc(1, ep->ext_nbytes, "dsp external");
    rec = (union record *)ep->ext_buf;

    dsp = *dsp_ip;	/* struct copy */

    /* Since libwdb users may want to operate in units other than mm,
     * we offer the opportunity to scale the solid (to get it into mm)
     * on the way out.
     */
    dsp.dsp_stom[15] *= local2mm;

    bu_vls_init(&str);
    bu_vls_struct_print(&str, rt_dsp_ptab, (char *)&dsp);
    if (RT_G_DEBUG & DEBUG_HF)
	bu_log("rt_dsp_export4_v4(%s)\n", bu_vls_addr(&str));

    rec->ss.ss_id = DBID_STRSOL;
    bu_strlcpy(rec->ss.ss_keyword, "dsp", sizeof(rec->ss.ss_keyword));
    bu_strlcpy(rec->ss.ss_args, bu_vls_addr(&str), DB_SS_LEN);


    if (BU_VLS_IS_INITIALIZED(&str)) bu_vls_free(&str);

    return 0;
}


/**
 * R T _ D S P _ I M P O R T 5
 *
 * Import an DSP from the database format to the internal format.
 * Apply modeling transformations as well.
 */
int
rt_dsp_import5(struct rt_db_internal *ip, const struct bu_external *ep, register const fastf_t *mat, const struct db_i *dbip, struct resource *resp)
{
    struct rt_dsp_internal *dsp_ip;
    unsigned char *cp;

    if (resp) RT_CK_RESOURCE(resp);

    if (RT_G_DEBUG & DEBUG_HF)
	bu_log("rt_dsp_import4_v5()\n");


    BU_CK_EXTERNAL(ep);

    BU_ASSERT_LONG(ep->ext_nbytes, >, 141);

    RT_CK_DB_INTERNAL(ip);

    ip->idb_major_type = DB5_MAJORTYPE_BRLCAD;
    ip->idb_type = ID_DSP;
    ip->idb_meth = &rt_functab[ID_DSP];
    dsp_ip = ip->idb_ptr = bu_malloc(sizeof(struct rt_dsp_internal), "rt_dsp_internal");
    memset(dsp_ip, 0, sizeof(*dsp_ip));

    dsp_ip->magic = RT_DSP_INTERNAL_MAGIC;

    /* get x, y counts */
    cp = (unsigned char *)ep->ext_buf;

    dsp_ip->dsp_xcnt = ntohl(*(uint32_t *)cp);
    cp += SIZEOF_NETWORK_LONG;
    if (dsp_ip->dsp_xcnt < 2) {
	bu_log("%s:%d DSP X dimension (%zu) < 2 \n",
	       __FILE__, __LINE__,
	       dsp_ip->dsp_xcnt);
    }


    dsp_ip->dsp_ycnt = ntohl(*(uint32_t *)cp);
    cp += SIZEOF_NETWORK_LONG;
    if (dsp_ip->dsp_ycnt < 2) {
	bu_log("%s:%d DSP X dimension (%zu) < 2 \n",
	       __FILE__, __LINE__,
	       dsp_ip->dsp_ycnt);
    }

    /* convert matrix */
    ntohd((unsigned char *)dsp_ip->dsp_stom, cp, 16);
    cp += SIZEOF_NETWORK_DOUBLE * 16;
    bn_mat_inv(dsp_ip->dsp_mtos, dsp_ip->dsp_stom);

    /* convert smooth flag */
    dsp_ip->dsp_smooth = ntohs(*(uint16_t *)cp);
    cp += SIZEOF_NETWORK_SHORT;

    dsp_ip->dsp_datasrc = *cp;
    cp++;
    switch (dsp_ip->dsp_datasrc) {
	case RT_DSP_SRC_V4_FILE:
	case RT_DSP_SRC_FILE:
	case RT_DSP_SRC_OBJ:
	    break;
	default:
	    bu_log("%s:%d bogus DSP datasrc '%c' (%d)\n",
		   __FILE__, __LINE__,
		   dsp_ip->dsp_datasrc, dsp_ip->dsp_datasrc);
	    break;
    }


    dsp_ip->dsp_cuttype = *cp;
    cp++;
    switch (dsp_ip->dsp_cuttype) {
	case DSP_CUT_DIR_ADAPT:
	case DSP_CUT_DIR_llUR:
	case DSP_CUT_DIR_ULlr:
	    break;
	default:
	    bu_log("%s:%d bogus DSP cut type '%c' (%d)\n",
		   __FILE__, __LINE__,
		   dsp_ip->dsp_cuttype, dsp_ip->dsp_cuttype);
	    break;
    }


    /* convert name of data location */
    bu_vls_init(&dsp_ip->dsp_name);
    bu_vls_strncpy(&dsp_ip->dsp_name, (char *)cp,
		   ep->ext_nbytes - (cp - (unsigned char *)ep->ext_buf));

    if (mat == NULL) mat = bn_mat_identity;
    if (dsp_get_data(dsp_ip, mat, dbip)!=0) {
	IMPORT_FAIL("unable to load displacement map data");
    }

    return 0; /* OK */
}


/**
 * R T _ D S P _ E X P O R T 5
 *
 * The name is added by the caller, in the usual place.
 */
int
rt_dsp_export5(struct bu_external *ep, const struct rt_db_internal *ip, double local2mm, const struct db_i *dbip, struct resource *resp)
{
    struct rt_dsp_internal *dsp_ip;
    unsigned long name_len;
    unsigned char *cp;
    size_t rem;

    if (resp) RT_CK_RESOURCE(resp);
    if (dbip) RT_CK_DBI(dbip);

    RT_CK_DB_INTERNAL(ip);
    if (ip->idb_type != ID_DSP) return -1;
    dsp_ip = (struct rt_dsp_internal *)ip->idb_ptr;
    RT_DSP_CK_MAGIC(dsp_ip);

    if (RT_G_DEBUG & DEBUG_HF)
	bu_log("rt_dsp_export4_v5()\n");

    name_len = bu_vls_strlen(&dsp_ip->dsp_name) + 1;

    BU_CK_EXTERNAL(ep);

    ep->ext_nbytes =
	SIZEOF_NETWORK_LONG * 2 +
	SIZEOF_NETWORK_DOUBLE * ELEMENTS_PER_MAT +
	SIZEOF_NETWORK_SHORT +
	2 + name_len;

    ep->ext_buf = bu_malloc(ep->ext_nbytes, "dsp external");
    cp = (unsigned char *)ep->ext_buf;
    rem = ep->ext_nbytes;

    memset(ep->ext_buf, 0, ep->ext_nbytes);

    /* Now we fill the buffer with the data, making sure everything is
     * converted to Big-Endian IEEE
     */

    *(uint32_t *)cp = htonl((uint32_t)dsp_ip->dsp_xcnt);
    cp += SIZEOF_NETWORK_LONG;
    rem -= SIZEOF_NETWORK_LONG;

    *(uint32_t *)cp = htonl((uint32_t)dsp_ip->dsp_ycnt);
    cp += SIZEOF_NETWORK_LONG;
    rem -= SIZEOF_NETWORK_LONG;

    /* Since libwdb users may want to operate in units other than mm,
     * we offer the opportunity to scale the solid (to get it into mm)
     * on the way out.
     */
    dsp_ip->dsp_stom[15] *= local2mm;

    htond(cp, (unsigned char *)dsp_ip->dsp_stom, 16);
    cp += SIZEOF_NETWORK_DOUBLE * 16;
    rem -= SIZEOF_NETWORK_DOUBLE * 16;

    *(uint16_t *)cp = htons((uint16_t)dsp_ip->dsp_smooth);
    cp += SIZEOF_NETWORK_SHORT;
    rem -= SIZEOF_NETWORK_SHORT;

    switch (dsp_ip->dsp_datasrc) {
	case RT_DSP_SRC_V4_FILE:
	case RT_DSP_SRC_FILE:
	case RT_DSP_SRC_OBJ:
	    *cp = dsp_ip->dsp_datasrc;
	    break;
	default:
	    *cp = RT_DSP_SRC_FILE;
	    break;
    }
    cp++;
    rem--;

    switch (dsp_ip->dsp_cuttype) {
	case DSP_CUT_DIR_ADAPT:
	case DSP_CUT_DIR_llUR:
	case DSP_CUT_DIR_ULlr:
	    *cp = dsp_ip->dsp_cuttype;
	    break;
	default:
	    *cp = DSP_CUT_DIR_ADAPT;
	    break;
    }
    cp++;
    rem--;

    bu_strlcpy((char *)cp, bu_vls_addr(&dsp_ip->dsp_name), rem);

    return 0; /* OK */
}


/**
 * R T _ D S P _ D E S C R I B E
 *
 * Make human-readable formatted presentation of this solid.  First
 * line describes type of solid.  Additional lines are indented one
 * tab, and give parameter values.
 */
int
rt_dsp_describe(struct bu_vls *str,
		const struct rt_db_internal *ip,
		int UNUSED(verbose),
		double UNUSED(mm2local),
		struct resource *resp,
		struct db_i *db_ip)
{
    register struct rt_dsp_internal *dsp_ip =
	(struct rt_dsp_internal *)ip->idb_ptr;
    struct bu_vls vls;

    if (resp) RT_CK_RESOURCE(resp);
    if (db_ip) RT_CK_DBI(db_ip);

    bu_vls_init(&vls);

    if (RT_G_DEBUG & DEBUG_HF)
	bu_log("rt_dsp_describe()\n");

    RT_DSP_CK_MAGIC(dsp_ip);

    switch (db_version(db_ip)) {
	case 4:
	    dsp_print_v4(&vls, dsp_ip);
	    break;
	case 5:
	    dsp_print_v5(&vls, dsp_ip);
	    break;
    }

    bu_vls_vlscat(str, &vls);

    if (BU_VLS_IS_INITIALIZED(&vls)) bu_vls_free(&vls);

    return 0;
}


/**
 * R T _ D S P _ I F R E E
 *
 * Free the storage associated with the rt_db_internal version of this
 * solid.
 */
void
rt_dsp_ifree(struct rt_db_internal *ip)
{
    register struct rt_dsp_internal *dsp_ip;

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
	dsp_ip->dsp_bip->idb_meth->ft_ifree((struct rt_db_internal *) dsp_ip->dsp_bip);
    }

    dsp_ip->magic = 0;			/* sanity */
    dsp_ip->dsp_mp = (struct bu_mapped_file *)0;

    if (BU_VLS_IS_INITIALIZED(&dsp_ip->dsp_name))
	bu_vls_free(&dsp_ip->dsp_name);
    else
	bu_log("Freeing Bogus DSP, VLS string not initialized\n");


    bu_free((char *)dsp_ip, "dsp ifree");
    ip->idb_ptr = GENPTR_NULL;	/* sanity */
}


HIDDEN void
hook_verify(const struct bu_structparse *sp,
	    const char *sp_name,
	    genptr_t base,
	    char *UNUSED(p))
{
    struct rt_dsp_internal *dsp_ip = (struct rt_dsp_internal *)base;

    if (!sp || !sp_name || !base) return;

    if (BU_STR_EQUAL(sp_name, "src")) {
	switch (dsp_ip->dsp_datasrc) {
	    case RT_DSP_SRC_V4_FILE:
	    case RT_DSP_SRC_FILE:
	    case RT_DSP_SRC_OBJ:
		break;
	    default:
		bu_log("Error in DSP data source field s/b one of [%c%c%c]\n",
		       RT_DSP_SRC_V4_FILE,
		       RT_DSP_SRC_FILE,
		       RT_DSP_SRC_OBJ);
		break;
	}

    } else if (BU_STR_EQUAL(sp_name, "w")) {
	if (dsp_ip->dsp_xcnt == 0)
	    bu_log("Error in DSP width dimension (0)\n");
    } else if (BU_STR_EQUAL(sp_name, "n")) {
	if (dsp_ip->dsp_ycnt == 0)
	    bu_log("Error in DSP width dimension (0)\n");
    } else if (BU_STR_EQUAL(sp_name, "cut")) {
	switch (dsp_ip->dsp_cuttype) {
	    case DSP_CUT_DIR_ADAPT:
	    case DSP_CUT_DIR_llUR:
	    case DSP_CUT_DIR_ULlr:
		break;
	    default:
		bu_log("Error in DSP cut type: %c s/b one of [%c%c%c]\n",
		       dsp_ip->dsp_cuttype,
		       DSP_CUT_DIR_ADAPT,
		       DSP_CUT_DIR_llUR,
		       DSP_CUT_DIR_ULlr);
		break;
	}
    }
}


const struct bu_structparse fake_dsp_printab[] = {
    {"%c",  1, "src", DSP_O(dsp_datasrc), hook_verify, NULL, NULL },
    {"%V",  1, "name", DSP_O(dsp_name), BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    {"%d",  1, "w",  DSP_O(dsp_xcnt), hook_verify, NULL, NULL },
    {"%d",  1, "n",  DSP_O(dsp_ycnt), hook_verify, NULL, NULL },
    {"%i",  1, "sm", DSP_O(dsp_smooth), BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    {"%c",  1, "cut", DSP_O(dsp_cuttype), hook_verify, NULL, NULL },
    {"%f", 16, "stom", DSP_AO(dsp_stom), hook_verify, NULL, NULL },
    {"",    0, (char *)0, 0, BU_STRUCTPARSE_FUNC_NULL, NULL, NULL }
};


/**
 * R T _ D S P _ G E T
 *
 * This is the generic routine to be listed in rt_functab[].ft_get for
 * those solid types which are fully described by their ft_parsetab
 * entry.
 *
 * 'attr' is specified to retrieve only one attribute, rather than
 * all.
 *
 * Example:  "db get ell.s B" to get only the B vector.
 */
int
rt_dsp_get(struct bu_vls *logstr, const struct rt_db_internal *intern, const char *attr)
{
    register const struct bu_structparse *sp = NULL;
    const struct rt_dsp_internal *dsp_ip;

    /* XXX if dsp_datasrc == RT_DSP_SRC_V4_FILE we have a V4 dsp
     * otherwise, a V5 dsp.  Take advantage of this.
     */

    RT_CK_DB_INTERNAL(intern);
    dsp_ip = (struct rt_dsp_internal *)intern->idb_ptr;

    if (attr == (char *)0) {
	/* Print out solid type and all attributes */

	bu_vls_printf(logstr, "dsp");

	switch (dsp_ip->dsp_datasrc) {
	    case RT_DSP_SRC_V4_FILE:
		sp = rt_dsp_ptab;
		break;
	    case RT_DSP_SRC_FILE:
	    case RT_DSP_SRC_OBJ:
		sp = fake_dsp_printab;
		break;
	}

	while (sp && sp->sp_name != NULL) {
	    bu_vls_printf(logstr, " %s ", sp->sp_name);
	    bu_vls_struct_item(logstr, sp, (char *)dsp_ip, ' ');
	    ++sp;
	}

	return BRLCAD_OK;
    }

    switch (dsp_ip->dsp_datasrc) {
	case RT_DSP_SRC_V4_FILE:
	    sp = rt_dsp_ptab;
	case RT_DSP_SRC_FILE:
	case RT_DSP_SRC_OBJ:
	    sp = fake_dsp_printab;
	    break;
    }

    if (bu_vls_struct_item_named(logstr, sp, attr,
				 (char *)dsp_ip, ' ') < 0) {
	bu_vls_printf(logstr,
		      "Objects of type %s do not have a %s attribute.",
		      "dsp", attr);
	return BRLCAD_ERROR;
    }

    return BRLCAD_OK;
}


/**
 * R T _ P A R S E T A B _ T C L A D J U S T
 *
 * For those solids entirely defined by their parsetab.  Invoked via
 * rt_functab[].ft_adjust()
 */
int
rt_dsp_adjust(struct bu_vls *logstr, struct rt_db_internal *intern, int argc, const char **argv)
{
    register const struct bu_structparse *sp = NULL;
    const struct rt_dsp_internal *dsp_ip;

    RT_CK_DB_INTERNAL(intern);
    dsp_ip = (struct rt_dsp_internal *)intern->idb_ptr;
    RT_DSP_CK_MAGIC(dsp_ip);
    BU_CK_VLS(&dsp_ip->dsp_name);

    switch (dsp_ip->dsp_datasrc) {
	case RT_DSP_SRC_V4_FILE:
	    sp = rt_dsp_ptab;
	case RT_DSP_SRC_FILE:
	case RT_DSP_SRC_OBJ:
	    sp = fake_dsp_printab;
	    break;
    }

    if (! sp) return BRLCAD_ERROR;

    return bu_structparse_argv(logstr, argc, argv, sp, (char *)intern->idb_ptr);
}


void
rt_dsp_make(const struct rt_functab *ftp, struct rt_db_internal *intern)
{
    struct rt_dsp_internal *dsp;

    intern->idb_major_type = DB5_MAJORTYPE_BRLCAD;
    intern->idb_type = ID_DSP;

    BU_ASSERT(&rt_functab[intern->idb_type] == ftp);
    intern->idb_meth = ftp;

    dsp =(struct rt_dsp_internal *)bu_calloc(sizeof(struct rt_dsp_internal), 1, "rt_dsp_internal");

    intern->idb_ptr = (genptr_t)dsp;
    dsp->magic = RT_DSP_INTERNAL_MAGIC;
    bu_vls_init(&dsp->dsp_name);
    bu_vls_strcpy(&dsp->dsp_name, "/dev/null");
    dsp->dsp_cuttype = DSP_CUT_DIR_ADAPT;
    MAT_IDN(dsp->dsp_mtos);
    MAT_IDN(dsp->dsp_stom);
    dsp->dsp_datasrc = RT_DSP_SRC_FILE;

}


/**
 * R T _ D S P _ P A R A M S
 *
 */
int
rt_dsp_params(struct pc_pc_set *ps, const struct rt_db_internal *ip)
{
    if (!ps) return 0;
    RT_CK_DB_INTERNAL(ip);

    return 0;			/* OK */
}


/**
 * S W A P _ C E L L _ P T S
 *
 */
HIDDEN int
swap_cell_pts(int A[3],
	      int B[3],
	      int C[3],
	      int D[3],
	      struct dsp_specific *dsp)

{
    switch (dsp->dsp_i.dsp_cuttype) {
	case DSP_CUT_DIR_llUR:
	    return 0;
	    break;

	case DSP_CUT_DIR_ADAPT: {
	    int lo[2], hi[2];
	    double h1, h2, h3, h4;
	    double cAD, cBC;  /* curvature in direction AD, and BC */


	    /*
	     * We look at the points in the diagonal next cells to
	     * determine the curvature along each diagonal of this
	     * cell.  This cell is divided into two triangles by
	     * cutting across the cell in the direction of least
	     * curvature.
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

	    lo[X] = A[X] - 1;
	    lo[Y] = A[Y] - 1;
	    hi[X] = D[X] + 1;
	    hi[Y] = D[Y] + 1;

	    /* a little bounds checking */
	    if (lo[X] < 0) lo[X] = 0;
	    if (lo[Y] < 0) lo[Y] = 0;
	    if (hi[X] > dsp->xsiz)
		hi[X] = dsp->xsiz;

	    if (hi[Y] > dsp->ysiz)
		hi[Y] = dsp->ysiz;

	    /* compute curvature along the A->D direction */
	    h1 = DSP(&dsp->dsp_i, lo[X], lo[Y]);
	    h2 = A[Z];
	    h3 = D[Z];
	    h4 = DSP(&dsp->dsp_i, hi[X], hi[Y]);

	    cAD = fabs(h3 + h1 - 2*h2) + fabs(h4 + h2 - 2*h3);


	    /* compute curvature along the B->C direction */
	    h1 = DSP(&dsp->dsp_i, hi[X], lo[Y]);
	    h2 = B[Z];
	    h3 = C[Z];
	    h4 = DSP(&dsp->dsp_i, lo[X], hi[Y]);

	    cBC = fabs(h3 + h1 - 2*h2) + fabs(h4 + h2 - 2*h3);

	    if (cAD < cBC) {
		/* A-D cut is fine, no need to permute */
		if (RT_G_DEBUG & DEBUG_HF)
		    bu_log("A-D cut (no swap)\n");

		return 0;
	    }

	    /* fallthrough */

	}
	case DSP_CUT_DIR_ULlr: {
	    /* prefer the B-C cut */
	    int tmp[3];

	    VMOVE(tmp, A);
	    VMOVE(A, B);
	    VMOVE(B, tmp);

	    VMOVE(tmp, D);
	    VMOVE(D, C);
	    VMOVE(C, tmp);

	    if (RT_G_DEBUG & DEBUG_HF)
		bu_log("B-C cut (swap)\n");
	}
	    return 0;
	    break;
    }
    bu_log("%s:%d Unknown DSP cut direction: %d\n",
	   __FILE__, __LINE__, dsp->dsp_i.dsp_cuttype);
    bu_bomb("");
    /* not reached */
    return -1;
}


/**
 * triangle with origin at A, X axis along AB, Y axis along AC
 *
 *	B--A	C	A--B	   C
 *	   |	|	|	   |
 *	   C	A--B	C	B--A
 */
int
project_pt(point_t out,
	   int A[3],
	   int B[3],
	   int C[3],
	   point_t pt)
{
    int dx, dy;
    double alpha, beta, x, y;
    vect_t AB, AC;

    if (RT_G_DEBUG & DEBUG_HF) {
	bu_log("     pt %g %g %g\n", V3ARGS(pt));
	bu_log(" origin %d %d %d\n", V3ARGS(A));
	bu_log("    Bpt %d %d %d\n", V3ARGS(B));
	bu_log("    Cpt %d %d %d\n", V3ARGS(C));
    }
    if ((B[X] - A[X]) < 0) dx = -1;
    else dx = 1;
    if ((C[Y] - A[Y]) < 0) dy = -1;
    else dy = 1;

    x = pt[X] - A[X];
    y = pt[Y] - A[Y];

    alpha = x * dx;
    beta = y * dy;
    if (RT_G_DEBUG & DEBUG_HF) {
	bu_log("alpha:%g beta:%g\n", alpha, beta);
    }
    if (alpha < -SMALL_FASTF) {
	bu_log("Alpha negative: %g  x:%g  dx:%d\n", alpha, x, dx);
    }
    if (beta < -SMALL_FASTF) {
	bu_log("Beta negative: %g  x:%g  dx:%d\n", beta, y, dy);
    }
    CLAMP(alpha, 0.0, 1.0);
    CLAMP(beta, 0.0, 1.0);

    if (RT_G_DEBUG & DEBUG_HF) {
	bu_log("x:%g y:%g dx:%d dy:%d alpha:%g beta:%g\n",
	       x, y, dx, dy, alpha, beta);
    }
    if ((alpha+beta) > (1.0+SMALL_FASTF)) {
	if (RT_G_DEBUG & DEBUG_HF) bu_log("Not this triangle\n");
	return 1;
    }

    VSUB2(AB, B, A);
    VSUB2(AC, C, A);

    VJOIN2(out, A, alpha, AB, beta, AC);
    if (RT_G_DEBUG & DEBUG_HF) bu_log("out: %g %g %g\n", V3ARGS(out));

    return 0;
}


/**
 * D S P _ P O S
 *
 * Given an arbitrary point return the projection of that point onto
 * the surface of the DSP.  If the point is outside the bounds of the
 * DSP then it will be projected to the nearest edge of the DSP.
 * Return the cell number and the height above/below the plane
 */
int
dsp_pos(point_t out, /* return value */
	struct soltab *stp, /* pointer to soltab for dsp */
	point_t p)	/* model space point */
{
    struct dsp_specific *dsp;
    point_t pt, tri_pt;
    unsigned int x, y;
    int A[3], B[3], C[3], D[3];

    /* init points */
    VSET(pt, 0, 0, 0);
    VSET(tri_pt, 0, 0, 0);

    RT_CK_SOLTAB(stp);

    if (stp->st_id != ID_DSP) return 1;

    dsp = (struct dsp_specific *)stp->st_specific;

    MAT4X3PNT(pt, dsp->dsp_i.dsp_mtos, p);


    if (RT_G_DEBUG & DEBUG_HF) {
	VPRINT("user_point", p);
	VPRINT("dsp_point", pt);
    }
    x = pt[X];
    y = pt[Y];

    V_MIN(x, XSIZ(dsp)-1);
    V_MIN(y, YSIZ(dsp)-1);

    if (RT_G_DEBUG & DEBUG_HF)
	bu_log("x:%d y:%d\n", x, y);

    VSET(A, x, y, DSP(&dsp->dsp_i, x, y));
    x += 1;
    VSET(B, x, y, DSP(&dsp->dsp_i, x, y));
    y += 1;
    VSET(D, x, y, DSP(&dsp->dsp_i, x, y));
    x -= 1;
    VSET(C, x, y, DSP(&dsp->dsp_i, x, y));
    y -= 1;

    if (RT_G_DEBUG & DEBUG_HF) {
	bu_log(" A: %d %d %d\n", V3ARGS(A));
	bu_log(" B: %d %d %d\n", V3ARGS(B));
	bu_log(" C: %d %d %d\n", V3ARGS(C));
	bu_log(" D: %d %d %d\n", V3ARGS(D));
    }

    swap_cell_pts(A, B, C, D, dsp);
    if (RT_G_DEBUG & DEBUG_HF) {
	bu_log(" A: %d %d %d\n", V3ARGS(A));
	bu_log(" B: %d %d %d\n", V3ARGS(B));
	bu_log(" C: %d %d %d\n", V3ARGS(C));
	bu_log(" D: %d %d %d\n", V3ARGS(D));
    }

    if (project_pt(tri_pt, B, A, D, pt)) {
	if (project_pt(tri_pt, C, D, A, pt)) {
	    bu_log("Now what???\n");
	}
    }

    MAT4X3PNT(out, dsp->dsp_i.dsp_stom, tri_pt);
    if (RT_G_DEBUG & DEBUG_HF) {
	VPRINT("user_pt", p);
	VPRINT("tri_pt", tri_pt);
	VPRINT("model_space", out);
	bu_log("X: %d Y:%d\n", x, y);
    }

    return 0;
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

/** @} */
/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
