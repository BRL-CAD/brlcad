/* rletotiff.c */

/*************************************************************************
 * rletotiff -    Main program for RLE to TIFF graphics format conversion.
 *
 *              Written by Bailey Brown, Jr.  June 4, 1990.
 *************************************************************************
 */

#define NO_DECLARE_MALLOC	/* tiffcompat.h declares it. */
#include "rle.h"
#undef TIFF			/* Defined in rle_config.h and tiffio.h. */
#ifndef USE_STDARG
#define USE_VARARGS 1		/* Needed by tiffcompat.h. */
#endif
#ifdef USE_PROTOTYPES
#undef USE_PROTOTYPES
#define USE_PROTOTYPES 1	/* Needs to have a value for tiffcompat.h. */
#endif

#include <stdio.h>
#include "tiffio.h"

#define SETFIELD(tif, fieldname, arg)  TIFFSetField(tif,fieldname,arg)
typedef unsigned char tiff_sample;

#ifdef USE_PROTOTYPES
void error(CONST_DECL char *s);
void get_tiff_scanline( tiff_sample* buf, int row);
void usage(void);
#else
void error();
void get_tiff_scanline();
void usage();
#endif
void get_scanlines();

static rle_hdr in_hdr;
/* needed to read all scanlines before converting
	to tiff (rle is upside down) */
static rle_pixel **scan_red;
static rle_pixel **scan_green;
static rle_pixel **scan_blue;
static tiff_sample *RGBscanline;

TIFF *tif;

static unsigned short compression = COMPRESSION_LZW;
static int flip = 0;

void
main(argc, argv)
int argc;
char *argv[];
{
    int i;
    long int rows_per_strip;
    int cflag = 0;
    char *outfname = NULL, *infname = NULL;

    in_hdr = *rle_hdr_init( (rle_hdr *)NULL );

    if (sizeof(rle_pixel) != sizeof(tiff_sample))
        error("tiff_sample and rle_pixel not same size");

    if ( scanargs( argc, argv, "% Cc%- f%- o!-outfile.tiff!s infile.rle%s\n(\
\tConvert URT image to 24-bit TIFF image.\n\
\t-c\tUse LZW compression (default)\n\
\t-C\tUse no compression\n\
\t-f\tDon't flip image top to bottom\n\
\t-o outfile.tiff is required)",
		   &cflag, &flip, &i, &outfname, &infname ) == 0 )
	exit( 1 );

    if ( cflag == 1 )
	compression = COMPRESSION_LZW;
    else if ( cflag == 2 )
	compression = COMPRESSION_NONE;

    rle_names( &in_hdr, cmd_name( argv ), infname, 0 );
    in_hdr.rle_file = rle_open_f(in_hdr.cmd, infname, "r");
    rle_get_setup_ok( &in_hdr, in_hdr.cmd, infname );

    in_hdr.xmax -= in_hdr.xmin;
    in_hdr.xmin = 0;
    if (in_hdr.ncolors != 3) error("infile must be 24-bit RGB");
    /* No alpha channel, either. */
    RLE_CLR_BIT( in_hdr, RLE_ALPHA );
    get_scanlines();
/* rletotiff specific code comes after here */
    RGBscanline = (tiff_sample*)malloc(
            sizeof(tiff_sample)*3*(in_hdr.xmax+1));
    if (RGBscanline == NULL) error("can't allocate RGB scanline");
    tif = TIFFOpen(outfname, "w");
    if (!tif) error("tiffopen ret null");
#ifndef TIFF2p4
    SETFIELD(tif,TIFFTAG_IMAGEWIDTH, in_hdr.xmax + 1);
    SETFIELD(tif,TIFFTAG_IMAGELENGTH, in_hdr.ymax - in_hdr.ymin + 1);
#else /* TIFF2p4 */
    SETFIELD(tif,TIFFTAG_IMAGEWIDTH, (unsigned long)(in_hdr.xmax + 1));
    SETFIELD(tif,TIFFTAG_IMAGELENGTH,
	     (unsigned long)(in_hdr.ymax - in_hdr.ymin + 1));
#endif /* TIFF2p4 */
    SETFIELD(tif, TIFFTAG_COMPRESSION, compression);
    rows_per_strip = (long)8*1024/(in_hdr.xmax+1);
    if (rows_per_strip < 1) rows_per_strip = 1L;
    SETFIELD(tif, TIFFTAG_ROWSPERSTRIP, rows_per_strip);
    SETFIELD(tif, TIFFTAG_XRESOLUTION, 1.0);
    SETFIELD(tif, TIFFTAG_YRESOLUTION, 1.0);
    SETFIELD(tif, TIFFTAG_RESOLUTIONUNIT, 1);
    SETFIELD(tif, TIFFTAG_BITSPERSAMPLE, 8);
    SETFIELD(tif, TIFFTAG_SAMPLESPERPIXEL, 3);
    SETFIELD(tif, TIFFTAG_PLANARCONFIG, 1);
    SETFIELD(tif, TIFFTAG_PHOTOMETRIC, 2);
    for (i = 0; i < in_hdr.ymax - in_hdr.ymin + 1; i++) {
        get_tiff_scanline(RGBscanline, i);
        TIFFWriteScanline(tif, RGBscanline, i, 1);
    }
    TIFFClose(tif);
/* rletotiff specific code comes before here */
    for (i = 0; i < in_hdr.ymax - in_hdr.ymin + 1; i++ ) {
        free(scan_red[i]);
        free(scan_green[i]);
        free(scan_blue[i]);
    }
    free(scan_red);
    free(scan_green);
    free(scan_blue);
}

