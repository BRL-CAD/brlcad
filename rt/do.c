/*
 *			D O . C 
 *
 *  Routines that process the various commands, and manage
 *  the overall process of running the raytracing process.
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
 *	This software is Copyright (C) 1987 by the United States Army.
 *	All rights reserved.
 */
#ifndef lint
static char RCSrt[] = "@(#)$Header$ (BRL)";
#endif

#include "conf.h"

#include <stdio.h>
#include <ctype.h>
#include <math.h>
#ifdef HAVE_UNIX_IO
# include <sys/types.h>
# include <sys/stat.h>
#endif

#include "machine.h"
#include "vmath.h"
#include "raytrace.h"
#include "fb.h"
#include "externs.h"
#include "./material.h"
#include "./rdebug.h"
#include "../librt/debug.h"

extern int	rdebug;			/* RT program debugging (not library) */

/***** Variables shared with viewing model *** */
extern FILE	*outfp;			/* optional pixel output file */
extern double	azimuth, elevation;
extern mat_t	view2model;
extern mat_t	model2view;
/***** end of sharing with viewing model *****/

extern void	grid_setup();
extern void	worker();

/***** variables shared with worker() ******/
extern struct application ap;
extern int	hypersample;		/* number of extra rays to fire */
extern fastf_t	aspect;			/* view aspect ratio X/Y */
extern fastf_t	cell_width;		/* model space grid cell width */
extern fastf_t	cell_height;		/* model space grid cell height */
extern point_t	eye_model;		/* model-space location of eye */
extern fastf_t  eye_backoff;		/* dist of eye from center */
extern fastf_t	rt_perspective;		/* persp (degrees X) 0 => ortho */
extern int	width;			/* # of pixels in X */
extern int	height;			/* # of lines in Y */
extern mat_t	Viewrotscale;
extern fastf_t	viewsize;
extern int	incr_mode;		/* !0 for incremental resolution */
extern int	incr_level;		/* current incremental level */
extern int	incr_nlevel;		/* number of levels */
extern int	npsw;
extern struct resource resource[];
/***** end variables shared with worker() */

/***** variables shared with rt.c *****/
extern char	*string_pix_start;	/* string spec of starting pixel */
extern char	*string_pix_end;	/* string spec of ending pixel */
extern int	pix_start;		/* pixel to start at */
extern int	pix_end;		/* pixel to end at */
extern int	nobjs;			/* Number of cmd-line treetops */
extern char	**objtab;		/* array of treetop strings */
extern char	*beginptr;		/* sbrk() at start of program */
extern int	matflag;		/* read matrix from stdin */
extern int	desiredframe;		/* frame to start at */
extern int	finalframe;		/* frame to halt at */
extern int	curframe;		/* current frame number */
extern char	*outputfile;		/* name of base of output file */
extern int	interactive;		/* human is watching results */
/***** end variables shared with rt.c *****/

/***** variables shared with refract.c *****/
extern int	max_bounces;		/* max reflection/recursion level */
extern int	max_ireflect;		/* max internal reflection level */
/***** end variables shared with refract.c *****/

void		def_tree();
void		do_ae();
void		res_pr();

/*
 *			O L D _ W A Y
 * 
 *  Determine if input file is old or new format, and if
 *  old format, handle process *  Returns 0 if new way, 1 if old way (and all done).
 *  Note that the rewind() will fail on ttys, pipes, and sockets (sigh).
 */
int
old_way( fp )
FILE *fp;
{
	int	c;

	viewsize = -42.0;

	/* Sneek a peek at the first character, and then put it back */
	if( (c = fgetc( fp )) == EOF )  {
		/* Claim old way, all (ie, nothing) done */
		return(1);
	}
	if( ungetc( c, fp ) != c )
		rt_bomb("do.c:old_way() ungetc failure\n");

	/*
	 * Old format files start immediately with a %.9e format,
	 * so the very first character should be a digit or '-'.
	 */
	if( (c < '0' || c > '9') && c != '-' )  {
		return( 0 );		/* Not old way */
	}

	if( old_frame( fp ) < 0 || viewsize <= 0.0 )  {
		rewind( fp );
		return(0);		/* Not old way */
	}
	rt_log("Interpreting command stream in old format\n");

	def_tree( ap.a_rt_i );		/* Load the default trees */

	curframe = 0;
	do {
		if( finalframe >= 0 && curframe > finalframe )
			return(1);
		if( curframe >= desiredframe )
			do_frame( curframe );
		curframe++;
	}  while( old_frame( fp ) >= 0 && viewsize > 0.0 );
	return(1);			/* old way, all done */
}

