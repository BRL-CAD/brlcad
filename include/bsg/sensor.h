/*                      S E N S O R . H
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
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
/** @addtogroup libbsg
 *
 * @brief
 * Public typed sensor API.
 *
 * Sensor refs are owning handles.  Destroy them with bsg_sensor_ref_destroy()
 * or a typed destroy helper.  Field and node sensor targets are borrowed refs;
 * callers must destroy sensors before destroying their watched field or node.
 * Timer and update sensors are manually triggered by this API.
 */
/** @{ */
/* @file bsg/sensor.h */

#ifndef BSG_SENSOR_H
#define BSG_SENSOR_H

#include "common.h"
#include <stdint.h>
#include "bsg/field.h"

__BEGIN_DECLS

typedef enum bsg_sensor_kind {
    BSG_SENSOR_KIND_INVALID = 0,
    BSG_SENSOR_KIND_FIELD,
    BSG_SENSOR_KIND_NODE,
    BSG_SENSOR_KIND_TIMER,
    BSG_SENSOR_KIND_UPDATE
} bsg_sensor_kind;

typedef struct bsg_sensor_ref {
    void *opaque;
    uint64_t generation;
} bsg_sensor_ref;

typedef struct bsg_field_sensor_ref { bsg_sensor_ref sensor; } bsg_field_sensor_ref;
typedef struct bsg_node_sensor_ref { bsg_sensor_ref sensor; } bsg_node_sensor_ref;
typedef struct bsg_timer_sensor_ref { bsg_sensor_ref sensor; } bsg_timer_sensor_ref;
typedef struct bsg_update_sensor_ref { bsg_sensor_ref sensor; } bsg_update_sensor_ref;

#define BSG_SENSOR_REF_NULL_INIT { NULL, 0 }
#define BSG_FIELD_REF_SENSOR_NULL_INIT { BSG_SENSOR_REF_NULL_INIT }
#define BSG_NODE_REF_SENSOR_NULL_INIT { BSG_SENSOR_REF_NULL_INIT }
#define BSG_TIMER_REF_SENSOR_NULL_INIT { BSG_SENSOR_REF_NULL_INIT }
#define BSG_UPDATE_REF_SENSOR_NULL_INIT { BSG_SENSOR_REF_NULL_INIT }

typedef int (*bsg_field_sensor_cb)(bsg_field_ref field, void *data);
typedef int (*bsg_node_sensor_cb)(bsg_node_ref node, bsg_field_ref changed_field, void *data);
typedef int (*bsg_timer_sensor_cb)(bsg_timer_sensor_ref sensor, void *data);
typedef int (*bsg_update_sensor_cb)(bsg_update_sensor_ref sensor, void *data);

BSG_EXPORT extern bsg_sensor_ref bsg_sensor_ref_null(void);
BSG_EXPORT extern int bsg_sensor_ref_is_null(bsg_sensor_ref ref);
BSG_EXPORT extern int bsg_sensor_ref_equal(bsg_sensor_ref a, bsg_sensor_ref b);
BSG_EXPORT extern bsg_sensor_kind bsg_sensor_ref_kind(bsg_sensor_ref ref);
BSG_EXPORT extern void bsg_sensor_ref_destroy(bsg_sensor_ref ref);

BSG_EXPORT extern bsg_sensor_ref bsg_field_sensor_ref_as_sensor(bsg_field_sensor_ref ref);
BSG_EXPORT extern bsg_sensor_ref bsg_node_sensor_ref_as_sensor(bsg_node_sensor_ref ref);
BSG_EXPORT extern bsg_sensor_ref bsg_timer_sensor_ref_as_sensor(bsg_timer_sensor_ref ref);
BSG_EXPORT extern bsg_sensor_ref bsg_update_sensor_ref_as_sensor(bsg_update_sensor_ref ref);

BSG_EXPORT extern bsg_field_sensor_ref bsg_field_sensor_ref_create(bsg_field_ref field,
								    bsg_field_sensor_cb cb,
								    void *data);
BSG_EXPORT extern bsg_node_sensor_ref bsg_node_sensor_ref_create(bsg_node_ref node,
								 bsg_node_sensor_cb cb,
								 void *data);
BSG_EXPORT extern bsg_timer_sensor_ref bsg_timer_sensor_ref_create(long interval_ms,
								   bsg_timer_sensor_cb cb,
								   void *data);
BSG_EXPORT extern bsg_update_sensor_ref bsg_update_sensor_ref_create(bsg_update_sensor_cb cb,
								      void *data);

BSG_EXPORT extern void bsg_field_sensor_ref_destroy(bsg_field_sensor_ref ref);
BSG_EXPORT extern void bsg_node_sensor_ref_destroy(bsg_node_sensor_ref ref);
BSG_EXPORT extern void bsg_timer_sensor_ref_destroy(bsg_timer_sensor_ref ref);
BSG_EXPORT extern void bsg_update_sensor_ref_destroy(bsg_update_sensor_ref ref);

BSG_EXPORT extern bsg_field_ref bsg_field_sensor_ref_field(bsg_field_sensor_ref ref);
BSG_EXPORT extern bsg_node_ref bsg_node_sensor_ref_node(bsg_node_sensor_ref ref);
BSG_EXPORT extern long bsg_timer_sensor_ref_interval_ms(bsg_timer_sensor_ref ref);

BSG_EXPORT extern int bsg_timer_sensor_ref_trigger(bsg_timer_sensor_ref ref);
BSG_EXPORT extern int bsg_update_sensor_ref_trigger(bsg_update_sensor_ref ref);
BSG_EXPORT extern int bsg_update_sensors_fire(void);

__END_DECLS

#endif /* BSG_SENSOR_H */

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
