/*
 *			F B L A B E L . C
 *
 *  Function -
 *	Draw a Label on a Frame buffer image.
 *
 *  Author -
 *	Paul Randal Stay
 * 
 *  Source -
 * 	SECAD/VLD Computing Consortium, Bldg 394
 *	The U.S. Army Ballistic Research Laboratory
 * 	Aberdeen Proving Ground, Maryland 21005
 *
 *  Copyright Notice -
 *	This software is Copyright (C) 1986 by the United States Army.
 *	All rights reserved.
 */


#include <stdio.h>
#include <ctype.h>
#include "fb.h"

#if defined(vax) || defined(pdp11)
#	define	LITTLEENDIAN	1
#else
#	define BIGENDIAN	1
#endif

#if defined(BIGENDIAN)
#define SWAB(shrt)	(shrt=(((shrt)>>8) & 0xff) | (((shrt)<<8) & 0xff00))
#define SWABV(shrt)	((((shrt)>>8) & 0xff) | (((shrt)<<8) & 0xff00))
#else
#define	SWAB(shrt)
#define SWABV(shrt)	(shrt)
#endif

/* The char fields in struct dispatch are signed.  Extend sign */
#define	SXT(c)		((c)|((c&0x80)?(~0xFF):0))

struct header {
	short		magic;
	unsigned short	size;
	short		maxx;
	short		maxy;
	short		xtend;
}; 
struct dispatch
	{
	unsigned short	addr;
	short		nbytes;
	char		up, down, left, right;
	short		width;
	};

#define FONTBUFSZ 200
#define FONTDIR		"/usr/lib/vfont"	/* Font directory.	*/
#define FONTNAME	"nonie.r.12"		/* Default font name.	*/
#define FONTNAMESZ	128

/* Variables controlling the font itself.				*/
extern FILE *ffdes;		/* Fontfile file descriptor.		*/
extern int offset;		/* Offset to data in the file.		*/
extern struct header hdr;	/* Header for font file.		*/
extern struct dispatch dir[256];/* Directory for character font.	*/
extern int width, height;	/* Width and height of current char.	*/

extern int	getopt();
extern char	*optarg;
extern int	optind;

#define MAX_LINE	2048		/* Max pixels/line */
static RGBpixel scanline[MAX_LINE];	/* 1 scanline pixel buffer */
static int scanbytes;			/* # of bytes of scanline */

static char	*framebuffer = NULL;
static char	*font1 = NULL;

FBIO *fbp;

static char usage[] = "\
Usage: fblabel [-h -c ] [-F framebuffer]\n\
	[-f fontstring] [-r red] [-g green] [-b blue] xpos ypos textstring\n";

/* Variables controlling the font itself */
FILE		*ffdes;		/* File pointer for current font.	*/
int		offset;		/* Current offset to character data.	*/
struct header	hdr;		/* Header for font file.		*/
struct dispatch	dir[256];	/* Directory for character font.	*/
int		width = 0,	/* Size of current character.		*/
height = 0;

static int		filterbuf[FONTBUFSZ][FONTBUFSZ];

static int	file_width = 512;	/* default input width */
static int	file_height = 512;	/* default input height */
static int clear = 0;

static RGBpixel pixcolor;
static int xpos;
static int ypos;
char * textstring;
static int	debug;

void	do_char(), do_line(), squash(), fill_buf(), clear_buf();

get_args( argc, argv )
register char **argv;
{

	register int c;

	pixcolor[RED]  = 255;
	pixcolor[GRN]  = 255;
	pixcolor[BLU]  = 255;

	while ( (c = getopt( argc, argv, "dhcF:f:r:g:b:" )) != EOF )  {
		switch( c )  {
		case 'd':
			debug = 1;
			break;
		case 'h':
			/* high-res */
			file_height = file_width = 1024;
			break;
		case 'c':
			clear = 1;
			break;
		case 'F':
			framebuffer = optarg;
			break;
		case 'f':
			font1 = optarg;
			break;
		case 'r':
		        pixcolor[RED] = atoi( optarg );
			break;
		case 'g':
		        pixcolor[GRN] = atoi( optarg );
			break;
		case 'b':
		        pixcolor[BLU] = atoi( optarg );
			break;
		default:		/* '?' */
			return(0);
		}
	}

	if( optind+3 > argc )
		return(0);
	xpos = atoi( argv[optind++]);
	ypos = atoi( argv[optind++]);
	textstring = argv[optind++];
	if(debug) (void)fprintf(stderr,"fblabel %d %d %s\n", xpos, ypos, textstring);

	if ( argc > optind )
		(void)fprintf( stderr, "fblabel: excess argument(s) ignored\n" );

	return(1);		/* OK */
}



