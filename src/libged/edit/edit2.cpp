/*                       E D I T 2 . C P P
 * BRL-CAD
 *
 * Copyright (c) 2008-2025 United States Government as represented by
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
 *
 * In the long term we want this command to pull the necessary info for each
 * primitive type from librt.  If we also want to enable the various options
 * for rotate, translate and scale developed in the edit.c file, there will be
 * a fair bit of design work to think through.  Probably the option parsing for
 * rotate, translate and scale should actually reside in librt, and be
 * available via the primitive edit functab in some fashion along with parameter
 * specific command line parsing.
 *
 * The current librt editing API (as of 2025) is most likely no in shape to
 * properly support what we would want in an edit command, and that is
 * deliberately not the initial focus of that effort.  The initial goal is to
 * extract MGED's primitive editing abilities into a form that is both testable
 * with non-GUI inputs and functional (or a least as functional as it was
 * previously) within MGED itself.  That's one reason that API should not be
 * considered stable - it is intended to eventually be the canonical API for
 * all geometry changes, and that will undoubtedly require maturing it beyond
 * its current "extracted from MGED" form.  However, because of how invasive
 * and difficult the migration out of MGED proved, we can't afford to try to
 * tackle too many goals at once - the first order of business is testable
 * primitive editing on MGED's level of capability. We will proceed further
 * once that milestone is achieved.
 */

#include "common.h"

#include <map>
#include <set>
#include <string>

#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include "./uri.hh"

#include "bu/cmd.h"
#include "bu/opt.h"

#include "../ged_private.h"
#include "../dbi.h"
#include "./ged_edit2.h"

