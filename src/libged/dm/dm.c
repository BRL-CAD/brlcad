/*                         S C R E E N G R A B . C
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

#define DM_MAX_TRIES 100

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
    struct bview *gdvp = NULL;
    struct dm *ndmp = NULL;

    if (!gd) {
	return NULL;
    }
    if (!dm_name || !strlen(dm_name)) {
	bu_vls_printf(gd->gedp->ged_result_str, ": no DM specified and no current DM set in GED\n");
	return NULL;
    }

    struct ged *gedp = gd->gedp;
    if (!BU_PTBL_LEN(&gedp->ged_views)) {
	bu_vls_printf(gedp->ged_result_str, ": no views defined in GED\n");
	return NULL;
    }
    int dm_cnt = 0;
    for (size_t i = 0; i < BU_PTBL_LEN(&gedp->ged_views); i++) {
	gdvp = (struct bview *)BU_PTBL_GET(&gedp->ged_views, i);
	if (!gdvp->dmp)
	    continue;
	dm_cnt++;
    }
    if (!dm_cnt) {
	bu_vls_printf(gedp->ged_result_str, ": no views have associated DMs defined\n");
	return NULL;
    }

    for (size_t i = 0; i < BU_PTBL_LEN(&gedp->ged_views); i++) {
	gdvp = (struct bview *)BU_PTBL_GET(&gedp->ged_views, i);
	ndmp = (struct dm *)gdvp->dmp;
	if (ndmp && BU_STR_EQUAL(dm_name, bu_vls_cstr(dm_get_pathname(ndmp)))) {
	    cdmp = ndmp;
	    break;
	}
    }
    if (!cdmp) {
	bu_vls_printf(gd->gedp->ged_result_str, ": no DM with name %s found\n", dm_name);
    }

    return cdmp;
}

static struct dm *
_dm_find(struct _ged_dm_info *gd, struct bu_vls *name)
{
    if (!gd)
	return NULL;

    struct ged *gedp = gd->gedp;
    if (!name) {
	if (!gedp->ged_gvp) {
	    bu_vls_printf(gedp->ged_result_str, ": no current view is set in GED\n");
	    return NULL;
	} else {
	    if (!gedp->ged_gvp->dmp) {
		bu_vls_printf(gedp->ged_result_str, ": no current DM is set in GED's current view\n");
		return NULL;
	    } else {
		return (struct dm *)gedp->ged_gvp->dmp;
	    }
	}
    }
    if (name && gedp->ged_gvp && gedp->ged_gvp->dmp) {
	struct dm *cdmp = (struct dm *)gedp->ged_gvp->dmp;
	if (BU_STR_EQUAL(bu_vls_cstr(name), bu_vls_cstr(dm_get_pathname(cdmp))))
	    return cdmp;
    }

    return _dm_name_lookup(gd, bu_vls_cstr(name));
}

int
_dm_cmd_bg(void *ds, int argc, const char **argv)
{
    const char *usage_string = "dm [options] bg [r/g/b]";
    const char *purpose_string = "get/set dm background color";
    if (_dm_cmd_msgs(ds, argc, argv, usage_string, purpose_string)) {
	return BRLCAD_OK;
    }

    argc--; argv++;

    struct _ged_dm_info *gd = (struct _ged_dm_info *)ds;
    struct ged *gedp = gd->gedp;
    struct dm *cdmp = _dm_find(gd, NULL);
    if (!cdmp)
	return BRLCAD_ERROR;

    if (!argc) {
	const unsigned char *dm_bg = dm_get_bg(cdmp);
	if (dm_bg) {
	    bu_vls_printf(gedp->ged_result_str, "%d/%d/%d\n", (short)dm_bg[0], (short)dm_bg[1], (short)dm_bg[2]);
	    return BRLCAD_OK;
	} else {
	    bu_vls_printf(gedp->ged_result_str, ": no background color available\n");
	    return BRLCAD_ERROR;
	}
    }

    struct bu_color c;
    if (bu_opt_color(NULL, argc, argv, &c) == -1) {
	bu_vls_printf(gedp->ged_result_str, "invalid color specification\n");
	return BRLCAD_ERROR;
    }
    unsigned char n_bg[3];
    bu_color_to_rgb_chars(&c, n_bg);
    dm_set_bg(cdmp, n_bg[0], n_bg[1], n_bg[2]);
    return BRLCAD_OK;
}


int
_dm_cmd_type(void *ds, int argc, const char **argv)
{
    const char *usage_string = "dm [options] type [name]";
    const char *purpose_string = "report type of display manager (null, txt, ogl, etc.).";
    if (_dm_cmd_msgs(ds, argc, argv, usage_string, purpose_string)) {
	return BRLCAD_OK;
    }

    argc--; argv++;

    struct _ged_dm_info *gd = (struct _ged_dm_info *)ds;
    struct dm *cdmp = _dm_find(gd, NULL);
    if (!cdmp) {
	return BRLCAD_ERROR;
    }

    bu_vls_printf(gd->gedp->ged_result_str, "%s\n", dm_get_type(cdmp));
    return BRLCAD_OK;
}

int
_dm_cmd_types(void *ds, int argc, const char **argv)
{
    const char *usage_string = "dm [options] types";
    const char *purpose_string = "list supported display manager types";
    if (_dm_cmd_msgs(ds, argc, argv, usage_string, purpose_string)) {
	return BRLCAD_OK;
    }

    argc--; argv++;

    struct _ged_dm_info *gd = (struct _ged_dm_info *)ds;

    struct bu_vls list = BU_VLS_INIT_ZERO;
    dm_list_types(&list, "\n");
    bu_vls_printf(gd->gedp->ged_result_str, "%s\n", bu_vls_cstr(&list));
    bu_vls_free(&list);

    return BRLCAD_OK;
}

int
_dm_cmd_initmsg(void *ds, int argc, const char **argv)
{
    const char *usage_string = "dm [options] initmsg";
    const char *purpose_string = "display libdm plugin initialization messages.";
    if (_dm_cmd_msgs(ds, argc, argv, usage_string, purpose_string)) {
	return BRLCAD_OK;
    }

    struct _ged_dm_info *gd = (struct _ged_dm_info *)ds;

    bu_vls_printf(gd->gedp->ged_result_str, "%s\n", dm_init_msgs());
    return BRLCAD_OK;
}

int
_dm_cmd_list(void *ds, int argc, const char **argv)
{
    const char *usage_string = "dm [options] list";
    const char *purpose_string = "list display manager instances known to GED.";
    if (_dm_cmd_msgs(ds, argc, argv, usage_string, purpose_string)) {
	return BRLCAD_OK;
    }

    argc--; argv++;

    struct _ged_dm_info *gd = (struct _ged_dm_info *)ds;
    struct ged *gedp = gd->gedp;

    struct bview *cv = gedp->ged_gvp;
    struct dm *cdmp = (struct dm *)cv->dmp;
    if (cdmp) {
	// Current dmp first, if we have a current instance
	if (gd->verbosity) {
	    bu_vls_printf(gedp->ged_result_str, " %s (%s)\n", bu_vls_cstr(dm_get_pathname(cdmp)), dm_get_type(cdmp));
	} else {
	    bu_vls_printf(gedp->ged_result_str, "%s\n", bu_vls_cstr(dm_get_pathname(cdmp)));
	}
    }

    if (!BU_PTBL_LEN(&gedp->ged_views) && !cdmp) {
	bu_vls_printf(gedp->ged_result_str, ": no views defined in GED\n");
	return BRLCAD_ERROR;
    }
    int dm_cnt = 0;
    for (size_t i = 0; i < BU_PTBL_LEN(&gedp->ged_views); i++) {
	struct bview *gdvp = (struct bview *)BU_PTBL_GET(&gedp->ged_views, i);
	if (!gdvp->dmp)
	    continue;
	dm_cnt++;
    }
    if (!dm_cnt && !cdmp) {
	bu_vls_printf(gedp->ged_result_str, ": no views have associated DMs defined\n");
	return BRLCAD_ERROR;
    }

    for (size_t i = 0; i < BU_PTBL_LEN(&gedp->ged_views); i++) {
	struct bview *gdvp = (struct bview *)BU_PTBL_GET(&gedp->ged_views, i);
	struct dm *ndmp = (struct dm *)gdvp->dmp;
	if (!ndmp || ndmp == cdmp)
	    continue;
	if (gd->verbosity) {
	    bu_vls_printf(gedp->ged_result_str, " %s (%s)\n", bu_vls_cstr(dm_get_pathname(ndmp)), dm_get_type(ndmp));
	} else {
	    bu_vls_printf(gedp->ged_result_str, "%s\n", bu_vls_cstr(dm_get_pathname(ndmp)));
	}
    }

    return BRLCAD_OK;
}

int
_dm_cmd_get(void *ds, int argc, const char **argv)
{
    const char *usage_string = "dm [options] get [--dm name] [var]";
    const char *purpose_string = "report value(s) set to dm variables. With empty var, report all values.";
    if (_dm_cmd_msgs(ds, argc, argv, usage_string, purpose_string)) {
	return BRLCAD_OK;
    }

    argc--; argv++;

    struct bu_vls dm_name = BU_VLS_INIT_ZERO;

    struct bu_opt_desc d[2];
    BU_OPT(d[0], "d", "dm", "name", &bu_opt_vls, &dm_name, "dm instance to use when reporting");
    BU_OPT_NULL(d[1]);

    int ac = bu_opt_parse(NULL, argc, argv, d);

    struct _ged_dm_info *gd = (struct _ged_dm_info *)ds;
    struct dm *cdmp = _dm_find(gd, &dm_name);
    if (!cdmp) {
	bu_vls_free(&dm_name);
	return BRLCAD_ERROR;
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
	return BRLCAD_OK;
    }

    struct bu_structparse *dmparse = dm_get_vparse(cdmp);
    void *mvars = dm_get_mvars(cdmp);
    if (!dmparse || !mvars) {
	// No variables to report
	return BRLCAD_OK;
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

    return BRLCAD_ERROR;
}

int
_dm_cmd_set(void *ds, int argc, const char **argv)
{
    const char *usage_string = "dm [options] set [--dm name] key val";
    const char *purpose_string = "assign value to dm variable, if it exists.";
    if (_dm_cmd_msgs(ds, argc, argv, usage_string, purpose_string)) {
	return BRLCAD_OK;
    }

    argc--; argv++;

    struct bu_vls dm_name = BU_VLS_INIT_ZERO;

    struct bu_opt_desc d[2];
    BU_OPT(d[0], "d", "dm", "name", &bu_opt_vls, &dm_name, "dm instance to use when reporting");
    BU_OPT_NULL(d[1]);

    int ac = bu_opt_parse(NULL, argc, argv, d);

    struct _ged_dm_info *gd = (struct _ged_dm_info *)ds;
    struct dm *cdmp = _dm_find(gd, &dm_name);
    if (!cdmp) {
	bu_vls_free(&dm_name);
	return BRLCAD_ERROR;
    }

    if (ac != 2) {
	bu_vls_printf(gd->gedp->ged_result_str, ": invalid argument count - need key and value");
	return BRLCAD_ERROR;
    }

    struct bu_structparse *dmparse = dm_get_vparse(cdmp);
    void *mvars = dm_get_mvars(cdmp);
    if (!dmparse || !mvars) {
	// No variables to set
	bu_vls_printf(gd->gedp->ged_result_str, "display manager has not associated variables\n");
	return BRLCAD_ERROR;
    }

    struct bu_vls tmp_vls = BU_VLS_INIT_ZERO;
    int ret;
    bu_vls_printf(&tmp_vls, "%s=\"%s\"", argv[0], argv[1]);
    ret = bu_struct_parse(&tmp_vls, dmparse, (char *)mvars, NULL);
    bu_vls_free(&tmp_vls);
    if (ret < 0) {
	bu_vls_printf(gd->gedp->ged_result_str, ": unable to set %s", argv[0]);
	return BRLCAD_ERROR;
    }

    if (gd->verbosity) {
	if (gd->verbosity > 1) {
	    bu_vls_printf(gd->gedp->ged_result_str, "%s=", argv[0]);
	    bu_vls_struct_item_named(gd->gedp->ged_result_str, dmparse, argv[0], (const char *)mvars, COMMA);
	} else {
	    bu_vls_struct_item_named(gd->gedp->ged_result_str, dmparse, argv[0], (const char *)mvars, COMMA);
	}
    }

    return BRLCAD_OK;
}

int
_dm_cmd_attach(void *ds, int argc, const char **argv)
{
    const char *usage_string = "dm [options] attach type [name]";
    const char *purpose_string = "create a DM of the specified type";
    if (_dm_cmd_msgs(ds, argc, argv, usage_string, purpose_string)) {
	return BRLCAD_OK;
    }

    argc--; argv++;

    struct _ged_dm_info *gd = (struct _ged_dm_info *)ds;
    struct ged *gedp = gd->gedp;
    struct bu_vls dm_name = BU_VLS_INIT_ZERO;

    if (argc != 1 && argc != 2) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s", usage_string);
	return BRLCAD_ERROR;
    }

    if (argc == 1) {
	// No name - generate one
	bu_vls_sprintf(&dm_name, "%s-0", argv[0]);
	int exists = 0;
	for (size_t i = 0; i < BU_PTBL_LEN(&gedp->ged_views); i++) {
	    struct bview *gdvp = (struct bview *)BU_PTBL_GET(&gedp->ged_views, i);
	    struct dm *ndmp = (struct dm *)gdvp->dmp;
	    if (!ndmp)
		continue;
	    if (BU_STR_EQUAL(bu_vls_cstr(dm_get_pathname(ndmp)), bu_vls_cstr(&dm_name))) {
		exists = 1;
		break;
	    }
	}
	int tries = 0;
	while (exists && tries < DM_MAX_TRIES) {
	    bu_vls_incr(&dm_name, NULL, "0:0:0:0:-", NULL, NULL);
	    exists = 0;
	    for (size_t i = 0; i < BU_PTBL_LEN(&gedp->ged_views); i++) {
		struct bview *gdvp = (struct bview *)BU_PTBL_GET(&gedp->ged_views, i);
		struct dm *ndmp = (struct dm *)gdvp->dmp;
		if (!ndmp)
		    continue;
		if (BU_STR_EQUAL(bu_vls_cstr(dm_get_pathname(ndmp)), bu_vls_cstr(&dm_name))) {
		    exists = 1;
		    break;
		}
	    }
	    tries++;
	}
	if (tries == DM_MAX_TRIES) {
	    bu_vls_printf(gedp->ged_result_str, "unable to generate DM name");
	    bu_vls_free(&dm_name);
	    return BRLCAD_ERROR;
	}
    } else {
	// Have name - see if it already exists
	bu_vls_sprintf(&dm_name, "%s", argv[1]);
	int exists = 0;
	for (size_t i = 0; i < BU_PTBL_LEN(&gedp->ged_views); i++) {
	    struct bview *gdvp = (struct bview *)BU_PTBL_GET(&gedp->ged_views, i);
	    struct dm *ndmp = (struct dm *)gdvp->dmp;
	    if (!ndmp)
		continue;
	    if (BU_STR_EQUAL(bu_vls_cstr(dm_get_pathname(ndmp)), bu_vls_cstr(&dm_name))) {
		exists = 1;
		break;
	    }
	}
	if (exists) {
	    bu_vls_printf(gedp->ged_result_str, "DM %s already exists", bu_vls_cstr(&dm_name));
	    bu_vls_free(&dm_name);
	    return BRLCAD_ERROR;
	}
    }

    struct bview *target_view = (gedp->ged_gvp->dmp) ? NULL : gedp->ged_gvp;
    if (!target_view) {
	BU_GET(target_view, struct bview);
	bv_init(target_view);
	bu_ptbl_ins(&gedp->ged_views, (long *)target_view);
    }

    // Make sure the view width and height are non-zero if we're in a
    // "headless" mode without a graphical display
    if (!target_view->gv_width) {
	target_view->gv_width = 512;
    }
    if (!target_view->gv_height) {
	target_view->gv_height = 512;
    }

    if (!gedp->ged_ctx) {
	gedp->ged_ctx = (void *)target_view;
    }

    const char *acmd = "attach";
    struct dm *dmp = dm_open(gedp->ged_ctx, gedp->ged_interp, argv[0], 1, &acmd);
    if (!dmp) {
	bu_vls_printf(gedp->ged_result_str, "failed to create DM %s", bu_vls_cstr(&dm_name));
	return BRLCAD_ERROR;
    }

    dm_set_vp(dmp, &gedp->ged_gvp->gv_scale);
    dm_configure_win(dmp, 0);
    dm_set_pathname(dmp, bu_vls_cstr(&dm_name));
    dm_set_zbuffer(dmp, 1);
    fastf_t windowbounds[6] = { -1, 1, -1, 1, (int)GED_MIN, (int)GED_MAX };
    dm_set_win_bounds(dmp, windowbounds);

    // We have the dmp - let the view know
    target_view->dmp = dmp;

    return BRLCAD_OK;
}

int
_dm_cmd_width(void *ds, int argc, const char **argv)
{
    const char *usage_string = "dm [options] width [name]";
    const char *purpose_string = "report current width in pixels of display manager.";
    if (_dm_cmd_msgs(ds, argc, argv, usage_string, purpose_string)) {
	return BRLCAD_OK;
    }

    argc--; argv++;

    struct bu_vls tmpname = BU_VLS_INIT_ZERO;
    struct _ged_dm_info *gd = (struct _ged_dm_info *)ds;
    bu_vls_sprintf(&tmpname, "%s", argv[0]);
    struct dm *cdmp = _dm_find(gd, &tmpname);
    bu_vls_free(&tmpname);
    if (!cdmp) {
	return BRLCAD_ERROR;
    }
    bu_vls_printf(gd->gedp->ged_result_str, "%d\n", dm_get_width(cdmp));
    return BRLCAD_OK;
}

int
_dm_cmd_height(void *ds, int argc, const char **argv)
{
    const char *usage_string = "dm [options] height [name]";
    const char *purpose_string = "report current height in pixels of display manager.";
    if (_dm_cmd_msgs(ds, argc, argv, usage_string, purpose_string)) {
	return BRLCAD_OK;
    }

    argc--; argv++;

    struct bu_vls tmpname = BU_VLS_INIT_ZERO;
    struct _ged_dm_info *gd = (struct _ged_dm_info *)ds;
    bu_vls_sprintf(&tmpname, "%s", argv[0]);
    struct dm *cdmp = _dm_find(gd, &tmpname);
    bu_vls_free(&tmpname);
    if (!cdmp) {
	return BRLCAD_ERROR;
    }

    bu_vls_printf(gd->gedp->ged_result_str, "%d\n", dm_get_height(cdmp));
    return BRLCAD_OK;
}

const struct bu_cmdtab _dm_cmds[] = {
    { "attach",          _dm_cmd_attach},
    { "bg",              _dm_cmd_bg},
    { "get",             _dm_cmd_get},
    { "height",          _dm_cmd_height},
    { "initmsg",         _dm_cmd_initmsg},
    { "list",            _dm_cmd_list},
    { "set",             _dm_cmd_set},
    { "type",            _dm_cmd_type},
    { "types",           _dm_cmd_types},
    { "width",           _dm_cmd_width},
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
	return BRLCAD_ERROR;
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
	return BRLCAD_OK;
    }

    int ret;
    if (bu_cmd(_dm_cmds, ac, argv, 0, (void *)&gd, &ret) == BRLCAD_OK) {
	return ret;
    } else {
	bu_vls_printf(gedp->ged_result_str, "subcommand %s not defined", argv[0]);
    }

    return BRLCAD_ERROR;
}

#ifdef GED_PLUGIN
#include "../include/plugin.h"

extern int ged_ert_core(struct ged *gedp, int argc, const char *argv[]);
struct ged_cmd_impl ert_cmd_impl = {"ert", ged_ert_core, GED_CMD_DEFAULT};
const struct ged_cmd ert_cmd = { &ert_cmd_impl };

extern int ged_screen_grab_core(struct ged *gedp, int argc, const char *argv[]);
struct ged_cmd_impl screen_grab_cmd_impl = {"screen_grab", ged_screen_grab_core, GED_CMD_DEFAULT};
const struct ged_cmd screen_grab_cmd = { &screen_grab_cmd_impl };
struct ged_cmd_impl screengrab_cmd_impl = {"screengrab", ged_screen_grab_core, GED_CMD_DEFAULT};
const struct ged_cmd screengrab_cmd = { &screengrab_cmd_impl };

struct ged_cmd_impl dm_cmd_impl = {"dm", ged_dm_core, GED_CMD_DEFAULT};
const struct ged_cmd dm_cmd = { &dm_cmd_impl };

const struct ged_cmd *dm_cmds[] = { &screen_grab_cmd, &screengrab_cmd, &dm_cmd, &ert_cmd, NULL };

static const struct ged_plugin pinfo = { GED_API,  dm_cmds, 4 };

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
