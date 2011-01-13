/*                           H U R T . C
 * BRL-CAD
 *
 * Copyright (c) 1985-2010 United  States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file hurt.c
 *
 * Humongous Unified Ray Tracer
 *
 */

#include "common.h"

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#ifdef HAVE_SYS_TYPES_H
# include <sys/types.h>
#endif
#ifdef HAVE_SYS_STAT_H
# include <sys/stat.h>
#endif
#include "bio.h"

#include "vmath.h"
#include "raytrace.h"
#include "fb.h"

#include "shadework.h"
#include "light.h"

/* private */
#include "./ext.h"
#include "rtprivate.h"
#include "brlcad_version.h"


/* Note: struct parsing requires no space after the commas.  take care
 * when formatting this file.  if the compile breaks here, it means
 * that spaces got inserted incorrectly.
 */
#define COMMA ','

#define GETOPT_STR ".:,:@:a:c:e:f:g:n:p:q:s:v:w:x:A:BC:D:E:F:G:H:J:K:MP:V:WX:!:"


FBIO		*fbp = FBIO_NULL;	/* Framebuffer handle */
FILE		*outfp = NULL;		/* optional pixel output file */
mat_t		view2model;
mat_t		model2view;

struct application ap;

size_t		width = 0;		/* # of pixels in X */
size_t		height = 0;		/* # of lines in Y */

double		azimuth = 0.0, elevation = 0.0;

int		hypersample = 0;		/* number of extra rays to fire */
unsigned int	jitter = 0;		/* ray jitter control variable */
fastf_t		rt_perspective = (fastf_t)0.0;	/* presp (degrees X) 0 => ortho */
fastf_t		aspect = (fastf_t)0.0;	/* view aspect ratio X/Y */
vect_t		dx_model;		/* view delta-X as model-space vect */
vect_t		dy_model;		/* view delta-Y as model-space vect */
vect_t		dx_unit;		/* view delta-X as unit-len vect */
vect_t		dy_unit;		/* view delta-Y as unit-len vect */
fastf_t		cell_width = (fastf_t)0.0;		/* model space grid cell width */
fastf_t		cell_height = (fastf_t)0.0;	/* model space grid cell height */
int		cell_newsize = 0;		/* new grid cell size */
point_t		eye_model = {(fastf_t)0.0, (fastf_t)0.0, (fastf_t)0.0};		/* model-space location of eye */
fastf_t         eye_backoff = (fastf_t)1.414;	/* dist from eye to center */
mat_t		Viewrotscale = { (fastf_t)0.0, (fastf_t)0.0, (fastf_t)0.0, (fastf_t)0.0,
				 (fastf_t)0.0, (fastf_t)0.0, (fastf_t)0.0, (fastf_t)0.0,
				 (fastf_t)0.0, (fastf_t)0.0, (fastf_t)0.0, (fastf_t)0.0,
				 (fastf_t)0.0, (fastf_t)0.0, (fastf_t)0.0, (fastf_t)0.0};
fastf_t		viewsize = (fastf_t)0.0;
int		npsw = 1;		/* number of worker PSWs to run */
struct resource	resource[MAX_PSW];	/* memory resources */

int		pix_start = -1;		/* pixel to start at */
int		pix_end = 0;		/* pixel to end at */
int		nobjs = 0;		/* Number of cmd-line treetops */
char		**objtab = (char **)NULL;	/* array of treetop strings */
int		matflag = 0;		/* read matrix from stdin */
int		desiredframe = 0;	/* frame to start at */
int		finalframe = -1;	/* frame to halt at */
int		curframe = 0;		/* current frame number,
					 * also shared with view.c */
char		*outputfile = (char *)NULL;	/* name of base of output file */
int		benchmark = 0;		/* No random numbers:  benchmark */

fastf_t		frame_delta_t = (fastf_t)(1.0/30.0); /* 1.0 / frames_per_second_playback */

/* temporary kludge to get rt to use a tighter tolerance for raytracing */
fastf_t		rt_dist_tol = (fastf_t)0.0005;	/* Value for rti_tol.dist */

fastf_t		rt_perp_tol = (fastf_t)0.0;	/* Value for rti_tol.perp */
char		*framebuffer = (char *)NULL;		/* desired framebuffer */

int		space_partition = 	/*space partitioning algorithm to use*/
RT_PART_NUBSPT;
int		nugrid_dimlimit = 0;	/* limit to each dimension of
					   the nugrid */
double		nu_gfactor = RT_NU_GFACTOR_DEFAULT;
/* constant factor in NUgrid algorithm, if applicable */

unsigned char *pixmap = NULL; /* Pixel Map for rerendering of black pixels */

int		per_processor_chunk = 0;	/* how many pixels to do at once */

point_t		viewbase_model;	/* model-space location of viewplane corner */

/* Local communication with worker() */
int cur_pixel;			/* current pixel number, 0..last_pixel */
int last_pixel;			/* last pixel number */

const char title[] = "BRL-CAD's Tiny Raytracer TRT";
const char usage[] = "\
Usage:  trt [options] model.g objects...\n\
Options:\n\
 -s #		Square grid size in pixels (default 512)\n\
 -w # -n #	Grid size width and height in pixels\n\
 -V #		View (pixel) aspect ratio (width/height)\n\
 -a #		Azimuth in deg\n\
 -e #		Elevation in deg\n\
 -M		Read matrix+cmds on stdin\n\
 -o model.pix	Output file, .pix format (default=fb)\n\
 -p #		Perspective, degrees side to side\n\
 -P #		Set number of processors\n\
";

vect_t ambient_color = { 1, 1, 1 };	/* Ambient white light */

int	ibackground[3] = {0};			/* integer 0..255 version */
int	inonbackground[3] = {0};		/* integer non-background */

struct mfuncs *mfHead = MF_NULL;	/* Head of list of shaders */

/* The default a_onehit = -1 requires at least one non-air hit,
 * (stop at first surface) and stops ray/geometry intersection after that.
 * Set to 0 to turn off first hit optimization, with -c 'set a_onehit=0'
 */
int a_onehit = -1;

struct command_tab rt_cmdtab[];


/* Viewing module specific "set" variables */
struct bu_structparse view_parse[] = {
	{"%f", 1, "gamma", 0, BU_STRUCTPARSE_FUNC_NULL},
	{"%d", 1, "bounces", 0, BU_STRUCTPARSE_FUNC_NULL},
	{"%d", 1, "ireflect", 0, BU_STRUCTPARSE_FUNC_NULL},
	{"%d", 1, "a_onehit", 0, BU_STRUCTPARSE_FUNC_NULL},
	{"%f", ELEMENTS_PER_VECT, "background", 0, BU_STRUCTPARSE_FUNC_NULL},
	{"%d", 1, "overlay", 0, BU_STRUCTPARSE_FUNC_NULL},
	{"%d", 1, "ov", 0, BU_STRUCTPARSE_FUNC_NULL},
	{"", 0, (char *)0, 0, BU_STRUCTPARSE_FUNC_NULL}
};

struct bu_structparse set_parse[] = {
	{"%d",      1, "width",     bu_byteoffset(width),           BU_STRUCTPARSE_FUNC_NULL },
	{"%d",      1, "height",    bu_byteoffset(height),          BU_STRUCTPARSE_FUNC_NULL },
	{"%f",      1, "perspective", bu_byteoffset(rt_perspective),        BU_STRUCTPARSE_FUNC_NULL },
	{"%f",      1, "angle",     bu_byteoffset(rt_perspective),  BU_STRUCTPARSE_FUNC_NULL },
#if !defined(_WIN32) || defined(__CYGWIN__)
	{"%d",  1, "rt_bot_minpieces", bu_byteoffset(rt_bot_minpieces), BU_STRUCTPARSE_FUNC_NULL },
	{"%d",  1, "rt_bot_tri_per_piece", bu_byteoffset(rt_bot_tri_per_piece), BU_STRUCTPARSE_FUNC_NULL },
	{"%f",  1, "rt_cline_radius", bu_byteoffset(rt_cline_radius), BU_STRUCTPARSE_FUNC_NULL },
#endif
	{"%p", bu_byteoffset(view_parse[0]), "View_Module-Specific Parameters", 0, BU_STRUCTPARSE_FUNC_NULL },
	{"",        0, (char *)0,   0,                              BU_STRUCTPARSE_FUNC_NULL }
};


/*
 * For certain hypersample values there is a particular advantage to subdividing
 * the pixel and shooting a ray in each sub-pixel.  This structure keeps track of
 * those patterns
 */
struct jitter_pattern {
    int   num_samples;/* number of samples, or coordinate pairs in coords[] */
    float rand_scale[2]; /* amount to scale bn_rand_half value */
    float coords[32]; /* center of each sub-pixel */
};

static struct jitter_pattern pt_pats[] = {

    {4, {0.5, 0.5}, 	/* -H 3 */
     { 0.25, 0.25,
       0.25, 0.75,
       0.75, 0.25,
       0.75, 0.75 } },

    {5, {0.4, 0.4}, 	/* -H 4 */
     { 0.2, 0.2,
       0.2, 0.8,
       0.8, 0.2,
       0.8, 0.8,
       0.5, 0.5} },

    {9, {0.3333, 0.3333}, /* -H 8 */
     { 0.17, 0.17,  0.17, 0.5,  0.17, 0.82,
       0.5, 0.17,    0.5, 0.5,   0.5, 0.82,
       0.82, 0.17,  0.82, 0.5,  0.82, 0.82 } },

    {16, {0.25, 0.25}, 	/* -H 15 */
     { 0.125, 0.125,  0.125, 0.375, 0.125, 0.625, 0.125, 0.875,
       0.375, 0.125,  0.375, 0.375, 0.375, 0.625, 0.375, 0.875,
       0.625, 0.125,  0.625, 0.375, 0.625, 0.625, 0.625, 0.875,
       0.875, 0.125,  0.875, 0.375, 0.875, 0.625, 0.875, 0.875} },

    { 0, {0.0, 0.0}, {0.0} } /* must be here to stop search */
};


