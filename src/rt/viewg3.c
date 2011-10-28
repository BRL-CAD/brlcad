/*                        V I E W G 3 . C
 * BRL-CAD
 *
 * Copyright (c) 1989-2011 United States Government as represented by
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
/** @file rt/viewg3.c
 *
 * Ray Tracing program RTG3 bottom half.
 *
 * This module turns RT library partition lists into the old GIFT type
 * shotlines with three components per card, and with both the
 * entrance and exit obliquity angles.
 *
 * The output format is:
 *	overall header card
 *		view header card
 *			ray (shotline) header card
 *				component card(s)
 *			ray (shotline) header card
 *			 :
 *			 :
 *
 * At present, the main use for this format ray file is to drive the
 * JTCG-approved COVART2 and COVART3 applications.
 *
 */

#include "common.h"

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

#include "vmath.h"
#include "raytrace.h"
#include "plot3.h"

#include "./rtuif.h"
#include "./ext.h"


#define MM2IN 0.03937008		/* mm times MM2IN gives inches */
#define TOL 0.01/MM2IN			/* GIFT has a 0.01 inch tolerance */


void part_compact(register struct application *ap, register struct partition *PartHeadp, fastf_t tolerance);

extern fastf_t gift_grid_rounding;
extern point_t viewbase_model;

extern int npsw;			/* number of worker PSWs to run */

extern int rpt_overlap;

extern struct bu_vls ray_data_file;	/* file name for ray data output (declared in do.c) */
FILE *shot_fp;				/* FILE pointer for ray data output */
static long line_num;			/* count of lines output to shotline file */

/* Viewing module specific "set" variables */
struct bu_structparse view_parse[] = {
    {"",	0, (char *)0,	0,		BU_STRUCTPARSE_FUNC_NULL, NULL, NULL }
};


static mat_t model2hv;			/* model coords to GIFT h, v in inches */

static FILE *plotfp;			/* optional plotting file */
static long line_num;			/* count of lines output to shotline file */

const char title[] = "RTG3";

void
usage(const char *argv0)
{
    bu_log("Usage:  %s [options] model.g objects... >file.ray\n", argv0);
    bu_log("Options:\n");
    bu_log(" -s #		Grid size in pixels, default 512\n");
    bu_log(" -a Az		Azimuth in degrees	(conflicts with -M)\n");
    bu_log(" -e Elev	Elevation in degrees	(conflicts with -M)\n");
    bu_log(" -M		Read model2view matrix on stdin (conflicts with -a, -e)\n");
    bu_log(" -g #		Grid cell width in millimeters (conflicts with -s)\n");
    bu_log(" -G #		Grid cell height in millimeters (conflicts with -s)\n");
    bu_log(" -J #		Jitter.  Default is off.  Any non-zero number is on\n");
    bu_log(" -o model.g3	Specify output file, GIFT-3 format (default=stdout)\n");
    bu_log(" -U #		Set use_air boolean to # (default=1)\n");
    bu_log(" -c \"set ray_data_file=ray_file_name\"         Specify ray data output file (az el x_start y_start z_start x_dir y_dir z_dir line_number_in_shotline_file ray_first_hit_x ray_first_hit_y ray_first_hit_z)\n");
    bu_log(" -c \"set save_overlaps=1\"     Reproduce FASTGEN behavior for regions flagged as FASTGEN regions\n");
    bu_log(" -c \"set rt_cline_radius=radius\"      Additional radius to be added to CLINE solids\n");
    bu_log(" -x #		Set librt debug flags\n");
}


int rayhit(register struct application *ap, struct partition *PartHeadp, struct seg *segp);
int raymiss(register struct application *ap);


/*
 * V I E W _ I N I T
 *
 * This routine is called by main().  It prints the overall shotline
 * header. Furthermore, pointers to rayhit() and raymiss() are set up
 * and are later called from do_run().
 */

static char * save_file;
static char * save_obj;

