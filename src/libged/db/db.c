/*                           D B . C
 * BRL-CAD
 *
 * Copyright (c) 2008-2026 United States Government as represented by
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
/** @file libged/db.c
 *
 * Database-oriented commands and the common "db" command namespace.
 *
 * Applications may extend this namespace by registering a BU_CLBK_DURING
 * callback on "db".  The callback is invoked only when the requested
 * subcommand is not in the libged table below, and receives the complete
 * original argv (beginning with "db").
 */

#include "common.h"

#include <string.h>

#include "bu/cmdschema.h"
#include "bu/malloc.h"

#include "../ged_private.h"


struct _ged_db_info {
    struct ged *gedp;
    int help;
};

struct db_find_args {
    int all;
    int help;
};

static const struct bu_cmd_option db_find_options[] = {
    BU_CMD_FLAG("a", NULL, struct db_find_args, all,
	"Include hidden combinations"),
    BU_CMD_FLAG("h", "help", struct db_find_args, help,
	"Print help"),
    BU_CMD_ALIAS_SHORT("?", "help", 1),
    BU_CMD_OPTION_NULL
};
static const struct bu_cmd_operand db_find_operands[] = {
    BU_CMD_OPERAND("object", BU_CMD_VALUE_DB_OBJECT, 1,
	BU_CMD_COUNT_UNLIMITED, "Objects to search for", "ged.db_object"),
    BU_CMD_OPERAND_NULL
};
static const struct bu_cmd_schema db_find_schema = {
    "find", "List combinations that reference specified objects",
    db_find_options, db_find_operands, BU_CMD_PARSE_OPTIONS_FIRST, {NULL}
};
static const struct bu_cmd_option db_find_dispatch_options[] = {
    BU_CMD_FLAG_UNBOUND("a", NULL, "a", "Include hidden combinations"),
    BU_CMD_FLAG_UNBOUND("h", "help", "h", "Print help"),
    BU_CMD_FLAG_UNBOUND("?", NULL, "?", "Print help"),
    BU_CMD_OPTION_NULL
};
static const struct bu_cmd_schema db_find_dispatch_schema = {
    "find", "List combinations that reference specified objects",
    db_find_dispatch_options, NULL, BU_CMD_PARSE_OPTIONS_FIRST, {NULL}
};
static const struct bu_cmd_schema db_version_schema = {
    "version", "Report the open database format version", NULL, NULL,
    BU_CMD_PARSE_OPTIONS_FIRST, {NULL}
};
static const struct bu_cmd_option db_root_options[] = {
    BU_CMD_FLAG("h", "help", struct _ged_db_info, help, "Print help"),
    BU_CMD_ALIAS_SHORT("?", "help", 1),
    BU_CMD_OPTION_NULL
};
static const struct bu_cmd_schema db_root_schema = {
    "db", "Database commands", db_root_options, NULL,
    BU_CMD_PARSE_OPTIONS_FIRST, {NULL}
};


static int
_db_cmd_msgs(void *bs, int argc, const char **argv, const char *usage, const char *purpose)
{
    struct _ged_db_info *gd = (struct _ged_db_info *)bs;

    if (argc == 2 && BU_STR_EQUAL(argv[1], HELPFLAG)) {
	bu_vls_printf(gd->gedp->ged_result_str, "%s\n%s\n", usage, purpose);
	return 1;
    }
    if (argc == 2 && BU_STR_EQUAL(argv[1], PURPOSEFLAG)) {
	bu_vls_printf(gd->gedp->ged_result_str, "%s\n", purpose);
	return 1;
    }

    return 0;
}


static void
find_ref(struct db_i *dbip,
	 struct rt_comb_internal *comb,
	 union tree *comb_leaf,
	 void *object,
	 void *comb_name_ptr,
	 void *user_ptr3,
	 void *UNUSED(user_ptr4))
{
    char *obj_name;
    char *comb_name;
    struct ged *gedp = (struct ged *)user_ptr3;

    if (dbip) RT_CK_DBI(dbip);
    if (comb) RT_CK_COMB(comb);
    RT_CK_TREE(comb_leaf);

    obj_name = (char *)object;
    if (!BU_STR_EQUAL(comb_leaf->tr_l.tl_name, obj_name))
	return;

    comb_name = (char *)comb_name_ptr;
    bu_vls_printf(gedp->ged_result_str, "%s ", comb_name);
}


