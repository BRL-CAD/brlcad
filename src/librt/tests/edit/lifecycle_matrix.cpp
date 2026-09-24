/*                    L I F E C Y C L E _ M A T R I X . C P P
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
/** @file lifecycle_matrix.cpp
 *
 * Check edit-session state transitions with a real database in both unit
 * systems.  The restored geometry is compared with the original object,
 * not just with an intermediate edit result.
 */

#include "common.h"

#include "bu/log.h"
#include "bu/str.h"
#include "raytrace.h"
#include "rt/rt_ecmds.h"
#include "test_utils.h"

struct directory *make_ell(struct rt_wdb *);

static const fastf_t inch_to_mm = 25.4;

static bool
same_ell(const struct rt_edit *edit, fastf_t a_length)
{
    const struct rt_ell_internal *ell =
	(const struct rt_ell_internal *)edit->es_int.idb_ptr;
    vect_t expected_a;
    vect_t expected_b;
    vect_t expected_c;
    point_t expected_v;
    VSET(expected_v, 10, 5, 20);
    VSET(expected_a, a_length, 0, 0);
    VSET(expected_b, 0, 3, 0);
    VSET(expected_c, 0, 0, 2);
    return ell && VNEAR_EQUAL(ell->v, expected_v, VUNITIZE_TOL) &&
	VNEAR_EQUAL(ell->a, expected_a, VUNITIZE_TOL) &&
	VNEAR_EQUAL(ell->b, expected_b, VUNITIZE_TOL) &&
	VNEAR_EQUAL(ell->c, expected_c, VUNITIZE_TOL);
}

static bool
callback_matches(struct rt_edit_map *map, int command, int mode,
		 bu_clbk_t expected, void *expected_data)
{
    bu_clbk_t callback = NULL;
    void *data = NULL;
    return rt_edit_map_clbk_get(&callback, &data, map, command, mode) ==
	BRLCAD_OK && callback == expected && data == expected_data;
}

static int
check_map(void)
{
    const int modes[] = {
	BU_CLBK_PRE, BU_CLBK_DURING, BU_CLBK_POST, BU_CLBK_LINGER
    };
    struct rt_edit_map *source = rt_edit_map_create();
    struct rt_edit_map *copy = rt_edit_map_create();
    int failures = 0;
    for (size_t i = 0; i < sizeof(modes) / sizeof(modes[0]); ++i) {
	int command = ECMD_GET_FILENAME + (int)i;
	if (rt_edit_map_clbk_set(source, command, modes[i],
		edit_test_filename_callback, source) != BRLCAD_OK ||
	    !callback_matches(source, command, modes[i],
		edit_test_filename_callback, source))
	    ++failures;
    }
    if (rt_edit_map_copy(copy, source) != BRLCAD_OK)
	++failures;
    for (size_t i = 0; i < sizeof(modes) / sizeof(modes[0]); ++i) {
	int command = ECMD_GET_FILENAME + (int)i;
	if (!callback_matches(copy, command, modes[i],
		edit_test_filename_callback, source))
	    ++failures;
    }
    if (rt_edit_map_clear(source) != BRLCAD_OK ||
	rt_edit_map_clear(copy) != BRLCAD_OK)
	++failures;
    for (size_t i = 0; i < sizeof(modes) / sizeof(modes[0]); ++i) {
	bu_clbk_t callback = NULL;
	void *data = NULL;
	int command = ECMD_GET_FILENAME + (int)i;
	if (rt_edit_map_clbk_get(&callback, &data, source, command,
		modes[i]) != BRLCAD_ERROR ||
	    rt_edit_map_clbk_get(&callback, &data, copy, command,
		modes[i]) != BRLCAD_ERROR)
	    ++failures;
    }
    rt_edit_map_destroy(copy);
    rt_edit_map_destroy(source);
    bu_log("callback map\t%s\n", failures ? "fail" : "pass");
    return failures;
}

