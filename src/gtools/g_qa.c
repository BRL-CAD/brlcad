/*
 *

plot the points where overlaps start/stop

 *	Designed to be a framework for 3d sampling of the geometry volume.
 *	Options
 *	-A flags	Analysis types
 * 	-a azimuth	
 *	-e elevation
 *	-f filename
 *	-g gridspacing[,lim]
 *	-G		create groups
 *	-m min_samples
 *	-n numhits
 *	-s samples_per_smallest_bbox
 *	-S samples_per_model_bbox
 *	-P ncpu
 *	-r bits		report types
 *	-t tol_dist	overlap tolerance
 *	-V volume_tolerance
 *	-U aircode	use_air
 *	-u units	
 *	-W weight_tolerance
 *	-h		help
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
char *options = "A:a:de:f:g:Gn:P:r:S:s:t:T:U:u:W:";
extern char *optarg;
extern int optind, opterr, getopt();

/* variables set by command line flags */
char *progname = "(noname)";

#define ANALYSIS_VOLUME 1
#define ANALYSIS_WEIGHT 2
#define ANALYSIS_OVERLAPS 4
int analysis_flags = ANALYSIS_VOLUME|ANALYSIS_OVERLAPS|ANALYSIS_WEIGHT;

double azimuth_deg;
double elevation_deg;
char *densityFileName;
double gridSpacing = 50.0;
double gridSpacingLimit = 0.25;
char makeOverlapAssemblies;
int require_num_hits = 1;
int ncpu = 1;
int report_bits;
double Samples_per_model_axis = 4.0;
double Samples_per_prim_axis;
double overlap_tolerance;
double volume_tolerance;
int use_air;
char *units;
double weight_tolerance;



int debug;
#define dlog if (debug) bu_log

#if 0
int npsw = 1;
double grid_size = 50.0; /* 50.0mm grid default */
double tol = 0.5;
int rpt_overlap = 1;



double overlap_tol = 0.1;
#endif

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
    vect_t lenDensity;
    vect_t volume;
    vect_t u_dir;
    vect_t v_dir;
    struct rt_i *rtip;
};
long steps[3]; /* # of "grid_size" steps to take along each axis (X,Y,Z) to cover the face of the bounding rpp? */
vect_t span; /* How much space does the geometry span in each of X,Y,Z directions */




struct density_entry {
    long	magic;
    double	density;
    char	*name;
} *densities;
int num_densities;
#define DENSITY_MAGIC 0xaf0127





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
};
static struct overlap_list *olist=NULL;	/* root of the list */



/*
 *	U S A G E --- tell user how to invoke this program, then exit
 */
void
usage(s)
     char *s;
{
    if (s) (void)fputs(s, stderr);

    (void) fprintf(stderr, "Usage: %s [-P #processors] [-g initial_gridsize] [-t vol_tolerance] [-f density_table_file] [-d(ebug)] geom.g obj [obj...]\n",
		   progname);
    exit(1);
}

/*
 *	P A R S E _ A R G S --- Parse through command line flags
 */