int
view_init(register struct application *ap, char *file, char *obj, int minus_o, int UNUSED(minus_F))
{
    /* Handling of air in librt */
    use_air = 1;

    line_num = 1;

    if (!minus_o)
	outfp = stdout;

    save_file = file;
    save_obj = obj;

    if (ray_data_file.vls_magic == BU_VLS_MAGIC) {
	if ((shot_fp=fopen(bu_vls_addr(&ray_data_file), "w")) == NULL) {
	    perror("RTG3");
	    bu_log("Cannot open ray data output file %s\n", bu_vls_addr(&ray_data_file));
	    bu_exit(EXIT_FAILURE, "Cannot open ray data output file\n");
	}
    }

    /*
     * Cause grid_setup() to align the grid on one inch boundaries,
     * or cell_width boundaries, if it is given.
     */
    if (cell_width > 0)
	gift_grid_rounding = cell_width;
    else if (cell_height > 0)
	gift_grid_rounding = cell_height;
    else
	gift_grid_rounding = 25.4;		/* one inch */

    ap->a_hit = rayhit;
    ap->a_miss = raymiss;
    ap->a_onehit = 0;

    if (!rpt_overlap)
	ap->a_logoverlap = rt_silent_logoverlap;

    output_is_binary = 0;		/* output is printable ascii */

    if (R_DEBUG & RDEBUG_RAYPLOT) {
	plotfp = fopen("rtg3.pl", "w");
	if (npsw > 1) {
	    bu_log("Note: writing rtg3.pl file can only be done using only 1 processor\n");
	    npsw = 1;
	}
    }

    return 0;		/* No framebuffer needed */
}


/*
 * V I E W _ 2 I N I T
 *
 * View_2init is called by do_frame(), which in turn is called by
 * main() in rt.c.  It writes the view-specific COVART header.
 *
 */
void
view_2init(struct application *UNUSED(ap), char *UNUSED(framename))
{
    if (outfp == NULL)
	bu_exit(EXIT_FAILURE, "outfp is NULL\n");

    /*
     * Overall header, to be read by COVART format:
     *  9220 FORMAT(BZ, I5, 10A4)
     * number of views, title
     * Initially, do only one view per run of RTG3.
     */
    fprintf(outfp, "%5d %s %s\n", 1, save_file, save_obj);

    /*
     * Header for each view, to be read by COVART format:
     * 9230 FORMAT(BZ, 2(5X, E15.8), 30X, E10.3)
     * azimuth, elevation, grid_spacing
     *
     * NOTE that GIFT provides several other numbers that are not used
     * by COVART; this should be investigated.
     *
     * NOTE that grid_spacing is assumed to be square (by COVART), and
     * that the units have been converted from MM to IN.  COVART,
     * given the appropriate code, will take, IN, M, FT, MM, and CM.
     * However, GIFT output is expected to be IN.
     *
     * NOTE that variables "azimuth and elevation" are not valid when
     * the -M flag is used.
     *
     * NOTE: %10g was changed to %10f so that a decimal point is
     * generated even when the number is an integer.  Otherwise the
     * client codes get confused get confused and are unable to
     * convert the number to scientific notation.
     */
    fprintf(outfp,
	    "     %-15.8f     %-15.8f                              %10f\n",
	    azimuth, elevation, cell_width*MM2IN);

    /*
     * GIFT uses an H, V coordinate system that is anchored at the
     * model origin, but rotated according to the view.  For
     * convenience later, build a matrix that will take a point in
     * model space (with units of mm), and convert it to a point in HV
     * space, with units of inches.
     */
    MAT_COPY(model2hv, Viewrotscale);
    model2hv[15] = 1/MM2IN;

    line_num += 2;
}


/*
 * R A Y M I S S
 *
 * Null function -- handle a miss
 * This function is called by rt_shootray(), which is called by
 * do_frame().
 */
int
raymiss(register struct application *UNUSED(ap))
{
    return 0;
}


/*
 * V I E W _ P I X E L
 *
 * This routine is called from do_run(), and in this case does
 * nothing.
 */
void
view_pixel(struct application *UNUSED(ap))
{
    return;
}


