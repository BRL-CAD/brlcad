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

#include <map>
#include <set>
#include <string>

#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include "bu/cmd.h"
#include "bu/opt.h"

#include "../ged_private.h"
#include "./ged_edit2.h"

// Container to hold information about top level options and
// specified geometry options common to all the subcommands.
struct edit_info {
    int verbosity;
    struct directory *dp;
};


// Rotate command
class cmd_rotate : public ged_subcmd {
    public:
	std::string usage()   { return std::string("edit [options] [geometry] rotate X Y Z"); }
	std::string purpose() { return std::string("rotate specified primitive or comb instance"); }
	int exec(struct ged *, void *, int, const char **);
};
static cmd_rotate edit_rotate_cmd;

int
cmd_rotate::exec(struct ged *gedp, void *u_data, int argc, const char **argv)
{
    if (!gedp || !u_data || !argc || !argv)
	return BRLCAD_ERROR;

    struct edit_info *einfo = (struct edit_info *)u_data;
    if (einfo->dp == RT_DIR_NULL)
	return BRLCAD_ERROR;

    argc--; argv++;

    if (argc < 3 || !argv) {
	bu_vls_printf(gedp->ged_result_str, "%s\n", usage().c_str());
	return BRLCAD_ERROR;
    }

    return BRLCAD_OK;
}


// Tra command
class cmd_tra : public ged_subcmd {
    public:
	std::string usage()   { return std::string("edit [options] [geometry] tra X Y Z"); }
	std::string purpose() { return std::string("translate specified primitive or comb instance relative to its current position"); }
	int exec(struct ged *, void *, int, const char **);
};
static cmd_tra edit_tra_cmd;

int
cmd_tra::exec(struct ged *gedp, void *u_data, int argc, const char **argv)
{
    if (!gedp || !u_data || !argc || !argv)
	return BRLCAD_ERROR;

    struct edit_info *einfo = (struct edit_info *)u_data;
    if (einfo->dp == RT_DIR_NULL)
	return BRLCAD_ERROR;

    argc--; argv++;

    if (argc < 3 || !argv) {
	bu_vls_printf(gedp->ged_result_str, "%s\n", usage().c_str());
	return BRLCAD_ERROR;
    }

    return BRLCAD_OK;
}


// Translate command
class cmd_translate : public ged_subcmd {
    public:
	std::string usage()   { return std::string("edit [options] [geometry] translate X Y Z"); }
	std::string purpose() { return std::string("translate specified primitive or comb instance to the specified absolute position"); }
	int exec(struct ged *, void *, int, const char **);
};
static cmd_translate edit_translate_cmd;

int
cmd_translate::exec(struct ged *gedp, void *u_data, int argc, const char **argv)
{
    if (!gedp || !u_data || !argc || !argv)
	return BRLCAD_ERROR;

    struct edit_info *einfo = (struct edit_info *)u_data;
    if (einfo->dp == RT_DIR_NULL)
	return BRLCAD_ERROR;

    argc--; argv++;

    if (argc < 3 || !argv) {
	bu_vls_printf(gedp->ged_result_str, "%s\n", usage().c_str());
	return BRLCAD_ERROR;
    }

    return BRLCAD_OK;
}


// Scale command
class cmd_scale : public ged_subcmd {
    public:
	std::string usage()   { return std::string("edit [options] [geometry] scale_factor"); }
	std::string purpose() { return std::string("scale specified primitive or comb instance by the specified factor (must be greater than 0)"); }
	int exec(struct ged *, void *, int, const char **);
};
static cmd_scale edit_scale_cmd;

int
cmd_scale::exec(struct ged *gedp, void *u_data, int argc, const char **argv)
{
    if (!gedp || !u_data || !argc || !argv)
	return BRLCAD_ERROR;

    struct edit_info *einfo = (struct edit_info *)u_data;
    if (einfo->dp == RT_DIR_NULL)
	return BRLCAD_ERROR;

    argc--; argv++;

    if (argc < 3 || !argv) {
	bu_vls_printf(gedp->ged_result_str, "%s\n", usage().c_str());
	return BRLCAD_ERROR;
    }

    return BRLCAD_OK;
}


