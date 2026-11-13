/*                    O B J E C T _ I O . C P P
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

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "bu/malloc.h"
#include "bu/str.h"
#include "raytrace.h"

struct rt_data_node {
    enum rt_data_type type = RT_DATA_NULL;
    bool bool_value = false;
    int64_t int_value = 0;
    uint64_t uint_value = 0;
    double double_value = 0.0;
    std::string string_value;
    std::vector<unsigned char> bytes_value;
    std::vector<rt_data_node *> array_value;
    std::vector<std::pair<std::string, rt_data_node *>> object_value;

    ~rt_data_node()
    {
	for (rt_data_node *node : array_value)
	    delete node;
	for (auto &entry : object_value)
	    delete entry.second;
    }
};

static rt_data_node *
data_node(enum rt_data_type type)
{
    rt_data_node *node = new rt_data_node;
    node->type = type;
    return node;
}

extern "C" struct rt_data_node *
rt_data_null(void)
{
    return data_node(RT_DATA_NULL);
}

extern "C" struct rt_data_node *
rt_data_bool(int value)
{
    rt_data_node *node = data_node(RT_DATA_BOOL);
    node->bool_value = value != 0;
    return node;
}

extern "C" struct rt_data_node *
rt_data_int(int64_t value)
{
    rt_data_node *node = data_node(RT_DATA_INT);
    node->int_value = value;
    return node;
}

extern "C" struct rt_data_node *
rt_data_uint(uint64_t value)
{
    rt_data_node *node = data_node(RT_DATA_UINT);
    node->uint_value = value;
    return node;
}

extern "C" struct rt_data_node *
rt_data_double(double value)
{
    rt_data_node *node = data_node(RT_DATA_DOUBLE);
    node->double_value = value;
    return node;
}

extern "C" struct rt_data_node *
rt_data_string(const char *value)
{
    if (!value)
	return nullptr;
    rt_data_node *node = data_node(RT_DATA_STRING);
    node->string_value = value;
    return node;
}

extern "C" struct rt_data_node *
rt_data_bytes(const void *data, size_t size)
{
    if (size && !data)
	return nullptr;
    rt_data_node *node = data_node(RT_DATA_BYTES);
    const unsigned char *bytes = static_cast<const unsigned char *>(data);
    if (size)
	node->bytes_value.assign(bytes, bytes + size);
    return node;
}

extern "C" struct rt_data_node *
rt_data_array(void)
{
    return data_node(RT_DATA_ARRAY);
}

extern "C" struct rt_data_node *
rt_data_object(void)
{
    return data_node(RT_DATA_OBJECT);
}

extern "C" void
rt_data_free(struct rt_data_node *node)
{
    delete node;
}

extern "C" enum rt_data_type
rt_data_node_type(const struct rt_data_node *node)
{
    return node ? node->type : RT_DATA_NULL;
}

#define DATA_GETTER(_name, _type, _field, _kind) \
extern "C" int \
_name(const struct rt_data_node *node, _type *value) \
{ \
    if (!node || !value || node->type != _kind) \
	return BRLCAD_ERROR; \
    *value = node->_field; \
    return BRLCAD_OK; \
}

DATA_GETTER(rt_data_get_bool, int, bool_value, RT_DATA_BOOL)
DATA_GETTER(rt_data_get_int, int64_t, int_value, RT_DATA_INT)
DATA_GETTER(rt_data_get_uint, uint64_t, uint_value, RT_DATA_UINT)
DATA_GETTER(rt_data_get_double, double, double_value, RT_DATA_DOUBLE)

extern "C" const char *
rt_data_get_string(const struct rt_data_node *node)
{
    return node && node->type == RT_DATA_STRING ? node->string_value.c_str() : nullptr;
}

extern "C" const void *
rt_data_get_bytes(const struct rt_data_node *node, size_t *size)
{
    if (!node || node->type != RT_DATA_BYTES || !size)
	return nullptr;
    *size = node->bytes_value.size();
    return node->bytes_value.empty() ? nullptr : node->bytes_value.data();
}

extern "C" int
rt_data_array_append(struct rt_data_node *array, struct rt_data_node *node)
{
    if (!array || !node || array->type != RT_DATA_ARRAY)
	return BRLCAD_ERROR;
    array->array_value.push_back(node);
    return BRLCAD_OK;
}

extern "C" size_t
rt_data_array_size(const struct rt_data_node *array)
{
    return array && array->type == RT_DATA_ARRAY ? array->array_value.size() : 0;
}

extern "C" const struct rt_data_node *
rt_data_array_get(const struct rt_data_node *array, size_t index)
{
    if (!array || array->type != RT_DATA_ARRAY || index >= array->array_value.size())
	return nullptr;
    return array->array_value[index];
}

extern "C" int
rt_data_object_set(struct rt_data_node *object, const char *name, struct rt_data_node *node)
{
    if (!object || !name || !node || object->type != RT_DATA_OBJECT)
	return BRLCAD_ERROR;
    auto entry = std::find_if(object->object_value.begin(), object->object_value.end(),
	[name](const auto &item) { return item.first == name; });
    if (entry != object->object_value.end()) {
	delete entry->second;
	entry->second = node;
	return BRLCAD_OK;
    }
    object->object_value.emplace_back(name, node);
    return BRLCAD_OK;
}

extern "C" size_t
rt_data_object_size(const struct rt_data_node *object)
{
    return object && object->type == RT_DATA_OBJECT ? object->object_value.size() : 0;
}

extern "C" const char *
rt_data_object_name(const struct rt_data_node *object, size_t index)
{
    if (!object || object->type != RT_DATA_OBJECT || index >= object->object_value.size())
	return nullptr;
    return object->object_value[index].first.c_str();
}

extern "C" const struct rt_data_node *
rt_data_object_value(const struct rt_data_node *object, size_t index)
{
    if (!object || object->type != RT_DATA_OBJECT || index >= object->object_value.size())
	return nullptr;
    return object->object_value[index].second;
}

extern "C" const struct rt_data_node *
rt_data_object_get(const struct rt_data_node *object, const char *name)
{
    if (!object || !name || object->type != RT_DATA_OBJECT)
	return nullptr;
    auto entry = std::find_if(object->object_value.begin(), object->object_value.end(),
	[name](const auto &item) { return item.first == name; });
    return entry == object->object_value.end() ? nullptr : entry->second;
}

static bool
context_valid(const struct rt_data_context *context)
{
    if (!context)
	return true;
    return context->size >= offsetof(struct rt_data_context, reserved) &&
	context->version == RT_DATA_CONTEXT_VERSION;
}

static int
legacy_export(struct rt_data_node **result, const struct rt_db_internal *intern)
{
    struct bu_vls serialized = BU_VLS_INIT_ZERO;
    const char **arguments = nullptr;
    int argument_count = 0;
    rt_data_node *parameters = nullptr;
    rt_data_node *payload = nullptr;
    int status = BRLCAD_ERROR;

    if (!intern->idb_meth->ft_get || intern->idb_meth->ft_get(&serialized, intern, nullptr) != BRLCAD_OK)
	goto cleanup;
    if (bu_argv_from_tcl_list(bu_vls_cstr(&serialized), &argument_count, &arguments) != 0 ||
	argument_count < 1 || !(argument_count % 2))
	goto cleanup;
    payload = rt_data_object();
    parameters = rt_data_array();
    if (!payload || !parameters)
	goto cleanup;
    for (int index = 1; index < argument_count; index += 2) {
	rt_data_node *parameter = rt_data_object();
	if (!parameter ||
	    rt_data_object_set(parameter, "name", rt_data_string(arguments[index])) != BRLCAD_OK ||
	    rt_data_object_set(parameter, "value", rt_data_string(arguments[index + 1])) != BRLCAD_OK ||
	    rt_data_array_append(parameters, parameter) != BRLCAD_OK) {
	    delete parameter;
	    goto cleanup;
	}
    }
    if (rt_data_object_set(payload, "parameters", parameters) != BRLCAD_OK)
	goto cleanup;
    parameters = nullptr;
    *result = payload;
    payload = nullptr;
    status = BRLCAD_OK;

cleanup:
    delete parameters;
    delete payload;
    if (arguments)
	bu_free((void *)arguments, "object data arguments");
    bu_vls_free(&serialized);
    return status;
}

extern "C" int
rt_obj_data_export(struct rt_data_node **result, const struct rt_db_internal *intern, const struct rt_data_context *context)
{
    rt_data_node *data = nullptr;
    rt_data_node *object = nullptr;
    const char *encoding = "native";

    if (!result || !intern || !context_valid(context))
	return BRLCAD_ERROR;
    RT_CK_DB_INTERNAL(intern);
    *result = nullptr;
    if (intern->idb_meth->ft_data_export) {
	if (intern->idb_meth->ft_data_export(&data, intern, context) != BRLCAD_OK || !data)
	    return BRLCAD_ERROR;
    } else {
	encoding = "legacy-parameters";
	if (legacy_export(&data, intern) != BRLCAD_OK)
	    return BRLCAD_ERROR;
    }
    object = rt_data_object();
    if (!object ||
	rt_data_object_set(object, "type", rt_data_string(intern->idb_meth->ft_label)) != BRLCAD_OK ||
	rt_data_object_set(object, "encoding", rt_data_string(encoding)) != BRLCAD_OK ||
	rt_data_object_set(object, "data", data) != BRLCAD_OK) {
	delete data;
	delete object;
	return BRLCAD_ERROR;
    }
    *result = object;
    return BRLCAD_OK;
}

static int
legacy_import(struct rt_db_internal *intern, const struct rt_data_node *data)
{
    const rt_data_node *parameters = rt_data_object_get(data, "parameters");
    std::vector<const char *> arguments;

    if (!parameters || parameters->type != RT_DATA_ARRAY || !intern->idb_meth->ft_adjust)
	return BRLCAD_ERROR;
    arguments.reserve(parameters->array_value.size() * 2);
    for (const rt_data_node *parameter : parameters->array_value) {
	const rt_data_node *name = rt_data_object_get(parameter, "name");
	const rt_data_node *value = rt_data_object_get(parameter, "value");
	if (!name || !value || name->type != RT_DATA_STRING || value->type != RT_DATA_STRING)
	    return BRLCAD_ERROR;
	arguments.push_back(name->string_value.c_str());
	arguments.push_back(value->string_value.c_str());
    }
    struct bu_vls messages = BU_VLS_INIT_ZERO;
    int status = intern->idb_meth->ft_adjust(&messages, intern,
	static_cast<int>(arguments.size()), arguments.data());
    bu_vls_free(&messages);
    return status;
}

extern "C" int
rt_obj_data_import(struct rt_db_internal *intern, const struct rt_data_node *object, const struct rt_data_context *context)
{
    const rt_data_node *type_node;
    const rt_data_node *encoding_node;
    const rt_data_node *data;
    const struct rt_functab *ftp;
    point_t origin = VINIT_ZERO;
    int status;

    if (!intern || !object || object->type != RT_DATA_OBJECT || !context_valid(context))
	return BRLCAD_ERROR;
    type_node = rt_data_object_get(object, "type");
    encoding_node = rt_data_object_get(object, "encoding");
    data = rt_data_object_get(object, "data");
    if (!type_node || !encoding_node || !data ||
	type_node->type != RT_DATA_STRING || encoding_node->type != RT_DATA_STRING)
	return BRLCAD_ERROR;
    ftp = rt_get_functab_by_label(type_node->string_value.c_str());
    if (!ftp)
	return BRLCAD_ERROR;

    RT_DB_INTERNAL_INIT(intern);
    if (ftp->ft_make)
	status = ftp->ft_make(ftp, intern, nullptr, origin, 1.0);
    else {
	rt_generic_make(ftp, intern);
	status = BRLCAD_OK;
    }
    if (status != BRLCAD_OK)
	return BRLCAD_ERROR;

    if (encoding_node->string_value == "native" && ftp->ft_data_import)
	status = ftp->ft_data_import(intern, data, context);
    else if (encoding_node->string_value == "legacy-parameters")
	status = legacy_import(intern, data);
    else
	status = BRLCAD_ERROR;
    if (status != BRLCAD_OK)
	rt_db_free_internal(intern);
    return status;
}

