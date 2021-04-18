/*                         S C R E E N G R A B . C
 * BRL-CAD
 *
 * Copyright (c) 2008-2021 United States Government as represented by
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
/** @file libged/screengrab.c
 *
 * The screengrab command.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include "bu/cmd.h"
#include "bu/opt.h"
#include "bu/vls.h"
#include "dm.h"

#include "../ged_private.h"

#ifndef COMMA
#  define COMMA ','
#endif

#define HELPFLAG "--print-help"
#define PURPOSEFLAG "--print-purpose"

struct _ged_dm_info {
    struct ged *gedp;
    long verbosity;
    const struct bu_cmdtab *cmds;
    struct bu_opt_desc *gopts;
};

static int
_dm_cmd_msgs(void *bs, int argc, const char **argv, const char *us, const char *ps)
{
    struct _ged_dm_info *gd = (struct _ged_dm_info *)bs;
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

struct dm *
_dm_name_lookup(struct _ged_dm_info *gd, const char *dm_name)
{
    struct dm *cdmp = NULL;
    if (!gd) {
	return NULL;
    }
    if (!dm_name || !strlen(dm_name)) {
	bu_vls_printf(gd->gedp->ged_result_str, ": no DM specified and no current DM set in GED\n");
	return NULL;
    }
    if (!gd->gedp->ged_all_dmp || !BU_PTBL_LEN(gd->gedp->ged_all_dmp)) {
	bu_vls_printf(gd->gedp->ged_result_str, ": no DMs defined in GED\n");
	return NULL;
    }
    for (size_t i = 0; i < BU_PTBL_LEN(gd->gedp->ged_all_dmp); i++) {
	struct dm *ndmp = (struct dm *)BU_PTBL_GET(gd->gedp->ged_all_dmp, i);
	if (BU_STR_EQUAL(dm_name, bu_vls_cstr(dm_get_pathname(ndmp)))) {
	    cdmp = ndmp;
	    break;
	}
    }
    if (!cdmp) {
	bu_vls_printf(gd->gedp->ged_result_str, ": no DM with name %s found\n", dm_name);
    }

    return cdmp;
}


int
_dm_cmd_type(void *ds, int argc, const char **argv)
{
    const char *usage_string = "dm [options] type [name]";
    const char *purpose_string = "report type of display manager (null, txt, ogl, etc.).";
    if (_dm_cmd_msgs(ds, argc, argv, usage_string, purpose_string)) {
	return GED_OK;
    }

    argc--; argv++;

    struct _ged_dm_info *gd = (struct _ged_dm_info *)ds;

    struct dm *cdmp = (struct dm *)gd->gedp->ged_dmp;
    if (!cdmp) {
	cdmp = _dm_name_lookup(gd, argv[0]);
	if (!cdmp) {
	    return GED_ERROR;
	}
    }
    bu_vls_printf(gd->gedp->ged_result_str, "%s\n", dm_get_type(cdmp));
    return GED_OK;
}

int
_dm_cmd_initmsg(void *ds, int argc, const char **argv)
{
    const char *usage_string = "dm [options] initmsg";
    const char *purpose_string = "display libdm plugin initialization messages.";
    if (_dm_cmd_msgs(ds, argc, argv, usage_string, purpose_string)) {
	return GED_OK;
    }

    struct _ged_dm_info *gd = (struct _ged_dm_info *)ds;

    bu_vls_printf(gd->gedp->ged_result_str, "%s\n", dm_init_msgs());
    return GED_OK;
}

int
_dm_cmd_list(void *ds, int argc, const char **argv)
{
    const char *usage_string = "dm [options] list";
    const char *purpose_string = "list display manager instances known to GED.";
    if (_dm_cmd_msgs(ds, argc, argv, usage_string, purpose_string)) {
	return GED_OK;
    }

    argc--; argv++;

    struct _ged_dm_info *gd = (struct _ged_dm_info *)ds;

    if (!gd->gedp->ged_all_dmp || !BU_PTBL_LEN(gd->gedp->ged_all_dmp)) {
	bu_vls_printf(gd->gedp->ged_result_str, ": no DMs defined in GED\n");
	return GED_ERROR;
    }
    for (size_t i = 0; i < BU_PTBL_LEN(gd->gedp->ged_all_dmp); i++) {
	struct dm *ndmp = (struct dm *)BU_PTBL_GET(gd->gedp->ged_all_dmp, i);
	if (gd->verbosity) {
	    if (ndmp == (struct dm *)gd->gedp->ged_dmp) {
		bu_vls_printf(gd->gedp->ged_result_str, "*%s (%s)\n", bu_vls_cstr(dm_get_pathname(ndmp)), dm_get_type(ndmp));
	    } else {
		bu_vls_printf(gd->gedp->ged_result_str, " %s (%s)\n", bu_vls_cstr(dm_get_pathname(ndmp)), dm_get_type(ndmp));
	    }
	} else {
	    bu_vls_printf(gd->gedp->ged_result_str, "%s\n", bu_vls_cstr(dm_get_pathname(ndmp)));
	}
    }

    return GED_OK;
}

int
_dm_cmd_get(void *ds, int argc, const char **argv)
{
    const char *usage_string = "dm [options] get [--dm name] [var]";
    const char *purpose_string = "report value(s) set to dm variables. With empty var, report all values.";
    if (_dm_cmd_msgs(ds, argc, argv, usage_string, purpose_string)) {
	return GED_OK;
    }

    argc--; argv++;

    struct bu_vls dm_name = BU_VLS_INIT_ZERO;

    struct bu_opt_desc d[2];
    BU_OPT(d[0], "d", "dm", "name", &bu_opt_vls, &dm_name, "dm instance to use when reporting");
    BU_OPT_NULL(d[1]);

    int ac = bu_opt_parse(NULL, argc, argv, d);

    struct _ged_dm_info *gd = (struct _ged_dm_info *)ds;

    struct dm *cdmp = (struct dm *)gd->gedp->ged_dmp;
    if (!cdmp) {
	cdmp = _dm_name_lookup(gd, bu_vls_cstr(&dm_name));
	if (!cdmp) {
	    bu_vls_free(&dm_name);
	    return GED_ERROR;
	}
    }

    if (!ac) {
	// Report all vars
	struct bu_vls rstr = BU_VLS_INIT_ZERO;
	bu_vls_sprintf(&rstr, "Display Manager %s (type %s) internal variables", bu_vls_cstr(dm_get_pathname(cdmp)), dm_get_dm_name(cdmp));
	struct bu_structparse *dmparse = dm_get_vparse(cdmp);
	void *mvars = dm_get_mvars(cdmp);
	if (dmparse && mvars) {
	    bu_vls_struct_print2(gd->gedp->ged_result_str, bu_vls_addr(&rstr), dmparse, (const char *)mvars);
	}
	bu_vls_free(&rstr);
	return GED_OK;
    }

    struct bu_structparse *dmparse = dm_get_vparse(cdmp);
    void *mvars = dm_get_mvars(cdmp);
    if (!dmparse || !mvars) {
	// No variables to report
	return GED_OK;
    }

    for (int i = 0; i < ac; i++) {
	if (gd->verbosity) {
	    bu_vls_printf(gd->gedp->ged_result_str, "%s=", argv[i]);
	    bu_vls_struct_item_named(gd->gedp->ged_result_str, dmparse, argv[i], (const char *)mvars, COMMA);
	} else {
	    bu_vls_struct_item_named(gd->gedp->ged_result_str, dmparse, argv[i], (const char *)mvars, COMMA);
	}
	if (i < ac - 1) {
	    bu_vls_printf(gd->gedp->ged_result_str, "\n");
	}
    }

    return GED_ERROR;
}

int
_dm_cmd_set(void *ds, int argc, const char **argv)
{
    const char *usage_string = "dm [options] set [--dm name] key val";
    const char *purpose_string = "assign value to dm variable, if it exists.";
    if (_dm_cmd_msgs(ds, argc, argv, usage_string, purpose_string)) {
	return GED_OK;
    }

    argc--; argv++;

    struct bu_vls dm_name = BU_VLS_INIT_ZERO;

    struct bu_opt_desc d[2];
    BU_OPT(d[0], "d", "dm", "name", &bu_opt_vls, &dm_name, "dm instance to use when reporting");
    BU_OPT_NULL(d[1]);

    int ac = bu_opt_parse(NULL, argc, argv, d);

    struct _ged_dm_info *gd = (struct _ged_dm_info *)ds;

    struct dm *cdmp = (struct dm *)gd->gedp->ged_dmp;
    if (!cdmp) {
	cdmp = _dm_name_lookup(gd, bu_vls_cstr(&dm_name));
	if (!cdmp) {
	    bu_vls_free(&dm_name);
	    return GED_ERROR;
	}
    }

    if (ac != 2) {
	bu_vls_printf(gd->gedp->ged_result_str, ": invalid argument count - need key and value");
	return GED_ERROR;
    }

    struct bu_structparse *dmparse = dm_get_vparse(cdmp);
    void *mvars = dm_get_mvars(cdmp);
    if (!dmparse || !mvars) {
	// No variables to set
	bu_vls_printf(gd->gedp->ged_result_str, "display manager has not associated variables\n");
	return GED_ERROR;
    }

    struct bu_vls tmp_vls = BU_VLS_INIT_ZERO;
    int ret;
    bu_vls_printf(&tmp_vls, "%s=\"%s\"", argv[0], argv[1]);
    ret = bu_struct_parse(&tmp_vls, dmparse, (char *)mvars, NULL);
    bu_vls_free(&tmp_vls);
    if (ret < 0) {
	bu_vls_printf(gd->gedp->ged_result_str, ": unable to set %s", argv[0]);
	return GED_ERROR;
    }

    if (gd->verbosity) {
	if (gd->verbosity > 1) {
	    bu_vls_printf(gd->gedp->ged_result_str, "%s=", argv[0]);
	    bu_vls_struct_item_named(gd->gedp->ged_result_str, dmparse, argv[0], (const char *)mvars, COMMA);
	} else {
	    bu_vls_struct_item_named(gd->gedp->ged_result_str, dmparse, argv[0], (const char *)mvars, COMMA);
	}
    }

    return GED_OK;
}

const struct bu_cmdtab _dm_cmds[] = {
    { "initmsg",         _dm_cmd_initmsg},
    { "type",            _dm_cmd_type},
    { "list",            _dm_cmd_list},
    { "get",             _dm_cmd_get},
    { "set",             _dm_cmd_set},
    { (char *)NULL,      NULL}
};

int
ged_dm_core(struct ged *gedp, int argc, const char *argv[])
{
    int help = 0;
    struct _ged_dm_info gd;
    gd.gedp = gedp;
    gd.cmds = _dm_cmds;
    gd.verbosity = 0;

    // Sanity
    if (UNLIKELY(!gedp || !argc || !argv)) {
	return GED_ERROR;
    }

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    // We know we're the dm command - start processing args
    argc--; argv++;

    // See if we have any high level options set
    struct bu_opt_desc d[3];
    BU_OPT(d[0], "h", "help",    "",  NULL,               &help,         "Print help");
    BU_OPT(d[1], "v", "verbose", "",  &bu_opt_incr_long,  &gd.verbosity, "Verbose output");
    BU_OPT_NULL(d[2]);

    gd.gopts = d;

    int ac = bu_opt_parse(NULL, argc, argv, d);

    if (!ac || help) {
	_ged_subcmd_help(gedp, (struct bu_opt_desc *)d, (const struct bu_cmdtab *)_dm_cmds, "dm", "[options] subcommand [args]", &gd, 0, NULL);
	return GED_OK;
    }

    if (!gedp->ged_dmp && (!gedp->ged_all_dmp || !BU_PTBL_LEN(gedp->ged_all_dmp))) {
	bu_vls_printf(gedp->ged_result_str, ": no display manager currently active and none known to GED");
	return GED_ERROR;
    }

    int ret;
    if (bu_cmd(_dm_cmds, ac, argv, 0, (void *)&gd, &ret) == BRLCAD_OK) {
	return ret;
    } else {
	bu_vls_printf(gedp->ged_result_str, "subcommand %s not defined", argv[0]);
    }

    return GED_ERROR;
}

#ifdef GED_PLUGIN
#include "../include/plugin.h"

extern int ged_screen_grab_core(struct ged *gedp, int argc, const char *argv[]);
struct ged_cmd_impl screen_grab_cmd_impl = {"screen_grab", ged_screen_grab_core, GED_CMD_DEFAULT};
const struct ged_cmd screen_grab_cmd = { &screen_grab_cmd_impl };
struct ged_cmd_impl screengrab_cmd_impl = {"screengrab", ged_screen_grab_core, GED_CMD_DEFAULT};
const struct ged_cmd screengrab_cmd = { &screengrab_cmd_impl };

struct ged_cmd_impl dm_cmd_impl = {"dm", ged_dm_core, GED_CMD_DEFAULT};
const struct ged_cmd dm_cmd = { &dm_cmd_impl };

const struct ged_cmd *dm_cmds[] = { &screen_grab_cmd, &screengrab_cmd, &dm_cmd, NULL };

static const struct ged_plugin pinfo = { GED_API,  dm_cmds, 3 };

COMPILER_DLLEXPORT const struct ged_plugin *ged_plugin_info()
{
    return &pinfo;
}
#endif /* GED_PLUGIN */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