/*
 *			O L D _ F R A M E
 *
 *  Acquire particulars about a frame, in the old format.
 *  Returns -1 if unable to acquire info, 0 if successful.
 */
int
old_frame( fp )
FILE *fp;
{
	register int i;
	char number[128];

	/* Visible part is from -1 to +1 in view space */
	if( fscanf( fp, "%s", number ) != 1 )  return(-1);
	viewsize = atof(number);
	if( fscanf( fp, "%s", number ) != 1 )  return(-1);
	eye_model[X] = atof(number);
	if( fscanf( fp, "%s", number ) != 1 )  return(-1);
	eye_model[Y] = atof(number);
	if( fscanf( fp, "%s", number ) != 1 )  return(-1);
	eye_model[Z] = atof(number);
	for( i=0; i < 16; i++ )  {
		if( fscanf( fp, "%s", number ) != 1 )
			return(-1);
		Viewrotscale[i] = atof(number);
	}
	return(0);		/* OK */
}

/*
 *			C M _ S T A R T
 *
 *  Process "start" command in new format input stream
 */
cm_start( argc, argv )
int	argc;
char	**argv;
{
	char	*buf;
	int	frame;

	frame = atoi(argv[1]);
	if( finalframe >= 0 && frame > finalframe )
		return(-1);	/* Indicate EOF -- user declared a halt */
	if( frame >= desiredframe )  {
		curframe = frame;
		return(0);
	}

	/* Skip over unwanted frames -- find next frame start */
	while( (buf = rt_read_cmd( stdin )) != (char *)0 )  {
		register char *cp;

		cp = buf;
		while( *cp && isspace(*cp) )  cp++;	/* skip spaces */
		if( strncmp( cp, "start", 5 ) != 0 )  continue;
		while( *cp && !isspace(*cp) )  cp++;	/* skip keyword */
		while( *cp && isspace(*cp) )  cp++;	/* skip spaces */
		frame = atoi(cp);
		rt_free( buf, "rt_read_cmd command buffer (skipping frames)" );
		if( finalframe >= 0 && frame > finalframe )
			return(-1);			/* "EOF" */
		if( frame >= desiredframe )  {
			curframe = frame;
			return(0);
		}
	}
	return(-1);		/* EOF */
}

cm_vsize( argc, argv )
int	argc;
char	**argv;
{
	viewsize = atof( argv[1] );
	return(0);
}

cm_eyept( argc, argv )
int	argc;
char	**argv;
{
	register int i;

	for( i=0; i<3; i++ )
		eye_model[i] = atof( argv[i+1] );
	return(0);
}

cm_lookat_pt( argc, argv )
int	argc;
char	**argv;
{
	point_t	pt;
	vect_t	dir;
	int	yflip = 0;

	if( argc < 4 )
		return(-1);
	pt[X] = atof(argv[1]);
	pt[Y] = atof(argv[2]);
	pt[Z] = atof(argv[3]);
	if( argc > 4 )
		yflip = atoi(argv[4]);

	/*
	 *  eye_pt must have been specified before here (for now)
	 */
	VSUB2( dir, pt, eye_model );
	VUNITIZE( dir );
	mat_lookat( Viewrotscale, dir, yflip );
	return(0);
}

cm_vrot( argc, argv )
int	argc;
char	**argv;
{
	register int i;

	for( i=0; i<16; i++ )
		Viewrotscale[i] = atof( argv[i+1] );
	return(0);
}

cm_orientation( argc, argv )
int	argc;
char	**argv;
{
	register int	i;
	quat_t		quat;

	for( i=0; i<4; i++ )
		quat[i] = atof( argv[i+1] );
	quat_quat2mat( Viewrotscale, quat );
	return(0);
}

