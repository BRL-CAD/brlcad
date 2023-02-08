/*                       S E L E C T . C P P
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
/** @file libged/select.cpp
 *
 * The select command.
 *
 * "Sets" in BRL-CAD have some interesting complications, particularly when it
 * comes to geometric scenes.  Graphically visible scene objects representing
 * .g geometry always correspond to one instance of one solid, or a set of data
 * representing an evaluation of a composite object.
 *
 * Because there may be thousands of such objects in a scene, commands such as
 * "who" report "collapsed" top level groups that represent the highest level
 * paths that contain beneath them all the drawn solids.   Generally this
 * report will be more compact and informative to users than a detailed
 * itemization of individual instance shape objects.
 *
 * However, when defining selection sets, it gets more complex.  Building selection
 * sets graphically by interrogating the scene will build up sets of individual
 * shape objects.  If the correct sub-sets of those objects are selected, we can
 * conceptually regard a parent comb as being fully selected, if we so choose.
 * However, we do not always want the "fully selected" parent comb to replace
 * all of its children in the set, since different types of actions may
 * want/need to operate on the individual comb instances rather than the parent
 * comb as an aggregate whole.
 *
 * We thus need "expand" and "collapse" operations for sets, the former of
 * which will take each entry in the set and replace it with all of its child
 * leaf instances.
 *
 * Likewise, the "collapse" operation will look at the children contained in the set,
 * and for cases where all children are present, those children will be replaced
 * with one reference to the parent.  This will be done working "up" the various
 * trees until either all potential collapses are missing one or more children, or
 * all entries collapse to top level objects.
 *
 * Nor do we want to always either expand OR collapse - in some cases we want
 * exactly what was selected.  For example, if we have the tree
 *
 * a
 *  b
 *   c
 *    d
 *     e
 *
 * and we want to select and operate on /a/b/c, we can't simply collapse (which
 * will result in /a) or expand (which will result in /a/b/c/d/e).  If /a/b/c/d/e
 * is part of a larger selection set and we want to replace it with /a/b/c, to
 * avoid a confusing multiple-path-instance state the selection command will need
 * to ensure all selections below /a/b/c are removed in favor of /a/b/c.
 *
 * As an exercise, we consider a potential use of sets - selecting an instance of
 * an object in a scene for editing operations.  There are a number of things we
 * will need to know:
 *
 * The solids associated with the selected instance itself.  If a solid was
 * selected we already know where to get the editing wireframe, but if comb was
 * selected from the tree, there's more work to do.  We have the advantage of
 * explicitly knowing the level at which the editing operations will take place
 * (a tree selection would correspond to the previously discussed "insert"
 * operation to the set), but we will need a list of that comb's child solids
 * so we can create appropriate editing wireframe visual objects to represent
 * the comb in the scene.  In the event of multiple selections we would need to
 * construct the instance set from multiple sources.
 *
 * If we are performing graphical selection operations, we may be either
 * refining a set by selecting objects to be removed from it, or selecting
 * objects not already part of the set to add to it.  In either case we will be
 * operating at the individual instance level, and must provide ways to allow
 * the user to refine their selection intent for editing purposes.
 *
 * If we want to (say) set all non-active objects to be highly transparent when
 * an editing operation commences, we will also need to be able to construct the
 * set of all scene objects which are NOT active in the currently processed set.
 *
 * If we want to edit primitive parameters, rather than comb instances, we will
 * need to boil down a set of selected comb instances to one or a set of solid
 * names.  (Usually a set count > 1 won't work for primitive editing if we have
 * multiple different independent solid leaf objects... in that case we may
 * want to just reject an attempt to solid edit...)  Given the solid, we will
 * then need to extract the set of all drawn instances of that solid from the
 * scene, so we can generate and update per-instance wireframes for all of them
 * to visually reflect the impact of the edit (MGED's inability to do this is
 * why solid editing is rejected if more than one instance of the object is
 * drawn in the scene - we don't want that limitation.)

 */

#include "common.h"

#include <string.h>

#include "bu/opt.h"
#include "../ged_private.h"

