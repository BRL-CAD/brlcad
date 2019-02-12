/*                       G E D _ U T I L . C
 * BRL-CAD
 *
 * Copyright (c) 2000-2019 United States Government as represented by
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

#include "bu/app.h"
#include "bu/path.h"
#include "bu/sort.h"
#include "bu/str.h"
#include "bu/vls.h"

#include "ged.h"
#include "./ged_private.h"

int
_ged_results_init(struct ged_results *results)
{
    if (UNLIKELY(!results))
	return GED_ERROR;
    BU_ALLOC(results->results_tbl, struct bu_ptbl);
    BU_PTBL_INIT(results->results_tbl);
    return GED_OK;
}


int
_ged_results_add(struct ged_results *results, const char *result_string)
{
    /* If there isn't a string, we can live with that */
    if (UNLIKELY(!result_string))
	return GED_OK;

    /* If we have nowhere to insert into and we *do* have a string, trouble */
    if (UNLIKELY(!results))
	return GED_ERROR;
    if (UNLIKELY(!(results->results_tbl)))
	return GED_ERROR;
    if (UNLIKELY(!(BU_PTBL_IS_INITIALIZED(results->results_tbl))))
	return GED_ERROR;

    /* We're good to go - copy the string and stuff it in. */
    bu_ptbl_ins(results->results_tbl, (long *)bu_strdup(result_string));

    return GED_OK;
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

int
_ged_vopt(struct bu_vls *UNUSED(msg), int UNUSED(argc), const char **UNUSED(argv), void *set_var)
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
    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    for (i = 0; i < argc; i++) {
	dp = db_lookup(gedp->ged_wdbp->dbip, argv[i], LOOKUP_QUIET);
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

    bu_free(exists, "exists array");
    bu_free(nonexists, "nonexists array");

    return nonexist_cnt;
}



/**
 * Returns
 * 0 on success
 * !0 on failure
 */
static int
_ged_densities_from_file(struct ged *gedp, const char *name, int fault_tolerant)
{
    struct bu_mapped_file *dfile = NULL;
    struct bu_vls msgs = BU_VLS_INIT_ZERO;
    char *buf = NULL;
    int ret = 0;
    int ecnt = 0;

    if (!bu_file_exists(name, NULL)) {
	bu_vls_printf(gedp->ged_result_str, "Could not find density file - %s\n", name);
	return GED_ERROR;
    }

    dfile = bu_open_mapped_file(name, "densities file");
    if (!dfile) {
	bu_vls_printf(gedp->ged_result_str, "Could not open density file - %s\n", name);
	return GED_ERROR;
    }

    buf = (char *)(dfile->buf);

    if (gedp->gd_densities) {
	analyze_densities_destroy(gedp->gd_densities);
    }

    if (gedp->gd_densities_source) {
	bu_free(gedp->gd_densities_source, "free densities source string");
    }

    (void)analyze_densities_create(&(gedp->gd_densities));

    ret = analyze_densities_load(gedp->gd_densities, buf, &msgs, &ecnt);

    if (!fault_tolerant && ecnt > 0) {
	bu_vls_printf(gedp->ged_result_str, "Problem reading densities file %s:\n%s\n", name, bu_vls_cstr(&msgs));
	if (gedp->gd_densities) {
	    analyze_densities_destroy(gedp->gd_densities);
	}
	bu_vls_free(&msgs);
	bu_close_mapped_file(dfile);
	return GED_ERROR;
    }

    bu_vls_free(&msgs);
    bu_close_mapped_file(dfile);

    if (ret > 0) {
	gedp->gd_densities_source = bu_strdup(name);
    }

    return (ret == 0) ? GED_ERROR : GED_OK;
}

/**
 * Load density information into the GED structure.
 *
 * Returns
 * 0 on success
 * !0 on failure
 */
