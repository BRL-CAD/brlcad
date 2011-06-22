/*                      G E N C O L O R . C
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

#include "bu.h"


#define MAX_BYTES (128*1024)

static const char Usage[] = "usage: gencolor [-r#] [val1 .. valN] > output_file\n";

int bytes_in_buf, copies_per_buf;

unsigned char buf[MAX_BYTES];


int
main(int argc, char **argv)
{
    int i, len, times;
    long count;
    unsigned char *bp;

    if (argc < 1 || isatty(fileno(stdout))) {
	bu_exit(1, "%s", Usage);
    }

    count = -1;
    if (argc > 1 && strncmp(argv[1], "-r", 2) == 0) {
	count = atoi(&argv[1][2]);
	argv++;
	argc--;
    }

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
	if (len <= 0) {
	    bu_exit(2, "%s", Usage);
	}
    } else {
	/* assume black */
	buf[0] = 0;
	len = 1;
    }

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

    if (count < 0) {
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
