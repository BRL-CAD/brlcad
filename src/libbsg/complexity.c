/*                   C O M P L E X I T Y . C
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 */
/** @file libbsg/complexity.c
 *
 * Complexity node field accessors.
 */

#include "common.h"

#include "bsg/complexity.h"
#include "field_private.h"
#include "object_private.h"

static bsg_node *
_complexity_node(bsg_complexity_ref complexity)
{
    if (!bsg_type_is_a(bsg_object_type(complexity.node.object), bsg_complexity_type()))
	return NULL;
    return bsg_object_ref_node(complexity.node.object);
}

bsg_field_ref
bsg_complexity_ref_value_field(bsg_complexity_ref complexity)
{
    return bsg_node_field_ref(_complexity_node(complexity), BSG_FIELD_COMPLEXITY_VALUE);
}

bsg_field_ref
bsg_complexity_ref_curve_scale_field(bsg_complexity_ref complexity)
{
    return bsg_node_field_ref(_complexity_node(complexity), BSG_FIELD_COMPLEXITY_CURVE_SCALE);
}

bsg_field_ref
bsg_complexity_ref_point_scale_field(bsg_complexity_ref complexity)
{
    return bsg_node_field_ref(_complexity_node(complexity), BSG_FIELD_COMPLEXITY_POINT_SCALE);
}

bsg_field_ref
bsg_complexity_ref_lod_policy_field(bsg_complexity_ref complexity)
{
    return bsg_node_field_ref(_complexity_node(complexity), BSG_FIELD_COMPLEXITY_LOD_POLICY);
}

bsg_field_ref
bsg_complexity_ref_lod_level_field(bsg_complexity_ref complexity)
{
    return bsg_node_field_ref(_complexity_node(complexity), BSG_FIELD_COMPLEXITY_LOD_LEVEL);
}

int
bsg_complexity_ref_set_value(bsg_complexity_ref complexity, double value)
{
    return bsg_field_set_double(bsg_complexity_ref_value_field(complexity), value);
}

double
bsg_complexity_ref_value(bsg_complexity_ref complexity)
{
    double value = 0.0;
    (void)bsg_field_get_double(bsg_complexity_ref_value_field(complexity), &value);
    return value;
}

int
bsg_complexity_ref_set_curve_scale(bsg_complexity_ref complexity, double scale)
{
    return bsg_field_set_double(bsg_complexity_ref_curve_scale_field(complexity), scale);
}

double
bsg_complexity_ref_curve_scale(bsg_complexity_ref complexity)
{
    double value = 1.0;
    (void)bsg_field_get_double(bsg_complexity_ref_curve_scale_field(complexity), &value);
    return value;
}

int
bsg_complexity_ref_set_point_scale(bsg_complexity_ref complexity, double scale)
{
    return bsg_field_set_double(bsg_complexity_ref_point_scale_field(complexity), scale);
}

double
bsg_complexity_ref_point_scale(bsg_complexity_ref complexity)
{
    double value = 1.0;
    (void)bsg_field_get_double(bsg_complexity_ref_point_scale_field(complexity), &value);
    return value;
}

int
bsg_complexity_ref_set_lod_policy(bsg_complexity_ref complexity, int policy)
{
    return bsg_field_set_enum(bsg_complexity_ref_lod_policy_field(complexity), policy);
}

int
bsg_complexity_ref_lod_policy(bsg_complexity_ref complexity)
{
    int value = 0;
    (void)bsg_field_get_enum(bsg_complexity_ref_lod_policy_field(complexity), &value);
    return value;
}

int
bsg_complexity_ref_set_lod_level(bsg_complexity_ref complexity, int level)
{
    return bsg_field_set_int(bsg_complexity_ref_lod_level_field(complexity), level);
}

int
bsg_complexity_ref_lod_level(bsg_complexity_ref complexity)
{
    int value = -1;
    (void)bsg_field_get_int(bsg_complexity_ref_lod_level_field(complexity), &value);
    return value;
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
