/*                   D R A W _ S T Y L E . H
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 */
/** @addtogroup libbsg
 *
 * @brief
 * Draw-style node fields.
 */
/** @{ */
/* @file bsg/draw_style.h */

#ifndef BSG_DRAW_STYLE_H
#define BSG_DRAW_STYLE_H

#include "common.h"
#include "bsg/field.h"
#include "bsg/node.h"

__BEGIN_DECLS

BSG_EXPORT extern bsg_field_ref bsg_draw_style_ref_line_width_field(bsg_draw_style_ref style);
BSG_EXPORT extern bsg_field_ref bsg_draw_style_ref_line_style_field(bsg_draw_style_ref style);
BSG_EXPORT extern bsg_field_ref bsg_draw_style_ref_fill_mode_field(bsg_draw_style_ref style);
BSG_EXPORT extern bsg_field_ref bsg_draw_style_ref_point_size_field(bsg_draw_style_ref style);
BSG_EXPORT extern bsg_field_ref bsg_draw_style_ref_transparency_policy_field(bsg_draw_style_ref style);

BSG_EXPORT extern int bsg_draw_style_ref_set_line_width(bsg_draw_style_ref style, int line_width);
BSG_EXPORT extern int bsg_draw_style_ref_line_width(bsg_draw_style_ref style);
BSG_EXPORT extern int bsg_draw_style_ref_set_line_style(bsg_draw_style_ref style, int line_style);
BSG_EXPORT extern int bsg_draw_style_ref_line_style(bsg_draw_style_ref style);
BSG_EXPORT extern int bsg_draw_style_ref_set_fill_mode(bsg_draw_style_ref style, int fill_mode);
BSG_EXPORT extern int bsg_draw_style_ref_fill_mode(bsg_draw_style_ref style);
BSG_EXPORT extern int bsg_draw_style_ref_set_point_size(bsg_draw_style_ref style, double point_size);
BSG_EXPORT extern double bsg_draw_style_ref_point_size(bsg_draw_style_ref style);
BSG_EXPORT extern int bsg_draw_style_ref_set_transparency_policy(bsg_draw_style_ref style, int policy);
BSG_EXPORT extern int bsg_draw_style_ref_transparency_policy(bsg_draw_style_ref style);

__END_DECLS

#endif /* BSG_DRAW_STYLE_H */

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
