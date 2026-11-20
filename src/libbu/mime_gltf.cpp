/*          M I M E _ G L T F . C P P
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

#include "common.h"

#include <algorithm>
#include <cstring>
#include <string>
#include "mime_private.h"
#include "mime_gltf.h"


int
mime_inspect_gltf(bu_mime_registry *registry, const bu_mime_probe_input *input, bu_mime_result *result)
{
    const unsigned char *data = input->data;
    size_t size = input->size;
    const char *type = "model/gltf+json";
    uint32_t container_version = 0;
    if (size >= 4 && !memcmp(data, "glTF", 4)) {
	type = "model/gltf-binary";
	if (size < mime_gltf::glb_header_size) return 1;
	container_version = mime_gltf::little_uint32(data + 4);
	bu_mime_traits traits = BU_MIME_TRAITS_INIT_ZERO;
	std::string version = std::to_string(container_version);
	int added = bu_mime_traits_add(&traits, "gltf.container-version", version.c_str()) &&
	    bu_mime_result_add_traits(result, type, &traits);
	bu_mime_traits_free(&traits);
	if (!added) return 0;
	if (size < mime_gltf::glb_json_offset) return 1;
	uint32_t chunk_type = mime_gltf::little_uint32(data + 16);
	if (!mime_gltf::valid_container(container_version, chunk_type))
	    return 1;
	size_t json_size = mime_gltf::little_uint32(data + 12);
	size = std::min(size - mime_gltf::glb_json_offset, json_size);
	data += mime_gltf::glb_json_offset;
    }
    size_t first = 0;
    if (size >= 3 && !memcmp(data, "\xef\xbb\xbf", 3)) first = 3;
    while (first < size && (data[first] == ' ' || data[first] == '\t' || data[first] == '\r' || data[first] == '\n')) ++first;
    if (first == size || data[first] != '{') return 1;
    mime_gltf::gltf_header header;
    nlohmann::json::sax_parse(data + first, data + size, &header);
    bool version_known = mime_gltf::version_string(header.version);
    if (!version_known && !container_version) return 1;
    if (version_known && bu_mime_result_add(registry, result, type,
	container_version ? BU_MIME_EVIDENCE_SIGNATURE : BU_MIME_EVIDENCE_HEURISTIC,
	BU_MIME_DEFAULT_PRIORITY) == BU_MIME_ERROR)
	return 0;
    bu_mime_traits traits = BU_MIME_TRAITS_INIT_ZERO;
    bool added = mime_gltf::append_traits(header, traits, container_version);
    if (!header.root_complete && !input->complete) result->incomplete = 1;
    added = added && bu_mime_result_add_traits(result, type, &traits);
    bu_mime_traits_free(&traits);
    return added;
}
