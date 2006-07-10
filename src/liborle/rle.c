/*                           R L E . C
 * BRL-CAD
 *
 * Copyright (c) 1883-2006 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file rle.c
 *
 *  These routines are appropriate for RLE encoding a framebuffer image
 *  from a program.
 *  This library is derived from 'ik-rle' code authored by :
 *	Mike Muuss, BRL.  10/18/83.
 *	[With a heavy debt to Spencer Thomas, U. of Utah,
 *	 for the RLE file format].
 *
 *  Author -
 *	Gary S. Moss
 *
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 *
 */
#ifndef lint
static const char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "common.h"

#include <stdio.h>

#include "machine.h"
#include "fb.h"
#include "orle.h"

#define PRNT_A1_DEBUG(_op,_n) \
	if(rle_debug) (void)fprintf(stderr,"%s(%d)\n",_op,_n)
#define PRNT_A2_DEBUG(_op,_n,_c) \
	if(rle_debug) (void)fprintf(stderr,"%s(%ld,%d)\n",_op,(long)(_n),_c)
#define CUR	RED		/* Must be rightmost part of Pixel.		*/

/*	States for run detection					*/
#define	DATA	0
#define	RUN2	1
#define	INRUN	2
#define UPPER 255			/* anything bigger ain't a byte */

/* An addition to the STDIO suite, to avoid using fwrite() for
	writing 2 bytes of data.
 */
#define putshort(s) \
	{register short v = s; \
	(void) putc( v & 0xFF, fp ); (void) putc( (v>>8) & 0xFF, fp );}

/* short instructions */
#define mk_short_1(oper,a1)		/* one argument short */ \
	{(void) putc( oper, fp ); (void) putc( a1, fp );}

#define mk_short_2(oper,a1,a2)		/* two argument short */ \
	{(void) putc( oper, fp ); (void) putc( a1, fp ); putshort( a2 )}

/* long instructions */
#define mk_long_1(oper,a1)		/* one argument long */ \
	{(void) putc( LONG|oper, fp ); (void) putc( 0, fp ); putshort( a1 )}

#define mk_long_2(oper,a1,a2)		/* two argument long */ \
	{(void) putc( LONG|oper, fp ); (void) putc( 0, fp ); putshort( a1 )\
		putshort( a2 )}

/* Choose between long and short format instructions.			*/
#define mk_inst_1(oper,a1)    /* one argument inst */ \
	{if( a1 > UPPER ) mk_long_1(oper,a1) else mk_short_1(oper,a1)}

#define mk_inst_2(oper,a1,a2) /* two argument inst */ \
	{if( a1 > UPPER ) mk_long_2(oper,a1,a2)	else mk_short_2(oper,a1,a2)}

/* Skip one or more blank lines in the RLE file.			*/
#define SkipBlankLines(nblank)	RSkipLines(nblank)

/* Select a color and do "carriage return" to start of scanline.
	color: 0 = Red, 1 = Green, 2 = Blue.
 */
#define SetColor(c)		RSetColor( _bw_flag ? 0 : c )

/* Skip a run of background.						*/
#define SkipPixels(nskip)	if( (nskip) > 0 ) RSkipPixels(nskip)

/* Output an enumerated set of intensities for current color channel.	*/
#define PutRun(color, num)	RRunData(num-1,color)

/* Opcode definitions.							*/
#define	RSkipLines(_n) \
	{PRNT_A1_DEBUG("Skip-Lines",_n); mk_inst_1(RSkipLinesOp,(_n))}

#define	RSetColor(_c) \
	{PRNT_A1_DEBUG("Set-Color",_c); mk_short_1(RSetColorOp,(_c))}

#define	RSkipPixels(_n) \
	{PRNT_A1_DEBUG("Skip-Pixels",_n); mk_inst_1(RSkipPixelsOp,(_n))}

#define	RByteData(_n) \
	{PRNT_A1_DEBUG("Byte-Data",_n); mk_inst_1(RByteDataOp,_n)}

