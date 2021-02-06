/*                         G Q A . C
 * BRL-CAD
 *
 * Copyright (c) 2008-2021 United States Government as represented by
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
/** @file libged/gqa.c
 *
 * performs a set of quantitative analyses on geometry.
 *
 * XXX need to look at gap computation
 *
 * plot the points where overlaps start/stop
 *
 * Designed to be a framework for 3d sampling of the geometry volume.
 * TODO: Need to move the sample pattern logic into LIBRT.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <math.h>
#include <limits.h>			/* home of INT_MAX aka MAXINT */


#include "bu/parallel.h"
#include "bu/getopt.h"
#include "vmath.h"
#include "raytrace.h"
#include "bn/plot3.h"
#include "analyze.h"

#include "../ged_private.h"

struct analyze_densities *_gd_densities;
char *_gd_densities_source;

/* bu_getopt() options */
const char *options = "A:a:de:f:g:Gn:N:p:P:qrS:s:t:U:u:vV:W:h?";
const char *options_str = "[-A A|a|b|c|e|g|m|o|v|w] [-a az] [-d] [-e el] [-f densityFile] [-g spacing|upper,lower|upper-lower] [-G] [-n nhits] [-N nviews] [-p plotPrefix] [-P ncpus] [-q] [-r] [-S nsamples] [-t overlap_tol] [-U useair] [-u len_units vol_units wt_units] [-v] [-V volume_tol] [-W weight_tol]";

#define ANALYSIS_VOLUMES          1
#define ANALYSIS_WEIGHTS          2
#define ANALYSIS_OVERLAPS         4
#define ANALYSIS_ADJ_AIR          8 /* adjacent air */
#define ANALYSIS_GAPS            16 /* space between regions */
#define ANALYSIS_EXP_AIR         32 /* exposed air */
#define ANALYSIS_BBOX            64 /* overall bounding box */
#define ANALYSIS_INTERFACES     128
#define ANALYSIS_CENTROIDS      256
#define ANALYSIS_MOMENTS        512
#define ANALYSIS_PLOT_OVERLAPS 1024

/* Note: struct parsing requires no space after the commas.  take care
 * when formatting this file.  if the compile breaks here, it means
 * that spaces got inserted incorrectly.
 */
#define COMMA ','

static int analysis_flags;
static int multiple_analyses;

static double azimuth_deg;
static double elevation_deg;
static char *densityFileName;
static double gridSpacing;
static double gridSpacingLimit;
static const double GRIDSPACING_STEP = 1.0 / 2.0;

static char makeOverlapAssemblies;
static size_t require_num_hits;
static int ncpu;
static double Samples_per_model_axis;
static double overlap_tolerance;
static double volume_tolerance;
static double weight_tolerance;
static int aborted = 0;

static int print_per_region_stats;
static int max_region_name_len;
static int use_air;
static int num_objects; /* number of objects specified on command line */
static int max_cpus;
static int num_views;
static int verbose;
static int quiet_missed_report;

static const char *plot_prefix = NULL; /* non-NULL means produce plot files */
static FILE *plot_weight;
static FILE *plot_volume;
static FILE *plot_overlaps;
static FILE *plot_adjair;
static FILE *plot_gaps;
static FILE *plot_expair;

static int overlap_color[3] = { 255, 255, 0 };	/* yellow */
static int gap_color[3] = { 128, 192, 255 };    /* cyan */
static int adjAir_color[3] = { 128, 255, 192 }; /* pale green */
static int expAir_color[3] = { 255, 128, 255 }; /* magenta */

static int debug = 0;
#define DLOG if (debug) bu_vls_printf

/* Some defines for re-using the values from the application structure
 * for other purposes
 */
#define A_LENDEN a_color[0]
#define A_LEN a_color[1]
#define A_STATE a_uptr


struct cstate {
    int curr_view; /* the "view" number we are shooting */
    int u_axis;    /* these 3 are in the range 0..2 inclusive and indicate which axis (X, Y, or Z) */
    int v_axis;    /* is being used for the U, V, or invariant vector direction */
    int i_axis;

    int sem_lists;
    int sem_worker;
    int sem_plot;

    /* sem_worker protects this */
    int v;         /* indicates how many "grid_size" steps in the v direction have been taken */

    int sem_stats;

    /* sem_stats protects this */
    double *m_lenDensity;
    double *m_len;
    double *m_volume;
    double *m_weight;
    unsigned long *shots;
    int first;     /* this is the first time we've computed a set of views */

    vect_t u_dir;  /* direction of U vector for "current view" */
    vect_t v_dir;  /* direction of V vector for "current view" */
    struct rt_i *rtip;
    long steps[3]; /* this is per-dimension, not per-view */
    vect_t span;   /* How much space does the geometry span in each of X, Y, Z directions */
    vect_t area;   /* area of the view for view with invariant at index */

    fastf_t *m_lenTorque; /* torque vector for each view */
    fastf_t *m_moi;       /* one vector per view for collecting the partial moments of inertia calculation */
    fastf_t *m_poi;       /* one vector per view for collecting the partial products of inertia calculation */

    struct resource *resp;
};


struct ged_gqa_plot {
    struct bn_vlblock *vbp;
    struct bu_list *vhead;
} ged_gqa_plot;

/* summary data structure for objects specified on command line */
static struct per_obj_data {
    char *o_name;
    double *o_len;
    double *o_lenDensity;
    double *o_volume;
    double *o_weight;
    fastf_t *o_lenTorque; /* torque vector for each view */
    fastf_t *o_moi;       /* one vector per view for collecting the partial moments of inertia calculation */
    fastf_t *o_poi;       /* one vector per view for collecting the partial products of inertia calculation */
} *obj_tbl;

/**
 * this is the data we track for each region
 */
static struct per_region_data {
    unsigned long hits;
    double *r_lenDensity; /* for per-region per-view weight computation */
    double *r_len;        /* for per-region, per-view computation */
    double *r_weight;
    double *r_volume;
    struct per_obj_data *optr;
} *reg_tbl;


/* Access to these lists should be in sections
 * of code protected by state->sem_lists
 */

/**
 * list of gaps
 */
static struct region_pair gapList = {
    {
	BU_LIST_HEAD_MAGIC,
	(struct bu_list *)&gapList,
	(struct bu_list *)&gapList
    },
    { "Gaps" },
    (struct region *)NULL,
    (unsigned long)0,
    (double)0.0,
    {0.0, 0.0, 0.0, }
};


/**
 * list of adjacent air
 */
static struct region_pair adjAirList = {
    {
	BU_LIST_HEAD_MAGIC,
	(struct bu_list *)&adjAirList,
	(struct bu_list *)&adjAirList
    },
    { (char *)"Adjacent Air" },
    (struct region *)NULL,
    (unsigned long)0,
    (double)0.0,
    {0.0, 0.0, 0.0, }
};


/**
 * list of exposed air
 */
static struct region_pair exposedAirList = {
    {
	BU_LIST_HEAD_MAGIC,
	(struct bu_list *)&exposedAirList,
	(struct bu_list *)&exposedAirList
    },
    { "Exposed Air" },
    (struct region *)NULL,
    (unsigned long)0,
    (double)0.0,
    {0.0, 0.0, 0.0, }
};


/**
 * list of overlaps
 */
static struct region_pair overlapList = {
    {
	BU_LIST_HEAD_MAGIC,
	(struct bu_list *)&overlapList,
	(struct bu_list *)&overlapList
    },
    { "Overlaps" },
    (struct region *)NULL,
    (unsigned long)0,
    (double)0.0,
    {0.0, 0.0, 0.0, }
};


/**
 * This structure holds the name of a unit value, and the conversion
 * factor necessary to convert from/to BRL-CAD standard units.
 *
 * The standard units are millimeters, cubic millimeters, and grams.
 *
 * XXX this section should be extracted to libbu/units.c
 */
struct cvt_tab {
    double val;
    char name[32];
};


static const struct cvt_tab units_tab[3][40] = {
    {
	/* length, stolen from bu/units.c with the "none" value
	 * removed Values for converting from given units to mm
	 */
	{1.0,		"mm"}, /* default */
	/* {0.0,		"none"}, */ /* this is removed to force a certain
					     * amount of error checking for the user
					     */
	{1.0e-7,	"angstrom"},
	{1.0e-7,	"decinanometer"},
	{1.0e-6,	"nm"},
	{1.0e-6,	"nanometer"},
	{1.0e-3,	"um"},
	{1.0e-3,	"micrometer"},
	{1.0e-3,	"micron"},
	{1.0,		"millimeter"},
	{10.0,		"cm"},
	{10.0,		"centimeter"},
	{1000.0,	"m"},
	{1000.0,	"meter"},
	{1000000.0,	"km"},
	{1000000.0,	"kilometer"},
	{25.4,		"in"},
	{25.4,		"inch"},
	{25.4,		"inches"},		/* for plural */
	{304.8,		"ft"},
	{304.8,		"foot"},
	{304.8,		"feet"},
	{456.2,		"cubit"},
	{914.4,		"yd"},
	{914.4,		"yard"},
	{5029.2,	"rd"},
	{5029.2,	"rod"},
	{1609344.0,	"mi"},
	{1609344.0,	"mile"},
	{1852000.0,	"nmile"},
	{1852000.0,	"nautical mile"},
	{1.495979e+14,	"AU"},
	{1.495979e+14,	"astronomical unit"},
	{9.460730e+18,	"lightyear"},
	{3.085678e+19,	"pc"},
	{3.085678e+19,	"parsec"},
	{0.0,		""}			/* LAST ENTRY */
    },
    {
	/* volume
	 * Values for converting from given units to mm^3
	 */
	{1.0, "cu mm"}, /* default */

	{1.0, "mm"},
	{1.0, "mm^3"},

	{1.0e3, "cm"},
	{1.0e3, "cm^3"},
	{1.0e3, "cu cm"},
	{1.0e3, "cc"},

	{1.0e6, "l"},
	{1.0e6, "liter"},
	{1.0e6, "litre"},

	{1.0e9, "m"},
	{1.0e9, "m^3"},
	{1.0e9, "cu m"},

	{16387.064, "in"},
	{16387.064, "in^3"},
	{16387.064, "cu in"},

	{28316846.592, "ft"},

	{28316846.592, "ft^3"},
	{28316846.592, "cu ft"},

	{764554857.984, "yds"},
	{764554857.984, "yards"},
	{764554857.984, "cu yards"},

	{0.0,		""}			/* LAST ENTRY */
    },
    {
	/* weight
	 * Values for converting given units to grams
	 */
	{1.0, "grams"}, /* default */

	{1.0, "g"},
	{0.0648, "gr"},
	{0.0648, "grains"},

	{1.0e3, "kg"},
	{1.0e3, "kilos"},
	{1.0e3, "kilograms"},

	{28.35, "oz"},
	{28.35, "ounce"},

	{453.6, "lb"},
	{453.6, "lbs"},
	{0.0,		""}			/* LAST ENTRY */
    }
};


/* this table keeps track of the "current" or "user selected units and
 * the associated conversion values
 */
#define LINE 0
#define VOL 1
#define WGT 2
static const struct cvt_tab *units[3] = {
    &units_tab[0][0],	/* linear */
    &units_tab[1][0],	/* volume */
    &units_tab[2][0]	/* weight */
};


/**
 * _gqa_read_units_double
 *
 * Read a non-negative floating point value with optional units
 *
 * Return
 * 1 Failure
 * 0 Success
 */
