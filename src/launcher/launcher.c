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
#include <string.h>

#include "bu.h"

#include "launcher.h"


static const char *usage =
    "Usage: brlcad-launcher [options]\n"
    "  -t, --text    force the console (text) menu\n"
    "  -g, --gui     force the graphical (windowed) menu\n"
    "  -l, --list    list discovered applications and exit\n"
    "  -h, --help    show this help and exit\n";


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
    struct app_registry reg;
    int force_text = 0;
    int force_gui = 0;
    int do_list = 0;
    int i;

    bu_setprogname(argv[0]);

    for (i = 1; i < argc; i++) {
	if (BU_STR_EQUAL(argv[i], "-t") || BU_STR_EQUAL(argv[i], "--text")) {
	    force_text = 1;
	} else if (BU_STR_EQUAL(argv[i], "-g") || BU_STR_EQUAL(argv[i], "--gui")) {
	    force_gui = 1;
	} else if (BU_STR_EQUAL(argv[i], "-l") || BU_STR_EQUAL(argv[i], "--list")) {
	    do_list = 1;
	} else if (BU_STR_EQUAL(argv[i], "-h") || BU_STR_EQUAL(argv[i], "--help")) {
	    fputs(usage, stdout);
	    return 0;
	} else {
	    fprintf(stderr, "brlcad-launcher: unknown option '%s'\n\n", argv[i]);
	    fputs(usage, stderr);
	    return 1;
	}
    }

    app_registry_init(&reg);
    app_registry_load(&reg);

    if (do_list) {
	list_apps(&reg);
	app_registry_free(&reg);
	return 0;
    }

    /* Prefer the graphical menu; fall back to the console menu if no window is
     * available (headless session, no windowed framebuffer backend, etc.). */
    if (!force_text) {
	if (ui_fb_run(&reg) == 0) {
	    app_registry_free(&reg);
	    return 0;
	}
	if (force_gui)
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