cm_end( argc, argv )
int	argc;
char	**argv;
{
	struct rt_i *rtip = ap.a_rt_i;

	if( rtip->HeadRegion == REGION_NULL )  {
		def_tree( rtip );		/* Load the default trees */
	}

	/* If no matrix or az/el specified yet, use params from cmd line */
	if( Viewrotscale[15] <= 0.0 )
		do_ae( azimuth, elevation );

	if( do_frame( curframe ) < 0 )  return(-1);
	return(0);
}

cm_tree( argc, argv )
int		argc;
CONST char	**argv;
{
	register struct rt_i *rtip = ap.a_rt_i;
	struct rt_vls	times;

	if( argc <= 1 )  {
		def_tree( rtip );		/* Load the default trees */
		return(0);
	}
	rt_vls_init( &times );

	rt_prep_timer();
	if( rt_gettrees(rtip, argc-1, &argv[1], npsw) < 0 )
		rt_log("rt_gettrees(%s) FAILED\n", argv[0]);
	(void)rt_get_timer( &times, NULL );

	rt_log("GETTREE: %s\n", rt_vls_addr(&times) );
	rt_vls_free( &times );
	return(0);
}

cm_multiview( argc, argv )
int	argc;
char	**argv;
{
	register struct rt_i *rtip = ap.a_rt_i;
	int i;
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

	if( rtip->HeadRegion == REGION_NULL )  {
		def_tree( rtip );		/* Load the default trees */
	}
	for( i=0; i<(sizeof(a)/sizeof(a[0])); i++ )  {
		do_ae( (double)a[i], (double)e[i] );
		(void)do_frame( curframe++ );
	}
	return(-1);	/* end RT by returning an error */
}

/*
 *			C M _ A N I M
 *
 *  Experimental animation code
 *
 *  Usage:  anim <path> <type> args
 */
cm_anim( argc, argv )
int	argc;
char	**argv;
{

	if( db_parse_anim( ap.a_rt_i->rti_dbip, argc, argv ) < 0 )  {
		rt_log("cm_anim:  %s %s failed\n", argv[1], argv[2]);
		return(-1);		/* BAD */
	}
	return(0);
}

/*
 *			C M _ C L E A N
 *
 *  Clean out results of last rt_prep(), and start anew.
 */
cm_clean( argc, argv )
int	argc;
char	**argv;
{
	/* Allow lighting model to clean up (e.g. lights, materials, etc) */
	view_cleanup( ap.a_rt_i );

	rt_clean( ap.a_rt_i );

	if(rdebug&RDEBUG_RTMEM_END)
		rt_prmem( "After cm_clean" );
	return(0);
}

/*
 *			C M _ C L O S E D B
 *
 *  To be invoked after a "clean" command, to close out the ".g" database.
 *  Intended for memory debugging, to help chase down memory "leaks".
 *  This terminates the program, as there is no longer a database.
 */
int
cm_closedb( argc, argv )
int	argc;
char	**argv;
{
	db_close( ap.a_rt_i->rti_dbip );
	ap.a_rt_i->rti_dbip = DBI_NULL;

	bu_free( (genptr_t)ap.a_rt_i, "struct rt_i" );
	ap.a_rt_i = RTI_NULL;

	rt_prmem( "After _closedb" );
	exit(0);

	return( 1 );	/* for compiler */
}

/* viewing module specific variables */
extern struct bu_structparse view_parse[];

/*
 *  Generic settable parameters.
 *  By setting the "base address" to zero in the bu_structparse call,
 *  the actual memory address is given here as the structure offset.
 *
 *  Strictly speaking, the C language only permits initializers of the
 *  form: address +- constant, where here the intent is to measure the
 *  byte address of the indicated variable.
 *  Matching compensation code for the CRAY is located in librt/parse.c
 */
#if CRAY
#	define byteoffset(_i)	(((int)&(_i)))	/* actually a word offset */
#else
#  if IRIX > 5
#	define byteoffset(_i)	((size_t)__INTADDR__(&(_i)))
#  else
#    if sgi || __convexc__ || ultrix || _HPUX_SOURCE
	/* "Lazy" way.  Works on reasonable machines with byte addressing */
#	define byteoffset(_i)	((int)((char *)&(_i)))
#    else
	/* "Conservative" way of finding # bytes as diff of 2 char ptrs */
