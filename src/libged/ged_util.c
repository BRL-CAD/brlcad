/*                       G E D _ U T I L . C
 * BRL-CAD
 *
 * Copyright (c) 2000-2020 United States Government as represented by
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
#include "bu/units.h"
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
_ged_densities_from_file(struct analyze_densities **dens, char **den_src, struct ged *gedp, const char *name, int fault_tolerant)
{
    struct analyze_densities *densities;
    struct bu_mapped_file *dfile = NULL;
    struct bu_vls msgs = BU_VLS_INIT_ZERO;
    char *buf = NULL;
    int ret = 0;
    int ecnt = 0;

    if (gedp == GED_NULL || gedp->ged_wdbp == RT_WDB_NULL || !dens) {
	return GED_ERROR;
    }

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

    (void)analyze_densities_create(&densities);

    ret = analyze_densities_load(densities, buf, &msgs, &ecnt);

    if (!fault_tolerant && ecnt > 0) {
	bu_vls_printf(gedp->ged_result_str, "Problem reading densities file %s:\n%s\n", name, bu_vls_cstr(&msgs));
	if (densities) {
	    analyze_densities_destroy(densities);
	}
	bu_vls_free(&msgs);
	bu_close_mapped_file(dfile);
	return GED_ERROR;
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

    return (ret == 0) ? GED_ERROR : GED_OK;
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

    if (gedp == GED_NULL || gedp->ged_wdbp == RT_WDB_NULL || !dens) {
	return GED_ERROR;
    }

    /* If we've explicitly been given a file, read that */
    if (filename) {
	return _ged_densities_from_file(dens, den_src, gedp, filename, fault_tolerant);
    }

    /* If we don't have an explicitly specified file, see if we have definitions in
     * the database itself. */
    if (gedp->ged_wdbp->dbip != DBI_NULL) {
	int ret = 0;
	int ecnt = 0;
	struct bu_vls msgs = BU_VLS_INIT_ZERO;
	struct directory *dp;
	struct rt_db_internal intern;
	struct rt_binunif_internal *bip;
	struct analyze_densities *densities;
	char *buf;

	dp = db_lookup(gedp->ged_wdbp->dbip, GED_DB_DENSITY_OBJECT, LOOKUP_QUIET);

	if (dp != (struct directory *)NULL) {

	    if (rt_db_get_internal(&intern, dp, gedp->ged_wdbp->dbip, NULL, &rt_uniresource) < 0) {
		bu_vls_printf(_ged_current_gedp->ged_result_str, "could not import %s\n", dp->d_namep);
		return GED_ERROR;
	    }

	    if ((intern.idb_major_type & DB5_MAJORTYPE_BINARY_MASK) == 0)
		return GED_ERROR;

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
		return GED_ERROR;
	    }

	    bu_vls_free(&msgs);

	    (*dens) = densities;
	    if (ret > 0) {
		if (den_src) {
		   (*den_src) = bu_strdup(gedp->ged_wdbp->dbip->dbi_filename);
		}
	    } else {
		if (ret == 0 && densities) {
		    analyze_densities_destroy(densities);
		}
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

    return GED_ERROR;
}

int
ged_dbcopy(struct ged *from_gedp, struct ged *to_gedp, const char *from, const char *to, int fflag)
{
    struct directory *from_dp;
    struct bu_external external;

    GED_CHECK_DATABASE_OPEN(from_gedp, GED_ERROR);
    GED_CHECK_DATABASE_OPEN(to_gedp, GED_ERROR);
    GED_CHECK_READ_ONLY(to_gedp, GED_ERROR);

    /* initialize result */
    bu_vls_trunc(from_gedp->ged_result_str, 0);
    bu_vls_trunc(to_gedp->ged_result_str, 0);

    GED_DB_LOOKUP(from_gedp, from_dp, from, LOOKUP_NOISY, GED_ERROR & GED_QUIET);

    if (!fflag && db_lookup(to_gedp->ged_wdbp->dbip, to, LOOKUP_QUIET) != RT_DIR_NULL) {
	bu_vls_printf(from_gedp->ged_result_str, "%s already exists.", to);
	return GED_ERROR;
    }

    if (db_get_external(&external, from_dp, from_gedp->ged_wdbp->dbip)) {
	bu_vls_printf(from_gedp->ged_result_str, "Database read error, aborting\n");
	return GED_ERROR;
    }

    if (wdb_export_external(to_gedp->ged_wdbp, &external, to,
			    from_dp->d_flags,  from_dp->d_minor_type) < 0) {
	bu_free_external(&external);
	bu_vls_printf(from_gedp->ged_result_str,
		      "Failed to write new object (%s) to database - aborting!!\n",
		      to);
	return GED_ERROR;
    }

    bu_free_external(&external);

    /* Need to do something extra for _GLOBAL */
    if (db_version(to_gedp->ged_wdbp->dbip) > 4 && BU_STR_EQUAL(to, DB5_GLOBAL_OBJECT_NAME)) {
	struct directory *to_dp;
	struct bu_attribute_value_set avs;
	const char *val;

	GED_DB_LOOKUP(to_gedp, to_dp, to, LOOKUP_NOISY, GED_ERROR & GED_QUIET);

	bu_avs_init_empty(&avs);
	if (db5_get_attributes(to_gedp->ged_wdbp->dbip, &avs, to_dp)) {
	    bu_vls_printf(from_gedp->ged_result_str, "Cannot get attributes for object %s\n", to_dp->d_namep);
	    return GED_ERROR;
	}

	if ((val = bu_avs_get(&avs, "title")) != NULL)
	    to_gedp->ged_wdbp->dbip->dbi_title = bu_strdup(val);

	if ((val = bu_avs_get(&avs, "units")) != NULL) {
	    double loc2mm;

	    if ((loc2mm = bu_mm_value(val)) > 0) {
		to_gedp->ged_wdbp->dbip->dbi_local2base = loc2mm;
		to_gedp->ged_wdbp->dbip->dbi_base2local = 1.0 / loc2mm;
	    }
	}

	if ((val = bu_avs_get(&avs, "regionid_colortable")) != NULL) {
	    rt_color_free();
	    db5_import_color_table((char *)val);
	}

	bu_avs_free(&avs);
    }

    return GED_OK;
}

int
ged_snap_to_grid(struct ged *gedp, fastf_t *vx, fastf_t *vy)
{
    int nh, nv;		/* whole grid units */
    point_t view_pt;
    point_t view_grid_anchor;
    fastf_t grid_units_h;		/* eventually holds only fractional horizontal grid units */
    fastf_t grid_units_v;		/* eventually holds only fractional vertical grid units */
    fastf_t sf;
    fastf_t inv_sf;

    if (gedp->ged_gvp == GED_VIEW_NULL)
	return 0;

    if (ZERO(gedp->ged_gvp->gv_grid.res_h) ||
	ZERO(gedp->ged_gvp->gv_grid.res_v))
	return 0;

    sf = gedp->ged_gvp->gv_scale*gedp->ged_wdbp->dbip->dbi_base2local;
    inv_sf = 1 / sf;

    VSET(view_pt, *vx, *vy, 0.0);
    VSCALE(view_pt, view_pt, sf);  /* view_pt now in local units */

    MAT4X3PNT(view_grid_anchor, gedp->ged_gvp->gv_model2view, gedp->ged_gvp->gv_grid.anchor);
    VSCALE(view_grid_anchor, view_grid_anchor, sf);  /* view_grid_anchor now in local units */

    grid_units_h = (view_grid_anchor[X] - view_pt[X]) / (gedp->ged_gvp->gv_grid.res_h * gedp->ged_wdbp->dbip->dbi_base2local);
    grid_units_v = (view_grid_anchor[Y] - view_pt[Y]) / (gedp->ged_gvp->gv_grid.res_v * gedp->ged_wdbp->dbip->dbi_base2local);
    nh = grid_units_h;
    nv = grid_units_v;

    grid_units_h -= nh;		/* now contains only the fraction part */
    grid_units_v -= nv;		/* now contains only the fraction part */

    if (grid_units_h <= -0.5)
	*vx = view_grid_anchor[X] - ((nh - 1) * gedp->ged_gvp->gv_grid.res_h * gedp->ged_wdbp->dbip->dbi_base2local);
    else if (0.5 <= grid_units_h)
	*vx = view_grid_anchor[X] - ((nh + 1) * gedp->ged_gvp->gv_grid.res_h * gedp->ged_wdbp->dbip->dbi_base2local);
    else
	*vx = view_grid_anchor[X] - (nh * gedp->ged_gvp->gv_grid.res_h * gedp->ged_wdbp->dbip->dbi_base2local);

    if (grid_units_v <= -0.5)
	*vy = view_grid_anchor[Y] - ((nv - 1) * gedp->ged_gvp->gv_grid.res_v * gedp->ged_wdbp->dbip->dbi_base2local);
    else if (0.5 <= grid_units_v)
	*vy = view_grid_anchor[Y] - ((nv + 1) * gedp->ged_gvp->gv_grid.res_v * gedp->ged_wdbp->dbip->dbi_base2local);
    else
	*vy = view_grid_anchor[Y] - (nv * gedp->ged_gvp->gv_grid.res_v * gedp->ged_wdbp->dbip->dbi_base2local);

    *vx *= inv_sf;
    *vy *= inv_sf;

    return 1;
}

int
ged_rot_args(struct ged *gedp, int argc, const char *argv[], char *coord, mat_t rmat)
{
    vect_t rvec;
    static const char *usage = "[-m|-v] x y z";

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_VIEW(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

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
	return GED_ERROR;
    }

    if (argc == 2) {
	if (bn_decode_vect(rvec, argv[1]) != 3) {
	    bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	    return GED_ERROR;
	}
    } else {
	double scan[3];

	if (sscanf(argv[1], "%lf", &scan[X]) < 1) {
	    bu_vls_printf(gedp->ged_result_str, "ged_eye: bad X value %s\n", argv[1]);
	    return GED_ERROR;
	}

	if (sscanf(argv[2], "%lf", &scan[Y]) < 1) {
	    bu_vls_printf(gedp->ged_result_str, "ged_eye: bad Y value %s\n", argv[2]);
	    return GED_ERROR;
	}

	if (sscanf(argv[3], "%lf", &scan[Z]) < 1) {
	    bu_vls_printf(gedp->ged_result_str, "ged_eye: bad Z value %s\n", argv[3]);
	    return GED_ERROR;
	}

	/* convert from double to fastf_t */
	VMOVE(rvec, scan);
    }

    VSCALE(rvec, rvec, -1.0);
    bn_mat_angles(rmat, rvec[X], rvec[Y], rvec[Z]);

    return GED_OK;
}

int
ged_arot_args(struct ged *gedp, int argc, const char *argv[], mat_t rmat)
{
    point_t pt = VINIT_ZERO;
    vect_t axisv;
    double axis[3]; /* not fastf_t due to sscanf */
    double angle; /* not fastf_t due to sscanf */
    static const char *usage = "x y z angle";

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_VIEW(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    if (argc != 5) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    if (sscanf(argv[1], "%lf", &axis[X]) != 1) {
	bu_vls_printf(gedp->ged_result_str, "%s: bad X value - %s\n", argv[0], argv[1]);
	return GED_ERROR;
    }

    if (sscanf(argv[2], "%lf", &axis[Y]) != 1) {
	bu_vls_printf(gedp->ged_result_str, "%s: bad Y value - %s\n", argv[0], argv[2]);
	return GED_ERROR;
    }

    if (sscanf(argv[3], "%lf", &axis[Z]) != 1) {
	bu_vls_printf(gedp->ged_result_str, "%s: bad Z value - %s\n", argv[0], argv[3]);
	return GED_ERROR;
    }

    if (sscanf(argv[4], "%lf", &angle) != 1) {
	bu_vls_printf(gedp->ged_result_str, "%s: bad angle - %s\n", argv[0], argv[4]);
	return GED_ERROR;
    }

    VUNITIZE(axis);
    VMOVE(axisv, axis);
    bn_mat_arb_rot(rmat, pt, axisv, angle*DEG2RAD);

    return GED_OK;
}

int
ged_tra_args(struct ged *gedp, int argc, const char *argv[], char *coord, vect_t tvec)
{
    static const char *usage = "[-m|-v] x y z";

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_VIEW(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

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
	return GED_ERROR;
    }

    if (argc == 2) {
	if (bn_decode_vect(tvec, argv[1]) != 3) {
	    bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	    return GED_ERROR;
	}
    } else {
	double scan[3];

	if (sscanf(argv[1], "%lf", &scan[X]) != 1) {
	    bu_vls_printf(gedp->ged_result_str, "%s: bad X value %s\n", argv[0], argv[1]);
	    return GED_ERROR;
	}

	if (sscanf(argv[2], "%lf", &scan[Y]) != 1) {
	    bu_vls_printf(gedp->ged_result_str, "%s: bad Y value %s\n", argv[0], argv[2]);
	    return GED_ERROR;
	}

	if (sscanf(argv[3], "%lf", &scan[Z]) != 1) {
	    bu_vls_printf(gedp->ged_result_str, "%s: bad Z value %s\n", argv[0], argv[3]);
	    return GED_ERROR;
	}

	/* convert from double to fastf_t */
	VMOVE(tvec, scan);
    }

    return GED_OK;
}

int
ged_scale_args(struct ged *gedp, int argc, const char *argv[], fastf_t *sf1, fastf_t *sf2, fastf_t *sf3)
{
    static const char *usage = "sf (or) sfx sfy sfz";
    int ret = GED_OK, args_read;
    double scan;

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_VIEW(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    if (argc != 2 && argc != 4) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    if (argc == 2) {
	if (!sf1 || sscanf(argv[1], "%lf", &scan) != 1) {
	    bu_vls_printf(gedp->ged_result_str, "\nbad scale factor '%s'", argv[1]);
	    return GED_ERROR;
	}
	*sf1 = scan;
    } else {
	args_read = sscanf(argv[1], "%lf", &scan);
	if (!sf1 || args_read != 1) {
	    bu_vls_printf(gedp->ged_result_str, "\nbad x scale factor '%s'", argv[1]);
	    ret = GED_ERROR;
	}
	*sf1 = scan;

	args_read = sscanf(argv[2], "%lf", &scan);
	if (!sf2 || args_read != 1) {
	    bu_vls_printf(gedp->ged_result_str, "\nbad y scale factor '%s'", argv[2]);
	    ret = GED_ERROR;
	}
	*sf2 = scan;

	args_read = sscanf(argv[3], "%lf", &scan);
	if (!sf3 || args_read != 1) {
	    bu_vls_printf(gedp->ged_result_str, "\nbad z scale factor '%s'", argv[3]);
	    ret = GED_ERROR;
	}
	*sf3 = scan;
    }
    return ret;
}

size_t
ged_who_argc(struct ged *gedp)
{
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
    struct display_list *gdlp;
    char **vp = start;

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


/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
