/*                    P I X F L I P - F B . C
 * BRL-CAD
 *
 * Copyright (c) 1988-2011 United States Government as represented by
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
 *
 */
/** @file pixflip-fb.c
 *
 * Given multiple .pix files with ordinary lines of pixels, sequence
 * through them on the current framebuffer.  A window-system version
 * of "pixtile and fbanim".  This program depends heavily on having
 * lots of virtual memory in which to buffer all the images.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <string.h>
#ifdef HAVE_SYS_TIME_H
#  include <sys/time.h>		/* For struct timeval */
#endif
#ifdef HAVE_SYS_TYPES_H
#  include <sys/types.h>
#endif
#ifdef HAVE_SYS_STAT_H
#  include <sys/stat.h>
#endif
#include "bio.h"

#include "bu.h"
#include "fb.h"


int file_width = 512;	/* width of input sub-images in pixels */
int file_height = 512;	/* height of input sub-images in scanlines */
int screen_width = 0;	/* number of screen pixels/line */
int screen_height = 0;	/* number of screen lines */
char *input_basename;		/* basename of input file(s) */
int framenumber = 0;	/* starting frame number (default is 0) */
int fps = 8;		/* frames/second */

FBIO *fbp;
int verbose = 0;
int rocking = 0;
int passes = 100;		/* limit on number of passes */
int zoom = 1;

#define MAXFRAMES 1000
unsigned char *frames[MAXFRAMES];	/* Pointers to pixel arrays */
int maxframe = 0;		/* Index of first unused slot in frames[] */

char usage[] = "\
Usage: pixflip-fb [-h]\n\
	[-s square_file_size] [-w file_width] [-n file_height]\n\
	[-S square_scr_size] [-W scr_width] [-N scr_height]\n\
	[-f frames/sec] [-p passes] [-r] [-v] [-z]\n\
	[-o startframe] basename [file2 ... fileN]\n";

void
showframe(int i)
{
    if (verbose) {
	fprintf(stderr, " %d", i);
	fflush(stderr);
    }

    if (i < 0 || i > maxframe) {
	fprintf(stderr, "pixflip-fb:  %d out of range\n", i);
	return;
    }

    fb_writerect(fbp, 0, 0, file_width, file_height, frames[i]);

    if (verbose) {
	fprintf(stderr, ", ");
	fflush(stderr);
    }
}


int
get_args(int argc, char **argv)
{
    int c;

    while ((c = bu_getopt(argc, argv, "hs:w:n:S:W:N:o:f:p:rzv")) != -1) {
	switch (c) {
	    case 'h':
		/* high-res */
		screen_height = screen_width = 1024;
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
		screen_height = screen_width = atoi(bu_optarg);
		break;
	    case 'W':
		screen_width = atoi(bu_optarg);
		break;
	    case 'N':
		screen_height = atoi(bu_optarg);
		break;
	    case 'o':
		framenumber = atoi(bu_optarg);
		break;
	    case 'f':
		fps = atoi(bu_optarg);
		break;
	    case 'p':
		passes = atoi(bu_optarg);
		if (passes<1) passes=1;
		break;
	    case 'r':
		rocking = 1;
		break;
	    case 'z':
		zoom = 0;
		break;
	    case 'v':
		verbose = 1;
		break;
	    default:		/* '?' */
		return 0;	/* Bad */
	}
    }

    if (bu_optind >= argc) {
	fprintf(stderr, "pixflip-fb: basename or filename(s) missing\n");
	return 0;	/* Bad */
    }

    return 1;		/* OK */
}


int
main(int argc, char **argv)
{
    int i;
    unsigned char *obuf;
    int scanbytes;		/* bytes per input image */
    int islist = 0;		/* set if a list, zero if basename */
    char name[256];
    int fd;
    struct timeval tv;
    fd_set readfds;

    FD_ZERO(&readfds);
    FD_SET(fileno(stdin), &readfds);

    if (!get_args(argc, argv)) {
	(void)fputs(usage, stderr);
	bu_exit(1, NULL);
    }

    if (bu_optind+1 == argc) {
	input_basename = argv[bu_optind];
	islist = 0;
    } else {
	islist = 1;
    }

    if (file_width < 1) {
	fprintf(stderr, "pixflip-fb: width of %d out of range\n", file_width);
	bu_exit(12, NULL);
    }

    if ((fbp = fb_open(NULL, screen_width, screen_height)) == FBIO_NULL) {
	fprintf(stderr, "pixflip-fb: fb_open failed\n");
	bu_exit(12, NULL);
    }
    screen_width = fb_getwidth(fbp);
    screen_height = fb_getheight(fbp);

    if (zoom) {
	if (verbose) fprintf(stderr,
			     "pixflip-fb: zoom = %d x %d\n",
			     screen_width/file_width,
			     screen_height/file_height);
	fb_view(fbp, file_width/2, file_height/2,
		screen_width/file_width,
		screen_height/file_height);
    }

    if (fps <= 1) {
	tv.tv_sec = fps ? 1L : 4L;
	tv.tv_usec = 0L;
    } else {
	tv.tv_sec = 0L;
	tv.tv_usec = (long) (1000000/fps);
    }

    scanbytes = file_width * file_height * sizeof(RGBpixel);

    for (maxframe = 0; maxframe < MAXFRAMES;) {

	if ((obuf = (unsigned char *)malloc(scanbytes)) == (unsigned char *)0) {
	    (void)fprintf(stderr, "pixflip-fb:  malloc %d failure\n", scanbytes);
	    break;
	}
	memset((char *)obuf, 0, scanbytes);
	frames[maxframe] = obuf;

	fprintf(stderr, "%d ", framenumber);  fflush(stdout);
	if (islist) {
	    /* See if we read all the files */
	    if (bu_optind >= argc)
		goto done;
	    bu_strlcpy(name, argv[bu_optind++], sizeof(name));
	} else {
	    snprintf(name, sizeof(name), "%s.%d", input_basename, framenumber);
	}
	if ((fd=open(name, 0))<0) {
	    perror(name);
	    goto done;
	}

	/* Read in .pix file.  Bottom to top */
	i = read(fd, obuf, scanbytes);
	if (i <= 0) {
	    perror(name);
	} else if (i != scanbytes) {
	    fprintf(stderr, "\npixflip-fb:  %s wanted %d got %d\n",
		    name, scanbytes, i);
	}
	close(fd);

	/* Show user the frame that was just read */
	showframe(maxframe++);
	framenumber++;
    }
done:
    while (passes-- > 0) {
	if (!rocking) {
	    /* Play from start to finish, over and over */
	    for (i=0; i<maxframe; i++) {
		showframe(i);
		select(fileno(stdin)+1, &readfds,
		       (fd_set *)0, (fd_set *)0, &tv);
	    }
	} else {
	    /* Play from start to finish and back */
	    for (i=0; i<maxframe; i++) {
		showframe(i);
		select(fileno(stdin)+1, &readfds,
		       (fd_set *)0, (fd_set *)0, &tv);
	    }
	    while (i-->0) {
		showframe(i);
		select(fileno(stdin)+1, &readfds,
		       (fd_set *)0, (fd_set *)0, &tv);
	    }
	}
    }
    fb_close(fbp);

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
