/*                   G L T F _ S C E N E . H
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
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
 *
 */

#ifndef BRLCAD_GLTF_SCENE_H
#define BRLCAD_GLTF_SCENE_H

#include "common.h"

#include <vector>

#include "bu/mime_runtime.h"
#include "gcv/runtime.h"
#include "vmath.h"
#include "tiny_gltf_v3.h"

struct rt_wdb;

namespace brlcad_gltf {

const bu_mime_capability *reader_capabilities(size_t &count);

bool profile_valid(const tg3_model &model, const char *source_path,
    const char *selected_type = nullptr, const bu_mime_traits *assertions = nullptr, gcv_context *context = nullptr);

/* Borrowed entries with static strings, describing selected scene features
 * the shared geometry converter does not reproduce. */
std::vector<gcv_conversion_loss> scene_losses(const tg3_model &model);

bool convert_scene(struct rt_wdb *database, const tg3_model &model, fastf_t coordinate_scale);

} // namespace brlcad_gltf

#endif

/*
 * Local Variables:
 * mode: C++
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
