/*
	fbfade -- "twinkle" fade in or out a frame buffer image

	created:	89/04/26	D A Gwyn with help from G S Moss

	Typical compilation:	cc -O -I/usr/include/brlcad -o fbfade \
					fbfade.c /usr/brlcad/lib/libfb.a
	Add -DNO_DRAND48, -DNO_VFPRINTF, or -DNO_STRRCHR if drand48(),
	vfprintf(), or strrchr() are not present in your C library
	(e.g. on 4BSD-based systems).

	This program displays a frame buffer image gradually, randomly
	selecting the pixel display sequence.  (Suggested by Gary Moss.)
	It requires fast single-pixel write support for best effect.

	Options:

	-h		assumes 1024x1024 frame buffer instead of 512x512

	-i in_fb_file	reads from the specified frame buffer file instead
			of assuming constant black ("fade out") value

	-f out_fb_file	writes to the specified frame buffer file instead
			of the one specified by the FB_FILE environment
			variable (the default frame buffer, if no FB_FILE)

	-F out_fb_file	same as -f fb_file (BRL-CAD package compatibility)

	out_fb_file	same as -f fb_file, for convenience
*/
#ifndef lint
static char	SCCSid[] = "%W% %E%";	/* for "what" utility */
static char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#define	USAGE	\
	 "fbfade [ -h ] [ -i in_fb_file ] [ out_fb_file ]"
#define	OPTSTR	"hi:f:F:"

#ifdef BSD	/* BRL-CAD */
#define	NO_DRAND48	1
#define	NO_VFPRINTF	1
#define	NO_STRRCHR	1
#endif

#include	<signal.h>
#include	<stdio.h>
#include	<string.h>
#if __STDC__
#include	<stdarg.h>
#include	<stdlib.h>
#define	SIZE_T	size_t
extern int	getopt( int, char const * const *, char const * );
extern double	drand48( void );	/* in UNIX System V C library */
#else
#ifdef NO_STRRCHR
#define	strrchr( s, c )	rindex( s, c )
#endif
#include	<varargs.h>
#define	SIZE_T	unsigned
extern void	exit();
extern char	*malloc();
extern int	getopt();
extern double	drand48();
#endif
#ifndef EXIT_SUCCESS
#define	EXIT_SUCCESS	0
#endif
#ifndef EXIT_FAILURE
#define	EXIT_FAILURE	1
#endif
extern char	*optarg;
extern int	optind;

#include	<fb.h>			/* BRL CAD package libfb.a interface */

typedef int	bool;
#define	false	0
#define	true	1

static char	*arg0;			/* argv[0] for error message */
static bool	hires = false;		/* set for 1Kx1K; clear for 512x512 */
static char	*in_fb_file = NULL;	/* input frame buffer name */
static char	*out_fb_file = NULL;	/* output frame buffer name */
static FBIO	*fbp = FBIO_NULL;	/* input/output frame buffer handle */
static int	width, height;		/* full frame buffer size */
static RGBpixel	*pix;			/* input image */
static RGBpixel	bg = { 0, 0, 0 };	/* background */


#ifdef NO_DRAND48
/* Simulate drand48() using 31-bit random() assumed to exist (e.g. in 4BSD): */

double
drand48(
#if __STDC__
	 void
#endif
       )
	{
	extern long	random();

	return (double)random() / 2147483648.0;	/* range [0,1) */
	}
#endif


static char *
Simple( path )
	char		*path;
	{
	register char	*s;		/* -> past last '/' in path */

	return (s = strrchr( path, '/' )) == NULL || *++s == '\0' ? path : s;
	}


static void
VMessage( format, ap )
	char	*format;
	va_list	ap;
	{
	(void)fprintf( stderr, "%s: ", arg0 );
#ifdef NO_VFPRINTF
	(void)fprintf( stderr, format,	/* kludge city */
		       ((int *)ap)[0], ((int *)ap)[1],
		       ((int *)ap)[2], ((int *)ap)[3]
		     );
#else
	(void)vfprintf( stderr, format, ap );
#endif
	(void)putc( '\n', stderr );
	(void)fflush( stderr );
	}


#if __STDC__
static void
Message( char *format, ... )
#else
static void
Message( va_alist )
	va_dcl
#endif
	{
#if !__STDC__
	register char	*format;	/* must be picked up by va_arg() */
#endif
	va_list		ap;

#if __STDC__
	va_start( ap, format );
#else
	va_start( ap );
	format = va_arg( ap, char * );
#endif
	VMessage( format, ap );
	va_end( ap );
	}


#if __STDC__
static void
Fatal( char *format, ... )
#else
static void
Fatal( va_alist )
	va_dcl
#endif
	{
#if !__STDC__
	register char	*format;	/* must be picked up by va_arg() */
#endif
	va_list		ap;

#if __STDC__
	va_start( ap, format );
#else
	va_start( ap );
	format = va_arg( ap, char * );
#endif
	VMessage( format, ap );
	va_end( ap );

	if ( fbp != FBIO_NULL && fb_close( fbp ) == -1 )
		Message( "Error closing frame buffer" );

	exit( EXIT_FAILURE );
	/*NOTREACHED*/
	}


