/*                    V I E W W E I G H T . C
 * BRL-CAD
 *
 * Copyright (c) 1988-2006 United States Government as represented by
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
/** @file viewweight.c
 *
 *  Ray Tracing program RTWEIGHT bottom half.
 *
 *  This module outputs the weights and moments of a target model
 *  using density values located in ".density" or "$HOME/.density"
 *  Output is given in metric and english units, although input is
 *  assumed in lbs/cu.in.
 *
 *  Author -
 *	Jim Hunt
 *
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005
 *
 */
#include "common.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "machine.h"
#include "vmath.h"
#include "raytrace.h"
#include "rtprivate.h"

#include "db.h"  /* Yes, I know I shouldn't be peeking, put I am only
			looking to see what units we prefer... */


extern struct resource resource[];

int	use_air = 0;		/* Handling of air in librt */
int	using_mlib = 0;		/* Material routines NOT used */

/* Viewing module specific "set" variables */
struct bu_structparse view_parse[] = {
	{"",	0, (char *)0,	0,	BU_STRUCTPARSE_FUNC_NULL}
};

char usage[] = "\
Usage:  rtweight [options] model.g objects...\n\
Options:\n\
 -s #		Grid size in pixels, default 512\n\
 -g #		Grid cell width [and height] in mm\n\
 -G #		Grid cell height [and width] in mm\n\
 -a Az		Azimuth in degrees\n\
 -e Elev	Elevation in degrees\n\
 -o file.out	Weights and Moments output file\n\
 -M		Read matrix, cmds on stdin\n\
 -r 		Report verbosely mass of each region\n\
 -x #		Set librt debug flags\n\
Files:\n\
 .density, OR\n\
 $HOME/.density\n\
";

int	hit(struct application *ap, struct partition *PartHeadp, struct seg *segp);
int	miss(register struct application *ap);
int	overlap(struct application *ap, struct partition *pp, struct region *reg1, struct region *reg2);

int	noverlaps = 0;

FILE	*densityfp;
char	*densityfile;
#define	DENSITY_FILE	".density"
#define MAXMATLS	32768
fastf_t	density[MAXMATLS];
char	*dens_name[MAXMATLS];

struct datapoint {
	struct datapoint *next;
	vect_t	centroid;
	fastf_t	weight;
	fastf_t	volume;
	};

extern int     	rpt_overlap;     	/* report region verbosely */
extern fastf_t  cell_width;      	/* model space grid cell width */
extern fastf_t  cell_height;     	/* model space grid cell height */
extern FILE     *outfp;          	/* optional output file */
extern char	*outputfile;     	/* name of base of output file */
extern int	output_is_binary;	/* !0 means output is binary */

/*
 *  			V I E W _ I N I T
 *
 *  Called at the start of a run.
 *  Returns 1 if framebuffer should be opened, else 0.
 */
int
view_init(register struct application *ap, char *file, char *obj, int minus_o)
{
    register int i;
    char buf[BUFSIZ+1];
    static char null = (char) 0;
    char *curdir = getenv( "PWD" );
    char *homedir = getenv( "HOME" );
    int line;

    if( !minus_o ) {
	outfp = stdout;
	output_is_binary = 0;
    }

    for( i=1; i<MAXMATLS; i++ ) {
	density[i] = -1;
	dens_name[i] = &null;
    }

#define maxm(a, b) (a>b?a:b)
    i = maxm(strlen(curdir), strlen(homedir)) + strlen(DENSITY_FILE) + 2;
    densityfile = bu_calloc( i, 1, "densityfile");

    sprintf(densityfile, "%s/%s", curdir, DENSITY_FILE);

    if( (densityfp=fopen( densityfile, "r" )) == (FILE *) 0 ) {
	sprintf(densityfile, "%s/%s", homedir, DENSITY_FILE);
	if( (densityfp=fopen( densityfile, "r" )) == (FILE *) 0 ) {
	    perror( densityfile );
	    exit( -1 );
	}
    }

    /* Read in density in terms of grams/cm^3 */

    for (line = 1 ; feof( densityfp ) != EOF ; line++) {
	int index;
	float dens;
	register char *ptr = buf;

	i = fscanf(densityfp, "%d %f %[^\n]", &index, &dens, buf);
	if (i == EOF)
	    break;
	if (i != 3) {
	    bu_log("error parsing line %d of density file.\n%d args recognized instead of 3\n", line, i);
	    continue;
	}

	if( index > 0 && index < MAXMATLS ) {
	    density[ index ] = dens;
	    dens_name[ index ] = bu_strdup(buf);
	} else
	    bu_log( "Material index %d in \"%s\" is out of range.\n",
		    index, densityfile );
    }
    ap->a_hit = hit;
    ap->a_miss = miss;
    ap->a_overlap = overlap;
    ap->a_onehit = 0;

    return(0);		/* no framebuffer needed */
}