/*
 *			M A I N
 */
int main(int argc, char **argv)
{
    struct rt_i *rtip = NULL;
    char *title_file = NULL, *title_obj = NULL;	/* name of file and first object */
    char idbuf[RT_BUFSIZE] = {0};		/* First ID record info */
    struct bu_vls	times;
    int i;

    bu_setlinebuf( stderr );

    azimuth = 35.0;			/* GIFT defaults */
    elevation = 25.0;

    AmbientIntensity=0.4;
    background[0] = background[1] = 0.0;
    background[2] = 1.0/255.0; /* slightly non-black */

    /* Before option processing, get default number of processors */
    npsw = bu_avail_cpus();		/* Use all that are present */
    if ( npsw > MAX_PSW )  npsw = MAX_PSW;

    /* Before option processing, do application-specific initialization */
    RT_APPLICATION_INIT( &ap );

    /* Process command line options */
    if ( !get_args( argc, argv ) )  {
	(void)fputs(usage, stderr);
	return 1;
    }
    /* Identify the versions of the libraries we are using. */
    if (rt_verbosity & VERBOSE_LIBVERSIONS) {
	(void)fprintf(stderr, "%s%s%s%s\n",
		      brlcad_ident(title),
		      rt_version(),
		      bn_version(),
		      bu_version()
	    );
    }

    /* Identify what host we're running on */
    if (rt_verbosity & VERBOSE_LIBVERSIONS) {
	char	hostname[512] = {0};
	if ( gethostname( hostname, sizeof(hostname) ) >= 0 && hostname[0] != '\0' )
	    (void)fprintf(stderr, "Running on %s\n", hostname);
    }

    if ( bu_optind >= argc )  {
	fprintf(stderr, "%s:  BRL-CAD geometry database not specified\n", argv[0]);
	(void)fputs(usage, stderr);
	return 1;
    }

    ap.a_logoverlap = ((void (*)())0);

    /* If user gave no sizing info at all, use 512 as default */
    if ( width <= 0 && cell_width <= 0 )
	width = 512;
    if ( height <= 0 && cell_height <= 0 )
	height = 512;

    /* If user didn't provide an aspect ratio, use the image
     * dimensions ratio as a default.
     */
    if (aspect <= 0.0) {
	aspect = (fastf_t)width / (fastf_t)height;
    }

    /*
     *  Handle parallel initialization, if applicable.
     */

    if ( npsw < 0 )  {
	/* Negative number means "all but" npsw */
	npsw = bu_avail_cpus() + npsw;
    }

    if (npsw > 1) {
	rt_g.rtg_parallel = 1;
	if (rt_verbosity & VERBOSE_MULTICPU)
	    fprintf(stderr, "Planning to run with %d processors\n", npsw );
    } else {
	rt_g.rtg_parallel = 0;
    }

    /* Initialize parallel processor support */
    bu_semaphore_init( RT_SEM_LAST );

    /*
     *  Do not use bu_log() or bu_malloc() before this point!
     */

    /* We need this to run rt_dirbuild */
    rt_init_resource( &rt_uniresource, MAX_PSW, NULL );
    bn_rand_init( rt_uniresource.re_randptr, 0 );

    title_file = argv[bu_optind];
    title_obj = argv[bu_optind+1];
    nobjs = argc - bu_optind - 1;
    objtab = &(argv[bu_optind+1]);

    if ( nobjs <= 0 )  {
	bu_log("%s: no objects specified -- raytrace aborted\n", argv[0]);
	return 1;
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
	if ( nobjs > 16 )
	    bu_vls_strcat( &str, " ...");
	else
	    bu_vls_putc( &str, ';' );
	bu_log("%s\n", bu_vls_addr(&str) );
	bu_vls_free(&str);
    }

    /* Build directory of GED database */
    bu_vls_init( &times );
    rt_prep_timer();
    if ( (rtip=rt_dirbuild(title_file, idbuf, sizeof(idbuf))) == RTI_NULL ) {
	bu_log("rt:  rt_dirbuild(%s) failure\n", title_file);
	return 2;
    }
    ap.a_rt_i = rtip;
    (void)rt_get_timer( &times, NULL );
    if (rt_verbosity & VERBOSE_MODELTITLE)
	bu_log("db title:  %s\n", idbuf);
    if (rt_verbosity & VERBOSE_STATS)
	bu_log("DIRBUILD: %s\n", bu_vls_addr(&times) );
    bu_vls_free( &times );

    /* Copy values from command line options into rtip */
    rtip->rti_space_partition = space_partition;
    rtip->rti_nugrid_dimlimit = nugrid_dimlimit;
    rtip->rti_nu_gfactor = nu_gfactor;
    rtip->useair = 0;
    if ( rt_dist_tol > 0 )  {
	rtip->rti_tol.dist = rt_dist_tol;
	rtip->rti_tol.dist_sq = rt_dist_tol * rt_dist_tol;
    }
    if ( rt_perp_tol > 0 )  {
	rtip->rti_tol.perp = rt_perp_tol;
	rtip->rti_tol.para = 1 - rt_perp_tol;
    }
    if (rt_verbosity & VERBOSE_TOLERANCE)
	rt_pr_tol( &rtip->rti_tol );

    /* before view_init */
    if ( outputfile && BU_STR_EQUAL( outputfile, "-") )
	outputfile = (char *)0;

    /*
     *  Initialize application.
     *  Note that width & height may not have been set yet,
     *  since they may change from frame to frame.
     */
    if ( view_init( &ap, title_file, title_obj, outputfile!=(char *)0, framebuffer!=(char *)0 ) != 0 )  {
	/* Framebuffer is desired */
	register int xx, yy;
	int	zoom;

	/* Ask for a fb big enough to hold the image, at least 512. */
	/* This is so MGED-invoked "postage stamps" get zoomed up big enough to see */
	xx = yy = 512;
	if ( width > xx || height > yy )  {
	    xx = width;
	    yy = height;
	}
	bu_semaphore_acquire( BU_SEM_SYSCALL );
	fbp = fb_open( framebuffer, xx, yy );
	bu_semaphore_release( BU_SEM_SYSCALL );
	if ( fbp == FBIO_NULL )  {
	    fprintf(stderr, "rt:  can't open frame buffer\n");
	    return 12;
	}

	bu_semaphore_acquire( BU_SEM_SYSCALL );
	/* If fb came out smaller than requested, do less work */
	if ( fb_getwidth(fbp) < width )  width = fb_getwidth(fbp);
	if ( fb_getheight(fbp) < height )  height = fb_getheight(fbp);

	/* If the fb is lots bigger (>= 2X), zoom up & center */
	if ( width > 0 && height > 0 )  {
	    zoom = fb_getwidth(fbp)/width;
	    if ( fb_getheight(fbp)/height < zoom )
		zoom = fb_getheight(fbp)/height;
	} else {
	    zoom = 1;
	}
	(void)fb_view( fbp, width/2, height/2,
		       zoom, zoom );
	bu_semaphore_release( BU_SEM_SYSCALL );
    }
    if ( (outputfile == (char *)0) && (fbp == FBIO_NULL) )  {
	/* If not going to framebuffer, or to a file, then use stdout */
	if ( outfp == NULL )  outfp = stdout;
	if ( isatty(fileno(outfp)) )  {
	    fprintf(stderr, "rt:  attempting to send binary output to terminal, aborting\n");
	    return 14;
	}
    }

    /*
     *  Initialize all the per-CPU memory resources.
     *  The number of processors can change at runtime, init them all.
     */
    for ( i=0; i < MAX_PSW; i++ )  {
	rt_init_resource( &resource[i], i, rtip );
	bn_rand_init( resource[i].re_randptr, i );
    }

    if ( !matflag )  {
	int frame_retval;
	def_tree( rtip );		/* Load the default trees */
	do_ae( azimuth, elevation );
	frame_retval = do_frame( curframe );
	if (frame_retval != 0) {
	    /* Release the framebuffer, if any */
	    if ( fbp != FBIO_NULL ) {
		fb_close(fbp);
	    }
	    return 1;
	}
    } else {
	register char	*buf;
	register int	ret;
	/*
	 * New way - command driven.
	 * Process sequence of input commands.
	 * All the work happens in the functions
	 * called by rt_do_cmd().
	 */
	while ( (buf = rt_read_cmd( stdin )) != (char *)0 )  {
	    ret = rt_do_cmd( rtip, buf, rt_cmdtab );
	    bu_free( buf, "rt_read_cmd command buffer" );
	    if ( ret < 0 )
		break;
	}
	if ( curframe < desiredframe )  {
	    fprintf(stderr,
		    "rt:  Desired frame %d not reached, last was %d\n",
		    desiredframe, curframe);
	}
    }

    /* Release the framebuffer, if any */
    if ( fbp != FBIO_NULL )
	fb_close(fbp);

    return 0;
}


/*
 *			G E T _ A R G S
 */
