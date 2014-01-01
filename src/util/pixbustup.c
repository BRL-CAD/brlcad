/*                     P I X B U S T U P . C
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
/** @file util/pixbustup.c
 *
 * Take concatenated .pix files, and write them into individual files.
 * Mostly a holdover from the days when RT wrote animations into one
 * huge file, but still occasionally useful.
 *
 */

#include "common.h"

#include <stdlib.h>
#include "bio.h"

#include "bu.h"


int infd;
unsigned char *in1;

static size_t scanbytes;		/* # of bytes of scanline */
static size_t nlines;		/* Number of input lines */
static size_t pix_line;		/* Number of pixels/line */

static void
printUsage()
{
    bu_log("Usage: pixbustup basename width [image_offset] [first_number] <input.pix\n");
}


int
main(int argc, char **argv)
{
    off_t image_offset;
    size_t framenumber;
    char *base_name;
    char name[128];

    if (argc < 3) {
	printUsage();
	return 1;
    }

    base_name = argv[1];
    nlines = atoi(argv[2]);

    if (nlines < 1) {
	bu_log("ERROR: need width of at least 1 pixel.");
	printUsage();
	return 2;
    }
    if (nlines > UINT32_MAX) {
	bu_log("ERROR: not ready to handle images bigger than %x bytes square.", UINT32_MAX);
	printUsage();
	return 3;
    }

    pix_line = nlines;	/* Square pictures */
    scanbytes = nlines * pix_line * 3;
    in1 = (unsigned char *) malloc(scanbytes);

    if (argc == 4) {
	image_offset = atoi(argv[3]);
	lseek(0, image_offset*scanbytes, 0);
    }
    if (argc == 5)
	framenumber = atoi(argv[4]);
    else
	framenumber = 0;

    for (;; framenumber++) {
	int fd;
	int rwval = read(0, in1, scanbytes);
	char *ifname;

	if ((size_t)rwval != scanbytes) {
	    if (rwval < 0) {
		perror("pixbustup READ ERROR");
	    }
	    break;
	}
	snprintf(name, 128, "%s.%ld", base_name, (unsigned long)framenumber);

	ifname = bu_realpath(name, NULL);
	if ((fd=creat(ifname, 0444))<0) {
	    perror(ifname);
	    bu_free(ifname, "ifname alloc from bu_realpath");
	    continue;
	}
	bu_free(ifname, "ifname alloc from bu_realpath");

	rwval = write(fd, in1, scanbytes);
	if ((size_t)rwval != scanbytes) {
	    if (rwval < 0) {
		perror("pixbustup WRITE ERROR");
	    }
	}
	(void)close(fd);
	printf("wrote %s\n", name);
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
