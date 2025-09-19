/*                    B V _ P R I V A T E . h
 * BRL-CAD
 *
 * Copyright (c) 2020-2025 United States Government as represented by
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
/** @file bv_private.h
 *
 * Internal shared data structures
 *
 */

#include "common.h"
#include "bu/list.h"
#include "bu/ptbl.h"
#include "bv/defines.h"
#include <unordered_map>


/* --- Forward declarations --- */
struct bv_scene;
struct bv_scene_obj;

/* --- bview_new: Render/View Context --- */
struct bview_new {
    struct bu_vls           name;
    struct bv_scene        *scene;         /* Associated scene graph (not owned) */

    struct bview_camera     camera;        /* Camera parameters */
    struct bv_scene_obj    *camera_object; /* Optional: camera as scene object */

    struct bview_viewport   viewport;      /* Viewport/window info */

    struct bview_material   material;      /* View-level material/appearance (optional) */
    struct bview_appearance appearance;    /* Display/appearance (axes, grid, bg, etc.) */
    struct bview_overlay    overlay;       /* HUD/overlay settings */

    struct bview_pick_set   pick_set;      /* Selection/pick state */

    /* Redraw callback */
    bview_redraw_cb         redraw_cb;
    void                   *redraw_cb_data;

    /* Legacy bview sync (for migration only) */
    struct bview           *old_bview;
};

/* --- bv_scene: Scene Graph --- */
struct bv_scene {
    struct bv_scene_obj    *root;           /* Root group object (SoSeparator analogy) */
    struct bu_ptbl          objects;        /* All objects (flat list for fast lookup) */
    struct bv_scene_obj    *default_camera; /* Default camera object, if any */
    /* Add scene-level fields as needed */
};

/* --- bv_scene_obj: Scene Graph Object --- */
struct bv_scene_obj {
    enum bv_scene_obj_type  type;           /* Explicit node type (geometry, group, etc.) */
    struct bu_vls           name;           /* Unique or descriptive name */
    struct bv_scene_obj    *parent;         /* Parent in hierarchy (NULL if root) */
    struct bu_ptbl          children;       /* Children (scene object pointers) */

    /* Transform */
    mat_t                   transform;      /* Local transform */
    mat_t                   world_transform;/* Cached world transform (optional) */

    /* Geometry, Material, Style */
    void                   *geometry;       /* Geometry pointer (type-specific) */
    struct bview_material   material;       /* Appearance/material for this object */

    int                     visible;        /* Visibility flag */

    /* User data for custom extension, Coin3D mapping, etc. */
    void                   *user_data;

    /* Optional fields for LoD, selection, etc. */
    int                     lod_level;      /* Level of detail (if used) */
    int                     selected;       /* Selection flag (optional, for convenience) */
};



struct bview_set_internal {
    struct bu_ptbl views;
    struct bu_ptbl shared_db_objs;
    struct bu_ptbl shared_view_objs;

    struct bv_scene_obj  *free_scene_obj;
    struct bu_list vlfree;
};

struct bv_scene_obj_internal {
    std::unordered_map<struct bview *, struct bv_scene_obj *> vobjs;
};

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
