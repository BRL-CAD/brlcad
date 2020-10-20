/*                         P L O T 3 . C
 * BRL-CAD
 *
 * Copyright (c) 2014-2020 United States Government as represented by
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

#include "common.h"

#include <stdio.h>

#include "bu/app.h"
#include "bu/malloc.h"
#include "bu/log.h"
#include "vmath.h"
#include "bu/file.h"
#include "bu/opt.h"
#include "bn/plot3.h"

int
main(int argc, const char *argv[])
{
    int ret = 0;
    int print_help = 0;
    int binary_mode = 0;
    int text_mode = 0;
    int mode = -1;
    struct bu_opt_desc d[4];
    BU_OPT(d[0], "h", "help",   "",  NULL, &print_help,  "Print help and exit");
    BU_OPT(d[1], "b", "binary", "",  NULL, &binary_mode, "Process plot file as binary plot data (default)");
    BU_OPT(d[2], "t", "text",   "",  NULL, &text_mode,   "Process plot file as text plot data");
    BU_OPT_NULL(d[3]);

    bu_setprogname(argv[0]);

    argc-=(argc>0); argv+=(argc>0); /* done with command name argv[0] */

    int opt_ret = bu_opt_parse(NULL, argc, argv, d);

    if (!opt_ret)
	bu_exit(1, "Usage: %s [opts] file\n", argv[0]);

    if (binary_mode && text_mode)
	bu_exit(1, "Error - specify either binary mode or text mode\n");


    if (binary_mode)
	mode = PL_OUTPUT_MODE_BINARY;

    if (text_mode)
	mode = PL_OUTPUT_MODE_BINARY;

    if (!bu_file_exists(argv[0], NULL)) {
	bu_exit(1, "file %s not found\n", argv[0]);
    }

    FILE *fp = fopen(argv[0], "rb");

    /* A non-readable file isn't a valid file */
    if (!fp)
	bu_exit(1, "Error - could not open file %s\n", argv[0]);

    ret = plot3_invalid(fp, mode);

    if (ret) {
	bu_log("INVALID: %s\n", argv[0]);
    } else {
	bu_log("VALID:   %s\n", argv[0]);
    }

    return ret;
}


/** @} */
/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
