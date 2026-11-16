/*                        F B F R E E . C
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
/** @file fbfree.c
 *
 * Free any resources associated with a frame buffer.
 * Just calls fb_free().
 *
 */

#include "common.h"

#include "bu/arg.h"
#include "bu/app.h"
#include "bu/log.h"
#include "bu/malloc.h"
#include "dm.h"

struct fbfree_args {
    int help;
    const char *framebuffer;
};

static const struct bu_arg_option fbfree_options[] = {
    BU_ARG_FLAG("h", "help", struct fbfree_args, help,
	"Print help and exit"),
    BU_ARG_ALIAS("?", NULL, "help", 1),
    BU_ARG_VALUE("F", "framebuffer", struct fbfree_args, framebuffer,
	BU_ARG_VALUE_STRING, BU_ARG_REQUIRED, "device",
	"Select the framebuffer device"),
    BU_ARG_OPTION_NULL
};

static const struct bu_arg_schema fbfree_schema = BU_ARG_SCHEMA(
    "fbfree", "Release resources associated with a framebuffer",
    fbfree_options, NULL, BU_ARG_PARSE_OPTIONS_FIRST,
    BU_ARG_SCHEMA_META_HELP(NULL, NULL, NULL, NULL, NULL));

static const struct bu_arg_grammar fbfree_grammar = {
    &fbfree_schema, NULL, NULL
};

int
main(int argc, char **argv)
{
    struct fbfree_args args = {0, NULL};
    struct bu_vls msg = BU_VLS_INIT_ZERO;
    struct fb *fbp;

    bu_setprogname(argv[0]);

    int introspection = bu_arg_introspect(&msg, &fbfree_grammar, argc - 1,
	(const char * const *)(argv + 1));
    if (introspection != 0) {
	bu_flog(introspection > 0 ? stdout : stderr, "%s", bu_vls_cstr(&msg));
	bu_vls_free(&msg);
	return introspection > 0 ? 0 : 1;
    }
    if (bu_arg_schema_parse_complete(&fbfree_schema, &args, &msg, argc - 1,
	    (const char **)(argv + 1)) < 0) {
	bu_flog(stderr, "%s", bu_vls_cstr(&msg));
	bu_vls_free(&msg);
	return 1;
    }
    bu_vls_free(&msg);
    if (args.help) {
	char *help = bu_arg_schema_help(&fbfree_schema, "fbfree");
	bu_flog(stdout, "%s", help ? help : "");
	bu_free(help, "fbfree help");
	return 0;
    }

    if ((fbp = fb_open(args.framebuffer, 0, 0)) == FB_NULL) {
	bu_flog(stderr, "fbfree: Can't open frame buffer\n");
	return 1;
    }
    return fb_free(fbp);
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
