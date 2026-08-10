/*                     R T R E E . C P P
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This file is in the public domain.
 */

#include "common.h"

#include <cstddef>

#include "../RTree.h"


static bool
count_hit(const std::size_t &, void *context)
{
    std::size_t *hit_count = static_cast<std::size_t *>(context);
    ++(*hit_count);
    return true;
}


int
main()
{
    // A volume this large has an ulp much greater than one.  Identical
    // rectangles used to leave PickSeeds' two indices at zero because
    // -(cover volume) - 1 rounded back to -(cover volume).
    const double rmin[3] = {
	-2251799813685247.8,
	-2251799813685330.0,
	-2251799813685213.0
    };
    const double rmax[3] = {
	2251799813685311.0,
	2251799813685229.0,
	2251799813685346.0
    };

    RTree<std::size_t, double, 3> tree;
    for (std::size_t i = 0; i < 9; ++i)
	tree.Insert(rmin, rmax, i);

    if (tree.Count() != 9)
	return 1;

    std::size_t hit_count = 0;
    const int found = tree.Search(rmin, rmax, count_hit, &hit_count);
    if (found != 9 || hit_count != 9)
	return 2;

    return 0;
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
