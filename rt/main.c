/*
 *			R T
 *
 * Ray Tracing program
 *
 * Author -
 *	Michael John Muuss
 *
 *	U. S. Army Ballistic Research Laboratory
 *	March 27, 1984
 *
 * $Revision$
 */
#ifndef lint
static char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include <stdio.h>
#include "vmath.h"
#include "raytrace.h"
#include "debug.h"

static char usage[] = "\
Usage:  rt [options] model.vg object [objects]\n\
Options:  -f[#] -x# -aAz -eElev -A%Ambient -l# [-o model.pix]\n";

/* Used for autosizing */
static vect_t base;		/* view position of model_min */
static fastf_t deltas;		/* distance between rays */
extern double atof();

/***** Variables shared with viewing model *** */
double AmbientIntensity = 0.1;	/* Ambient light intensity */
vect_t l0vec;			/* 0th light vector */
vect_t l1vec;			/* 1st light vector */
vect_t l2vec;			/* 2st light vector */
double azimuth, elevation;
int outfd;		/* fd of optional pixel output file */
FILE *outfp;		/* used to write .PP files */
int lightmodel;		/* Select lighting model */
int npts;		/* # of points to shoot: x,y */
/***** end of sharing with viewing model *****/

/*
 *			M A I N
 */
main(argc, argv)
int argc;
char **argv;
{
	static struct application ap;
	static mat_t view2model;
	static mat_t model2view;
	static vect_t tempdir;
	static int matflag = 0;		/* read matrix from stdin */
	static double utime;

	npts = 512;
	azimuth = -35.0;			/* GIFT defaults */
	elevation = -25.0;

	if( argc < 1 )  {
		fprintf(stderr, usage);
		exit(1);
	}

	argc--; argv++;
	while( argv[0][0] == '-' )  {
		switch( argv[0][1] )  {
		case 'M':
			matflag = 1;
			break;
		case 'A':
			AmbientIntensity = atof( &argv[0][2] );
			break;
		case 'x':
			sscanf( &argv[0][2], "%x", &debug );
			fprintf(stderr,"debug=x%x\n", debug);
			break;
		case 'f':
			/* "Fast" -- just a few pixels.  Or, arg's worth */
			npts = atoi( &argv[0][2] );
			if( npts < 2 || npts > 1024 )  {
				npts = 50;
			}
			break;
		case 'a':
			/* Set azimuth */
			azimuth = atof( &argv[0][2] );
			matflag = 0;
			break;
		case 'e':
			/* Set elevation */
			elevation = atof( &argv[0][2] );
			matflag = 0;
			break;
		case 'l':
			/* Select lighting model # */
			lightmodel = atoi( &argv[0][2] );
			break;
		case 'o':
			/* Output pixel file name */
			if( (outfd = creat( argv[1], 0444 )) <= 0 )  {
				perror( argv[1] );
				exit(10);
			}
			argc--; argv++;
			break;
		default:
			fprintf(stderr,"rt:  Option '%c' unknown\n", argv[0][1]);
			fprintf(stderr, usage);
			break;
		}
		argc--; argv++;
	}

	if( argc < 2 )  {
		fprintf(stderr, usage);
		exit(2);
	}

	/* initialize application based upon lightmodel # */
	view_init( &ap );
	ap.a_init( &ap, argv[0], argv[1] );

	timer_prep();		/* Start timing preparations */

	/* Build directory of GED database */
	dir_build( argv[0] );
	argc--; argv++;

	/* Load the desired portion of the model */
	while( argc > 0 )  {
		get_tree(argv[0]);
		argc--; argv++;
	}
	(void)timer_print("PREP");

	if( HeadSolid == SOLTAB_NULL )  {
		fprintf(stderr, "No solids remain after prep, exiting.\n");
		exit(11);
	}
	fprintf(stderr,"shooting at %d solids in %d regions\n",
		nsolids, nregions );

	/* Set up the online display and/or the display file */
	if( !(debug&DEBUG_QUICKIE) )
		dev_setup(npts);

	fprintf(stderr,"model X(%f,%f), Y(%f,%f), Z(%f,%f)\n",
		model_min[X], model_max[X],
		model_min[Y], model_max[Y],
		model_min[Z], model_max[Z] );

	if( !matflag )  {
		/*
		 * Unrotated view is TOP.
		 * Rotation of 270,0,270 takes us to a front view.
		 * Standard GIFT view is -35 azimuth, -25 elevation off front.
		 */
		mat_idn( model2view );
		mat_angles( model2view, 270.0-elevation, 0.0, 270.0+azimuth );
		fprintf(stderr,"Viewing %f azimuth, %f elevation off of front view\n",
			azimuth, elevation);
		mat_inv( view2model, model2view );

		if( !(debug&DEBUG_QUICKIE) )  {
			autosize( model2view, npts );
		VSET( tempdir, 0, 0, -1 );
		} else {
			base[X] = -3;
			base[Y] = -3;
			base[Z] = -10;
			deltas = 1;
			npts = 8;
			VSET( tempdir, 0, 0, 1 );
		}
	}  else  {
		static int i;

		/* Visible part is from -1 to +1 */
		for( i=0; i < 16; i++ )
			scanf( "%f", &model2view[i] );

		base[X] = base[Y] = base[Z] = -1;
		deltas = 2.0 / ((double)npts);
		mat_inv( view2model, model2view );
		VSET( tempdir, 0, 0, -1 );
	}

	MAT4X3VEC( ap.a_ray.r_dir, view2model, tempdir );
	VUNITIZE( ap.a_ray.r_dir );

	MAT4X3PNT( ap.a_ray.r_pt, view2model, base );

	fprintf(stderr,"Ambient light at %f%%\n", AmbientIntensity * 100.0 );

	/* Determine the Light location(s) in view space */
	/* 0:  Blue, at left edge, 1/2 high */
	tempdir[0] = 2 * (base[X]);
	tempdir[1] = (2/2) * (base[Y]);
	tempdir[2] = 2 * (base[Z] + npts*deltas);
	MAT4X3VEC( l0vec, view2model, tempdir );
	VUNITIZE(l0vec);

	/* 1: Red, at right edge, 1/2 high */
	tempdir[0] = 2 * (base[X] + npts*deltas);
	tempdir[1] = (2/2) * (base[Y]);
	tempdir[2] = 2 * (base[Z] + npts*deltas);
	MAT4X3VEC( l1vec, view2model, tempdir );
	VUNITIZE(l1vec);

	/* 2:  Grey, behind, and overhead */
	tempdir[0] = 2 * (base[X] + (npts/2)*deltas);
	tempdir[1] = 2 * (base[Y] + npts*deltas);
	tempdir[2] = 2 * (base[Z] + (npts/2)*deltas);
	MAT4X3VEC( l2vec, view2model, tempdir );
	VUNITIZE(l2vec);

	timer_prep();	/* start timing actual run */

	fflush(stderr);

	for( ap.a_y = npts-1; ap.a_y >= 0; ap.a_y--)  {
		for( ap.a_x = 0; ap.a_x < npts; ap.a_x++)  {
			VSET( tempdir,
				base[X] + ap.a_x * deltas,
				base[Y] + (npts-ap.a_y-1) * deltas,
				base[Z] +  2*npts*deltas );
			MAT4X3PNT( ap.a_ray.r_pt, view2model, tempdir );

			shootray( &ap );
		}
		ap.a_eol( &ap );	/* End of scan line */
	}
	ap.a_end( &ap );		/* End of application */

	/*
	 *  All done.  Display run statistics.
	 */
	utime = timer_print("SHOT");
	fprintf(stderr,"ft_shot(): %ld = %ld hits + %ld miss\n",
		nshots, nhits, nmiss );
	fprintf(stderr,"pruned:  %ld model RPP, %ld sub-tree RPP, %ld solid RPP\n",
		nmiss_model, nmiss_tree, nmiss_solid );
	fprintf(stderr,"pruning efficiency %.1f%%\n",
		((double)nhits*100.0)/nshots );
	fprintf(stderr,"%d output rays in %f sec = %f rays/sec\n",
		npts*npts, utime, (double)(npts*npts/utime) );
	return(0);
}
#ifdef never
/*
 *			A U T O S I Z E
 */
