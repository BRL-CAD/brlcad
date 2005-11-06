/* tifftorle.c */
/* main program for converting TIFF files to Utah Raster Toolkit RLE files.
 * by Bailey Brown, Jr.  21 June 1990
 * modified:
 *	by David R. L. Worthington, SRI International,  29 April 1991
 *	to handle TIFF files with only one channel and a possible colormap.
 */
 
 
#define NO_DECLARE_MALLOC	/* tiffcompat.h declares it. */
#include "rle.h"
#undef TIFF			/* Defined by rle_config.h. */
#ifndef USE_STDARG
#define USE_VARARGS 1		/* Needed by tiffcompat.h. */
#endif
#ifdef USE_PROTOTYPES
#undef USE_PROTOTYPES
#define USE_PROTOTYPES 1
#endif

#include <stdlib.h>
#include <math.h>
#include <stdio.h>
#include "tiffio.h"

#ifdef USE_PROTOTYPES
void error(CONST_DECL char *s);
#else
void error();
#endif
void get_scanlines();

static rle_hdr the_hdr;
TIFF *tif;
unsigned char	*tiffbuf;
/* needed to read all scanlines before converting
        to tiff (rle is upside down) */
static rle_pixel **scan_red;
static rle_pixel **scan_green;
static rle_pixel **scan_blue;
 
int flip = 0;
unsigned short samplesperpixel = 1;
unsigned short bitspersample = 8;	/* Ok to assume 8? */
unsigned char bitconvert[16];	/* Convert 1, 2, or 4-bit samples to 8-bit  */
unsigned char bc4[16] = { 0, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77,
			  0x88, 0x99, 0xaa, 0xbb, 0xcc, 0xee, 0xff };
unsigned char bc2[4] = { 0, 0x55, 0xaa, 0xff };
unsigned char bc1[2] = { 0, 0xff };
 
