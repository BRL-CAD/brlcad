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
 * rlestereo.c - Merge two RLE images to form a single red-green or red-blue
 * 		 image suitable for viewing with them goofy glasses. 
 *
 * Author:      Craig E. Kolb
 * 		Department of Mathematics
 *		Yale Unversity
 * Date:        Mon Jul 30 1990
 * Copyright (C) 1990, Craig E. Kolb
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif



#include <stdio.h>

#ifdef USE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif

#include "machine.h"
#include "externs.h"
#include "rle.h"

#define LEFTSCALE	0.7		/* Default left scale factor. */
#define RIGHTSCALE	1.0		/* Default right scale factor. */

/*
 * Macro to convert input values to greyscale, taking colormap into
 * account.
 */
#define REDC		0.30
#define GREENC		0.59
#define BLUEC		0.11
#define GREYCONV(r,g,b,m) (REDC*m[0][r]+GREENC*m[1][g]+BLUEC*m[2][b])

float	LeftScale = LEFTSCALE;		/* Scale factor for left channel. */
float	RightScale = RIGHTSCALE;	/* Scale factor for right channel. */
int	GreenFlag = 0;			/* True if second channel is green. */
int	Xres, Yres;			/* Image resolution. */
int	lcolors, rcolors;		/* # channels in left/right images. */
int	RightIndex;			/* Index of second channel (1 or 2). */
int	lchan1, lchan2;			/* Channel indices for left image. */
int	rchan1, rchan2;			/* Channel indices for right image. */
rle_pixel **lmap, **rmap;		/* Colormap for left/right images. */

void convert_line(rle_pixel **out, rle_pixel **left, rle_pixel **right);
int get_rle_setup(rle_hdr *the_hdr, int *xres, int *yres, char *prog, char *file);

/*
 * Read two named RLE files and produces a single image suitable for viewing
 * with red-blue or red-green glasses.  The 'left' image is converted to
 * greyscale and written on the red channel.  The 'right' image is converted
 * to greyscale and written on the blue or green channel.  The intensity of the
 * two channels may be scaled in order to compensate for the relative
 * intensities of the two base colors as viewed through the glasses.
 *
 * Options:
 * 
 *	-g		Produce red-green image rather than red-blue.
 *	-l scale	Scale factor for left-eye image (default 0.7).
 *	-r scale	Scale factor for right-eye image (default 1.0).
 */

