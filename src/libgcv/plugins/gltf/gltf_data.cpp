/*                   G L T F _ D A T A . C P P
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 */

#include "common.h"

#include <cerrno>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <string>

#include "bu/cv.h"
#include "bu/log.h"
#include "bu/malloc.h"
#include "bu/str.h"
#include "rt/object_io.h"

#include "gltf_data.h"

namespace brlcad_gltf {

static constexpr size_t gltf_data_max_depth = 64;
static const char *gltf_data_tag = "$rt_type";
static const char *gltf_data_value = "value";

static bool
set_string(tg3_str &destination, const std::string &source)
{
    if (source.size() > std::numeric_limits<uint32_t>::max())
	return false;
    destination.data = source.c_str();
    destination.len = static_cast<uint32_t>(source.size());
    return true;
}

std::unique_ptr<data_value>
data_value::create(const struct rt_data_node *node)
{
    std::unique_ptr<data_value> result(new data_value);
    if (!result->assign(node, 0))
	return nullptr;
    return result;
}

bool
data_value::assign_tagged(const char *type, const char *encoded, size_t depth)
{
    if (depth > gltf_data_max_depth)
	return false;
    value.type = TG3_VALUE_OBJECT;
    keys = {gltf_data_tag, gltf_data_value};
    children.reserve(2);
    for (const char *entry : {type, encoded}) {
	std::unique_ptr<data_value> child(new data_value);
	child->value.type = TG3_VALUE_STRING;
	child->text = entry;
	if (!set_string(child->value.string_val, child->text))
	    return false;
	children.push_back(std::move(child));
    }
    return finish_object();
}

void
data_value::finish_array()
{
    array_values.reserve(children.size());
    for (const auto &child : children)
	array_values.push_back(*child->get());
    value.array_data = array_values.empty() ? nullptr : array_values.data();
    value.array_count = static_cast<uint32_t>(array_values.size());
}

bool
data_value::finish_object()
{
    object_values.resize(children.size());
    for (size_t i = 0; i < children.size(); ++i) {
	if (!set_string(object_values[i].key, keys[i]))
	    return false;
	object_values[i].value = *children[i]->get();
    }
    value.object_data = object_values.empty() ? nullptr : object_values.data();
    value.object_count = static_cast<uint32_t>(object_values.size());
    return true;
}

bool
data_value::assign(const struct rt_data_node *node, size_t depth)
{
    if (!node || depth > gltf_data_max_depth)
	return false;

    switch (rt_data_node_type(node)) {
	case RT_DATA_NULL:
	    value.type = TG3_VALUE_NULL;
	    return true;
	case RT_DATA_BOOL: {
	    int entry;
	    if (rt_data_get_bool(node, &entry) != BRLCAD_OK)
		return false;
	    value.type = TG3_VALUE_BOOL;
	    value.bool_val = entry;
	    return true;
	}
	case RT_DATA_INT: {
	    int64_t entry;
	    if (rt_data_get_int(node, &entry) != BRLCAD_OK)
		return false;
	    value.type = TG3_VALUE_INT;
	    value.int_val = entry;
	    return true;
	}
	case RT_DATA_UINT: {
	    uint64_t entry;
	    if (rt_data_get_uint(node, &entry) != BRLCAD_OK)
		return false;
	    text = std::to_string(entry);
	    return assign_tagged("uint", text.c_str(), depth + 1);
	}
	case RT_DATA_DOUBLE: {
	    double entry;
	    if (rt_data_get_double(node, &entry) != BRLCAD_OK || !std::isfinite(entry))
		return false;
	    char encoded[64];
	    int length = snprintf(encoded, sizeof(encoded), "%.17g", entry);
	    if (length < 0 || static_cast<size_t>(length) >= sizeof(encoded))
		return false;
	    return assign_tagged("double", encoded, depth + 1);
	}
	case RT_DATA_STRING: {
	    const char *entry = rt_data_get_string(node);
	    if (!entry)
		return false;
	    value.type = TG3_VALUE_STRING;
	    text = entry;
	    return set_string(value.string_val, text);
	}
	case RT_DATA_BYTES: {
	    size_t size;
	    const void *entry = rt_data_get_bytes(node, &size);
	    if (size && !entry)
		return false;
	    signed char *encoded = bu_b64_encode_block(
		static_cast<const signed char *>(entry), size);
	    if (!encoded)
		return false;
	    text = reinterpret_cast<const char *>(encoded);
	    bu_free(encoded, "base64-encoded neutral data");
	    return assign_tagged("bytes", text.c_str(), depth + 1);
	}
	case RT_DATA_ARRAY: {
	    const size_t count = rt_data_array_size(node);
	    if (count > std::numeric_limits<uint32_t>::max())
		return false;
	    value.type = TG3_VALUE_ARRAY;
	    children.reserve(count);
	    for (size_t i = 0; i < count; ++i) {
		std::unique_ptr<data_value> child(new data_value);
		if (!child->assign(rt_data_array_get(node, i), depth + 1))
		    return false;
		children.push_back(std::move(child));
	    }
	    finish_array();
	    return true;
	}
	case RT_DATA_OBJECT: {
	    const size_t count = rt_data_object_size(node);
	    if (count > std::numeric_limits<uint32_t>::max())
		return false;
	    value.type = TG3_VALUE_OBJECT;
	    keys.reserve(count);
	    children.reserve(count);
	    for (size_t i = 0; i < count; ++i) {
		const char *name = rt_data_object_name(node, i);
		std::unique_ptr<data_value> child(new data_value);
		if (!name || strlen(name) > std::numeric_limits<uint32_t>::max() ||
		    !child->assign(rt_data_object_value(node, i), depth + 1))
		    return false;
		keys.emplace_back(name);
		children.push_back(std::move(child));
	    }
	    return finish_object();
	}
    }
    return false;
}

const tg3_value *
value_object_get(const tg3_value *object, const char *name)
{
    if (!object || !name || object->type != TG3_VALUE_OBJECT)
	return nullptr;
    for (uint32_t i = 0; i < object->object_count; ++i) {
	const tg3_kv_pair &entry = object->object_data[i];
	if (entry.key.data && strlen(name) == entry.key.len &&
	    !bu_strncmp(entry.key.data, name, entry.key.len))
	    return &entry.value;
    }
    return nullptr;
}

static struct rt_data_node *
data_from_value_impl(const tg3_value *value, size_t depth)
{
    if (!value || depth > gltf_data_max_depth)
	return nullptr;

    switch (value->type) {
	case TG3_VALUE_NULL:
	    return rt_data_null();
	case TG3_VALUE_BOOL:
	    return rt_data_bool(value->bool_val);
	case TG3_VALUE_INT:
	    return rt_data_int(value->int_val);
	case TG3_VALUE_REAL:
	    return std::isfinite(value->real_val) ? rt_data_double(value->real_val) : nullptr;
	case TG3_VALUE_STRING: {
	    if (!value->string_val.data && value->string_val.len)
		return nullptr;
	    std::string entry = value->string_val.data ?
		std::string(value->string_val.data, value->string_val.len) : std::string();
	    return rt_data_string(entry.c_str());
	}
	case TG3_VALUE_BINARY:
	    if (value->binary_val.count > std::numeric_limits<size_t>::max())
		return nullptr;
	    return rt_data_bytes(value->binary_val.data, static_cast<size_t>(value->binary_val.count));
	case TG3_VALUE_ARRAY: {
	    struct rt_data_node *array = rt_data_array();
	    if (!array)
		return nullptr;
	    for (uint32_t i = 0; i < value->array_count; ++i) {
		struct rt_data_node *child = data_from_value_impl(&value->array_data[i], depth + 1);
		if (!child || rt_data_array_append(array, child) != BRLCAD_OK) {
		    bu_log("Unable to decode neutral glTF array value %u\n", i);
		    rt_data_free(child);
		    rt_data_free(array);
		    return nullptr;
		}
	    }
	    return array;
	}
	case TG3_VALUE_OBJECT: {
	    const tg3_value *tag = value_object_get(value, gltf_data_tag);
	    const tg3_value *encoded = value_object_get(value, gltf_data_value);
	    if (value->object_count == 2 && tag && encoded &&
		tag->type == TG3_VALUE_STRING && encoded->type == TG3_VALUE_STRING) {
		if ((!tag->string_val.data && tag->string_val.len) ||
		    (!encoded->string_val.data && encoded->string_val.len))
		    return nullptr;
		std::string type = tag->string_val.data ?
		    std::string(tag->string_val.data, tag->string_val.len) : std::string();
		std::string text_value = encoded->string_val.data ?
		    std::string(encoded->string_val.data, encoded->string_val.len) : std::string();
		if (type == "uint") {
		    char *end = nullptr;
		    errno = 0;
		    unsigned long long entry = strtoull(text_value.c_str(), &end, 10);
		    if (text_value.empty() || text_value[0] == '-' || errno || !end || *end) {
			bu_log("Invalid unsigned neutral glTF value '%s'\n", text_value.c_str());
			return nullptr;
		    }
		    return rt_data_uint(static_cast<uint64_t>(entry));
		}
		if (type == "double") {
		    char *end = nullptr;
		    errno = 0;
		    double entry = strtod(text_value.c_str(), &end);
		    if (text_value.empty() || errno || !end || *end || !std::isfinite(entry)) {
			bu_log("Invalid double neutral glTF value '%s'\n", text_value.c_str());
			return nullptr;
		    }
		    return rt_data_double(entry);
		}
		if (type == "bytes") {
		    signed char *decoded = nullptr;
		    int size = bu_b64_decode_block(&decoded,
			reinterpret_cast<const signed char *>(text_value.data()), text_value.size());
		    if (size < 0)
			return nullptr;
		    struct rt_data_node *result = rt_data_bytes(decoded, static_cast<size_t>(size));
		    bu_free(decoded, "base64-decoded neutral data");
		    return result;
		}
		bu_log("Unknown neutral glTF value type '%s'\n", type.c_str());
		return nullptr;
	    }
	    struct rt_data_node *object = rt_data_object();
	    if (!object)
		return nullptr;
	    for (uint32_t i = 0; i < value->object_count; ++i) {
		const tg3_kv_pair &entry = value->object_data[i];
		if (!entry.key.data && entry.key.len) {
		    rt_data_free(object);
		    return nullptr;
		}
		std::string name = entry.key.data ?
		    std::string(entry.key.data, entry.key.len) : std::string();
		struct rt_data_node *child = data_from_value_impl(&entry.value, depth + 1);
		if (!child || rt_data_object_set(object, name.c_str(), child) != BRLCAD_OK) {
		    bu_log("Unable to decode neutral glTF object value '%s'\n", name.c_str());
		    rt_data_free(child);
		    rt_data_free(object);
		    return nullptr;
		}
	    }
	    return object;
	}
    }
    return nullptr;
}

struct rt_data_node *
data_from_value(const tg3_value *value)
{
    return data_from_value_impl(value, 0);
}

} // namespace brlcad_gltf

/*
 * Local Variables:
 * mode: C++
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */

