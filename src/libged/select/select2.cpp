/*                       S E L E C T . C P P
 * BRL-CAD
 *
 * Copyright (c) 2008-2022 United States Government as represented by
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
/** @file libged/select.cpp
 *
 * The select command.
 *
 */

#include "common.h"

#include <string.h>

#include "bu/opt.h"
#include "../ged_private.h"

struct _ged_select_info {
    struct ged *gedp;
    struct bu_vls curr_set;
    const char *key;
};

int
_select_cmd_msgs(void *bs, int argc, const char **argv, const char *us, const char *ps)
{
    struct _ged_select_info *gd = (struct _ged_select_info *)bs;
    if (argc == 2 && BU_STR_EQUAL(argv[1], HELPFLAG)) {
        bu_vls_printf(gd->gedp->ged_result_str, "%s\n%s\n", us, ps);
        return 1;
    }
    if (argc == 2 && BU_STR_EQUAL(argv[1], PURPOSEFLAG)) {
        bu_vls_printf(gd->gedp->ged_result_str, "%s\n", ps);
        return 1;
    }
    return 0;
}

int
_select_cmd_list(void *bs, int argc, const char **argv)
{
    struct _ged_select_info *gd = (struct _ged_select_info *)bs;
    const char *usage_string = "select [options] list [set_name_pattern]";
    const char *purpose_string = "list currently defined selection sets, or the contents of specified sets";
    if (_select_cmd_msgs(bs, argc, argv, usage_string, purpose_string)) {
        return BRLCAD_OK;
    }

    argc--; argv++;

    struct ged *gedp = gd->gedp;
    if (!gedp->ged_selection_sets)
	return BRLCAD_ERROR;

    if (!argc && !bu_vls_strlen(&gd->curr_set)) {
	struct bu_ptbl ssets = BU_PTBL_INIT_ZERO;
	size_t scnt = ged_selection_sets_lookup(&ssets, gedp->ged_selection_sets, "*");
	if (scnt) {
	    for (size_t i = 0; i < scnt; i++) {
		struct ged_selection_set *s = (struct ged_selection_set *)BU_PTBL_GET(&ssets, i);
		bu_vls_printf(gedp->ged_result_str, "%s\n", bu_vls_cstr(&s->name));
	    }
	}
	bu_ptbl_free(&ssets);
	return BRLCAD_OK;
    }

    if (bu_vls_strlen(&gd->curr_set)) {
	struct bu_ptbl ssets = BU_PTBL_INIT_ZERO;
	size_t scnt = ged_selection_sets_lookup(&ssets, gedp->ged_selection_sets, bu_vls_cstr(&gd->curr_set));
	if (!scnt) {
	    bu_vls_printf(gedp->ged_result_str, ": %s does not match any sets\n", bu_vls_cstr(&gd->curr_set));
	    return BRLCAD_ERROR;
	}

	for (size_t i = 0; i < scnt; i++) {
	    struct ged_selection_set *gs = (struct ged_selection_set *)BU_PTBL_GET(&ssets, i);
	    char **selection_names = NULL;
	    int ac = ged_selection_set_list(&selection_names, gs);
	    if (ac) {
		for (int j = 0; j < ac; j++) {
		    bu_vls_printf(gedp->ged_result_str, "%s\n", selection_names[j]);
		}
		bu_argv_free(ac, selection_names);
	    }
	}
	bu_ptbl_free(&ssets);

	if (!argc || BU_STR_EQUAL(bu_vls_cstr(&gd->curr_set), argv[0]))
	    return BRLCAD_OK;
    }

    struct bu_ptbl ssets = BU_PTBL_INIT_ZERO;
    size_t scnt = ged_selection_sets_lookup(&ssets, gedp->ged_selection_sets, argv[0]);
    if (!scnt) {
	bu_vls_printf(gedp->ged_result_str, ": %s does not match any sets\n", argv[0]);
	return BRLCAD_ERROR;
    }

    for (size_t i = 0; i < scnt; i++) {
	struct ged_selection_set *gs = (struct ged_selection_set *)BU_PTBL_GET(&ssets, i);
	char **selection_names = NULL;
	int ac = ged_selection_set_list(&selection_names, gs);
	if (ac) {
	    for (int j = 0; j < ac; j++) {
		bu_vls_printf(gedp->ged_result_str, "%s\n", selection_names[j]);
	    }
	    bu_argv_free(ac, selection_names);
	}
    }
    bu_ptbl_free(&ssets);

    return BRLCAD_OK;
}

int
_select_cmd_add(void *bs, int argc, const char **argv)
{
    struct _ged_select_info *gd = (struct _ged_select_info *)bs;
    const char *usage_string = "select [options] add path1 [path2] ... [pathN]";
    const char *purpose_string = "add paths to either the default set, or (if set with options) the specified set";
    if (_select_cmd_msgs(bs, argc, argv, usage_string, purpose_string)) {
        return BRLCAD_OK;
    }

    argc--; argv++;

    struct ged *gedp = gd->gedp;
    if (!argc) {
	bu_vls_printf(gedp->ged_result_str, "need at least one path to add");
	return BRLCAD_ERROR;
    }

    if (!gedp->ged_selection_sets)
	return BRLCAD_ERROR;

    struct bu_ptbl ssets = BU_PTBL_INIT_ZERO;
    size_t scnt = 0;
    if (bu_vls_strlen(&gd->curr_set)) {
	scnt = ged_selection_sets_lookup(&ssets, gedp->ged_selection_sets, bu_vls_cstr(&gd->curr_set));
    } else {
	scnt = ged_selection_sets_lookup(&ssets, gedp->ged_selection_sets, "default");
    }
    if (scnt != 1) {
	bu_vls_printf(gedp->ged_result_str, "invalid name for current set: %s", (bu_vls_strlen(&gd->curr_set)) ? bu_vls_cstr(&gd->curr_set): "default");
	return BRLCAD_ERROR;
    }

    struct ged_selection_set *gs = (struct ged_selection_set *)BU_PTBL_GET(&ssets, 0);
    bu_ptbl_free(&ssets);

    for (int i = 0; i < argc; i++) {
	if (!ged_selection_insert(gs, argv[i])) {
	    bu_vls_printf(gedp->ged_result_str, "unable to add path to selection: %s", argv[i]);
	    return BRLCAD_ERROR;
	}
    }
    return BRLCAD_OK;
}


