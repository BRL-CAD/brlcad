/*                   C O M P L E X I T Y . H
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 */
/** @addtogroup libbsg
 *
 * @brief
 * Complexity and LoD-policy node fields.
 */
/** @{ */
/* @file bsg/complexity.h */

#ifndef BSG_COMPLEXITY_H
#define BSG_COMPLEXITY_H

#include "common.h"
#include "bsg/field.h"
#include "bsg/node.h"

__BEGIN_DECLS

BSG_EXPORT extern bsg_field_ref bsg_complexity_ref_value_field(bsg_complexity_ref complexity);
BSG_EXPORT extern bsg_field_ref bsg_complexity_ref_curve_scale_field(bsg_complexity_ref complexity);
BSG_EXPORT extern bsg_field_ref bsg_complexity_ref_point_scale_field(bsg_complexity_ref complexity);
BSG_EXPORT extern bsg_field_ref bsg_complexity_ref_lod_policy_field(bsg_complexity_ref complexity);
BSG_EXPORT extern bsg_field_ref bsg_complexity_ref_lod_level_field(bsg_complexity_ref complexity);

BSG_EXPORT extern int bsg_complexity_ref_set_value(bsg_complexity_ref complexity, double value);
BSG_EXPORT extern double bsg_complexity_ref_value(bsg_complexity_ref complexity);
BSG_EXPORT extern int bsg_complexity_ref_set_curve_scale(bsg_complexity_ref complexity, double scale);
BSG_EXPORT extern double bsg_complexity_ref_curve_scale(bsg_complexity_ref complexity);
BSG_EXPORT extern int bsg_complexity_ref_set_point_scale(bsg_complexity_ref complexity, double scale);
BSG_EXPORT extern double bsg_complexity_ref_point_scale(bsg_complexity_ref complexity);
BSG_EXPORT extern int bsg_complexity_ref_set_lod_policy(bsg_complexity_ref complexity, int policy);
BSG_EXPORT extern int bsg_complexity_ref_lod_policy(bsg_complexity_ref complexity);
BSG_EXPORT extern int bsg_complexity_ref_set_lod_level(bsg_complexity_ref complexity, int level);
BSG_EXPORT extern int bsg_complexity_ref_lod_level(bsg_complexity_ref complexity);

__END_DECLS

#endif /* BSG_COMPLEXITY_H */

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
