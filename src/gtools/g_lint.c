/*                        G _ L I N T . C
 * BRL-CAD
 *
 * Copyright (C) 1995-2005 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this file; see the file named COPYING for more
 * information.
 */
/** @file g_lint.c
 *
 *	Sample some BRL-CAD geometry, reporting overlaps
 *	and potential problems with air regions.
 *
 *  Author -
 *	Paul Tanenbaum
 *
 *  Source -
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5068  USA
 */
#ifndef lint
static const char RCSid[] = "@(#)$Header$ (ARL)";
#endif

#include <stdlib.h>
#include "common.h"



#ifdef HAVE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif

#include <stdio.h>
#include <math.h>
#include <limits.h>			/* home of INT_MAX aka MAXINT */
#include "machine.h"
#include "bu.h"
#include "vmath.h"
#include "raytrace.h"
#include "plot3.h"

#define made_it()	bu_log("Made it to %s:%d\n", __FILE__, __LINE__);

#define	OPT_STRING	"a:ce:g:opr:st:ux:?"
#define	RAND_NUM	((fastf_t)random()/INT_MAX)
#define	RAND_OFFSET	((1 - cell_center) *		\
			 (RAND_NUM * celsiz - celsiz / 2))
#define	TITLE_LEN	80

/*
 *			G _ L I N T _ C T R L
 *
 *	Specification of what an how to report results
 */
struct g_lint_ctrl
{
    long		glc_magic;	/* Magic no. for integrity check */
    long		glc_debug;	/* Bits to tailor diagnostics */
    fastf_t		glc_tol;	/* Overlap/void tolerance */
    unsigned long	glc_what_to_report;	/* Bits to tailor the output */
    unsigned long	glc_how_to_report;	/* Nature of the output */
    FILE		*glc_fp;	/* The output stream */
    unsigned char	*glc_color;	/* RGB for plot3(5) output */
};
#define G_LINT_CTRL_NULL	((struct g_lint_ctrl *) 0)
#define G_LINT_CTRL_MAGIC	0x676c6374
/*
 *	The meanings of the bits in the what-to-report
 *	member of the g_lint_ctrl structure
 */
#define	G_LINT_OVLP	0x001
#define	G_LINT_A_CONT	0x002
#define	G_LINT_A_UNCONF	0x004
#define	G_LINT_A_1ST	0x008
#define	G_LINT_A_LAST	0x010
#define	G_LINT_A_ANY	(G_LINT_A_CONT | G_LINT_A_UNCONF |	\
			 G_LINT_A_1ST  | G_LINT_A_LAST)
#define	G_LINT_VAC	0x020
#define	G_LINT_ALL	(G_LINT_OVLP | G_LINT_A_ANY | G_LINT_VAC)
/*
 *	The possible values of the how-to-report flag
 */
#define	G_LINT_ASCII			0
#define	G_LINT_ASCII_WITH_ORIGIN	1
#define	G_LINT_PLOT3			2

/*
 *			G _ L I N T _ S E G
 *
 *	The critical information about a particular overlap
 *	found on one ray
 */
struct g_lint_seg
{
    long		gls_magic;	/* Magic no. for integrity check */
    double		gls_length;
    point_t		gls_origin;
    point_t		gls_entry;
    point_t		gls_exit;
    struct g_lint_seg	*gls_next;
};
#define	G_LINT_SEG_NULL		((struct g_lint_seg *) 0)
#define G_LINT_SEG_MAGIC	0x676c7367

/*
 *			G _ L I N T _ O V L P
 *
 *	A pair of overlapping regions and a list of all the
 *	offending ray intervals
 */
struct g_lint_ovlp
{
    long		glo_magic;	/* Magic no. for integrity check */
    struct region	*glo_r1;
    struct region	*glo_r2;
    struct g_lint_seg	*glo_segs;
    struct g_lint_seg	*glo_seg_last;
    double		glo_cum_length;
};
#define	G_LINT_OVLP_NULL	((struct g_lint_ovlp *) 0)
#define G_LINT_OVLP_MAGIC	0x676c6f76

static unsigned char	dflt_plot_rgb[] =
			{
			    255,   0,   0,	/* overlap */
			    255, 255,   0,	/* air_contiguous */
			      0, 255,   0,	/* air_unconfined */
			    255,   0, 255,	/* air_first */
			      0, 255, 255,	/* air_last */
			    128, 128, 128	/* vacuum */
			};

bu_rb_tree		*ovlp_log = 0;		/* Log of previous overlaps */
bu_rb_tree		*ovlps_by_vol;		/* For sorting by volume */

int log_2 (unsigned long x)
{
    int	result;

    for (result = 0; x > 1; ++result)
	x >>= 1;
    return (result);
}

/*
 *	The usage message -- it's a long 'un
 */
