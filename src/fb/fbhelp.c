/*                        F B H E L P . C
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
 *
 */
/** @file fbhelp.c
 *
 * Print out info about the selected frame buffer.
 * Just calls dm.help().
 *
 */

#include "common.h"

#include <stdlib.h>

#include "bio.h"
#include "bu/app.h"
#include "bu/getopt.h"
#include "dm.h"


static char *framebuffer = NULL;

static char usage[] = "\
Usage: fbhelp [-F framebuffer]\n";

static int
fbhelp_log_to_stdout(int *saved_stderr)
{
    if (fflush(stdout) == EOF || fflush(stderr) == EOF)
	return -1;

    *saved_stderr = dup(fileno(stderr));
    if (*saved_stderr == -1)
	return -1;

    if (dup2(fileno(stdout), fileno(stderr)) == -1) {
	close(*saved_stderr);
	*saved_stderr = -1;
	return -1;
    }

    return 0;
}

static int
fbhelp_restore_stderr(int saved_stderr)
{
    int ret = 0;

    if (fflush(stderr) == EOF)
	ret = -1;
    if (dup2(saved_stderr, fileno(stderr)) == -1)
	ret = -1;
    close(saved_stderr);
    return ret;
}

int
main(int argc, char **argv)
{
    int c;
    int close_result;
    int help_result;
    int saved_stderr = -1;
    struct fb *fbp;

    bu_setprogname(argv[0]);

    while ((c = bu_getopt(argc, argv, "F:h?")) != -1) {
	switch (c) {
	    case 'F':
		framebuffer = bu_optarg;
		break;
	    default:		/* '?' */
		(void)fputs(usage, stderr);
		return 1;
	}
    }
    if (argc > bu_optind) {
	fprintf(stderr, "fbhelp: excess argument(s) not supported\n");
	(void)fputs(usage, stderr);
	return 1;
    }
    if ((fbp = fb_open(framebuffer, 0, 0)) == FB_NULL) {
	fprintf(stderr, "fbhelp: Can't open frame buffer\n");
	return 1;
    }

    if (fbhelp_log_to_stdout(&saved_stderr) != 0) {
	fprintf(stderr, "fbhelp: unable to consolidate help output\n");
	fb_close(fbp);
	return 1;
    }

    fprintf(stdout, "\
A Frame Buffer display device is selected by\n\
setting the environment variable FB_FILE:\n\
(/bin/sh) FB_FILE=/dev/device; export FB_FILE\n\
(/bin/csh) setenv FB_FILE /dev/device\n\
Many programs also accept a \"-F framebuffer\" flag.\n\
Type \"man brlcad\" for more information.\n");

    fprintf(stdout, "=============== Available Devices ================\n");
    help_result = fflush(stdout) == EOF;
    if (fb_genhelp() != 0)
	help_result = 1;

    fprintf(stdout, "=============== Current Selection ================\n");
    if (fflush(stdout) == EOF)
	help_result = 1;
    if (fb_help(fbp) != 0)
	help_result = 1;
    if (fbhelp_restore_stderr(saved_stderr) != 0) {
	fprintf(stderr, "fbhelp: unable to restore standard error\n");
	fb_close(fbp);
	return 1;
    }

    close_result = fb_close(fbp);
    return help_result != 0 || close_result != 0;
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
