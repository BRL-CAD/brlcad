/*              M A T E R I A L S . H
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
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
#ifndef ART_MATERIALS_H
#define ART_MATERIALS_H

#include <iosfwd>

namespace renderer { class Assembly; }
namespace foundation { class SearchPaths; }
struct db_i;
struct db_full_path;
struct rt_comb_internal;

/** Create a region material from its resolved sRGB color and instance assignment.
 * If report is supplied, describe the resolved assignment and validated shader
 * graph instead of adding a renderer material.  No shader execution is performed.
 * Return false for an invalid explicit assignment or unusable shader.
 */
bool art_add_material(renderer::Assembly &assembly, const char *name,
    const float srgb[3], struct db_i *db, const struct db_full_path *path,
    const struct rt_comb_internal *comb, const foundation::SearchPaths &search_paths,
    std::ostream *report);

#endif
