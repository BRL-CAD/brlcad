/*
	SCCS id:	@(#) fb-rle.c	1.3
	Last edit: 	3/26/85 at 17:45:44
	Retrieved: 	8/13/86 at 03:10:39
	SCCS archive:	/m/cad/fb_utils/RCS/s.fb-rle.c

	Author:		Gary S. Moss
			U. S. Army Ballistic Research Laboratory
			Aberdeen Proving Ground
			Maryland 21005
			(301)278-6647 or AV-283-6647
 */
#if ! defined( lint )
static
char	sccsTag[] = "@(#) fb-rle.c	1.3	last edit 3/26/85 at 17:45:44";
#endif
#include <stdio.h>
#include <fb.h>
#include <rle.h>
static char	*usage[] = {
"",
"fb-rle (1.3)",
"",
"Usage: fb-rle [-CScdhvw] [file.rle]",
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
static int	fbsz;
static int	parsArgv();
static void	prntUsage();

/*	m a i n ( )							*/
main( argc, argv )
int	argc;
char	*argv[];
	{
	Pixel		scan_buf[1024];
	ColorMap	cmap;
	register int	scan_ln;
	if( ! parsArgv( argc, argv ) )
		{
		prntUsage();
		return	1;
		}
	setbuf( fp, malloc( BUFSIZ ) );
	fbsz = getfbsize();
	if( rle_verbose )
		(void) fprintf( stderr,
				"Background is %d %d %d\n",
				bgpixel.red, bgpixel.green, bgpixel.blue
				);
	if(	rle_whdr( fp, ncolors, bgflag, cmflag, &bgpixel )
	    ==	-1
		)
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
	if( ncolors == 0 )
		{ /* Only save colormap, so we are finished.		*/
		return	0;
		}
	/* Save image.							*/
	for( scan_ln = fbsz-1; scan_ln >= 0; --scan_ln )
		{
		if( fbread( 0, scan_ln, scan_buf, fbsz ) == -1 )
			return	1;
		if( rle_encode_ln( fp, scan_buf ) == -1 )
			return	1;
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
	while( (c = getopt( argc, argv, "CScdhvw" )) != EOF )
		{
		switch( c )
			{
			case 'C' : /* Crunch color map.			*/
				crunch = 1;
				cmflag = 0;
				break;
			case 'S' : /* 'Box' save, store entire image.	*/
				bgflag = 0;
				break;
			case 'c' : /* Only save color map.		*/
				ncolors = 0;
				break;
			case 'd' : /* For debugging.			*/
				rle_debug = 1;
				break;
			case 'h' : /* High resolution.			*/
				setfbsize( 1024 );
				break;
			case 'v' : /* Verbose on.			*/
				rle_verbose = 1;
				break;
			case 'w' : /* Monochrome (black & white) mode.	*/
				ncolors = 1;
				break;
			case '?' :
				return	0;
			}
		}
	if( argv[optind] != NULL )
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
