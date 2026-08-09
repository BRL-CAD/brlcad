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
    return result ? result : cdt_test_local_defects();
}