/*
 * R A Y H I T
 *
 * Rayhit() is called by rt_shootray() when the ray hits one or more
 * objects.  A per-shotline header record is written, followed by
 * information about each object hit.
 *
 * Note that the GIFT-3 format uses a different convention for the
 * "zero" distance along the ray.  RT has zero at the ray origin
 * (emanation plain), while GIFT has zero at the screen plain
 * translated so that it contains the model origin.  This difference
 * is compensated for by adding the 'dcorrection' distance correction
 * factor.
 *
 * Also note that the GIFT-3 format requires information about the
 * start point of the ray in two formats.  First, the h, v coordinates
 * of the grid cell CENTERS (in screen space coordinates) are needed.
 * Second, the ACTUAL h, v coordinates fired from are needed.
 *
 * An optional rtg3.pl UnixPlot file is written, permitting a color
 * vector display of ray-model intersections.
 */
int
rayhit(struct application *ap, register struct partition *PartHeadp, struct seg *UNUSED(segp))
{
    register struct partition *pp = PartHeadp->pt_forw;
    int comp_count;			/* component count */
    fastf_t dfirst, dlast;		/* ray distances */
    static fastf_t dcorrection = 0;	/* RT to GIFT dist corr */
    int card_count;			/* # comp. on this card */
    const char *fmt;			/* printf() format string */
    struct bu_vls str;
    char buf[128];			/* temp. sprintf() buffer */
    point_t hv;				/* GIFT h, v coords, in inches */
    point_t hvcen;
    int prev_id=-1;
    point_t first_hit = VINIT_ZERO;
    int first;

    if (pp == PartHeadp)
	return 0;			/* nothing was actually hit?? */

    if (ap->a_rt_i->rti_save_overlaps)
	rt_rebuild_overlaps(PartHeadp, ap, 1);

    part_compact(ap, PartHeadp, TOL);

    /* count components in partitions */
    comp_count = 0;
    for (pp=PartHeadp->pt_forw; pp!=PartHeadp; pp=pp->pt_forw) {
	if (pp->pt_regionp->reg_regionid > 0) {
	    prev_id = pp->pt_regionp->reg_regionid;
	    comp_count++;
	} else if (prev_id <= 0) {
	    /* normally air would be output along with a solid
	     * partition, but this will require a '111' partition
	     */
	    prev_id = pp->pt_regionp->reg_regionid;
	    comp_count++;
	} else
	    prev_id = pp->pt_regionp->reg_regionid;
    }
    pp = PartHeadp->pt_back;
    if (pp!=PartHeadp && pp->pt_regionp->reg_regionid <= 0)
	comp_count++;  /* a trailing '111' ident */
    if (comp_count == 0)
	return 0;

    /* Set up variable length string, to buffer this shotline in.
     * Note that there is one component per card, and that each card
     * (line) is 80 characters long.  Hence the parameters given to
     * rt-vls-extend().
     */

    bu_vls_init(&str);
    bu_vls_extend(&str, 80 * (comp_count+1));

    /*
     * Find the H, V coordinates of the grid cell center.  RT uses the
     * lower left corner of each cell.
     */
    {
	point_t center;
	fastf_t dx;
	fastf_t dy;

	dx = ap->a_x + 0.5;
	dy = ap->a_y + 0.5;
	VJOIN2(center, viewbase_model, dx, dx_model, dy, dy_model);
	MAT4X3PNT(hvcen, model2hv, center);
    }

    /*
     * Find exact h, v coordinates of actual ray start by projecting
     * start point into GIFT h, v coordinates.
     */
    MAT4X3PNT(hv, model2hv, ap->a_ray.r_pt);

    /*
     * In RT, rays are launched from the plane of the screen, and ray
     * distances are relative to the start point.  In GIFT-3 output
     * files, ray distances are relative to the (H, V) plane
     * translated so that it contains the origin.  A distance
     * correction is required to convert between the two.  Since this
     * really should be computed only once, not every time, the
     * trip_count flag was added.
     */
    {

	static int trip_count;
	vect_t tmp;
	vect_t viewZdir;

	if (trip_count == 0) {

	    VSET(tmp, 0, 0, -1);		/* viewing direction */
	    MAT4X3VEC(viewZdir, view2model, tmp);
	    VUNITIZE(viewZdir);
	    /* dcorrection will typically be negative */
	    dcorrection = VDOT(ap->a_ray.r_pt, viewZdir);
	    trip_count = 1;
	}
    }

    /* This code is for diagnostics.
     * bu_log("dcorrection=%g\n", dcorrection);
     */

    /* dfirst and dlast have been made negative to account for GIFT
     * looking in the opposite direction of RT.
     */

    dfirst = -(PartHeadp->pt_forw->pt_inhit->hit_dist + dcorrection);
    dlast = -(PartHeadp->pt_back->pt_outhit->hit_dist + dcorrection);

    /*
     * Output the ray header.  The GIFT statements that would have
     * generated this are:
     *
     * 410	write(1, 411) hcen, vcen, h, v, ncomp, dfirst, dlast, a, e
     * 411	format(2f7.1, 2f9.3, i3, 2f8.2, ' A', f6.1, ' E', f6.1)
     */

#define SHOT_FMT "%7.1f%7.1f%9.3f%9.3f%3d%8.2f%8.2f A%6.1f E%6.1f"

    if (rt_perspective > 0) {
	bn_ae_vec(&azimuth, &elevation, ap->a_ray.r_dir);
    }

    bu_vls_printf(&str, SHOT_FMT,
		  hvcen[0], hvcen[1],
		  hv[0], hv[1],
		  comp_count,
		  dfirst * MM2IN, dlast * MM2IN,
		  azimuth, elevation);

    /*
     * As an aid to debugging, take advantage of the fact that there
     * are more than 80 columns on UNIX "cards", and add debugging
     * information to the end of the line to allow this shotline to be
     * reproduced offline.
     *
     * -b gives the shotline x, y coordinates when re-running RTG3,
     * -p and -d are used with RTSHOT
     *
     * The easy way to activate this is with the harmless -!1 option
     * when running RTG3.
     */
    if (R_DEBUG || bu_debug || RT_G_DEBUG) {
	bu_vls_printf(&str, "   -b%d, %d -p %26.20e %26.20e %26.20e -d %26.20e %26.20e %26.20e\n",
		      ap->a_x, ap->a_y,
		      V3ARGS(ap->a_ray.r_pt),
		      V3ARGS(ap->a_ray.r_dir));
    } else {
	bu_vls_putc(&str, '\n');
    }

    /* loop here to deal with individual components */
    card_count = 0;
    prev_id = -1;
    first = 1;
    for (pp=PartHeadp->pt_forw; pp!=PartHeadp; pp=pp->pt_forw) {
	/*
	 * The GIFT statements that would have produced this output
	 * are:
	 *
	 * do 632 i=icomp, iend
	 * if (clos(icomp).gt.999.99.or.slos(i).gt.999.9) goto 635
	 * 632	continue
	 * 	write(1, 633)(item(i), clos(i), cangi(i), cango(i),
	 * &			kspac(i), slos(i), i=icomp, iend)
	 * 633	format(1x, 3(i4, f6.2, 2f5.1, i1, f5.1))
	 *	goto 670
	 * 635	write(1, 636)(item(i), clos(i), cangi(i), cango(i),
	 * &			kspac(i), slos(i), i=icomp, iend)
	 * 636	format(1x, 3(i4, f6.1, 2f5.1, i1, f5.0))
	 */
	fastf_t comp_thickness;	/* component line of sight thickness */
	fastf_t in_obliq;	/* in obliquity angle */
	fastf_t out_obliq;	/* out obliquity angle */
	int region_id;		/* solid region's id */
	int air_id;		/* air id */
	fastf_t dot_prod;	/* dot product of normal and ray dir */
	fastf_t air_thickness;	/* air line of sight thickness */
	vect_t normal;		/* surface normal */
	register struct partition *nextpp = pp->pt_forw;

	region_id = pp->pt_regionp->reg_regionid;

	if (region_id <= 0 && prev_id > 0) {
	    /* air region output with previous partition */
	    prev_id = region_id;
	    continue;
	}
	comp_thickness = pp->pt_outhit->hit_dist -
	    pp->pt_inhit->hit_dist;

	/* The below code is meant to catch components with zero or
	 * negative thicknesses.  This is not supposed to be possible,
	 * but the condition has been seen.
	 */

	if (nextpp == PartHeadp) {
	    if (region_id <= 0) {
		/* last partition is air, need a 111 'phantom armor' before AND after */
		bu_log("WARNING: adding 'phantom armor' (id=111) with zero thickness before and after air region %s\n",
		       pp->pt_regionp->reg_name);
		region_id = 111;
		air_id = pp->pt_regionp->reg_aircode;
		air_thickness = comp_thickness;
		comp_thickness = 0.0;
	    } else {
		/* Last partition, no air follows, use code 9 */
		air_id = 9;
		air_thickness = 0.0;
	    }
	} else if (region_id <= 0) {
	    /* air region, need a 111 'phantom armor' */
	    bu_log("WARNING: adding 'phantom armor' (id=111) with zero thickness before air region %s\n",
		   pp->pt_regionp->reg_name);
	    prev_id = region_id;
	    region_id = 111;
	    air_id = pp->pt_regionp->reg_aircode;
	    air_thickness = comp_thickness;
	    comp_thickness = 0.0;
	} else if (nextpp->pt_regionp->reg_regionid <= 0 &&
		   nextpp->pt_regionp->reg_aircode != 0) {
	    /* Next partition is air region */
	    air_id = nextpp->pt_regionp->reg_aircode;
	    air_thickness = nextpp->pt_outhit->hit_dist -
		nextpp->pt_inhit->hit_dist;
	    prev_id = air_id;
	} else {
	    /* 2 solid regions, maybe with gap */
	    air_id = 0;
	    air_thickness = nextpp->pt_inhit->hit_dist -
		pp->pt_outhit->hit_dist;
	    if (air_thickness < 0.0)
		air_thickness = 0.0;
	    if (!NEAR_ZERO(air_thickness, 0.1)) {
		air_id = 1;	/* air gap */
		if (R_DEBUG & RDEBUG_HITS)
		    bu_log("air gap added\n");
	    } else {
		air_thickness = 0.0;
	    }
	    prev_id = region_id;
	}

	/*
	 * Compute the obliquity angles in degrees, ie, the
	 * "declension" angle down off the normal vector.  RT normals
	 * always point outwards; the "inhit" normal points opposite
	 * the ray direction, the "outhit" normal points along the ray
	 * direction.  Hence the one sign change.  XXX this should
	 * probably be done with atan2()
	 */

	if (first) {
	    first = 0;
	    VJOIN1(first_hit, ap->a_ray.r_pt, pp->pt_inhit->hit_dist, ap->a_ray.r_dir);
	}
    out:
	RT_HIT_NORMAL(normal, pp->pt_inhit, pp->pt_inseg->seg_stp, &(ap->a_ray), pp->pt_inflip);
	dot_prod = VDOT(ap->a_ray.r_dir, normal);
	if (dot_prod > 1.0)
	    dot_prod = 1.0;
	if (dot_prod < -1.0)
	    dot_prod = (-1.0);

	in_obliq = acos(-dot_prod) *
	    bn_radtodeg;
	RT_HIT_NORMAL(normal, pp->pt_outhit, pp->pt_outseg->seg_stp, &(ap->a_ray), pp->pt_outflip);
	dot_prod = VDOT(ap->a_ray.r_dir, normal);
	if (dot_prod > 1.0)
	    dot_prod = 1.0;
	if (dot_prod < -1.0)
	    dot_prod = (-1.0);

	out_obliq = acos(dot_prod) *
	    bn_radtodeg;

	/* Check for exit obliquties greater than 90 degrees. */

			      if (in_obliq > 90.0)
				  in_obliq = 90.0;
	if (in_obliq < 0.0)
	    in_obliq = 0.0;
	if (out_obliq > 90.0)
	    out_obliq = 90.0;
	if (out_obliq < 0.0)
	    out_obliq = 0.0;

	/*
	 * Handle 3-components per card output format, with a leading
	 * space in front of the first component.
	 */
	if (card_count == 0) {
	    bu_vls_strcat(&str, " ");
	}
	comp_thickness *= MM2IN;
	/* Check thickness fields for format overflow */
	if (comp_thickness > 999.99 || air_thickness*MM2IN > 999.9)
	    fmt = "%4d%6.1f%5.1f%5.1f%1d%5.0f";
	else
	    fmt = "%4d%6.2f%5.1f%5.1f%1d%5.1f";
#ifdef SPRINTF_NOT_PARALLEL
	bu_semaphore_acquire(BU_SEM_SYSCALL);
#endif
	snprintf(buf, 128, fmt,
		 region_id,
		 comp_thickness,
		 in_obliq, out_obliq,
		 air_id, air_thickness*MM2IN);
#ifdef SPRINTF_NOT_PARALLEL
	bu_semaphore_release(BU_SEM_SYSCALL);
#endif
	bu_vls_strcat(&str, buf);
	card_count++;
	if (card_count >= 3) {
	    bu_vls_strcat(&str, "\n");
	    card_count = 0;
	}

	/* A color rtg3.pl UnixPlot file of output commands is
	 * generated.  This is processed by plot(1) plotting filters
	 * such as pl-fb.  Portions of a ray passing through air
	 * within the model are represented in blue, while portions
	 * passing through a solid are assigned green.  This will
	 * always be done single CPU, to prevent output garbling.
	 * (See view_init).
	 */
	if (R_DEBUG & RDEBUG_RAYPLOT) {
	    vect_t inpt;
	    vect_t outpt;
	    VJOIN1(inpt, ap->a_ray.r_pt, pp->pt_inhit->hit_dist,
		   ap->a_ray.r_dir);
	    VJOIN1(outpt, ap->a_ray.r_pt, pp->pt_outhit->hit_dist,
		   ap->a_ray.r_dir);
	    pl_color(plotfp, 0, 255, 0);	/* green */
	    pdv_3line(plotfp, inpt, outpt);

	    if (air_thickness > 0) {
		vect_t air_end;
		VJOIN1(air_end, ap->a_ray.r_pt,
		       pp->pt_outhit->hit_dist + air_thickness,
		       ap->a_ray.r_dir);
		pl_color(plotfp, 0, 0, 255);	/* blue */
		pdv_3cont(plotfp, air_end);
	    }
	}
	if (nextpp == PartHeadp && air_id != 9) {
	    /* need to output a 111 'phantom armor' at end of shotline */
	    air_id = 9;
	    air_thickness = 0.0;
	    region_id = 111;
	    comp_thickness = 0.0;
	    goto out;
	}
    }

    /* If partway through building the line, add a newline */
    if (card_count > 0) {
	/*
	 * Note that GIFT zero-fills the unused component slots, but
	 * neither COVART II nor COVART III require it, so just end
	 * the line here.
	 */
	bu_vls_strcat(&str, "\n");
    }

    /* Single-thread through file output.  COVART will accept
     * non-sequential ray data provided the ray header and its
     * associated data are not separated.  CAVEAT: COVART will not
     * accept headers out of sequence.
     */
    bu_semaphore_acquire(BU_SEM_SYSCALL);

    fputs(bu_vls_addr(&str), outfp);

    if (shot_fp) {
	fprintf(shot_fp, "%.5f %.5f %.5f %.5f %.5f %.5f %.5f %.5f %ld %.5f %.5f %.5f\n",
		azimuth, elevation, V3ARGS(ap->a_ray.r_pt), V3ARGS(ap->a_ray.r_dir),
		line_num, V3ARGS(first_hit));

	line_num +=  1 + (comp_count / 3);
	if (comp_count % 3)
	    line_num++;
    }

    /* End of single-thread region */
    bu_semaphore_release(BU_SEM_SYSCALL);

    /* Release vls storage */
    bu_vls_free(&str);

    return 0;
}


