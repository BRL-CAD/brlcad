/*              T E S T _ D A T A B A S E _ S O U R C E . C
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 */
/** @file libbsg/tests/test_database_source.c
 *
 * Unit tests for public database-source metadata refs and fields.
 */

#include "common.h"
#include "../bsg_private.h"

#include <stdio.h>
#include <string.h>

#include "bu/app.h"
#include "bu/log.h"
#include "bu/malloc.h"
#include "bu/str.h"
#include "vmath.h"
#include "bsg/database_source.h"
#include "bsg/draw_intent.h"
#include "bsg/field.h"
#include "bsg/node.h"
#include "bsg/scene_builder.h"
#include "bsg/separator.h"
#include "bsg/util.h"

static int g_fail = 0;

#define CHECK(_cond, _msg) \
    do { \
	if (!(_cond)) { \
	    bu_log("FAIL [%s:%d] %s\n", __FILE__, __LINE__, (_msg)); \
	    g_fail++; \
	} \
    } while (0)

static struct bsg_view *
make_view(const char *name)
{
    struct bsg_view *v;
    BU_ALLOC(v, struct bsg_view);
    bsg_view_init(v, NULL);
    bu_vls_sprintf(&v->gv_name, "%s", name);
    return v;
}

static void
free_view(struct bsg_view *v)
{
    if (!v)
	return;
    bsg_view_free(v);
    bu_free(v, "test view");
}

static int
test_database_source_fields(void)
{
    printf("=== Test 1: database-source fields ===\n");
    struct bsg_view *v = make_view("database-source-fields");
    bsg_database_source_ref source = bsg_database_source_ref_create(v, "shape-src");
    CHECK(!bsg_database_source_ref_is_null(source), "source ref created");
    CHECK(bsg_node_is_a(bsg_database_source_ref_as_node(source), bsg_database_source_type()),
	    "source has database-source type");
    CHECK(!bsg_node_ref_is_null(bsg_separator_ref_as_node(
		    bsg_database_source_ref_as_separator(source))),
	    "source converts to separator");
    CHECK(!bsg_scene_ref_is_null(bsg_database_source_ref_as_scene(source)),
	    "source converts to scene ref");
    CHECK(bsg_database_source_ref_is_container(source),
	    "source is a database-source container");
    CHECK(!bsg_scene_is_database_source(bsg_database_source_ref_as_scene(source)),
	    "source container is not database-scope geometry");

    CHECK(!bsg_field_ref_is_null(
		    bsg_database_source_ref_database_path_field(source)),
	    "path field exists");
    CHECK(!bsg_field_ref_is_null(
		    bsg_node_ref_field(bsg_database_source_ref_as_node(source),
			"realizedSourceRevision")),
	    "field lookup by name");

    CHECK(bsg_database_source_ref_set_database_path(source, "/all.g/box.s"),
	    "set database path");
    CHECK(BU_STR_EQUAL(bsg_database_source_ref_database_path(source), "/all.g/box.s"),
	    "read database path");
    CHECK(bsg_database_source_ref_set_draw_mode(source, BSG_DRAW_MODE_SHADED),
	    "set draw mode");
    CHECK(bsg_database_source_ref_draw_mode(source) == BSG_DRAW_MODE_SHADED,
	    "read draw mode");
    CHECK(bsg_database_source_ref_set_material_policy(source,
		    BSG_DATABASE_SOURCE_MATERIAL_DATABASE),
	    "set material policy");
    CHECK(bsg_database_source_ref_material_policy(source) ==
	    BSG_DATABASE_SOURCE_MATERIAL_DATABASE,
	    "read material policy");
    CHECK(bsg_database_source_ref_set_source_revision(source, 7),
	    "set source revision");
    CHECK(bsg_database_source_ref_set_inputs_revision(source, 3),
	    "set inputs revision");
    CHECK(bsg_database_source_ref_set_realized_source_revision(source, 6),
	    "set realized source revision");
    CHECK(bsg_database_source_ref_set_realized_inputs_revision(source, 3),
	    "set realized inputs revision");
    CHECK(bsg_database_source_ref_is_stale(source),
	    "revision mismatch is stale");
    CHECK(bsg_database_source_ref_mark_realized_current(source),
	    "mark realized current");
    CHECK(!bsg_database_source_ref_is_stale(source),
	    "current revisions are not stale");
    CHECK(bsg_database_source_ref_set_stale_reason(source,
		    BSG_DATABASE_SOURCE_STALE_SETTINGS_CHANGED),
	    "set stale reason");
    CHECK(bsg_database_source_ref_is_stale(source),
	    "explicit stale reason is stale");
    CHECK(bsg_database_source_ref_set_realization_identity(source, 0x1234),
	    "set realization identity");
    CHECK(bsg_database_source_ref_realization_identity(source) == 0x1234,
	    "read realization identity");
    CHECK(bsg_database_source_ref_set_realization_status(source,
		    BSG_DATABASE_SOURCE_REALIZATION_CURRENT),
	    "set realization status");
    CHECK(bsg_database_source_ref_realization_status(source) ==
	    BSG_DATABASE_SOURCE_REALIZATION_CURRENT,
	    "read realization status");
    CHECK(bsg_database_source_ref_set_realization_role_flags(source,
		    BSG_DATABASE_SOURCE_REALIZATION_ROLE_CSG |
		    BSG_DATABASE_SOURCE_REALIZATION_ROLE_MESH),
	    "set realization role flags");
    CHECK(bsg_database_source_ref_realization_role_flags(source) ==
	    (BSG_DATABASE_SOURCE_REALIZATION_ROLE_CSG |
	     BSG_DATABASE_SOURCE_REALIZATION_ROLE_MESH),
	    "read realization role flags");
    CHECK(bsg_database_source_ref_set_realization_view_policy(source,
		    1, 10.0, 2048, 0.75, 0.25),
	    "set realization view policy");
    CHECK(bsg_database_source_ref_realization_view_dependent(source) == 1,
	    "read realization view-dependent flag");
    CHECK(NEAR_EQUAL(bsg_database_source_ref_realization_view_scale(source),
		10.0, SMALL_FASTF),
	    "read realization view scale");
    CHECK(bsg_database_source_ref_realization_bot_threshold(source) == 2048,
	    "read realization BoT threshold");
    CHECK(NEAR_EQUAL(bsg_database_source_ref_realization_curve_scale(source),
		0.75, SMALL_FASTF),
	    "read realization curve scale");
    CHECK(NEAR_EQUAL(bsg_database_source_ref_realization_point_scale(source),
		0.25, SMALL_FASTF),
	    "read realization point scale");

    bsg_node_ref_destroy(bsg_database_source_ref_as_node(source));
    free_view(v);
    return 0;
}

