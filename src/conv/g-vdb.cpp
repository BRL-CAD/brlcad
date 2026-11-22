/*                           G - V D B . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
#include "common.h"

#include <cmath>
#include <limits>

#include <openvdb/openvdb.h>

#include "bu/app.h"
#include "bu/getopt.h"
#include "analyze.h"


struct voxel_fill {
    openvdb::FloatGrid::Accessor &accessor;
    double threshold;
    openvdb::Coord coord;
    double fill = 0.0;

    void store() const
    {
	if (fill > 0.0 && fill >= threshold)
	    accessor.setValue(coord, 1.0f);
    }
};


static void
accumulate_fill(void *data, int x, int y, int z, const char *region, fastf_t fill)
{
    if (!region)
	return;

    auto &sample = *static_cast<struct voxel_fill *>(data);
    const openvdb::Coord coord(x, y, z);
    // The sampler emits each region's share of a voxel before advancing.
    if (coord != sample.coord) {
	sample.store();
	sample.coord = coord;
	sample.fill = 0.0;
    }
    sample.fill += fill;
}


int
main(int argc, char **argv)
{
    static const char *usage = "[-s \"dx dy dz\"] [-d n] [-t f] model.g objects...\n";
    vect_t voxel_size = {1.0, 1.0, 1.0};
    double threshold = 0.5;
    int detail = 4;
    int opt;

    char title[1024] = {0};

    bu_setprogname(argv[0]);

    /* Check for command-line arguments.  Make sure we have at least a
     * geometry file and one geometry object on the command line.
     */
    if (argc < 3) {
	bu_exit(1, "Usage: %s %s", argv[0], usage);
    }

    bu_optind = 1;
    while ((opt = bu_getopt(argc, (char * const *)argv, (const char *)"s:d:t:")) != -1) {
	double scan[3];

	switch (opt) {
	    case 's':
		if (sscanf(bu_optarg, "%lf %lf %lf",
			   &scan[0],
			   &scan[1],
			   &scan[2]) != 3) {
		    bu_exit(1, "Usage: %s %s", argv[0], usage);
		} else {
		    /* convert from double to fastf_t */
		    VMOVE(voxel_size, scan);
		}
		break;

	    case 'd':
		if (sscanf(bu_optarg, "%d", &detail) != 1) {
		    bu_exit(1, "Usage: %s %s", argv[0], usage);
		}
		break;

	    case 't':
		if (sscanf(bu_optarg, "%lf", &threshold) != 1) {
		    bu_exit(1, "Usage: %s %s", argv[0], usage);
		}
		break;

	    default:
		bu_exit(1, "Usage: %s %s", argv[0], usage);
	}
    }
    argc -= bu_optind;
    argv += bu_optind;

    if (argc < 2)
	bu_exit(1, "Usage: %s %s", bu_getprogname(), usage);
    for (double size : voxel_size) {
	if (!std::isfinite(size) || size <= 0.0)
	    bu_exit(1, "Voxel sizes must be finite and positive\n");
    }
    if (detail <= 0 || !std::isfinite(threshold) || threshold < 0.0 || threshold > 1.0)
	bu_exit(1, "Detail must be positive and fill threshold must be in [0, 1]\n");

    struct rt_i *rtip = rt_dirbuild(argv[0], title, sizeof(title));
    if (rtip == RTI_NULL)
	bu_exit(2, "Building the database directory for [%s] FAILED\n", argv[0]);
    for (int i = 1; i < argc; i++) {
	if (rt_gettree(rtip, argv[i]) < 0) {
	    rt_i_destroy(rtip);
	    bu_exit(2, "Loading the geometry for [%s] FAILED\n", argv[i]);
	}
    }
    rt_prep_parallel(rtip, 1);

    for (size_t axis = 0; axis < ELEMENTS_PER_POINT; axis++) {
	const double cells = (rtip->mdl_max[axis] - rtip->mdl_min[axis]) / voxel_size[axis];
	if (!std::isfinite(cells) || cells <= 0.0 || cells >= std::numeric_limits<int>::max()) {
	    rt_i_destroy(rtip);
	    bu_exit(2, "Geometry bounds cannot be sampled at the requested voxel size\n");
	}
    }

    openvdb::initialize();
    auto grid = openvdb::FloatGrid::create(0.0f);
    auto transform = openvdb::math::Transform::createLinearTransform();
    transform->postScale(openvdb::Vec3d(voxel_size[X], voxel_size[Y], voxel_size[Z]));
    transform->postTranslate(openvdb::Vec3d(
	rtip->mdl_min[X] + 0.5 * voxel_size[X],
	rtip->mdl_min[Y] + 0.5 * voxel_size[Y],
	rtip->mdl_min[Z] + 0.5 * voxel_size[Z]));
    grid->setTransform(transform);

    {
	auto accessor = grid->getAccessor();
	struct voxel_fill sample{accessor, threshold, openvdb::Coord(0, 0, 0)};
	voxelize(rtip, voxel_size, detail, accumulate_fill, &sample);
	sample.store();
    }
    rt_i_destroy(rtip);

    grid->pruneGrid();
    grid->setGridClass(openvdb::GRID_FOG_VOLUME);
    grid->setName("default");

    try {
	openvdb::io::File file("file.vdb");
	file.write(openvdb::GridPtrVec{grid});
    } catch (const openvdb::Exception &error) {
	bu_exit(3, "Could not write file.vdb: %s\n", error.what());
    }

    return 0;
}


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8