autosize( m2v, n )
register matp_t m2v;
int n;
{
	register struct soltab *stp;
	vect_t top;
	double f;

	MAT4X3PNT( base, m2v, model_min );
	MAT4X3PNT( top, m2v, model_max );

	deltas = (top[X]-base[X])/n;
	f = (top[Y]-base[Y])/n;
	if( f > deltas )  deltas = f;
	fprintf(stderr,"view  X(%f,%f), Y(%f,%f), Z(%f,%f)\n",
		base[X], top[X],
		base[Y], top[Y],
		base[Z], top[Z] );
	fprintf(stderr,"Deltas=%f (units between rays)\n", deltas );
}
#else

/*
 *			A U T O S I Z E
 */
autosize( m2v, n )
matp_t m2v;
int n;
{
	register struct soltab *stp;
	static fastf_t xmin, xmax;
	static fastf_t ymin, ymax;
	static fastf_t zmin, zmax;
	static vect_t xlated;

	/* init maxima and minima */
	xmax = ymax = zmax = -100000000.0;
	xmin = ymin = zmin =  100000000.0;

	for( stp=HeadSolid; stp != 0; stp=stp->st_forw ) {
		FAST fastf_t rad;

		rad = sqrt(stp->st_radsq);
		MAT4X3PNT( xlated, m2v, stp->st_center );
#define MIN(v,t) {FAST fastf_t rt; rt=(t); if(rt<v) v = rt;}
#define MAX(v,t) {FAST fastf_t rt; rt=(t); if(rt>v) v = rt;}
		MIN( xmin, xlated[0]-rad );
		MAX( xmax, xlated[0]+rad );
		MIN( ymin, xlated[1]-rad );
		MAX( ymax, xlated[1]+rad );
		MIN( zmin, xlated[2]-rad );
		MAX( zmax, xlated[2]+rad );
	}

	/* Provide a slight border */
	xmin -= xmin * 0.03;
	ymin -= ymin * 0.03;
	zmin -= zmin * 0.03;
	xmax *= 1.03;
	ymax *= 1.03;
	zmax *= 1.03;

	VSET( base, xmin, ymin, zmin );

	deltas = (xmax-xmin)/n;
	MAX( deltas, (ymax-ymin)/n );
	fprintf(stderr,"view X(%f,%f), Y(%f,%f), Z(%f,%f)\n",
		xmin, xmax, ymin, ymax, zmin, zmax );
	fprintf(stderr,"Deltas=%f (units between rays)\n", deltas );
}
#endif
