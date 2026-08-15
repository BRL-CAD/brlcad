/*                         K I L L T R E E . C
 * BRL-CAD
 *
 * Copyright (c) 2008-2026 United States Government as represented by
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
/** @file libged/killtree.c
 *
 * The killtree command.
 *
 */

#include "common.h"

#include <string.h>

#include "bu/hash.h"
#include "bu/opt.h"

#include "../ged_private.h"


#define AV_STEP BU_PAGE_SIZE


struct killtree_data {
    struct ged *gedp;
    int killrefs;
    int print;
    int force;
    const char *top;
    int ac;
    char **av;
    size_t av_capacity;
    int pending_ac;
    char **pending;
    size_t pending_capacity;
    struct bu_hash_tbl *queued;
    struct bu_hash_tbl *visited;
};


struct killtree_args {
    int killrefs;
    int force;
    int print;
};


#include "../include/plugin.h"

#define KILLTREE_OPTIONS(args) \
    BU_OPT_FLAG(args, "a", NULL, killrefs, \
	"Remove references from outside the deleted trees"), \
    BU_OPT_FLAG(args, "f", NULL, force, \
	"Force deletion of protected global data"), \
    BU_OPT_FLAG(args, "n", NULL, print, \
	"Report changes without modifying the database"),

BU_OPT_DESC_BUILDER(killtree_options, struct killtree_args, KILLTREE_OPTIONS);
static const ged_opt_spec killtree_opt_spec =
    GED_OPT("killtree", "Delete database object trees",
	killtree_options, "options-first objects:object+");


/* this finds references to 'obj' that are not within the 'topobj'
 * combination hierarchy, elsewhere in the database.  this is so we
 * can make sure we don't kill objects that are referenced somewhere
 * else in the database.
 */
static int
find_reference(struct db_i *dbip, const char *topobj, const char *obj)
{
    int ret;
    struct bu_vls str = BU_VLS_INIT_ZERO;

    if (!dbip || !topobj || !obj)
	return 0;

    /* FIXME: these should be wrapped in quotes, but need to dewrap
     * after bu_argv_from_string().
     */
    bu_vls_printf(&str, "-depth >0 -not -below -name %s -name %s", topobj, obj);

    ret = db_search(NULL, DB_SEARCH_TREE, bu_vls_cstr(&str), 0, NULL, dbip, NULL, NULL, NULL);

    bu_vls_free(&str);

    return ret;
}


static void
append_name(char ***names, int *count, size_t *capacity, const char *name)
{
    if ((size_t)(*count + 2) >= *capacity) {
	*names = (char **)bu_realloc(*names, sizeof(char *) * (*capacity + AV_STEP), "grow name array");
	*capacity += AV_STEP;
    }

    (*names)[(*count)++] = bu_strdup(name);
    (*names)[*count] = NULL;
}


static void
collect_callback(struct db_i *dbip, struct directory *dp, void *ptr)
{
    struct killtree_data *gktdp = (struct killtree_data *)ptr;
    int ref_exists = 0;
    const uint8_t *name;
    size_t name_len;

    if (dbip == DBI_NULL || dp == RT_DIR_NULL)
	return;

    name = (const uint8_t *)dp->d_namep;
    name_len = strlen(dp->d_namep);

    if (bu_hash_get(gktdp->visited, name, name_len)
	|| bu_hash_get(gktdp->queued, name, name_len))
	return;

    /* don't bother checking for references if the -f or -a flags are
     * presented to force a full kill and all references respectively.
     */
    if (!gktdp->force && !gktdp->killrefs)
	ref_exists = find_reference(dbip, gktdp->top, dp->d_namep);

    /* if a reference exists outside of the subtree we're killing, we
     * don't kill this object or it'll create invalid reference
     * elsewhere in the database.  do nothing.
     */
    if (ref_exists)
	return;

    (void)bu_hash_set(gktdp->queued, name, name_len, gktdp);
    append_name(&gktdp->pending, &gktdp->pending_ac,
		&gktdp->pending_capacity, dp->d_namep);
}


/* Deletion is deliberately separate from collection.  db_treewalk_basic()
 * resolves every reference as it walks, so mutating the directory from its
 * callback makes a repeated leaf look like a missing object later in the same
 * traversal.
 */
static void
killtree_process(struct db_i *dbip, struct directory *dp, struct killtree_data *gktdp)
{
    const uint8_t *name;
    size_t name_len;

    if (dbip == DBI_NULL || dp == RT_DIR_NULL)
	return;

    name = (const uint8_t *)dp->d_namep;
    name_len = strlen(dp->d_namep);

    if (bu_hash_get(gktdp->visited, name, name_len))
	return;

    (void)bu_hash_set(gktdp->visited, name, name_len, gktdp);

    if (gktdp->print) {
	if (!gktdp->killrefs)
	    bu_vls_printf(gktdp->gedp->ged_result_str, "%s ", dp->d_namep);
	else {
	    append_name(&gktdp->av, &gktdp->ac, &gktdp->av_capacity, dp->d_namep);

	    bu_vls_printf(gktdp->gedp->ged_result_str, "%s ", dp->d_namep);
	}
    } else {
	_dl_eraseAllNamesFromDisplay(gktdp->gedp, dp->d_namep, 0);

	bu_vls_printf(gktdp->gedp->ged_result_str, "KILL %s:  %s\n",
		      (dp->d_flags & RT_DIR_COMB) ? "COMB" : "Solid",
		      dp->d_namep);

	if (!gktdp->killrefs) {
	    if (db_delete(dbip, dp) != 0 || db_dirdelete(dbip, dp) != 0) {
		bu_vls_printf(gktdp->gedp->ged_result_str, "an error occurred while deleting %s\n", dp->d_namep);
	    }
	} else {
	    append_name(&gktdp->av, &gktdp->ac, &gktdp->av_capacity, dp->d_namep);

	    if (db_delete(dbip, dp) != 0 || db_dirdelete(dbip, dp) != 0) {
		bu_vls_printf(gktdp->gedp->ged_result_str, "an error occurred while deleting %s\n", dp->d_namep);

		/* Remove from list */
		bu_free((void *)gktdp->av[--gktdp->ac], "killtree_callback");
		gktdp->av[gktdp->ac] = (char *)0;
	    }
	}
    }
}


