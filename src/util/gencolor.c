/*                      G E N C O L O R . C
 * BRL-CAD
 *
 * Copyright (c) 1986-2014 United States Government as represented by
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
/** @file util/gencolor.c
 *
 * Output a pattern of bytes either forever or the requested
 * number of times.  It gets the byte values either from the
 * command line, or stdin.  Defaults to black.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include "bio.h"
#include "bu/log.h"
#include "bu/str.h"
#include "bu/getopt.h"

#define MAX_BYTES (128*1024)

static const char Usage[] = "Usage: gencolor [-r#] [val1 .. valN] > output_file\n";

int32_t count = -1;
int outputtype = 0; /* 1 for pix, 2 for bw;
 * if this is used and resolution is not yet set,
 * give resolution a default value of 1
 */
int resolution = 0; /* 1 for low-res, 2 for hi-res;
 * if this is used and outputtype is not yet set,
 * give outputtype a default value of 1
 */
int setrcount = 0; /* set to 1 if -r is detected */

unsigned char buf[MAX_BYTES];

void
printusage(int i)
{
    bu_log("%s\n", Usage);
    bu_log("  (Must redirect output; cannot send to tty)\n");
    if (i != 3) bu_exit(i, NULL);
}

void
get_args(int argc, char **argv)
{
    int c;

    bu_optind = 1;
    while ((c = bu_getopt(argc, argv, "r:pbLHh?")) != -1) {
	switch (c) {
	    case 'r':
		count = atoi(bu_optarg);
		if (count > INT32_MAX)
		    count = INT32_MAX;
		setrcount = 1;
		break;
	    case 'p':
		outputtype = 1;
		if (resolution == 0) resolution = 1;
		break;
	    case 'b':
		outputtype = 2;
		if (resolution == 0) resolution = 1;
		break;
	    case 'L':
		resolution = 1;
		if (outputtype == 0) outputtype = 1;
		break;
	    case 'H':
		resolution = 2;
		if (outputtype == 0) outputtype = 1;
		break;
	    default:		/* 'h' '?' */
		printusage(0);
	}
    }

    if( isatty(fileno(stdout))) printusage(1);
    if (argc == 1 ){
    	printusage(3);
	fprintf(stderr, "       Program continues running:\n");
    }

    return;
}

int
main(int argc, char **argv)
{
    int i, len, times, bytes_in_buf, copies_per_buf;
    int remainder = 0;
    int32_t basemultiple = 262144; /* This is 512 squared. */
    unsigned char *bp;

    get_args(argc, argv);
    argc = argc - bu_optind + 1;
    argv = argv + bu_optind - 1;

    if (argc > 1) {
	/* get values from the command line */
	i = 0;
	while (argc > 1 && i < MAX_BYTES) {
	    buf[i] = atoi(argv[i+1]);
	    argc--;
	    i++;
	}
	len = i;
    } else if (!isatty(fileno(stdin))) {
	/* get values from stdin */
	len = fread((char *)buf, 1, MAX_BYTES, stdin);
	if (len <= 0)
	    printusage(2);
    } else {
	/* assume black */
	buf[0] = 0;
	len = 1;
    }

/* If -r was used, ignore -p,-b,-L,-H */
    if (!setrcount) {
	if (outputtype == 1) {
	    if (resolution == 1)
		count = basemultiple*3;
	    else
		count = basemultiple*12;
	} else {
	    if (resolution == 1)
		count = basemultiple;
	    else
		count = basemultiple*4;
	}
	remainder = count % len;
	count = count/len; /* e.g., len is 3 for RGB for a pix file */
    }

finishup:

    /*
     * Replicate the pattern as many times as it will fit
     * in the buffer.
     */
    copies_per_buf = 1;
    bytes_in_buf = len;
    bp = &buf[len];
    while ((MAX_BYTES - bytes_in_buf) >= len) {
	for (i = 0; i < len; i++)
	    *bp++ = buf[i];
	copies_per_buf++;
	bytes_in_buf += len;
    }

    if (count <= 0) {
	/* output forever */
	while (1) {
	    if (write(1, (char *)buf, bytes_in_buf) != bytes_in_buf) {
		perror("write");
		break;
	    }
	}
	return 1;
    }

    while (count > 0) {
	times = copies_per_buf > count ? count : copies_per_buf;
	if (write(1, (char *)buf, len * times) != len * times) {
	    perror("write");
	    return 1;
	}
	count -= times;
    }

    if (remainder > 0) {
	count = 1;
	len = remainder;
	remainder = 0;
	goto finishup;
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