static int
check_unit(fastf_t local2base, const char *unit)
{
    struct db_i *dbip = db_open_inmem();
    if (dbip == DBI_NULL)
	return 1;
    dbip->dbi_local2base = local2base;
    dbip->dbi_base2local = 1.0 / local2base;
    struct rt_wdb *wdbp = wdb_dbopen(dbip, RT_WDB_TYPE_DB_INMEM);
    if (!wdbp) {
	db_close(dbip);
	return 1;
    }
    struct directory *dp = make_ell(wdbp);
    struct db_full_path path;
    db_full_path_init(&path);
    db_add_node_to_full_path(&path, dp);
    struct bn_tol tol = BN_TOL_INIT_TOL;
    struct rt_edit *edit = rt_edit_create(&path, dbip, &tol, NULL);
    int failures = 0;
    if (!edit || !same_ell(edit, 4) ||
	!NEAR_EQUAL(edit->local2base, local2base, VUNITIZE_TOL)) {
	bu_log("lifecycle initial\t%s\tfail\n", unit);
	++failures;
    } else {
	bu_log("lifecycle initial\t%s\tpass\n", unit);
	int opt_result = rt_edit_set_opt(edit, "test-option", "present");
	const char *option = rt_edit_get_opt(edit, "test-option");
	if (opt_result < 0 || !option || !BU_STR_EQUAL(option, "present") ||
	    rt_edit_map_clbk_set(edit->m, ECMD_GET_FILENAME, BU_CLBK_DURING,
		edit_test_filename_callback, edit) != BRLCAD_OK ||
	    rt_edit_revert(edit) != BRLCAD_ERROR ||
	    rt_edit_checkpoint(edit) != BRLCAD_OK)
	    ++failures;

	uint8_t *saved_checkpoint = edit->es_ckpt.ext_buf;
	size_t saved_size = edit->es_ckpt.ext_nbytes;
	/* Force export failure without altering the saved checkpoint. */
	edit->es_int.idb_type = -1;
	int checkpoint_result = rt_edit_checkpoint(edit);
	edit->es_int.idb_type = ID_ELL;
	if (checkpoint_result != BRLCAD_ERROR ||
	    edit->es_ckpt.ext_buf != saved_checkpoint ||
	    edit->es_ckpt.ext_nbytes != saved_size || !same_ell(edit, 4))
	    ++failures;

	/* This placeholder has no importer; failed revert must preserve ELL. */
	edit->es_int.idb_type = ID_UNUSED1;
	int revert_result = rt_edit_revert(edit);
	edit->es_int.idb_type = ID_ELL;
	if (revert_result != BRLCAD_ERROR || !same_ell(edit, 4) ||
	    edit->es_ckpt.ext_buf != saved_checkpoint)
	    ++failures;
	bu_log("failed checkpoint/revert preserve state\t%s\t%s\n", unit,
	    failures ? "fail" : "pass");

	rt_edit_set_edflag(edit, RT_PARAMS_EDIT_SCALE);
	edit->update_views = 0;
	edit->e_inpara = 2;
	edit->e_mvalid = 1;
	edit->es_scale = 2.0;
	edit->e_para[0] = 2.0;
	edit->e_para[1] = 3.0;
	if (rt_edit_process(edit) != BRLCAD_ERROR ||
	    edit->e_inpara || edit->e_mvalid || !ZERO(edit->es_scale) ||
	    edit->update_views || !EQUAL(edit->e_para[0], 2.0) ||
	    !same_ell(edit, 4))
	    ++failures;
	rt_edit_set_edflag(edit, ECMD_ELL_SCALE_A);
	if (rt_edit_process(edit) != BRLCAD_OK || !same_ell(edit, 4) ||
	    edit->update_views != 1)
	    ++failures;
	if (rt_edit_process(edit) != BRLCAD_OK || edit->update_views != 1)
	    ++failures;
	bu_log("failed edit consumes input\t%s\t%s\n", unit,
	    failures ? "fail" : "pass");

	const fastf_t target_length = 2.0 * inch_to_mm;
	rt_edit_set_edflag(edit, ECMD_ELL_SCALE_A);
	edit->mv_context = 1;
	edit->e_inpara = 1;
	edit->e_para[0] = target_length / local2base;
	int edit_result = rt_edit_process(edit);
	bool scaled = same_ell(edit, target_length) &&
	    NEAR_EQUAL(edit->e_para[0], target_length / local2base,
		VUNITIZE_TOL);
	edit->acc_sc_sol = 2.5;
	VSET(edit->e_keypoint, -1, -1, -1);
	point_t original_keypoint = {10, 5, 20};
	if (edit_result != BRLCAD_OK || !scaled ||
	    rt_edit_revert(edit) != BRLCAD_OK || !same_ell(edit, 4) ||
	    !EQUAL(edit->acc_sc_sol, 1.0) ||
	    !VNEAR_EQUAL(edit->e_keypoint, original_keypoint, VUNITIZE_TOL))
	    ++failures;
	if (rt_edit_revert(edit) != BRLCAD_OK || !same_ell(edit, 4))
	    ++failures;
	bu_log("checkpoint/edit/revert\t%s\t%s\n", unit,
		failures ? "fail" : "pass");

	edit->model2objview[0] = 2.0;
	rt_edit_reset(edit);
	bu_clbk_t callback = NULL;
	void *data = NULL;
	if (edit->es_int.idb_ptr || edit->es_ckpt.ext_buf || edit->dbip ||
	    edit->tol || edit->vp || rt_edit_get_opt(edit, "test-option") ||
	    rt_edit_map_clbk_get(&callback, &data, edit->m,
		ECMD_GET_FILENAME, BU_CLBK_DURING) != BRLCAD_ERROR ||
	    !NEAR_EQUAL(edit->model2objview[0], 1.0, VUNITIZE_TOL) ||
	    !NEAR_EQUAL(edit->local2base, 1.0, VUNITIZE_TOL)) {
	    bu_log("lifecycle reset\t%s\tfail\n", unit);
	    ++failures;
	} else {
	    bu_log("lifecycle reset\t%s\tpass\n", unit);
	}

	if (rt_edit_reinit(edit, &path, dbip, &tol, NULL) != BRLCAD_OK ||
	    edit->dbip != dbip || edit->tol != &tol ||
	    !NEAR_EQUAL(edit->local2base, local2base, VUNITIZE_TOL) ||
	    !same_ell(edit, 4)) {
	    bu_log("lifecycle reinit\t%s\tfail\n", unit);
	    ++failures;
	} else {
	    bu_log("lifecycle reinit\t%s\tpass\n", unit);
	}
    }
    rt_edit_destroy(edit);
    db_free_full_path(&path);
    db_close(dbip);
    return failures;
}

int
rt_edit_test_lifecycle_matrix(void)
{
    int failures = check_map();
    struct rt_edit *idle = rt_edit_create(NULL, NULL, NULL, NULL);
    if (!idle)
	return BRLCAD_ERROR;
    idle->e_inpara = 1;
    idle->e_mvalid = 1;
    idle->es_scale = 2.0;
    if (rt_edit_process(NULL) != BRLCAD_ERROR ||
	rt_edit_process(idle) != BRLCAD_ERROR ||
	idle->e_inpara || idle->e_mvalid || !ZERO(idle->es_scale) ||
	idle->update_views)
	++failures;
    rt_edit_destroy(idle);
    failures += check_unit(1.0, "mm");
    failures += check_unit(inch_to_mm, "in");
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