#	define byteoffset(_i)	((int)(((char *)&(_i))-((char *)0)))
#    endif
#  endif
#endif
struct bu_structparse set_parse[] = {
#if !defined(__alpha)	/* XXX Alpha does not support this initialization! */
	{"%d",	1, "width",	byteoffset(width),		FUNC_NULL },
	{"%d",	1, "height",	byteoffset(height),		FUNC_NULL },
	{"%f",	1, "perspective", byteoffset(rt_perspective),	FUNC_NULL },
	{"%f",	1, "angle",	byteoffset(rt_perspective),	FUNC_NULL },
	{"i", byteoffset(view_parse[0]),"View_Module-Specific Parameters", 0, FUNC_NULL },
#endif
	{"",	0, (char *)0,	0,				FUNC_NULL }
};

/*
 *			C M _ S E T
 *
 *  Allow variable values to be set or examined.
 */
cm_set( argc, argv )
int	argc;
char	**argv;
{
	struct rt_vls	str;

	if( argc <= 1 ) {
		bu_struct_print( "Generic and Application-Specific Parameter Values",
			set_parse, (char *)0 );
		return(0);
	}
	rt_vls_init( &str );
	rt_vls_from_argv( &str, argc-1, argv+1 );
	if( bu_struct_parse( &str, set_parse, (char *)0 ) < 0 )  {
		rt_vls_free( &str );
		return(-1);
	}
	rt_vls_free( &str );
	return(0);
}

/* 
 *			C M _ A E
 */
cm_ae( argc, argv )
int	argc;
char	**argv;
{
	azimuth = atof(argv[1]);	/* set elevation and azimuth */
	elevation = atof(argv[2]);
	Viewrotscale[15] = 0.0;		/* set scale to 0.0 so that cm_end */
					/* will do the do_ae               */

	return(0);
}

/*
 *			C M _ O P T
 */
cm_opt( argc, argv )
int	argc;
char	**argv;
{
	if( get_args( argc, argv ) <= 0 )
		return(-1);
	return(0);
}


/*
 *			D E F _ T R E E
 *
 *  Load default tree list, from command line.
 */
void
def_tree( rtip )
register struct rt_i	*rtip;
{
	struct rt_vls	times;

	if( rtip->rti_magic != RTI_MAGIC )  {
		rt_log("rtip=x%x, rti_magic=x%x s/b x%x\n", rtip,
			rtip->rti_magic, RTI_MAGIC );
		rt_bomb("def_tree:  bad rtip\n");
	}

	rt_vls_init( &times );
	rt_prep_timer();
	if( rt_gettrees(rtip, nobjs, (CONST char **)objtab, npsw) < 0 )
		rt_log("rt_gettrees(%s) FAILED\n", objtab[0]);
	(void)rt_get_timer( &times, NULL );
	rt_log("GETTREE: %s\n", rt_vls_addr(&times));
	rt_vls_free( &times );

#ifdef HAVE_SBRK
	rt_log("Additional dynamic memory used=%d. bytes\n",
		(char *)sbrk(0)-beginptr );
	beginptr = (char *) sbrk(0);
#endif
}

/*
 *			D O _ P R E P
 *
 *  This is a separate function primarily as a service to REMRT.
 */
void
do_prep( rtip )
struct rt_i	*rtip;
{
	struct rt_vls	times;

	RT_CHECK_RTI(rtip);
	if( rtip->needprep )  {
		/* Allow lighting model to set up (e.g. lights, materials, etc) */
		view_setup(rtip);

		/* Allow RT library to prepare itself */
		rt_vls_init( &times );
		rt_prep_timer();
		rt_prep_parallel(rtip, npsw);

		(void)rt_get_timer( &times, NULL );
		rt_log( "PREP: %s\n", rt_vls_addr(&times) );
		rt_vls_free( &times );
	}
#ifdef HAVE_SBRK
	rt_log("Additional dynamic memory used=%d. bytes\n",
		(char *)sbrk(0)-beginptr );
	beginptr = (char *) sbrk(0);
#endif
}

/*
 *			D O _ F R A M E
 *
 *  Do all the actual work to run a frame.
 *
 *  Returns -1 on error, 0 if OK.
 */