static char	*usage[] = {
    "Usage: 'g_lint [options] model.g object ...'\n",
    "Options:\n",
    "  -a azim      View target from azimuth of azim (0.0 degrees)\n",
    "  -c           Fire rays from center of grid cell (random point)\n",
    "  -e elev      View target from elevation of elev (0.0 degrees)\n",
    "  -g gridsize  Use grid-cell spacing of gridsize (100.0 mm)\n",
    "  -o           Include ray-origin on each line\n",
    "  -p           Produce plot3(5) output\n",
    "  -r bits      Set report-specification flag=bits...\n",
    "                  1  overlaps\n",
    "                  2  contiguous unlike airs\n",
    "                  4  unconfined airs\n",
    "                  8  air first on shotlines\n",
    "                 16  air last on shotlines\n",
    "                 32  vacuums\n",
    "  -s           Sort overlaps (report without sorting)\n",
    "  -t tol       Ignore overlaps/voids of length < tol (0.0 mm)\n",
    "  -u           Report on air (overlaps only)\n",
    "  -x bits      Set diagnostic flag=bits\n",
    0
};

/*			P R I N T U S A G E ( )
 *
 *	Reports a usage message on stderr.
 */
void printusage (void)
{
    char	**u;

    for (u = usage; *u != 0; ++u)
	bu_log("%s", *u);
}

/*
 *		C R E A T E _ S E G M E N T ( )
 */
struct g_lint_seg *create_segment (void)
{
    struct g_lint_seg	*sp;

    sp = bu_malloc(sizeof(struct g_lint_seg), "g_lint segment structure");
    sp -> gls_magic = G_LINT_SEG_MAGIC;
    sp -> gls_length = -1.0;
    sp -> gls_next = G_LINT_SEG_NULL;

    return (sp);
}

/*
 *		P R I N T _ S E G M E N T ( )
 *
 *	This routine writes one overlap segent to stdout.
 *	It's the workhorse of the reporting process for overlaps.
 */
void print_segment (const char *r1name, const char *r2name, double seg_length, point_t origin, point_t entrypt, point_t exitpt)
{
    printf("overlap ");
    if (origin)
	printf("%g %g %g ", V3ARGS(origin));
    printf("%s %s %g    %g %g %g    %g %g %g\n",
	r1name, r2name, seg_length, V3ARGS(entrypt), V3ARGS(exitpt));
}

/*
 *		C R E A T E _ O V E R L A P ( )
 */
struct g_lint_ovlp *create_overlap (struct region *r1, struct region *r2)
{
    struct g_lint_ovlp	*op;

    BU_CKMAG(r1, RT_REGION_MAGIC, "region structure");
    BU_CKMAG(r2, RT_REGION_MAGIC, "region structure");

    op = bu_malloc(sizeof(struct g_lint_ovlp), "g_lint overlap structure");
    op -> glo_magic = G_LINT_OVLP_MAGIC;
    op -> glo_cum_length = 0.0;
    op -> glo_segs = op -> glo_seg_last = G_LINT_SEG_NULL;

    if (r1 < r2)
    {
	op -> glo_r1 = r1;
	op -> glo_r2 = r2;
    }
    else if (r1 > r2)
    {
	op -> glo_r1 = r2;
	op -> glo_r2 = r1;
    }
    else
    {
	bu_log("%s:%d: Self-overlap of region '%s' (ox%x)\n",
	    __FILE__, __LINE__, r1 -> reg_name, r1);
	bu_log("This shouldn't happen\n");
	exit (1);
    }

    return (op);
}

/*
 *		F R E E _ O V E R L A P ( )
 */
void free_overlap (struct g_lint_ovlp *op)
{
    BU_CKMAG(op, G_LINT_OVLP_MAGIC, "g_lint overlap structure");

    /*
     *		XXX	Oughta do something cleaner than what follows...
     */
    if (op -> glo_segs != G_LINT_SEG_NULL)
	bu_log("%s:%d: Memory Leak!\n", __FILE__, __LINE__);

    bzero((void *) op, sizeof(struct g_lint_ovlp));
    bu_free((genptr_t) op, "g_lint overlap structure");
}

/*
 *		_ P R I N T _ O V E R L A P ( )
 *
 *	The call-back for finally outputting data
 *	for all the overlap segments between any two regions.
 */
void _print_overlap (void *v, int show_origin)
{
    const char		*r1name, *r2name;
    struct g_lint_ovlp	*op = (struct g_lint_ovlp *) v;
    struct g_lint_seg	*sp;

    BU_CKMAG(op, G_LINT_OVLP_MAGIC, "g_lint overlap structure");
    BU_CKMAG(op -> glo_r1, RT_REGION_MAGIC, "region structure");
    BU_CKMAG(op -> glo_r2, RT_REGION_MAGIC, "region structure");

    r1name = op -> glo_r1 -> reg_name;
    r2name = op -> glo_r2 -> reg_name;
    for (sp = op -> glo_segs; sp != G_LINT_SEG_NULL; sp = sp -> gls_next)
	print_segment(r1name, r2name,
	    sp -> gls_length,
	    show_origin ?  sp -> gls_origin : 0,
	    sp -> gls_entry, sp -> gls_exit);
}

/*
 *		P R I N T _ O V E R L A P ( )
 *
 *	A wrapper for _print_overlap()
 *	for use when you don't want to print the ray origin.
 */
void print_overlap (void *v, int depth)
{
    _print_overlap(v, 0);
}

