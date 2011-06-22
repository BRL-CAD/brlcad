/*                      D U N N S N A P . C
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
/** @file util/dunnsnap.c
 *
 * Checks status of the Dunn camera and exposes the number of frames
 * of film specified in the argument (default is 1 frame).
 *
 * dunnsnap [num_frames]
 *
 */

#include "common.h"

#include <stdlib.h>
#include "bio.h"

#include "bu.h"
#include "fb.h"


/* Shared with dunncomm.c */
extern int fd;
extern char cmd;
extern void dunnopen(void);
extern int ready(int nsecs);
extern void getexposure(char *title);
extern int dunnsend(char color, int val);
extern int goodstatus(void);
extern void hangten(void);


static int nframes = 1;
static char *framebuffer;
static int scr_width = 0;
static int scr_height = 0;

static char usage[] = "\
Usage: dunnsnap [-h] [-F framebuffer]\n\
	[-{sS} squarescrsize] [-{wW} scr_width] [-{nN} scr_height]\n\
	[num_frames]\n";

int
get_args(int argc, char **argv)
{
    int c;

    while ((c = bu_getopt(argc, argv, "hF:s:S:w:W:n:N:")) != -1) {
	switch (c) {
	    case 'h':
		/* high-res */
		scr_height = scr_width = 1024;
		break;
	    case 'F':
		framebuffer = bu_optarg;
		break;
	    case 's':
	    case 'S':
		scr_height = scr_width = atoi(bu_optarg);
		break;
	    case 'w':
	    case 'W':
		scr_width = atoi(bu_optarg);
		break;
	    case 'n':
	    case 'N':
		scr_height = atoi(bu_optarg);
		break;

	    default:		/* '?' */
		return 0;
	}
    }

    if (bu_optind < argc) {
	nframes = atoi(argv[bu_optind]);
    }
    if (argc > ++bu_optind)
	(void)fprintf(stderr, "dunnsnap: excess argument(s) ignored\n");

    return 1;		/* OK */
}


int
main(int argc, char **argv)
{
    ssize_t ret;
    FBIO *fbp = FBIO_NULL;

    if (!get_args(argc, argv)) {
	(void)fputs(usage, stderr);
	bu_exit (1, NULL);
    }

    dunnopen();

    if (framebuffer != (char *)0) {
	if ((fbp = fb_open(framebuffer, scr_width, scr_height)) == FBIO_NULL)
	    bu_exit (12, NULL);
    }

    /* check argument */
    if (nframes < 0) {
	fprintf(stderr, "dunnsnap: negative frame count\n");
	goto bad;
    }
    if (nframes >= 10000)
	fprintf(stderr, "dunnsnap: What a lot of film!\n");

    if (!ready(2)) {
	fprintf(stderr, "dunnsnap:  camera not ready at startup\n");
	goto bad;
    }

    /* loop until number of frames specified have been exposed */

    while (nframes>0) {

	if (!ready(20)) {
	    fprintf(stderr, "dunnsnap: camera not ready at frame start\n");
	    goto bad;
	}

	if (!goodstatus()) {
	    fprintf(stderr, "dunnsnap: badstatus\n");
	    goto bad;
	}

	/* send expose command to camera */
	cmd = 'I';	/* expose command */
	ret = write(fd, &cmd, 1);
	if (ret < 0)
	    perror("write");
	hangten();

	/* Wait a long time here, because exposure can be lengthy */
	if (!ready(45)) {
	    fprintf(stderr, "dunnsnap: camera not ready after expose cmd\n");
	    goto bad;
	}
	--nframes;
    }
    if (fbp != FBIO_NULL)
	fb_close(fbp);

    return 0;

 bad:
    if (fbp != FBIO_NULL)
	fb_close(fbp);

    return 1;
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
