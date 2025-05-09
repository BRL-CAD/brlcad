/*                       G E D _ U T I L . C
 * BRL-CAD
 *
 * Copyright (c) 2000-2025 United States Government as represented by
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
/** @addtogroup libged */
/** @{ */
/** @file libged/ged_util.c
 *
 * Utility routines for common operations in libged.
 *
 */
/** @} */

#include "common.h"

#include <stdlib.h>
#include <string.h>

#include <vector>
#include <set>
#include <string>

#ifdef HAVE_SYS_TYPES_H
#  include <sys/types.h>
#endif

#include "bio.h"
#include "bresource.h"


#include "bu/app.h"
#include "bu/cmd.h"
#include "bu/env.h"
#include "bu/file.h"
#include "bu/opt.h"
#include "bu/path.h"
#include "bu/sort.h"
#include "bu/str.h"
#include "bu/units.h"
#include "bu/vls.h"
#include "bv.h"

#include "ged.h"
#include "./ged_private.h"

int
_ged_subcmd_help(struct ged *gedp, struct bu_opt_desc *gopts, const struct bu_cmdtab *cmds, const char *cmdname, const char *cmdargs, void *gd, int argc, const char **argv)
{
    if (!gedp || !gopts || !cmds || !cmdname)
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
	const struct bu_cmdtab *ctp = NULL;
	int ret;
	const char *helpflag[2];
	helpflag[1] = PURPOSEFLAG;
	size_t maxcmdlen = 0;

	// It's possible for a command table to associate multiple strings with
	// the same function, for compatibility or convenience.  In those
	// instances, rather than repeat the same line, we instead group all
	// strings leading to the same subcommand together.
	std::vector<const struct bu_cmdtab *> uniq_cmds;
	std::map<int (*)(void *, int, const char *[]), std::set<std::string>> cmd_strings;
	std::map<int (*)(void *, int, const char *[]), std::string> cmd_labels;
	for (ctp = cmds; ctp->ct_name != (char *)NULL; ctp++) {
	    cmd_strings[ctp->ct_func].insert(std::string(ctp->ct_name));
	}
	std::map<int (*)(void *, int, const char *[]), std::set<std::string>>::iterator c_it;
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
	std::map<int (*)(void *, int, const char *[]), std::string>::iterator l_it;
	for (l_it = cmd_labels.begin(); l_it != cmd_labels.end(); l_it++)
	    maxcmdlen = (maxcmdlen > l_it->second.length()) ? maxcmdlen : l_it->second.length();

	std::set<int (*)(void *, int, const char *[])> processed_funcs;

	for (ctp = cmds; ctp->ct_name != (char *)NULL; ctp++) {
	    if (processed_funcs.find(ctp->ct_func) != processed_funcs.end())
		continue;
	    processed_funcs.insert(ctp->ct_func);
	    std::string &lbl = cmd_labels[ctp->ct_func];
	    bu_vls_printf(gedp->ged_result_str, "  %s%*s", lbl.c_str(), (int)(maxcmdlen - lbl.length()) +   2, " ");
	    if (!BU_STR_EQUAL(ctp->ct_name, "help")) {
		helpflag[0] = ctp->ct_name;
		bu_cmd(cmds, 2, helpflag, 0, gd, &ret);
	    } else {
		bu_vls_printf(gedp->ged_result_str, "print help and exit\n");
	    }
	}
    } else {
	int ret;
	const char **helpargv = (const char **)bu_calloc((size_t)argc+1, sizeof(char *), "help argv");
	helpargv[0] = argv[0];
	helpargv[1] = HELPFLAG;
	for (int i = 1; i < argc; i++) {
	    helpargv[i+1] = argv[i];
	}
	bu_cmd(cmds, argc+1, helpargv, 0, gd, &ret);
	bu_free((void *)helpargv, "help argv");
	return ret;
    }

    return BRLCAD_OK;
}

int
_ged_subcmd_exec(struct ged *gedp, struct bu_opt_desc *gopts, const struct bu_cmdtab *cmds, const char *cmdname, const char *cmdargs, void *gd, int argc, const char **argv, int help, int cmd_pos)
{
    if (!gedp || !gopts || !cmds || !cmdname)
	return BRLCAD_ERROR;

    if (help) {
	if (cmd_pos >= 0) {
	    argc = argc - cmd_pos;
	    argv = &argv[cmd_pos];
	    _ged_subcmd_help(gedp, gopts, cmds, cmdname, cmdargs, gd, argc, argv);
	} else {
	    _ged_subcmd_help(gedp, gopts, cmds, cmdname, cmdargs, gd, 0, NULL);
	}
	return BRLCAD_OK;
    }

    // Must have a subcommand
    if (cmd_pos == -1) {
	bu_vls_printf(gedp->ged_result_str, ": no valid subcommand specified\n");
	_ged_subcmd_help(gedp, gopts, cmds, cmdname, cmdargs, gd, 0, NULL);
	return BRLCAD_ERROR;
    }

    int ret;
    if (bu_cmd(cmds, argc, argv, 0, (void *)gd, &ret) == BRLCAD_OK) {
	return ret;
    } else {
	bu_vls_printf(gedp->ged_result_str, "subcommand %s not defined", argv[0]);
    }


    return BRLCAD_OK;
}

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

/* NOTE - caller must initialize vmin and vmax to INFINITY and -INFINITY
 * respectively (we don't do it here so callers may run this routine
 * repeatedly over different tables to accumulate bounds. */
static int
scene_bounding_sph(struct bu_ptbl *so, vect_t *vmin, vect_t *vmax, int pflag)
{
    struct bv_scene_obj *sp;
    vect_t minus, plus;
    int is_empty = 1;

    /* calculate the bounding for of all solids being displayed */
    for (size_t i = 0; i < BU_PTBL_LEN(so); i++) {
	struct bv_scene_group *g = (struct bv_scene_group *)BU_PTBL_GET(so, i);
	if (BU_PTBL_LEN(&g->children)) {
	    for (size_t j = 0; j < BU_PTBL_LEN(&g->children); j++) {
		sp = (struct bv_scene_obj *)BU_PTBL_GET(&g->children, j);
		minus[X] = sp->s_center[X] - sp->s_size;
		minus[Y] = sp->s_center[Y] - sp->s_size;
		minus[Z] = sp->s_center[Z] - sp->s_size;
		VMIN((*vmin), minus);
		plus[X] = sp->s_center[X] + sp->s_size;
		plus[Y] = sp->s_center[Y] + sp->s_size;
		plus[Z] = sp->s_center[Z] + sp->s_size;
		VMAX((*vmax), plus);

		is_empty = 0;
	    }
	} else {
	    // If we're an evaluated object, the group itself has the
	    // necessary info.
	    minus[X] = g->s_center[X] - g->s_size;
	    minus[Y] = g->s_center[Y] - g->s_size;
	    minus[Z] = g->s_center[Z] - g->s_size;
	    VMIN((*vmin), minus);
	    plus[X] = g->s_center[X] + g->s_size;
	    plus[Y] = g->s_center[Y] + g->s_size;
	    plus[Z] = g->s_center[Z] + g->s_size;
	    VMAX((*vmax), plus);
	}
    }
    if (!pflag) {
	bu_log("todo - handle pflag\n");
    }

    return is_empty;
}


int
_ged_results_init(struct ged_results *results)
{
    if (UNLIKELY(!results))
	return BRLCAD_ERROR;
    BU_ALLOC(results->results_tbl, struct bu_ptbl);
    BU_PTBL_INIT(results->results_tbl);
    return BRLCAD_OK;
}


int
_ged_results_add(struct ged_results *results, const char *result_string)
{
    /* If there isn't a string, we can live with that */
    if (UNLIKELY(!result_string))
	return BRLCAD_OK;

    /* If we have nowhere to insert into and we *do* have a string, trouble */
    if (UNLIKELY(!results))
	return BRLCAD_ERROR;
    if (UNLIKELY(!(results->results_tbl)))
	return BRLCAD_ERROR;
    if (UNLIKELY(!(BU_PTBL_IS_INITIALIZED(results->results_tbl))))
	return BRLCAD_ERROR;

    /* We're good to go - copy the string and stuff it in. */
    bu_ptbl_ins(results->results_tbl, (long *)bu_strdup(result_string));

    return BRLCAD_OK;
}

size_t
ged_results_count(struct ged_results *results)
{
    if (UNLIKELY(!results)) return 0;
    if (UNLIKELY(!(results->results_tbl))) return 0;
    return (size_t)BU_PTBL_LEN(results->results_tbl);
}

const char *
ged_results_get(struct ged_results *results, size_t index)
{
    return (const char *)BU_PTBL_GET(results->results_tbl, index);
}

void
ged_results_clear(struct ged_results *results)
{
    int i = 0;
    if (UNLIKELY(!results)) return;
    if (UNLIKELY(!(results->results_tbl))) return;

    /* we clean up everything except the ged_results structure itself */
    for (i = (int)BU_PTBL_LEN(results->results_tbl) - 1; i >= 0; i--) {
	char *rstring = (char *)BU_PTBL_GET(results->results_tbl, i);
	if (rstring)
	    bu_free(rstring, "free results string");
    }
    bu_ptbl_reset(results->results_tbl);
}

void
ged_results_free(struct ged_results *results) {
    if (UNLIKELY(!results)) return;
    if (UNLIKELY(!(results->results_tbl))) return;

    ged_results_clear(results);
    bu_ptbl_free(results->results_tbl);
    bu_free(results->results_tbl, "done with results ptbl");
}

/*********************************************************/
/*           comparison functions for bu_sort            */
/*********************************************************/

/**
 * Given two pointers to pointers to directory entries, do a string
 * compare on the respective names and return that value.
 */
int
cmpdirname(const void *a, const void *b, void *UNUSED(arg))
{
    struct directory **dp1, **dp2;

    dp1 = (struct directory **)a;
    dp2 = (struct directory **)b;
    return bu_strcmp((*dp1)->d_namep, (*dp2)->d_namep);
}

/**
 * Given two pointers to pointers to directory entries, compare
 * the dp->d_len sizes.
 */
int
cmpdlen(const void *a, const void *b, void *UNUSED(arg))
{
    int cmp = 0;
    struct directory **dp1, **dp2;

    dp1 = (struct directory **)a;
    dp2 = (struct directory **)b;
    if ((*dp1)->d_len > (*dp2)->d_len) cmp = 1;
    if ((*dp1)->d_len < (*dp2)->d_len) cmp = -1;
    return cmp;
}

/*********************************************************/
/*                _ged_vls_col_pr4v                      */
/*********************************************************/


