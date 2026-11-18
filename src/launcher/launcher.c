/*                      L A U N C H E R . C
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
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
/** @file launcher.c
 *
 * The BRL-CAD application launcher.  Serves as the "main" BRL-CAD application
 * on platforms that expect one, presenting the GUI programs and a command
 * shell as a menu.  The set of entries is discovered at run time from manifest
 * files (see registry.c); the presentation is a libfb window (ui_fb.c) with an
 * automatic console fallback (ui_text.c).  It uses only existing BRL-CAD
 * libraries -- no Tcl/Tk or Qt.
 *
 */

#include "common.h"

#include <stdio.h>

#include "bu/arg.h"
#include "bu/app.h"
#include "bu/log.h"
#include "bu/malloc.h"
#include "bu/vls.h"

#include "launcher.h"


struct launcher_args {
    int help;
    int force_text;
    int force_gui;
    int list;
};

static const struct bu_arg_option launcher_options[] = {
    BU_ARG_FLAG("h", "help", struct launcher_args, help,
	"Print help and exit"),
    BU_ARG_FLAG("t", "text", struct launcher_args, force_text,
	"Force the console menu"),
    BU_ARG_FLAG("g", "gui", struct launcher_args, force_gui,
	"Request the graphical menu; report failure and use the console fallback"),
    BU_ARG_FLAG("l", "list", struct launcher_args, list,
	"List discovered applications and their availability, then exit"),
    BU_ARG_OPTION_NULL
};

static const char * const launcher_interface_options[] = {
    "text", "gui", NULL
};

static const struct bu_arg_constraint launcher_constraints[] = {
    BU_ARG_CONSTRAINT(BU_ARG_CONSTRAINT_OPTION_COUNT,
	BU_ARG_CONDITION_ALWAYS, launcher_interface_options, 0, 1,
	"select at most one interface mode"),
    BU_ARG_CONSTRAINT_NULL
};

static const struct bu_arg_schema launcher_schema = BU_ARG_SCHEMA(
    "brlcad-launcher", "Discover and start BRL-CAD applications",
    launcher_options, NULL, BU_ARG_PARSE_OPTIONS_FIRST,
    BU_ARG_SCHEMA_META_HELP(NULL, launcher_constraints, NULL, NULL, NULL));

static const struct bu_arg_grammar launcher_grammar = {
    &launcher_schema, NULL, NULL
};

static int
launcher_help(FILE *stream, int status)
{
    char *help = bu_arg_schema_help(&launcher_schema, "brlcad-launcher");
    bu_flog(stream, "%s", help ? help : "");
    bu_free(help, "launcher help");
    return status;
}


static void
list_apps(struct app_registry *r)
{
    int i;
    printf("Discovered BRL-CAD applications:\n");
    for (i = 0; i < r->count; i++) {
	struct app_entry *e = &r->apps[i];
	printf("  %-16s %-10s %s%s\n",
	       e->name,
	       e->exec,
	       e->description,
	       e->available ? "" : "  (not installed)");
    }
}


int
main(int argc, char *argv[])
{
    struct launcher_args args = {0};
    struct app_registry reg;
    struct bu_vls msg = BU_VLS_INIT_ZERO;
    int introspection;

    bu_setprogname(argv[0]);

    introspection = bu_arg_introspect(&msg, &launcher_grammar, argc - 1,
	(const char * const *)(argv + 1));
    if (introspection != 0) {
	bu_flog(introspection > 0 ? stdout : stderr, "%s", bu_vls_cstr(&msg));
	bu_vls_free(&msg);
	return introspection > 0 ? 0 : 1;
    }

    if (bu_arg_schema_parse_complete(&launcher_schema, &args, &msg,
	    argc - 1, (const char **)(argv + 1)) < 0) {
	bu_flog(stderr, "%s", bu_vls_cstr(&msg));
	bu_vls_free(&msg);
	return 1;
    }
    bu_vls_free(&msg);
    if (args.help)
	return launcher_help(stdout, 0);

    app_registry_init(&reg);
    app_registry_load(&reg);

    if (args.list) {
	list_apps(&reg);
	app_registry_free(&reg);
	return 0;
    }

    /* Prefer the graphical menu; fall back to the console menu if no window is
     * available (headless session, no windowed framebuffer backend, etc.). */
    if (!args.force_text) {
	if (ui_fb_run(&reg) == 0) {
	    app_registry_free(&reg);
	    return 0;
	}
	if (args.force_gui)
	    fprintf(stderr, "brlcad-launcher: no graphical framebuffer available; using text menu.\n");
    }

    ui_text_run(&reg);

    app_registry_free(&reg);
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