/*
 *		P R I N T _ O V E R L A P _ O ( )
 *
 *	A wrapper for _print_overlap()
 *	for use when you do want to print the ray origin.
 */
void print_overlap_o (void *v, int depth)
{
    _print_overlap(v, 1);
}

/*
 *		C O M P A R E _ O V E R L A P S ( )
 *
 *	    The red-black-tree comparison callback for the overlap log
 */
int compare_overlaps (void *v1, void *v2)
{
    struct g_lint_ovlp	*o1 = (struct g_lint_ovlp *) v1;
    struct g_lint_ovlp	*o2 = (struct g_lint_ovlp *) v2;

    BU_CKMAG(o1, G_LINT_OVLP_MAGIC, "g_lint overlap structure");
    BU_CKMAG(o2, G_LINT_OVLP_MAGIC, "g_lint overlap structure");

    if (o1 -> glo_r1 < o2 -> glo_r1)
	return (-1);
    else if (o1 -> glo_r1 > o2 -> glo_r1)
	return (1);
    else if (o1 -> glo_r2 < o2 -> glo_r2)
	return (-1);
    else if (o1 -> glo_r2 > o2 -> glo_r2)
	return (1);
    else
	return (0);
}

/*
 *		C O M P A R E _ B Y _ V O L ( )
 *
 *	    The red-black-tree comparison callback for
 *	    the final re-sorting of the overlaps by volume
 */
int compare_by_vol (void *v1, void *v2)
{
    struct g_lint_ovlp	*o1 = (struct g_lint_ovlp *) v1;
    struct g_lint_ovlp	*o2 = (struct g_lint_ovlp *) v2;

    BU_CKMAG(o1, G_LINT_OVLP_MAGIC, "g_lint overlap structure");
    BU_CKMAG(o2, G_LINT_OVLP_MAGIC, "g_lint overlap structure");

    if (o1 -> glo_cum_length < o2 -> glo_cum_length)
	return (1);
    else if (o1 -> glo_cum_length > o2 -> glo_cum_length)
	return (-1);
    else
	return (0);
}

/*
 *		I N S E R T _ B Y _ V O L ( )
 *
 *	The call-back, used in traversing the overlap log,
 *	to insert overlaps into the sorted-by-volume tree.
 */
void insert_by_vol (void *v, int depth)
{
    int			rc;	/* Return code from bu_rb_insert() */
    struct g_lint_ovlp	*op = (struct g_lint_ovlp *) v;

    if( (rc = bu_rb_insert(ovlps_by_vol, (void *) op)))
    {
	bu_log("%s:%d: bu_rb_insert() returns %d:  This should not happen\n",
	    __FILE__, __LINE__, rc);
	exit (1);
    }
}

/*
 *		U P D A T E _ O V L P _ L O G ( )
 *
 *		Log an overlap found along a ray.
 *
 *	If regions r1 and r2 were not already known to overlap,
 *	this routine creates a new entry in the overlap log.
 *	Either way, it then modifies their entry to record this
 *	particular find.
 */
void update_ovlp_log (struct region *r1, struct region *r2, double seg_length, fastf_t *origin, fastf_t *entrypt, fastf_t *exitpt)
{
    int			rc;	/* Return code from bu_rb_insert() */
    struct g_lint_ovlp	*op;
    struct g_lint_seg	*sp;

    /*
     *	Prepare an overlap query
     */
    op = create_overlap (r1, r2);

    /*
     *	Add this overlap, if necessary.
     *	Either way, op will end up pointing to the unique
     *	(struct g_lint_ovlp) for regions r1 and r2.
     */
    if ((rc = bu_rb_insert(ovlp_log, (void *) op)) < 0)
    {
	free_overlap(op);
	op = (struct g_lint_ovlp *) bu_rb_curr1(ovlp_log);
    }
    else if (rc > 0)
    {
	bu_log("%s:%d: bu_rb_insert() returns %d:  This should not happen\n",
	    __FILE__, __LINE__, rc);
	exit (1);
    }

    /*
     *	Fill in a new segment structure and add it to the overlap
     */
    sp = create_segment();
    op -> glo_cum_length += (sp -> gls_length = seg_length);
    VMOVE(sp -> gls_origin, origin);
    VMOVE(sp -> gls_entry, entrypt);
    VMOVE(sp -> gls_exit, exitpt);
    if (op -> glo_segs == G_LINT_SEG_NULL)
	op -> glo_segs = sp;
    else
	op -> glo_seg_last -> gls_next = sp;
    op -> glo_seg_last = sp;
}

unsigned char *get_color (unsigned char *ucp, unsigned long x)
{
    int	index;

    for (index = 0; x > 1; ++index)
	x >>= 1;

    return (ucp + (index * 3));
}

/*			R P T _ H I T
 *
 *	Ray-hit handler for use by rt_shootray().
 *
 *	Checks every partition along the ray, reporting on stdout
 *	possible problems with modeled air.  Conditions reported
 *	are:
 *	    - Contiguous partitions with differing non-zero air codes,
 *	    - First partition along a ray with a non-zero air code,
 *	    - Last partition along a ray with a non-zero air code, and
 *	    - Void between successive partitions (exit point from one
 *		not equal to entry point of next), if it is longer
 *		than tolerance, and either of the partitions is air;
 *		and
 *	    - Void between successive partitions (exit point from one
 *		not equal to entry point of next), if it is longer
 *		than tolerance.
 *	The function returns the number of possible problems it
 *	discovers.
 */
