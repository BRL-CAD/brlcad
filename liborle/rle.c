/*
	SCCS id:	@(#) librle.c	1.3
	Last edit: 	3/22/85 at 13:59:56	G S M
	Retrieved: 	8/13/86 at 10:27:09
	SCCS archive:	/m/cad/librle/RCS/s.librle.c

	Author : Gary S. Moss, BRL.

	These routines are appropriate for RLE encoding a framebuffer image
	from a program.

	This library is derived from 'ik-rle' code authored by :
		Mike Muuss, BRL.  10/18/83.
		[With a heavy debt to Spencer Thomas, U. of Utah,
		 for the RLE file format].
 */
#if ! defined( lint )
static
char	sccsTag[] = "@(#) librle.c	1.3	last edit 3/22/85 at 13:59:56";
#endif
#include <stdio.h>
#include <fb.h>
#include <rle.h>
extern int	debug, verbose;

#define CUR red		/* Must be rightmost part of Pixel.		*/
/* 
 * States for run detection
 */
#define	DATA	0
#define	RUN2	1
#define	INRUN	2

/* An addition to the STDIO suite, to avoid using fwrite() for
	writing 2 bytes of data.
 */
#define putshort(s) \
	{ \
	register int v = s; \
	(void) putc( v & 0xFF, fp ); \
	(void) putc( (v>>8) & 0xFF, fp ); \
	}

/* Skip one or more blank lines in the RLE file.			*/
#define SkipBlankLines(nblank) \
	{ \
	putshort(RSkipLines(nblank)); \
	if( debug ) (void) fprintf( stderr, "SkipLines %d\n", nblank ); \
	}

/* Select a color and do "carriage return" to start of scanline.
	color: 0 = Red, 1 = Green, 2 = Blue.
 */
#define SetColor(c) \
	{ \
	putshort(RSetColor((_bw_flag ? 0 : c))); \
	if(debug)  (void) fprintf( stderr, "SetColor %d\n", c); \
	}

/* Skip a run of background.						*/
#define SkipPixels(nskip) \
	{ \
	if( nskip > 0 ) \
		{ \
		putshort( RSkipPixels(nskip) ); \
		if(debug) (void) fprintf( stderr, "SkipPixels %d\n", nskip); \
		} \
	}

/* Perform a newline action.  Since CR is implied by the Set Color
	operation, this is only used to switch to a new scanline when the
	current one has been fully described.
 */
#define NewScanLine() \
	{ \
	putshort(RNewLine); \
	if(debug) (void) fprintf( stderr, "New Scan Line\n" ); \
	}

/* Output an enumerated set of intensities for current color channel.	*/
#define PutRun(color, num) \
	{ \
	putshort(RRunData(num)); \
	putshort(color); \
	if(debug) \
		(void) fprintf( stderr, \
				"Run Data, len=%d, intensity=%d\n", \
				 num, color ); \
	}
/*
 * Opcode definitions
 */
#define	    RSkipLines(n)	    ((RSkipLinesOp<<12) | ((n) & 0x3ff))
#define	    RSetColor(c)	    ((RSetColorOp<<12) | ((c) & 03))
					/* has side effect of performing */
					/* "carriage return" action */
#define	    RSkipPixels(n)	    ((RSkipPixelsOp<<12) | ((n) & 0x3ff))
#define	    RNewLine		    RSkipLines(1)
#define	    RByteData(n)	    ((RByteDataOp<<12) | (((n)-1) & 0x3ff))
					/* followed by ((n+1)/2)*2 bytes */
					/* of data.  If n is odd, last */
					/* byte will be ignored */
					/* "cursor" is left at pixel */
					/* following last pixel written */
#define	    RRunData(n)		    ((RRunDataOp<<12) | (((n)-1) & 0x3ff))
					/* next word contains color data */
					/* "cursor" is left at pixel after */
					/* end of run */
#define NSEG	1024/3		/* Number of run segments of storage */
static struct runs
	{
	Pixel *first;
	Pixel *last;
	} runs[NSEG];		/* ptrs to non-background run segs */

#define HIDDEN
HIDDEN void	_put_Data();
HIDDEN int	_put_Color_Map_Seg();
HIDDEN int	_get_Color_Map_Seg();

/* Global data.								*/
int	_bg_flag = 2;
int	_bw_flag = 0;
int	_cm_flag = 0;
Pixel	_bg_pixel = { 0, 0, 0, 0 };


