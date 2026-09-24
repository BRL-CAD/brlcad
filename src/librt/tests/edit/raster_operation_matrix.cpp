/*          R A S T E R _ O P E R A T I O N _ M A T R I X . C P P
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
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
/** @file raster_operation_matrix.cpp
 *
 * Execute every DSP, EBM, and VOL descriptor command against a fresh
 * database object in mm and inch units, checking all editable fields.
 */

#include "common.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "bu/app.h"
#include "bu/file.h"
#include "bu/log.h"
#include "bu/malloc.h"
#include "bu/path.h"
#include "bu/str.h"
#include "raytrace.h"
#include "rt/rt_ecmds.h"
#include "test_utils.h"

struct directory *make_dsp(struct rt_wdb *, const char *, unsigned int, unsigned int);
struct directory *make_ebm(struct rt_wdb *, const char *);

enum {
    EBM_FNAME = 12053, EBM_FSIZE = 12054, EBM_HEIGHT = 12055,
    VOL_CSIZE = 13048, VOL_FSIZE = 13049, VOL_THRESH_LO = 13050,
    VOL_THRESH_HI = 13051, VOL_FNAME = 13052,
    DSP_FNAME = 25056, DSP_FSIZE = 25057, DSP_SCALE_X = 25058,
    DSP_SCALE_Y = 25059, DSP_SCALE_ALT = 25060,
    DSP_SMOOTH = 25061, DSP_DATASRC = 25062,
    DSP_SAMPLES_X = 8, DSP_SAMPLES_Y = 8,
    EBM_SAMPLES_X = 4, EBM_SAMPLES_Y = 4,
    VOL_SAMPLES_X = 4, VOL_SAMPLES_Y = 4, VOL_SAMPLES_Z = 4,
    DSP_SAMPLE_BYTES = 2,
    MAX_DATA_BYTES = DSP_SAMPLES_X * DSP_SAMPLES_Y * DSP_SAMPLE_BYTES
};

static const fastf_t inch_to_mm = 25.4;

struct dsp_state {
    const char *name;
    uint32_t xcnt, ycnt;
    unsigned short smooth;
    unsigned char cuttype;
    char datasrc;
    mat_t stom, mtos;
};

struct ebm_state {
    const char *name;
    uint32_t xdim, ydim;
    fastf_t tallness;
    char datasrc;
    mat_t mat;
};

struct vol_state {
    const char *name;
    uint32_t xdim, ydim, zdim;
    uint32_t lo, hi;
    char datasrc;
    vect_t cellsize;
    mat_t mat;
};

static bool
create_data_file(char path[MAXPATHLEN], size_t bytes)
{
    unsigned char data[MAX_DATA_BYTES] = {0};
    if (bytes > sizeof(data))
	return false;
    FILE *file = bu_temp_file(path, MAXPATHLEN);
    if (!file)
	return false;
    for (size_t i = 0; i < bytes; i++)
	data[i] = (unsigned char)(i & UINT8_MAX);
    size_t written = fwrite(data, 1, bytes, file);
    int closed = fclose(file);
    if (written == bytes && closed == 0)
	return true;
    bu_file_delete(path);
    return false;
}

static bool
create_data_files(char initial[MAXPATHLEN], char selected[MAXPATHLEN], size_t bytes)
{
    if (!create_data_file(initial, bytes))
	return false;
    if (!create_data_file(selected, bytes)) {
	bu_file_delete(initial);
	return false;
    }
    return true;
}

static bool
delete_data_files(const char *initial, const char *selected)
{
    bool first = bu_file_delete(initial);
    bool second = bu_file_delete(selected);
    return first && second;
}

static struct rt_wdb *
open_database(struct db_i **dbip, fastf_t local2base)
{
    *dbip = db_open_inmem();
    if (*dbip == DBI_NULL)
	return NULL;
    (*dbip)->dbi_local2base = local2base;
    (*dbip)->dbi_base2local = 1.0 / local2base;
    struct rt_wdb *wdbp = wdb_dbopen(*dbip, RT_WDB_TYPE_DB_INMEM);
    if (!wdbp) {
	db_close(*dbip);
	*dbip = DBI_NULL;
    }
    return wdbp;
}

