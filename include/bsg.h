/*                          B S G . H
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
/** @addtogroup libbsg
 *
 * @brief
 * Umbrella header for the BSG scene-graph library.
 *
 * Installed BSG headers define the public scene-graph, typed payload, render,
 * and backend adapter surface used by the modern drawing stack.  Private
 * storage, raw-node mutation helpers, and lifecycle internals remain confined
 * to libbsg implementation headers under src/libbsg.
 *
 * Headers that explicitly describe themselves as backward-compatibility
 * bridges are transitional: prefer their documented canonical replacements
 * for new code.
 */
/** @{ */
/* @file bsg.h */

#ifndef BSG_H
#define BSG_H

#include "bsg/adc.h"
#include "bsg/action.h"
#include "bsg/backend_adapter.h"
#include "bsg/backend_scene.h"
#include "bsg/camera.h"
#include "bsg/complexity.h"
#include "bsg/database_source.h"
#include "bsg/defines.h"
#include "bsg/draw_style.h"
#include "bsg/draw_intent.h"
#include "bsg/edit_preview.h"
#include "bsg/export.h"
#include "bsg/feature.h"
#include "bsg/field.h"
#include "bsg/geometry.h"
#include "bsg/group.h"
#include "bsg/hud.h"
#include "bsg/interaction.h"
#include "bsg/light.h"
#include "bsg/lod.h"
#include "bsg/material.h"
#include "bsg/measure.h"
#include "bsg/node.h"
#include "bsg/object.h"
#include "bsg/obol_node.h"
#include "bsg/overlay.h"
#include "bsg/payload.h"
#include "bsg/pick.h"
#include "bsg/polygon.h"
#include "bsg/render_item.h"
#include "bsg/scene_object.h"
#include "bsg/scene_set.h"
#include "bsg/selection.h"
#include "bsg/separator.h"
#include "bsg/sensor.h"
#include "bsg/snap.h"
#include "bsg/snap_action.h"
#include "bsg/state.h"
#include "bsg/switch.h"
#include "bsg/util.h"
#include "bsg/visibility.h"
#include "bsg/view_set.h"
#include "bsg/view_state.h"

#endif /* BSG_H */

/** @} */
/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