int get_args( int argc, register char **argv )
{
    register int c;
    register int i;

    bu_optind = 1;		/* restart */



    while ( (c=bu_getopt( argc, argv, GETOPT_STR )) != EOF )  {
	switch ( c )  {
	    case 'q':
		i = atoi(bu_optarg);
		if (i <= 0)
		    bu_exit(EXIT_FAILURE, "-q %d is < 0\n", i);
		if ( i > BN_RANDHALFTABSIZE)
		    bu_exit(EXIT_FAILURE, "-q %d is > maximum (%d)\n", i, 
			    BN_RANDHALFTABSIZE);
		bn_randhalftabsize = i;
		break;
	    case '.':
		nu_gfactor = (double)atof( bu_optarg );
		break;
	    case COMMA:
		space_partition = atoi(bu_optarg);
		break;
	    case '@':
		nugrid_dimlimit = atoi(bu_optarg);
		break;
	    case 'c':
		(void)rt_do_cmd( (struct rt_i *)0, bu_optarg, rt_cmdtab );
		break;
	    case 'C':
	    {
		int		r, g, b;
		register char	*cp = bu_optarg;

		r = atoi(cp);
		while ( (*cp >= '0' && *cp <= '9') )  cp++;
		while ( *cp && (*cp < '0' || *cp > '9') ) cp++;
		g = atoi(cp);
		while ( (*cp >= '0' && *cp <= '9') )  cp++;
		while ( *cp && (*cp < '0' || *cp > '9') ) cp++;
		b = atoi(cp);

		if ( r < 0 || r > 255 )  r = 255;
		if ( g < 0 || g > 255 )  g = 255;
		if ( b < 0 || b > 255 )  b = 255;

		if (r == 0)
		    background[0] = 0.0;
		else
		    background[0] = r / 255.0;
		if (g == 0)
		    background[1] = 0.0;
		else
		    background[1] = g / 255.0;
		if (b == 0)
		    background[2] = 0.0;
		else
		    background[2] = b / 255.0;
	    }
	    break;
	    case 'J':
		sscanf( bu_optarg, "%x", &jitter );
		break;
	    case 'H':
		hypersample = atoi( bu_optarg );
		if ( hypersample > 0 )
		    jitter = 1;
		break;
	    case 'F':
		framebuffer = bu_optarg;
		break;
	    case 'D':
		desiredframe = atoi( bu_optarg );
		break;
	    case 'K':
		finalframe = atoi( bu_optarg );
		break;
	    case 'M':
		matflag = 1;
		break;
	    case 'A':
		AmbientIntensity = atof( bu_optarg );
		break;
	    case 'x':
		sscanf( bu_optarg, "%x", (unsigned int *)&rt_g.debug );
		break;
	    case 'X':
		sscanf( bu_optarg, "%x", (unsigned int *)&rdebug );
		break;
	    case '!':
		sscanf( bu_optarg, "%x", (unsigned int *)&bu_debug );
		break;

	    case 's':
		/* Square size */
		width = height = atoi( bu_optarg );
		break;
	    case 'n':
		i = atoi( bu_optarg );
		height = i;
		break;
	    case 'W':
		(void)rt_do_cmd( (struct rt_i *)0, "set background=1.0/1.0/1.0", rt_cmdtab );
		break;
	    case 'w':
		i = atoi( bu_optarg );
		width = i;
		break;
	    case 'g':
		cell_width = atof( bu_optarg );
		cell_newsize = 1;
		break;
	    case 'G':
		cell_height = atof( bu_optarg );
		cell_newsize = 1;
		break;

	    case 'a':
		/* Set azimuth */
		azimuth = atof( bu_optarg );
		matflag = 0;
		break;
	    case 'e':
		/* Set elevation */
		elevation = atof( bu_optarg );
		matflag = 0;
		break;
	    case 'p':
		rt_perspective = atof( bu_optarg );
		if ( rt_perspective < 0 || rt_perspective > 179 ) {
		    fprintf(stderr, "persp=%g out of range\n", rt_perspective);
		    rt_perspective = 0;
		}
		break;
	    case 'v': /* Set level of "non-debug" debugging output */
		sscanf( bu_optarg, "%x", (unsigned int *)&rt_verbosity );
		break;
	    case 'E':
		eye_backoff = atof( bu_optarg );
		break;

	    case 'P':
		npsw = atoi( bu_optarg );
		break;
	    case 'B':
		/*  Remove all intentional random effects
		 *  (dither, etc) for benchmarking purposes.
		 */
		benchmark = 1;
		bn_mathtab_constant();
		break;
	    case 'f':
		/* set expected playback rate in frames-per-second.
		 * This actually gets stored as the delta-t per frame.
		 */
		if ( NEAR_ZERO(frame_delta_t=atof( bu_optarg ), SMALL) ) {
		    fprintf(stderr, "Invalid frames/sec (%s) == 0.0\n",
			    bu_optarg);
		    frame_delta_t = 30.0;
		}
		frame_delta_t = 1.0 / frame_delta_t;
		break;
	    case 'V':
		/* View aspect */
	    {
		fastf_t xx, yy;
		register char *cp = bu_optarg;

		xx = atof(cp);
		while ( (*cp >= '0' && *cp <= '9')
			|| *cp == '.' )  cp++;
		while ( *cp && (*cp < '0' || *cp > '9') ) cp++;
		yy = atof(cp);
		if ( NEAR_ZERO(yy, SMALL) )
		    aspect = xx;
		else
		    aspect = xx/yy;
		if ( aspect <= 0.0 ) {
		    fprintf(stderr, "Bogus aspect %g, using 1.0\n", aspect);
		    aspect = 1.0;
		}
	    }
	    break;
	    default:		/* '?' */
		fprintf(stderr, "unknown option %c\n", c);
		return 0;	/* BAD */
	}
    }

    return 1;			/* OK */
}


/*
 *			C M _ S T A R T
 *
 *  Process "start" command in new format input stream
 */
int cm_start( int argc, char **argv)
{
    char	*buf = (char *)NULL;
    int	frame;
    if (argc < 1) return -1;

    frame = atoi(argv[1]);
    if ( finalframe >= 0 && frame > finalframe )
	return -1;	/* Indicate EOF -- user declared a halt */
    if ( frame >= desiredframe )  {
	curframe = frame;
	return 0;
    }

    /* Skip over unwanted frames -- find next frame start */
    while ( (buf = rt_read_cmd( stdin )) != (char *)0 )  {
	register char *cp;

	cp = buf;
	while ( *cp && isspace(*cp) )  cp++;	/* skip spaces */
	if ( strncmp( cp, "start", 5 ) != 0 )  continue;
	while ( *cp && !isspace(*cp) )  cp++;	/* skip keyword */
	while ( *cp && isspace(*cp) )  cp++;	/* skip spaces */
	frame = atoi(cp);
	bu_free( buf, "rt_read_cmd command buffer (skipping frames)" );
	buf = (char *)0;
	if ( finalframe >= 0 && frame > finalframe )
	    return -1;			/* "EOF" */
	if ( frame >= desiredframe )  {
	    curframe = frame;
	    return 0;
	}
    }
    return -1;		/* EOF */
}

int cm_vsize( int argc, char **argv)
{
    if (argc < 1) return -1;
    viewsize = atof( argv[1] );
    return 0;
}

int cm_eyept(int argc, char **argv)
{
    register int i;

    if (argc < 1) return -1;
    for ( i=0; i<3; i++ )
	eye_model[i] = atof( argv[i+1] );
    return 0;
}

int cm_lookat_pt(int argc, char **argv)
{
    point_t	pt;
    vect_t	dir;
    int	yflip = 0;
    quat_t quat;

    if ( argc < 4 )
	return -1;
    pt[X] = atof(argv[1]);
    pt[Y] = atof(argv[2]);
    pt[Z] = atof(argv[3]);
    if ( argc > 4 )
	yflip = atoi(argv[4]);

    /*
     *  eye_pt should have been specified before here and
     *  different from the lookat point or the lookat point will
     *  be from the "front"
     */
    if (VNEAR_EQUAL(pt, eye_model, VDIVIDE_TOL)) {
	VSETALLN(quat, 0.5, 4);
	quat_quat2mat(Viewrotscale, quat); /* front */
    } else {
	VSUB2( dir, pt, eye_model );
	VUNITIZE( dir );
	bn_mat_lookat( Viewrotscale, dir, yflip );
    }

    return 0;
}

int cm_vrot( int argc, char **argv)
{
    register int i;

    if (argc < 1) return -1;

    for ( i=0; i<16; i++ )
	Viewrotscale[i] = atof( argv[i+1] );
    return 0;
}

int cm_orientation(int argc, char **argv)
{
    register int	i;
    quat_t		quat;

    if (argc < 1) return -1;
    for ( i=0; i<4; i++ )
	quat[i] = atof( argv[i+1] );
    quat_quat2mat( Viewrotscale, quat );
    return 0;
}

int cm_end(int argc, char **argv)
{
    struct rt_i *rtip = ap.a_rt_i;

    if (argc < 0 || !argv) return -1;

    if ( rtip && BU_LIST_IS_EMPTY( &rtip->HeadRegion ) )  {
	def_tree( rtip );		/* Load the default trees */
    }

    /* If no matrix or az/el specified yet, use params from cmd line */
    if ( Viewrotscale[15] <= 0.0 )
	do_ae( azimuth, elevation );

    if ( do_frame( curframe ) < 0 )  return -1;
    return 0;
}

int cm_tree( int argc, const char **argv)
{
    register struct rt_i *rtip = ap.a_rt_i;
    struct bu_vls	times;

    if ( argc <= 1 )  {
	def_tree( rtip );		/* Load the default trees */
	return 0;
    }
    bu_vls_init( &times );

    rt_prep_timer();
    if ( rt_gettrees(rtip, argc-1, &argv[1], npsw) < 0 )
	bu_log("rt_gettrees(%s) FAILED\n", argv[0]);
    (void)rt_get_timer( &times, NULL );

    if (rt_verbosity & VERBOSE_STATS)
	bu_log("GETTREE: %s\n", bu_vls_addr(&times) );
    bu_vls_free( &times );
    return 0;
}

int cm_multiview( int argc, char **argv)
{
    register struct rt_i *rtip = ap.a_rt_i;
    size_t i;
    static int a[] = {
	35,   0,
	0,  90, 135, 180, 225, 270, 315,
	0,  90, 135, 180, 225, 270, 315
    };
    static int e[] = {
	25, 90,
	30, 30, 30, 30, 30, 30, 30,
	60, 60, 60, 60, 60, 60, 60
    };

    if (argc < 0 || !argv) return -1;

    if ( rtip && BU_LIST_IS_EMPTY( &rtip->HeadRegion ) )  {
	def_tree( rtip );		/* Load the default trees */
    }
    for ( i=0; i<(sizeof(a)/sizeof(a[0])); i++ )  {
	do_ae( (double)a[i], (double)e[i] );
	(void)do_frame( curframe++ );
    }
    return -1;	/* end RT by returning an error */
}

/*
 *			C M _ A N I M
 *
 *  Experimental animation code
 *
 *  Usage:  anim <path> <type> args
 */