int
_ged_read_densities(struct ged *gedp, const char *filename, int fault_tolerant)
{
    struct bu_vls d_path_dir = BU_VLS_INIT_ZERO;

    if (gedp == GED_NULL || gedp->ged_wdbp == RT_WDB_NULL) {
	return GED_ERROR;
    }

    /* If we've explicitly been given a file, read that */
    if (filename) {
	return _ged_densities_from_file(gedp, filename, fault_tolerant);
    }

    /* If we don't have an explicitly specified file, see if we have definitions in
     * the database itself. */
    if (gedp->ged_wdbp->dbip != DBI_NULL) {
	int ret = 0;
	int ecnt = 0;
	struct bu_vls msgs = BU_VLS_INIT_ZERO;
	struct directory *dp;
	struct rt_db_internal intern;
	struct rt_binunif_internal *bu;
	char *buf;

	dp = db_lookup(gedp->ged_wdbp->dbip, "_DENSITIES", LOOKUP_QUIET);

	if (dp != (struct directory *)NULL) {

	    if (rt_db_get_internal(&intern, dp, gedp->ged_wdbp->dbip, NULL, &rt_uniresource) < 0) {
		bu_vls_printf(_ged_current_gedp->ged_result_str, "could not import %s\n", dp->d_namep);
		return GED_ERROR;
	    }

	    if ((intern.idb_major_type & DB5_MAJORTYPE_BINARY_MASK) == 0)
		return GED_ERROR;

	    bu = (struct rt_binunif_internal *)intern.idb_ptr;

	    RT_CHECK_BINUNIF (bu);


	    if (gedp->gd_densities) {
		analyze_densities_destroy(gedp->gd_densities);
	    }

	    if (gedp->gd_densities_source) {
		bu_free(gedp->gd_densities_source, "free densities source string");
	    }

	    (void)analyze_densities_create(&gedp->gd_densities);

	    buf = (char *)bu_malloc(bu->count+1, "density buffer");
	    memcpy(buf, bu->u.int8, bu->count);

	    ret = analyze_densities_load(gedp->gd_densities, buf, &msgs, &ecnt);

	    if (!fault_tolerant && ecnt > 0) {
		bu_vls_printf(gedp->ged_result_str, "Problem reading densities from .g file:\n%s\n", bu_vls_cstr(&msgs));
		if (gedp->gd_densities) {
		    analyze_densities_destroy(gedp->gd_densities);
		}
		bu_vls_free(&msgs);
		return GED_ERROR;
	    }

	    bu_vls_free(&msgs);
	    bu_free((void *)buf, "density buffer");

	    if (ret > 0) {
		gedp->gd_densities_source = bu_strdup(gedp->ged_wdbp->dbip->dbi_filename);
	    }

	    return (ret == 0) ? GED_ERROR : GED_OK;
	}
    }

    /* If we don't have an explicitly specified file and the database doesn't have any information, see if we
     * have .density files in either the current database directory or HOME. */

    /* Try .g path first */
    if (bu_path_component(&d_path_dir, gedp->ged_wdbp->dbip->dbi_filename, BU_PATH_DIRNAME)) {

	bu_vls_printf(&d_path_dir, "/.density");

	if (bu_file_exists(bu_vls_cstr(&d_path_dir), NULL)) {
	    int ret = _ged_densities_from_file(gedp, bu_vls_cstr(&d_path_dir), fault_tolerant);
	    bu_vls_free(&d_path_dir);
	    return ret;
	}
    }

    /* Try HOME */
    if (bu_dir(NULL, 0, BU_DIR_HOME, NULL)) {

	bu_vls_sprintf(&d_path_dir, "%s/.density", bu_dir(NULL, 0, BU_DIR_HOME, NULL));

	if (bu_file_exists(bu_vls_cstr(&d_path_dir), NULL)) {
	    int ret = _ged_densities_from_file(gedp, bu_vls_cstr(&d_path_dir), fault_tolerant);
	    bu_vls_free(&d_path_dir);
	    return ret;
	}
    }



    return GED_ERROR;
}



/* Wrappers for setting up/tearing down IO handler */
#ifndef _WIN32
void
_ged_create_io_handler(void **UNUSED(chan), struct bu_process *p, int fd, int mode, void *data, io_handler_callback_t callback)
{
    int *fdp;
    if (!p) return;
    fdp = (int *)bu_process_fd(p, fd);
    Tcl_CreateFileHandler(*fdp, mode, callback, (ClientData)data);
}

void
_ged_delete_io_handler(void *UNUSED(interp), void *UNUSED(chan), struct bu_process *p, int fd, void *UNUSED(data), io_handler_callback_t UNUSED(callback))
{
    int *fdp;
    if (!p) return;
    fdp = (int *)bu_process_fd(p, fd);
    Tcl_DeleteFileHandler(*fdp);
    close(*fdp);
}

#else
void
_ged_create_io_handler(void **chan, struct bu_process *p, int fd, int mode, void *data, io_handler_callback_t callback)
{
    HANDLE *fdp;
    if (!chan || !p) return;
    fdp = (HANDLE *)bu_process_fd(p, fd);
    (*chan) = (void *)Tcl_MakeFileChannel(*fdp, mode);
    Tcl_CreateChannelHandler((Tcl_Channel)(*chan), mode, callback, (ClientData)data);
}

void
_ged_delete_io_handler(void *interp, void *chan, struct bu_process *p, int fd, void *data, io_handler_callback_t callback)
{
    HANDLE *fdp;
    Tcl_Interp *tcl_interp;
    if (!chan || !p) return;
    tcl_interp = (Tcl_Interp *)interp;
    fdp = (HANDLE *)bu_process_fd(p, fd);
    Tcl_DeleteChannelHandler((Tcl_Channel)chan, callback, (ClientData)data);
    Tcl_Close(tcl_interp, (Tcl_Channel)chan);
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