/* beginning of a frame */
void
view_2init(struct application *ap)
{
    register struct region *rp;
    register struct rt_i *rtip = ap->a_rt_i;

    for( BU_LIST_FOR( rp, region, &(rtip->HeadRegion) ) )  {
	rp->reg_udata = (genptr_t) NULL;
    }
}

/* end of each pixel */
void	view_pixel(struct application *ap)
{
}

/* end of each line */
void	view_eol(struct application *ap)
{
}

/* end of a frame */
void	view_end(struct application *ap)
{
    register struct datapoint *dp;
    register struct region *rp;
    register fastf_t total_weight = 0;
    fastf_t sum_x = 0, sum_y = 0, sum_z = 0;
    struct rt_i *rtip = ap->a_rt_i;
    struct db_i *dbp = ap->a_rt_i->rti_dbip;
    fastf_t conversion = 1.0;	/* Conversion factor for mass */
    fastf_t volume = 0;
    char units[128] = {0};
    char unit2[128] = {0};
    int MAX_ITEM = 0;
    time_t clock;
    struct tm *locltime;
    char *timeptr;

    /* default units */
    strcpy(units, "grams");
    strcpy(unit2, "in.");

    (void) time( &clock );
    locltime = localtime( &clock );
    timeptr = asctime( locltime );

    /* XXX this should really use bu_units_conversion() and bu_units_string() */
    if( dbp->dbi_base2local == 304.8 )  {
	/* Feet */
	strcpy( units, "grams" );
	strcpy( unit2, "ft." );
    } else if( dbp->dbi_base2local == 25.4 )  {
	/* inches */
	conversion = 0.002204623;  /* lbs./gram */
	strcpy( units, "lbs." );
	strcpy( unit2, "in." );
    } else if( dbp->dbi_base2local == 1.0 )  {
	/* mm */
	conversion = 0.001;  /* kg/gram */
	strcpy( units, "kg" );
	strcpy( unit2, "mm" );
    } else if( dbp->dbi_base2local == 1000.0 )  {
	/* km */
	conversion = 0.001;  /* kg/gram */
	strcpy( units, "kg" );
	strcpy( unit2, "m" );
    } else if( dbp->dbi_base2local == 0.1 )  {
	/* cm */
	strcpy( units, "grams" );
	strcpy( unit2, "cm." );
    } else {
	bu_log("Warning: base2mm=%g, using default of %s--%s\n",
	       dbp->dbi_base2local, units, unit2 );
    }

    if( noverlaps )
	bu_log( "%d overlap%c detected.\n\n", noverlaps,
		noverlaps==1 ? '\0' : 's' );

    fprintf( outfp, "RT Weight Program Output:\n" );
    fprintf( outfp, "\nDatabase Title: \"%s\"\n", dbp->dbi_title );
    fprintf( outfp, "Time Stamp: %s\n\nDensity Table Used:%s\n\n", timeptr, densityfile );
    fprintf( outfp, "Material  Density(g/cm^3)  Name\n" );
    { register int i;
    for( i=1; i<MAXMATLS; i++ ) {
	if( density[i] >= 0 )
	    fprintf( outfp, "%5d     %10.4f       %s\n",
		     i, density[i], dens_name[i] );
    } }

    if( rpt_overlap ) {
	/* ^L is char code for FormFeed/NewPage */
	fprintf( outfp, "Weight by region (in %s, density given in g/cm^3):\n\n", units );
	fprintf( outfp, "  Weight Matl LOS  Material Name  Density Name\n" );
	fprintf( outfp, " ------- ---- --- --------------- ------- -------------\n" );
    }
    for( BU_LIST_FOR( rp, region, &(rtip->HeadRegion) ) )  {
	register fastf_t weight = 0;
	register int l = strlen(rp->reg_name);
	register fastf_t *ptr;

	/* */
	if( MAX_ITEM < rp->reg_regionid )
	    MAX_ITEM = rp->reg_regionid;
	/* */
	for( dp = (struct datapoint *) rp->reg_udata;
	     dp != (struct datapoint *) NULL; dp = dp->next ) {
	    sum_x += dp->weight * dp->centroid[X];
	    sum_y += dp->weight * dp->centroid[Y];
	    sum_z += dp->weight * dp->centroid[Z];
	    weight += dp->weight;
	    volume += dp->volume;
	}

	weight *= conversion;
	total_weight += weight;

	ptr = (fastf_t *) bu_malloc( sizeof(fastf_t), "ptr" );
	*ptr = weight;
	rp->reg_udata = (genptr_t) ptr;

	l = l > 37 ? l-37 : 0;
	if( rpt_overlap )
	    fprintf( outfp, "%8.3f %4d %3d %-15.15s %7.4f %-37.37s\n",
		     weight, rp->reg_gmater, rp->reg_los,
		     dens_name[rp->reg_gmater],
		     density[rp->reg_gmater], &rp->reg_name[l] );
    }

    if( rpt_overlap ) {
	register int i;
	/*
	  #define MAX_ITEM 10001
	  fastf_t item_wt[MAX_ITEM];
	*/
	fastf_t *item_wt;
	MAX_ITEM++;
	item_wt = (fastf_t *) bu_malloc( sizeof(fastf_t) * (MAX_ITEM + 1), "item_wt" );
	for( i=1; i<=MAX_ITEM; i++ )
	    item_wt[i] = -1.0;
	fprintf(outfp,"Weight by item number (in %s):\n\n",units);
	fprintf(outfp,"Item  Weight  Region Names\n" );
	fprintf(outfp,"---- -------- --------------------\n" );
	for( BU_LIST_FOR( rp, region, &(rtip->HeadRegion) ) )  {
	    register int i = rp->reg_regionid;

	    if( item_wt[i] < 0 )
		item_wt[i] = *(fastf_t *)rp->reg_udata;
	    else
		item_wt[i] += *(fastf_t *)rp->reg_udata;
	}
	for( i=1; i<MAX_ITEM; i++ ) {
	    int CR = 0;
	    if( item_wt[i] < 0 )
		continue;
	    fprintf(outfp,"%4d %8.3f ", i, item_wt[i] );
	    for( BU_LIST_FOR( rp, region, &(rtip->HeadRegion) ) )  {
		if( rp->reg_regionid == i ) {
		    register int l = strlen(rp->reg_name);
		    l = l > 65 ? l-65 : 0;
		    if( CR )
			fprintf(outfp,"              ");
		    fprintf(outfp,"%-65.65s\n", &rp->reg_name[l] );
		    CR = 1;
		}
	    }
	}
    }

    volume *= (dbp->dbi_base2local*dbp->dbi_base2local*dbp->dbi_base2local);
    sum_x *= (conversion / total_weight) * dbp->dbi_base2local;
    sum_y *= (conversion / total_weight) * dbp->dbi_base2local;
    sum_z *= (conversion / total_weight) * dbp->dbi_base2local;

    fprintf( outfp, "RT Weight Program Output:\n" );
    fprintf( outfp, "\nDatabase Title: \"%s\"\n", dbp->dbi_title );
    fprintf( outfp, "Time Stamp: %s\n\n", timeptr );
    fprintf( outfp, "Total volume = %g %s^3\n\n", volume, unit2 );
    fprintf( outfp, "Centroid: X = %g %s\n", sum_x, unit2 );
    fprintf( outfp, "          Y = %g %s\n", sum_y, unit2 );
    fprintf( outfp, "          Z = %g %s\n", sum_z, unit2 );
    fprintf( outfp, "\nTotal mass = %g %s\n\n", total_weight, units );
}

