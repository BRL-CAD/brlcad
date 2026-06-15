/*                     P I X B U S T U P . C
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
/** @file util/pixbustup.c
 *
 * Take concatenated .pix files, and write them into individual files.
 * Mostly a holdover from the days when RT wrote animations into one
 * huge file, but still occasionally useful.
 *
 */

#include "common.h"

#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include "bio.h"

#include "bu/app.h"
#include "bu/malloc.h"
#include "bu/log.h"
#include "bu/file.h"

int infd;
unsigned char *in1;

static size_t scanbytes;		/* # of bytes of scanline */
static size_t nlines;		/* Number of input lines */
static size_t pix_line;		/* Number of pixels/line */

static void
printUsage(void)
{
    bu_log("Usage: pixbustup basename width [image_offset] [first_number] <input.pix\n");
}

static int
parse_positive_size_arg(const char *arg, size_t *out_value, const char *label)
{
    char *end = NULL;
    unsigned long long value;

    errno = 0;
    value = strtoull(arg, &end, 10);
    if (errno != 0 || end == arg || *end != '\0' || value == 0 || value > (unsigned long long)((size_t)-1)) {
	bu_log("pixbustup: invalid %s '%s'\n", label, arg);
	return 0;
    }

    *out_value = (size_t)value;
    return 1;
}

static int
parse_nonnegative_size_arg(const char *arg, size_t *out_value, const char *label)
{
    char *end = NULL;
    unsigned long long value;

    errno = 0;
    value = strtoull(arg, &end, 10);
    if (errno != 0 || end == arg || *end != '\0' || value > (unsigned long long)((size_t)-1)) {
	bu_log("pixbustup: invalid %s '%s'\n", label, arg);
	return 0;
    }

    *out_value = (size_t)value;
    return 1;
}

static int
parse_nonnegative_offset_arg(const char *arg, b_off_t *out_value, const char *label)
{
    char *end = NULL;
    long long value;

    errno = 0;
    value = strtoll(arg, &end, 10);
    if (errno != 0 || end == arg || *end != '\0' || value < 0) {
	bu_log("pixbustup: invalid %s '%s'\n", label, arg);
	return 0;
    }

    *out_value = (b_off_t)value;
    return 1;
}


int
main(int argc, char **argv)
{
    b_off_t image_offset;
    size_t framenumber;
    char *base_name;
    char name[128];

    bu_setprogname(argv[0]);

    if (argc < 3 || argc > 5) {
	printUsage();
	return 1;
    }

    base_name = argv[1];
    if (!parse_positive_size_arg(argv[2], &nlines, "width")) {
	printUsage();
	return 1;
    }

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
	if (!parse_nonnegative_offset_arg(argv[3], &image_offset, "image offset")) {
	    printUsage();
	    return 1;
	}
	bu_lseek(0, image_offset*scanbytes, 0);
    }
    if (argc == 5) {
	if (!parse_nonnegative_size_arg(argv[4], &framenumber, "first frame number")) {
	    printUsage();
	    return 1;
	}
    } else {
	framenumber = 0;
    }

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

	ifname = bu_file_realpath(name, NULL);
	fd = creat(ifname, 0444);
	if (fd < 0) {
	    perror(ifname);
	    bu_free(ifname, "ifname alloc from bu_file_realpath");
	    continue;
	}
	bu_free(ifname, "ifname alloc from bu_file_realpath");

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