void
_ged_vls_col_pr4v(struct bu_vls *vls,
		  struct directory **list_of_names,
		  size_t num_in_list,
		  int no_decorate,
		  int ssflag)
{
    size_t lines, i, j, k, this_one;
    size_t namelen;
    size_t maxnamelen;	/* longest name in list */
    size_t cwidth;	/* column width */
    size_t numcol;	/* number of columns */

    if (!ssflag) {
	bu_sort((void *)list_of_names,
		(unsigned)num_in_list, (unsigned)sizeof(struct directory *),
		cmpdirname, NULL);
    } else {
	bu_sort((void *)list_of_names,
		(unsigned)num_in_list, (unsigned)sizeof(struct directory *),
		cmpdlen, NULL);
    }

    /*
     * Traverse the list of names, find the longest name and set the
     * the column width and number of columns accordingly.  If the
     * longest name is greater than 80 characters, the number of
     * columns will be one.
     */
    maxnamelen = 0;
    for (k = 0; k < num_in_list; k++) {
	namelen = strlen(list_of_names[k]->d_namep);
	if (namelen > maxnamelen)
	    maxnamelen = namelen;
    }

    if (maxnamelen <= 16)
	maxnamelen = 16;
    cwidth = maxnamelen + 4;

    if (cwidth > 80)
	cwidth = 80;
    numcol = _GED_TERMINAL_WIDTH / cwidth;

    /*
     * For the number of (full and partial) lines that will be needed,
     * print in vertical format.
     */
    lines = (num_in_list + (numcol - 1)) / numcol;
    for (i = 0; i < lines; i++) {
	for (j = 0; j < numcol; j++) {
	    this_one = j * lines + i;
	    bu_vls_printf(vls, "%s", list_of_names[this_one]->d_namep);
	    namelen = strlen(list_of_names[this_one]->d_namep);

	    /*
	     * Region and ident checks here....  Since the code has
	     * been modified to push and sort on pointers, the
	     * printing of the region and ident flags must be delayed
	     * until now.  There is no way to make the decision on
	     * where to place them before now.
	     */
	    if (!no_decorate && list_of_names[this_one]->d_flags & RT_DIR_COMB) {
		bu_vls_putc(vls, '/');
		namelen++;
	    }

	    if (!no_decorate && list_of_names[this_one]->d_flags & RT_DIR_REGION) {
		bu_vls_putc(vls, 'R');
		namelen++;
	    }

	    /*
	     * Size check (partial lines), and line termination.  Note
	     * that this will catch the end of the lines that are full
	     * too.
	     */
	    if (this_one + lines >= num_in_list) {
		bu_vls_putc(vls, '\n');
		break;
	    } else {
		/*
		 * Pad to next boundary as there will be another entry
		 * to the right of this one.
		 */
		while (namelen++ < cwidth)
		    bu_vls_putc(vls, ' ');
	    }
	}
    }
}

/*********************************************************/

void
_ged_cmd_help(struct ged *gedp, const char *usage, struct bu_opt_desc *d)
{
    struct bu_vls str = BU_VLS_INIT_ZERO;
    char *option_help;

    bu_vls_sprintf(&str, "%s", usage);

    if ((option_help = bu_opt_describe(d, NULL))) {
	bu_vls_printf(&str, "Options:\n%s\n", option_help);
	bu_free(option_help, "help str");
    }

    bu_vls_vlscat(gedp->ged_result_str, &str);
    bu_vls_free(&str);
}

// TODO - replace with bu_opt_incr_long
int
_ged_vopt(struct bu_vls *UNUSED(msg), size_t UNUSED(argc), const char **UNUSED(argv), void *set_var)
{
    int *v_set = (int *)set_var;
    if (v_set) {
	(*v_set) = (*v_set) + 1;
    }
    return 0;
}

/* Sort the argv array to list existing objects first and everything else at
 * the end.  Returns the number of argv entries where db_lookup failed */
int
_ged_sort_existing_objs(struct db_i *dbip, int argc, const char *argv[], struct directory **dpa)
{
    int i = 0;
    int exist_cnt = 0;
    int nonexist_cnt = 0;
    struct directory *dp;
    const char **exists = (const char **)bu_calloc(argc, sizeof(const char *), "obj exists array");
    const char **nonexists = (const char **)bu_calloc(argc, sizeof(const char *), "obj nonexists array");
    if (dbip == DBI_NULL)
	return BRLCAD_ERROR;
    for (i = 0; i < argc; i++) {
	dp = db_lookup(dbip, argv[i], LOOKUP_QUIET);
	if (dp == RT_DIR_NULL) {
	    nonexists[nonexist_cnt] = argv[i];
	    nonexist_cnt++;
	} else {
	    exists[exist_cnt] = argv[i];
	    if (dpa) dpa[exist_cnt] = dp;
	    exist_cnt++;
	}
    }
    for (i = 0; i < exist_cnt; i++) {
	argv[i] = exists[i];
    }
    for (i = 0; i < nonexist_cnt; i++) {
	argv[i + exist_cnt] = nonexists[i];
    }

    bu_free((void *)exists, "exists array");
    bu_free((void *)nonexists, "nonexists array");

    return nonexist_cnt;
}



/**
 * Returns
 * 0 on success
 * !0 on failure
 */
static int
_ged_densities_from_file(struct analyze_densities **dens, char **den_src, struct ged *gedp, const char *name, int fault_tolerant)
{
    struct analyze_densities *densities;
    struct bu_mapped_file *dfile = NULL;
    struct bu_vls msgs = BU_VLS_INIT_ZERO;
    char *buf = NULL;
    int ret = 0;
    int ecnt = 0;

    if (gedp == GED_NULL || gedp->dbip == NULL || !dens) {
	return BRLCAD_ERROR;
    }

    if (!bu_file_exists(name, NULL)) {
	bu_vls_printf(gedp->ged_result_str, "Could not find density file - %s\n", name);
	return BRLCAD_ERROR;
    }

    dfile = bu_open_mapped_file(name, "densities file");
    if (!dfile) {
	bu_vls_printf(gedp->ged_result_str, "Could not open density file - %s\n", name);
	return BRLCAD_ERROR;
    }

    buf = (char *)(dfile->buf);

    (void)analyze_densities_create(&densities);

    ret = analyze_densities_load(densities, buf, &msgs, &ecnt);

    if (!fault_tolerant && ecnt > 0) {
	bu_vls_printf(gedp->ged_result_str, "Problem reading densities file %s:\n%s\n", name, bu_vls_cstr(&msgs));
	if (densities) {
	    analyze_densities_destroy(densities);
	}
	bu_vls_free(&msgs);
	bu_close_mapped_file(dfile);
	return BRLCAD_ERROR;
    }

    bu_vls_free(&msgs);
    bu_close_mapped_file(dfile);

    (*dens) = densities;
    if (ret > 0) {
	if (den_src) {
	    (*den_src) = bu_strdup(name);
	}
    } else {
	if (ret == 0 && densities) {
	    analyze_densities_destroy(densities);
	}
    }

    return (ret == 0) ? BRLCAD_ERROR : BRLCAD_OK;
}

/**
 * Load density information.
 *
 * Returns
 * 0 on success
 * !0 on failure
 */
int
_ged_read_densities(struct analyze_densities **dens, char **den_src, struct ged *gedp, const char *filename, int fault_tolerant)
{
    struct bu_vls d_path_dir = BU_VLS_INIT_ZERO;

    if (gedp == GED_NULL || gedp->dbip == DBI_NULL || !dens) {
	return BRLCAD_ERROR;
    }

    /* If we've explicitly been given a file, read that */
    if (filename) {
	return _ged_densities_from_file(dens, den_src, gedp, filename, fault_tolerant);
    }

    /* If we don't have an explicitly specified file, see if we have definitions in
     * the database itself. */
    if (gedp->dbip != DBI_NULL) {
	int ret = 0;
	int ecnt = 0;
	struct bu_vls msgs = BU_VLS_INIT_ZERO;
	struct directory *dp;
	struct rt_db_internal intern;
	struct rt_binunif_internal *bip;
	struct analyze_densities *densities;
	char *buf;

	dp = db_lookup(gedp->dbip, GED_DB_DENSITY_OBJECT, LOOKUP_QUIET);

	if (dp != (struct directory *)NULL) {

	    if (rt_db_get_internal(&intern, dp, gedp->dbip, NULL, &rt_uniresource) < 0) {
		bu_vls_printf(gedp->ged_result_str, "could not import %s\n", dp->d_namep);
		return BRLCAD_ERROR;
	    }

	    if ((intern.idb_major_type & DB5_MAJORTYPE_BINARY_MASK) == 0)
		return BRLCAD_ERROR;

	    bip = (struct rt_binunif_internal *)intern.idb_ptr;

	    RT_CHECK_BINUNIF (bip);

	    (void)analyze_densities_create(&densities);

	    buf = (char *)bu_calloc(bip->count+1, sizeof(char), "density buffer");
	    memcpy(buf, bip->u.int8, bip->count);
	    rt_db_free_internal(&intern);

	    ret = analyze_densities_load(densities, buf, &msgs, &ecnt);

	    bu_free((void *)buf, "density buffer");

	    if (!fault_tolerant && ecnt > 0) {
		bu_vls_printf(gedp->ged_result_str, "Problem reading densities from .g file:\n%s\n", bu_vls_cstr(&msgs));
		bu_vls_free(&msgs);
		if (densities) {
		    analyze_densities_destroy(densities);
		}
		return BRLCAD_ERROR;
	    }

	    bu_vls_free(&msgs);

	    (*dens) = densities;
	    if (ret > 0) {
		if (den_src) {
		   (*den_src) = bu_strdup(gedp->dbip->dbi_filename);
		}
	    } else {
		if (ret == 0 && densities) {
		    analyze_densities_destroy(densities);
		}
	    }
	    return (ret == 0) ? BRLCAD_ERROR : BRLCAD_OK;
	}
    }

    /* If we don't have an explicitly specified file and the database doesn't have any information, see if we
     * have .density files in either the current database directory or HOME. */

    /* Try .g path first */
    if (bu_path_component(&d_path_dir, gedp->dbip->dbi_filename, BU_PATH_DIRNAME)) {

	bu_vls_printf(&d_path_dir, "/.density");

	if (bu_file_exists(bu_vls_cstr(&d_path_dir), NULL)) {
	    int ret = _ged_densities_from_file(dens, den_src, gedp, bu_vls_cstr(&d_path_dir), fault_tolerant);
	    bu_vls_free(&d_path_dir);
	    return ret;
	}
    }

    /* Try HOME */
    if (bu_dir(NULL, 0, BU_DIR_HOME, NULL)) {

	bu_vls_sprintf(&d_path_dir, "%s/.density", bu_dir(NULL, 0, BU_DIR_HOME, NULL));

	if (bu_file_exists(bu_vls_cstr(&d_path_dir), NULL)) {
	    int ret = _ged_densities_from_file(dens, den_src, gedp, bu_vls_cstr(&d_path_dir), fault_tolerant);
	    bu_vls_free(&d_path_dir);
	    return ret;
	}
    }

    return BRLCAD_ERROR;
}

