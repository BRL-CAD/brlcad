/*			C H E C K . C
 * BRL-CAD
 *
 * Copyright (c) 2018 United States Government as represented by
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

#include "common.h"

#include <stdlib.h>
#include <string.h>

#include "bu/debug.h"
#include "bu/getopt.h"
#include "bu/str.h"
#include "bu/log.h"
#include "bu/parallel.h"
#include "bn/plot3.h"
#include "bn/str.h"
#include "raytrace.h"

#include "./ged_private.h"
#include "analyze.h"

#define MAX_WIDTH (32*1024)
#define OVLP_TOL 0.1

struct overlap_list {
    struct overlap_list *next;	/* next one */
    char *reg1;			/* overlapping region 1 */
    char *reg2;			/* overlapping region 2 */
    size_t count;		/* number of time reported */
    double maxdepth;		/* maximum overlap depth */
};


struct overlaps_context {
    struct overlap_list **list;
    int numberOfOverlaps;
    struct bn_vlblock *vbp;
    struct bu_list *vhead;
};


HIDDEN void
log_overlaps(const char *reg1, const char *reg2, double depth, vect_t ihit, vect_t ohit, void *context)
{
    struct overlaps_context *callbackdata = (struct overlaps_context*)context;
    struct overlap_list **olist = (struct overlap_list **) callbackdata->list;
    struct overlap_list *prev_ol = (struct overlap_list *)0;
    struct overlap_list *op;		/* overlap list */
    struct overlap_list *new_op;


    BN_ADD_VLIST(callbackdata->vbp->free_vlist_hd, callbackdata->vhead, ihit, BN_VLIST_LINE_MOVE);
    BN_ADD_VLIST(callbackdata->vbp->free_vlist_hd, callbackdata->vhead, ohit, BN_VLIST_LINE_DRAW);
    callbackdata->numberOfOverlaps++;

    BU_ALLOC(new_op, struct overlap_list);

    for (op=*olist; op; prev_ol=op, op=op->next) {
	if ((BU_STR_EQUAL(reg1, op->reg1)) && (BU_STR_EQUAL(reg2, op->reg2))) {
	    op->count++;
	    if (depth > op->maxdepth)
		op->maxdepth = depth;
	    bu_free((char *) new_op, "overlap list");
	    return;	/* already on list */
	}
    }

    /* we have a new overlapping region pair */
    op = new_op;
    if (*olist)		/* previous entry exists */
	prev_ol->next = op;
    else
	*olist = op;	/* finally initialize root */

    BU_ALLOC(op->reg1,char);
    BU_ALLOC(op->reg2,char);
    bu_strlcpy(op->reg1, reg1, sizeof(op->reg1));
    bu_strlcpy(op->reg2, reg2, sizeof(op->reg2));
    op->maxdepth = depth;
    op->next = NULL;
    op->count = 1;
}


HIDDEN void
overlapHandler(const struct xray *rp, const struct partition *pp, const struct bu_ptbl *regiontable, void *context)
{
    struct region *reg1 = (struct region *)NULL;
    struct region *reg2 = (struct region *)NULL;
    register double depth;
    register struct hit *ihitp = pp->pt_inhit;
    register struct hit *ohitp = pp->pt_outhit;
    vect_t ihit;
    vect_t ohit;

    VJOIN1(ihit, rp->r_pt, ihitp->hit_dist, rp->r_dir);
    VJOIN1(ohit, rp->r_pt, ohitp->hit_dist, rp->r_dir);

    depth = ohitp->hit_dist - ihitp->hit_dist;
    if (depth < OVLP_TOL)
	return;

    reg1 = (struct region *)BU_PTBL_GET(regiontable, 0);
    RT_CK_REGION(reg1);
    reg2 = (struct region *)BU_PTBL_GET(regiontable, 1);
    RT_CK_REGION(reg2);
    bu_semaphore_acquire(BU_SEM_SYSCALL);
    log_overlaps(reg1->reg_name, reg2->reg_name, depth, ihit, ohit, context);
    bu_semaphore_release(BU_SEM_SYSCALL);
}