main(argc, argv)
int argc;
char **argv;
{

	if ( !get_args( argc, argv ) ) {
		fputs( usage, stderr);
		exit(1);
	}

	if( (fbp = fb_open( framebuffer, file_width, file_height )) == NULL )
		exit(12);

	if( clear ) {
		fb_clear( fbp, PIXEL_NULL);
		fb_wmap( fbp, COLORMAP_NULL );
	}

	if( get_font(font1) == 0 )  {
		fprintf(stderr, "fblabel:  Can't get font\n");
		exit(1);
	}

	do_line( textstring);

	fb_close( fbp );
	exit(0);
}

void
do_line( line )
register char	*line;
{
	register int    currx;
	register int    char_count, char_id;
	register int	len = strlen( line );
	if( ffdes == NULL )
	{
		fprintf(stderr,
		     "ERROR: do_line() called before get_Font().\n" );
		return;
	}
	currx = xpos;

	for( char_count = 0; char_count < len; char_count++ )  {
		char_id = (int) line[char_count] & 0377;

		/* Obtain the dimensions for the character */
		width = SXT(dir[char_id].right) + SXT(dir[char_id].left);
		height = SXT(dir[char_id].up) + SXT(dir[char_id].down);
		if(debug) fprintf(stderr,"%c w=%2d h=%2d, currx=%d\n", char_id, width, height, currx);

		/*
		 *  Space characters are frequently not represented
		 *  in the font set, so leave white space here.
		 */
	 	if( width <= 1 )  {
	 		char_id = 'n';	/* 1-en space */
			width = SXT(dir[char_id].right) + SXT(dir[char_id].left);
	 		if( width <= 1 )  {
		 		char_id = 'N';	/* 1-en space */
				width = SXT(dir[char_id].right) +
					SXT(dir[char_id].left);
	 			if( width <= 1 )
	 				width = 16;	/* punt */
	 		}
	 		currx += width;
	 		continue;
	 	}

		/* locate the bitmap for the character in the file */
		if( fseek( ffdes, (long)(SWABV(dir[char_id].addr)+offset), 0 )
		     == EOF
		     )
		{
			 fprintf(stderr,  "fseek() to %ld failed.\n",
			 (long)(SWABV(dir[char_id].addr) + offset)
			     );
			 return;
		}

		if( currx + width > fb_getwidth(fbp) - 1 )  {
			fprintf(stderr,"fblabel:  Ran off screen\n");
			break;
		}

		do_char( char_id, currx, ypos, SXT(dir[char_id].down)%2 );
		currx += SWABV(dir[char_id].width) + 2;
	 }
	 return;
}

void
do_char( c, x, y, odd )
int c;
int x, y, odd;
{	
	 register int    i, j;
	 int		base;
	 int     	totwid = width;
	 int     	up, down;
	 static float	resbuf[FONTBUFSZ];
	 static RGBpixel	fbline[FONTBUFSZ];

	 /* Read in the character bit map, with two blank lines on each end. */
	 for (i = 0; i < 2; i++)
		 clear_buf (totwid, filterbuf[i]);
	 for (i = height + 1; i >= 2; i--)
		 fill_buf (width, filterbuf[i]);
	 for (i = height + 2; i < height + 4; i++)
		 clear_buf (totwid, filterbuf[i]);

	 up = SXT(dir[c].up);
	 down = SXT(dir[c].down);

	 /* Initial base line for filtering depends on odd flag. */
	 base = (odd ? 1 : 2);

	 /* Produce a RGBpixel buffer from a description of the character and
	  * the read back data from the frame buffer for anti-aliasing.
	  */

	 for (i = height + base; i >= base; i--)
	 {
		 squash(	filterbuf[i - 1],	/* filter info */
			 filterbuf[i],
			 filterbuf[i + 1],
			 resbuf,
			 totwid + 4
		     );
		 fb_read( fbp, x, y - down + i, fbline, totwid+3);
		 for (j = 0; j < (totwid + 3) - 1; j++)
		 {	
			 register int	tmp;
			 /* EDITOR'S NOTE : do not rearrange this code,
			  * the SUN compiler can't handle more
			  * complex expressions.
			  */

			 tmp = fbline[j][RED] & 0377;
			 fbline[j][RED] =
			     (int)(pixcolor[RED]*resbuf[j]+(1-resbuf[j])*tmp);
			 fbline[j][RED] &= 0377;
			 tmp = fbline[j][GRN] & 0377;
			 fbline[j][GRN] =
			     (int)(pixcolor[GRN]*resbuf[j]+(1-resbuf[j])*tmp);
			 fbline[j][GRN] &= 0377;
			 tmp = fbline[j][BLU] & 0377;
			 fbline[j][BLU] =
			     (int)(pixcolor[BLU]*resbuf[j]+(1-resbuf[j])*tmp);
			 fbline[j][BLU] &= 0377;
		 }
		 fb_write( fbp, x, y - down + i, fbline,  totwid+3 );
	 }
	 return;
}


 /*	b i t x ( )
	 Extract a bit field from a bit string.
  */

bitx( bitstring, posn )
register char *bitstring;
register int posn;
{
	for (; posn >= 8; posn -= 8, bitstring++);
#if defined( CANT_DO_ZERO_SHIFT )
	if (posn == 0)
		return (int) (*bitstring) & 1;
	else
#endif
		return (int) (*bitstring) & (1 << posn);
}

