/*                       E D I T 2 . C P P
 * BRL-CAD
 *
 * Copyright (c) 2008-2023 United States Government as represented by
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
/** @file libged/edit2.cpp
 *
 * Testing command for experimenting with editing routines.
 *
 * Among other things, we want the edit command to be aware of
 * the GED selection state, if we have a default one set and
 * the edit command doesn't explicitly specify an object or objects
 * to operate on.
 */

#include "common.h"

#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include "bu/cmd.h"
#include "bu/opt.h"

#include "../ged_private.h"
#include "./ged_edit2.h"

static int
_edit_cmd_msgs(void *einfop, int argc, const char **argv, const char *us, const char *ps)
{
    struct _ged_edit2_info *einfo = (struct _ged_edit2_info *)einfop;
    if (argc == 2 && BU_STR_EQUAL(argv[1], HELPFLAG)) {
	bu_vls_printf(einfo->gedp->ged_result_str, "%s\n%s\n", us, ps);
	return 1;
    }
    if (argc == 2 && BU_STR_EQUAL(argv[1], PURPOSEFLAG)) {
	bu_vls_printf(einfo->gedp->ged_result_str, "%s\n", ps);
	return 1;
    }
    return 0;
}

extern "C" int
_edit2_cmd_rotate(void *einfop, int argc, const char **argv)
{
    if (!einfop || !argc || !argv)
	return BRLCAD_ERROR;

    const char *usage_string = "edit [options] [geometry] rotate X Y Z";
    const char *purpose_string = "rotate specified primitive or comb instance";
    if (_edit_cmd_msgs(einfop, argc, argv, usage_string, purpose_string))
	return BRLCAD_OK;

    argc--; argv++;

    struct _ged_edit2_info *einfo = (struct _ged_edit2_info *)einfop;
    if (argc < 3 || !argv) {
	bu_vls_printf(einfo->gedp->ged_result_str, "%s\n", usage_string);
	return BRLCAD_ERROR;
    }

    return BRLCAD_OK;
}

extern "C" int
_edit2_cmd_tra(void *einfop, int argc, const char **argv)
{
    if (!einfop || !argc || !argv)
	return BRLCAD_ERROR;

    const char *usage_string = "edit [options] [geometry] tra X Y Z";
    const char *purpose_string = "translate specified primitive or comb instance relative to its current position";
    if (_edit_cmd_msgs(einfop, argc, argv, usage_string, purpose_string))
	return BRLCAD_OK;

    argc--; argv++;

    struct _ged_edit2_info *einfo = (struct _ged_edit2_info *)einfop;
    if (argc < 3 || !argv) {
	bu_vls_printf(einfo->gedp->ged_result_str, "%s\n", usage_string);
	return BRLCAD_ERROR;
    }

    return BRLCAD_OK;
}

extern "C" int
_edit2_cmd_translate(void *einfop, int argc, const char **argv)
{
    if (!einfop || !argc || !argv)
	return BRLCAD_ERROR;

    const char *usage_string = "edit [options] [geometry] translate X Y Z";
    const char *purpose_string = "translate specified primitive or comb instance to the specified aeinfopolute position";
    if (_edit_cmd_msgs(einfop, argc, argv, usage_string, purpose_string))
	return BRLCAD_OK;

    argc--; argv++;

    struct _ged_edit2_info *einfo = (struct _ged_edit2_info *)einfop;
    if (argc < 3 || !argv) {
	bu_vls_printf(einfo->gedp->ged_result_str, "%s\n", usage_string);
	return BRLCAD_ERROR;
    }

    return BRLCAD_OK;
}

extern "C" int
_edit2_cmd_scale(void *einfop, int argc, const char **argv)
{
    if (!einfop || !argc || !argv)
	return BRLCAD_ERROR;

    const char *usage_string = "edit [options] [geometry] scale factor";
    const char *purpose_string = "scale specified primitive or comb instance by the specified factor (must be greater than 0)";
    if (_edit_cmd_msgs(einfop, argc, argv, usage_string, purpose_string))
	return BRLCAD_OK;

    argc--; argv++;

    struct _ged_edit2_info *einfo = (struct _ged_edit2_info *)einfop;
    if (!argc || !argv) {
	bu_vls_printf(einfo->gedp->ged_result_str, "%s\n", usage_string);
	return BRLCAD_ERROR;
    }

    return BRLCAD_OK;
}

const struct bu_cmdtab _edit2_cmds[] = {
    { "rot",           _edit2_cmd_rotate},
    { "rotate",        _edit2_cmd_rotate},
    { "tra",           _edit2_cmd_tra},
    { "translate",     _edit2_cmd_translate},
    { "sca",           _edit2_cmd_scale},
    { "scale",         _edit2_cmd_scale},
    { (char *)NULL,    NULL}
};