static int rpt_hit (struct application *ap, struct partition *ph, struct seg *dummy)
{
    struct partition	*pp;
    vect_t		delta;
    fastf_t		mag_del;
    int			problems = 0;
    int			last_air = 0;
    struct g_lint_ctrl	*cp = (struct g_lint_ctrl *) ap -> a_uptr;
    int			show_origin;
    int			do_plot3;
    fastf_t		tolerance;
    unsigned long	what_to_report;
    unsigned long	debug;
    unsigned char	*color;

    RT_AP_CHECK(ap);
    BU_CKMAG(ph, PT_HD_MAGIC, "partition-list head");
    BU_CKMAG(cp, G_LINT_CTRL_MAGIC, "g_lint control structure");

    show_origin = (cp -> glc_how_to_report == G_LINT_ASCII_WITH_ORIGIN);
    do_plot3 = (cp -> glc_how_to_report == G_LINT_PLOT3);
    tolerance = cp -> glc_tol;
    what_to_report = cp -> glc_what_to_report;
    debug = cp -> glc_debug;

    /*
     *	Before we do anything else,
     *	compute all the hit points along this partition
     */
    for (pp = ph -> pt_forw; pp != ph; pp = pp -> pt_forw)
    {
	BU_CKMAG(pp, PT_MAGIC, "partition structure");

	RT_HIT_NORM(pp -> pt_inhit, pp -> pt_inseg -> seg_stp,
	    &ap -> a_ray);
	RT_HIT_NORM(pp -> pt_outhit, pp -> pt_outseg -> seg_stp,
	    &ap -> a_ray);
    }

    /*
     *	Now, do the real work of checking the partitions...
     */
    for (pp = ph -> pt_forw; pp != ph; pp = pp -> pt_forw)
    {
	/* Check air partitions */
	if (what_to_report & G_LINT_A_ANY)
	{
	    if (pp -> pt_regionp -> reg_regionid <= 0)
	    {
		if ((what_to_report & G_LINT_A_CONT)
		 && last_air && (pp -> pt_regionp -> reg_aircode != last_air))
		{
		    VSUB2(delta, pp -> pt_inhit -> hit_point,
			pp -> pt_back -> pt_outhit -> hit_point);
		    if ((mag_del = MAGNITUDE(delta)) < tolerance)
		    {
			if (do_plot3)
			{
			    color = get_color(cp -> glc_color, G_LINT_A_CONT);
			    pl_color(cp -> glc_fp, V3ARGS(color));
			    pdv_3point(cp -> glc_fp,
				    pp -> pt_inhit -> hit_point);
			}
			else
			{
			    printf("air_contiguous ");
			    if (show_origin)
				printf("%g %g %g ",
				    V3ARGS(ap -> a_ray.r_pt));
			    printf("%s %d %s %d %g %g %g\n",
				pp -> pt_back -> pt_regionp -> reg_name,
				last_air,
				pp -> pt_regionp -> reg_name,
				pp -> pt_regionp -> reg_aircode,
				pp -> pt_inhit -> hit_point[X],
				pp -> pt_inhit -> hit_point[Y],
				pp -> pt_inhit -> hit_point[Z]);
			}
			++problems;
		    }
		}

		if (pp -> pt_back == ph)
		{
		    if (what_to_report & G_LINT_A_1ST)
		    {
			if (do_plot3)
			{
			    color = get_color(cp -> glc_color, G_LINT_A_1ST);
			    pl_color(cp -> glc_fp, V3ARGS(color));
			    pdv_3point(cp -> glc_fp,
				    pp -> pt_inhit -> hit_point);
			}
			else
			{
			    printf("air_first ");
			    if (show_origin)
				printf("%g %g %g ", V3ARGS(ap -> a_ray.r_pt));
			    printf("%s %d %g %g %g\n",
				pp -> pt_regionp -> reg_name,
				pp -> pt_regionp -> reg_aircode,
				pp -> pt_inhit -> hit_point[X],
				pp -> pt_inhit -> hit_point[Y],
				pp -> pt_inhit -> hit_point[Z]);
			}
			++problems;
		    }
		}
		else if (what_to_report & G_LINT_A_UNCONF)
		{
		    VSUB2(delta, pp -> pt_inhit -> hit_point,
			pp -> pt_back -> pt_outhit -> hit_point);
		    if (debug & G_LINT_A_UNCONF)
		    {
			bu_log("inhit (%g,%g,%g) - back outhit (%g,%g,%g) ",
			    V3ARGS(pp -> pt_inhit -> hit_point),
			    V3ARGS(pp -> pt_back -> pt_outhit -> hit_point));
			bu_log(" = (%g,%g,%g), mag=%g\n",
			    V3ARGS(delta), MAGNITUDE(delta));
		    }
		    if ((mag_del = MAGNITUDE(delta)) > tolerance)
		    {
			if (do_plot3)
			{
			    color = get_color(cp -> glc_color, G_LINT_A_UNCONF);
			    pl_color(cp -> glc_fp, V3ARGS(color));
			    pdv_3line(cp -> glc_fp,
				pp -> pt_back -> pt_outhit -> hit_point,
				pp -> pt_inhit -> hit_point);
			}
			else
			{
			    printf("air_unconfined ");
			    if (show_origin)
				printf("%g %g %g ", V3ARGS(ap -> a_ray.r_pt));
			    printf("%s (%s) %s (%s) %g    %g %g %g    %g %g %g\n",
				pp -> pt_back -> pt_regionp -> reg_name,
				pp -> pt_back -> pt_outseg -> seg_stp->st_name,
				pp -> pt_regionp -> reg_name,
				pp -> pt_inseg -> seg_stp -> st_name,
				mag_del,
				pp -> pt_back -> pt_outhit -> hit_point[X],
				pp -> pt_back -> pt_outhit -> hit_point[Y],
				pp -> pt_back -> pt_outhit -> hit_point[Z],
				pp -> pt_inhit -> hit_point[X],
				pp -> pt_inhit -> hit_point[Y],
				pp -> pt_inhit -> hit_point[Z]);
			}
			++problems;
		    }
		}

		if (pp -> pt_forw == ph)
		{
		    if (what_to_report & G_LINT_A_LAST)
		    {
			if (do_plot3)
			{
			    color = get_color(cp -> glc_color, G_LINT_A_LAST);
			    pl_color(cp -> glc_fp, V3ARGS(color));
			    pdv_3point(cp -> glc_fp,
				    pp -> pt_outhit -> hit_point);
			}
			else
			{
			    printf("air_last ");
			    if (show_origin)
				printf("%g %g %g ", V3ARGS(ap -> a_ray.r_pt));
			    printf("%s %d %g %g %g\n",
				pp -> pt_regionp -> reg_name,
				pp -> pt_regionp -> reg_aircode,
				pp -> pt_outhit -> hit_point[X],
				pp -> pt_outhit -> hit_point[Y],
				pp -> pt_outhit -> hit_point[Z]);
			}
			++problems;
		    }
		}
		else if (what_to_report & G_LINT_A_UNCONF)
		{
		    VSUB2(delta, pp -> pt_forw -> pt_inhit -> hit_point,
			pp -> pt_outhit -> hit_point);
		    if (debug & G_LINT_A_UNCONF)
		    {
			bu_log("forw inhit (%g,%g,%g) - outhit (%g,%g,%g) ",
			    V3ARGS(pp -> pt_forw -> pt_inhit -> hit_point),
			    V3ARGS(pp -> pt_outhit -> hit_point));
			bu_log(" = (%g,%g,%g), mag=%g\n",
			    V3ARGS(delta), MAGNITUDE(delta));
		    }
		    if ((mag_del = MAGNITUDE(delta)) > tolerance)
		    {
			if (do_plot3)
			{
			    color = get_color(cp -> glc_color, G_LINT_A_UNCONF);
			    pl_color(cp -> glc_fp, V3ARGS(color));
			    pdv_3line(cp -> glc_fp,
				pp -> pt_outhit -> hit_point,
				pp -> pt_forw -> pt_inhit -> hit_point);
			}
			else
			{
			    printf("air_unconfined ");
			    if (show_origin)
				printf("%g %g %g ", V3ARGS(ap -> a_ray.r_pt));
			    printf("%s (%s) %s (%s) %g    %g %g %g    %g %g %g\n",
				pp -> pt_regionp -> reg_name,
				pp -> pt_outseg -> seg_stp -> st_name,
				pp -> pt_forw -> pt_regionp -> reg_name,
				pp -> pt_forw -> pt_inseg -> seg_stp -> st_name,
				mag_del,
				pp -> pt_outhit -> hit_point[X],
				pp -> pt_outhit -> hit_point[Y],
				pp -> pt_outhit -> hit_point[Z],
				pp -> pt_forw -> pt_inhit -> hit_point[X],
				pp -> pt_forw -> pt_inhit -> hit_point[Y],
				pp -> pt_forw -> pt_inhit -> hit_point[Z]);
			}
			++problems;
		    }
		}
		last_air = pp -> pt_regionp -> reg_aircode;
	    }
	    else
		last_air = 0;
	}

	/* Look for vacuum */
	if ((what_to_report & G_LINT_VAC) && (pp -> pt_back != ph))
	{
	    VSUB2(delta, pp -> pt_inhit -> hit_point,
		pp -> pt_back -> pt_outhit -> hit_point);
	    if (debug & G_LINT_VAC)
	    {
		bu_log("inhit (%g,%g,%g) - back outhit (%g,%g,%g) ",
		    V3ARGS(pp -> pt_inhit -> hit_point),
		    V3ARGS(pp -> pt_back -> pt_outhit -> hit_point));
		bu_log(" = (%g,%g,%g), mag=%g\n",
		    V3ARGS(delta), MAGNITUDE(delta));
	    }
	    if ((mag_del = MAGNITUDE(delta)) > tolerance)
	    {
		if (do_plot3)
		{
		    color = get_color(cp -> glc_color, G_LINT_VAC);
		    pl_color(cp -> glc_fp, V3ARGS(color));
		    pdv_3line(cp -> glc_fp,
			pp -> pt_back -> pt_outhit -> hit_point,
			pp -> pt_inhit -> hit_point);
		}
		else
		{
		    printf("vacuum ");
		    if (show_origin)
			printf("%g %g %g ", V3ARGS(ap -> a_ray.r_pt));
		    printf("%s (%s) %s (%s) %g    %g %g %g    %g %g %g\n",
			pp -> pt_back -> pt_regionp -> reg_name,
			pp -> pt_back -> pt_outseg -> seg_stp -> st_name,
			pp -> pt_regionp -> reg_name,
			pp -> pt_inseg -> seg_stp -> st_name,
			mag_del,
			pp -> pt_back -> pt_outhit -> hit_point[X],
			pp -> pt_back -> pt_outhit -> hit_point[Y],
			pp -> pt_back -> pt_outhit -> hit_point[Z],
			pp -> pt_inhit -> hit_point[X],
			pp -> pt_inhit -> hit_point[Y],
			pp -> pt_inhit -> hit_point[Z]);
		}
		++problems;
	    }
	}
    }
    return (problems);
}