static int
_db_find_core(struct ged *gedp, int argc, const char *argv[])
{
    int k;
    struct directory *dp;
    struct rt_db_internal intern;
    struct rt_comb_internal *comb = (struct rt_comb_internal *)NULL;
    int operand_index;
    struct db_find_args args = {0, 0};
    static const char *usage = "<objects>";

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    bu_vls_trunc(gedp->ged_result_str, 0);

    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    operand_index = bu_cmd_schema_parse_complete(&db_find_schema, &args,
	gedp->ged_result_str, argc - 1, argv + 1);
    if (operand_index < 0)
	return BRLCAD_ERROR;
    if (args.help) {
	char *help = bu_cmd_schema_describe(&db_find_schema);
	bu_vls_printf(gedp->ged_result_str, "Usage: %s [-a] <objects>\n%s",
	    argv[0], help ? help : "");
	if (help)
	    bu_free(help, "db find help");
	return GED_HELP;
    }
    argc -= operand_index + 1;
    argv += operand_index + 1;

    FOR_ALL_DIRECTORY_START(dp, gedp->dbip)
	if (!(dp->d_flags & RT_DIR_COMB) ||
	    (!args.all && (dp->d_flags & RT_DIR_HIDDEN)))
	    continue;

	if (rt_db_get_internal(&intern, dp, gedp->dbip, (fastf_t *)NULL) < 0) {
	    bu_vls_printf(gedp->ged_result_str, "Database read error, aborting");
	    return BRLCAD_ERROR;
	}

	comb = (struct rt_comb_internal *)intern.idb_ptr;
	for (k = 1; k < argc; k++)
	    db_tree_funcleaf(gedp->dbip,
			     comb,
			     comb->tree,
			     find_ref,
			     (void *)argv[k],
			     (void *)dp->d_namep,
			     (void *)gedp,
			     (void *)NULL);

	rt_db_free_internal(&intern);
    FOR_ALL_DIRECTORY_END;

    return BRLCAD_OK;
}


int
ged_find_core(struct ged *gedp, int argc, const char *argv[])
{
    if (argc > 0 && argv && argv[0])
	bu_log("WARNING: top-level '%s' is deprecated and will be removed in a future release - use 'db find' instead.\n", argv[0]);

    return _db_find_core(gedp, argc, argv);
}


static int
_db_version_core(struct ged *gedp, int argc, const char *argv[])
{
    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    bu_vls_trunc(gedp->ged_result_str, 0);

    if (argc != 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s", argv[0]);
	return BRLCAD_ERROR;
    }

    bu_vls_printf(gedp->ged_result_str, "%d", db_version(gedp->dbip));
    return BRLCAD_OK;
}


int
ged_db_version_core(struct ged *gedp, int argc, const char *argv[])
{
    if (argc > 0 && argv && argv[0])
	bu_log("WARNING: top-level '%s' is deprecated and will be removed in a future release - use 'db version' instead.\n", argv[0]);

    return _db_version_core(gedp, argc, argv);
}


static int
_db_cmd_find(void *bs, int argc, const char **argv)
{
    const char *usage = "db find [-a] <objects>";
    const char *purpose = "list combinations that reference specified objects";
    struct _ged_db_info *gd = (struct _ged_db_info *)bs;

    if (_db_cmd_msgs(bs, argc, argv, usage, purpose))
	return BRLCAD_OK;

    if (argc == 1) {
	bu_vls_printf(gd->gedp->ged_result_str, "Usage: %s", usage);
	return GED_HELP;
    }

    return _db_find_core(gd->gedp, argc, argv);
}


static int
_db_cmd_version(void *bs, int argc, const char **argv)
{
    const char *usage = "db version";
    const char *purpose = "report the open database format version";
    struct _ged_db_info *gd = (struct _ged_db_info *)bs;

    if (_db_cmd_msgs(bs, argc, argv, usage, purpose))
	return BRLCAD_OK;

    if (argc != 1) {
	bu_vls_printf(gd->gedp->ged_result_str, "Usage: %s", usage);
	return BRLCAD_ERROR;
    }

    return _db_version_core(gd->gedp, argc, argv);
}


