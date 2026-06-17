/*                 T E S T _ S E N S O R _ R E F . C
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 */
/** @file libbsg/tests/test_sensor_ref.c
 *
 * Public typed sensor API coverage.
 */

#include "common.h"

#include "bu/app.h"
#include "bu/malloc.h"
#include "bu/str.h"
#include "bsg/field.h"
#include "bsg/node.h"
#include "bsg/sensor.h"
#include "bsg/separator.h"
#include "bsg/util.h"

#define CHECK(_expr, _msg) do { if (!(_expr)) { bu_log("FAIL: %s\n", (_msg)); return 1; } } while (0)

static struct bsg_view *
make_view(void)
{
    struct bsg_view *v;
    BU_ALLOC(v, struct bsg_view);
    bsg_view_init(v, NULL);
    bu_vls_sprintf(&v->gv_name, "sensor_ref_view");
    return v;
}

static void
free_view(struct bsg_view *v)
{
    if (!v)
	return;
    bsg_view_free(v);
    bu_free(v, "sensor_ref_view");
}

static int s_field_count = 0;
static int s_node_count = 0;
static int s_timer_count = 0;
static int s_update_count = 0;
static int s_order[16];
static int s_order_len = 0;
static bsg_field_ref s_last_field = BSG_FIELD_REF_NULL_INIT;
static bsg_node_ref s_last_node = BSG_NODE_REF_NULL_INIT;

static void
reset_counts(void)
{
    s_field_count = 0;
    s_node_count = 0;
    s_timer_count = 0;
    s_update_count = 0;
    s_order_len = 0;
    s_last_field = bsg_field_ref_null();
    s_last_node = bsg_node_ref_null();
}

static void
push_order(int value)
{
    if (s_order_len < (int)(sizeof(s_order) / sizeof(s_order[0])))
	s_order[s_order_len++] = value;
}

static int
field_cb(bsg_field_ref field, void *data)
{
    s_field_count++;
    s_last_field = field;
    push_order(1);
    if (data)
	*(int *)data = 1;
    return 0;
}

static int
node_cb(bsg_node_ref node, bsg_field_ref changed_field, void *data)
{
    s_node_count++;
    s_last_node = node;
    s_last_field = changed_field;
    push_order(2);
    if (data)
	*(int *)data = 1;
    return 0;
}

static int
timer_cb(bsg_timer_sensor_ref sensor, void *UNUSED(data))
{
    CHECK(bsg_timer_sensor_ref_interval_ms(sensor) == 25, "timer interval visible in callback");
    s_timer_count++;
    push_order(3);
    return 0;
}

static int
update_cb_a(bsg_update_sensor_ref UNUSED(sensor), void *UNUSED(data))
{
    s_update_count++;
    push_order(4);
    return 0;
}

static int
update_cb_b(bsg_update_sensor_ref UNUSED(sensor), void *UNUSED(data))
{
    s_update_count++;
    push_order(5);
    return 0;
}

