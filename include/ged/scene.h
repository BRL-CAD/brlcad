/*                         S C E N E . H
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License version 2.1.
 */
/** @file ged/scene.h
 *
 * Renderer-neutral semantic scene operations.
 */

#ifndef GED_SCENE_H
#define GED_SCENE_H

#include "ged/defines.h"
#include "ged/scene_types.h"

__BEGIN_DECLS

struct ged_scene_result;

GED_EXPORT void ged_scene_style_init(struct ged_scene_style *style);
GED_EXPORT void ged_scene_realization_policy_init(
    struct ged_scene_realization_policy *policy);
GED_EXPORT void ged_scene_draw_request_init(
    struct ged_scene_draw_request *request);
GED_EXPORT void ged_scene_erase_request_init(
    struct ged_scene_erase_request *request);
GED_EXPORT void ged_scene_redraw_request_init(
    struct ged_scene_redraw_request *request);

GED_EXPORT struct ged_scene_result *ged_scene_result_create(void);
GED_EXPORT void ged_scene_result_clear(struct ged_scene_result *result);
GED_EXPORT void ged_scene_result_destroy(struct ged_scene_result *result);
GED_EXPORT enum ged_scene_status ged_scene_result_status(
    const struct ged_scene_result *result);
GED_EXPORT int ged_scene_result_changed(
    const struct ged_scene_result *result);
GED_EXPORT size_t ged_scene_result_path_count(
    const struct ged_scene_result *result);
GED_EXPORT const char *ged_scene_result_path_at(
    const struct ged_scene_result *result, size_t index);
GED_EXPORT size_t ged_scene_result_group_count(
    const struct ged_scene_result *result);
GED_EXPORT size_t ged_scene_result_shape_count(
    const struct ged_scene_result *result);
GED_EXPORT size_t ged_scene_result_conflict_count(
    const struct ged_scene_result *result);
GED_EXPORT uint64_t ged_scene_result_revision_before(
    const struct ged_scene_result *result);
GED_EXPORT uint64_t ged_scene_result_revision_after(
    const struct ged_scene_result *result);
GED_EXPORT const char *ged_scene_result_diagnostic(
    const struct ged_scene_result *result);

GED_EXPORT enum ged_scene_status ged_scene_draw(
    struct ged *gedp, const struct ged_scene_draw_request *request,
    struct ged_scene_result *result);
GED_EXPORT enum ged_scene_status ged_scene_erase(
    struct ged *gedp, const struct ged_scene_erase_request *request,
    struct ged_scene_result *result);
GED_EXPORT enum ged_scene_status ged_scene_redraw(
    struct ged *gedp, const struct ged_scene_redraw_request *request,
    struct ged_scene_result *result);

GED_EXPORT uint64_t ged_scene_revision(const struct ged *gedp);
GED_EXPORT int ged_scene_available(const struct ged *gedp);
GED_EXPORT enum ged_scene_path_state ged_scene_path_state_get(
    struct ged *gedp, ged_view_context *view, const char *path,
    enum ged_scene_draw_mode mode);
GED_EXPORT enum ged_scene_path_state ged_scene_path_state_ids(
    struct ged *gedp, ged_view_context *view,
    const unsigned long long *ids, size_t count,
    enum ged_scene_draw_mode mode);

GED_EXPORT enum ged_scene_delta_kind ged_scene_delta_kind(
    const struct ged_scene_delta *delta);
GED_EXPORT ged_view_context *ged_scene_delta_view(
    const struct ged_scene_delta *delta);
GED_EXPORT size_t ged_scene_delta_path_count(
    const struct ged_scene_delta *delta);
GED_EXPORT const char *ged_scene_delta_path_at(
    const struct ged_scene_delta *delta, size_t index);
GED_EXPORT int ged_scene_delta_affects_all(
    const struct ged_scene_delta *delta);
GED_EXPORT size_t ged_scene_delta_group_count(
    const struct ged_scene_delta *delta);
GED_EXPORT size_t ged_scene_delta_shape_count(
    const struct ged_scene_delta *delta);
GED_EXPORT int ged_scene_delta_is_presentation_only(
    const struct ged_scene_delta *delta);
GED_EXPORT uint64_t ged_scene_delta_revision_before(
    const struct ged_scene_delta *delta);
GED_EXPORT uint64_t ged_scene_delta_revision_after(
    const struct ged_scene_delta *delta);
GED_EXPORT const char *ged_scene_delta_diagnostic(
    const struct ged_scene_delta *delta);

GED_EXPORT ged_scene_observer_token ged_scene_observer_add(
    struct ged *gedp, ged_scene_observer_func_t callback,
    void *client_data);
GED_EXPORT int ged_scene_observer_remove(
    struct ged *gedp, ged_scene_observer_token token);

__END_DECLS

#endif /* GED_SCENE_H */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