static bool
same_matrix(const mat_t actual, const mat_t expected, const char *name)
{
    bool same = true;
    for (int i = 0; i < 16; i++) {
	if (!NEAR_EQUAL(actual[i], expected[i], VUNITIZE_TOL)) {
	    bu_log("%s[%d]: expected %.17g, got %.17g\n",
		   name, i, expected[i], actual[i]);
	    same = false;
	}
    }
    return same;
}

static bool
same_dsp(const struct rt_edit *edit, const void *expected_data)
{
    const struct dsp_state *expected = (const struct dsp_state *)expected_data;
    const struct rt_dsp_internal *actual =
	(const struct rt_dsp_internal *)edit->es_int.idb_ptr;
    bool same = BU_STR_EQUAL(bu_vls_cstr(&actual->dsp_name), expected->name) &&
	actual->dsp_xcnt == expected->xcnt && actual->dsp_ycnt == expected->ycnt &&
	actual->dsp_smooth == expected->smooth &&
	actual->dsp_cuttype == expected->cuttype &&
	actual->dsp_datasrc == expected->datasrc;
    if (!same)
	bu_log("DSP fields: name=%s, samples=%u/%u, smooth=%u, cut=%c, source=%c\n",
	       bu_vls_cstr(&actual->dsp_name), actual->dsp_xcnt, actual->dsp_ycnt,
	       actual->dsp_smooth, actual->dsp_cuttype, actual->dsp_datasrc);
    return same_matrix(actual->dsp_stom, expected->stom, "DSP stom") &&
	same_matrix(actual->dsp_mtos, expected->mtos, "DSP mtos") && same;
}

static bool
same_ebm(const struct rt_edit *edit, const void *expected_data)
{
    const struct ebm_state *expected = (const struct ebm_state *)expected_data;
    const struct rt_ebm_internal *actual =
	(const struct rt_ebm_internal *)edit->es_int.idb_ptr;
    bool same = BU_STR_EQUAL(actual->name, expected->name) &&
	actual->xdim == expected->xdim && actual->ydim == expected->ydim &&
	NEAR_EQUAL(actual->tallness, expected->tallness, VUNITIZE_TOL) &&
	actual->datasrc == expected->datasrc;
    if (!same)
	bu_log("EBM fields: name=%s, samples=%u/%u, height=%.17g, source=%c\n",
	       actual->name, actual->xdim, actual->ydim,
	       actual->tallness, actual->datasrc);
    return same_matrix(actual->mat, expected->mat, "EBM mat") && same;
}

static bool
same_vol(const struct rt_edit *edit, const void *expected_data)
{
    const struct vol_state *expected = (const struct vol_state *)expected_data;
    const struct rt_vol_internal *actual =
	(const struct rt_vol_internal *)edit->es_int.idb_ptr;
    bool same = BU_STR_EQUAL(actual->name, expected->name) &&
	actual->xdim == expected->xdim && actual->ydim == expected->ydim &&
	actual->zdim == expected->zdim && actual->lo == expected->lo &&
	actual->hi == expected->hi && actual->datasrc == expected->datasrc &&
	VNEAR_EQUAL(actual->cellsize, expected->cellsize, VUNITIZE_TOL);
    if (!same)
	bu_log("VOL fields: name=%s, samples=%u/%u/%u, thresholds=%u/%u, "
	       "cell=(%g %g %g), source=%c\n", actual->name,
	       actual->xdim, actual->ydim, actual->zdim, actual->lo, actual->hi,
	       V3ARGS(actual->cellsize), actual->datasrc);
    return same_matrix(actual->mat, expected->mat, "VOL mat") && same;
}

typedef bool (*state_check)(const struct rt_edit *, const void *);

static bool
commands_match(int type, const int *commands, size_t count)
{
    const struct rt_edit_prim_desc *desc = EDOBJ[type].ft_edit_desc ?
	EDOBJ[type].ft_edit_desc() : NULL;
    if (!desc || desc->ncmd != (int)count)
	return false;
    for (int i = 0; i < desc->ncmd; i++) {
	bool found = false;
	for (size_t j = 0; j < count; j++) {
	    if (desc->cmds[i].cmd_id == commands[j])
		found = true;
	}
	if (!found)
	    return false;
    }
    return true;
}

