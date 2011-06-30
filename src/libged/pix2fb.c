/*                        P I X 2 F B . C
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
 *
 */
/** @file libged/pix2fb.c
 *
 * Program to take bottom-up pixel files and send them to a framebuffer.
 *
 */

#include "common.h"

#include <stdlib.h>
#ifdef HAVE_SYS_TYPES_H
#  include <sys/types.h>
#endif
#ifdef HAVE_SYS_STAT_H
#  include <sys/stat.h>
#endif
#ifdef HAVE_WINSOCK_H
#  include <winsock.h>
#endif
#include "bio.h"

#include "bu.h"
#include "fb.h"
#include "fbserv_obj.h"

#include "pkg.h"
#include "ged.h"


static int multiple_lines = 0;	/* Streamlined operation */

static char *file_name;
static int infd;

static int fileinput = 0;	/* file or pipe on input? */
static int autosize = 0;	/* !0 to autosize input */

static size_t file_width = 512;	/* default input width */
static size_t file_height = 512;/* default input height */
static int scr_width = 0;	/* screen tracks file if not given */
static int scr_height = 0;
static int file_xoff, file_yoff;
static int scr_xoff, scr_yoff;
static int clear = 0;
static int zoom = 0;
static int inverse = 0;		/* Draw upside-down */
static int one_line_only = 0;	/* insist on 1-line writes */
static int pause_sec = 0; 	/* Pause that many seconds before
				   closing the FB and exiting */

static char usage[] = "\
Usage: pix-fb [-a -h -i -c -z -1] [-m #lines] [-F framebuffer]\n\
	[-s squarefilesize] [-w file_width] [-n file_height]\n\
	[-x file_xoff] [-y file_yoff] [-X scr_xoff] [-Y scr_yoff]\n\
	[-S squarescrsize] [-W scr_width] [-N scr_height] [-p seconds]\n\
	[file.pix]\n";

static int
get_args(int argc, char **argv)
{
    int c;

    bu_optind = 1;
    while ((c = bu_getopt(argc, argv, "1m:ahiczF:p:s:w:n:x:y:X:Y:S:W:N:")) != -1) {
	switch (c) {
	    case '1':
		one_line_only = 1;
		break;
	    case 'm':
		multiple_lines = atoi(bu_optarg);
		break;
	    case 'a':
		autosize = 1;
		break;
	    case 'h':
		/* high-res */
		file_height = file_width = 1024;
		scr_height = scr_width = 1024;
		autosize = 0;
		break;
	    case 'i':
		inverse = 1;
		break;
	    case 'c':
		clear = 1;
		break;
	    case 'z':
		zoom = 1;
		break;
	    case 's':
		/* square file size */
		file_height = file_width = atoi(bu_optarg);
		autosize = 0;
		break;
	    case 'w':
		file_width = atoi(bu_optarg);
		autosize = 0;
		break;
	    case 'n':
		file_height = atoi(bu_optarg);
		autosize = 0;
		break;
	    case 'x':
		file_xoff = atoi(bu_optarg);
		break;
	    case 'y':
		file_yoff = atoi(bu_optarg);
		break;
	    case 'X':
		scr_xoff = atoi(bu_optarg);
		break;
	    case 'Y':
		scr_yoff = atoi(bu_optarg);
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
	    case 'p':
		pause_sec=atoi(bu_optarg);
		break;

	    default:		/* '?' */
		return 0;
	}
    }

    if (bu_optind >= argc) {
	if (isatty(fileno(stdin)))
	    return 0;
	file_name = "-";
	infd = 0;
    } else {
	file_name = argv[bu_optind];
	if ((infd = open(file_name, 0)) < 0) {
	    perror(file_name);
	    (void)fprintf(stderr,
			  "pix-fb: cannot open \"%s\" for reading\n",
			  file_name);
	    bu_exit(1, NULL);
	}
#ifdef _WIN32
	setmode(infd, O_BINARY);
#endif
	fileinput++;
    }

    if (argc > ++bu_optind)
	(void)fprintf(stderr, "pix-fb: excess argument(s) ignored\n");

    return 1;		/* OK */
}


int
ged_pix2fb(struct ged *gedp, int argc, const char *argv[])
{
    int ret;

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_FBSERV(gedp, GED_ERROR);
    GED_CHECK_FBSERV_FBP(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    if (!get_args(argc, (char **)argv)) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    ret = fb_read_fd(gedp->ged_fbsp->fbs_fbp, infd,
		     file_width, file_height,
		     file_xoff, file_yoff,
		     scr_width, scr_height,
		     scr_xoff, scr_yoff,
		     fileinput, file_name, one_line_only, multiple_lines,
		     autosize, inverse, clear, zoom,
		     gedp->ged_result_str);

    if (infd != 0)
	close(infd);

    sleep(pause_sec);

    if (ret == BRLCAD_OK)
	return GED_OK;

    return GED_ERROR;
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
