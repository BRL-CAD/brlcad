/*
 *			R T . C 
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
#include <math.h>
#include "machine.h"
#include "vmath.h"
#include "raytrace.h"
#include "fb.h"
#include "./mathtab.h"
#include "./rdebug.h"
#include "../librt/debug.h"

#ifdef HEP
# include <synch.h>
# undef stderr
# define stderr stdout
# define PARALLEL 1
#endif

extern int	getopt();
extern char	*optarg;
extern int	optind;

extern char	usage[];

extern void	wray(), wraypts();

extern double	atof();
extern char	*sbrk();

int		rdebug;			/* RT program debugging (not library) */

/***** Variables shared with viewing model *** */
FBIO		*fbp = FBIO_NULL;	/* Framebuffer handle */
FILE		*outfp = NULL;		/* optional pixel output file */
int		hex_out = 0;		/* Binary or Hex .pix output file */
double		AmbientIntensity = 0.4;	/* Ambient light intensity */
double		azimuth, elevation;
int		lightmodel;		/* Select lighting model */
mat_t		view2model;
mat_t		model2view;
/***** end of sharing with viewing model *****/

extern void	grid_setup();
extern void	worker();
/***** variables shared with worker() ******/
struct application ap;
int		stereo = 0;	/* stereo viewing */
vect_t		left_eye_delta;
int		hypersample=0;	/* number of extra rays to fire */
int		perspective=0;	/* perspective view -vs- parallel view */
vect_t		dx_model;	/* view delta-X as model-space vector */
vect_t		dy_model;	/* view delta-Y as model-space vector */
point_t		eye_model;	/* model-space location of eye */
point_t		viewbase_model;	/* model-space location of viewplane corner */
int		npts;		/* # of points to shoot: x,y */
mat_t		Viewrotscale;
fastf_t		viewsize;
fastf_t		zoomout=1;	/* >0 zoom out, 0..1 zoom in */

#ifdef PARALLEL
char		*scanbuf;	/*** Output buffering, for parallelism */
#endif

int		npsw = 1;		/* number of worker PSWs to run */
struct resource	resource[MAX_PSW];	/* memory resources */
/***** end variables shared with worker() */

static char	*beginptr;		/* sbrk() at start of program */
static int	matflag = 0;		/* read matrix from stdin */
static int	desiredframe = 0;
static char	*outputfile = (char *)0;/* name of base of output file */
static char	*framebuffer = NULL;	/* Name of framebuffer */

#ifdef PARALLEL
static int	lock_tab[12];		/* Lock usage counters */
static char	*all_title[12] = {
	"malloc",
	"worker",
	"stats",
	"???"
};

/*
 *			L O C K _ P R
 */
lock_pr()
{
	register int i;
	for( i=0; i<3; i++ )  {
		if(lock_tab[i] == 0)  continue;
		fprintf(stderr,"%10d %s\n", lock_tab[i], all_title[i]);
	}
}
#endif PARALLEL

/*
 *			R E S _ P R
 */
res_pr()
{
	register struct resource *res;
	register int i;

	res = &resource[0];
	for( i=0; i<npsw; i++, res++ )  {
		fprintf(stderr,"cpu%d seg  len=%10d get=%10d free=%10d\n",
			i,
			res->re_seglen, res->re_segget, res->re_segfree );
		fprintf(stderr,"cpu%d part len=%10d get=%10d free=%10d\n",
			i,
			res->re_partlen, res->re_partget, res->re_partfree );
		fprintf(stderr,"cpu%d bitv len=%10d get=%10d free=%10d\n",
			i,
			res->re_bitvlen, res->re_bitvget, res->re_bitvfree );
	}
}

/*
 *			G E T _ A R G S
 */
