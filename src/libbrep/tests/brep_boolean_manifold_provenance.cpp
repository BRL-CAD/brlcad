/*       B R E P _ B O O L E A N _ M A N I F O L D _ P R O V E N A N C E . C P P
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 */

#include "common.h"

#include <cstdint>
#include <cstdio>
#include <set>

#include <manifold/manifold.h>

#include "bu/app.h"


static manifold::Manifold
tagged_cube(uint32_t original_id, uint32_t face_id_base,
    double x, double y, double z, std::set<uint32_t> &face_ids)
{
    manifold::MeshGL64 mesh = manifold::Manifold::Cube().GetMeshGL64();
    for (size_t vertex = 0; vertex < mesh.NumVert(); ++vertex) {
	mesh.vertProperties[vertex * mesh.numProp] += x;
	mesh.vertProperties[vertex * mesh.numProp + 1] += y;
	mesh.vertProperties[vertex * mesh.numProp + 2] += z;
    }

    if (mesh.runOriginalID.size() != 1 ||
	mesh.faceID.size() != mesh.NumTri())
    {
	return manifold::Manifold();
    }
    mesh.runOriginalID[0] = original_id;
    for (size_t triangle = 0; triangle < mesh.NumTri(); ++triangle) {
	mesh.faceID[triangle] += face_id_base;
	face_ids.insert(mesh.faceID[triangle]);
    }
    return manifold::Manifold(mesh);
}


static bool
provenance_is_complete(const manifold::Manifold &result,
    uint32_t first_original_id, uint32_t second_original_id,
    const std::set<uint32_t> &first_face_ids,
    const std::set<uint32_t> &second_face_ids,
    bool first_backside, bool second_backside, const char *operation)
{
    if (result.Status() != manifold::Manifold::Error::NoError ||
	result.IsEmpty())
    {
	std::fprintf(stderr, "%s did not produce a valid nonempty manifold\n",
	    operation);
	return false;
    }

    const manifold::MeshGL64 mesh = result.GetMeshGL64();
    if (mesh.runIndex.size() != mesh.runOriginalID.size() + 1 ||
	mesh.faceID.size() != mesh.NumTri() || mesh.runIndex.empty() ||
	mesh.runIndex.front() != 0 || mesh.runIndex.back() !=
	mesh.triVerts.size() ||
	mesh.runFlags.size() != mesh.runOriginalID.size())
    {
	std::fprintf(stderr, "%s returned incomplete provenance arrays\n",
	    operation);
	return false;
    }

    bool saw_first = false;
    bool saw_second = false;
    for (size_t run = 0; run < mesh.runOriginalID.size(); ++run) {
	const uint32_t original_id = mesh.runOriginalID[run];
	const bool first = original_id == first_original_id;
	const bool second = original_id == second_original_id;
	if (!first && !second) {
	    std::fprintf(stderr, "%s introduced unknown original ID %u\n",
		operation, original_id);
	    return false;
	}
	if (mesh.runIndex[run] % 3 != 0 || mesh.runIndex[run + 1] % 3 != 0 ||
	    mesh.runIndex[run] > mesh.runIndex[run + 1])
	{
	    std::fprintf(stderr, "%s returned an invalid triangle run\n",
		operation);
	    return false;
	}
	const bool expected_backside = first ? first_backside : second_backside;
	if (mesh.runIndex[run] != mesh.runIndex[run + 1] &&
	    mesh.Backside(run) != expected_backside)
	{
	    std::fprintf(stderr,
		"%s returned the wrong orientation for original ID %u\n",
		operation, original_id);
	    return false;
	}

	const std::set<uint32_t> &face_ids = first ? first_face_ids :
	    second_face_ids;
	for (size_t triangle = mesh.runIndex[run] / 3;
	     triangle < mesh.runIndex[run + 1] / 3; ++triangle)
	{
	    const uint32_t face_id = mesh.faceID[triangle];
	    if (face_ids.find(face_id) == face_ids.end()) {
		std::fprintf(stderr,
		    "%s associated original ID %u with foreign face ID %u\n",
		    operation, original_id, face_id);
		return false;
	    }
	}
	saw_first = saw_first || first;
	saw_second = saw_second || second;
    }

    if (!saw_first || !saw_second) {
	std::fprintf(stderr, "%s did not retain faces from both operands\n",
	    operation);
	return false;
    }
    return true;
}


int
main(int, char *argv[])
{
    bu_setprogname(argv[0]);
    const uint32_t first_original_id = manifold::Manifold::ReserveIDs(2);
    const uint32_t second_original_id = first_original_id + 1;
    const uint32_t first_face_base = 100;
    const uint32_t second_face_base = 200;
    std::set<uint32_t> first_face_ids;
    std::set<uint32_t> second_face_ids;

    const manifold::Manifold first = tagged_cube(first_original_id,
	first_face_base, 0.0, 0.0, 0.0, first_face_ids);
    const manifold::Manifold second = tagged_cube(second_original_id,
	second_face_base, 0.35, 0.25, 0.15, second_face_ids);
    if (first.Status() != manifold::Manifold::Error::NoError ||
	second.Status() != manifold::Manifold::Error::NoError)
    {
	std::fprintf(stderr, "unable to construct tagged Manifold operands\n");
	return 1;
    }

    const manifold::Manifold results[] = {
	first + second,
	first ^ second,
	first - second
    };
    const char *operations[] = {"union", "intersection", "difference"};
    const bool first_backside[] = {false, false, false};
    const bool second_backside[] = {false, false, true};
    for (size_t result = 0; result < sizeof(results) / sizeof(results[0]);
	 ++result)
    {
	if (!provenance_is_complete(results[result], first_original_id,
		second_original_id, first_face_ids, second_face_ids,
		first_backside[result], second_backside[result],
		operations[result]))
	{
	    return 1;
	}
	const manifold::Manifold simplified = results[result].Simplify();
	if (!provenance_is_complete(simplified, first_original_id,
		second_original_id, first_face_ids, second_face_ids,
		first_backside[result], second_backside[result],
		operations[result]))
	{
	    std::fprintf(stderr, "%s lost provenance during simplification\n",
		operations[result]);
	    return 1;
	}
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