static void
Sig_Catcher( sig )
	int	sig;
	{
	(void)signal( sig, SIG_DFL );

	/* The following is not guaranteed to work, but it's worth a try. */
	Fatal( "Interrupted by signal %d", sig );
	}


int
main( argc, argv )
	int	argc;
	char	*argv[];
	{
	/* Plant signal catcher. */
	{
	static int	getsigs[] =	/* signals to catch */
		{
		SIGHUP,			/* hangup */
		SIGINT,			/* interrupt */
		SIGQUIT,		/* quit */
		SIGPIPE,		/* write on a broken pipe */
		SIGTERM,		/* software termination signal */
		0
		};
	register int	i;

	for ( i = 0; getsigs[i] != 0; ++i )
		if ( signal( getsigs[i], SIG_IGN ) != SIG_IGN )
			(void)signal( getsigs[i], Sig_Catcher );
	}

	/* Process arguments. */

	arg0 = Simple( argv[0] );	/* save for possible error message */

	{
		register int	c;
		register bool	errors = false;

		while ( (c = getopt( argc, argv, OPTSTR )) != EOF )
			switch( c )
				{
			default:	/* just in case */
			case '?':	/* invalid option */
				errors = true;
				break;

			case 'h':	/* -h */
				hires = true;
				break;

			case 'i':	/* -i in_fb_file */
				in_fb_file = optarg;
				break;

			case 'f':	/* -f out_fb_file */
			case 'F':	/* -F out_fb_file */
				out_fb_file = optarg;
				break;
				}

		if ( errors )
			Fatal( "Usage: %s", USAGE );
	}

	if ( optind < argc )		/* out_fb_file */
		{
		if ( optind < argc - 1 || out_fb_file != NULL )
			{
			Message( "Usage: %s", USAGE );
			Fatal( "Can't handle multiple output frame buffers!" );
			}

		out_fb_file = argv[optind];
		}

	/* Open frame buffer for unbuffered input. */

	width = height = hires ? 1024 : 512;	/* starting default */

	if ( in_fb_file != NULL )
		if ( (fbp = fb_open( in_fb_file, width, height )) == FBIO_NULL )
			Fatal( "Couldn't open input frame buffer" );
		else	{
			register int	y;
			register int	wt = fb_getwidth( fbp );
			register int	ht = fb_getheight( fbp );

			/* Use actual input image size instead of 512/1024. */

			width = wt;
			height = ht;

			if ( (long)(SIZE_T)((long)wt * (long)ht
					   * (long)sizeof(RGBpixel)
					   )
			  != (long)wt * (long)ht * (long)sizeof(RGBpixel)
			   )
				Fatal( "Integer overflow, malloc unusable" );

			if ( (pix = (RGBpixel *)malloc( (SIZE_T)wt * (SIZE_T)ht
						      * sizeof(RGBpixel)
						      )
			     ) == NULL
			   )
				Fatal( "Not enough memory for pixel array" );

			for ( y = 0; y < ht; ++y )
				if ( fb_read( fbp, 0, y, pix[y * wt], wt ) == -1
				   )
					Fatal( "Error reading raster" );

			if ( fb_close( fbp ) == -1 )
				{
				fbp = FBIO_NULL;	/* avoid second try */
				Fatal( "Error closing input frame buffer" );
				}
			}

	/* Open frame buffer for unbuffered output. */

	if ( (fbp = fb_open( out_fb_file, width, height )) == FBIO_NULL )
		Fatal( "Couldn't open output frame buffer" );
	else	{
		register int	wt = fb_getwidth( fbp );
		register int	ht = fb_getheight( fbp );

		if ( in_fb_file == NULL )
			{
			/* Use actual frame buffer size instead of 512/1024. */

			width = wt;
			height = ht;
			}
		else
			if ( wt < width || ht < height )
				Fatal(
			  "Output frame buffer too small (%dx%d); %dx%d needed",
				       wt, ht, width, height
				     );
		}

	/* The following is probably an optimally fast shuffling algorithm;
	   unfortunately, it requires a huge auxiliary array. */
	{
	register long	*loc;		/* keeps track of pixel shuffling */
	register long	wxh = (long)width * (long)height;
					/* down-counter */

	if ( (loc = (long *)malloc( (SIZE_T)wxh * sizeof(long) )) == NULL )
		Fatal( "Not enough memory for location array" );

	/* Initialize pixel location array to sequential order. */

	for ( wxh = (long)width * (long)height; --wxh >= 0L; )
		loc[wxh] = wxh;

	/* Select a pixel at random, paint it, and adjust the location array. */

	for ( wxh = (long)width * (long)height; --wxh >= 0L; )
		{
		register long	r = (long)((double)wxh * drand48());

		if ( fb_write( fbp,
			       (int)(loc[r] % width), (int)(loc[r] / width),
			       in_fb_file == NULL ? bg : pix[loc[r]],
			       1
			     ) == -1
		   )
			Fatal( "Error writing pixel" );

		loc[r] = loc[wxh];	/* track the shuffle */
		}
	}

	/* Close the frame buffer. */

	if ( fb_close( fbp ) == -1 )
		{
		fbp = FBIO_NULL;	/* avoid second try */
		Fatal( "Error closing output frame buffer" );
		}

	exit( EXIT_SUCCESS );
	}
