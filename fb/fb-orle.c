/*
 *			F B - R L E . C
 *
 *  Encode a frame buffer image using the RLE library
 *
 *  Author -
 *	Gary S. Moss
 *  
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 *  
 *  Copyright Notice -
 *	This software is Copyright (C) 1986 by the United States Army.
 *	All rights reserved.
 */
#ifndef lint
static char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include <stdio.h>
#include "fb.h"
#include "rle.h"

#ifndef pdp11
#define MAX_DMA	1024*64
#else
#define MAX_DMA	1024*16
#endif
#define DMA_PIXELS	(MAX_DMA/sizeof(Pixel))
#define DMA_SCANS	(DMA_PIXELS/width)
#define PIXEL_OFFSET	((scan_ln%dma_scans)*width)
static char	*usage[] = {
"",
"fb-rle (1.11)",
"",
"Usage: fb-rle [-CScdhvw][-l X Y][-p X Y][file.rle]",
"",
"If no rle file is specifed, fb-rle will write to its standard output.",
"If the environment variable FB_FILE is set, its value will be used",
"	to specify the framebuffer file or device to read from.",
0
};

static FBIO	*fbp;
static FILE	*fp = stdout;
static Pixel	bgpixel = { 0, 0, 0, 0 };
static int	bgflag = 1;
static int	ncolors = 3;
static int	cmflag = 1;
static int	crunch = 0;
static int	xpos = 0, ypos = 0, xlen = 0, ylen = 0;
static int	parsArgv();
static void	do_Crunch();
static void	prntUsage();
static int	width = 512;

/*	m a i n ( )							*/
main( argc, argv )
int	argc;
char	*argv[];
	{
	register int	scan_ln;
	register int	dma_scans;
	static Pixel	scan_buf[DMA_PIXELS];
	static ColorMap	cmap;
	static int	scan_bytes;
	static int	dma_pixels;

	if( ! parsArgv( argc, argv ) )
		{
		prntUsage();
		return	1;
		}
	setbuf( fp, malloc( BUFSIZ ) );
	dma_pixels = DMA_PIXELS;
	dma_scans = DMA_SCANS;
	scan_bytes = width * sizeof(Pixel);
	rle_wlen( xlen, ylen, 1 );
	rle_wpos( xpos, ypos, 1 );

	if( (fbp = fb_open( NULL, width, width )) == NULL )  {
		fprintf(stderr,"fb_open failed\n");
		exit(12);
	}

	/* Read color map, see if it's linear */
	cmflag = 1;		/* Need to save colormap */
	if( fb_rmap( fbp, &cmap ) == -1 )
		cmflag = 0;
	if( is_linear_cmap( &cmap ) )
		cmflag = 0;
	if( crunch && (cmflag == 0) )
		crunch = 0;

	/* Acquire "background" pixel from special location */
	if(	bgflag
	    &&	fb_read( fbp, 1, 1, &bgpixel, 1 ) == -1
		)
		{
		(void) fprintf( stderr, "Couldn't read background!\n" );
		return	1;
		}	
	if( bgflag && rle_verbose )
		(void) fprintf( stderr,
				"Background saved as %d %d %d\n",
				bgpixel.red, bgpixel.green, bgpixel.blue
				);

	/* Write RLE header */
	if( rle_whdr( fp, ncolors, bgflag, cmflag, &bgpixel ) == -1 )
		return	1;

	/* Follow RLE header with colormap */
	if( cmflag )  {
		if( rle_wmap( fp, &cmap ) == -1 )
			return	1;
		if( rle_debug )
			(void) fprintf( stderr,
					"Color map saved.\n"
					);
	}

	if( ncolors == 0 )
		/* Only save colormap, so we are finished.		*/
		return	0;

	/* Save image.							*/
	{	register int	page_fault = 1;
		register int	y_buffer = (ypos + ylen) - dma_scans;
	for( scan_ln = ypos + (ylen-1); scan_ln >= ypos; --scan_ln )
		{
		if( page_fault )
			{
			if( fb_read( fbp, 0, y_buffer, scan_buf, dma_pixels ) == -1)
				{
				(void) fprintf(	stderr,
					"read of %d pixels from (0,%d) failed!\n",
						dma_pixels,
						y_buffer
						);
						
				return	1;
				}
			if( crunch )
				do_Crunch( scan_buf, dma_pixels, &cmap );
			}
		if( rle_encode_ln( fp, scan_buf+PIXEL_OFFSET ) == -1 )
			return	1;
		if( page_fault = ! (scan_ln%dma_scans) )
			y_buffer -= dma_scans;
		if( y_buffer < 0 )
			{
			dma_scans += y_buffer * xlen;
			y_buffer = 0;
			}
		}
	}
	fb_close( fbp );
	return	0;
	}

