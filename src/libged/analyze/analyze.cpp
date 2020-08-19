/*                      A N A L Y Z E . C P P
 * BRL-CAD
 *
 * Copyright (c) 2020 United States Government as represented by
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
/** @file analyze.cpp
 *
 * The analyze command
 *
 */

#include "common.h"

extern "C" {
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
}

#include <limits>

extern "C" {
#include "bu/cmd.h"
#include "bu/opt.h"
#include "../ged_private.h"
#include "./ged_analyze.h"
}

#define HELPFLAG "--print-help"
#define PURPOSEFLAG "--print-purpose"

struct _ged_analyze_info {
    struct ged *gedp = NULL;
    const struct bu_cmdtab *cmds = NULL;
    struct bu_opt_desc *gopts = NULL;
    int verbosity = 0;
};


static int
_analyze_cmd_msgs(void *cs, int argc, const char **argv, const char *us, const char *ps)
{
    struct _ged_analyze_info *gc = (struct _ged_analyze_info *)cs;
    if (argc == 2 && BU_STR_EQUAL(argv[1], HELPFLAG)) {
	bu_vls_printf(gc->gedp->ged_result_str, "%s\n%s\n", us, ps);
	return 1;
    }
    if (argc == 2 && BU_STR_EQUAL(argv[1], PURPOSEFLAG)) {
	bu_vls_printf(gc->gedp->ged_result_str, "%s\n", ps);
	return 1;
    }
    return 0;
}

/* Analyze solid in internal form.
 * TODO - this switch table probably indicates this should
 * be a functab callback... */
/**
 * TODO: primitives that still need implementing
 * ehy
 * metaball
 * nmg
 */
static void
analyze_do_summary(struct ged *gedp, const struct rt_db_internal *ip)
{
    /* XXX Could give solid name, and current units, here */

    switch (ip->idb_type) {

	case ID_ARB8:
	    analyze_arb8(gedp, ip);
	    break;

	case ID_BOT:
	    analyze_general(gedp, ip);
	    break;

	case ID_ARBN:
	    analyze_arbn(gedp, ip);
	    break;

	case ID_ARS:
	    analyze_ars(gedp, ip);
	    break;

	case ID_TGC:
	    analyze_general(gedp, ip);
	    break;

	case ID_ELL:
	    analyze_general(gedp, ip);
	    break;

	case ID_TOR:
	    analyze_general(gedp, ip);
	    break;

	case ID_RPC:
	    analyze_general(gedp, ip);
	    break;

	case ID_ETO:
	    analyze_general(gedp, ip);
	    break;

	case ID_EPA:
	    analyze_general(gedp, ip);
	    break;

	case ID_PARTICLE:
	    analyze_general(gedp, ip);
	    break;

	case ID_SUPERELL:
	    analyze_superell(gedp, ip);
	    break;

	case ID_SKETCH:
	    analyze_sketch(gedp, ip);
	    break;

	case ID_HYP:
	    analyze_general(gedp, ip);
	    break;

	case ID_PIPE:
	    analyze_general(gedp, ip);
	    break;

	case ID_VOL:
	    analyze_general(gedp, ip);
	    break;

	 case ID_EXTRUDE:
	    analyze_general(gedp, ip);
	    break;

	 case ID_RHC:
	    analyze_general(gedp, ip);
	    break;

	default:
	    bu_vls_printf(gedp->ged_result_str, "\nanalyze: unable to process %s solid\n",
			  OBJ[ip->idb_type].ft_name);
	    break;
    }
}

extern "C" int
_analyze_cmd_summarize(void *bs, int argc, const char **argv)
{
    const char *usage_string = "analyze [options] summarize obj1 <obj2 ...>";
    const char *purpose_string = "Summary of analytical information about listed objects";
    if (_analyze_cmd_msgs(bs, argc, argv, usage_string, purpose_string)) {
	return GED_OK;
    }

    struct _ged_analyze_info *gc = (struct _ged_analyze_info *)bs;
    struct ged *gedp = gc->gedp;
    struct rt_db_internal intern;

    argc--; argv++;
    if (!argc) {
	bu_vls_printf(gc->gedp->ged_result_str, "%s\n", usage_string);
	return GED_ERROR;
    }

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* use the names that were input */
    for (int i = 0; i < argc; i++) {
	struct directory *ndp = db_lookup(gedp->ged_wdbp->dbip,  argv[i], LOOKUP_NOISY);
	if (ndp == RT_DIR_NULL)
	    continue;

	GED_DB_GET_INTERNAL(gedp, &intern, ndp, bn_mat_identity, &rt_uniresource, GED_ERROR);

	_ged_do_list(gedp, ndp, 1);
	analyze_do_summary(gedp, &intern);
	rt_db_free_internal(&intern);
    }

    return GED_OK;
}