static int
run_case(struct db_i *dbip, struct directory *dp, const char *unit,
         const char *name, int command_id, const fastf_t *parameters,
         size_t parameter_count, const char *filename,
         bool expect_error, state_check check, const void *expected)
{
    struct db_full_path path;
    db_full_path_init(&path);
    db_add_node_to_full_path(&path, dp);
    struct bn_tol tol = BN_TOL_INIT_TOL;
    struct rt_edit *edit = rt_edit_create(&path, dbip, &tol, NULL);
    db_free_full_path(&path);
    const struct rt_edit_prim_desc *descriptor =
	EDOBJ[dp->d_minor_type].ft_edit_desc ?
	EDOBJ[dp->d_minor_type].ft_edit_desc() : NULL;
    bool found = false;
    if (descriptor) {
	for (int i = 0; i < descriptor->ncmd; i++) {
	    if (descriptor->cmds[i].cmd_id == command_id)
		found = true;
	}
    }
    bool ok = edit && found;
    if (ok && filename)
	ok = rt_edit_map_clbk_set(edit->m, ECMD_GET_FILENAME, BU_CLBK_DURING,
	    edit_test_filename_callback, (void *)filename) == BRLCAD_OK;
    if (ok) {
	edit->mv_context = 1;
	rt_edit_set_edflag(edit, command_id);
	edit->e_inpara = (int)parameter_count;
	for (size_t i = 0; i < parameter_count; i++)
	    edit->e_para[i] = parameters[i];
	int result = rt_edit_process(edit);
	ok = (expect_error ? result == BRLCAD_ERROR : result == BRLCAD_OK) &&
	    NEAR_EQUAL(edit->local2base, dbip->dbi_local2base, VUNITIZE_TOL);
	if (!check(edit, expected))
	    ok = false;
	for (size_t i = 0; i < parameter_count; i++) {
	    if (isnan(parameters[i]) ? !isnan(edit->e_para[i]) :
		!NEAR_EQUAL(edit->e_para[i], parameters[i], VUNITIZE_TOL))
		ok = false;
	}
	if (!ok)
	    bu_log("%s: result=%d, log=%s\n", name, result,
		   bu_vls_cstr(edit->log_str));
    }
    bu_log("%s\t%d\t%s\t%s\n", name, command_id, unit,
	   ok ? "pass" : "fail");
    if (edit)
	rt_edit_destroy(edit);
    return ok ? 0 : 1;
}