get_args( argc, argv )
register char **argv;
{
	register int c;

	while( (c=getopt( argc, argv, "SH:F:D:MA:x:X:s:f:a:e:l:O:o:p:P:" )) != EOF )  {
		switch( c )  {
		case 'S':
			stereo = 1;
			break;
		case 'H':
			hypersample = atoi( optarg );
			break;
		case 'F':
			framebuffer = optarg;
			break;
		case 'D':
			desiredframe = atoi( optarg );
			break;
		case 'M':
			matflag = 1;
			break;
		case 'A':
			AmbientIntensity = atof( optarg );
			break;
		case 'x':
			sscanf( optarg, "%x", &rt_g.debug );
			fprintf(stderr,"librt rt_g.debug=x%x\n", rt_g.debug);
		case 'X':
			sscanf( optarg, "%x", &rdebug );
			fprintf(stderr,"rt rdebug=x%x\n", rdebug);
			break;
		case 's':
			/* Square size -- fall through */
		case 'f':
			/* "Fast" -- arg's worth of pixels */
			npts = atoi( optarg );
			if( npts < 2 || npts > (1024*8) )  {
				fprintf(stderr,"npts=%d out of range\n", npts);
				npts = 50;
			}
			break;
		case 'a':
			/* Set azimuth */
			azimuth = atof( optarg );
			matflag = 0;
			break;
		case 'e':
			/* Set elevation */
			elevation = atof( optarg );
			matflag = 0;
			break;
		case 'l':
			/* Select lighting model # */
			lightmodel = atoi( optarg );
			break;
		case 'O':
			/* Output pixel file name, Hex format */
			outputfile = optarg;
			hex_out = 1;
			break;
		case 'o':
			/* Output pixel file name, binary format */
			outputfile = optarg;
			hex_out = 0;
			break;
		case 'p':
			perspective = 1;
			zoomout = atof( optarg );
			if( zoomout <= 0 )  zoomout = 1;
			break;
		case 'P':
			/* Number of parallel workers */
			npsw = atoi( optarg );
			if( npsw < 1 || npsw > MAX_PSW )  {
				fprintf(stderr,"npsw out of range 1..%d\n", MAX_PSW);
				npsw = 1;
			}
			break;
		default:		/* '?' */
			fprintf(stderr,"unknown option %c\n", c);
			return(0);	/* BAD */
		}
	}
	return(1);			/* OK */
}

/*
 *			M A I N
 */