do_frame( framenumber )
int framenumber;
{
	struct rt_vls	times;
	char framename[128];		/* File name to hold current frame */
	struct rt_i *rtip = ap.a_rt_i;
	double	utime;			/* CPU time used */
	double	nutime;			/* CPU time used, normalized by ncpu */
	double	wallclock;		/* # seconds of wall clock time */
	int	npix;			/* # of pixel values to be done */
	int	lim;
	vect_t	work, temp;
	quat_t	quat;

	rt_log( "\n...................Frame %5d...................\n",
		framenumber);

	/* Compute model RPP, etc */
	do_prep( rtip );

	rt_log("Tree: %d solids in %d regions\n",
		rtip->nsolids, rtip->nregions );
	if( rtip->nsolids <= 0 )  {
		rt_log("rt ERROR: No solids\n");
		exit(3);
	}
	rt_log("Model: X(%g,%g), Y(%g,%g), Z(%g,%g)\n",
		rtip->mdl_min[X], rtip->mdl_max[X],
		rtip->mdl_min[Y], rtip->mdl_max[Y],
		rtip->mdl_min[Z], rtip->mdl_max[Z] );

	/*
	 *  Perform Grid setup.
	 *  This may alter cell size or width/height.
	 */
	grid_setup();
	/* az/el 0,0 is when screen +Z is model +X */
	VSET( work, 0, 0, 1 );
	MAT3X3VEC( temp, view2model, work );
	ae_vec( &azimuth, &elevation, temp );
	rt_log(
		"View: %g azimuth, %g elevation off of front view\n",
		azimuth, elevation);
	quat_mat2quat( quat, model2view );
	rt_log("Orientation: %g, %g, %g, %g\n", V4ARGS(quat) );
	rt_log("Eye_pos: %g, %g, %g\n", V3ARGS(eye_model) );
	rt_log("Size: %gmm\n", viewsize);
#if 0
	/*
	 *  This code shows how the model2view matrix can be reconstructed
	 *  using the information from the Orientation, Eye_pos, and Size
	 *  messages.
	 */
	{
		mat_t	rotscale, xlate;
		mat_t	new;
		quat_t	newquat;

		mat_print("model2view", model2view);
		quat_quat2mat( rotscale, quat );
		rotscale[15] = 0.5 * viewsize;
		mat_idn( xlate );
		MAT_DELTAS( xlate, -eye_model[X], -eye_model[Y], -eye_model[Z] );
		mat_mul( new, rotscale, xlate );
		mat_print("reconstructed m2v", new);
		quat_mat2quat( newquat, new );
		HPRINT( "reconstructed orientation:", newquat );
	}
#endif
	rt_log("Grid: (%g, %g) mm, (%d, %d) pixels\n",
		cell_width, cell_height,
		width, height );
	rt_log("Beam: radius=%g mm, divergence=%g mm/1mm\n",
		ap.a_rbeam, ap.a_diverge );

	/* Process -b and ??? options now, for this frame */
	if( pix_start == -1 )  {
		pix_start = 0;
		pix_end = height * width - 1;
	}
	if( string_pix_start )  {
		int xx, yy;
		register char *cp = string_pix_start;

		xx = atoi(cp);
		while( *cp >= '0' && *cp <= '9' )  cp++;
		while( *cp && (*cp < '0' || *cp > '9') ) cp++;
		yy = atoi(cp);
		rt_log("only pixel %d %d\n", xx, yy);
		if( xx * yy >= 0 )  {
			pix_start = yy * width + xx;
			pix_end = pix_start;
		}
	}
	if( string_pix_end )  {
		int xx, yy;
		register char *cp = string_pix_end;

		xx = atoi(cp);
		while( *cp >= '0' && *cp <= '9' )  cp++;
		while( *cp && (*cp < '0' || *cp > '9') ) cp++;
		yy = atoi(cp);
		rt_log("ending pixel %d %d\n", xx, yy);
		if( xx * yy >= 0 )  {
			pix_end = yy * width + xx;
		}
	}

	/*
	 *  After the parameters for this calculation have been established,
	 *  deal with CPU limits and priorities, where appropriate.
	 *  Because limits exist, they better be adequate.
	 *  We assume that the Cray can produce MINRATE pixels/sec
	 *  on images with extreme amounts of glass & mirrors.
	 */
#ifdef CRAY2
#define MINRATE	35
#else
#define MINRATE	65
#endif
	npix = width*height*(hypersample+1);
	if( (lim = rt_cpuget()) > 0 )  {
		rt_cpuset( lim + npix / MINRATE + 100 );
	}

	/*
	 *  If this image is unlikely to be for debugging,
	 *  be gentle to the machine.
	 */
	if( !interactive )  {
		if( npix > 256*256 )
			rt_pri_set(10);
		else if( npix > 512*512 )
			rt_pri_set(14);
	}

	/*
	 *  Determine output file name
	 *  On UNIX only, check to see if this is a "restart".
	 */
	if( outputfile != (char *)0 )  {
#ifdef CRAY_COS
		/* Dots in COS file names make them permanant files. */
		sprintf( framename, "F%d", framenumber );
		if( (outfp = fopen( framename, "w" )) == NULL )  {
			perror( framename );
			if( matflag )  return(0);
			return(-1);
		}
		/* Dispose to shell script starts with "!" */
		if( framenumber <= 0 || outputfile[0] == '!' )  {
			sprintf( framename, outputfile );
		}  else  {
			sprintf( framename, "%s.%d", outputfile, framenumber );
		}
#else
		if( framenumber <= 0 )  {
			sprintf( framename, outputfile );
		}  else  {
			sprintf( framename, "%s.%d", outputfile, framenumber );
		}
#ifdef HAVE_UNIX_IO
		/*
		 *  This code allows the computation of a particular frame
		 *  to a disk file to be resumed automaticly.
		 *  This is worthwhile crash protection.
		 *  This use of stat() and fseek() is UNIX-specific.
		 *
		 *  It is not appropriate for the RT "top part" to assume
		 *  anything about the data that the view module will be
		 *  storing.  Therefore, it is the responsibility of
		 *  view_2init() to also detect that some existing data
		 *  is in the file, and take appropriate measures
		 *  (like reading it in).
		 *  view_2init() can depend on the file being open for both
		 *  reading and writing, but must do it's own positioning.
		 */
		{
			struct stat sb;
			if( stat( framename, &sb ) >= 0 && sb.st_size > 0 )  {
				/* File exists, with partial results */
				register int	fd;
				if( (fd = open( framename, 2 )) < 0 ||
				    (outfp = fdopen( fd, "r+" )) == NULL )  {
					perror( framename );
					if( matflag )  return(0);	/* OK */
					return(-1);			/* Bad */
				}
			}
		}
#endif

		/* Ordinary case for creating output file */
		if( outfp == NULL && (outfp = fopen( framename, "w" )) == NULL )  {
			perror( framename );
			if( matflag )  return(0);	/* OK */
			return(-1);			/* Bad */
		}
#endif /* CRAY_COS */
		rt_log("Output file is '%s'\n", framename);
	}

	/* initialize lighting, may update pix_start */
	view_2init( &ap, framename );

	/* Just while doing the ray-tracing */
	if(rdebug&RDEBUG_RTMEM)
		bu_debug |= (BU_DEBUG_MEM_CHECK|BU_DEBUG_MEM_LOG);

	rtip->nshots = 0;
	rtip->nmiss_model = 0;
	rtip->nmiss_tree = 0;
	rtip->nmiss_solid = 0;
	rtip->nmiss = 0;
	rtip->nhits = 0;
	rtip->rti_nrays = 0;

	rt_log("\n");
	fflush(stdout);
	fflush(stderr);

	/*
	 *  Compute the image
	 *  It may prove desirable to do this in chunks
	 */
	rt_prep_timer();
	if( incr_mode )  {
		for( incr_level = 1; incr_level <= incr_nlevel; incr_level++ )  {
			if( incr_level > 1 )
				view_2init( &ap );
			do_run( 0, (1<<incr_level)*(1<<incr_level)-1 );
		}
	} else {
		do_run( pix_start, pix_end );

		/* Reset values to full size, for next frame (if any) */
		pix_start = 0;
		pix_end = height*width - 1;
	}
	rt_vls_init( &times );
	utime = rt_get_timer( &times, &wallclock );

	/*
	 *  End of application.  Done outside of timing section.
	 *  Typically, writes any remaining results out.
	 */
	view_end( &ap );

	/* Stop memory debug printing until next frame, leave full checking on */
	if(rdebug&RDEBUG_RTMEM)
		bu_debug &= ~BU_DEBUG_MEM_LOG;

	/*
	 *  Certain parallel systems (eg, Alliant) count the entire 
	 *  multi-processor complex as one computer, and charge only once.
	 *  This matches the desired behavior here.
	 *  Other vendors (eg, SGI) count each processor separately,
	 *  and charge for all of them.  These results need to be normalized.
	 *  Otherwise, all we would know is that a given workload takes about
	 *  the same amount of CPU time, regardless of the number of CPUs.
	 */
#if !defined(alliant)
	if( npsw > 1 )  {
		nutime = utime / npsw;			/* compensate */
	} else
#endif
		nutime = utime;

	/*
	 *  All done.  Display run statistics.
	 */
	rt_log("SHOT: %s\n", rt_vls_addr( &times ) );
	rt_vls_free( &times );
#ifdef HAVE_SBRK
	rt_log("Additional dynamic memory used=%d. bytes\n",
		(char *)sbrk(0)-beginptr );
		beginptr = (char *) sbrk(0);
#endif
	rt_log("%ld solid/ray intersections: %ld hits + %ld miss\n",
		rtip->nshots, rtip->nhits, rtip->nmiss );
	rt_log("pruned %.1f%%:  %ld model RPP, %ld dups skipped, %ld solid RPP\n",
		rtip->nshots>0?((double)rtip->nhits*100.0)/rtip->nshots:100.0,
		rtip->nmiss_model, rtip->nmiss_tree, rtip->nmiss_solid );
	rt_log(
		"Frame %5d: %8d pixels in %10.2f sec = %10.2f pixels/sec\n",
		framenumber,
		width*height, nutime, (double)(width*height)/nutime );
	rt_log(
		"Frame %5d: %8d rays   in %10.2f sec = %10.2f rays/sec (RTFM)\n",
		framenumber,
		rtip->rti_nrays, nutime, (double)(rtip->rti_nrays)/nutime );
	rt_log(
		"Frame %5d: %8d rays   in %10.2f sec = %10.2f rays/CPU_sec\n",
		framenumber,
		rtip->rti_nrays, utime, (double)(rtip->rti_nrays)/utime );
	rt_log(
		"Frame %5d: %8d rays   in %10.2f sec = %10.2f rays/sec (wallclock)\n",
		framenumber,
		rtip->rti_nrays,
		wallclock, (double)(rtip->rti_nrays)/wallclock );

	if( outfp != NULL )  {
#ifdef CRAY_COS
		int status;
		char dn[16];
		char message[128];

		strncpy( dn, outfp->ldn, sizeof(outfp->ldn) );	/* COS name */
#endif
		(void)fclose(outfp);
		outfp = NULL;
#ifdef CRAY_COS
		status = 0;
		/* Binary out */
		(void)DISPOSE( &status, "DN      ", dn,
			"TEXT    ", framename,
			"NOWAIT  ",
			"DF      ", "BB      " );
		sprintf(message,
			"%s Dispose,dn='%s',text='%s'.  stat=0%o",
			(status==0) ? "Good" : "---BAD---",
			dn, framename, status );
		rt_log( "%s\n", message);
		remark(message);	/* Send to log files */
#else
		/* Protect finished product */
		if( outputfile != (char *)0 )
			chmod( framename, 0444 );
#endif
	}

	if(rdebug&RDEBUG_STATS)  {
		/* Print additional statistics */
		res_pr();
	}

	rt_log("\n");
	return(0);		/* OK */
}