extern "C" int
_analyze_cmd_help(void *bs, int argc, const char **argv)
{
    struct _ged_analyze_info *gc = (struct _ged_analyze_info *)bs;
    if (!argc || !argv || BU_STR_EQUAL(argv[0], "help")) {
	bu_vls_printf(gc->gedp->ged_result_str, "analyze [options] subcommand [args]\n");
	if (gc->gopts) {
	    char *option_help = bu_opt_describe(gc->gopts, NULL);
	    if (option_help) {
		bu_vls_printf(gc->gedp->ged_result_str, "Options:\n%s\n", option_help);
		bu_free(option_help, "help str");
	    }
	}
	bu_vls_printf(gc->gedp->ged_result_str, "Available subcommands:\n");
	const struct bu_cmdtab *ctp = NULL;
	int ret;
	const char *helpflag[2];
	helpflag[1] = PURPOSEFLAG;
	size_t maxcmdlen = 0;
	for (ctp = gc->cmds; ctp->ct_name != (char *)NULL; ctp++) {
	    maxcmdlen = (maxcmdlen > strlen(ctp->ct_name)) ? maxcmdlen : strlen(ctp->ct_name);
	}
	for (ctp = gc->cmds; ctp->ct_name != (char *)NULL; ctp++) {
	    bu_vls_printf(gc->gedp->ged_result_str, "  %s%*s", ctp->ct_name, (int)(maxcmdlen - strlen(ctp->ct_name)) +   2, " ");
	    if (!BU_STR_EQUAL(ctp->ct_name, "help")) {
		helpflag[0] = ctp->ct_name;
		bu_cmd(gc->cmds, 2, helpflag, 0, (void *)gc, &ret);
	    } else {
		bu_vls_printf(gc->gedp->ged_result_str, "print help and exit\n");
	    }
	}
    } else {
	int ret;
	const char **helpargv = (const char **)bu_calloc(argc+1, sizeof(char *), "help argv");
	helpargv[0] = argv[0];
	helpargv[1] = HELPFLAG;
	for (int i = 1; i < argc; i++) {
	    helpargv[i+1] = argv[i];
	}
	bu_cmd(gc->cmds, argc+1, helpargv, 0, (void *)gc, &ret);
	bu_free(helpargv, "help argv");
	return ret;
    }

    return GED_OK;
}


const struct bu_cmdtab _analyze_cmds[] = {
      { "summarize",            _analyze_cmd_summarize},
      { (char *)NULL,      NULL}
  };


extern "C" int
ged_analyze_core(struct ged *gedp, int argc, const char *argv[])
{
    int help = 0;
    struct _ged_analyze_info gc;
    gc.gedp = gedp;
    gc.cmds = _analyze_cmds;
    gc.verbosity = 0;

    // Sanity
    if (UNLIKELY(!gedp || !argc || !argv)) {
	return GED_ERROR;
    }

    // Clear results
    bu_vls_trunc(gedp->ged_result_str, 0);

    // See if we have any high level options set
    struct bu_opt_desc d[3];
    BU_OPT(d[0], "h", "help",    "",      NULL,                 &help,         "Print help");
    BU_OPT(d[1], "v", "verbose", "",      NULL,                 &gc.verbosity, "Verbose output");
    BU_OPT_NULL(d[2]);

    gc.gopts = d;

    if (argc == 1) {
	_analyze_cmd_help(&gc, 0, NULL);
	return GED_OK;
    }

    // High level options are only defined prior to the subcommand
    int cmd_pos = -1;
    for (int i = 1; i < argc; i++) {
	if (bu_cmd_valid(_analyze_cmds, argv[i]) == BRLCAD_OK) {
	    cmd_pos = i;
	    break;
	}
    }

    int acnt = (cmd_pos >= 0) ? cmd_pos : argc;

    bu_opt_parse(NULL, acnt, argv, d);

    if (help) {
	if (cmd_pos >= 0) {
	    argc = argc - cmd_pos;
	    argv = &argv[cmd_pos];
	    _analyze_cmd_help(&gc, argc, argv);
	} else {
	    _analyze_cmd_help(&gc, 0, NULL);
	}
	return GED_OK;
    }

    // Jump the processing past any options specified. If we don't have a
    // subcommand, assume all args are geometry objects and the command mode is
    // summarize. This will get us the old behavior, except in the case where
    // we happen to have an object with a name that matches a subcommand of
    // analyze.  In that case, the full "analyze summarize objname" is needed.
    const char *scmd = "summarize";
    if (cmd_pos != -1) {
	argc = argc - cmd_pos;
	argv = &argv[cmd_pos];
    } else {
	argv[0] = scmd;
    }

    int ret;
    if (bu_cmd(_analyze_cmds, argc, argv, 0, (void *)&gc, &ret) == BRLCAD_OK) {
	return ret;
    } else {
	bu_vls_printf(gedp->ged_result_str, "subcommand %s not defined", argv[0]);
    }

    return GED_ERROR;
}

// Local Variables:

#ifdef GED_PLUGIN
#include "../include/plugin.h"
extern "C" {
    struct ged_cmd_impl analyze_cmd_impl = { "analyze", ged_analyze_core, GED_CMD_DEFAULT };
    const struct ged_cmd analyze_cmd = { &analyze_cmd_impl };

    const struct ged_cmd *analyze_cmds[] = { &analyze_cmd, NULL };

    static const struct ged_plugin pinfo = { GED_API,  analyze_cmds, 1 };

    COMPILER_DLLEXPORT const struct ged_plugin *ged_plugin_info()
    {
	return &pinfo;
    }
}
#endif

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8

