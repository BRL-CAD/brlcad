/*
 *			V I E W W E I G H T
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
 *  Copyright Notice -
 *	This software is Copyright (C) 1988 by the United States Army.
 *	All rights reserved.
 */

#include "conf.h"

#include <stdio.h>
#include <time.h>

#include "machine.h"
#include "vmath.h"
#include "raytrace.h"
#include "./rdebug.h"

#include "db.h"  /* Yes, I know I shouldn't be peeking, put I am only
			looking to see what units we prefer... */

extern struct resource resource[];

int	use_air = 0;		/* Handling of air in librt */
int	using_mlib = 0;		/* Material routines NOT used */

/* Viewing module specific "set" variables */
struct bu_structparse view_parse[] = {
	"",	0, (char *)0,	0,	FUNC_NULL
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

int	hit();
int	miss();
int	overlap();

int	noverlaps = 0;

FILE	*densityfp;
char	*densityfile;
#define	DENSITY_FILE	".density"
#define MAXMATLS	99
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
view_init( ap, file, obj, minus_o )
register struct application *ap;
char *file, *obj;
	{
	register int i;
	char buf[BUFSIZ+1];
	static char null = (char) 0;

	if( !minus_o ) {
		outfp = stdout;
		output_is_binary = 0;
		}

	for( i=1; i<MAXMATLS; i++ ) {
		density[i] = -1;
		dens_name[i] = &null;
		}

	if( (densityfp=fopen( densityfile=DENSITY_FILE, "r" )) == (FILE *) 0 ) {
		char *homedir = getenv( "HOME" );
		densityfile = (char *)malloc( strlen(homedir) + strlen(DENSITY_FILE) + 1 );
		strcpy( densityfile, homedir );
		strcat( densityfile, "/" );
		strcat( densityfile, DENSITY_FILE );
		if( (densityfp=fopen( densityfile, "r" )) == (FILE *) 0 ) {
			perror( densityfile );
			exit( -1 );
			}
		}

	/* Read in density in terms of grams/cm^3 */
	while( feof( densityfp ) != EOF ) {
		int index;
		float dens;
		register char *ptr = buf;
		if( fscanf( densityfp, "%d %f", &index, &dens ) == EOF )
			break;
		(void)fgets( buf, BUFSIZ, densityfp );
		if( index > 0 && index < MAXMATLS ) {
			while( *ptr == ' ' || *ptr == (char) 9 )
				ptr++;
			density[ index ] = dens;
			dens_name[ index ] = malloc( strlen( ptr ) + 1 );
			strcpy( dens_name[index], ptr );
			for( ptr=dens_name[index]; *ptr; ptr++ )
				if( *ptr==(char)13 || *ptr==(char)10 ) {
					*ptr = (char) 0;
					break;
					}
			}
		else
			rt_log( "Material index %d in \"%s\" is out of range.\n", index, densityfile );
		}
	ap->a_hit = hit;
	ap->a_miss = miss;
	ap->a_overlap = overlap;
	ap->a_onehit = 0;

	return(0);		/* no framebuffer needed */
}

/* beginning of a frame */
void
view_2init( ap )
struct application *ap;
	{
	register struct region *rp;
	register struct rt_i *rtip = ap->a_rt_i;
	
	for( rp = rtip->HeadRegion; rp != (struct region *) NULL;
			rp = rp->reg_forw ) {
		rp->reg_udata = (genptr_t) NULL;
		}
	}

/* end of each pixel */
void	view_pixel(ap)
struct application *ap; { }

/* end of each line */
void	view_eol(ap)
struct application *ap; { }

/* end of a frame */
void	view_end( ap )
struct application *ap;
	{
	register struct datapoint *dp;
	register struct region *rp;
	register fastf_t total_weight = 0;
	fastf_t sum_x = 0, sum_y = 0, sum_z = 0;
	struct rt_i *rtip = ap->a_rt_i;
	struct db_i *dbp = ap->a_rt_i->rti_dbip;
	fastf_t conversion = 1.0;	/* Conversion factor for mass */
	fastf_t volume = 0;
	static char units[] = { "grams" };
	static char unit2[] = { "in." };
	int MAX_ITEM = 0;
	time_t clock;
	struct tm *locltime;
	char *timeptr;

	(void) time( &clock );
	locltime = localtime( &clock );
	timeptr = asctime( locltime );
	
