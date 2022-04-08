/*                       G E D _ U T I L . C
 * BRL-CAD
 *
 * Copyright (c) 2000-2022 United States Government as represented by
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

#ifdef HAVE_SYS_TYPES_H
#  include <sys/types.h>
#endif

#include "bio.h"
#include "bresource.h"


#include "bu/app.h"
#include "bu/cmd.h"
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
	for (ctp = cmds; ctp->ct_name != (char *)NULL; ctp++) {
	    maxcmdlen = (maxcmdlen > strlen(ctp->ct_name)) ? maxcmdlen : strlen(ctp->ct_name);
	}
	for (ctp = cmds; ctp->ct_name != (char *)NULL; ctp++) {
	    bu_vls_printf(gedp->ged_result_str, "  %s%*s", ctp->ct_name, (int)(maxcmdlen - strlen(ctp->ct_name)) +   2, " ");
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

void
ged_push_scene_obj(struct ged *gedp, struct bv_scene_obj *sp)
{
    BV_FREE_VLIST(&RTG.rtg_vlfree, &(sp->s_vlist));
    if (sp->s_u_data) {
	struct ged_bv_data *bdata = (struct ged_bv_data *)sp->s_u_data;
	bdata->s_fullpath.fp_len = 0; // Don't free memory, but implicitly clear contents
    }
    bu_ptbl_ins(&gedp->free_solids, (long *)sp);
}

struct bv_scene_obj *
ged_pop_scene_obj(struct ged *gedp)
{
    struct bv_scene_obj *sp = NULL;
    if (BU_PTBL_LEN(&gedp->free_solids)) {
	sp = (struct bv_scene_obj *)BU_PTBL_GET(&gedp->free_solids, (BU_PTBL_LEN(&gedp->free_solids) - 1));
	bu_ptbl_rm(&gedp->free_solids, (long *)sp);
    } else {
	BU_ALLOC(sp, struct bv_scene_obj); // from GET_BV_SCENE_OBJ in bv/defines.h
	struct ged_bv_data *bdata;
	BU_GET(bdata, struct ged_bv_data);
	db_full_path_init(&bdata->s_fullpath);
	sp->s_u_data = (void *)bdata;
    }
    return sp;
}

int
scene_bounding_sph(struct bu_ptbl *so, vect_t *min, vect_t *max, int pflag)
{
    struct bv_scene_obj *sp;
    vect_t minus, plus;
    int is_empty = 1;

    VSETALL((*min),  INFINITY);
    VSETALL((*max), -INFINITY);

    /* calculate the bounding for of all solids being displayed */
    for (size_t i = 0; i < BU_PTBL_LEN(so); i++) {
	struct bv_scene_group *g = (struct bv_scene_group *)BU_PTBL_GET(so, i);
	if (BU_PTBL_LEN(&g->children)) {
	    for (size_t j = 0; j < BU_PTBL_LEN(&g->children); j++) {
		sp = (struct bv_scene_obj *)BU_PTBL_GET(&g->children, j);
		minus[X] = sp->s_center[X] - sp->s_size;
		minus[Y] = sp->s_center[Y] - sp->s_size;
		minus[Z] = sp->s_center[Z] - sp->s_size;
		VMIN((*min), minus);
		plus[X] = sp->s_center[X] + sp->s_size;
		plus[Y] = sp->s_center[Y] + sp->s_size;
		plus[Z] = sp->s_center[Z] + sp->s_size;
		VMAX((*max), plus);

		is_empty = 0;
	    }
	} else {
	    // If we're an evaluated object, the group itself has the
	    // necessary info.
	    minus[X] = g->s_center[X] - g->s_size;
	    minus[Y] = g->s_center[Y] - g->s_size;
	    minus[Z] = g->s_center[Z] - g->s_size;
	    VMIN((*min), minus);
	    plus[X] = g->s_center[X] + g->s_size;
	    plus[Y] = g->s_center[Y] + g->s_size;
	    plus[Z] = g->s_center[Z] + g->s_size;
	    VMAX((*max), plus);
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
    numcol = GED_TERMINAL_WIDTH / cwidth;

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

struct directory **
_ged_getspace(struct db_i *dbip,
	      size_t num_entries)
{
    struct directory **dir_basep;

    if (num_entries == 0)
	num_entries = db_directory_size(dbip);

    /* Allocate and cast num_entries worth of pointers */
    dir_basep = (struct directory **) bu_calloc((num_entries+1), sizeof(struct directory *), "_ged_getspace *dir[]");
    return dir_basep;
}


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
_ged_sort_existing_objs(struct ged *gedp, int argc, const char *argv[], struct directory **dpa)
{
    int i = 0;
    int exist_cnt = 0;
    int nonexist_cnt = 0;
    struct directory *dp;
    const char **exists = (const char **)bu_calloc(argc, sizeof(const char *), "obj exists array");
    const char **nonexists = (const char **)bu_calloc(argc, sizeof(const char *), "obj nonexists array");
    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    for (i = 0; i < argc; i++) {
	dp = db_lookup(gedp->dbip, argv[i], LOOKUP_QUIET);
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

    if (gedp == GED_NULL || gedp->ged_wdbp == RT_WDB_NULL || !dens) {
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
		bu_vls_printf(_ged_current_gedp->ged_result_str, "could not import %s\n", dp->d_namep);
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

    GED_DB_LOOKUP(from_gedp, from_dp, from, LOOKUP_NOISY, BRLCAD_ERROR & BRLCAD_QUIET);

    if (!fflag && db_lookup(to_gedp->dbip, to, LOOKUP_QUIET) != RT_DIR_NULL) {
	bu_vls_printf(from_gedp->ged_result_str, "%s already exists.", to);
	return BRLCAD_ERROR;
    }

    if (db_get_external(&external, from_dp, from_gedp->dbip)) {
	bu_vls_printf(from_gedp->ged_result_str, "Database read error, aborting\n");
	return BRLCAD_ERROR;
    }

    if (wdb_export_external(to_gedp->ged_wdbp, &external, to,
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

	GED_DB_LOOKUP(to_gedp, to_dp, to, LOOKUP_NOISY, BRLCAD_ERROR & BRLCAD_QUIET);

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
	return BRLCAD_HELP;
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
	return BRLCAD_HELP;
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
	return BRLCAD_HELP;
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
	return BRLCAD_HELP;
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
    const char *cmd2 = getenv("GED_TEST_NEW_CMD_FORMS");
    if (BU_STR_EQUAL(cmd2, "1")) {
	if (!gedp || !gedp->ged_gvp)
	    return 0;
	struct bu_ptbl *sg = bv_set_view_db_objs(gedp->ged_gvp);
	return BU_PTBL_LEN(sg);
    }

    struct display_list *gdlp = NULL;
    size_t visibleCount = 0;

    if (!gedp || !gedp->ged_gdp || !gedp->ged_gdp->gd_headDisplay)
	return 0;

    for (BU_LIST_FOR(gdlp, display_list, gedp->ged_gdp->gd_headDisplay)) {
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
    const char *cmd2 = getenv("GED_TEST_NEW_CMD_FORMS");
    if (BU_STR_EQUAL(cmd2, "1")) {
	if (!gedp || !gedp->ged_gvp)
	    return 0;
	struct bu_ptbl *sg = bv_set_view_db_objs(gedp->ged_gvp);
	for (size_t i = 0; i < BU_PTBL_LEN(sg); i++) {
	    struct bv_scene_group *g = (struct bv_scene_group *)BU_PTBL_GET(sg, i);
	    if ((vp != NULL) && ((const char **)vp < end)) {
		*vp++ = bu_strdup(bu_vls_cstr(&g->s_name));
	    } else {
		bu_vls_printf(gedp->ged_result_str, "INTERNAL ERROR: ged_who_argv() ran out of space at %s\n", bu_vls_cstr(&g->s_name));
		break;
	    }
	}
	return (int)BU_PTBL_LEN(sg);
    }

    struct display_list *gdlp;

    if (!gedp || !gedp->ged_gdp || !gedp->ged_gdp->gd_headDisplay)
	return 0;

    if (UNLIKELY(!start || !end)) {
	bu_vls_printf(gedp->ged_result_str, "INTERNAL ERROR: ged_who_argv() called with NULL args\n");
	return 0;
    }

    for (BU_LIST_FOR(gdlp, display_list, gedp->ged_gdp->gd_headDisplay)) {
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

/**
 * Once the vlist has been created, perform the common tasks
 * in handling the drawn solid.
 *
 * This routine must be prepared to run in parallel.
 */
void
_ged_drawH_part2(int dashflag, struct bu_list *vhead, const struct db_full_path *pathp, struct db_tree_state *tsp, struct _ged_client_data *dgcdp)
{

    if (dgcdp->vs.color_override) {
	unsigned char wcolor[3];

	wcolor[0] = (unsigned char)dgcdp->vs.color[0];
	wcolor[1] = (unsigned char)dgcdp->vs.color[1];
	wcolor[2] = (unsigned char)dgcdp->vs.color[2];
	dl_add_path(dashflag, vhead, pathp, tsp, wcolor, dgcdp);
    } else {
	dl_add_path(dashflag, vhead, pathp, tsp, NULL, dgcdp);
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

#define WIN_EDITOR "\"c:/Program Files/Windows NT/Accessories/wordpad\""
#define MAC_EDITOR "/Applications/TextEdit.app/Contents/MacOS/TextEdit"
#define EMACS_EDITOR "emacs"
#define NANO_EDITOR "nano"
#define VIM_EDITOR "vim"
#define VI_EDITOR "vi"

int
_ged_editit(const char *editstring, const char *filename)
{
#ifdef HAVE_UNISTD_H
    int xpid = 0;
    int status = 0;
#endif
    int pid = 0;
    char **avtmp = (char **)NULL;
    const char *terminal = (char *)NULL;
    const char *terminal_opt = (char *)NULL;
    const char *editor = (char *)NULL;
    const char *editor_opt = (char *)NULL;
    const char *file = (const char *)filename;

#if defined(SIGINT) && defined(SIGQUIT)
    void (*s2)();
    void (*s3)();
#endif

    if (!file) {
	bu_log("INTERNAL ERROR: editit filename missing\n");
	return 0;
    }

    char *editstring_cpy = NULL;

    /* convert the edit string into pieces suitable for arguments to execlp */

    if (editstring != NULL) {
	editstring_cpy = bu_strdup(editstring);
	avtmp = (char **)bu_calloc(5, sizeof(char *), "ged_editit: editstring args");
	bu_argv_from_string(avtmp, 4, editstring_cpy);

	if (avtmp[0] && !BU_STR_EQUAL(avtmp[0], "(null)"))
	    terminal = avtmp[0];
	if (avtmp[1] && !BU_STR_EQUAL(avtmp[1], "(null)"))
	    terminal_opt = avtmp[1];
	if (avtmp[2] && !BU_STR_EQUAL(avtmp[2], "(null)"))
	    editor = avtmp[2];
	if (avtmp[3] && !BU_STR_EQUAL(avtmp[3], "(null)"))
	    editor_opt = avtmp[3];
    } else {
	editor = getenv("EDITOR");

	/* still unset? try windows */
	if (!editor || editor[0] == '\0') {
	    if (bu_file_exists(WIN_EDITOR, NULL)) {
		editor = WIN_EDITOR;
	    }
	}

	/* still unset? try mac os x */
	if (!editor || editor[0] == '\0') {
	    if (bu_file_exists(MAC_EDITOR, NULL)) {
		editor = MAC_EDITOR;
	    }
	}

	/* still unset? try emacs */
	if (!editor || editor[0] == '\0') {
	    editor = bu_which(EMACS_EDITOR);
	}

	/* still unset? try nano */
	if (!editor || editor[0] == '\0') {
	    editor = bu_which(NANO_EDITOR);
	}

	/* still unset? try vim */
	if (!editor || editor[0] == '\0') {
	    editor = bu_which(VIM_EDITOR);
	}

	/* still unset? As a last resort, go with vi -
	 * vi is part of the POSIX standard, which is as
	 * close as we can get currently to an editor
	 * that should always be present:
	 * http://pubs.opengroup.org/onlinepubs/9699919799/utilities/vi.html */
	if (!editor || editor[0] == '\0') {
	    editor = bu_which(VI_EDITOR);
	}
    }

    if (!editor) {
	bu_log("INTERNAL ERROR: editit editor missing\n");
	return 0;
    }

    /* print a message to let the user know they need to quit their
     * editor before the application will come back to life.
     */
    {
	size_t length;
	struct bu_vls str = BU_VLS_INIT_ZERO;
	struct bu_vls sep = BU_VLS_INIT_ZERO;
	char *editor_basename;

	if (terminal && editor_opt) {
	    bu_log("Invoking [%s %s %s] via %s\n\n", editor, editor_opt, file, terminal);
	} else if (terminal) {
	    bu_log("Invoking [%s %s] via %s\n\n", editor, file, terminal);
	} else if (editor_opt) {
	    bu_log("Invoking [%s %s %s]\n\n", editor, editor_opt, file);
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

#if defined(SIGINT) && defined(SIGQUIT)
    s2 = signal(SIGINT, SIG_IGN);
    s3 = signal(SIGQUIT, SIG_IGN);
#endif

#ifdef HAVE_UNISTD_H
    if ((pid = fork()) < 0) {
	perror("fork");
	return 0;
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
	    char buffer[RT_MAXLINE + 1] = {0};
	    STARTUPINFO si = {0};
	    PROCESS_INFORMATION pi = {0};
	    si.cb = sizeof(STARTUPINFO);
	    si.lpReserved = NULL;
	    si.lpReserved2 = NULL;
	    si.cbReserved2 = 0;
	    si.lpDesktop = NULL;
	    si.dwFlags = 0;

	    snprintf(buffer, RT_MAXLINE, "%s %s", editor, file);

	    CreateProcess(NULL, buffer, NULL, NULL, TRUE, NORMAL_PRIORITY_CLASS, NULL, NULL, &si, &pi);
	    WaitForSingleObject(pi.hProcess, INFINITE);
	    return 1;
#else
	    char *editor_basename = bu_path_basename(editor, NULL);
	    if (BU_STR_EQUAL(editor_basename, "TextEdit")) {
		/* close stdout/stderr so we don't get blather from TextEdit about service registration failure */
		close(fileno(stdout));
		close(fileno(stderr));
	    }
	    bu_free(editor_basename, "editor_basename free");

	    if (!terminal && !editor_opt) {
		(void)execlp(editor, editor, file, NULL);
	    } else if (!terminal) {
		(void)execlp(editor, editor, editor_opt, file, NULL);
	    } else if (terminal && !terminal_opt) {
		(void)execlp(terminal, terminal, editor, file, NULL);
	    } else if (terminal && !editor_opt) {
		(void)execlp(terminal, terminal, terminal_opt, editor, file, NULL);
	    } else {
		(void)execlp(terminal, terminal, terminal_opt, editor, editor_opt, file, NULL);
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

    if (editstring != NULL) {
	bu_free((void *)avtmp, "ged_editit: avtmp");
	bu_free(editstring_cpy, "editstring copy");
    }

    return 1;
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

	const char *cmd2 = getenv("GED_TEST_NEW_CMD_FORMS");
	if (BU_STR_EQUAL(cmd2, "1")) {
	    struct bu_ptbl *db_objs = bv_set_view_db_objs(gedp->ged_gvp);
	    (void)scene_bounding_sph(db_objs, &(extremum[0]), &(extremum[1]), 1);
	} else {
	    (void)dl_bounding_sph(gedp->ged_gdp->gd_headDisplay, &(extremum[0]), &(extremum[1]), 1);
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
    if (rrtp->stderr_active && bu_process_read((char *)line, &count, rrtp->p, BU_PROCESS_STDERR, RT_MAXLINE) <= 0) {
	read_failed_stderr = 1;
    }
    if (rrtp->stdout_active && bu_process_read((char *)line, &count, rrtp->p, BU_PROCESS_STDOUT, RT_MAXLINE) <= 0) {
	read_failed_stdout = 1;
    }

    if (read_failed_stderr || read_failed_stdout) {
	int aborted;

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
	retcode = bu_process_wait(&aborted, rrtp->p, 120);

	if (aborted)
	    bu_log("Raytrace aborted.\n");
	else if (retcode)
	    bu_log("Raytrace failed.\n");
	else
	    bu_log("Raytrace complete.\n");

	if (gedp->ged_gdp->gd_rtCmdNotify != (void (*)(int))0)
	    gedp->ged_gdp->gd_rtCmdNotify(aborted);

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

void
_ged_rt_output_handler(void *clientData, int mask)
{
    const char *cmd2 = getenv("GED_TEST_NEW_CMD_FORMS");
    if (BU_STR_EQUAL(cmd2, "1")) {
	_ged_rt_output_handler2(clientData, mask);
	return;
    }
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
    if (rrtp->stderr_active && bu_process_read((char *)line, &count, rrtp->p, BU_PROCESS_STDERR, RT_MAXLINE) <= 0) {
	read_failed_stderr = 1;
    }
    if (rrtp->stdout_active && bu_process_read((char *)line, &count, rrtp->p, BU_PROCESS_STDOUT, RT_MAXLINE) <= 0) {
	read_failed_stdout = 1;
    }

    if (read_failed_stderr || read_failed_stdout) {
	int aborted;

	/* Done watching for output, undo subprocess I/O hooks. */
	if (gedp->ged_delete_io_handler) {
	    if (rrtp->stderr_active)
		(*gedp->ged_delete_io_handler)(rrtp, BU_PROCESS_STDERR);
	    if (rrtp->stdout_active)
		(*gedp->ged_delete_io_handler)(rrtp, BU_PROCESS_STDOUT);
	}


	/* Either EOF has been sent or there was a read error.
	 * there is no need to block indefinitely */
	retcode = bu_process_wait(&aborted, rrtp->p, 120);

	if (aborted)
	    bu_log("Raytrace aborted.\n");
	else if (retcode)
	    bu_log("Raytrace failed.\n");
	else
	    bu_log("Raytrace complete.\n");

	if (gedp->ged_gdp->gd_rtCmdNotify != (void (*)(int))0)
	    gedp->ged_gdp->gd_rtCmdNotify(aborted);

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
	    const char *cmd2 = getenv("GED_TEST_NEW_CMD_FORMS");
	    if (BU_STR_EQUAL(cmd2, "1")) {
		struct bu_ptbl *sg = bv_set_view_db_objs(gedp->ged_gvp);
		for (size_t i = 0; i < BU_PTBL_LEN(sg); i++) {
		    struct bv_scene_group *g = (struct bv_scene_group *)BU_PTBL_GET(sg, i);
		    fprintf(fp, "draw %s;\n", bu_vls_cstr(&g->s_name));
		}
	    } else {
		struct display_list *gdlp;
		for (BU_LIST_FOR(gdlp, display_list, gedp->ged_gdp->gd_headDisplay)) {
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

    dl_bitwise_and_fullpath(gedp->ged_gdp->gd_headDisplay, ~RT_DIR_USED);

    dl_write_animate(gedp->ged_gdp->gd_headDisplay, fp);

    dl_bitwise_and_fullpath(gedp->ged_gdp->gd_headDisplay, ~RT_DIR_USED);

    fprintf(fp, "end;\n");
}

int
_ged_run_rt(struct ged *gedp, int cmd_len, const char **gd_rt_cmd, int argc, const char **argv)
{
    FILE *fp_in;
    vect_t eye_model;
    struct ged_subprocess *run_rtp;
    struct bu_process *p = NULL;

    bu_process_exec(&p, gd_rt_cmd[0], cmd_len, gd_rt_cmd, 0, 0);

    if (bu_process_pid(p) == -1) {
	bu_vls_printf(gedp->ged_result_str, "\nunable to successfully launch subprocess: ");
	for (int i = 0; i < cmd_len; i++) {
	    bu_vls_printf(gedp->ged_result_str, "%s ", gd_rt_cmd[i]);
	}
	bu_vls_printf(gedp->ged_result_str, "\n");
	return BRLCAD_ERROR;
    }

    fp_in = bu_process_open(p, BU_PROCESS_STDIN);

    _ged_rt_set_eye_model(gedp, eye_model);
    _ged_rt_write(gedp, fp_in, eye_model, argc, argv);

    bu_process_close(p, BU_PROCESS_STDIN);

    BU_GET(run_rtp, struct ged_subprocess);
    run_rtp->magic = GED_CMD_MAGIC;
    run_rtp->stdin_active = 0;
    run_rtp->stdout_active = 0;
    run_rtp->stderr_active = 0;
    bu_ptbl_ins(&gedp->ged_subp, (long *)run_rtp);

    run_rtp->p = p;
    run_rtp->aborted = 0;
    run_rtp->gedp = gedp;

    /* If we know how, set up hooks so the parent process knows to watch for output. */
    if (gedp->ged_create_io_handler) {
	(*gedp->ged_create_io_handler)(run_rtp, BU_PROCESS_STDERR, _ged_rt_output_handler, (void *)run_rtp);
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
	    comb->region_flag = 1;
	    comb->region_id = ident;
	    comb->aircode = air;
	    comb->los = gedp->ged_wdbp->wdb_los_default;
	    comb->GIFTmater = gedp->ged_wdbp->wdb_mat_default;
	    bu_vls_printf(gedp->ged_result_str, "Creating region with attrs: region_id=%d, ", ident);
	    if (air)
		bu_vls_printf(gedp->ged_result_str, "air=%d, ", air);
	    bu_vls_printf(gedp->ged_result_str, "los=%d, material_id=%d\n",
			  gedp->ged_wdbp->wdb_los_default,
			  gedp->ged_wdbp->wdb_mat_default);
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

int
_ged_set_metaball(struct ged *gedp, struct rt_metaball_internal *mbip, const char *attribute, fastf_t sf)
{
    RT_METABALL_CK_MAGIC(mbip);

    switch (attribute[0]) {
	case 'm':
	case 'M':
	    if (sf <= METABALL_METABALL)
		mbip->method = METABALL_METABALL;
	    else if (sf >= METABALL_BLOB)
		mbip->method = METABALL_BLOB;
	    else
		mbip->method = (int)sf;

	    break;
	case 't':
	case 'T':
	    if (sf < 0)
		mbip->threshold = -sf;
	    else
		mbip->threshold = sf;

	    break;
	default:
	    bu_vls_printf(gedp->ged_result_str, "bad metaball attribute - %s", attribute);
	    return BRLCAD_ERROR;
    }

    return BRLCAD_OK;
}

int
_ged_select_botpts(struct ged *gedp, struct rt_bot_internal *botip, double vx, double vy, double vwidth, double vheight, double vminz, int rflag)
{
    size_t i;
    fastf_t vr = 0.0;
    fastf_t vmin_x = 0.0;
    fastf_t vmin_y = 0.0;
    fastf_t vmax_x = 0.0;
    fastf_t vmax_y = 0.0;

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_VIEW(gedp, BRLCAD_ERROR);

    if (rflag) {
	vr = vwidth;
    } else {
	vmin_x = vx;
	vmin_y = vy;

	if (vwidth > 0)
	    vmax_x = vx + vwidth;
	else {
	    vmin_x = vx + vwidth;
	    vmax_x = vx;
	}

	if (vheight > 0)
	    vmax_y = vy + vheight;
	else {
	    vmin_y = vy + vheight;
	    vmax_y = vy;
	}
    }

    if (rflag) {
	for (i = 0; i < botip->num_vertices; i++) {
	    point_t vloc;
	    point_t vpt;
	    vect_t diff;
	    fastf_t mag;

	    MAT4X3PNT(vpt, gedp->ged_gvp->gv_model2view, &botip->vertices[i*3]);

	    if (vpt[Z] < vminz)
		continue;

	    VSET(vloc, vx, vy, vpt[Z]);
	    VSUB2(diff, vpt, vloc);
	    mag = MAGNITUDE(diff);

	    if (mag > vr)
		continue;

	    bu_vls_printf(gedp->ged_result_str, "%zu ", i);
	}
    } else {
	for (i = 0; i < botip->num_vertices; i++) {
	    point_t vpt;

	    MAT4X3PNT(vpt, gedp->ged_gvp->gv_model2view, &botip->vertices[i*3]);

	    if (vpt[Z] < vminz)
		continue;

	    if (vmin_x <= vpt[X] && vpt[X] <= vmax_x &&
		vmin_y <= vpt[Y] && vpt[Y] <= vmax_y) {
		bu_vls_printf(gedp->ged_result_str, "%zu ", i);
	    }
	}
    }

    return BRLCAD_OK;
}

/*
 * Returns point mbp_i.
 */
struct wdb_metaball_pnt *
_ged_get_metaball_pt_i(struct rt_metaball_internal *mbip, int mbp_i)
{
    int i = 0;
    struct wdb_metaball_pnt *curr_mbpp;

    for (BU_LIST_FOR(curr_mbpp, wdb_metaball_pnt, &mbip->metaball_ctrl_head)) {
	if (i == mbp_i)
	    return curr_mbpp;

	++i;
    }

    return (struct wdb_metaball_pnt *)NULL;
}


#define GED_METABALL_SCALE(_d, _scale) \
    if ((_scale) < 0.0) \
	(_d) = -(_scale); \
    else		  \
	(_d) *= (_scale);

int
_ged_scale_metaball(struct ged *gedp, struct rt_metaball_internal *mbip, const char *attribute, fastf_t sf, int rflag)
{
    int mbp_i;
    struct wdb_metaball_pnt *mbpp;

    RT_METABALL_CK_MAGIC(mbip);

    if (!rflag && sf > 0)
	sf = -sf;

    switch (attribute[0]) {
	case 'f':
	case 'F':
	    if (sscanf(attribute+1, "%d", &mbp_i) != 1)
		mbp_i = 0;

	    if ((mbpp = _ged_get_metaball_pt_i(mbip, mbp_i)) == (struct wdb_metaball_pnt *)NULL)
		return BRLCAD_ERROR;

	    BU_CKMAG(mbpp, WDB_METABALLPT_MAGIC, "wdb_metaball_pnt");
	    GED_METABALL_SCALE(mbpp->fldstr, sf);

	    break;
	case 's':
	case 'S':
	    if (sscanf(attribute+1, "%d", &mbp_i) != 1)
		mbp_i = 0;

	    if ((mbpp = _ged_get_metaball_pt_i(mbip, mbp_i)) == (struct wdb_metaball_pnt *)NULL)
		return BRLCAD_ERROR;

	    BU_CKMAG(mbpp, WDB_METABALLPT_MAGIC, "wdb_metaball_pnt");
	    GED_METABALL_SCALE(mbpp->sweat, sf);

	    break;
	default:
	    bu_vls_printf(gedp->ged_result_str, "bad metaball attribute - %s", attribute);
	    return BRLCAD_ERROR;
    }

    return BRLCAD_OK;
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


/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
