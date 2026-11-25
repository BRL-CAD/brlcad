/*                     T E S T _ S C E N E . C P P
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License version 2.1.
 */
/** @file test_scene.cpp */

#include "common.h"

#include <cstdio>
#include <memory>
#include <string>
#include <vector>

#include "bu/app.h"
#include "bu/file.h"
#include "ged.h"
#include "ged/history.h"
#include "ged/scene.h"
#include "wdb.h"

#include "../dbi.h"

static int failures = 0;

#define CHECK(_condition, _message) do { \
	if (!(_condition)) { \
	    bu_log("FAIL [%s:%d] %s\n", __FILE__, __LINE__, (_message)); \
	    failures++; \
	} \
    } while (0)

struct observer_state {
    size_t calls = 0;
    enum ged_scene_delta_kind kind = GED_SCENE_DELTA_UNKNOWN;
    std::string path;
    uint64_t revision_before = 0;
    uint64_t revision_after = 0;
};

static void
scene_changed(struct ged *UNUSED(gedp), const struct ged_scene_delta *delta,
	void *client_data)
{
    struct observer_state *state =
	static_cast<struct observer_state *>(client_data);
    state->calls++;
    state->kind = ged_scene_delta_kind(delta);
    const char *path = ged_scene_delta_path_at(delta, 0);
    state->path = path ? path : "";
    state->revision_before = ged_scene_delta_revision_before(delta);
    state->revision_after = ged_scene_delta_revision_after(delta);
}

static int
create_fixture(const char *path)
{
    struct rt_wdb *wdbp = wdb_fopen(path);
    if (!wdbp)
	return 0;
    point_t center = VINIT_ZERO;
    const int ret = mk_sph(wdbp, "sphere.s", center, 1.0);
    db_close(wdbp->dbip);
    return ret == 0;
}

static void
rebuild_legacy_display(struct ged *gedp, struct db_i *retired, void *UNUSED(data))
{
    ged_history_rebuild_legacy_display(gedp, retired);
}

static struct bv_scene_obj *
legacy_solid(struct ged *gedp)
{
    struct display_list *display = ged_dl(gedp);
    if (!display || BU_LIST_IS_EMPTY(&display->l))
	return nullptr;
    auto *item = BU_LIST_FIRST(display_list, &display->l);
    return BU_LIST_NON_EMPTY(&item->dl_head_scene_obj) ?
	BU_LIST_FIRST(bv_scene_obj, &item->dl_head_scene_obj) : nullptr;
}

static void
test_legacy_history(const char *path)
{
    const std::unique_ptr<struct ged, decltype(&ged_close)> owner(ged_open("db", path, 1), ged_close);
    auto *gedp = owner.get();
    CHECK(gedp, "open the legacy history fixture");
    if (!gedp)
	return;
    ged_history_set_replacement_handlers(gedp, nullptr, rebuild_legacy_display, nullptr);

    const char *draw[] = {"draw", "-C", "15/45/95", "sphere.s"};
    CHECK(ged_exec(gedp, 4, draw) == BRLCAD_OK, "draw the legacy primitive");
    auto *initial = legacy_solid(gedp);
    CHECK(initial, "initial scene contains a legacy solid");

    const char *enable[] = {"db", "history", "enable", "session"};
    const int enabled = ged_exec(gedp, 4, enable);
    if (enabled == BRLCAD_OK) {
	const char *kill[] = {"kill", "-f", "sphere.s", DB5_GLOBAL_OBJECT_NAME};
	CHECK(ged_exec(gedp, 4, kill) != BRLCAD_OK && legacy_solid(gedp) == initial &&
	    db_lookup(gedp->dbip, "sphere.s", LOOKUP_QUIET),
	    "failed staged deletion preserves the live legacy scene");
	const char *title[] = {"title", "History model with a legacy scene"};
	CHECK(ged_exec(gedp, 2, title) == BRLCAD_OK, "publish with legacy geometry displayed");
	auto *solid = legacy_solid(gedp);
	CHECK(solid && solid->s_os->color_override && solid->s_color[0] == 15 && solid->s_color[2] == 95,
	    "publication preserves explicit wireframe color");
	if (solid) {
	    auto *data = static_cast<struct ged_bv_data *>(solid->s_u_data);
	    CHECK(data && DB_FULL_PATH_CUR_DIR(&data->s_fullpath) == db_lookup(gedp->dbip, "sphere.s", LOOKUP_QUIET),
		"the legacy display path belongs to the new database");
	}
	const char *undo[] = {"db", "undo"};
	CHECK(ged_exec(gedp, 2, undo) == BRLCAD_OK && legacy_solid(gedp),
	    "undo retains the surviving legacy draw path");
    } else {
	CHECK(strstr(bu_vls_cstr(gedp->ged_result_str), "Atomic DB5 replacement is unavailable"),
	    "enable session history while legacy geometry is displayed");
    }
    const char *zap[] = {"zap"};
    CHECK(ged_exec(gedp, 1, zap) == BRLCAD_OK, "release the legacy display fixture");
}