int
parse_args(ac, av)
     int ac;
     char *av[];
{
    int  c;
    char *strrchr();
    char unitstr[64];
    double conv;

    if (  ! (progname=strrchr(*av, '/'))  )
	progname = *av;
    else
	++progname;

    /* Turn off getopt's error messages */
    opterr = 0;

    /* get all the option flags from the command line */
    while ((c=getopt(ac,av,options)) != EOF)
	switch (c) {
	case 'A'	: {
	    char *p;
	    analysis_flags = 0;
	    for (p = optarg; *p ; p++) {
		switch (*p) {
		case 'a' :
		    analysis_flags = ANALYSIS_VOLUME|ANALYSIS_WEIGHT|ANALYSIS_OVERLAPS;
		    break;
		case 'o' :
		    analysis_flags |= ANALYSIS_OVERLAPS;
		    break;
		case 'w' :
		    analysis_flags |= ANALYSIS_WEIGHT;
		    break;
		case 'v' :
		    analysis_flags |= ANALYSIS_VOLUME;
		    break;
		}
	    }
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

	case 'g'	: {
#define LEN_UNITS 32
	    int i, j;
	    char unit1[LEN_UNITS];
	    char unit2[LEN_UNITS];
	    double a, b;
	    char *p;

	    /* NOTE:  The 32 in this string must match LEN_UNITS  */
	    i = j = 0;

	    if (p = strchr(optarg, ',')) {
		*p++ = '\0';
	    }

	    bu_log("\"%s\"  \"%s\"\n", optarg, p);

	    i = sscanf(optarg, "%lg%s", &a, unit1);

	    if (i > 0) {
		if (a <= 0.0) {
		    bu_log("gridSpacing must be a positive value > 0.0, not \"%s\"\n", unit1);
		    break;
		}
		if (i > 1) {
		    conv = bu_units_conversion(unit1);
		    if (conv != 0.0)
			gridSpacing = a * conv;
		    else {
			bu_log("bad units specification \"%s\" for %s\n", unit1, optarg);
			gridSpacing = a;
		    }
		} else {
		    gridSpacing = a;
		}

	    }


	    if (p) {
		j = sscanf(p, "%lg%s", &b, unit2);
		if (j > 0)
		    if (b <= 0.0) {
			bu_log("gridSpacing must be a positive value > 0.0, not \"%s\"\n", unit1);
			break;
		    }
		    if (j > 1) {
			conv = bu_units_conversion(unit2);
			if (conv != 0.0)
			    gridSpacingLimit = b * conv;
			else {
			    bu_log("bad units specification \"%s\" for %s\n", unit2, p);
			    gridSpacingLimit = b;
			}
		    } else {
			gridSpacingLimit = b;
		    }

		}
	    }
	    bu_log("spacing:%g limit:%g\n", gridSpacing, gridSpacingLimit);
	    break;
	case 'G'	:
	    makeOverlapAssemblies = 1;
	    break;
	case 'n'	:
	    if (sscanf(optarg, "%d", &c) != 1 || c < 0) {
		bu_log("num_hits must be >= 0, not \"%s\"\n", optarg);
		break;
	    }
	    require_num_hits = c;
	    break;

	case 'P'	:
	    /* cannot ask for more cpu's than the machine has */
	    if ((c=atoi(optarg)) > 0 && ncpu > c) ncpu = c;
	    break;	
	case 'S'	:
	    if (sscanf(optarg, "%g", &conv) != 1 || conv <= 0.0) {
		bu_log("error in specifying minimum samples per model axis: \"%s\"\n", optarg);
		break;
	    }
	    Samples_per_model_axis = conv;
	    break;
	case 's'	:
	    if (sscanf(optarg, "%g", &conv) != 1 || conv <= 0.0) {
		bu_log("error in specifying minimum samples per primitive axis: \"%s\"\n", optarg);
		break;
	    }
	    Samples_per_prim_axis = conv;
	    bu_log("option -s samples_per_axis_min not implemented\n");
	    break;
	case 't'	: 
	    if (sscanf(optarg, "%lg", &conv) != 1 || conv <= 0.0 ) {
		bu_log("error in overlap tolerance distance \"%s\"\n", optarg);
		break;
	    }
	    overlap_tolerance = conv;
	    break;
	case 'V'	: 
	    if (sscanf(optarg, "%lg", &conv) != 1 || conv <= 0.0 ) {
		bu_log("error in volume tolerance \"%s\"\n", optarg);
		break;
	    }
	    volume_tolerance = conv;
	    break;
	case 'W'	: 
	    if (sscanf(optarg, "%lg", &conv) != 1 || conv <= 0.0 ) {
		bu_log("error in weight tolerance \"%s\"\n", optarg);
		break;
	    }
	    weight_tolerance = conv;
	    break;

	case 'U'	: use_air = atoi(optarg); break;
	case 'u'	: units = optarg; break;
	case '?'	:
	case 'h'	:
	default		: usage("Bad or help flag specified\n"); break;
	}

    return(optind);
}




/*
 *			O V E R L A P
 *
 *  Write end points of partition to the standard output.
 *  If this routine return !0, this partition will be dropped
 *  from the boolean evaluation.
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
		bu_log("\nOVERLAP %d: %s\nOVERLAP %d: %s\nOVERLAP %d: depth %gmm\nOVERLAP %d: in_hit_point (%g,%g,%g) mm\nOVERLAP %d: out_hit_point (%g,%g,%g) mm\n------------------------------------------------------------\n",
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
			 * we increase the counter and return 
			 */
			if( (strcmp(reg1->reg_name,op->reg1) == 0) && (strcmp(reg2->reg_name,op->reg2) == 0) ) {
				op->count++;
				bu_semaphore_release( BU_SEM_SYSCALL );
				bu_free( (char *) new_op, "overlap list");
				return	0;	/* already on list */
			} 
		}
		
		for( op=olist; op; prev_ol=op, op=op->next ) {
			/* if this pair was seen in reverse, decrease the unique counter */
			if ( (strcmp(reg1->reg_name, op->reg2) == 0) && (strcmp(reg2->reg_name, op->reg1) == 0) ) {
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


void
multioverlap(struct application *ap, struct partition *pp, struct bu_ptbl *regiontable, struct partition *InputHdp)
{
    int n_regions;

    RT_CK_AP(ap);
    RT_CK_PARTITION(pp);
    BU_CK_PTBL(regiontable);
    RT_CK_PT_HD(InputHdp);
    struct region **reg;

    /* just blow em away for now */
    for( BU_PTBL_FOR( reg, (struct region **), regiontable ) )  {
	if (*reg == REGION_NULL) continue;
	
	
	*reg = REGION_NULL;
    }
}



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
hit(register struct application *ap, struct partition *PartHeadp, struct seg *segs)
{
    /* see raytrace.h for all of these guys */
    register struct partition *pp;
    register struct hit *hitp;
    register struct soltab *stp;
    struct curvature cur;
    point_t		pt;
    vect_t		inormal;
    vect_t		onormal;


    /* examine each partition until we get back to the head */
    for( pp=PartHeadp->pt_forw; pp != PartHeadp; pp = pp->pt_forw )  {
	double dist;
	struct density_entry *de;

	/* inhit info */
	dist = pp->pt_outhit->hit_dist - pp->pt_inhit->hit_dist;

	/* try to factor in the density of this object */
	if (pp->pt_regionp->reg_gmater >= num_densities) {
	    bu_log("density index %d on region %s is outside of range of table [1..%d]\nSet GIFTmater on region or add entry to density table\n",
		   pp->pt_regionp->reg_gmater,
		   pp->pt_regionp->reg_name,
		   num_densities);
	    bu_bomb("");
	} else {
	    de = &densities[pp->pt_regionp->reg_gmater];
	    if (de->magic == DENSITY_MAGIC) {
		ap->a_color[0] += de->density * dist;
		ap->a_color[1] += dist;
	    } else {
		bu_log("density index %d from region %s is not set.\nAdd entry to density table\n",
		       pp->pt_regionp->reg_gmater, pp->pt_regionp->reg_name);
		bu_bomb("");
	    }
	}
	((unsigned)pp->pt_regionp->reg_udata)++;
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
miss(register struct application *ap)
{
#if 0
    bu_log("missed\n");
#endif
    return(0);
}


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
    //    ap.a_multioverlap = multioverlap;
    ap.a_overlap = overlap;
    ap.a_resource = &resource[cpu];
    ap.a_color[0] = 0.0; /* really the cumulative length*density */
    ap.a_color[1] = 0.0; /* really the cumulative length*density */

    ap.a_ray.r_dir[state->u_axis] = ap.a_ray.r_dir[state->v_axis] = 0.0;
    ap.a_ray.r_dir[state->i_axis] = 1.0;

    while (1) {
	/* get a row to work on */
	bu_semaphore_acquire(SEM_WORK);
	/* bu_log("stateV: %d  steps %d\n", state->v, steps[state->v_axis]); */
	if (state->v >= steps[state->v_axis]) {
	    state->lenDensity[state->i_axis] += ap.a_color[0]; /* add our length*density value */
	    state->volume[state->i_axis] += ap.a_color[1]; /* add our length*density value */
	    bu_semaphore_release(SEM_WORK);
	    return;
	}
	v = state->v++;
	bu_semaphore_release(SEM_WORK);

	v_coord = gridSpacing * 0.5 + v * gridSpacing; 

	state->lenDensity[state->i_axis] = 0.0;
	state->volume[state->i_axis] = 0.0;


	if (state->lenDensity[state->i_axis] == 0.0 || (v&1) == 0) {
	    /* shoot all the rays in this row */
	    for (u=0 ; u < steps[state->u_axis]; u++) {
		ap.a_ray.r_pt[state->u_axis] = ap.a_rt_i->mdl_min[state->u_axis] + u * gridSpacing + 0.5 * gridSpacing;
		ap.a_ray.r_pt[state->v_axis] = ap.a_rt_i->mdl_min[state->v_axis] + v * gridSpacing + 0.5 * gridSpacing;
		ap.a_ray.r_pt[state->i_axis] = ap.a_rt_i->mdl_min[state->i_axis];

		/* bu_log("%g %g %g -> %g %g %g\n", V3ARGS(ap.a_ray.r_pt), V3ARGS(ap.a_ray.r_dir));*/
		(void)rt_shootray( &ap );
	    }
	} else {
	    /* shoot only the rays we need to on this row */
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
void
clear_region_values(struct rt_i *rtip)
{
    struct region *regp;
    RT_CK_RTI(rtip);
    for( BU_LIST_FOR( regp, region, &(rtip->HeadRegion) ) )  {
	RT_CK_REGION(regp);
	regp->reg_udata = (genptr_t)0;	
    }
}

int
check_region_values(struct rt_i *rtip)
{
    struct region *regp;
    int r = 0;
    RT_CK_RTI(rtip);
    for( BU_LIST_FOR( regp, region, &(rtip->HeadRegion) ) )  {
	RT_CK_REGION(regp);
	if (regp->reg_udata == (genptr_t)0) {
	    bu_log("%s was not hit\n", regp->reg_name);
	    r = 1;
	} else if (((unsigned)regp->reg_udata) < require_num_hits) {
	    dlog("%s hit only %u times (< %u)\n", regp->reg_name, (unsigned)regp->reg_udata, require_num_hits);
	    r = 1;
	}
    }

    return r;
}

void
report_results(struct state *state)
{
    state->volume[state->i_axis] *= gridSpacing*gridSpacing;

    bu_log("\tvolume %lg cu mm  Weight %lg\n",
	   state->volume[state->i_axis],
	   state->lenDensity[state->i_axis]*gridSpacing*gridSpacing);
}

void
report_overlaps()
{
    struct overlap_list *op, *prev_ol;

    if (olist == (struct overlap_list *)NULL) {
	bu_log("No overlaps\n");
	return;
    }

    bu_log("Overlaps\ntotal encountered:%d  total pairs:%d  unique pairs:%d\n", noverlaps, overlap_count, unique_overlap_count);
    for( op=olist; op; prev_ol=op, op=op->next ) {
	bu_log("%s %s\n", op->reg1, op->reg2);
    }
}



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
	densities[idx].density = density;
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
/* 
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
 */
int
get_densities( struct rt_i * rtip)
{
    struct directory *dp;
}

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
	bu_log("Grid spacing %gmm is larger than model bounding box axis\n", gridSpacing);
	bu_log("Adjusted to %gmm to get %g samples per model axis\n", newGridSpacing, Samples_per_model_axis);
	gridSpacing = newGridSpacing;
    }

    return 0;
}

void 
compute_views(struct state *state)
{
    int axis;
    int num_axes = 3;
    double inv_spacing = 1.0/gridSpacing;

    VSCALE(steps, span, inv_spacing);

    bu_log("grid spacing %g mm  %d x %d x %d steps\n", gridSpacing, V3ARGS(steps));

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
	    report_results(state);
	}



}

/*
 *	t e r m i n a t e _ c h e c k
 */

int 
terminate_check(struct state *state)
{
    double low, hi, val, delta;
    int i;
    int axis_count = 3;

	if (volume_tolerance) {
	    /* find the range of values for volume */
	    low = hi = 0.0;
	    val = state->volume[0];
	    for (i=1 ; i < axis_count ; i++) {
		delta = val - state->volume[i];

		if (delta < low) low = delta;
		if (delta > hi) hi = delta;
	    }
	    delta = hi - low;

	    if (delta < volume_tolerance)
		return 0;
	}
	if (weight_tolerance) {

	    low = hi = 0.0;
	    val = state->lenDensity[0];
	    for (i=1 ; i < axis_count ; i++) {
		delta = val - state->lenDensity[i];

		if (delta < low) low = delta;
		if (delta > hi) hi = delta;
	    }
	    delta = hi - low;

	    if (delta < weight_tolerance)
		return 0;

	}

	/* refine the gridSpacing and try again */
	gridSpacing *= 0.5;
	if (gridSpacing < gridSpacingLimit) {
	    bu_log("grid spacing refined to %g which is below lower limit %g\n", gridSpacingLimit, gridSpacing);
	    return 0;
	}

	return 1;
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

    ncpu = bu_avail_cpus();

    /* parse command line arguments */
    arg_count = parse_args(ac, av);
	
    if ((ac-arg_count) < 2) {
	usage("oops\n");
    }

    bu_semaphore_init(RT_SEM_LAST+2);

    rt_init_resource( &rt_uniresource, ncpu, NULL );
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

    /* Walk trees.
     * Here we identify any object trees in the database that the user
     * wants included in the ray trace.
     */
    for ( arg_count++ ; arg_count < ac ; arg_count++ )  {
	if( rt_gettree(rtip, av[arg_count]) < 0 )
	    fprintf(stderr,"rt_gettree(%s) FAILED\n", av[arg_count]);
    }

    /*
     *  Initialize all the per-CPU memory resources.
     *  The number of processors can change at runtime, init them all.
     */
    for( i=0; i < ncpu; i++ )  {
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
    dlog("bounds %g %g %g  %g %g %g\n", V3ARGS(rtip->mdl_min), V3ARGS(rtip->mdl_max));

    VSUB2(span, rtip->mdl_max, rtip->mdl_min);

    if (options_prep(rtip, span)) return -1;
    bu_log("flags %0x\n", analysis_flags);

    /* initialize some stuff */
    VSETALL(state.lenDensity, 0.0);
    VSETALL(state.volume, 0.0);
    state.rtip = rtip;

    /* compute */
    do {

	compute_views(&state);

    } while (terminate_check(&state));
    report_overlaps();

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