int
_gqa_read_units_double(double *val, char *buf, const struct cvt_tab *cvt)
{
    double a;
#define UNITS_STRING_SZ 256
    char units_string[UNITS_STRING_SZ+1] = {0};
    int i;


    i = sscanf(buf, "%lg" CPP_SCAN(UNITS_STRING_SZ), &a, units_string);

    if (i < 0) return 1;

    if (i == 1) {
	*val = a;

	return 0;
    }
    if (i == 2) {
	*val = a;
	for (; cvt->name[0] != '\0';) {
	    if (!bu_strncmp(cvt->name, units_string, sizeof(units_string))) {
		goto found_units;
	    } else {
		cvt++;
	    }
	}
	bu_vls_printf(_ged_current_gedp->ged_result_str, "Bad units specifier \"%s\" on value \"%s\"\n", units_string, buf);
	return 1;

    found_units:
	*val = a * cvt->val;
	return 0;
    }
    bu_vls_printf(_ged_current_gedp->ged_result_str, "%s sscanf problem on \"%s\" got %d\n", CPP_FILELINE, buf, i);
    return 1;
}


/* the above should be extracted to libbu/units.c */


/**
 * Parse through command line flags
 */
static int
parse_args(int ac, char *av[])
{
    int c;
    int i;
    double a;
    char *p;

    /* Turn off getopt's error messages */
    bu_opterr = 0;
    bu_optind = 1;

    /* get all the option flags from the command line */
    while ((c=bu_getopt(ac, av, options)) != -1) {
	switch (c) {
	    case 'A':
		{
		    analysis_flags = 0;
		    multiple_analyses = 0;
		    for (p = bu_optarg; *p; p++) {
			switch (*p) {
			    case 'A' :
				multiple_analyses = 1;
				analysis_flags = analysis_flags \
				    | ANALYSIS_ADJ_AIR \
				    | ANALYSIS_BBOX \
				    | ANALYSIS_CENTROIDS \
				    | ANALYSIS_EXP_AIR \
				    | ANALYSIS_GAPS \
				    | ANALYSIS_MOMENTS \
				    | ANALYSIS_OVERLAPS \
				    | ANALYSIS_VOLUMES \
				    | ANALYSIS_WEIGHTS;
				break;
			    case 'a' :
				if (analysis_flags)
				    multiple_analyses = 1;

				analysis_flags |= ANALYSIS_ADJ_AIR;

				break;
			    case 'b' :
				if (analysis_flags)
				    multiple_analyses = 1;

				analysis_flags |= ANALYSIS_BBOX;

				break;
			    case 'c' :
				if (analysis_flags)
				    multiple_analyses = 1;

				analysis_flags |= ANALYSIS_WEIGHTS;
				analysis_flags |= ANALYSIS_CENTROIDS;

				break;
			    case 'e' :
				if (analysis_flags)
				    multiple_analyses = 1;

				analysis_flags |= ANALYSIS_EXP_AIR;
				break;
			    case 'g' :
				if (analysis_flags)
				    multiple_analyses = 1;

				analysis_flags |= ANALYSIS_GAPS;
				break;
			    case 'm' :
				if (analysis_flags)
				    multiple_analyses = 1;

				analysis_flags |= ANALYSIS_WEIGHTS;
				analysis_flags |= ANALYSIS_CENTROIDS;
				analysis_flags |= ANALYSIS_MOMENTS;

				break;
			    case 'o' :
				if (analysis_flags)
				    multiple_analyses = 1;

				analysis_flags |= ANALYSIS_OVERLAPS;
				break;
			    case 'p' :
				if (analysis_flags)
				    multiple_analyses = 1;

				analysis_flags |= ANALYSIS_OVERLAPS;
				analysis_flags |= ANALYSIS_PLOT_OVERLAPS;
				break;
			    case 'v' :
				if (analysis_flags)
				    multiple_analyses = 1;

				analysis_flags |= ANALYSIS_VOLUMES;
				break;
			    case 'w' :
				if (analysis_flags)
				    multiple_analyses = 1;

				analysis_flags |= ANALYSIS_WEIGHTS;
				break;
			    default:
				bu_vls_printf(_ged_current_gedp->ged_result_str, "Unknown analysis type \"%c\" requested.\n", *p);
				return -1;
			}
		    }
		    break;
		}
	    case 'a':
		bu_vls_printf(_ged_current_gedp->ged_result_str, "azimuth not implemented\n");
		if (bn_decode_angle(&azimuth_deg,bu_optarg) == 0) {
		    bu_vls_printf(_ged_current_gedp->ged_result_str, "error parsing azimuth \"%s\"\n", bu_optarg);
		    return -1;
		}
		break;
	    case 'e':
		bu_vls_printf(_ged_current_gedp->ged_result_str, "elevation not implemented\n");
		if (bn_decode_angle(&elevation_deg,bu_optarg) == 0) {
		    bu_vls_printf(_ged_current_gedp->ged_result_str, "error parsing elevation \"%s\"\n", bu_optarg);
		    return -1;
		}
		break;
	    case 'd': debug = 1; break;

	    case 'f': densityFileName = bu_optarg; break;

	    case 'g':
		{
		    double value1, value2;
		    i = 0;

		    /* find out if we have two or one args; user can
		     * separate them with , or - delimiter
		     */
		    p = strchr(bu_optarg, COMMA);
		    if (p)
			*p++ = '\0';
		    else {
			p = strchr(bu_optarg, '-');
			if (p)
			    *p++ = '\0';
		    }


		    if (_gqa_read_units_double(&value1, bu_optarg, units_tab[0])) {
			bu_vls_printf(_ged_current_gedp->ged_result_str, "error parsing grid spacing value \"%s\"\n", bu_optarg);
			return -1;
		    }

		    if (p) {
			/* we've got 2 values, they are upper limit
			 * and lower limit.
			 */
			if (_gqa_read_units_double(&value2, p, units_tab[0])) {
			    bu_vls_printf(_ged_current_gedp->ged_result_str, "error parsing grid spacing limit value \"%s\"\n", p);
			    return -1;
			}

			gridSpacing = value1;
			gridSpacingLimit = value2;
		    } else {
			gridSpacingLimit = value1;

			gridSpacing = 0.0; /* flag it */
		    }
		    break;
		}
	    case 'G':
		makeOverlapAssemblies = 1;
		bu_vls_printf(_ged_current_gedp->ged_result_str, "-G option unimplemented\n");
		return -1;
	    case 'n':
		if (sscanf(bu_optarg, "%d", &c) != 1 || c < 0) {
		    bu_vls_printf(_ged_current_gedp->ged_result_str, "num_hits must be integer value >= 0, not \"%s\"\n", bu_optarg);
		    return -1;
		}

		require_num_hits = (size_t)c;
		break;

	    case 'N':
		num_views = atoi(bu_optarg);
		break;
	    case 'p':
		plot_prefix = bu_optarg;
		break;
	    case 'P':
		/* cannot ask for more cpu's than the machine has */
		c = atoi(bu_optarg);
		if (c > 0 && c <= max_cpus)
		    ncpu = c;
		break;
	    case 'q':
		quiet_missed_report = 1;
		break;
	    case 'r':
		print_per_region_stats = 1;
		break;
	    case 'S':
		if (sscanf(bu_optarg, "%lg", &a) != 1 || a <= 1.0) {
		    bu_vls_printf(_ged_current_gedp->ged_result_str, "error in specifying minimum samples per model axis: \"%s\"\n", bu_optarg);
		    break;
		}
		Samples_per_model_axis = a + 1;
		break;
	    case 't':
		if (_gqa_read_units_double(&overlap_tolerance, bu_optarg, units_tab[0])) {
		    bu_vls_printf(_ged_current_gedp->ged_result_str, "error in overlap tolerance distance \"%s\"\n", bu_optarg);
		    return -1;
		}
		break;
	    case 'v':
		verbose = 1;
		break;
	    case 'V':
		if (_gqa_read_units_double(&volume_tolerance, bu_optarg, units_tab[1])) {
		    bu_vls_printf(_ged_current_gedp->ged_result_str, "error in volume tolerance \"%s\"\n", bu_optarg);
		    return -1;
		}
		break;
	    case 'W':
		if (_gqa_read_units_double(&weight_tolerance, bu_optarg, units_tab[2])) {
		    bu_vls_printf(_ged_current_gedp->ged_result_str, "error in weight tolerance \"%s\"\n", bu_optarg);
		    return -1;
		}
		break;

	    case 'U':
		errno = 0;
		use_air = strtol(bu_optarg, (char **)NULL, 10);
		if (errno == ERANGE || errno == EINVAL) {
		    bu_vls_printf(_ged_current_gedp->ged_result_str, "error in air argument %s\n", bu_optarg);
		    return -1;
		}
		break;
	    case 'u':
		{
		    char *ptr = bu_optarg;
		    const struct cvt_tab *cv;
		    static const char *dim[3] = {"length", "volume", "weight"};
		    char *units_name[3] = {NULL, NULL, NULL};
		    char **units_ap;

		    /* fill in units_name with the names we parse out */
		    units_ap = units_name;

		    /* acquire unit names */
		    for (i = 0; i < 3 && ptr; i++) {
			int found_unit;

			if (i == 0) {
			    *units_ap = strtok(ptr, CPP_XSTR(COMMA));
			} else {
			    *units_ap = strtok(NULL, CPP_XSTR(COMMA));
			}

			/* got something? */
			if (*units_ap == NULL)
			    break;

			/* got something valid? */
			found_unit = 0;
			for (cv = &units_tab[i][0]; cv->name[0] != '\0'; cv++) {
			    if (units_name[i] && BU_STR_EQUAL(cv->name, units_name[i])) {
				units[i] = cv;
				found_unit = 1;
				break;
			    }
			}

			if (!found_unit) {
			    bu_vls_printf(_ged_current_gedp->ged_result_str, "Units \"%s\" not found in conversion table\n", units_name[i]);
			    return -1;
			}

			++units_ap;
		    }

		    bu_vls_printf(_ged_current_gedp->ged_result_str, "Units: ");
		    for (i = 0; i < 3; i++) {
			bu_vls_printf(_ged_current_gedp->ged_result_str, " %s: %s", dim[i], units[i]->name);
		    }
		    bu_vls_printf(_ged_current_gedp->ged_result_str, "\n");
		}
		break;

	    default: /* '?' 'h' */
		return -1;
	}
    }

    return bu_optind;
}

/**
 * Write end points of partition to the standard output.  If this
 * routine return !0, this partition will be dropped from the boolean
 * evaluation.
 *
 * Returns:
 * 0 to eliminate partition with overlap entirely
 * 1 to retain partition in output list, claimed by reg1
 * 2 to retain partition in output list, claimed by reg2
 *
 * This routine must be prepared to run in parallel
 */
int
_gqa_overlap(struct application *ap,
	struct partition *pp,
	struct region *reg1,
	struct region *reg2,
	struct partition *hp)
{
    struct cstate *state = (struct cstate *)ap->A_STATE;
    struct xray *rp = &ap->a_ray;
    struct hit *ihitp = pp->pt_inhit;
    struct hit *ohitp = pp->pt_outhit;
    point_t ihit;
    point_t ohit;
    double depth;

    if (!hp) /* unexpected */
	return 0;

    /* if one of the regions is air, let it loose */
    if (reg1->reg_aircode && ! reg2->reg_aircode)
	return 2;
    if (reg2->reg_aircode && ! reg1->reg_aircode)
	return 1;

    depth = ohitp->hit_dist - ihitp->hit_dist;

    if (depth < overlap_tolerance)
	/* too small to matter, pick one or none */
	return 1;