static int
run_dsp_unit(fastf_t local2base, const char *unit)
{
    char initial_file[MAXPATHLEN] = {0}, selected_file[MAXPATHLEN] = {0};
    if (!create_data_files(initial_file, selected_file, MAX_DATA_BYTES)) {
	bu_log("Cannot create DSP matrix data files\n");
	return 1;
    }
    struct db_i *dbip;
    struct rt_wdb *wdbp = open_database(&dbip, local2base);
    if (!wdbp) {
	delete_data_files(initial_file, selected_file);
	return 1;
    }
    struct directory *dp = make_dsp(wdbp, initial_file, DSP_SAMPLES_X, DSP_SAMPLES_Y);
    struct dsp_state base = {};
    base.name = initial_file;
    base.xcnt = DSP_SAMPLES_X;
    base.ycnt = DSP_SAMPLES_Y;
    base.smooth = 1;
    base.cuttype = DSP_CUT_DIR_llUR;
    base.datasrc = RT_DSP_SRC_FILE;
    MAT_IDN(base.stom);
    MAT_IDN(base.mtos);
    struct dsp_state expected = base;
    int failures = 0;

    expected.name = selected_file;
    failures += run_case(dbip, dp, unit, "dsp filename", DSP_FNAME,
	NULL, 0, selected_file, false, same_dsp, &expected);

    expected = base;
    expected.xcnt = 4;
    expected.ycnt = 16;
    const fastf_t counts[] = {4, 16};
    failures += run_case(dbip, dp, unit, "dsp sample counts", DSP_FSIZE,
	counts, 2, NULL, false, same_dsp, &expected);

    const int scales[] = {DSP_SCALE_X, DSP_SCALE_Y, DSP_SCALE_ALT};
    const int diagonal[] = {MSX, MSY, MSZ};
    const fastf_t cell_size_base = 2 * inch_to_mm;
    const fastf_t cell_size[] = {cell_size_base / local2base};
    for (size_t i = 0; i < sizeof(scales) / sizeof(scales[0]); i++) {
	expected = base;
	expected.stom[diagonal[i]] = cell_size_base;
	expected.mtos[diagonal[i]] = 1.0 / cell_size_base;
	failures += run_case(dbip, dp, unit, "dsp cell/altitude scale", scales[i],
	    cell_size, 1, NULL, false, same_dsp, &expected);
    }

    expected = base;
    expected.smooth = 0;
    const fastf_t smooth[] = {0};
    failures += run_case(dbip, dp, unit, "dsp smooth flag", DSP_SMOOTH,
	smooth, 1, NULL, false, same_dsp, &expected);

    expected = base;
    expected.datasrc = RT_DSP_SRC_OBJ;
    const fastf_t object_source[] = {1};
    failures += run_case(dbip, dp, unit, "dsp data source", DSP_DATASRC,
	object_source, 1, NULL, false, same_dsp, &expected);

    const fastf_t fractional_counts[] = {4.5, 8};
    failures += run_case(dbip, dp, unit, "dsp reject fractional counts", DSP_FSIZE,
	fractional_counts, 2, NULL, true, same_dsp, &base);
    const fastf_t oversized_counts[] = {9, 9};
    failures += run_case(dbip, dp, unit, "dsp reject undersized file", DSP_FSIZE,
	oversized_counts, 2, NULL, true, same_dsp, &base);
    const fastf_t overflow_counts[] = {(fastf_t)UINT32_MAX + 1.0, 1};
    failures += run_case(dbip, dp, unit, "dsp reject count overflow", DSP_FSIZE,
	overflow_counts, 2, NULL, true, same_dsp, &base);
    const fastf_t fractional_source[] = {1.5};
    failures += run_case(dbip, dp, unit, "dsp reject fractional source", DSP_DATASRC,
	fractional_source, 1, NULL, true, same_dsp, &base);
    const fastf_t nonfinite_scale[] = {NAN};
    failures += run_case(dbip, dp, unit, "dsp reject nonfinite scale", DSP_SCALE_X,
	nonfinite_scale, 1, NULL, true, same_dsp, &base);

    /* The database search path, not the current working directory, resolves
     * a relative DSP source name. */
    char basename[MAXPATHLEN] = {0};
    bu_path_basename(initial_file, basename);
    char *dirname = bu_path_dirname(initial_file);
    struct rt_db_internal intern;
    RT_DB_INTERNAL_INIT(&intern);
    if (rt_db_get_internal(&intern, dp, dbip, NULL) < 0) {
	bu_log("Cannot import DSP search-path fixture\n");
	++failures;
    } else {
	struct rt_dsp_internal *dsp = (struct rt_dsp_internal *)intern.idb_ptr;
	bu_vls_strcpy(&dsp->dsp_name, basename);
	if (rt_db_put_internal(dp, dbip, &intern) < 0) {
	    rt_db_free_internal(&intern);
	    bu_log("Cannot write DSP search-path fixture\n");
	    ++failures;
	} else {
	    bu_free(dbip->dbi_filepath[0], "DSP old search path");
	    dbip->dbi_filepath[0] = dirname;
	    dirname = NULL;
	    expected = base;
	    expected.name = basename;
	    expected.xcnt = 4;
	    expected.ycnt = 16;
	    failures += run_case(dbip, dp, unit, "dsp relative search path",
		DSP_FSIZE, counts, 2, NULL, false, same_dsp, &expected);
	}
    }
    if (dirname)
	bu_free(dirname, "DSP search path");

    db_close(dbip);
    if (!delete_data_files(initial_file, selected_file))
	++failures;
    return failures;
}

