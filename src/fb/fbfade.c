/*                        F B F A D E . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2007 United States Government as represented by
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
 *
 */
/** @file fbfade.c
	fbfade -- "twinkle" fade in or out a frame buffer image

	created:	89/04/29	D A Gwyn with help from G S Moss

	Typical compilation:	cc -O -I/usr/include/brlcad -o fbfade \
					fbfade.c /usr/brlcad/lib/libfb.a
	Add -DNO_DRAND48, -DNO_VFPRINTF, or -DNO_STRRCHR if drand48(),
	vfprintf(), or strrchr() are not present in your C library
	(e.g. on 4BSD-based systems).

	This program displays a frame buffer image gradually, randomly
	selecting the pixel display sequence.  (Suggested by Gary Moss.)
	It requires fast single-pixel write support for best effect.

	Options:

	-h		assumes 1024x1024 default input size instead of 512x512

	-f in_fb_file	reads from the specified frame buffer file instead
			of assuming constant black ("fade out") value

	-s size		input size (width & height)

	-w width	input width

	-n height	input height

	-F out_fb_file	writes to the specified frame buffer file instead
			of the one specified by the FB_FILE environment
			variable (the default frame buffer, if no FB_FILE)

	-S size		output size (width & height)

	-W width	output width

	-N height	output height

	out_fb_file	same as -F out_fb_file, for convenience
*/
#ifndef lint
static const char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#define	USAGE1 "fbfade [ -s size ] [ -w width ] [ -n height ] [ -f in_fb_file ]"
#define	USAGE2	\
	"\t[ -h ] [ -S size ] [ -W width ] [ -N height ] [ [ -F ] out_fb_file ]"
#define	OPTSTR	"f:F:hn:N:s:S:w:W:"

#include "common.h"

#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif

#include	<signal.h>
#include	<stdlib.h>
#include	<stdio.h>
#ifdef HAVE_STRING_H
#include	<string.h>
#else
#include	<strings.h>
#endif
#ifdef HAVE_STDARG_H
#include	<stdarg.h>
#else
#include	<varargs.h>
#endif

#include "machine.h"
#include "bu.h"
#include "fb.h"			/* BRL-CAD package libfb.a interface */


#define SIZE_T size_t
typedef int bool_t;

static char	*arg0;			/* argv[0] for error message */
static bool_t	hires = 0;		/* set for 1Kx1K; clear for 512x512 */
static char	*in_fb_file = NULL;	/* input image name */
static char	*out_fb_file = NULL;	/* output frame buffer name */
static FBIO	*fbp = FBIO_NULL;	/* libfb input/output handle */
static int	src_width = 0,
		src_height = 0;		/* input image size */
static int	dst_width = 0,
		dst_height = 0;		/* output frame buffer size */
static RGBpixel	*pix;			/* input image */
static RGBpixel	bg = { 0, 0, 0 };	/* background */


#ifndef HAVE_DRAND48
/* Simulate drand48() using 31-bit random() assumed to exist (e.g. in 4BSD): */

double
drand48()
	{
	extern long	random();

	return (double)random() / 2147483648.0;	/* range [0,1) */
	}
#endif

static char *
Simple(char *path)
{
	register char	*s;		/* -> past last '/' in path */

	return (s = strrchr( path, '/' )) == NULL || *++s == '\0' ? path : s;
	}


