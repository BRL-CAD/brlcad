/*
	SCCS id:	@(#) fb-rle.c	1.1
	Last edit: 	3/21/85 at 14:00:59
	Retrieved: 	8/13/86 at 03:10:28
	SCCS archive:	/m/cad/fb_utils/RCS/s.fb-rle.c

	Author:		Gary S. Moss
			U. S. Army Ballistic Research Laboratory
			Aberdeen Proving Ground
			Maryland 21005
			(301)278-6647 or AV-283-6647
 */
#if ! defined( lint )
static
char	sccsTag[] = "@(#) fb-rle.c	1.1	last edit 3/21/85 at 14:00:59";
#endif
#include <stdio.h>
#include <fb.h>

static char	*usage[] = {
"",
"_fb-rle (1.1)",
"",
"Usage: _fb-rle [-BCdhvw]",
"",
0
};
static Pixel	bgpixel = { 0, 0, 0, 0 };
int		debug = 0;
int		verbose = 0;
static int	bgflag = 2;
static int	bwflag = 0;
static int	cmflag = 1;
static int	crunch = 0;
static int	bbw = 0;		/* black/white background color */
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
	setbuf( stdout, malloc( BUFSIZ ) );
	fbsz = getfbsize();
	if( verbose )
		(void) fprintf( stderr,
				"Background is %d %d %d\n",
				bgpixel.red, bgpixel.green, bgpixel.blue
				);
	if( bwflag )
		bbw =	0.35 * bgpixel.red +
			0.55 * bgpixel.green +
			0.10 * bgpixel.blue;

	if(	rle_whdr( stdout, bwflag, bgflag, cmflag, bbw, &bgpixel )
	    ==	-1
		)
		return	1;

	if( fbopen( NULL, APPEND ) == -1 )
		return	1;

	if( cmflag )
		{
		if(	fb_rmap( &cmap ) == -1
		     ||	rle_wmap( stdout, &cmap ) == -1
			)
			return	1;
		}

	for( scan_ln = fbsz-1; scan_ln >= 0; --scan_ln )
		{
		if( fbread( 0, scan_ln, scan_buf, fbsz ) == -1 )
			return	1;
		if( rle_encode_ln( stdout, scan_buf ) == -1 )
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

	/* Parse options.					*/
	while( (c = getopt( argc, argv, "BCdhvw" )) != EOF )
		{
		switch( c )
			{
			case 'B' :
				bgflag = 2;
				break;
			case 'C' :
				crunch = 1;
				cmflag = 0;
				break;
			case 'd' :
				debug = 1;
				break;
			case 'h' :
				setfbsize( 1024 );
				break;
			case 'v' :
				verbose = 1;
				break;
			case 'w' :
				bwflag = 1;
				break;
			case '?' :
				return	0;
			}
		}
	if( argc != optind )
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
