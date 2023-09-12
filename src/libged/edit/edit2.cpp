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

// Common base class method to allow subcmd help printing to collect and
// format the help from all commands, as well as allow executing the
// command from a map of class pointers.
class ged_subcmd {
    public:
	virtual ~ged_subcmd() {}
	virtual std::string usage()   { return std::string(""); }
	virtual std::string purpose() { return std::string(""); }
	virtual int exec(struct ged *, int, const char **) { return BRLCAD_ERROR; }
};

int
_ged_subcmd2_help(struct ged *gedp, struct bu_opt_desc *gopts, std::map<std::string, ged_subcmd *> &subcmds, const char *cmdname, const char *cmdargs, int argc, const char **argv)
{
    if (!gedp || !gopts || !cmdname)
	return BRLCAD_ERROR;

    if (!argc || !argv || BU_STR_EQUAL(argv[0], "help")) {
	bu_vls_printf(gedp->ged_result_str, "%s %s\n", cmdname, cmdargs);
	if (gopts) {
	    char *option_help = bu_opt_describe(gopts, NULL);
	    if (option_help) {
		bu_vls_printf(gedp->ged_result_str, "Options:\n%s\n", option_help);
		bu_free(option_help, "help str");
	    }
	}
	bu_vls_printf(gedp->ged_result_str, "Available subcommands:\n");

	// It's possible for a command table to associate multiple strings with
	// the same function, for compatibility or convenience.  In those
	// instances, rather than repeat the same line, we instead group all
	// strings leading to the same subcommand together.
	
	// Group the strings referencing the same method
	std::map<ged_subcmd *, std::set<std::string>> cmd_strings;
	std::map<std::string, ged_subcmd *>::iterator cm_it;
	for (cm_it = subcmds.begin(); cm_it != subcmds.end(); cm_it++)
	    cmd_strings[cm_it->second].insert(cm_it->first);
	
	// Generate the labels to be printed
	std::map<ged_subcmd *, std::string> cmd_labels;
	std::map<ged_subcmd *, std::set<std::string>>::iterator c_it;
	for (c_it = cmd_strings.begin(); c_it != cmd_strings.end(); c_it++) {
	    std::set<std::string>::iterator s_it;
	    std::string label;
	    for (s_it = c_it->second.begin(); s_it != c_it->second.end(); s_it++) {
		if (s_it != c_it->second.begin())
		    label.append(std::string(","));
		label.append(*s_it);
	    }
	    cmd_labels[c_it->first] = label;
	}

	// Find the maximum label length we need to accommodate when printing
	std::map<ged_subcmd *, std::string>::iterator l_it;
	size_t maxcmdlen = 0;
	for (l_it = cmd_labels.begin(); l_it != cmd_labels.end(); l_it++)
	    maxcmdlen = (maxcmdlen > l_it->second.length()) ? maxcmdlen : l_it->second.length();

	// Generate the help table
	std::set<ged_subcmd *> processed_funcs;
	for (cm_it = subcmds.begin(); cm_it != subcmds.end(); cm_it++) {
	    // We're only going to print one line per unique, even if the
	    // command has multiple aliases in the table
	    if (processed_funcs.find(cm_it->second) != processed_funcs.end())
		continue;
	    processed_funcs.insert(cm_it->second);

	    // Unseen command - print
	    std::string &lbl = cmd_labels[cm_it->second];
	    bu_vls_printf(gedp->ged_result_str, "  %s%*s", lbl.c_str(), (int)(maxcmdlen - lbl.length()) +   2, " ");
	    bu_vls_printf(gedp->ged_result_str, "%s\n", cm_it->second->purpose().c_str());
	}
    } else {

	std::map<std::string, ged_subcmd *>::iterator cm_it = subcmds.find(std::string(cmdname));
	if (cm_it == subcmds.end())
	    return BRLCAD_ERROR;
	bu_vls_printf(gedp->ged_result_str, "%s", cm_it->second->usage().c_str());
	return BRLCAD_OK;

    }

    return BRLCAD_OK;
}

