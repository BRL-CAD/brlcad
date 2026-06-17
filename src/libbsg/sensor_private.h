/*                S E N S O R _ P R I V A T E . H
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 */
/** @file sensor_private.h
 *
 * Private typed sensor notification hooks.
 */

#ifndef BSG_SENSOR_PRIVATE_H
#define BSG_SENSOR_PRIVATE_H

#include "common.h"
#include "bsg/field.h"

__BEGIN_DECLS

extern void bsg_sensor_notify_field_ref(bsg_field_ref field);

__END_DECLS

#endif /* BSG_SENSOR_PRIVATE_H */