int
_select_cmd_rm(void *bs, int argc, const char **argv)
{
    struct _ged_select_info *gd = (struct _ged_select_info *)bs;
    const char *usage_string = "select [options] rm path1 [path2] ... [pathN]";
    const char *purpose_string = "remove paths from either the default set, or (if set with options) the specified set";
    if (_select_cmd_msgs(bs, argc, argv, usage_string, purpose_string)) {
        return BRLCAD_OK;
    }

    argc--; argv++;

    struct ged *gedp = gd->gedp;
    if (!argc) {
	bu_vls_printf(gedp->ged_result_str, "need at least one path to remove");
	return BRLCAD_ERROR;
    }

    if (!gedp->ged_selection_sets)
	return BRLCAD_ERROR;

    struct bu_ptbl ssets = BU_PTBL_INIT_ZERO;
    size_t scnt = 0;
    if (bu_vls_strlen(&gd->curr_set)) {
	scnt = ged_selection_sets_lookup(&ssets, gedp->ged_selection_sets, bu_vls_cstr(&gd->curr_set));
    } else {
	scnt = ged_selection_sets_lookup(&ssets, gedp->ged_selection_sets, "default");
    }
    if (scnt != 1) {
	bu_vls_printf(gedp->ged_result_str, "invalid name for current set: %s", (bu_vls_strlen(&gd->curr_set)) ? bu_vls_cstr(&gd->curr_set): "default");
	return BRLCAD_ERROR;
    }

    struct ged_selection_set *gs = (struct ged_selection_set *)BU_PTBL_GET(&ssets, 0);
    bu_ptbl_free(&ssets);

    for (int i = 0; i < argc; i++) {
	ged_selection_remove(gs, argv[i]);
    }

    return BRLCAD_OK;
}

const struct bu_cmdtab _select_cmds[] = {
    { "list",       _select_cmd_list},
    { "add",        _select_cmd_add},
    { "rm",         _select_cmd_rm},
    { (char *)NULL,      NULL}
};


extern "C" int
ged_select2_core(struct ged *gedp, int argc, const char *argv[])
{
    int help = 0;
    struct _ged_select_info gd;

    // Sanity
    if (UNLIKELY(!gedp || !argc || !argv)) {
	return BRLCAD_ERROR;
    }

    // Initialize select info
    gd.gedp = gedp;
    bu_vls_init(&gd.curr_set);
    gd.key = NULL;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    // We know we're the select command - start processing args
    argc--; argv++;

    // See if we have any high level options set
    struct bu_opt_desc d[4];
    BU_OPT(d[0], "h", "help", "",      NULL,          &help,         "Print help");
    BU_OPT(d[1], "S", "set",  "name",  &bu_opt_vls,   &gd.curr_set,  "Specify set to operate on");
    BU_OPT_NULL(d[2]);

    // High level options are only defined prior to the subcommand
    int cmd_pos = -1;
    for (int i = 0; i < argc; i++) {
	if (bu_cmd_valid(_select_cmds, argv[i]) == BRLCAD_OK) {
	    cmd_pos = i;
	    break;
	}
    }

    // Clear out any high level opts prior to subcommand
    int acnt = (cmd_pos >= 0) ? cmd_pos : argc;
    int ac_ret = bu_opt_parse(NULL, acnt, argv, d);
    if (ac_ret) {
	help = 1;
    } else {
	for (int i = 0; i < acnt; i++) {
	    argc--; argv++;
	}
    }

    if (help) {
	if (cmd_pos >= 0) {
	    argc = argc - cmd_pos;
	    argv = &argv[cmd_pos];
	    _ged_subcmd_help(gedp, (struct bu_opt_desc *)d, (const struct bu_cmdtab *)_select_cmds, "select", "[options] subcommand [args]", &gd, argc, argv);
	} else {
	    _ged_subcmd_help(gedp, (struct bu_opt_desc *)d, (const struct bu_cmdtab *)_select_cmds, "select", "[options] subcommand [args]", &gd, 0, NULL);
	}
	bu_vls_free(&gd.curr_set);
	return BRLCAD_OK;
    }

    int ret;
    if (bu_cmd(_select_cmds, argc, argv, 0, (void *)&gd, &ret) == BRLCAD_OK) {
	bu_vls_free(&gd.curr_set);
	return ret;
    } else {
	bu_vls_printf(gedp->ged_result_str, "subcommand %s not defined", argv[0]);
    }

    bu_vls_free(&gd.curr_set);

    return BRLCAD_ERROR;
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