main(argc, argv)
int argc;
char **argv;
{
	static struct rt_i *rtip;
	static vect_t temp;
	static double utime;
	char *title_file, *title_obj;	/* name of file and first object */
	register int x,y;
	char framename[128];		/* File name to hold current frame */
	char outbuf[132];
	char idbuf[132];		/* First ID record info */
	int framenumber = 0;
	static struct region *regp;

	beginptr = sbrk(0);
	npts = 512;
	azimuth = -35.0;			/* GIFT defaults */
	elevation = -25.0;

	if ( !get_args( argc, argv ) )  {
		(void)fputs(usage, stderr);
		exit(1);
	}
	if( optind+1 >= argc )  {
		fprintf(stderr,"database & object(s) list missing\n");
		(void)fputs(usage, stderr);
		exit(1);
	}

	RES_INIT( &rt_g.res_malloc );
	RES_INIT( &rt_g.res_worker );
	RES_INIT( &rt_g.res_stats );
#ifdef PARALLEL
	scanbuf = rt_malloc( npts*npts*3 + sizeof(long), "scanbuf" );
#endif

	title_file = argv[optind];
	title_obj = argv[optind+1];

	rt_prep_timer();		/* Start timing preparations */

	/* Build directory of GED database */
	if( (rtip=rt_dirbuild(argv[optind++], idbuf, sizeof(idbuf))) == RTI_NULL ) {
		fprintf(stderr,"rt:  rt_dirbuild failure\n");
		exit(2);
	}
	ap.a_rt_i = rtip;
	fprintf(stderr, "db title:  %s\n", idbuf);

	(void)rt_read_timer( outbuf, sizeof(outbuf) );
	fprintf(stderr,"DB TOC: %s\n", outbuf);
	rt_prep_timer();

	/* Load the desired portion of the model */
	for( ; optind < argc; optind++ )  {
		if( rt_gettree(rtip, argv[optind]) < 0 )
			fprintf(stderr,"rt_gettree(%s) FAILED\n", argv[optind]);
	}
	(void)rt_read_timer( outbuf, sizeof(outbuf) );
	fprintf(stderr,"DB WALK: %s\n", outbuf);
#ifdef CRAY_COS
	remark(outbuf);		/* Info for JStat */
#endif CRAY_COS

	/* Allow library to prepare itself */
	rt_prep_timer();
	rt_prep(rtip);

	/* Initialize the material library for all regions */
	for( regp=rtip->HeadRegion; regp != REGION_NULL; regp=regp->reg_forw )  {
		if( mlib_setup( regp ) == 0 )  {
			rt_log("mlib_setup failure on %s\n", regp->reg_name);
		}
	}

	/* 
	 *  Initialize application.
	 */
	if( view_init( &ap, title_file, title_obj, npts, outputfile!=(char *)0 ) != 0 )  {
		/* Framebuffer is desired */
		register int sz = 512;
		while( sz < npts )
			sz <<= 1;
		if( (fbp = fb_open( framebuffer, sz, sz )) == FBIO_NULL )  {
			rt_log("rt:  can't open frame buffer\n");
			exit(12);
		}
		fb_clear( fbp, PIXEL_NULL );
		fb_wmap( fbp, COLORMAP_NULL );
		/* KLUDGE ALERT:  The library want zoom before window! */
		fb_zoom( fbp, fb_getwidth(fbp)/npts, fb_getheight(fbp)/npts );
		fb_window( fbp, npts/2, npts/2 );
	}

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

	if(rdebug&RDEBUG_RTMEM)
		rt_g.debug |= DEBUG_MEM;

#ifdef PARALLEL
	/* Get enough dynamic memory to keep from making malloc sbrk() */
	for( x=0; x<npsw; x++ )  {
		rt_get_pt(&resource[x]);
		rt_get_seg(&resource[x]);
		rt_get_bitv(&resource[x]);
	}
#ifdef HEP
	/* This isn't useful with the Caltech malloc() in most systems,
	 * but is very helpful with the ordinary malloc(). */
	rt_free( rt_malloc( (20+npsw)*8192, "worker prefetch"), "worker");
#endif HEP

	fprintf(stderr,"PARALLEL: npsw=%d\n", npsw );
#ifdef HEP
	for( x=0; x<npsw; x++ )  {
		/* This is expensive when GEMINUS>1 */
		Dcreate( worker, x );
	}
#endif HEP
#endif PARALLEL
	fprintf(stderr,"initial dynamic memory use=%d.\n",sbrk(0)-beginptr );

do_more:
	if( !matflag )  {
		vect_t	diag;
		mat_t	toEye;

		/*
		 *  Compute the rotation specified by the azimuth and
		 *  elevation parameters.  First, note that these are
		 *  specified relative to the GIFT "front view", ie,
		 *  model (X,Y,Z) is view (Z,X,Y):  looking down X axis.
		 *  Then, a positive azimuth represents rotating the *model*
		 *  around the Y axis, or, rotating the *eye* in -Y.
		 *  A positive elevation represents rotating the *model*
		 *  around the X axis, or, rotating the *eye* in -X.
		 *  This is the "Gwyn compatable" azim/elev interpretation.
		 *  Note that GIFT azim/elev values are the negatives of
		 *  this interpretation.
		 */
		mat_idn( Viewrotscale );
		mat_angles( Viewrotscale, 270.0-elevation, 0.0, 270.0+azimuth );
		fprintf(stderr,"Viewing %g azimuth, %g elevation off of front view\n",
			azimuth, elevation);

		/* Look at the center of the model */
		mat_idn( toEye );
		toEye[MDX] = -(rtip->mdl_max[X]+rtip->mdl_min[X])/2;
		toEye[MDY] = -(rtip->mdl_max[Y]+rtip->mdl_min[Y])/2;
		toEye[MDZ] = -(rtip->mdl_max[Z]+rtip->mdl_min[Z])/2;

		/* Fit a sphere to the model RPP, diameter is viewsize */
		VSUB2( diag, rtip->mdl_max, rtip->mdl_min );
		viewsize = MAGNITUDE( diag );
		fprintf(stderr,"view size = %g\n", viewsize);

		Viewrotscale[15] = 0.5*viewsize;	/* Viewscale */
		mat_mul( model2view, Viewrotscale, toEye );
		mat_inv( view2model, model2view );
		VSET( temp, 0, 0, 1.414 );
		MAT4X3PNT( eye_model, view2model, temp );
	}  else  {
		register int i;
		char number[128];

		/* Visible part is from -1 to +1 in view space */
		if( fscanf( stdin, "%s", number ) != 1 )  goto out;
		viewsize = atof(number);
		if( fscanf( stdin, "%s", number ) != 1 )  goto out;
		eye_model[X] = atof(number);
		if( fscanf( stdin, "%s", number ) != 1 )  goto out;
		eye_model[Y] = atof(number);
		if( fscanf( stdin, "%s", number ) != 1 )  goto out;
		eye_model[Z] = atof(number);
		for( i=0; i < 16; i++ )  {
			if( fscanf( stdin, "%s", number ) != 1 )
				goto out;
			Viewrotscale[i] = atof(number);
		}
	}
	if( framenumber++ < desiredframe )  goto do_more;

	if( outputfile != (char *)0 )  {
#ifdef CRAY_COS
		/* Dots in COS file names make them permanant files. */
		sprintf( framename, "F%d", framenumber-1 );
		if( (outfp = fopen( framename, "w" )) == NULL )  {
			perror( framename );
			if( matflag )  goto do_more;
			exit(22);
		}
		/* Dispose to shell script starts with "!" */
		if( framenumber-1 <= 0 || outputfile[0] == '!' )  {
			sprintf( framename, outputfile );
		}  else  {
			sprintf( framename, "%s.%d", outputfile, framenumber-1 );
		}
#else
		if( framenumber-1 <= 0 )  {
			sprintf( framename, outputfile );
		}  else  {
			sprintf( framename, "%s.%d", outputfile, framenumber-1 );
		}
		if( (outfp = fopen( framename, "w" )) == NULL )  {
			perror( framename );
			if( matflag )  goto do_more;
			exit(22);
		}
		chmod( framename, 0444 );
#endif CRAY_COS
		fprintf(stderr,"Output file is '%s'\n", framename);
	}

	grid_setup();
	fprintf(stderr,"Beam radius=%g mm, divergance=%g mm/1mm\n",
		ap.a_rbeam, ap.a_diverge );

	/* initialize lighting */
	view_2init( &ap );

	rtip->nshots = 0;
	rtip->nmiss_model = 0;
	rtip->nmiss_tree = 0;
	rtip->nmiss_solid = 0;
	rtip->nmiss = 0;
	rtip->nhits = 0;
	rtip->rti_nrays = 0;

	fflush(stdout);
	fflush(stderr);

	/*
	 *  Compute the image
	 *  It may prove desirable to do this in chunks
	 */
	rt_prep_timer();
	do_run( 0, npts*npts - 1 );
	utime = rt_read_timer( outbuf, sizeof(outbuf) );

#ifndef PARALLEL
	view_end( &ap );		/* End of application */
#endif

	/*
	 *  All done.  Display run statistics.
	 */
	fprintf(stderr,"Dynamic memory use=%d.\n",sbrk(0)-beginptr );
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

	if( outfp != NULL )  {
#ifdef CRAY_COS
		int status;
		char dn[16];
		char message[128];

		strncpy( dn, outfp->ldn, sizeof(outfp->ldn) );	/* COS name */
#endif CRAY_COS
#ifdef PARALLEL
		if( fwrite( scanbuf, sizeof(char), npts*npts*3, outfp ) != npts*npts*3 )  {
			fprintf(stderr,"fwrite failure\n");
			goto out;
		}
#endif PARALLEL
		(void)fclose(outfp);
		outfp = NULL;
#ifdef CRAY_COS
		status = 0;
		if( hex_out )  {
			(void)DISPOSE( &status, "DN      ", dn,
				"TEXT    ", framename,
				"NOWAIT  " );
		} else {
			/* Binary out */
			(void)DISPOSE( &status, "DN      ", dn,
				"TEXT    ", framename,
				"NOWAIT  ",
				"DF      ", "BB      " );
		}
		sprintf(message,
			"%s Dispose,dn='%s',text='%s'.  stat=0%o",
			(status==0) ? "Good" : "---BAD---",
			dn, framename, status );
		fprintf(stderr, "%s\n", message);
		remark(message);	/* Send to log files */
#endif CRAY_COS
	}

#ifdef PARALLEL
	/* No live fb display yet */
	if( fbp )
		fb_write( fbp, 0, 0, scanbuf, npts*npts );
#endif PARALLEL

#ifdef STAT_PARALLEL
	lock_pr();
	res_pr();
#endif PARALLEL

	if( matflag )  goto do_more;
out:
	if( framenumber < desiredframe )  {
		fprintf(stderr,
			"rt:  Desired frame %d not reached, last was %d\n",
			desiredframe, framenumber);
	}
#ifdef HEP
	fprintf(stderr,"rt: killing workers\n");
	for( x=0; x<npsw; x++ )
		Diawrite( &work_word, -1 );
	fprintf(stderr,"rt: exit\n");
#endif
	return(0);
}