struct _ged_select_info {
    struct ged *gedp;
    struct bu_vls curr_set;
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
    if (!gedp->dbi_state || argc > 1)
	return BRLCAD_ERROR;

    if (!argc) {
	std::vector<std::string> ssets = gedp->dbi_state->list_selection_sets();
	for (size_t i = 0; i < ssets.size(); i++) {
	    bu_vls_printf(gedp->ged_result_str, "%s\n", ssets[i].c_str());
	}
	return BRLCAD_OK;
    }

    const char *sname = argv[0];
    std::vector<BSelectState *> ss = gedp->dbi_state->get_selected_states(sname);
    if (!ss.size()) {
	bu_vls_printf(gedp->ged_result_str, ": %s does not match any selection sets\n", sname);
	return BRLCAD_ERROR;
    }

    for (size_t i = 0; i < ss.size(); i++) {
	std::vector<std::string> paths = ss[i]->list_selected_paths();

	for (size_t j = 0; j < paths.size(); j++) {
	    bu_vls_printf(gedp->ged_result_str, "%s\n", paths[j].c_str());
	}
    }
    return BRLCAD_OK;
}

int
_select_cmd_clear(void *bs, int argc, const char **argv)
{
    struct _ged_select_info *gd = (struct _ged_select_info *)bs;
    const char *usage_string = "select [options] clear [set_name_pattern]";
    const char *purpose_string = "clear contents of specified sets (or default set if none specified)";
    if (_select_cmd_msgs(bs, argc, argv, usage_string, purpose_string)) {
        return BRLCAD_OK;
    }

    argc--; argv++;

    struct ged *gedp = gd->gedp;

    if (!gedp->dbi_state)
	return BRLCAD_ERROR;

    const char *sname = NULL;
    if (argc) {
	sname = argv[0];
    } else if (bu_vls_strlen(&gd->curr_set)) {
	sname = bu_vls_cstr(&gd->curr_set);
    }

    std::vector<BSelectState *> ss = gedp->dbi_state->get_selected_states(sname);
    if (!ss.size()) {
	if (sname)
	    bu_vls_printf(gedp->ged_result_str, ": %s does not match any selection sets\n", sname);
	return BRLCAD_ERROR;
    }

    for (size_t i = 0; i < ss.size(); i++) {
	ss[i]->clear();
    }

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

    if (!gedp->dbi_state)
	return BRLCAD_ERROR;

    const char *sname = NULL;
    if (bu_vls_strlen(&gd->curr_set))
	sname = bu_vls_cstr(&gd->curr_set);

    std::vector<BSelectState *> ss = gedp->dbi_state->get_selected_states(sname);
    if (ss.size() != 1) {
	if (sname)
	    bu_vls_printf(gedp->ged_result_str, ": %s does not match one selection set\n", sname);
	return BRLCAD_ERROR;
    }

    struct bu_vls dpath = BU_VLS_INIT_ZERO;
    for (int i = 0; i < argc; i++) {
	bu_vls_sprintf(&dpath, "%s", argv[i]);
	if (bu_vls_cstr(&dpath)[0] != '/')
	    bu_vls_prepend(&dpath, "/");
	if (!ss[0]->select_path(bu_vls_cstr(&dpath), false)) {
	    bu_vls_printf(gedp->ged_result_str, "Selection set %s: unable to add path: %s", (sname) ? sname : "default", argv[i]);
	    bu_vls_free(&dpath);
	    return BRLCAD_ERROR;
	}
    }

    ss[0]->characterize();

    bu_vls_free(&dpath);

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

    if (!gedp->dbi_state)
	return BRLCAD_ERROR;

    const char *sname = NULL;
    if (bu_vls_strlen(&gd->curr_set))
	sname = bu_vls_cstr(&gd->curr_set);

    std::vector<BSelectState *> ss = gedp->dbi_state->get_selected_states(sname);
    if (ss.size() != 1) {
	if (sname)
	    bu_vls_printf(gedp->ged_result_str, ": %s does not match one selection set\n", sname);
	return BRLCAD_ERROR;
    }

    struct bu_vls dpath = BU_VLS_INIT_ZERO;
    for (int i = 0; i < argc; i++) {
	bu_vls_sprintf(&dpath, "%s", argv[i]);
	if (bu_vls_cstr(&dpath)[0] != '/')
	    bu_vls_prepend(&dpath, "/");
	if (!ss[0]->deselect_path(bu_vls_cstr(&dpath), false)) {
	    bu_vls_printf(gedp->ged_result_str, "Selection set %s: unable to remove path: %s", (sname) ? sname : "default", argv[i]);
	    bu_vls_free(&dpath);
	    return BRLCAD_ERROR;
	}
    }
    bu_vls_free(&dpath);

    ss[0]->characterize();

    return BRLCAD_OK;
}

