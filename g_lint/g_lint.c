/*
 *			G _ L I N T . C
 *
 *	Sample some MGED(1) geometry, reporting overlaps
 *	and potential problems with air regions.
 */

#ifndef lint
static char	RCSid[] = "@(#)$Header$";
#endif

#include <stdio.h>
#include <math.h>
#include <values.h>
#include "machine.h"
#include "vmath.h"
#include "raytrace.h"

#define		OPT_STRING		"a:ce:g:t:u?"
#define		RAND_NUM		((fastf_t)random()/MAXINT)
#define		RAND_OFFSET		((1 - cell_center) *		\
					 (RAND_NUM * celsiz - celsiz / 2))
#define		TITLE_LEN		80

static char	*usage[] = {
    "Usage: 'g_lint [options] model.g object ...'\n",
    "Options:\n",
    "  -a azim      View target from azimuth of azim (0.0 degrees)\n",
    "  -e elev      View target from elevation of elev (0.0 degrees)\n",
    "  -c           Fire rays from center of grid cell (random point)\n",
    "  -g gridsize  Use grid-cell spacing of gridsize (100.0 mm)\n",
    "  -t tol       Ignore overlaps/voids of length < tol (0.0 mm)\n",
    "  -u           Report on air (overlaps only)\n",
    0
};

/*			C H E C K _ A I R
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
 *		than tolerance.
 *	The function returns the number of possible problems it
 *	discovers.
 */
static int check_air (ap, ph)

struct application	*ap;
struct partition	*ph;

{
    struct partition	*pp;
    vect_t		delta;
    fastf_t		mag_del;
    int			problems = 0;
    int			last_air = 0;
    fastf_t		tolerance = *((fastf_t *) ap -> a_uptr);

