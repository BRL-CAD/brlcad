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

/* declarations to support use of getopt() system call */
char *options = "A:a:de:f:g:Gn:P:rS:s:t:U:u:V:W:";
extern char *optarg;
extern int optind, opterr, getopt();

/* variables set by command line flags */
char *progname = "(noname)";
char *usage_msg = "Usage: %s [options] model object [object...]\n\
";

#define NUM_VIEWS 3


#define ANALYSIS_VOLUME 1
#define ANALYSIS_WEIGHT 2
#define ANALYSIS_OVERLAPS 4
#define ANALYSIS_ADJ_AIR 8
#define ANALYSIS_GAP	16
#define ANALYSIS_EXT_AIR 32
#define ANALYSIS_BOX 64

int analysis_flags = ANALYSIS_VOLUME | ANALYSIS_OVERLAPS | ANALYSIS_WEIGHT | ANALYSIS_EXT_AIR | ANALYSIS_ADJ_AIR | ANALYSIS_GAP ;


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
vect_t grams;
int print_per_region_stats;

int use_air = 1;

int max_cpus;

int debug;
#define dlog if (debug) bu_log

static long	noverlaps;		/* Number of overlaps seen */
static long	overlap_count;		/* Number of overlap pairs seen */
static long	unique_overlap_count;	/* Number of unique overlap pairs seen */



struct resource	resource[MAX_PSW];	/* memory resources for multi-cpu processing */

struct state {
    int curr_view;	/* the "view" number we are shooting */
    int u_axis; /* these three are in the range 0..2 inclusive and indicate which axis (X,Y,or Z) */
    int v_axis; /* is being used for the U, V, or invariant vector direction */
    int i_axis;
    int v;	/* this indicates how many "grid_size" steps in the v direction have been taken */
    double densityLen[NUM_VIEWS];	/* per-view values */
    double volume[NUM_VIEWS];	/* per-vew computation of volume */
    vect_t u_dir;		/* direction of U vector for "current view" */
    vect_t v_dir;		/* direction of V vector for "current view" */
    struct rt_i *rtip;
};
/* # of "grid_size" steps to take along each axis (X,Y,Z) 
 * to cover the face of the bounding rpp?
 */
long steps[3]; 


/* How much space does the geometry span in each of X,Y,Z directions */
vect_t span; 

/* the entries in the density table */
struct density_entry {
    long	magic;
    double	grams_per_cu_mm;
    char	*name;
} *densities;
int num_densities;
#define DENSITY_MAGIC 0xaf0127


/* this is the data we track for each region
 */
struct per_region_data {
    unsigned long hits;
    double	lenDensity[NUM_VIEWS]; /* for per-region per-view weight computation */
    double	len[NUM_VIEWS]; /* for per-region, per-veiew computation */
};



/*
 *  For each unique pair of regions that we find an overlap for
 *  we build up one of these structures.
 *  Note that we could also discriminate at the solid pair level.
 */
struct overlap_list {
    struct overlap_list *next;	/* next one */
    const char 	*reg1;		/* overlapping region 1 */
    const char	*reg2;		/* overlapping region 2 */
    long	count;			/* number of time reported */
    double max_thickness;
    vect_t coord;
};
static struct overlap_list *olist=NULL;	/* root of the list */


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
			    ANALYSIS_ADJ_AIR | ANALYSIS_GAP | ANALYSIS_EXT_AIR;
			break;
		    case 'a' :
			analysis_flags |= ANALYSIS_ADJ_AIR;
			break;
		    case 'b' :
			analysis_flags |= ANALYSIS_BOX;
			break;
		    case 'e' :
			analysis_flags |= ANALYSIS_EXT_AIR;
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

	case 'P'	:
	    /* cannot ask for more cpu's than the machine has */
	    if ((c=atoi(optarg)) > 0 && c <= max_cpus ) ncpu = c;
	    break;	
	case 'r'	:
	    print_per_reigon_stats = 1;
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
	    densities = bu_realloc(densities, sizeof(struct density_entry)*num_densities*2, "density entries");
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

