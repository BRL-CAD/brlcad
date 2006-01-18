/*                          M A I N . C
 * BRL-CAD
 *
 * Copyright (c) 1985-2006 United  States Government as represented by
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
/** @file main.c
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
 */
#ifndef lint
static const char RCSrt[] = "@(#)$Header$ (BRL)";
#endif

#include "common.h"

#include <stdio.h>
#ifdef HAVE_STRING_H
#  include <string.h>
#else
#  include <strings.h>
#endif
#include <ctype.h>
#include <signal.h>
#include <math.h>
#ifdef HAVE_UNISTD_H
#  include <unistd.h>
#endif

#ifdef HAVE_FCNTL_H
#  include <fcntl.h>
#endif

#include "machine.h"
#include "vmath.h"
#include "raytrace.h"
#include "fb.h"
#include "./ext.h"
#include "rtprivate.h"
#include "../librt/debug.h"
#include "pkg.h"

extern char	usage[];

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
int		report_progress;	/* !0 = user wants progress report */
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
long		n_malloc;		/* Totals at last check */
long		n_free;
long		n_realloc;
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

int	save_overlaps=0;	/* flag for setting rti_save_overlaps */


/*
 *			S I G I N F O _ H A N D L E R
 */
void
siginfo_handler(int arg)
{
	report_progress = 1;
#ifdef SIGUSR1
	(void)signal( SIGUSR1, siginfo_handler );
#endif
#ifdef SIGINFO
	(void)signal( SIGINFO, siginfo_handler );
#endif
}

/*
 *			M E M O R Y _ S U M M A R Y
 */
void
memory_summary(void)
{
#ifdef HAVE_SBRK
	if (rt_verbosity & VERBOSE_STATS)  {
		long	mdelta = bu_n_malloc - n_malloc;
		long	fdelta = bu_n_free - n_free;
		fprintf(stderr,
			"Additional mem=%ld., #malloc=%ld, #free=%ld, #realloc=%ld (%ld retained)\n",
			(long)((char *)sbrk(0)-beginptr),
			mdelta,
			fdelta,
			bu_n_realloc - n_realloc,
			mdelta - fdelta);
	}
	beginptr = (char *) sbrk(0);
#endif
	n_malloc = bu_n_malloc;
	n_free = bu_n_free;
	n_realloc = bu_n_realloc;
}

/*
 *			M A I N
 */
