/*                      B I N D I N G S . C P P
 * BRL-CAD
 *
 * Copyright (c) 2021-2026 United States Government as represented by
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
/** @file bindings.cpp
 *
 * Brief description
 *
 */

#include "common.h"
#include <QtGlobal>
#include <chrono>
#include <unordered_map>

extern "C" {
#include "bn/str.h"
#include "bv/defines.h"
#include "bv/util.h"
}

#include "qtcad/defines.h"
#include "bindings.h"

extern "C" {
#include "bu/binding.h"
#include "bu/file.h"
#include "bu/log.h"
}

typedef void (*bounds_update_t)(struct bview *);
static std::unordered_map<struct bview *, bounds_update_t> drag_bounds_updates;
static std::unordered_map<struct bview *, long long> drag_update_ts;
static const long long drag_update_interval_ms = 16;

static void
suspend_drag_bounds_update(struct bview *v)
{
    if (!v || !v->gv_bounds_update)
	return;
    if (drag_bounds_updates.find(v) == drag_bounds_updates.end()) {
	drag_bounds_updates[v] = v->gv_bounds_update;
	v->gv_bounds_update = NULL;
    }
}

static void
restore_drag_bounds_update(struct bview *v, int refresh_bounds)
{
    if (!v)
	return;
    std::unordered_map<struct bview *, bounds_update_t>::iterator it = drag_bounds_updates.find(v);
    if (it == drag_bounds_updates.end())
	return;
    v->gv_bounds_update = it->second;
    if (refresh_bounds && v->gv_bounds_update)
	(*(v->gv_bounds_update))(v);
    drag_bounds_updates.erase(it);
    drag_update_ts.erase(v);
}

static int bindings_loaded = 0;

static void qtcad_action_toggle_adc(const char *, void *, void *event_data) {
    struct bview *v = (struct bview *)event_data;
    if (v) v->gv_s->gv_adc.draw = !v->gv_s->gv_adc.draw;
}
static void qtcad_action_toggle_model_axes(const char *, void *, void *event_data) {
    struct bview *v = (struct bview *)event_data;
    if (v) v->gv_s->gv_model_axes.draw = !v->gv_s->gv_model_axes.draw;
}
static void qtcad_action_toggle_view_axes(const char *, void *, void *event_data) {
    struct bview *v = (struct bview *)event_data;
    if (v) v->gv_s->gv_view_axes.draw = !v->gv_s->gv_view_axes.draw;
}

static void qtcad_action_view_set(const char *, void *registered_data, void *event_data) {
    struct bview *v = (struct bview *)event_data;
    const char *aet_str = (const char *)registered_data;
    if (v && aet_str) {
        bn_decode_vect(v->gv_aet, aet_str);
        bv_mat_aet(v);
        bv_update(v);
    }
}

static void qtcad_action_mouse_release(const char *event_json, void *, void *event_data) {
    struct bview *v = (struct bview *)event_data;
    if (!v) return;
    
    int cx = 0, cy = 0, mode = 0, dx = 1, dy = 1;
    char *p;
    if ((p = strstr(event_json, "\"cx\":"))) sscanf(p + 5, "%d", &cx);
    if ((p = strstr(event_json, "\"cy\":"))) sscanf(p + 5, "%d", &cy);
    if ((p = strstr(event_json, "\"mode\":"))) sscanf(p + 7, "%d", &mode);
    
    int button = 0;
    if ((p = strstr(event_json, "\"button\":"))) sscanf(p + 9, "%d", &button);

    unsigned long long view_flags = BV_IDLE;
    if (button == 0) { // Left
        if (mode != BV_CENTER) { view_flags = BV_SCALE; dx = 10; dy = 5; }
        else { view_flags = BV_CENTER; dx = cx; dy = cy; }
    } else if (button == 2) { // Right
        if (mode == BV_CENTER) return;
        view_flags = BV_SCALE; dx = 1; dy = 2;
    } else if (button == 1) { // Middle
        view_flags = BV_CENTER; dx = cx; dy = cy;
    } else {
        return;
    }
    
    point_t keypt = VINIT_ZERO;
    bv_adjust(v, dx, dy, keypt, 0, view_flags);
}

static void qtcad_action_adjust_view(const char *event_json, void *registered_data, void *event_data) {
    struct bview *v = (struct bview *)event_data;
    if (!v) return;
    
    int dx = 0, dy = 0, mode = 0;
    int view_flags = (int)(intptr_t)registered_data;
    char *p;
    
    if ((p = strstr(event_json, "\"dx\":"))) sscanf(p + 5, "%d", &dx);
    if ((p = strstr(event_json, "\"dy\":"))) sscanf(p + 5, "%d", &dy);
    if ((p = strstr(event_json, "\"mode\":"))) sscanf(p + 7, "%d", &mode);

    if (view_flags == 0) { // dynamic fallback
        view_flags = mode;
        if (mode == BV_CENTER) view_flags = BV_SCALE;
    }

    if (view_flags == BV_SCALE) {
        int mdelta = (abs(dx) > abs(dy)) ? dx : -dy;
        int f = (int)(2*100*(double)abs(mdelta)/(double)v->gv_height);
        if (mdelta > 0) { dy = 101 + f; dx = 100; }
        else { dy = 99 - f; dx = 100; }
    }

    point_t center;
    MAT_DELTAS_GET_NEG(center, v->gv_center);

    if (view_flags & (BV_ROT | BV_TRANS | BV_SCALE))
        suspend_drag_bounds_update(v);

    bv_adjust(v, dx, dy, center, 0, view_flags);
}