/*
 * V I E W _ E O L
 *
 * View_eol() is called by rt_shootray() in do_run().  In this case,
 * it does nothing.
 */
void
view_eol(struct application *UNUSED(ap))
{
}


/*
 * V I E W _ E N D
 *
 * View_end() is called by rt_shootray in do_run().  It outputs a
 * special 999.9 "end of view" marker, composed of a "999.9" shotline
 * header, with one all-zero component record.  This is the way GIFT
 * did it.  Note that the component count must also be zero on this
 * shotline, or else the client codes get confused.
 */
void
view_end(struct application *UNUSED(ap))
{
    fprintf(outfp, SHOT_FMT,
	    999.9, 999.9,
	    999.9, 999.9,
	    0,			/* component count */
	    0.0, 0.0,
	    azimuth, elevation);
    /* An abbreviated component record: just give item code 0.  This
     * is not required since GIFT truncates the above line at the
     * first 999.9: putting out the abovementioned 0 caused a lot of
     * oscillation over the last year, so the line has been removed.
     */

    fflush(outfp);
}


void view_setup(struct rt_i *UNUSED(rtip)) {}
void view_cleanup(struct rt_i *UNUSED(rtip)) {}


/*
 * P A R T _ C O M P A C T
 *
 * This routine takes at partition-head pointer, an application
 * structure pointer, and a tolerance.  It goes through the partition
 * list shot-line by shot-line and checks for regions with identical
 * region-ids abutting.  If one is found, and the distance between the
 * two abbutting regions is less than the tolerance, the two
 * corresponding partions are collapsed into one, and the outhit from
 * the second partions becomes the governing outhit.  This will
 * prevent the occurance of multiple hits per same region.
 *
 */