int
main(int argc, char **argv)
{
	int y, i, Xtmp, Ytmp, scaleflag = 0;
	char *leftname = NULL, *rightname = NULL;
	char *outname = NULL;
	FILE *outfile = stdout;
	int oflag;
	rle_pixel **lline, **rline, **outline;
	rle_hdr left_hdr, right_hdr;
	int rle_cnt = 0, rle_err;
	char *err_name = NULL;

	if (scanargs(argc, argv,
		"% g%- l%-leftscale!f r%-rightscale!f o%-outfile!s \n\
	leftimage!s rightimage!s",
		&GreenFlag, &scaleflag, &LeftScale, &scaleflag, &RightScale,
		&oflag, &outname,
		&leftname, &rightname) == 0) {
		exit(-1);
	}

	/*
	 * Open left and right images.
	 */
	left_hdr = right_hdr = rle_dflt_hdr;
	left_hdr.rle_file = rle_open_f(cmd_name(argv), leftname, "r");
	right_hdr.rle_file = rle_open_f(cmd_name(argv), rightname, "r");

	for (;;)
	{
	    /*
	     * Read headers of both images.
	     */
	    if ( (rle_err = 
		  get_rle_setup(&left_hdr, 
				&Xres, 
				&Yres, 
				cmd_name(argv),
				leftname)) != RLE_SUCCESS )
	    {
		err_name = leftname;
		break;
	    }
	    if ( (rle_err =
		  get_rle_setup(&right_hdr, &Xtmp, &Ytmp, cmd_name(argv),
				rightname)) != RLE_SUCCESS )
	    {
		err_name = rightname;
		break;
	    }

	    /* Open the output file when first header is successfully read. */
	    if ( rle_cnt == 0 )
		outfile = rle_open_f( cmd_name( argv ), outname, "w" );

	    /* Count the input images. */
	    rle_cnt++;

	    if (Xres != Xtmp || Yres != Ytmp) {
		fprintf(stderr,"Images are of different sizes.\n");
		exit(-1);
	    }

	    rle_dflt_hdr = left_hdr;
	    rle_dflt_hdr.ncolors = 3;
	    rle_dflt_hdr.ncmap = 0;
	    rle_dflt_hdr.rle_file = outfile;
	    rle_addhist(argv, &left_hdr, &rle_dflt_hdr);
	    rle_put_setup(&rle_dflt_hdr);

	    /*
	     * Ignore alpha and any channels > #2.
	     */
	    RLE_CLR_BIT(left_hdr, RLE_ALPHA);
	    RLE_CLR_BIT(right_hdr, RLE_ALPHA);
	    for (i = 3; i < left_hdr.ncolors; i++)
		RLE_CLR_BIT(left_hdr, i);
	    for (i = 3; i < right_hdr.ncolors; i++)
		RLE_CLR_BIT(right_hdr, i);

	    lcolors = left_hdr.ncolors > 3 ? 3 : left_hdr.ncolors;
	    rcolors = right_hdr.ncolors > 3 ? 3 : right_hdr.ncolors;

	    if (lcolors == 1)
		lchan1 = lchan2 = 0;
	    else if (lcolors == 2) {
		lchan1 = 1;
		lchan2 = 0;
	    } else {
		lchan1 = 1;
		lchan2 = 2;
	    }

	    if (rcolors == 1)
		rchan1 = rchan2 = 0;
	    else if (rcolors == 2) {
		rchan1 = 1;
		rchan2 = 0;
	    } else {
		rchan1 = 1;
		rchan2 = 2;
	    }

	    rle_row_alloc(&left_hdr, &lline);
	    rle_row_alloc(&right_hdr, &rline);
	    rle_row_alloc(&rle_dflt_hdr, &outline);
	    /*
	     * Zero unused channel of output scanline.
	     */
	    if (GreenFlag) {
		RightIndex = 1;
		bzero( (char *)outline[2], Xres * sizeof(rle_pixel));
	    } else {
		RightIndex = 2;
		bzero( (char *)outline[1], Xres * sizeof(rle_pixel));
	    }

	    /*
	     * In case there's a colormap, DTRT.  We use a gamma of 1.
	     * here because we want the output image to have the same
	     * gamma as the left image.
	     */
	    lmap = buildmap(&left_hdr, 3, 1., 1.0);
	    rmap = buildmap(&right_hdr, 3, 1., 1.0);
	    /*
	     * For each output scanline...
	     */
	    for (y = 0; y < Yres; y++) {
		/*
		 * Read left and right scanline, converting each
		 * to greyscale data.
		 */
		rle_getrow(&left_hdr, lline);
		rle_getrow(&right_hdr, rline);
		convert_line(outline, lline, rline);
		rle_putrow(outline, Xres, &rle_dflt_hdr);
	    }
	    while ( rle_getskip(&left_hdr) != 32768 )
		;
	    while ( rle_getskip(&right_hdr) != 32768 )
		;

	    rle_row_free(&left_hdr, lline);
	    rle_row_free(&right_hdr, rline);
	    rle_row_free(&rle_dflt_hdr, outline);

	    rle_puteof( &rle_dflt_hdr );
	}
	fclose(left_hdr.rle_file);
	fclose(right_hdr.rle_file);

	/* Check for an error.  EOF or EMPTY is ok if at least one image
	 * has been read.  Otherwise, print an error message.
	 */
	if ( rle_cnt == 0 || (rle_err != RLE_EOF && rle_err != RLE_EMPTY) )
	    rle_get_error( rle_err, cmd_name( argv ), err_name );
	exit(0);
}

int
get_rle_setup(rle_hdr *the_hdr, int *xres, int *yres, char *prog, char *file)
{
    	int err;

	err = rle_get_setup(the_hdr);
	*xres = the_hdr->xmax - the_hdr->xmin + 1;
	*yres = the_hdr->ymax - the_hdr->ymin + 1;
	the_hdr->xmax -= the_hdr->xmin;
	the_hdr->xmin = 0;
	return err;
}

void
convert_line(rle_pixel **out, rle_pixel **left, rle_pixel **right)
{
	register int i;
	
	/*
	 * Convert input scanlines to 'stereo' output scanline.
	 */
	for (i = 0; i < Xres; i++) {
		out[0][i] = LeftScale * GREYCONV(left[0][i], left[lchan1][i],
				left[lchan2][i], lmap);
		out[RightIndex][i] = RightScale * GREYCONV(right[0][i],
				right[rchan1][i], right[rchan2][i], rmap);
	}
}
