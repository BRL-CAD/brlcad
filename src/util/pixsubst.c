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
#include "bu/arg.h"

static void
read_channel(unsigned char *channel, const char *value, const char *label)
{
    struct bu_vls msg = BU_VLS_INIT_ZERO;

    if (!bu_arg_unsigned_char_from_str(&msg, channel, value, 10))
	bu_exit(1, "%s: %s\n", label, bu_vls_cstr(&msg));
    bu_vls_free(&msg);
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
	read_channel(&pixin[0], *argv, "input red value");
	argv++;
	read_channel(&pixin[1], *argv, "input green value");
	argv++;
	read_channel(&pixin[2], *argv, "input blue value");
    }

    argv++;
    read_channel(&pixout[0], *argv, "output red value");
    argv++;
    read_channel(&pixout[1], *argv, "output green value");
    argv++;
    read_channel(&pixout[2], *argv, "output blue value");

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