#ifdef cray
#ifdef PARALLEL
RES_INIT(p)
register int *p;
{
	register int i = p - (&rt_g.res_malloc);
	if(rdebug&RDEBUG_PARALLEL) 
		fprintf(stderr,"RES_INIT 0%o, i=%d, rt_g=0%o\n", p, i, &rt_g);
	LOCKASGN(p);
	if(rdebug&RDEBUG_PARALLEL) 
		fprintf(stderr,"    start value = 0%o\n", *p );
}
RES_ACQUIRE(p)
register int *p;
{
	register int i = p - (&rt_g.res_malloc);
	if( i < 0 || i > 12 )  {
		fprintf("RES_ACQUIRE(0%o)? %d?\n", p, i);
		abort();
	}
	lock_tab[i]++;		/* Not interlocked */
	if(rdebug&RDEBUG_PARALLEL) fputc( 'A'+i, stderr );
	LOCKON(p);
	if(rdebug&RDEBUG_PARALLEL) fputc( '0'+i, stderr );
}
RES_RELEASE(p)
register int *p;
{
	register int i = p - (&rt_g.res_malloc);
	if(rdebug&RDEBUG_PARALLEL) fputc( 'a'+i, stderr );
	LOCKOFF(p);
	if(rdebug&RDEBUG_PARALLEL) fputc( '\n', stderr);
}
#else
RES_INIT() {}
RES_ACQUIRE() {}
RES_RELEASE() {}
#endif PARALLEL
#endif cray