static int
run_ebm_unit(fastf_t local2base, const char *unit)
{
    char initial_file[MAXPATHLEN] = {0}, selected_file[MAXPATHLEN] = {0};
    const size_t data_bytes = EBM_SAMPLES_X * EBM_SAMPLES_Y;
    if (!create_data_files(initial_file, selected_file, data_bytes)) {
	bu_log("Cannot create EBM matrix data files\n");
	return 1;
    }
    struct db_i *dbip;
    struct rt_wdb *wdbp = open_database(&dbip, local2base);
    if (!wdbp) {
	delete_data_files(initial_file, selected_file);
	return 1;
    }
    struct directory *dp = make_ebm(wdbp, initial_file);
    struct ebm_state base = {};
    base.name = initial_file;
    base.xdim = EBM_SAMPLES_X;
    base.ydim = EBM_SAMPLES_Y;
    base.tallness = 10.0;
    base.datasrc = RT_EBM_SRC_FILE;
    MAT_IDN(base.mat);
    struct ebm_state expected = base;
    int failures = 0;

    expected.name = selected_file;
    failures += run_case(dbip, dp, unit, "ebm filename", EBM_FNAME,
	NULL, 0, selected_file, false, same_ebm, &expected);

    expected = base;
    expected.xdim = 2;
    expected.ydim = 8;
    const fastf_t counts[] = {2, 8};
    failures += run_case(dbip, dp, unit, "ebm bitmap size", EBM_FSIZE,
	counts, 2, NULL, false, same_ebm, &expected);

    expected = base;
    expected.tallness = 2 * inch_to_mm;
    const fastf_t height[] = {expected.tallness / local2base};
    failures += run_case(dbip, dp, unit, "ebm extrusion depth", EBM_HEIGHT,
	height, 1, NULL, false, same_ebm, &expected);

    const fastf_t fractional_counts[] = {2.5, 4};
    failures += run_case(dbip, dp, unit, "ebm reject fractional counts", EBM_FSIZE,
	fractional_counts, 2, NULL, true, same_ebm, &base);
    const fastf_t oversized_counts[] = {5, 5};
    failures += run_case(dbip, dp, unit, "ebm reject undersized file", EBM_FSIZE,
	oversized_counts, 2, NULL, true, same_ebm, &base);
    const fastf_t overflow_counts[] = {(fastf_t)UINT32_MAX + 1.0, 1};
    failures += run_case(dbip, dp, unit, "ebm reject count overflow", EBM_FSIZE,
	overflow_counts, 2, NULL, true, same_ebm, &base);

    db_close(dbip);
    if (!delete_data_files(initial_file, selected_file))
	++failures;
    return failures;
}

static struct directory *
make_vol_file(struct rt_wdb *wdbp, const char *data_file)
{
    struct rt_vol_internal *vol;
    BU_ALLOC(vol, struct rt_vol_internal);
    vol->magic = RT_VOL_INTERNAL_MAGIC;
    if (bu_strlcpy(vol->name, data_file, RT_VOL_NAME_LEN) >= RT_VOL_NAME_LEN) {
	BU_PUT(vol, struct rt_vol_internal);
	return RT_DIR_NULL;
    }
    vol->datasrc = RT_VOL_SRC_FILE;
    vol->xdim = VOL_SAMPLES_X;
    vol->ydim = VOL_SAMPLES_Y;
    vol->zdim = VOL_SAMPLES_Z;
    vol->lo = 5;
    vol->hi = 250;
    VSET(vol->cellsize, 1, 1, 1);
    MAT_IDN(vol->mat);
    vol->map = NULL;
    vol->bip = NULL;
    if (wdb_export(wdbp, "vol_matrix", vol, ID_VOL, 1.0) != 0)
	return RT_DIR_NULL;
    return db_lookup(wdbp->dbip, "vol_matrix", LOOKUP_QUIET);
}