#define	RRunData(_n,_c) \
	{PRNT_A2_DEBUG("Run-Data",_n,_c); mk_inst_2(RRunDataOp,(_n),(_c))}

#define NSEG	1024/3		/* Number of run segments of storage */
static struct runs
	{
	RGBpixel *first;
	RGBpixel *last;
	} runs[NSEG];		/* ptrs to non-background run segs */

/* Global data.								*/
int	_bg_flag;
int	_bw_flag;
int	_cm_flag;
RGBpixel	_bg_pixel;

int	_ncmap = 3;	/* Default : (3) channels in color map.		*/
int	_cmaplen = 8;	/* Default : (8) log base 2 entries in map.	*/
int	_pixelbits = 8;	/* Default : (8) bits per pixel.		*/
int	rle_debug = 0;
int	rle_verbose = 0;

#undef HIDDEN
#define HIDDEN static
HIDDEN void	_put_Data(register FILE *fp, register unsigned char *cp, int n);
HIDDEN int	_put_Color_Map_Seg(FILE *fp, register short unsigned int *cmap_seg);
HIDDEN int	_put_Std_Map(FILE *fp);
HIDDEN int	_get_Color_Map_Seg(FILE *fp, register short unsigned int *cmap_seg);

/* Functions to read instructions, depending on format.			*/
HIDDEN int	(*_func_Get_Inst)();	/* Ptr to appropriate function.	*/
HIDDEN int	_get_Old_Inst(register FILE *fp, register int *op, register int *dat);	/* Old format inst. reader.	*/
HIDDEN int	_get_New_Inst(register FILE *fp, register int *opcode, register int *datum);	/* New extended inst. reader.	*/
HIDDEN void	_enc_Color_Seg(FILE *fp, register int seg, register int color);
HIDDEN void	_enc_Segment(FILE *fp, register RGBpixel (*data_p), register RGBpixel (*last_p));
HIDDEN int	_bg_Get_Runs(register RGBpixel (*pixelp), register RGBpixel (*endpix));

void		prnt_XSetup(char *msg, register Xtnd_Rle_Header *setup);

static Xtnd_Rle_Header	w_setup;	/* Header being written out.	*/
static Xtnd_Rle_Header	r_setup;	/* Header being read in.	*/

void
rle_rlen(int *xlen, int *ylen)
{
	*xlen = r_setup.h_xlen;
	*ylen = r_setup.h_ylen;
	}

void
rle_wlen(int xlen, int ylen, int mode)
{
	if( mode == 0 )		/* Read mode.				*/
		{
		r_setup.h_xlen = xlen;
		r_setup.h_ylen = ylen;
		}
	else			/* Write mode.				*/
		{
		w_setup.h_xlen = xlen;
		w_setup.h_ylen = ylen;
		}
	return;
	}

void
rle_rpos(int *xpos, int *ypos)
{
	*xpos = r_setup.h_xpos;
	*ypos = r_setup.h_ypos;
	}

void
rle_wpos(int xpos, int ypos, int mode)
{
	if( mode == 0 )		/* Read mode.				*/
		{
		r_setup.h_xpos = xpos;
		r_setup.h_ypos = ypos;
		}
	else			/* Write mode.				*/
		{
		w_setup.h_xpos = xpos;
		w_setup.h_ypos = ypos;
		}
	return;
	}

/*	r l e _ r h d r ( )
	This routine should be called before 'rle_decode_ln()' or 'rle_rmap()'
	to position the file pointer correctily and set up the global flags
	_bw_flag and _cm_flag, and to fill in _bg_pixel if necessary, and
	to pass information back to the caller in flags and bgpixel.

	Returns -1 for failure, 0 otherwise.
 */