static void
VMessage(char *format, va_list ap)
{
	(void)fprintf( stderr, "%s: ", arg0 );
#ifndef HAVE_VPRINTF
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


#if defined(HAVE_STDARG_H)
static void
Message( char *format, ... )
#else
static void
Message( va_alist )
	va_dcl
#endif
	{
#if !defined(HAVE_STDARG_H)
	register char	*format;	/* must be picked up by va_arg() */
#endif
	va_list		ap;

#if defined(HAVE_STDARG_H)
	va_start( ap, format );
#else
	va_start( ap );
	format = va_arg( ap, char * );
#endif
	VMessage( format, ap );
	va_end( ap );
	}


#if defined(HAVE_STDARG_H)
static void
Fatal( char *format, ... )
#else
static void
Fatal( va_alist )
	va_dcl
#endif
	{
#if !defined(HAVE_STDARG_H)
	register char	*format;	/* must be picked up by va_arg() */
#endif
	va_list		ap;

#if defined(HAVE_STDARG_H)
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
Sig_Catcher(int sig)
{
	(void)signal( sig, SIG_DFL );

	/* The following is not guaranteed to work, but it's worth a try. */
	Fatal( "Interrupted by signal %d", sig );
	}


int
main(int argc, char **argv)
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
		register bool_t	errors = 0;

		while ( (c = bu_getopt( argc, argv, OPTSTR )) != EOF )
			switch( c )
				{
			default:	/* '?': invalid option */
				errors = 1;
				break;

			case 'f':	/* -f in_fb_file */
				in_fb_file = bu_optarg;
				break;

			case 'F':	/* -F out_fb_file */
				out_fb_file = bu_optarg;
				break;

			case 'h':	/* -h */
				hires = 1;
				break;

			case 'n':	/* -n height */
				if ( (src_height = atoi( bu_optarg )) <= 0 )
					errors = 1;

				break;

			case 'N':	/* -N height */
				if ( (dst_height = atoi( bu_optarg )) <= 0 )
					errors = 1;

				break;

			case 's':	/* -s size */
				if ( (src_height = src_width = atoi( bu_optarg ))
				  <= 0
				   )
					errors = 1;

				break;

			case 'S':	/* -S size */
				if ( (dst_height = dst_width = atoi( bu_optarg ))
				  <= 0
				   )
					errors = 1;

				break;

			case 'w':	/* -w width */
				if ( (src_width = atoi( bu_optarg )) <= 0 )
					errors = 1;

				break;

			case 'W':	/* -W width */
				if ( (dst_width = atoi( bu_optarg )) <= 0 )
					errors = 1;

				break;
				}

		if ( errors )
			Fatal( "Usage: %s\n%s", USAGE1, USAGE2 );
	}

	if ( bu_optind < argc )		/* out_fb_file */
		{
		if ( bu_optind < argc - 1 || out_fb_file != NULL )
			{
			Message( "Usage: %s\n%s", USAGE1, USAGE2 );
			Fatal( "Can't handle multiple output frame buffers!" );
			}

		out_fb_file = argv[bu_optind];
		}

	/* Open frame buffer for unbuffered input. */

	if ( src_width == 0 )
		src_width = hires ? 1024 : 512;		/* starting default */

	if ( src_height == 0 )
		src_height = hires ? 1024 : 512;	/* starting default */

	if ( in_fb_file != NULL ) {
		if ( (fbp = fb_open( in_fb_file, src_width, src_height ))
		  == FBIO_NULL
		   )
			Fatal( "Couldn't open input frame buffer" );
		else	{
			register int	y;
			register int	wt = fb_getwidth( fbp );
			register int	ht = fb_getheight( fbp );

			/* Use smaller actual input size instead of request. */

			if ( wt < src_width )
				src_width = wt;

			if ( ht < src_height )
				src_height = ht;

			if ( (long)(SIZE_T)((long)src_width * (long)src_height
					   * (long)sizeof(RGBpixel)
					   )
			  != (long)src_width * (long)src_height
					     * (long)sizeof(RGBpixel)
			   )
				Fatal( "Integer overflow, malloc unusable" );

			if ( (pix = (RGBpixel *)malloc( (SIZE_T)src_width
						      * (SIZE_T)src_height
						      * sizeof(RGBpixel)
						      )
			     ) == NULL
			   )
				Fatal( "Not enough memory for pixel array" );

			for ( y = 0; y < src_height; ++y )
				if ( fb_read( fbp, 0, y, pix[y * src_width],
					      src_width
					    ) == -1
				   )
					Fatal( "Error reading raster" );

			if ( fb_close( fbp ) == -1 )
				{
				fbp = FBIO_NULL;	/* avoid second try */
				Fatal( "Error closing input frame buffer" );
				}
			}
	}

	/* Open frame buffer for unbuffered output. */

	if ( dst_width == 0 )
		dst_width = src_width;		/* default */

	if ( dst_height == 0 )
		dst_height = src_height;	/* default */

	if ( (fbp = fb_open( out_fb_file, dst_width, dst_height )) == FBIO_NULL
	   )
		Fatal( "Couldn't open output frame buffer" );
	else	{
		register int	wt = fb_getwidth( fbp );
		register int	ht = fb_getheight( fbp );

		/* Use smaller actual frame buffer size for output. */

		if ( wt < dst_width )
			dst_width = wt;

		if ( ht < dst_height )
			dst_height = ht;

		/* Avoid selecting pixels outside the input image. */

		if ( dst_width > src_width )
			dst_width = src_width;

		if ( dst_height > src_height )
			dst_height = src_height;
		}

	/* The following is probably an optimally fast shuffling algorithm;
	   unfortunately, it requires a huge auxiliary array.  The way it
	   works is to start with an array of all pixel indices, then repeat:
	   select an entry at random from the array, output that index, replace
	   that entry with the last array entry, then reduce the array size. */
	{
	register long	*loc;		/* keeps track of pixel shuffling */
	register long	wxh = (long)dst_width * (long)dst_height;
					/* down-counter */

	if ( (long)(SIZE_T)(wxh * (long)sizeof(long))
	  != wxh * (long)sizeof(long)
	   )
		Fatal( "Integer overflow, malloc unusable" );

	if ( (loc = (long *)malloc( (SIZE_T)wxh * sizeof(long) )) == NULL )
		Fatal( "Not enough memory for location array" );

	/* Initialize pixel location array to sequential order. */

	while ( --wxh >= 0L )
		loc[wxh] = wxh;

	/* Select a pixel at random, paint it, and adjust the location array. */

	for ( wxh = (long)dst_width * (long)dst_height; --wxh >= 0L; )
		{
		register long	r = (long)((double)wxh * drand48());
		register long	x = loc[r] % dst_width;
		register long	y = loc[r] / dst_width;

		if ( fb_write( fbp, (int)x, (int)y,
			       in_fb_file == NULL ? bg
						  : pix[x + y * src_width],
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

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
