/*                      P I X P A S T E . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2011 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file util/pixpaste.c
 *
 * pixpaste will insert an arbitrary pix file into another pixfile.
 * If the image being pasted does not fit within the destination file
 * then the excess is discarded.
 *
 * Author -
 * Christopher T. Johnson
 * September 12, 1992
 *
 * This software is Copyright (C) 1992 by Paladin Software.
 * All rights reserved.
 */

#include "common.h"

#include <stdlib.h>
#include <string.h>
#include "bio.h"

#include "bu.h"
#include "vmath.h"
#include "bn.h"
#include "fb.h"


static long int org_width = 512L;	/* Default file sizes 512x512 */
static long int org_height = 512L;
static long int paste_width = 512L;
static long int paste_height = 512L;
static long int base_x = 0L;		/* Default to lower left corner */
static long int base_y = 0L;
static int Verbose = 0;

static char stdiobuf[4*1024*1024] = {0};
static FILE *orig = (FILE *)NULL;
static FILE *paste = (FILE *)NULL;
static char *orig_name = (char *)NULL;
static char *paste_name = (char *)NULL;
static int paste_autosize = 0;
static int orig_autosize = 0;
static int paste_isfile = 0;
static int orig_isfile = 0;
static long int num_bytes = 3L;

static char usage[] = "\
pixpaste: Copyright (C) 1992 Paladin Software\n\
pixpaste: All rights reserved\n\
pixpaste: Usage: pixpaste [-v] [-h] [-H] [-a] [-A] [-# num_bytes]\n\
		 [-s orig_square_size] [-w orig_width] [-n orig_height]\n\
		 [-S paste_square_size] [-W paste_width] [-N paste_height]\n\
		 [-x horizontal] [-y vertical] orig_file paste_file\n\
	A '-' can be used to indicate stdin for orig_file or paste_file\n";

int
get_args(int argc, char **argv)
{
    int c;

    while ((c = bu_getopt(argc, argv, "vahHs:w:n:S:W:N:x:y:#:")) != -1) {
	switch (c) {
	    case 'v':
		Verbose = 1;
		break;
	    case 'a':
		orig_autosize = 1;
		break;
	    case 'A':
		paste_autosize = 1;
		break;
	    case 'h':
		org_width = org_height = 1024L;
		orig_autosize = 0;
		break;
	    case 'H':
		paste_width = paste_height = 1024L;
		paste_autosize = 0;
		break;
	    case 's':
		org_width = org_height = atol(bu_optarg);
		orig_autosize = 0;
		break;
	    case 'S':
		paste_width = paste_height = atol(bu_optarg);
		paste_autosize = 0;
		break;
	    case 'w':
		org_width = atol(bu_optarg);
		orig_autosize = 0;
		break;
	    case 'W':
		paste_width = atol(bu_optarg);
		paste_autosize = 0;
		break;
	    case 'n':
		org_height = atol(bu_optarg);
		orig_autosize = 0;
		break;
	    case 'N':
		paste_height = atol(bu_optarg);
		paste_autosize = 0;
		break;
	    case 'x':
		base_x = atol(bu_optarg);
		break;
	    case 'y':
		base_y = atol(bu_optarg);
		break;
	    case '#':
		num_bytes = atol(bu_optarg);
		break;
	    default:		/* '?' */
		return 0;
	}
    }
    if (bu_optind >= argc) {
	return 0;
    } else {
	orig_name = argv[bu_optind];
	if (BU_STR_EQUAL(orig_name, "-")) {
	    if (isatty(fileno(stdin))) return 0;
	    orig = stdin;
	} else {
	    if ((orig = fopen(orig_name, "r")) == NULL) {
		perror(orig_name);
		(void)fprintf(stderr,
			      "pixpaste: cannot open \"%s\" for reading\n",
			      orig_name);
		return 0;
	    }
	    orig_isfile = 1;
	}
    }
    if (++bu_optind >= argc) {
	return 0;
    } else {
	paste_name = argv[bu_optind];
	if (BU_STR_EQUAL(paste_name, "-")) {
	    if (isatty(fileno(stdin))) return 0;
	    paste = stdin;
	    if (!orig_isfile) {
		(void)fprintf(stderr,
			      "pixpaste: The original file and paste file cannot both be stdin!.\n");
		return 0;
	    }
	} else {
	    if ((paste = fopen(paste_name, "r")) == NULL) {
		perror(paste_name);
		(void)fprintf(stderr,
			      "pixpaste: cannot open \"%s\" for reading",
			      paste_name);
		return 0;
	    }
	    paste_isfile=1;
	}
    }
    return 1;	/* OK */
}