/*		    N O _ O P _ O V E R L A P
 *			N O _ O P _ H I T
 *		       N O _ O P _ M I S S
 *
 *	Null event handlers for use by rt_shootray().
 *
 */
static int no_op_overlap (struct application *ap, struct partition *pp, struct region *r1, struct region *r2)
{
	return( 0 );
}

static int no_op_hit (struct application *ap, struct partition *ph, struct seg *dummy)
{
    return (1);
}

static int no_op_miss (struct application *ap)
{
    return (1);
}

/*			R P T _ O V L P
 *
 *	Overlap handler for use by rt_shootray().
 *
 *	Reports the current overlap on stdout, if the overlap
 *	is of length greater than tolerance.  The function
 *	returns 1 if the overlap was large enough to report,
 *	otherwise 0.
 */
static int rpt_ovlp (struct application *ap, struct partition *pp, struct region *r1, struct region *r2)
{
    vect_t		delta;
    fastf_t		mag_del;
    struct g_lint_ctrl	*cp;
    int			show_origin;
    int			do_plot3;
    fastf_t		tolerance;

    RT_AP_CHECK(ap);
    BU_CKMAG(pp, PT_MAGIC, "partition structure");
    BU_CKMAG(r1, RT_REGION_MAGIC, "region structure");
    BU_CKMAG(r2, RT_REGION_MAGIC, "region structure");
    cp = (struct g_lint_ctrl *) ap -> a_uptr;
    BU_CKMAG(cp, G_LINT_CTRL_MAGIC, "g_lint control structure");

    show_origin = (cp -> glc_how_to_report == G_LINT_ASCII_WITH_ORIGIN);
    do_plot3 = (cp -> glc_how_to_report == G_LINT_PLOT3);
    tolerance = cp -> glc_tol;

    /* Compute entry and exit points, and the vector between them */
    RT_HIT_NORM(pp -> pt_inhit, pp -> pt_inseg -> seg_stp, &ap -> a_ray);
    RT_HIT_NORM(pp -> pt_outhit, pp -> pt_outseg -> seg_stp, &ap -> a_ray);
    VSUB2(delta, pp -> pt_inhit -> hit_point, pp -> pt_outhit -> hit_point);

    if ((mag_del = MAGNITUDE(delta)) > tolerance)
    {
	if (do_plot3)
	{
	    pl_color(cp -> glc_fp,
			V3ARGS(&(cp -> glc_color[log_2(G_LINT_OVLP)])));
	    pdv_3line(cp -> glc_fp, pp -> pt_inhit -> hit_point,
				    pp -> pt_outhit -> hit_point);
	}
	else if (ovlp_log)
	    update_ovlp_log(r1, r2, mag_del,
		ap -> a_ray.r_pt,
		pp -> pt_inhit -> hit_point,
		pp -> pt_outhit -> hit_point);
	else
	    print_segment(r1 -> reg_name, r2 -> reg_name, mag_del,
		show_origin ? ap -> a_ray.r_pt : 0,
		pp -> pt_inhit -> hit_point,
		pp -> pt_outhit -> hit_point);
    }
    return (mag_del > tolerance);
}