static void qtcad_action_wheel_scale(const char *event_json, void *, void *event_data) {
    struct bview *v = (struct bview *)event_data;
    if (!v) return;
    
    int delta = 0;
    char *p;
    if ((p = strstr(event_json, "\"delta\":"))) sscanf(p + 8, "%d", &delta);

    int dx = 100 + delta;
    int dy = 100;
    point_t origin = VINIT_ZERO;
    bv_adjust(v, dx, dy, origin, 0, BV_SCALE);
}

extern "C" QCAD_EXPORT int qtcad_load_bindings(const char *json_path) {
    if (bu_binding_load(json_path) < 0) {
        bu_log("libqtcad: Failed to load bindings from %s\n", json_path);
        return -1;
    }

    if (!bu_binding_action_is_bound("dm", "view_toggle_adc") ||
        !bu_binding_action_is_bound("dm", "view_2")) {
        bu_log("libqtcad Validation Failed: Provided bindings missing required standard actions.\n");
        return -1;
    }
    return 0;
}

static void qtcad_init_bindings(void) {
    if (bindings_loaded) return;
    bindings_loaded = 1;

    bu_binding_register_action("view_toggle_adc", qtcad_action_toggle_adc, NULL);
    bu_binding_register_action("view_toggle_model_axes", qtcad_action_toggle_model_axes, NULL);
    bu_binding_register_action("view_toggle_view_axes", qtcad_action_toggle_view_axes, NULL);
    
    bu_binding_register_action("view_2", qtcad_action_view_set, (void*)"35 -25 0");
    bu_binding_register_action("view_3", qtcad_action_view_set, (void*)"35 25 0");
    bu_binding_register_action("view_4", qtcad_action_view_set, (void*)"45 45 0");
    bu_binding_register_action("view_5", qtcad_action_view_set, (void*)"145 25 0");
    bu_binding_register_action("view_6", qtcad_action_view_set, (void*)"215 25 0");
    bu_binding_register_action("view_7", qtcad_action_view_set, (void*)"325 25 0");
    bu_binding_register_action("view_front", qtcad_action_view_set, (void*)"0 0 0");
    bu_binding_register_action("view_top", qtcad_action_view_set, (void*)"270 90 0");
    bu_binding_register_action("view_bottom", qtcad_action_view_set, (void*)"270 -90 0");
    bu_binding_register_action("view_left", qtcad_action_view_set, (void*)"90 0 0");
    bu_binding_register_action("view_rear", qtcad_action_view_set, (void*)"180 0 0");
    bu_binding_register_action("view_right", qtcad_action_view_set, (void*)"270 0 0");

    bu_binding_register_action("view_mouse_release_left", qtcad_action_mouse_release, NULL);
    bu_binding_register_action("view_mouse_release_right", qtcad_action_mouse_release, NULL);
    bu_binding_register_action("view_mouse_release_middle", qtcad_action_mouse_release, NULL);

    bu_binding_register_action("view_mouse_drag_rotate", qtcad_action_adjust_view, (void*)(intptr_t)BV_ROT);
    bu_binding_register_action("view_mouse_drag_translate", qtcad_action_adjust_view, (void*)(intptr_t)BV_TRANS);
    bu_binding_register_action("view_mouse_drag_scale", qtcad_action_adjust_view, (void*)(intptr_t)BV_SCALE);
    bu_binding_register_action("view_mouse_drag_adjust", qtcad_action_adjust_view, (void*)(intptr_t)0);
    bu_binding_register_action("view_mouse_wheel_scale", qtcad_action_wheel_scale, NULL);
}

static void append_modifiers(struct bu_vls *vls, QInputEvent *e) {
    if (!e->modifiers()) return;
    bu_vls_printf(vls, ", \"modifiers\": [");
    int added = 0;
    if (e->modifiers().testFlag(Qt::ShiftModifier)) { bu_vls_printf(vls, "\"Shift\""); added=1; }
    if (e->modifiers().testFlag(Qt::ControlModifier)) { if(added) bu_vls_printf(vls, ", "); bu_vls_printf(vls, "\"Control\""); added=1; }
    if (e->modifiers().testFlag(Qt::AltModifier)) { if(added) bu_vls_printf(vls, ", "); bu_vls_printf(vls, "\"Alt\""); }
    bu_vls_printf(vls, "]");
}

