/*
 *			O P T . C
 *
 *  Option handling for Ray Tracing main program.
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
 *	This software is Copyright (C) 1989 by the United States Army.
 *	All rights reserved.
 */
#ifndef lint
static char RCSrt[] = "@(#)$Header$ (BRL)";
#endif

#include "conf.h"

#include <stdio.h>
#include <ctype.h>
#include <math.h>
#ifdef USE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif

#include "machine.h"
#include "externs.h"
#include "vmath.h"
#include "raytrace.h"
#include "fb.h"
#include "./ext.h"

#include "./rdebug.h"
#include "../librt/debug.h"

extern int	rdebug;			/* RT program debugging (not library) */

/***** Variables shared with viewing model *** */
int		doubles_out = 0;	/* u_char or double .pix output file */
double		AmbientIntensity = 0.4;	/* Ambient light intensity */
double		azimuth, elevation;
int		lightmodel = 0;		/* Select lighting model */
int		rpt_overlap = 0;	/* report overlapping region names */
int		rpt_dist = 0;		/* report distance to each pixel */
/***** end of sharing with viewing model *****/

/***** variables shared with worker() ******/
int		stereo = 0;		/* stereo viewing */
int		hypersample=0;		/* number of extra rays to fire */
int		jitter=0;		/* jitter ray starting positions */
fastf_t		rt_perspective=0;	/* presp (degrees X) 0 => ortho */
fastf_t		aspect = 1;		/* view aspect ratio X/Y */
vect_t		dx_model;		/* view delta-X as model-space vect */
vect_t		dy_model;		/* view delta-Y as model-space vect */
fastf_t		cell_width;		/* model space grid cell width */
fastf_t		cell_height;		/* model space grid cell height */
int		cell_newsize=0;		/* new grid cell size */
point_t		eye_model;		/* model-space location of eye */
fastf_t         eye_backoff = 1.414;	/* dist from eye to center */
int		width;			/* # of pixels in X */
int		height;			/* # of lines in Y */
mat_t		Viewrotscale;
fastf_t		viewsize=0;
int		incr_mode = 0;		/* !0 for incremental resolution */
int		incr_level;		/* current incremental level */
int		incr_nlevel;		/* number of levels */
int		npsw = 1;		/* number of worker PSWs to run */
struct resource	resource[MAX_PSW];	/* memory resources */
/***** end variables shared with worker() *****/

/***** variables shared with do.c *****/
char		*string_pix_start;	/* string spec of starting pixel */
char		*string_pix_end;	/* string spec of ending pixel */
int		pix_start = -1;		/* pixel to start at */
int		pix_end;		/* pixel to end at */
int		nobjs;			/* Number of cmd-line treetops */
char		**objtab;		/* array of treetop strings */
int		matflag = 0;		/* read matrix from stdin */
int		desiredframe = 0;	/* frame to start at */
int		finalframe = -1;	/* frame to halt at */
int		curframe = 0;		/* current frame number,
					 * also shared with view.c */
char		*outputfile = (char *)0;/* name of base of output file */
int		interactive = 0;	/* human is watching results */
int		benchmark = 0;		/* No random numbers:  benchmark */
/***** end variables shared with do.c *****/


/***** variables shared with view.c *****/
fastf_t		frame_delta_t = 1./30.; /* 1.0 / frames_per_second_playback */
/***** end variables shared with view.c *****/


fastf_t		rt_dist_tol = 0;	/* Value for rti_tol.dist */
fastf_t		rt_perp_tol = 0;	/* Value for rti_tol.perp */
char		*framebuffer;		/* desired framebuffer */

int		space_partition = 	/*space partitioning algorithm to use*/
			RT_PART_NUBSPT;
double		nu_gfactor = RT_NU_GFACTOR_DEFAULT;
		      /* constant factor in NUgrid algorithm, if applicable */

#define MAX_WIDTH	(32*1024)

extern struct command_tab	rt_cmdtab[];

/*
 *			G E T _ A R G S
 */