extern "C" int
ged_edit2_core(struct ged *gedp, int argc, const char *argv[])
{
    int help = 0;
    int optend_pos = -1;
    std::vector<std::string> geom_objs;
    struct edit_info einfo;
    einfo.verbosity = 0;
    einfo.dp = RT_DIR_NULL;

    // Clear results buffer
    bu_vls_trunc(gedp->ged_result_str, 0);

    // If we don't have a context that allows us to edit, there's no point
    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_READ_ONLY(gedp, BRLCAD_ERROR);

    // We know we're the edit command
    argc--;argv++;

    // Set up our command map
    std::map<std::string, ged_subcmd *> edit_cmds;
    edit_cmds[std::string("rot")]       = &edit_rotate_cmd;
    edit_cmds[std::string("rotate")]    = &edit_rotate_cmd;
    edit_cmds[std::string("tra")]       = &edit_tra_cmd;
    edit_cmds[std::string("translate")] = &edit_translate_cmd;
    edit_cmds[std::string("sca")]       = &edit_scale_cmd;
    edit_cmds[std::string("scale")]     = &edit_scale_cmd;

    // See if we have any high level options set
    struct bu_opt_desc d[3];
    BU_OPT(d[0], "h", "help",     "",          NULL,         &help,              "Print help");
    BU_OPT(d[1], "v", "verbose", "",           NULL,         &einfo.verbosity,   "Verbose output");
    BU_OPT_NULL(d[2]);

    // Set up the edit container and initialize
    struct bu_opt_desc *bdesc = (struct bu_opt_desc *)d;

    const char *bargs_help = "[options] <geometry_specifier> subcommand [args]";
    if (!argc) {
	_ged_subcmd2_help(gedp, bdesc, edit_cmds, "edit", bargs_help, 0, NULL);
	return BRLCAD_OK;
    }

    // High level options are only defined prior to the geometry specifier
    std::vector<unsigned long long> gs;
    for (int i = 0; i < argc; i++) {
	// TODO - de-quote so we can support obj names matching options...
	gs = gedp->dbi_state->digest_path(argv[i]);
	if (gs.size()) {
	    optend_pos = i;
	    break;
	}
    }

    int acnt = (optend_pos >= 0) ? optend_pos : argc;

    int opt_ret = bu_opt_parse(NULL, acnt, argv, d);

    if (help || opt_ret < 0) {
	if (optend_pos >= 0) {
	    argc = argc - optend_pos;
	    argv = &argv[optend_pos];
	    _ged_subcmd2_help(gedp, bdesc, edit_cmds, "edit", bargs_help, argc, argv);
	} else {
	    _ged_subcmd2_help(gedp, bdesc, edit_cmds, "edit", bargs_help, 0, NULL);
	}
	return BRLCAD_OK;
    }

    // Must have a specifier
    if (optend_pos == -1) {
	bu_vls_printf(gedp->ged_result_str, ": no valid geometry specifier found, nothing to edit\n");
	_ged_subcmd2_help(gedp, bdesc, edit_cmds, "edit", bargs_help, 0, NULL);
	return BRLCAD_ERROR;
    }

    // There are some generic commands (rot, tra, etc.) that apply universally
    // to all geometry, but the majority of editing operations are specific to
    // each individual geometric primitive type.  We first decode the specifier,
    // to determine what operations we're able to support.
    struct directory *dp = gedp->dbi_state->get_hdp(gs[0]);
    if (!dp) {
	bu_vls_printf(gedp->ged_result_str, ": geometry specifier lookup failed for %s\n", argv[0]);
	return BRLCAD_ERROR;
    }

    if (gs.size() > 1) {
	bu_log("Comb instance specifier - only generic edit operations.\n");
    } else {
	// TODO - once we have the object type, ask librt what subcommands and parameters are
	// supported for this primitive.  We can offer these in addition to the standard
	// commands.
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
    //
    // For command line, if we do have temporary editing objects, the edit
    // command should reject an edit on an object with an intermediate state
    // active unless an explicit flag is set to make sure user intent is clear.
    // If no flag, error out with message and options.  If -i flag, edit the
    // intermediate, non-disk temporary state.  if -f/-w, apply the operation
    // (if any) and then finalize and write the intermediate state to disk.  If
    // -F, erase the intermediate state and start over with the on-disk state -
    // the equivalent to an MGED reset.
    if (opt_ret > 0) {
	for (int gcnt = 0; gcnt < opt_ret; gcnt++) {
	    geom_objs.push_back(std::string(argv[gcnt]));
	}
    } else {
	// If nothing is specified, we need at least one selected geometry item or
	// we have nothing to work on
	std::vector<BSelectState *> ss = gedp->dbi_state->get_selected_states("default");
	if (ss.size()) {
	    geom_objs = ss[0]->list_selected_paths();
	}
    }

    if (!geom_objs.size()) {
	bu_vls_printf(gedp->ged_result_str, "Error: no geometry objects specified or selected\n");
	return BRLCAD_ERROR;
    }

    // Jump the processing past any options specified
    argc = argc - optend_pos;
    argv = &argv[optend_pos];

    // Execute the subcommand
    return edit_cmds[std::string(argv[0])]->exec(gedp, &einfo, argc, argv);
}



// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8