/*	p a r s A r g v ( )						*/
static int
parsArgv( argc, argv )
register char	**argv;
	{
	register int	c;
	extern int	optind;
	extern char	*optarg;

	/* Parse options.						*/
	while( (c = getopt( argc, argv, "CScdhl:p:vw" )) != EOF )
		{
		switch( c )
			{
		case 'C' : /* Crunch color map.				*/
			crunch = 1;
			cmflag = 0;
			break;
		case 'S' : /* 'Box' save, store entire image.		*/
			bgflag = 0;
			break;
		case 'c' : /* Only save color map.			*/
			ncolors = 0;
			break;
		case 'd' : /* For debugging.				*/
			rle_debug = 1;
			break;
		case 'h' : /* High resolution.				*/
			width = 1024;
			break;
		case 'l' : /* Length in x and y.			*/
			if( argc - optind < 1 )
				{
				(void) fprintf( stderr,
				"-l option requires an X and Y argument!\n"
						);
				return	0;
				}
			xlen = atoi( optarg );
			ylen = atoi( argv[optind++] );
			break;
		case 'p' : /* Position of bottom-left corner.		*/
			if( argc - optind < 1 )
				{
				(void) fprintf( stderr,
				"-p option requires an X and Y argument!\n"
						);
				return	0;
				}
			xpos = atoi( optarg );
			ypos = atoi( argv[optind++] );
			break;
		case 'v' : /* Verbose on.				*/
			rle_verbose = 1;
			break;
		case 'w' : /* Monochrome (black & white) mode.		*/
			ncolors = 1;
			break;
		case '?' :
			return	0;
			}
		}
	if( argv[optind] != NULL )
		{
		if( access( argv[optind], 0 ) == 0 )
			{
			(void) fprintf( stderr,
					"\"%s\" already exists.\n",
					argv[optind]
					);
			exit( 1 );
			}
		if( (fp = fopen( argv[optind], "w" )) == NULL )
			{
			(void) fprintf( stderr,
					"Can't open %s for writing!\n",
					argv[optind]
					);
			return	0;
			}
		}
	if( argc > ++optind )
		{
		(void) fprintf( stderr, "Too many arguments!\n" );
		return	0;
		}
	if( isatty(fileno(fp)) )
		return 0;
	if( xlen == 0 )
		xlen = width;
	if( ylen == 0 )
		ylen = width;
	return	1;
	}

/*	p r n t U s a g e ( )
	Print usage message.
 */
static void
prntUsage()
	{
	register char	**p = usage;

	while( *p )
		{
		(void) fprintf( stderr, "%s\n", *p++ );
		}
	return;
	}

/*	d o _ C r u n c h ( )						*/
static void
do_Crunch( scan_buf, pixel_ct, cmap )
register Pixel		*scan_buf;
register int		pixel_ct;
register ColorMap	*cmap;
	{
	for( ; pixel_ct > 0; pixel_ct--, scan_buf++ )
		{
		scan_buf->red = cmap->cm_red[scan_buf->red]>>8;
		scan_buf->green = cmap->cm_green[scan_buf->green]>>8;
		scan_buf->blue = cmap->cm_blue[scan_buf->blue]>>8;
		}
	return;
	}

/*
 *  Check for a color map being linear in R, G, and B.
 *  Returns 1 for linear map, 0 for non-linear map
 *  (ie, non-identity map).
 */
is_linear_cmap( cmap )
register ColorMap *cmap;
{
	register int i;
	unsigned short v;

	for( i=0; i<256; i++ )  {
		v = (unsigned short)(i<<8);
		if( cmap->cm_red[i] != v )  return(0);
		if( cmap->cm_green[i] != v )  return(0);
		if( cmap->cm_blue[i] != v )  return(0);
	}
	return(1);
}
