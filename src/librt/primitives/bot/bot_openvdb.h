/*                    B O T _ O P E N V D B . H
 * BRL-CAD
 *
 * Copyright (c) 2024-2026 United States Government as represented by
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
/** @file librt/primitives/bot/bot_openvdb.h
 *
 * Shared OpenVDB utilities for BoT processing.
 *
 * Provides four building blocks used by remesh, repair, and the
 * flood-fill exterior classifier:
 *
 *  rt_bot_to_openvdb_sdf()        — BoT triangle mesh → FloatGrid SDF
 *  rt_bot_from_openvdb_sdf()      — FloatGrid SDF → solid BoT
 *  rt_bot_openvdb_repair()        — OpenVDB level-set BoT repair
 *  rt_rtip_to_openvdb_occupancy() — raytrace an rt_i into a BoolGrid
 *
 * OpenVDB-specific declarations are guarded by BRLCAD_OPENVDB.  Translation
 * units using those declarations must be compiled with the OpenVDB headers
 * and ABI definitions.
 */

#ifndef LIBRT_BOT_OPENVDB_H
#define LIBRT_BOT_OPENVDB_H

#include "common.h"
#include "rt/defines.h"
#include "rt/geom.h"

RT_EXPORT int rt_bot_openvdb_available(void);

/**
 * Repair a non-manifold solid BoT using the OpenVDB level-set pipeline.
 *
 * @param bot        Source BoT (read-only).
 * @param voxel_size Voxel edge length in model units.  Pass <= 0 to
 *                   auto-size to bbox_diagonal / 100.
 * @param volume     Optional output for the absolute volume of the result.
 * @return           New rt_bot_internal on success, NULL on failure.
 *                   Caller owns the result and must free it via
 *                   rt_bot_internal_free() / BU_PUT().
 */
RT_EXPORT struct rt_bot_internal *rt_bot_openvdb_repair(struct rt_bot_internal *bot,
	                                                 double voxel_size,
	                                                 fastf_t *volume);

#ifdef BRLCAD_OPENVDB

#  include "raytrace.h"

/* Suppress warnings from OpenVDB and its transitive headers. */
#  if defined(__GNUC__)
#    pragma GCC diagnostic push
#    pragma GCC diagnostic ignored "-Wunused-parameter"
#    pragma GCC diagnostic ignored "-Wshadow"
#    pragma GCC diagnostic ignored "-Wpedantic"
#    pragma GCC diagnostic ignored "-Wignored-attributes"
#  endif

#  include <openvdb/openvdb.h>

#  if defined(__GNUC__)
#    pragma GCC diagnostic pop
#  endif

/**
 * Convert a BoT triangle mesh to an OpenVDB signed-distance field.
 *
 * The BoT must be roughly watertight for inside/outside to be
 * meaningful.  For severely broken meshes the signs near defects
 * may be wrong, but surface extraction will still produce a closed
 * manifold mesh.
 *
 * @param bot        Source BoT (read-only).
 * @param voxel_size Voxel edge length in model units.  Pass <= 0 to
 *                   auto-size to bbox_diagonal / 100.
 * @return           FloatGrid SDF, or an empty Ptr on failure.
 */
RT_EXPORT openvdb::FloatGrid::Ptr rt_bot_to_openvdb_sdf(struct rt_bot_internal *bot, double voxel_size);

/**
 * Extract a solid BoT from an OpenVDB signed-distance field.
 *
 * The output is always a closed 2-manifold (RT_BOT_SOLID, RT_BOT_CCW).
 * Quads produced by volumeToMesh are split into two triangles each.
 *
 * @param grid       Source SDF (e.g. from rt_bot_to_openvdb_sdf).
 * @param adaptivity Mesh simplification factor [0.0, 1.0].
 *                   0.0 = full voxel resolution; 1.0 = maximum merging.
 * @return           New rt_bot_internal on success, NULL on failure.
 *                   Caller owns the result and must free it via
 *                   rt_bot_internal_free() / BU_PUT().
 */
RT_EXPORT struct rt_bot_internal *rt_bot_from_openvdb_sdf(openvdb::FloatGrid::Ptr grid, double adaptivity);

/**
 * Voxelize a prepped rt_i into a solid-occupancy BoolGrid.
 *
 * Shoots rays in +X, +Y, +Z.  Active (true) voxels are cells whose
 * centre lies inside the solid geometry.  The grid is padded by one
 * cell on every side so that a BFS flood-fill seed can always start
 * outside the model.
 *
 * @param rtip       Prepped raytrace instance (rt_prep already called).
 * @param voxel_size Voxel edge length in model units (> 0).
 * @param nx,ny,nz   Output: grid dimensions including padding.
 * @return           BoolGrid with active voxels = solid interior cells.
 */
RT_EXPORT openvdb::BoolGrid::Ptr rt_rtip_to_openvdb_occupancy(struct rt_i *rtip,
						              double voxel_size,
						              int *nx, int *ny, int *nz);

#endif /* BRLCAD_OPENVDB */
#endif /* LIBRT_BOT_OPENVDB_H */

/*
 * Local Variables:
 * mode: C++
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
