/*
 *			P I X - R L E . C
 *
 *  Encode a .pix file using the Utah Raster Toolkit RLE library
 *
 *  Author -
 *	Michael John Muuss
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
#include <time.h>

#include "fb.h"
#include "svfb_global.h"

static struct sv_globals	outrle;
#define		outfp		outrle.svfb_fd
static char			comment[128];
static char			host[128];
static rle_pixel		**rows;
static long			now;
static char			*who;
extern char			*getenv();

extern char	*malloc();
extern char	*strdup();

static FILE	*infp = stdin;
static char	*infile;

static int	background[3];

static int	file_width = 512;
static int	file_height = 512;

static char	usage[] = "\
Usage: pix-rle [-h -d] [-s squarefilesize]  [-C r/g/b]\n\
	[-w file_width] [-n file_height] [file.pix] [file.rle]\n\
\n\
If omitted, the .pix file is taken from stdin\n\
and the .rle file is written to stdout\n";


/*
 *			G E T _ A R G S
 */
static int
get_args( argc, argv )
register char	**argv;
{
	register int	c;
	extern int	optind;
	extern char	*optarg;

	while( (c = getopt( argc, argv, "hs:w:n:C:" )) != EOF )  {
		switch( c )  {
		case 'h':
			/* high-res */
			file_height = file_width = 1024;
			break;
		case 's':
			/* square file size */
			file_height = file_width = atoi(optarg);
			break;
		case 'w':
			file_width = atoi(optarg);
			break;
		case 'n':
			file_height = atoi(optarg);
			break;
		case 'C':
			{
				register char *cp = optarg;
				register int *conp = background;

				/* premature null => atoi gives zeros */
				for( c=0; c < 3; c++ )  {
					*conp++ = atoi(cp);
					while( *cp && *cp++ != '/' ) ;
				}
			}
			break;
		default:
		case '?':
			return	0;
		}
	}
	if( argv[optind] != NULL )  {
		if( (infp = fopen( (infile=argv[optind]), "r" )) == NULL )  {
			perror(infile);
			return	0;
		}
		optind++;
	} else {
		infile = "-";
	}
	if( argv[optind] != NULL )  {
		if( access( argv[optind], 0 ) == 0 )  {
			(void) fprintf( stderr,
				"\"%s\" already exists.\n",
				argv[optind] );
			exit( 1 );
		}
		if( (outfp = fopen( argv[optind], "w" )) == NULL )  {
			perror(argv[optind]);
			return	0;
		}
	}
	if( argc > ++optind )
		(void) fprintf( stderr, "pix-rle: Excess arguments ignored\n" );

	if( isatty(fileno(infp)) || isatty(fileno(outfp)) )
		return 0;
	return	1;
}

/*
 *			M A I N
 */
main( argc, argv )
int	argc;
char	*argv[];
{
	register RGBpixel *scan_buf;
	register int	y;

	infp = stdin;
	outfp = stdout;
	if( !get_args( argc, argv ) )  {
		(void)fputs(usage, stderr);
		exit( 1 );
	}
	scan_buf = (RGBpixel *)malloc( sizeof(RGBpixel) * file_width );

	outrle.sv_ncolors = 3;
	SV_SET_BIT(outrle, SV_RED);
	SV_SET_BIT(outrle, SV_GREEN);
	SV_SET_BIT(outrle, SV_BLUE);
	outrle.sv_background = 2;		/* use background */
	outrle.sv_bg_color = background;
	outrle.sv_alpha = 0;		/* no alpha channel */
	outrle.sv_ncmap = 0;		/* no color map */
	outrle.sv_cmaplen = 0;
	outrle.sv_cmap = (rle_map *)0;
	outrle.sv_xmin = outrle.sv_ymin = 0;
	outrle.sv_xmax = file_width-1;
	outrle.sv_ymax = file_height-1;
	outrle.sv_comments = (char **)0;

	/* Add comments to the header file, since we have one */
	sprintf( comment, "converted_from=%s", infile );
	rle_putcom( strdup(comment), &outrle );
	now = time(0);
	sprintf( comment, "converted_date=%24.24s", ctime(&now) );
	rle_putcom( strdup(comment), &outrle );
	if( (who = getenv("USER")) != (char *)0 ) {
		sprintf( comment, "converted_by=%s", who);
		rle_putcom( strdup(comment), &outrle );
	} else {
		if( (who = getenv("LOGNAME")) != (char *)0 ) {
			sprintf( comment, "converted_by=%s", who);
			rle_putcom( strdup(comment), &outrle );
		}
	}
#	ifdef BSD
	gethostname( host, sizeof(host) );
	sprintf( comment, "converted_host=%s", host);
	rle_putcom( strdup(comment), &outrle );
#	endif

	sv_setup( RUN_DISPATCH, &outrle );
	rle_row_alloc( &outrle, &rows );

	/* Read image a scanline at a time, and encode it */
	for( y = 0; y < file_height; y++ )  {
		if( fread( (char *)scan_buf, sizeof(RGBpixel), file_width, infp ) != file_width)  {
			(void) fprintf(	stderr,
				"pix-rle: read of %d pixels on line %d failed!\n",
				file_width, y );
			exit(1);
		}

		/* Grumble, convert to Utah layout */
		{
			register unsigned char	*pp = (unsigned char *)scan_buf;
			register rle_pixel	*rp = rows[0];
			register rle_pixel	*gp = rows[1];
			register rle_pixel	*bp = rows[2];
			register int		i;

			for( i=0; i<file_width; i++ )  {
				*rp++ = *pp++;
				*gp++ = *pp++;
				*bp++ = *pp++;
			}
		}
		sv_putrow( rows, file_width, &outrle );
	}
	sv_puteof( &outrle );

	fclose( infp );
	fclose( outfp );
	exit(0);
}