int cm_anim(int argc, const char **argv)
{
    if (argc < 1 || !argv) return -1;
    if ( db_parse_anim( ap.a_rt_i->rti_dbip, argc, argv ) < 0 )  {
	bu_log("cm_anim:  %s %s failed\n", argv[1], argv[2]);
	return -1;		/* BAD */
    }
    return 0;
}

/*
 *			C M _ C L E A N
 *
 *  Clean out results of last rt_prep(), and start anew.
 */
int cm_clean(int argc, char **argv)
{
    if (argc < 0 || !argv) return -1;

    /* Allow lighting model to clean up (e.g. lights, materials, etc) */
    view_cleanup( ap.a_rt_i );

    rt_clean( ap.a_rt_i );

    return 0;
}

/*
 *			C M _ S E T
 *
 *  Allow variable values to be set or examined.
 */
int cm_set(int argc, char **argv)
{
    struct bu_vls	str;

    if ( argc <= 1 ) {
	bu_struct_print( "Generic and Application-Specific Parameter Values",
			 set_parse, (char *)0 );
	return 0;
    }
    bu_vls_init( &str );
    bu_vls_from_argv( &str, argc-1, (const char **)argv+1 );
    if ( bu_struct_parse( &str, set_parse, (char *)0 ) < 0 )  {
	bu_vls_free( &str );
	return -1;
    }
    bu_vls_free( &str );
    return 0;
}

/*
 *			C M _ A E
 */
int cm_ae( int argc, char **argv)
{
    if (argc < 1 || !argv) return -1;
    azimuth = atof(argv[1]);	/* set elevation and azimuth */
    elevation = atof(argv[2]);
    do_ae(azimuth, elevation);

    return 0;
}

/*
 *			C M _ O P T
 */
int cm_opt(int argc, char **argv)
{
    int old_bu_optind=bu_optind;	/* need to restore this value after calling get_args() */

    if ( get_args( argc, argv ) <= 0 ) {
	bu_optind = old_bu_optind;
	return -1;
    }
    bu_optind = old_bu_optind;
    return 0;
}


/*
 *  Command table for RT control script language
 */

struct command_tab rt_cmdtab[] = {
    {"start", "frame number", "start a new frame",
     cm_start,	2, 2},
    {"viewsize", "size in mm", "set view size",
     cm_vsize,	2, 2},
    {"eye_pt", "xyz of eye", "set eye point",
     cm_eyept,	4, 4},
    {"lookat_pt", "x y z [yflip]", "set eye look direction, in X-Y plane",
     cm_lookat_pt,	4, 5},
    {"viewrot", "4x4 matrix", "set view direction from matrix",
     cm_vrot,	17, 17},
    {"orientation", "quaturnion", "set view direction from quaturnion",
     cm_orientation,	5, 5},
    {"end", 	"", "end of frame setup, begin raytrace",
     cm_end,		1, 1},
    {"multiview", "", "produce stock set of views",
     cm_multiview,	1, 1},
    {"anim", 	"path type args", "specify articulation animation",
     cm_anim,	4, 999},
    {"tree", 	"treetop(s)", "specify alternate list of tree tops",
     cm_tree,	1, 999},
    {"clean", "", "clean articulation from previous frame",
     cm_clean,	1, 1},
    {"set", 	"", "show or set parameters",
     cm_set,		1, 999},
    {"ae", "azim elev", "specify view as azim and elev, in degrees",
     cm_ae,		3, 3},
    {"opt", "-flags", "set flags, like on command line",
     cm_opt,		2, 999},
    {(char *)0, (char *)0, (char *)0,
     0,		0, 0	/* END */}
};


/*
 *			D E F _ T R E E
 *
 *  Load default tree list, from command line.
 */
void
def_tree(register struct rt_i *rtip)
{
    struct bu_vls	times;

    RT_CK_RTI(rtip);

    bu_vls_init( &times );
    rt_prep_timer();
    if ( rt_gettrees(rtip, nobjs, (const char **)objtab, npsw) < 0 )
	bu_log("rt_gettrees(%s) FAILED\n", objtab[0]);
    (void)rt_get_timer( &times, NULL );

    if (rt_verbosity & VERBOSE_STATS)
	bu_log("GETTREE: %s\n", bu_vls_addr(&times));
    bu_vls_free( &times );
}

/*
 *			D O _ P R E P
 *
 *  This is a separate function primarily as a service to REMRT.
 */
void
do_prep(struct rt_i *rtip)
{
    struct bu_vls	times;

    RT_CHECK_RTI(rtip);
    if ( rtip->needprep )  {
	/* Allow lighting model to set up (e.g. lights, materials, etc) */
	view_setup(rtip);

	/* Allow RT library to prepare itself */
	bu_vls_init( &times );
	rt_prep_timer();
	rt_prep_parallel(rtip, npsw);

	(void)rt_get_timer( &times, NULL );
	if (rt_verbosity & VERBOSE_STATS)
	    bu_log( "PREP: %s\n", bu_vls_addr(&times) );
	bu_vls_free( &times );
    }
    if (rt_verbosity & VERBOSE_STATS)  {
	bu_log("%s: %d nu, %d cut, %d box (%d empty)\n",
	       rtip->rti_space_partition == RT_PART_NUGRID ?
	       "NUGrid" : "NUBSP",
	       rtip->rti_ncut_by_type[CUT_NUGRIDNODE],
	       rtip->rti_ncut_by_type[CUT_CUTNODE],
	       rtip->rti_ncut_by_type[CUT_BOXNODE],
	       rtip->nempty_cells );
    }
}


/*
 *			G R I D _ S E T U P
 *
 *  In theory, the grid can be specified by providing any two of
 *  these sets of parameters:
 *	number of pixels (width, height)
 *	viewsize (in model units, mm)
 *	number of grid cells (cell_width, cell_height)
 *  however, for now, it is required that the view size always be specified,
 *  and one or the other parameter be provided.
 */
void
grid_setup(void)
{
    vect_t temp;
    mat_t toEye;

    if ( viewsize <= 0.0 )
	bu_exit(EXIT_FAILURE, "viewsize <= 0");
    /* model2view takes us to eye_model location & orientation */
    MAT_IDN( toEye );
    MAT_DELTAS_VEC_NEG( toEye, eye_model );
    Viewrotscale[15] = 0.5*viewsize;	/* Viewscale */
    bn_mat_mul( model2view, Viewrotscale, toEye );
    bn_mat_inv( view2model, model2view );

    /* Determine grid cell size and number of pixels */
    if ( cell_newsize ) {
	if ( cell_width <= 0.0 ) cell_width = cell_height;
	if ( cell_height <= 0.0 ) cell_height = cell_width;
	width = (viewsize / cell_width) + 0.99;
	height = (viewsize / (cell_height*aspect)) + 0.99;
	cell_newsize = 0;
    } else {
	/* Chop -1.0..+1.0 range into parts */
	cell_width = viewsize / width;
	cell_height = viewsize / (height*aspect);
    }

    /* Create basis vectors dx and dy for emanation plane (grid) */
    VSET( temp, 1, 0, 0 );
    MAT3X3VEC( dx_unit, view2model, temp );	/* rotate only */
    VSCALE( dx_model, dx_unit, cell_width );

    VSET( temp, 0, 1, 0 );
    MAT3X3VEC( dy_unit, view2model, temp );	/* rotate only */
    VSCALE( dy_model, dy_unit, cell_height );

    /* "Lower left" corner of viewing plane */
    if ( rt_perspective > 0.0 )  {
	fastf_t	zoomout;
	zoomout = 1.0 / tan( bn_degtorad * rt_perspective / 2.0 );
	VSET( temp, -1, -1/aspect, -zoomout );	/* viewing plane */
	/*
	 * divergence is perspective angle divided by the number
	 * of pixels in that angle. Extra factor of 0.5 is because
	 * perspective is a full angle while divergence is the tangent
	 * (slope) of a half angle.
	 */
	ap.a_diverge = tan( bn_degtorad * rt_perspective * 0.5 / width );
	ap.a_rbeam = 0;
    }  else  {
	/* all rays go this direction */
	VSET( temp, 0, 0, -1 );
	MAT4X3VEC( ap.a_ray.r_dir, view2model, temp );
	VUNITIZE( ap.a_ray.r_dir );

	VSET( temp, -1, -1/aspect, 0 );	/* eye plane */
	ap.a_rbeam = 0.5 * viewsize / width;
	ap.a_diverge = 0;
    }
    if ( NEAR_ZERO(ap.a_rbeam, SMALL) && NEAR_ZERO(ap.a_diverge, SMALL) )
	bu_exit(EXIT_FAILURE, "zero-radius beam");
    MAT4X3PNT( viewbase_model, view2model, temp );

    if ( jitter & JITTER_FRAME )  {
	/* Move the frame in a smooth circular rotation in the plane */
	fastf_t		ang;	/* radians */
	fastf_t		dx, dy;

	ang = curframe * frame_delta_t * bn_twopi / 10;	/* 10 sec period */
	dx = cos(ang) * 0.5;	/* +/- 1/4 pixel width in amplitude */
	dy = sin(ang) * 0.5;
	VJOIN2( viewbase_model, viewbase_model,
		dx, dx_model,
		dy, dy_model );
    }

    if ( cell_width <= 0 || cell_width >= INFINITY ||
	 cell_height <= 0 || cell_height >= INFINITY )  {
	bu_log("grid_setup: cell size ERROR (%g, %g) mm\n",
	       cell_width, cell_height );
	bu_exit(EXIT_FAILURE, "cell size");
    }
    if ( width <= 0 || height <= 0 )  {
	bu_log("grid_setup: ERROR bad image size (%d, %d)\n",
	       width, height );
	bu_exit(EXIT_FAILURE, "bad size");
    }
}


/*
 *			D O _ F R A M E
 *
 *  Do all the actual work to run a frame.
 *
 *  Returns -1 on error, 0 if OK.
 */