/*
 * get_scanlines() reads in all RLE raster data at once.  This is
 * necessary because of RLE stores images bottom to top and TIFF stores
 * them top to bottom.
 */
void
get_scanlines()
{
    int i,j;
    rle_pixel **scan;

    scan = (rle_pixel **)malloc( in_hdr.ncolors * sizeof(rle_pixel *) );
    if ( scan == 0 ) error("can't allocate scan");
    scan_red = (rle_pixel**)malloc(sizeof(rle_pixel*)*
				   (in_hdr.ymax-in_hdr.ymin+1));
    scan_green = (rle_pixel**)malloc(sizeof(rle_pixel*)*
				     (in_hdr.ymax-in_hdr.ymin+1));
    scan_blue = (rle_pixel**)malloc(sizeof(rle_pixel*)*
				    (in_hdr.ymax-in_hdr.ymin+1));
    if (!(scan_red && scan_green && scan_blue))
	error("can't allocate a scan buffer");
    if (flip) {
        for (i = 0; i < in_hdr.ymax-in_hdr.ymin+1; i++) {
            for (j=0; j < 3; j++) {
                scan[j] = (rle_pixel*)malloc(sizeof(rle_pixel)*(in_hdr.xmax+1));
                if (scan[j] == NULL) error("can't allocate current scanline");
            }
            rle_getrow(&in_hdr, scan);
            scan_red[i] = scan[0];
            scan_green[i] = scan[1];
            scan_blue[i] = scan[2];
            for (j=0; j < 3; j++) {
                scan[j] = (rle_pixel*)malloc(sizeof(rle_pixel)*(in_hdr.xmax+1));
                if (scan[j] == NULL) error("can't allocate current scanline");
            }
        }
    } else {
        for (i = 0; i < in_hdr.ymax-in_hdr.ymin+1; i++) {
            for (j=0; j < 3; j++) {
                scan[j] = (rle_pixel*)malloc(sizeof(rle_pixel)*(in_hdr.xmax+1));
                if (scan[j] == NULL) error("can't allocate current scanline");
            }
            rle_getrow(&in_hdr, scan);
            scan_red[in_hdr.ymax - in_hdr.ymin - i] = scan[0];
            scan_green[in_hdr.ymax - in_hdr.ymin - i] = scan[1];
            scan_blue[in_hdr.ymax - in_hdr.ymin - i] = scan[2];
        }
    }
    free( scan );
}

void
get_tiff_scanline( buf, row )
tiff_sample *buf;
int row;
{
    register int i;
    register int k;

    for (i=k=0; i < in_hdr.xmax+1; i++, k +=3) {
        buf[k] = scan_red[row][i];
        buf[k+1] = scan_green[row][i];
        buf[k+2] = scan_blue[row][i];
    }
}

void
error(s)
CONST_DECL char *s;
{
    fprintf(stderr,"%s: %s\n", in_hdr.cmd, s);
    exit(2);
}

void
usage()
{
    fprintf(stderr,"Usage:\n");
    fprintf(stderr,"\trletotiff [options] infile outfile\n");
    fprintf(stderr,"options:\n");
    fprintf(stderr,"\t-c lzw or -c none (compression, default lzw)\n");
    fprintf(stderr,"\t-v  (vertical flip, default not)\n");
    exit(2);
}