// Rotate command
class cmd_rotate : public ged_subcmd {
    public:
	~cmd_rotate() {};
	std::string usage()   { return std::string("edit [options] [geometry] rotate X Y Z"); }
	std::string purpose() { return std::string("rotate specified primitive or comb instance"); }
	int exec(struct ged *, int, const char **);
};
static cmd_rotate edit_rotate_cmd;

int
cmd_rotate::exec(struct ged *gedp, int argc, const char **argv)
{
    if (!gedp || !argc || !argv)
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
	~cmd_tra() {};
	std::string usage()   { return std::string("edit [options] [geometry] tra X Y Z"); }
	std::string purpose() { return std::string("translate specified primitive or comb instance relative to its current position"); }
	int exec(struct ged *, int, const char **);
};
static cmd_tra edit_tra_cmd;

int
cmd_tra::exec(struct ged *gedp, int argc, const char **argv)
{
    if (!gedp || !argc || !argv)
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
	~cmd_translate() {};
	std::string usage()   { return std::string("edit [options] [geometry] translate X Y Z"); }
	std::string purpose() { return std::string("translate specified primitive or comb instance to the specified absolute position"); }
	int exec(struct ged *, int, const char **);
};
static cmd_translate edit_translate_cmd;

int
cmd_translate::exec(struct ged *gedp, int argc, const char **argv)
{
    if (!gedp || !argc || !argv)
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
	~cmd_scale() {};
	std::string usage()   { return std::string("edit [options] [geometry] scale_factor"); }
	std::string purpose() { return std::string("scale specified primitive or comb instance by the specified factor (must be greater than 0)"); }
	int exec(struct ged *, int, const char **);
};
static cmd_scale edit_scale_cmd;

int
cmd_scale::exec(struct ged *gedp, int argc, const char **argv)
{
    if (!gedp || !argc || !argv)
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
    int verbosity = 0;
    int cmd_pos = -1;
    struct bu_vls geom_specifier = BU_VLS_INIT_ZERO;
    std::vector<std::string> geom_objs;

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
    struct bu_opt_desc d[4];
    BU_OPT(d[0], "h", "help",     "",          NULL,         &help,            "Print help");
    BU_OPT(d[1], "G", "geometry", "specifier", &bu_opt_vls,  &geom_specifier,  "Specify geometry to operate on");
    BU_OPT(d[2], "v", "verbose", "",           NULL,         &verbosity,       "Verbose output");
    BU_OPT_NULL(d[3]);

    // Set up the edit container and initialize
    struct bu_opt_desc *bdesc = (struct bu_opt_desc *)d;

    const char *bargs_help = "[options] [geometry] subcommand [args]";
    if (!argc) {
	_ged_subcmd2_help(gedp, bdesc, edit_cmds, "edit", bargs_help, 0, NULL);
	return BRLCAD_OK;
    }

    // High level options are only defined prior to the subcommand
    for (int i = 0; i < argc; i++) {
	if (edit_cmds.find(std::string(argv[i])) != edit_cmds.end()) {
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
	    _ged_subcmd2_help(gedp, bdesc, edit_cmds, "edit", bargs_help, argc, argv);
	} else {
	    _ged_subcmd2_help(gedp, bdesc, edit_cmds, "edit", bargs_help, 0, NULL);
	}
	return BRLCAD_OK;
    }

    // Must have a subcommand
    if (cmd_pos == -1) {
	bu_vls_printf(gedp->ged_result_str, ": no valid subcommand specified\n");
	_ged_subcmd2_help(gedp, bdesc, edit_cmds, "edit", bargs_help, 0, NULL);
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
    argc = argc - cmd_pos;
    argv = &argv[cmd_pos];

    // Execute the subcommand
    return edit_cmds[std::string(argv[0])]->exec(gedp, argc, argv);
}



// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8

