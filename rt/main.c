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
static char RCSrt[] = "@(#)$Header$ (BRL)";
#endif

#include <stdio.h>
#include "../h/machine.h"
#include "../h/vmath.h"
#include "../h/raytrace.h"
#include "mathtab.h"

#ifdef HEP
# include <synch.h>
# undef stderr
# define stderr stdout
#endif

extern char usage[];

extern void wray(), wraypts();
extern double atof();

/***** Variables shared with viewing model *** */
double AmbientIntensity = 0.4;	/* Ambient light intensity */
double azimuth, elevation;
int lightmodel;		/* Select lighting model */
mat_t view2model;
mat_t model2view;
int hex_out = 0;	/* Binary or Hex .pix output file */
/***** end of sharing with viewing model *****/

/***** variables shared with worker() */
static int	stereo = 0;	/* stereo viewing */
vect_t left_eye_delta;
static int	hypersample=0;	/* number of extra rays to fire */
static int	perspective=0;	/* perspective view -vs- parallel view */
vect_t	dx_model;	/* view delta-X as model-space vector */
vect_t	dy_model;	/* view delta-Y as model-space vector */
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
	static struct rt_i *rtip;
	static vect_t temp;
	static int matflag = 0;		/* read matrix from stdin */
	static double utime;
	char *title_file, *title_obj;	/* name of file and first object */
	char *outputfile = (char *)0;	/* name of base of output file */
	static float	zoomout=1;	/* >0 zoom out, 0..1 zoom in */
	static FILE *outfp = NULL;	/* optional pixel output file */
	register int x,y;
	char framename[128];		/* File name to hold current frame */
	char outbuf[132];
	char idbuf[132];		/* First ID record info */
	mat_t Viewrotscale;
	mat_t toEye;
	fastf_t viewsize;
	int framenumber = 0;
	int desiredframe = 0;
	static struct region *regp;

	npts = 512;
	azimuth = -35.0;			/* GIFT defaults */
	elevation = -25.0;

	if( argc <= 1 )  {
		fprintf(stderr, usage);
		exit(1);
	}

	argc--; argv++;
	while( argv[0][0] == '-' )  {
		switch( argv[0][1] )  {
		case 'S':
			stereo = 1;
			break;
		case 'h':
			hypersample = atoi( &argv[0][2] );
			break;
		case 'F':
			desiredframe = atoi( &argv[0][2] );
			break;
		case 'M':
			matflag = 1;
			break;
		case 'A':
			AmbientIntensity = atof( &argv[0][2] );
			break;
		case 'x':
			sscanf( &argv[0][2], "%x", &rt_g.debug );
			fprintf(stderr,"rt_g.debug=x%x\n", rt_g.debug);
			break;
		case 'f':
			/* "Fast" -- just a few pixels.  Or, arg's worth */
			npts = atoi( &argv[0][2] );
			if( npts < 2 || npts > (1024*8) )  {
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
		case 'O':
			/* Output pixel file name, Hex format */
			outputfile = argv[1];
			hex_out = 1;
			argc--; argv++;
			break;
		case 'o':
			/* Output pixel file name, binary format */
			outputfile = argv[1];
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
	RES_RELEASE( &rt_g.res_pt );
	RES_RELEASE( &rt_g.res_seg );
	RES_RELEASE( &rt_g.res_malloc );
	RES_RELEASE( &rt_g.res_bitv );
#ifdef HEP
	scanbuf = rt_malloc( npts*npts*3 + sizeof(long), "scanbuf" );
#endif

	title_file = argv[0];
	title_obj = argv[1];

	rt_prep_timer();		/* Start timing preparations */

	/* Build directory of GED database */
	if( (rtip=rt_dirbuild(argv[0], idbuf, sizeof(idbuf))) == RTI_NULL ) {
		fprintf(stderr,"rt:  rt_dirbuild failure\n");
		exit(2);
	}
	ap.a_rt_i = rtip;
	fprintf(stderr, "db title:  %s\n", idbuf);
	argc--; argv++;

	(void)rt_read_timer( outbuf, sizeof(outbuf) );
	fprintf(stderr,"DB TOC: %s\n", outbuf);
	rt_prep_timer();

	/* Load the desired portion of the model */
	while( argc > 0 )  {
		if( rt_gettree(rtip, argv[0]) < 0 )
			fprintf("rt_gettree(%s) FAILED\n", argv[0]);
		argc--; argv++;
	}
	(void)rt_read_timer( outbuf, sizeof(outbuf) );
	fprintf(stderr,"DB WALK: %s\n", outbuf);
	rt_prep_timer();

	/* Allow library to prepare itself */
	rt_prep(rtip);

	/* Initialize the material library for all regions */
	for( regp=rtip->HeadRegion; regp != REGION_NULL; regp=regp->reg_forw )  {
		if( mlib_setup( regp ) == 0 )  {
			rt_log("mlib_setup failure\n");
		}
	}

	/* initialize application */
	view_init( &ap, title_file, title_obj, npts, outputfile!=(char *)0 );

	(void)rt_read_timer( outbuf, sizeof(outbuf) );
	fprintf(stderr, "PREP: %s\n", outbuf );

	if( rtip->HeadSolid == SOLTAB_NULL )  {
		fprintf(stderr,"rt: No solids remain after prep.\n");
		exit(3);
	}
	fprintf(stderr,"shooting at %d solids in %d regions\n",
		rtip->nsolids, rtip->nregions );

	fprintf(stderr,"model X(%g,%g), Y(%g,%g), Z(%g,%g)\n",
		rtip->mdl_min[X], rtip->mdl_max[X],
		rtip->mdl_min[Y], rtip->mdl_max[Y],
		rtip->mdl_min[Z], rtip->mdl_max[Z] );

#ifdef HEP
	(void)Disete( &work_word );
	if( npsw < 1 || npsw > 128 )
		npsw = 4;
	/* Get enough dynamic memory to keep from making malloc sbrk() */
	for( x=0; x<npsw; x++ )  {
		rt_get_pt();
		rt_get_seg();
		get_bitv();
	}
	rt_free( rt_malloc( (20+npsw)*8192, "worker prefetch"), "worker");

	fprintf(stderr,"creating %d worker PSWs\n", npsw );
	for( x=0; x<npsw; x++ )  {
		Dcreate( worker, &ap );
	}
	fprintf(stderr,"creates done, DMend=%d.\n",sbrk(0) );
#endif

do_more:
	if( !matflag )  {
		vect_t view_min;		/* view position of rtip->mdl_min */
		vect_t view_max;		/* view position of rtip->mdl_max */
		fastf_t f;

		mat_idn( Viewrotscale );
		mat_angles( Viewrotscale, 270.0-elevation, 0.0, 270.0+azimuth );
		fprintf(stderr,"Viewing %g azimuth, %g elevation off of front view\n",
			azimuth, elevation);

		rt_viewbounds( view_min, view_max, Viewrotscale );
		viewsize = (view_max[X]-view_min[X]);
		f = (view_max[Y]-view_min[Y]);
		if( f > viewsize )  viewsize = f;
		f = (view_max[Z]-view_min[Z]);
		if( f > viewsize )  viewsize = f;

		/* First, go to view center, then back off for eye pos */
		mat_idn( toEye );
		toEye[MDX] = -(view_max[X]+view_min[X])/2;
		toEye[MDY] = -(view_max[Y]+view_min[Y])/2;
		toEye[MDZ] = -(view_max[Z]+view_min[Z])/2;
		Viewrotscale[15] = 0.5*viewsize;	/* Viewscale */
		mat_mul( model2view, Viewrotscale, toEye );
		mat_inv( view2model, model2view );
		VSET( temp, 0, 0, 1.414 );
		MAT4X3PNT( eye_model, view2model, temp );
	}  else  {
		register int i;
		char number[128];

		/* Visible part is from -1 to +1 in view space */
		if( scanf( "%s", number ) != 1 )  goto out;
		viewsize = atof(number);
		if( scanf( "%s", number ) != 1 )  goto out;
		eye_model[X] = atof(number);
		if( scanf( "%s", number ) != 1 )  goto out;
		eye_model[Y] = atof(number);
		if( scanf( "%s", number ) != 1 )  goto out;
		eye_model[Z] = atof(number);
		for( i=0; i < 16; i++ )  {
			if( scanf( "%s", number ) != 1 )
				goto out;
			Viewrotscale[i] = atof(number);
		}
	}
	if( framenumber++ < desiredframe )  goto do_more;

	if( outputfile != (char *)0 )  {
		if( framenumber-1 <= 0 )
			sprintf( framename, outputfile );
		else
			sprintf( framename, "%s.%d", outputfile, framenumber-1 );
		if( (outfp = fopen( framename, "w" )) == NULL )  {
			perror( framename );
			if( matflag )  goto do_more;
			exit(22);
		}
		chmod( framename, 0444 );
		fprintf(stderr,"Output file is %s\n", framename);
	}

	/* model2view takes us to eye_model location & orientation */
	mat_idn( toEye );
	toEye[MDX] = -eye_model[X];
	toEye[MDY] = -eye_model[Y];
	toEye[MDZ] = -eye_model[Z];
	Viewrotscale[15] = 0.5*viewsize;	/* Viewscale */
	mat_mul( model2view, Viewrotscale, toEye );
	mat_inv( view2model, model2view );

	/* Chop -1.0..+1.0 range into npts parts */
	VSET( temp, 2.0/npts, 0, 0 );
	MAT4X3VEC( dx_model, view2model, temp );
	VSET( temp, 0, 2.0/npts, 0 );
	MAT4X3VEC( dy_model, view2model, temp );
	if( stereo )  {
		/* Move left 2.5 inches (63.5mm) */
		VSET( temp, 2.0*(-63.5/viewsize), 0, 0 );
		rt_log("red eye: moving %f relative screen (left)\n", temp[X]);
		MAT4X3VEC( left_eye_delta, view2model, temp );
		VPRINT("left_eye_delta", left_eye_delta);
	}

	/* "Lower left" corner of viewing plane */
	if( perspective )  {
		VSET( temp, -1, -1, -zoomout );	/* viewing plane */
		/*
		 * Divergance is (0.5 * viewsize / npts) mm at
		 * a ray distance of (viewsize * zoomout) mm.
		 */
		ap.a_diverge = (0.5 / npts) / zoomout;
		ap.a_rbeam = 0;
	}  else  {
		VSET( temp, 0, 0, -1 );
		MAT4X3VEC( ap.a_ray.r_dir, view2model, temp );
		VUNITIZE( ap.a_ray.r_dir );

		VSET( temp, -1, -1, 0 );	/* eye plane */
		ap.a_rbeam = 0.5 * viewsize / npts;
		ap.a_diverge = 0;
	}
	MAT4X3PNT( viewbase_model, view2model, temp );

	fprintf(stderr,"Beam radius=%g mm, divergance=%g mm/1mm\n",
		ap.a_rbeam, ap.a_diverge );

	/* initialize lighting */
	view_2init( &ap, outfp );

	fflush(stdout);
	fflush(stderr);
	rt_prep_timer();	/* start timing actual run */

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
	utime = rt_read_timer( outbuf, sizeof(outbuf) );	/* final time */
#ifndef HEP
	view_end( &ap );		/* End of application */
#endif

	/*
	 *  All done.  Display run statistics.
	 */
	fprintf(stderr, "SHOT: %s\n", outbuf );
	fprintf(stderr,"%ld solid/ray intersections: %ld hits + %ld miss\n",
		rtip->nshots, rtip->nhits, rtip->nmiss );
	fprintf(stderr,"pruned %.1f%%:  %ld model RPP, %ld dups skipped, %ld solid RPP\n",
		rtip->nshots>0?((double)rtip->nhits*100.0)/rtip->nshots:100.0,
		rtip->nmiss_model, rtip->nmiss_tree, rtip->nmiss_solid );
	fprintf(stderr,"%d pixels in %.2f sec = %.2f pixels/sec\n",
		npts*npts, utime, (double)(npts*npts)/utime );
	fprintf(stderr,"Frame %d: %d rays in %.2f sec = %.2f rays/sec\n",
		framenumber-1,
		rtip->rti_nrays, utime, (double)(rtip->rti_nrays)/utime );
#ifdef HEP
	if( write( fileno(outfp), scanbuf, npts*npts*3 ) != npts*npts*3 )  {
		perror("pixel output write");
		goto out;
	}
#endif

	if( outfp != NULL )  {
		(void)fclose(outfp);
		outfp = NULL;
	}
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

#define CRT_BLEND(v)	(0.26*(v)[X] + 0.66*(v)[Y] + 0.08*(v)[Z])
#define NTSC_BLEND(v)	(0.30*(v)[X] + 0.59*(v)[Y] + 0.11*(v)[Z])

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
	LOCAL vect_t point;		/* Ref point on eye or view plane */
	LOCAL vect_t colorsum;
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
		a.a_rt_i = ap->a_rt_i;
		a.a_rbeam = ap->a_rbeam;
		a.a_diverge = ap->a_diverge;
		VSETALL( colorsum, 0 );
		for( com=0; com<=hypersample; com++ )  {
			if( hypersample )  {
				FAST fastf_t dx, dy;
				dx = a.a_x + rand_half();
				dy = (npts-a.a_y-1) + rand_half();
				VJOIN2( point, viewbase_model,
					dx, dx_model, dy, dy_model );
			}  else  {
				register int yy = npts-a.a_y-1;
				VJOIN2( point, viewbase_model,
					a.a_x, dx_model,
					yy, dy_model );
			}
			if( perspective )  {
				VSUB2( a.a_ray.r_dir,
					point, eye_model );
				VUNITIZE( a.a_ray.r_dir );
				VMOVE( a.a_ray.r_pt, eye_model );
			} else {
				VMOVE( a.a_ray.r_pt, point );
			 	VMOVE( a.a_ray.r_dir, ap->a_ray.r_dir );
			}
			a.a_level = 0;		/* recursion level */
			rt_shootray( &a );

			if( stereo )  {
				FAST fastf_t right,left;

				right = CRT_BLEND(a.a_color);

				VADD2(  point, point,
					left_eye_delta );
				if( perspective )  {
					VSUB2( a.a_ray.r_dir,
						point, eye_model );
					VUNITIZE( a.a_ray.r_dir );
					VMOVE( a.a_ray.r_pt, eye_model );
				} else {
					VMOVE( a.a_ray.r_pt, point );
				}
				a.a_level = 0;		/* recursion level */
				rt_shootray( &a );

				left = CRT_BLEND(a.a_color);
				VSET( a.a_color, left, 0, right );
			}
			VADD2( colorsum, colorsum, a.a_color );
		}
		if( hypersample )  {
			FAST fastf_t f;
			f = 1.0 / (hypersample+1);
			VSCALE( a.a_color, colorsum, f );
		}
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
				rt_log("Negative RGB %d,%d,%d\n", r, g, b );
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
