/*                           O P T . C
 * BRL-CAD
 *
 * Copyright (c) 1989-2011 United States Government as represented by
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
/** @file rt/opt.c
 *
 * Option handling for Ray Tracing main program.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <math.h>
#include <string.h>

#include "vmath.h"
#include "raytrace.h"
#include "fb.h"

#include "./rtuif.h"
#include "./ext.h"


/* Note: struct parsing requires no space after the commas.  take care
 * when formatting this file.  if the compile breaks here, it means
 * that spaces got inserted incorrectly.
 */
#define COMMA ','


int		rpt_dist = 0;		/* report distance to each pixel */
size_t		width = 0;		/* # of pixels in X */
size_t		height = 0;		/* # of lines in Y */


/***** Variables shared with viewing model *** */
int		doubles_out = 0;	/* u_char or double .pix output file */
double		azimuth = 0.0, elevation = 0.0;
int		lightmodel = 0;		/* Select lighting model */
int		rpt_overlap = 1;	/* report overlapping region names */
int		default_background = 1; /* Default is black */
int		output_is_binary = 1;	/* !0 means output file is binary */

/***** end of sharing with viewing model *****/

/***** variables shared with worker() ******/
int		query_x = 0;
int		query_y = 0;
int		Query_one_pixel = 0;
int		query_rdebug  = 0;
int		query_debug = 0;
int		stereo = 0;		/* stereo viewing */
int		hypersample = 0;		/* number of extra rays to fire */
unsigned int	jitter = 0;		/* ray jitter control variable */
fastf_t		rt_perspective = (fastf_t)0.0;	/* presp (degrees X) 0 => ortho */
fastf_t		aspect = (fastf_t)1.0;	/* view aspect ratio X/Y (needs to be 1.0 for g/G options) */
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
int		incr_mode = 0;		/* !0 for incremental resolution */
int             full_incr_mode = 0;     /* !0 for fully incremental resolution */
size_t		incr_level = 0;		/* current incremental level */
size_t		incr_nlevel = 0;	/* number of levels */
size_t          full_incr_sample = 0;    /* current fully incremental sample */
size_t          full_incr_nsamples = 0;  /* number of samples in the fully incremental mode */
int		npsw = 1;		/* number of worker PSWs to run */
struct resource	resource[MAX_PSW];	/* memory resources */
int		transpose_grid = 0;     /* reverse the order of grid traversal */
int             random_mode = 0;        /* Mode to shoot rays at random directions */
/***** end variables shared with worker() *****/

/***** Photon Mapping Variables *****/
double		pmargs[9];
char		pmfile[255];
/***** ************************ *****/

/***** variables shared with do.c *****/
char		*string_pix_start = (char *)NULL;	/* string spec of starting pixel */
char		*string_pix_end = (char *)NULL;	/* string spec of ending pixel */
int		pix_start = -1;		/* pixel to start at */
int		pix_end = 0;		/* pixel to end at */
int		nobjs = 0;		/* Number of cmd-line treetops */
char		**objtab = (char **)NULL;	/* array of treetop strings */
int		matflag = 0;		/* read matrix from stdin */
int		orientflag = 0;		/* 1 means orientation has been set */
int		desiredframe = 0;	/* frame to start at */
int		finalframe = -1;	/* frame to halt at */
int		curframe = 0;		/* current frame number,
					 * also shared with view.c */
char		*outputfile = (char *)NULL;	/* name of base of output file */
int		interactive = 0;	/* human is watching results */
int		benchmark = 0;		/* No random numbers:  benchmark */

int		sub_grid_mode = 0;	/* mode to raytrace a rectangular portion of view */
int		sub_xmin = 0;		/* lower left of sub rectangle */
int		sub_ymin = 0;
int		sub_xmax = 0;		/* upper right of sub rectangle */
int		sub_ymax = 0;

/**
 *  If this variable is set to zero, then "air" solids in the model
 *  will be entirely discarded.
 *  If this variable is set non-zero, then "air" solids will be
 *  retained, and can be expected to show up in the partition lists.
 */
int		use_air = 0;		/* whether librt should handle air */

/***** end variables shared with do.c *****/


/***** variables shared with view.c *****/
fastf_t		frame_delta_t = (fastf_t)(1.0/30.0); /* 1.0 / frames_per_second_playback */
double		airdensity;    /* is the scene hazy (we shade the void space */
double		haze[3] = { 0.8, 0.9, 0.99 };	      /* color of the haze */
int             do_kut_plane = 0;
plane_t         kut_plane;

double units = 1.0;
int default_units = 1;
int model_units = 0;

/***** end variables shared with view.c *****/

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

#define MAX_WIDTH	(32*1024)

