/*
 *			M A I N . C
 *
 *  Ray Tracing User Interface (RTUIF) main program, using LIBRT library.
 *
 *  Invoked by MGED for quick pictures.
 *  Is linked with each of several "back ends":
 *	view.c, viewpp.c, viewray.c, viewcheck.c, etc
 *  to produce different executable programs:
 *	rt, rtpp, rtray, rtcheck, etc.
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
 *	This software is Copyright (C) 1985,1987 by the United States Army.
 *	All rights reserved.
 */
#ifndef lint
static char RCSrt[] = "@(#)$Header$ (BRL)";
#endif

#include "conf.h"

#include <stdio.h>
#include <ctype.h>
#include <math.h>

#include "machine.h"
#include "externs.h"
#include "bu.h"
#include "vmath.h"
#include "bn.h"
#include "raytrace.h"
#include "fb.h"
#include "./ext.h"
#include "./rdebug.h"
#include "../librt/debug.h"

extern char	usage[];

int		rdebug;			/* RT program debugging (not library) */

/***** Variables shared with viewing model *** */
FBIO		*fbp = FBIO_NULL;	/* Framebuffer handle */
FILE		*outfp = NULL;		/* optional pixel output file */
int		output_is_binary = 1;	/* !0 means output file is binary */
mat_t		view2model;
mat_t		model2view;
/***** end of sharing with viewing model *****/

/***** variables shared with worker() ******/
struct application ap;
vect_t		left_eye_delta;
extern int	width;			/* # of pixels in X */
extern int	height;			/* # of lines in Y */
extern int	incr_mode;		/* !0 for incremental resolution */
extern int	incr_nlevel;		/* number of levels */
extern int	npsw;			/* number of worker PSWs to run */
/***** end variables shared with worker() *****/

/***** variables shared with do.c *****/
extern int	pix_start;		/* pixel to start at */
extern int	pix_end;		/* pixel to end at */
extern int	nobjs;			/* Number of cmd-line treetops */
extern char	**objtab;		/* array of treetop strings */
char		*beginptr;		/* sbrk() at start of program */
extern int	matflag;		/* read matrix from stdin */
extern int	desiredframe;		/* frame to start at */
extern int	curframe;		/* current frame number,
					 * also shared with view.c */
extern char	*outputfile;		/* name of base of output file */
extern int	interactive;		/* human is watching results */
/***** end variables shared with do.c *****/


extern fastf_t	rt_dist_tol;		/* Value for rti_tol.dist */
extern fastf_t	rt_perp_tol;		/* Value for rti_tol.perp */
extern char	*framebuffer;		/* desired framebuffer */

extern struct command_tab	rt_cmdtab[];

extern char	version[];		/* From vers.c */

extern struct resource	resource[];	/* from opt.c */

/*
 *			M A I N
 */