    VJOIN1(ihit, rp->r_pt, ihitp->hit_dist, rp->r_dir);
    VJOIN1(ohit, rp->r_pt, ohitp->hit_dist, rp->r_dir);

    if (plot_overlaps) {
	bu_semaphore_acquire(state->sem_plot);
	pl_color(plot_overlaps, V3ARGS(overlap_color));
	pdv_3line(plot_overlaps, ihit, ohit);
	bu_semaphore_release(state->sem_plot);
    }

    if (analysis_flags & ANALYSIS_PLOT_OVERLAPS) {
	bu_semaphore_acquire(state->sem_worker);
	BN_ADD_VLIST(ged_gqa_plot.vbp->free_vlist_hd, ged_gqa_plot.vhead, ihit, BN_VLIST_LINE_MOVE);
	BN_ADD_VLIST(ged_gqa_plot.vbp->free_vlist_hd, ged_gqa_plot.vhead, ohit, BN_VLIST_LINE_DRAW);
	bu_semaphore_release(state->sem_worker);
    }

    if (analysis_flags & ANALYSIS_OVERLAPS) {
	bu_semaphore_acquire(state->sem_lists);
	add_unique_pair(&overlapList, reg1, reg2, depth, ihit);
	bu_semaphore_release(state->sem_lists);

	if (plot_overlaps) {
	    bu_semaphore_acquire(state->sem_plot);
	    pl_color(plot_overlaps, V3ARGS(overlap_color));
	    pdv_3line(plot_overlaps, ihit, ohit);
	    bu_semaphore_release(state->sem_plot);
	}
    } else {
	bu_semaphore_acquire(state->sem_worker);
	bu_vls_printf(_ged_current_gedp->ged_result_str, "overlap %s %s\n", reg1->reg_name, reg2->reg_name);
	bu_semaphore_release(state->sem_worker);
    }

    /* XXX We should somehow flag the volume/weight calculations as invalid */

    /* since we have no basis to pick one over the other, just pick */
    return 1;	/* No further consideration to this partition */
}


/**
 * Does nothing.
 */
void
logoverlap(struct application *ap,
	   const struct partition *pp,
	   const struct bu_ptbl *regiontable,
	   const struct partition *InputHdp)
{
    RT_CK_AP(ap);
    RT_CK_PT(pp);
    BU_CK_PTBL(regiontable);
    if (!InputHdp)
	return;

    /* do nothing */

    return;
}


void _gqa_exposed_air(struct application *ap,
		      struct partition *pp,
		      point_t last_out_point,
		      point_t pt,
		      point_t opt)
{
    struct cstate *state = (struct cstate *)ap->A_STATE;

    /* this shouldn't be air */

    bu_semaphore_acquire(state->sem_lists);
    add_unique_pair(&exposedAirList,
	    pp->pt_regionp,
	    (struct region *)NULL,
	    pp->pt_outhit->hit_dist - pp->pt_inhit->hit_dist, /* thickness */
	    last_out_point); /* location */
    bu_semaphore_release(state->sem_lists);

    if (plot_expair) {
	bu_semaphore_acquire(state->sem_plot);
	pl_color(plot_expair, V3ARGS(expAir_color));
	pdv_3line(plot_expair, pt, opt);
	bu_semaphore_release(state->sem_plot);
    }
}


/**
 * rt_shootray() was told to call this on a hit.  It passes the
 * application structure which describes the state of the world (see
 * raytrace.h), and a circular linked list of partitions, each one
 * describing one in and out segment of one region.
 *
 * this routine must be prepared to run in parallel
 */
int
_gqa_hit(struct application *ap, struct partition *PartHeadp, struct seg *segs)
{
    /* see raytrace.h for all of these guys */
    struct partition *pp;
    point_t pt, opt, last_out_point;
    int last_air = 0;  /* what was the aircode of the last item */
    int air_first = 1; /* are we in an air before a solid */
    double dist;       /* the thickness of the partition */
    double gap_dist;
    double last_out_dist = -1.0;
    double val;
    struct cstate *state = (struct cstate *)ap->A_STATE;

    if (!segs) /* unexpected */
	return 0;

    if (PartHeadp->pt_forw == PartHeadp) return 1;


    /* examine each partition until we get back to the head */
    for (pp=PartHeadp->pt_forw; pp != PartHeadp; pp = pp->pt_forw) {

	long int material_id = pp->pt_regionp->reg_gmater;
	fastf_t grams_per_cu_mm = analyze_densities_density(_gd_densities, material_id);

	/* inhit info */
	dist = pp->pt_outhit->hit_dist - pp->pt_inhit->hit_dist;
	VJOIN1(pt, ap->a_ray.r_pt, pp->pt_inhit->hit_dist, ap->a_ray.r_dir);
	VJOIN1(opt, ap->a_ray.r_pt, pp->pt_outhit->hit_dist, ap->a_ray.r_dir);

	if (debug) {
	    bu_semaphore_acquire(state->sem_worker);
	    bu_vls_printf(_ged_current_gedp->ged_result_str, "%s %g->%g\n", pp->pt_regionp->reg_name,
			  pp->pt_inhit->hit_dist, pp->pt_outhit->hit_dist);
	    bu_semaphore_release(state->sem_worker);
	}

	/* checking for air sticking out of the model.  This is done
	 * here because there may be any number of air regions
	 * sticking out of the model along the ray.
	 */
	if (analysis_flags & ANALYSIS_EXP_AIR) {

	    gap_dist = (pp->pt_inhit->hit_dist - last_out_dist);

	    /* if air is first on the ray, or we're moving from void/gap to air
	     * then this is exposed air
	     */
	    if (pp->pt_regionp->reg_aircode &&
		(air_first || gap_dist > overlap_tolerance)) {
		_gqa_exposed_air(ap, pp, last_out_point, pt, opt);
	    } else {
		air_first = 0;
	    }
	}

	/* looking for voids in the model */
	if (analysis_flags & ANALYSIS_GAPS) {
	    if (pp->pt_back != PartHeadp) {
		/* if this entry point is further than the previous
		 * exit point then we have a void
		 */
		gap_dist = pp->pt_inhit->hit_dist - last_out_dist;

		if (gap_dist > overlap_tolerance) {

		    /* like overlaps, we only want to report unique pairs */
		    bu_semaphore_acquire(state->sem_lists);
		    add_unique_pair(&gapList,
			    pp->pt_regionp,
			    pp->pt_back->pt_regionp,
			    gap_dist,
			    pt);
		    bu_semaphore_release(state->sem_lists);

		    /* like overlaps, let's plot */
		    if (plot_gaps) {
			vect_t gapEnd;
			VJOIN1(gapEnd, pt, -gap_dist, ap->a_ray.r_dir);

			bu_semaphore_acquire(state->sem_plot);
			pl_color(plot_gaps, V3ARGS(gap_color));
			pdv_3line(plot_gaps, pt, gapEnd);
			bu_semaphore_release(state->sem_plot);
		    }
		}
	    }
	}

	/* computing the weight of the objects */
	if (analysis_flags & ANALYSIS_WEIGHTS) {
	    if (debug) {
		bu_semaphore_acquire(state->sem_worker);
		bu_vls_printf(_ged_current_gedp->ged_result_str, "Hit %s doing weight\n", pp->pt_regionp->reg_name);
		bu_semaphore_release(state->sem_worker);
	    }

	    /* make sure mater index is within range of densities */
	    if (pp->pt_regionp->reg_gmater < 0) {
		bu_semaphore_acquire(state->sem_worker);
		bu_vls_printf(_ged_current_gedp->ged_result_str, "density index %d on region %s is outside of range\nSet GIFTmater on region or add entry to density table\n",
			      pp->pt_regionp->reg_gmater,
			      pp->pt_regionp->reg_name);
		bu_semaphore_release(state->sem_worker);
		return GED_ERROR;
	    } else {

		struct per_region_data *prd;
		vect_t cmass;
		vect_t lenTorque;
		fastf_t Lx = state->span[0]/state->steps[0];
		fastf_t Ly = state->span[1]/state->steps[1];
		fastf_t Lz = state->span[2]/state->steps[2];
		fastf_t Lx_sq;
		fastf_t Ly_sq;
		fastf_t Lz_sq;
		fastf_t cell_area;
		int los;

		switch (state->i_axis) {
		    case 0:
			Lx_sq = dist*pp->pt_regionp->reg_los*0.01;
			Lx_sq *= Lx_sq;
			Ly_sq = Ly*Ly;
			Lz_sq = Lz*Lz;
			cell_area = Ly_sq;
			break;
		    case 1:
			Lx_sq = Lx*Lx;
			Ly_sq = dist*pp->pt_regionp->reg_los*0.01;
			Ly_sq *= Ly_sq;
			Lz_sq = Lz*Lz;
			cell_area = Lx_sq;
			break;
		    case 2:
		    default:
			Lx_sq = Lx*Lx;
			Ly_sq = Ly*Ly;
			Lz_sq = dist*pp->pt_regionp->reg_los*0.01;
			Lz_sq *= Lz_sq;
			cell_area = Lx_sq;
			break;
		}

		/* factor in the density of this object weight
		 * computation, factoring in the LOS percentage
		 * material of the object
		 */
		los = pp->pt_regionp->reg_los;

		if (los < 1) {
		    bu_semaphore_acquire(state->sem_worker);
		    bu_vls_printf(_ged_current_gedp->ged_result_str, "bad LOS (%d) on %s\n", los, pp->pt_regionp->reg_name);
		    bu_semaphore_release(state->sem_worker);
		}

		/* accumulate the total weight values */
		val = grams_per_cu_mm * dist * (pp->pt_regionp->reg_los * 0.01);
		ap->A_LENDEN += val;

		prd = ((struct per_region_data *)pp->pt_regionp->reg_udata);
		/* accumulate the per-region per-view weight values */
		bu_semaphore_acquire(state->sem_stats);
		prd->r_lenDensity[state->i_axis] += val;

		/* accumulate the per-object per-view weight values */
		prd->optr->o_lenDensity[state->i_axis] += val;

		if (analysis_flags & ANALYSIS_CENTROIDS) {
		    /* calculate the center of mass for this partition */
		    VJOIN1(cmass, pt, dist*0.5, ap->a_ray.r_dir);

		    /* calculate the lenTorque for this partition (i.e. centerOfMass * lenDensity) */
		    VSCALE(lenTorque, cmass, val);

		    /* accumulate per-object per-view torque values */
		    VADD2(&prd->optr->o_lenTorque[state->i_axis*3], &prd->optr->o_lenTorque[state->i_axis*3], lenTorque);

		    /* accumulate the total lenTorque */
		    VADD2(&state->m_lenTorque[state->i_axis*3], &state->m_lenTorque[state->i_axis*3], lenTorque);

		    if (analysis_flags & ANALYSIS_MOMENTS) {
			vectp_t moi;
			vectp_t poi = &state->m_poi[state->i_axis*3];
			fastf_t dx_sq = cmass[X]*cmass[X];
			fastf_t dy_sq = cmass[Y]*cmass[Y];
			fastf_t dz_sq = cmass[Z]*cmass[Z];
			fastf_t mass = val * cell_area;
			static const fastf_t ONE_TWELFTH = 1.0 / 12.0;

			/* Collect moments and products of inertia for the current object */
			moi = &prd->optr->o_moi[state->i_axis*3];
			moi[X] += ONE_TWELFTH*mass*(Ly_sq + Lz_sq) + mass*(dy_sq + dz_sq);
			moi[Y] += ONE_TWELFTH*mass*(Lx_sq + Lz_sq) + mass*(dx_sq + dz_sq);
			moi[Z] += ONE_TWELFTH*mass*(Lx_sq + Ly_sq) + mass*(dx_sq + dy_sq);
			poi = &prd->optr->o_poi[state->i_axis*3];
			poi[X] -= mass*cmass[X]*cmass[Y];
			poi[Y] -= mass*cmass[X]*cmass[Z];
			poi[Z] -= mass*cmass[Y]*cmass[Z];

			/* Collect moments and products of inertia for all objects */
			moi = &state->m_moi[state->i_axis*3];
			moi[X] += ONE_TWELFTH*mass*(Ly_sq + Lz_sq) + mass*(dy_sq + dz_sq);
			moi[Y] += ONE_TWELFTH*mass*(Lx_sq + Lz_sq) + mass*(dx_sq + dz_sq);
			moi[Z] += ONE_TWELFTH*mass*(Lx_sq + Ly_sq) + mass*(dx_sq + dy_sq);
			poi = &state->m_poi[state->i_axis*3];
			poi[X] -= mass*cmass[X]*cmass[Y];
			poi[Y] -= mass*cmass[X]*cmass[Z];
			poi[Z] -= mass*cmass[Y]*cmass[Z];
		    }
		}

		bu_semaphore_release(state->sem_stats);
	    }
	}

	/* compute the volume of the object */
	if (analysis_flags & ANALYSIS_VOLUMES) {
	    struct per_region_data *prd = ((struct per_region_data *)pp->pt_regionp->reg_udata);
	    ap->A_LEN += dist; /* add to total volume */
	    {
		bu_semaphore_acquire(state->sem_stats);

		/* add to region volume */
		prd->r_len[state->curr_view] += dist;

		/* add to object volume */
		prd->optr->o_len[state->curr_view] += dist;

		bu_semaphore_release(state->sem_stats);
	    }
	    if (debug) {
		bu_semaphore_acquire(state->sem_worker);
		bu_vls_printf(_ged_current_gedp->ged_result_str, "\t\tvol hit %s oDist:%g objVol:%g %s\n",
			      pp->pt_regionp->reg_name, dist, prd->optr->o_len[state->curr_view], prd->optr->o_name);
		bu_semaphore_release(state->sem_worker);
	    }

	    if (plot_volume) {
		VJOIN1(opt, ap->a_ray.r_pt, pp->pt_outhit->hit_dist, ap->a_ray.r_dir);

		bu_semaphore_acquire(state->sem_plot);
		if (ap->a_user & 1) {
		    pl_color(plot_volume, V3ARGS(gap_color));
		} else {
		    pl_color(plot_volume, V3ARGS(adjAir_color));
		}

		pdv_3line(plot_volume, pt, opt);
		bu_semaphore_release(state->sem_plot);
	    }
	}


	/* look for two adjacent air regions */
	if (analysis_flags & ANALYSIS_ADJ_AIR) {
	    if (last_air && pp->pt_regionp->reg_aircode &&
		pp->pt_regionp->reg_aircode != last_air) {

		double d = pp->pt_outhit->hit_dist - pp->pt_inhit->hit_dist;
		point_t aapt;

		bu_semaphore_acquire(state->sem_lists);
		add_unique_pair(&adjAirList, pp->pt_back->pt_regionp, pp->pt_regionp, 0.0, pt);
		bu_semaphore_release(state->sem_lists);


		d *= 0.25;
		VJOIN1(aapt, pt, d, ap->a_ray.r_dir);

		bu_semaphore_acquire(state->sem_plot);
		pl_color(plot_adjair, V3ARGS(adjAir_color));
		pdv_3line(plot_adjair, pt, aapt);
		bu_semaphore_release(state->sem_plot);
	    }
	}

	/* note that this region has been seen */
	((struct per_region_data *)pp->pt_regionp->reg_udata)->hits++;

	last_air = pp->pt_regionp->reg_aircode;
	last_out_dist = pp->pt_outhit->hit_dist;
	VJOIN1(last_out_point, ap->a_ray.r_pt, pp->pt_outhit->hit_dist, ap->a_ray.r_dir);
    }


    if (analysis_flags & ANALYSIS_EXP_AIR && last_air) {
	/* the last thing we hit was air.  Make a note of that */
	pp = PartHeadp->pt_back;

	_gqa_exposed_air(ap, pp, last_out_point, pt, opt);
    }


    /* This value is returned by rt_shootray a hit usually returns 1,
     * miss 0.
     */
    return 1;
}