int
rle_rhdr(FILE *fp, int *flags, register unsigned char *bgpixel)
{
	static short	x_magic;
	static char	*verbage[] =
		{
		"Frame buffer image saved in Old Run Length Encoded form\n",
		"Frame buffer image saved in Old B&W RLE form\n",
		"File not in RLE format, can't display (magic=0x%x)\n",
		"Saved with background color %d %d %d\n",
		"Saved in overlay mode\n",
		"Saved as a straight box image\n",
		NULL
		};

	if( fp != stdin && fseek( fp, 0L, 0 ) == -1 )
		{
		(void) fprintf( stderr, "Seek to RLE header failed!\n" );
		return	-1;
		}
	if( fread( (char *)&x_magic, sizeof(short), 1, fp ) != 1 )
		{
		(void) fprintf( stderr, "Read of magic word failed!\n" );
		return	-1;
		}
	SWAB( x_magic );
	if( x_magic != XtndRMAGIC )
		{ Old_Rle_Header	setup;
		if( fread( (char *) &setup, sizeof(setup), 1, fp ) != 1 )
			{
			(void) fprintf( stderr,
					"Read of Old RLE header failed!\n"
					);
			return	-1;
			}
		SWAB( setup.xpos );
		SWAB( setup.ypos );
		SWAB( setup.xsize );
		SWAB( setup.ysize );
		r_setup.h_xpos = setup.xpos;
		r_setup.h_ypos = setup.ypos;
		r_setup.h_xlen = setup.xsize;
		r_setup.h_ylen = setup.ysize;
		switch( x_magic & ~0xff) {
		case RMAGIC :
			if( rle_verbose )
				(void) fprintf( stderr,	verbage[0] );
			r_setup.h_ncolors = 3;
			break;
		case WMAGIC :
			if( rle_verbose )
				(void) fprintf( stderr, verbage[1] );
			r_setup.h_ncolors = 1;
			break;
		default:
			(void) fprintf(	stderr, verbage[2], x_magic & ~0xff);
			return	-1;
		} /* End switch */
		switch( x_magic & 0xFF ) {
		case 'B' : /* Background given.				*/
			r_setup.h_flags = H_CLEARFIRST;
			r_setup.h_background[0] = setup.bg_r;
			r_setup.h_background[1] = setup.bg_g;
			r_setup.h_background[2] = setup.bg_b;
			break;
		default: /* Straight 'box' save.			*/
			r_setup.h_flags = H_BOXSAVE;
			r_setup.h_background[0] = 0;
			r_setup.h_background[1] = 0;
			r_setup.h_background[2] = 0;
			if( rle_verbose )
				(void) fprintf( stderr, verbage[5] );
			break;
		} /* End switch */
		r_setup.h_pixelbits = 8;
		r_setup.h_ncmap = setup.map ? 3 : 0;
		r_setup.h_cmaplen = 8;
		_func_Get_Inst = _get_Old_Inst;
		} /* End if */
	else
		{
		if( fread( (char *)&r_setup, sizeof(Xtnd_Rle_Header), 1, fp )
		    != 1
			)
			{
			(void) fprintf( stderr,
					"Read of RLE header failed!\n"
					);
			return	-1;
			}
		SWAB( r_setup.h_xpos );
		SWAB( r_setup.h_ypos );
		SWAB( r_setup.h_xlen );
		SWAB( r_setup.h_ylen );
		_func_Get_Inst = _get_New_Inst;
		}
	if( rle_verbose )
		(void) fprintf( stderr,
				"Positioned at (%d, %d), size (%d %d)\n",
				r_setup.h_xpos,
				r_setup.h_ypos,
				r_setup.h_xlen,
				r_setup.h_ylen
				);
	if( r_setup.h_flags == H_CLEARFIRST )
		{
		if( rle_verbose )
			(void) fprintf( stderr,
					verbage[3],
					r_setup.h_background[0],
					r_setup.h_background[1],
					r_setup.h_background[2]
					);
		if( bgpixel != RGBPIXEL_NULL )
			{
			/* No command-line backgr., use saved values.	*/
			_bg_pixel[RED] = r_setup.h_background[0];
			_bg_pixel[GRN] = r_setup.h_background[1];
			_bg_pixel[BLU] = r_setup.h_background[2];
			COPYRGB( bgpixel, _bg_pixel );
			}
		}
	_bw_flag = r_setup.h_ncolors == 1;
	if( r_setup.h_flags & H_CLEARFIRST )
		*flags = NO_BOX_SAVE;
	else
		*flags = 0;
	if( r_setup.h_ncmap == 0 )
		*flags |= NO_COLORMAP;
	if( r_setup.h_ncolors == 0 )
		*flags |= NO_IMAGE;
	if( rle_debug )
		{
		(void) fprintf( stderr, "Magic=0x%x\n", x_magic );
		prnt_XSetup( "Setup structure read", &r_setup );
		}
	return	0;
	}

