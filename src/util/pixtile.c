/*                       P I X T I L E . C
 * BRL-CAD
 *
 * Copyright (c) 1986-2011 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file util/pixtile.c
 *
 * Given multiple .pix files with ordinary lines of pixels, produce a
 * single image with each image side-by-side, right to left, bottom to
 * top on STDOUT.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <string.h>
#include "bio.h"

#include "bu.h"


int file_width = 64;	/* width of input sub-images in pixels (64) */
int file_height = 64;	/* height of input sub-images in scanlines (64) */
int scr_width = 512;	/* number of output pixels/line (512, 1024) */
int scr_height = 512;	/* number of output lines (512, 1024) */
char *base_name;		/* basename of input file(s) */
int framenumber = 0;	/* starting frame number (default is 0) */

char usage[] = "\
Usage: pixtile [-h] [-s squareinsize] [-w file_width] [-n file_height]\n\
	[-S squareoutsize] [-W out_width] [-N out_height]\n\
	[-o startframe] basename [file2 ... fileN] >file.pix\n";


/*
 * G E T _ A R G S
 */
int
get_args(int argc, char **argv)
{
    int c;

    while ((c = bu_getopt(argc, argv, "hs:w:n:S:W:N:o:")) != -1) {
	switch (c) {
	    case 'h':
		/* high-res */
		scr_height = scr_width = 1024;
		break;
	    case 's':
		/* square input file size */
		file_height = file_width = atoi(bu_optarg);
		break;
	    case 'w':
		file_width = atoi(bu_optarg);
		break;
	    case 'n':
		file_height = atoi(bu_optarg);
		break;
	    case 'S':
		scr_height = scr_width = atoi(bu_optarg);
		break;
	    case 'W':
		scr_width = atoi(bu_optarg);
		break;
	    case 'N':
		scr_height = atoi(bu_optarg);
		break;
	    case 'o':
		framenumber = atoi(bu_optarg);
		break;
	    default:		/* '?' */
		return 0;	/* Bad */
	}
    }

    if (isatty(fileno(stdout))) {
	return 0;	/* Bad */
    }

    if (bu_optind >= argc) {
	fprintf(stderr, "pixtile: basename or filename(s) missing\n");
	return 0;	/* Bad */
    }

    return 1;		/* OK */
}


int
main(int argc, char **argv)
{
    int i;
    char *obuf;
    int im_line;		/* number of images across output scanline */
    int im_high;		/* number of images (swaths) high */
    int scanbytes;		/* bytes per input line */
    int swathbytes;		/* bytes per swath of images */
    int image;		/* current sub-image number */
    int rel = 0;		/* Relative image # within swath */
    int maximage;		/* Maximum # of images that will fit */
    int islist = 0;		/* set if a list, zero if basename */
    int is_stream = 0;	/* set if input is stream on stdin */
    char name[256] = {0};
    ssize_t ret;

    if (!get_args(argc, argv)) {
	(void)fputs(usage, stderr);
	bu_exit (1, NULL);
    }

    if (bu_optind+1 == argc) {
	base_name = argv[bu_optind];
	islist = 0;
	if (base_name[0] == '-' && base_name[1] == '\0')
	    is_stream = 1;
    } else {
	islist = 1;
    }

    if (file_width < 1) {
	fprintf(stderr, "pixtile: width of %d out of range\n", file_width);
	bu_exit (12, NULL);
    }

    scanbytes = file_width * 3;

    /* number of images across line */
    im_line = scr_width/file_width;

    /* number of images high */
    im_high = scr_height/file_height;

    /* One swath of images */
    swathbytes = scr_width * file_height * 3;

    maximage = im_line * im_high;

    if ((obuf = (char *)malloc(swathbytes)) == (char *)0) {
	(void)fprintf(stderr, "pixtile:  malloc %d failure\n", swathbytes);
	bu_exit (10, NULL);
    }

    image = 0;
    while (image < maximage) {
	memset(obuf, 0, swathbytes);
	/*
	 * Collect together one swath
	 */
	for (rel = 0; rel<im_line; rel++, image++, framenumber++) {
	    int fd;

	    if (image >= maximage) {
		fprintf(stderr, "\npixtile: frame full\n");
		/* All swaths already written out */
		bu_exit (0, NULL);
	    }
	    fprintf(stderr, "%d ", framenumber);  fflush(stdout);
	    if (is_stream) {
		fd = 0;		/* stdin */
	    } else {
		if (islist) {
		    /* See if we read all the files */
		    if (bu_optind >= argc)
			goto done;
		    bu_strlcpy(name, argv[bu_optind++], sizeof(name));
		} else {
		    snprintf(name, sizeof(name), "%s.%d", base_name, framenumber);
		}
		if ((fd=open(name, 0))<0) {
		    perror(name);
		    goto done;
		}
	    }
	    /* Read in .pix file.  Bottom to top */
	    for (i=0; i<file_height; i++) {
		int j;

		/* virtual image l/r offset */
		j = (rel*file_width);

		/* select proper scanline within image */
		j += i*scr_width;

		if (bu_mread(fd, &obuf[j*3], scanbytes) != scanbytes) {
		    perror("READ ERROR");
		    break;
		}
	    }
	    if (fd > 0) close(fd);
	}
        ret = write(1, obuf, swathbytes);
	if (ret < 0)
	    perror("write");

	rel = 0;	/* in case we fall through */
    }
 done:
    /* Flush partial frame? */
    if (rel != 0) {
	ret = write(1, obuf, swathbytes);
	if (ret < 0)
	    perror("write");
    }

    fprintf(stderr, "\n");

    return 0;
}


/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