HIDDEN int
check_grid_setup (int *cell_newsize, 
		  size_t *width, 
		  size_t *height,
		  fastf_t *cell_width,
		  fastf_t *cell_height,
		  fastf_t aspect,
		  fastf_t viewsize,
		  point_t eye_model,
		  mat_t Viewrotscale,
		  mat_t model2view,
		  mat_t view2model)
{
    mat_t toEye;

    if (viewsize <= 0.0) {
	bu_log("viewsize <= 0");
	return GED_ERROR;
    }
    /* model2view takes us to eye_model location & orientation */
    MAT_IDN(toEye);
    MAT_DELTAS_VEC_NEG(toEye, eye_model);
    Viewrotscale[15] = 0.5*viewsize;	/* Viewscale */
    bn_mat_mul(model2view, Viewrotscale, toEye);
    bn_mat_inv(view2model, model2view);

    /* Determine grid cell size and number of pixels */
    if (*cell_newsize) {
	if ((*cell_width) <= 0.0)
	    *cell_width = *cell_height;
	if ((*cell_height) <= 0.0) 
	    *cell_height = *cell_width;
	*width = (viewsize / (*cell_width)) + 0.99;
	*height = (viewsize / ((*cell_height)*aspect)) + 0.99;
	*cell_newsize = 0;
    } else {
	/* Chop -1.0..+1.0 range into parts */
	*cell_width = viewsize / (*width);
	*cell_height = viewsize / ((*height)*aspect);
    }

    if ((*cell_width) <= 0 || (*cell_width) >= INFINITY ||
	(*cell_height) <= 0 || (*cell_height) >= INFINITY) {
	bu_log("grid_setup: cell size ERROR (%g, %g) mm\n", *cell_width, *cell_height);
	return GED_ERROR;
    }

    if ((*width) <= 0 || (*height) <= 0) {
	bu_log("grid_setup: ERROR bad image size (%zu, %zu)\n", width, height);
	return GED_ERROR;
    }

    return GED_OK;
}


HIDDEN int
check_do_ae(double azim,
	    double elev,
	    struct rt_i *rtip,
	    mat_t Viewrotscale,
	    fastf_t *viewsize,
	    fastf_t aspect,
	    mat_t model2view,
	    mat_t view2model,
	    point_t eye_model)
{
    vect_t temp;
    vect_t diag;
    mat_t toEye;
    fastf_t eye_backoff = (fastf_t)M_SQRT2;
    if (rtip == NULL) {
	bu_log("analov_do_ae: ERROR: rtip is null");
	return GED_ERROR;
    }

    if (rtip->nsolids <= 0) {
	bu_log("ERROR: no primitives active\n");
	return GED_ERROR;
    }

    if (rtip->nregions <= 0) {
	bu_log("ERROR: no regions active\n");
	return GED_ERROR;
    }

    if (rtip->mdl_max[X] >= INFINITY) {
	bu_log("do_ae: infinite model bounds? setting a unit minimum\n");
	VSETALL(rtip->mdl_min, -1);
    }
    if (rtip->mdl_max[X] <= -INFINITY) {
	bu_log("do_ae: infinite model bounds? setting a unit maximum\n");
	VSETALL(rtip->mdl_max, 1);
    }

    /*
     * Enlarge the model RPP just slightly, to avoid nasty effects
     * with a solid's face being exactly on the edge NOTE: This code
     * is duplicated out of librt/tree.c/rt_prep(), and has to appear
     * here to enable the viewsize calculation to match the final RPP.
     */
    rtip->mdl_min[X] = floor(rtip->mdl_min[X]);
    rtip->mdl_min[Y] = floor(rtip->mdl_min[Y]);
    rtip->mdl_min[Z] = floor(rtip->mdl_min[Z]);
    rtip->mdl_max[X] = ceil(rtip->mdl_max[X]);
    rtip->mdl_max[Y] = ceil(rtip->mdl_max[Y]);
    rtip->mdl_max[Z] = ceil(rtip->mdl_max[Z]);

    MAT_IDN(Viewrotscale);
    bn_mat_angles(Viewrotscale, 270.0+elev, 0.0, 270.0-azim);

    /* Look at the center of the model */
    MAT_IDN(toEye);
    toEye[MDX] = -((rtip->mdl_max[X]+rtip->mdl_min[X])/2.0);
    toEye[MDY] = -((rtip->mdl_max[Y]+rtip->mdl_min[Y])/2.0);
    toEye[MDZ] = -((rtip->mdl_max[Z]+rtip->mdl_min[Z])/2.0);

    /* Fit a sphere to the model RPP, diameter is viewsize, unless
     * viewsize command used to override.
     */
    if ((*viewsize) <= 0) {
	VSUB2(diag, rtip->mdl_max, rtip->mdl_min);
	*viewsize = MAGNITUDE(diag);
	if (aspect > 1) {
	    /* don't clip any of the image when autoscaling */
	    *viewsize *= aspect;
	}
    }

    /* sanity check: make sure viewsize still isn't zero in case
     * bounding box is empty, otherwise bn_mat_int() will bomb.
     */
    if (*viewsize < 0 || ZERO(*viewsize)) {
	*viewsize = 2.0; /* arbitrary so Viewrotscale is normal */
    }

    Viewrotscale[15] = 0.5*(*viewsize);	/* Viewscale */
    bn_mat_mul(model2view, Viewrotscale, toEye);
    bn_mat_inv(view2model, model2view);
    VSET(temp, 0, 0, eye_backoff);
    MAT4X3PNT(eye_model, view2model, temp);
    return GED_OK;
}


