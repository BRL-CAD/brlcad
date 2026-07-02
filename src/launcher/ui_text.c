/*                       U I _ T E X T . C
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
/** @file ui_text.c
 *
 * Console front end for the BRL-CAD launcher.  Used when no graphical
 * framebuffer window is available, guaranteeing the launcher runs everywhere.
 *
 */

#include "common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "bu.h"
#include "brlcad_version.h"

#include "launcher.h"


static void
print_banner(void)
{
    printf("\n");
    printf("   ____  ____  _        ____    _    ____\n");
    printf("  | __ )|  _ \\| |      / ___|  / \\  |  _ \\\n");
    printf("  |  _ \\| |_) | |     | |     / _ \\ | | | |\n");
    printf("  | |_) |  _ <| |___  | |___ / ___ \\| |_| |\n");
    printf("  |____/|_| \\_\\_____|  \\____/_/   \\_\\____/\n");
    printf("\n");
    printf("  BRL-CAD %d.%d.%d -- Application Launcher\n",
	   BRLCAD_VERSION_MAJOR, BRLCAD_VERSION_MINOR, BRLCAD_VERSION_PATCH);
    printf("\n");
}


static void
print_menu(const struct app_registry *r)
{
    int i;
    for (i = 0; i < r->count; i++) {
	const struct app_entry *e = &r->apps[i];
	printf("   %2d) %-16s %s%s\n",
	       i + 1,
	       e->name,
	       e->description,
	       e->available ? "" : "  (not installed)");
    }
    printf("    q) Quit\n");
    printf("\n  Select an option: ");
    fflush(stdout);
}


int
ui_text_run(struct app_registry *r)
{
    char line[64];

    if (!r || r->count == 0) {
	printf("No BRL-CAD applications were found.\n");
	return 0;
    }

    print_banner();

    for (;;) {
	char *s;
	long sel;

	print_menu(r);

	if (!fgets(line, sizeof(line), stdin)) {
	    printf("\n");
	    break;
	}

	s = line;
	while (*s && isspace((unsigned char)*s))
	    s++;

	if (*s == 'q' || *s == 'Q')
	    break;

	sel = strtol(s, NULL, 10);
	if (sel < 1 || sel > r->count) {
	    printf("  Please enter a number between 1 and %d, or q to quit.\n\n", r->count);
	    continue;
	}

	{
	    const struct app_entry *e = &r->apps[sel - 1];
	    if (!e->available) {
		printf("  '%s' is not installed on this system.\n\n", e->name);
		continue;
	    }
	    printf("  Launching %s ...\n\n", e->name);
	    if (app_launch(e) != 0)
		printf("  Failed to launch %s.\n\n", e->name);
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