int
ged_dbcopy(struct ged *from_gedp, struct ged *to_gedp, const char *from, const char *to, int fflag)
{
    struct directory *from_dp;
    struct bu_external external;

    GED_CHECK_DATABASE_OPEN(from_gedp, BRLCAD_ERROR);
    GED_CHECK_DATABASE_OPEN(to_gedp, BRLCAD_ERROR);
    GED_CHECK_READ_ONLY(to_gedp, BRLCAD_ERROR);

    /* initialize result */
    bu_vls_trunc(from_gedp->ged_result_str, 0);
    bu_vls_trunc(to_gedp->ged_result_str, 0);

    GED_DB_LOOKUP(from_gedp, from_dp, from, LOOKUP_NOISY, BRLCAD_ERROR & GED_QUIET);

    if (!fflag && db_lookup(to_gedp->dbip, to, LOOKUP_QUIET) != RT_DIR_NULL) {
	bu_vls_printf(from_gedp->ged_result_str, "%s already exists.", to);
	return BRLCAD_ERROR;
    }

    if (db_get_external(&external, from_dp, from_gedp->dbip)) {
	bu_vls_printf(from_gedp->ged_result_str, "Database read error, aborting\n");
	return BRLCAD_ERROR;
    }

    struct rt_wdb *wdbp = wdb_dbopen(to_gedp->dbip, RT_WDB_TYPE_DB_DEFAULT);
    if (wdb_export_external(wdbp, &external, to,
			    from_dp->d_flags,  from_dp->d_minor_type) < 0) {
	bu_free_external(&external);
	bu_vls_printf(from_gedp->ged_result_str,
		      "Failed to write new object (%s) to database - aborting!!\n",
		      to);
	return BRLCAD_ERROR;
    }

    bu_free_external(&external);

    /* Need to do something extra for _GLOBAL */
    if (db_version(to_gedp->dbip) > 4 && BU_STR_EQUAL(to, DB5_GLOBAL_OBJECT_NAME)) {
	struct directory *to_dp;
	struct bu_attribute_value_set avs;
	const char *val;

	GED_DB_LOOKUP(to_gedp, to_dp, to, LOOKUP_NOISY, BRLCAD_ERROR & GED_QUIET);

	bu_avs_init_empty(&avs);
	if (db5_get_attributes(to_gedp->dbip, &avs, to_dp)) {
	    bu_vls_printf(from_gedp->ged_result_str, "Cannot get attributes for object %s\n", to_dp->d_namep);
	    return BRLCAD_ERROR;
	}

	if ((val = bu_avs_get(&avs, "title")) != NULL)
	    to_gedp->dbip->dbi_title = bu_strdup(val);

	if ((val = bu_avs_get(&avs, "units")) != NULL) {
	    double loc2mm;

	    if ((loc2mm = bu_mm_value(val)) > 0) {
		to_gedp->dbip->dbi_local2base = loc2mm;
		to_gedp->dbip->dbi_base2local = 1.0 / loc2mm;
	    }
	}

	if ((val = bu_avs_get(&avs, "regionid_colortable")) != NULL) {
	    rt_color_free();
	    db5_import_color_table((char *)val);
	}

	bu_avs_free(&avs);
    }

    return BRLCAD_OK;
}