main(argc, argv)
int argc;
char **argv;
{
	static struct rt_i *rtip;
	char *title_file, *title_obj;	/* name of file and first object */
	register int	x;
	char idbuf[132];		/* First ID record info */
	void	application_init();
	int i;

	bu_setlinebuf( stderr );

	/* Identify the versions of the libraries we are using. */
	(void)fprintf(stderr, "%s%s%s%s\n",
		version+5,
		rt_version+5,
		bn_version+5,
		bu_version+5
	      );	/* +5 to skip @(#) */

#ifdef HAVE_SBRK
	beginptr = (char *) sbrk(0);
#endif
	azimuth = 35.0;			/* GIFT defaults */
	elevation = 25.0;

	/* Before option processing, get default number of processors */
	npsw = rt_avail_cpus();		/* Use all that are present */
	if( npsw > DEFAULT_PSW )  npsw = DEFAULT_PSW;
	if( npsw > MAX_PSW )  npsw = MAX_PSW;

	/* Before option processing, do application-specific initialization */
	application_init();

	/* Process command line options */
	if ( !get_args( argc, argv ) )  {
		(void)fputs(usage, stderr);
		exit(1);
	}
	if( optind >= argc )  {
		fprintf(stderr,"rt:  MGED database not specified\n");
		(void)fputs(usage, stderr);
		exit(1);
	}

	/* If user gave no sizing info at all, use 512 as default */
	if( width <= 0 && cell_width <= 0 )
		width = 512;
	if( height <= 0 && cell_height <= 0 )
		height = 512;

	if( incr_mode )  {
		x = height;
		if( x < width )  x = width;
		incr_nlevel = 1;
		while( (1<<incr_nlevel) < x )
			incr_nlevel++;
		height = width = 1<<incr_nlevel;
		fprintf(stderr, "incremental resolution, nlevels = %d, width=%d\n",
			incr_nlevel, width);
	}

	/*
	 *  Handle parallel initialization, if applicable.
	 */
#ifndef PARALLEL
	npsw = 1;			/* force serial */
#endif
	if( npsw < 0 )  {
		/* Negative number means "all but" npsw */
		npsw = rt_avail_cpus() + npsw;
	}
	if( npsw > MAX_PSW )  npsw = MAX_PSW;
	if( npsw > 1 )  {
	    rt_g.rtg_parallel = 1;
	    fprintf(stderr,"Planning to run with %d processors\n", npsw );
	} else
		rt_g.rtg_parallel = 0;
	RES_INIT( &rt_g.res_syscall );
	RES_INIT( &rt_g.res_worker );
	RES_INIT( &rt_g.res_stats );
	RES_INIT( &rt_g.res_results );
	RES_INIT( &rt_g.res_model );

	/*
	 *  Do not use rt_log() or rt_malloc() before this point!
	 */

	if( bu_debug )  {
		bu_printb( "libbu bu_debug", bu_debug, BU_DEBUG_FORMAT );
		bu_log("\n");
	}

	if( rt_g.debug )  {
		bu_printb( "librt rt_g.debug", rt_g.debug, DEBUG_FORMAT );
		bu_log("\n");
	}
	if( rdebug )  {
		bu_printb( "rt rdebug", rdebug, RDEBUG_FORMAT );
		bu_log("\n");
	}

	title_file = argv[optind];
	title_obj = argv[optind+1];
	nobjs = argc - optind - 1;
	objtab = &(argv[optind+1]);

	if( nobjs <= 0 )  {
		bu_log("rt: no objects specified\n");
		exit(1);
	}

	/* Build directory of GED database */
	if( (rtip=rt_dirbuild(title_file, idbuf, sizeof(idbuf))) == RTI_NULL ) {
		bu_log("rt:  rt_dirbuild(%s) failure\n", title_file);
		exit(2);
	}
	ap.a_rt_i = rtip;
	bu_log("db title:  %s\n", idbuf);

	/* Copy values from command line options into rtip */
	rtip->rti_space_partition = space_partition;
	rtip->useair = use_air;
	if( rt_dist_tol > 0 )  {
		rtip->rti_tol.dist = rt_dist_tol;
		rtip->rti_tol.dist_sq = rt_dist_tol * rt_dist_tol;
	}
	if( rt_perp_tol > 0 )  {
		rtip->rti_tol.perp = rt_perp_tol;
		rtip->rti_tol.para = 1 - rt_perp_tol;
	}
	rt_pr_tol( &rtip->rti_tol );

	/* before view_init */
	if( outputfile && strcmp( outputfile, "-") == 0 )
		outputfile = (char *)0;

	/* 
	 *  Initialize application.
	 *  Note that width & height may not have been set yet,
	 *  since they may change from frame to frame.
	 */
	if( view_init( &ap, title_file, title_obj, outputfile!=(char *)0 ) != 0 )  {
		/* Framebuffer is desired */
		register int xx, yy;
		int	zoom;

		/* Ask for a fb big enough to hold the image, at least 512. */
		/* This is so MGED-invoked "postage stamps" get zoomed up big enough to see */
		xx = yy = 512;
		if( width > xx || height > yy )  {
			xx = width;
			yy = height;
		}
		RES_ACQUIRE( &rt_g.res_syscall );
		fbp = fb_open( framebuffer, xx, yy );
		RES_RELEASE( &rt_g.res_syscall );
		if( fbp == FBIO_NULL )  {
			fprintf(stderr,"rt:  can't open frame buffer\n");
			exit(12);
		}

		/* If the fb is lots bigger (>= 2X), zoom up & center */
		RES_ACQUIRE( &rt_g.res_syscall );
		if( width > 0 && height > 0 )  {
			zoom = fb_getwidth(fbp)/width;
			if( fb_getheight(fbp)/height < zoom )
				zoom = fb_getheight(fbp)/height;
		} else {
			zoom = 1;
		}
		(void)fb_view( fbp, width/2, height/2,
			zoom, zoom );
		RES_RELEASE( &rt_g.res_syscall );
	} else if( outputfile == (char *)0 )  {
		/* If not going to framebuffer, or to a file, then use stdout */
		if( outfp == NULL )  outfp = stdout;
		/* output_is_binary is changed by view_init, as appropriate */
		if( output_is_binary && isatty(fileno(outfp)) )  {
			fprintf(stderr,"rt:  attempting to send binary output to terminal, aborting\n");
			exit(14);
		}
	}
#ifdef HAVE_SBRK
	fprintf(stderr,"initial dynamic memory use=%d.\n", (char *)sbrk(0)-beginptr );
	beginptr = (char *) sbrk(0);
#endif

	if( !matflag )  {
		def_tree( rtip );		/* Load the default trees */
		do_ae( azimuth, elevation );
		(void)do_frame( curframe );
	} else if( !isatty(fileno(stdin)) && old_way( stdin ) )  {
		; /* All is done */
	} else {
		register char	*buf;
		register int	ret;
		/*
		 * New way - command driven.
		 * Process sequence of input commands.
		 * All the work happens in the functions
		 * called by rt_do_cmd().
		 */
		while( (buf = rt_read_cmd( stdin )) != (char *)0 )  {
			if( rdebug&RDEBUG_PARSE )
				fprintf(stderr,"cmd: %s\n", buf );
			ret = rt_do_cmd( rtip, buf, rt_cmdtab );
			rt_free( buf, "rt_read_cmd command buffer" );
			if( ret < 0 )
				break;
		}
		if( curframe < desiredframe )  {
			fprintf(stderr,
				"rt:  Desired frame %d not reached, last was %d\n",
				desiredframe, curframe);
		}
	}

	/* Release the framebuffer, if any */
	if( fbp != FBIO_NULL )
		fb_close(fbp);

	return(0);
}
