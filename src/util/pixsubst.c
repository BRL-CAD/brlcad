/*                      P I X S U B S T . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2026 United States Government as represented by
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
/** @file util/pixsubst.c
 *
 */

#include "common.h"

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#include "bu/app.h"
#include "bu/log.h"


static int
parse_byte_arg(const char *arg, unsigned char *out_value, const char *label)
{
    char *end = NULL;
    long int value;

    errno = 0;
    value = strtol(arg, &end, 10);
    if (errno != 0 || end == arg || *end != '\0' || value < 0 || value > 255) {
	bu_log("pixsubst: invalid %s '%s'\n", label, arg);
	return 0;
    }

    *out_value = (unsigned char)value;
    return 1;
}


int
main(int argc, char **argv)
{

    unsigned char pix[3], pixin[3], pixout[3];
    int npixels;
    size_t ret;

    bu_setprogname(argv[0]);

    if (argc != 4 && argc != 7) {
	bu_log("Usage: %s [R_in G_in B_in] R_out G_out B_out < pix_in > pix_out\n", argv[0]);
	bu_log("\t\tRGB_in is changed to RGB_out .\n");
	bu_log("\t\tIf RGB_in is not provided, the first pixel input is used.\n");
	bu_exit(1, NULL);
    }

    if (argc == 7) {
	argv++;
	if (!parse_byte_arg(*argv, &pixin[0], "input red value"))
	    bu_exit(1, NULL);
	argv++;
	if (!parse_byte_arg(*argv, &pixin[1], "input green value"))
	    bu_exit(1, NULL);
	argv++;
	if (!parse_byte_arg(*argv, &pixin[2], "input blue value"))
	    bu_exit(1, NULL);
    }

    argv++;
    if (!parse_byte_arg(*argv, &pixout[0], "output red value"))
	bu_exit(1, NULL);
    argv++;
    if (!parse_byte_arg(*argv, &pixout[1], "output green value"))
	bu_exit(1, NULL);
    argv++;
    if (!parse_byte_arg(*argv, &pixout[2], "output blue value"))
	bu_exit(1, NULL);

    if (argc == 4) {
	if ((npixels=fread(pixin, sizeof(unsigned char), 3, stdin)) != 3) {
	    bu_exit(1, "Unexpected end of input\n");
	}
	ret = fwrite(pixout, sizeof(unsigned char), npixels, stdout);
	if (ret < (size_t)npixels)
	    perror("fwrite");
    }

    while ((npixels=fread(pix, sizeof(unsigned char), 3, stdin)) == 3) {
	if (pix[0] == pixin[0] && pix[1] == pixin[1] && pix[2] == pixin[2]) {
	    ret = fwrite(pixout, sizeof(unsigned char), npixels, stdout);
	    if (ret < (size_t)npixels)
		perror("fwrite");
	} else {
	    ret = fwrite(pix, sizeof(unsigned char), npixels, stdout);
	    if (ret < (size_t)npixels)
		perror("fwrite");
	}
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
