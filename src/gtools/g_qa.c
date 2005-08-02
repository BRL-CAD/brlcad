/*	G _ Q A . C --- perform quality assurance checks on geometry
 *
 *

 plot the points where overlaps start/stop

 *	Designed to be a framework for 3d sampling of the geometry volume.
 */
#include <stdlib.h>

#include "common.h"

#ifdef HAVE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif

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
#define SEM_LIST RT_SEM_LAST+1


/* declarations to support use of getopt() system call */
char *options = "A:a:de:f:g:Gn:N:P:rS:s:t:U:u:V:W:";
extern char *optarg;
extern int optind, opterr, getopt();

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

int analysis_flags = ANALYSIS_VOLUME | ANALYSIS_OVERLAPS | ANALYSIS_WEIGHT | ANALYSIS_EXP_AIR | ANALYSIS_ADJ_AIR | ANALYSIS_GAP ;


double azimuth_deg;
double elevation_deg;
char *densityFileName;
double gridSpacing = 50.0;
double gridSpacingLimit = 0.25; /* limit to 1/4 mm */
char makeOverlapAssemblies;
int require_num_hits = 1;
int ncpu = 1;
double Samples_per_model_axis = 4.0;
double Samples_per_prim_axis;
double overlap_tolerance;
double volume_tolerance = -1.0;
double weight_tolerance = -1.0;
/*double *grams;*/
int print_per_region_stats;
int max_region_name_len;
int use_air = 1;
int num_objects; /* number of objects specified on command line */
int max_cpus;
int num_views = 3;

FILE *plot_fp;
int overlap_color[3] = { 255, 255, 0 };
int gap_color[3] = { 128, 128, 255 };
int adjAir_color[3] = { 255, 0, 0 };
int expAir_color[3] = { 255, 128, 255 };

int debug;
#define dlog if (debug) bu_log

struct resource	resource[MAX_PSW];	/* memory resources for multi-cpu processing */

struct cstate {
    int curr_view;	/* the "view" number we are shooting */
    int u_axis; /* these three are in the range 0..2 inclusive and indicate which axis (X,Y,or Z) */
    int v_axis; /* is being used for the U, V, or invariant vector direction */
    int i_axis;
    int v;	/* this indicates how many "grid_size" steps in the v direction have been taken */
    double	*m_lenDensity;
    double	*m_len;
    double	*m_volume;
    double	*m_weight;
    unsigned long *shots;
    vect_t u_dir;		/* direction of U vector for "current view" */
    vect_t v_dir;		/* direction of V vector for "current view" */
    struct rt_i *rtip;
    long steps[3];	/* this is per-dimension, not per-view */
    vect_t span;	/* How much space does the geometry span in each of X,Y,Z directions */
    vect_t area;	/* area of the view for view with invariant at index */
    int first;		/* this is the first time we've computed a set of views */
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
    struct region 	*r1;
    struct region 	*r2;
    unsigned long	count;
    double		max_dist;
    vect_t		coord;
};

static struct region_pair gapList = { 
    { BU_LIST_HEAD_MAGIC,
      (struct bu_list *)&gapList,
      (struct bu_list *)&gapList },
    (struct region *)"Gaps",
    (struct region *)NULL,
    (unsigned long)0,
    (double)0.0,
    {0.0, 0.0, 0.0,}
};
static struct region_pair adjAirList = { 
    { BU_LIST_HEAD_MAGIC,
      (struct bu_list *)&adjAirList,
      (struct bu_list *)&adjAirList },
    (struct region *)"Adjacent Air",
    (struct region *)NULL,
    (unsigned long)0,
    (double)0.0,
    {0.0, 0.0, 0.0,}
};
static struct region_pair exposedAirList = { 
    { BU_LIST_HEAD_MAGIC,
      (struct bu_list *)&exposedAirList,
      (struct bu_list *)&exposedAirList },
    (struct region *)"Overlaps",
    (struct region *)NULL,
    (unsigned long)0,
    (double)0.0,
    {0.0, 0.0, 0.0,}
};
static struct region_pair overlapList = { 
    { BU_LIST_HEAD_MAGIC,
      (struct bu_list *)&overlapList,
      (struct bu_list *)&overlapList },
    (struct region *)"Overlaps",
    (struct region *)NULL,
    (unsigned long)0,
    (double)0.0,
    {0.0, 0.0, 0.0,}
};
    

/* XXX this section should be extracted to libbu/units.c */
struct cvt_tab {
    double val;
    char name[32];
};