int
main(int argc, char **argv)
{
    unsigned char *origbuf, *pastebuf;
    unsigned char *buffer;
    long int i;
    long int row, result;

    if (!get_args(argc, argv)) {
	(void)fprintf(stderr, "%s", usage);
	bu_exit (1, NULL);
    }
    /* Should we autosize the original? */
    if (orig_isfile && orig_autosize) {
	size_t w, h;
	if (fb_common_file_size(&w, &h, orig_name, num_bytes)) {
	    org_width = (long)w;
	    org_height = (long)h;
	} else {
	    (void) fprintf(stderr, "pixpaste: unable to autosize \"%s\"\n", orig_name);
	}
    }

    /* Should we autosize the paste file? */
    if (paste_isfile && paste_autosize) {
	size_t w, h;
	if (fb_common_file_size(&w, &h, paste_name, num_bytes)) {
	    paste_width = (long)w;
	    paste_height = (long)h;
	} else {
	    (void) fprintf(stderr, "pixpaste: unable to autosize \"%s\"\n", paste_name);
	}
    }

    (void) setvbuf(orig, stdiobuf, _IOFBF, sizeof(stdiobuf));

/*
 * Spew some interesting info at the people...
 */
    if (Verbose) {
	(void) fprintf(stderr, "pixpaste: Copyright (C) 1992 Paladin Software\npixpaste: All rights reserved\n");
	(void) fprintf(stderr, "pixpaste: Original image %ldx%ld\n",
		       org_width, org_height);
	(void) fprintf(stderr, "pixpaste: Inserted image %ldx%ld\n",
		       paste_width, paste_height);
	(void) fprintf(stderr, "pixpaste: Inserted at %ldx%ld\n",
		       base_x, base_y);
    }

/*
 * Make a buffer will hold a single scan line of assuming a worst
 * case paste of 1 pixel of overlap.
 */
    buffer = (unsigned char *)bu_malloc((org_width+paste_width)*num_bytes, "buffer");

/*
 * Set up the original buffer and the paste buffer.
 */
    if (base_x + paste_width < 0 ||
	base_x >= org_width ||
	base_y + paste_height < 0 ||
	base_y >= org_height) {
	if (Verbose) {
	    (void)fprintf(stderr, "\
pixpaste: No overlap between paste and original image\n\
pixpaste: new image == original image.\n");
	}
	for (i=0; i<org_height; i++) {
	    int E=0;
	    result = fread(buffer, num_bytes, org_width, orig);
	    if (result != org_width) {
		E=1;
		(void)fprintf(stderr, "pixpaste: original file is short.\n");
	    }
	    result = fwrite(buffer, num_bytes, result, stdout);
	    if (!E && result != org_width) {
		perror("pixpaste: fwrite1");
		E=1;
	    }
	    if (E) break;
	}
	bu_exit (0, NULL);
    }

    if (base_x < 0) {
	pastebuf = buffer;
	origbuf  = buffer - num_bytes*base_x;
    } else {
	pastebuf = buffer + num_bytes*base_x;
	origbuf  = buffer;
    }
/*
 * if the base_y scan line is below the bottom of the original
 * then we skip scan lines in the paste file.
 */
    if (base_y < 0) {
	row = base_y;
    } else {
	row = 0;
    }

    while (row < 0) {
	result = fread(pastebuf, num_bytes, paste_width, paste);
	if (result != paste_width) {
	    (void)fprintf(stderr, "pixpaste: paste file is short.\n");
	    row=0;
	    paste_height=0;
	}
	row++;
    }
/*
 * While the current row is less than the base Y scan line move
 * scan lines from the original to stdout.
 */
    while (row < base_y) {
	result=fread(origbuf, num_bytes, org_width, orig);
	if (result != org_width) {
	    (void)fprintf(stderr, "pixpaste: original image is short.\n");
	    result = fwrite(origbuf, num_bytes, result, stdout);
	    if (result == 0) {
		perror("fwrite");
	    }
	    bu_exit (0, NULL);
	}
	result = fwrite(origbuf, num_bytes, org_width, stdout);
	if (result != org_width) {
	    perror("pixpaste: fwrite2");
	    bu_exit (3, NULL);
	}
	row++;
    }
/*
 * Read a scan line from the original.  Read a scan line from the
 * paste. Output the composite. until paste_height is reached.
 * If EOF original STOP.  If EOF paste treat as if reached paste_height.
 */
    while (row < org_height && row < base_y+paste_height) {
	result=fread(origbuf, num_bytes, org_width, orig);
	if (result != org_width) {
	    long int jj;
	    for (jj=result; jj<num_bytes*org_width; jj++) {
		origbuf[jj]=0;
	    }
	}
	result=fread(pastebuf, num_bytes, paste_width, paste);
	if (result != paste_width) {
	    (void)fprintf(stderr, "pixpaste: paste image is short.\n");
	    base_y = paste_height = 0;
	}
	result = fwrite(origbuf, num_bytes, org_width, stdout);
	if (result != org_width) {
	    perror("pixpaste: fwrite3");
	    bu_exit(3, NULL);
	}
	row++;
    }

/*
 * Output the rest of the original file.
 */
    while (row < org_height) {
	result=fread(origbuf, num_bytes, org_width, orig);
	if (result != org_width) {
	    long int jj;
	    for (jj=result; jj<num_bytes*org_width; jj++) {
		origbuf[jj]=0;
	    }
	}
	result = fwrite(origbuf, num_bytes, org_width, stdout);
	if (result != org_width) {
	    perror("pixpaste: fwrite4");
	    bu_exit(3, NULL);
	}
	row++;
    }
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
