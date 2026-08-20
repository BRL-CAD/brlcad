/*        B R E P _ C D T _ S U B T O L E R A N C E . C P P
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
#include "cdt/test_api.h"

int
main(int, char **)
{
    const int edge_result = cdt_test_subtolerance_edge_collapse();
    if (edge_result)
	return edge_result;
    const int ring_result = cdt_test_subtolerance_ring();
    return ring_result ? 10 + ring_result : 0;
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