int main(int argc, char **argv)
{
	struct rt_i *rtip = NULL;
	char *title_file = NULL, *title_obj = NULL;	/* name of file and first object */
	char idbuf[RT_BUFSIZE] = {0};		/* First ID record info */
	void	application_init();
	struct bu_vls	times;
	int i;

#ifdef _WIN32
#if 1
	_setmode(_fileno(stdin), _O_BINARY);
	_setmode(_fileno(stdout), _O_BINARY);
	_setmode(_fileno(stderr), _O_BINARY);
#else
	_fmode = _O_BINARY;
#endif
#endif

#ifndef _WIN32
	bu_setlinebuf( stderr );
#endif

#ifdef HAVE_SBRK
	beginptr = (char *) sbrk(0);
#endif
	azimuth = 35.0;			/* GIFT defaults */
	elevation = 25.0;

	AmbientIntensity=0.4;
	background[0] = background[1] = 0.0;
	background[2] = 1.0/255.0; /* slightly non-black */

	/* Before option processing, get default number of processors */
	npsw = bu_avail_cpus();		/* Use all that are present */
	if( npsw > DEFAULT_PSW )  npsw = DEFAULT_PSW;
	if( npsw > MAX_PSW )  npsw = MAX_PSW;

	/* Before option processing, do application-specific initialization */
	RT_APPLICATION_INIT( &ap );
	application_init();

	/* Process command line options */
	if ( !get_args( argc, argv ) )  {
		(void)fputs(usage, stderr);
		exit(1);
	}
	/* Identify the versions of the libraries we are using. */
	if (rt_verbosity & VERBOSE_LIBVERSIONS) {
		(void)fprintf(stderr, "%s%s%s%s\n",
			version+5,
			rt_version+5,
			bn_version+5,
			bu_version+5
		      );	/* +5 to skip @(#) */
	}
#if defined(DEBUG)
	(void)fprintf(stderr, "WARNING: Compile-time debugging is enabled and may limit performance\n");
#endif
#if defined(NO_BOMBING_MACROS) || defined(NO_MAGIC_CHECKING) || defined(NO_BADRAY_CECHKING) || defined(NO_DEBUG_CHECKING)
	(void)fprintf(stderr, "WARNING: Run-time debugging is disabled and may enhance performance\n");
#endif

	/* Identify what host we're running on */
	if (rt_verbosity & VERBOSE_LIBVERSIONS) {
		char	hostname[512];
		hostname[0] = '\0';
#ifndef _WIN32
		if( gethostname( hostname, sizeof(hostname) ) >= 0 &&
		    hostname[0] != '\0' )
			(void)fprintf(stderr, "Running on %s\n", hostname);
#else
	sprintf(hostname,"Microsoft Windows");
	(void)fprintf(stderr, "Running on %s\n", hostname);
#endif
	}

	if( bu_optind >= argc )  {
		fprintf(stderr,"rt:  MGED database not specified\n");
		(void)fputs(usage, stderr);
		exit(1);
	}

	if (rpt_overlap)
		ap.a_logoverlap = ((void (*)())0);
	else
		ap.a_logoverlap = rt_silent_logoverlap;

	/* If user gave no sizing info at all, use 512 as default */
	if( width <= 0 && cell_width <= 0 )
		width = 512;
	if( height <= 0 && cell_height <= 0 )
		height = 512;

	/* If user didn't provide an aspect ratio, use the image
	 * dimensions ratio as a default.
	 */
	if (aspect <= 0.0) {
	    aspect = (fastf_t)width / (fastf_t)height;
	}

	if( sub_grid_mode ) {
		/* check that we have a legal subgrid */
		if( sub_xmax >= width || sub_ymax >= height ) {
			fprintf( stderr, "rt: illegal values for subgrid %d,%d,%d,%d\n",
				 sub_xmin, sub_ymin, sub_xmax, sub_ymax );
			fprintf( stderr, "\tFor a %d X %d image, the subgrid must be within 0,0,%d,%d\n",
				 width, height, width-1, height-1 );
			exit( 1 );
		}
	}

	if( incr_mode )  {
		int x = height;
		if( x < width )  x = width;
		incr_nlevel = 1;
		while( (1<<incr_nlevel) < x )
			incr_nlevel++;
		height = width = 1<<incr_nlevel;
		if (rt_verbosity & VERBOSE_INCREMENTAL)
			fprintf(stderr,
			    "incremental resolution, nlevels = %d, width=%d\n",
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
		npsw = bu_avail_cpus() + npsw;
	}
	if( npsw > MAX_PSW )  npsw = MAX_PSW;
	if( npsw > 1 )  {
	    rt_g.rtg_parallel = 1;
	    if (rt_verbosity & VERBOSE_MULTICPU)
	        fprintf(stderr,"Planning to run with %d processors\n", npsw );
	} else
		rt_g.rtg_parallel = 0;

	/* Initialize parallel processor support */
	bu_semaphore_init( RT_SEM_LAST );

	/*
	 *  Do not use bu_log() or bu_malloc() before this point!
	 */

	if( bu_debug )  {
		bu_printb( "libbu bu_debug", bu_debug, BU_DEBUG_FORMAT );
		bu_log("\n");
	}

	if( RT_G_DEBUG )  {
		bu_printb( "librt rt_g.debug", rt_g.debug, DEBUG_FORMAT );
		bu_log("\n");
	}
	if( rdebug )  {
		bu_printb( "rt rdebug", rdebug, RDEBUG_FORMAT );
		bu_log("\n");
	}

	/* We need this to run rt_dirbuild */
	rt_init_resource( &rt_uniresource, MAX_PSW, NULL );
	bn_rand_init( rt_uniresource.re_randptr, 0 );

	title_file = argv[bu_optind];
	title_obj = argv[bu_optind+1];
	nobjs = argc - bu_optind - 1;
	objtab = &(argv[bu_optind+1]);

	if( nobjs <= 0 )  {
		bu_log("%s: no objects specified -- raytrace aborted\n", argv[0]);
		exit(1);
	}

	/* Echo back the command line arugments as given, in 3 Tcl commands */
	if (rt_verbosity & VERBOSE_MODELTITLE) {
		struct bu_vls str;
		bu_vls_init(&str);
		bu_vls_from_argv( &str, bu_optind, (const char **)argv );
		bu_vls_strcat( &str, "\nopendb "  );
		bu_vls_strcat( &str, title_file );
		bu_vls_strcat( &str, ";\ntree " );
		bu_vls_from_argv( &str,
			nobjs <= 16 ? nobjs : 16,
			(const char **)argv+bu_optind+1 );
		if( nobjs > 16 )
			bu_vls_strcat( &str, " ...");
		else
			bu_vls_putc( &str, ';' );
		bu_log("%s\n", bu_vls_addr(&str) );
		bu_vls_free(&str);
	}

	/* Build directory of GED database */
	bu_vls_init( &times );
	rt_prep_timer();
	if( (rtip=rt_dirbuild(title_file, idbuf, sizeof(idbuf))) == RTI_NULL ) {
		bu_log("rt:  rt_dirbuild(%s) failure\n", title_file);
		exit(2);
	}
	ap.a_rt_i = rtip;
	(void)rt_get_timer( &times, NULL );
	if (rt_verbosity & VERBOSE_MODELTITLE)
		bu_log("db title:  %s\n", idbuf);
	if (rt_verbosity & VERBOSE_STATS)
		bu_log("DIRBUILD: %s\n", bu_vls_addr(&times) );
	bu_vls_free( &times );
	memory_summary();

	/* Copy values from command line options into rtip */
	rtip->rti_space_partition = space_partition;
	rtip->rti_nugrid_dimlimit = nugrid_dimlimit;
	rtip->rti_nu_gfactor = nu_gfactor;
	rtip->useair = use_air;
	rtip->rti_save_overlaps = save_overlaps;
	if( rt_dist_tol > 0 )  {
		rtip->rti_tol.dist = rt_dist_tol;
		rtip->rti_tol.dist_sq = rt_dist_tol * rt_dist_tol;
	}
	if( rt_perp_tol > 0 )  {
		rtip->rti_tol.perp = rt_perp_tol;
		rtip->rti_tol.para = 1 - rt_perp_tol;
	}
	if (rt_verbosity & VERBOSE_TOLERANCE)
		rt_pr_tol( &rtip->rti_tol );

	/* before view_init */
	if( outputfile && strcmp( outputfile, "-") == 0 )
		outputfile = (char *)0;

	pkg_init();

	/* 
	 *  Initialize application.
	 *  Note that width & height may not have been set yet,
	 *  since they may change from frame to frame.
	 */
	if( view_init( &ap, title_file, title_obj, outputfile!=(char *)0, framebuffer!=(char *)0 ) != 0 )  {
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
		bu_semaphore_acquire( BU_SEM_SYSCALL );
		fbp = fb_open( framebuffer, xx, yy );
		bu_semaphore_release( BU_SEM_SYSCALL );
		if( fbp == FBIO_NULL )  {
			fprintf(stderr,"rt:  can't open frame buffer\n");
			pkg_terminate();
			exit(12);
		}

		bu_semaphore_acquire( BU_SEM_SYSCALL );
		/* If fb came out smaller than requested, do less work */
		if( fb_getwidth(fbp) < width )  width = fb_getwidth(fbp);
		if( fb_getheight(fbp) < height )  height = fb_getheight(fbp);

		/* If the fb is lots bigger (>= 2X), zoom up & center */
		if( width > 0 && height > 0 )  {
			zoom = fb_getwidth(fbp)/width;
			if( fb_getheight(fbp)/height < zoom )
				zoom = fb_getheight(fbp)/height;
		} else {
			zoom = 1;
		}
		(void)fb_view( fbp, width/2, height/2,
			zoom, zoom );
		bu_semaphore_release( BU_SEM_SYSCALL );
	}
	if( (outputfile == (char *)0) && (fbp == FBIO_NULL) )  {
		/* If not going to framebuffer, or to a file, then use stdout */
		if( outfp == NULL )  outfp = stdout;
		/* output_is_binary is changed by view_init, as appropriate */
		if( output_is_binary && isatty(fileno(outfp)) )  {
			fprintf(stderr,"rt:  attempting to send binary output to terminal, aborting\n");
			pkg_terminate();
			exit(14);
		}
	}

	/*
	 *  Initialize all the per-CPU memory resources.
	 *  The number of processors can change at runtime, init them all.
	 */
	for( i=0; i < MAX_PSW; i++ )  {
		rt_init_resource( &resource[i], i, rtip );
		bn_rand_init( resource[i].re_randptr, i );
	}
	memory_summary();

#ifdef SIGUSR1
	(void)signal( SIGUSR1, siginfo_handler );
#endif
#ifdef SIGINFO
	(void)signal( SIGINFO, siginfo_handler );
#endif

	if( !matflag )  {
		int frame_retval;
		def_tree( rtip );		/* Load the default trees */
		do_ae( azimuth, elevation );
		frame_retval = do_frame( curframe );
		if (frame_retval != 0) {
		    /* Release the framebuffer, if any */
		    if( fbp != FBIO_NULL ) {
			fb_close(fbp);
		    }
		    pkg_terminate();
		    return 1;
		}
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
			if( R_DEBUG&RDEBUG_PARSE )
				fprintf(stderr,"cmd: %s\n", buf );
			ret = rt_do_cmd( rtip, buf, rt_cmdtab );
			bu_free( buf, "rt_read_cmd command buffer" );
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

	pkg_terminate();
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
