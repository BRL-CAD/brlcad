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
 * 	toutah - 
 *		Convert a IRIS image to Utah raster toolkit.
 *
 *	To comple:
 *		cc -I/usr/include/gl toutah.c -o toutah -lrle -limage
 *
 * 				Paul Haeberli - 1990
 */
#include "stdio.h"
#include "image.h"
#include "rle.h"

short rbuf[8192]; 
short gbuf[8192]; 
short bbuf[8192]; 

rle_hdr	hdr;

main(argc,argv)
int argc;
char **argv;
{
    IMAGE *image;
    int i, y, z;
    int xsize, ysize, zsize;
    unsigned char *rows[3];
    char *infname = NULL, *outfname = NULL;

    if ( scanargs( argc, argv, "% o%-outfile.rle!s, infile.rgb!s\n\
(Convert SGI image in infile.rgb to URT RLE image in outfile.rle.)",
		   &i, &outfname, &infname ) == 0 )
	exit( 1 );

/* open the input file */
    image=iopen(infname,"r");
    if(!image) {
	fprintf(stderr,"%s: can't open input file %s\n",cmd_name(argv),infname);
	exit(1);
    }
    xsize = image->xsize;
    ysize = image->ysize;
    zsize = image->zsize;

/* open the output file */
    hdr = *rle_hdr_init( NULL );
    rle_names( &hdr, cmd_name( argv ), outfname, 0 );
    rle_addhist( argv, (rle_hdr *)NULL, &hdr );
    hdr.rle_file = rle_open_f(hdr.cmd,outfname,"w");

/* allocate row buffers */
    rows[0] = (unsigned char *)malloc(xsize*sizeof(unsigned char));
    rows[1] = (unsigned char *)malloc(xsize*sizeof(unsigned char));
    rows[2] = (unsigned char *)malloc(xsize*sizeof(unsigned char));

/* set up the header */
    hdr.xmin = 0;
    hdr.xmax = xsize - 1;
    hdr.ymin = 0;
    hdr.ymax = ysize - 1;
    hdr.ncolors = 3;
    hdr.background = 0;
    hdr.alpha = 0;
    hdr.ncmap = 0;
    RLE_SET_BIT(hdr,RLE_RED);
    RLE_SET_BIT(hdr,RLE_GREEN);
    RLE_SET_BIT(hdr,RLE_BLUE);
    rle_put_setup(&hdr);

/* copy image data */
    for(y=0; y<ysize; y++) {
	if(image->zsize<3) {
	    getrow(image,rbuf,y,0);
	    stoc(rbuf,rows[0],xsize);
	    stoc(rbuf,rows[1],xsize);
	    stoc(rbuf,rows[2],xsize);
	} else {
	    getrow(image,rbuf,y,0);
	    getrow(image,gbuf,y,1);
	    getrow(image,bbuf,y,2);
	    stoc(rbuf,rows[0],xsize);
	    stoc(gbuf,rows[1],xsize);
	    stoc(bbuf,rows[2],xsize);
	}
	rle_putrow(rows,xsize,&hdr);
    }

/* close the output file */
    rle_puteof(&hdr);
    exit(0);
}

stoc(sptr,cptr,n)
register unsigned short *sptr;
register unsigned char *cptr;
register int n;
{
    while(n--) {
	if(n>=8) {
	    cptr[0] = sptr[0];
	    cptr[1] = sptr[1];
	    cptr[2] = sptr[2];
	    cptr[3] = sptr[3];
	    cptr[4] = sptr[4];
	    cptr[5] = sptr[5];
	    cptr[6] = sptr[6];
	    cptr[7] = sptr[7];
	    sptr+=8; 
	    cptr+=8;
	    n -= 7;
	} else {
	    *cptr++ = *sptr++;
	}
    }
}