/*	r l e _ w h d r ( )
 	This routine should be called after 'setfbsize()', unless the
	framebuffer image is the default size (512).
	This routine should be called before 'rle_encode_ln()' to set up
	the global data: _bg_flag, _bw_flag, _cm_flag, and _bg_pixel.
	Returns -1 for failure, 0 otherwise.
 */
int
rle_whdr(FILE *fp, int ncolors, int bgflag, int cmflag, unsigned char *bgpixel)
{
	/* Magic numbers for output file.				*/
	static short	x_magic = XtndRMAGIC; /* Extended magic number.	*/
	static RGBpixel	black = { 0, 0, 0 };

	/* safty check */
	if( bgpixel == NULL )
		bgpixel = black;

	/* If black and white mode, compute NTSC value of background.	*/
	if( ncolors == 1 )
		{
		register int	bbw;
		if( rle_verbose )
			(void) fprintf( stderr,
					"Image being saved as monochrome.\n"
					);
		bbw = 0.35 * bgpixel[RED] +
			0.55 * bgpixel[GRN] +
			0.10 * bgpixel[BLU];
		w_setup.h_background[0] = bbw;
		w_setup.h_background[1] = bbw;
		w_setup.h_background[2] = bbw;
		} else {
		w_setup.h_background[0] = bgpixel[RED];
		w_setup.h_background[1] = bgpixel[GRN];
		w_setup.h_background[2] = bgpixel[BLU];
		}
	w_setup.h_flags = bgflag ? H_CLEARFIRST : 0;
	w_setup.h_ncolors = ncolors;
	w_setup.h_pixelbits = _pixelbits;
	w_setup.h_ncmap = cmflag ? _ncmap : 0;
	w_setup.h_cmaplen = _cmaplen;

	if( fp != stdout && fseek( fp, 0L, 0 ) == -1 )
		{
		(void) fprintf( stderr, "Seek to RLE header failed!\n" );
		return	-1;
		}
	SWAB( x_magic );
	if( fwrite( (char *) &x_magic, sizeof(short), 1, fp ) != 1 )
		{
		(void) fprintf( stderr, "Write of magic number failed!\n" );
		return	-1;
		}
	SWAB( w_setup.h_xpos );
	SWAB( w_setup.h_ypos );
	SWAB( w_setup.h_xlen );
	SWAB( w_setup.h_ylen );
	if( fwrite( (char *) &w_setup, sizeof w_setup, 1, fp ) != 1 )
		{
		(void) fprintf( stderr, "Write of RLE header failed!\n" );
		return	-1;
		}
	SWAB( w_setup.h_xpos );
	SWAB( w_setup.h_ypos );
	SWAB( w_setup.h_xlen );
	SWAB( w_setup.h_ylen );
	if( rle_debug )
		{
		(void) fprintf( stderr, "Magic=0x%x\n", x_magic );
		prnt_XSetup( "Setup structure written", &w_setup );
		}
	_bg_flag = bgflag;
	_bw_flag = ncolors == 1;
	_cm_flag = cmflag;
	COPYRGB(_bg_pixel, bgpixel);
	return	0;
	}

/*	r l e _ r m a p ( )
	Read a color map in RLE format.
	Returns -1 upon failure, 0 otherwise.
 */
int
rle_rmap(FILE *fp, ColorMap *cmap)
{
	if( rle_verbose )
		(void) fprintf( stderr, "Reading color map\n");
	if(	_get_Color_Map_Seg( fp, cmap->cm_red ) == -1
	     ||	_get_Color_Map_Seg( fp, cmap->cm_green ) == -1
	     ||	_get_Color_Map_Seg( fp, cmap->cm_blue ) == -1
		)
		return	-1;
	else
		return	0;
	}

