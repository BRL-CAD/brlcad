/*                      L A U N C H E R . H
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
/** @file launcher.h
 *
 * Shared declarations for the BRL-CAD application launcher: the dynamic
 * application registry and the two user-interface front ends (a libfb
 * windowed UI and a console fallback).
 *
 */

#ifndef LAUNCHER_H
#define LAUNCHER_H

#include "common.h"

#include "bu.h"

__BEGIN_DECLS

#define APP_NAME_LEN 128
#define APP_TEXT_LEN 256

/**
 * A single launchable entry, populated from a manifest file (or a built-in
 * fallback).  Nothing about the set of entries is compiled in: the registry is
 * discovered at run time.
 */
struct app_entry {
    char name[APP_NAME_LEN];		/**< display name, e.g. "MGED" */
    char exec[APP_NAME_LEN];		/**< executable basename, or "@shell" */
    char description[APP_TEXT_LEN];	/**< one-line description */
    char category[APP_NAME_LEN];	/**< grouping label */
    char icon[APP_NAME_LEN];		/**< optional icon file (unused for now) */
    int order;				/**< sort key (ascending) */
    int console;			/**< nonzero if launched attached to a console */
    int available;			/**< nonzero if the executable was resolved */
    char exec_path[MAXPATHLEN];		/**< resolved absolute path to the executable */
};

struct app_registry {
    struct app_entry *apps;
    int count;
    int cap;
};

/* registry.c */
extern void app_registry_init(struct app_registry *r);
extern int app_registry_load(struct app_registry *r);
extern void app_registry_free(struct app_registry *r);

/* launch.c */
extern int app_launch(const struct app_entry *e);

/* ui_fb.c -- returns 0 if a graphical window was used, -1 if none was available */
extern int ui_fb_run(struct app_registry *r);

/* ui_text.c -- console menu; always available */
extern int ui_text_run(struct app_registry *r);

__END_DECLS

#endif /* LAUNCHER_H */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
