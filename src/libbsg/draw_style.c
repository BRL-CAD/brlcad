/*                   D R A W _ S T Y L E . C
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 */
/** @file libbsg/draw_style.c
 *
 * Draw-style node field accessors.
 */

#include "common.h"

#include "bsg/draw_style.h"
#include "field_private.h"
#include "object_private.h"

static bsg_node *
_draw_style_node(bsg_draw_style_ref style)
{
    if (!bsg_type_is_a(bsg_object_type(style.node.object), bsg_draw_style_type()))
	return NULL;
    return bsg_object_ref_node(style.node.object);
}

bsg_field_ref
bsg_draw_style_ref_line_width_field(bsg_draw_style_ref style)
{
    return bsg_node_field_ref(_draw_style_node(style), BSG_FIELD_DRAW_LINE_WIDTH);
}

bsg_field_ref
bsg_draw_style_ref_line_style_field(bsg_draw_style_ref style)
{
    return bsg_node_field_ref(_draw_style_node(style), BSG_FIELD_DRAW_LINE_STYLE);
}

bsg_field_ref
bsg_draw_style_ref_fill_mode_field(bsg_draw_style_ref style)
{
    return bsg_node_field_ref(_draw_style_node(style), BSG_FIELD_DRAW_FILL_MODE);
}

bsg_field_ref
bsg_draw_style_ref_point_size_field(bsg_draw_style_ref style)
{
    return bsg_node_field_ref(_draw_style_node(style), BSG_FIELD_DRAW_POINT_SIZE);
}

bsg_field_ref
bsg_draw_style_ref_transparency_policy_field(bsg_draw_style_ref style)
{
    return bsg_node_field_ref(_draw_style_node(style), BSG_FIELD_DRAW_TRANSPARENCY_POLICY);
}

int
bsg_draw_style_ref_set_line_width(bsg_draw_style_ref style, int line_width)
{
    return bsg_field_set_int(bsg_draw_style_ref_line_width_field(style), line_width);
}

int
bsg_draw_style_ref_line_width(bsg_draw_style_ref style)
{
    int value = 1;
    (void)bsg_field_get_int(bsg_draw_style_ref_line_width_field(style), &value);
    return value;
}

int
bsg_draw_style_ref_set_line_style(bsg_draw_style_ref style, int line_style)
{
    return bsg_field_set_int(bsg_draw_style_ref_line_style_field(style), line_style);
}

int
bsg_draw_style_ref_line_style(bsg_draw_style_ref style)
{
    int value = 0;
    (void)bsg_field_get_int(bsg_draw_style_ref_line_style_field(style), &value);
    return value;
}

int
bsg_draw_style_ref_set_fill_mode(bsg_draw_style_ref style, int fill_mode)
{
    return bsg_field_set_enum(bsg_draw_style_ref_fill_mode_field(style), fill_mode);
}

int
bsg_draw_style_ref_fill_mode(bsg_draw_style_ref style)
{
    int value = 0;
    (void)bsg_field_get_enum(bsg_draw_style_ref_fill_mode_field(style), &value);
    return value;
}

int
bsg_draw_style_ref_set_point_size(bsg_draw_style_ref style, double point_size)
{
    return bsg_field_set_double(bsg_draw_style_ref_point_size_field(style), point_size);
}

double
bsg_draw_style_ref_point_size(bsg_draw_style_ref style)
{
    double value = 1.0;
    (void)bsg_field_get_double(bsg_draw_style_ref_point_size_field(style), &value);
    return value;
}

int
bsg_draw_style_ref_set_transparency_policy(bsg_draw_style_ref style, int policy)
{
    return bsg_field_set_enum(bsg_draw_style_ref_transparency_policy_field(style), policy);
}

int
bsg_draw_style_ref_transparency_policy(bsg_draw_style_ref style)
{
    int value = 0;
    (void)bsg_field_get_enum(bsg_draw_style_ref_transparency_policy_field(style), &value);
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
