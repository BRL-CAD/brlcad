/*              T E S T _ F B _ S T A N D A R D _ S T R E A M . C
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
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
#include "dm.h"


int
main(int argc, char **argv)
{
    const char *paths[] = {"/dev/stdout", "/dev/stderr"};
    size_t i;

    bu_setprogname(argv[0]);
    (void)argc;

#if defined(_WIN32) && !defined(__CYGWIN__)
    return 0;
#else
    for (i = 0; i < sizeof(paths) / sizeof(paths[0]); ++i) {
	struct fb *fbp = fb_open(paths[i], 1, 1);
	if (fbp == FB_NULL) {
	    fprintf(stderr, "failed to open %s as a framebuffer\n", paths[i]);
	    return 1;
	}
	if (fb_close(fbp) != 0) {
	    fprintf(stderr, "failed to close %s framebuffer\n", paths[i]);
	    return 1;
	}
    }

    if (fputs("standard output remains open\n", stdout) == EOF || fflush(stdout) == EOF)
	return 1;
    if (fputs("standard error remains open\n", stderr) == EOF || fflush(stderr) == EOF)
	return 1;
    return 0;
#endif
}