void
part_compact(register struct application *ap, register struct partition *PartHeadp, fastf_t tolerance)
{

    fastf_t gap;
    struct partition *pp;
    struct partition *nextpp;

    /* first eliminate zero thickness partitions */
    pp = PartHeadp->pt_forw;
    while (pp != PartHeadp) {
	fastf_t comp_thickness;

	nextpp = pp->pt_forw;
	comp_thickness = pp->pt_outhit->hit_dist - pp->pt_inhit->hit_dist;
	if (comp_thickness <= 0.0) {
	    DEQUEUE_PT(pp);
	    FREE_PT(pp, ap->a_resource);
	}
	pp = nextpp;
    }

    for (pp = PartHeadp->pt_forw; pp != PartHeadp; pp = pp->pt_forw) {
    top:		nextpp = pp->pt_forw;
	if (nextpp == PartHeadp) {
	    break;
	}
	if (pp->pt_regionp->reg_regionid > 0 && nextpp->pt_regionp->reg_regionid > 0) {
	    if (pp->pt_regionp->reg_regionid != nextpp->pt_regionp->reg_regionid) {
		continue;
	    }
	} else if (pp->pt_regionp->reg_regionid <= 0 && nextpp->pt_regionp->reg_regionid <= 0) {
	    if (pp->pt_regionp->reg_aircode != nextpp->pt_regionp->reg_aircode) {
		continue;
	    }
	} else
	    continue;

	gap = nextpp->pt_inhit->hit_dist - pp->pt_outhit->hit_dist;

	/* The following line is a diagnostic that is worth reusing:
	 * bu_log("gap=%e\n", gap);
	 */

	if (gap > tolerance) {
	    continue;
	}

	/* Eliminate the gap by collapsing the two partitions into
	 * one.
	 */
	pp->pt_outseg = nextpp->pt_outseg;
	pp->pt_outhit = nextpp->pt_outhit;
	pp->pt_outflip = nextpp->pt_outflip;

	/*
	 * Dequeue and free the unwanted partition structure.
	 * Referenced segments, etc, will be freed by rt_shootray().
	 */
	DEQUEUE_PT(nextpp);
	FREE_PT(nextpp, ap->a_resource);

	goto top;
    }

}


void application_init (void) {}

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
