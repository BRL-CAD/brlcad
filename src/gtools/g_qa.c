/*                          G _ Q A . C
 * BRL-CAD
 *
 * Copyright (c) 2005-2007 United States Government as represented by
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
/** @file g_qa.c
 *
 * Author: Lee Butler
 *
 * perform quantitative analysis checks on geometry
 *
 * XXX need to look at gap computation
 *
 * plot the points where overlaps start/stop
 *
 *	Designed to be a framework for 3d sampling of the geometry volume.
 */

#include "common.h"

#ifdef HAVE_STRING_H
#  include <string.h>
#else
#  include <strings.h>
#endif
#include <stdlib.h>
#include <errno.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <math.h>
#include <limits.h>			/* home of INT_MAX aka MAXINT */

#include "machine.h"
#include "bu.h"
#include "vmath.h"
#include "raytrace.h"
#include "plot3.h"


#define SEM_WORK RT_SEM_LAST
#define SEM_SHOTS RT_SEM_LAST+1
#define SEM_STATS RT_SEM_LAST+2 /* semaphore for statistics */
#define SEM_LIST RT_SEM_LAST+3
#define TOTAL_SEMAPHORES SEM_LIST+1

/* bu_getopt() options */
char *options = "A:a:de:f:g:Gn:N:pP:rS:s:t:U:u:vV:W:";

/* variables set by command line flags */
char *progname = "(noname)";
char *usage_msg = "Usage: %s [options] model object [object...]\n\
";


#define ANALYSIS_VOLUME 1
#define ANALYSIS_WEIGHT 2
#define ANALYSIS_OVERLAPS 4
#define ANALYSIS_ADJ_AIR 8
#define ANALYSIS_GAP	16
#define ANALYSIS_EXP_AIR 32 /* exposed air */
#define ANALYSIS_BOX 64
#define ANALYSIS_INTERFACES 128

#ifndef HUGE
#  ifdef MAXFLT
#    define HUGE MAXFLOAT
#  else
#    ifdef DBL_MAX
#      define HUGE DBL_MAX
#    else
#      define HUGE ((float)3.40282346638528860e+38)
#    endif
#  endif
#endif

int analysis_flags = ANALYSIS_VOLUME | ANALYSIS_OVERLAPS | ANALYSIS_WEIGHT | \
	ANALYSIS_EXP_AIR | ANALYSIS_ADJ_AIR | ANALYSIS_GAP ;
int multiple_analyses = 1;


double azimuth_deg;
double elevation_deg;
char *densityFileName;
double gridSpacing = 50.0;
double gridSpacingLimit = 0.25; /* limit to 1/4 mm */
char makeOverlapAssemblies;
int require_num_hits = 1;
int ncpu = 1;
double Samples_per_model_axis = 2.0;
double overlap_tolerance;
double volume_tolerance = -1.0;
double weight_tolerance = -1.0;

int print_per_region_stats;
int max_region_name_len;
int use_air = 1;
int num_objects; /* number of objects specified on command line */
int max_cpus;
int num_views = 3;
int verbose;

int plot_files;	/* Boolean: Should we produce plot files? */
FILE *plot_weight;
FILE *plot_volume;
FILE *plot_overlaps;
FILE *plot_adjair;
FILE *plot_gaps;
FILE *plot_expair;

int overlap_color[3] = { 255, 255, 0 };	 /* yellow */
int gap_color[3] = { 128, 192, 255 };    /* cyan */
int adjAir_color[3] = { 128, 255, 192 }; /* pale green */
int expAir_color[3] = { 255, 128, 255 }; /* magenta */

int debug;
#define DLOG if (debug) bu_log

/* Some defines for re-using the values from the application structure
 * for other purposes
 */
#define A_LENDEN a_color[0]
#define A_LEN a_color[1]
#define A_STATE a_uptr



struct resource	resource[MAX_PSW];	/* memory resources for multi-cpu processing */

struct cstate {
    int curr_view;	/* the "view" number we are shooting */
    int u_axis; /* these 3 are in the range 0..2 inclusive and indicate which axis (X,Y,or Z) */
    int v_axis; /* is being used for the U, V, or invariant vector direction */
    int i_axis;

    /* SEM_WORK protects this */
    int v;	/* indicates how many "grid_size" steps in the v direction have been taken */

    /* SEM_STATS protects this */
    double	*m_lenDensity;
    double	*m_len;
    double	*m_volume;
    double	*m_weight;
    unsigned long *shots;
    int first;		/* this is the first time we've computed a set of views */

    vect_t u_dir;		/* direction of U vector for "current view" */
    vect_t v_dir;		/* direction of V vector for "current view" */
    struct rt_i *rtip;
    long steps[3];	/* this is per-dimension, not per-view */
    vect_t span;	/* How much space does the geometry span in each of X,Y,Z directions */
    vect_t area;	/* area of the view for view with invariant at index */

};

/* the entries in the density table */
struct density_entry {
    long	magic;
    double	grams_per_cu_mm;
    char	*name;
} *densities;
int num_densities;
#define DENSITY_MAGIC 0xaf0127


/* summary data structure for objects specified on command line */
struct per_obj_data {
    char *o_name;
    double *o_len;
    double *o_lenDensity;
    double *o_volume;
    double *o_weight;
} *obj_tbl;

/* this is the data we track for each region
 */
struct per_region_data {
    unsigned long hits;
    double	*r_lenDensity; /* for per-region per-view weight computation */
    double	*r_len; /* for per-region, per-veiew computation */
    double	*r_weight;
    double	*r_volume;
    struct per_obj_data *optr;
} *reg_tbl;



struct region_pair {
    struct bu_list 	l;
    union {
	char *name;
	struct region 	*r1;
    } r;
    struct region 	*r2;
    unsigned long	count;
    double		max_dist;
    vect_t		coord;
};

/* Access to these lists should be in sections
 * of code protected by SEM_LIST
 */

static struct region_pair gapList = { /* list of gaps */
    {
	BU_LIST_HEAD_MAGIC,
	(struct bu_list *)&gapList,
	(struct bu_list *)&gapList 
    },
    { "Gaps" },
    (struct region *)NULL,
    (unsigned long)0,
    (double)0.0,
    {0.0, 0.0, 0.0,}
};
static struct region_pair adjAirList = { /* list of adjacent air */
    {
	BU_LIST_HEAD_MAGIC,
	(struct bu_list *)&adjAirList,
	(struct bu_list *)&adjAirList
    },
    { (char *)"Adjacent Air" },
    (struct region *)NULL,
    (unsigned long)0,
    (double)0.0,
    {0.0, 0.0, 0.0,}
};
static struct region_pair exposedAirList = { /* list of exposed air */
    {
	BU_LIST_HEAD_MAGIC,
	(struct bu_list *)&exposedAirList,
	(struct bu_list *)&exposedAirList
    },
    { "Exposed Air" },
    (struct region *)NULL,
    (unsigned long)0,
    (double)0.0,
    {0.0, 0.0, 0.0,}
};
static struct region_pair overlapList = { /* list of overlaps */
    {
	BU_LIST_HEAD_MAGIC,
	(struct bu_list *)&overlapList,
	(struct bu_list *)&overlapList
    },
    { "Overlaps" },
    (struct region *)NULL,
    (unsigned long)0,
    (double)0.0,
    {0.0, 0.0, 0.0,}
};


/* XXX this section should be extracted to libbu/units.c */

/* This structure holds the name of a unit value, and the conversion
 * factor necessary to convert from/to BRL-CAD statndard units.
 *
 * The standard units are millimeters, cubic millimeters, and grams.
 */
struct cvt_tab {
    double val;
    char name[32];
};