/*
 *			D O _ A E
 *
 *  Compute the rotation specified by the azimuth and elevation
 *  parameters.  First, note that these are specified relative
 *  to the GIFT "front view", ie, model (X,Y,Z) is view (Z,X,Y):
 *  looking down X axis, Y right, Z up.
 *  A positive azimuth represents rotating the *eye* around the
 *  Y axis, or, rotating the *model* in -Y.
 *  A positive elevation represents rotating the *eye* around the
 *  X axis, or, rotating the *model* in -X.
 */
void
do_ae( azim, elev )
double azim, elev;
{
	vect_t	temp;
	vect_t	diag;
	mat_t	toEye;
	struct rt_i *rtip = ap.a_rt_i;

	if( rtip->nsolids <= 0 )
		rt_bomb("do_ae: no solids active\n");
	if( rtip->mdl_max[X] >= INFINITY || rtip->mdl_max[X] <= -INFINITY )
		rt_bomb("do_ae: infinite model bounds?\n");

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

	mat_idn( Viewrotscale );
	mat_angles( Viewrotscale, 270.0+elev, 0.0, 270.0-azim );

	/* Look at the center of the model */
	mat_idn( toEye );
	toEye[MDX] = -(rtip->mdl_max[X]+rtip->mdl_min[X])/2;
	toEye[MDY] = -(rtip->mdl_max[Y]+rtip->mdl_min[Y])/2;
	toEye[MDZ] = -(rtip->mdl_max[Z]+rtip->mdl_min[Z])/2;

	/* Fit a sphere to the model RPP, diameter is viewsize,
	 * unless viewsize command used to override.
	 */
	if( viewsize <= 0 ) {
		VSUB2( diag, rtip->mdl_max, rtip->mdl_min );
		viewsize = MAGNITUDE( diag );
		if( aspect > 1 ) {
			/* don't clip any of the image when autoscaling */
			viewsize *= aspect;
		}
	}
	Viewrotscale[15] = 0.5*viewsize;	/* Viewscale */
	mat_mul( model2view, Viewrotscale, toEye );
	mat_inv( view2model, model2view );
	VSET( temp, 0, 0, eye_backoff );
	MAT4X3PNT( eye_model, view2model, temp );
}

