/*                          D M O D . C
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
/** @file dmod.c
 *
 * Modify a stream of doubles.
 *
 * Allows any number of add, subtract, multiply, divide, or
 * exponentiation operations to be performed on a stream of values.
 *
 */

#include "common.h"

#include <math.h>
#include "bio.h"

#include "bu/app.h"
#include "bu/exit.h"

#include "mod_args.h"


int
main(int argc, char *argv[])
{
    int i, n;
    double *bp;
    double arg;
    int j;
    size_t ret;
    FILE *infp = stdin;
    const char *input_file = NULL;
    struct sig_mod_args args = {0};
    double buf[BU_PAGE_SIZE] = {0.0};		/* working buffer */

    bu_setprogname(argv[0]);

    int parse_status = sig_mod_args_parse(&args, &input_file, "dmod",
	"Apply ordered arithmetic operations to binary doubles", argc, argv);
    if (parse_status <= 0)
	return parse_status < 0 ? 1 : 0;
    if (input_file) {
	infp = fopen(input_file, "rb");
	if (!infp)
	    bu_exit(1, "dmod: cannot open \"%s\" for reading\n", input_file);
    }
    if (isatty(fileno(infp)) || isatty(fileno(stdout)))
	bu_exit(1, "dmod requires redirected binary input and output\n");
    setmode(fileno(infp), O_BINARY);
    setmode(fileno(stdout), O_BINARY);

    while ((n = fread(buf, sizeof(*buf), BU_PAGE_SIZE, infp)) > 0) {
	for (i = 0; i < (int)args.operations.count; i++) {
	    arg = args.operations.values[i].value;
	    switch (args.operations.values[i].type) {
		double d;

		case SIG_MOD_ADD:
		    bp = &buf[0];
		    for (j = n; j > 0; j--) {
			*bp++ += arg;
		    }
		    break;
		case SIG_MOD_MULTIPLY:
		    bp = &buf[0];
		    for (j = n; j > 0; j--) {
			*bp++ *= arg;
		    }
		    break;
		case SIG_MOD_POWER:
		    bp = &buf[0];
		    for (j = n; j > 0; j--) {
			d = pow(*bp, arg);
			*bp++ = d;
		    }
		    break;
		case SIG_MOD_ABSOLUTE:
		    bp = &buf[0];
		    for (j = n; j > 0; j--) {
			if (*bp < 0.0)
			    *bp = - *bp;
			bp++;
		    }
		    break;
		default:
		    break;
	    }
	}
	ret = fwrite(buf, sizeof(*buf), n, stdout);
	if (ret != (size_t)n)
	    perror("fwrite");
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
