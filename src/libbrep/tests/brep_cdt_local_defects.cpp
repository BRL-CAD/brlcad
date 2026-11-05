/*             B R E P _ C D T _ L O C A L _ D E F E C T S . C P P
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by the
 * U.S. Army Research Laboratory.
 *
 * Distributed under the terms of the GNU Lesser General Public License
 * (LGPL), version 2.1.
 */

#include "common.h"

#include "bu/app.h"
#include "bu/log.h"
#include "cdt/test_api.h"

int
main(int argc, const char **argv)
{
    bu_setprogname(argv[0]);
    if (argc != 1)
	return 1;

    const struct {
	const char *name;
	int (*run)(void);
	int exit_offset;
    } tests[] = {
	{"repair_nearest_triangle", cdt_test_repair_nearest_triangle, 70},
	{"surface_length_estimates", cdt_test_surface_length_estimates, 60},
	{"periodic_edge_normals", cdt_test_periodic_edge_normals, 50},
	{"edge_singular_pair", cdt_test_edge_singular_pair, 0},
	{"closed_edge_seed_policy", cdt_test_closed_edge_seed_policy, 5},
	{"linear_edge_spacing", cdt_test_linear_edge_spacing, 10},
	{"rtree_finalization", cdt_test_rtree_finalization, 0},
	{"bounded_edge_midpoint", cdt_test_bounded_edge_midpoint, 20},
	{"local_defects", cdt_test_local_defects, 0},
	{"assembled_mesh_validation", cdt_test_assembled_mesh_validation, 0},
	{"assembled_shared_chords", cdt_test_assembled_shared_chords, 30},
	{"repair_edge_tube", cdt_test_repair_edge_tube, 0},
	{"repair_triangle_split", cdt_test_repair_triangle_split, 0},
	{"repair_patch_limits", cdt_test_repair_patch_limits, 0},
	{"repair_source_coverage", cdt_test_repair_source_coverage, 0},
	{"periodic_surface_conditioning", cdt_test_periodic_surface_conditioning, 0},
	{"repair_duplicate_quarantine", cdt_test_repair_duplicate_quarantine, 40},
	{"repair_periodic_strip", cdt_test_repair_periodic_strip, 0},
	{"repair_rigorous_boundary", cdt_test_repair_rigorous_boundary, 0},
	{"repair_patch_boundary", cdt_test_repair_patch_boundary, 0},
	{"repair_patch_mapping", cdt_test_repair_patch_mapping, 0}
    };
    for (const auto &test : tests) {
	/* Log before entering the check so a crash leaves its name in CTest's
	 * captured output.  bu_log writes directly to the standard stream. */
	bu_log("Running cdt_test_%s\n", test.name);
	const int result = test.run();
	if (result) {
	    /* Keep the historical exit codes for existing test consumers. */
	    const int exit_code = test.exit_offset + result;
	    bu_log("FAILED cdt_test_%s: result %d (exit code %d)\n",
		test.name, result, exit_code);
	    return exit_code;
	}
    }
    return 0;
}

/*
 * Local Variables:
 * tab-width: 8
 * mode: C++
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
