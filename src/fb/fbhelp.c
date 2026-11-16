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

#include "bio.h"
#include "bu/arg.h"
#include "bu/app.h"
#include "bu/log.h"
#include "bu/malloc.h"
#include "dm.h"

struct fbhelp_args {
    int help;
    const char *framebuffer;
};

static const struct bu_arg_option fbhelp_options[] = {
    BU_ARG_FLAG("h", "help", struct fbhelp_args, help,
	"Print help and exit"),
    BU_ARG_ALIAS("?", NULL, "help", 1),
    BU_ARG_VALUE("F", "framebuffer", struct fbhelp_args, framebuffer,
	BU_ARG_VALUE_STRING, BU_ARG_REQUIRED, "device",
	"Select the framebuffer device"),
    BU_ARG_OPTION_NULL
};

static const struct bu_arg_schema fbhelp_schema = BU_ARG_SCHEMA(
    "fbhelp", "Describe available and selected framebuffer devices",
    fbhelp_options, NULL, BU_ARG_PARSE_OPTIONS_FIRST,
    BU_ARG_SCHEMA_META_HELP(NULL, NULL, NULL, NULL, NULL));

static const struct bu_arg_grammar fbhelp_grammar = {
    &fbhelp_schema, NULL, NULL
};

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
    struct fbhelp_args args = {0, NULL};
    struct bu_vls msg = BU_VLS_INIT_ZERO;
    int close_result;
    int help_result;
    int saved_stderr = -1;
    struct fb *fbp;

    bu_setprogname(argv[0]);

    int introspection = bu_arg_introspect(&msg, &fbhelp_grammar, argc - 1,
	(const char * const *)(argv + 1));
    if (introspection != 0) {
	bu_flog(introspection > 0 ? stdout : stderr, "%s", bu_vls_cstr(&msg));
	bu_vls_free(&msg);
	return introspection > 0 ? 0 : 1;
    }
    if (bu_arg_schema_parse_complete(&fbhelp_schema, &args, &msg, argc - 1,
	    (const char **)(argv + 1)) < 0) {
	bu_flog(stderr, "%s", bu_vls_cstr(&msg));
	bu_vls_free(&msg);
	return 1;
    }
    bu_vls_free(&msg);
    if (args.help) {
	char *help = bu_arg_schema_help(&fbhelp_schema, "fbhelp");
	bu_flog(stdout, "%s", help ? help : "");
	bu_free(help, "fbhelp help");
	return 0;
    }
    if ((fbp = fb_open(args.framebuffer, 0, 0)) == FB_NULL) {
	bu_flog(stderr, "fbhelp: Can't open frame buffer\n");
	return 1;
    }

    if (fbhelp_log_to_stdout(&saved_stderr) != 0) {
	bu_flog(stderr, "fbhelp: unable to consolidate help output\n");
	fb_close(fbp);
	return 1;
    }

    bu_flog(stdout, "\
A Frame Buffer display device is selected by\n\
setting the environment variable FB_FILE:\n\
(/bin/sh) FB_FILE=/dev/device; export FB_FILE\n\
(/bin/csh) setenv FB_FILE /dev/device\n\
Many programs also accept a \"-F framebuffer\" flag.\n\
Type \"man brlcad\" for more information.\n");

    bu_flog(stdout, "=============== Available Devices ================\n");
    help_result = fflush(stdout) == EOF;
    if (fb_genhelp() != 0)
	help_result = 1;

    bu_flog(stdout, "=============== Current Selection ================\n");
    if (fflush(stdout) == EOF)
	help_result = 1;
    if (fb_help(fbp) != 0)
	help_result = 1;
    if (fbhelp_restore_stderr(saved_stderr) != 0) {
	bu_flog(stderr, "fbhelp: unable to restore standard error\n");
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