void
main(argc, argv)
int argc;
char *argv[];
{
#ifndef TIFF2p4
        unsigned short imagelength, imagewidth;
#else /* TIFF2p4 */
        unsigned long imagelength, imagewidth;
#endif /* TIFF2p4 */
	unsigned short photometric, shortval;
	unsigned short planarconfig, matteing;
        int row, i;
        rle_pixel *rows[4];
	char *infname = NULL, *outfname = NULL;
 
	the_hdr = *rle_hdr_init( (rle_hdr *)NULL );
	if ( scanargs( argc, argv, "% o%-outfile.rle!s infile.tiff!s\n(\
\tConvert TIFF image to URT.)",
		       &i, &outfname, &infname ) == 0 )
	    exit( 1 );

	rle_names( &the_hdr, cmd_name( argv ), outfname, 0 );

        tif = TIFFOpen(infname, "rb");
        if (!tif) error("can't open input file");
        the_hdr.rle_file = rle_open_f(cmd_name(argv), outfname, "w");

	if (TIFFGetField(tif, TIFFTAG_IMAGEWIDTH, &imagewidth) == 0)
	    error("TIFFGetField TIFFTAG_IMAGEWIDTH failed!");
	if (TIFFGetField(tif, TIFFTAG_IMAGELENGTH, &imagelength) == 0)
 	    error("TIFFGetField TIFFTAG_IMAGELENGTH failed!");
	if (TIFFGetField(tif, TIFFTAG_SAMPLESPERPIXEL, &samplesperpixel) == 0) 
	    error("TIFFGetField TIFFTAG_SAMPLESPERPIXEL failed!");
	if (TIFFGetField(tif, TIFFTAG_BITSPERSAMPLE, &bitspersample) == 0) 
	    error("TIFFGetField TIFFTAG_BITSPERSAMPLE failed!");
        if (TIFFGetField(tif, TIFFTAG_PHOTOMETRIC, &photometric) == 0)
	    error("TIFFGetField TIFFTAG_PHOTOMETRIC failed!");
	switch ( bitspersample )
	{
	case 4:
	    for ( i = 0; i < 16; i++ )
		if ( photometric == PHOTOMETRIC_RGB )
		    bitconvert[i] = bc4[i];
		else
		    bitconvert[i] = i;
	    break;
	case 2:
	    for ( i = 0; i < 4; i++ )
		if ( photometric == PHOTOMETRIC_RGB )
		    bitconvert[i] = bc2[i];
		else
		    bitconvert[i] = i;
	    break;
	case 1:
	    if ( photometric == PHOTOMETRIC_RGB )
	    {
		bitconvert[0] = bc1[0];
		bitconvert[1] = bc1[1];
	    }
	    else
	    {
		bitconvert[0] = 0;
		bitconvert[1] = 1;
	    }
	    break;
	case 8:
	    break;
	default:
	    error("Bits per sample not 1, 2, 4, or 8");
	    break;
	}
	if (TIFFGetField(tif, TIFFTAG_PLANARCONFIG, &planarconfig ) == 0)
	    error("TIFFGetField TIFFTAG_PLANARCONFIG failed!");
	/* Might not be present. */
	(void)TIFFGetField(tif, TIFFTAG_MATTEING, &matteing );
	    
	switch ( photometric ) {
	case PHOTOMETRIC_MINISWHITE:
	{
	    int size =  1 << bitspersample;
	    if ( planarconfig == PLANARCONFIG_SEPARATE && matteing )
		samplesperpixel = 1;
	    the_hdr.ncolors =  1;
	    the_hdr.ncmap = 1;
	    the_hdr.cmaplen = bitspersample;
	    the_hdr.cmap = (unsigned short *)malloc((unsigned)size*sizeof(short));
	    for (i=0; i<size; i++)
		the_hdr.cmap[i] = 65535L - ((long)i * 65535L)/(size - 1);
	    break;
	}
	case PHOTOMETRIC_MINISBLACK:
    	{
	    int size =  1 << bitspersample;
	    if ( planarconfig == PLANARCONFIG_SEPARATE && matteing )
		samplesperpixel = 1;
	    the_hdr.ncolors =  1;
	    the_hdr.ncmap = 1;
	    the_hdr.cmaplen = bitspersample;
	    the_hdr.cmap = (unsigned short *)malloc((unsigned)size*sizeof(short));
	    for (i=0; i<size; i++)
		the_hdr.cmap[i] = ((long)i * 65535L)/(size - 1);
	    break;
	}
	case PHOTOMETRIC_RGB:
	    if (planarconfig != PLANARCONFIG_CONTIG)
		error("can't handle separate planar config");

	    the_hdr.ncolors =  3;
	    the_hdr.ncmap = 0;
	    the_hdr.cmaplen = 0;
	    the_hdr.cmap = 0;
	    break;
	case PHOTOMETRIC_PALETTE:	/* Handle Colormap */
    	{
	    unsigned short	*red, *green, *blue;
	    int		size;

	    size = 1 << bitspersample;
	    if ( planarconfig == PLANARCONFIG_SEPARATE && matteing )
		samplesperpixel = 1;
	    if (TIFFGetField(tif, TIFFTAG_COLORMAP, &red, &green, &blue)) {
		the_hdr.ncolors =  1;
		the_hdr.ncmap = 3;
		the_hdr.cmaplen = bitspersample;
		the_hdr.cmap = (unsigned short *)malloc((unsigned)
							(3*size*sizeof(short)) );
		for (i=0; i<size; i++) {
		    the_hdr.cmap[i] = red[i];
		    the_hdr.cmap[i + size] = green[i];
		    the_hdr.cmap[i + 2*size] = blue[i];
		}
	    } else error("TIFFGetField TIFFTAG_COLORMAP failed!");
	    break;
	}
	default:
	    error("tiff file PhotoMetric Interpretation not recognized");
	}
	RLE_SET_BIT(the_hdr, RLE_RED);
	if (the_hdr.ncolors == 3) {
	    RLE_SET_BIT(the_hdr, RLE_GREEN);
	    RLE_SET_BIT(the_hdr, RLE_BLUE);
	}

        the_hdr.bg_color = NULL;
        the_hdr.alpha = 0;
        the_hdr.background = 0;
        the_hdr.xmin = 0;
        the_hdr.xmax = imagewidth - 1;
        the_hdr.ymin = 0;
        the_hdr.ymax = imagelength - 1;
 
	rle_addhist( argv, (rle_hdr *)NULL, &the_hdr );
        rle_put_setup( &the_hdr );
 
        tiffbuf = (unsigned char*)malloc((unsigned)TIFFScanlineSize(tif));
        if (!tiffbuf) error("can't allocate tiff scanline buffer");

	get_scanlines();

        rows[0] = NULL;
        for ( row = 0; row < the_hdr.ymax + 1; row++) {
	    rows[1] = scan_red[row];
	    if (the_hdr.ncolors == 3) {
		rows[2] = scan_green[row];
		rows[3] = scan_blue[row];
	    }
	    rle_putrow(rows+1, the_hdr.xmax+1, &the_hdr);
        }
}