/**
 * rt_shootray() was told to call this on a miss.
 *
 * This routine must be prepared to run in parallel
 */
int
_gqa_miss(struct application *ap)
{
    RT_CK_APPLICATION(ap);

    return 0;
}


/**
 * This routine must be prepared to run in parallel
 */
int
get_next_row(struct cstate *state)
{
    int v;
    /* look for more work */
    bu_semaphore_acquire(state->sem_worker);

    if (state->v < state->steps[state->v_axis])
	v = state->v++;	/* get a row to work on */
    else
	v = 0; /* signal end of work */

    bu_semaphore_release(state->sem_worker);

    return v;
}


/**
 * This routine must be prepared to run in parallel
 */
void
plane_worker(int cpu, void *ptr)
{
    struct application ap;
    int u, v;
    double v_coord;
    struct cstate *state = (struct cstate *)ptr;
    unsigned long shot_cnt;

    if (aborted)
	return;

    RT_APPLICATION_INIT(&ap);
    ap.a_rt_i = (struct rt_i *)state->rtip;	/* application uses this instance */
    ap.a_hit = _gqa_hit;    /* where to go on a hit */
    ap.a_miss = _gqa_miss;  /* where to go on a miss */
    ap.a_logoverlap = logoverlap;
    ap.a_overlap = _gqa_overlap;
    ap.a_resource = &state->resp[cpu];
    ap.A_LENDEN = 0.0; /* really the cumulative length*density for weight computation*/
    ap.A_LEN = 0.0;    /* really the cumulative length for volume computation */

    /* gross hack */
    ap.a_ray.r_dir[state->u_axis] = ap.a_ray.r_dir[state->v_axis] = 0.0;
    ap.a_ray.r_dir[state->i_axis] = 1.0;

    ap.A_STATE = ptr; /* really copying the state ptr to the a_uptr */

    u = -1;

    v = get_next_row(state);

    shot_cnt = 0;
    while (v) {

	v_coord = v * gridSpacing;
	if (debug) {
	    bu_semaphore_acquire(state->sem_worker);
	    bu_vls_printf(_ged_current_gedp->ged_result_str, "  v = %d v_coord=%g\n", v, v_coord);
	    bu_semaphore_release(state->sem_worker);
	}

	if ((v&1) || state->first) {
	    /* shoot all the rays in this row.  This is either the
	     * first time a view has been computed or it is an odd
	     * numbered row in a grid refinement
	     */
	    for (u=1; u < state->steps[state->u_axis]; u++) {
		ap.a_ray.r_pt[state->u_axis] = ap.a_rt_i->mdl_min[state->u_axis] + u*gridSpacing;
		ap.a_ray.r_pt[state->v_axis] = ap.a_rt_i->mdl_min[state->v_axis] + v*gridSpacing;
		ap.a_ray.r_pt[state->i_axis] = ap.a_rt_i->mdl_min[state->i_axis];

		if (debug) {
		    bu_semaphore_acquire(state->sem_worker);
		    bu_vls_printf(_ged_current_gedp->ged_result_str, "%5g %5g %5g -> %g %g %g\n", V3ARGS(ap.a_ray.r_pt),
				  V3ARGS(ap.a_ray.r_dir));
		    bu_semaphore_release(state->sem_worker);
		}
		ap.a_user = v;
		(void)rt_shootray(&ap);

		if (aborted)
		    return;

		shot_cnt++;
	    }
	} else {
	    /* shoot only the rays we need to on this row.  Some of
	     * them have been computed in a previous iteration.
	     */
	    for (u=1; u < state->steps[state->u_axis]; u+=2) {
		ap.a_ray.r_pt[state->u_axis] = ap.a_rt_i->mdl_min[state->u_axis] + u*gridSpacing;
		ap.a_ray.r_pt[state->v_axis] = ap.a_rt_i->mdl_min[state->v_axis] + v*gridSpacing;
		ap.a_ray.r_pt[state->i_axis] = ap.a_rt_i->mdl_min[state->i_axis];

		if (debug) {
		    bu_semaphore_acquire(state->sem_worker);
		    bu_vls_printf(_ged_current_gedp->ged_result_str, "%5g %5g %5g -> %g %g %g\n", V3ARGS(ap.a_ray.r_pt),
				  V3ARGS(ap.a_ray.r_dir));
		    bu_semaphore_release(state->sem_worker);
		}
		ap.a_user = v;
		(void)rt_shootray(&ap);

		if (aborted)
		    return;

		shot_cnt++;

		if (debug) {
		    if (u+1 < state->steps[state->u_axis]) {
			bu_semaphore_acquire(state->sem_worker);
			bu_vls_printf(_ged_current_gedp->ged_result_str, "  ---\n");
			bu_semaphore_release(state->sem_worker);
		    }
		}
	    }
	}

	/* iterate */
	v = get_next_row(state);
    }

    if (debug && (u == -1)) {
	bu_semaphore_acquire(state->sem_worker);
	bu_vls_printf(_ged_current_gedp->ged_result_str, "didn't shoot any rays\n");
	bu_semaphore_release(state->sem_worker);
    }

    /* There's nothing else left to work on in this view.  It's time
     * to add the values we have accumulated to the totals for the
     * view and return.  When all threads have been through here,
     * we'll have returned to serial computation.
     */
    bu_semaphore_acquire(state->sem_stats);
    state->shots[state->curr_view] += shot_cnt;
    state->m_lenDensity[state->curr_view] += ap.A_LENDEN; /* add our length*density value */
    state->m_len[state->curr_view] += ap.A_LEN; /* add our volume value */
    bu_semaphore_release(state->sem_stats);
}


int
find_cmd_line_obj(struct per_obj_data *obj_rpt, const char *name)
{
    int i;
    char *str = bu_strdup(name);
    char *p;

    p = strchr(str, '/');
    if (p) {
	*p = '\0';
    }

    for (i = 0; i < num_objects; i++) {
	if (BU_STR_EQUAL(obj_rpt[i].o_name, str)) {
	    bu_free(str, "");
	    return i;
	}
    }

    bu_vls_printf(_ged_current_gedp->ged_result_str, "%s Didn't find object named \"%s\" in %d entries\n", CPP_FILELINE, name, num_objects);

    return -1;
}


/**
 * Allocate data structures for tracking statistics on a per-view
 * basis for each of the view, object and region levels.
 */
