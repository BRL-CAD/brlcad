/*                    O B J E C T _ I O . H
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 */
/** @file rt/object_io.h
 *
 * Format-neutral object data exchange.
 */

#ifndef RT_OBJECT_IO_H
#define RT_OBJECT_IO_H

#include "common.h"

#include <stddef.h>
#include <stdint.h>

#include "bu/avs.h"
#include "bu/vls.h"
#include "rt/defines.h"

__BEGIN_DECLS

struct db_i;
struct rt_db_internal;
struct rt_data_node;

enum rt_data_type {
    RT_DATA_NULL = 0,
    RT_DATA_BOOL,
    RT_DATA_INT,
    RT_DATA_UINT,
    RT_DATA_DOUBLE,
    RT_DATA_STRING,
    RT_DATA_BYTES,
    RT_DATA_ARRAY,
    RT_DATA_OBJECT
};

#define RT_DATA_CONTEXT_VERSION 1u
#define RT_DATA_CONTEXT_RESERVED 4u

struct rt_data_context {
    size_t size;
    uint32_t version;
    uint64_t flags;
    const char *media_type;
    const struct db_i *dbip;
    double local2base;
    double base2local;
    const struct bu_attribute_value_set *options;
    struct bu_vls *messages;
    void *reserved[RT_DATA_CONTEXT_RESERVED];
};

#define RT_DATA_CONTEXT_INIT { \
    sizeof(struct rt_data_context), RT_DATA_CONTEXT_VERSION, 0, NULL, NULL, \
    1.0, 1.0, NULL, NULL, {NULL, NULL, NULL, NULL} \
}

RT_EXPORT extern struct rt_data_node *rt_data_null(void);
RT_EXPORT extern struct rt_data_node *rt_data_bool(int value);
RT_EXPORT extern struct rt_data_node *rt_data_int(int64_t value);
RT_EXPORT extern struct rt_data_node *rt_data_uint(uint64_t value);
RT_EXPORT extern struct rt_data_node *rt_data_double(double value);
RT_EXPORT extern struct rt_data_node *rt_data_string(const char *value);
RT_EXPORT extern struct rt_data_node *rt_data_bytes(const void *data, size_t size);
RT_EXPORT extern struct rt_data_node *rt_data_array(void);
RT_EXPORT extern struct rt_data_node *rt_data_object(void);
RT_EXPORT extern void rt_data_free(struct rt_data_node *node);
RT_EXPORT extern enum rt_data_type rt_data_node_type(const struct rt_data_node *node);

RT_EXPORT extern int rt_data_get_bool(const struct rt_data_node *node, int *value);
RT_EXPORT extern int rt_data_get_int(const struct rt_data_node *node, int64_t *value);
RT_EXPORT extern int rt_data_get_uint(const struct rt_data_node *node, uint64_t *value);
RT_EXPORT extern int rt_data_get_double(const struct rt_data_node *node, double *value);
RT_EXPORT extern const char *rt_data_get_string(const struct rt_data_node *node);
RT_EXPORT extern const void *rt_data_get_bytes(const struct rt_data_node *node, size_t *size);

/** Append node to array, transferring ownership on success. */
RT_EXPORT extern int rt_data_array_append(struct rt_data_node *array, struct rt_data_node *node);
RT_EXPORT extern size_t rt_data_array_size(const struct rt_data_node *array);
RT_EXPORT extern const struct rt_data_node *rt_data_array_get(const struct rt_data_node *array, size_t index);

/** Set a named value, transferring ownership on success. */
RT_EXPORT extern int rt_data_object_set(struct rt_data_node *object, const char *name, struct rt_data_node *node);
RT_EXPORT extern size_t rt_data_object_size(const struct rt_data_node *object);
RT_EXPORT extern const char *rt_data_object_name(const struct rt_data_node *object, size_t index);
RT_EXPORT extern const struct rt_data_node *rt_data_object_value(const struct rt_data_node *object, size_t index);
RT_EXPORT extern const struct rt_data_node *rt_data_object_get(const struct rt_data_node *object, const char *name);

RT_EXPORT extern int rt_obj_data_export(struct rt_data_node **result,
                                        const struct rt_db_internal *intern,
                                        const struct rt_data_context *context);
RT_EXPORT extern int rt_obj_data_import(struct rt_db_internal *intern,
                                        const struct rt_data_node *data,
                                        const struct rt_data_context *context);

__END_DECLS

#endif /* RT_OBJECT_IO_H */
