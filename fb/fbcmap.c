/*
	SCCS id:	@(#) fbcmap.c	1.6
	Last edit: 	3/28/85 at 15:44:52	G S M
	Retrieved: 	8/13/86 at 03:13:13
	SCCS archive:	/m/cad/fb_utils/RCS/s.fbcmap.c

*/
#if ! defined( lint )
static
char	sccsTag[] = "@(#) fbcmap.c	1.6	last edit 3/28/85 at 15:44:52";
#endif
/*
	F B C M A P

	Mike Muuss, 7/17/82
	VAX version 10/18/83

	Conversion to generic frame buffer utility using libfb(3).
	In the process, the name has been changed to fbclear from ikclear.
	Gary S. Moss, BRL. 03/12/85

	Compile:	make fbcmap

	Usage:	fbcmap [-h] [flavor]

	When invoked with no arguments, or with a flavor of 0,
	the standard ramp color-map is written.
	Other flavors provide interesting alternatives.
 */
#include <stdio.h>
#include <fb.h>
static ColorMap cmap;
static int	flavor = 0;

main(argc, argv)
char *argv[];
{
	register int		i;
	register int		fudge;
	register ColorMap	*cp = &cmap;

	if( ! pars_Argv( argc, argv ) )
		{
		(void) fprintf( stderr, "Usage : fbcmap	[-h] [(1-4)]\n" );
		return	1;
		}
	if( fbopen( NULL, APPEND ) == -1 )
		return	1;

	switch( flavor )  {
	case 0 : /* Standard - Linear color map.			*/
		(void) fprintf( stderr,
				"Color map #0, linear (standard).\n"
				);
		cp = (ColorMap *) NULL;
		break;
	case 1 : /* Reverse linear color map.				*/
		(void) fprintf( stderr,
				"Color map #1, reverse-linear.\n"
				);
		for( i = 0; i < 256; i++ )
			{
			cp->cm_red[255-i] =
			cp->cm_green[255-i] =
			cp->cm_blue[255-i] = i;
			}
		break;
	case 2 :
		/* Experimental correction, for POLAROID 8x10 print film */
		(void) fprintf( stderr,
			"Color map #2, corrected for POLAROID 809/891 film.\n"
				);
		/* First entry black.					*/
#define BOOST(point, bias) \
	((int)((bias)+((float)(point)/256.*(255-(bias)))))
		for( i = 1; i < 256; i++ )  {
			fudge = BOOST(i, 70);
			cp->cm_red[i] = fudge;		/* B */
		}
		for( i = 1; i < 256; i++ )  {
			fudge = i;
			cp->cm_green[i] = fudge;	/* G */
		}
		for( i = 1; i < 256; i++ )  {
			fudge = BOOST( i, 30 );
			cp->cm_blue[i] = fudge;	/* R */
		}
		break;
	case 3 : /* Standard, with low intensities set to black.	*/
		(void) fprintf( stderr,
				"Color map #3, low 100 entries black.\n"
				);
		for( i = 100; i < 256; i++ )  {
			cp->cm_red[i] =
			cp->cm_green[i] =
			cp->cm_blue[i] = i;
		}
		break;
	case 4 : /* Amplify middle of the range, for Moss's dim pictures */
#define UPSHIFT	64
		(void) fprintf( stderr,
	"Color map #4, amplify middle range to boost dim pictures.\n"
				);
		/* First entry black.					*/
		for( i = 1; i< 256-UPSHIFT; i++ )  {
			register int j = i + UPSHIFT;
			cp->cm_red[i] =
			cp->cm_green[i] =
			cp->cm_blue[i] = j;
		}
		for( i = 256-UPSHIFT; i < 256; i++ )  {
			cp->cm_red[i] =
			cp->cm_green[i] =
			cp->cm_blue[i] = 255;	/* Full Scale */
		}
		break;
	default:
		(void) fprintf(	stderr,
				"Color map #%d, flavor not implemented!\n",
				flavor
				);
		return	1;
	}
	return fb_wmap( cp ) == -1;
}

/*	p a r s _ A r g v ( )
 */
int
pars_Argv( argc, argv )
register char	**argv;
	{
	register int	c;
	extern int	optind;

	while( (c = getopt( argc, argv, "h" )) != EOF )
		{
		switch( c )
			{
			case 'h' : /* High resolution frame buffer.	*/
				setfbsize( 1024 );
				break;
			case '?' :
				return	0;
			}
		}
	if( argv[optind] != NULL )
		flavor = atoi( argv[optind] );
	return	1;
	}

