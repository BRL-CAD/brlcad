/*
 *			R T
 *
 *  Demonstration Ray Tracing main program, using RT library.
 *  Invoked by MGED for quick pictures.
 *  Is linked with each of three "back ends" (view.c, viewpp.c, viewray.c)
 *  to produce three executable programs:  rt, rtpp, rtray.
 *
 *  Author -
 *	Michael John Muuss
 *
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005
 *  
 *  Copyright Notice -
 *	This software is Copyright (C) 1985 by the United States Army.
 *	All rights reserved.
 */
#ifndef lint
static char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include <stdio.h>
#include "../h/machine.h"
#include "../h/vmath.h"
#include "raytrace.h"
#include "debug.h"

#ifdef HEP
# include <synch.h>
#endif

extern char usage[];

extern double atof();

/***** Variables shared with viewing model *** */
double AmbientIntensity = 0.4;	/* Ambient light intensity */
double azimuth, elevation;
int lightmodel;		/* Select lighting model */
mat_t view2model;
mat_t model2view;
/***** end of sharing with viewing model *****/

/***** variables shared with worker() */
static int	perspective=0;	/* perspective view -vs- parallel view */
static vect_t	dx_model;	/* view delta-X as model-space vector */
static vect_t	dy_model;	/* view delta-Y as model-space vector */
static point_t	eye_model;	/* model-space location of eye */
static point_t	viewbase_model;	/* model-space location of viewplane corner */
static int npts;	/* # of points to shoot: x,y */
int npsw = 1;		/* HEP: number of worker PSWs to run */
void worker();
int work_word;		/* semaphored (x<<16)|y */
#ifdef HEP
char *scanbuf;		/*** Output buffering, for parallelism */
#endif
/***** end variables shared with worker() */

/*
 *			M A I N
 */
main(argc, argv)
int argc;
char **argv;
{
	static struct application ap;
	static vect_t temp;
	static int matflag = 0;		/* read matrix from stdin */
	static double utime;
	char *title_file, *title_obj;	/* name of file and first object */
	static float	zoomout=1;	/* >0 zoom out, 0..1 zoom in */
	static int outfd;		/* fd of optional pixel output file */
	register int x,y;
	char outbuf[132];

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
		case 'p':
			perspective = 1;
			if( argv[0][2] != '\0' )
				zoomout = atof( &argv[0][2] );
			if( zoomout <= 0 )  zoomout = 1;
			break;
		case 'P':
			/* Number of parallel workers */
			npsw = atoi( &argv[0][2] );
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
	RES_RELEASE( &res_pt );
	RES_RELEASE( &res_seg );
	RES_RELEASE( &res_malloc );
#ifdef HEP
	scanbuf = vmalloc( npts*npts*3 + sizeof(long), "scanbuf" );
#endif

	title_file = argv[0];
	title_obj = argv[1];

	prep_timer();		/* Start timing preparations */

	/* Build directory of GED database */
	if( dir_build( argv[0], 1 ) < 0 )  {
		fprintf(stderr,"rt:  dir_build failure\n");
		exit(2);
	}
	argc--; argv++;

	(void)read_timer( outbuf, sizeof(outbuf) );
	fprintf(stderr,"DB TOC: %s\n", outbuf);
	prep_timer();

	/* Load the desired portion of the model */
	while( argc > 0 )  {
		(void)get_tree(argv[0]);
		argc--; argv++;
	}
	(void)read_timer( outbuf, sizeof(outbuf) );
	fprintf(stderr,"DB WALK: %s\n", outbuf);
	prep_timer();

	/* Allow library to prepare itself */
	rt_prep();

	/* initialize application */
	view_init( &ap, title_file, title_obj, npts, outfd );

	(void)read_timer( outbuf, sizeof(outbuf) );
	fprintf(stderr, "PREP: %s\n", outbuf );

	if( HeadSolid == SOLTAB_NULL )  {
		fprintf(stderr,"rt: No solids remain after prep.\n");
		exit(3);
	}
	fprintf(stderr,"shooting at %d solids in %d regions\n",
		nsolids, nregions );

	fprintf(stderr,"model X(%f,%f), Y(%f,%f), Z(%f,%f)\n",
		mdl_min[X], mdl_max[X],
		mdl_min[Y], mdl_max[Y],
		mdl_min[Z], mdl_max[Z] );

#ifdef HEP
	(void)Disete( &work_word );
	if( npsw < 1 || npsw > 128 )
		npsw = 4;
	vfree( vmalloc( (20+npsw)*2048, "worker prefetch"), "worker");
	rtlog("creating %d worker PSWs\n", npsw );
	for( x=0; x<npsw; x++ )  {
		Dcreate( worker, &ap );
	}
	rtlog("creates done\n");
#endif

do_more:
	if( !matflag )  {
		vect_t view_min;		/* view position of mdl_min */
		vect_t view_max;		/* view position of mdl_max */
		mat_t Viewrot, toViewcenter;
		fastf_t f;
		fastf_t viewsize;

		/*
		 * Unrotated view is TOP.
		 * Rotation of 270,0,270 takes us to a front view.
		 * Standard GIFT view is -35 azimuth, -25 elevation off front.
		 */
		mat_idn( Viewrot );
		mat_angles( Viewrot, 270.0-elevation, 0.0, 270.0+azimuth );
		fprintf(stderr,"Viewing %f azimuth, %f elevation off of front view\n",
			azimuth, elevation);

		viewbounds( view_min, view_max, Viewrot );
		viewsize = (view_max[X]-view_min[X]);
		f = (view_max[Y]-view_min[Y]);
		if( f > viewsize )  viewsize = f;
		f = (view_max[Z]-view_min[Z]);
		if( f > viewsize )  viewsize = f;

		mat_idn( toViewcenter );
		toViewcenter[MDX] = -(view_max[X]+view_min[X])/2;
		toViewcenter[MDY] = -(view_max[Y]+view_min[Y])/2;
		toViewcenter[MDZ] = -(view_max[Z]+view_min[Z])/2;

		mat_mul( model2view, Viewrot, toViewcenter );
		model2view[15] = 0.5*viewsize;	/* Viewscale */
	}  else  {
		register int i;

		/* Visible part is from -1 to +1 in view space */
		for( i=0; i < 16; i++ )
			if( scanf( "%f", &model2view[i] ) != 1 )
				goto out;
	}
	mat_inv( view2model, model2view );

	fprintf(stderr,"Deltas=%f (model units between rays)\n",
		model2view[15]*2/npts );

	VSET( temp, 0, 0, -1 );
	MAT4X3VEC( ap.a_ray.r_dir, view2model, temp );
	VUNITIZE( ap.a_ray.r_dir );

	/* Chop -1.0..+1.0 range into npts parts */
	VSET( temp, 2.0/npts, 0, 0 );
	MAT4X3VEC( dx_model, view2model, temp );
	VSET( temp, 0, 2.0/npts, 0 );
	MAT4X3VEC( dy_model, view2model, temp );

	VSET( temp, 0, 0, zoomout );
	MAT4X3PNT( eye_model, view2model, temp );	/* perspective only */

	/* "Upper left" corner of viewing plane */
	if( perspective )  {
		VSET( temp, -1, -1, 0 );	/* viewing plane */
	}  else  {
		VSET( temp, -1, -1, 1.1 );	/* eye plane */
	}
	MAT4X3PNT( viewbase_model, view2model, temp );

	/* initialize lighting */
	view_2init( &ap );

	fflush(stdout);
	fflush(stderr);
	prep_timer();	/* start timing actual run */

	for( y = npts-1; y >= 0; y--)  {
		for( x = 0; x < npts; x++)  {
#ifndef HEP
			work_word = (x<<16) | y;
			worker( &ap );
#else
			/* Wait until empty, then fill */
			Diawrite( &work_word, (x<<16) | y );
#endif
		}
#ifndef HEP
		view_eol( &ap );	/* End of scan line */
#endif
	}
	utime = read_timer( outbuf, sizeof(outbuf) );	/* final time */
#ifndef HEP
	view_end( &ap );		/* End of application */
#endif

	/*
	 *  All done.  Display run statistics.
	 */
	fprintf(stderr, "SHOT: %s\n", outbuf );
	fprintf(stderr,"%ld solid/ray intersections: %ld hits + %ld miss\n",
		nshots, nhits, nmiss );
	fprintf(stderr,"pruned %.1f%%:  %ld model RPP, %ld dups skipped, %ld solid RPP\n",
		((double)nhits*100.0)/nshots,
		nmiss_model, nmiss_tree, nmiss_solid );
	fprintf(stderr,"%d output rays in %f sec = %f rays/sec\n",
		npts*npts, utime, (double)(npts*npts/utime) );
#ifdef HEP
	if( write( outfd, scanbuf, npts*npts*3 ) != npts*npts*3 )  {
		perror("pixel output write");
		exit(1);
	}
#endif

	if( matflag )  goto do_more;
out:
#ifdef HEP
	fprintf(stderr,"rt: killing workers\n");
	for( x=0; x<npsw; x++ )
		Diawrite( &work_word, -1 );
	fprintf(stderr,"rt: exit\n");
#endif
	return(0);
}

