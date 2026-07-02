/*                      R E G I S T R Y . C
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
/** @file registry.c
 *
 * Dynamic discovery of launchable BRL-CAD applications.
 *
 * At start-up the launcher scans a data directory for small "*.app" manifest
 * files and builds its menu from whatever it finds -- nothing is compiled in.
 * This mirrors the run-time plugin discovery pattern used elsewhere in BRL-CAD
 * (e.g. libgcv), so a new GUI program appears in the launcher simply by
 * installing a manifest alongside it, with no launcher change or rebuild.
 *
 * Manifest format (simple "key = value" text, '#' begins a comment):
 *
 *	name        = MGED
 *	exec        = mged
 *	description = Interactive geometry editor
 *	category    = Modeling
 *	order       = 10
 *	console     = 1
 *
 * The special exec value "@shell" denotes a command-line shell entry, resolved
 * by launch.c rather than by looking up an executable.
 *
 */

#include "common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "bu.h"

#include "launcher.h"

/* Relative location, under BU_DIR_DATA, of the manifest directory. */
#define APPS_SUBDIR "launcher/apps"


static struct app_entry *
registry_grow(struct app_registry *r)
{
    if (r->count >= r->cap) {
	int ncap = (r->cap > 0) ? (r->cap * 2) : 8;
	r->apps = (struct app_entry *)bu_realloc(r->apps, ncap * sizeof(struct app_entry), "app_registry");
	r->cap = ncap;
    }
    memset(&r->apps[r->count], 0, sizeof(struct app_entry));
    return &r->apps[r->count++];
}


/* Trim leading and trailing ASCII whitespace in place, returning the start. */
static char *
trim(char *s)
{
    char *end;
    while (*s && isspace((unsigned char)*s))
	s++;
    if (*s == '\0')
	return s;
    end = s + strlen(s) - 1;
    while (end > s && isspace((unsigned char)*end))
	*end-- = '\0';
    return s;
}


/*
 * Resolve an entry's executable to an absolute path and record whether it is
 * available.  The "@shell" pseudo-entry is always considered available.
 */
static void
registry_resolve(struct app_entry *e)
{
    if (e->exec[0] == '@') {
	bu_strlcpy(e->exec_path, e->exec, sizeof(e->exec_path));
	e->available = 1;
	return;
    }

    bu_dir(e->exec_path, sizeof(e->exec_path), BU_DIR_BIN, e->exec, BU_DIR_EXT, NULL);
    e->available = bu_file_exists(e->exec_path, NULL) ? 1 : 0;
}


/*
 * Parse a single manifest file into a fresh registry entry.  Returns 1 on
 * success, 0 if the file could not be read or lacked required fields.
 */
static int
registry_parse(struct app_registry *r, const char *path)
{
    FILE *fp;
    char line[APP_TEXT_LEN * 2];
    struct app_entry tmp;

    fp = fopen(path, "r");
    if (!fp)
	return 0;

    memset(&tmp, 0, sizeof(tmp));
    tmp.order = 1000;

    while (fgets(line, sizeof(line), fp)) {
	char *key, *val, *eq;

	/* strip comments */
	eq = strchr(line, '#');
	if (eq)
	    *eq = '\0';

	eq = strchr(line, '=');
	if (!eq)
	    continue;
	*eq = '\0';
	key = trim(line);
	val = trim(eq + 1);
	if (key[0] == '\0' || val[0] == '\0')
	    continue;

	if (BU_STR_EQUAL(key, "name"))
	    bu_strlcpy(tmp.name, val, sizeof(tmp.name));
	else if (BU_STR_EQUAL(key, "exec"))
	    bu_strlcpy(tmp.exec, val, sizeof(tmp.exec));
	else if (BU_STR_EQUAL(key, "description"))
	    bu_strlcpy(tmp.description, val, sizeof(tmp.description));
	else if (BU_STR_EQUAL(key, "category"))
	    bu_strlcpy(tmp.category, val, sizeof(tmp.category));
	else if (BU_STR_EQUAL(key, "icon"))
	    bu_strlcpy(tmp.icon, val, sizeof(tmp.icon));
	else if (BU_STR_EQUAL(key, "order"))
	    tmp.order = atoi(val);
	else if (BU_STR_EQUAL(key, "console"))
	    tmp.console = atoi(val);
    }
    fclose(fp);

    if (tmp.name[0] == '\0' || tmp.exec[0] == '\0')
	return 0;

    registry_resolve(&tmp);
    *registry_grow(r) = tmp;
    return 1;
}


