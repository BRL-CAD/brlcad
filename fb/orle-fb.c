/*
	SCCS id:	@(#) rle-fb.c	1.1
	Last edit: 	3/21/85 at 14:01:17
	Retrieved: 	8/13/86 at 03:16:39
	SCCS archive:	/m/cad/fb_utils/RCS/s.rle-fb.c

	Author:		Gary S. Moss
			U. S. Army Ballistic Research Laboratory
			Aberdeen Proving Ground
			Maryland 21005
			(301)278-6647 or AV-283-6647
 */
#if ! defined( lint )
static
char	sccsTag[] = "@(#) rle-fb.c	1.1	last edit 3/21/85 at 14:01:17";
#endif
#include <stdio.h>
#include <fb.h>

static char	*usage[] = {
"",
"_rle-fb (1.1)",
"",
"Usage: _rle-fb [-Odhv][-b (rgbBG)][-pi X Y]",
"",
0
};
/* Only for debugging library.						*/
int		debug = 0;
int		verbose = 0;

static Pixel	bgpixel = { 0, 0, 0, 0 };
static int	bgflag = 0;
static int	bwflag = 0;
static int	cmflag = 0;
static int	olflag = 0;
static int	fbsz;
static int	pars_Argv();
static void	prnt_Usage();

/*	m a i n ( )							*/
main( argc, argv )
int	argc;
char	*argv[];
	{
	Pixel		scan_buf[1024];
	ColorMap	cmap;
	register int	scan_ln;
	if( ! pars_Argv( argc, argv ) )
		{
		prnt_Usage();
		return	1;
		}
	if(	rle_rhdr( stdin, bgflag, &bwflag, &cmflag, olflag, &bgpixel )
	    ==	-1
		)
		return	1;

	fbsz = getfbsize();
	if( fbopen( NULL, APPEND ) == -1 )
		return	1;

	if( verbose )
		(void) fprintf( stderr,
				"Background is %d %d %d\n",
				bgpixel.red, bgpixel.green, bgpixel.blue
				);

	/* If color map provided, use it, else go with standard map. */
	if( cmflag )
		{
		if( verbose && ! olflag )
			(void) fprintf( stderr,
					"Loading saved color map from file\n"
					);
		if( rle_rmap( stdin, &cmap ) == -1 )
			return	1;
		if( ! olflag )
			{
			/* User-supplied map */
			if( fb_wmap( &cmap ) == -1 )
				return	1;
			}
		}
	else
	if( ! olflag )
		{
		if( verbose )
			(void) fprintf( stderr,
					"Creating standard color map\n"
					);
		if( fb_wmap( (ColorMap *) NULL ) == -1 )
			return	1;
		}
	for( scan_ln = fbsz-1; scan_ln >= 0; --scan_ln )
		{
		if( rle_decode_ln( stdin, scan_buf ) == -1 )
			return	1;
		if( fbwrite( 0, scan_ln, scan_buf, fbsz ) == -1 )
			return	1;
		}
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
	while( (c = getopt( argc, argv, "Ob:dhvp:i:" )) != EOF )
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
			}
		case 'd' :
			debug = 1;
			break;
		case 'h' : /* High resolution mode.		*/
			setfbsize( 1024 );
			break;
		case 'v' :
			verbose = 1;
			break;
		case 'p' :
			(void) fprintf( stderr,
					"-p option not implemented!\n"
					);
			break;
		case 'i' :
			(void) fprintf( stderr,
					"-i option not implemented!\n"
					);
			break;
		case '?' :
			return	0;
		} /* end switch */
		} /* end while */
	if( argc != optind )
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