#if 0
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
    vect_t	ihit;
    vect_t	ohit;
    double depth;

	

    VJOIN1( ihit, rp->r_pt, ihitp->hit_dist, rp->r_dir );
    VJOIN1( ohit, rp->r_pt, ohitp->hit_dist, rp->r_dir );
    depth = ohitp->hit_dist - ihitp->hit_dist;
    if( depth < overlap_tolerance )
	return(0);

    bu_semaphore_acquire( BU_SEM_SYSCALL );
    //	pdv_3line( outfp, ihit, ohit );
    noverlaps++;
    bu_semaphore_release( BU_SEM_SYSCALL );

    if( !(analysis_flags&ANALYSIS_OVERLAPS) ) {
	bu_log("\nOVERLAP %d: %s\nOVERLAP %d: %s\nOVERLAP %d: depth %gmm\nOVERLAP %d: in_hit_point (%g,%g,%g)mm\nOVERLAP %d: out_hit_point (%g,%g,%g) mm\n------------------------------------------------------------\n",
	       noverlaps,reg1->reg_name,
	       noverlaps,reg2->reg_name,
	       noverlaps,depth,
	       noverlaps,ihit[X],ihit[Y],ihit[Z],
	       noverlaps,ohit[X],ohit[Y],ohit[Z]);

	/* If we report overlaps, don't print if already noted once.
	 * Build up a linked list of known overlapping regions and compare 
	 * againt it.
	 */
    } else {
	struct overlap_list	*prev_ol = (struct overlap_list *)0;
	struct overlap_list	*op;		/* overlap list */
	struct overlap_list     *new_op;
	new_op =(struct overlap_list *)bu_malloc(sizeof(struct overlap_list),"overlap list");

	/* look for it in our list */
	bu_semaphore_acquire( BU_SEM_SYSCALL );
	for( op=olist; op; prev_ol=op, op=op->next ) {

	    /* if we already have an entry for this region pair, 
	     * we increase the counter, check the depth and 
	     * update thickness maximum and entry point if need be
	     * and return 
	     */
	    if( (strcmp(reg1->reg_name,op->reg1) == 0) && (strcmp(reg2->reg_name,op->reg2) == 0) ) {
		op->count++;
		if (depth > op->max_thickness) {
		    op->max_thickness = depth;
		    VMOVE(op->coord, ihit);
		}


		bu_semaphore_release( BU_SEM_SYSCALL );
		bu_free( (char *) new_op, "overlap list");
		return	0;	/* already on list */
	    } 
	}
		
	for( op=olist; op; prev_ol=op, op=op->next ) {
	    /* if this pair was seen in reverse, decrease the unique counter */
	    if ((strcmp(reg1->reg_name, op->reg2) == 0) && (strcmp(reg2->reg_name, op->reg1) == 0)) {
		unique_overlap_count--;
		break;
	    }
	}
		
	/* we have a new overlapping region pair */
	overlap_count++;
	unique_overlap_count++;
		
	op = new_op;
	if( olist )		/* previous entry exists */
	    prev_ol->next = op;
	else
	    olist = op;	/* finally initialize root */
	op->reg1 = reg1->reg_name;
	op->reg2 = reg2->reg_name;
	op->next = NULL;
	op->count = 1;
	op->max_thickness = depth;
	VMOVE(op->coord, ihit);
	bu_semaphore_release( BU_SEM_SYSCALL );
    }

    /* useful debugging */
    if (0) {
	struct overlap_list	*op;		/* overlap list */
	bu_log("PRINTING LIST::reg1==%s, reg2==%s\n", reg1->reg_name, reg2->reg_name);
	for (op=olist; op; op=op->next) {
	    bu_log("\tpair: %s  %s  %d matches\n", op->reg1, op->reg2, op->count);
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
    struct state *state = ap->a_uptr;

    /* examine each partition until we get back to the head */
    for( pp=PartHeadp->pt_forw; pp != PartHeadp; pp = pp->pt_forw )  {
	struct density_entry *de;

	/* inhit info */
	dist = pp->pt_outhit->hit_dist - pp->pt_inhit->hit_dist;
	VJOIN1(pt, ap->a_ray.r_pt, pp->pt_inhit->hit_dist, ap->a_ray.r_dir);


	/* checking for air sticking out of the model */
	if (analysis_flags & ANALYSIS_EXT_AIR && air_first) {
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
		if ((pp->pt_inhit->hit_dist - last_out_dist) > overlap_tolerance) {
		    bu_log("Void found between %s (%g %g %g) and %s (%g %g %g)\n",
			   pp->pt_back->pt_regionp->reg_name, V3ARGS(last_out_point),
			   pp->pt_regionp->reg_name, V3ARGS(pt) );
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
		    /* factor in the density of this object */
		    /* weight computation, factoring in the LOS percentage material of the object */
		    int los = pp->pt_regionp->reg_los;

		    if (los < 1) {
			bu_log("bad LOS (%d) on %s\n", los, pp->pt_regionp->reg_name );
		    }

		    /* accumulate the total weight values */
		    val = de->grams_per_cu_mm * dist * (pp->pt_regionp->reg_los * 0.01);
		    ap->a_color[0] += val;

		    /* accumulate the per-region per-view weight values */
		    ((struct per_region_data *)pp->pt_regionp->reg_udata)->lenDensity[state->i_axis] += val;
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
	    ((struct per_region_data *)pp->pt_regionp->reg_udata)->len[state->i_axis] += dist; /* add to region volume */
	}


	/* look for two adjacent air regions */
	if (analysis_flags & ANALYSIS_ADJ_AIR) {
	    if (last_air && pp->pt_regionp->reg_aircode && pp->pt_regionp->reg_aircode != last_air) {
		bu_log("air contiguous between %s and %s at (%g %g %g)",
		       pp->pt_back->pt_regionp->reg_name,
		       pp->pt_regionp->reg_name, V3ARGS(pt)
		       );
	    }
	}

	/* note that this region has been seen */
	((struct per_region_data *)pp->pt_regionp->reg_udata)->hits++;

	last_air = pp->pt_regionp->reg_aircode;
	last_out_dist = pp->pt_outhit->hit_dist;
	VJOIN1(last_out_point, ap->a_ray.r_pt, pp->pt_outhit->hit_dist, ap->a_ray.r_dir);
    }


    if (analysis_flags & ANALYSIS_EXT_AIR && last_air) {
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
    struct state *state = (struct state *)ptr;


    RT_APPLICATION_INIT(&ap);
    ap.a_rt_i = (struct rt_i *)state->rtip;	/* application uses this instance */
    ap.a_hit = hit;			/* where to go on a hit */
    ap.a_miss = miss;		/* where to go on a miss */
    ap.a_logoverlap = logoverlap;
    ap.a_overlap = overlap;
    ap.a_resource = &resource[cpu];
    ap.a_color[0] = 0.0; /* really the cumulative length*density */
    ap.a_color[1] = 0.0; /* really the cumulative length*density */

    ap.a_ray.r_dir[state->u_axis] = ap.a_ray.r_dir[state->v_axis] = 0.0;
    ap.a_ray.r_dir[state->i_axis] = 1.0;

    ap.a_uptr = ptr;

    while (1) {

	/* look for more work */
	bu_semaphore_acquire(SEM_WORK);
	if (state->v >= steps[state->v_axis]) {
	    /* There's nothing else left to work on in this view.
	     * It's time to add the values we have accumulated 
	     * to the totals for the view and return.  When all 
	     * threads have been through here, we'll have returned
	     * to serial computation.
	     */
	    state->densityLen[state->i_axis] += ap.a_color[0]; /* add our length*density value */
	    state->volume[state->i_axis] += ap.a_color[1]; /* add our volume value */
	    bu_semaphore_release(SEM_WORK);

	    return;
	}

	/* get a row to work on */
	v = state->v++;
	bu_semaphore_release(SEM_WORK);

	v_coord = gridSpacing * 0.5 + v * gridSpacing; 

	state->densityLen[state->i_axis] = 0.0;
	state->volume[state->i_axis] = 0.0;


	if (state->densityLen[state->i_axis] == 0.0 || (v&1) == 0) {
	    /* shoot all the rays in this row */
	    /* this is either the first time a view has been computed
	     * or it is an even numbered row in a grid refinement
	     */
	    for (u=0 ; u < steps[state->u_axis]; u++) {
		ap.a_ray.r_pt[state->u_axis] = ap.a_rt_i->mdl_min[state->u_axis] + u * gridSpacing + 0.5 * gridSpacing;
		ap.a_ray.r_pt[state->v_axis] = ap.a_rt_i->mdl_min[state->v_axis] + v * gridSpacing + 0.5 * gridSpacing;
		ap.a_ray.r_pt[state->i_axis] = ap.a_rt_i->mdl_min[state->i_axis];

		/* bu_log("%g %g %g -> %g %g %g\n", V3ARGS(ap.a_ray.r_pt), V3ARGS(ap.a_ray.r_dir));*/
		(void)rt_shootray( &ap );
	    }
	} else {
	    /* shoot only the rays we need to on this row.
	     * Some of them have been computed in a previous iteration.
	     */
	    for (u=1 ; u < steps[state->u_axis]; u+=2) {
		ap.a_ray.r_pt[state->u_axis] = ap.a_rt_i->mdl_min[state->u_axis] + u * gridSpacing + 0.5 * gridSpacing;
		ap.a_ray.r_pt[state->v_axis] = ap.a_rt_i->mdl_min[state->v_axis] + v * gridSpacing + 0.5 * gridSpacing;
		ap.a_ray.r_pt[state->i_axis] = ap.a_rt_i->mdl_min[state->i_axis];

		/* bu_log("%g %g %g -> %g %g %g\n", V3ARGS(ap.a_ray.r_pt), V3ARGS(ap.a_ray.r_dir));*/
		(void)rt_shootray( &ap );
	    }
	}
    }
}

/* 
 * allocate_per_reigon_data
 *
 *	Allocate data structures for tracking statistics on a per-region basis
 */
void
allocate_per_reigon_data(struct rt_i *rtip)
{
    struct region *regp;
    struct per_region_data *ptr;
    int i;

    ptr = bu_calloc(rtip->nregions, sizeof(struct per_region_data), "per_region_data");

    i = 0;
    for( BU_LIST_FOR( regp, region, &(rtip->HeadRegion) ) )  {
	regp->reg_udata = &ptr[i++];
    }
}



/*
 *	compute_results
 *
 *	Report the results of a particular view computation
 *
 */
void
compute_results(struct state *state)
{
    state->volume[state->i_axis] *= gridSpacing*gridSpacing;

    bu_log("\t");
    if (analysis_flags & ANALYSIS_VOLUME) {
	bu_log(" volume %lg %s",
	       state->volume[state->i_axis] / units[VOL]->val,
	       units[VOL]->name
	       );
    }

    if (analysis_flags & ANALYSIS_WEIGHT) {
	grams[state->i_axis] = state->densityLen[state->i_axis]*gridSpacing*gridSpacing;

	bu_log(" Weight %lg %s",
	       grams[state->i_axis] / units[WGT]->val,
	       units[WGT]->name);
    }
    bu_log("\n");
}



/*
 *	report_overlaps
 *
 */
void
report_overlaps()
{
    struct overlap_list *op, *prev_ol;

    if (olist == (struct overlap_list *)NULL) {
	bu_log("No overlaps\n");
	return;
    }

    bu_log("Overlaps\ttotal encountered:%d  total pairs:%d  unique pairs:%d\n", noverlaps, overlap_count, unique_overlap_count);
    for( op=olist; op; prev_ol=op, op=op->next ) {
	bu_log("%s %s  %d times, max_thickness %gmm at %g %g %g\n", op->reg1, op->reg2, op->count,
	       op->max_thickness, V3ARGS(op->coord));
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
	bu_log("Grid spacing %g %s is larger than model bounding box axis\n",
	       gridSpacing / units[LINE]->val, units[LINE]->name);

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
    }
    return 0;
}

/*
 *	compute_views
 *
 */
void 
compute_views(struct state *state)
{
    int axis;
    int num_axes = 3;
    double inv_spacing = 1.0/gridSpacing;

    VSCALE(steps, span, inv_spacing);

    bu_log("grid spacing %gmm  %d x %d x %d\n", gridSpacing, V3ARGS(steps));

    for (axis=0 ; axis < num_axes ; axis++) {
	state->curr_view = axis;

	state->v = 0;
	/* xy, yz, zx */
	state->u_axis = axis;
	state->v_axis = (axis+1) % 3;
	state->i_axis = (axis+2) % 3;


	state->u_dir[state->u_axis] = 1;
	state->u_dir[state->v_axis] = 0;
	state->u_dir[state->i_axis] = 0;

	state->v_dir[state->u_axis] = 0;
	state->v_dir[state->v_axis] = 1;
	state->v_dir[state->i_axis] = 0;

	bu_parallel(plane_worker, ncpu, (genptr_t)state); /* xy plane */
	compute_results(state);
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
terminate_check(struct state *state)
{
    double low, hi, val, delta;
    int axis;
    struct region *regp;
    unsigned long hits;


    RT_CK_RTI(state->rtip);
    for( BU_LIST_FOR( regp, region, &(state->rtip->HeadRegion) ) )  {
	RT_CK_REGION(regp);

	hits =((struct per_region_data *)regp->reg_udata)->hits;

	if (hits == 0) {
	    bu_log("%s was not hit\n", regp->reg_name);
	    return 1;
	} else if ( hits < require_num_hits) {
	    bu_log("%s hit only %u times (< %u)\n", regp->reg_name, hits, require_num_hits);
	    return 1;
	}
    }


    /* since the weight computation sets a global variable, it must
     * come first before the volume and gridspacing checks.
     */
    if (analysis_flags & ANALYSIS_WEIGHT) {
	/* XXX we rely on the fact that report_results() has been called
	 * to compute the values for the global array grams[]
	 */

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
    }
    if (analysis_flags & ANALYSIS_VOLUME) {
	/* find the range of values for volume */
	low = hi = 0.0;
	val = state->volume[0];
	for (axis=1 ; axis < NUM_VIEWS ; axis++) {
	    delta = val - state->volume[axis];

	    if (delta < low) low = delta;
	    if (delta > hi) hi = delta;
	}
	delta = hi - low;

	if (delta < volume_tolerance) {
	    bu_log("volume computations agree within %g mm^3\nwhich is within tolerance %g mm^3\n",
		   delta, volume_tolerance);
	    return 0;
	}
    }

    /* refine the gridSpacing and try again */
    gridSpacing *= 0.5;
    if (gridSpacing < gridSpacingLimit) {
	bu_log("grid spacing refined to %g which is below lower limit %g\n", gridSpacing, gridSpacingLimit);
	return 0;
    }

    return 1;
}	

/* summary data structure for objects specified on command line */
struct rpt_tab {
    char *name;
    double volume[3];
    double weight[3];
};

int
find_cmd_line_obj(struct rpt_tab *obj_rpt, int tot, char *name)
{
    int i;

    for (i=0 ; i < tot ; i++) {
	if (!strcmp(obj_rpt[i].name, name)) {
	    return i;
	}
    }
    bu_log("%s Didn't find object named \"%s\"\n", BU_FLSTR, name);
    bu_bomb("");
}

/*
 *	summary_reports
 *
 */
void
summary_reports(struct state *state, int start, int ac, char *av[])
{
    int i, tot;
    struct region *regp;
    struct rpt_tab  *obj_rpt;
   char *p;
#define BSIZE 1024
    char region_name[BSIZE];
    double gss = gridSpacing * gridSpacing;
    vect_t u;
    int region_number;
    int max_name_len = 0;
    struct rt_i *rtip = state->rtip;

    /* build data structures for the list of
     * objects the user specified on the command line
     */
    obj_rpt = bu_calloc(sizeof(struct rpt_tab), ac-start, "report tables");
    start++;
    tot = ac - start;
    for (i=0 ; i < tot ; i++) {
	obj_rpt[i].name = av[start+i];
    } 


    /* pre-compute some values for units conversion */
    u[LINE] = 1.0/units[LINE]->val;
    u[VOL] = 1.0/units[VOL]->val;
    u[WGT] = 1.0/units[WGT]->val;

    /* Find out what the longest region name is */
    for( BU_LIST_FOR( regp, region, &(rtip->HeadRegion) ) )  {
	int m = strlen(regp->reg_name);
	if (m > max_name_len) max_name_len = m;
    }


    if (analysis_flags & ANALYSIS_VOLUME) {
	
	bu_log("--- Volume  ---\n  regions:\n");
	bu_log("\t%*s  Volume (per view) in %s\n", -max_name_len, "Name", units[VOL]->name );
	for( BU_LIST_FOR( regp, region, &(rtip->HeadRegion) ) )  {
	    double *v =  ((struct per_region_data *)regp->reg_udata)->len;

	    RT_CK_REGION(regp);

	    /* extract the top-level object for the given region from the name
	     * ensure null termination of the copied string
	     */
	    strncpy(region_name, &regp->reg_name[1], sizeof(region_name)-1);
	    region_name[sizeof(region_name)-1] = '\0';
	    if (p = strchr(region_name, '/'))   *p = '\0';
	    region_number = find_cmd_line_obj(obj_rpt, tot, region_name);

	    /* convert linear to volume estimate */
	    VSCALE(v, v, gss);
	    VSCALE(v, v, u[VOL]); /* convert to user units */

	    /* find the top level object for this region, and add the volume to
	     * the accumulated volume for that object
	     */

	    VADD2(obj_rpt[region_number].volume, obj_rpt[region_number].volume, v);

	    if (print_per_region_stats)
		bu_log("\t%*s (%g %g %g)\n", -max_name_len, regp->reg_name, V3ARGS(v));
	}

	/* print the volume summary */
	bu_log("  objects:\n");
	bu_log("\t%*s  Volume (per view) in %s\n", -max_name_len, "Name", units[VOL]->name );
	for (i=0 ; i < tot ; i++) {
	    bu_log("\t%*s %g %g %g\n", -max_name_len,
		   obj_rpt[i].name,
		   V3ARGS(obj_rpt[i].volume));

	}

	double avg_vol = state->volume[0] + state->volume[1] + state->volume[2] / 3.0;
	bu_log("  Average total volume: %g %s\n", avg_vol / units[VOL]->val, units[VOL]->name);
    }
    if (analysis_flags & ANALYSIS_WEIGHT) {
	bu_log("--- Weight ---\n  regions:\n");
	bu_log("\t%*s  Weight (per view) %s\n", -max_name_len, "Name", units[WGT]->name );
	for( BU_LIST_FOR( regp, region, &(rtip->HeadRegion) ) )  {
	    double *w = ((struct per_region_data *)regp->reg_udata)->lenDensity;

	    RT_CK_REGION(regp);

	    /* extract the top-level object for the given region from the name
	     * ensure null termination of the copied string
	     */
	    strncpy(region_name, &regp->reg_name[1], sizeof(region_name)-1);
	    region_name[sizeof(region_name)-1] = '\0';
	    if (p = strchr(region_name, '/'))   *p = '\0';
	    region_number = find_cmd_line_obj(obj_rpt, tot, region_name);

	    /* convert linear to volume estimate */
	    VSCALE(w, w, gss);
	    VSCALE(w, w, u[WGT]); /* compute weight in user units */

	    VADD2(obj_rpt[region_number].weight, obj_rpt[region_number].weight, w);

	    if (print_per_region_stats)
		bu_log("\t%*s Weight (%g %g %g)\n", -max_name_len, regp->reg_name, V3ARGS(w));
	}
	/* print the weight summary */
	bu_log("  objects:\n");
	bu_log("\t%*s  Weight (per view) %s\n", -max_name_len, "Name", units[WGT]->name );
	for (i=0 ; i < tot ; i++) {
	    bu_log("\t%*s %g %g %g  %s\n", -max_name_len,
		   obj_rpt[i].name,
		   V3ARGS(obj_rpt[i].weight),
		   units[WGT]->name);
	}

	double avg_densityLen = state->densityLen[0] + state->densityLen[1] + state->densityLen[2] / 3.0;
	double weight = avg_densityLen * gridSpacing * gridSpacing;

	bu_log("  Average total weight: %g %s\n", weight / units[WGT]->val);
    }

    if (analysis_flags & ANALYSIS_OVERLAPS) report_overlaps();
    if (analysis_flags & ANALYSIS_ADJ_AIR) { }
    if (analysis_flags & ANALYSIS_GAP) { }
    if (analysis_flags & ANALYSIS_EXT_AIR) { }
}


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
    struct state state;
    double lim, val;
    int crv;
    int start_objs;

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


    /* Walk trees.
     * Here we identify any object trees in the database that the user
     * wants included in the ray trace.
     */
    start_objs = arg_count;

    for ( arg_count++ ; arg_count < ac ; arg_count++ )  {
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
    if (analysis_flags & ANALYSIS_BOX) {
	bu_log("bounding box: %g %g %g  %g %g %g\n", V3ARGS(rtip->mdl_min), V3ARGS(rtip->mdl_max));
    }

    VSUB2(span, rtip->mdl_max, rtip->mdl_min);

    if (options_prep(rtip, span)) return -1;

    /* initialize some stuff */
    VSETALL(state.densityLen, 0.0);
    VSETALL(state.volume, 0.0);
    state.rtip = rtip;


    if ( (analysis_flags & (ANALYSIS_ADJ_AIR|ANALYSIS_EXT_AIR)) && ! use_air ) {
	bu_bomb("Error:  Air regions discarded but air analysis requested!\nSet use_air non-zero or eliminate air analysis\n");
    }

    allocate_per_reigon_data(rtip);

    /* compute */
    do {

	compute_views(&state);

    } while (terminate_check(&state));

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