void
allocate_per_region_data(struct cstate *state, int start, int ac, const char *av[])
{
    struct region *regp;
    struct rt_i *rtip = state->rtip;
    int i;
    int m;
    int index;

    if (start > ac) {
	/* what? */
	bu_log("WARNING: Internal error (start:%d > ac:%d).\n", start, ac);
	return;
    }

    if (num_objects < 1) {
	/* what?? */
	bu_log("WARNING: No objects remaining.\n");
	return;
    }

    if (num_views == 0) {
	/* crap. */
	bu_log("WARNING: No views specified.\n");
	return;
    }

    if (rtip->nregions == 0) {
	/* dammit! */
	bu_log("WARNING: No regions remaining.\n");
	return;
    }

    state->m_lenDensity = (double *)bu_calloc(num_views, sizeof(double), "densityLen");
    state->m_len = (double *)bu_calloc(num_views, sizeof(double), "volume");
    state->m_volume = (double *)bu_calloc(num_views, sizeof(double), "volume");
    state->m_weight = (double *)bu_calloc(num_views, sizeof(double), "volume");
    state->shots = (unsigned long *)bu_calloc(num_views, sizeof(unsigned long), "volume");
    state->m_lenTorque = (fastf_t *)bu_calloc(num_views, sizeof(vect_t), "lenTorque");
    state->m_moi = (fastf_t *)bu_calloc(num_views, sizeof(vect_t), "moments of inertia");
    state->m_poi = (fastf_t *)bu_calloc(num_views, sizeof(vect_t), "products of inertia");

    /* build data structures for the list of objects the user
     * specified on the command line
     */
    obj_tbl = (struct per_obj_data *)bu_calloc(num_objects, sizeof(struct per_obj_data), "report tables");
    for (i = 0; i < num_objects; i++) {
	obj_tbl[i].o_name = (char *)av[start+i];
	obj_tbl[i].o_len = (double *)bu_calloc(num_views, sizeof(double), "o_len");
	obj_tbl[i].o_lenDensity = (double *)bu_calloc(num_views, sizeof(double), "o_lenDensity");
	obj_tbl[i].o_volume = (double *)bu_calloc(num_views, sizeof(double), "o_volume");
	obj_tbl[i].o_weight = (double *)bu_calloc(num_views, sizeof(double), "o_weight");
	obj_tbl[i].o_lenTorque = (fastf_t *)bu_calloc(num_views, sizeof(vect_t), "lenTorque");
	obj_tbl[i].o_moi = (fastf_t *)bu_calloc(num_views, sizeof(vect_t), "moments of inertia");
	obj_tbl[i].o_poi = (fastf_t *)bu_calloc(num_views, sizeof(vect_t), "products of inertia");
    }

    /* build objects for each region */
    reg_tbl = (struct per_region_data *)bu_calloc(rtip->nregions, sizeof(struct per_region_data), "per_region_data");


    for (i = 0, BU_LIST_FOR (regp, region, &(rtip->HeadRegion)), i++) {
	regp->reg_udata = &reg_tbl[i];

	reg_tbl[i].r_lenDensity = (double *)bu_calloc(num_views, sizeof(double), "r_lenDensity");
	reg_tbl[i].r_len = (double *)bu_calloc(num_views, sizeof(double), "r_len");
	reg_tbl[i].r_volume = (double *)bu_calloc(num_views, sizeof(double), "len");
	reg_tbl[i].r_weight = (double *)bu_calloc(num_views, sizeof(double), "len");

	m = (int)strlen(regp->reg_name);
	if (m > max_region_name_len) max_region_name_len = m;
	index = find_cmd_line_obj(obj_tbl, &regp->reg_name[1]);
	if (index == -1)
	    reg_tbl[i].optr = NULL;
	else
	    reg_tbl[i].optr = &obj_tbl[index];
    }
}


/**
 * list_report
 */
void
list_report(struct region_pair *list)
{
    struct region_pair *rp;

    if (BU_LIST_IS_EMPTY(&list->l)) {
	bu_vls_printf(_ged_current_gedp->ged_result_str, "No %s\n", (char *)list->r.name);

	return;
    }

    bu_vls_printf(_ged_current_gedp->ged_result_str, "list %s:\n", (char *)list->r.name);

    for (BU_LIST_FOR (rp, region_pair, &(list->l))) {
	if (rp->r2) {
	    bu_vls_printf(_ged_current_gedp->ged_result_str, "%s %s count:%lu dist:%g%s @ (%g %g %g)\n",
			  rp->r.r1->reg_name, rp->r2->reg_name, rp->count,
			  rp->max_dist / units[LINE]->val, units[LINE]->name, V3ARGS(rp->coord));
	} else {
	    bu_vls_printf(_ged_current_gedp->ged_result_str, "%s count:%lu dist:%g%s @ (%g %g %g)\n",
			  rp->r.r1->reg_name, rp->count,
			  rp->max_dist / units[LINE]->val, units[LINE]->name, V3ARGS(rp->coord));
	}
    }
}


/**
 * Do some computations prior to raytracing based upon options the
 * user has specified
 *
 * Returns:
 * 0 continue, ready to go
 * !0 error encountered, terminate processing
 */
int
options_prep(struct rt_i *UNUSED(rtip), vect_t span)
{
    double newGridSpacing = gridSpacing;
    int axis;

    /* figure out where the density values are coming from and get
     * them.
     */
    if (analysis_flags & ANALYSIS_WEIGHTS) {
	if (densityFileName) {
	    DLOG(_ged_current_gedp->ged_result_str, "density from file\n");
	    if (_ged_read_densities(&_gd_densities, &_gd_densities_source, _ged_current_gedp, densityFileName, 0) != GED_OK) {
		return GED_ERROR;
	    }
	} else {
	    DLOG(_ged_current_gedp->ged_result_str, "density from db\n");
	    if (_ged_read_densities(&_gd_densities, &_gd_densities_source, _ged_current_gedp, NULL, 0) != GED_OK) {
		return GED_ERROR;
	    }
	}
    }
    /* refine the grid spacing if the user has set a lower bound on
     * the number of rays per model axis
     */
    for (axis=0; axis < 3; axis++) {
	if (span[axis] < newGridSpacing*Samples_per_model_axis) {
	    /* along this axis, the gridSpacing is larger than the
	     * model span.  We need to refine.
	     */
	    newGridSpacing = span[axis] / Samples_per_model_axis;
	}
    }

    if (!ZERO(newGridSpacing - gridSpacing)) {
	bu_log("Initial grid spacing %g %s does not allow %g samples per axis.\n",
		      gridSpacing / units[LINE]->val, units[LINE]->name, Samples_per_model_axis - 1);

	bu_log("Adjusted initial grid spacing to %g %s to get %g samples per model axis.\n",
		      newGridSpacing / units[LINE]->val, units[LINE]->name, Samples_per_model_axis);

	gridSpacing = newGridSpacing;
    }

    /* if the vol/weight tolerances are not set, pick something */
    if (analysis_flags & ANALYSIS_VOLUMES) {
	if (volume_tolerance < 0.0) {
	    /* using 1/1000th the volume as a default tolerance, no particular reason */
	    volume_tolerance = span[X] * span[Y] * span[Z] * 0.001;
	    bu_log("Using estimated volume tolerance %g %s\n", volume_tolerance / units[VOL]->val, units[VOL]->name);
	} else
	    bu_log("Using volume tolerance %g %s\n", volume_tolerance / units[VOL]->val, units[VOL]->name);
	if (plot_prefix) {
	    struct bu_vls vp = BU_VLS_INIT_ZERO;
	    bu_vls_printf(&vp, "%svolume.plot3", plot_prefix);
	    bu_log("Plotting volumes to %s\n", bu_vls_cstr(&vp));
	    plot_volume = fopen(bu_vls_cstr(&vp), "wb");
	    if (plot_volume == (FILE *)NULL) {
		bu_vls_printf(_ged_current_gedp->ged_result_str, "cannot open plot file %s\n", bu_vls_cstr(&vp));
		/* not a critical failure */
	    }
	    bu_vls_free(&vp);
	}
    }
    if (analysis_flags & ANALYSIS_WEIGHTS) {
	if (weight_tolerance < 0.0) {
	    double max_den = 0.0;
	    long int curr_id = -1;
	    while ((curr_id = analyze_densities_next(_gd_densities, curr_id)) != -1) {
		if (analyze_densities_density(_gd_densities, curr_id) > max_den)
		    max_den = analyze_densities_density(_gd_densities, curr_id);
	    }
	    weight_tolerance = span[X] * span[Y] * span[Z] * 0.1 * max_den;
	    bu_vls_printf(_ged_current_gedp->ged_result_str, "setting weight tolerance to %g %s\n",
			  weight_tolerance / units[WGT]->val,
			  units[WGT]->name);
	} else {
	    bu_vls_printf(_ged_current_gedp->ged_result_str, "weight tolerance   %g\n", weight_tolerance);
	}
    }
    if (analysis_flags & ANALYSIS_GAPS) {
	if (plot_prefix) {
	    struct bu_vls vp = BU_VLS_INIT_ZERO;
	    bu_vls_printf(&vp, "%sgaps.plot3", plot_prefix);
	    bu_log("Plotting gaps to %s\n", bu_vls_cstr(&vp));
	    plot_gaps = fopen(bu_vls_cstr(&vp), "wb");
	    if (plot_gaps == (FILE *)NULL) {
		bu_vls_printf(_ged_current_gedp->ged_result_str, "cannot open plot file %s\n", bu_vls_cstr(&vp));
		/* not a critical failure */
	    }
	    bu_vls_free(&vp);
	}
    }
    if (analysis_flags & ANALYSIS_OVERLAPS) {
	if (!ZERO(overlap_tolerance))
	    bu_vls_printf(_ged_current_gedp->ged_result_str, "overlap tolerance to %g\n", overlap_tolerance);
	if (plot_prefix) {
	    struct bu_vls vp = BU_VLS_INIT_ZERO;
	    bu_vls_printf(&vp, "%soverlaps.plot3", plot_prefix);
	    bu_log("Plotting overlaps to %s\n", bu_vls_cstr(&vp));
	    plot_overlaps = fopen(bu_vls_cstr(&vp), "wb");
	    if (plot_overlaps == (FILE *)NULL) {
		bu_vls_printf(_ged_current_gedp->ged_result_str, "cannot open plot file %s\n", bu_vls_cstr(&vp));
		/* not a critical failure */
	    }
	    bu_vls_free(&vp);
	}
    }

    if (print_per_region_stats)
	if ((analysis_flags & (ANALYSIS_VOLUMES|ANALYSIS_WEIGHTS)) == 0)
	    bu_vls_printf(_ged_current_gedp->ged_result_str, "Note: -r option ignored: neither volume or weight options requested\n");

    if (analysis_flags & ANALYSIS_ADJ_AIR)
	if (plot_prefix) {
	    struct bu_vls vp = BU_VLS_INIT_ZERO;
	    bu_vls_printf(&vp, "%sadj_air.plot3", plot_prefix);
	    bu_log("Plotting adjacent air to %s\n", bu_vls_cstr(&vp));
	    plot_adjair = fopen(bu_vls_cstr(&vp), "wb");
	    if (plot_adjair == (FILE *)NULL) {
		bu_vls_printf(_ged_current_gedp->ged_result_str, "cannot open plot file %s\n", bu_vls_cstr(&vp));
		/* not a critical failure */
	    }
	    bu_vls_free(&vp);
	}

    if (analysis_flags & ANALYSIS_EXP_AIR)
	if (plot_prefix) {
	    struct bu_vls vp = BU_VLS_INIT_ZERO;
	    bu_vls_printf(&vp, "%sexp_air.plot3", plot_prefix);
	    bu_log("Plotting exposed air to %s\n", bu_vls_cstr(&vp));
	    plot_expair = fopen(bu_vls_cstr(&vp), "wb");
	    if (plot_expair == (FILE *)NULL) {
		bu_vls_printf(_ged_current_gedp->ged_result_str, "cannot open plot file %s\n", bu_vls_cstr(&vp));
		/* not a critical failure */
	    }
	    bu_vls_free(&vp);
	}


    if ((analysis_flags & (ANALYSIS_ADJ_AIR|ANALYSIS_EXP_AIR)) && ! use_air) {
	bu_vls_printf(_ged_current_gedp->ged_result_str, "Error:  Air regions discarded but air analysis requested!\nSet use_air non-zero or eliminate air analysis\n");
	return GED_ERROR;
    }

    return GED_OK;
}