static int
_db_cmd_forward(void *bs, const char *target, int argc, const char **argv)
{
    struct _ged_db_info *gd = (struct _ged_db_info *)bs;

    if (argc == 2 && BU_STR_EQUAL(argv[1], HELPFLAG)) {
	bu_vls_printf(gd->gedp->ged_result_str, "db %s [args]\nrun the libged '%s' database command\n", argv[0], target);
	return BRLCAD_OK;
    }
    if (argc == 2 && BU_STR_EQUAL(argv[1], PURPOSEFLAG)) {
	bu_vls_printf(gd->gedp->ged_result_str, "run the libged '%s' database command\n", target);
	return BRLCAD_OK;
    }

    const char **av = (const char **)bu_calloc((size_t)argc + 1, sizeof(const char *), "db subcommand argv");
    av[0] = target;
    for (int i = 1; i < argc; i++)
	av[i] = argv[i];

    int ret = ged_exec(gd->gedp, argc, av);
    bu_free((void *)av, "db subcommand argv");
    return ret;
}


/* Keep this an explicit database-command allowlist.  In particular, do not
 * turn "db" into a second spelling for every command in libged.  The generated
 * wrappers give each entry a distinct function pointer, allowing standard
 * subcommand help to report each command separately.
 */
#define DB_FORWARD_COMMANDS(X) \
    X(adjust, adjust) \
    X(arced, arced) \
    X(attr, attr) \
    X(bo, bo) \
    X(bot_face_sort, bot_face_sort) \
    X(bot_smooth, bot_smooth) \
    X(c, c) \
    X(cat, cat) \
    X(cc, cc) \
    X(close, closedb) \
    X(closedb, closedb) \
    X(color, color) \
    X(comb, comb) \
    X(comb_color, comb_color) \
    X(concat, concat) \
    X(copyeval, copyeval) \
    X(cp, cp) \
    X(dbip, dbip) \
    X(dump, dump) \
    X(dup, dup) \
    X(edcomb, edcomb) \
    X(edit, edit) \
    X(edmater, edmater) \
    X(expand, expand) \
    X(facetize, facetize) \
    X(form, form) \
    X(g, g) \
    X(get, get) \
    X(get_type, get_type) \
    X(hide, hide) \
    X(i, i) \
    X(importFg4Section, importFg4Section) \
    X(item, item) \
    X(keep, keep) \
    X(kill, kill) \
    X(killall, killall) \
    X(killtree, killtree) \
    X(l, l) \
    X(listeval, listeval) \
    X(log, log) \
    X(ls, ls) \
    X(lt, lt) \
    X(make, make) \
    X(make_name, make_name) \
    X(match, match) \
    X(mater, mater) \
    X(mirror, mirror) \
    X(move_arb_edge, move_arb_edge) \
    X(move_arb_face, move_arb_face) \
    X(mv, mv) \
    X(mvall, mvall) \
    X(nirt, nirt) \
    X(nmg_collapse, nmg_collapse) \
    X(ocenter, ocenter) \
    X(open, opendb) \
    X(opendb, opendb) \
    X(orotate, orotate) \
    X(oscale, oscale) \
    X(otranslate, otranslate) \
    X(pathlist, pathlist) \
    X(paths, paths) \
    X(prcolor, prcolor) \
    X(push, push) \
    X(put, put) \
    X(r, r) \
    X(rm, rm) \
    X(rmater, rmater) \
    X(rmap, rmap) \
    X(rotate_arb_face, rotate_arb_face) \
    X(shader, shader) \
    X(shells, shells) \
    X(showmats, showmats) \
    X(summary, summary) \
    X(title, title) \
    X(tol, tol) \
    X(tops, tops) \
    X(track, track) \
    X(unhide, unhide) \
    X(units, units) \
    X(whatid, whatid) \
    X(whichair, whichair) \
    X(whichid, whichid) \
    X(wmater, wmater) \
    X(xpush, xpush)

#define DB_DECLARE_FORWARD(_name, _target) \
    static int _db_cmd_ ## _name(void *bs, int argc, const char **argv) \
    { \
	return _db_cmd_forward(bs, #_target, argc, argv); \
    }

DB_FORWARD_COMMANDS(DB_DECLARE_FORWARD)

#define DB_DECLARE_SCHEMA(_name, _target) \
    static const struct bu_cmd_schema db_ ## _name ## _schema = { \
	GED_STR(_name), "Forward a database command to libged", NULL, NULL, \
	BU_CMD_PARSE_OPTIONS_FIRST, {NULL} \
    };
DB_FORWARD_COMMANDS(DB_DECLARE_SCHEMA)