/*	r l e _ w m a p ( )
	Write a color map in RLE format.
	Returns -1 upon failure, 0 otherwise.
 */
int
rle_wmap(FILE *fp, ColorMap *cmap)
{
	if( w_setup.h_ncmap == 0 )
		{
		(void) fprintf( stderr,
		"Writing color map conflicts with header information!\n"
				);
		(void) fprintf( stderr,
				"rle_whdr(arg 2 == 0) No map written.\n"
				);
		return	-1;
		}
	if( rle_verbose )
		(void) fprintf( stderr, "Writing color map\n" );
	if( cmap == (ColorMap *) NULL )
		{
		return _put_Std_Map( fp );
		}
	if(	_put_Color_Map_Seg( fp, cmap->cm_red ) == -1
	     ||	_put_Color_Map_Seg( fp, cmap->cm_green ) == -1
	     ||	_put_Color_Map_Seg( fp, cmap->cm_blue ) == -1
		)
		return	-1;
	else
		return	0;
	}

/*	r l e _ d e c o d e _ l n ( )
	Decode one scanline into 'scan_buf'.
	Buffer is assumed to be filled with background color.
	Returns -1 on failure, 1 if buffer is altered
	and 0 if untouched.
 */
int
rle_decode_ln(register FILE *fp, RGBpixel (*scan_buf))
{	static int	lines_to_skip = 0;
		static int	opcode, datum;
		static short	word;

		register int	n;
		register unsigned char	*pp;
		register int	dirty_flag = 0;

	if( lines_to_skip > 0 )
		{
		lines_to_skip--;
		return	dirty_flag;
		}
	pp = &(scan_buf[r_setup.h_xpos][RED]); /* Pointer into pixel. */
	while( (*_func_Get_Inst)( fp, &opcode, &datum ) != EOF )
		{
		switch( opcode )
			{
		case RSkipLinesOp :
			lines_to_skip = datum;
			PRNT_A1_DEBUG( "Skip-Lines", lines_to_skip );
			if( lines_to_skip-- < 1 )
				return	-1;
			return	dirty_flag;
		case RSetColorOp:
			/* Select "color channel" that following ops go to.
				Set `pp' to point to starting pixel element;
		 		by adding STRIDE to pp, will move to
				corresponding color element in next pixel.
				If Black & White image:  point to left-most
				byte (Red for Ikonas) in long, and Run and
				Data will ignore strides below.
		 	*/
			PRNT_A1_DEBUG( "Set-Color", datum );
			if( (n = _bw_flag ? 0 : datum) > 2 )
				{
				(void) fprintf( stderr,	"Bad color %d\n", n );
				if( ! rle_debug )
					return	-1;
				}
			pp = &(scan_buf[r_setup.h_xpos][n]);
			break;
		case RSkipPixelsOp: /* advance pixel ptr */
			n = datum;
			PRNT_A1_DEBUG( "Skip-Pixels", n );
			pp += n * STRIDE;
			break;
		case RByteDataOp:
			n = datum + 1;
			PRNT_A1_DEBUG( "Byte-Data", n );
			if( ! _bw_flag )
				{
				while( n-- > 0 )
					{
					*pp = getc(fp);
					pp += STRIDE;
					}
				}
			else
				{ /* Ugh, black & white.		*/
				register unsigned char c;
				while( n-- > 0 )
					{
					/* Implicit knowledge of sizeof(RGBpixel) */
					*pp++ = c = getc( fp );
					*pp++ = c;
					*pp++ = c;
					}
				}
			if( (datum + 1) & 1 )
				{ /* word align file ptr		*/
				(void) getc( fp );
				}
			dirty_flag = 1;
			break;
		case RRunDataOp:
			n = datum + 1;
			{ register char *p = (char *) &word;
			*p++ = getc( fp );
			*p++ = getc( fp );
			SWAB( word );
			}
			PRNT_A2_DEBUG( "Run-Data", (long)n,	word );
			if( ! _bw_flag )
				{
				register unsigned char inten = (unsigned char)word;
				while( n-- > 0 )
					{
					*pp = inten;
					pp += STRIDE;
					}
				}
			else
				{ /* Ugh, black & white.		*/
				while( n-- > 0 )
					{
					/* Implicit knowledge of sizeof(RGBpixel) */
					*pp++ = (unsigned char) word;
					*pp++ = (unsigned char) word;
					*pp++ = (unsigned char) word;
					}
				}
			dirty_flag = 1;
			break;
		default:
			(void) fprintf( stderr,
					"Unrecognized opcode: %d (x%x x%x)\n",
					opcode, opcode, datum
					);
			if( ! rle_debug )
				return	-1;
			}
		}
	return	dirty_flag;
	}