// Container to hold information about top level options and
// specified geometry options common to all the subcommands.
struct edit_info {
    int verbosity;
    struct directory *dp;
    struct ged *gedp;
    float *prand;
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
	std::string usage()   { return std::string("edit [options] [geometry] scale factor"); }
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


// Perturb command
class cmd_perturb : public ged_subcmd {
    public:
	std::string usage()   { return std::string("edit [options] [geometry] perturb factor"); }
	std::string purpose() { return std::string("perturb primitive or primitives below comb by the specified factor (must be greater than 0)"); }
	int exec(struct ged *, void *, int, const char **);
	struct edit_info *einfo;
    private:
	int dp_perturb(struct directory *dp);
	fastf_t factor = 0;
};
static cmd_perturb edit_perturb_cmd;

int
cmd_perturb::dp_perturb(struct directory *dp)
{
    fastf_t lfactor = factor + factor*0.1*bn_rand_half(einfo->prand);
    bu_log("%s: %g\n", dp->d_namep, lfactor);
    // Get the rt_db_internal
    struct rt_db_internal intern;
    RT_DB_INTERNAL_INIT(&intern);
    struct db_i *dbip = einfo->gedp->dbip;
    if (rt_db_get_internal(&intern, dp, dbip, NULL, &rt_uniresource) < 0) {
	bu_log("rt_db_get_internal failed for %s\n", dp->d_namep);
	return BRLCAD_ERROR;
    }

    if (!intern.idb_meth || !intern.idb_meth->ft_perturb) {
	// If we have no perturbation method, it's a no-go
	return BRLCAD_ERROR;
    }

    struct rt_db_internal *pintern;
    if (intern.idb_meth->ft_perturb(&pintern, &intern, 1, lfactor) != BRLCAD_OK) {
	bu_log("librt perturbation failed for %s\n", dp->d_namep);
	return BRLCAD_ERROR;
    }
    if (!pintern) {
	bu_log("librt perturbation failed for %s\n", dp->d_namep);
	return BRLCAD_ERROR;
    }

    std::string oname(dp->d_namep);
    db_delete(dbip, dp);
    db_dirdelete(dbip, dp);
    struct directory *ndp = db_diradd(dbip, oname.c_str(), RT_DIR_PHONY_ADDR, 0, RT_DIR_SOLID, (void *)&pintern->idb_type);
    if (ndp == RT_DIR_NULL) {
	bu_log("Cannot add %s to directory\n", oname.c_str());
	rt_db_free_internal(pintern);
	return BRLCAD_ERROR;
    }

    if (rt_db_put_internal(ndp, dbip, pintern, &rt_uniresource) < 0) {
	bu_log("Failed to write %s to database\n", oname.c_str());
	rt_db_free_internal(pintern);
	return BRLCAD_ERROR;
    }

    return BRLCAD_OK;
}

int
cmd_perturb::exec(struct ged *gedp, void *u_data, int argc, const char **argv)
{
    if (!gedp || !u_data || !argc || !argv)
	return BRLCAD_ERROR;

    einfo = (struct edit_info *)u_data;
    if (einfo->dp == RT_DIR_NULL)
	return BRLCAD_ERROR;

    argc--; argv++;

    if (argc < 1 || !argv) {
	bu_vls_printf(gedp->ged_result_str, "%s\n", usage().c_str());
	return BRLCAD_ERROR;
    }

    if (bu_opt_fastf_t(NULL, 1, argv, (void *)&factor) != 1) {
	bu_vls_printf(gedp->ged_result_str, "%s\n", usage().c_str());
	return BRLCAD_ERROR;
    }
    if (NEAR_ZERO(factor, SMALL_FASTF))
	return BRLCAD_OK;

    struct bu_ptbl objs = BU_PTBL_INIT_ZERO;
    if (db_search(&objs, DB_SEARCH_RETURN_UNIQ_DP, "-type shape", 1, &einfo->dp, einfo->gedp->dbip, NULL, NULL, NULL) < 0) {
    	bu_vls_printf(gedp->ged_result_str, "search error\n");
	return BRLCAD_ERROR;
    }
    if (!BU_PTBL_LEN(&objs)) {
    	bu_vls_printf(gedp->ged_result_str, "no solids\n");
	return BRLCAD_OK;
    }

    int ret = BRLCAD_OK;
    for (size_t i = 0; i < BU_PTBL_LEN(&objs); i++) {
	struct directory *odp = (struct directory *)BU_PTBL_GET(&objs, i);
	int oret = dp_perturb(odp);
	einfo->gedp->dbi_state->changed.insert(odp);
	if (oret != BRLCAD_OK)
	    ret = BRLCAD_ERROR;
    }
    bu_ptbl_free(&objs);

    einfo->gedp->dbi_state->Sync();

    return ret;
}


extern "C" int
ged_edit2_core(struct ged *gedp, int argc, const char *argv[])
{
    int help = 0;
    std::vector<std::string> geom_objs;
    struct edit_info einfo;
    einfo.verbosity = 0;
    einfo.dp = RT_DIR_NULL;
    einfo.gedp = gedp;
    bn_rand_init(einfo.prand, 0);

    // Clear results buffer
    bu_vls_trunc(gedp->ged_result_str, 0);

    // If we don't have a context that allows us to edit, there's no point
    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_READ_ONLY(gedp, BRLCAD_ERROR);

    // We know we're the edit command
    argc--;argv++;

    // Set up our command map.  Most command editing operations
    // will be primitive specific, but there are some that are
    // common to most object types
    std::map<std::string, ged_subcmd *> edit_cmds;
    edit_cmds[std::string("rot")]       = &edit_rotate_cmd;
    edit_cmds[std::string("rotate")]    = &edit_rotate_cmd;
    edit_cmds[std::string("tra")]       = &edit_tra_cmd;
    edit_cmds[std::string("translate")] = &edit_translate_cmd;
    edit_cmds[std::string("sca")]       = &edit_scale_cmd;
    edit_cmds[std::string("scale")]     = &edit_scale_cmd;
    edit_cmds[std::string("perturb")]   = &edit_perturb_cmd;

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

    // If we're sure we don't have high level options, we can be more
    // confident about reporting when the user specifies an invalid
    // geometry specifier.  If we do have options in play, we can't
    // reliably distinguish between an option argument and an invalid
    // geometry specifier - the string may be intended as either.
    bool maybe_opts =(argv[0][0] == '-') ? true : false;

    // High level options are only defined prior to the geometry specifier
    // and/or subcommand.
    int geom_pos = INT_MAX;
    DbiPath gs;
    for (int i = 0; i < argc; i++) {
	// TODO - de-quote so we can support obj names matching options...
	// TODO - Allow URI specifications to call out primitive params
	try
	{
	    uri obj_uri(std::string("g:") + std::string(argv[i]));
	    std::string obj_path = obj_uri.get_path();
	    if (!obj_path.length()) {
		obj_path = std::string(argv[i]);
	    } else {
		// TODO - store query and fragment info - for example, fragment
		// could be use to specify an edge or vertex to move on an arb

		// obj.s#V1
		if (obj_uri.get_fragment().length() > 0)
		    bu_log("have fragment: %s\n", obj_uri.get_fragment().c_str());
		// obj.s?V*
		if (obj_uri.get_query().length() > 0)
		    bu_log("have query: %s\n", obj_uri.get_query().c_str());
	    }
	    gs.Init(obj_path.c_str());
	}
	catch (std::invalid_argument &uri_e)
	{
	    std::string uri_error = uri_e.what();
	    bu_log("invalid uri: %s\n", uri_error.c_str());
	    gs.Init(argv[i]);
	}
	if (gs.valid()) {
	    geom_pos = i;

	    break;
	}
    }
    // Record geometry specifier
    const char *geom_str = (geom_pos < INT_MAX) ? argv[geom_pos] : NULL;

    // We can't recognize primitive specific subcommands without a primitive
    // to key on, but we can recognize the general commands.
    int cmd_pos = INT_MAX;
    std::map<std::string, ged_subcmd *>::iterator e_it;
    for (e_it = edit_cmds.begin(); e_it != edit_cmds.end(); ++e_it) {
	for (int i = 0; i < argc; i++) {
	    std::string astr(argv[i]);
	    if (e_it->first == astr) {
		cmd_pos = i;
		break;
	    }
	}
    }
    // Record command string
    const char *cmd_str = (cmd_pos < INT_MAX) ? argv[cmd_pos] : NULL;

    // If we have no geometry specifier and no command, or a generic command
    // precedes the geometry specifier, we're just going to print generic help
    if (geom_pos == INT_MAX && cmd_pos == INT_MAX) {
	if (!maybe_opts && geom_pos == INT_MAX) {
	    bu_vls_printf(gedp->ged_result_str, "Invalid geometry specifier: %s\n", argv[0]);
	} else {
	    _ged_subcmd2_help(gedp, bdesc, edit_cmds, "edit", bargs_help, 0, NULL);
	}
	return BRLCAD_ERROR;
    }
    if (geom_pos < INT_MAX && cmd_pos < INT_MAX && (geom_pos > cmd_pos || cmd_pos != geom_pos + 1)) {
	_ged_subcmd2_help(gedp, bdesc, edit_cmds, "edit", bargs_help, 0, NULL);
	return BRLCAD_ERROR;
    }

    int acnt = (geom_pos < cmd_pos) ? geom_pos : cmd_pos;

    // Check whether we might have high level options
    struct bu_vls opterrs = BU_VLS_INIT_ZERO;
    int opt_ret = bu_opt_parse(&opterrs, acnt, argv, d);
    // If we had any high level option errors, print help
    if (opt_ret < 0) {
	bu_vls_printf(gedp->ged_result_str, "%s", bu_vls_cstr(&opterrs));
	_ged_subcmd2_help(gedp, bdesc, edit_cmds, "edit", bargs_help, 0, NULL);
	bu_vls_free(&opterrs);
	return BRLCAD_ERROR;
    }
    bu_vls_free(&opterrs);

    // If we had something the options didn't process, it's either an option
    // error or invalid geometry processor.
    if (opt_ret) {
	bu_vls_printf(gedp->ged_result_str, "Invalid geometry specifier: %s", argv[0]);
	return BRLCAD_ERROR;
    }

    // Because we limited the subset of argv bu_opt was processing, we need to
    // manually shift anything still remaining to the beginning of the argv
    // array for subsequent processing.
    if (argc > acnt) {
	int remaining = argc - acnt;
	for (int i = 0; i < remaining; i++) {
	    argv[i] = argv[acnt+i];
	}
    }
    argc -= acnt;

    // Clear geometry specifier, if present
    if (geom_str) {
	argc--; argv++;
    }

    if (help) {
	if (cmd_str) {
	    _ged_subcmd2_help(gedp, bdesc, edit_cmds, cmd_str, bargs_help, argc, argv);
	} else {
	    _ged_subcmd2_help(gedp, bdesc, edit_cmds, "edit", bargs_help, 0, NULL);
	}
	return BRLCAD_OK;
    }

    // Must have a specifier to continue further - we can print command
    // specific help without a specifier, but we can't do any actual processing
    // without one.
    if (!geom_str) {
	bu_vls_printf(gedp->ged_result_str, ": no valid geometry specifier found, nothing to edit\n");
	_ged_subcmd2_help(gedp, bdesc, edit_cmds, "edit", bargs_help, 0, NULL);
	return BRLCAD_ERROR;
    }

    // There are some generic commands (rot, tra, etc.) that apply universally
    // to all geometry, but the majority of editing operations are specific to
    // each individual geometric primitive type.  We first decode the specifier,
    // to determine what operations we're able to support.
    einfo.dp = gs.GetDp();
    if (gs.Depth() == 0 && !einfo.dp) {
	bu_vls_printf(gedp->ged_result_str, ": geometry specifier lookup failed for %s\n", argv[0]);
	return BRLCAD_ERROR;
    }

    if (gs.Depth() > 1) {
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
#if 0
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
#endif

    // Execute the subcommand
    if (!cmd_str) {
	bu_log("No subcommand specified\n");
	return BRLCAD_ERROR;
    }

    return edit_cmds[std::string(cmd_str)]->exec(gedp, &einfo, argc, argv);
}



// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8