static int
test_database_source_scene_binding(void)
{
    printf("=== Test 3: scene-ref binding ===\n");
    struct bsg_view *v = make_view("database-source-scene-ref");
    bsg_scene_ref shape = bsg_scene_shape_create(v, "scene-shape-source");
    bsg_database_source_ref source = bsg_database_source_ref_from_scene(shape);
    struct bsg_database_source_record in = {0};
    struct bsg_database_source_record out = {0};

    CHECK(!bsg_database_source_ref_is_null(source),
	    "scene ref bound as source metadata");

    in.database_path = "/scene/ref/source.s";
    in.draw_mode = BSG_DRAW_MODE_WIRE;
    in.material_policy = BSG_DATABASE_SOURCE_MATERIAL_OVERRIDE;
    in.source_revision = 77;
    in.inputs_revision = 11;
    in.stale_reason = BSG_DATABASE_SOURCE_STALE_SOURCE_CHANGED;

    CHECK(bsg_database_source_record_apply(source, &in),
	    "scene source record apply");
    CHECK(bsg_database_source_record_get(source, &out),
	    "scene source record get");
    CHECK(BU_STR_EQUAL(out.database_path, in.database_path),
	    "scene source record path");
    CHECK(out.source_revision == in.source_revision,
	    "scene source revision");
    CHECK(out.inputs_revision == in.inputs_revision,
	    "scene input revision");
    CHECK(out.stale_reason == in.stale_reason,
	    "scene stale reason");

    bsg_scene_ref_destroy(shape);
    free_view(v);
    return 0;
}