        switch( dbp->dbi_localunit ) {
                case ID_FT_UNIT:
			strcpy( unit2, "ft." );
                case ID_IN_UNIT:
                        conversion = 0.002204623;  /* lbs./gram */
			strcpy( units, "lbs." );
                case ID_NO_UNIT:
			break;
                case ID_MM_UNIT:
			conversion = 0.001;  /* kg/gram */
			strcpy( units, "kg" );
			strcpy( unit2, "mm" );
			break;
		case ID_M_UNIT:
			conversion = 0.001;  /* kg/gram */
			strcpy( units, "kg" );
			strcpy( unit2, "m" );
                case ID_CM_UNIT:
			break;
		}
	
	if( noverlaps )
		rt_log( "%d overlap%c detected.\n\n", noverlaps,
			noverlaps==1 ? (char) 0 : 's' );

	fprintf( outfp, "RT Check Program Output:\n" );
	fprintf( outfp, "\nDatabase Title: \"%s\"\n", dbp->dbi_title );
	fprintf( outfp, "Time Stamp: %s\n\nDensity Table Used:\n\n", timeptr );
	fprintf( outfp, "Material  Density(g/cm^3)  Name\n" );
	{ register int i;
	for( i=1; i<MAXMATLS; i++ ) {
		if( density[i] >= 0 )
			fprintf( outfp, "%5d     %10.4f       %s\n",
				i, density[i], dens_name[i] );
		} }

	if( rpt_overlap ) {
		fprintf( outfp, "Weight by region (in %s, density given in g/cm^3):\n\n", units );
		fprintf( outfp, "  Weight Matl LOS  Material Name  Density Name\n" );
		fprintf( outfp, " ------- ---- --- --------------- ------- -------------\n" );
		}
	for( rp = rtip->HeadRegion; rp != (struct region *) NULL;
			rp = rp->reg_forw ) {
		register fastf_t weight = 0;
		register l = strlen(rp->reg_name);
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

		ptr = (fastf_t *) malloc( sizeof(fastf_t) );
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
		item_wt = (fastf_t *) malloc( sizeof(fastf_t) * MAX_ITEM );
		for( i=1; i<=MAX_ITEM; i++ )
			item_wt[i] = -1.0;
		fprintf(outfp,"Weight by item number (in %s):\n\n",units);
		fprintf(outfp,"Item  Weight  Region Names\n" );
		fprintf(outfp,"---- -------- --------------------\n" );
		for( rp = rtip->HeadRegion; rp != (struct region *) NULL;
				rp = rp->reg_forw ) {
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
			for( rp = rtip->HeadRegion;
					rp != (struct region *) NULL;
					rp = rp->reg_forw ) {
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
	
	fprintf( outfp, "RT Check Program Output:\n" );
	fprintf( outfp, "\nDatabase Title: \"%s\"\n", dbp->dbi_title );
	fprintf( outfp, "Time Stamp: %s\n\n", timeptr );
	fprintf( outfp, "Total volume = %g %s^3\n\n", volume, unit2 );
	fprintf( outfp, "Centroid: X = %g %s\n", sum_x, unit2 );
	fprintf( outfp, "          Y = %g %s\n", sum_y, unit2 );
	fprintf( outfp, "          Z = %g %s\n", sum_z, unit2 );
	fprintf( outfp, "\nTotal mass = %g %s\n\n", total_weight, units );
	}

void	view_setup() {}

/* Associated with "clean" command, before new tree is loaded  */
void	view_cleanup() {}

int
hit( ap, PartHeadp )
struct application *ap;
struct partition *PartHeadp;
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
		rt_log( "\nhit: partition %d\n", part_count );
#endif
		dp = (struct datapoint *) malloc( sizeof(struct datapoint));
		addp = reg->reg_udata;
		reg->reg_udata = (genptr_t) dp;
		dp->next = (struct datapoint *) addp;
        	bu_semaphore_release( BU_SEM_SYSCALL );

		if( density[ reg->reg_gmater ] < 0 ) {
			rt_log( "Material type %d used, but has no density file entry.\n", reg->reg_gmater );
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
			rt_log( "hit: reg_name=\"%s\"\n",reg->reg_name );
			rt_log( "hit: gmater=%d, los=%d, density=%gg/cc, depth=%gmm, wt=%gg\n",
			reg->reg_gmater, reg->reg_los, density[reg->reg_gmater],
			depth, dp->weight );
        		bu_semaphore_release( BU_SEM_SYSCALL );
#endif
			}
		}
	return(1);	/* report hit to main routine */
	}

int
miss( ap )
register struct application *ap;
	{
	return(0);
	}

int
overlap( ap, pp, reg1, reg2 )
struct application      *ap;
struct partition        *pp;
struct region           *reg1;
struct region           *reg2;
	{
	bu_semaphore_acquire( BU_SEM_SYSCALL );
	noverlaps++;
        bu_semaphore_release( BU_SEM_SYSCALL );
	return(0);
	}

void application_init () {}
