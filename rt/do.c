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

#include <stdio.h>
#include <ctype.h>
#include <math.h>
#include "machine.h"
#include "vmath.h"
#include "raytrace.h"
#include "fb.h"
#include "./material.h"
#include "./mathtab.h"
#include "./rdebug.h"
#include "../librt/debug.h"

#ifdef unix
# include <sys/types.h>
# include <sys/stat.h>
#endif

#ifdef HEP
# include <synch.h>
# undef stderr
# define stderr stdout
# define PARALLEL 1
#endif

extern char	*sbrk();

extern int	rdebug;			/* RT program debugging (not library) */

/***** Variables shared with viewing model *** */
extern FILE	*outfp;			/* optional pixel output file */
extern int	hex_out;		/* Binary or Hex .pix output file */
extern double	azimuth, elevation;
extern mat_t	view2model;
extern mat_t	model2view;
extern int	using_mlib;		/* !0 = material routines used */
/***** end of sharing with viewing model *****/

extern void	grid_setup();
extern void	worker();

/***** variables shared with worker() ******/
extern struct application ap;
extern int	hypersample;		/* number of extra rays to fire */
extern fastf_t	aspect;			/* view aspect ratio X/Y */
extern point_t	eye_model;		/* model-space location of eye */
extern fastf_t  eye_backoff;		/* dist of eye from center */
extern fastf_t	rt_perspective;		/* persp (degrees X) 0 => ortho */
extern int	width;			/* # of pixels in X */
extern int	height;			/* # of lines in Y */
extern mat_t	Viewrotscale;
extern fastf_t	viewsize;
extern char	*scanbuf;		/* For optional output buffering */
extern int	incr_mode;		/* !0 for incremental resolution */
extern int	incr_level;		/* current incremental level */
extern int	incr_nlevel;		/* number of levels */
extern int	npsw;
extern struct resource resource[];
/***** end variables shared with worker() */