/* 	r l e _ e n c o d e _ l n ( )
	Encode a given scanline of pixels into RLE format.
	Returns -1 upon failure, 0 otherwise.
 */
int
rle_encode_ln(register FILE *fp, RGBpixel (*scan_buf))
{	register RGBpixel *scan_p;
		register RGBpixel *last_p;
		register int	i;
		register int	color;		/* holds current color */
		register int	nseg;		/* number of non-bg run segments */

	scan_p = (RGBpixel *)&(scan_buf[w_setup.h_xpos][RED]);
	last_p = (RGBpixel *)&(scan_buf[w_setup.h_xpos+(w_setup.h_xlen-1)][RED]);
	if( _bg_flag )
		{
		if( (nseg = _bg_Get_Runs( scan_p, last_p )) == -1 )
			return	-1;
		if( nseg <= 0 )
			{
			RSkipLines( 1 );
			return	0;
			}
		}
	else
		{
		runs[0].first = scan_p;
		runs[0].last = last_p;
		nseg = 1;
		}
	if( _bw_flag )
		{	register RGBpixel *pixelp;
		/* Compute NTSC Black & White in blue row.		*/
		for( pixelp=scan_p; pixelp <= last_p; pixelp++ )
			(*pixelp)[BLU] =  .35 * (*pixelp)[RED] +
					.55 * (*pixelp)[GRN] +
					.10 * (*pixelp)[BLU];
		}

	/* do all 3 colors */
	for( color = 0; color < 3; color++ )
		{
		if( _bw_flag && color != 2 )
			continue;
		SetColor( color );
		if( runs[0].first != scan_p )
			{	register int	runlen = runs[0].first-scan_p;
			SkipPixels( runlen );
			}
		for( i = 0; i < nseg; i++ )
			{	register int	runlen = (runs[i+1].first-1) -
							 runs[i].last;
			_enc_Color_Seg( fp, i, color );
			/* Move along to next segment for encoding,
				if this was not the last segment.
			 */
			if( i < nseg-1 )
				SkipPixels( runlen );
			}
		}
	RSkipLines(1);
	return	0;
	}

/*	_ b g _ G e t _ R u n s ( )
	Fill the 'runs' segment array from 'pixelp' to 'endpix'.
	This routine will fail and return -1 if the array fills up
	before all pixels are processed, otherwise a 'nseg' is returned.
 */
HIDDEN int
_bg_Get_Runs(register RGBpixel (*pixelp), register RGBpixel (*endpix))
{	/* find non-background runs */
		register int	nseg = 0;
	while( pixelp <= endpix && nseg < NSEG )
		{
		if(	(*pixelp)[RED] != _bg_pixel[RED]
		    ||	(*pixelp)[GRN] != _bg_pixel[GRN]
		    ||	(*pixelp)[BLU] != _bg_pixel[BLU]
			)
			{
			/* We have found the start of a segment */
			runs[nseg].first = pixelp++;

			/* find the end of this run */
			while(	pixelp <= endpix
			    &&	 (	(*pixelp)[RED] != _bg_pixel[RED]
				   ||	(*pixelp)[GRN] != _bg_pixel[GRN]
				   ||	(*pixelp)[BLU] != _bg_pixel[BLU]
		    		 )
				)
				pixelp++;
			/* last pixel in segment */
			runs[nseg++].last = pixelp-1;
			}
		pixelp++;
		}
	if( nseg >= NSEG )
		{
		(void) fprintf( stderr,
		"Encoding incomplete, segment array 'runs[%d]' is full!\n",
				NSEG
				);
		return	-1;
		}
	return	nseg;
	}