/*
 *  			W O R K E R
 *  
 *  Compute one pixel, and store it.
 */
void
worker( ap )
register struct application *ap;
{
	LOCAL struct application a;
	LOCAL vect_t tempdir;
	register int com;

	a.a_onehit = 1;
#ifndef HEP
	{
		com = work_word;
#else
	while(1)  {
		if( (com = Diaread( &work_word )) < 0 )
			return;
		/* Note: ap->... not valid until first time here */
#endif
		a.a_x = (com>>16)&0xFFFF;
		a.a_y = (com&0xFFFF);
		a.a_hit = ap->a_hit;
		a.a_miss = ap->a_miss;
	 	VMOVE( a.a_ray.r_dir, ap->a_ray.r_dir );
		VJOIN2( a.a_ray.r_pt, viewbase_model,
			a.a_x, dx_model, 
			(npts-a.a_y-1), dy_model );
		if( perspective )  {
			VSUB2( a.a_ray.r_dir,
				a.a_ray.r_pt, eye_model );
			VUNITIZE( a.a_ray.r_dir );
			VMOVE( a.a_ray.r_pt, eye_model );
		}

		a.a_level = 0;		/* recursion level */
		shootray( &a );
#ifndef HEP
		view_pixel( &a );
#else
		{
			register char *pixelp;
			register int r,g,b;
			/* .pix files go bottom to top */
			pixelp = scanbuf+(((npts-a.a_y-1)*npts)+a.a_x)*3;
			r = a.a_color[0]*255.;
			g = a.a_color[1]*255.;
			b = a.a_color[2]*255.;
			/* Truncate glints, etc */
			if( r > 255 )  r=255;
			if( g > 255 )  g=255;
			if( b > 255 )  b=255;
			if( r<0 || g<0 || b<0 )  {
				rtlog("Negative RGB %d,%d,%d\n", r, g, b );
				r = 0x80;
				g = 0xFF;
				b = 0x80;
			}
			*pixelp++ = r ;
			*pixelp++ = g ;
			*pixelp++ = b ;
		}
#endif
	}
}