/*	r l e _ w h d r ( )
 	This routine should be called after 'setfbsize()', unless the
	framebuffer image is the default size (512).
	This routine should be called before 'rle_encode_ln()' to set up
	the global data: _bg_flag, _bw_flag, _cm_flag, and _bg_pixel.
	Returns -1 for failure, 0 otherwise.
 */
rle_whdr( fp, bwflag, bgflag, cmflag, bbw, bgpixel )
FILE		*fp;
int		bwflag, bgflag, cmflag;
int		bbw;
Pixel		*bgpixel;
	{
	/* Magic numbers for output file.				*/
	static char	tab_magic[] = "\0OB";
	Rle_Header	setup;
	_bg_flag = bgflag;
	_bw_flag = bwflag;
	_cm_flag = cmflag;
	_bg_pixel = *bgpixel;
	setup.magic = (_bw_flag ? WMAGIC : RMAGIC) | tab_magic[_bg_flag];
	setup.xpos = 0;
	setup.ypos = 0;
	setup.xsize = _fbsize;
	setup.ysize = _fbsize;
	setup.bg_r = _bw_flag ? bbw : _bg_pixel.red;
	setup.bg_g = _bw_flag ? bbw : _bg_pixel.green;
	setup.bg_b = _bw_flag ? bbw : _bg_pixel.blue;
	setup.map = _cm_flag;
	if( fwrite( &setup, sizeof setup, 1, fp ) != 1 )
		{
		(void) fprintf( stderr, "Write of RLE header failed!\n" );
		return	-1;
		}
	return	0;
	}

/*	r l e _ r h d r ( )
	This routine should be called before 'rle_decode_ln()' to set up
	the global data: _bg_flag, _bw_flag, _cm_flag, and _bg_pixel.
	Returns -1 for failure, 0 otherwise.
 */
rle_rhdr( fp, bgflag, bwflag, cmflag, olflag, bgpixel )
FILE	*fp;
int	bgflag;
int	*bwflag;
int	*cmflag;
int	olflag;
Pixel	*bgpixel;
	{
	Rle_Header	setup;
	register int	mode;
	static char	*verbage[] =
		{
		"Frame buffer image saved in ColorRun Length Encoded form\n",
		"Frame buffer image saved in B&W RLE form\n",
		"File not in RLE format, can't display (magic=0x%x)\n",
		"Saved with background color %d %d %d\n",
		"Saved in overlay mode\n",
		"Saved as a straight box image\n",
		NULL
		};

	if( fread( (char *)&setup, sizeof(Rle_Header), 1, fp ) != 1 )
		{
		(void) fprintf( stderr, "Read of RLE header failed!\n" );
		return	-1;
		}
	switch( setup.magic & ~0xff) {
	case RMAGIC :
		if( verbose )
			(void) fprintf( stderr,	verbage[0] );
		_bw_flag = 0;
		mode = setup.magic & 0xFF;
		break;
	case WMAGIC :
		if( verbose )
			(void) fprintf( stderr, verbage[1] );
		_bw_flag = 1;
		mode = setup.magic & 0xFF;
		break;
	default:
		(void) fprintf(	stderr, verbage[2], setup.magic );
		return	-1;
	}
#ifdef POSFLAG
	if( posflag )
		{
		if( verbose )
			(void) fprintf( stderr,
					"Originally positioned at (%d, %d)\n",
					setup.xpos, setup.ypos
					);
		if( posflag == 1 )
			{ /* Incremental positioning.			*/
			setup.xpos = (posx + setup.xpos) % _fbsize;
			setup.ypos = (posy + setup.ypos) % _fbsize;
			}
		else
			{
			setup.xpos = posx % _fbsize;
			setup.ypos = posy % _fbsize;
			}
		}
	if (verbose) 
		(void) fprintf( stderr,
				"Positioned at (%d, %d), size (%d %d)\n",
				setup.xpos,
				setup.ypos,
				setup.xsize,
				setup.ysize
				);
#endif POSFLAG
	switch( mode ) {
	case 'B' : /* Background given.					*/
		if( verbose )
			(void) fprintf( stderr,
					verbage[3],
					setup.bg_r, setup.bg_g, setup.bg_b
					);
		if( bgflag == 0 )
			{
			/* No command-line backgr., use saved values.	*/
			_bg_pixel.red = setup.bg_r;
			_bg_pixel.green = setup.bg_g;
			_bg_pixel.blue = setup.bg_b;
			}
		break;
	case 'O': /* Overlay mode.					*/
		if( verbose )
			(void) fprintf( stderr, verbage[4] );
		break;
	default:
		if( verbose )
			(void) fprintf( stderr, verbage[5] );
		(void) fprintf( stderr, "unknown display mode x%x\n", mode );
		break;
	}

	/*
	 * Checks
	 */
	if( setup.xsize > _fbsize )  {
		(void) fprintf( stderr,	"truncating xsize from %d to %d\n",
			setup.xsize,
			_fbsize
			);
		setup.xsize = _fbsize;
	}
	if( setup.ysize > _fbsize )  {
		(void) fprintf( stderr,	"truncating ysize from %d to %d\n",
			setup.ysize,
			_fbsize
			);
		setup.ysize = _fbsize;
	}
	if( olflag && bgflag )
		{
		(void) fprintf( stderr,
		"Error:  both overlay and background modes selected\n"
				);
		return	-1;
		}
	*bwflag = _bw_flag;
	*cmflag = setup.map;
	*bgpixel = _bg_pixel;
	return	0;
	}

