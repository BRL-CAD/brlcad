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

#include "brep/cdt.h"

int
main(int argc, const char **UNUSED(argv))
{
    if (argc != 1)
	return 1;
    int result = cdt_test_edge_singular_pair();
    if (result)
	return result;
    result = cdt_test_linear_edge_spacing();
    if (result)
	return 10 + result;
    result = cdt_test_bounded_edge_midpoint();
    if (result)
	return 20 + result;
    result = cdt_test_local_defects();
    if (result)
	return result;
    result = cdt_test_assembled_mesh_validation();
    if (result)
	return result;
    result = cdt_test_assembled_shared_chords();
    if (result)
	return 30 + result;
    result = cdt_test_repair_edge_tube();
    if (result)
	return result;
    result = cdt_test_repair_triangle_split();
    if (result)
	return result;
    result = cdt_test_repair_patch_limits();
    if (result)
	return result;
    result = cdt_test_repair_periodic_strip();
    if (result)
	return result;
    result = cdt_test_repair_rigorous_boundary();
    return result ? result : cdt_test_repair_patch_boundary();
}
