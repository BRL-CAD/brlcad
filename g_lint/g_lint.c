/*
 *			G _ L I N T . C
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
 *  
 *  Distribution Notice -
 *	Re-distribution of this software is restricted, as described in
 *	your "Statement of Terms and Conditions for the Release of
 *	The BRL-CAD Package" agreement.
 *
 *  Copyright Notice -
 *	This software is Copyright (C) 1995 by the United States Army
 *	in all countries except the USA.  All rights reserved.
 */
#ifndef lint
static char RCSid[] = "@(#)$Header$ (ARL)";
#endif

#include <stdio.h>
#include <math.h>
#include <values.h>
#include "machine.h"
#include "vmath.h"
#include "raytrace.h"

#define made_it()	bu_log("Made it to %s:%d\n", __FILE__, __LINE__);

#define	OPT_STRING	"a:ce:g:opr:t:ux:?"
#define	RAND_NUM	((fastf_t)random()/MAXINT)
#define	RAND_OFFSET	((1 - cell_center) *		\
			 (RAND_NUM * celsiz - celsiz / 2))
#define	TITLE_LEN	80

struct g_lint_ctrl
{
    long		glc_magic;	/* Magic no. for integrity check */
    long		glc_debug;	/* Bits to tailor diagnostics */
    struct application	*glc_ap;	/* To obtain ray orig. point */
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

static unsigned char	dflt_plot_rgb[] =
			{
			    255,   0,   0,	/* overlap */
			    255, 255,   0,	/* air_contiguous */
			      0, 255,   0,	/* air_unconfined */
			    255,   0, 255,	/* air_first */
			      0, 255, 255,	/* air_last */
			    128, 128, 128	/* vacuum */
			};

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
    "  -t tol       Ignore overlaps/voids of length < tol (0.0 mm)\n",
    "  -u           Report on air (overlaps only)\n",
    "  -x bits      Set diagnostic flag=bits\n",
    0
};

/*			P R I N T U S A G E ( )
 *
 *	Reports a usage message on stderr.
 */
void printusage ()
{
    char	**u;

    for (u = usage; *u != 0; ++u)
	bu_log("%s", *u);
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
static int rpt_hit (ap, ph)

struct application	*ap;
struct partition	*ph;

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
    RT_CKMAG(ph, PT_HD_MAGIC, "partition-list head");
    RT_CKMAG(cp, G_LINT_CTRL_MAGIC, "g_lint control structure");

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
	RT_CKMAG(pp, PT_MAGIC, "partition structure");

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
				    V3ARGS((cp -> glc_ap -> a_ray).r_pt));
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
				printf("%g %g %g ",
				    V3ARGS(cp -> glc_ap -> a_ray.r_pt));
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
				printf("%g %g %g ",
				    V3ARGS(cp -> glc_ap -> a_ray.r_pt));
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
				printf("%g %g %g ",
				    V3ARGS(cp -> glc_ap -> a_ray.r_pt));
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
				printf("%g %g %g ",
				    V3ARGS(cp -> glc_ap -> a_ray.r_pt));
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
			printf("%g %g %g ",
			    V3ARGS(cp -> glc_ap -> a_ray.r_pt));
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

/*			N O _ O P
 *
 *	Null event handler for use by rt_shootray().
 *
 *	Does nothing.  Returns 1.
 */
static int no_op (ap, ph)

struct application	*ap;
struct partition	*ph;

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
static int rpt_ovlp (ap, pp, r1, r2)

struct application	*ap;
struct partition	*pp;
struct region		*r1;
struct region		*r2;

{
    vect_t		delta;
    fastf_t		mag_del;
    struct g_lint_ctrl	*cp;
    int			show_origin;
    int			do_plot3;
    fastf_t		tolerance;

    RT_AP_CHECK(ap);
    RT_CKMAG(pp, PT_MAGIC, "partition structure");
    RT_CKMAG(r1, RT_REGION_MAGIC, "region structure");
    RT_CKMAG(r2, RT_REGION_MAGIC, "region structure");
    cp = (struct g_lint_ctrl *) ap -> a_uptr;
    RT_CKMAG(cp, G_LINT_CTRL_MAGIC, "g_lint control structure");

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
	else
	{
	    printf("overlap ");
	    if (show_origin)
		printf("%g %g %g ",
		    V3ARGS(cp -> glc_ap -> a_ray.r_pt));
	    printf("%s %s %g    %g %g %g    %g %g %g\n",
		r1 -> reg_name, r2 -> reg_name,
		mag_del,
		pp -> pt_inhit -> hit_point[X],
		pp -> pt_inhit -> hit_point[Y],
		pp -> pt_inhit -> hit_point[Z],
		pp -> pt_outhit -> hit_point[X],
		pp -> pt_outhit -> hit_point[Y],
		pp -> pt_outhit -> hit_point[Z]);
	}
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
    RT_CKMAG(cp, G_LINT_CTRL_MAGIC, "g_lint control structure");
    
    pdv_3space(cp -> glc_fp, rtip->rti_pmin, rtip->rti_pmax);
}

main (argc, argv)

int	argc;
char	**argv;

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
    unsigned char	*color;
    vect_t		unit_D;			/* View basis vectors */
    vect_t		unit_H;
    vect_t		unit_V;

    extern int		optind;			/* For use with getopt(3) */
    extern int		opterr;
    extern char		*optarg;

    extern int		getopt();

    bu_log("%s\n", rt_version);

    control.glc_magic = G_LINT_CTRL_MAGIC;
    control.glc_debug = 0;
    control.glc_ap = 0;
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
		control.glc_ap = &ap;
		control.glc_how_to_report = G_LINT_ASCII_WITH_ORIGIN;
		break;
	    case 'p':
		control.glc_ap = &ap;
		control.glc_how_to_report = G_LINT_PLOT3;
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
    bu_log("OK, use_air=%d, what_to_report=0x%x\n",
	use_air, control.glc_what_to_report);
    if (control.glc_what_to_report & ~G_LINT_ALL)
	bu_log("WARNING: Ignoring undefined bits of report specification\n");

    /* Read in the geometry model */
    bu_log("Database file:  '%s'\n", argv[optind]);
    bu_log("Building the directory... ");
    if ((rtip = rt_dirbuild(argv[optind] , db_title, TITLE_LEN)) == RTI_NULL)
    {
	bu_log("Could not build directory for file '%s'\n", argv[optind]);
	exit(1);
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

    /* Initialize the application structure */
    ap.a_hit =
	(control.glc_what_to_report & ~G_LINT_OVLP) ? rpt_hit
						    : no_op;
    ap.a_miss = no_op;
    ap.a_resource = RESOURCE_NULL;
    ap.a_overlap =
	(control.glc_what_to_report & G_LINT_OVLP) ? rpt_ovlp
						   : no_op;
    ap.a_onehit = 0;		/* Don't stop at first partition */
    ap.a_uptr = (char *) &control;
    ap.a_rt_i = rtip;
    ap.a_zero1 = 0;		/* Sanity checks for LIBRT(3) */
    ap.a_zero2 = 0;
    ap.a_purpose = "Look for possible problems in geometry";

    /* Compute the basis vectors for the view coordinate system
     *	(N.B. I use VMOVEN() here instead of VMOVE() to emphasize that
     *	 each call copies exactly three elements of the array).
     */
    mat_idn(view2model);
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

    /* Do the actual gridding */
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
}