/*	r l e _ r m a p ( )
	Read a color map in RLE format.
	Returns -1 upon failure, 0 otherwise.
 */
rle_rmap( fp, cmap )
FILE		*fp;
ColorMap	*cmap;
	{
	if(verbose)
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
rle_wmap( fp, cmap )
FILE		*fp;
ColorMap	*cmap;
	{
	if(verbose)
		(void) fprintf( stderr, "Writing color map\n" );
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
rle_decode_ln( fp, scan_buf )
FILE	*fp;
Pixel	*scan_buf;
	{
	static Rle_Word inst;
	static short	word;
	static int	lines_to_skip = 0;
	register int	n;
	register u_char	*pp = (u_char *) scan_buf; /* Pointer into pixel. */
	register int	dirty_flag = 0;

	if( lines_to_skip > 0 )
		{
		lines_to_skip--;
		return	dirty_flag;
		}
	while( _Get_Inst( fp, &inst ) != EOF )
		{
		if( debug )
			(void) fprintf( stderr,
					"op %d, datum %d\n",
					inst.opcode,
					inst.datum
					);
		switch( n = inst.opcode )
			{
			case RSkipLinesOp :
				lines_to_skip = inst.datum;
				if( debug )
					(void) fprintf( stderr,
							"Skip Lines %d\n",
							lines_to_skip
							);
				if( lines_to_skip-- < 1 )
					return	-1;
				else
					return	dirty_flag;
			case RSetColorOp:
				if( debug )
					(void) fprintf( stderr,
							"Set Color %d\n",
							inst.datum
							);
			/*
			 * Select "color channel" that following ops go to.
			 *
			 * Set `pp' to point to starting pixel element;
			 * by adding STRIDE to pp, will move to corresponding
			 * color element in next pixel.
			 * If Black & White image:  point to left-most
			 * byte (Red for Ikonas) in long,
			 * and Run and Data will ignore strides below.
			 */
				if( _bw_flag )
					n = 0;
				else
					n = inst.datum;
				switch( n )
					{
					case 0:
						pp = &(scan_buf->red);
						break;
					case 1:
						pp = &(scan_buf->green);
						break;
					case 2:
						pp = &(scan_buf->blue);
						break;
					default:
						(void) fprintf( stderr,
							"Bad color %d\n",
								n
								);
						return	-1;
					}
				break;
			case RSkipPixelsOp:
				n = inst.datum;
				if( debug )
					(void) fprintf( stderr,
							"Skip Pixels %d\n",
							n
							);
				pp += n * STRIDE; /* advance pixel ptr */
				break;
			case RByteDataOp:
				n = inst.datum + 1;
				if( debug )
					(void) fprintf( stderr,
						"Byte Data, count=%d.\n",
							n
							);
	
				if( ! _bw_flag )
					{
					while( n-- > 0 )
						{
						*pp = getc( fp );
						pp += STRIDE;
						}
					}
				else
					{ /* Ugh, black & white.	*/
					register u_char c;
					while( n-- > 0 )
						{
						*pp++ = c = getc( fp );
						*pp++ = c;
						*pp++ = c;
						}
					}
				if( feof( fp ) )
					{
					(void) fprintf( stderr,
				"unexpected EOF while reading Byte Data\n"
							);
					return	-1;
					}
				if( (inst.datum+1) & 01 )
					/* word align file ptr */
					(void) getc( fp );
				dirty_flag = 1;
				break;
			case RRunDataOp:
				n = inst.datum + 1;
				{
				register char *p = (char *) &word;
				*p++ = getc( fp );
				*p++ = getc( fp );
				}
				if(debug)
					(void) fprintf( stderr,	
							"Run-Data(len=%d,inten=%d)\n",
							n,
							word
							);
	
				if( ! _bw_flag )
					{
					register u_char inten = (u_char)word;
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
						*pp++ = (u_char) word;
						*pp++ = (u_char) word;
						*pp++ = (u_char) word;
						}
					}
				dirty_flag = 1;
				break;
			default:
				(void) fprintf( stderr,
						"Unrecognized opcode: %d (x%x x%x)\n",
						inst.opcode, inst.opcode, inst.datum
						);
				if( ! debug )
					return	-1;
			}
		}
	return	dirty_flag;
	}

/* 	r l e _ e n c o d e _ l n ( )
	Encode a given scanline of pixels into RLE format.
	Returns -1 upon failure, 0 otherwise.
 */
rle_encode_ln( fp, scan_buf )
FILE	*fp;
Pixel	*scan_buf;
	{
	register Pixel *scan_p = scan_buf;
	register Pixel *last_p = &scan_buf[_fbsize];
	register int	i;
	register int	color;		/* holds current color */
	register int	nseg;		/* number of non-bg run segments */

	if( _bg_flag )
		{
		if( (nseg = _bg_Get_Runs( scan_p, last_p )) == -1 )
			return	-1;
		}
	else
		{
		runs[0].first = scan_p;
		runs[0].last = last_p;
		nseg = 1;
		}
	if( nseg <= 0 )
		{
		SkipBlankLines( 1 );
		return	0;
		}
	if( _bw_flag )
		{
		register Pixel *pixelp;
		/* Compute Black & White in blue row */
		for( pixelp=scan_p; pixelp <= last_p; pixelp++ )
			pixelp->blue =  .35 * pixelp->red +
					.55 * pixelp->green +
					.10 * pixelp->blue;
		}

	/* do all 3 colors */
	for( color = 0; color < 3; color++ )
		{
		if( _bw_flag && color != 2 )
			continue;

		SetColor( color );
		if( runs[0].first != scan_p )
			{
			SkipPixels( runs[0].first - scan_p );
			}
		for( i = 0; i < nseg; i++ )
			{
			_encode_Seg_Color( fp, i, color );
			/* Move along to next segment for encoding,
				if this was not the last segment.
			 */
			if( i < nseg-1 )
				SkipPixels( runs[i+1].first-runs[i].last-1 );
			}
		}
	NewScanLine();
	return	0;
	}

/*	_ b g _ G e t _ R u n s ( )
	Fill the 'runs' segment array from 'pixelp' to 'endpix'.
	This routine will fail and return -1 if the array fills up
	before all pixels are processed, otherwise a 'nseg' is returned.
 */
_bg_Get_Runs( pixelp, endpix )
register Pixel *pixelp;
register Pixel *endpix;
	{
	/* find non-background runs */
	register int	nseg = 0;
	while( pixelp <= endpix && nseg < NSEG )
		{
		if(	pixelp->red != _bg_pixel.red
		    ||	pixelp->green != _bg_pixel.green
		    ||	pixelp->blue != _bg_pixel.blue
			)
			{
			/* We have found the start of a segment */
			runs[nseg].first = pixelp++;

			/* find the end of this run */
			while(	pixelp <= endpix
			    &&	 (	pixelp->red != _bg_pixel.red
				   ||	pixelp->green != _bg_pixel.green
				   ||	pixelp->blue != _bg_pixel.blue
		    		 )
				)
				pixelp++;

			/* last pixel in segment */
			runs[nseg++].last = pixelp-1;
			}
		pixelp++;
		}
	if(verbose)
		(void) fprintf( stderr," (%d segments)\n", nseg );
	if( nseg >= NSEG )
		{
		(void) fprintf( stderr,
				"Encoding incomplete, " );
		(void) fprintf( stderr, 
				"segment array 'runs[%d]' is full!\n",
				NSEG
				);
		return	-1;
		}
	return	nseg;
	}

/*	_ e n c o d e _ S e g _ C o l o r ( )
	Encode a segment, 'seg', for specified 'color'.
 */
_encode_Seg_Color( fp, seg, color )
FILE	*fp;
int	color;
register int	seg;
	{
	static Pixel *data_p;
	static Pixel *last_p;

	switch( color )
		{
		case 0:
			data_p = (Pixel *) &(runs[seg].first->red);
			last_p = (Pixel *) &(runs[seg].last->red);
			break;
		case 1:
			data_p = (Pixel *) &(runs[seg].first->green);
			last_p = (Pixel *) &(runs[seg].last->green);
			break;
		case 2:
			data_p = (Pixel *) &(runs[seg].first->blue);
			last_p = (Pixel *) &(runs[seg].last->blue);
			break;
		}
	_encode_Segment( fp, data_p, last_p );
	return;
	}

/*	_ e n c o d e _ S e g m e n t ( )
	Output code for segment.
 */
_encode_Segment( fp, data_p, last_p )
FILE		*fp;
register Pixel	*data_p;
register Pixel	*last_p;
	{
	register Pixel	*runs_p = data_p;
	register Pixel	*pixelp;
	register int	state = DATA;
	register u_char	runval = data_p->CUR;

	for( pixelp = data_p + 1; pixelp <= last_p; pixelp++ )
		{
		switch( state )
			{
			case DATA :
				if( runval == pixelp->CUR )
					{
					/* 2 in a row, may be a run */
					state = RUN2;
					}
				else
					{
					/* continue accumulating data, look
						for a new run starting here,
						too
					 */
					runval = pixelp->CUR;
					runs_p = pixelp;
					}
				break;
			case RUN2:
				if( runval == pixelp->CUR )
					{
					/* 3 in a row is a run */
					state = INRUN;
					/* Flush out data sequence
						encountered before this run
					 */
					_put_Data(	fp,
							&(data_p->CUR),
						 	runs_p-data_p
						 	);
					}
				else
					{
					/* not a run, */
					/* but maybe this starts one */
					state = DATA;
					runval = pixelp->CUR;
					runs_p = pixelp;
					}
				break;
			case INRUN:
				if( runval != pixelp->CUR )
					{
					/* if run out */
					state = DATA;
					PutRun(	runval,
						pixelp - runs_p
						);
					/* who knows, might be more */
					runval = pixelp->CUR;
					runs_p = pixelp;
					/* starting new 'data' run */
					data_p = pixelp;
					}
				break;
				}
			}
		/* Write out last portion of section being encoded.	*/
		if( state == INRUN )
			{
			PutRun( runval, pixelp - runs_p );
			}
		else
			{
			_put_Data( fp, &data_p->CUR, pixelp - data_p );
			}
	return;
	}

/*	_ p u t _ D a t a ( )
	Put one or more pixels of byte data into the output file.
 */
HIDDEN void
_put_Data( fp, cp, n )
register FILE	*fp;
register u_char *cp;
int	n;
	{
	register int count = n;

	if( count == 0 )
		return;
	putshort( RByteData(n) );

	if( fp->_cnt >= count )
		{
		register u_char *op = fp->_ptr;
		fp->_cnt -= count;
		while( count-- > 0 )
			{
			*op++ = *cp;
			cp += STRIDE;
			}
		fp->_ptr = op;
		}
	else
		{
		while( count-- > 0 )
			{
			(void) putc( (int) *cp, fp );
			cp += STRIDE;
			}
		}
	if( n & 1 )
		(void) putc( 0, fp );	/* short align output */
	if( debug )
		(void) fprintf( stderr, "Byte Data, len=%d\n", n );
	return;
	}

/*	_ g e t _ C o l o r _ M a p _ S e g ( )
	Read the color map stored in the RLE file.
	The RLE format stores color map entries as short integers, so
	we have to stuff them into u_chars.
 */
static
_get_Color_Map_Seg( fp, cmap_seg )
FILE	*fp;
register u_char	*cmap_seg;
	{
	short	rle_cmap[256];
	register short	*cm = rle_cmap;
	register int	i;

	if( fread( (char *) rle_cmap, sizeof(short), 256, fp ) != 256 )
		{
		(void) fprintf( stderr,	"Failed to read color map!\n" );
		return	-1;
		}
	for( i = 0; i < 256; ++i )
		{
		*cmap_seg++ = (u_char) *cm++;
		}
	return	0;
	}

/*	_ p u t _ C o l o r _ M a p _ S e g ( )
	Output color map segment to RLE file as shorts.
 */
HIDDEN
_put_Color_Map_Seg( fp, cmap_seg )
FILE	*fp;
register u_char	*cmap_seg;
	{
	short		rle_cmap[256];
	register short	*cm = rle_cmap;
	register int	i;

	for( i = 0; i < 256; ++i )
		{
		*cm++ = (short) *cmap_seg++;
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
