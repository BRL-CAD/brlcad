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

#include <stdio.h>
#include <ctype.h>
#include <math.h>
#include "machine.h"
#include "vmath.h"
#include "raytrace.h"
#include "fb.h"
#include "./ext.h"

#include "./rdebug.h"
#include "../librt/debug.h"

extern int	getopt();
extern char	*optarg;
extern int	optind;

extern int	rdebug;			/* RT program debugging (not library) */

/***** Variables shared with viewing model *** */
int		hex_out = 0;		/* Binary or Hex .pix output file */
double		AmbientIntensity = 0.4;	/* Ambient light intensity */
double		azimuth, elevation;
int		lightmodel = 0;		/* Select lighting model */
int		rpt_overlap = 0;	/* report overlapping region names */
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
int		pix_start = -1;		/* pixel to start at */
int		pix_end;		/* pixel to end at */
int		nobjs;			/* Number of cmd-line treetops */
char		**objtab;		/* array of treetop strings */
int		matflag = 0;		/* read matrix from stdin */
int		desiredframe = 0;	/* frame to start at */
int		finalframe = -1;	/* frame to halt at */
int		curframe = 0;		/* current frame number */
char		*outputfile = (char *)0;/* name of base of output file */
int		interactive = 0;	/* human is watching results */
int		benchmark = 0;		/* No random numbers:  benchmark */
/***** end variables shared with do.c *****/

char		*framebuffer;		/* desired framebuffer */

#define MAX_WIDTH	(32*1024)


/*
 *			G E T _ A R G S
 */
get_args( argc, argv )
register char **argv;
{
	register int c;
	register int i;

	optind = 1;		/* restart */

#define GETOPT_STR	\
	"E:SJ:H:F:D:K:MA:x:X:s:f:a:e:l:O:o:p:P:Bb:n:w:iIU:V:g:G:r"

	while( (c=getopt( argc, argv, GETOPT_STR )) != EOF )  {
		switch( c )  {
		case 'U':
			use_air = atoi( optarg );
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
			sscanf( optarg, "%x", &jitter );
			break;
		case 'H':
			hypersample = atoi( optarg );
			if( hypersample > 0 )
				jitter = 1;
			break;
		case 'F':
			framebuffer = optarg;
			break;
		case 'D':
			desiredframe = atoi( optarg );
			break;
		case 'K':
			finalframe = atoi( optarg );
			break;
		case 'M':
			matflag = 1;
			break;
		case 'A':
			AmbientIntensity = atof( optarg );
			break;
		case 'x':
			sscanf( optarg, "%x", &rt_g.debug );
			break;
		case 'X':
			sscanf( optarg, "%x", &rdebug );
			break;

		case 'f':
			/* "Fast" - arg's worth of pixels - historical */
		case 's':
			/* Square size */
			i = atoi( optarg );
			if( i < 2 || i > MAX_WIDTH )
				fprintf(stderr,"squaresize=%d out of range\n", i);
			else
				width = height = i;
			break;
		case 'n':
			i = atoi( optarg );
			if( i < 2 || i > MAX_WIDTH )
				fprintf(stderr,"height=%d out of range\n", i);
			else
				height = i;
			break;
		case 'w':
			i = atoi( optarg );
			if( i < 2 || i > MAX_WIDTH )
				fprintf(stderr,"width=%d out of range\n", i);
			else
				width = i;
			break;
		case 'g':
			cell_width = atof( optarg );
			break;
		case 'G':
			cell_height = atof( optarg );
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
			rt_perspective = atof( optarg );
			if( rt_perspective < 0 || rt_perspective > 179 ) {
				fprintf(stderr,"persp=%g out of range\n", rt_perspective);
				rt_perspective = 0;
			}
			break;
		case 'E':
			eye_backoff = atof( optarg );
			break;

		case 'P':
			/* Number of parallel workers */
			npsw = atoi( optarg );
			if( npsw == 0 || npsw < -MAX_PSW || npsw > MAX_PSW )  {
				fprintf(stderr,"abs(npsw) out of range 1..%d, using -P%d\n",
					MAX_PSW, MAX_PSW);
				npsw = MAX_PSW;
			}
			break;
		case 'B':
			/*  Remove all intentional random effects
			 *  (dither, etc) for benchmarking.
			 */
			benchmark = 1;
			mathtab_constant();
			break;
		case 'b':
			/* Specify a single pixel to be done */
			{
				int xx, yy;
				register char *cp = optarg;

				xx = atoi(cp);
				while( *cp >= '0' && *cp <= '9' )  cp++;
				while( *cp && (*cp < '0' || *cp > '9') ) cp++;
				yy = atoi(cp);
				fprintf(stderr,"only pixel %d %d\n", xx, yy);
				if( xx * yy >= 0 )  {
					pix_start = yy * width + xx;
					pix_end = pix_start;
				}
			}
			break;
		case 'V':
			/* View aspect */
			{
				fastf_t xx, yy;
				register char *cp = optarg;

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
		default:		/* '?' */
			fprintf(stderr,"unknown option %c\n", c);
			return(0);	/* BAD */
		}
	}
	return(1);			/* OK */
}
