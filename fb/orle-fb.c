/*
	SCCS id:	@(#) rle-fb.c	1.5
	Last edit: 	3/27/85 at 11:28:44
	Retrieved: 	8/13/86 at 03:17:07
	SCCS archive:	/m/cad/fb_utils/RCS/s.rle-fb.c

	Author:		Gary S. Moss
			U. S. Army Ballistic Research Laboratory
			Aberdeen Proving Ground
			Maryland 21005
			(301)278-6647 or AV-283-6647
 */
#if ! defined( lint )
static
char	sccsTag[] = "@(#) rle-fb.c	1.5	last edit 3/27/85 at 11:28:44";
#endif
#include <stdio.h>
#include <fb.h>
#include <rle.h>
#include <memory.h>
#ifndef pdp11
#define MAX_DMA	1024*64
#else
#define MAX_DMA	1024*16
#endif
#define PIXELS_PER_DMA	(MAX_DMA/sizeof(Pixel))
#define SCANS_PER_DMA	(PIXELS_PER_DMA/fbsz)
#define PIXEL_OFFSET	((scan_ln%scans_per_dma)*fbsz)
#define Fbread_Dma( y )\
		if( y >= scans_per_dma &&\
		    fbread(0,(y)-scans_per_dma,scan_buf,pixels_per_dma)==-1)\
			return 1;

typedef unsigned char	u_char;
static char	*usage[] = {
"",
"rle-fb (1.5)",
"",
"Usage: rle-fb [-Odv][-b (rgbBG)][-p X Y] [file.rle]",
"",
"If no rle file is specifed, rle-fb will read its standard input.",
"If the environment variable FB_FILE is set, its value will be used",
"	to specify the framebuffer file or device to write to.",
0
};

static FILE	*fp = stdin;
static Pixel	bgpixel = { 0, 0, 0, 0 };
static int	bgflag = 0;
static int	cmflag = 0;
static int	olflag = 0;
static int	pars_Argv();
static void	prnt_Cmap();
static void	prnt_Usage();

/*	m a i n ( )							*/
main( argc, argv )
int	argc;
char	*argv[];
	{
	register int	scan_ln;
	register int	fbsz = 512;
	register int	scans_per_dma;
	static Pixel	scan_buf[PIXELS_PER_DMA];
	static Pixel	bg_scan[1024];
	static ColorMap	cmap;
	static int	get_flags;
	static int	xlen, ylen;
	static int	xpos, ypos;
	static int	scan_bytes;
	static int	pixels_per_dma;

	if( ! pars_Argv( argc, argv ) )
		{
		prnt_Usage();
		return	1;
		}
	if(	rle_rhdr(	fp,
				&get_flags,
				bgflag ? NULL : &bgpixel,
				&xlen, &ylen, &xpos, &ypos
				)
	    ==	-1
		)
		return	1;

	/* Automatic selection of high res. device.			*/
	if( xlen > 512 || ylen > 512 )
		{
		fbsz = 1024;
		setfbsize( fbsz );
		}
	pixels_per_dma = PIXELS_PER_DMA;
	scans_per_dma = SCANS_PER_DMA;
	scan_bytes = fbsz * sizeof(Pixel);
	if( fbopen( NULL, APPEND ) == -1 )
		return	1;

	if( rle_verbose )
		(void) fprintf( stderr,
				"Background is %d %d %d\n",
				bgpixel.red, bgpixel.green, bgpixel.blue
				);

	/* If color map provided, use it, else go with standard map. */
	if( ! (get_flags & NO_COLORMAP) )
		{
		if( rle_verbose && ! olflag )
			(void) fprintf( stderr,
					"Loading saved color map from file\n"
					);
		if( rle_rmap( fp, &cmap ) == -1 )
			return	1;
		if( rle_verbose )
			prnt_Cmap( &cmap );
		if( ! olflag )
			{
			/* User-supplied map */
			if( rle_verbose )
				(void) fprintf( stderr,
					"Writing color map to framebuffer\n"
						);
			if( fb_wmap( &cmap ) == -1 )
				return	1;
			}
		}
	else
	if( ! olflag )
		{
		if( rle_verbose )
			(void) fprintf( stderr,
					"Creating standard color map\n"
					);
		if( fb_wmap( (ColorMap *) NULL ) == -1 )
			return	1;
		}
	/* Fill buffer with background.					*/
	if( ! olflag && (get_flags & NO_BOX_SAVE) )
		{
		register int	i;
		register Pixel	*to;
		register Pixel	*from;

		to = bg_scan;
		from = &bgpixel;
		for( i = 0; i < fbsz; i++ )
			*to++ = *from;
		}

	{	register int	page_fault = 1;
		register int	dirty_flag = 1;
		register int	y_buffer = fbsz - scans_per_dma;

	for( scan_ln = fbsz - 1; scan_ln >= fbsz - ylen; scan_ln-- )
		{
		if( page_fault )
			{
			if( olflag )
				{ /* Overlay -- read cluster from fb.	*/
				if(	fbread(	0,
						y_buffer,
						scan_buf,
						pixels_per_dma
						)
				    ==	-1
					)
					return	1;
				}
			else
			if( (get_flags & NO_BOX_SAVE) && dirty_flag )
				{ /* Fill buffer with background.	*/
				register int	i;
				register Pixel	*scan_ptr = scan_buf;
				for( i = 0; i < scans_per_dma; ++i )
					{
					(void) memcpy(	(char *) scan_ptr,
							(char *) bg_scan,
							scan_bytes
							);
					scan_ptr += fbsz;
					}
				}
			dirty_flag = 0;
			page_fault = 0;
			}
		{ register int
			touched = rle_decode_ln( fp, scan_buf+PIXEL_OFFSET );

			if( touched == -1 )
				return	1;
			else
				dirty_flag += touched;
		}
		if( page_fault = ! (scan_ln%scans_per_dma) )
			{
			if( fbwrite( 0, y_buffer, scan_buf, pixels_per_dma )
			    ==	-1
				)
				return	1;
			y_buffer -= scans_per_dma;
			}
		} /* end for */
	} /* end block */
	return	0;
	}

