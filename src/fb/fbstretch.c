/*                     F B S T R E T C H . C
 * BRL-CAD
 *
 * Copyright (C) 2004-2005 United States Government as represented by
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
 *
 */
/** @file fbstretch.c
	fbstretch -- stretch a frame buffer image

	created:	89/04/29	D A Gwyn

	Typical compilation:	cc -O -I/usr/include/brlcad -o fbstretch \
					fbstretch.c /usr/brlcad/lib/libfb.a
	Add -DNO_VFPRINTF or -DNO_STRRCHR if vfprintf() or strrchr()
	are not present in your C library (e.g. on 4BSD-based systems).

	This program converts a frame buffer image so that it is stretched or
	compressed in the horizontal and/or vertical directions.  The image
	scaling origin is taken to be the lower left-hand corner of the
	display.  When compressing, pixel averaging is used by default;
	when expanding, pixel replication is used.  Pixel averaging may be
	meaningless for some color maps, so there is an option to use
	sampling instead.

	The main use of this utility is to compensate for differences in
	pixel aspect ratios among different display devices.

	Options:

	-a		"no averaging": samples for compression instead of
			averaging pixels

	-v		"verbose": prints information about sizes and scaling
			on the standard error output

	-x x_scale	horizontal scaling factor (default: out width/in width)

	-y y_scale	vertical scaling factor (default: out height/in height)

	-f in_fb	reads from the specified frame buffer file instead
			of modifying the one specified by the -f option "in
			place"

	-F out_fb	writes to the specified frame buffer file instead
			of the one specified by the FB_FILE environment
			variable (the default frame buffer, if no FB_FILE)

	-h		assumes 1024x1024 default input size instead of 512x512

	-s size		input size (width & height)

	-w width	input width

	-n height	input height

	-S size		output size (width & height)

	-W width	output width

	-N height	output height

	out_fb		same as -F out_fb, for convenience
*/
#ifndef lint
static const char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "common.h"

#ifdef HAVE_UNISTD_H
#  include <unistd.h>
#endif

#include	<assert.h>
#include	<signal.h>
#include	<stdio.h>
#include	<stdlib.h>
#ifdef HAVE_STRING_H
#  include	<string.h>
#else
#  include	<strings.h>
#endif
#if HAVE_STDARG_H
#  include	<stdarg.h>
#else
#  include	<varargs.h>
#endif

#include "machine.h"
#include "bu.h"
#include "fb.h"			/* BRL-CAD package libfb.a interface */


#define	USAGE1	"fbstretch [ -h ] [ -s size ] [ -w width ] [ -n height ]"
#define	USAGE2	"\t[ -f in_fb_file ] [ -a ] [ -v ] [ -x x_sc ] [ -y y_sc ]"
#define	USAGE3 "\t[ -S size ] [ -W width ] [ -N height ] [ [ -F ] out_fb_file ]"
#define	OPTSTR	"af:F:hn:N:s:S:vw:W:x:y:"

#ifndef EXIT_SUCCESS
#define	EXIT_SUCCESS	0
#endif
#ifndef EXIT_FAILURE
#define	EXIT_FAILURE	1
#endif

typedef int	bool;
#define	false	0
#define	true	1

#define	EPSILON	0.0001			/* fudge for converting float to int */

static char	*arg0;			/* argv[0] for error message */
static bool	hires = false;		/* set for 1Kx1K; clear for 512x512 */
static bool	sample = false;		/* set: sampling; clear: averaging */
static bool	verbose = false;	/* set for size info printout */
static float	x_scale = -1.0;		/* horizontal scaling factor */
static float	y_scale = -1.0;		/* vertical scaling factor */
static bool	x_compress;		/* set iff compressing horizontally */
static bool	y_compress;		/* set iff compressing vertically */
static char	*src_file = NULL;	/* source frame buffer name */
static FBIO	*src_fbp = FBIO_NULL;	/* source frame buffer handle */
static char	*dst_file = NULL;	/* destination frame buffer name */
static FBIO	*dst_fbp = FBIO_NULL;	/* destination frame buffer handle */
static int	src_width = 0,
		src_height = 0;		/* source image size */