#define DB_TREE_FORWARD(_name, _target) \
    BU_CMD_TREE_NODE(&db_ ## _name ## _schema, NULL, NULL, \
	BU_CMD_TREE_CHILD_AFTER_OPTIONS, _db_cmd_ ## _name),
static const struct bu_cmd_tree_node db_subcommands[] = {
    DB_FORWARD_COMMANDS(DB_TREE_FORWARD)
    BU_CMD_TREE_NODE(&db_find_dispatch_schema, NULL, NULL,
	BU_CMD_TREE_CHILD_AFTER_OPTIONS, _db_cmd_find),
    BU_CMD_TREE_NODE(&db_version_schema, NULL, NULL,
	BU_CMD_TREE_CHILD_AFTER_OPTIONS, _db_cmd_version),
    BU_CMD_TREE_NODE_NULL
};
#undef DB_TREE_FORWARD
static const struct bu_cmd_tree db_tree = {
    &db_root_schema, db_subcommands, BU_CMD_TREE_CHILD_AFTER_OPTIONS
};


int
ged_db_core(struct ged *gedp, int argc, const char *argv[])
{
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    const int original_argc = argc;
    const char **original_argv = argv;
    struct _ged_db_info gd;
    gd.gedp = gedp;
    gd.help = 0;

    bu_vls_trunc(gedp->ged_result_str, 0);

    argc--;
    argv++;

    if (!argc || BU_STR_EQUAL(argv[0], "help")) {
	char *help = bu_cmd_tree_describe(&db_tree);
	if (argc > 1 && BU_STR_EQUAL(argv[1], "find")) {
	    if (help) bu_free(help, "db tree help");
	    help = bu_cmd_schema_describe(&db_find_schema);
	}
	if (help) {
	    bu_vls_strcat(gedp->ged_result_str, help);
	    bu_free(help, "db tree help");
	}
	return GED_HELP;
    }

    if (BU_STR_EQUAL(argv[0], "-h") || BU_STR_EQUAL(argv[0], "--help") ||
	BU_STR_EQUAL(argv[0], "?")) {
	char *help = bu_cmd_tree_describe(&db_tree);
	if (help) {
	    bu_vls_strcat(gedp->ged_result_str, help);
	    bu_free(help, "db tree help");
	}
	return GED_HELP;
    }

    int ret = BRLCAD_ERROR;
    if (bu_cmd_tree_dispatch(&db_tree, (void *)&gd, argc, argv, &ret) == 0)
	return ret;

    if (gd.help) {
	char *root_help = bu_cmd_tree_describe(&db_tree);
	if (root_help) {
	    bu_vls_strcat(gedp->ged_result_str, root_help);
	    bu_free(root_help, "db tree help");
	}
	return GED_HELP;
    }

    bu_clbk_t clbk = NULL;
    void *clbk_data = NULL;
    if (ged_clbk_get(&clbk, &clbk_data, gedp, "db", BU_CLBK_DURING) == BRLCAD_OK && clbk)
	return ged_clbk_exec(gedp->ged_result_str, gedp, GED_CMD_RECURSION_LIMIT, clbk, original_argc, original_argv, (void *)gedp, clbk_data);

    bu_vls_printf(gedp->ged_result_str, "db: unknown subcommand '%s'\n", argv[0]);
    char *help = bu_cmd_tree_describe(&db_tree);
    if (help) {
	bu_vls_strcat(gedp->ged_result_str, help);
	bu_free(help, "db tree help");
    }
    return BRLCAD_ERROR | GED_UNKNOWN;
}


#include "../include/plugin.h"

#define GED_DB_COMMANDS(X, XID) \
    X(db, ged_db_core, GED_CMD_DEFAULT, &db_root_schema) \
    X(dbfind, ged_find_core, GED_CMD_DEFAULT, &db_find_schema) \
    X(dbversion, ged_db_version_core, GED_CMD_DEFAULT, &db_version_schema) \
    X(find, ged_find_core, GED_CMD_DEFAULT, &db_find_schema) \
    X(version, ged_db_version_core, GED_CMD_DEFAULT, &db_version_schema) \

GED_DECLARE_COMMAND_SET_WITH_NATIVE_SCHEMA(GED_DB_COMMANDS)
GED_DECLARE_PLUGIN_MANIFEST_WITH_NATIVE_SCHEMA("libged_db", 1, GED_DB_COMMANDS)

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