void init_plot3 (struct application *ap)
{
    register struct rt_i	*rtip;
    struct g_lint_ctrl		*cp;

    RT_AP_CHECK(ap);
    rtip = ap -> a_rt_i;
    RT_CHECK_RTI(rtip);
    cp = (struct g_lint_ctrl *) ap -> a_uptr;
    BU_CKMAG(cp, G_LINT_CTRL_MAGIC, "g_lint control structure");

    pdv_3space(cp -> glc_fp, rtip->rti_pmin, rtip->rti_pmax);
}

int
main (int argc, char **argv)
{
    struct application	ap;
    char		db_title[TITLE_LEN+1];	/* Title of database */
    char		*sp;			/* String from strtoul(3) */
    fastf_t		azimuth = 0.0;
    fastf_t		celsiz = 100.0;		/* Spatial sampling rate */
    fastf_t		elevation = 0.0;
    struct g_lint_ctrl	control;		/* Info handed to librt(3) */
    int			cell_center = 0;	/* Fire from center of cell? */
    int			ch;			/* Character from getopt(3) */
    int			complement_bits;	/* Used by -r option */
    int			i;			/* Dummy loop index */
    int			use_air = 0;		/* Does air count? */
    mat_t		model2view;		/* Model-to-view matrix */
    mat_t		view2model;		/* View-to-model matrix */
    point_t		cell;			/* Cell center, view coords */
    point_t		g_min;			/* Lower-left of grid plane */
    point_t		g_max;			/* Upper-right of grid plane */
    point_t		mdl_cell;		/* Cell center, model coords */
    point_t		mdl_extrema[2];		/* mdl_min and mdl_max */
    point_t		mdl_bb_vertex;		/* A bounding-box vertex */
    point_t		mdl_row_orig;		/* "Origin" of view-plane row */
    point_t		mdl_vpo;		/* View-plane origin */
    point_t		v_bb_vertex;	/* bounding-box vertex, view coords */
    struct rt_i		*rtip;
    vect_t		unit_D;			/* View basis vectors */
    vect_t		unit_H;
    vect_t		unit_V;

    extern int		optind;			/* For use with getopt(3) */
    extern char		*optarg;

    extern int		getopt(int, char *const *, const char *);

    bu_log("%s\n", rt_version);

    control.glc_magic = G_LINT_CTRL_MAGIC;
    control.glc_debug = 0;
    control.glc_tol = 0.0;
    control.glc_what_to_report = G_LINT_OVLP;
    control.glc_how_to_report = G_LINT_ASCII;
    control.glc_fp = stdout;
    control.glc_color = (unsigned char *) dflt_plot_rgb;

    /* Handle command-line options */
    while ((ch = getopt(argc, argv, OPT_STRING)) != EOF)
	switch (ch)
	{
	    case 'a':
		if (sscanf(optarg, "%lf", &azimuth) != 1)
		{
		    bu_log("Invalid azimuth specification: '%s'\n", optarg);
		    printusage();
		    exit (1);
		}
		break;
	    case 'c':
		cell_center = 1;
		break;
	    case 'e':
		if (sscanf(optarg, "%lf", &elevation) != 1)
		{
		    bu_log("Invalid elevation specification: '%s'\n", optarg);
		    printusage();
		    exit (1);
		}
		if ((elevation < -90.0) || (elevation > 90.0))
		{
		    bu_log("Illegal elevation: '%g'\n", elevation);
		    exit (1);
		}
		break;
	    case 'g':
		if (sscanf(optarg, "%lf", &celsiz) != 1)
		{
		    bu_log("Invalid grid-size specification: '%s'\n", optarg);
		    printusage();
		    exit (1);
		}
		if (celsiz < 0.0)
		{
		    bu_log("Illegal grid size: '%g'\n", celsiz);
		    exit (1);
		}
		break;
	    case 'o':
		control.glc_how_to_report = G_LINT_ASCII_WITH_ORIGIN;
		break;
	    case 'p':
		control.glc_how_to_report = G_LINT_PLOT3;
		if (ovlp_log)
		{
		    bu_rb_free1(ovlp_log, BU_RB_RETAIN_DATA);
		    ovlp_log = 0;
		}
		break;
	    case 'r':
		if (*optarg == '-')
		{
		    complement_bits = 1;
		    ++optarg;
		}
		else
		    complement_bits = 0;
		control.glc_what_to_report = strtoul(optarg, &sp, 0);
		if (sp == optarg)
		{
		    bu_log("Invalid report specification: '%s'\n", optarg);
		    printusage();
		    exit (1);
		}
		if (complement_bits)
		{
		    control.glc_what_to_report = ~(control.glc_what_to_report);
		    control.glc_what_to_report &= G_LINT_ALL;
		}
		use_air = ((control.glc_what_to_report & G_LINT_A_ANY) != 0);
		break;
	    case 's':
		ovlp_log = bu_rb_create1("overlap log", compare_overlaps);
		if (control.glc_how_to_report == G_LINT_PLOT3)
		    control.glc_how_to_report = G_LINT_ASCII;
		bu_rb_uniq_on1(ovlp_log);
		break;
	    case 't':
		if (sscanf(optarg, "%lf", &(control.glc_tol)) != 1)
		{
		    bu_log("Invalid tolerance specification: '%s'\n",
			optarg);
		    printusage();
		    exit (1);
		}
		if (control.glc_tol < 0.0)
		{
		    bu_log("Illegal tolerance: '%g'\n", control.glc_tol);
		    exit (1);
		}
		break;
	    case 'u':
		use_air = 1;
		break;
	    case 'x':
		control.glc_debug = strtoul(optarg, &sp, 16);
		if (sp == optarg)
		{
		    bu_log("Invalid debug-flag specification: '%s'\n", optarg);
		    printusage();
		    exit (1);
		}
		break;
	    default:
		printusage();
		exit (1);
	}

    if (argc - optind < 2)
    {
	printusage();
	exit (1);
    }
    if (control.glc_what_to_report & ~G_LINT_ALL)
	bu_log("WARNING: Ignoring undefined bits of report specification\n");

    /* Read in the geometry model */
    bu_log("Database file:  '%s'\n", argv[optind]);
    bu_log("Building the directory... ");
    if ((rtip = rt_dirbuild(argv[optind] , db_title, TITLE_LEN)) == RTI_NULL)
    {
	bu_log("Could not build directory for file '%s'\n", argv[optind]);
	exit (1);
    }
    rtip -> useair = use_air;
    bu_log("\nPreprocessing the geometry... ");
    while (++optind < argc)
    {
	if (rt_gettree(rtip, argv[optind]) == -1)
	    exit (1);
	bu_log("\nObject '%s' processed", argv[optind]);
    }
    bu_log("\nPrepping the geometry... ");
    rt_prep(rtip);
    bu_log("\n");

    /*
     *	Initialize the application structure
     */
    RT_APPLICATION_INIT(&ap);
    ap.a_hit =
	(control.glc_what_to_report & ~G_LINT_OVLP) ? rpt_hit
						    : no_op_hit;
    ap.a_miss = no_op_miss;
    ap.a_resource = RESOURCE_NULL;
    ap.a_overlap =
	(control.glc_what_to_report & G_LINT_OVLP) ? rpt_ovlp
						   : no_op_overlap;
    ap.a_onehit = 0;		/* Don't stop at first partition */
    ap.a_uptr = (char *) &control;
    ap.a_rt_i = rtip;
    ap.a_purpose = "Look for possible problems in geometry";

    /* Compute the basis vectors for the view coordinate system
     *	(N.B. I use VMOVEN() here instead of VMOVE() to emphasize that
     *	 each call copies exactly three elements of the array).
     */
    MAT_IDN(view2model);
    mat_ae(view2model, azimuth, elevation);
    mat_inv(model2view, view2model);
    VMOVEN(unit_D, model2view, 3);
    VMOVEN(unit_H, model2view + 4, 3);
    VMOVEN(unit_V, model2view + 8, 3);

    /* Compute the limits of gridding
     *	0. initialize mdl_extrema, g_min, and g_max,
     *	1. build each vertex of the bounding box in turn,
     *	2. rotate it into view coordinates,
     *	3. keep a running record of the horizontal and vertical
     *	   extrema in the view plane.
     *	4. Expand the extrema to the next whole multiple of celsiz.
     */
    VMOVE(mdl_extrema[0], rtip -> mdl_min);
    VMOVE(mdl_extrema[1], rtip -> mdl_max);
    VSETALL(g_min, 0.0);
    VMOVE(g_max, g_min);
    for (i = 0; i < 8; ++i)
    {
	VSET(mdl_bb_vertex,
	    mdl_extrema[(i & 0x4) > 0][X],
	    mdl_extrema[(i & 0x2) > 0][Y],
	    mdl_extrema[(i & 0x1) > 0][Z]);
	MAT4X3PNT(v_bb_vertex, model2view, mdl_bb_vertex);
	VMINMAX(g_min, g_max, v_bb_vertex);
    }
    for (i = 0; i < 3; ++i)
    {
	g_min[i] = celsiz * floor(g_min[i] / celsiz);
	g_max[i] = celsiz * ceil(g_max[i] / celsiz);
    }
    VSCALE(mdl_vpo, unit_D, g_max[0]);

    if (control.glc_how_to_report == G_LINT_PLOT3)
	init_plot3(&ap);

    /*
     *	Do the actual gridding
     */
    bu_log("Firing rays... ");
    VSCALE(ap.a_ray.r_dir, unit_D, -1.0);
    cell[2] = 0.0;
    for (cell[1] = g_max[2]; cell[1] >= g_min[2]; cell[1] -= celsiz)
    {
	VJOIN1(mdl_row_orig, mdl_vpo, cell[1], unit_V);
	for (cell[0] = g_min[1]; cell[0] <= g_max[1]; cell[0] += celsiz)
	{
	    VJOIN1(mdl_cell, mdl_row_orig, cell[0], unit_H);
	    VJOIN2(ap.a_ray.r_pt,
		mdl_cell,
		RAND_OFFSET, unit_H,
		RAND_OFFSET, unit_V);
	    VMOVE(ap.a_uvec, cell);
	    (void) rt_shootray(&ap);
	}
    }

    /*
     *	If overlaps have been collected for sorting,
     *	sort them now and then print them out.
     */
    if (ovlp_log)
    {
	ovlps_by_vol = bu_rb_create1("overlaps by volume", compare_by_vol);
	bu_rb_uniq_on1(ovlps_by_vol);
	bu_rb_walk1(ovlp_log, insert_by_vol, INORDER);

	if (control.glc_how_to_report == G_LINT_ASCII_WITH_ORIGIN)
	    bu_rb_walk1(ovlps_by_vol, print_overlap_o, INORDER);
	else
	    bu_rb_walk1(ovlps_by_vol, print_overlap, INORDER);
    }

    exit (0);
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
