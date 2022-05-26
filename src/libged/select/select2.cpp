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
    const char *usage_string = "select [options] list";
    const char *purpose_string = "list currently defined selection sets";
    if (_select_cmd_msgs(bs, argc, argv, usage_string, purpose_string)) {
        return BRLCAD_OK;
    }

    struct ged *gedp = gd->gedp;
    struct bu_hash_tbl *t = gedp->ged_selections;
    if (!t)
	return BRLCAD_ERROR;

    int scnt = 0;
    struct bu_hash_entry *entry = bu_hash_next(t, NULL);

    while (entry) {
	scnt++;
	entry = bu_hash_next(t, entry);
    }

    bu_vls_sprintf(gedp->ged_result_str, "Found %d sets", scnt);

    return BRLCAD_OK;
}


const struct bu_cmdtab _select_cmds[] = {
    { "list",       _select_cmd_list},
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
    gd.key = NULL;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    // We know we're the select command - start processing args
    argc--; argv++;

    // See if we have any high level options set
    struct bu_vls sname = BU_VLS_INIT_ZERO;
    struct bu_opt_desc d[4];
    BU_OPT(d[0], "h", "help",     "",         NULL,    &help,  "Print help");
    BU_OPT(d[1], "S", "set",  "name",  &bu_opt_vls,   &sname,  "Specify set to operate on");
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
	bu_vls_free(&sname);
	return BRLCAD_OK;
    }

    int ret;
    if (bu_cmd(_select_cmds, argc, argv, 0, (void *)&gd, &ret) == BRLCAD_OK) {
	bu_vls_free(&sname);
	return ret;
    } else {
	bu_vls_printf(gedp->ged_result_str, "subcommand %s not defined", argv[0]);
    }

    bu_vls_free(&sname);

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
