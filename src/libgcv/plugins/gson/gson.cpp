/*                         G S O N . C P P
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

#include <cmath>
#include <fstream>
#include <stdexcept>
#include <string>

#include "bu/cv.h"
#include "bu/units.h"
#include "raytrace.h"
#include "wdb.h"
#include "gcv/api.h"

#include "../../../libbu/json.hpp"

using json = nlohmann::json;

static const char *gson_format = "BRL-CAD database";
static const char *gson_media_type = "model/vnd.brlcad+json";
static const uint64_t gson_version = 1;

static json
node_to_json(const struct rt_data_node *node)
{
    switch (rt_data_node_type(node)) {
	case RT_DATA_NULL:
	    return nullptr;
	case RT_DATA_BOOL: {
	    int value;
	    if (rt_data_get_bool(node, &value) != BRLCAD_OK) throw std::runtime_error("invalid boolean");
	    return value != 0;
	}
	case RT_DATA_INT: {
	    int64_t value;
	    if (rt_data_get_int(node, &value) != BRLCAD_OK) throw std::runtime_error("invalid integer");
	    return value;
	}
	case RT_DATA_UINT: {
	    uint64_t value;
	    if (rt_data_get_uint(node, &value) != BRLCAD_OK) throw std::runtime_error("invalid unsigned integer");
	    return value;
	}
	case RT_DATA_DOUBLE: {
	    double value;
	    if (rt_data_get_double(node, &value) != BRLCAD_OK || !std::isfinite(value))
		throw std::runtime_error("non-finite number");
	    return value;
	}
	case RT_DATA_STRING: {
	    const char *value = rt_data_get_string(node);
	    if (!value) throw std::runtime_error("invalid string");
	    return value;
	}
	case RT_DATA_BYTES: {
	    size_t size;
	    const void *bytes = rt_data_get_bytes(node, &size);
	    signed char *encoded = bu_b64_encode_block((const signed char *)bytes, size);
	    json value = {{"$bytes", encoded ? (const char *)encoded : ""}};
	    bu_free(encoded, "GSON base64");
	    return value;
	}
	case RT_DATA_ARRAY: {
	    json value = json::array();
	    for (size_t i = 0; i < rt_data_array_size(node); i++)
		value.push_back(node_to_json(rt_data_array_get(node, i)));
	    return value;
	}
	case RT_DATA_OBJECT: {
	    json value = json::object();
	    for (size_t i = 0; i < rt_data_object_size(node); i++)
		value[rt_data_object_name(node, i)] = node_to_json(rt_data_object_value(node, i));
	    return value;
	}
    }
    throw std::runtime_error("unknown data type");
}

static struct rt_data_node *
json_to_node(const json &value)
{
    if (value.is_null()) return rt_data_null();
    if (value.is_boolean()) return rt_data_bool(value.get<bool>());
    if (value.is_number_unsigned()) return rt_data_uint(value.get<uint64_t>());
    if (value.is_number_integer()) return rt_data_int(value.get<int64_t>());
    if (value.is_number_float()) {
	double number = value.get<double>();
	return std::isfinite(number) ? rt_data_double(number) : nullptr;
    }
    if (value.is_string()) return rt_data_string(value.get_ref<const std::string &>().c_str());
    if (value.is_array()) {
	struct rt_data_node *array = rt_data_array();
	for (const auto &item : value) {
	    struct rt_data_node *node = json_to_node(item);
	    if (!array || !node || rt_data_array_append(array, node) != BRLCAD_OK) {
		rt_data_free(node);
		rt_data_free(array);
		return nullptr;
	    }
	}
	return array;
    }
    if (value.is_object() && value.size() == 1 && value.contains("$bytes") && value["$bytes"].is_string()) {
	signed char *decoded = nullptr;
	const std::string encoded = value["$bytes"].get<std::string>();
	int size = bu_b64_decode_block(&decoded, (const signed char *)encoded.data(), encoded.size());
	struct rt_data_node *node = size >= 0 ? rt_data_bytes(decoded, (size_t)size) : nullptr;
	bu_free(decoded, "GSON decoded bytes");
	return node;
    }
    if (value.is_object()) {
	struct rt_data_node *object = rt_data_object();
	for (auto entry = value.begin(); entry != value.end(); ++entry) {
	    struct rt_data_node *node = json_to_node(entry.value());
	    if (!object || !node || rt_data_object_set(object, entry.key().c_str(), node) != BRLCAD_OK) {
		rt_data_free(node);
		rt_data_free(object);
		return nullptr;
	    }
	}
	return object;
    }
    return nullptr;
}

static json
avs_to_json(const struct bu_attribute_value_set *avs)
{
    json value = json::object();
    if (!BU_AVS_IS_INITIALIZED(avs)) return value;
    for (size_t i = 0; i < avs->count; i++) value[avs->avp[i].name] = avs->avp[i].value;
    return value;
}

static int
json_to_avs(struct bu_attribute_value_set *avs, const json &value)
{
    if (!value.is_object()) return BRLCAD_ERROR;
    bu_avs_init(avs, value.size(), "GSON attributes");
    for (auto entry = value.begin(); entry != value.end(); ++entry) {
	if (!entry.value().is_string() || !bu_avs_add(avs, entry.key().c_str(),
		entry.value().get_ref<const std::string &>().c_str())) {
	    bu_avs_free(avs);
	    return BRLCAD_ERROR;
	}
    }
    return BRLCAD_OK;
}

static int
gson_can_read(const char *path)
{
    try {
	std::ifstream stream(path);
	json document;
	stream >> document;
	return document.value("format", "") == gson_format && document.value("version", 0u) == gson_version;
    } catch (...) {
	return 0;
    }
}

static int
gson_write(struct gcv_context *context, const struct gcv_opts *UNUSED(options),
	const void *UNUSED(options_data), const char *path)
{
    struct rt_data_context data_context = RT_DATA_CONTEXT_INIT;
    struct directory *directory;
    json document;
    json objects = json::array();

    if (!context || !context->dbip || !path) return 0;
    data_context.media_type = gson_media_type;
    data_context.dbip = context->dbip;
    data_context.local2base = context->dbip->dbi_local2base;
    data_context.base2local = context->dbip->dbi_base2local;
    document["format"] = gson_format;
    document["version"] = gson_version;
    document["title"] = context->dbip->dbi_title;
    const char *units = bu_units_string(context->dbip->dbi_local2base);
    document["units"] = units ? units : "mm";

    FOR_ALL_DIRECTORY_START(directory, context->dbip) {
	struct rt_db_internal intern;
	struct rt_data_node *data = nullptr;
	if (directory->d_major_type == DB5_MAJORTYPE_ATTRIBUTE_ONLY) continue;
	if (rt_db_get_internal(&intern, directory, context->dbip, nullptr) < 0 ||
	    rt_obj_data_export(&data, &intern, &data_context) != BRLCAD_OK) {
	    bu_log("Unable to serialize '%s' to GSON\n", directory->d_namep);
	    return 0;
	}
	json object;
	object["name"] = directory->d_namep;
	object["attributes"] = avs_to_json(&intern.idb_avs);
	object["object"] = node_to_json(data);
	objects.push_back(std::move(object));
	rt_data_free(data);
	rt_db_free_internal(&intern);
    } FOR_ALL_DIRECTORY_END;
    document["objects"] = std::move(objects);
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    if (!stream) return 0;
    stream << document.dump(2) << '\n';
    return stream.good() ? 1 : 0;
}

static int
gson_read(struct gcv_context *context, const struct gcv_opts *UNUSED(options),
	const void *UNUSED(options_data), const char *path)
{
    try {
	std::ifstream stream(path, std::ios::binary);
	json document;
	stream >> document;
	if (document.value("format", "") != gson_format || document.value("version", 0u) != gson_version ||
	    !document["objects"].is_array()) return 0;
	double local2base = bu_units_conversion(document.value("units", "mm").c_str());
	if (local2base <= 0.0 || db_update_ident(context->dbip,
		document.value("title", "Untitled BRL-CAD Database").c_str(), local2base) < 0) return 0;
	struct rt_data_context data_context = RT_DATA_CONTEXT_INIT;
	data_context.media_type = gson_media_type;
	data_context.dbip = context->dbip;
	data_context.local2base = local2base;
	data_context.base2local = 1.0 / local2base;
	struct rt_wdb *wdbp = wdb_dbopen(context->dbip, RT_WDB_TYPE_DB_INMEM);
	for (const auto &object : document["objects"]) {
	    struct rt_db_internal intern;
	    struct bu_attribute_value_set avs = BU_AVS_INIT_ZERO;
	    struct rt_data_node *data = json_to_node(object["object"]);
	    if (!data || !object["name"].is_string() ||
		json_to_avs(&avs, object["attributes"]) != BRLCAD_OK) {
		bu_log("Invalid GSON object envelope\n");
		rt_data_free(data);
		return 0;
	    }
	    std::string name = object["name"].get<std::string>();
	    RT_DB_INTERNAL_INIT(&intern);
	    if (rt_obj_data_import(&intern, data, &data_context) != BRLCAD_OK) {
		bu_log("Unable to import GSON object '%s'\n", name.c_str());
		rt_data_free(data);
		bu_avs_free(&avs);
		return 0;
	    }
	    bu_avs_merge(&intern.idb_avs, &avs);
	    bu_avs_free(&avs);
	    rt_data_free(data);
	    if (wdb_put_internal(wdbp, name.c_str(), &intern, 1.0) < 0) {
		bu_log("Unable to write imported GSON object '%s'\n", name.c_str());
		rt_db_free_internal(&intern);
		return 0;
	    }
	    rt_db_free_internal(&intern);
	}
	return 1;
    } catch (const std::exception &error) {
	bu_log("Unable to process GSON file '%s': %s\n", path, error.what());
	return 0;
    }
}

extern "C" {
    static const struct gcv_filter gson_read_filter = {
	"GSON Reader", GCV_FILTER_READ, BU_MIME_MODEL_VND_BRLCAD_PLUS_JSON,
	gson_can_read, nullptr, nullptr, gson_read
    };
    static const struct gcv_filter gson_write_filter = {
	"GSON Writer", GCV_FILTER_WRITE, BU_MIME_MODEL_VND_BRLCAD_PLUS_JSON,
	nullptr, nullptr, nullptr, gson_write
    };
    static const struct gcv_filter * const filters[] = {&gson_read_filter, &gson_write_filter, nullptr};
    const struct gcv_plugin gcv_plugin_info_s = {filters};
    COMPILER_DLLEXPORT const struct gcv_plugin *gcv_plugin_info(void) { return &gcv_plugin_info_s; }
}