/*	_ e n c _ C o l o r _ S e g ( )
	Encode a segment, 'seg', for specified 'color'.
 */
HIDDEN void
_enc_Color_Seg(FILE *fp, register int seg, register int color)
{	static RGBpixel *data_p;
		static RGBpixel *last_p;

	data_p = (RGBpixel *) &((*(runs[seg].first))[color]);
	last_p = (RGBpixel *) &((*(runs[seg].last))[color]);

	_enc_Segment( fp, data_p, last_p );
	return;
	}

/*	_ e n c _ S e g m e n t ( )
	Output code for segment.
 */
HIDDEN void
_enc_Segment(FILE *fp, register RGBpixel (*data_p), register RGBpixel (*last_p))
{	register RGBpixel	*pixelp;
		register RGBpixel	*runs_p = data_p;
		register int	state = DATA;
		register unsigned char	runval = (*data_p)[CUR];
	for( pixelp = data_p + 1; pixelp <= last_p; pixelp++ )
		{
		switch( state )
			{
		case DATA :
			if( runval == (*pixelp)[CUR] )
				/* 2 in a row, may be a run.		*/
				state = RUN2;
			else
				{
				/* Continue accumulating data, look for a
					new run starting here, too
				 */
				runval = (*pixelp)[CUR];
				runs_p = pixelp;
				}
			break;
		case RUN2:
			if( runval == (*pixelp)[CUR] )
				{
				/* 3 in a row is a run.			*/
				state = INRUN;
				/* Flush out data sequence encountered
					before this run
				 */
				if( runs_p > data_p )
					_put_Data(	fp,
							&((*data_p)[CUR]),
						 	runs_p-data_p
						 	);
				}
			else
				{ /* Not a run, but maybe this starts one. */
				state = DATA;
				runval = (*pixelp)[CUR];
				runs_p = pixelp;
				}
			break;
		case INRUN:
			if( runval != (*pixelp)[CUR] )
				{
				/* If run out				*/
				state = DATA;
				PutRun(	runval,	pixelp - runs_p );
				/* who knows, might be more */
				runval = (*pixelp)[CUR];
				runs_p = pixelp;
				/* starting new 'data' run */
				data_p = pixelp;
				}
			break;
			} /* end switch */
		} /* end for */
		/* Write out last portion of section being encoded.	*/
		if( state == INRUN )
			{
			PutRun( runval, pixelp - runs_p );
			}
		else
		if( pixelp > data_p )
			_put_Data( fp, &(*data_p)[CUR], pixelp - data_p );
	return;
	}

/*	_ p u t _ D a t a ( )
	Put one or more pixels of byte data into the output file.
 */
HIDDEN void
_put_Data(register FILE *fp, register unsigned char *cp, int n)
{	register int	count = n;
	RByteData(n-1);

#if 0
	/* More STDIO optimization, watch out...			*/
	if( fp->_cnt >= count )
		{ register unsigned char *op = (unsigned char *)fp->_ptr;
		fp->_cnt -= count;
		while( count-- > 0 )
			{
			*op++ = *cp;
			cp += STRIDE;
			}
#if defined(BSD) && ! defined(sun)
		fp->_ptr = (char *)op;
#else
		fp->_ptr = op;
#endif
		}
	else
#endif
	while( count-- > 0 )
		{
		(void) putc( (int) *cp, fp );
		cp += STRIDE;
		}
	if( n & 1 )
		(void) putc( 0, fp );	/* short align output */
	return;
	}

/*	_ g e t _ C o l o r _ M a p _ S e g ( )
	Read the color map stored in the RLE file.
	The RLE format stores color map entries as short integers
	RIGHT justified in the word, while libfb expects color
	maps to be LEFT justified within a short.
 */