/*
 * Provide a reasonable default menu when no manifests are installed (e.g. when
 * running directly from a build tree).  Entries whose executable is not present
 * are still added but flagged unavailable, so the UI can gray them out.
 */
static void
registry_add_builtins(struct app_registry *r)
{
    static const struct {
	const char *name;
	const char *exec;
	const char *desc;
	const char *cat;
	int order;
	int console;
    } defs[] = {
	{ "MGED",     "mged",     "Interactive geometry editor",   "Modeling",   10, 1 },
	{ "Archer",   "archer",   "Geometry editor and modeler",   "Modeling",   20, 0 },
	{ "RtWizard", "rtwizard", "Compose raytraced images",      "Rendering",  30, 0 },
	{ "ISST",     "isst",     "Real-time raytrace viewer",     "Rendering",  40, 0 },
	{ "Command Shell", "@shell", "Shell with BRL-CAD tools on PATH", "Tools", 90, 1 }
    };
    size_t i;

    for (i = 0; i < sizeof(defs) / sizeof(defs[0]); i++) {
	struct app_entry *e = registry_grow(r);
	bu_strlcpy(e->name, defs[i].name, sizeof(e->name));
	bu_strlcpy(e->exec, defs[i].exec, sizeof(e->exec));
	bu_strlcpy(e->description, defs[i].desc, sizeof(e->description));
	bu_strlcpy(e->category, defs[i].cat, sizeof(e->category));
	e->order = defs[i].order;
	e->console = defs[i].console;
	registry_resolve(e);
    }
}


static int
registry_cmp(const void *a, const void *b)
{
    const struct app_entry *ea = (const struct app_entry *)a;
    const struct app_entry *eb = (const struct app_entry *)b;
    if (ea->order != eb->order)
	return ea->order - eb->order;
    return bu_strcmp(ea->name, eb->name);
}


void
app_registry_init(struct app_registry *r)
{
    if (!r)
	return;
    r->apps = NULL;
    r->count = 0;
    r->cap = 0;
}


int
app_registry_load(struct app_registry *r)
{
    char dir[MAXPATHLEN] = {0};
    char **files = NULL;
    size_t nfiles = 0;
    size_t i;
    int i2;
    int navail = 0;

    if (!r)
	return 0;

    bu_dir(dir, sizeof(dir), BU_DIR_DATA, APPS_SUBDIR, NULL);

    if (bu_file_directory(dir))
	nfiles = bu_file_list(dir, "*.app", &files);

    for (i = 0; i < nfiles; i++) {
	char path[MAXPATHLEN] = {0};
	if (bu_file_directory(files[i]))
	    continue;
	bu_dir(path, sizeof(path), BU_DIR_DATA, APPS_SUBDIR, files[i], NULL);
	(void)registry_parse(r, path);
    }
    if (files)
	bu_argv_free(nfiles, files);

    /* Nothing discovered -- fall back to a sensible built-in set. */
    if (r->count == 0)
	registry_add_builtins(r);

    if (r->count > 1)
	qsort(r->apps, r->count, sizeof(struct app_entry), registry_cmp);

    for (i2 = 0; i2 < r->count; i2++)
	if (r->apps[i2].available)
	    navail++;

    return navail;
}


void
app_registry_free(struct app_registry *r)
{
    if (!r)
	return;
    if (r->apps)
	bu_free(r->apps, "app_registry");
    r->apps = NULL;
    r->count = 0;
    r->cap = 0;
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
