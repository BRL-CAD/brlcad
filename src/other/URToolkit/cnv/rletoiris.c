/*
 * This software is copyrighted as noted below.  It may be freely copied,
 * modified, and redistributed, provided that the copyright notice is 
 * preserved on all copies.
 * 
 * There is no warranty or other guarantee of fitness for this software,
 * it is provided solely "as is".  Bug reports or fixes may be sent
 * to the author, who may or may not act on them as he desires.
 *
 * You may not include this software in a program or other software product
 * without supplying the source, or without informing the end-user that the 
 * source is available for no extra charge.
 *
 * If you modify this software, you should include a notice giving the
 * name of the person performing the modification, the date of modification,
 * and the reason for such modification.
 */
/* 
 * 	fromutah -
 *		Convert a UTAH raster toolkit image into an IRIS image.
 *
 *	To compile:
 *		cc -I/usr/include/gl fromutah.c -o fromutah -lrle -limage 
 *
 *				Paul Haeberli - 1988
 */
#include "stdio.h"
#include "image.h"
#include "rle.h"

short sbuf[4096];

rle_hdr hdr;

main(argc,argv)
int argc;
char *argv[];
{
    unsigned char *rows[3];
    int i, y, xsize, ysize;
    IMAGE *oimage;
    FILE *infile;
    char *infname = NULL, *outfname = NULL;

/* get args */
    if ( scanargs( argc, argv, "% o!-outimage.rgb!s in.rle%s\n(\
Convert URT RLE image in in.rle to SGI image format in outimage.rgb.\n\
Must specify -o outimage.rgb.)",
		   &i, &outfname, &infname ) == 0 )
	exit( 1 );

/* open the input file */
    hdr = *rle_hdr_init( (rle_hdr *)NULL );
    hdr.rle_file = rle_open_f(cmd_name(argv), infname, "r");
    rle_names( &hdr, cmd_name(argv), infname, 0 );

    rle_get_setup_ok(&hdr, cmd_name(argv), infname);

    RLE_CLR_BIT(hdr,RLE_ALPHA);
    if(hdr.ncolors != 3) {
	fprintf(stderr,"%s: input image must have 3 channels\n", hdr.cmd);
	exit(1);
    }
/* get the size */
    xsize = hdr.xmax-hdr.xmin+1;
    ysize = hdr.ymax-hdr.ymin+1;

/* open the output file */
    oimage = iopen(outfname,"w",RLE(1),3,xsize,ysize,3);

/* allocate row buffers */
    rows[0] = (unsigned char*)malloc(xsize);
    rows[1] = (unsigned char*)malloc(xsize);
    rows[2] = (unsigned char*)malloc(xsize);

/* copy the image data */
    for(y=0; y<ysize; y++) {
	rle_getrow( &hdr, rows );
	ctos(rows[0],sbuf,xsize);
	putrow(oimage,sbuf,y,0);
	ctos(rows[1],sbuf,xsize);
	putrow(oimage,sbuf,y,1);
	ctos(rows[2],sbuf,xsize);
	putrow(oimage,sbuf,y,2);
    }

/* close the output file */
    iclose(oimage);
    exit(0);
}

ctos(cptr,sptr,n)
register unsigned char *cptr;
register unsigned short *sptr;
register int n;
{
    while(n--) {
	if(n>=8) {
	    sptr[0] = cptr[0];
	    sptr[1] = cptr[1];
	    sptr[2] = cptr[2];
	    sptr[3] = cptr[3];
	    sptr[4] = cptr[4];
	    sptr[5] = cptr[5];
	    sptr[6] = cptr[6];
	    sptr[7] = cptr[7];
	    sptr+=8; 
	    cptr+=8;
	    n -= 7;
	} else {
	    *sptr++ = *cptr++;
	}
    }
}