int
do_frame(int framenumber)
{
    struct bu_vls	times;
    char framename[128] = {0};		/* File name to hold current frame */
    struct rt_i *rtip = ap.a_rt_i;
    double	utime = 0.0;			/* CPU time used */
    double	nutime = 0.0;			/* CPU time used, normalized by ncpu */
    double	wallclock;		/* seconds of wall clock time */
    vect_t	work, temp;
    quat_t	quat;

    if (rt_verbosity & VERBOSE_FRAMENUMBER)
	bu_log( "\n...................Frame %5d...................\n",
		framenumber);

    /* Compute model RPP, etc */
    do_prep( rtip );

    if (rt_verbosity & VERBOSE_VIEWDETAIL)
	bu_log("Tree: %d solids in %d regions\n", rtip->nsolids, rtip->nregions );

    if (rtip->nsolids <= 0)
	bu_exit(3, "ERROR: No primitives remaining.\n");

    if (rtip->nregions <= 0)
	bu_exit(3, "ERROR: No regions remaining.\n");

    if (rt_verbosity & VERBOSE_VIEWDETAIL)
	bu_log("Model: X(%g,%g), Y(%g,%g), Z(%g,%g)\n",
	       rtip->mdl_min[X], rtip->mdl_max[X],
	       rtip->mdl_min[Y], rtip->mdl_max[Y],
	       rtip->mdl_min[Z], rtip->mdl_max[Z] );

    /*
     *  Perform Grid setup.
     *  This may alter cell size or width/height.
     */
    grid_setup();
    /* az/el 0, 0 is when screen +Z is model +X */
    VSET( work, 0, 0, 1 );
    MAT3X3VEC( temp, view2model, work );
    bn_ae_vec( &azimuth, &elevation, temp );

    if (rt_verbosity & VERBOSE_VIEWDETAIL)
	bu_log(
	    "View: %g azimuth, %g elevation off of front view\n",
	    azimuth, elevation);
    quat_mat2quat( quat, model2view );
    if (rt_verbosity & VERBOSE_VIEWDETAIL) {
	bu_log("Orientation: %g, %g, %g, %g\n", V4ARGS(quat) );
	bu_log("Eye_pos: %g, %g, %g\n", V3ARGS(eye_model) );
	bu_log("Size: %gmm\n", viewsize);
	bu_log("Grid: (%g, %g) mm, (%d, %d) pixels\n",
	       cell_width, cell_height,
	       width, height );
	bu_log("Beam: radius=%g mm, divergence=%g mm/1mm\n",
	       ap.a_rbeam, ap.a_diverge );
    }

    /* Process -b and ??? options now, for this frame */
    if ( pix_start == -1 )  {
	pix_start = 0;
	pix_end = height * width - 1;
    }

    /* Allocate data for pixel map for rerendering of black pixels */
    if (pixmap == NULL) {
	pixmap = (unsigned char*)bu_calloc(sizeof(RGBpixel), (size_t)width*(size_t)height, "pixmap allocate");
    }

    /*
     *  Determine output file name
     *  On UNIX only, check to see if this is a "restart".
     */
    if ( outputfile != (char *)0 )  {
	if ( framenumber <= 0 )  {
	    snprintf( framename, 128, "%s", outputfile );
	}  else  {
	    snprintf( framename, 128, "%s.%d", outputfile, framenumber );
	}

	/* Ordinary case for creating output file */
	if ( outfp == NULL && (outfp = fopen( framename, "w+b" )) == NULL )  {
	    perror( framename );
	    if ( matflag )  return 0;	/* OK */
	    return -1;			/* Bad */
	}
	if (rt_verbosity & VERBOSE_OUTPUTFILE)
	    bu_log("Output file is '%s' %dx%d pixels\n",
		   framename, width, height);
    }

    /* initialize lighting, may update pix_start */
    view_2init( &ap, framename );

    rtip->nshots = 0;
    rtip->nmiss_model = 0;
    rtip->nmiss_tree = 0;
    rtip->nmiss_solid = 0;
    rtip->nmiss = 0;
    rtip->nhits = 0;
    rtip->rti_nrays = 0;

    if (rt_verbosity & (VERBOSE_LIGHTINFO|VERBOSE_STATS))
	bu_log("\n");
    fflush(stdout);
    fflush(stderr);

    /*
     *  Compute the image
     *  It may prove desirable to do this in chunks
     */
    rt_prep_timer();

    do_run( pix_start, pix_end );

    /* Reset values to full size, for next frame (if any) */
    pix_start = 0;
    pix_end = height*width - 1;

    bu_vls_init( &times );
    utime = rt_get_timer( &times, &wallclock );

    /*
     *  End of application.  Done outside of timing section.
     *  Typically, writes any remaining results out.
     */
    view_end( &ap );

    nutime = utime / npsw;

    /* prevent a bogus near-zero time to prevent infinate and near-infinate
     * results without relying on IEEE floating point zero comparison.
     */
    if (NEAR_ZERO(nutime, VDIVIDE_TOL)) {
	bu_log("WARNING:  Raytrace timings are likely to be meaningless\n");
	nutime = VDIVIDE_TOL;
    }

    /*
     *  All done.  Display run statistics.
     */
    if (rt_verbosity & VERBOSE_STATS)
	bu_log("SHOT: %s\n", bu_vls_addr( &times ) );
    bu_vls_free( &times );
    if (rt_verbosity & VERBOSE_STATS) {
	bu_log("%ld solid/ray intersections: %ld hits + %ld miss\n",
	       rtip->nshots, rtip->nhits, rtip->nmiss );
	bu_log("pruned %.1f%%:  %ld model RPP, %ld dups skipped, %ld solid RPP\n",
	       rtip->nshots>0?((double)rtip->nhits*100.0)/rtip->nshots:100.0,
	       rtip->nmiss_model, rtip->ndup, rtip->nmiss_solid );
	bu_log("Frame %2d: %10d pixels in %9.2f sec = %12.2f pixels/sec\n",
	       framenumber,
	       width*height, nutime, ((double)(width*height))/nutime );
	bu_log("Frame %2d: %10d rays   in %9.2f sec = %12.2f rays/sec (RTFM)\n",
	       framenumber,
	       rtip->rti_nrays, nutime, ((double)(rtip->rti_nrays))/nutime );
	bu_log("Frame %2d: %10d rays   in %9.2f sec = %12.2f rays/CPU_sec\n",
	       framenumber,
	       rtip->rti_nrays, utime, ((double)(rtip->rti_nrays))/utime );
	bu_log("Frame %2d: %10d rays   in %9.2f sec = %12.2f rays/sec (wallclock)\n",
	       framenumber,
	       rtip->rti_nrays,
	       wallclock, ((double)(rtip->rti_nrays))/wallclock );
    }
    if ( outfp != NULL )  {
	(void)fclose(outfp);
	outfp = NULL;
    }

    bu_log("\n");
    bu_free(pixmap, "pixmap allocate");
    pixmap = (unsigned char *)NULL;
    return 0;		/* OK */
}

/*
 *			D O _ A E
 *
 *  Compute the rotation specified by the azimuth and elevation
 *  parameters.  First, note that these are specified relative
 *  to the GIFT "front view", ie, model (X, Y, Z) is view (Z, X, Y):
 *  looking down X axis, Y right, Z up.
 *  A positive azimuth represents rotating the *eye* around the
 *  Y axis, or, rotating the *model* in -Y.
 *  A positive elevation represents rotating the *eye* around the
 *  X axis, or, rotating the *model* in -X.
 */
void
do_ae(double azim, double elev)
{
    vect_t	temp;
    vect_t	diag;
    mat_t	toEye;
    struct rt_i *rtip = ap.a_rt_i;

    if (rtip->nsolids <= 0)
	bu_exit(EXIT_FAILURE, "ERROR: no primitives active\n");

    if (rtip->nregions <= 0)
	bu_exit(EXIT_FAILURE, "ERROR: no regions active\n");

    if ( rtip->mdl_max[X] >= INFINITY ) {
	bu_log("do_ae: infinite model bounds? setting a unit minimum\n");
	VSETALL( rtip->mdl_min, -1 );
    }
    if ( rtip->mdl_max[X] <= -INFINITY ) {
	bu_log("do_ae: infinite model bounds? setting a unit maximum\n");
	VSETALL( rtip->mdl_max, 1 );
    }

    /*
     *  Enlarge the model RPP just slightly, to avoid nasty
     *  effects with a solid's face being exactly on the edge
     *  NOTE:  This code is duplicated out of librt/tree.c/rt_prep(),
     *  and has to appear here to enable the viewsize calculation to
     *  match the final RPP.
     */
    rtip->mdl_min[X] = floor( rtip->mdl_min[X] );
    rtip->mdl_min[Y] = floor( rtip->mdl_min[Y] );
    rtip->mdl_min[Z] = floor( rtip->mdl_min[Z] );
    rtip->mdl_max[X] = ceil( rtip->mdl_max[X] );
    rtip->mdl_max[Y] = ceil( rtip->mdl_max[Y] );
    rtip->mdl_max[Z] = ceil( rtip->mdl_max[Z] );

    MAT_IDN( Viewrotscale );
    bn_mat_angles( Viewrotscale, 270.0+elev, 0.0, 270.0-azim );

    /* Look at the center of the model */
    MAT_IDN( toEye );
    toEye[MDX] = -((rtip->mdl_max[X]+rtip->mdl_min[X])/2.0);
    toEye[MDY] = -((rtip->mdl_max[Y]+rtip->mdl_min[Y])/2.0);
    toEye[MDZ] = -((rtip->mdl_max[Z]+rtip->mdl_min[Z])/2.0);

    /* Fit a sphere to the model RPP, diameter is viewsize,
     * unless viewsize command used to override.
     */
    if ( viewsize <= 0 ) {
	VSUB2( diag, rtip->mdl_max, rtip->mdl_min );
	viewsize = MAGNITUDE( diag );
	if ( aspect > 1 ) {
	    /* don't clip any of the image when autoscaling */
	    viewsize *= aspect;
	}
    }
    Viewrotscale[15] = 0.5*viewsize;	/* Viewscale */
    bn_mat_mul( model2view, Viewrotscale, toEye );
    bn_mat_inv( view2model, model2view );
    VSET( temp, 0, 0, eye_backoff );
    MAT4X3PNT( eye_model, view2model, temp );
}


