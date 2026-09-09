/*                 A R R A N G E _ T E S T _ M A S K S . C P P
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 */

#include "common.h"

#include <cstdio>
#include <random>
#include <vector>

#include "../arrange_private.h"

using namespace arrange;

namespace {

int failures = 0;

#define CHECK(condition, message) do { \
    if (!(condition)) { \
        std::fprintf(stderr, "FAIL [%s:%d]: %s\n", __FILE__, __LINE__, message); \
        ++failures; \
    } \
} while (0)

bool
oracle(const Raster &allowed, const SearchState &state,
        const OrientedRaster &orientation, const Cell &position)
{
    for (int z = 0; z < orientation.collision.dims[Z]; ++z) {
        for (int y = 0; y < orientation.collision.dims[Y]; ++y) {
            for (int x = 0; x < orientation.collision.dims[X]; ++x) {
                if (!orientation.collision.get(x, y, z))
                    continue;
                if (!allowed.get(position.x + x, position.y + y,
                        position.z + z) ||
                        state.occupied.get(position.x + x, position.y + y,
                            position.z + z))
                    return false;
            }
        }
    }
    return true;
}

void
test_bit_rows()
{
    Raster raster({{130, 2, 2}});
    CHECK(raster.set(0, 0, 0), "set first bit");
    CHECK(raster.set(63, 0, 0), "set end of first word");
    CHECK(raster.set(64, 0, 0), "set start of second word");
    CHECK(raster.set(129, 1, 1), "set final partial-word bit");
    CHECK(!raster.set(64, 0, 0), "duplicate set is rejected");
    CHECK(raster.get(63, 0, 0), "read first-word boundary");
    CHECK(raster.get(64, 0, 0), "read second-word boundary");
    CHECK(raster.get(129, 1, 1), "read cropped-row boundary");
    CHECK(!raster.get(130, 1, 1), "out-of-bounds read is empty");
    CHECK(raster.count() == 4, "unique cell count");

    Raster allowed({{130, 1, 1}});
    for (int x = 0; x < 130; ++x)
        allowed.set(x, 0, 0);
    SearchState state;
    state.occupied = Raster(allowed.dims);
    state.occupied.set(64, 0, 0);
    OrientedRaster object;
    object.shape = Raster({{64, 1, 1}});
    object.shape.set(63, 0, 0);
    object.collision = object.shape;
    CHECK(!placement_feasible(allowed, state, object, {1, 0, 0}),
        "shifted row collision crosses a 64-bit word boundary");
    CHECK(placement_feasible(allowed, state, object, {0, 0, 0}),
        "shifted row accepts the adjacent non-collision");
}

void
test_orientations_and_clearance()
{
    CHECK(orientation_transforms(OrientationMode::ORTHOGONAL, Z).size() == 24,
        "24 right-handed orthogonal orientations");
    CHECK(orientation_transforms(OrientationMode::UPRIGHT, Z).size() == 4,
        "four upright quarter turns");

    Raster source({{2, 3, 1}});
    source.set(0, 0, 0);
    source.set(1, 2, 0);
    AxisTransform turn = orientation_transforms(OrientationMode::UPRIGHT, Z)[1];
    Raster oriented = orient_raster(source, turn);
    CHECK(oriented.dims[X] == 3 && oriented.dims[Y] == 2,
        "quarter turn permutes dimensions");
    CHECK(oriented.count() == source.count(), "orientation preserves occupancy");

    std::array<int, 3> pad;
    Raster dilated = dilate_raster(oriented, 1, Dimension::PLANAR, Z, pad);
    CHECK(pad[X] == 1 && pad[Y] == 1 && pad[Z] == 0,
        "2D clearance does not pad projection axis");
    CHECK(dilated.dims[X] == 5 && dilated.dims[Y] == 4 && dilated.dims[Z] == 1,
        "clearance expands the collision kernel");
}