int
ged_killtree_core(struct ged *gedp, int argc, const char *argv[])
{
    struct directory *dp;
    int i;
    struct killtree_data gktd;
    struct killtree_args args = {0, 0, 0};
    int object_count = 0;
    const char **objects = NULL;
    const char *command = argv[0];

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_READ_ONLY(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	ged_cmd_help_append(gedp->ged_result_str, command, command);
	return GED_HELP;
    }

    argc--; argv++;
    object_count = bu_opt_parse_build_with_policy(gedp->ged_result_str,
	argc, argv, killtree_options, &args, BU_OPT_PARSE_OPTIONS_FIRST);
    if (object_count < 1) {
	ged_cmd_help_append(gedp->ged_result_str, command, command);
	return BRLCAD_ERROR;
    }

    objects = argv;

    gktd.gedp = gedp;
    gktd.killrefs = args.killrefs;
    gktd.print = args.print;
    gktd.force = args.force;
    gktd.ac = 1;
    gktd.top = NULL;
    gktd.pending_ac = 0;
    gktd.queued = NULL;
    gktd.visited = bu_hash_create(0);

    gktd.av = (char **)bu_calloc(1, sizeof(char *) * AV_STEP, "alloc av");
    gktd.av_capacity = AV_STEP;
    gktd.pending = (char **)bu_calloc(1, sizeof(char *) * AV_STEP, "alloc pending names");
    gktd.pending_capacity = AV_STEP;
    gktd.av[0] = "killrefs";
    gktd.av[1] = (char *)0;

    if (gktd.print) {
	gktd.av[gktd.ac++] = bu_strdup("-n");
	gktd.av[gktd.ac] = (char *)0;
    }


    /* Update references once before we start all of this - db_search
     * needs nref to be current to work correctly. */
    db_update_nref(gedp->dbip);


    /* Objects that would be killed are in the first sublist */
    if (gktd.print)
	bu_vls_printf(gedp->ged_result_str, "{");

    for (i = 0; i < object_count; i++) {
	dp = db_lookup(gedp->dbip, objects[i], LOOKUP_QUIET);
	if (dp == RT_DIR_NULL) {
	    size_t name_len = strlen(objects[i]);
	    if (!bu_hash_get(gktd.visited, (const uint8_t *)objects[i], name_len))
		(void)db_lookup(gedp->dbip, objects[i], LOOKUP_NOISY);
	    continue;
	}

	/* ignore phony objects */
	if (dp->d_addr == RT_DIR_PHONY_ADDR)
	    continue;

	/* stash the what's killed so we can find refs elsewhere */
	gktd.top = objects[i];

	gktd.queued = bu_hash_create(0);
	db_treewalk_basic(gedp->dbip, dp,
		    collect_callback, collect_callback, (void *)&gktd);

	for (int j = 0; j < gktd.pending_ac; j++) {
	    dp = db_lookup(gedp->dbip, gktd.pending[j], LOOKUP_QUIET);
	    killtree_process(gedp->dbip, dp, &gktd);
	    bu_free(gktd.pending[j], "pending killtree name");
	    gktd.pending[j] = NULL;
	}
	gktd.pending_ac = 0;
	bu_hash_destroy(gktd.queued);
	gktd.queued = NULL;
    }

    /* Close the sublist of would-be killed objects. Also open the
     * sublist of objects that reference the would-be killed objects.
     */
    if (gktd.print)
	bu_vls_printf(gedp->ged_result_str, "} {");

    if (gktd.killrefs && gktd.ac > 1) {
	gedp->ged_internal_call = 1;
	(void)ged_exec_killrefs(gedp, gktd.ac, (const char **)gktd.av);
	gedp->ged_internal_call = 0;

	for (i = 1; i < gktd.ac; i++) {
	    if (!gktd.print)
		bu_vls_printf(gedp->ged_result_str, "Freeing %s\n", gktd.av[i]);
	    bu_free((void *)gktd.av[i], "killtree_data");
	    gktd.av[i] = NULL;
	}
    }

    if (gktd.print)
	bu_vls_printf(gedp->ged_result_str, "}");

    bu_free(gktd.av, "free av");
    gktd.av = NULL;
    bu_free(gktd.pending, "free pending names");
    gktd.pending = NULL;
    bu_hash_destroy(gktd.visited);
    gktd.visited = NULL;

    /* Done removing stuff - update references. */
    db_update_nref(gedp->dbip);

    return BRLCAD_OK;
}

#define GED_KILLTREE_COMMANDS(X, XID) \
    X(killtree, ged_killtree_core, GED_CMD_DEFAULT, &killtree_opt_spec) \

GED_DECLARE_COMMAND_SET_WITH_OPT_SPEC(GED_KILLTREE_COMMANDS)
GED_DECLARE_PLUGIN_MANIFEST_WITH_OPT_SPEC("libged_killtree", 1, GED_KILLTREE_COMMANDS)

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