/***********************************************************************
 *
 *  compute_point
 *
 * Compute the origin for this ray, based upon the number of samples
 * per pixel and the number of the current sample.  For certain
 * ray-counts, it is highly advantageous to subdivide the pixel and
 * fire each ray in a specific sub-section of the pixel.
 */
static void
jitter_start_pt(vect_t point, struct application *a, int samplenum, int pat_num)
{
    fastf_t dx, dy;

    if (pat_num >= 0) {
	dx = a->a_x + pt_pats[pat_num].coords[samplenum*2] +
	    (bn_rand_half(a->a_resource->re_randptr) *
	     pt_pats[pat_num].rand_scale[X] );

	dy = a->a_y + pt_pats[pat_num].coords[samplenum*2 + 1] +
	    (bn_rand_half(a->a_resource->re_randptr) *
	     pt_pats[pat_num].rand_scale[Y] );
    } else {
	dx = a->a_x + bn_rand_half(a->a_resource->re_randptr);
	dy = a->a_y + bn_rand_half(a->a_resource->re_randptr);
    }
    VJOIN2( point, viewbase_model, dx, dx_model, dy, dy_model );
}


void do_pixel(int cpu,
	      int pat_num,
	      int pixelnum)
{
    struct	application	a;
    struct	pixel_ext	pe;
    vect_t			point;		/* Ref point on eye or view plane */
    vect_t			colorsum = {(fastf_t)0.0, (fastf_t)0.0, (fastf_t)0.0};
    int				samplenum = 0;
    static const double one_over_255 = 1.0 / 255.0;
    const int pindex = (pixelnum * sizeof(RGBpixel));

    /* Obtain fresh copy of global application struct */
    a = ap;				/* struct copy */
    a.a_resource = &resource[cpu];

    a.a_y = pixelnum/width;
    a.a_x = pixelnum - (a.a_y * width);

    /* Check the pixel map to determine if this image should be rendered or not */
    if (pixmap) {
	a.a_user= 1;	/* Force Shot Hit */

	if (pixmap[pindex + RED] + pixmap[pindex + GRN] + pixmap[pindex + BLU]) {
	    /* non-black pixmap pixel */

	    a.a_color[RED]= (double)(pixmap[pindex + RED]) * one_over_255;
	    a.a_color[GRN]= (double)(pixmap[pindex + GRN]) * one_over_255;
	    a.a_color[BLU]= (double)(pixmap[pindex + BLU]) * one_over_255;

	    /* we're done */
	    view_pixel( &a );
	    if ( a.a_x == width-1 ) {
		view_eol( &a );		/* End of scan line */
	    }
	    return;
	}
    }

    /* our starting point, used for non-jitter */
    VJOIN2 (point, viewbase_model, a.a_x, dx_model, a.a_y, dy_model);

    /* not tracing the corners of a prism by default */
    a.a_pixelext=(struct pixel_ext *)NULL;

    /* black or no pixmap, so compute the pixel(s) */

    /* LOOP BELOW IS UNROLLED ONE SAMPLE SINCE THAT'S THE COMMON CASE.
     *
     * XXX - If you edit the unrolled or non-unrolled section, be sure
     * to edit the other section.
     */
    if (hypersample == 0) {
	/* not hypersampling, so just do it */

	/****************/
	/* BEGIN UNROLL */
	/****************/

	if (jitter & JITTER_CELL ) {
	    jitter_start_pt(point, &a, samplenum, pat_num);
	}

	if (a.a_rt_i->rti_prismtrace) {
	    /* compute the four corners */
	    pe.magic = PIXEL_EXT_MAGIC;
	    VJOIN2(pe.corner[0].r_pt, viewbase_model, a.a_x, dx_model, a.a_y, dy_model );
	    VJOIN2(pe.corner[1].r_pt, viewbase_model, (a.a_x+1), dx_model, a.a_y, dy_model );
	    VJOIN2(pe.corner[2].r_pt, viewbase_model, (a.a_x+1), dx_model, (a.a_y+1), dy_model );
	    VJOIN2(pe.corner[3].r_pt, viewbase_model, a.a_x, dx_model, (a.a_y+1), dy_model );
	    a.a_pixelext = &pe;
	}

	if ( rt_perspective > 0.0 )  {
	    VSUB2( a.a_ray.r_dir, point, eye_model );
	    VUNITIZE( a.a_ray.r_dir );
	    VMOVE( a.a_ray.r_pt, eye_model );
	    if (a.a_rt_i->rti_prismtrace) {
		VSUB2(pe.corner[0].r_dir, pe.corner[0].r_pt, eye_model);
		VSUB2(pe.corner[1].r_dir, pe.corner[1].r_pt, eye_model);
		VSUB2(pe.corner[2].r_dir, pe.corner[2].r_pt, eye_model);
		VSUB2(pe.corner[3].r_dir, pe.corner[3].r_pt, eye_model);
	    }
	} else {
	    VMOVE( a.a_ray.r_pt, point );
	    VMOVE( a.a_ray.r_dir, ap.a_ray.r_dir );

	    if (a.a_rt_i->rti_prismtrace) {
		VMOVE(pe.corner[0].r_dir, a.a_ray.r_dir);
		VMOVE(pe.corner[1].r_dir, a.a_ray.r_dir);
		VMOVE(pe.corner[2].r_dir, a.a_ray.r_dir);
		VMOVE(pe.corner[3].r_dir, a.a_ray.r_dir);
	    }
	}

	a.a_level = 0;		/* recursion level */
	a.a_purpose = "main ray";
	(void)rt_shootray( &a );

	VADD2( colorsum, colorsum, a.a_color );

	/**************/
	/* END UNROLL */
	/**************/

    } else {
	/* hypersampling, so iterate */

	for ( samplenum=0; samplenum<=hypersample; samplenum++ )  {
	    /* shoot at a point based on the jitter pattern number */

	    /**********************/
	    /* BEGIN NON-UNROLLED */
	    /**********************/

	    if (jitter & JITTER_CELL ) {
		jitter_start_pt(point, &a, samplenum, pat_num);
	    }

	    if (a.a_rt_i->rti_prismtrace) {
		/* compute the four corners */
		pe.magic = PIXEL_EXT_MAGIC;
		VJOIN2(pe.corner[0].r_pt, viewbase_model, a.a_x, dx_model, a.a_y, dy_model );
		VJOIN2(pe.corner[1].r_pt, viewbase_model, (a.a_x+1), dx_model, a.a_y, dy_model );
		VJOIN2(pe.corner[2].r_pt, viewbase_model, (a.a_x+1), dx_model, (a.a_y+1), dy_model );
		VJOIN2(pe.corner[3].r_pt, viewbase_model, a.a_x, dx_model, (a.a_y+1), dy_model );
		a.a_pixelext = &pe;
	    }

	    if ( rt_perspective > 0.0 )  {
		VSUB2( a.a_ray.r_dir, point, eye_model );
		VUNITIZE( a.a_ray.r_dir );
		VMOVE( a.a_ray.r_pt, eye_model );
		if (a.a_rt_i->rti_prismtrace) {
		    VSUB2(pe.corner[0].r_dir, pe.corner[0].r_pt, eye_model);
		    VSUB2(pe.corner[1].r_dir, pe.corner[1].r_pt, eye_model);
		    VSUB2(pe.corner[2].r_dir, pe.corner[2].r_pt, eye_model);
		    VSUB2(pe.corner[3].r_dir, pe.corner[3].r_pt, eye_model);
		}
	    } else {
		VMOVE( a.a_ray.r_pt, point );
		VMOVE( a.a_ray.r_dir, ap.a_ray.r_dir );

		if (a.a_rt_i->rti_prismtrace) {
		    VMOVE(pe.corner[0].r_dir, a.a_ray.r_dir);
		    VMOVE(pe.corner[1].r_dir, a.a_ray.r_dir);
		    VMOVE(pe.corner[2].r_dir, a.a_ray.r_dir);
		    VMOVE(pe.corner[3].r_dir, a.a_ray.r_dir);
		}
	    }

	    a.a_level = 0;		/* recursion level */
	    a.a_purpose = "main ray";
	    (void)rt_shootray( &a );

	    VADD2( colorsum, colorsum, a.a_color );

	    /********************/
	    /* END NON-UNROLLED */
	    /********************/
	} /* for samplenum <= hypersample */

	{
	    /* scale the hypersampled results */
	    fastf_t f;
	    f = 1.0 / (hypersample+1);
	    VSCALE( a.a_color, colorsum, f );
	}
    } /* end unrolling else case */

    /* bu_log("2: [%d,%d] : [%.2f,%.2f,%.2f]\n", pixelnum%width, pixelnum/width, a.a_color[0], a.a_color[1], a.a_color[2]); */

    /* we're done */
    view_pixel( &a );
    if ( a.a_x == width-1 ) {
	view_eol( &a );		/* End of scan line */
    }
    return;
}


/*
 *  			W O R K E R
 *
 *  Compute some pixels, and store them.
 *  A "self-dispatching" parallel algorithm.
 *  Executes until there is no more work to be done, or is told to stop.
 *
 *  In order to reduce the traffic through the res_worker critical section,
 *  a multiple pixel block may be removed from the work queue at once.
 *
 *  For a general-purpose version, see LIBRT rt_shoot_many_rays()
 */