void
test_direct_oracle()
{
    std::mt19937 generator(19790531u);
    std::bernoulli_distribution allowed_sample(0.8);
    std::bernoulli_distribution occupied_sample(0.12);
    std::bernoulli_distribution object_sample(0.35);

    for (int trial = 0; trial < 100; ++trial) {
        Raster allowed({{9, 7, 5}});
        SearchState state;
        state.occupied = Raster(allowed.dims);
        for (int z = 0; z < allowed.dims[Z]; ++z) {
            for (int y = 0; y < allowed.dims[Y]; ++y) {
                for (int x = 0; x < allowed.dims[X]; ++x) {
                    if (allowed_sample(generator))
                        allowed.set(x, y, z);
                    if (occupied_sample(generator))
                        state.occupied.set(x, y, z);
                }
            }
        }

        OrientedRaster orientation;
        orientation.shape = Raster({{3, 2, 2}});
        for (int z = 0; z < 2; ++z)
            for (int y = 0; y < 2; ++y)
                for (int x = 0; x < 3; ++x)
                    if (object_sample(generator))
                        orientation.shape.set(x, y, z);
        if (orientation.shape.count() == 0)
            orientation.shape.set(0, 0, 0);
        orientation.collision = orientation.shape;

        for (int z = 0; z <= allowed.dims[Z] - orientation.collision.dims[Z]; ++z)
            for (int y = 0; y <= allowed.dims[Y] - orientation.collision.dims[Y]; ++y)
                for (int x = 0; x <= allowed.dims[X] - orientation.collision.dims[X]; ++x) {
                    Cell position = {x, y, z};
                    CHECK(placement_feasible(allowed, state, orientation, position) ==
                        oracle(allowed, state, orientation, position),
                        "direct feasibility agrees with dense oracle");
                }
    }
}

void
test_deterministic_search()
{
    Raster allowed({{8, 6, 1}});
    for (int y = 0; y < 6; ++y)
        for (int x = 0; x < 8; ++x)
            allowed.set(x, y, 0);

    Raster part({{3, 2, 1}});
    for (int y = 0; y < 2; ++y)
        for (int x = 0; x < 3; ++x)
            part.set(x, y, 0);

    auto geometry = std::make_shared<ItemGeometry>();
    geometry->source = part;
    for (const AxisTransform &transform :
            orientation_transforms(OrientationMode::UPRIGHT, Z)) {
        OrientedRaster orientation;
        orientation.transform = transform;
        orientation.shape = orient_raster(part, transform);
        orientation.collision = dilate_raster(orientation.shape, 0,
            Dimension::PLANAR, Z, orientation.pad);
        geometry->orientations.push_back(std::move(orientation));
    }

    std::vector<Item> items;
    for (std::size_t i = 0; i < 5; ++i) {
        Item item;
        item.name = "part";
        item.source_index = i;
        item.geometry = geometry;
        item.value = part.count();
        items.push_back(std::move(item));
    }

    NestOptions options;
    options.dimension = Dimension::PLANAR;
    options.axis = Z;
    options.orientation = OrientationMode::UPRIGHT;
    options.quality = Quality::BALANCED;
    SearchState first = search_arrangement(allowed, items, options);
    SearchState second = search_arrangement(allowed, items, options);
    CHECK(first.placements.size() == 5, "all planar fixtures fit");
    CHECK(first.placements.size() == second.placements.size(),
        "repeat placement counts match");
    for (std::size_t i = 0; i < first.placements.size(); ++i) {
        const Placement &left = first.placements[i];
        const Placement &right = second.placements[i];
        CHECK(left.item_index == right.item_index &&
            left.orientation_index == right.orientation_index &&
            left.position.x == right.position.x &&
            left.position.y == right.position.y &&
            left.position.z == right.position.z,
            "repeat transforms are deterministic");
    }

    options.quality = Quality::THOROUGH;
    SearchState improved = search_arrangement(allowed, items, options);
    CHECK(!better_state(first, improved),
        "thorough search does not regress the balanced objective");
}

} // namespace

int
main()
{
    test_bit_rows();
    test_orientations_and_clearance();
    test_direct_oracle();
    test_deterministic_search();
    if (failures)
        std::fprintf(stderr, "%d arrange mask test(s) failed\n", failures);
    return failures ? 1 : 0;
}
