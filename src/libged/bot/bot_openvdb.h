/*                    B O T _ O P E N V D B . H
 * BRL-CAD
 *
 * Copyright (c) 2024-2025 United States Government as represented by
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
/** @file libged/bot/bot_openvdb.h
 *
 * Shared OpenVDB utilities for the bot plugin.
 *
 * Provides three building blocks used by remesh, repair, and the
 * flood-fill exterior classifier:
 *
 *  bot_to_sdf()               — BoT triangle mesh → OpenVDB FloatGrid SDF
 *  sdf_to_bot()               — FloatGrid SDF → solid rt_bot_internal
 *  rt_rtip_to_occupancy_grid()— raytrace a prepped rt_i into a BoolGrid
 *
 * All declarations are guarded by OPENVDB_ABI_VERSION_NUMBER so that
 * the header is a no-op when compiled without OpenVDB.  Every translation
 * unit that includes this header is expected to be compiled with the
 * OpenVDB ABI defines (i.e., BRLCAD_ENABLE_OPENVDB is ON).
 */

#ifndef LIBGED_BOT_OPENVDB_H
#define LIBGED_BOT_OPENVDB_H

#include "common.h"

#ifdef BRLCAD_OPENVDB

#  include "rt/geom.h"
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
openvdb::FloatGrid::Ptr bot_to_sdf(struct rt_bot_internal *bot, double voxel_size);

/**
 * Extract a solid BoT from an OpenVDB signed-distance field.
 *
 * The output is always a closed 2-manifold (RT_BOT_SOLID, RT_BOT_CCW).
 * Quads produced by volumeToMesh are split into two triangles each.
 *
 * @param grid       Source SDF (e.g. from bot_to_sdf or meshToVolume).
 * @param adaptivity Mesh simplification factor [0.0, 1.0].
 *                   0.0 = full voxel resolution; 1.0 = maximum merging.
 * @return           New rt_bot_internal on success, NULL on failure.
 *                   Caller owns the result and must free it via
 *                   rt_bot_internal_free() / BU_PUT().
 */
struct rt_bot_internal *sdf_to_bot(openvdb::FloatGrid::Ptr grid, double adaptivity);

/**
 * Repair a non-manifold solid BoT using the OpenVDB level-set pipeline.
 *
 * Converts the mesh to a signed-distance field and extracts a
 * guaranteed-manifold mesh from the 0-isovalue surface.  The result is
 * validated for manifoldness and positive volume (negative volume would
 * indicate an inside-out reconstruction).
 *
 * @param bot        Source BoT (read-only).
 * @param voxel_size Voxel edge length in model units.  Pass <= 0 to
 *                   auto-size to bbox_diagonal / 100.
 * @return           New rt_bot_internal on success, NULL on failure.
 *                   Caller owns the result and must free it via
 *                   rt_bot_internal_free() / BU_PUT().
 */
struct rt_bot_internal *bot_openvdb_repair(struct rt_bot_internal *bot, double voxel_size);

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
openvdb::BoolGrid::Ptr rt_rtip_to_occupancy_grid(struct rt_i *rtip,
						 double voxel_size,
						 int *nx, int *ny, int *nz);

#endif /* BRLCAD_OPENVDB */
#endif /* LIBGED_BOT_OPENVDB_H */

/*
 * Local Variables:
 * mode: C++
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