void 
get_scanlines()
{
    int i,j,k,l,n;
 
    scan_red = (rle_pixel**)malloc(sizeof(rle_pixel*)*(the_hdr.ymax+1));
    if (!scan_red)
        error("can't allocate a scan buffer");
    if (the_hdr.ncolors == 3) {
	scan_green = (rle_pixel**)malloc(sizeof(rle_pixel*)*(the_hdr.ymax+1));
	scan_blue = (rle_pixel**)malloc(sizeof(rle_pixel*)*(the_hdr.ymax+1));
	if (!(scan_green && scan_blue))
	    error("can't allocate a scan buffer");
    }
    for (i = 0; i < the_hdr.ymax+1; i++) {
	if ( !flip )
	    n = the_hdr.ymax - i;
	else
	    n = i;
	TIFFReadScanline(tif, tiffbuf, i, 0);
	scan_red[n] = (unsigned char*)malloc(the_hdr.xmax+1);
	if (!scan_red[n])
	    error("can't allocate scan buffer");
	if (the_hdr.ncolors == 3) {
	    scan_green[n] = (unsigned char*)malloc(the_hdr.xmax+1);
	    scan_blue[n] = (unsigned char*)malloc(the_hdr.xmax+1);
	    if (!(scan_green[n] && scan_blue[n]))
		error("can't allocate scan buffer");
	}
	if ( bitspersample == 8 )
	{
	    for (k = j = 0; j < the_hdr.xmax+1; k+= samplesperpixel, j++) {
		scan_red[n][j] = tiffbuf[k];
		if (the_hdr.ncolors == 3) {
		    scan_green[n][j] = tiffbuf[k+1];
		    scan_blue[n][j] = tiffbuf[k+2];
		}
	    }
	}
	else
	{
	    unsigned char mask = 0xff >> (8 - bitspersample);
	    int phase = 8 / bitspersample, shift = 8 - bitspersample;
	    unsigned char pix;

	    for ( k = j = 0; j <= the_hdr.xmax; j++ )
	    {
#define getpixel \
		pix = bitconvert[(tiffbuf[k] >> shift) & mask]; \
		if ( --phase == 0 ) { \
		    phase = 8 / bitspersample; \
		    shift = 8 - bitspersample; \
		    k++; \
		} \
		else \
		    shift -= bitspersample;
		getpixel;
		scan_red[n][j] = pix;
		if ( the_hdr.ncolors == 3 )
		{
		    getpixel;
		    scan_green[n][j] = pix;
		    getpixel;
		    scan_blue[n][j] = pix;
		}
		/* Skip any other channels (e.g., alpha). */
		for ( l = samplesperpixel - the_hdr.ncolors; l > 0; l-- )
		{
		    getpixel;
		}
#undef getpixel
	    }
	}
    }
}
 
void 
error(s)
CONST_DECL char *s;
{
    fprintf(stderr,"%s: %s in %s\n", the_hdr.cmd, s,
	    TIFFFileName(tif));
    exit(2);
}