static int
test_database_source_record_and_binding(void)
{
    printf("=== Test 2: record apply and existing-node binding ===\n");
    struct bsg_view *v = make_view("database-source-record");
    bsg_shape_ref shape = bsg_shape_ref_create(v, "legacy-shape-source");
    bsg_database_source_ref source =
	bsg_database_source_ref_from_node(bsg_shape_ref_as_node(shape));
    struct bsg_database_source_record in = {0};
    struct bsg_database_source_record out = {0};

    CHECK(!bsg_database_source_ref_is_null(source),
	    "existing node bound as source metadata");
    CHECK(bsg_node_ref_is_null(bsg_separator_ref_as_node(
		    bsg_database_source_ref_as_separator(source))),
	    "shape binding is not a separator");
    CHECK(!bsg_database_source_ref_is_container(source),
	    "shape binding is not a database-source container");

    in.database_path = "/all.g/arb8.s";
    in.draw_mode = BSG_DRAW_MODE_WIRE;
    in.material_policy = BSG_DATABASE_SOURCE_MATERIAL_OVERRIDE;
    in.source_revision = 42;
    in.inputs_revision = 5;
    in.realized_source_revision = 41;
    in.realized_inputs_revision = 4;
    in.stale_reason = BSG_DATABASE_SOURCE_STALE_VIEW_INPUT_CHANGED;
    in.realization_identity = 0xbeef;
    in.realization_status = BSG_DATABASE_SOURCE_REALIZATION_CURRENT;
    in.realization_role_flags = BSG_DATABASE_SOURCE_REALIZATION_ROLE_MESH;
    in.realization_view_dependent = 1;
    in.realization_view_scale = 123.0;
    in.realization_bot_threshold = 4096;
    in.realization_curve_scale = 0.5;
    in.realization_point_scale = 0.125;

    CHECK(bsg_database_source_record_apply(source, &in), "record apply");
    CHECK(bsg_database_source_record_get(source, &out), "record get");
    CHECK(BU_STR_EQUAL(out.database_path, in.database_path), "record path");
    CHECK(out.draw_mode == in.draw_mode, "record draw mode");
    CHECK(out.material_policy == in.material_policy, "record material policy");
    CHECK(out.source_revision == in.source_revision, "record source revision");
    CHECK(out.inputs_revision == in.inputs_revision, "record inputs revision");
    CHECK(out.realized_source_revision == in.realized_source_revision,
	    "record realized source revision");
    CHECK(out.realized_inputs_revision == in.realized_inputs_revision,
	    "record realized inputs revision");
    CHECK(out.stale_reason == in.stale_reason, "record stale reason");
    CHECK(out.realization_identity == in.realization_identity,
	    "record realization identity");
    CHECK(out.realization_status == in.realization_status,
	    "record realization status");
    CHECK(out.realization_role_flags == in.realization_role_flags,
	    "record realization role flags");
    CHECK(out.realization_view_dependent == in.realization_view_dependent,
	    "record realization view-dependent flag");
    CHECK(NEAR_EQUAL(out.realization_view_scale, in.realization_view_scale,
		SMALL_FASTF),
	    "record realization view scale");
    CHECK(out.realization_bot_threshold == in.realization_bot_threshold,
	    "record realization BoT threshold");
    CHECK(NEAR_EQUAL(out.realization_curve_scale, in.realization_curve_scale,
		SMALL_FASTF),
	    "record realization curve scale");
    CHECK(NEAR_EQUAL(out.realization_point_scale, in.realization_point_scale,
		SMALL_FASTF),
	    "record realization point scale");
    CHECK(bsg_database_source_ref_is_stale(source), "record is stale");

    bsg_node_ref_destroy(bsg_shape_ref_as_node(shape));
    free_view(v);
    return 0;
}

int
main(int argc, char **argv)
{
    bu_setprogname(argv[0]);
    (void)argc;

    test_database_source_fields();
    test_database_source_record_and_binding();
    test_database_source_scene_binding();

    if (g_fail) {
	bu_log("%d failures\n", g_fail);
	return 1;
    }

    printf("All database-source tests passed\n");
    return 0;
}

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