static int
test_field_and_node_sensors(void)
{
    struct bsg_view *v = make_view();
    bsg_separator_ref root = bsg_view_scene_separator_ref(v, 1);
    bsg_node_ref root_node = bsg_separator_ref_as_node(root);
    bsg_field_ref color = bsg_node_ref_color_field(root_node);
    bsg_field_ref visibility = bsg_node_ref_visibility_field(root_node);
    int field_data = 0;
    int node_data = 0;

    CHECK(!bsg_field_ref_is_null(color), "color field exists");
    CHECK(!bsg_field_ref_is_null(visibility), "visibility field exists");

    bsg_field_sensor_ref field_sensor = bsg_field_sensor_ref_create(color, field_cb, &field_data);
    bsg_node_sensor_ref node_sensor = bsg_node_sensor_ref_create(root_node, node_cb, &node_data);

    CHECK(!bsg_sensor_ref_is_null(bsg_field_sensor_ref_as_sensor(field_sensor)), "field sensor created");
    CHECK(!bsg_sensor_ref_is_null(bsg_node_sensor_ref_as_sensor(node_sensor)), "node sensor created");
    CHECK(bsg_sensor_ref_kind(bsg_field_sensor_ref_as_sensor(field_sensor)) == BSG_SENSOR_KIND_FIELD,
	    "field sensor kind");
    CHECK(bsg_sensor_ref_kind(bsg_node_sensor_ref_as_sensor(node_sensor)) == BSG_SENSOR_KIND_NODE,
	    "node sensor kind");
    CHECK(bsg_field_ref_equal(bsg_field_sensor_ref_field(field_sensor), color), "field sensor target");
    CHECK(bsg_node_ref_equal(bsg_node_sensor_ref_node(node_sensor), root_node), "node sensor target");

    reset_counts();
    bsg_node_ref_set_visible(root_node, 0);
    CHECK(s_field_count == 0, "field sensor ignores other fields");
    CHECK(s_node_count == 1, "node sensor fires for visibility field");
    CHECK(node_data == 1, "node sensor data updated");
    CHECK(bsg_node_ref_equal(s_last_node, root_node), "node callback target");
    CHECK(bsg_field_ref_equal(s_last_field, visibility), "node callback changed field");

    reset_counts();
    field_data = 0;
    node_data = 0;
    bsg_node_ref_set_color(root_node, 11, 22, 33);
    CHECK(s_field_count == 1, "field sensor fires for watched field");
    CHECK(s_node_count == 1, "node sensor fires for watched node");
    CHECK(field_data == 1 && node_data == 1, "sensor data updated");
    CHECK(s_order_len == 2 && s_order[0] == 1 && s_order[1] == 2,
	    "field/node sensors fire in creation order");
    CHECK(bsg_field_ref_equal(s_last_field, color), "changed field propagated");

    bsg_field_sensor_ref_destroy(field_sensor);
    CHECK(bsg_sensor_ref_is_null(bsg_field_sensor_ref_as_sensor(field_sensor)),
	    "destroyed field sensor is inactive");

    reset_counts();
    bsg_node_ref_set_color(root_node, 44, 55, 66);
    CHECK(s_field_count == 0, "destroyed field sensor does not fire");
    CHECK(s_node_count == 1, "node sensor still fires after field sensor deletion");

    bsg_node_sensor_ref_destroy(node_sensor);
    CHECK(bsg_sensor_ref_is_null(bsg_node_sensor_ref_as_sensor(node_sensor)),
	    "destroyed node sensor is inactive");

    reset_counts();
    bsg_node_ref_set_visible(root_node, 1);
    CHECK(s_field_count == 0 && s_node_count == 0, "destroyed sensors do not fire");

    free_view(v);
    return 0;
}

static int
test_timer_and_update_sensors(void)
{
    reset_counts();

    bsg_timer_sensor_ref timer = bsg_timer_sensor_ref_create(25, timer_cb, NULL);
    CHECK(!bsg_sensor_ref_is_null(bsg_timer_sensor_ref_as_sensor(timer)), "timer sensor created");
    CHECK(bsg_sensor_ref_kind(bsg_timer_sensor_ref_as_sensor(timer)) == BSG_SENSOR_KIND_TIMER,
	    "timer sensor kind");
    CHECK(bsg_timer_sensor_ref_interval_ms(timer) == 25, "timer interval stored");
    CHECK(bsg_timer_sensor_ref_trigger(timer) == 1, "timer trigger fires");
    CHECK(s_timer_count == 1 && s_order_len == 1 && s_order[0] == 3,
	    "timer callback invoked");
    bsg_timer_sensor_ref_destroy(timer);
    CHECK(bsg_timer_sensor_ref_trigger(timer) == 0, "destroyed timer does not trigger");

    reset_counts();
    bsg_update_sensor_ref update_a = bsg_update_sensor_ref_create(update_cb_a, NULL);
    bsg_update_sensor_ref update_b = bsg_update_sensor_ref_create(update_cb_b, NULL);
    CHECK(!bsg_sensor_ref_is_null(bsg_update_sensor_ref_as_sensor(update_a)), "update sensor A created");
    CHECK(!bsg_sensor_ref_is_null(bsg_update_sensor_ref_as_sensor(update_b)), "update sensor B created");
    CHECK(bsg_update_sensors_fire() == 2, "global update fire count");
    CHECK(s_update_count == 2 && s_order_len == 2 && s_order[0] == 4 && s_order[1] == 5,
	    "update sensors fire in creation order");

    reset_counts();
    CHECK(bsg_update_sensor_ref_trigger(update_b) == 1, "direct update trigger");
    CHECK(s_update_count == 1 && s_order_len == 1 && s_order[0] == 5,
	    "direct update trigger invokes only selected sensor");

    bsg_update_sensor_ref_destroy(update_a);
    reset_counts();
    CHECK(bsg_update_sensors_fire() == 1, "destroyed update sensor skipped");
    CHECK(s_update_count == 1 && s_order_len == 1 && s_order[0] == 5,
	    "remaining update sensor still fires");

    bsg_update_sensor_ref_destroy(update_b);
    CHECK(bsg_update_sensors_fire() == 0, "no update sensors remain");
    return 0;
}

int
main(int UNUSED(argc), const char **argv)
{
    bu_setprogname(argv[0]);

    if (test_field_and_node_sensors())
	return 1;
    if (test_timer_and_update_sensors())
	return 1;

    bu_log("sensor-ref tests passed\n");
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