void
view_reports(struct cstate *state)
{
    if (analysis_flags & ANALYSIS_VOLUMES) {
	int obj;
	int view;

	/* for each object, compute the volume for all views */
	for (obj = 0; obj < num_objects; obj++) {
	    double val;
	    /* compute volume of object for given view */
	    view = state->curr_view;

	    /* compute the per-view volume of this object */

	    if (state->shots[view] > 0) {
		val = obj_tbl[obj].o_volume[view] =
		    obj_tbl[obj].o_len[view] * (state->area[view] / state->shots[view]);

		if (verbose)
		    bu_vls_printf(_ged_current_gedp->ged_result_str, "\t%s volume %g %s\n",
				  obj_tbl[obj].o_name,
				  val / units[VOL]->val,
				  units[VOL]->name);
	    }
	}
    }
    if (analysis_flags & ANALYSIS_WEIGHTS) {
	int obj;
	int view = state->curr_view;

	for (obj = 0; obj < num_objects; obj++) {
	    double grams_per_cu_mm = obj_tbl[obj].o_lenDensity[view] *
		(state->area[view] / state->shots[view]);


	    if (verbose)
		bu_vls_printf(_ged_current_gedp->ged_result_str, "\t%s %g %s\n",
			      obj_tbl[obj].o_name,
			      grams_per_cu_mm / units[WGT]->val,
			      units[WGT]->name);
	}
    }
}


/**
 * These checks are unique because they must both be completed.  Early
 * termination before they are done is not an option.  The results
 * computed here are used later.
 *
 * Returns:
 * 0 terminate
 * 1 continue processing
 */
static int
weight_volume_terminate(struct cstate *state)
{
    /* Both weight and volume computations rely on this routine to
     * compute values that are printed in summaries.  Hence, both
     * checks must always be done before this routine exits.  So we
     * store the status (can we terminate processing?) in this
     * variable and act on it once both volume and weight computations
     * are done.
     */
    int can_terminate = 1;

    double low, hi, val, delta;

    if (analysis_flags & ANALYSIS_WEIGHTS) {
	/* for each object, compute the weight for all views */
	int obj;

	for (obj = 0; obj < num_objects; obj++) {
	    int view;
	    double tmp;

	    if (verbose)
		bu_vls_printf(_ged_current_gedp->ged_result_str, "object %d\n", obj);

	    /* compute weight of object for given view */
	    low = INFINITY;
	    hi = -INFINITY;
	    tmp = 0.0;
	    for (view = 0; view < num_views; view++) {
		val = obj_tbl[obj].o_weight[view] =
		    obj_tbl[obj].o_lenDensity[view] * (state->area[view] / state->shots[view]);
		V_MIN(low, val);
		V_MAX(hi, val);
		tmp += val;
	    }
	    delta = hi - low;

	    if (verbose)
		bu_vls_printf(_ged_current_gedp->ged_result_str,
		    "\t%s running avg weight %g %s hi=(%g) low=(%g)\n",
		    obj_tbl[obj].o_name,
		    (tmp / num_views) / units[WGT]->val,
		    units[WGT]->name,
		    hi / units[WGT]->val,
		    low / units[WGT]->val);

	    if (delta > weight_tolerance) {
		/* this object differs too much in each view, so we
		 * need to refine the grid. signal that we cannot
		 * terminate.
		 */
		can_terminate = 0;
		if (verbose)
		    bu_vls_printf(_ged_current_gedp->ged_result_str, "\t%s differs too much in weight per view.\n",
				  obj_tbl[obj].o_name);
	    }
	}
	if (can_terminate) {
	    if (verbose)
		bu_vls_printf(_ged_current_gedp->ged_result_str, "all objects within tolerance on weight calculation\n");
	}
    }

    if (analysis_flags & ANALYSIS_VOLUMES) {
	/* find the range of values for object volumes */
	int obj;

	/* for each object, compute the volume for all views */
	for (obj = 0; obj < num_objects; obj++) {
	    int view;
	    double tmp;

	    /* compute volume of object for given view */
	    low = INFINITY;
	    hi = -INFINITY;
	    tmp = 0.0;
	    for (view = 0; view < num_views; view++) {
		val = obj_tbl[obj].o_volume[view] =
		    obj_tbl[obj].o_len[view] * (state->area[view] / state->shots[view]);
		V_MIN(low, val);
		V_MAX(hi, val);
		tmp += val;
	    }
	    delta = hi - low;

	    if (verbose)
		bu_vls_printf(_ged_current_gedp->ged_result_str,
		    "\t%s running avg volume %g %s hi=(%g) low=(%g)\n",
		    obj_tbl[obj].o_name,
		    (tmp / num_views) / units[VOL]->val, units[VOL]->name,
		    hi / units[VOL]->val,
		    low / units[VOL]->val);

	    if (delta > volume_tolerance) {
		/* this object differs too much in each view, so we
		 * need to refine the grid.
		 */
		can_terminate = 0;
		if (verbose)
		    bu_vls_printf(_ged_current_gedp->ged_result_str, "\tvolume tol not met on %s.  Refine grid\n",
				  obj_tbl[obj].o_name);
		break;
	    }
	}
    }

    if (can_terminate) {
	return 0; /* signal we don't want to go onward */
    }
    return 1;
}


/**
 * Check to see if we are done processing due to some user specified
 * limit being achieved.
 *
 * Returns:
 * 0 Terminate
 * 1 Continue processing
 */
int
terminate_check(struct cstate *state)
{
    int wv_status;
    int view;
    int obj;

    DLOG(_ged_current_gedp->ged_result_str, "terminate_check\n");
    RT_CK_RTI(state->rtip);

    if (plot_overlaps) fflush(plot_overlaps);
    if (plot_weight) fflush(plot_weight);
    if (plot_volume) fflush(plot_volume);
    if (plot_adjair) fflush(plot_adjair);
    if (plot_gaps) fflush(plot_gaps);
    if (plot_expair) fflush(plot_expair);

    /* this computation is done first, because there are side effects
     * that must be obtained whether we terminate or not
     */
    wv_status = weight_volume_terminate(state);


    /* if we've reached the grid limit, we're done, no matter what */
    if (gridSpacing < gridSpacingLimit) {
	bu_vls_printf(_ged_current_gedp->ged_result_str, "NOTE: Stopped, grid spacing refined to %g (below lower limit %g).\n",
	    gridSpacing, gridSpacingLimit);
	return 0;
    }

    /* if we are doing one of the "Error" checking operations:
     * Overlap, gap, adj_air, exp_air, then we ALWAYS go to the grid
     * spacing limit and we ALWAYS terminate on first error/list-entry
     */
    if ((analysis_flags & ANALYSIS_OVERLAPS)) {
	if (BU_LIST_NON_EMPTY(&overlapList.l)) {
	    /* since we've found an overlap, we can quit */
	    return 0;
	} else {
	    bu_vls_printf(_ged_current_gedp->ged_result_str, "overlaps list at %gmm is empty\n", gridSpacing / GRIDSPACING_STEP);
	}
    }
    if ((analysis_flags & ANALYSIS_GAPS)) {
	if (BU_LIST_NON_EMPTY(&gapList.l)) {
	    /* since we've found a gap, we can quit */
	    return 0;
	}
    }
    if ((analysis_flags & ANALYSIS_ADJ_AIR)) {
	if (BU_LIST_NON_EMPTY(&adjAirList.l)) {
	    /* since we've found adjacent air, we can quit */
	    return 0;
	}
    }
    if ((analysis_flags & ANALYSIS_EXP_AIR)) {
	if (BU_LIST_NON_EMPTY(&exposedAirList.l)) {
	    /* since we've found exposed air, we can quit */
	    return 0;
	}
    }


    if (analysis_flags & (ANALYSIS_WEIGHTS|ANALYSIS_VOLUMES)) {
	/* volume/weight checks only get to terminate processing if
	 * there are no "error" check computations being done
	 */
	if (analysis_flags & (ANALYSIS_GAPS|ANALYSIS_ADJ_AIR|ANALYSIS_OVERLAPS|ANALYSIS_EXP_AIR)) {
	    if (verbose)
		bu_vls_printf(_ged_current_gedp->ged_result_str, "Volume/Weight tolerance met.  Cannot terminate calculation due to error computations\n");
	} else {
	    struct region *regp;
	    int all_hit = 1;
	    size_t hits;

	    if (require_num_hits > 0) {
		/* check to make sure every region was hit at least once */
		for (BU_LIST_FOR (regp, region, &(state->rtip->HeadRegion))) {
		    RT_CK_REGION(regp);

		    hits = (size_t)((struct per_region_data *)regp->reg_udata)->hits;
		    if (hits < require_num_hits) {
			all_hit = 0;
			if (verbose) {
			    if (hits == 0 && !quiet_missed_report) {
				bu_vls_printf(_ged_current_gedp->ged_result_str, "%s was not hit\n", regp->reg_name);
			    } else if (hits) {
				bu_vls_printf(_ged_current_gedp->ged_result_str, "%s hit only %zu times (< %zu)\n",
					      regp->reg_name, hits, require_num_hits);
			    }
			}
		    }
		}

		if (all_hit && wv_status == 0) {
		    if (verbose)
			bu_vls_printf(_ged_current_gedp->ged_result_str, "%s: Volume/Weight tolerance met. Terminate\n", CPP_FILELINE);
		    return 0; /* terminate */
		}
	    } else {
		if (wv_status == 0) {
		    if (verbose)
			bu_vls_printf(_ged_current_gedp->ged_result_str, "%s: Volume/Weight tolerance met. Terminate\n", CPP_FILELINE);
		    return 0; /* terminate */
		}
	    }
	}
    }

    for (view=0; view < num_views; view++) {
	for (obj = 0; obj < num_objects; obj++) {
	    VSCALE(&obj_tbl[obj].o_moi[view*3], &obj_tbl[obj].o_moi[view*3], 0.25);
	    VSCALE(&obj_tbl[obj].o_poi[view*3], &obj_tbl[obj].o_poi[view*3], 0.25);
	}

	VSCALE(&state->m_moi[view*3], &state->m_moi[view*3], 0.25);
	VSCALE(&state->m_poi[view*3], &state->m_poi[view*3], 0.25);
    }

    return 1;
}


/**
 * summary_reports
 */