get_args( argc, argv )
register char **argv;
{
	register int c;
	register int i;

	bu_optind = 1;		/* restart */

#define GETOPT_STR	\
	".:,:a:b:c:d:e:f:g:il:n:o:p:rs:w:x:A:BC:D:E:F:G:H:IJ:K:MN:O:P:RST:U:V:X:"

	while( (c=bu_getopt( argc, argv, GETOPT_STR )) != EOF )  {
		switch( c )  {
		case '.':
			nu_gfactor = (double)atof( bu_optarg );
			break;
		case ',':
			space_partition = atoi(bu_optarg);
			break;
		case 'c':
			(void)rt_do_cmd( (struct rt_i *)0, bu_optarg, rt_cmdtab );
			break;
		case 'C':
			{
				char		buf[128];
				int		r,g,b;
				register char	*cp = bu_optarg;

				r = atoi(cp);
				while( (*cp >= '0' && *cp <= '9') )  cp++;
				while( *cp && (*cp < '0' || *cp > '9') ) cp++;
				g = atoi(cp);
				while( (*cp >= '0' && *cp <= '9') )  cp++;
				while( *cp && (*cp < '0' || *cp > '9') ) cp++;
				b = atoi(cp);

				if( r < 0 || r > 255 )  r = 255;
				if( g < 0 || g > 255 )  g = 255;
				if( b < 0 || b > 255 )  b = 255;

				sprintf(buf,"set background=%f/%f/%f",
					r/255., g/255., b/255. );
				(void)rt_do_cmd( (struct rt_i *)0, buf,
					rt_cmdtab );
			}
			break;
		case 'T':
			{
				double		f;
				char		*cp;
				f = 0;
				if( sscanf( bu_optarg, "%lf", &f ) == 1 )  {
					if( f > 0 )
						rt_dist_tol = f;
				}
				f = 0;
				if( (cp = strchr(bu_optarg, '/')) ||
				    (cp = strchr(bu_optarg, ',')) )  {
					if( sscanf( cp+1, "%lf", &f ) == 1 )  {
						if( f > 0 && f < 1 )
							rt_perp_tol = f;
					}
				}
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
			if( hypersample > 0 )
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
			sscanf( bu_optarg, "%x", &rt_g.NMG_debug);
			rt_log("NMG_debug=0x%x\n", rt_g.NMG_debug);
			break;
		case 'M':
			matflag = 1;
			break;
		case 'A':
			AmbientIntensity = atof( bu_optarg );
			break;
		case 'x':
			sscanf( bu_optarg, "%x", &rt_g.debug );
			break;
		case 'X':
			sscanf( bu_optarg, "%x", &rdebug );
			break;

		case 's':
			/* Square size */
			i = atoi( bu_optarg );
			if( i < 2 || i > MAX_WIDTH )
				fprintf(stderr,"squaresize=%d out of range\n", i);
			else
				width = height = i;
			break;
		case 'n':
			i = atoi( bu_optarg );
			if( i < 2 || i > MAX_WIDTH )
				fprintf(stderr,"height=%d out of range\n", i);
			else
				height = i;
			break;
		case 'w':
			i = atoi( bu_optarg );
			if( i < 2 || i > MAX_WIDTH )
				fprintf(stderr,"width=%d out of range\n", i);
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
			/* Select lighting model # */
			lightmodel = atoi( bu_optarg );
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
			if( rt_perspective < 0 || rt_perspective > 179 ) {
				fprintf(stderr,"persp=%g out of range\n", rt_perspective);
				rt_perspective = 0;
			}
			break;
		case 'E':
			eye_backoff = atof( bu_optarg );
			break;

		case 'P':
			/* Number of parallel workers */
			npsw = atoi( bu_optarg );
			if( npsw == 0 || npsw < -MAX_PSW || npsw > MAX_PSW )  {
				fprintf(stderr,"abs(npsw) out of range 1..%d, using -P%d\n",
					MAX_PSW, MAX_PSW);
				npsw = MAX_PSW;
			}
			break;
		case 'B':
			/*  Remove all intentional random effects
			 *  (dither, etc) for benchmarking purposes.
			 */
			benchmark = 1;
			mathtab_constant();
			break;
		case 'b':
			/* Specify a single pixel to be done */
			/* Actually processed in do_frame() */
			string_pix_start = bu_optarg;
			break;
		case 'f':
			/* set expected playback rate in frames-per-second.
			 * This actually gets stored as the delta-t per frame.
			 */
			if ( (frame_delta_t=atof( bu_optarg )) == 0.0) {
				fprintf(stderr, "Invalid frames/sec (%s) == 0.0\n",
					bu_optarg);
				frame_delta_t = 30.0;
			}
			frame_delta_t = 1.0 / frame_delta_t;
			break;
#if 0
		case ?:
			/* XXX what letter to use? */
			/* Specify the pixel to end at */
			/* Actually processed in do_frame() */
			string_pix_end = bu_optarg;
			break;
#endif
		case 'V':
			/* View aspect */
			{
				fastf_t xx, yy;
				register char *cp = bu_optarg;

				xx = atof(cp);
				while( (*cp >= '0' && *cp <= '9')
					|| *cp == '.' )  cp++;
				while( *cp && (*cp < '0' || *cp > '9') ) cp++;
				yy = atof(cp);
				if( yy == 0 )
					aspect = xx;
				else
					aspect = xx/yy;
				if( aspect == 0 ) {
					fprintf(stderr,"Bogus aspect %d, using 1.0\n", aspect);
					aspect = 1;
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
		default:		/* '?' */
			fprintf(stderr,"unknown option %c\n", c);
			return(0);	/* BAD */
		}
	}

	/* Compat */
	if( rt_g.debug || rdebug || rt_g.NMG_debug )
		bu_debug |= BU_DEBUG_COREDUMP;

	if( rt_g.debug & DEBUG_MEM_FULL )  bu_debug |= BU_DEBUG_MEM_CHECK;
	if( rt_g.debug & DEBUG_MEM )  bu_debug |= BU_DEBUG_MEM_LOG;
	if( rt_g.debug & DEBUG_PARALLEL )  bu_debug |= BU_DEBUG_PARALLEL;
	if( rt_g.debug & DEBUG_MATH )  bu_debug |= BU_DEBUG_MATH;

	if( rdebug & RDEBUG_RTMEM_END )  bu_debug |= BU_DEBUG_MEM_CHECK;

	return(1);			/* OK */
}
