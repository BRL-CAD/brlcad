/*
	SCCS id:	@(#) fb-rle.c	1.6
	Last edit: 	6/6/85 at 13:21:05
	Retrieved: 	8/13/86 at 03:11:00
	SCCS archive:	/m/cad/fb_utils/RCS/s.fb-rle.c

	Author:		Gary S. Moss
			U. S. Army Ballistic Research Laboratory
			Aberdeen Proving Ground
			Maryland 21005
			(301)278-6647 or AV-283-6647
 */
#if ! defined( lint )
static
char	sccsTag[] = "@(#) fb-rle.c	1.6	last edit 6/6/85 at 13:21:05";
#endif
#include <stdio.h>
#include <fb.h>
#include <rle.h>
#ifndef pdp11
#define MAX_DMA	1024*64
#else
#define MAX_DMA	1024*16
#endif
#define DMA_PIXELS	(MAX_DMA/sizeof(Pixel))
#define DMA_SCANS	(DMA_PIXELS/_fbsize)
#define PIXEL_OFFSET	((scan_ln%dma_scans)*_fbsize)
static char	*usage[] = {
"",
"fb-rle (1.6)",
"",
"Usage: fb-rle [-CScdhvw][-l X Y][-p X Y][file.rle]",
"",
"If no rle file is specifed, fb-rle will write to its standard output.",
"If the environment variable FB_FILE is set, its value will be used",
"	to specify the framebuffer file or device to read from.",
0
};
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
	scan_bytes = _fbsize * sizeof(Pixel);
	if( rle_verbose )
		(void) fprintf( stderr,
				"Background is %d %d %d\n",
				bgpixel.red, bgpixel.green, bgpixel.blue
				);
	rle_wlen( xlen, ylen, 1 );
	rle_wpos( xpos, ypos, 1 );
	if( rle_whdr( fp, ncolors, bgflag, cmflag, &bgpixel ) == -1 )
		return	1;
	if( fbopen( NULL, APPEND ) == -1 )
		return	1;
	if( cmflag )
		{
		if( fb_rmap( &cmap ) == -1 )
			{ /* No map saved, assume standard map.		*/
			if( rle_wmap( fp, (ColorMap *) NULL ) == -1 )
				return	1;
			}
		else
			{
			if( rle_wmap( fp, &cmap ) == -1 )
				return	1;
			}
		if( rle_debug )
			(void) fprintf( stderr,
					"Color map saved.\n"
					);
		}
	else
	if( crunch && fb_rmap( &cmap ) == -1 )
		{
		(void) fprintf( stderr, "Could not read colormap!\n" );
		crunch = 0;
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
			if( fbread( 0, y_buffer, scan_buf, dma_pixels ) == -1)
				return	1;
			if( crunch )
				do_Crunch( scan_buf, dma_pixels, &cmap );
			}
		if( rle_encode_ln( fp, scan_buf+PIXEL_OFFSET ) == -1 )
			return	1;
		if( page_fault = ! (scan_ln%dma_scans) )
			y_buffer -= dma_scans;
		}
	}
	return	0;
	}

/*	p a r s A r g v ( )
 */
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
			setfbsize( 1024 );
			break;
		case 'l' : /* Length in x and y.			*/
			if( argc - optind < 2 )
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
			if( argc - optind < 2 )
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
		if( access( argv[optind], 0 ) == 0 )
			{
			(void) fprintf( stderr,
					"\"%s\" already exists.\n",
					argv[optind]
					);
			if( isatty( 0 ) )
				{ register int	c;
				(void) fprintf( stderr,
						"Overwrite \"%s\" [y] ? ",
						argv[optind]
						);
				c = getchar();
				while( c != '\n' && getchar() != '\n' )
					;
				if( c != 'y' )
					exit( 1 );
				}
			else
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
	if( argc > ++optind )
		{
		(void) fprintf( stderr, "Too many arguments!\n" );
		return	0;
		}
	if( xlen == 0 )
		xlen = _fbsize;
	if( ylen == 0 )
		ylen = _fbsize;
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
		scan_buf->red = cmap->cm_red[scan_buf->red];
		scan_buf->green = cmap->cm_green[scan_buf->green];
		scan_buf->blue = cmap->cm_blue[scan_buf->blue];
		}
	return;
	}
