/*                         B W - P S . C
 * BRL-CAD
 *
 * Copyright (c) 1986-2026 United States Government as represented by
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
/** @file util/bw-ps.c
 *
 * Convert a black and white (bw) file to an 8-bit PostScript image.
 *
 */

#include "common.h"

#include <errno.h>
#include <limits.h>
#include <stdlib.h> /* for atof() */
#include <math.h>
#include <time.h> /* for ctime() */
#include "bio.h"
#include "bu/app.h"
#include "bu/getopt.h"
#include "bu/opt.h"
#include "bu/exit.h"


#define DEFAULT_SIZE 6.75	/* default output size in inches */
#define MAX_BYTES (64*128)	/* max bytes per image chunk */

static int encapsulated = 0;	/* encapsulated postscript */
static int center = 0;		/* center output on 8.5 x 11 page */
static int landscape = 0;	/* landscape mode */

static size_t width = 512;		/* input size in pixels */
static size_t height = 512;
static double outwidth = DEFAULT_SIZE;		/* output image size in inches */
static double outheight = DEFAULT_SIZE;
static size_t xpoints;		/* output image size in points */
static size_t ypoints;
static size_t pagewidth = 612;	/* page size in points - 8.5 inches */
static size_t pageheight = 792;	/* 11 inches */

static char Stdin[] = "stdin";
static char *file_name;
static FILE *infp;

static char usage[] = "\
Usage: bw-ps [-e] [-c] [-L]\n\
	[-s input_squaresize] [-w input_width] [-n input_height]\n\
	[-S inches_square] [-W inches_width] [-N inches_height] [file.bw]\n";

static int
parse_positive_double_arg(const char *arg, double *value, const char *label)
{
    double _d;

    if (!bu_opt_scan_double(arg, &_d, label))
	return 0;
    if (_d <= 0.0) {
	bu_log("%s: %s must be positive '%s'\n", bu_getprogname(), label, arg);
	return 0;
    }

    *value = _d;
    return 1;
}

void
prolog(FILE *fp, char *name, int w, int h)


/* in points */
{
    time_t ltime;

    ltime = time(0);

    if (encapsulated) {
	fputs("%!PS-Adobe-2.0 EPSF-1.2\n", fp);
	fprintf(fp, "%%%%Title: %s\n", name);
	fputs("%%Creator: BRL-CAD bw-ps\n", fp);
	fprintf(fp, "%%%%CreationDate: %s", ctime(&ltime));
	fputs("%%Pages: 0\n", fp);
    } else {
	fputs("%!PS-Adobe-1.0\n", fp);
	fputs("%begin(plot)\n", fp);
	fprintf(fp, "%%%%Title: %s\n", name);
	fputs("%%Creator: BRL-CAD bw-ps\n", fp);
	fprintf(fp, "%%%%CreationDate: %s", ctime(&ltime));
    }
    fprintf(fp, "%%%%BoundingBox: 0 0 %d %d\n", w, h);
    fputs("%%EndComments\n\n", fp);

    if (!encapsulated && landscape) {
	int tmp;
	tmp = pagewidth;
	pagewidth = pageheight;
	pageheight = tmp;
	fprintf(fp, "90 rotate\n");
	fprintf(fp, "0 -%lu translate\n", (unsigned long)pageheight);
    }
    if (!encapsulated && center) {
	int xtrans, ytrans;
	xtrans = (pagewidth - w)/2.0;
	ytrans = (pageheight - h)/2.0;
	fprintf(fp, "%d %d translate\n", xtrans, ytrans);
    }
    fprintf(fp, "%d %d scale\n\n", w, h);
}


void
postlog(FILE *fp)
{
    if (!encapsulated)
	fputs("%end(plot)\n", fp);

    fputs("\nshowpage\n", fp);
}


int
get_args(int argc, char **argv)
{
    int c;

    while ((c = bu_getopt(argc, argv, "ecLs:w:n:S:W:N:h?")) != -1) {
	switch (c) {
	    case 'e':
		/* Encapsulated PostScript */
		encapsulated++;
		break;
	    case 'c':
		center = 1;
		break;
	    case 'L':
		landscape = 1;
		break;
	    case 's':
		/* square file size */
		if (!bu_opt_scan_size_t_range(bu_optarg, &width, 1, SIZE_MAX, "input size"))
		    return 0;
		height = width;
		break;
	    case 'w':
		if (!bu_opt_scan_size_t_range(bu_optarg, &width, 1, SIZE_MAX, "input width"))
		    return 0;
		break;
	    case 'n':
		if (!bu_opt_scan_size_t_range(bu_optarg, &height, 1, SIZE_MAX, "input height"))
		    return 0;
		break;
	    case 'S':
		/* square file size */
		if (!parse_positive_double_arg(bu_optarg, &outwidth, "output size"))
		    return 0;
		outheight = outwidth;
		break;
	    case 'W':
		if (!parse_positive_double_arg(bu_optarg, &outwidth, "output width"))
		    return 0;
		break;
	    case 'N':
		if (!parse_positive_double_arg(bu_optarg, &outheight, "output height"))
		    return 0;
		break;

	    default:		/* 'h' '?' */
		return 0;
	}
    }

    if (bu_optind >= argc) {
	if (isatty(fileno(stdin)))
	    return 0;
	file_name = Stdin;
	infp = stdin;
    } else {
	file_name = argv[bu_optind];
	bu_optind++;
	if (argc > bu_optind) {
	    fprintf(stderr, "bw-ps: excess argument(s) not supported\n");
	    return 0;
	}
	if ((infp = fopen(file_name, "rb")) == NULL) {
	    fprintf(stderr,
		    "bw-ps: cannot open \"%s\" for reading\n",
		    file_name);
	    return 0;
	}
	/*fileinput++;*/
    }

    if (argc > bu_optind) {
	fprintf(stderr, "bw-ps: excess argument(s) not supported\n");
	return 0;
    }

    return 1;		/* OK */
}


int
main(int argc, char **argv)
{
    size_t num = 0;
    size_t scans_per_patch, bytes_per_patch;
    size_t y;
    FILE *ofp = NULL;

    bu_setprogname(argv[0]);

    setmode(fileno(stdin), O_BINARY);
    setmode(fileno(stdout), O_BINARY);

    if (!get_args(argc, argv)) {
	(void)fputs(usage, stderr);
	bu_exit (1, NULL);
    }

    ofp = stdout;

    if (encapsulated) {
	xpoints = width;
	ypoints = height;
    } else {
	xpoints = outwidth * 72 + 0.5;
	ypoints = outheight * 72 + 0.5;
    }
    prolog(ofp, file_name, xpoints, ypoints);

    scans_per_patch = MAX_BYTES / width;
    if (scans_per_patch > height)
	scans_per_patch = height;
    bytes_per_patch = scans_per_patch * width;

    for (y = 0; y < height; y += scans_per_patch) {
	/* start a patch */
	fprintf(ofp, "save\n");
	fprintf(ofp, "%lu %lu 8 [%lu 0 0 %lu 0 %ld] {<\n ",
		(unsigned long)width, (unsigned long)scans_per_patch,		/* patch size */
		(unsigned long)width, (unsigned long)height,			/* total size = 1.0 */
		-(long)y);				/* patch y origin */

	/* data */
	num = 0;
	while (num < bytes_per_patch) {
	    fprintf(ofp, "%02x", getc(infp));
	    if ((++num % 32) == 0)
		fprintf(ofp, "\n ");
	}

	/* close this patch */
	fprintf(ofp, ">} image\n");
	fprintf(ofp, "restore\n");
    }

    postlog(ofp);
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