int CADkeyPressEvent(struct bview *v, int UNUSED(x_prev), int UNUSED(y_prev), QKeyEvent *k)
{
    QTCAD_EVENT("keyPress", 1);
    if (!v) return 0;
    qtcad_init_bindings();

    struct bu_vls ev_json = BU_VLS_INIT_ZERO;
    char key_str[8] = {0};
    if (k->key() >= 0x20 && k->key() <= 0x7E) {
        key_str[0] = (char)k->key();
    } else {
        QString kstr = QKeySequence(k->key()).toString();
        strncpy(key_str, kstr.toStdString().c_str(), 7);
    }
    
    bu_vls_sprintf(&ev_json, "{\"type\": \"key-press\", \"key\": \"%s\"", key_str);
    append_modifiers(&ev_json, k);
    bu_vls_printf(&ev_json, "}");
    
    int handled = bu_binding_process_event("dm", bu_vls_cstr(&ev_json), v);
    bu_vls_free(&ev_json);
    return handled;
}

int CADmousePressEvent(struct bview *v, int UNUSED(x_prev), int UNUSED(y_prev), QMouseEvent *e)
{
    QTCAD_EVENT("mousePress", 1);
    if (!v) return 0;
    qtcad_init_bindings();

    int button = -1;
    if (e->buttons().testFlag(Qt::LeftButton)) button = 0;
    else if (e->buttons().testFlag(Qt::MiddleButton)) button = 1;
    else if (e->buttons().testFlag(Qt::RightButton)) button = 2;
    if (button < 0) return 0;

    struct bu_vls ev_json = BU_VLS_INIT_ZERO;
    bu_vls_sprintf(&ev_json, "{\"type\": \"mouse-press\", \"button\": %d", button);
    append_modifiers(&ev_json, e);
    bu_vls_printf(&ev_json, "}");
    
    int handled = bu_binding_process_event("dm", bu_vls_cstr(&ev_json), v);
    bu_vls_free(&ev_json);
    return handled;
}

int CADmouseReleaseEvent(struct bview *v, double x_press, double y_press, int UNUSED(x_prev), int UNUSED(y_prev), QMouseEvent *e, int mode)
{
    QTCAD_EVENT("mouseRelease", 1);
    if (!v) return 0;
    qtcad_init_bindings();
    restore_drag_bounds_update(v, 1);

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    double cx = e->x();
    double cy = e->y();
#else
    double cx = e->position().x();
    double cy = e->position().y();
#endif

    if ((fabs(cx - x_press) > 10) || (fabs(cy - y_press) > 10)) return 0;

    int button = -1;
    if (e->button() == Qt::LeftButton) button = 0;
    else if (e->button() == Qt::MiddleButton) button = 1;
    else if (e->button() == Qt::RightButton) button = 2;
    if (button < 0) return 0;

    struct bu_vls ev_json = BU_VLS_INIT_ZERO;
    bu_vls_sprintf(&ev_json, "{\"type\": \"mouse-release\", \"button\": %d, \"cx\": %d, \"cy\": %d, \"mode\": %d", button, (int)cx, (int)cy, mode);
    append_modifiers(&ev_json, e);
    bu_vls_printf(&ev_json, "}");
    
    int handled = bu_binding_process_event("dm", bu_vls_cstr(&ev_json), v);
    bu_vls_free(&ev_json);
    return handled;
}

int CADmouseMoveEvent(struct bview *v, int x_prev, int y_prev, QMouseEvent *e, int mode)
{
    QTCAD_EVENT("mouseMove", 2);
    if (!v) return 0;
    qtcad_init_bindings();

    if (x_prev == -INT_MAX) return 0;
    if (!e->buttons().testFlag(Qt::LeftButton)) return 0;

    long long now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
	std::chrono::steady_clock::now().time_since_epoch()).count();
    std::unordered_map<struct bview *, long long>::iterator ts_it = drag_update_ts.find(v);
    if (ts_it != drag_update_ts.end() && (now_ms - ts_it->second) < drag_update_interval_ms) return -1;
    drag_update_ts[v] = now_ms;

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    int dx = e->x() - x_prev;
    int dy = e->y() - y_prev;
#else
    int dx = e->position().x() - x_prev;
    int dy = e->position().y() - y_prev;
#endif

    struct bu_vls ev_json = BU_VLS_INIT_ZERO;
    bu_vls_sprintf(&ev_json, "{\"type\": \"mouse-move\", \"button\": 0, \"dx\": %d, \"dy\": %d, \"mode\": %d", dx, dy, mode);
    append_modifiers(&ev_json, e);
    bu_vls_printf(&ev_json, "}");
    
    int handled = bu_binding_process_event("dm", bu_vls_cstr(&ev_json), v);
    bu_vls_free(&ev_json);
    return handled;
}

int CADwheelEvent(struct bview *v, QWheelEvent *e)
{
    QTCAD_EVENT("mouseWheel", 1);
    if (!v) return 0;
    qtcad_init_bindings();

    QPoint delta = e->angleDelta();
    int mdelta = -1 * delta.y() / 8;

    struct bu_vls ev_json = BU_VLS_INIT_ZERO;
    bu_vls_sprintf(&ev_json, "{\"type\": \"mouse-wheel\", \"delta\": %d", mdelta);
    append_modifiers(&ev_json, e);
    bu_vls_printf(&ev_json, "}");
    
    int handled = bu_binding_process_event("dm", bu_vls_cstr(&ev_json), v);
    bu_vls_free(&ev_json);
    return handled;
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