extern struct command_tab	rt_cmdtab[];

/*
 *			G E T _ A R G S
 */
int
get_args(int argc, const char *argv[])
{
    register int c;
    register int i;
    char *env_str;              

    bu_optind = 1;		/* restart */


#define GETOPT_STR	\
	".:,:@:a:b:c:d:e:f:g:h:ij:k:l:n:o:p:q:rs:tu:v:w:x:A:BC:D:E:F:G:H:IJ:K:MN:O:P:Q:RST:U:V:WX:!:+:"

    while ( (c=bu_getopt( argc, (char * const *)argv, GETOPT_STR )) != -1 )  {
	switch ( c )  {
	    case 'q':
		i = atoi(bu_optarg);
		if (i <= 0) {
		    bu_exit(EXIT_FAILURE, "-q %d is < 0\n", i);
		}
		if ( i > BN_RANDHALFTABSIZE) {
		    bu_exit(EXIT_FAILURE, "-q %d is > maximum (%d)\n",
			    i, BN_RANDHALFTABSIZE);
		}
		bn_randhalftabsize = i;
		break;
	    case 'h':
		i = sscanf(bu_optarg, "%lg,%lg,%lg,%lg",
			   &airdensity, &haze[X], &haze[Y], &haze[Z]);
		break;
	    case 't':
		transpose_grid = 1;
		break;
	    case 'j':
	    {
		register char	*cp = bu_optarg;

		sub_xmin = atoi(cp);
		while ( (*cp >= '0' && *cp <= '9') )  cp++;
		while ( *cp && (*cp < '0' || *cp > '9') ) cp++;
		sub_ymin = atoi(cp);
		while ( (*cp >= '0' && *cp <= '9') )  cp++;
		while ( *cp && (*cp < '0' || *cp > '9') ) cp++;
		sub_xmax = atoi(cp);
		while ( (*cp >= '0' && *cp <= '9') )  cp++;
		while ( *cp && (*cp < '0' || *cp > '9') ) cp++;
		sub_ymax = atoi(cp);

		bu_log("Sub-rectangle: (%d,%d) (%d,%d)\n",
		       sub_xmin, sub_ymin,
		       sub_xmax, sub_ymax );
		if ( sub_xmin >= 0 && sub_xmin < sub_xmax &&
		     sub_ymin >= 0 && sub_ymin < sub_ymax )  {
		    sub_grid_mode = 1;
		} else {
		    sub_grid_mode = 0;
		    bu_log("WARNING: bad sub-rectangle, ignored\n");
		}
	    }
	    break;
	    case 'k':      /* define cutting plane */
	    {
		fastf_t f;

		do_kut_plane = 1;
		i = sscanf(bu_optarg, "%lg,%lg,%lg,%lg",
			   &kut_plane[X], &kut_plane[Y], &kut_plane[Z], &kut_plane[H]);
		if( i != 4 ) {
		    bu_exit( EXIT_FAILURE, "ERROR: bad cutting plane\n" );
		}

		/* verify that normal has unit length */
		f = MAGNITUDE( kut_plane );
		if( f <= SMALL ) {
		    bu_exit( EXIT_FAILURE, "Bad normal for cutting plane, length=%g\n", f );
		}
		f = 1.0 /f;
		VSCALE( kut_plane, kut_plane, f );
		kut_plane[W] *= f;
		break;
	    }
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
		char		buf[128];
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

#if defined(_WIN32)
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
#else
		sprintf(buf, "set background=%f/%f/%f",
			r/255., g/255., b/255. );
		(void)rt_do_cmd( (struct rt_i *)0, buf,
				 rt_cmdtab );
#endif
	    }
	    break;
	    case 'T':
	    {
		double		f;
		char		*cp;
		f = 0;
		if ( sscanf( bu_optarg, "%lf", &f ) == 1 )  {
		    if ( f > 0 )
			rt_dist_tol = f;
		}
		f = 0;
		if ( (cp = strchr(bu_optarg, '/')) ||
		     (cp = strchr(bu_optarg, COMMA)) )  {
		    if ( sscanf( cp+1, "%lf", &f ) == 1 )  {
			if ( f > 0 && f < 1 )
			    rt_perp_tol = f;
		    }
		}
		bu_log("Using tolerance %lg", f);
		break;
	    }
	    case 'U':
		use_air = atoi( bu_optarg );
		break;
	    case 'I':
		interactive = 1;
		break;
	    case 'i':
		incr_mode = 1;
		break;
	    case 'S':
		stereo = 1;
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
	    case 'N':
		sscanf( bu_optarg, "%x", (unsigned int *)&rt_g.NMG_debug);
		bu_log("NMG_debug=0x%x\n", rt_g.NMG_debug);
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
		i = atoi( bu_optarg );
		if ( i < 1 || i > MAX_WIDTH )
		    fprintf(stderr, "squaresize=%d out of range\n", i);
		else
		    width = height = i;
		break;
	    case 'n':
		i = atoi( bu_optarg );
		if ( i < 1 || i > MAX_WIDTH )
		    fprintf(stderr, "height=%d out of range\n", i);
		else
		    height = i;
		break;
	    case 'W':
		(void)rt_do_cmd( (struct rt_i *)0, "set background=1.0/1.0/1.0", rt_cmdtab );
		default_background = 0;
		break;
	    case 'w':
		i = atoi( bu_optarg );
		if ( i < 1 || i > MAX_WIDTH )
		    fprintf(stderr, "width=%d out of range\n", i);
		else
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
	    case 'l':
	    {
		char	*item;

		/* Select lighting model # */
		lightmodel= 1;	/* Initialize with Full Lighting Model */
		item= strtok(bu_optarg, ",");
		lightmodel= atoi(item);

		if (lightmodel == 7) {
		    /* Process the photon mapping arguments */
		    item= strtok(NULL, ",");
		    pmargs[0]= item ? atoi(item) : 16384;		/* Number of Global Photons */
		    item= strtok(NULL, ",");
		    pmargs[1]= item ? atof(item) : 50;		/* Percent of Global Photons that should be used for Caustic Photons */
		    item= strtok(NULL, ",");
		    pmargs[2]= item ? atoi(item) : 10;		/* Number of Irradiance Sample Rays, Total Rays is this number squared */
		    item= strtok(NULL, ",");
		    pmargs[3]= item ? atof(item) : 60.0;		/* Angular Tolerance */
		    item= strtok(NULL, ",");
		    pmargs[4]= item ? atoi(item) : 0;		/* Random Seed */
		    item= strtok(NULL, ",");
		    pmargs[5]= item ? atoi(item) : 0;		/* Importance Mapping */
		    item= strtok(NULL, ",");
		    pmargs[6]= item ? atoi(item) : 0;		/* Irradiance Hypersampling */
		    item= strtok(NULL, ",");
		    pmargs[7]= item ? atoi(item) : 0;		/* Visualize Irradiance */
		    item= strtok(NULL, ",");
		    pmargs[8]= item ? atof(item) : 1.0;		/* Scale Lumens */
		    item= strtok(NULL,",");
		    if (item) {
			bu_strlcpy(pmfile, item, sizeof(pmfile));
		    } else {
			pmfile[0]= 0;
		    }
		}
	    }
	    break;
	    case 'O':
		/* Output pixel file name, double precision format */
		outputfile = bu_optarg;
		doubles_out = 1;
		break;
	    case 'o':
		/* Output pixel file name, unsigned char format */
		outputfile = bu_optarg;
		doubles_out = 0;
		break;
	    case 'p':
		rt_perspective = atof( bu_optarg );
		if ( rt_perspective < 0 || rt_perspective > 179 ) {
		    fprintf(stderr, "persp=%g out of range\n", rt_perspective);
		    rt_perspective = 0;
		}
		break;
	    case 'u':
	    	if (BU_STR_EQUAL(bu_optarg,"model")) {
	    		model_units = 1;
	    		default_units = 0;
	    	} else {
	    		units = bu_units_conversion(bu_optarg);
	    		if (units <= 0.0) {
	    			units = 1.0;
	    			default_units = 1;
	    			bu_log("WARNING: bad units, using default (%s)\n", bu_units_string(units));
	    		} else {
	    			default_units = 0;
	    		}
			}
		break;
	    case 'v': /* Set level of "non-debug" debugging output */
		sscanf( bu_optarg, "%x", (unsigned int *)&rt_verbosity );
		bu_printb( "Verbosity:", rt_verbosity,
			   VERBOSE_FORMAT);
		bu_log("\n");
		break;
	    case 'E':
		eye_backoff = atof( bu_optarg );
		break;

	    case 'P':
	    {
		/* Number of parallel workers */
		int avail_cpus;

		avail_cpus = bu_avail_cpus();

		npsw = atoi( bu_optarg );

		if (npsw > avail_cpus ) {
		    fprintf( stderr, "Requesting %d cpus, only %d available.",
			     npsw, avail_cpus );

		    if ((bu_debug & BU_DEBUG_PARALLEL) ||
			(RT_G_DEBUG & DEBUG_PARALLEL)) {
			fprintf(stderr, "\nAllowing surplus cpus due to debug flag.\n");
		    } else {
			fprintf( stderr, "  Will use %d.\n", avail_cpus );
			npsw = avail_cpus;
		    }
		}
		if ( npsw == 0 || npsw < -MAX_PSW || npsw > MAX_PSW )  {
		    fprintf(stderr, "Numer of requested cpus (%d) is out of range 1..%d", npsw, MAX_PSW);

		    if ((bu_debug & BU_DEBUG_PARALLEL) ||
			(RT_G_DEBUG & DEBUG_PARALLEL)) {
			fprintf(stderr, ", but allowing due to debug flag\n");
		    } else {
			fprintf(stderr, ", using -P1\n");
			npsw = 1;
		    }
		}
	    }
	    break;
	    case 'Q':
		Query_one_pixel = ! Query_one_pixel;
		sscanf(bu_optarg, "%d,%d\n", &query_x, &query_y);
		break;
	    case 'B':
		/*  Remove all intentional random effects
		 *  (dither, etc) for benchmarking purposes.
		 */
		benchmark = 1;
		bn_mathtab_constant();
		break;
	    case 'b':
		/* Specify a single pixel to be done */
		/* Actually processed in do_frame() */
		string_pix_start = bu_optarg;
		npsw = 1;	/* Cancel running in parallel */
		break;
	    case 'f':
		/* set expected playback rate in frames-per-second.
		 * This actually gets stored as the delta-t per frame.
		 */
		frame_delta_t = atof(bu_optarg);
		if (ZERO(frame_delta_t)) {
		    fprintf(stderr, "Invalid frames/sec (%s) == 0.0\n",
			    bu_optarg);
		    frame_delta_t = 30.0;
		}
		frame_delta_t = 1.0 / frame_delta_t;
		break;
	    case 'V':
	    {
		/* View aspect */

		fastf_t xx, yy;
		register char *cp = bu_optarg;

		xx = atof(cp);
		while ( (*cp >= '0' && *cp <= '9')
			|| *cp == '.' )  cp++;
		while ( *cp && (*cp < '0' || *cp > '9') ) cp++;
		yy = atof(cp);
		if ( ZERO(yy) )
		    aspect = xx;
		else
		    aspect = xx/yy;
		if ( aspect <= 0.0 ) {
		    fprintf(stderr, "Bogus aspect %g, using 1.0\n", aspect);
		    aspect = 1.0;
		}
	    }
	    break;
	    case 'r':
		/* report overlapping region names */
		rpt_overlap = 1;
		break;
	    case 'R':
		/* DON'T report overlapping region names */
		rpt_overlap = 0;
		break;
	    case 'd':
		rpt_dist = atoi( bu_optarg );
		break;
	    case '+':
	    {
		register char	*cp = bu_optarg;
		switch (*cp) {
		    case 't':
			output_is_binary = 0;
			break;
		    default:
			fprintf(stderr, "ERROR: unknown option %c\n", *cp);
			return 0;	/* BAD */
		}
	    }
	    break;
	    case EOF:
		fprintf(stderr, "ERROR: unknown option %c\n", c);
		return 0;	/* BAD */
	    default:		/* '?' */
		fprintf(stderr, "ERROR: bad option specified\n");
		return 0;	/* BAD */
	}
    }

    /* sanity checks for sane values */
    if ( aspect <= 0.0 ) {
	aspect = 1.0;
    }

    /* Compat */
    if (RT_G_DEBUG || R_DEBUG || rt_g.NMG_debug )
	bu_debug |= BU_DEBUG_COREDUMP;

    if (RT_G_DEBUG & DEBUG_MEM_FULL)
	bu_debug |= BU_DEBUG_MEM_CHECK;
    if (RT_G_DEBUG & DEBUG_MEM)
	bu_debug |= BU_DEBUG_MEM_LOG;
    if (RT_G_DEBUG & DEBUG_PARALLEL)
	bu_debug |= BU_DEBUG_PARALLEL;
    if (RT_G_DEBUG & DEBUG_MATH)
	bu_debug |= BU_DEBUG_MATH;

    if (R_DEBUG & RDEBUG_RTMEM_END)
	bu_debug |= BU_DEBUG_MEM_CHECK;

    /* TODO: add options instead of reading from ENV */
    env_str = getenv("LIBRT_RAND_MODE");
    if(env_str != NULL && atoi(env_str) == 1){
	random_mode = 1;
	bu_log("random mode\n");
    }
    /* TODO: Read from command line */
    /* Read from ENV with we're going to use the experimental mode */
    env_str = getenv("LIBRT_EXP_MODE");
    if(env_str != NULL && atoi(env_str) == 1){
	full_incr_mode = 1;
	full_incr_nsamples = 10;
	bu_log("multi-sample mode\n");
    }

    return 1;			/* OK */
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