void
worker(int cpu, genptr_t arg)
{
    int	pixel_start;
    int	pixelnum;
    int	pat_num = -1;

    if (cpu < 0 && !arg) return;

    /* The more CPUs at work, the bigger the bites we take */
    if ( per_processor_chunk <= 0 )  per_processor_chunk = npsw;

    if ( cpu >= MAX_PSW )  {
	bu_log("rt/worker() cpu %d > MAX_PSW %d, array overrun\n", cpu, MAX_PSW);
	bu_exit(EXIT_FAILURE, "rt/worker() cpu > MAX_PSW, array overrun\n");
    }
    RT_CK_RESOURCE( &resource[cpu] );

    pat_num = -1;
    if (hypersample) {
	int i, ray_samples;

	ray_samples = hypersample + 1;
	for (i=0; pt_pats[i].num_samples != 0; i++) {
	    if (pt_pats[i].num_samples == ray_samples) {
		pat_num = i;
		goto pat_found;
	    }
	}
    }
 pat_found:

    while (1) {
	bu_semaphore_acquire(RT_SEM_WORKER);
	pixel_start = cur_pixel;
	cur_pixel += per_processor_chunk;
	bu_semaphore_release(RT_SEM_WORKER);

	for (pixelnum = pixel_start; pixelnum < pixel_start+per_processor_chunk; pixelnum++) {

	    if (pixelnum > last_pixel)
		return;

	    do_pixel(cpu, pat_num, pixelnum);
	}
    }
}


/*
 *			D O _ R U N
 *
 *  Compute a run of pixels, in parallel if the hardware permits it.
 *
 *  For a general-purpose version, see LIBRT rt_shoot_many_rays()
 */
void do_run( int a, int b )
{
    int		cpu;

    cur_pixel = a;
    last_pixel = b;

    if ( !rt_g.rtg_parallel )  {
	/*
	 * SERIAL case -- one CPU does all the work.
	 */
	npsw = 1;
	worker(0, NULL);
    } else {
	/*
	 *  Parallel case.
	 */

	/* hack to bypass a bug in the Linux 2.4 kernel pthreads
	 * implementation. cpu statistics are only traceable on a
	 * process level and the timers will report effectively no
	 * elapsed cpu time.  this allows the stats of all threads
	 * to be gathered up by an encompassing process that may
	 * be timed.
	 *
	 * XXX this should somehow only apply to a build on a 2.4
	 * linux kernel.
	 */
	bu_parallel( worker, npsw, NULL );

    } /* end parallel case */

    /* Tally up the statistics */
    for ( cpu=0; cpu < npsw; cpu++ ) {
	rt_add_res_stats( ap.a_rt_i, &resource[cpu] );
    }
    return;
}

/*
 *  			V I E W _ P I X E L
 *
 *  Arrange to have the pixel output.
 *  a_uptr has region pointer, for reference.
 */
void
view_pixel(register struct application *ap)
{
    register int	r, g, b;

    if ( ap->a_user == 0 )  {
	/* Shot missed the model, don't dither */
	r = ibackground[0];
	g = ibackground[1];
	b = ibackground[2];
	VSETALL( ap->a_color, -1e-20 );	/* background flag */
    } else {
	r = ap->a_color[0]*255.+bn_rand0to1(ap->a_resource->re_randptr);
	g = ap->a_color[1]*255.+bn_rand0to1(ap->a_resource->re_randptr);
	b = ap->a_color[2]*255.+bn_rand0to1(ap->a_resource->re_randptr);

	if ( r > 255 ) r = 255;
	else if ( r < 0 )  r = 0;
	if ( g > 255 ) g = 255;
	else if ( g < 0 )  g = 0;
	if ( b > 255 ) b = 255;
	else if ( b < 0 )  b = 0;
	if ( r == ibackground[0] && g == ibackground[1] &&
	     b == ibackground[2] )  {
	    r = inonbackground[0];
	    g = inonbackground[1];
	    b = inonbackground[2];
	}

	/* Make sure it's never perfect black */
	if (r==0 && g==0 && b==0 && benchmark==0)
	    b = 1;
    }

    {
	RGBpixel	p;
	int		npix;

	p[0] = r;
	p[1] = g;
	p[2] = b;

	if ( outfp != NULL )  {
	    bu_semaphore_acquire( BU_SEM_SYSCALL );
	    if ( fseek( outfp, (ap->a_y*width*3) + (ap->a_x*3), 0 ) != 0 )
		fprintf(stderr, "fseek error\n");
	    if ( fwrite( p, 3, 1, outfp ) != 1 )
		bu_exit(EXIT_FAILURE, "pixel fwrite error");
	    bu_semaphore_release( BU_SEM_SYSCALL);
	}

	if ( fbp != FBIO_NULL )  {
	    /* Framebuffer output */
	    bu_semaphore_acquire( BU_SEM_SYSCALL );
	    npix = fb_write( fbp, ap->a_x, ap->a_y, (unsigned char *)p, 1 );
	    bu_semaphore_release( BU_SEM_SYSCALL);
	    if ( npix < 1 )  bu_exit(EXIT_FAILURE, "pixel fb_write error");
	}
    }
    return;
}

/*
 *  			V I E W _ E O L
 *
 *  This routine is not used;  view_pixel() determines when the last
 *  pixel of a scanline is really done, for parallel considerations.
 */
void
view_eol(register struct application *ap)
{
    RT_CK_APPLICATION(ap);
}

/*
 *			V I E W _ E N D
 */
void
view_end(struct application *ap)
{
    RT_CK_APPLICATION(ap);
}

/*
 *			V I E W _ S E T U P
 *
 *  Called before rt_prep() in do.c
 */
void
view_setup(struct rt_i *rtip)
{
    register struct region *regp;

    RT_CHECK_RTI(rtip);
    /*
     *  Initialize the material library for all regions.
     *  As this may result in some regions being dropped,
     *  (eg, light solids that become "implicit" -- non drawn),
     *  this must be done before allowing the library to prep
     *  itself.  This is a slight layering violation;  later it
     *  may be clear how to repackage this operation.
     */
    regp = BU_LIST_FIRST( region, &rtip->HeadRegion );
    while ( BU_LIST_NOT_HEAD( regp, &rtip->HeadRegion ) )  {
	switch ( mlib_setup( &mfHead, regp, rtip ) )  {
	    case -1:
	    default:
		bu_log("mlib_setup failure on %s\n", regp->reg_name);
		break;
	    case 0:
	    {
		struct region *r = BU_LIST_NEXT( region, &regp->l );
		/* zap reg_udata? beware of light structs */
		rt_del_regtree( rtip, regp, &rt_uniresource );
		regp = r;
		continue;
	    }
	    case 1:
		/* Full success */
		break;
	    case 2:
		/* Full success, and this region should get dropped later */
		/* Add to list of regions to drop */
		bu_ptbl_ins( &rtip->delete_regs, (long *)regp );
		break;
	}
	regp = BU_LIST_NEXT( region, &regp->l );
    }
}

/*
 *			V I E W _ R E _ S E T U P
 *
 *	This routine is used to do a "mlib_setup" on reprepped regions.
 *	only regions with a NULL reg_mfuncs pointer will be processed.
 */
void
view_re_setup( struct rt_i *rtip )
{
    struct region *rp;

    rp = BU_LIST_FIRST( region, &(rtip->HeadRegion) );
    while ( BU_LIST_NOT_HEAD( rp, &(rtip->HeadRegion) ) ) {
	if ( !rp->reg_mfuncs ) {
	    switch ( mlib_setup( &mfHead, rp, rtip ) ) {
		default:
		case -1:
		    bu_log( "view_re_setup(): mlib_setup failed for region %s\n", rp->reg_name );
		    break;
		case 0:
		{
		    struct region *r = BU_LIST_NEXT( region, &rp->l );
		    /* zap reg_udata? beware of light structs */
		    rt_del_regtree( rtip, rp, &rt_uniresource );
		    rp = r;
		    continue;
		}
		case 1:
		    break;
	    }
	}
	rp = BU_LIST_NEXT( region, &rp->l );
    }
}

/*
 *			V I E W _ C L E A N U P
 *
 *  Called before rt_clean() in do.c
 */
void view_cleanup(struct rt_i	*rtip)
{
    register struct region	*regp;

    RT_CHECK_RTI(rtip);
    for ( BU_LIST_FOR( regp, region, &(rtip->HeadRegion) ) )  {
	mlib_free( regp );
    }
    if ( env_region.reg_mfuncs )  {
	bu_free( (char *)env_region.reg_name, "env_region.reg_name" );
	env_region.reg_name = (char *)0;
	mlib_free( &env_region );
    }

    light_cleanup();
}

/*
 *			H I T _ N O T H I N G
 *
 *  a_miss() routine called when no part of the model is hit.
 *  Background texture mapping could be done here.
 *  For now, return a pleasant dark blue.
 */
static int hit_nothing(register struct application *ap)
{
    if ( env_region.reg_mfuncs )  {
	struct gunk {
	    struct partition part;
	    struct hit	hit;
	    struct shadework sw;
	} u;

	memset((char *)&u, 0, sizeof(u));
	/* Make "miss" hit the environment map */
	/* Build up the fakery */
	u.part.pt_magic = PT_MAGIC;
	u.part.pt_inhit = u.part.pt_outhit = &u.hit;
	u.part.pt_regionp = &env_region;
	u.hit.hit_magic = RT_HIT_MAGIC;
	u.hit.hit_dist = ap->a_rt_i->rti_radius * 2;	/* model diam */
	u.hit.hit_rayp = &ap->a_ray;

	u.sw.sw_transmit = u.sw.sw_reflect = 0.0;
	u.sw.sw_refrac_index = 1.0;
	u.sw.sw_extinction = 0;
	u.sw.sw_xmitonly = 1;		/* don't shade env map! */

	/* "Surface" Normal points inward, UV is azim/elev of ray */
	u.sw.sw_inputs = MFI_NORMAL|MFI_UV;
	VREVERSE( u.sw.sw_hit.hit_normal, ap->a_ray.r_dir );
	/* U is azimuth, atan() range: -pi to +pi */
	u.sw.sw_uv.uv_u = bn_atan2( ap->a_ray.r_dir[Y],
				    ap->a_ray.r_dir[X] ) * bn_inv2pi;
	if ( u.sw.sw_uv.uv_u < 0 )
	    u.sw.sw_uv.uv_u += 1.0;
	/*
	 *  V is elevation, atan() range: -pi/2 to +pi/2,
	 *  because sqrt() ensures that X parameter is always >0
	 */
	u.sw.sw_uv.uv_v = bn_atan2( ap->a_ray.r_dir[Z],
				    sqrt( ap->a_ray.r_dir[X] * ap->a_ray.r_dir[X] +
					  ap->a_ray.r_dir[Y] * ap->a_ray.r_dir[Y]) ) *
	    bn_invpi + 0.5;
	u.sw.sw_uv.uv_du = u.sw.sw_uv.uv_dv = 0;

	VSETALL( u.sw.sw_color, 1 );
	VSETALL( u.sw.sw_basecolor, 1 );

	(void)viewshade( ap, &u.part, &u.sw );

	VMOVE( ap->a_color, u.sw.sw_color );
	ap->a_user = 1;		/* Signal view_pixel:  HIT */
	ap->a_uptr = (genptr_t)&env_region;
	return 1;
    }

    ap->a_user = 0;		/* Signal view_pixel:  MISS */
    VMOVE( ap->a_color, background );	/* In case someone looks */
    return 0;
}