/***** variables shared with rt.c *****/
extern int	pix_start;		/* pixel to start at */
extern int	pix_end;		/* pixel to end at */
extern int	nobjs;			/* Number of cmd-line treetops */
extern char	**objtab;		/* array of treetop strings */
extern char	*beginptr;		/* sbrk() at start of program */
extern int	matflag;		/* read matrix from stdin */
extern int	desiredframe;		/* frame to start at */
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
	viewsize = -42.0;
	if( old_frame( fp ) < 0 || viewsize <= 0.0 )  {
		rewind( fp );
		return(0);		/* Not old way */
	}
	rt_log("Interpreting command stream in old format\n");

	def_tree( ap.a_rt_i );		/* Load the default trees */

	curframe = 0;
	do {
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
		rt_free( buf, "cmd buf (skiping frames)" );
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
int	argc;
char	**argv;
{
	register struct rt_i *rtip = ap.a_rt_i;
	char outbuf[132];
	register int i;

	if( argc <= 1 )  {
		def_tree( rtip );		/* Load the default trees */
		return(0);
	}
	rt_prep_timer();
	for( i=1; i < argc; i++ )  {
		if( rt_gettree(rtip, argv[i]) < 0 )
			fprintf(stderr,"rt_gettree(%s) FAILED\n", argv[i]);
	}
	(void)rt_read_timer( outbuf, sizeof(outbuf) );
	fprintf(stderr,"GETTREE: %s\n", outbuf);
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
	struct rt_i *rtip = ap.a_rt_i;
	struct animate *anp;
	struct directory **dir;
	int i;
	int	at_root = 0;

	if( argv[1][0] == '/' )
		at_root = 1;
	if( (i = rt_plookup( rtip, &dir, argv[1], LOOKUP_NOISY )) <= 0 )
		return(-1);		/* error */
	if( i > 1 )
		at_root = 0;

	GETSTRUCT( anp, animate );
	anp->an_path = dir;
	anp->an_pathlen = i;

	if( strcmp( argv[2], "matrix" ) == 0 )  {
		anp->an_type = AN_MATRIX;
		if( strcmp( argv[3], "rstack" ) == 0 )
			anp->an_u.anu_m.anm_op = ANM_RSTACK;
		else if( strcmp( argv[3], "rarc" ) == 0 )
			anp->an_u.anu_m.anm_op = ANM_RARC;
		else if( strcmp( argv[3], "lmul" ) == 0 )
			anp->an_u.anu_m.anm_op = ANM_LMUL;
		else if( strcmp( argv[3], "rmul" ) == 0 )
			anp->an_u.anu_m.anm_op = ANM_RMUL;
		else if( strcmp( argv[3], "rboth" ) == 0 )
			anp->an_u.anu_m.anm_op = ANM_RBOTH;
		else  {
			fprintf(stderr,"cm_anim:  Matrix op %s unknown\n",
				argv[3]);
			goto bad;
		}
		for( i=0; i<16; i++ )
			anp->an_u.anu_m.anm_mat[i] = atof( argv[i+4] );
	} else {
		fprintf(stderr,"cm_anim:  type %s unknown\n", argv[2]);
		goto bad;
	}
	if( rt_add_anim( rtip, anp, at_root ) < 0 )  {
		fprintf(stderr,"cm_anim:  %s %s failed\n", argv[1], argv[2]);
		goto bad;
	}
	return(0);
bad:
	rt_free( (char *)dir, "directory []");
	rt_free( (char *)anp, "animate");
	return(-1);		/* BAD */
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
	register struct region *regp;

	/* The linkage here needs to be much better for things
	 * like rtrad, etc. XXXX
	 */
	for( regp=ap.a_rt_i->HeadRegion; regp != REGION_NULL; regp=regp->reg_forw )
		mlib_free( regp );
	rt_clean( ap.a_rt_i );
	if(rdebug&RDEBUG_RTMEM)
		rt_prmem( "After rt_clean" );
	return(0);
}

/* viewing module specific variables */
extern struct structparse view_parse[];

/*
 *  Generic settable parameters.
 *  By setting the "base address" to zero in the rt_structparse call,
 *  the actual memory address is given here as the structure offset.
 */
struct structparse set_parse[] = {
	"%d",	"width",	(stroff_t)&width,		FUNC_NULL,
	"%d",	"height",	(stroff_t)&height,		FUNC_NULL,
	"%f",	"angle",	(stroff_t)&rt_perspective,	FUNC_NULL,
	"indir", "View Module",	(stroff_t)view_parse,		FUNC_NULL,
	(char *)0,(char *)0,	(stroff_t)0,			FUNC_NULL
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
	if( argc <= 1 ) {
		rt_structprint( "Generic and Application-Specific Parameter Values",
			set_parse, (stroff_t)0 );
		return(0);
	}
	while( argc > 1 ) {
		rt_structparse( argv[1], set_parse, (stroff_t)0 );
		argc--;
		argv++;
	}
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
	char outbuf[132];
	register int i;

	rt_prep_timer();
	for( i=0; i < nobjs; i++ )  {
		if( rt_gettree(rtip, objtab[i]) < 0 )
			fprintf(stderr,"rt_gettree(%s) FAILED\n", objtab[i]);
	}
	(void)rt_read_timer( outbuf, sizeof(outbuf) );
	fprintf(stderr,"GETTREE: %s\n", outbuf);
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
	char outbuf[132];
	char framename[128];		/* File name to hold current frame */
	struct rt_i *rtip = ap.a_rt_i;
	static double utime;
	register struct region *regp;
	int	npix;			/* # of pixel values to be done */
	int	lim;

	fprintf(stderr, "\n...................Frame %5d...................\n",
		framenumber);

	/*
	 *  Deal with CPU limits and priorities, where appropriate.
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

	if( rtip->needprep )  {

		/*
		 *  Initialize the material library for all regions.
		 *  As this may result in some regions being dropped,
		 *  (eg, light solids that become "implicit" -- non drawn),
		 *  this must be done before allowing the library to prep
		 *  itself.  This is a slight layering violation;  later it
		 *  may be clear how to repackage this operation.
		 */
		for( regp=rtip->HeadRegion; regp != REGION_NULL; )  {
			if(using_mlib) switch( mlib_setup( regp ) )  {
			case -1:
			default:
				rt_log("mlib_setup failure on %s\n", regp->reg_name);
				break;
			case 0:
				if(rdebug&RDEBUG_MATERIAL)
					rt_log("mlib_setup: drop region %s\n", regp->reg_name);
				{
					struct region *r = regp->reg_forw;
					/* zap reg_udata? beware of light structs */
					rt_del_regtree( rtip, regp );
					regp = r;
					continue;
				}
			case 1:
				/* Full success */
				if(rdebug&RDEBUG_MATERIAL)
					((struct mfuncs *)(regp->reg_mfuncs))->
						mf_print( regp, regp->reg_udata );
				/* Perhaps this should be a function? */
				break;
			}
			regp = regp->reg_forw;
		}

		/* Allow RT library to prepare itself */
		rt_prep_timer();
		rt_prep(rtip);

		(void)rt_read_timer( outbuf, sizeof(outbuf) );
		fprintf(stderr, "PREP: %s\n", outbuf );
	}

	if( rt_g.rtg_parallel && resource[0].re_seg == SEG_NULL )  {
		register int x;
		/* 
		 *  Get dynamic memory to keep from having to call
		 *  malloc(), because the portability of doing sbrk()
		 *  sys-calls when running in parallel mode is unknown.
		 */
		for( x=0; x<npsw; x++ )  {
			rt_get_seg(&resource[x]);
			rt_get_pt(rtip, &resource[x]);
			rt_get_bitv(rtip, &resource[x]);
		}
		fprintf(stderr,"Additional dynamic memory used=%d. bytes\n",
			sbrk(0)-beginptr );
		beginptr = sbrk(0);
	}

	fprintf(stderr,"shooting at %d solids in %d regions\n",
		rtip->nsolids, rtip->nregions );
	if( rtip->HeadSolid == SOLTAB_NULL )  {
		fprintf(stderr,"rt ERROR: No solids\n");
		exit(3);
	}
	fprintf(stderr,"model X(%g,%g), Y(%g,%g), Z(%g,%g)\n",
		rtip->mdl_min[X], rtip->mdl_max[X],
		rtip->mdl_min[Y], rtip->mdl_max[Y],
		rtip->mdl_min[Z], rtip->mdl_max[Z] );

	if(rdebug&RDEBUG_RTMEM)
		rt_g.debug |= DEBUG_MEM;	/* Just for the tracing */

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
#ifdef unix
		/*
		 *  This code allows the computation of a particular frame
		 *  to a disk file to be resumed automaticly.
		 *  This is worthwhile crash protection.
		 *  This use of stat() and fseek() is UNIX-specific.
		 */
		{
			struct stat sb;
			if( stat( framename, &sb ) >= 0 && sb.st_size > 0 )  {
				/* File exists, with partial results */
				register int pixoff, des_pos, fd;
				pixoff = sb.st_size / sizeof(RGBpixel);
				des_pos = pixoff * sizeof(RGBpixel);
				if( pix_start == 0 && pix_end > pixoff )  {
					fprintf(stderr, "Continuing with pixel %d (%d, %d) [size %d, want %d]\n",
						pixoff,
						pixoff % width,
						pixoff / width,
						sb.st_size,
						des_pos );
					pix_start = pixoff;
					/*
					 *  Append to existing UNIX file.
					 *  Ensure that positioning is precisely pixel aligned.
					 *  The file size is almost certainly
					 *  not an exact multiple of three bytes.
					 *  Use UNIX sys-calls here because SYSV
					 *  stdio & fseek() on append-mode files
					 *  don't seem to work right, and this
					 *  way is simpler anyway.
					 */
					if( (fd = open( framename, 2 )) < 0 ||
					    (outfp = fdopen( fd, "r+" )) == NULL )  {
						perror( framename );
						if( matflag )  return(0);	/* OK */
						return(-1);			/* Bad */
					}
					(void)fseek( outfp, (long)des_pos, 0);
					/*
					 *  view_2init() reads the file into
					 *  scanbuf, when needed by buffering.
					 */
				}
			}
		}
#endif
		if( outfp == NULL && (outfp = fopen( framename, "w" )) == NULL )  {
			perror( framename );
			if( matflag )  return(0);	/* OK */
			return(-1);			/* Bad */
		}
#endif CRAY_COS
		fprintf(stderr,"Output file is '%s'\n", framename);
	}

	grid_setup();
	fprintf(stderr,"Beam radius=%g mm, divergence=%g mm/1mm\n",
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

	fprintf(stderr,"\n");
	fflush(stdout);
	fflush(stderr);

	/*
	 *  Compute the image
	 *  It may prove desirable to do this in chunks
	 */
	rt_prep_timer();
	if( incr_mode )  {
		for( incr_level = 0; incr_level < incr_nlevel; incr_level++ )  {
			if( incr_level > 0 )
				view_2init( &ap );
			do_run( 0, (1<<incr_level)*(1<<incr_level)-1 );
			view_end( &ap );
		}
	} else {
		do_run( pix_start, pix_end );

		/* Reset values to full size, for next frame (if any) */
		pix_start = 0;
		pix_end = height*width - 1;
	}
	utime = rt_read_timer( outbuf, sizeof(outbuf) );

	/*
	 *  End of application.  Done outside of timing section.
	 *  Typically, writes any remaining results out.
	 */
	view_end( &ap );

	if(rdebug&RDEBUG_RTMEM)
		rt_g.debug &= ~DEBUG_MEM;	/* Stop until next frame */

	/*
	 *  All done.  Display run statistics.
	 */
	fprintf(stderr,"SHOT: %s\n", outbuf );
	fprintf(stderr,"Additional dynamic memory used=%d. bytes\n",
		sbrk(0)-beginptr );
		beginptr = sbrk(0);
	fprintf(stderr,"%ld solid/ray intersections: %ld hits + %ld miss\n",
		rtip->nshots, rtip->nhits, rtip->nmiss );
	fprintf(stderr,"pruned %.1f%%:  %ld model RPP, %ld dups skipped, %ld solid RPP\n",
		rtip->nshots>0?((double)rtip->nhits*100.0)/rtip->nshots:100.0,
		rtip->nmiss_model, rtip->nmiss_tree, rtip->nmiss_solid );
	fprintf(stderr,
		"Frame %5d: %8d pixels in %10.2f sec = %10.2f pixels/sec\n",
		framenumber,
		width*height, utime, (double)(width*height)/utime );
	fprintf(stderr,
		"Frame %5d: %8d rays   in %10.2f sec = %10.2f rays/sec (RTFM)\n",
		framenumber,
		rtip->rti_nrays, utime, (double)(rtip->rti_nrays)/utime );

	if( outfp != NULL )  {
#ifdef CRAY_COS
		int status;
		char dn[16];
		char message[128];

		strncpy( dn, outfp->ldn, sizeof(outfp->ldn) );	/* COS name */
#endif CRAY_COS
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
#else
		/* Protect finished product */
		if( outputfile != (char *)0 )
			chmod( framename, 0444 );
#endif CRAY_COS
	}

#ifdef STAT_PARALLEL
	lock_pr();
	res_pr();
#endif STAT_PARALLEL

	fprintf(stderr,"\n");
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

	if( rtip->mdl_max[X] >= INFINITY || rtip->mdl_max[X] <= -INFINITY )
		rt_bomb("do_ae called before rt_gettree");


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
	fprintf(stderr,
		"Viewing %g azimuth, %g elevation off of front view\n",
		azim, elev);

	/* Look at the center of the model */
	mat_idn( toEye );
	toEye[MDX] = -(rtip->mdl_max[X]+rtip->mdl_min[X])/2;
	toEye[MDY] = -(rtip->mdl_max[Y]+rtip->mdl_min[Y])/2;
	toEye[MDZ] = -(rtip->mdl_max[Z]+rtip->mdl_min[Z])/2;

	/* Fit a sphere to the model RPP, diameter is viewsize,
	 * unless viewsize command used to override.
	 */
	VSUB2( diag, rtip->mdl_max, rtip->mdl_min );
	if( viewsize <= 0 ) {
		viewsize = MAGNITUDE( diag );
		if( aspect > 1 ) {
			/* don't clip any of the image when autoscaling */
			viewsize *= aspect;
		}
	}
	fprintf(stderr,"view size = %g\n", viewsize);

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