int
ged_check_overlaps(struct ged *gedp, int argc, const char *argv[])
{
    int i, c;
    size_t index;
    int getfromview = 1;
    int debug = 0;			/* debug flag */
    size_t width = 0;			/* # of pixels in X */
    size_t height = 0;			/* # of lines in Y */
    fastf_t cell_width = (fastf_t)0.0;	/* model space grid cell width */
    fastf_t cell_height = (fastf_t)0.0;	/* model space grid cell height */
    int cell_newsize = 0;		/* new grid cell size */
    fastf_t azimuth = 35.0, elevation = 25.0;
    size_t npsw = bu_avail_cpus();	/* number of worker PSWs to run */
    fastf_t aspect = (fastf_t)1.0;	/* view aspect ratio X/Y (needs to be 1.0 for g/G options) */
    size_t nobjs = 0;			/* Number of cmd-line treetops */
    const char **objtab;		/* array of treetop strings */
    char *rt_options = ".:a:de:g:n:s:w:G:P:V:h?";
    char *check_help = "Options:\n -s #\t\tSquare grid size in pixels (default 512)\n -w # -n #\t\tGrid size width and height in pixels\n -V #\t\tView (pixel) aspect ratio (width/height)\n -a #\t\tAzimuth in degrees\n -e #\t\tElevation in degrees\n -g #\t\tGrid cell width\n -G #\t\tGrid cell height\n -d\t\tDebug flag to print some information\n -P #\t\tSet number of processors\n\n";
    struct rt_i *rtip = NULL;

    size_t tnobjs = 0;
    size_t nvobjs = 0;
    char **tobjtab;
    char **objp;

    mat_t Viewrotscale = { (fastf_t)0.0, (fastf_t)0.0, (fastf_t)0.0, (fastf_t)0.0,
			   (fastf_t)0.0, (fastf_t)0.0, (fastf_t)0.0, (fastf_t)0.0,
			   (fastf_t)0.0, (fastf_t)0.0, (fastf_t)0.0, (fastf_t)0.0,
			   (fastf_t)0.0, (fastf_t)0.0, (fastf_t)0.0, (fastf_t)0.0};
    fastf_t viewsize = (fastf_t)0.0;
    mat_t model2view;
    mat_t view2model;
    point_t eye_model = {(fastf_t)0.0, (fastf_t)0.0, (fastf_t)0.0};
    quat_t quat;

    /* overlap specific variables */
    struct overlap_list *nextop=(struct overlap_list *)NULL;
    struct overlap_list *olist = NULL;
    struct overlap_list *op;
    struct overlaps_context overlapData;
    void *callbackData;
    /* end overlap specific variables */

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    bu_optind = 1;	/* restart parsing of args */

    while ((c=bu_getopt(argc, (char **)argv, rt_options)) != -1) {
	if (bu_optopt == '?')
	    c = 'h';
	switch (c) {
	    case 's':
		/* Square size */
		i = atoi(bu_optarg);
		if (i < 1 || i > MAX_WIDTH)
		    fprintf(stderr, "squaresize=%d out of range\n", i);
		else
		    width = height = i;
		break;
	    case 'n':
		i = atoi(bu_optarg);
		if (i < 1 || i > MAX_WIDTH)
		    fprintf(stderr, "height=%d out of range\n", i);
		else
		    height = i;
		break;
	    case 'w':
		i = atoi(bu_optarg);
		if (i < 1 || i > MAX_WIDTH)
		    fprintf(stderr, "width=%d out of range\n", i);
		else
		    width = i;
		break;
	    case 'g':
		cell_width = atof(bu_optarg);
		cell_newsize = 1;
		break;
	    case 'G':
		cell_height = atof(bu_optarg);
		cell_newsize = 1;
		break;

	    case 'a':
		/* Set azimuth */
		if (bn_decode_angle(&azimuth,bu_optarg) == 0) {
		    fprintf(stderr, "WARNING: Unexpected units for azimuth angle, using default value\n");
		}
		getfromview = 0;
		break;
	    case 'e':
		/* Set elevation */
		if (bn_decode_angle(&elevation,bu_optarg) == 0) {
		    fprintf(stderr, "WARNING: Unexpected units for elevation angle, using default value\n");
		}
		getfromview = 0;
		break;
	    case 'P':
		{
		    /* Number of parallel workers */
		    size_t avail_cpus;
		    avail_cpus = bu_avail_cpus();
		    npsw = atoi(bu_optarg);

		    if (npsw > avail_cpus) {
			fprintf(stderr, "Requesting %lu cpus, only %lu available.",
				(unsigned long)npsw, (unsigned long)avail_cpus);

			if ((bu_debug & BU_DEBUG_PARALLEL) ||
			    (RT_G_DEBUG & DEBUG_PARALLEL)) {
			    fprintf(stderr, "\nAllowing surplus cpus due to debug flag.\n");
			} else {
			    fprintf(stderr, "  Will use %lu.\n", (unsigned long)avail_cpus);
			    npsw = avail_cpus;
			}
		    }
		    if (npsw < 1 || npsw > MAX_PSW) {
			fprintf(stderr, "Numer of requested cpus (%lu) is out of range 1..%d", (unsigned long)npsw, MAX_PSW);

			if ((bu_debug & BU_DEBUG_PARALLEL) ||
			    (RT_G_DEBUG & DEBUG_PARALLEL)) {
			    fprintf(stderr, ", but allowing due to debug flag\n");
			} else {
			    fprintf(stderr, ", using -P1\n");
			    npsw = 1;
			}
		    }
		}
		break;
	    case 'V':
		{
		    /* View aspect */
		    fastf_t xx, yy;
		    register char *cp = bu_optarg;

		    xx = atof(cp);
		    while ((*cp >= '0' && *cp <= '9')
			   || *cp == '.') cp++;
		    while (*cp && (*cp < '0' || *cp > '9')) cp++;
		    yy = atof(cp);
		    if (ZERO(yy))
			aspect = xx;
		    else
			aspect = xx/yy;
		    if (aspect <= 0.0) {
			fprintf(stderr, "Bogus aspect %g, using 1.0\n", aspect);
			aspect = 1.0;
		    }
		}
		break;
	    case 'd':
		debug = 1;
		break;
	    case 'h':
		bu_vls_printf(gedp->ged_result_str, "Usage: %s [options] [objects]\n %s", argv[0], check_help);
		return GED_HELP;
		break;
	    default:		/* '?' 'h' */
		if(bu_optopt != 'h')
		    fprintf(stderr, "ERROR: argument missing or bad option specified\n");
		return 0;	/* BAD */
	}
    }

    /* If user gave no sizing info at all, use 512 as default */
    if (width <= 0 && cell_width <= 0)
	width = 512;
    if (height <= 0 && cell_height <= 0)
	height = 512;

    /* If user didn't provide an aspect ratio, use the image
     * dimensions ratio as a default.
     */
    if (aspect <= 0.0) {
	aspect = (fastf_t)width / (fastf_t)height;
    }

    nobjs = argc - bu_optind;
    objtab = argv + (bu_optind);

    if (nobjs <= 0){
	nvobjs = ged_count_tops(gedp);
    }

    tnobjs = nvobjs + nobjs;

    if (tnobjs <= 0) {
	bu_vls_printf(gedp->ged_result_str,"no objects specified or in view -- raytrace aborted\n");
	return GED_ERROR;
    }

    tobjtab = (char **)bu_calloc(tnobjs, sizeof(char *), "alloc tobjtab");
    objp = &tobjtab[0];

    /* copy all specified objects if any */
    for(index = 0; index<nobjs; index++)
	*objp++ = bu_strdup(objtab[index]);

    /* else copy all the objects in view if any */
    if (nobjs <= 0) {
	nvobjs = ged_build_tops(gedp, objp, &tobjtab[tnobjs]);
	/* now, as we know the exact number of objects in the view, check again for > 0 */
	if (nvobjs <= 0) {
	    bu_vls_printf(gedp->ged_result_str,"no objects specified or in view, aborting\n");
	    bu_free(tobjtab, "free tobjtab");
	    tobjtab = NULL;
	    return GED_ERROR;
	}
    }

    tnobjs = nvobjs + nobjs;

    rtip = rt_new_rti(gedp->ged_wdbp->dbip);

    for (index=0; index<tnobjs; index++) {
	if (rt_gettree(rtip, tobjtab[index]) < 0) {
	    bu_vls_printf(gedp->ged_result_str, "error: object '%s' does not exists, aborting\n", tobjtab[index]);
	    bu_free(tobjtab, "free tobjtab");
	    tobjtab = NULL;
	    rt_free_rti(rtip);
	    rtip = NULL;
	    return GED_ERROR;
	}
    }

    if (!nobjs && getfromview) {
	_ged_rt_set_eye_model(gedp, eye_model);
	viewsize =  gedp->ged_gvp->gv_size;
	quat_mat2quat(quat, gedp->ged_gvp->gv_rotation);
	quat_quat2mat(Viewrotscale, quat);
    } else {
	if (check_do_ae(azimuth, elevation, rtip, Viewrotscale, &viewsize, aspect, model2view, view2model, eye_model)) {
	    rt_free_rti(rtip);
	    rtip = NULL;
	    bu_free(tobjtab, "free tobjtab");
	    tobjtab = NULL;
	    return GED_ERROR;
	}
    }

    if (check_grid_setup(&cell_newsize, &width, &height, &cell_width, &cell_height, aspect, viewsize, eye_model, Viewrotscale, model2view, view2model)) {
	rt_free_rti(rtip);
	rtip = NULL;
	bu_free(tobjtab, "free tobjtab");
	tobjtab = NULL;
	return GED_ERROR;
    }

    if (debug) {
	vect_t work, temp;
	VSET(work, 0, 0, 1);
	MAT3X3VEC(temp, view2model, work);
	bn_ae_vec(&azimuth, &elevation, temp);
	bu_vls_printf(gedp->ged_result_str, "\nView: %g azimuth, %g elevation off of front view\n", azimuth, elevation);
	bu_vls_printf(gedp->ged_result_str, "Orientation: %g, %g, %g, %g\n", V4ARGS(quat));
	bu_vls_printf(gedp->ged_result_str, "Eye_pos: %g, %g, %g\n", V3ARGS(eye_model));
	bu_vls_printf(gedp->ged_result_str, "Size: %gmm\n", viewsize);
	bu_vls_printf(gedp->ged_result_str, "Grid: (%g, %g) mm, (%zu, %zu) pixels\n", cell_width, cell_height, width, height);
    }

    overlapData.list = &olist;
    overlapData.numberOfOverlaps = 0;
    overlapData.vbp = rt_vlblock_init();
    overlapData.vhead = bn_vlblock_find(overlapData.vbp, 0xFF, 0xFF, 0x00);
    callbackData = (void*)(&overlapData);

    if (analyze_overlaps(rtip, width, height, cell_width, cell_height, aspect, viewsize, model2view, npsw, overlapHandler, callbackData)) {
	rt_free_rti(rtip);
	rtip = NULL;
	bu_free(tobjtab, "free tobjtab");
	tobjtab = NULL;
	bn_vlblock_free(overlapData.vbp);
	return GED_ERROR;
    }

    /* show overlays */
    dl_set_flag(gedp->ged_gdp->gd_headDisplay, DOWN);
    _ged_cvt_vlblock_to_solids(gedp, overlapData.vbp, "OVERLAPS", 0);

    bu_vls_printf(gedp->ged_result_str, "\nNumber of Overlaps: %d\n", overlapData.numberOfOverlaps);
    for (op=olist; op; op=op->next) {
	bu_vls_printf(gedp->ged_result_str,"\t<%s, %s>: %zu overlap%c detected, maximum depth is %gmm\n", op->reg1, op->reg2, op->count, op->count>1 ? 's' : (char) 0, op->maxdepth);
    }

    /* free our structures */
    bn_vlblock_free(overlapData.vbp);

    op = olist;
    while (op) {
	/* free struct */
	nextop = op->next;
	bu_free(op, "overlap_list");
	bu_free(op->reg1,"reg1 name");
	bu_free(op->reg2,"reg2 name");
	op = nextop;
    }
    olist = (struct overlap_list *)NULL;

    /* Free tobjtab */
    bu_free(tobjtab, "free tobjtab");
    tobjtab = NULL;

    /* Release the ray-tracer instance */
    rt_free_rti(rtip);
    rtip = NULL;

    return GED_OK; /*all okay return code*/
}

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
