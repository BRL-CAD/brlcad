/*                      H I D E L I N E . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2011 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file mged/hideline.c
 *
 * Takes the vector list for the current display and raytraces along
 * those vectors.  If the first point hit in the model is the same as
 * that vector, continue the line through that point; otherwise, stop
 * drawing that vector or draw dotted line.  Produces Unix-plot type
 * output.
 *
 * The command is "H file.pl [stepsize] [%epsilon]". Stepsize is the
 * number of segments into which the window size should be broken.
 * %Epsilon specifies how close two points must be before they are
 * considered equal. A good values for stepsize and %epsilon are 128
 * and 1, respectively.
 *
 */

#include "common.h"

#include <stdio.h>
#include <string.h>

#include "bu.h"
#include "vmath.h"
#include "raytrace.h"
#include "solid.h"
#include "plot3.h"

#include "./mged.h"
#include "./mged_dm.h"


#define MAXOBJECTS 3000

#define MOVE(v) VMOVE(last_move, (v))

#define DRAW(v) { vect_t _a, _b;\
		  MAT4X3PNT(_a, view_state->vs_gvp->gv_model2view, last_move);\
		  MAT4X3PNT(_b, view_state->vs_gvp->gv_model2view, (v));\
		  pdv_3line(plotfp, _a, _b); }

extern struct db_i *dbip;	/* current database instance */

fastf_t epsilon;
vect_t aim_point;


/**
 * hit_headon - routine called by rt_shootray if ray hits model
 */
static int
hit_headon(struct application *ap, struct partition *PartHeadp, struct seg *UNUSED(segp))
{
    char diff_solid;
    vect_t diff;
    fastf_t len = 0.0;
    struct solid *sp = NULL;

    if (PartHeadp->pt_forw->pt_forw != PartHeadp)
	Tcl_AppendResult(INTERP, "hit_headon: multiple partitions\n", (char *)NULL);

    VJOIN1(PartHeadp->pt_forw->pt_inhit->hit_point, ap->a_ray.r_pt,
	   PartHeadp->pt_forw->pt_inhit->hit_dist, ap->a_ray.r_dir);
    VSUB2(diff, PartHeadp->pt_forw->pt_inhit->hit_point, aim_point);

    diff_solid = (FIRST_SOLID(sp) !=
		  PartHeadp->pt_forw->pt_inseg->seg_stp->st_dp);
    len = MAGNITUDE(diff);

    if (NEAR_ZERO(len, epsilon) || (diff_solid && VDOT(diff, ap->a_ray.r_dir) > 0))
	return 1;
    else
	return 0;
}


/**
 * hit_tangent - routine called by rt_shootray if ray misses model
 *
 * We know we are shooting at the model since we are aiming at the
 * vector list MGED created. However, shooting at an edge or shooting
 * tangent to a curve produces only one intersection point at which
 * time rt_shootray reports a miss. Therefore, this routine is really
 * a "hit" routine.
 */
static int
hit_tangent(struct application *UNUSED(ap))
{
    return 1;		/* always a hit */
}


/**
 * hit_overlap - called by rt_shootray if ray hits an overlap
 */
static int
hit_overlap(struct application *UNUSED(ap), struct partition *UNUSED(ph), struct region *UNUSED(r1), struct region *UNUSED(r2), struct partition *UNUSED(hp))
{
    return 0;		/* never a hit */
}


/**
 * F _ H I D E L I N E
 */
