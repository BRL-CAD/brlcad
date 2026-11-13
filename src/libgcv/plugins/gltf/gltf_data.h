/*                    G L T F _ D A T A . H
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 */

#ifndef LIBGCV_GLTF_DATA_H
#define LIBGCV_GLTF_DATA_H

#include "common.h"

#include <memory>
#include <string>
#include <vector>

#include "tiny_gltf_v3.h"

struct rt_data_node;

namespace brlcad_gltf {

class data_value {
public:
    static std::unique_ptr<data_value> create(const struct rt_data_node *node);

    const tg3_value *get() const { return &value; }

private:
    data_value() = default;
    bool assign(const struct rt_data_node *node, size_t depth);
    bool assign_tagged(const char *type, const char *encoded, size_t depth);
    void finish_array();
    bool finish_object();

    tg3_value value = {};
    std::string text;
    std::vector<std::string> keys;
    std::vector<std::unique_ptr<data_value>> children;
    std::vector<tg3_value> array_values;
    std::vector<tg3_kv_pair> object_values;
};

struct rt_data_node *data_from_value(const tg3_value *value);
const tg3_value *value_object_get(const tg3_value *object, const char *name);

} // namespace brlcad_gltf

#endif /* LIBGCV_GLTF_DATA_H */