int
ged_rot_args(struct ged *gedp, int argc, const char *argv[], char *coord, mat_t rmat)
{
    vect_t rvec;
    static const char *usage = "[-m|-v] x y z";

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_VIEW(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    /* process possible coord flag */
    if (argv[1][0] == '-' && (argv[1][1] == 'v' || argv[1][1] == 'm') && argv[1][2] == '\0') {
	*coord = argv[1][1];
	--argc;
	++argv;
    } else
	*coord = gedp->ged_gvp->gv_coord;

    if (argc != 2 && argc != 4) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    if (argc == 2) {
	if (bn_decode_vect(rvec, argv[1]) != 3) {
	    bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	    return BRLCAD_ERROR;
	}
    } else {
	double scan[3];

	if (sscanf(argv[1], "%lf", &scan[X]) < 1) {
	    bu_vls_printf(gedp->ged_result_str, "ged_eye: bad X value %s\n", argv[1]);
	    return BRLCAD_ERROR;
	}

	if (sscanf(argv[2], "%lf", &scan[Y]) < 1) {
	    bu_vls_printf(gedp->ged_result_str, "ged_eye: bad Y value %s\n", argv[2]);
	    return BRLCAD_ERROR;
	}

	if (sscanf(argv[3], "%lf", &scan[Z]) < 1) {
	    bu_vls_printf(gedp->ged_result_str, "ged_eye: bad Z value %s\n", argv[3]);
	    return BRLCAD_ERROR;
	}

	/* convert from double to fastf_t */
	VMOVE(rvec, scan);
    }

    VSCALE(rvec, rvec, -1.0);
    bn_mat_angles(rmat, rvec[X], rvec[Y], rvec[Z]);

    return BRLCAD_OK;
}

int
ged_arot_args(struct ged *gedp, int argc, const char *argv[], mat_t rmat)
{
    point_t pt = VINIT_ZERO;
    vect_t axisv;
    double axis[3]; /* not fastf_t due to sscanf */
    double angle; /* not fastf_t due to sscanf */
    static const char *usage = "x y z angle";

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_VIEW(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    if (argc != 5) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    if (sscanf(argv[1], "%lf", &axis[X]) != 1) {
	bu_vls_printf(gedp->ged_result_str, "%s: bad X value - %s\n", argv[0], argv[1]);
	return BRLCAD_ERROR;
    }

    if (sscanf(argv[2], "%lf", &axis[Y]) != 1) {
	bu_vls_printf(gedp->ged_result_str, "%s: bad Y value - %s\n", argv[0], argv[2]);
	return BRLCAD_ERROR;
    }

    if (sscanf(argv[3], "%lf", &axis[Z]) != 1) {
	bu_vls_printf(gedp->ged_result_str, "%s: bad Z value - %s\n", argv[0], argv[3]);
	return BRLCAD_ERROR;
    }

    if (sscanf(argv[4], "%lf", &angle) != 1) {
	bu_vls_printf(gedp->ged_result_str, "%s: bad angle - %s\n", argv[0], argv[4]);
	return BRLCAD_ERROR;
    }

    VUNITIZE(axis);
    VMOVE(axisv, axis);
    bn_mat_arb_rot(rmat, pt, axisv, angle*DEG2RAD);

    return BRLCAD_OK;
}

int
ged_tra_args(struct ged *gedp, int argc, const char *argv[], char *coord, vect_t tvec)
{
    static const char *usage = "[-m|-v] x y z";

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_VIEW(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    /* process possible coord flag */
    if (argv[1][0] == '-' && (argv[1][1] == 'v' || argv[1][1] == 'm') && argv[1][2] == '\0') {
	*coord = argv[1][1];
	--argc;
	++argv;
    } else
	*coord = gedp->ged_gvp->gv_coord;

    if (argc != 2 && argc != 4) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    if (argc == 2) {
	if (bn_decode_vect(tvec, argv[1]) != 3) {
	    bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	    return BRLCAD_ERROR;
	}
    } else {
	double scan[3];

	if (sscanf(argv[1], "%lf", &scan[X]) != 1) {
	    bu_vls_printf(gedp->ged_result_str, "%s: bad X value %s\n", argv[0], argv[1]);
	    return BRLCAD_ERROR;
	}

	if (sscanf(argv[2], "%lf", &scan[Y]) != 1) {
	    bu_vls_printf(gedp->ged_result_str, "%s: bad Y value %s\n", argv[0], argv[2]);
	    return BRLCAD_ERROR;
	}

	if (sscanf(argv[3], "%lf", &scan[Z]) != 1) {
	    bu_vls_printf(gedp->ged_result_str, "%s: bad Z value %s\n", argv[0], argv[3]);
	    return BRLCAD_ERROR;
	}

	/* convert from double to fastf_t */
	VMOVE(tvec, scan);
    }

    return BRLCAD_OK;
}

int
ged_scale_args(struct ged *gedp, int argc, const char *argv[], fastf_t *sf1, fastf_t *sf2, fastf_t *sf3)
{
    static const char *usage = "sf (or) sfx sfy sfz";
    int ret = BRLCAD_OK, args_read;
    double scan;

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_VIEW(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    if (!sf1 || !sf2 || !sf3) {
	bu_vls_printf(gedp->ged_result_str, "%s: invalid input state", argv[0]);
	return BRLCAD_ERROR;
    }

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    if (argc != 2 && argc != 4) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    if (argc == 2) {
	if (!sf1 || bu_sscanf(argv[1], "%lf", &scan) != 1) {
	    bu_vls_printf(gedp->ged_result_str, "\nbad scale factor '%s'", argv[1]);
	    return BRLCAD_ERROR;
	}
	*sf1 = scan;
    } else {
	args_read = bu_sscanf(argv[1], "%lf", &scan);
	if (!sf1 || args_read != 1) {
	    bu_vls_printf(gedp->ged_result_str, "\nbad x scale factor '%s'", argv[1]);
	    ret = BRLCAD_ERROR;
	}
	*sf1 = scan;

	args_read = bu_sscanf(argv[2], "%lf", &scan);
	if (!sf2 || args_read != 1) {
	    bu_vls_printf(gedp->ged_result_str, "\nbad y scale factor '%s'", argv[2]);
	    ret = BRLCAD_ERROR;
	}
	*sf2 = scan;

	args_read = bu_sscanf(argv[3], "%lf", &scan);
	if (!sf3 || args_read != 1) {
	    bu_vls_printf(gedp->ged_result_str, "\nbad z scale factor '%s'", argv[3]);
	    ret = BRLCAD_ERROR;
	}
	*sf3 = scan;
    }
    return ret;
}

size_t
ged_who_argc(struct ged *gedp)
{
    if (gedp->new_cmd_forms) {
	if (!gedp || !gedp->ged_gvp || !gedp->dbi_state)
	    return 0;
	BViewState *bvs = gedp->dbi_state->get_view_state(gedp->ged_gvp);
	if (bvs)
	    return bvs->count_drawn_paths(-1, true);
	return 0;
    }

    struct display_list *gdlp = NULL;
    size_t visibleCount = 0;

    if (!gedp || !gedp->i->ged_gdp || !gedp->i->ged_gdp->gd_headDisplay)
	return 0;

    for (BU_LIST_FOR(gdlp, display_list, gedp->i->ged_gdp->gd_headDisplay)) {
	visibleCount++;
    }

    return visibleCount;
}


/**
 * Build a command line vector of the tops of all objects in view.
 *
 * Returns the number of items displayed.
 *
 * FIXME: crazy inefficient for massive object lists.  needs to work
 * with preallocated memory.
 */
int
ged_who_argv(struct ged *gedp, char **start, const char **end)
{
    char **vp = start;
    if (!gedp)
	return 0;
    if (gedp->new_cmd_forms) {
	if (!gedp->ged_gvp || !gedp->dbi_state)
	    return 0;
	BViewState *bvs = gedp->dbi_state->get_view_state(gedp->ged_gvp);
	if (bvs) {
	    std::vector<std::string> drawn_paths = bvs->list_drawn_paths(-1, true);
	    for (size_t i = 0; i < drawn_paths.size(); i++) {
		if ((vp != NULL) && ((const char **)vp < end)) {
		    *vp++ = bu_strdup(drawn_paths[i].c_str());
		} else {
		    bu_vls_printf(gedp->ged_result_str, "INTERNAL ERROR: ged_who_argv() ran out of space at %s\n", drawn_paths[i].c_str());
		    break;
		}
	    }
	} else {
	    return 0;
	}
    }

    struct display_list *gdlp;

    if (!gedp || !gedp->i->ged_gdp || !gedp->i->ged_gdp->gd_headDisplay)
	return 0;

    if (UNLIKELY(!start || !end)) {
	bu_vls_printf(gedp->ged_result_str, "INTERNAL ERROR: ged_who_argv() called with NULL args\n");
	return 0;
    }

    for (BU_LIST_FOR(gdlp, display_list, gedp->i->ged_gdp->gd_headDisplay)) {
	if (((struct directory *)gdlp->dl_dp)->d_addr == RT_DIR_PHONY_ADDR)
	    continue;

	if ((vp != NULL) && ((const char **)vp < end)) {
	    *vp++ = bu_strdup(bu_vls_addr(&gdlp->dl_path));
	} else {
	    bu_vls_printf(gedp->ged_result_str, "INTERNAL ERROR: ged_who_argv() ran out of space at %s\n", ((struct directory *)gdlp->dl_dp)->d_namep);
	    break;
	}
    }

    if ((vp != NULL) && ((const char **)vp < end)) {
	*vp = (char *) 0;
    }

    return vp-start;
}

void
_ged_do_list(struct ged *gedp, struct directory *dp, int verbose)
{
    int id;
    struct rt_db_internal intern;

    RT_CK_DBI(gedp->dbip);

    if (dp->d_major_type == DB5_MAJORTYPE_ATTRIBUTE_ONLY) {
	/* this is the _GLOBAL object */
	struct bu_attribute_value_set avs;
	struct bu_attribute_value_pair *avp;

	bu_vls_strcat(gedp->ged_result_str, dp->d_namep);
	bu_vls_strcat(gedp->ged_result_str, ": global attributes object\n");
	bu_avs_init_empty(&avs);
	if (db5_get_attributes(gedp->dbip, &avs, dp)) {
	    bu_vls_printf(gedp->ged_result_str, "Cannot get attributes for %s\n", dp->d_namep);
	    return;
	}
/* !!! left off here*/
	for (BU_AVS_FOR(avp, &avs)) {
	    if (BU_STR_EQUAL(avp->name, "units")) {
		double conv;
		const char *str;

		conv = atof(avp->value);
		bu_vls_strcat(gedp->ged_result_str, "\tunits: ");

		str = bu_units_string(conv);
		if (str == NULL) {
		    bu_vls_strcat(gedp->ged_result_str, "Unrecognized units\n");
		} else {
		    bu_vls_strcat(gedp->ged_result_str, str);
		    bu_vls_putc(gedp->ged_result_str, '\n');
		}
	    } else {
		bu_vls_putc(gedp->ged_result_str, '\t');
		bu_vls_strcat(gedp->ged_result_str, avp->name);
		bu_vls_strcat(gedp->ged_result_str, ": ");
		bu_vls_strcat(gedp->ged_result_str, avp->value);
		bu_vls_putc(gedp->ged_result_str, '\n');
	    }
	}
    } else {

	if ((id = rt_db_get_internal(&intern, dp, gedp->dbip,
				     (fastf_t *)NULL, &rt_uniresource)) < 0) {
	    bu_vls_printf(gedp->ged_result_str, "rt_db_get_internal(%s) failure\n", dp->d_namep);
	    rt_db_free_internal(&intern);
	    return;
	}

	bu_vls_printf(gedp->ged_result_str, "%s:  ", dp->d_namep);

	if (!OBJ[id].ft_describe ||
	    OBJ[id].ft_describe(gedp->ged_result_str,
				&intern,
				verbose,
				gedp->dbip->dbi_base2local) < 0)
	    bu_vls_printf(gedp->ged_result_str, "%s: describe error\n", dp->d_namep);
	rt_db_free_internal(&intern);
    }
}

void
_ged_cvt_vlblock_to_solids(struct ged *gedp, struct bv_vlblock *vbp, const char *name, int copy)
{
    size_t i;
    char shortname[32] = {0};
    char namebuf[64] = {0};

    bu_strlcpy(shortname, name, sizeof(shortname));

    for (i = 0; i < vbp->nused; i++) {
	if (BU_LIST_IS_EMPTY(&(vbp->head[i])))
	    continue;
	snprintf(namebuf, 64, "%s%lx", shortname, vbp->rgb[i]);
	invent_solid(gedp, namebuf, &vbp->head[i], vbp->rgb[i], copy, 1.0, 0, 0);
    }
}

/* print a message to let the user know they need to quit their
     * editor before the application will come back to life.
     */

static void
editit_msg(const char *editor, int opts_cnt, const char **editor_opts, const char *file, const char *terminal)
{
    size_t length;
    struct bu_vls str = BU_VLS_INIT_ZERO;
    struct bu_vls sep = BU_VLS_INIT_ZERO;
    char *editor_basename;

    if (!editor)
	return;

    if (terminal && opts_cnt > 0) {
	bu_vls_sprintf(&str, "Invoking [%s ", editor);
	for (int i = 0; i < opts_cnt; i++)
	    bu_vls_printf(&str, "%s ", editor_opts[i]);
	bu_vls_printf(&str, "%s] via %s\n\n", file, terminal);
	bu_log("%s", bu_vls_cstr(&str));
    } else if (terminal) {
	bu_log("Invoking [%s %s] via %s\n\n", editor, file, terminal);
    } else if (opts_cnt > 0) {
	bu_vls_sprintf(&str, "Invoking [%s ", editor);
	for (int i = 0; i < opts_cnt; i++)
	    bu_vls_printf(&str, "%s ", editor_opts[i]);
	bu_vls_printf(&str, "%s]\n\n", file);
	bu_log("%s", bu_vls_cstr(&str));
    } else {
	bu_log("Invoking [%s %s]\n\n", editor, file);
    }
    editor_basename = bu_path_basename(editor, NULL);
    bu_vls_sprintf(&str, "\nNOTE: You must QUIT %s before %s will respond and continue.\n", editor_basename, bu_getprogname());
    for (length = bu_vls_strlen(&str) - 2; length > 0; length--) {
	bu_vls_putc(&sep, '*');
    }
    bu_log("%s%s%s\n\n", bu_vls_addr(&sep), bu_vls_addr(&str), bu_vls_addr(&sep));
    bu_vls_free(&str);
    bu_vls_free(&sep);
    bu_free(editor_basename, "editor_basename free");
}


/* Edit string breakdown has some limitations - eventually this should go away */
static void
editit_parse_editstring(char **editor, char **terminal, char **editor_opt, char **terminal_opt, const char *editstring)
{
    /* Convert the edit string into pieces suitable for arguments to execlp.
     * Because we are using bu_argv_from_string, there are some limitations on
     * what can successfully be passed through the various layers of logic
     * using this approach.  For more robust handling use the struct ged
     * entries to assign this information before calling _ged_editit. */
    char **avtmp = (char **)NULL;
    char *editstring_cpy = (char *)bu_calloc(2*strlen(editstring), sizeof(char), "editstring_cpy");
    bu_str_escape(editstring, "\\", editstring_cpy, 2*strlen(editstring));
    avtmp = (char **)bu_calloc(5, sizeof(char *), "ged_editit: editstring args");
    bu_argv_from_string(avtmp, 4, editstring_cpy);

    if (avtmp[0] && !BU_STR_EQUAL(avtmp[0], "(null)"))
	*terminal = bu_strdup(avtmp[0]);
    if (avtmp[1] && !BU_STR_EQUAL(avtmp[1], "(null)"))
	*terminal_opt = bu_strdup(avtmp[1]);
    if (avtmp[2] && !BU_STR_EQUAL(avtmp[2], "(null)"))
	*editor = bu_strdup(avtmp[2]);
    if (avtmp[3] && !BU_STR_EQUAL(avtmp[3], "(null)"))
	*editor_opt = bu_strdup(avtmp[3]);

    bu_free((void *)avtmp, "ged_editit: avtmp");
    bu_free(editstring_cpy, "editstring copy");
}

int
_ged_editit(struct ged *gedp, const char *editstring, const char *filename)
{
    int ret = 1;
    if (!gedp)
	return 0;

#ifdef HAVE_UNISTD_H
    int xpid = 0;
    int status = 0;
#endif
    int pid = 0;
    char *editor = (char *)NULL;
    char *editor_opt = (char *)NULL;
    char *terminal = (char *)NULL;
    char *terminal_opt = (char *)NULL;

    int eac = 0;
    char *eargv[MAXPATHLEN] = {NULL};
    int listexec = 0;

#if defined(SIGINT) && defined(SIGQUIT)
    void (*s2)(int);
    void (*s3)(int);
#endif

    if (!filename) {
	bu_log("INTERNAL ERROR: editit filename missing\n");
	return 0;
    }

    if (editstring && strlen(editstring)) {

	// Highest precedence - parse an edit string, if we have one
	editit_parse_editstring(&editor, &terminal, &editor_opt, &terminal_opt, editstring);

	// Produce the editing message
	editit_msg(editor, (editor_opt) ? 1 : 0, (const char **)&editor_opt, filename, terminal);

	// Tell exec logic we're in list mode
	listexec = 1;

    } else if (strlen(gedp->editor)) {

	/* Second highest precedence - assemble ged struct info into editit form, if defined */
	editor = bu_strdup(gedp->editor);

	// If we're going to use a terminal, stage that first - unlike editstring
	// mode, we'll be feeding the argv array directly to exec
	if (strlen(gedp->terminal)) {
	    terminal = bu_strdup(gedp->terminal);
	    eargv[0] = terminal;
	    eac = 1;
	    for (size_t i = 0; i < BU_PTBL_LEN(&gedp->terminal_opts); i++) {
		eargv[eac] = bu_strdup((const char *)BU_PTBL_GET(&gedp->terminal_opts, i));
		eac++;
	    }
	}

	// Terminal handled - add editor and any opts
	eargv[eac] = editor;
	eac++;
	for (size_t i = 0; i < BU_PTBL_LEN(&gedp->editor_opts); i++) {
	    eargv[eac] = bu_strdup((const char *)BU_PTBL_GET(&gedp->editor_opts, i));
	    eac++;
	}

	// Last comes the filename
	eargv[eac] = bu_strdup(filename);
	eac++;

	// Produce the editing message
	editit_msg(editor, BU_PTBL_LEN(&gedp->editor_opts), (const char **)gedp->editor_opts.buffer, filename, terminal);

    } else {

	// If neither an edit string nor gedp are telling us what to use,
	// try to find something
	struct bu_ptbl eo = BU_PTBL_INIT_ZERO;
	editor = bu_strdup(bu_editor(&eo, 0, 0, NULL));
	eargv[0] = editor;
	eac = 1;
	for (size_t i = 0; i < BU_PTBL_LEN(&eo); i++) {
	    eargv[eac] = bu_strdup((const char *)BU_PTBL_GET(&eo, i));
	    eac++;
	}

	// Append filename
	eargv[eac] = bu_strdup(filename);
	eac++;

	// Produce the editing message
	editit_msg(editor, (BU_PTBL_LEN(&eo)) ? 1 : 0, (const char **)eo.buffer, filename, terminal);
    }

    if (!editor) {
	bu_log("INTERNAL ERROR: editit editor missing\n");
	ret = 0;
	goto editit_cleanup;
    }

#if defined(SIGINT) && defined(SIGQUIT)
    s2 = signal(SIGINT, SIG_IGN);
    s3 = signal(SIGQUIT, SIG_IGN);
#endif

#ifdef HAVE_UNISTD_H
    if ((pid = fork()) < 0) {
	perror("fork");
	ret = 0;
	goto editit_cleanup;
    }
#endif

    if (pid == 0) {
	/* Don't call bu_log() here in the child! */

#if defined(SIGINT) && defined(SIGQUIT)
	/* deja vu */
	(void)signal(SIGINT, SIG_DFL);
	(void)signal(SIGQUIT, SIG_DFL);
#endif

	{
#if defined(_WIN32) && !defined(__CYGWIN__)
	    STARTUPINFO si = {0};
	    PROCESS_INFORMATION pi = {0};
	    si.cb = sizeof(STARTUPINFO);
	    si.lpReserved = NULL;
	    si.lpReserved2 = NULL;
	    si.cbReserved2 = 0;
	    si.lpDesktop = NULL;
	    si.dwFlags = 0;

	    // Windows doesn't have terminal launches, so we just append all the args
	    struct bu_vls buffer = BU_VLS_INIT_ZERO;
	    if (listexec) {
		bu_vls_printf(&buffer, "%s " , editor);
		if (editor_opt)
		    bu_vls_printf(&buffer, "%s " , editor_opt);
		bu_vls_printf(&buffer, "%s" , filename);
	    } else {
		for (int i = 0; i < eac - 1; i++)
		    bu_vls_printf(&buffer, "%s " , eargv[i]);
		bu_vls_printf(&buffer, "%s" , eargv[eac - 1]);
	    }

	    // Note - CreateProcess requires us to use bu_vls_addr here to get
	    // something usable as an LPSTR - const char * won't work.
	    if (!CreateProcess(NULL, bu_vls_addr(&buffer), NULL, NULL, TRUE, NORMAL_PRIORITY_CLASS, NULL, NULL, &si, &pi)) {
		DWORD ec = GetLastError();
		LPSTR mb = NULL;
		size_t mblen = FormatMessageA(
			FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
			NULL, ec, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&mb, 0, NULL);
		std::string mstr(mb, mblen);
		LocalFree(mb);
		fprintf(stderr, "CreateProcess failed with editor \"%s\" and file \"%s\": %s\n", eargv[0], filename, mstr.c_str());
	    }
	    WaitForSingleObject(pi.hProcess, INFINITE);
	    bu_vls_free(&buffer);
	    return 1;
#else

	    /* If we're using TextEdit close stdout/stderr so we don't get blather
	     * about service registration failure */
	    char *editor_basename = bu_path_basename(editor, NULL);
	    if (BU_STR_EQUAL(editor_basename, "TextEdit")) {
		close(fileno(stdout));
		close(fileno(stderr));
	    }
	    bu_free(editor_basename, "editor_basename free");

	    // Non-Windows platforms might be doing one of several things.  If we're
	    // listexec, then av isn't necessarily in execution order and we need to be
	    // careful about what goes where
	    if (listexec) {
		if (!terminal && !editor_opt) {
		    (void)execlp(editor, editor, filename, NULL);
		} else if (!terminal) {
		    (void)execlp(editor, editor, editor_opt, filename, NULL);
		} else if (terminal && !terminal_opt) {
		    (void)execlp(terminal, terminal, editor, filename, NULL);
		} else if (terminal && !editor_opt) {
		    (void)execlp(terminal, terminal, terminal_opt, editor, filename, NULL);
		} else {
		    (void)execlp(terminal, terminal, terminal_opt, editor, editor_opt, filename, NULL);
		}
	    } else {
		// Not doing listexec, so we can just go with the av array.
		(void)execvp(eargv[0], (char * const *)eargv);
	    }
#endif

	    /* should not reach */
	    perror(editor);
	    bu_exit(1, NULL);
	}
    }

#ifdef HAVE_UNISTD_H
    /* wait for the editor to terminate */
    while ((xpid = wait(&status)) >= 0) {
	if (xpid == pid) {
	    break;
	}
    }
#endif

#if defined(SIGINT) && defined(SIGQUIT)
    (void)signal(SIGINT, s2);
    (void)signal(SIGQUIT, s3);
#endif

editit_cleanup:
    for (int i = 0; i < eac; i++)
	bu_free(eargv[i], "eargv");

    if (listexec) {
	// listexec isn't using eargv
	bu_free(editor, "editor");
	bu_free(editor_opt, "editor_opt");
	bu_free(terminal, "terminal");
	bu_free(terminal_opt, "terminal_opt");
    }

    return ret;
}

/* Terminals to try. */
#define UXTERM_COMMAND "uxterm"
#define XTERM_COMMAND "xterm"

/* Can the mac terminal be used to launch applications?  Doesn't seem like it
 * in initial trials, but maybe there's some trick? */
#define MAC_BINARY "/Applications/Utilities/Terminal.app/Contents/MacOS/Terminal"

static int
have_terminal(struct ged *gedp)
{
    const char *terminal_list[] = {
	UXTERM_COMMAND, XTERM_COMMAND,
	NULL
    };

    bu_ptbl_reset(&gedp->terminal_opts);

    // Try to find a terminal emulator
    int i = 0;
    const char *terminal = terminal_list[i];
    const char *terminal_fullpath = NULL;
    struct bu_vls twp = BU_VLS_INIT_ZERO;
    while (terminal && !terminal_fullpath) {

	// Check the default which lookup
	terminal_fullpath = bu_which(terminal_list[i]);

	// If that didn't work, try some common paths
	if (!terminal_fullpath) {
	    bu_vls_sprintf(&twp, "/usr/X11R6/bin/%s", terminal_list[i]);
	    terminal_fullpath = bu_which(bu_vls_cstr(&twp));
	}
	if (!terminal_fullpath) {
	    bu_vls_sprintf(&twp, "/usr/X11/bin/%s", terminal_list[i]);
	    terminal_fullpath = bu_which(bu_vls_cstr(&twp));
	}

	if (terminal_fullpath)
	    break;

	// Nope, try the next one
	i++;
	terminal = terminal_list[i];
    }
    bu_vls_free(&twp);

    if (terminal_fullpath) {
	snprintf(gedp->terminal, MAXPATHLEN, "%s", terminal_fullpath);
	static const char *topt = "-e";
	bu_ptbl_ins(&gedp->terminal_opts, (long *)topt);
	return 1;
    }

    return 0;
}

int
ged_set_editor(struct ged *gedp, int non_gui)
{
    /* There are two possible situations - in classic mode the assumption is
     * made that the command window is a controlling terminal, and an editor
     * should be launched that will utilize that controlling window.  In GUI
     * mode, the editor will be launched either as a separate GUI application
     * or in a separate terminal. */
    int need_terminal = 0;
    const char *editor = NULL;

    if (non_gui) {
        // Console editors only
        editor = bu_editor(&gedp->editor_opts, 1, gedp->app_editors_cnt, gedp->app_editors);
    } else {

        // Because we know we're willing to try setting up to use
        // a terminal, we do a little extra checking of any user
        // specified editors.  Define a "short circuiting" list
        // to specify that will prevent libbu's default lists from
        // being invoked
        const char *check_for_editors[2] = {"MGED_NULL_EDITOR", NULL};

        // First check for user-specified GUI editors
        editor = bu_editor(&gedp->editor_opts, 2, 2, (const char **)check_for_editors);
        if (!editor) {
            // First check for user-specified console editors
            // Falling back to console, will need terminal
            editor = bu_editor(&gedp->editor_opts, 1, 2, (const char **)check_for_editors);
            if (editor)
                need_terminal = 1;
        }

        // If the user specified editor can't be launched without a terminal,
        // check if we can do that.  If not, we'll have to fall back to other
        // options
        if (need_terminal) {
            int h = have_terminal(gedp);
            if (!h)
                editor = NULL;
        }

        // If initial attempts didn't find an editor, be more aggressive
        if (!editor) {
            editor = bu_editor(&gedp->editor_opts, 2, gedp->app_editors_cnt, gedp->app_editors);
            if (!editor) {
                // Falling back to console, will need terminal
                editor = bu_editor(&gedp->editor_opts, 1, gedp->app_editors_cnt, gedp->app_editors);
                if (editor)
                    need_terminal = 1;
            }

            if (need_terminal) {
                int h = have_terminal(gedp);
                if (!h)
                    editor = NULL;
            }
        }

    }

    if (!editor) {
        // No suitable editor found
        return 0;
    }

    // Copy editor into ged struct
    snprintf(gedp->editor, MAXPATHLEN, "%s", editor);

    return 1;
}

void
ged_clear_editor(struct ged *gedp)
{
    if (!gedp)
        return;
    gedp->editor[0] = '\0';
    gedp->terminal[0] = '\0';
    bu_ptbl_reset(&gedp->editor_opts);
    bu_ptbl_reset(&gedp->terminal_opts);
}


void
_ged_rt_set_eye_model(struct ged *gedp,
		      vect_t eye_model)
{
    if (gedp->ged_gvp->gv_s->gv_zclip || gedp->ged_gvp->gv_perspective > 0) {
	vect_t temp;

	VSET(temp, 0.0, 0.0, 1.0);
	MAT4X3PNT(eye_model, gedp->ged_gvp->gv_view2model, temp);
    } else {
	/* not doing zclipping, so back out of geometry */
	int i;
	vect_t direction;
	vect_t extremum[2];
	double t_in;
	vect_t diag1;
	vect_t diag2;
	point_t ecenter;

	VSET(eye_model, -gedp->ged_gvp->gv_center[MDX],
	     -gedp->ged_gvp->gv_center[MDY], -gedp->ged_gvp->gv_center[MDZ]);

	for (i = 0; i < 3; ++i) {
	    extremum[0][i] = INFINITY;
	    extremum[1][i] = -INFINITY;
	}

	if (gedp->new_cmd_forms) {
	    VSETALL(extremum[0],  INFINITY);
	    VSETALL(extremum[1], -INFINITY);
	    struct bu_ptbl *db_objs = bv_view_objs(gedp->ged_gvp, BV_DB_OBJS);
	    if (db_objs)
		(void)scene_bounding_sph(db_objs, &(extremum[0]), &(extremum[1]), 1);
	    struct bu_ptbl *local_db_objs = bv_view_objs(gedp->ged_gvp, BV_DB_OBJS | BV_LOCAL_OBJS);
	    if (local_db_objs)
		(void)scene_bounding_sph(local_db_objs, &(extremum[0]), &(extremum[1]), 1);
	} else {
	    (void)dl_bounding_sph(gedp->i->ged_gdp->gd_headDisplay, &(extremum[0]), &(extremum[1]), 1);
	}

	VMOVEN(direction, gedp->ged_gvp->gv_rotation + 8, 3);
	for (i = 0; i < 3; ++i)
	    if (NEAR_ZERO(direction[i], 1e-10))
		direction[i] = 0.0;

	VSUB2(diag1, extremum[1], extremum[0]);
	VADD2(ecenter, extremum[1], extremum[0]);
	VSCALE(ecenter, ecenter, 0.5);
	VSUB2(diag2, ecenter, eye_model);
	t_in = MAGNITUDE(diag1) + MAGNITUDE(diag2);
	VJOIN1(eye_model, eye_model, t_in, direction);
    }
}

void
_ged_rt_output_handler2(void *clientData, int type)
{
    struct ged_subprocess *rrtp = (struct ged_subprocess *)clientData;
    int count = 0;
    int retcode = 0;
    int read_failed_stderr = 0;
    int read_failed_stdout = 0;
    char line[RT_MAXLINE+1] = {0};

    if ((rrtp == (struct ged_subprocess *)NULL) || (rrtp->gedp == (struct ged *)NULL))
	return;

    BU_CKMAG(rrtp, GED_CMD_MAGIC, "ged subprocess");

    struct ged *gedp = rrtp->gedp;

    /* Get data from rt */
    if (rrtp->stderr_active && (count = bu_process_read_n(rrtp->p, BU_PROCESS_STDERR, RT_MAXLINE, (char *)line)) <= 0) {
	read_failed_stderr = 1;
    }
    if (rrtp->stdout_active && (count = bu_process_read_n(rrtp->p, BU_PROCESS_STDOUT, RT_MAXLINE, (char *)line)) <= 0) {
	read_failed_stdout = 1;
    }

    if (read_failed_stderr || read_failed_stdout) {
	/* Done watching for output, undo subprocess I/O hooks. */
	if (type != -1 && gedp->ged_delete_io_handler) {

	    if (rrtp->stdin_active || rrtp->stdout_active || rrtp->stderr_active) {
		// If anyone else is still listening, we're not done yet.
		if (rrtp->stdin_active) {
		    (*gedp->ged_delete_io_handler)(rrtp, BU_PROCESS_STDIN);
		    return;
		}
		if (rrtp->stdout_active) {
		    (*gedp->ged_delete_io_handler)(rrtp, BU_PROCESS_STDOUT);
		    return;
		}
		if (rrtp->stderr_active) {
		    (*gedp->ged_delete_io_handler)(rrtp, BU_PROCESS_STDERR);
		    return;
		}
	    }

	    return;
	}

	/* Either EOF has been sent or there was a read error.
	 * there is no need to block indefinitely */
	retcode = bu_process_wait_n(&rrtp->p, 120);
	int aborted = (retcode == ERROR_PROCESS_ABORTED);

	if (aborted)
	    bu_log("Raytrace aborted.\n");
	else if (retcode)
	    bu_log("Raytrace failed.\n");
	else
	    bu_log("Raytrace complete.\n");

	if (gedp->i->ged_gdp->gd_rtCmdNotify != (void (*)(int))0)
	    gedp->i->ged_gdp->gd_rtCmdNotify(aborted);

	if (rrtp->end_clbk)
	    rrtp->end_clbk(0, NULL, &aborted, rrtp->end_clbk_data);

	/* free rrtp */
	bu_ptbl_rm(&gedp->ged_subp, (long *)rrtp);
	BU_PUT(rrtp, struct ged_subprocess);

	return;
    }

    /* for feelgoodedness */
    line[count] = '\0';

    /* handle (i.e., probably log to stderr) the resulting line */
    if (gedp->ged_output_handler != (void (*)(struct ged *, char *))0)
	ged_output_handler_cb(gedp, line);
    else
	bu_vls_printf(gedp->ged_result_str, "%s", line);

}

static int
ged_rt_output_handler_helper(struct ged_subprocess* rrtp, bu_process_io_t type)
{
    int active = 0;
    int count = 0;
    char line[RT_MAXLINE+1] = {0};

    if (type == BU_PROCESS_STDERR) active = rrtp->stderr_active;
    if (type == BU_PROCESS_STDOUT) active = rrtp->stdout_active;

    if (active && (count = bu_process_read_n(rrtp->p, type, RT_MAXLINE, (char *)line)) <= 0) {
	/* Done watching for output or a bad read, undo subprocess I/O hooks. */
	struct ged *gedp = rrtp->gedp;
	if (gedp->ged_delete_io_handler) {
	    (*gedp->ged_delete_io_handler)(rrtp, type);
	} else {
	    if (type == BU_PROCESS_STDERR) rrtp->stderr_active = 0;
	    if (type == BU_PROCESS_STDOUT) rrtp->stdout_active = 0;
	}

	return 1;
    }


    /* for feelgoodedness */
    line[count] = '\0';

    /* handle (i.e., probably log to stderr) the resulting line */
    if (rrtp->gedp->ged_output_handler != (void (*)(struct ged *, char *))0)
	ged_output_handler_cb(rrtp->gedp, line);
    else
	bu_vls_printf(rrtp->gedp->ged_result_str, "%s", line);

    return 0;
}

void
_ged_rt_output_handler(void *clientData, int mask)
{
    struct ged_subprocess *rrtp = (struct ged_subprocess *)clientData;
    if ((rrtp == (struct ged_subprocess *)NULL) || (rrtp->gedp == (struct ged *)NULL))
	return;

    struct ged *gedp = rrtp->gedp;
    if (gedp->new_cmd_forms) {
	_ged_rt_output_handler2(clientData, mask);
	return;
    }

    BU_CKMAG(rrtp, GED_CMD_MAGIC, "ged subprocess");

    /* Get data from rt */
    if (ged_rt_output_handler_helper(rrtp, BU_PROCESS_STDERR) || ged_rt_output_handler_helper(rrtp, BU_PROCESS_STDOUT)) {
	/* don't fully clean up until we mark each stream inactive */
	if (rrtp->stderr_active || rrtp->stdout_active)
	    return;

	int retcode = 0;

	/* Either EOF has been sent or there was a read error.
	 * there is no need to block indefinitely */
	retcode = bu_process_wait_n(&rrtp->p, 120);
	int aborted = (retcode == ERROR_PROCESS_ABORTED);

	if (aborted)
	    bu_log("Raytrace aborted.\n");
	else if (retcode)
	    bu_log("Raytrace failed.\n");
	else
	    bu_log("Raytrace complete.\n");

	if (gedp->i->ged_gdp->gd_rtCmdNotify != (void (*)(int))0)
	    gedp->i->ged_gdp->gd_rtCmdNotify(aborted);

	if (rrtp->end_clbk)
	    rrtp->end_clbk(0, NULL, &aborted, rrtp->end_clbk_data);

	/* free rrtp */
	bu_ptbl_rm(&gedp->ged_subp, (long *)rrtp);
	BU_PUT(rrtp, struct ged_subprocess);

	return;
    }
}


static void
dl_bitwise_and_fullpath(struct bu_list *hdlp, int flag_val)
{
    struct display_list *gdlp;
    struct display_list *next_gdlp;
    size_t i;
    struct bv_scene_obj *sp;

    gdlp = BU_LIST_NEXT(display_list, hdlp);
    while (BU_LIST_NOT_HEAD(gdlp, hdlp)) {
        next_gdlp = BU_LIST_PNEXT(display_list, gdlp);

        for (BU_LIST_FOR(sp, bv_scene_obj, &gdlp->dl_head_scene_obj)) {
	    if (!sp->s_u_data)
		continue;
	    struct ged_bv_data *bdata = (struct ged_bv_data *)sp->s_u_data;

	    for (i = 0; i < bdata->s_fullpath.fp_len; i++) {
                DB_FULL_PATH_GET(&bdata->s_fullpath, i)->d_flags &= flag_val;
	    }
        }

        gdlp = next_gdlp;
    }
}



static void
dl_write_animate(struct bu_list *hdlp, FILE *fp)
{
    struct display_list *gdlp;
    struct display_list *next_gdlp;
    size_t i;
    struct bv_scene_obj *sp;

    gdlp = BU_LIST_NEXT(display_list, hdlp);
    while (BU_LIST_NOT_HEAD(gdlp, hdlp)) {
        next_gdlp = BU_LIST_PNEXT(display_list, gdlp);

        for (BU_LIST_FOR(sp, bv_scene_obj, &gdlp->dl_head_scene_obj)) {
	    if (!sp->s_u_data)
		continue;
	    struct ged_bv_data *bdata = (struct ged_bv_data *)sp->s_u_data;

	    for (i = 0; i < bdata->s_fullpath.fp_len; i++) {
                if (!(DB_FULL_PATH_GET(&bdata->s_fullpath, i)->d_flags & RT_DIR_USED)) {
		    struct animate *anp;
                    for (anp = DB_FULL_PATH_GET(&bdata->s_fullpath, i)->d_animate; anp; anp=anp->an_forw) {
			db_write_anim(fp, anp);
		    }
                    DB_FULL_PATH_GET(&bdata->s_fullpath, i)->d_flags |= RT_DIR_USED;
		}
	    }
        }

        gdlp = next_gdlp;
    }
}

void
_ged_rt_write(struct ged *gedp,
	      FILE *fp,
	      vect_t eye_model,
	      int argc,
	      const char **argv)
{
    quat_t quat;

    /* Double-precision IEEE floating point only guarantees 15-17
     * digits of precision; single-precision only 6-9 significant
     * decimal digits.  Using a %.15e precision specifier makes our
     * printed value dip into unreliable territory (1+15 digits).
     * Looking through our history, %.14e seems to be safe as the
     * value prior to printing quaternions was %.9e, although anything
     * from 9->14 "should" be safe as it's above our calculation
     * tolerance and above single-precision capability.
     */
    fprintf(fp, "viewsize %.14e;\n", gedp->ged_gvp->gv_size);
    quat_mat2quat(quat, gedp->ged_gvp->gv_rotation);
    fprintf(fp, "orientation %.14e %.14e %.14e %.14e;\n", V4ARGS(quat));
    fprintf(fp, "eye_pt %.14e %.14e %.14e;\n",
		  eye_model[X], eye_model[Y], eye_model[Z]);

    fprintf(fp, "start 0; clean;\n");

    /* If no objects were specified, activate all objects currently displayed.
     * Otherwise, simply draw the specified objects. If the caller passed
     * -1 in argc it means the objects are specified on the command line.
     * (TODO - we shouldn't be doing that anywhere for this; long command
     * strings make Windows unhappy.  Once all the callers have been
     * adjusted to pass the object lists for itemization via pipes below,
     * remove the -1 case.) */
    if (argc >= 0) {
	if (!argc) {
	    if (gedp->new_cmd_forms) {
		BViewState *bvs = gedp->dbi_state->get_view_state(gedp->ged_gvp);
		if (bvs) {
		    std::vector<std::string> drawn_paths = bvs->list_drawn_paths(-1, true);
		    for (size_t i = 0; i < drawn_paths.size(); i++) {
			fprintf(fp, "draw %s;\n", drawn_paths[i].c_str());
		    }
		}
	    } else {
		struct display_list *gdlp;
		for (BU_LIST_FOR(gdlp, display_list, gedp->i->ged_gdp->gd_headDisplay)) {
		    if (((struct directory *)gdlp->dl_dp)->d_addr == RT_DIR_PHONY_ADDR)
			continue;
		    fprintf(fp, "draw %s;\n", bu_vls_addr(&gdlp->dl_path));
		}
	    }
	} else {
	    int i = 0;
	    while (i < argc) {
		fprintf(fp, "draw %s;\n", argv[i++]);
	    }
	}

	fprintf(fp, "prep;\n");
    }

    dl_bitwise_and_fullpath(gedp->i->ged_gdp->gd_headDisplay, ~RT_DIR_USED);

    dl_write_animate(gedp->i->ged_gdp->gd_headDisplay, fp);

    dl_bitwise_and_fullpath(gedp->i->ged_gdp->gd_headDisplay, ~RT_DIR_USED);

    fprintf(fp, "end;\n");
}

int
_ged_run_rt(struct ged *gedp, int cmd_len, const char **gd_rt_cmd, int argc, const char **argv, int stdout_is_txt, int *pid_ctx, bu_clbk_t end_clbk, void *end_clbk_data)
{
    FILE *fp_in;
    vect_t eye_model;
    struct ged_subprocess *run_rtp;
    struct bu_process *p = NULL;

    bu_process_create(&p, gd_rt_cmd, BU_PROCESS_DEFAULT);

    if (pid_ctx)
	*pid_ctx = bu_process_pid(p);

    if (bu_process_pid(p) == -1) {
	bu_vls_printf(gedp->ged_result_str, "\nunable to successfully launch subprocess: ");
	for (int i = 0; i < cmd_len; i++) {
	    bu_vls_printf(gedp->ged_result_str, "%s ", gd_rt_cmd[i]);
	}
	bu_vls_printf(gedp->ged_result_str, "\n");
	return BRLCAD_ERROR;
    }

    fp_in = bu_process_file_open(p, BU_PROCESS_STDIN);

    _ged_rt_set_eye_model(gedp, eye_model);
    _ged_rt_write(gedp, fp_in, eye_model, argc, argv);

    bu_process_file_close(p, BU_PROCESS_STDIN);

    BU_GET(run_rtp, struct ged_subprocess);
    run_rtp->magic = GED_CMD_MAGIC;
    run_rtp->stdin_active = 0;
    run_rtp->stdout_active = 0;
    run_rtp->stderr_active = 0;
    run_rtp->end_clbk = end_clbk;
    run_rtp->end_clbk_data = end_clbk_data;
    bu_ptbl_ins(&gedp->ged_subp, (long *)run_rtp);

    run_rtp->p = p;
    run_rtp->aborted = 0;
    run_rtp->gedp = gedp;

    /* If we know how, set up hooks so the parent process knows to watch for output. */
    if (gedp->ged_create_io_handler) {
	(*gedp->ged_create_io_handler)(run_rtp, BU_PROCESS_STDERR, _ged_rt_output_handler, (void *)run_rtp);
	if (stdout_is_txt)
	    (*gedp->ged_create_io_handler)(run_rtp, BU_PROCESS_STDOUT, _ged_rt_output_handler, (void *)run_rtp);
    }
    return BRLCAD_OK;
}


struct rt_object_selections *
ged_get_object_selections(struct ged *gedp, const char *object_name)
{
    struct rt_object_selections *obj_selections;

    obj_selections = (struct rt_object_selections *)bu_hash_get(gedp->ged_selections, (uint8_t *)object_name, strlen(object_name));

    if (!obj_selections) {
	BU_ALLOC(obj_selections, struct rt_object_selections);
	obj_selections->sets = bu_hash_create(0);
	(void)bu_hash_set(gedp->ged_selections, (uint8_t *)object_name, strlen(object_name), (void *)obj_selections);
    }

    return obj_selections;
}


struct rt_selection_set *
ged_get_selection_set(struct ged *gedp, const char *object_name, const char *selection_name)
{
    struct rt_object_selections *obj_selections;
    struct rt_selection_set *set;

    obj_selections = ged_get_object_selections(gedp, object_name);
    set = (struct rt_selection_set *)bu_hash_get(obj_selections->sets, (uint8_t *)selection_name, strlen(selection_name));
    if (!set) {
	BU_ALLOC(set, struct rt_selection_set);
	BU_PTBL_INIT(&set->selections);
	bu_hash_set(obj_selections->sets, (uint8_t *)selection_name, strlen(selection_name), (void *)set);
    }

    return set;
}


/*
 * Add an instance of object 'objp' to combination 'name'.
 * If the combination does not exist, it is created.
 * region_flag is 1 (region), or 0 (group).
 *
 * Preserves the GIFT semantics.
 */
struct directory *
_ged_combadd(struct ged *gedp,
	     struct directory *objp,
	     char *combname,
	     int region_flag,	/* true if adding region */
	     db_op_t relation,	/* = UNION, SUBTRACT, INTERSECT */
	     int ident,		/* "Region ID" */
	     int air		/* Air code */)
{
    int ac = 1;
    const char *av[2];

    av[0] = objp->d_namep;
    av[1] = NULL;

    if (_ged_combadd2(gedp, combname, ac, av, region_flag, relation, ident, air, NULL, 1) & BRLCAD_ERROR)
	return RT_DIR_NULL;

    /* Done changing stuff - update nref. */
    db_update_nref(gedp->dbip, &rt_uniresource);

    return db_lookup(gedp->dbip, combname, LOOKUP_QUIET);
}


/*
 * Add an instance of object 'objp' to combination 'name'.
 * If the combination does not exist, it is created.
 * region_flag is 1 (region), or 0 (group).
 *
 * Preserves the GIFT semantics.
 */
int
_ged_combadd2(struct ged *gedp,
	      char *combname,
	      int argc,
	      const char *argv[],
	      int region_flag,	/* true if adding region */
	      db_op_t relation,	/* = UNION, SUBTRACT, INTERSECT */
	      int ident,	/* "Region ID" */
	      int air,		/* Air code */
	      matp_t m,         /* Matrix */
	      int validate      /* 1 to check if new members exist, 0 to just add them */)
{
    struct directory *dp;
    struct directory *objp;
    struct rt_db_internal intern;
    struct rt_comb_internal *comb;
    union tree *tp;
    struct rt_tree_array *tree_list;
    size_t node_count;
    size_t actual_count;
    size_t curr_count;
    int i;

    if (argc < 1)
	return BRLCAD_ERROR;

    /*
     * Check to see if we have to create a new combination
     */
    if ((dp = db_lookup(gedp->dbip,  combname, LOOKUP_QUIET)) == RT_DIR_NULL) {
	int flags;

	if (region_flag)
	    flags = RT_DIR_REGION | RT_DIR_COMB;
	else
	    flags = RT_DIR_COMB;

	RT_DB_INTERNAL_INIT(&intern);
	intern.idb_major_type = DB5_MAJORTYPE_BRLCAD;
	intern.idb_type = ID_COMBINATION;
	intern.idb_meth = &OBJ[ID_COMBINATION];

	GED_DB_DIRADD(gedp, dp, combname, -1, 0, flags, (void *)&intern.idb_type, 0);

	BU_ALLOC(comb, struct rt_comb_internal);
	RT_COMB_INTERNAL_INIT(comb);

	intern.idb_ptr = (void *)comb;

	if (region_flag) {
	    struct rt_wdb *wdbp = wdb_dbopen(gedp->dbip, RT_WDB_TYPE_DB_DEFAULT);
	    comb->region_flag = 1;
	    comb->region_id = ident;
	    comb->aircode = air;
	    comb->los = wdbp->wdb_los_default;
	    comb->GIFTmater = wdbp->wdb_mat_default;
	    bu_vls_printf(gedp->ged_result_str, "Creating region with attrs: region_id=%d, ", ident);
	    if (air)
		bu_vls_printf(gedp->ged_result_str, "air=%d, ", air);
	    bu_vls_printf(gedp->ged_result_str, "los=%d, material_id=%d\n",
			  wdbp->wdb_los_default,
			  wdbp->wdb_mat_default);
	} else {
	    comb->region_flag = 0;
	}

	goto addmembers;
    } else if (!(dp->d_flags & RT_DIR_COMB)) {
	bu_vls_printf(gedp->ged_result_str, "%s exists, but is not a combination\n", dp->d_namep);
	return BRLCAD_ERROR;
    }

    /* combination exists, add a new member */
    GED_DB_GET_INTERNAL(gedp, &intern, dp, (fastf_t *)NULL, &rt_uniresource, 0);

    comb = (struct rt_comb_internal *)intern.idb_ptr;
    RT_CK_COMB(comb);

    if (region_flag && !comb->region_flag) {
	bu_vls_printf(gedp->ged_result_str, "%s: not a region\n", dp->d_namep);
	return BRLCAD_ERROR;
    }

addmembers:
    if (comb->tree && db_ck_v4gift_tree(comb->tree) < 0) {
	db_non_union_push(comb->tree, &rt_uniresource);
	if (db_ck_v4gift_tree(comb->tree) < 0) {
	    bu_vls_printf(gedp->ged_result_str, "Cannot flatten tree for editing\n");
	    rt_db_free_internal(&intern);
	    return BRLCAD_ERROR;
	}
    }

    /* make space for an extra leaf */
    curr_count = db_tree_nleaves(comb->tree);
    node_count = curr_count + argc;
    tree_list = (struct rt_tree_array *)bu_calloc(node_count, sizeof(struct rt_tree_array), "tree list");

    /* flatten tree */
    if (comb->tree) {
	actual_count = argc + (struct rt_tree_array *)db_flatten_tree(tree_list, comb->tree, OP_UNION, 1, &rt_uniresource) - tree_list;
	BU_ASSERT(actual_count == node_count);
	comb->tree = TREE_NULL;
    }

    for (i = 0; i < argc; ++i) {
	if (validate) {
	    objp = db_lookup(gedp->dbip, argv[i], LOOKUP_NOISY);
	    if (objp == RT_DIR_NULL) {
		bu_vls_printf(gedp->ged_result_str, "skip member %s\n", argv[i]);
		continue;
	    }
	}

	/* insert new member at end */
	switch (relation) {
	case DB_OP_INTERSECT:
	    tree_list[curr_count].tl_op = OP_INTERSECT;
	    break;
	case DB_OP_SUBTRACT:
	    tree_list[curr_count].tl_op = OP_SUBTRACT;
	    break;
	default:
	    if (relation != DB_OP_UNION) {
		bu_vls_printf(gedp->ged_result_str, "unrecognized relation (assume UNION)\n");
	    }
	    tree_list[curr_count].tl_op = OP_UNION;
	    break;
	}

	/* make new leaf node, and insert at end of list */
	BU_GET(tp, union tree);
	RT_TREE_INIT(tp);
	tree_list[curr_count].tl_tree = tp;
	tp->tr_l.tl_op = OP_DB_LEAF;
	tp->tr_l.tl_name = bu_strdup(argv[i]);
	if (m) {
	    tp->tr_l.tl_mat = (matp_t)bu_malloc(sizeof(mat_t), "mat copy");
	    MAT_COPY(tp->tr_l.tl_mat, m);
	} else {
	    tp->tr_l.tl_mat = (matp_t)NULL;
	}

	++curr_count;
    }

    /* rebuild the tree */
    comb->tree = (union tree *)db_mkgift_tree(tree_list, node_count, &rt_uniresource);

    /* and finally, write it out */
    GED_DB_PUT_INTERNAL(gedp, dp, &intern, &rt_uniresource, 0);

    bu_free((char *)tree_list, "combadd: tree_list");

    /* Done changing stuff - update nref. */
    db_update_nref(gedp->dbip, &rt_uniresource);

    return BRLCAD_OK;
}

void
_ged_wait_status(struct bu_vls *logstr,
		 int status)
{
    int sig = status & 0x7f;
    int core = status & 0x80;
    int ret = status >> 8;

    if (status == 0) {
	bu_vls_printf(logstr, "Normal exit\n");
	return;
    }

    bu_vls_printf(logstr, "Abnormal exit x%x", status);

    if (core)
	bu_vls_printf(logstr, ", core dumped");

    if (sig)
	bu_vls_printf(logstr, ", terminating signal = %d", sig);
    else
	bu_vls_printf(logstr, ", return (exit) code = %d", ret);
}

struct directory **
_ged_build_dpp(struct ged *gedp,
	       const char *path) {
    struct directory *dp;
    struct directory **dpp;
    int i;
    char *begin;
    char *end;
    char *newstr;
    char *list;
    int ac;
    const char **av;
    const char **av_orig = NULL;
    struct bu_vls vls = BU_VLS_INIT_ZERO;

    /*
     * First, build an array of the object's path components.
     * We store the list in av_orig below.
     */
    newstr = bu_strdup(path);
    begin = newstr;
    while ((end = strchr(begin, '/')) != NULL) {
	*end = '\0';
	bu_vls_printf(&vls, "%s ", begin);
	begin = end + 1;
    }
    bu_vls_printf(&vls, "%s ", begin);
    free((void *)newstr);

    list = bu_vls_addr(&vls);

    if (bu_argv_from_tcl_list(list, &ac, &av_orig) != 0) {
	bu_vls_printf(gedp->ged_result_str, "-1");
	bu_vls_free(&vls);
	return (struct directory **)NULL;
    }

    /* skip first element if empty */
    av = av_orig;
    if (*av[0] == '\0') {
	--ac;
	++av;
    }

    /* ignore last element if empty */
    if (*av[ac-1] == '\0')
	--ac;

    /*
     * Next, we build an array of directory pointers that
     * correspond to the object's path.
     */
    dpp = (struct directory **)bu_calloc((size_t)ac+1, sizeof(struct directory *), "_ged_build_dpp: directory pointers");
    for (i = 0; i < ac; ++i) {
	if ((dp = db_lookup(gedp->dbip, av[i], 0)) != RT_DIR_NULL)
	    dpp[i] = dp;
	else {
	    /* object is not currently being displayed */
	    bu_vls_printf(gedp->ged_result_str, "-1");

	    bu_free((void *)dpp, "_ged_build_dpp: directory pointers");
	    bu_free((char *)av_orig, "free av_orig");
	    bu_vls_free(&vls);
	    return (struct directory **)NULL;
	}
    }

    dpp[i] = RT_DIR_NULL;

    bu_free((char *)av_orig, "free av_orig");
    bu_vls_free(&vls);
    return dpp;
}

/*
 * This routine walks through the directory entry list and mallocs enough
 * space for pointers to hold:
 * a) all of the entries if called with an argument of 0, or
 * b) the number of entries specified by the argument if > 0.
 */
struct directory **
_ged_dir_getspace(struct db_i *dbip,
		  size_t num_entries)
{
    size_t i;
    struct directory **dir_basep;
    struct directory *dp;

    if (num_entries == 0) {
	/* Set num_entries to the number of entries */
	for (i = 0; i < RT_DBNHASH; i++)
	    for (dp = dbip->dbi_Head[i]; dp != RT_DIR_NULL; dp = dp->d_forw)
		num_entries++;
    }

    /* Allocate and cast num_entries worth of pointers */
    dir_basep = (struct directory **) bu_malloc((num_entries+1) * sizeof(struct directory *),
						"dir_getspace *dir[]");
    return dir_basep;
}

#if 0

// TODO - need to generalize the path specifier parsing per notes in TODO.  This is a first
// cut at recasting what search is using now, which doesn't implement the full resolving logic.

int
_ged_characterize_pathspec(struct bu_vls *normalized, struct ged *gedp, const char *pathspec)
{
    struct bu_vls np = BU_VLS_INIT_ZERO;
    int flags = 0;

    // Start with nothing - if we get a valid answer we'll print it
    if (normalized) {
	bu_vls_trunc(normalized, 0);
    }

    if (!pathspec)
       	return GED_PATHSPEC_INVALID;

    if (BU_STR_EQUAL(pathspec, "/"))
	return flags;

    if (BU_STR_EQUAL(pathspec, ".")) {
	flags |= GED_PATHSPEC_LOCAL;
	return flags;
    }

    if (BU_STR_EQUAL(pathspec, "|")) {
	flags |= GED_PATHSPEC_FLAT;
	return flags;
    }

    bu_vls_sprintf(&np, "%s", pathspec);
    if (bu_vls_cstr(&np)[0] == '|') {
	flags |= GED_PATHSPEC_FLAT;
	bu_vls_nibble(&np, 1);
    }
    if (BU_STR_EQUAL(bu_vls_cstr(&np), "/")) {
	bu_vls_free(&np);
	return flags;
    }

    if (BU_STR_EQUAL(bu_vls_cstr(&np), ".")) {
	flags |= GED_PATHSPEC_LOCAL;
	bu_vls_free(&np);
	return flags;
    }

    if (bu_vls_cstr(&np)[0] != '/')
	flags |= GED_PATHSPEC_LOCAL;

    const char *bu_norm = bu_path_normalize(bu_vls_cstr(&np));

    if (bu_norm && !BU_STR_EQUAL(bu_norm , "/")) {
	struct bu_vls tmp = BU_VLS_INIT_ZERO;
	char *tbasename = bu_path_basename(bu_vls_cstr(&np), NULL);
	bu_vls_sprintf(&tmp, "%s", tbasename);
	bu_free(tbasename, "free bu_path_basename string (caller's responsibility per bu/log.h)");
	bu_vls_sprintf(&np, "%s", bu_vls_cstr(&tmp));
	bu_vls_free(&tmp);
    } else {
	bu_vls_sprintf(&np, "%s", "/");
    }

    // If we've gotten this far, normalizing to nothing is considered invalid.
    if (!bu_vls_strlen(&np)) {
	bu_vls_free(&np);
	return GED_PATHSPEC_INVALID;
    }

    // If we reduced to the root fullpath, we're done
    if (BU_STR_EQUAL(bu_vls_cstr(&np), "/")) {
	bu_vls_free(&np);
	return flags;
    }

    /* We've handled the toplevel special cases.  If we got here, we have a specific
     * path - now the only question is whether that path is valid */
    flags |= GED_PATHSPEC_SPECIFIC;
    struct directory *path_dp = db_lookup(gedp->dbip, bu_vls_cstr(&np), LOOKUP_QUIET);
    if (path_dp == RT_DIR_NULL) {
	flags = GED_PATHSPEC_INVALID;
    } else {
	if (normalized)
	    bu_vls_sprintf(normalized, "%s", bu_vls_cstr(&np));
    }
    bu_vls_free(&np);

    return flags;
}

#endif

struct display_list *
ged_dl(struct ged *gedp)
{
    if (!gedp || !gedp->i || !gedp->i->ged_gdp)
	return NULL;
    return (struct display_list *)gedp->i->ged_gdp->gd_headDisplay;
}

void
ged_dl_notify_func_set(struct ged *gedp, ged_drawable_notify_func_t f)
{
    if (!gedp || !gedp->i || !gedp->i->ged_gdp)
	return;

    gedp->i->ged_gdp->gd_rtCmdNotify = f;
}

ged_drawable_notify_func_t
ged_dl_notify_func_get(struct ged *gedp)
{
    if (!gedp || !gedp->i || !gedp->i->ged_gdp)
	return NULL;

    return gedp->i->ged_gdp->gd_rtCmdNotify;
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