/*
 *			C O L O R V I E W
 *
 *  Manage the coloring of whatever it was we just hit.
 *  This can be a recursive procedure.
 */
int
colorview(register struct application *ap, struct partition *PartHeadp, struct seg *finished_segs)
{
    register struct partition *pp;
    register struct hit *hitp;
    struct shadework sw;

    pp = PartHeadp->pt_forw;
    if ( ap->a_flag == 1 )
    {
	/* This ray is an escaping internal ray after refraction through glass.
	 * Sometimes, after refraction and starting a new ray at the glass exit,
	 * the new ray hits a sliver of the same glass, and gets confused. This bit
	 * of code attempts to spot this behavior and skip over the glass sliver.
	 * Any sliver less than 0.05mm thick will be skipped (0.05 is a SWAG).
	 */
	if ( (genptr_t)pp->pt_regionp == ap->a_uptr &&
	     pp->pt_forw != PartHeadp &&
	     pp->pt_outhit->hit_dist - pp->pt_inhit->hit_dist < 0.05 )
	    pp = pp->pt_forw;
    }

    for (; pp != PartHeadp; pp = pp->pt_forw )
	if ( pp->pt_outhit->hit_dist >= 0.0 ) break;

    if ( pp == PartHeadp )  {
	bu_log("colorview:  no hit out front?\n");
	return 0;
    }


    RT_CK_PT(pp);
    hitp = pp->pt_inhit;
    RT_CK_HIT(hitp);
    RT_CK_RAY(hitp->hit_rayp);
    ap->a_uptr = (genptr_t)pp->pt_regionp;	/* note which region was shaded */

    if ( hitp->hit_dist >= INFINITY )  {
	bu_log("colorview:  entry beyond infinity\n");
	VSET( ap->a_color, .5, 0, 0 );
	ap->a_user = 1;		/* Signal view_pixel:  HIT */
	ap->a_dist = hitp->hit_dist;
	goto out;
    }

    /* Check to see if eye is "inside" the solid
     * It might only be worthwhile doing all this in perspective mode
     * XXX Note that hit_dist can be faintly negative, e.g. -1e-13
     *
     * XXX we should certainly only do this if the eye starts out inside
     *  an opaque solid.  If it starts out inside glass or air we don't
     *  really want to do this
     */

    if ( hitp->hit_dist < 0.0 && pp->pt_regionp->reg_aircode == 0 ) {
	struct application sub_ap;
	fastf_t f;

	if ( pp->pt_outhit->hit_dist >= INFINITY ||
	     ap->a_level > max_bounces )  {
	    VSETALL( ap->a_color, 0.18 );	/* 18% Grey */
	    ap->a_user = 1;		/* Signal view_pixel:  HIT */
	    ap->a_dist = hitp->hit_dist;
	    goto out;
	}
	/* Push on to exit point, and trace on from there */
	sub_ap = *ap;	/* struct copy */
	sub_ap.a_level = ap->a_level+1;
	f = pp->pt_outhit->hit_dist+hitp->hit_dist+0.0001;
	VJOIN1(sub_ap.a_ray.r_pt, ap->a_ray.r_pt, f, ap->a_ray.r_dir);
	sub_ap.a_purpose = "pushed eye position";
	(void)rt_shootray( &sub_ap );

	/* The eye is inside a solid and we are "Looking out" so
	 * we are going to darken what we see beyond to give a visual
	 * cue that something is wrong.
	 */
	VSCALE( ap->a_color, sub_ap.a_color, 0.80 );

	ap->a_user = 1;		/* Signal view_pixel: HIT */
	ap->a_dist = f + sub_ap.a_dist;
	ap->a_uptr = sub_ap.a_uptr;	/* which region */
	goto out;
    }

    memset((char *)&sw, 0, sizeof(sw));
    sw.sw_transmit = sw.sw_reflect = 0.0;
    sw.sw_refrac_index = 1.0;
    sw.sw_extinction = 0;
    sw.sw_xmitonly = 0;		/* want full data */
    sw.sw_inputs = 0;		/* no fields filled yet */
    sw.sw_frame = curframe;
    sw.sw_pixeltime = sw.sw_frametime = curframe * frame_delta_t;
    sw.sw_segs = finished_segs;
    VSETALL( sw.sw_color, 1 );
    VSETALL( sw.sw_basecolor, 1 );

    /* individual shaders must handle reflection & refraction */
    (void)viewshade( ap, pp, &sw );

    VMOVE( ap->a_color, sw.sw_color );
    ap->a_user = 1;		/* Signal view_pixel:  HIT */
    /* XXX This is always negative when eye is inside air solid */
    ap->a_dist = hitp->hit_dist;

 out:
    RT_CK_REGION(ap->a_uptr);
    return 1;
}


/*
 *  			V I E W _ I N I T
 *
 *  Called once, early on in RT setup, before view size is set.
 */
int
view_init(register struct application *ap, char *file, char *obj, int minus_o, int minus_F)
{
    RT_CK_APPLICATION(ap);
    if (!file && !obj && !ap) return -1; /* unreached */

    if (rt_verbosity & VERBOSE_LIBVERSIONS)
	bu_log("%s", optical_version());

    optical_shader_init(&mfHead);	/* in liboptical/init.c */

    if (minus_F || (!minus_o && !minus_F)) {
	return 1;		/* open a framebuffer */
    }
    return 0;
}

void
collect_soltabs( struct bu_ptbl *stp_list, union tree *tr )
{
    switch ( tr->tr_op ) {
	case OP_UNION:
	case OP_INTERSECT:
	case OP_XOR:
	    collect_soltabs( stp_list, tr->tr_b.tb_left );
	    collect_soltabs( stp_list, tr->tr_b.tb_right );
	    break;
	case OP_SUBTRACT:
	    collect_soltabs( stp_list, tr->tr_b.tb_left );
	    break;
	case OP_SOLID:
	    bu_ptbl_ins( stp_list, (long *)tr->tr_a.tu_stp );
	    break;
    }
}

/*
 *  			V I E W 2 _ I N I T
 *
 *  Called each time a new image is about to be done.
 */
void
view_2init(register struct application *ap, char *framename)
{
    register int i;
    struct bu_ptbl stps;

    RT_CK_APPLICATION(ap);
    if (!ap && !framename) return;

    ap->a_refrac_index = 1.0;	/* RI_AIR -- might be water? */
    ap->a_cumlen = 0.0;
    ap->a_miss = hit_nothing;
    ap->a_onehit = a_onehit;

    bu_log("Single pixel I/O, unbuffered\n");

    ap->a_hit = colorview;

    /* If user did not specify any light sources then
     *	create default light sources
     */
    if ( BU_LIST_IS_EMPTY( &(LightHead.l) )  ||
	 BU_LIST_UNINITIALIZED( &(LightHead.l ) ) )  {
	if (R_DEBUG&RDEBUG_SHOWERR)bu_log("No explicit light\n");
	light_maker(1, view2model);
    }
    ap->a_rt_i->rti_nlights = light_init(ap);

    /* Now OK to delete invisible light regions.
     * Actually we just remove the references to these regions
     * from the soltab structures in the space paritioning tree
     */
    bu_ptbl_init( &stps, 8, "soltabs to delete" );
    for ( i=0; i<BU_PTBL_LEN( &ap->a_rt_i->delete_regs ); i++ ) {
	struct region *rp;
	struct soltab *stp;
	int j;


	rp = (struct region *)BU_PTBL_GET( &ap->a_rt_i->delete_regs, i );

	/* make a list of soltabs containing primitives referenced by
	 * invisible light regions
	 */
	collect_soltabs( &stps, rp->reg_treetop );

	/* remove the invisible light region pointers from the soltab structs */
	for ( j=0; j<BU_PTBL_LEN( &stps ); j++ ) {
	    int k;
	    struct region *rp2;
	    stp = (struct soltab *)BU_PTBL_GET( &stps, j );

	    k = BU_PTBL_LEN( &stp->st_regions ) - 1;
	    for (; k>=0; k-- ) {
		rp2 = (struct region *)BU_PTBL_GET( &stp->st_regions, k );
		if ( rp2 == rp ) {
		    bu_ptbl_rm( &stp->st_regions, (long *)rp2 );
		}
	    }

	}

	bu_ptbl_reset( &stps );
    }
    bu_ptbl_free( &stps );

    /* Create integer version of background color */
    inonbackground[0] = ibackground[0] = background[0] * 255.0 + 0.5;
    inonbackground[1] = ibackground[1] = background[1] * 255.0 + 0.5;
    inonbackground[2] = ibackground[2] = background[2] * 255.0 + 0.5;

    /*
     * If a non-background pixel comes out the same color as the
     * background, modify it slightly, to permit compositing.
     * Perturb the background color channel with the largest intensity.
     */
    if ( inonbackground[0] > inonbackground[1] )  {
	if ( inonbackground[0] > inonbackground[2] )  i = 0;
	else i = 2;
    } else {
	if ( inonbackground[1] > inonbackground[2] ) i = 1;
	else i = 2;
    }
    if ( inonbackground[i] < 127 ) inonbackground[i]++;
    else inonbackground[i]--;

}


/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