static const struct cvt_tab units_tab[3][40] = {
    { /* length, stolen from bu/units.c with the  "none" value removed
       * Values for converting from given units to mm
       */
	{1.0,		"mm"}, /* default */
	/*	{0.0,		"none"},*/ /* this is removed to force a certain
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
	{25.4,		"inche"},		/* for plural */
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
    {/* volume
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
    { /* weight
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

/* this table keeps track of the "current" or "user selected units and the
 * associated conversion values
 */
#define LINE 0
#define VOL 1
#define WGT 2
static const struct cvt_tab *units[3] = {
    &units_tab[0][0],	/* linear */
    &units_tab[1][0],	/* volume */
    &units_tab[2][0]	/* weight */
};


/*
 *	read_units_double
 *
 *	Read a non-negative floating point value with optional units
 *
 *	Return
 *		1 Failure
 *		0 Success
 */
int
read_units_double(double *val, char *buf, const struct cvt_tab *cvt)
{
    double a;
    char units_string[256] = {0};
    int i;


    i = sscanf(buf, "%lg%s", &a, units_string);

    if (i < 0) return 1;

    if (i == 1) {
	*val = a;

	return 0;
    }
    if (i == 2) {
	*val = a;
	for ( ; cvt->name[0] != '\0' ; ) {
	    if (!strncmp(cvt->name, units_string, 256)) {
		goto found_units;
	    } else {
		cvt++;
	    }
	}
	bu_log("Bad units specifier \"%s\" on value \"%s\"\n", units_string, buf);
	return 1;

    found_units:
	*val = a * cvt->val;
	return 0;
    }
    bu_log("%s sscanf problem on \"%s\"  got %d\n", BU_FLSTR, buf, i);
    return 1;
}

/* the above should be extracted to libbu/units.c */




/*
 *	U S A G E --- tell user how to invoke this program, then exit
 */
void
usage(s)
     char *s;
{
    if (s) (void)fputs(s, stderr);

    (void) fprintf(stderr, usage_msg, progname);

    exit(1);
}



/*
 *	P A R S E _ A R G S --- Parse through command line flags
 */
int
parse_args(int ac, char *av[])
{
    int  c;
    int i;
    double a;
    char *p;

    if (  ! (progname=strrchr(*av, '/'))  )
	progname = *av;
    else
	++progname;

    /* Turn off getopt's error messages */
    bu_opterr = 0;

    /* get all the option flags from the command line */
    while ((c=bu_getopt(ac,av,options)) != EOF) {
	switch (c) {
	case 'A'	:
	    {
		char *p;
		analysis_flags = 0;
		multiple_analyses = 0;
		for (p = bu_optarg; *p ; p++) {
		    switch (*p) {
		    case 'A' :
			analysis_flags = ANALYSIS_VOLUME | ANALYSIS_WEIGHT | \
			    ANALYSIS_OVERLAPS | ANALYSIS_ADJ_AIR | ANALYSIS_GAP | \
			    ANALYSIS_EXP_AIR;
			multiple_analyses = 1;
			break;
		    case 'a' :
			if (analysis_flags)
			    multiple_analyses = 1;

			    analysis_flags |= ANALYSIS_ADJ_AIR;

			break;
		    case 'b' :
			if (analysis_flags)
			    multiple_analyses = 1;

			analysis_flags |= ANALYSIS_BOX;

			break;
		    case 'e' :
			if (analysis_flags)
			    multiple_analyses = 1;

			analysis_flags |= ANALYSIS_EXP_AIR;
			break;
		    case 'g' :
			if (analysis_flags)
			    multiple_analyses = 1;

			analysis_flags |= ANALYSIS_GAP;
			break;
		    case 'o' :
			if (analysis_flags)
			    multiple_analyses = 1;

			analysis_flags |= ANALYSIS_OVERLAPS;
			break;
		    case 'v' :
			if (analysis_flags)
			    multiple_analyses = 1;

			analysis_flags |= ANALYSIS_VOLUME;
			break;
		    case 'w' :
			if (analysis_flags)
			    multiple_analyses = 1;

			analysis_flags |= ANALYSIS_WEIGHT;
			break;
		    default:
			bu_log("Unknown analysis type \"%c\" requested.\n", *p);
			bu_bomb("");
			break;
		    }
		}
		break;
	    }
	case 'a'	:
	    bu_log("azimuth not implemented\n");
	    if (sscanf(bu_optarg, "%lg", &azimuth_deg) != 1) {
		bu_log("error parsing azimuth \"%s\"\n", bu_optarg);
		bu_bomb("");
	    }
	    break;
	case 'e'	:
	    bu_log("elevation not implemented\n");
	    if (sscanf(bu_optarg, "%lg", &elevation_deg) != 1) {
		bu_log("error parsing elevation \"%s\"\n", bu_optarg);
		bu_bomb("");
	    }
	    break;
	case 'd'	: debug = 1; break;

	case 'f'	: densityFileName = bu_optarg; break;

	case 'g'	:
	    {
		double value1, value2;
		i = 0;


		/* find out if we have two or one args 
		 * user can separate them with , or - delimiter
		 */
		if (p = strchr(bu_optarg, ','))  	 *p++ = '\0';
		else if (p = strchr(bu_optarg, '-')) *p++ = '\0';


		if (read_units_double(&value1, bu_optarg, units_tab[0])) {
		    bu_log("error parsing grid spacing value \"%s\"\n", bu_optarg);
		    bu_bomb("");
		}
		if (p) {
		    /* we've got 2 values, they are upper limit and lower limit */
		    if (read_units_double(&value2, p, units_tab[0])) {
			bu_log("error parsing grid spacing limit value \"%s\"\n", p);
			bu_bomb("");
		    }
		    gridSpacing = value1;
		    gridSpacingLimit = value2;
		} else {
		    gridSpacingLimit = value1;
		    
		    gridSpacing = 0.0; /* flag it */
		}

		bu_log("set grid spacing:%g %s limit:%g %s\n",
		       gridSpacing / units[LINE]->val, units[LINE]->name,
		       gridSpacingLimit / units[LINE]->val, units[LINE]->name);
		break;
	    }
	case 'G'	:
	    makeOverlapAssemblies = 1;
	    bu_log("-G option unimplemented\n");
	    bu_bomb("");
	    break;
	case 'n'	:
	    if (sscanf(bu_optarg, "%d", &c) != 1 || c < 0) {
		bu_log("num_hits must be integer value >= 0, not \"%s\"\n", bu_optarg);
		bu_bomb("");
	    }
	    require_num_hits = c;
	    break;

	case 'N'	:
	    num_views = atoi(bu_optarg);
	    break;
	case 'p'	:
	    plot_files = ! plot_files;
	    break;
	case 'P'	:
	    /* cannot ask for more cpu's than the machine has */
	    if ((c=atoi(bu_optarg)) > 0 && c <= max_cpus ) ncpu = c;
	    break;
	case 'r'	:
	    print_per_region_stats = 1;
	    break;
	case 'S'	:
	    if (sscanf(bu_optarg, "%lg", &a) != 1 || a <= 1.0) {
		bu_log("error in specifying minimum samples per model axis: \"%s\"\n", bu_optarg);
		break;
	    }
	    Samples_per_model_axis = a + 1;
	    break;
	case 't'	:
	    if (read_units_double(&overlap_tolerance, bu_optarg, units_tab[0])) {
		bu_log("error in overlap tolerance distance \"%s\"\n", bu_optarg);
		bu_bomb("");
	    }
	    break;
	case 'v'	:
	    verbose = 1;
	    break;
	case 'V'	:
	    if (read_units_double(&volume_tolerance, bu_optarg, units_tab[1])) {
		bu_log("error in volume tolerance \"%s\"\n", bu_optarg);
		exit(-1);
	    }
	    break;
	case 'W'	:
	    if (read_units_double(&weight_tolerance, bu_optarg, units_tab[2])) {
		bu_log("error in weight tolerance \"%s\"\n", bu_optarg);
		exit(-1);
	    }
	    break;

	case 'U'	:
	    use_air = strtol(bu_optarg, (char **)NULL, 10);
	    if (errno == ERANGE || errno == EINVAL) {
		perror("-U argument");
		bu_bomb("");
	    }
	    break;
	case 'u'	:
	    {
		char *ptr = bu_optarg;
		const struct cvt_tab *cv;
		static const char *dim[3] = {"length", "volume", "weight"};
		char *units_name[3];

		for (i=0 ; i < 3 && ptr; i++) {
		    units_name[i] = strsep(&ptr, ",");

		    /* make sure the unit value is in the table */
		    if (*units_name[i] != '\0') {
			for (cv= &units_tab[i][0] ; cv->name[0] != '\0' ; cv++) {
			    if (!strcmp(cv->name, units_name[i])) {
				goto found_cv;
			    }
			}
			bu_log("Units \"%s\" not found in coversion table\n", units_name[i]);
			bu_bomb("");
		    found_cv:
			units[i] = cv;
		    }
		}

		bu_log("Units: ");
		for (i=0 ; i < 3 ; i++) {
		    bu_log(" %s: %s", dim[i], units[i]->name);
		}
		bu_log("\n");
		break;
	    }

	case '?'	:
	case 'h'	:
	default		:
	    fprintf(stderr, "Bad or help flag '%c' specified\n", c);
	    usage("");
	    break;
	}
    }

    return(bu_optind);
}


/*
 *	parse_densities_buffer
 *
 */
void
parse_densities_buffer(char *buf, unsigned long len)
{
    char *p, *q, *last;
    long idx;
    double density;

    buf[len] = '\0';
    last = &buf[len];

    p = buf;

    densities = bu_malloc(sizeof(struct density_entry)*128, "density entries");
    num_densities = 128;

    while (*p) {
	idx = strtol(p, &q, 10);
	if (q == (char *)NULL) {
	    bu_bomb("could not convert idx\n");
	}

	if (idx < 0) {
	    bu_log("bad density index (< 0) %d\n", idx);
	    bu_bomb("");
	}

	density = strtod(q, &p);
	if (q == p) {
	    bu_bomb("could not convert density\n");
	}

	if (density < 0.0) {
	    bu_log("bad density (< 0) %g\n", density);
	    bu_bomb("");
	}

	while ((*p == '\t') || (*p == ' ')) p++;

	if ((q = strchr(p, '\n'))) {
	    *q++ = '\0';
	} else {
	    q = last;
	}

	while (idx >= num_densities) {
	    densities = bu_realloc(densities, sizeof(struct density_entry)*num_densities*2,
				   "density entries");
	    num_densities *= 2;
	}

	densities[idx].magic = DENSITY_MAGIC;
	/* since BRL-CAD does computation in mm, but the table is
	 * in grams / (cm^3) we convert the table on input
	 */
	densities[idx].grams_per_cu_mm = density / 1000.0;
	densities[idx].name = bu_strdup(p);

	p = q;
    } while (p && p < last);

#ifdef PRINT_DENSITIES
    for (idx=0 ; idx < num_densities ; idx++) {
	if (densities[idx].magic == DENSITY_MAGIC) {
	    bu_log("%4d %6g %s\n",
		   idx,
		   densities[idx].density,
		   densities[idx].name
		   );
	}
    }
#endif
}
/*	g e t _ d e n s i t i e s _ f r o m _ f i l e
 *
 * Returns
 *	 0 on success
 *	!0 on failure
 */
int
get_densities_from_file(char *name)
{
    FILE *fp;
    struct stat sb;
    char *buf;

    if ((fp=fopen(name, "r")) == (FILE *)NULL) {
	perror(name);
	return 1;
    }

    if (fstat(fileno(fp), &sb)) {
	perror(name);
	return 1;
    }

    buf = bu_malloc(sb.st_size, "density buffer");
    fread(buf, sb.st_size, 1, fp);
    parse_densities_buffer(buf, (unsigned long)sb.st_size);
    bu_free(buf, "density buffer");
    fclose(fp);
    return 0;
}

/*
 *	g e t _ d e n s i t i e s _ f r o m _ d a t a b a s e
 *
 * Returns
 *	 0 on success
 *	!0 on failure
 */
int
get_densities_from_database(struct rt_i *rtip)
{
    struct directory *dp;
    struct rt_db_internal intern;
    struct rt_binunif_internal *bu;

    dp = db_lookup(rtip->rti_dbip, "_DENSITIES", LOOKUP_QUIET);
    if (dp == (struct directory *)NULL) {
	bu_log("No \"_DENSITIES\" density table object in database\n");
	return -1;
    }

    if (rt_db_get_internal(&intern, dp, rtip->rti_dbip, NULL, &rt_uniresource) < 0) {
	bu_log("could not import %s\n", dp->d_namep);
	return 1;
    }

    if ((intern.idb_major_type & DB5_MAJORTYPE_BINARY_MASK) == 0) {
	return 1;
    }


    bu = (struct rt_binunif_internal *)intern.idb_ptr;

    RT_CHECK_BINUNIF(bu);

    parse_densities_buffer(bu->u.int8, bu->count);
    return 0;
}

/*
 *	add_unique_pair
 *
 *	This routine must be prepared to run in parallel
 *
 *
 *
 */
struct region_pair *
add_unique_pair(struct region_pair *list, /* list to add into */
		struct region *r1,	/* first region involved */
		struct region *r2,	/* second region involved */
		double dist,		/* distance/thickness metric value */
		point_t pt)		/* location where this takes place */
{
    struct region_pair *rp, *rpair;

    /* look for it in our list */
    bu_semaphore_acquire( SEM_LIST );
    for( BU_LIST_FOR(rp, region_pair, &list->l) ) {

	if ( (r1 == rp->r.r1 && r2 == rp->r2) || (r1 == rp->r2 && r2 == rp->r.r1) ) {
	    /* we already have an entry for this region pair,
	     * we increase the counter, check the depth and
	     * update thickness maximum and entry point if need be
	     * and return.
	     */
	    rp->count++;

	    if (dist > rp->max_dist) {
		rp->max_dist = dist;
		VMOVE(rp->coord, pt);
	    }
	    rpair = rp;
	    goto found;
	}
    }
    /* didn't find it in the list.  Add it */
    rpair = bu_malloc(sizeof(struct region_pair), "region_pair");
    rpair->r.r1 = r1;
    rpair->r2 = r2;
    rpair->count = 1;
    rpair->max_dist = dist;
    VMOVE(rpair->coord, pt);
    list->max_dist ++; /* really a count */

    /* insert in the list at the "nice" place */
    for( BU_LIST_FOR(rp, region_pair, &list->l) ) {
	if (strcmp(rp->r.r1->reg_name, r1->reg_name) <= 0 )
	    break;
    }
    BU_LIST_INSERT(&rp->l, &rpair->l);
 found:
    bu_semaphore_release( SEM_LIST );
    return rpair;
}



/*
 *			O V E R L A P
 *
 *  Write end points of partition to the standard output.
 *  If this routine return !0, this partition will be dropped
 *  from the boolean evaluation.
 *
 *	Returns:
 *	 0	to eliminate partition with overlap entirely
 *	 1	to retain partition in output list, claimed by reg1
 *	 2	to retain partition in output list, claimed by reg2
 *
 *	This routine must be prepared to run in parallel
 *
 */
int
overlap(struct application *ap,
	struct partition *pp,
	struct region *reg1,
	struct region *reg2)
{

    register struct xray	*rp = &ap->a_ray;
    register struct hit	*ihitp = pp->pt_inhit;
    register struct hit	*ohitp = pp->pt_outhit;
    point_t ihit;
    point_t ohit;
    double depth;

    /* if one of the regions is air, let it loose */
    if (reg1->reg_aircode && ! reg2->reg_aircode)
	return 2;
    if (reg2->reg_aircode && ! reg1->reg_aircode)
	return 1;

    depth = ohitp->hit_dist - ihitp->hit_dist;

    if( depth < overlap_tolerance ) {
	/* too small to matter, pick one or none */
	return(1);
    }

    VJOIN1( ihit, rp->r_pt, ihitp->hit_dist, rp->r_dir );
    VJOIN1( ohit, rp->r_pt, ohitp->hit_dist, rp->r_dir );

    if (plot_overlaps) {
	bu_semaphore_acquire(BU_SEM_SYSCALL);
	pl_color(plot_overlaps, V3ARGS(overlap_color));
	pdv_3line(plot_overlaps, ihit, ohit);
	bu_semaphore_release(BU_SEM_SYSCALL);
    }

    if (analysis_flags & ANALYSIS_OVERLAPS) {
#if 0
	struct region_pair *rp =
#endif
	    add_unique_pair(&overlapList, reg1, reg2, depth, ihit);

	if (plot_overlaps) {
	    bu_semaphore_acquire(BU_SEM_SYSCALL);
	    pl_color(plot_overlaps, V3ARGS(overlap_color));
	    pdv_3line(plot_overlaps, ihit, ohit);
#if 0
	    pdv_3line(plot_overlaps, ihit, rp->coord);
	    pdv_3line(plot_overlaps, ihit, rp->coord);
#endif
	    bu_semaphore_release(BU_SEM_SYSCALL);
	}
    } else {
	bu_log("overlap %s %s\n", reg1->reg_name, reg2->reg_name);
    }

    /* XXX We should somehow flag the volume/weight calculations as invalid */

    /* since we have no basis to pick one over the other, just pick */
    return(1);	/* No further consideration to this partition */
}

/*
 *	logoverlap
 *
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
    return;
}

void exposed_air(struct partition *pp,
		 point_t last_out_point,
		 point_t pt,
		 point_t opt)
{
    /* this shouldn't be air */

    add_unique_pair(&exposedAirList,
		    pp->pt_regionp,
		    (struct region *)NULL,
		    pp->pt_outhit->hit_dist - pp->pt_inhit->hit_dist, /* thickness */
		    last_out_point); /* location */

    if (plot_expair) {
	bu_semaphore_acquire(BU_SEM_SYSCALL);
	pl_color(plot_expair, V3ARGS(expAir_color));
	pdv_3line(plot_expair, pt, opt);
	bu_semaphore_release(BU_SEM_SYSCALL);
    }


}


/*
 *  rt_shootray() was told to call this on a hit.  It passes the
 *  application structure which describes the state of the world
 *  (see raytrace.h), and a circular linked list of partitions,
 *  each one describing one in and out segment of one region.
 *
 *	This routine must be prepared to run in parallel
 *
 */
int
hit(register struct application *ap, struct partition *PartHeadp, struct seg *segs)
{
    /* see raytrace.h for all of these guys */
    register struct partition *pp;
    point_t		pt, opt, last_out_point;
    int			last_air = 0; /* what was the aircode of the last item */
    int			air_first = 1; /* are we in an air before a solid */
    double	dist;	/* the thickness of the partition */
    double	gap_dist;
    double	last_out_dist = -1.0;
    double	val;
    struct cstate *state = ap->A_STATE;

    if (PartHeadp->pt_forw == PartHeadp) return 1;



    /* examine each partition until we get back to the head */
    for( pp=PartHeadp->pt_forw; pp != PartHeadp; pp = pp->pt_forw )  {
	struct density_entry *de;

	/* inhit info */
	dist = pp->pt_outhit->hit_dist - pp->pt_inhit->hit_dist;
	VJOIN1(pt, ap->a_ray.r_pt, pp->pt_inhit->hit_dist, ap->a_ray.r_dir);
	VJOIN1(opt, ap->a_ray.r_pt, pp->pt_outhit->hit_dist, ap->a_ray.r_dir);

	DLOG("%s %g->%g\n",
			  pp->pt_regionp->reg_name,
			  pp->pt_inhit->hit_dist,
			  pp->pt_outhit->hit_dist);

	/* checking for air sticking out of the model .
	 * This is done here because there may be any number of air
	 * regions sticking out of the model along the ray.
	 */
	if (analysis_flags & ANALYSIS_EXP_AIR) {

	    gap_dist = (pp->pt_inhit->hit_dist - last_out_dist);

	    /* if air is first on the ray, or we're moving from void/gap to air
	     * then this is exposed air
	     */
	    if (pp->pt_regionp->reg_aircode &&
		(air_first || gap_dist > overlap_tolerance)) {
		exposed_air(pp, last_out_point, pt, opt);
	    } else {
		air_first = 0;
	    }
	}

	/* looking for voids in the model */
	if (analysis_flags & ANALYSIS_GAP) {
	    if (pp->pt_back != PartHeadp) {
		/* if this entry point is further than the previous exit point
		 * then we have a void
		 */
		gap_dist = pp->pt_inhit->hit_dist - last_out_dist;

		if (gap_dist > overlap_tolerance) {

		    /* like overlaps, we only want to report unique pairs */
		    add_unique_pair(&gapList,
				    pp->pt_regionp,
				    pp->pt_back->pt_regionp,
				    gap_dist,
				    pt);

		    /* like overlaps, let's plot */
		    if (plot_gaps) {
			vect_t gapEnd;
			VJOIN1(gapEnd, pt, -gap_dist, ap->a_ray.r_dir);

			bu_semaphore_acquire(BU_SEM_SYSCALL);
			pl_color(plot_gaps, V3ARGS(gap_color));
			pdv_3line(plot_gaps, pt, gapEnd);
			bu_semaphore_release(BU_SEM_SYSCALL);
		    }
		}
	    }
	}

	/* computing the weight of the objects */
	if (analysis_flags & ANALYSIS_WEIGHT) {
	    DLOG("Hit %s doing weight\n", pp->pt_regionp->reg_name);
	    /* make sure mater index is within range of densities */
	    if (pp->pt_regionp->reg_gmater >= num_densities) {
		bu_log("density index %d on region %s is outside of range of table [1..%d]\n",
		       pp->pt_regionp->reg_gmater,
		       pp->pt_regionp->reg_name,
		       num_densities);
		bu_log("Set GIFTmater on region or add entry to density table\n");
		bu_bomb(""); /* XXX this should do something else */
	    } else {

		/* make sure the density index has been set */
		de = &densities[pp->pt_regionp->reg_gmater];
		if (de->magic == DENSITY_MAGIC) {
		    struct per_region_data *prd;

		    /* factor in the density of this object
		     * weight computation, factoring in the LOS
		     * percentage material of the object
		     */
		    int los = pp->pt_regionp->reg_los;

		    if (los < 1) {
			bu_log("bad LOS (%d) on %s\n", los, pp->pt_regionp->reg_name );
		    }

		    /* accumulate the total weight values */
		    val = de->grams_per_cu_mm * dist * (pp->pt_regionp->reg_los * 0.01);
		    ap->A_LENDEN += val;

		    prd = ((struct per_region_data *)pp->pt_regionp->reg_udata);
		    /* accumulate the per-region per-view weight values */
		    bu_semaphore_acquire(SEM_STATS);
		    prd->r_lenDensity[state->i_axis] += val;

		    /* accumulate the per-object per-view weight values */
		    prd->optr->o_lenDensity[state->i_axis] += val;
		    bu_semaphore_release(SEM_STATS);

		} else {
		    bu_log("density index %d from region %s is not set.\n",
			   pp->pt_regionp->reg_gmater, pp->pt_regionp->reg_name);
		    bu_log("Add entry to density table\n");
		    bu_bomb("");
		}
	    }
	}

	/* compute the volume of the object */
	if (analysis_flags & ANALYSIS_VOLUME) {
	    struct per_region_data *prd = ((struct per_region_data *)pp->pt_regionp->reg_udata);
	    ap->A_LEN += dist; /* add to total volume */
	    {
		bu_semaphore_acquire(SEM_STATS);

		/* add to region volume */
		prd->r_len[state->curr_view] += dist;

		/* add to object volume */
		prd->optr->o_len[state->curr_view] += dist;

		bu_semaphore_release(SEM_STATS);
	    }

	    DLOG("\t\tvol hit %s oDist:%g objVol:%g %s\n",
		     pp->pt_regionp->reg_name,
		     dist,
		     prd->optr->o_len[state->curr_view],
		     prd->optr->o_name);

	    if (plot_volume) {
		point_t opt;

		VJOIN1(opt, ap->a_ray.r_pt, pp->pt_outhit->hit_dist, ap->a_ray.r_dir);

		bu_semaphore_acquire(BU_SEM_SYSCALL);
		if (ap->a_user & 1) {
		    pl_color(plot_volume, V3ARGS(gap_color));
		} else {
		    pl_color(plot_volume, V3ARGS(adjAir_color));
		}

		pdv_3line(plot_volume, pt, opt);
		bu_semaphore_release(BU_SEM_SYSCALL);
	    }
	}


	/* look for two adjacent air regions */
	if (analysis_flags & ANALYSIS_ADJ_AIR) {
	    if (last_air && pp->pt_regionp->reg_aircode &&
		pp->pt_regionp->reg_aircode != last_air) {

		double d = pp->pt_outhit->hit_dist - pp->pt_inhit->hit_dist;
		point_t aapt;

		add_unique_pair(&adjAirList, pp->pt_back->pt_regionp, pp->pt_regionp, 0.0, pt);


		d *= 0.25;
		VJOIN1(aapt, pt, d, ap->a_ray.r_dir);

		bu_semaphore_acquire(BU_SEM_SYSCALL);
		pl_color(plot_adjair, V3ARGS(adjAir_color));
		pdv_3line(plot_adjair, pt, aapt);
		bu_semaphore_release(BU_SEM_SYSCALL);

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

	exposed_air(pp, last_out_point, pt, opt);
    }


    /*
     * This value is returned by rt_shootray
     * a hit usually returns 1, miss 0.
     */
    return(1);
}

/*
 * rt_shootray() was told to call this on a miss.
 *
 *	This routine must be prepared to run in parallel
 *
 */
int
miss(register struct application *ap)
{
#if 0
    bu_log("missed\n");
#endif
    return(0);
}

/*
 *	get_next_row
 *
 *
 *	This routine must be prepared to run in parallel
 *
 */
int
get_next_row(struct cstate *state)
{
    int v;
    /* look for more work */
    bu_semaphore_acquire(SEM_WORK);

    if (state->v < state->steps[state->v_axis])
	v = state->v++;	/* get a row to work on */
    else
	v = 0; /* signal end of work */

    bu_semaphore_release(SEM_WORK);

    return v;
}
/*
 *	plane_worker
 *
 *
 *	This routine must be prepared to run in parallel
 *
 */
void
plane_worker (int cpu, genptr_t ptr)
{
    struct application ap;
    int u, v;
    double v_coord;
    struct cstate *state = (struct cstate *)ptr;

    RT_APPLICATION_INIT(&ap);
    ap.a_rt_i = (struct rt_i *)state->rtip;	/* application uses this instance */
    ap.a_hit = hit;			/* where to go on a hit */
    ap.a_miss = miss;		/* where to go on a miss */
    ap.a_logoverlap = logoverlap;
    ap.a_overlap = overlap;
    ap.a_resource = &resource[cpu];
    ap.A_LENDEN = 0.0; /* really the cumulative length*density  for weight computation*/
    ap.A_LEN = 0.0; /* really the cumulative length for volume computation */

    /* gross hack */
    ap.a_ray.r_dir[state->u_axis] = ap.a_ray.r_dir[state->v_axis] = 0.0;
    ap.a_ray.r_dir[state->i_axis] = 1.0;

    ap.A_STATE = ptr; /* really copying the state ptr to the a_uptr */

    u = -1;

    while ((v = get_next_row(state))) {

	v_coord = v * gridSpacing;
	DLOG("  v = %d v_coord=%g\n", v, v_coord);

	if ( (v&1) || state->first) {
	    /* shoot all the rays in this row.
	     * This is either the first time a view has been computed
	     * or it is an odd numbered row in a grid refinement
	     */
	    for (u=1 ; u < state->steps[state->u_axis]; u++) {
		ap.a_ray.r_pt[state->u_axis] = ap.a_rt_i->mdl_min[state->u_axis] + u*gridSpacing;
		ap.a_ray.r_pt[state->v_axis] = ap.a_rt_i->mdl_min[state->v_axis] + v*gridSpacing;
		ap.a_ray.r_pt[state->i_axis] = ap.a_rt_i->mdl_min[state->i_axis];

		DLOG("%5g %5g %5g -> %g %g %g\n", V3ARGS(ap.a_ray.r_pt), V3ARGS(ap.a_ray.r_dir));
		ap.a_user = v;
		(void)rt_shootray( &ap );

		bu_semaphore_acquire(SEM_STATS);
		state->shots[state->curr_view]++;
		bu_semaphore_release(SEM_STATS);
	    }
	} else {
	    /* shoot only the rays we need to on this row.
	     * Some of them have been computed in a previous iteration.
	     */
	    for (u=1 ; u < state->steps[state->u_axis]; u+=2) {
		ap.a_ray.r_pt[state->u_axis] = ap.a_rt_i->mdl_min[state->u_axis] + u*gridSpacing;
		ap.a_ray.r_pt[state->v_axis] = ap.a_rt_i->mdl_min[state->v_axis] + v*gridSpacing;
		ap.a_ray.r_pt[state->i_axis] = ap.a_rt_i->mdl_min[state->i_axis];

		DLOG("%5g %5g %5g -> %g %g %g\n", V3ARGS(ap.a_ray.r_pt), V3ARGS(ap.a_ray.r_dir));
		ap.a_user = v;
		(void)rt_shootray( &ap );

		bu_semaphore_acquire(SEM_STATS);
		state->shots[state->curr_view]++;
		bu_semaphore_release(SEM_STATS);

		if (debug)
		    if (u+1 <  state->steps[state->u_axis])
			bu_log("  ---\n");
	    }
	}
    }

    if (u == -1) {
	DLOG("didn't shoot any rays\n");
    }

    /* There's nothing else left to work on in this view.
     * It's time to add the values we have accumulated
     * to the totals for the view and return.  When all
     * threads have been through here, we'll have returned
     * to serial computation.
     */
    bu_semaphore_acquire(SEM_STATS);
    state->m_lenDensity[state->curr_view] += ap.A_LENDEN; /* add our length*density value */
    state->m_len[state->curr_view] += ap.A_LEN; /* add our volume value */
    bu_semaphore_release(SEM_STATS);
}

/*
 *
 */
int
find_cmd_line_obj(struct per_obj_data *obj_rpt, const char *name)
{
    int i;
    char *str = strdup(name);
    char *p;

    if ((p=strchr(str, '/'))) {
	*p = '\0';
    }

    for (i=0 ; i < num_objects ; i++) {
	if (!strcmp(obj_rpt[i].o_name, str)) {
	    bu_free(str, "");
	    return i;
	}
    }
    bu_log("%s Didn't find object named \"%s\" in %d entries\n", BU_FLSTR, name, num_objects);
    bu_bomb("");
    /* NOTREACHED */
    return -1; /* stupid compiler */
}

/*
 * allocate_per_region_data
 *
 *	Allocate data structures for tracking statistics on a per-view basis
 *	for each of the view, object and region levels.
 */
void
allocate_per_region_data(struct cstate *state, int start, int ac, char *av[])
{
    struct region *regp;
    struct rt_i *rtip = state->rtip;
    int i;
    int m;

    state->m_lenDensity = bu_calloc(sizeof(double), num_views, "densityLen");
    state->m_len = bu_calloc(sizeof(double), num_views, "volume");
    state->m_volume = bu_calloc(sizeof(double), num_views, "volume");
    state->m_weight = bu_calloc(sizeof(double), num_views, "volume");
    state->shots = bu_calloc(sizeof(unsigned long), num_views, "volume");

    /* build data structures for the list of
     * objects the user specified on the command line
     */
    obj_tbl = bu_calloc(sizeof(struct per_obj_data), num_objects, "report tables");
    for (i=0 ; i < num_objects ; i++) {
	obj_tbl[i].o_name = av[start+i];
	obj_tbl[i].o_len = bu_calloc(sizeof(double), num_views, "o_len");
	obj_tbl[i].o_lenDensity = bu_calloc(sizeof(double), num_views, "o_lenDensity");
	obj_tbl[i].o_volume = bu_calloc(sizeof(double), num_views, "o_volume");
	obj_tbl[i].o_weight = bu_calloc(sizeof(double), num_views, "o_weight");
    }

    /* build objects for each region */
    reg_tbl = bu_calloc(rtip->nregions, sizeof(struct per_region_data), "per_region_data");


    for( i=0 , BU_LIST_FOR( regp, region, &(rtip->HeadRegion) ) , i++)  {
	regp->reg_udata = &reg_tbl[i];

	reg_tbl[i].r_lenDensity = bu_calloc(sizeof(double), num_views, "r_lenDensity");
	reg_tbl[i].r_len = bu_calloc(sizeof(double), num_views, "r_len");
	reg_tbl[i].r_volume = bu_calloc(sizeof(double), num_views, "len");
	reg_tbl[i].r_weight = bu_calloc(sizeof(double), num_views, "len");

	m = strlen(regp->reg_name);
	if (m > max_region_name_len) max_region_name_len = m;
	reg_tbl[i].optr = &obj_tbl[ find_cmd_line_obj(obj_tbl, &regp->reg_name[1]) ];

    }
}




/*
 *	list_report
 *
 */
void
list_report(struct region_pair *list)
{
    struct region_pair *rp;

    if (BU_LIST_IS_EMPTY(&list->l)) {
	bu_log("No %s\n", (char *)list->r.name);
	return;
    }

    bu_log("list %s:\n", (char *)list->r.name);

    for (BU_LIST_FOR(rp, region_pair, &(list->l))) {
	if (rp->r2) {
	    bu_log("%s %s count:%lu dist:%g%s @ (%g %g %g)\n",
	       rp->r.r1->reg_name, rp->r2->reg_name, rp->count,
	       rp->max_dist / units[LINE]->val, units[LINE]->name, V3ARGS(rp->coord));
	} else {
	    bu_log("%s count:%lu dist:%g%s @ (%g %g %g)\n",
	       rp->r.r1->reg_name, rp->count,
	       rp->max_dist / units[LINE]->val, units[LINE]->name, V3ARGS(rp->coord));
	}
    }
}






/*
 *	options_prep
 *
 *	Do some computations prior to raytracing based upon
 *	options the user has specified
 *
 *	Returns:
 *		0	continue, ready to go
 *		!0	error encountered, terminate processing
 */
int
options_prep(struct rt_i *rtip, vect_t span)
{
    double newGridSpacing = gridSpacing;
    int axis;

    /* figure out where the density values are comming from and get them */
    if (analysis_flags & ANALYSIS_WEIGHT) {
	if (densityFileName) {
	    DLOG("density from file\n");
	    if (get_densities_from_file(densityFileName)) {
		return -1;
	    }
	} else {
	    DLOG("density from db\n");
	    if (get_densities_from_database(rtip)) {
		return -1;
	    }
	}
    }
    /* refine the grid spacing if the user has set a
     * lower bound on the number of rays per model axis
     */
    for (axis=0 ; axis < 3 ; axis++) {
	if (span[axis] < newGridSpacing*Samples_per_model_axis) {
	    /* along this axis, the gridSpacing is
	     * larger than the model span.  We need to refine.
	     */
	    newGridSpacing = span[axis] / Samples_per_model_axis;
	}
    }

    if (newGridSpacing != gridSpacing) {
	bu_log("Grid spacing %g %s is does not allow %g samples per axis\n",
	       gridSpacing / units[LINE]->val, units[LINE]->name, Samples_per_model_axis - 1);

	bu_log("Adjusted to %g %s to get %g samples per model axis\n",
	       newGridSpacing / units[LINE]->val, units[LINE]->name, Samples_per_model_axis);

	gridSpacing = newGridSpacing;
    }

    /* if the vol/weight tolerances are not set, pick something */
    if (analysis_flags & ANALYSIS_VOLUME) {
	char *name = "volume.pl";
	if (volume_tolerance == -1.0) {
	    volume_tolerance = span[X] * span[Y] * span[Z] * 0.001;
	    bu_log("setting volume tolerance to %g %s\n",
		   volume_tolerance / units[VOL]->val, units[VOL]->name);
	} else {
	    bu_log("volume tolerance   %g\n", volume_tolerance);
	}
	if ( plot_files ) {
	    if ( (plot_volume=fopen(name, "w")) == (FILE *)NULL) {
		bu_log("cannot open plot file %s\n", name);
		bu_bomb("");
	    }
	}
    }
    if (analysis_flags & ANALYSIS_WEIGHT) {
	if (weight_tolerance == -1.0) {
	    double max_den = 0.0;
	    int i;
	    for (i=0 ; i < num_densities ; i++) {
		if (densities[i].grams_per_cu_mm > max_den)
		    max_den = densities[i].grams_per_cu_mm;
	    }
	    weight_tolerance = span[X] * span[Y] * span[Z] * 0.1 * max_den;
	    bu_log("setting weight tolerance to %g %s\n",
		   weight_tolerance / units[WGT]->val,
		   units[WGT]->name);
	} else {
	    bu_log("weight tolerance   %g\n", weight_tolerance);
	}
    }
    if (analysis_flags & ANALYSIS_GAP) {
	char *name = "gaps.pl";
	if ( plot_files ) {
	    if ( (plot_gaps=fopen(name, "w")) == (FILE *)NULL) {
		bu_log("cannot open plot file %s\n", name);
		bu_bomb("");
	    }
	}
    }
    if (analysis_flags & ANALYSIS_OVERLAPS) {
	if (overlap_tolerance != 0.0) {
	    bu_log("overlap tolerance to %g\n", overlap_tolerance);
	}
	if ( plot_files ) {
	    char *name = "overlaps.pl";
	    if ((plot_overlaps=fopen(name, "w")) == (FILE *)NULL) {
		bu_log("cannot open plot file %s\n", name);
		bu_bomb("");
	    }
	}
    }

    if (print_per_region_stats) {
	if ( (analysis_flags & (ANALYSIS_VOLUME|ANALYSIS_WEIGHT)) == 0) {
	    bu_log("Note: -r option ignored: neither volume or weight options requested\n");
	}
    }


    if ( analysis_flags & ANALYSIS_ADJ_AIR) {
	if (plot_files) {
	    char *name = "adj_air.pl";
	    if ( (plot_adjair=fopen(name, "w")) == (FILE *)NULL) {
		bu_log("cannot open plot file %s\n", name);
		bu_bomb("");
	    }
	}
    }


    if ( analysis_flags & ANALYSIS_EXP_AIR) {
	if (plot_files) {
	    char *name = "exp_air.pl";
	    if ( (plot_expair=fopen(name, "w")) == (FILE *)NULL) {
		bu_log("cannot open plot file %s\n", name);
		bu_bomb("");
	    }
	}
    }

    if ( (analysis_flags & (ANALYSIS_ADJ_AIR|ANALYSIS_EXP_AIR)) && ! use_air ) {
	bu_log("Error:  Air regions discarded but air analysis requested!\n");
	bu_bomb("Set use_air non-zero or eliminate air analysis\n");
    }

    return 0;
}

/*
 *	view_reports
 *
 */
void
view_reports(struct cstate *state)
{
    if (analysis_flags & ANALYSIS_VOLUME) {
	int obj;
	int view;

	/* for each object, compute the volume for all views */
	for (obj=0 ; obj < num_objects ; obj++) {
	    double val;
	    /* compute volume of object for given view */
	    view = state->curr_view;

	    /* compute the per-view volume of this object */

	    if (state->shots[view] > 0) {
		val = obj_tbl[obj].o_volume[view] =
		    obj_tbl[obj].o_len[view] * (state->area[view] / state->shots[view]);

		if (verbose)
		    bu_log("\t%s volume %g %s\n",
		       obj_tbl[obj].o_name,
		       val / units[VOL]->val,
		       units[VOL]->name);
	    }
	}
    }
    if (analysis_flags & ANALYSIS_WEIGHT) {
	int obj;
	int view = state->curr_view;

	for (obj=0 ; obj < num_objects ; obj++) {
	    double grams_per_cu_mm = obj_tbl[obj].o_lenDensity[view] *
		(state->area[view] / state->shots[view]);


		if (verbose)
		    bu_log("\t%s %g %s\n",
			   obj_tbl[obj].o_name,
			   grams_per_cu_mm / units[WGT]->val,
			   units[WGT]->name);
	}
    }
}

/*	w e i g h t _ v o l u m e _ t e r m i n a t e
 *
 * These checks are unique because they must both be completed.
 * Early termination before they are done is not an option.
 * The results computed here are used later
 *
 * Returns:
 *	0 terminate
 *	1 continue processing
 */
static int
weight_volume_terminate(struct cstate *state)
{
    /* Both weight and volume computations rely on this routine to compute values
     * that are printed in summaries.  Hence, both checks must always be done before
     * this routine exits.  So we store the status (can we terminate processing?)
     * in this variable and act on it once both volume and weight computations are done
     */
    int can_terminate = 1;

    double low, hi, val, delta;

    if (analysis_flags & ANALYSIS_WEIGHT) {
	/* for each object, compute the weight for all views */
	int obj;

	for (obj=0 ; obj < num_objects ; obj++) {
	    int view = 0;
	    if (verbose)
		bu_log("object %d\n", obj);
	    /* compute weight of object for given view */
	    val = obj_tbl[obj].o_weight[view] =
		obj_tbl[obj].o_lenDensity[view] * (state->area[view] / state->shots[view]);

	    low = hi = 0.0;

	    /* compute the per-view weight of this object */
	    for (view=1 ; view < num_views ; view++) {
		obj_tbl[obj].o_weight[view] =
		    obj_tbl[obj].o_lenDensity[view] *
		    (state->area[view] / state->shots[view]);

		delta = val - obj_tbl[obj].o_weight[view];
		if (delta < low) low = delta;
		if (delta > hi) hi = delta;
	    }
	    delta = hi - low;

	    if (verbose)
		bu_log("\t%s weight %g %s +%g -%g\n",
		       obj_tbl[obj].o_name,
		       val / units[WGT]->val,
		       units[WGT]->name,
		       fabs(hi / units[WGT]->val),
		       fabs(low / units[WGT]->val));

	    if (delta > weight_tolerance) {
		/* this object differs too much in each view, so we need to refine the grid */
		/* signal that we cannot terminate */
		can_terminate = 0;
		if (verbose)
		    bu_log("\t%s differs too much in weight per view.\n",
			   obj_tbl[obj].o_name);
	    }
	}
	if (can_terminate) {
	    if (verbose)
		bu_log("all objects within tolerance on weight calculation\n");
	}
    }

    if (analysis_flags & ANALYSIS_VOLUME) {
	/* find the range of values for object volumes */
	int obj;

	/* for each object, compute the volume for all views */
	for (obj=0 ; obj < num_objects ; obj++) {

	    /* compute volume of object for given view */
	    int view = 0;
	    val = obj_tbl[obj].o_volume[view] =
		obj_tbl[obj].o_len[view] * (state->area[view] / state->shots[view]);

	    low = hi = 0.0;
	    /* compute the per-view volume of this object */
	    for (view=1 ; view < num_views ; view++) {
		obj_tbl[obj].o_volume[view] =
		    obj_tbl[obj].o_len[view] * (state->area[view] / state->shots[view]);

		delta = val - obj_tbl[obj].o_volume[view];
		if (delta < low) low = delta;
		if (delta > hi) hi = delta;
	    }
	    delta = hi - low;

	    if (verbose)
		bu_log("\t%s volume %g %s +(%g) -(%g)\n",
		       obj_tbl[obj].o_name,
		       val / units[VOL]->val, units[VOL]->name,
		       hi / units[VOL]->val,
		       fabs(low / units[VOL]->val));

	    if (delta > volume_tolerance) {
		/* this object differs too much in each view, so we need to refine the grid */
		can_terminate = 0;
		if (verbose)
		    bu_log("\tvolume tol not met on %s.  Refine grid\n",
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


/*
 *	t e r m i n a t e _ c h e c k
 *
 *	Check to see if we are done processing due to
 *	some user specified limit being achieved.
 *
 *	Returns:
 *	0	Terminate
 *	1	Continue processing
 */
int
terminate_check(struct cstate *state)
{
    int wv_status;

    DLOG("terminate_check\n");
    RT_CK_RTI(state->rtip);

    if (plot_overlaps) fflush(plot_overlaps);
    if (plot_weight) fflush(plot_weight);
    if (plot_volume) fflush(plot_volume);
    if (plot_adjair) fflush(plot_adjair);
    if (plot_gaps) fflush(plot_gaps);
    if (plot_expair) fflush(plot_expair);


    /* this computation is done first, because there are
     * side effects that must be obtained whether we terminate or not
     */
    wv_status = weight_volume_terminate(state);


    /* if we've reached the grid limit, we're done, no matter what */
    if (gridSpacing < gridSpacingLimit) {
	if (verbose)
	    bu_log("grid spacing refined to %g (below lower limit %g)\n",
		   gridSpacing, gridSpacingLimit);
	return 0;
    }

    /* if we are doing one of the "Error" checking operations:
     * Overlap, gap, adj_air, exp_air, then we ALWAYS go to the 
     * grid spacing limit and we ALWAYS terminate on first 
     * error/list-entry
     */
    if ( (analysis_flags & ANALYSIS_OVERLAPS)) {
	if (BU_LIST_NON_EMPTY(&overlapList.l)) {
	    /* since we've found an overlap, we can quit */
	    return 0;
	} else {
	    bu_log("overlaps list is empty\n");
	}
    }
    if ( (analysis_flags & ANALYSIS_GAP)) {
	if (BU_LIST_NON_EMPTY(&gapList.l)) {
	    /* since we've found a gap, we can quit */
	    return 0;
	}
    }
    if ( (analysis_flags & ANALYSIS_ADJ_AIR)) {
	if (BU_LIST_NON_EMPTY(&adjAirList.l)) {
	    /* since we've found adjacent air, we can quit */
	    return 0;
	}
    }
    if ( (analysis_flags & ANALYSIS_EXP_AIR)) {
	if (BU_LIST_NON_EMPTY(&exposedAirList.l)) {
	    /* since we've found exposed air, we can quit */
	    return 0;
	}
    }


    if (analysis_flags & (ANALYSIS_WEIGHT|ANALYSIS_VOLUME)) {
	/* volume/weight checks only get to terminate processing if there
	 * are no "error" check computations being done
	 */
	if (analysis_flags & (ANALYSIS_GAP|ANALYSIS_ADJ_AIR|ANALYSIS_OVERLAPS|ANALYSIS_EXP_AIR)) {
	    if (verbose)
		bu_log("Volume/Weight tolerance met.  Cannot terminate calculation due to error computations\n");
	} else {
	    struct region *regp;
	    int all_hit = 1;
	    unsigned long hits;

	    if (require_num_hits > 0) {
		/* check to make sure every region was hit at least once */
		for( BU_LIST_FOR( regp, region, &(state->rtip->HeadRegion) ) )  {
		    RT_CK_REGION(regp);

		    hits = ((struct per_region_data *)regp->reg_udata)->hits;
		    if ( hits < require_num_hits) {
			all_hit = 0;
			if (verbose) {
			    if (hits == 0) {
				bu_log("%s was not hit\n", regp->reg_name);
			    } else {
				bu_log("%s hit only %u times (< %u)\n",
				       regp->reg_name, hits, require_num_hits);
			    }
			}
		    }
		}

		if (all_hit && wv_status == 0) {
		    if (verbose)
			bu_log("%s: Volume/Weight tolerance met. Terminate\n", BU_FLSTR);
		    return 0; /* terminate */
		}
	    } else {
		if (wv_status == 0) {
		    if (verbose)
			bu_log("%s: Volume/Weight tolerance met. Terminate\n", BU_FLSTR);
		    return 0; /* terminate */
		}
	    }
	}
    }
    return 1;
}


/*
 *	summary_reports
 *
 */
void
summary_reports(struct cstate *state, int start, int ac, char *av[])
{
    int view;
    int obj;
    double avg;
    struct region *regp;

    if (multiple_analyses)
	bu_log("Summaries:\n");
    else
	bu_log("Summary:\n");

    if (analysis_flags & ANALYSIS_WEIGHT) {
	bu_log("Weight:\n");
	for (obj=0 ; obj < num_objects ; obj++) {
	    avg = 0.0;

	    for (view=0 ; view < num_views ; view++) {
		/* computed in terminate_check() */
		avg += obj_tbl[obj].o_weight[view];
	    }
	    avg /= num_views;
	    bu_log("\t%*s %g %s\n", -max_region_name_len, obj_tbl[obj].o_name,
		   avg / units[WGT]->val, units[WGT]->name);
	}


	if (print_per_region_stats) {
	    double *wv;
	    bu_log("\tregions:\n");
	    for( BU_LIST_FOR( regp, region, &(state->rtip->HeadRegion) ))  {
		double low = HUGE;
		double hi = -HUGE;

		avg = 0.0;

		for (view=0 ; view < num_views ; view++) {
		    wv = &((struct per_region_data *)regp->reg_udata)->r_weight[view];

		    *wv = ((struct per_region_data *)regp->reg_udata)->r_lenDensity[view] *
			(state->area[view]/state->shots[view]);

		    *wv /= units[WGT]->val;

		    avg += *wv;

		    if (*wv < low) low = *wv;
		    if (*wv > hi) hi = *wv;
		}

		avg /= num_views;
		bu_log("\t%s %g %s +(%g) -(%g)\n",
		       regp->reg_name,
		       avg,
		       units[WGT]->name,
		       hi - avg,
		       avg - low);
	    }
	}


	/* print grand totals */
	avg = 0.0;
	for (view=0 ; view < num_views ; view++) {
	    avg += state->m_weight[view] =
		state->m_lenDensity[view] *
		( state->area[view] / state->shots[view]);
	}

	avg /= num_views;
	bu_log("  Average total weight: %g %s\n", avg / units[WGT]->val, units[WGT]->name);
    }



    if (analysis_flags & ANALYSIS_VOLUME) {
	bu_log("Volume:\n");

	/* print per-object */
	for (obj=0 ; obj < num_objects ; obj++) {
	    avg = 0.0;

	    for (view=0 ; view < num_views ; view++)
		avg += obj_tbl[obj].o_volume[view];

	    avg /= num_views;
	    bu_log("\t%*s %g %s\n", -max_region_name_len, obj_tbl[obj].o_name,
		   avg / units[VOL]->val, units[VOL]->name);
	}

	if (print_per_region_stats) {
	    double *vv;

	    bu_log("\tregions:\n");
	    for( BU_LIST_FOR( regp, region, &(state->rtip->HeadRegion) ))  {
		double low = HUGE;
		double hi = -HUGE;
		avg = 0.0;

		for (view=0 ; view < num_views ; view++) {
		    vv = &((struct per_region_data *)regp->reg_udata)->r_volume[view];

		    /* convert view length to a volume */
		    *vv = ((struct per_region_data *)regp->reg_udata)->r_len[view] *
			(state->area[view] / state->shots[view]);

		    /* convert to user's units */
		    *vv /= units[VOL]->val;

		    /* find limits of values */
		    if (*vv < low) low = *vv;
		    if (*vv > hi) hi = *vv;

		    avg += *vv;
		}

		avg /= num_views;

		bu_log("\t%s volume:%g %s +(%g) -(%g)\n",
		       regp->reg_name,
		       avg,
		       units[VOL]->name,
		       hi - avg,
		       avg - low);
	    }
	}



	/* print grand totals */
	avg = 0.0;
	for (view=0 ; view < num_views ; view++) {
	    avg += state->m_volume[view] =
		state->m_len[view] * ( state->area[view] / state->shots[view]);
	}

	avg /= num_views;
	bu_log("  Average total volume: %g %s\n", avg / units[VOL]->val, units[VOL]->name);
    }
    if (analysis_flags & ANALYSIS_OVERLAPS) list_report(&overlapList);
    if (analysis_flags & ANALYSIS_ADJ_AIR)  list_report(&adjAirList);
    if (analysis_flags & ANALYSIS_GAP) list_report(&gapList);
    if (analysis_flags & ANALYSIS_EXP_AIR) list_report(&exposedAirList);

    for( BU_LIST_FOR( regp, region, &(state->rtip->HeadRegion) ) )  {
	unsigned long hits;

	RT_CK_REGION(regp);
	hits = ((struct per_region_data *)regp->reg_udata)->hits;
	if ( hits < require_num_hits) {
	    if (hits == 0) {
		bu_log("%s was not hit\n", regp->reg_name);
	    } else {
		bu_log("%s hit only %u times (< %u)\n",
		       regp->reg_name, hits, require_num_hits);
	    }

	    return;
	}
    }
}

/*
 *	M A I N
 *
 *	Call parse_args to handle command line arguments first, then
 *	process input.
 */
int
main(int ac, char *av[])
{
    int arg_count;
    struct rt_i *rtip;
#define IDBUFSIZE 2048
    char idbuf[IDBUFSIZE];
    int i;
    struct cstate state;
    int start_objs; /* index in command line args where geom object list starts */

    max_cpus = ncpu = bu_avail_cpus();

    /* parse command line arguments */
    arg_count = parse_args(ac, av);

    if ((ac-arg_count) < 2) {
	usage("Error: Must specify model and objects on command line\n");
    }

    bu_semaphore_init(TOTAL_SEMAPHORES);

    rt_init_resource( &rt_uniresource, max_cpus, NULL );
    bn_rand_init( rt_uniresource.re_randptr, 0 );

    /*
     *  Load database.
     *  rt_dirbuild() returns an "instance" pointer which describes
     *  the database to be ray traced.  It also gives back the
     *  title string in the header (ID) record.
     */
    if( (rtip=rt_dirbuild(av[arg_count], idbuf, IDBUFSIZE)) == RTI_NULL ) {
	fprintf(stderr,"g_qa: rt_dirbuild failure on %s\n", av[arg_count]);
	exit(2);
    }
    rtip->useair = use_air;

    start_objs = ++arg_count;
    num_objects = ac - arg_count;


    /* Walk trees.
     * Here we identify any object trees in the database that the user
     * wants included in the ray trace.
     */
    for ( ; arg_count < ac ; arg_count++ )  {
	if( rt_gettree(rtip, av[arg_count]) < 0 )
	    fprintf(stderr,"rt_gettree(%s) FAILED\n", av[arg_count]);
    }

    /*
     *  Initialize all the per-CPU memory resources.
     *  The number of processors can change at runtime, init them all.
     */
    for( i=0; i < max_cpus; i++ )  {
	rt_init_resource( &resource[i], i, rtip );
	bn_rand_init( resource[i].re_randptr, i );
    }

    /*
     * This gets the database ready for ray tracing.
     * (it precomputes some values, sets up space partitioning, etc.)
     */
    rt_prep_parallel(rtip,ncpu);

    /* we now have to subdivide space
     *
     */
    VSUB2(state.span, rtip->mdl_max, rtip->mdl_min);
    state.area[0] = state.span[1] * state.span[2];
    state.area[1] = state.span[2] * state.span[0];
    state.area[2] = state.span[0] * state.span[1];

    if (analysis_flags & ANALYSIS_BOX) {
	bu_log("bounding box: %g %g %g  %g %g %g\n",
	       V3ARGS(rtip->mdl_min), V3ARGS(rtip->mdl_max));

	VPRINT("Area:", state.area);
    }
    if (verbose) bu_log("ncpu: %d\n", ncpu);

    /* if the user did not specify the initial grid spacing limit, we need
     * to compute a reasonable one for them.
     */
    if (gridSpacing == 0.0) {
	double min_span = MAX_FASTF;
	VPRINT("span", state.span);

	V_MIN(min_span, state.span[X]);
	V_MIN(min_span, state.span[Y]);
	V_MIN(min_span, state.span[Z]);

	gridSpacing = gridSpacingLimit;
	do {
	    gridSpacing *= 2.0;
	} while (gridSpacing < min_span);
	gridSpacing *= 0.25;
	if (gridSpacing < gridSpacingLimit) gridSpacing = gridSpacingLimit;
	bu_log("initial spacing %g\n", gridSpacing);

    }

    if (options_prep(rtip, state.span)) return -1;


    /* initialize some stuff */
    state.rtip = rtip;
    state.first = 1;
    allocate_per_region_data(&state, start_objs, ac, av);

    /* compute */
    do {
	double inv_spacing = 1.0/gridSpacing;
	int view;

	VSCALE(state.steps, state.span, inv_spacing);

	bu_log("grid spacing %g %s %d x %d x %d\n",
	       gridSpacing / units[LINE]->val,
	       units[LINE]->name,
	       state.steps[0]-1,
	       state.steps[1]-1,
	       state.steps[2]-1);


	for (view=0 ; view < num_views ; view++) {

	    if (verbose)
		bu_log("  view %d\n", view);

	    /* gross hack
	     * By assuming we have <= 3 views, we can let the view # indicate
	     * a coordinate axis.
	     *  Note this is used as an index into state.area[]
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

	    bu_parallel(plane_worker, ncpu, (genptr_t)&state);
	    view_reports(&state);

	}

	state.first = 0;
	gridSpacing *= 0.5;

    } while (terminate_check(&state));

    if (plot_overlaps) fclose(plot_overlaps);
    if (plot_weight) fclose(plot_weight);
    if (plot_volume) fclose(plot_volume);
    if (plot_adjair) fclose(plot_adjair);
    if (plot_gaps) fclose(plot_gaps);
    if (plot_expair) fclose(plot_expair);


    if (verbose)
	bu_log("Computation Done\n");
    summary_reports(&state, start_objs, ac, av);

    return(0);
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