void
summary_reports(struct cstate *state)
{
    int view;
    int obj;
    double avg_mass;
    struct region *regp;

    if (multiple_analyses)
	bu_vls_printf(_ged_current_gedp->ged_result_str, "Summaries (%gmm grid spacing):\n", gridSpacing / GRIDSPACING_STEP);
    else
	bu_vls_printf(_ged_current_gedp->ged_result_str, "Summary (%gmm grid spacing):\n", gridSpacing / GRIDSPACING_STEP);

    if (analysis_flags & ANALYSIS_WEIGHTS) {
	bu_vls_printf(_ged_current_gedp->ged_result_str, "Weight:\n");
	for (obj = 0; obj < num_objects; obj++) {
	    avg_mass = 0.0;

	    for (view=0; view < num_views; view++) {
		/* computed in terminate_check() */
		avg_mass += obj_tbl[obj].o_weight[view];
	    }
	    avg_mass /= num_views;
	    bu_vls_printf(_ged_current_gedp->ged_result_str, "\t%*s %g %s\n", -max_region_name_len, obj_tbl[obj].o_name,
			  avg_mass / units[WGT]->val, units[WGT]->name);

	    if (analysis_flags & ANALYSIS_CENTROIDS &&
		!ZERO(avg_mass)) {
		vect_t centroid = VINIT_ZERO;
		fastf_t Dx_sq, Dy_sq, Dz_sq;
		fastf_t inv_total_mass = 1.0/avg_mass;

		for (view=0; view < num_views; view++) {
		    vect_t torque;
		    fastf_t cell_area = state->area[view] / state->shots[view];

		    VSCALE(torque, &obj_tbl[obj].o_lenTorque[view*3], cell_area);
		    VADD2(centroid, centroid, torque);
		}

		VSCALE(centroid, centroid, 1.0/(fastf_t)num_views);
		VSCALE(centroid, centroid, inv_total_mass);
		bu_vls_printf(_ged_current_gedp->ged_result_str,
			      "\t\tcentroid: (%g %g %g) mm\n", V3ARGS(centroid));

		/* Do the final calculations for the moments of
		 * inertia for the current object.
		 */
		if (analysis_flags & ANALYSIS_MOMENTS) {
		    struct bu_vls title = BU_VLS_INIT_ZERO;
		    mat_t tmat; /* total mat */

		    MAT_ZERO(tmat);
		    for (view=0; view < num_views; view++) {
			vectp_t moi = &obj_tbl[obj].o_moi[view*3];
			vectp_t poi = &obj_tbl[obj].o_poi[view*3];

			tmat[MSX] += moi[X];
			tmat[MSY] += moi[Y];
			tmat[MSZ] += moi[Z];
			tmat[1] += poi[X];
			tmat[2] += poi[Y];
			tmat[6] += poi[Z];
		    }

		    tmat[MSX] /= (fastf_t)num_views;
		    tmat[MSY] /= (fastf_t)num_views;
		    tmat[MSZ] /= (fastf_t)num_views;
		    tmat[1] /= (fastf_t)num_views;
		    tmat[2] /= (fastf_t)num_views;
		    tmat[6] /= (fastf_t)num_views;

		    /* Lastly, apply the parallel axis theorem */
		    Dx_sq = centroid[X]*centroid[X];
		    Dy_sq = centroid[Y]*centroid[Y];
		    Dz_sq = centroid[Z]*centroid[Z];
		    tmat[MSX] -= avg_mass*(Dy_sq + Dz_sq);
		    tmat[MSY] -= avg_mass*(Dx_sq + Dz_sq);
		    tmat[MSZ] -= avg_mass*(Dx_sq + Dy_sq);
		    tmat[1] += avg_mass*centroid[X]*centroid[Y];
		    tmat[2] += avg_mass*centroid[X]*centroid[Z];
		    tmat[6] += avg_mass*centroid[Y]*centroid[Z];

		    tmat[4] = tmat[1];
		    tmat[8] = tmat[2];
		    tmat[9] = tmat[6];

		    bu_vls_printf(&title, "For the Moments and Products of Inertia For %s", obj_tbl[obj].o_name);
		    bn_mat_print_vls(bu_vls_addr(&title), tmat, _ged_current_gedp->ged_result_str);
		    bu_vls_free(&title);
		}
	    }
	}


	if (print_per_region_stats) {
	    double *wv;
	    bu_vls_printf(_ged_current_gedp->ged_result_str, "\tregions:\n");
	    for (BU_LIST_FOR (regp, region, &(state->rtip->HeadRegion))) {
		double low = INFINITY;
		double hi = -INFINITY;

		avg_mass = 0.0;

		for (view=0; view < num_views; view++) {
		    wv = &((struct per_region_data *)regp->reg_udata)->r_weight[view];

		    *wv = ((struct per_region_data *)regp->reg_udata)->r_lenDensity[view] *
			(state->area[view]/state->shots[view]);

		    *wv /= units[WGT]->val;

		    avg_mass += *wv;

		    if (*wv < low) low = *wv;
		    if (*wv > hi) hi = *wv;
		}

		avg_mass /= num_views;
		bu_vls_printf(_ged_current_gedp->ged_result_str, "\t%s %g %s +(%g) -(%g)\n",
			      regp->reg_name,
			      avg_mass,
			      units[WGT]->name,
			      hi - avg_mass,
			      avg_mass - low);
	    }
	}

	/* print grand totals */
	avg_mass = 0.0;
	for (view=0; view < num_views; view++) {
	    avg_mass += state->m_weight[view] =
		state->m_lenDensity[view] *
		(state->area[view] / state->shots[view]);
	}

	avg_mass /= num_views;
	bu_vls_printf(_ged_current_gedp->ged_result_str, "  Average total weight: %g %s\n", avg_mass / units[WGT]->val, units[WGT]->name);

	if (analysis_flags & ANALYSIS_CENTROIDS &&
	    !ZERO(avg_mass)) {
	    vect_t centroid = VINIT_ZERO;
	    fastf_t Dx_sq, Dy_sq, Dz_sq;
	    fastf_t inv_total_mass = 1.0/avg_mass;

	    for (view=0; view < num_views; view++) {
		vect_t torque;
		fastf_t cell_area = state->area[view] / state->shots[view];

		VSCALE(torque, &state->m_lenTorque[view*3], cell_area);
		VADD2(centroid, centroid, torque);
	    }

	    VSCALE(centroid, centroid, 1.0/(fastf_t)num_views);
	    VSCALE(centroid, centroid, inv_total_mass);
	    bu_vls_printf(_ged_current_gedp->ged_result_str,
			  "  Average centroid: (%g %g %g) mm\n", V3ARGS(centroid));

	    /* Do the final calculations for the moments of inertia
	     * for the current object.
	     */
	    if (analysis_flags & ANALYSIS_MOMENTS) {
		mat_t tmat; /* total mat */

		MAT_ZERO(tmat);
		for (view=0; view < num_views; view++) {
		    vectp_t moi = &state->m_moi[view*3];
		    vectp_t poi = &state->m_poi[view*3];

		    tmat[MSX] += moi[X];
		    tmat[MSY] += moi[Y];
		    tmat[MSZ] += moi[Z];
		    tmat[1] += poi[X];
		    tmat[2] += poi[Y];
		    tmat[6] += poi[Z];
		}

		tmat[MSX] /= (fastf_t)num_views;
		tmat[MSY] /= (fastf_t)num_views;
		tmat[MSZ] /= (fastf_t)num_views;
		tmat[1] /= (fastf_t)num_views;
		tmat[2] /= (fastf_t)num_views;
		tmat[6] /= (fastf_t)num_views;

		/* Lastly, apply the parallel axis theorem */
		Dx_sq = centroid[X]*centroid[X];
		Dy_sq = centroid[Y]*centroid[Y];
		Dz_sq = centroid[Z]*centroid[Z];
		tmat[MSX] -= avg_mass*(Dy_sq + Dz_sq);
		tmat[MSY] -= avg_mass*(Dx_sq + Dz_sq);
		tmat[MSZ] -= avg_mass*(Dx_sq + Dy_sq);
		tmat[1] += avg_mass*centroid[X]*centroid[Y];
		tmat[2] += avg_mass*centroid[X]*centroid[Z];
		tmat[6] += avg_mass*centroid[Y]*centroid[Z];

		tmat[4] = tmat[1];
		tmat[8] = tmat[2];
		tmat[9] = tmat[6];

		bn_mat_print_vls("For the Moments and Products of Inertia For\n\tAll Specified Objects",
				 tmat, _ged_current_gedp->ged_result_str);
	    }
	}
    }


    if (analysis_flags & ANALYSIS_VOLUMES) {
	bu_vls_printf(_ged_current_gedp->ged_result_str, "Volume:\n");

	/* print per-object */
	for (obj = 0; obj < num_objects; obj++) {
	    avg_mass = 0.0;

	    for (view=0; view < num_views; view++)
		avg_mass += obj_tbl[obj].o_volume[view];

	    avg_mass /= num_views;
	    bu_vls_printf(_ged_current_gedp->ged_result_str, "\t%*s %g %s\n", -max_region_name_len, obj_tbl[obj].o_name,
			  avg_mass / units[VOL]->val, units[VOL]->name);
	}

	if (print_per_region_stats) {
	    double *vv;

	    bu_vls_printf(_ged_current_gedp->ged_result_str, "\tregions:\n");
	    for (BU_LIST_FOR (regp, region, &(state->rtip->HeadRegion))) {
		double low = INFINITY;
		double hi = -INFINITY;
		avg_mass = 0.0;

		for (view=0; view < num_views; view++) {
		    vv = &((struct per_region_data *)regp->reg_udata)->r_volume[view];

		    /* convert view length to a volume */
		    *vv = ((struct per_region_data *)regp->reg_udata)->r_len[view] *
			(state->area[view] / state->shots[view]);

		    /* convert to user's units */
		    *vv /= units[VOL]->val;

		    /* find limits of values */
		    if (*vv < low) low = *vv;
		    if (*vv > hi) hi = *vv;

		    avg_mass += *vv;
		}

		avg_mass /= num_views;

		bu_vls_printf(_ged_current_gedp->ged_result_str, "\t%s volume:%g %s +(%g) -(%g)\n",
			      regp->reg_name,
			      avg_mass,
			      units[VOL]->name,
			      hi - avg_mass,
			      avg_mass - low);
	    }
	}


	/* print grand totals */
	avg_mass = 0.0;
	for (view=0; view < num_views; view++) {
	    avg_mass += state->m_volume[view] =
		state->m_len[view] * (state->area[view] / state->shots[view]);
	}

	avg_mass /= num_views;
	bu_vls_printf(_ged_current_gedp->ged_result_str, "  Average total volume: %g %s\n", avg_mass / units[VOL]->val, units[VOL]->name);
    }
    if (analysis_flags & ANALYSIS_OVERLAPS) list_report(&overlapList);
    if (analysis_flags & ANALYSIS_ADJ_AIR) list_report(&adjAirList);
    if (analysis_flags & ANALYSIS_GAPS) list_report(&gapList);
    if (analysis_flags & ANALYSIS_EXP_AIR) list_report(&exposedAirList);

    for (BU_LIST_FOR (regp, region, &(state->rtip->HeadRegion))) {
	size_t hits;
	struct region_pair *rp;
	int is_overlap_only_hit;

	RT_CK_REGION(regp);
	hits = (size_t)((struct per_region_data *)regp->reg_udata)->hits;
	if (hits < require_num_hits) {
	    if (hits == 0 && !quiet_missed_report) {
		is_overlap_only_hit = 0;
		if (analysis_flags & ANALYSIS_OVERLAPS) {
		    /* If the region is in the overlap list, it has
		     * been hit even though the hit count is zero.
		     * Do not report zero hit regions if they are in
		     * the overlap list.
		     */
		    for (BU_LIST_FOR (rp, region_pair, &(overlapList.l))) {
			if (rp->r.r1->reg_name == regp->reg_name) {
			    is_overlap_only_hit = 1;
			    break;
			} else if (rp->r2) {
			    if (rp->r2->reg_name == regp->reg_name) {
				is_overlap_only_hit = 1;
				break;
			    }
			}
		    }
		}
		if (!is_overlap_only_hit) {
		    bu_vls_printf(_ged_current_gedp->ged_result_str, "%s was not hit\n", regp->reg_name);
		}
	    } else if (hits) {
		bu_vls_printf(_ged_current_gedp->ged_result_str, "%s hit only %zu times (< %zu)\n",
			      regp->reg_name, hits, require_num_hits);
	    }
	}
    }
}