    for (pp = ph -> pt_forw; pp != ph; pp = pp -> pt_forw)
    {
	RT_HIT_NORM(pp -> pt_inhit, pp -> pt_inseg -> seg_stp,
	    &ap -> a_ray);
	RT_HIT_NORM(pp -> pt_outhit, pp -> pt_outseg -> seg_stp,
	    &ap -> a_ray);

	/* Check air partitions */
	if (pp -> pt_regionp -> reg_regionid <= 0)
	{
	    if (last_air && (pp -> pt_regionp -> reg_aircode != last_air))
	    {
		printf("air_contiguity %d %d %g %g %g\n",
		    last_air, pp -> pt_regionp -> reg_aircode,
		    pp -> pt_inhit -> hit_point[X],
		    pp -> pt_inhit -> hit_point[Y],
		    pp -> pt_inhit -> hit_point[Z]);
		++problems;
	    }
	    else if (pp -> pt_back == ph)
	    {
		printf("air_first %d %g %g %g\n",
		    pp -> pt_regionp -> reg_aircode,
		    pp -> pt_inhit -> hit_point[X],
		    pp -> pt_inhit -> hit_point[Y],
		    pp -> pt_inhit -> hit_point[Z]);
		++problems;
	    }
	    if (pp -> pt_forw == ph)
	    {
		printf("air_last %d %g %g %g\n",
		    pp -> pt_regionp -> reg_aircode,
		    pp -> pt_outhit -> hit_point[X],
		    pp -> pt_outhit -> hit_point[Y],
		    pp -> pt_outhit -> hit_point[Z]);
		++problems;
	    }
	    last_air = pp -> pt_regionp -> reg_aircode;
	}
	else
	    last_air = 0;
	
	/* Look for vacuum */
	if (pp -> pt_back != ph)
	{
	    VSUB2(delta, pp -> pt_inhit -> hit_point,
		pp -> pt_back -> pt_outhit -> hit_point);
	    if ((mag_del = MAGNITUDE(delta)) > tolerance)
	    {
		printf("vacuum %s (%s) %s (%s) %g    %g %g %g    %g %g %g\n",
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
    vect_t	delta;
    fastf_t	mag_del;
    fastf_t	tolerance = *((fastf_t *) ap -> a_uptr);

    /* Compute entry and exit points, and the vector between them */
    RT_HIT_NORM(pp -> pt_inhit, pp -> pt_inseg -> seg_stp, &ap -> a_ray);
    RT_HIT_NORM(pp -> pt_outhit, pp -> pt_outseg -> seg_stp, &ap -> a_ray);
    VSUB2(delta, pp -> pt_inhit -> hit_point, pp -> pt_outhit -> hit_point);

    if ((mag_del = MAGNITUDE(delta)) > tolerance)
    {
	printf("overlap %s %s %g    %g %g %g    %g %g %g\n",
	    r1 -> reg_name, r2 -> reg_name,
	    mag_del,
	    pp -> pt_inhit -> hit_point[X],
	    pp -> pt_inhit -> hit_point[Y],
	    pp -> pt_inhit -> hit_point[Z],
	    pp -> pt_outhit -> hit_point[X],
	    pp -> pt_outhit -> hit_point[Y],
	    pp -> pt_outhit -> hit_point[Z]);
    }
    return (mag_del > tolerance);
}

main (argc, argv)

int	argc;
char	**argv;

{
    struct application	ap;
    char		db_title[TITLE_LEN+1];	/* Title of database */
    fastf_t		azimuth = 0.0;
    fastf_t		celsiz = 100.0;		/* Spatial sampling rate */
    fastf_t		elevation = 0.0;
    fastf_t		tolerance = 0.0;	/* Overlap/void tolerance */
    int			cell_center = 0;	/* Fire from center of cell? */
    int			ch;			/* Character from getopt() */
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
    extern int		opterr;
    extern char		*optarg;

    int			getopt();
    void		printusage();

    /* Handle command-line options */
    while ((ch = getopt(argc, argv, OPT_STRING)) != EOF)
	switch (ch)
	{
	    case 'a':
		if (sscanf(optarg, "%lf", &azimuth) != 1)
		{
		    fprintf(stderr, "Invalid azimuth specification: '%s'\n",
			    optarg);
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
		    fprintf(stderr, "Invalid elevation specification: '%s'\n",
			    optarg);
		    printusage();
		    exit (1);
		}
		if ((elevation < -90.0) || (elevation > 90.0))
		{
		    fprintf(stderr, "Illegal elevation: '%g'\n", elevation);
		    exit (1);
		}
		break;
	    case 'g':
		if (sscanf(optarg, "%lf", &celsiz) != 1)
		{
		    fprintf(stderr, "Invalid grid-size specification: '%s'\n",
			    optarg);
		    printusage();
		    exit (1);
		}
		if (celsiz < 0.0)
		{
		    fprintf(stderr, "Illegal grid size: '%g'\n", celsiz);
		    exit (1);
		}
		break;
	    case 't':
		if (sscanf(optarg, "%lf", &tolerance) != 1)
		{
		    fprintf(stderr,
			"Invalid tolerance specification: '%s'\n",
			optarg);
		    printusage();
		    exit (1);
		}
		if (tolerance < 0.0)
		{
		    fprintf(stderr, "Illegal tolerance: '%g'\n",
			tolerance);
		    exit (1);
		}
		break;
	    case 'u':
		use_air = 1;
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

    /* Read in the geometry model */
    fprintf(stderr, "Database file:  '%s'\n", argv[optind]);
    fputs("Building the directory... ", stderr);
    if ((rtip = rt_dirbuild(argv[optind] , db_title, TITLE_LEN)) == RTI_NULL)
    {
	fprintf(stderr, "Could not build directory for file '%s'\n",
	    argv[optind]);
	exit(1);
    }
    rtip -> useair = use_air;
    fputs ("\n", stderr);
    fputs("Preprocessing the geometry... ", stderr);
    while (++optind < argc)
    {
	if (rt_gettree(rtip, argv[optind]) == -1)
	    exit (1);
	fprintf(stderr, "\nObject '%s' processed", argv[optind]);
    }
    fputs("\n", stderr);
    fputs("Prepping the geometry... ", stderr);
    rt_prep(rtip);
    fputs("\n", stderr);

    /* Initialize the application structure */
    ap.a_hit = use_air ? check_air : no_op;
    ap.a_miss = no_op;
    ap.a_overlap = rpt_ovlp;	/* Report any overlaps */
    ap.a_onehit = 0;		/* Don't stop at first partition */
    ap.a_uptr = (char *) &tolerance;
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

    /* Do the actual gridding */
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

/*			P R I N T U S A G E ( )
 *
 *	Reports a usage message on stderr.
 */
void printusage ()
{
    char	**u;

    for (u = usage; *u != 0; ++u)
	fputs(*u, stderr);
}