HIDDEN int
_get_Color_Map_Seg(FILE *fp, register short unsigned int *cmap_seg)
{	static unsigned short	rle_cmap[256];
		register unsigned short	*cm = rle_cmap;
		register int	i;
	if( fread( (char *) rle_cmap, sizeof(rle_cmap), 1, fp ) != 1 )
		{
		(void) fprintf( stderr,	"Failed to read color map!\n" );
		return	-1;
		}
	for( i = 0; i < 256; i++, cm++ )
		{
		SWAB( *cm );
		*cmap_seg++ = (*cm) << 8;
		}
	return	0;
	}

/*	_ p u t _ C o l o r _ M a p _ S e g ( )
	Output color map segment to RLE file as shorts.  See above.
 */
HIDDEN int
_put_Color_Map_Seg(FILE *fp, register short unsigned int *cmap_seg)
{	static unsigned short	rle_cmap[256];
		register unsigned short	*cm = rle_cmap;
		register int	i;
	for( i = 0; i < 256; i++, cm++ )
		{
		*cm = *cmap_seg++ >> 8;
		SWAB( *cm );
		}
	if( fwrite( (char *) rle_cmap, sizeof(rle_cmap), 1, fp ) != 1 )
		{
		(void) fprintf(	stderr,
				"Write of color map segment failed!\n"
				);
		return	-1;
		}
	return	0;
	}

/*	_ p u t _ S t d _ M a p ( )
	Output standard color map to RLE file as shorts.
 */
HIDDEN int
_put_Std_Map(FILE *fp)
{	static unsigned short	rle_cmap[256*3];
		register unsigned short	*cm = rle_cmap;
		register int	i, segment;
	for( segment = 0; segment < 3; segment++ )
		for( i = 0; i < 256; i++, cm++ )
			{
			*cm = i;
			SWAB( *cm );
			}
	if( fwrite( (char *) rle_cmap, sizeof(rle_cmap), 1, fp ) != 1 )
		{
		(void) fprintf(	stderr,
				"Write of standard color map failed!\n"
				);
		return	-1;
		}
	return	0;
	}

HIDDEN int
_get_New_Inst(register FILE *fp, register int *opcode, register int *datum)
{
	static short	long_data;

	*opcode = getc( fp );
	*datum = getc( fp );
	if( *opcode & LONG )
		{ register char	*p = (char *) &long_data;
		*opcode &= ~LONG;
		*p++ = getc( fp );
		*p++ = getc( fp );
		SWAB( long_data );
		*datum = long_data;
		}
	if( feof( fp ) )
		return	EOF;
	return	1;
	}

HIDDEN int
_get_Old_Inst(register FILE *fp, register int *op, register int *dat)
{
	static Old_Inst	instruction;
	register char	*p;

	p = (char *) &instruction;

	*p++ = getc( fp );
	*p++ = getc( fp );
 	SWAB( *((short *)&instruction) );
	if( feof( fp ) )
		return	EOF;
#ifndef CRAY
	/* These only work on machines where sizeof(short) == 2 */
	*op = instruction.opcode;
	*dat = instruction.datum;
#endif
	return	1;
	}

void
prnt_XSetup(char *msg, register Xtnd_Rle_Header *setup)
{
	(void) fprintf( stderr, "%s : \n", msg );
	(void) fprintf( stderr,
			"\th_xpos=%d, h_ypos=%d\n\th_xlen=%d, h_ylen=%d\n",
			setup->h_xpos, setup->h_ypos,
			setup->h_xlen, setup->h_ylen
			);
	(void) fprintf( stderr,
			"\th_flags=0x%x\n\th_ncolors=%d\n\th_pixelbits=%d\n",
			setup->h_flags, setup->h_ncolors, setup->h_pixelbits
			);
	(void) fprintf( stderr,
			"\th_ncmap=%d\n\th_cmaplen=%d\n",
			setup->h_ncmap, setup->h_cmaplen
			);
	(void) fprintf( stderr,
			"\th_background=[%d %d %d]\n",
			setup->h_background[0],
			setup->h_background[1],
			setup->h_background[2]
			);
	return;
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