static const struct cvt_tab units_tab[3][40] = {
    { /* length, stolen from bu/units.c with the  "none" value removed
       * Values for converting from given units to mm 
       */
	{1.0,		"mm"}, /* default */
	/*	{0.0,		"none"},*/
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

#define LINE 0
#define VOL 1
#define WGT 2
static const struct cvt_tab *units[3] = { 
    &units_tab[0][0],	/* linear */
    &units_tab[1][0],	/* volume */
    &units_tab[2][0]	/* weight */
};



const struct cvt_tab *vol_tab;
const struct cvt_tab *wgt_tab;
const struct cvt_tab *ovl_tab;



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
    double a, conv;
    char units_string[256];
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
	    if (!strcmp(cvt->name, units_string)) {
		goto found_units;
	    } else {
		cvt++;
	    }
	}
	bu_log("Bad units specifier \"%s\" on value \"%s\"\n", units, buf);
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
    char *strrchr();
    int i, j;
    double a, b;
    char *p;

    if (  ! (progname=strrchr(*av, '/'))  )
	progname = *av;
    else
	++progname;

    /* Turn off getopt's error messages */
    opterr = 0;

    /* get all the option flags from the command line */
    while ((c=getopt(ac,av,options)) != EOF)
	switch (c) {
	case 'A'	:
	    {
		char *p;
		analysis_flags = 0;
		for (p = optarg; *p ; p++) {
		    switch (*p) {
		    case 'A' :
			analysis_flags = ANALYSIS_VOLUME | ANALYSIS_WEIGHT | ANALYSIS_OVERLAPS |\
			    ANALYSIS_ADJ_AIR | ANALYSIS_GAP | ANALYSIS_EXP_AIR;
			break;
		    case 'a' :
			analysis_flags |= ANALYSIS_ADJ_AIR;
			break;
		    case 'b' :
			analysis_flags |= ANALYSIS_BOX;
			break;
		    case 'e' :
			analysis_flags |= ANALYSIS_EXP_AIR;
			break;
		    case 'g' :
			analysis_flags |= ANALYSIS_GAP;
			break;
		    case 'o' :
			analysis_flags |= ANALYSIS_OVERLAPS;
			break;
		    case 'v' :
			analysis_flags |= ANALYSIS_VOLUME;
			break;
		    case 'w' :
			analysis_flags |= ANALYSIS_WEIGHT;
			break;
		    }
		}
		break;
	    }
	case 'a'	:
	    bu_log("azimuth not implemented\n");
	    if (sscanf(optarg, "%lg", &azimuth_deg) != 1) {
		bu_log("error parsing azimuth \"%s\"\n", optarg);
	    }
	    break;
	case 'e'	:
	    bu_log("elevation not implemented\n");
	    if (sscanf(optarg, "%lg", &elevation_deg) != 1) {
		bu_log("error parsing elevation \"%s\"\n", optarg);
	    }
	    break;
	case 'd'	: debug = !debug; break;

	case 'f'	: densityFileName = optarg; break;

	case 'g'	:
	    {
		i = j = 0;

		if (p = strchr(optarg, ',')) {
		    *p++ = '\0';
		}

		if (read_units_double(&gridSpacing, optarg, &units_tab[0][0])) {
		    bu_log("error parsing grid spacing value \"%s\"\n", optarg);
		    exit(-1);
		}
		if (p) {
		    if (read_units_double(&gridSpacingLimit, p, units_tab[0])) {
			bu_log("error parsing grid spacing limit value \"%s\"\n", p);
			exit(-1);
		    }
		}

		bu_log("grid spacing:%gmm limit:%gmm\n", gridSpacing, gridSpacingLimit);
		break;
	    }
	case 'G'	:
	    makeOverlapAssemblies = 1;
	    bu_log("-G option unimplemented\n");
	    break;
	case 'n'	:
	    if (sscanf(optarg, "%d", &c) != 1 || c < 0) {
		bu_log("num_hits must be integer value >= 0, not \"%s\"\n", optarg);
		break;
	    }
	    require_num_hits = c;
	    break;

	case 'N'	:
	    num_views = atoi(optarg);
	    break;
	case 'P'	:
	    /* cannot ask for more cpu's than the machine has */
	    if ((c=atoi(optarg)) > 0 && c <= max_cpus ) ncpu = c;
	    break;	
	case 'r'	:
	    print_per_region_stats = 1;
	    break;
	case 'S'	:
	    if (sscanf(optarg, "%lg", &a) != 1 || a <= 0.0) {
		bu_log("error in specifying minimum samples per model axis: \"%s\"\n", optarg);
		break;
	    }
	    Samples_per_model_axis = a;
	    break;
	case 's'	:
	    if (sscanf(optarg, "%lg", &a) != 1 || a <= 0.0) {
		bu_log("error in specifying minimum samples per primitive axis: \"%s\"\n", optarg);
		break;
	    }
	    Samples_per_prim_axis = a;
	    bu_log("option -s samples_per_axis_min not implemented\n");
	    break;
	case 't'	: 
	    if (read_units_double(&overlap_tolerance, optarg, &units_tab[0][0])) {
		bu_log("error in overlap tolerance distance \"%s\"\n", optarg);
		exit(-1);
	    }
	    break;
	case 'V'	: 
	    if (read_units_double(&volume_tolerance, optarg, units_tab[1])) {
		bu_log("error in volume tolerance \"%s\"\n", optarg);
		exit(-1);
	    }
	    break;
	case 'W'	: 
	    if (read_units_double(&volume_tolerance, optarg, units_tab[2])) {
		bu_log("error in weight tolerance \"%s\"\n", optarg);
		exit(-1);
	    }
	    break;

	case 'U'	: use_air = atoi(optarg); break;
	case 'u'	:
	    {
		char *ptr = optarg;
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
			bu_log("Did not find units \"%s\" in coversion table\n", units_name[i]);
			bu_bomb("");
		    found_cv:
			units[i] = cv;
		    }
		}

		bu_log("Units:\n");
		for (i=0 ; i < 3 ; i++) {
		    bu_log(" %s: %s", dim[i], units[i]->name);
		}
		bu_log("\n");
		break;
	    }

	case '?'	:
	case 'h'	:
	default		: usage("Bad or help flag '%c' specified\n", c); break;
	}

    return(optind);
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
    char name[128];

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
	
	while (isblank(*p)) p++;

	if (q = strchr(p, '\n')) {
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
	bu_log("No density table object in database\n");
	return -1;
    }

    if (rt_db_get_internal(&intern, dp, rtip->rti_dbip, NULL, &rt_uniresource) < 0) {
	bu_log("could not import %s\n", dp->d_namep);
	return 1;
    }

    if (intern.idb_major_type&DB5_MAJORTYPE_BINARY_MASK == 0) {
	return 1;
    }


    bu = (struct rt_binunif_internal *)intern.idb_ptr;

    RT_CHECK_BINUNIF(bu);

    parse_densities_buffer(bu->u.int8, bu->count);
    return 0;
}