extern "C" int
ged_edit2_core(struct ged *gedp, int argc, const char *argv[])
{
    int help = 0;
    struct bu_vls geom_specifier = BU_VLS_INIT_ZERO;
    struct _ged_edit2_info einfo;

    // Clear results buffer
    bu_vls_trunc(gedp->ged_result_str, 0);

    // If we don't have a context that allows us to edit, there's no point
    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_READ_ONLY(gedp, BRLCAD_ERROR);

    // We know we're the edit command
    argc--;argv++;

    // See if we have any high level options set
    struct bu_opt_desc d[4];
    BU_OPT(d[0], "h", "help",     "",          NULL,         &help,            "Print help");
    BU_OPT(d[1], "G", "geometry", "specifier", &bu_opt_vls,  &geom_specifier,  "Specify geometry to operate on");
    BU_OPT(d[2], "v", "verbose", "",           NULL,         &einfo.verbosity, "Verbose output");
    BU_OPT_NULL(d[3]);


    // Set up the edit container and initialize
    const struct bu_cmdtab *bcmds = (const struct bu_cmdtab *)_edit2_cmds;
    struct bu_opt_desc *bdesc = (struct bu_opt_desc *)d;
    einfo.verbosity = 0;
    einfo.gedp = gedp;
    einfo.cmds = bcmds;
    einfo.gopts = bdesc;

    const char *bargs_help = "[options] [geometry] subcommand [args]";
    if (!argc) {
	_ged_subcmd_help(gedp, bdesc, bcmds, "edit", bargs_help, &einfo, 0, NULL);
	return BRLCAD_OK;
    }

    // High level options are only defined prior to the subcommand
    int cmd_pos = -1;
    for (int i = 0; i < argc; i++) {
	if (bu_cmd_valid(_edit2_cmds, argv[i]) == BRLCAD_OK) {
	    cmd_pos = i;
	    break;
	}
    }

    int acnt = (cmd_pos >= 0) ? cmd_pos : argc;

    int opt_ret = bu_opt_parse(NULL, acnt, argv, d);

    if (help || opt_ret < 0) {
	if (cmd_pos >= 0) {
	    argc = argc - cmd_pos;
	    argv = &argv[cmd_pos];
	    _ged_subcmd_help(gedp, bdesc, bcmds, "edit", bargs_help, &einfo, argc, argv);
	} else {
	    _ged_subcmd_help(gedp, bdesc, bcmds, "edit", bargs_help, &einfo, 0, NULL);
	}
	return BRLCAD_OK;
    }

    // Must have a subcommand
    if (cmd_pos == -1) {
	bu_vls_printf(gedp->ged_result_str, ": no valid subcommand specified\n");
	_ged_subcmd_help(gedp, bdesc, bcmds, "edit", bargs_help, &einfo, 0, NULL);
	return BRLCAD_ERROR;
    }

    // TODO - need to unpack and validate these prior to subcommand processing - this
    // will be the same regardless of the specific edit operation, and we'll need
    // directory pointers, combs, paths, etc rather than strings for actual editing
    // to succeed.  No point in having the subcommands all replicate that.
    //
    // We'll need to give some thought to the interaction between command line object
    // specification and selection - the user may want to command line manipulate the
    // selected object without writing to disk (say, for example, as a precision part
    // of an otherwise interactive GUI editing session) and not expect that edit to
    // be immediately written to disk, but if they specify the object on the command
    // line explicitly it's possible they want to override the selection, discard any
    // uncommitted changes, and instead do the specified command line operation.
    //
    // My current thought is that if a command line object is explicitly
    // specified, and no options are supplied (say, -f to force a clearing of
    // the state and write, or -S to indicate an operation on a temporary
    // selection corresponding to the specified name in lieu of writing to
    // disk) we abort in the case of a specified object and an active selection
    // conflicting, with a note about how to proceed.  Note that the situation
    // is more complex than it might appear, since a primitive may be part of a
    // selected comb tree and have implications that aren't immediately obvious
    // - we'll have to implement something with the C++ state processing
    // powerful enough to detect and report such situations intelligently, as
    // well as rebuilding the selection state in the event of a forced edit
    // invalidating temporary selection editing objects.
    //
    // Another option might be for libged to maintain an explicit set of
    // temporary edit objects, and have the edit command track in-process
    // edits independent of the selection command - that way, a user could
    // also manually perform multiple intermediate edits on a specified object
    // that isn't selected without being forced to have each step written to
    // disk.  This is potentially useful in situations like large bot objects
    // where writing many copies to disk for multiple edits would be problematic
    // without frequent garbage_collect runs on the .g file.  The drawing layer
    // could then handle such states consistently regardless of selection state -
    // perhaps ghosting in the proposed new state unless the selection flag is
    // set, and ghosting the saved-to-disk state when selection is active.
    if (opt_ret > 0) {
	for (int gcnt = 0; gcnt < opt_ret; gcnt++) {
	    einfo.geom_objs.push_back(std::string(argv[gcnt]));
	}
    } else {
	// If nothing is specified, we need at least one selected geometry item or
	// we have nothing to work on
	std::vector<BSelectState *> ss = gedp->dbi_state->get_selected_states("default");
	if (ss.size()) {
	    einfo.geom_objs = ss[0]->list_selected_paths();
	}
    }

    if (!einfo.geom_objs.size()) {
	bu_vls_printf(gedp->ged_result_str, "Error: no geometry objects specified or selected\n");
	return BRLCAD_ERROR;
    }

    // Jump the processing past any options specified
    argc = argc - cmd_pos;
    argv = &argv[cmd_pos];

    // Execute the subcommand
    int ret;
    if (bu_cmd(_edit2_cmds, argc, argv, 0, (void *)&einfo, &ret) == BRLCAD_OK) {
	return ret;
    } else {
	bu_vls_printf(gedp->ged_result_str, "subcommand %s not defined", argv[0]);
    }

    return BRLCAD_ERROR;
}



// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8

