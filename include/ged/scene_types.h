/*                 S C E N E _ T Y P E S . H
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License version 2.1.
 */
/** @file ged/scene_types.h
 *
 * Renderer-neutral semantic scene request and observer types.
 */

#ifndef GED_SCENE_TYPES_H
#define GED_SCENE_TYPES_H

#include <stddef.h>
#include <stdint.h>

struct ged;
struct ged_scene_delta;
struct bview;

/* The legacy backend uses bview as its concrete view context. */
typedef struct bview ged_view_context;

enum ged_scene_status {
    GED_SCENE_OK = 0,
    GED_SCENE_INVALID,
    GED_SCENE_NOT_FOUND,
    GED_SCENE_ERROR
};

enum ged_scene_draw_mode {
    GED_SCENE_DRAW_DEFAULT = -1,
    GED_SCENE_DRAW_WIRE = 0,
    GED_SCENE_DRAW_SHADED_BOTS,
    GED_SCENE_DRAW_SHADED,
    GED_SCENE_DRAW_EVALUATED_WIRE,
    GED_SCENE_DRAW_HIDDEN_LINE,
    GED_SCENE_DRAW_EVALUATED_POINTS
};

enum ged_scene_path_state {
    GED_SCENE_PATH_NOT_DRAWN = 0,
    GED_SCENE_PATH_DRAWN,
    GED_SCENE_PATH_PARTIALLY_DRAWN
};

enum ged_scene_realization_mode {
    GED_SCENE_REALIZE_AUTO = 0,
    GED_SCENE_REALIZE_EAGER,
    GED_SCENE_REALIZE_PROGRESSIVE
};

enum ged_scene_style_flag {
    GED_SCENE_STYLE_SOLID_LINES_ONLY = 1u << 0,
    GED_SCENE_STYLE_NON_SUBTRACT_ONLY = 1u << 1
};

struct ged_scene_style {
    enum ged_scene_draw_mode draw_mode;
    double opacity;
    unsigned char color[3];
    int color_override;
    int line_width;
    int mixed_modes;
    unsigned flags;
};

struct ged_scene_realization_policy {
    enum ged_scene_realization_mode mode;
    int strict;
};

struct ged_scene_draw_request {
    ged_view_context *view;
    const char *const *paths;
    size_t path_count;
    struct ged_scene_style style;
    struct ged_scene_realization_policy realization;
    int autoview;
};

enum ged_scene_erase_match {
    GED_SCENE_ERASE_EXACT = 0,
    GED_SCENE_ERASE_SUBTREE
};

struct ged_scene_erase_request {
    ged_view_context *view;
    const char *path;
    enum ged_scene_erase_match match;
    enum ged_scene_draw_mode draw_mode;
};

/** Typed request to rebuild all or selected retained draw sources. */
struct ged_scene_redraw_request {
    ged_view_context *view;
    const char *const *paths;
    size_t path_count;
};

enum ged_scene_delta_kind {
    GED_SCENE_DELTA_UNKNOWN = 0,
    GED_SCENE_DELTA_DRAW,
    GED_SCENE_DELTA_ERASE,
    GED_SCENE_DELTA_REDRAW,
    GED_SCENE_DELTA_VISIBILITY,
    GED_SCENE_DELTA_HIGHLIGHT,
    GED_SCENE_DELTA_STYLE,
    GED_SCENE_DELTA_EDIT_SCOPE,
    GED_SCENE_DELTA_SOURCE,
    GED_SCENE_DELTA_CLEAR,
    GED_SCENE_DELTA_TEARDOWN
};

typedef uintptr_t ged_scene_observer_token;

typedef void (*ged_scene_observer_func_t)(
    struct ged *gedp,
    const struct ged_scene_delta *delta,
    void *client_data);

#endif /* GED_SCENE_TYPES_H */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