#ifdef sgi
/* Horrible bug in 3.3.1 and 3.4 and 3.5 -- hypot ruins stack! */
long float
hypot(a,b)
double a,b;
{
	return(sqrt(a*a+b*b));
}
#endif

#ifdef alliant
RES_ACQUIRE(p)
register int *p;		/* known to be a5 */
{
	register int i;
	i = p - (&rt_g.res_malloc);

#ifdef PARALLEL
	asm("loop:");
	do  {
		/* Just wasting time anyways, so log it */
		lock_tab[i]++;	/* non-interlocked */
	} while( *p );
	asm("	tas	a5@");
	asm("	bne	loop");
#endif
}

#ifdef never
MAT4X3PNT( o, m, i )
register fastf_t *o;	/* a5 */
register fastf_t *m;	/* a4 */
register fastf_t *i;	/* a3 */
{
#ifdef NON_VECTOR
	FAST fastf_t f;
	f = 1.0/((m)[12]*(i)[X] + (m)[13]*(i)[Y] + (m)[14]*(i)[Z] + (m)[15]);
	(o)[X]=((m)[0]*(i)[X] + (m)[1]*(i)[Y] + (m)[ 2]*(i)[Z] + (m)[3]) * f;
	(o)[Y]=((m)[4]*(i)[X] + (m)[5]*(i)[Y] + (m)[ 6]*(i)[Z] + (m)[7]) * f;
	(o)[Z]=((m)[8]*(i)[X] + (m)[9]*(i)[Y] + (m)[10]*(i)[Z] + (m)[11])* f;
#else
	register int i;		/* d7 */
	register int vm;	/* d6, vector mask */
	register int vi;	/* d5, vector increment */
	register int vl;	/* d4, vector length */

	vm = -1;
	vi = 4;
	vl = 4;

	asm("fmoved	a3@, fp0");
	asm("vmuld	a4@, fp0, v7");

	asm("fmoved	a3@(8), fp0");
	asm("vmuadd	fp0, a4@(8), v7, v7");

	asm("fmoved	a3@(16), fp0");
	asm("vmuadd	fp0, a4@(16), v7, v7");

	asm("vaddd	a4@(24), v7, v7");

#ifdef RECIPROCAL
	asm("moveq	#1, d0");
	asm("fmoveld	d0, fp0");
	asm("fdivd	a4@(120), fp0, fp0");
	asm("vmuld	v7, fp0, v7");
#else
	asm("fmovedd	a4@(120), fp7");
	asm("vrdivd	v7, fp7, v7");
#endif

	vi = 1;
	asm("vmoved	v7, a5@");
#endif
}
/* Apply a 4x4 matrix to a 3-tuple which is a relative Vector in space */
MAT4X3VEC( o, m, i )
register fastf_t *o;
register fastf_t *m;
register fastf_t *i;
{
#ifdef NON_VECTOR
	FAST fastf_t f;
	f = 1.0/((m)[15]);
	(o)[X] = ((m)[0]*(i)[X] + (m)[1]*(i)[Y] + (m)[ 2]*(i)[Z]) * f;
	(o)[Y] = ((m)[4]*(i)[X] + (m)[5]*(i)[Y] + (m)[ 6]*(i)[Z]) * f;
	(o)[Z] = ((m)[8]*(i)[X] + (m)[9]*(i)[Y] + (m)[10]*(i)[Z]) * f;
#else
	register int i;		/* d7 */
	register int vm;	/* d6, vector mask */
	register int vi;	/* d5, vector increment */
	register int vl;	/* d4, vector length */

	vm = -1;
	vi = 4;
	vl = 3;

	asm("fmoved	a3@, fp0");
	asm("vmuld	a4@, fp0, v7");

	asm("fmoved	a3@(8), fp0");
	asm("vmuadd	fp0, a4@(8), v7, v7");

	asm("fmoved	a3@(16), fp0");
	asm("vmuadd	fp0, a4@(16), v7, v7");

#ifdef RECIPROCAL
	asm("moveq	#1, d0");
	asm("fmoveld	d0, fp0");
	asm("fdivd	a4@(120), fp0, fp0");
	asm("vmuld	v7, fp0, v7");
#else
	asm("fmovedd	a4@(120), fp7");
	asm("vrdivd	v7, fp7, v7");
#endif

	vi = 1;
	asm("vmoved	v7, a5@");
#endif
}
#endif never
#endif alliant