static int	dst_width = 0,
		dst_height = 0;		/* destination image size */
static unsigned char	*src_buf;		/* calloc()ed input scan line buffer */
static unsigned char	*dst_buf;		/* calloc()ed output scan line buffer */


static char *
Simple(char *path)
{
	register char	*s;		/* -> past last '/' in path */

	s = strrchr( path, '/' );
	return s == NULL || *++s == '\0' ? path : s;
	}


static void
VMessage(char *format, va_list ap)
{
	(void)fprintf( stderr, "%s: ", arg0 );
#if !defined(HAVE_VPRINTF)
	(void)fprintf( stderr, format,	/* kludge city */
		       ((int *)ap)[0], ((int *)ap)[1],
		       ((int *)ap)[2], ((int *)ap)[3],
		       ((int *)ap)[4], ((int *)ap)[5],
		       ((int *)ap)[6], ((int *)ap)[7]
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

	if ( src_fbp != FBIO_NULL && fb_close( src_fbp ) == -1 )
		Message( "Error closing input frame buffer" );

	if ( dst_fbp != FBIO_NULL && dst_fbp != src_fbp
	  && fb_close( dst_fbp ) == -1
	   )
		Message( "Error closing output frame buffer" );

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
		register bool	errors = false;

		while ( (c = bu_getopt( argc, argv, OPTSTR )) != EOF )
			switch( c )
				{
			default:	/* '?': invalid option */
				errors = true;
				break;

			case 'a':	/* -a */
				sample = true;
				break;

			case 'f':	/* -f in_fb */
				src_file = bu_optarg;
				break;

			case 'F':	/* -F out_fb */
				dst_file = bu_optarg;
				break;

			case 'h':	/* -h */
				hires = true;
				break;

			case 'n':	/* -n height */
				if ( (src_height = atoi( bu_optarg )) <= 0 )
					errors = true;

				break;

			case 'N':	/* -N height */
				if ( (dst_height = atoi( bu_optarg )) <= 0 )
					errors = true;

				break;

			case 's':	/* -s size */
				if ( (src_height = src_width = atoi( bu_optarg ))
				  <= 0
				   )
					errors = true;

				break;

			case 'S':	/* -S size */
				if ( (dst_height = dst_width = atoi( bu_optarg ))
				  <= 0
				   )
					errors = true;

				break;

			case 'v':
				verbose = true;
				break;

			case 'w':	/* -w width */
				if ( (src_width = atoi( bu_optarg )) <= 0 )
					errors = true;

				break;

			case 'W':	/* -W width */
				if ( (dst_width = atoi( bu_optarg )) <= 0 )
					errors = true;

				break;

			case 'x':	/* -x x_scale */
				if ( (x_scale = atof( bu_optarg )) <= 0 )
					{
					Message( "Nonpositive x scale factor" );
					errors = true;
					}

				break;

			case 'y':	/* -y y_scale */
				if ( (y_scale = atof( bu_optarg )) <= 0 )
					{
					Message( "Nonpositive y scale factor" );
					errors = true;
					}

				break;
				}

		if ( errors )
			Fatal( "Usage: %s\n%s\n%s", USAGE1, USAGE2, USAGE3 );
	}

	if ( bu_optind < argc )		/* dst_file */
		{
		if ( bu_optind < argc - 1 || dst_file != NULL )
			{
			Message( "Usage: %s\n%s\n%s", USAGE1, USAGE2, USAGE3 );
			Fatal( "Can't handle multiple output frame buffers!" );
			}

		dst_file = argv[bu_optind];
		}

	if ( dst_file == NULL )
		dst_file = getenv( "FB_FILE" );	/* needed for later strcmp */

	/* Figure out what scale factors to use before messing up size info. */

	if ( x_scale < 0.0 ) {
		if ( src_width == 0 || dst_width == 0 )
			x_scale = 1.0;
		else
			x_scale = (double)dst_width / (double)src_width;
	}

	if ( y_scale < 0.0 ) {
		if ( src_height == 0 || dst_height == 0 )
			y_scale = 1.0;
		else
			y_scale = (double)dst_height / (double)src_height;
	}

	if ( verbose )
		Message( "Scale factors %gx%g", x_scale, y_scale );

	/* Open frame buffer(s) for unbuffered input/output. */

	if ( src_width == 0 )
		src_width = hires ? 1024 : 512;		/* starting default */

	if ( src_height == 0 )
		src_height = hires ? 1024 : 512;	/* starting default */

	if ( (src_fbp = fb_open( src_file == NULL ? dst_file : src_file,
				 src_width, src_height
			       )
	     ) == FBIO_NULL
	   )
		Fatal( "Couldn't open input image" );
	else	{
		register int	wt, ht;	/* actual frame buffer size */

		/* Use smaller input size in preference to requested size. */

		if ( (wt = fb_getwidth( src_fbp )) < src_width )
			src_width = wt;

		if ( (ht = fb_getheight( src_fbp )) < src_height )
			src_height = ht;

		if ( verbose )
			Message( "Source image %dx%d", src_width, src_height );

		if ( dst_width == 0 )
			dst_width = src_width * x_scale + EPSILON;

		if ( dst_height == 0 )
			dst_height = src_height * y_scale + EPSILON;

		if ( verbose )
			Message( "Requested output size %dx%d",
				 dst_width, dst_height
			       );

		if ( src_file == NULL
		     || (dst_file != NULL && strcmp( src_file, dst_file ) == 0)
		   )
			dst_fbp = src_fbp;	/* No No No Not a Second Time */
		else if ( (dst_fbp = fb_open( dst_file, dst_width, dst_height ))
		       == FBIO_NULL
			)
			Fatal( "Couldn't open output frame buffer" );

		/* Use smaller output size in preference to requested size. */

		if ( (wt = fb_getwidth( dst_fbp )) < dst_width )
			dst_width = wt;

		if ( (ht = fb_getheight( dst_fbp )) < dst_height )
			dst_height = ht;

		if ( verbose )
			Message( "Destination image %dx%d",
				 dst_width, dst_height
			       );
	}

	/* Determine compression/expansion directions. */

	x_compress = x_scale < 1 - EPSILON;
	y_compress = y_scale < 1 - EPSILON;

	/* Allocate input/output scan line buffers.  These could overlap, but
	   I decided to keep them separate for simplicity.  The algorithms are
	   arranged so that source and destination can access the same image; if
	   at some future time offsets are supported, that would no longer hold.
	   calloc is used instead of malloc just to avoid integer overflow. */

	if ( (src_buf = (unsigned char *)calloc(
		       y_compress ? (int)(1 / y_scale + 1 - EPSILON) * src_width
				  : src_width,
					    sizeof(RGBpixel)
					  )
	     ) == NULL
	  || (dst_buf = (unsigned char *)calloc(
		       y_compress ? dst_width
				  : (int)(y_scale + 1 - EPSILON) * dst_width,
					    sizeof(RGBpixel)
					  )
	     ) == NULL
	   )
		Fatal( "Insufficient memory for scan line buffers." );

#define	Src( x, y )	(&src_buf[(x) + src_width * (y) * sizeof(RGBpixel)])
#define	Dst( x, y )	(&dst_buf[(x) + dst_width * (y) * sizeof(RGBpixel)])

	/* Do the horizontal/vertical expansion/compression.  I wanted to merge
	   these but didn't like the extra bookkeeping overhead in the loops. */

	if ( x_compress && y_compress )
		{
		register int	src_x, src_y;	/* source rect. pixel coords. */
		register int	dst_x, dst_y;	/* destination pixel coords. */
		register int	top_x, top_y;	/* source rect. upper bounds */
		register int	bot_x, bot_y;	/* source rect. lower bounds */

#if 0	/* (already done by calloc()!) */
		/* Clear end of output scan line buffer to background. */

		for ( dst_x = src_width * y_scale + EPSILON;
		      dst_x < dst_width;
		      ++dst_x
		    )	{
			assert(dst_x >= 0);
			Dst( dst_x, 0 )[RED] = 0;
			Dst( dst_x, 0 )[GRN] = 0;
			Dst( dst_x, 0 )[BLU] = 0;
			}
#endif

		/* Compute coords. of source rectangle and destination pixel. */

		dst_y = 0;
    ccyloop:
		if ( dst_y >= dst_height )
			goto done;	/* that's all folks */

		bot_y = dst_y / y_scale + EPSILON;

		if ( (top_y = (dst_y + 1) / y_scale + EPSILON) > src_height )
			top_y = src_height;

		if ( top_y <= bot_y )
			{
			/* End of image. */

			/* Clear beginning of output scan line buffer. */

			dst_x = src_width * y_scale + EPSILON;

			if ( dst_x < dst_width )
				++dst_x;	/* sometimes needed */

			while ( --dst_x >= 0 )
				{
				assert(dst_x < dst_width);
				Dst( dst_x, 0 )[RED] = 0;
				Dst( dst_x, 0 )[GRN] = 0;
				Dst( dst_x, 0 )[BLU] = 0;
				}

			/* Clear out top margin. */

			for ( ; dst_y < dst_height; ++dst_y )
				if ( fb_write( dst_fbp, 0, dst_y,
					       (unsigned char *)Dst( 0, 0 ),
					       dst_width
					     ) == -1
				   )
					Fatal( "Error writing top margin" );

			goto done;	/* that's all folks */
			}

		assert(0 <= bot_y && bot_y < top_y && top_y <= src_height);
		assert(0 <= dst_y && dst_y <= bot_y);
		assert(top_y - bot_y <= (int)(1 / y_scale + 1 - EPSILON));

		/* Fill input scan line buffer. */

		for ( src_y = bot_y; src_y < top_y; ++src_y )
			if ( fb_read( src_fbp, 0, src_y,
				      (unsigned char *)Src( 0, src_y - bot_y ),
				      src_width
				    ) == -1
			   )
				Fatal( "Error reading scan line" );

		dst_x = 0;
    ccxloop:
		if ( dst_x >= dst_width )
			goto ccflush;

		bot_x = dst_x / x_scale + EPSILON;

		if ( (top_x = (dst_x + 1) / x_scale + EPSILON) > src_width )
			top_x = src_width;

		if ( top_x <= bot_x )
			{
    ccflush:		/* End of band; flush buffer. */

			if ( fb_write( dst_fbp, 0, dst_y,
				       (unsigned char *)Dst( 0, 0 ),
				       dst_width
				     ) == -1
			   )
				Fatal( "Error writing scan line" );

			++dst_y;
			goto ccyloop;
			}

		assert(0 <= bot_x && bot_x < top_x && top_x <= src_width);
		assert(0 <= dst_x && dst_x <= bot_x);
		assert(top_x - bot_x <= (int)(1 / x_scale + 1 - EPSILON));

		/* Copy sample or averaged source pixel(s) to destination. */

		if ( sample )
			{
			Dst( dst_x, 0 )[RED] = Src( bot_x, 0 )[RED];
			Dst( dst_x, 0 )[GRN] = Src( bot_x, 0 )[GRN];
			Dst( dst_x, 0 )[BLU] = Src( bot_x, 0 )[BLU];
			}
		else	{
			int		sum[3];	/* pixel value accumulator */
			register float	tally;	/* # of pixels accumulated */

			/* "Read in" source rectangle and average pixels. */

			sum[RED] = sum[GRN] = sum[BLU] = 0;

			for ( src_y = top_y - bot_y; --src_y >= 0; )
				for ( src_x = bot_x; src_x < top_x; ++src_x )
					{
					sum[RED] += Src( src_x, src_y )[RED];
					sum[GRN] += Src( src_x, src_y )[GRN];
					sum[BLU] += Src( src_x, src_y )[BLU];
					}

			tally = (top_x - bot_x) * (top_y - bot_y);
			assert(tally > 0.0);
			Dst( dst_x, 0 )[RED] = sum[RED] / tally + 0.5;
			Dst( dst_x, 0 )[GRN] = sum[GRN] / tally + 0.5;
			Dst( dst_x, 0 )[BLU] = sum[BLU] / tally + 0.5;
			}

		++dst_x;
		goto ccxloop;
		}
	else if ( x_compress && !y_compress )
		{
		register int	src_x, src_y;	/* source rect. pixel coords. */
		register int	dst_x, dst_y;	/* dest. rect. pixel coords. */
		register int	bot_x, top_x;	/* source rectangle bounds */
		register int	bot_y, top_y;	/* destination rect. bounds */

#if 0	/* (already done by calloc()!) */
		/* Clear end of output scan line buffer to background. */

		for ( dst_x = src_width * y_scale + EPSILON;
		      dst_x < dst_width;
		      ++dst_x
		    )
			for ( dst_y = (int)(y_scale + 1 - EPSILON);
			      --dst_y >= 0;
			    )	{
				assert(dst_x >= 0 && dst_y < dst_height);
				Dst( dst_x, dst_y )[RED] = 0;
				Dst( dst_x, dst_y )[GRN] = 0;
				Dst( dst_x, dst_y )[BLU] = 0;
				}
#endif

		/* Compute coords. of source and destination rectangles. */

		src_y = (dst_height - 1) / y_scale + EPSILON;
    ceyloop:
		if ( src_y < 0 )
			goto done;	/* that's all folks */

		bot_y = src_y * y_scale + EPSILON;

		if ( (top_y = (src_y + 1) * y_scale + EPSILON) > dst_height )
			top_y = dst_height;

		assert(0 <= src_y && src_y <= bot_y && src_y < src_height);
		assert(bot_y < top_y && top_y <= dst_height);
		assert(top_y - bot_y <= (int)(y_scale + 1 - EPSILON));

		/* Fill input scan line buffer. */

		if ( fb_read( src_fbp, 0, src_y, (unsigned char *)Src( 0, 0 ),
			      src_width
			    ) == -1
		   )
			Fatal( "Error reading scan line" );

		dst_x = 0;
    cexloop:
		if ( dst_x >= dst_width )
			goto ceflush;

		bot_x = dst_x / x_scale + EPSILON;

		if ( (top_x = (dst_x + 1) / x_scale + EPSILON) > src_width )
			top_x = src_width;

		if ( top_x <= bot_x )
			{
    ceflush:		/* End of band; flush buffer. */

			for ( dst_y = top_y; --dst_y >= bot_y; )
				if ( fb_write( dst_fbp, 0, dst_y,
					       (unsigned char *)Dst( 0, dst_y - bot_y
							      ),
					       dst_width
					     ) == -1
				   )
					Fatal( "Error writing scan line" );

			--src_y;
			goto ceyloop;
			}

		assert(0 <= bot_x && bot_x < top_x && top_x <= src_width);
		assert(0 <= dst_x && dst_x <= bot_x);
		assert(top_x - bot_x <= (int)(1 / x_scale + 1 - EPSILON));

		/* Replicate sample or averaged source pixel(s) to dest. */

		if ( sample )
			for ( dst_y = top_y - bot_y; --dst_y >= 0; )
				{
				Dst( dst_x, dst_y )[RED] = Src( bot_x, 0 )[RED];
				Dst( dst_x, dst_y )[GRN] = Src( bot_x, 0 )[GRN];
				Dst( dst_x, dst_y )[BLU] = Src( bot_x, 0 )[BLU];
				}
		else	{
			int		sum[3];	/* pixel value accumulator */
			register float	tally;	/* # of pixels accumulated */

			/* "Read in" source rectangle and average pixels. */

			sum[RED] = sum[GRN] = sum[BLU] = 0;

			for ( src_x = bot_x; src_x < top_x; ++src_x )
				{
				sum[RED] += Src( src_x, 0 )[RED];
				sum[GRN] += Src( src_x, 0 )[GRN];
				sum[BLU] += Src( src_x, 0 )[BLU];
				}

			tally = top_x - bot_x;
			assert(tally > 0.0);
			sum[RED] = sum[RED] / tally + 0.5;
			sum[GRN] = sum[GRN] / tally + 0.5;
			sum[BLU] = sum[BLU] / tally + 0.5;

			for ( dst_y = top_y - bot_y; --dst_y >= 0; )
				{
				Dst( dst_x, dst_y )[RED] = sum[RED];
				Dst( dst_x, dst_y )[GRN] = sum[GRN];
				Dst( dst_x, dst_y )[BLU] = sum[BLU];
				}
			}

		++dst_x;
		goto cexloop;
		}
	else if ( !x_compress && y_compress )
		{
		register int	src_x, src_y;	/* source rect. pixel coords. */
		register int	dst_x, dst_y;	/* dest. rect. pixel coords. */
		register int	bot_x, top_x;	/* destination rect. bounds */
		register int	bot_y, top_y;	/* source rectangle bounds */

		assert(dst_width >= src_width);	/* (thus no right margin) */

		/* Compute coords. of source and destination rectangles. */

		dst_y = 0;
    ecyloop:
		if ( dst_y >= dst_height )
			goto done;	/* that's all folks */

		bot_y = dst_y / y_scale + EPSILON;

		if ( (top_y = (dst_y + 1) / y_scale + EPSILON) > src_height )
			top_y = src_height;

		if ( top_y <= bot_y )
			{
			/* End of image. */

			/* Clear output scan line buffer. */

			for ( dst_x = dst_width; --dst_x >= 0; )
				{
				Dst( dst_x, 0 )[RED] = 0;
				Dst( dst_x, 0 )[GRN] = 0;
				Dst( dst_x, 0 )[BLU] = 0;
				}

			/* Clear out top margin. */

			for ( ; dst_y < dst_height; ++dst_y )
				if ( fb_write( dst_fbp, 0, dst_y,
					       (unsigned char *)Dst( 0, 0 ),
					       dst_width
					     ) == -1
				   )
					Fatal( "Error writing top margin" );

			goto done;	/* that's all folks */
			}

		assert(0 <= bot_y && bot_y < top_y && top_y <= src_height);
		assert(0 <= dst_y && dst_y <= bot_y);
		assert(top_y - bot_y <= (int)(1 / y_scale + 1 - EPSILON));

		/* Fill input scan line buffer. */

		for ( src_y = bot_y; src_y < top_y; ++src_y )
			if ( fb_read( src_fbp, 0, src_y,
				      (unsigned char *)Src( 0, src_y - bot_y ),
				      src_width
				    ) == -1
			   )
				Fatal( "Error reading scan line" );

		src_x = (dst_width - 1) / x_scale + EPSILON;
    ecxloop:
		if ( src_x < 0 )
			{
			/* End of band; flush buffer. */
			if ( fb_write( dst_fbp, 0, dst_y,
				       (unsigned char *)Dst( 0, 0 ),
				       dst_width
				     ) == -1
			   )
				Fatal( "Error writing scan line" );

			++dst_y;
			goto ecyloop;
			}

		bot_x = src_x * x_scale + EPSILON;

		if ( (top_x = (src_x + 1) * x_scale + EPSILON) > dst_width )
			top_x = dst_width;

		assert(0 <= src_x && src_x <= bot_x && src_x <= src_width);
		assert(bot_x < top_x && top_x <= dst_width);
		assert(top_x - bot_x <= (int)(x_scale + 1 - EPSILON));

		/* Replicate sample or averaged source pixel(s) to dest. */

		if ( sample )
			for ( dst_x = top_x; --dst_x >= bot_x; )
				{
				Dst( dst_x, 0 )[RED] = Src( src_x, 0 )[RED];
				Dst( dst_x, 0 )[GRN] = Src( src_x, 0 )[GRN];
				Dst( dst_x, 0 )[BLU] = Src( src_x, 0 )[BLU];
				}
		else	{
			int		sum[3];	/* pixel value accumulator */
			register float	tally;	/* # of pixels accumulated */

			/* "Read in" source rectangle and average pixels. */

			sum[RED] = sum[GRN] = sum[BLU] = 0;

			for ( src_y = top_y - bot_y; --src_y >= 0; )
				{
				sum[RED] += Src( src_x, src_y )[RED];
				sum[GRN] += Src( src_x, src_y )[GRN];
				sum[BLU] += Src( src_x, src_y )[BLU];
				}

			tally = top_y - bot_y;
			assert(tally > 0.0);
			sum[RED] = sum[RED] / tally + 0.5;
			sum[GRN] = sum[GRN] / tally + 0.5;
			sum[BLU] = sum[BLU] / tally + 0.5;

			for ( dst_x = top_x; --dst_x >= bot_x; )
				{
				Dst( dst_x, 0 )[RED] = sum[RED];
				Dst( dst_x, 0 )[GRN] = sum[GRN];
				Dst( dst_x, 0 )[BLU] = sum[BLU];
				}
			}

		--src_x;
		goto ecxloop;
		}
	else if ( !x_compress && !y_compress )
		{
		register int	src_x, src_y;	/* source pixel coords. */
		register int	dst_x, dst_y;	/* dest. rect. pixel coords. */
		register int	bot_x, bot_y;	/* dest. rect. lower bounds */
		register int	top_x, top_y;	/* dest. rect. upper bounds */

		assert(dst_width >= src_width);	/* (thus no right margin) */

		/* Compute coords. of source and destination rectangles. */

		src_y = (dst_height - 1) / y_scale + EPSILON;
    eeyloop:
		if ( src_y < 0 )
			goto done;	/* that's all folks */

		bot_y = src_y * y_scale + EPSILON;

		if ( (top_y = (src_y + 1) * y_scale + EPSILON) > dst_height )
			top_y = dst_height;

		assert(0 <= src_y && src_y <= bot_y && src_y < src_height);
		assert(bot_y < top_y && top_y <= dst_height);
		assert(top_y - bot_y <= (int)(y_scale + 1 - EPSILON));

		/* Fill input scan line buffer. */
		if ( fb_read( src_fbp, 0, src_y, (unsigned char *)Src( 0, 0 ),
			      src_width
			    ) == -1
		   )
			Fatal( "Error reading scan line" );

		src_x = (dst_width - 1) / x_scale + EPSILON;
    eexloop:
		if ( src_x < 0 )
			{
			/* End of band; flush buffer. */

			for ( dst_y = top_y; --dst_y >= bot_y; )
				if ( fb_write( dst_fbp, 0, dst_y,
					       (unsigned char *)Dst( 0, dst_y - bot_y
							      ),
					       dst_width
					     ) == -1
				   )
					Fatal( "Error writing scan line" );

			--src_y;
			goto eeyloop;
			}

		bot_x = src_x * x_scale + EPSILON;

		if ( (top_x = (src_x + 1) * x_scale + EPSILON) > dst_width )
			top_x = dst_width;

		assert(0 <= src_x && src_x <= bot_x && src_x <= src_width);
		assert(bot_x < top_x && top_x <= dst_width);
		assert(top_x - bot_x <= (int)(x_scale + 1 - EPSILON));

		/* Replicate sample source pixel to destination. */

		for ( dst_y = top_y - bot_y; --dst_y >= 0; )
			for ( dst_x = top_x; --dst_x >= bot_x; )
				{
				Dst( dst_x, dst_y )[RED] = Src( src_x, 0 )[RED];
				Dst( dst_x, dst_y )[GRN] = Src( src_x, 0 )[GRN];
				Dst( dst_x, dst_y )[BLU] = Src( src_x, 0 )[BLU];
				}

		--src_x;
		goto eexloop;
		}

    done:
	/* Close the frame buffers. */

	assert(src_fbp != FBIO_NULL && dst_fbp != FBIO_NULL);

	if ( fb_close( src_fbp ) == -1 )
		Message( "Error closing input frame buffer" );

	if ( dst_fbp != src_fbp && fb_close( dst_fbp ) == -1 )
		Message( "Error closing output frame buffer" );

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