int
f_hideline(ClientData UNUSED(clientData), Tcl_Interp *interp, int argc, const char *argv[])
{
    FILE *plotfp = NULL;
    char *objname[MAXOBJECTS] = {0};
    char title[512] = {0};
    char visible = 0;
    fastf_t len = 0.0;
    fastf_t ratio = 0.0;
    fastf_t step = 0.0;
    fastf_t u = 0.0;
    int i = 0;
    int numobjs = 0;
    struct application a;
    struct bn_vlist *vp = NULL;
    struct ged_display_list *gdlp = NULL;
    struct ged_display_list *next_gdlp = NULL;
    struct resource resource;
    struct rt_i *rtip = NULL;
    struct solid *sp = NULL;
    vect_t dir = {0.0, 0.0, 0.0};
    vect_t last = {0.0, 0.0, 0.0};
    vect_t last_move = {0.0, 0.0, 0.0};
    vect_t temp = {0.0, 0.0, 0.0};

    CHECK_DBI_NULL;

    if (argc < 2 || 4 < argc) {
	struct bu_vls vls;

	bu_vls_init(&vls);
	bu_vls_printf(&vls, "help H");
	Tcl_Eval(interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return TCL_ERROR;
    }

    if ((plotfp = fopen(argv[1], "w")) == NULL) {
	Tcl_AppendResult(interp, "f_hideline: unable to open \"", argv[1],
			 "\" for writing.\n", (char *)NULL);
	return TCL_ERROR;
    }
    pl_space(plotfp, (int)GED_MIN, (int)GED_MIN, (int)GED_MAX, (int)GED_MAX);

    /* Build list of objects being viewed */
    numobjs = 0;

    gdlp = BU_LIST_NEXT(ged_display_list, &gedp->ged_gdp->gd_headDisplay);
    while (BU_LIST_NOT_HEAD(gdlp, &gedp->ged_gdp->gd_headDisplay)) {
	next_gdlp = BU_LIST_PNEXT(ged_display_list, gdlp);

	FOR_ALL_SOLIDS(sp, &gdlp->gdl_headSolid) {
	    for (i = 0; i < numobjs; i++) {
		if (objname[i] == FIRST_SOLID(sp)->d_namep)
		    break;
	    }
	    if (i == numobjs)
		objname[numobjs++] = FIRST_SOLID(sp)->d_namep;
	}

	gdlp = next_gdlp;
    }

    Tcl_AppendResult(interp, "Generating hidden-line drawing of the following regions:\n",
		     (char *)NULL);
    for (i = 0; i < numobjs; i++)
	Tcl_AppendResult(interp, "\t", objname[i], "\n", (char *)NULL);

    /* Initialization for librt */
    if ((rtip = rt_dirbuild(dbip->dbi_filename, title, sizeof(title))) == RTI_NULL) {
	Tcl_AppendResult(interp, "f_hideline: unable to open model file \"",
			 dbip->dbi_filename, "\"\n", (char *)NULL);
	return TCL_ERROR;
    }
    a.a_hit = hit_headon;
    a.a_miss = hit_tangent;
    a.a_overlap = hit_overlap;
    a.a_rt_i = rtip;
    a.a_resource = &resource;
    a.a_level = 0;
    a.a_onehit = 1;
    a.a_diverge = 0;
    a.a_rbeam = 0;

    if (argc > 2) {
	sscanf(argv[2], "%lf", &step);
	step = view_state->vs_gvp->gv_scale/step;
	sscanf(argv[3], "%lf", &epsilon);
	epsilon *= view_state->vs_gvp->gv_scale/100;
    } else {
	step = view_state->vs_gvp->gv_scale/256;
	epsilon = 0.1*view_state->vs_gvp->gv_scale;
    }

    for (i = 0; i < numobjs; i++) {
	if (rt_gettree(rtip, objname[i]) == -1) {
	    Tcl_AppendResult(interp, "f_hideline: rt_gettree failed on \"", objname[i], "\"\n", (char *)NULL);
	}
    }

    /* Crawl along the vectors raytracing as we go */
    VSET(temp, 0.0, 0.0, -1.0);				/* looking at model */
    MAT4X3VEC(a.a_ray.r_dir, view_state->vs_gvp->gv_view2model, temp);
    VUNITIZE(a.a_ray.r_dir);

    gdlp = BU_LIST_NEXT(ged_display_list, &gedp->ged_gdp->gd_headDisplay);
    while (BU_LIST_NOT_HEAD(gdlp, &gedp->ged_gdp->gd_headDisplay)) {
	next_gdlp = BU_LIST_PNEXT(ged_display_list, gdlp);

	FOR_ALL_SOLIDS(sp, &gdlp->gdl_headSolid) {

	    ratio = sp->s_size / VIEWSIZE;		/* ignore if small or big */
	    if (ratio >= dmp->dm_bound || ratio < 0.001)
		continue;

	    Tcl_AppendResult(interp, "Primitive\n", (char *)NULL);
	    for (BU_LIST_FOR(vp, bn_vlist, &(sp->s_vlist))) {
		int nused = vp->nused;
		int *cmd = vp->cmd;
		point_t *pt = vp->pt;
		for (i = 0; i < nused; i++, cmd++, pt++) {
		    Tcl_AppendResult(interp, "\tVector\n", (char *)NULL);
		    switch (*cmd) {
			case BN_VLIST_POLY_START:
			case BN_VLIST_POLY_VERTNORM:
			    break;
			case BN_VLIST_POLY_MOVE:
			case BN_VLIST_LINE_MOVE:
			    /* move */
			    VMOVE(last, *pt);
			    MOVE(last);
			    break;
			case BN_VLIST_POLY_DRAW:
			case BN_VLIST_POLY_END:
			case BN_VLIST_LINE_DRAW:
			    /* setup direction && length */
			    VSUB2(dir, *pt, last);
			    len = MAGNITUDE(dir);
			    VUNITIZE(dir);
			    visible = FALSE;
			    {
				struct bu_vls tmp_vls;

				bu_vls_init(&tmp_vls);
				bu_vls_printf(&tmp_vls, "\t\tDraw 0 -> %g, step %g\n", len, step);
				Tcl_AppendResult(interp, bu_vls_addr(&tmp_vls), (char *)NULL);
				bu_vls_free(&tmp_vls);
			    }
			    for (u = 0; u <= len; u += step) {
				VJOIN1(aim_point, last, u, dir);
				MAT4X3PNT(temp, view_state->vs_gvp->gv_model2view, aim_point);
				temp[Z] = 100;			/* parallel project */
				MAT4X3PNT(a.a_ray.r_pt, view_state->vs_gvp->gv_view2model, temp);
				if (rt_shootray(&a)) {
				    if (!visible) {
					visible = 1;
					MOVE(aim_point);
				    }
				} else {
				    if (visible) {
					visible = 0;
					DRAW(aim_point);
				    }
				}
			    }
			    if (visible)
				DRAW(aim_point);
			    VMOVE(last, *pt); /* new last vertex */
		    }
		}
	    }
	}

	gdlp = next_gdlp;
    }

    fclose(plotfp);
    return TCL_OK;
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