extern "C" int
ged_gqa_core(struct ged *gedp, int argc, const char *argv[])
{
    int arg_count;
    struct rt_i *rtip;
    int i;
    struct cstate state;
    int start_objs; /* index in command line args where geom object list starts */
    struct region_pair *rp;
    struct region *regp;
    static const char *usage = "object [object ...]";
    struct resource resp[MAX_PSW];	/* memory resources for multi-cpu processing */

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s %s", argv[0], options_str, usage);
	return GED_HELP;
    }

    _ged_current_gedp = gedp;

    analysis_flags = ANALYSIS_VOLUMES | ANALYSIS_OVERLAPS | ANALYSIS_WEIGHTS |
	ANALYSIS_EXP_AIR | ANALYSIS_ADJ_AIR | ANALYSIS_GAPS | ANALYSIS_CENTROIDS | ANALYSIS_MOMENTS;
    multiple_analyses = 1;
    azimuth_deg = 0.0;
    elevation_deg = 0.0;
    densityFileName = (char *)0;

    /* FIXME: this is completely arbitrary, should probably be based
     * on the model size.
     */
    gridSpacing = 50.0;

    /* default grid spacing limit is based on the current distance
     * tolerance, one order of magnitude greater.
     *
     * FIXME: should probably be based on the model size.
     */
    gridSpacingLimit = 10.0 * gedp->ged_wdbp->wdb_tol.dist;

    makeOverlapAssemblies = 0;
    require_num_hits = 1;
    max_cpus = ncpu = bu_avail_cpus();
    Samples_per_model_axis = 2.0;
    overlap_tolerance = 0.0;
    volume_tolerance = -1.0;
    weight_tolerance = -1.0;
    print_per_region_stats = 0;
    max_region_name_len = 0;
    use_air = 1;
    num_objects = 0;
    num_views = 3;
    verbose = 0;
    quiet_missed_report = 0;
    plot_prefix = NULL;
    plot_weight = (FILE *)0;
    plot_volume = (FILE *)0;
    plot_overlaps = (FILE *)0;
    plot_adjair = (FILE *)0;
    plot_gaps = (FILE *)0;
    plot_expair = (FILE *)0;
    debug = 0;

    /* parse command line arguments */
    arg_count = parse_args(argc, (char **)argv);

    if (arg_count < 0 || (argc-arg_count) < 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s %s", argv[0], options_str, usage);
	return GED_ERROR;
    }

    if (analysis_flags & ANALYSIS_PLOT_OVERLAPS) {
	ged_gqa_plot.vbp = rt_vlblock_init();
	ged_gqa_plot.vhead = bn_vlblock_find(ged_gqa_plot.vbp, 0xFF, 0xFF, 0x00);
    }

    rtip = rt_new_rti(gedp->ged_wdbp->dbip);
    rtip->useair = use_air;

    start_objs = arg_count;
    num_objects = argc - arg_count;

    /* Initialize all the per-CPU memory resources.  The number of
     * processors can change at runtime, init them all.
     */
    memset(resp, 0, sizeof(resp));
    for (i = 0; i < MAX_PSW; i++) {
	rt_init_resource(&resp[i], i, rtip);
    }
    state.resp = resp;

    /* Walk trees.  Here we identify any object trees in the database
     * that the user wants included in the ray trace.
     */
    for (; arg_count < argc; arg_count++) {
	if (rt_gettree(rtip, argv[arg_count]) < 0) {
	    fprintf(stderr, "rt_gettree(%s) FAILED\n", argv[arg_count]);
	    return GED_ERROR;
	}
    }

    /* This gets the database ready for ray tracing.  (it precomputes
     * some values, sets up space partitioning, etc.)
     */
    rt_prep_parallel(rtip, ncpu);

    /* we now have to subdivide space */
    VSUB2(state.span, rtip->mdl_max, rtip->mdl_min);
    state.area[0] = state.span[1] * state.span[2];
    state.area[1] = state.span[2] * state.span[0];
    state.area[2] = state.span[0] * state.span[1];

    if (analysis_flags & ANALYSIS_BBOX) {
	bu_vls_printf(gedp->ged_result_str, "bounding box: %g %g %g  %g %g %g\n",
		      V3ARGS(rtip->mdl_min), V3ARGS(rtip->mdl_max));

	bu_vls_printf(gedp->ged_result_str, "Area: (%g, %g, %g)\n", state.area[X], state.area[Y], state.area[Z]);
    }
    if (verbose) bu_vls_printf(gedp->ged_result_str, "ncpu: %d\n", ncpu);

    /* if the user did not specify the initial grid spacing limit, we
     * need to compute a reasonable one for them.
     */
    if (ZERO(gridSpacing)) {
	double min_span = MAX_FASTF;
	VPRINT("span", state.span);

	V_MIN(min_span, state.span[X]);
	V_MIN(min_span, state.span[Y]);
	V_MIN(min_span, state.span[Z]);

	gridSpacing = gridSpacingLimit;
	do {
	    gridSpacing *= 2.0;
	} while (gridSpacing < min_span);

	/* dial it back a little bit */
	gridSpacing *= 0.25;
	V_MAX(gridSpacing, gridSpacingLimit);

	bu_log("Trying estimated initial grid spacing: %g %s\n",
	    gridSpacing / units[LINE]->val, units[LINE]->name);
    } else {
	bu_log("Trying initial grid spacing: %g %s\n",
	    gridSpacing / units[LINE]->val, units[LINE]->name);
    }

    bu_log("Using grid spacing lower limit: %g %s\n",
	    gridSpacingLimit / units[LINE]->val, units[LINE]->name);

    if (options_prep(rtip, state.span) != GED_OK) return GED_ERROR;

    /* initialize some stuff */
    state.sem_worker = bu_semaphore_register("gqa_sem_worker");
    state.sem_stats = bu_semaphore_register("gqa_sem_stats");
    state.sem_lists = bu_semaphore_register("gqa_sem_lists");
    state.sem_plot = bu_semaphore_register("gqa_sem_plot");
    state.rtip = rtip;
    state.first = 1;
    allocate_per_region_data(&state, start_objs, argc, argv);

    /* compute */
    do {
	double inv_spacing = 1.0/gridSpacing;
	int view;

	VSCALE(state.steps, state.span, inv_spacing);

	bu_log("Processing with grid spacing %g %s %ld x %ld x %ld\n",
		      gridSpacing / units[LINE]->val,
		      units[LINE]->name,
		      state.steps[0]-1,
		      state.steps[1]-1,
		      state.steps[2]-1);


	for (view=0; view < num_views; view++) {

	    if (verbose)
		bu_vls_printf(gedp->ged_result_str, "  view %d\n", view);

	    /* gross hack.  By assuming we have <= 3 views, we can let
	     * the view # indicate a coordinate axis.  Note this is
	     * used as an index into state.area[]
	     */
	    state.i_axis = state.curr_view = view;
	    state.u_axis = (state.curr_view+1) % 3;
	    state.v_axis = (state.curr_view+2) % 3;

	    state.u_dir[state.u_axis] = 1;
	    state.u_dir[state.v_axis] = 0;
	    state.u_dir[state.i_axis] = 0;

	    state.v_dir[state.u_axis] = 0;
	    state.v_dir[state.v_axis] = 1;
	    state.v_dir[state.i_axis] = 0;
	    state.v = 1;

	    bu_parallel(plane_worker, ncpu, (void *)&state);

	    if (aborted)
		goto aborted;

	    view_reports(&state);
	}

	state.first = 0;
	gridSpacing *= GRIDSPACING_STEP;

    } while (terminate_check(&state));

aborted:
    if (plot_overlaps) fclose(plot_overlaps);
    if (plot_weight) fclose(plot_weight);
    if (plot_volume) fclose(plot_volume);
    if (plot_adjair) fclose(plot_adjair);
    if (plot_gaps) fclose(plot_gaps);
    if (plot_expair) fclose(plot_expair);


    if (verbose)
	bu_vls_printf(gedp->ged_result_str, "Computation Done\n");

    if (!aborted) {
	summary_reports(&state);

	if (analysis_flags & ANALYSIS_PLOT_OVERLAPS)
	    _ged_cvt_vlblock_to_solids(gedp, ged_gqa_plot.vbp, "OVERLAPS", 0);
    } else
	aborted = 0; /* reset flag */

    if (analysis_flags & ANALYSIS_PLOT_OVERLAPS)
	bn_vlblock_free(ged_gqa_plot.vbp);

    /* Clear out the lists */
    while (BU_LIST_WHILE (rp, region_pair, &overlapList.l)) {
	BU_LIST_DEQUEUE(&rp->l);
	bu_free(rp, "overlapList items");
    }
    while (BU_LIST_WHILE (rp, region_pair, &adjAirList.l)) {
	BU_LIST_DEQUEUE(&rp->l);
	bu_free(rp, "adjAirList items");
    }
    while (BU_LIST_WHILE (rp, region_pair, &gapList.l)) {
	BU_LIST_DEQUEUE(&rp->l);
	bu_free(rp, "gapList items");
    }
    while (BU_LIST_WHILE (rp, region_pair, &exposedAirList.l)) {
	BU_LIST_DEQUEUE(&rp->l);
	bu_free(rp, "exposedAirList items");
    }

    /* Free dynamically allocated state */
    bu_free(state.m_lenDensity, "m_lenDensity");
    bu_free(state.m_len, "m_len");
    bu_free(state.m_volume, "m_volume");
    bu_free(state.m_weight, "m_weight");
    bu_free(state.shots, "m_shots");
    bu_free(state.m_lenTorque, "m_lenTorque");
    bu_free(state.m_moi, "m_moi");
    bu_free(state.m_poi, "m_poi");

    for (i = 0; i < num_objects; i++) {
	bu_free(obj_tbl[i].o_len, "o_len");
	bu_free(obj_tbl[i].o_lenDensity, "o_lenDensity");
	bu_free(obj_tbl[i].o_volume, "o_volume");
	bu_free(obj_tbl[i].o_weight, "o_weight");
	bu_free(obj_tbl[i].o_lenTorque, "o_lenTorque");
	bu_free(obj_tbl[i].o_moi, "o_moi");
	bu_free(obj_tbl[i].o_poi, "o_poi");
    }
    bu_free(obj_tbl, "object table");
    obj_tbl = NULL;

    for (i = 0, BU_LIST_FOR (regp, region, &(rtip->HeadRegion)), i++) {
	bu_free(reg_tbl[i].r_lenDensity, "r_lenDensity");
	bu_free(reg_tbl[i].r_len, "r_len");
	bu_free(reg_tbl[i].r_volume, "r_volume");
	bu_free(reg_tbl[i].r_weight, "r_weight");
    }
    bu_free(reg_tbl, "object table");
    reg_tbl = NULL;

    if (_gd_densities) {
	analyze_densities_destroy(_gd_densities);
	_gd_densities = NULL;
    }

    if (_gd_densities_source) {
	bu_free(_gd_densities_source, "free densities source string");
	_gd_densities_source = NULL;
    }

    rt_free_rti(rtip);

    return GED_OK;
}


#ifdef GED_PLUGIN
#include "../include/plugin.h"
extern "C" {
    struct ged_cmd_impl gqa_cmd_impl = {
	"gqa",
	ged_gqa_core,
	GED_CMD_DEFAULT
    };

    const struct ged_cmd gqa_cmd = { &gqa_cmd_impl };
    const struct ged_cmd *gqa_cmds[] = { &gqa_cmd, NULL };

    static const struct ged_plugin pinfo = { GED_API,  gqa_cmds, 1 };

    COMPILER_DLLEXPORT const struct ged_plugin *ged_plugin_info()
    {
	return &pinfo;
    }
}
#endif /* GED_PLUGIN */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