void	view_setup(void) {}

/* Associated with "clean" command, before new tree is loaded  */
void	view_cleanup(void) {}

int
hit(struct application *ap, struct partition *PartHeadp, struct seg *segp)
{
    struct partition *pp;
    register struct xray *rp = &ap->a_ray;
    genptr_t addp;
    int part_count = 0;

    for( pp = PartHeadp->pt_forw; pp != PartHeadp; pp = pp->pt_forw ) {
	register struct region	*reg = pp->pt_regionp;
	register struct hit	*ihitp = pp->pt_inhit;
	register struct hit	*ohitp = pp->pt_outhit;
	register fastf_t	depth;
	register struct datapoint *dp;

	if( reg->reg_aircode )
	    continue;

	/* fill in hit points and hit distances */
	VJOIN1(ihitp->hit_point, rp->r_pt, ihitp->hit_dist, rp->r_dir );
	VJOIN1(ohitp->hit_point, rp->r_pt, ohitp->hit_dist, rp->r_dir );
	depth = ohitp->hit_dist - ihitp->hit_dist;

	part_count++;
	/* add the datapoint structure in and then calculate it
	   in parallel, the region structures are a shared resource */
	bu_semaphore_acquire( BU_SEM_SYSCALL );
#if 0
	bu_log( "\nhit: partition %d\n", part_count );
#endif
	dp = (struct datapoint *) bu_malloc( sizeof(struct datapoint), "dp");
	addp = reg->reg_udata;
	reg->reg_udata = (genptr_t) dp;
	dp->next = (struct datapoint *) addp;
	bu_semaphore_release( BU_SEM_SYSCALL );

	if( density[ reg->reg_gmater ] < 0 ) {
	    bu_log( "Material type %d used, but has no density file entry.\n", reg->reg_gmater );
	    bu_semaphore_acquire( BU_SEM_SYSCALL );
	    reg->reg_gmater = 0;
	    bu_semaphore_release( BU_SEM_SYSCALL );
	}
	else if( density[ reg->reg_gmater ] > 0 ) {
	    VBLEND2( dp->centroid, 0.5, ihitp->hit_point, 0.5, ohitp->hit_point );

	    /* Compute mass in terms of grams */
	    dp->weight = depth * density[reg->reg_gmater] *
		(fastf_t) reg->reg_los *
		cell_height * cell_height * 0.00001;
	    dp->volume = depth * cell_height * cell_width;
#if 0
	    bu_semaphore_acquire( BU_SEM_SYSCALL );
	    bu_log( "hit: reg_name=\"%s\"\n",reg->reg_name );
	    bu_log( "hit: gmater=%d, los=%d, density=%gg/cc, depth=%gmm, wt=%gg\n",
		    reg->reg_gmater, reg->reg_los, density[reg->reg_gmater],
		    depth, dp->weight );
	    bu_semaphore_release( BU_SEM_SYSCALL );
#endif
	}
    }
    return(1);	/* report hit to main routine */
}

int
miss(register struct application *ap)
{
    return(0);
}

int
overlap(struct application *ap, struct partition *pp, struct region *reg1, struct region *reg2)
{
    bu_semaphore_acquire( BU_SEM_SYSCALL );
    noverlaps++;
    bu_semaphore_release( BU_SEM_SYSCALL );
    return(0);
}

void application_init (void) {}

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