int
_select_cmd_collapse(void *bs, int argc, const char **argv)
{
    struct _ged_select_info *gd = (struct _ged_select_info *)bs;
    const char *usage_string = "select [options] collapse [set_name_pattern]";
    const char *purpose_string = "collapse specified set(s)";
    if (_select_cmd_msgs(bs, argc, argv, usage_string, purpose_string)) {
        return BRLCAD_OK;
    }

    argc--; argv++;

    struct ged *gedp = gd->gedp;
    if (!gedp->dbi_state)
	return BRLCAD_ERROR;

    const char *sname = NULL;
    if (argc) {
	sname = argv[0];
    } else if (bu_vls_strlen(&gd->curr_set)) {
	sname = bu_vls_cstr(&gd->curr_set);
    }

    std::vector<BSelectState *> ss = gedp->dbi_state->get_selected_states(sname);
    if (!ss.size()) {
	if (sname)
	    bu_vls_printf(gedp->ged_result_str, ": %s does not match any selection sets\n", sname);
	return BRLCAD_ERROR;
    }

    for (size_t i = 0; i < ss.size(); i++) {
	ss[i]->collapse();
    }

    // TODO - for now, printing results - maybe tie this to a verbose option at some point... 
    for (size_t i = 0; i < ss.size(); i++) {
	std::vector<std::string> paths = ss[i]->list_selected_paths();

	for (size_t j = 0; j < paths.size(); j++) {
	    bu_vls_printf(gedp->ged_result_str, "%s\n", paths[j].c_str());
	}
    }

    return BRLCAD_OK;
}

int
_select_cmd_expand(void *bs, int argc, const char **argv)
{
    struct _ged_select_info *gd = (struct _ged_select_info *)bs;
    const char *usage_string = "select [options] expand [set_name_pattern]";
    const char *purpose_string = "expand specified set(s)";
    if (_select_cmd_msgs(bs, argc, argv, usage_string, purpose_string)) {
        return BRLCAD_OK;
    }

    argc--; argv++;

    struct ged *gedp = gd->gedp;

    if (!gedp->dbi_state)
	return BRLCAD_ERROR;

    const char *sname = NULL;
    if (argc) {
	sname = argv[0];
    } else if (bu_vls_strlen(&gd->curr_set)) {
	sname = bu_vls_cstr(&gd->curr_set);
    }

    std::vector<BSelectState *> ss = gedp->dbi_state->get_selected_states(sname);
    if (!ss.size()) {
	if (sname)
	    bu_vls_printf(gedp->ged_result_str, ": %s does not match any selection sets\n", sname);
	return BRLCAD_ERROR;
    }

    for (size_t i = 0; i < ss.size(); i++) {
	ss[i]->expand();
    }

    // TODO - for now, printing results - maybe tie this to a verbose option at some point...
    for (size_t i = 0; i < ss.size(); i++) {
	std::vector<std::string> paths = ss[i]->list_selected_paths();

	for (size_t j = 0; j < paths.size(); j++) {
	    bu_vls_printf(gedp->ged_result_str, "%s\n", paths[j].c_str());
	}
    }

    return BRLCAD_OK;
}

const struct bu_cmdtab _select_cmds[] = {
    { "add",        _select_cmd_add},
    { "clear",      _select_cmd_clear},
    { "collapse",   _select_cmd_collapse},
    { "expand",     _select_cmd_expand},
    { "list",       _select_cmd_list},
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