get_font( fontname )
char *fontname;
{	
	FILE		*newff;
	struct header	lochdr;
	static char	fname[FONTNAMESZ];
	if( fontname == NULL )
		fontname = FONTNAME;
	if( fontname[0] != '/' )		/* absolute path */
		(void) sprintf( fname, "%s/%s", FONTDIR, fontname );
	else
		(void) strncpy( fname, fontname, FONTNAMESZ );

	/* Open the file and read in the header information. */
	if( (newff = fopen( fname, "r" )) == NULL )
	{
		fb_log( "Error opening font file '%s'\n", fname );
		ffdes = NULL;
		return	0;
	}
	if( ffdes != NULL )
		(void) fclose(ffdes);
	ffdes = newff;
	if( fread( (char *) &lochdr, (int) sizeof(struct header), 1, ffdes ) != 1 )
	{
		fb_log( "get_Font() read failed!\n" );
		ffdes = NULL;
		return	0;
	}
	SWAB( lochdr.magic );
	SWAB( lochdr.size );
	SWAB( lochdr.maxx );
	SWAB( lochdr.maxy );
	SWAB( lochdr.xtend );

	if( lochdr.magic != 0436 )
	{
		fb_log( "Not a font file \"%s\": magic=0%o\n",
		fname, (int) lochdr.magic
		    );
		ffdes = NULL;
		return	0;
	}
	hdr = lochdr;

	/* Read in the directory for the font. */
	if( fread( (char *) dir, (int) sizeof(struct dispatch), 256, ffdes ) != 256 )
	{
		fb_log( "get_Font() read failed!\n" );
		ffdes = NULL;
		return	0;
	}
	/* Addresses of characters in the file are relative to
			point in the file after the directory, so grab the
			current position.
		 */
	offset = ftell( ffdes );
	return	1;
}

/* 
 * squash - Filter super-sampled image for one scan line
 */

/* Cone filtering weights. 
 * #define CNTR_WT 0.23971778
 * #define MID_WT  0.11985889
 * #define CRNR_WT 0.07021166
 */

/* Gaussian filtering weights. */
#define CNTR_WT 0.3011592441
#define MID_WT 0.1238102667
#define CRNR_WT 0.0508999223

/*	Squash takes three super-sampled "bit arrays", and returns an array
	of intensities at half the resolution.  N is the size of the bit
	arrays.  The "bit arrays" are actually int arrays whose values are
	assumed to be only 0 or 1.
 */
void
squash( buf0, buf1, buf2, ret_buf, n )
register int	*buf0, *buf1, *buf2;	
register float	*ret_buf;
register int	n;
{
	register int    j;

	for (j = 1; j < n - 1; j++) {
		ret_buf[j] =
			(
			 buf2[j - 1] * CRNR_WT +
			 buf2[j] * MID_WT +
			 buf2[j + 1] * CRNR_WT +
			 buf1[j - 1] * MID_WT +
			 buf1[j] * CNTR_WT +
			 buf1[j + 1] * MID_WT +
			 buf0[j - 1] * CRNR_WT +
			 buf0[j] * MID_WT +
			 buf0[j + 1] * CRNR_WT
			);
	}
	return;
}

/*	f i l l _ b u f ( )
	Fills in the buffer by reading a row of a bitmap from the
	character font file.  The file pointer is assumed to be in the
	correct position.
 */
void
fill_buf( wid, buf )
register int	wid;
register int	*buf;
{
	char            bitrow[FONTBUFSZ];
	register int    j;

	if (ffdes == NULL)
		return;
	/* Read the row, rounding width up to nearest byte value. */
	if (fread(bitrow, (wid / 8) + ((wid % 8 == 0) ? 0 : 1), 1, ffdes)
	    < 1
		) {
		(void) fprintf(stderr, "fill_buf() read failed!\n");
		return;
	}
	/*
	 * For each bit in the row, set the array value to 1 if it's on. The
	 * bitx routine extracts the bit value.  Can't just use the j-th bit
	 * because the bytes are backwards. 
	 */
	for (j = 0; j < wid; j++)
		if (bitx(bitrow, (j & ~7) + (7 - (j & 7))))
			buf[j + 2] = 1;
		else
			buf[j + 2] = 0;

	/*
	 * Need two samples worth of background on either end to make the
	 * filtering come out right without special casing the filtering. 
	 */
	buf[0] = buf[1] = buf[wid + 2] = buf[wid + 3] = 0;
	return;
}

/*	c l e a r _ b u f ( )
	Just sets all the buffer values to zero (this is used to
	"read" background areas of the character needed for filtering near
	the edges of the character definition.
 */
void
clear_buf( wid, buf )
int		wid;
register int	*buf;
{
	register int    i, w = wid + 4;

	/* Clear each value in the row.					 */
	for (i = 0; i < w; i++)
		buf[i] = 0;
	return;
}