int
main(int argc, char **argv)
{
    bu_setprogname(argv[0]);
    if (argc != 1)
	return 1;

    char path[MAXPATHLEN] = {0};
    FILE *temp = bu_temp_file(path, MAXPATHLEN);
    if (!temp)
	return 1;
    std::fclose(temp);
    if (!create_fixture(path)) {
	bu_file_delete(path);
	return 1;
    }

    test_legacy_history(path);
    struct ged *gedp = ged_open("db", path, 1);
    CHECK(gedp != NULL, "fixture opens in a GED context");
    if (gedp) {
	gedp->dbi_state = new DbiState(gedp);
	gedp->new_cmd_forms = 1;
	CHECK(ged_scene_available(gedp), "semantic scene is available");
	CHECK(ged_scene_path_state_get(gedp, gedp->ged_gvp, "sphere.s",
	    GED_SCENE_DRAW_DEFAULT) == GED_SCENE_PATH_NOT_DRAWN,
	    "fixture path starts undrawn");

	struct observer_state observer;
	ged_scene_observer_token token = ged_scene_observer_add(gedp,
	    scene_changed, &observer);
	CHECK(token != 0, "scene observer registration succeeds");

	const char *paths[] = {"sphere.s"};
	struct ged_scene_draw_request draw_request;
	ged_scene_draw_request_init(&draw_request);
	draw_request.view = gedp->ged_gvp;
	draw_request.paths = paths;
	draw_request.path_count = 1;
	draw_request.autoview = 1;
	struct ged_scene_result *result = ged_scene_result_create();
	CHECK(result != NULL, "scene result allocation succeeds");
	CHECK(ged_scene_draw(gedp, &draw_request, result) == GED_SCENE_OK,
	    "scene draw succeeds");
	CHECK(ged_scene_result_changed(result) == 1 &&
	    ged_scene_result_path_count(result) == 1 &&
	    BU_STR_EQUAL(ged_scene_result_path_at(result, 0), "sphere.s"),
	    "draw result reports the changed path");
	CHECK(ged_scene_result_revision_before(result) == 0 &&
	    ged_scene_result_revision_after(result) == 1,
	    "draw result reports its revision transition");
	CHECK(observer.calls == 1 && observer.kind == GED_SCENE_DELTA_DRAW &&
	    observer.path == "sphere.s" && observer.revision_before == 0 &&
	    observer.revision_after == 1,
	    "draw publishes one structured delta");

	std::vector<unsigned long long> ids =
	    static_cast<DbiState *>(gedp->dbi_state)->digest_path("sphere.s");
	CHECK(ged_scene_path_state_ids(gedp, gedp->ged_gvp, ids.data(),
	    ids.size(), GED_SCENE_DRAW_DEFAULT) == GED_SCENE_PATH_DRAWN,
	    "indexed query reports the drawn path");

	ged_scene_result_clear(result);
	CHECK(ged_scene_draw(gedp, &draw_request, result) == GED_SCENE_OK &&
	    !ged_scene_result_changed(result) && observer.calls == 1,
	    "idempotent draw publishes no delta");

	struct ged_scene_redraw_request redraw_request;
	ged_scene_redraw_request_init(&redraw_request);
	redraw_request.view = gedp->ged_gvp;
	CHECK(ged_scene_redraw(gedp, &redraw_request, result) == GED_SCENE_OK,
	    "whole-view redraw succeeds");
	const size_t redraw_changed = ged_scene_result_changed(result);
	const size_t observer_calls_after_redraw = observer.calls;
	CHECK(observer_calls_after_redraw == 1 + redraw_changed &&
	    (!redraw_changed || observer.kind == GED_SCENE_DELTA_REDRAW),
	    "redraw publishes a delta only when presentation changes");
	const uint64_t revision_after_redraw = ged_scene_revision(gedp);
	redraw_request.paths = paths;
	redraw_request.path_count = 1;
	CHECK(ged_scene_redraw(gedp, &redraw_request, result) ==
	    GED_SCENE_INVALID,
	    "legacy redraw rejects unsupported path filtering");

	struct ged_scene_erase_request erase_request;
	ged_scene_erase_request_init(&erase_request);
	erase_request.view = gedp->ged_gvp;
	erase_request.path = "sphere.s";
	CHECK(ged_scene_erase(gedp, &erase_request, result) == GED_SCENE_OK &&
	    ged_scene_result_changed(result), "scene erase succeeds");
	CHECK(observer.calls == observer_calls_after_redraw + 1 &&
	    observer.kind == GED_SCENE_DELTA_ERASE &&
	    observer.revision_after == revision_after_redraw + 1,
	    "erase publishes one structured delta");
	CHECK(ged_scene_path_state_get(gedp, gedp->ged_gvp, "sphere.s",
	    GED_SCENE_DRAW_DEFAULT) == GED_SCENE_PATH_NOT_DRAWN,
	    "erased path state is visible through the scene API");

	CHECK(ged_scene_observer_remove(gedp, token) == 1,
	    "scene observer removal succeeds");
	CHECK(ged_scene_draw(gedp, &draw_request, result) == GED_SCENE_OK &&
	    observer.calls == observer_calls_after_redraw + 1,
	    "removed observer receives no delta");
	const char *kill[] = {"kill", "sphere.s"};
	CHECK(ged_exec(gedp, 2, kill) == BRLCAD_OK,
	    "delete a displayed primitive");
	const unsigned int effects = ged_db_change_reconcile(gedp);
	CHECK((effects & (GED_DB_CHANGE_EFFECT_DATABASE | GED_DB_CHANGE_EFFECT_VIEW)) ==
	    (GED_DB_CHANGE_EFFECT_DATABASE | GED_DB_CHANGE_EFFECT_VIEW),
	    "reconciliation reports database and view changes for a deleted primitive");
	CHECK(ged_scene_path_state_get(gedp, gedp->ged_gvp, "sphere.s",
	    GED_SCENE_DRAW_DEFAULT) == GED_SCENE_PATH_NOT_DRAWN,
	    "reconciliation removes the deleted primitive from the view");

	const char *enable[] = {"db", "history", "enable", "session"};
	const int enabled = ged_exec(gedp, 4, enable);
	if (enabled == BRLCAD_OK ||
	    !strstr(bu_vls_cstr(gedp->ged_result_str), "Atomic DB5 replacement is unavailable")) {
	    CHECK(enabled == BRLCAD_OK, "enable history with an active scene state");
	    auto *dbis = static_cast<DbiState *>(gedp->dbi_state);
	    const char *make[] = {"make", "sphere.s", "sph"};
	    CHECK(ged_exec(gedp, 3, make) == BRLCAD_OK &&
		dbis->dbip == gedp->dbip &&
		dbis->get_hdp(ids.back()) == db_lookup(gedp->dbip, "sphere.s", LOOKUP_QUIET),
		"publication rebinds the scene index to the new directory");
	    CHECK(ged_scene_draw(gedp, &draw_request, result) == GED_SCENE_OK,
		"draw newly committed geometry");
	    const char *attr[] = {"attr", "set", "sphere.s", "rgb", "0/255/0"};
	    CHECK(ged_exec(gedp, 5, attr) == BRLCAD_OK, "edit a drawn primitive");
	    CHECK(ged_scene_path_state_get(gedp, gedp->ged_gvp, "sphere.s",
		GED_SCENE_DRAW_DEFAULT) == GED_SCENE_PATH_DRAWN,
		"replacement keeps drawn paths with surviving objects");
	    const char *undo[] = {"db", "undo"};
	    CHECK(ged_exec(gedp, 2, undo) == BRLCAD_OK &&
		ged_scene_path_state_get(gedp, gedp->ged_gvp, "sphere.s",
		    GED_SCENE_DRAW_DEFAULT) == GED_SCENE_PATH_DRAWN,
		"undo refreshes the drawn primitive in the replacement database");
	    CHECK(ged_exec(gedp, 2, undo) == BRLCAD_OK &&
		ged_scene_path_state_get(gedp, gedp->ged_gvp, "sphere.s",
		    GED_SCENE_DRAW_DEFAULT) == GED_SCENE_PATH_NOT_DRAWN,
		"undo removes a primitive absent in the replacement database");
	}
	ged_scene_result_destroy(result);

	delete static_cast<DbiState *>(gedp->dbi_state);
	gedp->dbi_state = NULL;
	ged_close(gedp);
    }

    bu_file_delete(path);
    return failures ? 1 : 0;
}

/*
 * Local Variables:
 * mode: C++
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