static int
run_vol_unit(fastf_t local2base, const char *unit)
{
    char initial_file[MAXPATHLEN] = {0}, selected_file[MAXPATHLEN] = {0};
    const size_t data_bytes = VOL_SAMPLES_X * VOL_SAMPLES_Y * VOL_SAMPLES_Z;
    if (!create_data_files(initial_file, selected_file, data_bytes)) {
	bu_log("Cannot create VOL matrix data files\n");
	return 1;
    }
    struct db_i *dbip;
    struct rt_wdb *wdbp = open_database(&dbip, local2base);
    if (!wdbp) {
	delete_data_files(initial_file, selected_file);
	return 1;
    }
    struct directory *dp = make_vol_file(wdbp, initial_file);
    if (dp == RT_DIR_NULL) {
	bu_log("Cannot create VOL matrix fixture\n");
	db_close(dbip);
	delete_data_files(initial_file, selected_file);
	return 1;
    }
    struct vol_state base = {};
    base.name = initial_file;
    base.xdim = VOL_SAMPLES_X;
    base.ydim = VOL_SAMPLES_Y;
    base.zdim = VOL_SAMPLES_Z;
    base.lo = 5;
    base.hi = 250;
    base.datasrc = RT_VOL_SRC_FILE;
    VSET(base.cellsize, 1, 1, 1);
    MAT_IDN(base.mat);
    struct vol_state expected = base;
    int failures = 0;

    expected.name = selected_file;
    failures += run_case(dbip, dp, unit, "vol filename", VOL_FNAME,
	NULL, 0, selected_file, false, same_vol, &expected);

    expected = base;
    expected.xdim = 2;
    expected.ydim = 4;
    expected.zdim = 8;
    const fastf_t counts[] = {2, 4, 8};
    failures += run_case(dbip, dp, unit, "vol voxel counts", VOL_FSIZE,
	counts, 3, NULL, false, same_vol, &expected);

    expected = base;
    VSET(expected.cellsize, inch_to_mm, 2 * inch_to_mm, 3 * inch_to_mm);
    const fastf_t cellsize[] = {
	inch_to_mm / local2base,
	2 * inch_to_mm / local2base,
	3 * inch_to_mm / local2base
    };
    failures += run_case(dbip, dp, unit, "vol cell size", VOL_CSIZE,
	cellsize, 3, NULL, false, same_vol, &expected);

    expected = base;
    expected.lo = 50;
    const fastf_t low[] = {50};
    failures += run_case(dbip, dp, unit, "vol low threshold", VOL_THRESH_LO,
	low, 1, NULL, false, same_vol, &expected);

    expected = base;
    expected.hi = 200;
    const fastf_t high[] = {200};
    failures += run_case(dbip, dp, unit, "vol high threshold", VOL_THRESH_HI,
	high, 1, NULL, false, same_vol, &expected);

    const fastf_t fractional_counts[] = {2.5, 4, 8};
    failures += run_case(dbip, dp, unit, "vol reject fractional counts", VOL_FSIZE,
	fractional_counts, 3, NULL, true, same_vol, &base);

    db_close(dbip);
    if (!delete_data_files(initial_file, selected_file))
	++failures;
    return failures;
}

int
rt_edit_test_raster_operation_matrix(void)
{
    const int dsp_commands[] = {
	DSP_FNAME, DSP_FSIZE, DSP_SCALE_X, DSP_SCALE_Y,
	DSP_SCALE_ALT, DSP_SMOOTH, DSP_DATASRC
    };
    const int ebm_commands[] = {EBM_FNAME, EBM_FSIZE, EBM_HEIGHT};
    const int vol_commands[] = {
	VOL_FNAME, VOL_FSIZE, VOL_CSIZE, VOL_THRESH_LO, VOL_THRESH_HI
    };
    if (!commands_match(ID_DSP, dsp_commands,
	sizeof(dsp_commands) / sizeof(dsp_commands[0])) ||
	!commands_match(ID_EBM, ebm_commands,
	sizeof(ebm_commands) / sizeof(ebm_commands[0])) ||
	!commands_match(ID_VOL, vol_commands,
	sizeof(vol_commands) / sizeof(vol_commands[0]))) {
	bu_log("Raster edit descriptors differ from the operation matrix\n");
	return BRLCAD_ERROR;
    }
    bu_log("operation\tcommand_id\tunits\tresult\n");
    int failures = run_dsp_unit(1.0, "mm");
    failures += run_dsp_unit(inch_to_mm, "in");
    failures += run_ebm_unit(1.0, "mm");
    failures += run_ebm_unit(inch_to_mm, "in");
    failures += run_vol_unit(1.0, "mm");
    failures += run_vol_unit(inch_to_mm, "in");
    return failures ? BRLCAD_ERROR : BRLCAD_OK;
}

/*
 * Local Variables:
 * mode: C++
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