/*
 *			R E S _ P R
 */
void
res_pr()
{
	register struct resource *res;
	register int i;

	fprintf(stderr,"\nResource use summary, by processor:\n");
	res = &resource[0];
	for( i=0; i<npsw; i++, res++ )  {
		fprintf(stderr, "---CPU %d:\n", i);
		if( res->re_magic != RESOURCE_MAGIC )  {
			fprintf(stderr,"Bad magic number!!\n");
			continue;
		}
		fprintf(stderr,"seg       len=%10d get=%10d free=%10d\n",
			res->re_seglen, res->re_segget, res->re_segfree );
		fprintf(stderr,"partition len=%10d get=%10d free=%10d\n",
			res->re_partlen, res->re_partget, res->re_partfree );
#if 0
		fprintf(stderr,"bitv_elem len=%10d get=%10d free=%10d\n",
			res->re_bitvlen, res->re_bitvget, res->re_bitvfree );
#endif
		fprintf(stderr,"boolstack len=%10d\n",
			res->re_boolslen);
	}
}

/*
 *  Command table for RT control script language
 */

struct command_tab rt_cmdtab[] = {
	"start", "frame number", "start a new frame",
		cm_start,	2, 2,
	"viewsize", "size in mm", "set view size",
		cm_vsize,	2, 2,
	"eye_pt", "xyz of eye", "set eye point",
		cm_eyept,	4, 4,
	"lookat_pt", "x y z [yflip]", "set eye look direction, in X-Y plane",
		cm_lookat_pt,	4, 5,
	"viewrot", "4x4 matrix", "set view direction from matrix",
		cm_vrot,	17,17,
	"orientation", "quaturnion", "set view direction from quaturnion",
		cm_orientation,	5, 5,
	"end", 	"", "end of frame setup, begin raytrace",
		cm_end,		1, 1,
	"multiview", "", "produce stock set of views",
		cm_multiview,	1, 1,
	"anim", 	"path type args", "specify articulation animation",
		cm_anim,	4, 999,
	"tree", 	"treetop(s)", "specify alternate list of tree tops",
		cm_tree,	1, 999,
	"clean", "", "clean articulation from previous frame",
		cm_clean,	1, 1,
	"_closedb", "", "Close .g database, (for memory debugging)",
		cm_closedb,	1, 1,
	"set", 	"", "show or set parameters",
		cm_set,		1, 999,
	"ae", "azim elev", "specify view as azim and elev, in degrees",
		cm_ae,		3, 3,
	"opt", "-flags", "set flags, like on command line",
		cm_opt,		2, 999,
	(char *)0, (char *)0, (char *)0,
		0,		0, 0	/* END */
};