/*
 *	add_unique_pair
 */
struct region_pair *
add_unique_pair(struct region_pair *list, struct region *r1, struct region *r2, double dist, point_t pt)
{
    struct region_pair *rp, *rpair;

    /* look for it in our list */
    bu_semaphore_acquire( SEM_LIST );
    for( BU_LIST_FOR(rp, region_pair, &list->l) ) {

	if ( (r1 == rp->r1 && r2 == rp->r2) || (r1 == rp->r2 && r2 == rp->r1) ) {
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
    rpair->r1 = r1;
    rpair->r2 = r2;
    rpair->count = 1;
    rpair->max_dist = dist;
    VMOVE(rpair->coord, pt);
    list->max_dist ++; /* really a count */

    /* insert in the list at the "nice" place */
    for( BU_LIST_FOR(rp, region_pair, &list->l) ) {
	if (strcmp(rp->r1->reg_name, r1->reg_name) <= 0 )
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
 */
int
overlap(struct application *ap, struct partition *pp, struct region *reg1, struct region *reg2)
{	

    register struct xray	*rp = &ap->a_ray;
    register struct hit	*ihitp = pp->pt_inhit;
    register struct hit	*ohitp = pp->pt_outhit;
    point_t ihit;
    point_t ohit;
    double depth;

    depth = ohitp->hit_dist - ihitp->hit_dist;

    if( depth < overlap_tolerance ) {
	/* too small to matter, pick one or none */
	return(1);
    }

    VJOIN1( ihit, rp->r_pt, ihitp->hit_dist, rp->r_dir );
    VJOIN1( ohit, rp->r_pt, ohitp->hit_dist, rp->r_dir );

    if (plot_fp) {
	pl_color(plot_fp, V3ARGS(overlap_color));
	pdv_3line(plot_fp, ihit, ohit);
    }

    {
	struct region_pair *rp = 
	    add_unique_pair(&overlapList, reg1, reg2, depth, ihit);

	if (plot_fp) {
	    pl_color(plot_fp, V3ARGS(overlap_color));
	    pdv_3line(plot_fp, ihit, ohit);
	    pdv_3line(plot_fp, ihit, rp->coord);
	    pdv_3line(plot_fp, ohit, rp->coord);
	}
    }

    

    return(0);	/* No further consideration to this partition */
}

/*
 *	logoverlap
 *
 */
void
logoverlap(struct application *ap, const struct partition *pp, const struct bu_ptbl *regiontable, const struct partition *InputHdp)
{
    RT_CK_AP(ap);
    RT_CK_PT(pp);
    BU_CK_PTBL(regiontable);
    return;
}


/*
 *  rt_shootray() was told to call this on a hit.  He gives up the
 *  application structure which describes the state of the world
 *  (see raytrace.h), and a circular linked list of partitions,
 *  each one describing one in and out segment of one region.
 */
int
hit(register struct application *ap, struct partition *PartHeadp, struct seg *segs)
{
    /* see raytrace.h for all of these guys */
    register struct partition *pp;
    register struct hit *hitp;
    register struct soltab *stp;
    struct curvature cur;
    point_t		pt, last_out_point;
    vect_t		inormal;
    vect_t		onormal;
    int			last_air = 0; /* what was the aircode of the last item */
    int			air_first = 1; /* are we in an air before a solid */
    double	dist;	/* the thickness of the partition */
    double	last_out_dist;
    double	val;
    struct cstate *state = ap->a_uptr;

    /* examine each partition until we get back to the head */
    for( pp=PartHeadp->pt_forw; pp != PartHeadp; pp = pp->pt_forw )  {
	struct density_entry *de;

	/* inhit info */
	dist = pp->pt_outhit->hit_dist - pp->pt_inhit->hit_dist;
	VJOIN1(pt, ap->a_ray.r_pt, pp->pt_inhit->hit_dist, ap->a_ray.r_dir);


	/* checking for air sticking out of the model */
	if (analysis_flags & ANALYSIS_EXP_AIR && air_first) {
	    if (pp->pt_regionp->reg_aircode) {
		/* this shouldn't be air */
		bu_log("air %s before solid material on ray at %g %g %g\n",
		       pp->pt_regionp->reg_name, V3ARGS(pt) );
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
		double gap_dist = pp->pt_inhit->hit_dist - last_out_dist;
		if (gap_dist > overlap_tolerance) {

		    /* like overlaps, we only want to report unique pairs */
		    add_unique_pair(&gapList,
				    pp->pt_regionp,
				    pp->pt_back->pt_regionp, 
				    gap_dist,
				    pt);

		    /* like overlaps, let's plot */
		    if (plot_fp) {
			vect_t gapEnd;

			pl_color(plot_fp, V3ARGS(gap_color));
			VJOIN1(gapEnd, pt, gap_dist, ap->a_ray.r_dir);
			pdv_3line(plot_fp, pt, gapEnd);
		    }
		}
	    }
	}

	/* computing the weight of the objects */
	if (analysis_flags & ANALYSIS_WEIGHT) {

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

		    /* factor in the density of this object */
		    /* weight computation, factoring in the LOS percentage material of the object */
		    int los = pp->pt_regionp->reg_los;

		    if (los < 1) {
			bu_log("bad LOS (%d) on %s\n", los, pp->pt_regionp->reg_name );
		    }

		    /* accumulate the total weight values */
		    val = de->grams_per_cu_mm * dist * (pp->pt_regionp->reg_los * 0.01);
		    ap->a_color[0] += val;

		    prd = ((struct per_region_data *)pp->pt_regionp->reg_udata);
		    /* accumulate the per-region per-view weight values */
		    prd->r_lenDensity[state->i_axis] += val;

		    /* accumulate the per-object per-view weight values */
		    prd->optr->o_lenDensity[state->i_axis] += val;

		} else {
		    bu_log("density index %d from region %s is not set.\nAdd entry to density table\n",
			   pp->pt_regionp->reg_gmater, pp->pt_regionp->reg_name);
		    bu_bomb("");
		}
	    }
	}

	/* compute the volume of the object */
	if (analysis_flags & ANALYSIS_VOLUME) {
	    ap->a_color[1] += dist; /* add to total volume */

	    /* add to region volume */
	    ((struct per_region_data *)pp->pt_regionp->reg_udata)->r_len[state->i_axis] += dist;

	    /* add to object volume */
	    ((struct per_region_data *)pp->pt_regionp->reg_udata)->optr->o_len[state->i_axis] += dist;


	    dlog("\t\thit %g %g %g %g\n",
		   dist,
		   ap->a_color[1],
		   ((struct per_region_data *)pp->pt_regionp->reg_udata)->r_len[state->i_axis],
		   ((struct per_region_data *)pp->pt_regionp->reg_udata)->optr->o_len[state->i_axis]);
	}


	/* look for two adjacent air regions */
	if (analysis_flags & ANALYSIS_ADJ_AIR) {
	    if (last_air && pp->pt_regionp->reg_aircode && pp->pt_regionp->reg_aircode != last_air) {

		add_unique_pair(&adjAirList, pp->pt_back->pt_regionp, pp->pt_regionp, 0.0, pt);
	    }
	}

	/* note that this region has been seen */
	((struct per_region_data *)pp->pt_regionp->reg_udata)->hits++;

	last_air = pp->pt_regionp->reg_aircode;
	last_out_dist = pp->pt_outhit->hit_dist;
	VJOIN1(last_out_point, ap->a_ray.r_pt, pp->pt_outhit->hit_dist, ap->a_ray.r_dir);
    }


    if (analysis_flags & ANALYSIS_EXP_AIR && last_air) {
	bu_log("Air %s was last on ray at %g %g %g)\n",
	       PartHeadp->pt_back->pt_regionp->reg_name,
	       V3ARGS(last_out_point));
    }


    /*
     * This value is returned by rt_shootray
     * a hit usually returns 1, miss 0.
     */
    return(1);
}

/*
 * rt_shootray() was told to call this on a miss.
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
 */
void
plane_worker (int cpu, genptr_t ptr)
{
    struct application ap;
    int u, v;
    double v_coord;
    double z_coord;
    struct cstate *state = (struct cstate *)ptr;


    RT_APPLICATION_INIT(&ap);
    ap.a_rt_i = (struct rt_i *)state->rtip;	/* application uses this instance */
    ap.a_hit = hit;			/* where to go on a hit */
    ap.a_miss = miss;		/* where to go on a miss */
    ap.a_logoverlap = logoverlap;
    ap.a_overlap = overlap;
    ap.a_resource = &resource[cpu];
    ap.a_color[0] = 0.0; /* really the cumulative length*density */
    ap.a_color[1] = 0.0; /* really the cumulative length*density */

    /* gross hack */
    ap.a_ray.r_dir[state->u_axis] = ap.a_ray.r_dir[state->v_axis] = 0.0;
    ap.a_ray.r_dir[state->i_axis] = 1.0;

    ap.a_uptr = ptr; /* really copying the state ptr to the a_uptr */

    while (v = get_next_row(state)) {

	v_coord = v * gridSpacing; 
	dlog("  v = %d v_coord=%g\n", v, v_coord);

	if ( state->first || (v&1) ) {
	    /* shoot all the rays in this row */
	    /* this is either the first time a view has been computed
	     * or it is an even numbered row in a grid refinement
	     */
	    for (u=1 ; u < state->steps[state->u_axis]; u++) {
		ap.a_ray.r_pt[state->u_axis] = ap.a_rt_i->mdl_min[state->u_axis] + u * gridSpacing;
		ap.a_ray.r_pt[state->v_axis] = ap.a_rt_i->mdl_min[state->v_axis] + v * gridSpacing;
		ap.a_ray.r_pt[state->i_axis] = ap.a_rt_i->mdl_min[state->i_axis];

		dlog("%5g %5g %5g -> %g %g %g\n", V3ARGS(ap.a_ray.r_pt), V3ARGS(ap.a_ray.r_dir));
		(void)rt_shootray( &ap );
		state->shots[state->curr_view]++;
	    }
	} else {
	    /* shoot only the rays we need to on this row.
	     * Some of them have been computed in a previous iteration.
	     */
	    for (u=1 ; u < state->steps[state->u_axis]; u+=2) {
		ap.a_ray.r_pt[state->u_axis] = ap.a_rt_i->mdl_min[state->u_axis] + u * gridSpacing;
		ap.a_ray.r_pt[state->v_axis] = ap.a_rt_i->mdl_min[state->v_axis] + v * gridSpacing;
		ap.a_ray.r_pt[state->i_axis] = ap.a_rt_i->mdl_min[state->i_axis];

		dlog("%5g %5g %5g -> %g %g %g\n", V3ARGS(ap.a_ray.r_pt), V3ARGS(ap.a_ray.r_dir));
		(void)rt_shootray( &ap );
		state->shots[state->curr_view]++;

		if (debug)
		    if (u+1 <  state->steps[state->u_axis])
			bu_log("  ---\n");
	    }
	}
    }

    /* There's nothing else left to work on in this view.
     * It's time to add the values we have accumulated 
     * to the totals for the view and return.  When all 
     * threads have been through here, we'll have returned
     * to serial computation.
     */
    bu_semaphore_acquire(SEM_WORK);
    state->m_lenDensity[state->curr_view] += ap.a_color[0]; /* add our length*density value */
    state->m_len[state->curr_view] += ap.a_color[1]; /* add our volume value */
    bu_semaphore_release(SEM_WORK);
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

    if (p=strchr(str, '/')) *p = '\0';

    for (i=0 ; i < num_objects ; i++) {
	if (!strcmp(obj_rpt[i].o_name, str)) {
	    bu_free(str, "");
	    return i;
	}
    }
    bu_log("%s Didn't find object named \"%s\" in %d entries\n", BU_FLSTR, name, num_objects);
    bu_bomb("");
}

/* 
 * allocate_per_reigon_data
 *
 *	Allocate data structures for tracking statistics on a per-view basis
 *	for each of the view, object and region levels.
 */
void
allocate_per_reigon_data(struct cstate *state, int start, int ac, char *av[])
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

    i = 0;
    for( BU_LIST_FOR( regp, region, &(rtip->HeadRegion) ) )  {
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
 *	compute_view_results
 *
 *	Report the results of a particular view computation
 *	for one iteration.
 */
#if 0
void
compute_view_results(struct cstate *state)
{
    struct region *regp;
    double v = state->volume[state->i_axis] * gss;

    if (analysis_flags & ANALYSIS_VOLUME) {
	bu_log("\tvolume  %lg %s on axis %d\n",
	       v / units[VOL]->val, units[VOL]->name, state->i_axis );

	for( BU_LIST_FOR( regp, region, &(state->rtip->HeadRegion) ) )  {
	    bu_log("\t\t%s %g %g %g %g\n", regp->reg_name,
		   state->volume[state->i_axis],
		   ((struct per_region_data *)regp->reg_udata)->len[0],
		   ((struct per_region_data *)regp->reg_udata)->len[1],
		   ((struct per_region_data *)regp->reg_udata)->len[2]);
	}
    }

    if (analysis_flags & ANALYSIS_WEIGHT) {
	grams[state->i_axis] = state->densityLen[state->i_axis]*gridSpacing*gridSpacing;

	bu_log("\tWeight %lg %s\n",
	       grams[state->i_axis] / units[WGT]->val,
	       units[WGT]->name);
    }
}
#endif

/*
 *	list_report
 *
 */
void
list_report(struct region_pair *list)
{
    struct region_pair *rp;

    if (BU_LIST_IS_EMPTY(&list->l)) {
	bu_log("No %s\n", (char *)list->r1);
	return;
    }

    bu_log("list %s:\n", (char *)list->r1);

    for (BU_LIST_FOR(rp, region_pair, &overlapList.l)) {
	if (rp->r2) {
	    bu_log("%s %s %lu %g (%g %g %g)\n",
	       rp->r1->reg_name, rp->r2->reg_name, rp->count,
	       rp->max_dist, V3ARGS(rp->coord));
	} else {
	    bu_log("%s %lu %g (%g %g %g)\n",
	       rp->r1->reg_name, rp->count,
	       rp->max_dist, V3ARGS(rp->coord));
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
	    dlog("density from file\n");
	    if (get_densities_from_file(densityFileName)) {
		return -1;
	    }
	} else {
	    dlog("density from db\n");
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
	       gridSpacing / units[LINE]->val, units[LINE]->name, Samples_per_model_axis);

	bu_log("Adjusted to %g %s to get %g samples per model axis\n",
	       newGridSpacing / units[LINE]->val, units[LINE]->name, Samples_per_model_axis);

	gridSpacing = newGridSpacing;
    }

    /* if the vol/weight tolerances are not set, pick something */
    if (analysis_flags & ANALYSIS_VOLUME) {
	if (volume_tolerance == -1.0) {
	    volume_tolerance = span[X] * span[Y] * span[Z] * 0.0001;
	    bu_log("setting volume tolerance to %g %s\n", volume_tolerance / units[VOL]->val, units[VOL]->name);
	} else {
	    bu_log("volume tolerance   %g\n", volume_tolerance);
	}
    }
    if (analysis_flags & ANALYSIS_WEIGHT) {
	if (weight_tolerance == -1.0) {
	    struct density_entry *de;
	    double max_den = 0.0;
	    int i;
	    for (i=0 ; i < num_densities ; i++) {
		if (densities[i].grams_per_cu_mm > max_den)
		    max_den = densities[i].grams_per_cu_mm;
	    }
	    weight_tolerance = span[X] * span[Y] * span[Z] * 0.00000001 * max_den;
	    bu_log("setting weight tolerance to %g\n", weight_tolerance);
	} else {
	    bu_log("weight tolerance   %g\n", weight_tolerance);
	}
    }
    if (analysis_flags & ANALYSIS_OVERLAPS) {
	if (overlap_tolerance != 0.0) {
	    bu_log("overlap tolerance to %g\n", overlap_tolerance);
	}
	if ( (plot_fp=fopen("overlaps.pl", "w")) == (FILE *)NULL) {
	    bu_log("cannot open plot file\n");
	    bu_bomb("");
	}
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
    state->curr_view;
    if (analysis_flags & ANALYSIS_VOLUME) {
	int obj;
	int view;

	/* for each object, compute the volume for all views */
	for (obj=0 ; obj < num_objects ; obj++) {
	    double val;
	    /* compute volume of object for given view */
	    view = state->curr_view;

	    /* compute the per-view volume of this object */

	    val = obj_tbl[obj].o_volume[view] = 
		obj_tbl[obj].o_len[view] * (state->area[view] / state->shots[view]);

	    bu_log("\t%s volume %g %s\n", obj_tbl[obj].o_name,  val / units[VOL]->val, units[VOL]->name);
	}
    }
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
    double low, hi, val, delta;
    int axis;
    struct region *regp;
    unsigned long hits;


    bu_log("terminate_check\n");
    RT_CK_RTI(state->rtip);
    /* check to make sure every region was hit at least once */
    for( BU_LIST_FOR( regp, region, &(state->rtip->HeadRegion) ) )  {
	RT_CK_REGION(regp);

	hits = ((struct per_region_data *)regp->reg_udata)->hits;
	if ( hits < require_num_hits) {
	    if (hits == 0)	
		bu_log("%s was not hit\n", regp->reg_name);
	    else
		bu_log("%s hit only %u times (< %u)\n", regp->reg_name, hits, require_num_hits);

	    return 1;
	}
    }

    if (analysis_flags & ANALYSIS_WEIGHT) {
#if 0
	/* find the range of the mass estimates */
	low = hi = 0.0;
	val = grams[0];
	for (axis=1 ; axis < NUM_VIEWS ; axis++) {
	    delta = val - grams[axis];

	    if (delta < low) low = delta;
	    if (delta > hi) hi = delta;
	}
	delta = hi - low;
	/* compare values to tolerance */
	if (delta < weight_tolerance) {
	    bu_log("weight computations agree within %g g which is within tolerance %g g\n", delta, weight_tolerance);
	    return 0;
	}
#endif
    }
    if (analysis_flags & ANALYSIS_VOLUME) {
	/* find the range of values for object volumes */
	int can_terminate = 1; /* assume everyone is within tolerance */
	int obj;
	int view;

	/* for each object, compute the volume for all views */
	for (obj=0 ; obj < num_objects ; obj++) {

	    /* compute volume of object for given view */
	    view = 0;
	    val = obj_tbl[obj].o_volume[view] = obj_tbl[obj].o_len[view] * (state->area[view] / state->shots[view]);

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
	    if (delta > volume_tolerance) {
		/* this object differs too much in each view, so we need to refine the grid */
		can_terminate = 0;
		break;
	    }
	    bu_log("%s volume %g %s +%g -%g\n",
		   obj_tbl[obj].o_name,
		   val / units[VOL]->val, units[VOL]->name,
		   hi / units[VOL]->val,
		   low / units[VOL]->val);
	}
	if (can_terminate) {
	    bu_log("all objects within tolerance on volume calculation\n");
	    return 0;
	}
    }

    if ( (analysis_flags & ANALYSIS_OVERLAPS)) {
	if (BU_LIST_NON_EMPTY(&overlapList.l) && gridSpacing < gridSpacingLimit*4) {
	    /* since we've found an overlap, we can quit */
	    return 0;
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
    /* refine the gridSpacing and try again */
    if (gridSpacing < gridSpacingLimit) {
	bu_log("grid spacing refined to %g which is below lower limit %g\n", gridSpacing, gridSpacingLimit);
	return 0;
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
    bu_log("Summary:\n");
    if (analysis_flags & ANALYSIS_VOLUME) {
	double avg;
	int obj;
	int view;
	double v;

	/* print per-object */
	for (obj=0 ; obj < num_objects ; obj++) {
	    avg = 0.0;

	    for (view=0 ; view < num_views ; view++)
		avg += obj_tbl[obj].o_volume[view];

	    avg /= num_views;
	    bu_log("%*s %g %s\n", -max_region_name_len, obj_tbl[obj].o_name,
		   avg / units[VOL]->val, units[VOL]->name);
	}
	/* print grand totals */
	avg = 0.0;
	v = 0.0;
	for (view=0 ; view < num_views ; view++) {
	    avg += state->m_volume[view] = state->m_len[view] * ( state->area[view] / state->shots[view]);
	}

	avg /= num_views;
	bu_log("  Average total volume: %g %s\n", avg / units[VOL]->val, units[VOL]->name);
    }
    if (analysis_flags & ANALYSIS_OVERLAPS) list_report(&overlapList);
    if (analysis_flags & ANALYSIS_ADJ_AIR)  list_report(&adjAirList);
    if (analysis_flags & ANALYSIS_GAP) list_report(&gapList);
    if (analysis_flags & ANALYSIS_EXP_AIR) list_report(&exposedAirList);
}
#if 0

void
summary_reports(struct cstate *state, int start, int ac, char *av[])
{
    int i;
    struct region *regp;
   char *p;
#define BSIZE 1024
    char region_name[BSIZE];
    vect_t u;
    int region_number;
    struct rt_i *rtip = state->rtip;

    /* pre-compute some values for units conversion */
    u[LINE] = 1.0/units[LINE]->val;
    u[VOL] = 1.0/units[VOL]->val;
    u[WGT] = 1.0/units[WGT]->val;


    /* Find out what the longest region name is */
    for( BU_LIST_FOR( regp, region, &(rtip->HeadRegion) ) )  {
    }


    if (analysis_flags & ANALYSIS_VOLUME) {
	
	bu_log("--- Volume  ---\n");
	/* print per-region */
	if (print_per_region_stats) {
	    bu_log("regions:\n\t%*s  Volume (per view) in %s\n", -max_region_name_len, "Name", units[VOL]->name );
	    for( BU_LIST_FOR( regp, region, &(rtip->HeadRegion) ) )  {
		double *v =  ((struct per_region_data *)regp->reg_udata)->r_len;

		RT_CK_REGION(regp);

		/* XXX		VSCALE(v, v, gss);	/* convert linear to volume estimate */
		VSCALE(v, v, u[VOL]); /* convert to user units */

		bu_log("%*s %g %g %g", -max_region_name_len, regp->reg_name, V3ARGS(v));
	    }
	}
	/* print per-object */
	for (i=0 ; i < num_objects ; i++) {

	    /*	XXX    VSCALE(obj_tbl[i].volume, obj_tbl[i].volume, gss); */
	    VSCALE(obj_tbl[i].volume, obj_tbl[i].volume, u[VOL]);

	    bu_log("%*s %g %g %g\n", -max_region_name_len, obj_tbl[i].name, V3ARGS(obj_tbl[i].volume));

	}
	/* print grand totals */
	double avg_vol = (state->volume[0] + state->volume[1] + state->volume[2]) / 3.0;
	bu_log("  Average total volume: %g %s\n", avg_vol / units[VOL]->val, units[VOL]->name);
    }


    if (analysis_flags & ANALYSIS_WEIGHT) {
	bu_log("--- Weight ---\n");

	if (print_per_region_stats) {
	    bu_log("regions:\n\t%*s  Weight (per view) %s\n", -max_region_name_len, "Name", units[WGT]->name );

	    for( BU_LIST_FOR( regp, region, &(rtip->HeadRegion) ) )  {
		double *w =  ((struct per_region_data *)regp->reg_udata)->lenDensity;
		RT_CK_REGION(regp);

		/* XXX		VSCALE(w, w, gss);	/* convert linear to volume estimate */
		VSCALE(w, w, u[WGT]);	/* compute weight in user units */

		bu_log("%*s %g %g %g", -max_region_name_len, regp->reg_name, V3ARGS(w));
	    }
	}
	/* print per-object data */
	for (i=0 ; i < num_objects ; i++) {
	    /* XXX	    VSCALE(obj_tbl[i].weight, obj_tbl[i].weight, gss); */
	    VSCALE(obj_tbl[i].weight, obj_tbl[i].weight, u[WGT]);

	    bu_log("%*s %g %g %g", -max_region_name_len, regp->reg_name, V3ARGS(obj_tbl[i].weight));
	}

	double avg_densityLen = (state->densityLen[0] + state->densityLen[1] + state->densityLen[2]) / 3.0;
	/* XXX 	double weight = avg_densityLen * gss; */
	bu_log("  Average total weight: %g %s\n", weight / units[WGT]->val);
    }

}
#endif

/*
 *	M A I N
 *
 *	Call parse_args to handle command line arguments first, then
 *	process input.
 */
int
main(ac,av)
     int ac;
     char *av[];
{
    int arg_count;
    FILE *inp;
    int status;
    struct rt_i *rtip;
    int u_axis, v_axis, i_axis;
    long u, v;
    char idbuf[132];
    vect_t origin;
    vect_t dir;
    int i;
    struct directory *dp;
    struct cstate state;
    double lim, val;
    int crv;
    int start_objs; /* index in command line args where geom object list starts */

    max_cpus = ncpu = bu_avail_cpus();

    /* parse command line arguments */
    arg_count = parse_args(ac, av);
	
    if ((ac-arg_count) < 2) {
	usage("Error: Must specify model and objects on command line\n");
    }

    bu_semaphore_init(RT_SEM_LAST+2);

    rt_init_resource( &rt_uniresource, max_cpus, NULL );
    bn_rand_init( rt_uniresource.re_randptr, 0 );

    /*
     *  Load database.
     *  rt_dirbuild() returns an "instance" pointer which describes
     *  the database to be ray traced.  It also gives back the
     *  title string in the header (ID) record.
     */
    if( (rtip=rt_dirbuild(av[arg_count], idbuf, sizeof(idbuf))) == RTI_NULL ) {
	fprintf(stderr,"rtexample: rt_dirbuild failure\n");
	exit(2);
    }
    rtip->useair = use_air;

    /* XXX set the default linear units if the user hasn't specified them on the command line */


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
	bu_log("bounding box: %g %g %g  %g %g %g\n", V3ARGS(rtip->mdl_min), V3ARGS(rtip->mdl_max));

	VPRINT("Area:", state.area);
    }
    bu_log("ncpu: %d\n", ncpu);


    if (options_prep(rtip, state.span)) return -1;


    if ( (analysis_flags & (ANALYSIS_ADJ_AIR|ANALYSIS_EXP_AIR)) && ! use_air ) {
	bu_bomb("Error:  Air regions discarded but air analysis requested!\nSet use_air non-zero or eliminate air analysis\n");
    }

    /* initialize some stuff */
    state.rtip = rtip;
    state.first = 1;
    allocate_per_reigon_data(&state, start_objs, ac, av);

    /* compute */
    do {
	double inv_spacing = 1.0/gridSpacing;
	int view;

	VSCALE(state.steps, state.span, inv_spacing);

	bu_log("grid spacing %gmm  %d x %d x %d\n", gridSpacing, V3ARGS(state.steps));


	for (view=0 ; view < num_views ; view++) {

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