/*	p a r s _ A r g v ( )
 */
static int
pars_Argv( argc, argv )
register char	**argv;
	{
	register int	c;
	extern int	optind;
	extern char	*optarg;

	/* Parse options.						*/
	while( (c = getopt( argc, argv, "Ob:dvp:" )) != EOF )
		{
		switch( c ) {
		case 'O' : /* Overlay mode.				*/
			olflag = 1;
			break;
		case 'b' : /* User-specified background.		*/
			bgflag = optarg[0];
			switch( bgflag ) {
			case 'r':
				bgpixel.red = 255;
				break;
			case 'g':
				bgpixel.green = 255;
				break;
			case 'b':
				bgpixel.blue = 255;
				break;
			case 'w':
				bgpixel.red =
				bgpixel.green =
				bgpixel.blue = 255;
				break;
			case 'B':		/* Black */
				break;
			case 'G':		/* 18% grey, for alignments */
				bgpixel.red =
				bgpixel.green =
				bgpixel.blue = 255.0 * 0.18;
				break;
			default:
				(void) fprintf( stderr,
						"Background '%c' unknown\n",
						bgflag
						);
				bgflag = 0;
				break;
			} /* End switch */
			break;
		case 'd' :
			rle_debug = 1;
			break;
		case 'v' :
			rle_verbose = 1;
			break;
		case 'p' :
			(void) fprintf( stderr,
					"-p option not implemented!\n"
					);
			break;
		case '?' :
			return	0;
		} /* end switch */
		} /* end while */

	if( argv[optind] != NULL )
		if( (fp = fopen( argv[optind], "r" )) == NULL )
			{
			(void) fprintf( stderr,
					"Can't open %s for reading!\n",
					argv[optind]
					);
			return	0;
			}
	if( argc > ++optind )
		{
		(void) fprintf( stderr, "Too many arguments!\n" );
		return	0;
		}
	return	1;
	}

/*	p r n t _ U s a g e ( )
	Print usage message.
 */
static void
prnt_Usage()
	{
	register char	**p = usage;

	while( *p )
		{
		(void) fprintf( stderr, "%s\n", *p++ );
		}
	return;
	}

static void
prnt_Cmap( cmap )
ColorMap	*cmap;
	{
	register u_char	*cp;
	register int	i;

	(void) fprintf( stderr, "\t\t\t_________ Color map __________\n" );
	(void) fprintf( stderr, "Red segment :\n" );
	for( i = 0, cp = cmap->cm_red; i < 16; ++i, cp += 16 )
		{
		(void) fprintf( stderr,
	"%3d %3d %3d %3d %3d %3d %3d %3d %3d %3d %3d %3d %3d %3d %3d %3d\n",
	/* 1   2   3   4   5   6   7   8   9  10  11  12  13  14  15  16 */
				cp[0],
				cp[1],
				cp[2],
				cp[3],
				cp[4],
				cp[5],
				cp[6],
				cp[7],
				cp[8],
				cp[9],
				cp[10],
				cp[11],
				cp[12],
				cp[13],
				cp[14],
				cp[15]
				);
		}
	(void) fprintf( stderr, "Green segment :\n" );
	for( i = 0, cp = cmap->cm_green; i < 16; ++i, cp += 16 )
		{
		(void) fprintf( stderr, 
	"%3d %3d %3d %3d %3d %3d %3d %3d %3d %3d %3d %3d %3d %3d %3d %3d\n",
	/* 1   2   3   4   5   6   7   8   9  10  11  12  13  14  15  16 */
				cp[0],
				cp[1],
				cp[2],
				cp[3],
				cp[4],
				cp[5],
				cp[6],
				cp[7],
				cp[8],
				cp[9],
				cp[10],
				cp[11],
				cp[12],
				cp[13],
				cp[14],
				cp[15]
				);
		}
	(void) fprintf( stderr, "Blue segment :\n" );
	for( i = 0, cp = cmap->cm_blue; i < 16; ++i, cp += 16 )
		{
		(void) fprintf( stderr,
	"%3d %3d %3d %3d %3d %3d %3d %3d %3d %3d %3d %3d %3d %3d %3d %3d\n",
	/* 1   2   3   4   5   6   7   8   9  10  11  12  13  14  15  16 */
				cp[0],
				cp[1],
				cp[2],
				cp[3],
				cp[4],
				cp[5],
				cp[6],
				cp[7],
				cp[8],
				cp[9],
				cp[10],
				cp[11],
				cp[12],
				cp[13],
				cp[14],
				cp[15]
				);
		}
	return;
	}
