/*          M I M E _ I N S P E C T . C P P
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
#include <cctype>
#include <cstring>
#include <sstream>
#include <set>
#include <string>
#include <vector>
#include "mime_private.h"
#include "mime_skeletal.h"
#include "mime_nff.h"
#include "mime_raw.h"
#include "mime_xml.h"
#include "mime_p21.h"
#include "mime_png.h"
#include "mime_rle.h"
#include "mime_sun.h"
#include "mime_binary.h"
#include "mime_blender.h"
#include "mime_scene_text.h"
#include "mime_x.h"
#include "mime_assbin.h"
#include "mime_ply.h"
#include "mime_3ds.h"
#include "mime_fbx.h"
#include "mime_opengex.h"
#include "mime_obj.h"
#include "mime_stl.h"
#include "mime_bmp.h"
#include "mime_cals.h"
#include "mime_vicar.h"
#include "mime_zarr.h"
#include "mime_stac.h"
#include "mime_esric.h"
#include "mime_tar.h"
#include "mime_lerc.h"
#include "mime_leveller.h"
#include "mime_hf2.h"
#include "mime_sigdem.h"
#include "mime_byn.h"
#include "mime_isg.h"
#include "mime_raster_label.h"
#include "mime_noaa_grid.h"
#include "mime_nsidc.h"
#include "mime_iris.h"
#include "mime_nitf.h"
#include "mime_geopackage.h"
#include "mime_grib.h"
#include "mime_terrain.h"
#include "mime_off.h"
#include "mime_dxf.h"
#include "mime_dwg.h"
#include "mime_pnm.h"
#include "mime_avi.h"
#include "mime_unreal.h"
#include "bu/str.h"

namespace {
using mime_detail::decimal_field;
using mime_detail::trim;
using mime_detail::upper;

/* Traits describe declarations, not validation of the document body. */
bool fact(bu_mime_result *result, const char *type, const char *key, const std::string &value)
{
    /* Malformed declarations do not become facts or detector failures. */
    if (value.empty() || value.find('\0') != std::string::npos)
	return true;
    bu_mime_trait item = BU_MIME_TRAIT_STRING(key, value.c_str());
    bu_mime_traits traits = {&item, 1};
    return bu_mime_result_add_traits(result, type, &traits) != 0;
}

bool set_fact(bu_mime_result *result, const char *type, const char *key, const std::vector<std::string> &values)
{
    std::vector<const char *> members;
    for (const auto &value : values) members.push_back(value.c_str());
    bu_mime_trait item = BU_MIME_TRAIT_SET(key, members.data(), members.size());
    bu_mime_traits traits = {&item, 1};
    return bu_mime_result_add_traits(result, type, &traits) != 0;
}

bool identify(bu_mime_registry *registry, bu_mime_result *result, const char *type, bu_mime_evidence evidence, unsigned int priority = BU_MIME_DEFAULT_PRIORITY)
{
    return bu_mime_result_add(registry, result, type, evidence, priority) != BU_MIME_ERROR;
}

using mime_binary::little_u32;
using mime_binary::big_u32;

bool scene_signature(const bu_mime_probe_input &input, const void *magic, size_t length, bu_mime_result *result)
{
    size_t available = std::min(input.size, length);
    if (!available || memcmp(input.data, magic, available)) return false;
    if (input.size >= length) return true;
    if (!input.complete) result->incomplete = 1;
    return false;
}

bool cad_header_profiles(bu_mime_registry *registry, const bu_mime_probe_input &input, bu_mime_result *result)
{
    const auto dwg = mime_dwg::inspect(input.data, input.size, input.complete != 0);
    if (dwg.identified) {
	bu_mime_traits traits = BU_MIME_TRAITS_INIT_ZERO;
	const bool valid = identify(registry, result, mime_dwg::type, BU_MIME_EVIDENCE_HEURISTIC) &&
	    dwg.append_traits(traits) && bu_mime_result_add_traits(result, mime_dwg::type, &traits);
	bu_mime_traits_free(&traits);
	if (!valid) return false;
    }
    if (dwg.incomplete) result->incomplete = 1;
    static const char rhino_magic[] = "3D Geometry File Format ";
    const size_t rhino_header_size = 32;
    if (scene_signature(input, rhino_magic, sizeof(rhino_magic) - 1, result)) {
	const char *type = "model/vnd.mcneel.3dm";
	if (!identify(registry, result, type, BU_MIME_EVIDENCE_SIGNATURE)) return false;
	if (input.size < rhino_header_size) {
	    if (!input.complete) result->incomplete = 1;
	    return true;
	}
	std::string field(reinterpret_cast<const char *>(input.data + sizeof(rhino_magic) - 1),
	    rhino_header_size - (sizeof(rhino_magic) - 1));
	/* The SDK accepts X as the historical experimental version-2 spelling. */
	if (field.back() == 'X') field.back() = '2';
	size_t first = field.find_first_not_of(' ');
	std::string version = first == std::string::npos ? "" : decimal_field(field.substr(first));
	return version.empty() || version == "0" ||
	    (fact(result, type, "version", version) && fact(result, type, "encoding", "binary") &&
	    fact(result, type, "byte-order", "little-endian"));
    }
    static const char jt_magic[] = "Version ";
    const size_t jt_version_size = 80;
    if (!scene_signature(input, jt_magic, sizeof(jt_magic) - 1, result)) return true;
    if (input.size <= jt_version_size) {
	if (!input.complete) result->incomplete = 1;
	return true;
    }
    if (input.data[jt_version_size] > 1) return true;
    std::string header(reinterpret_cast<const char *>(input.data), jt_version_size);
    size_t begin = sizeof(jt_magic) - 1;
    size_t dot = header.find('.', begin);
    if (dot == std::string::npos) return true;
    std::string major = decimal_field(header.substr(begin, dot - begin));
    size_t end = dot + 1;
    while (end < header.size() && std::isdigit(static_cast<unsigned char>(header[end]))) ++end;
    std::string minor = decimal_field(header.substr(dot + 1, end - dot - 1));
    if (major.empty() || minor.empty()) return true;
    const char *type = "model/jt";
    return identify(registry, result, type, BU_MIME_EVIDENCE_HEURISTIC) &&
	fact(result, type, "version", major + '.' + minor) && fact(result, type, "jt.major-version", major) &&
	fact(result, type, "encoding", "binary") &&
	fact(result, type, "byte-order", input.data[jt_version_size] ? "big-endian" : "little-endian");
}

bool pnm_profile(bu_mime_registry *registry, const bu_mime_probe_input &input, bu_mime_result *result)
{
    mime_pnm_input source = {NULL, input.data, input.data ? input.data + input.size : NULL, 0};
    mime_pnm_header header;
    const bool complete = mime_pnm_header_read(&source, &header) != 0;
    if (!header.identified) return true;
    const char *type = mime_pnm_type(&header);
    if (!identify(registry, result, type, BU_MIME_EVIDENCE_SIGNATURE)) return false;
    bu_mime_traits traits = BU_MIME_TRAITS_INIT_ZERO;
    const bool valid = mime_pnm_append_traits(&header, &traits) && bu_mime_result_add_traits(result, type, &traits);
    bu_mime_traits_free(&traits);
    if (!complete && source.ended && !input.complete) result->incomplete = 1;
    return valid;
}

bool jpeg_profile(bu_mime_registry *registry, const bu_mime_probe_input &input, bu_mime_result *result)
{
    const char *type = "image/jpeg";
    if (input.size < 3 || input.data[0] != 0xff || input.data[1] != 0xd8 || input.data[2] != 0xff) return true;
    if (!identify(registry, result, type, BU_MIME_EVIDENCE_SIGNATURE) || !fact(result, type, "encoding", "binary"))
	return false;
    size_t at = 2;
    while (at < input.size) {
	if (input.data[at++] != 0xff) return true;
	while (at < input.size && input.data[at] == 0xff) ++at;
	if (at == input.size) break;
	unsigned marker = input.data[at++];
	if (!marker || marker == 0xd9 || marker == 0xda) return true;
	if (marker == 1 || (marker >= 0xd0 && marker <= 0xd7)) continue;
	if (input.size - at < 2) break;
	size_t length = (size_t(input.data[at]) << 8) | input.data[at + 1];
	if (length < 2) return true;
	if (length > input.size - at) break;
	bool frame = marker >= 0xc0 && marker <= 0xcf && marker != 0xc4 && marker != 0xc8 && marker != 0xcc;
	if (frame) {
	    const size_t frame_header = 8;
	    const size_t component_bytes = 3;
	    if (length < frame_header || !input.data[at + 7] ||
		length != frame_header + component_bytes * input.data[at + 7] || !input.data[at + 2]) return true;
	    std::vector<std::string> features;
	    const unsigned arithmetic_mask = 0x08;
	    const unsigned differential_mask = 0x04;
	    const unsigned process_mask = 0x03;
	    const unsigned progressive_process = 2;
	    const unsigned lossless_process = 3;
	    if (marker & arithmetic_mask) features.emplace_back("arithmetic");
	    if (marker & differential_mask) features.emplace_back("differential");
	    if ((marker & process_mask) == progressive_process) features.emplace_back("progressive");
	    if ((marker & process_mask) == lossless_process) features.emplace_back("lossless");
	    std::string sampling;
	    for (size_t i = 0; i < input.data[at + 7]; ++i) {
		unsigned factors = input.data[at + frame_header + component_bytes * i + 1];
		if (!(factors >> 4) || !(factors & 0x0f)) return true;
		if (!sampling.empty()) sampling += ',';
		sampling += std::to_string(factors >> 4) + 'x' + std::to_string(factors & 0x0f);
	    }
	    return fact(result, type, "bit-depth", std::to_string(input.data[at + 2])) &&
		fact(result, type, "channels", std::to_string(input.data[at + 7])) &&
		fact(result, type, "jpeg.sampling-factors", sampling) &&
		set_fact(result, type, "jpeg.frame-features", features);
	}
	at += length;
    }
    if (!input.complete) result->incomplete = 1;
    return true;
}

bool binary_scene_profiles(bu_mime_registry *registry, const bu_mime_probe_input &input, bu_mime_result *result)
{
    const size_t no_version_word = 0;
    const struct {
	const char *magic;
	size_t magic_size;
	size_t version_offset;
	const char *type;
    } formats[] = {
	{"IDPO", sizeof("IDPO") - 1, sizeof("IDPO") - 1, "model/vnd.idsoftware.mdl"},
	{"IDST", sizeof("IDST") - 1, sizeof("IDST") - 1, "model/vnd.valve.mdl"},
	{"IDSQ", sizeof("IDSQ") - 1, sizeof("IDSQ") - 1, "model/vnd.valve.mdl"},
	{"IDP2", sizeof("IDP2") - 1, sizeof("IDP2") - 1, "model/vnd.idsoftware.md2"},
	{"IDP3", sizeof("IDP3") - 1, sizeof("IDP3") - 1, "model/vnd.idsoftware.md3"},
	{"INTERQUAKEMODEL", sizeof("INTERQUAKEMODEL"), sizeof("INTERQUAKEMODEL"), "model/vnd.interquake.iqm"},
	{"BB3D", sizeof("BB3D") - 1, sizeof("BB3D") - 1 + sizeof(uint32_t), "model/vnd.blitzresearch.b3d"},
	{"IDPC", sizeof("IDPC") - 1, sizeof("IDPC") - 1, "model/vnd.idsoftware.mdc"},
	{"MS3D000000", sizeof("MS3D000000") - 1, sizeof("MS3D000000") - 1, "model/vnd.chumbalumsoft.ms3d"},
	{"TERRAGENTERRAIN ", sizeof("TERRAGENTERRAIN ") - 1, no_version_word, "application/terragen"}
    };
    for (const auto &format : formats) {
	if (!scene_signature(input, format.magic, format.magic_size, result)) continue;
	if (!identify(registry, result, format.type, BU_MIME_EVIDENCE_SIGNATURE) ||
	    !fact(result, format.type, "encoding", "binary") ||
	    !fact(result, format.type, "byte-order", "little-endian"))
	    return false;
	if (BU_STR_EQUAL(format.type, "model/vnd.valve.mdl") &&
	    !fact(result, format.type, "valve.mdl-kind", format.magic[3] == 'Q' ? "sequence-group" : "model"))
	    return false;
	if (BU_STR_EQUAL(format.type, "model/vnd.blitzresearch.b3d")) {
	    const size_t chunk_header_size = 2 * sizeof(uint32_t);
	    if (input.size < chunk_header_size) {if (!input.complete) result->incomplete = 1; return true;}
	    const uint32_t payload_size = little_u32(input.data + sizeof(uint32_t));
	    if (!fact(result, format.type, "b3d.payload-size", std::to_string(payload_size))) return false;
	    if (payload_size < sizeof(uint32_t)) return true;
	    const size_t header_size = chunk_header_size + sizeof(uint32_t);
	    if (input.size < header_size) {if (!input.complete) result->incomplete = 1; return true;}
	    const uint32_t code = little_u32(input.data + chunk_header_size);
	    if (!fact(result, format.type, "b3d.version-code", std::to_string(code))) return false;
	    /* The published word packs major*100+minor. Preserve invalid signed
	     * values as raw words without interpreting them as future revisions. */
	    if (code > static_cast<uint32_t>(INT32_MAX)) return true;
	    constexpr uint32_t version_factor = 100;
	    const uint32_t minor = code % version_factor;
	    const std::string version = std::to_string(code / version_factor) + '.' +
		(minor < 10 ? "0" : "") + std::to_string(minor);
	    return fact(result, format.type, "version", version);
	}
	if (format.version_offset == no_version_word) return true;
	if (input.size >= format.version_offset + sizeof(uint32_t))
	    return fact(result, format.type, "version", std::to_string(little_u32(input.data + format.version_offset)));
	if (!input.complete) result->incomplete = 1;
	return true;
    }
    return true;
}

bool model_header_profiles(bu_mime_registry *registry, const bu_mime_probe_input &input, bu_mime_result *result)
{
    const size_t bsp_magic_size = sizeof("IBSP") - 1;
    if (scene_signature(input, "IBSP", bsp_magic_size, result)) {
	const char *type = "model/vnd.idsoftware.bsp";
	if (!identify(registry, result, type, BU_MIME_EVIDENCE_SIGNATURE) ||
	    !fact(result, type, "encoding", "binary") || !fact(result, type, "byte-order", "little-endian")) return false;
	if (input.size >= bsp_magic_size + sizeof(uint32_t))
	    return fact(result, type, "version", std::to_string(little_u32(input.data + bsp_magic_size)));
	if (!input.complete) result->incomplete = 1;
	return true;
    }
    const size_t ogre_header_size = sizeof(uint16_t);
    const unsigned char ogre_header_high_byte = 0x10; // Ogre's header chunk ID is 0x1000.
    const std::string ogre_tag = "[MeshSerializer_v";
    if (input.size >= ogre_header_size + ogre_tag.size() &&
	((input.data[0] == 0 && input.data[1] == ogre_header_high_byte) || (input.data[0] == ogre_header_high_byte && input.data[1] == 0)) &&
	!memcmp(input.data + ogre_header_size, ogre_tag.data(), ogre_tag.size())) {
	const char *type = "model/vnd.ogre.mesh";
	if (!identify(registry, result, type, BU_MIME_EVIDENCE_SIGNATURE) ||
	    !fact(result, type, "encoding", "binary") ||
	    !fact(result, type, "byte-order", input.data[0] ? "big-endian" : "little-endian")) return false;
	std::string header(reinterpret_cast<const char *>(input.data + ogre_header_size), input.size - ogre_header_size);
	size_t close = header.find(']');
	if (close != std::string::npos) {
	    if (!fact(result, type, "ogre.serializer", header.substr(0, close + 1))) return false;
	    std::string version = header.substr(ogre_tag.size(), close - ogre_tag.size());
	    int compared;
	    if (bu_mime_version_compare(version.c_str(), version.c_str(), &compared) && !fact(result, type, "version", version)) return false;
	} else if (!input.complete) result->incomplete = 1;
	return true;
    }
    const size_t quick3d_magic_size = sizeof("quick3Do") - 1;
    if (scene_signature(input, "quick3Do", quick3d_magic_size, result) ||
	scene_signature(input, "quick3Ds", quick3d_magic_size, result)) {
	const char *type = "model/vnd.quick3d";
	if (!identify(registry, result, type, BU_MIME_EVIDENCE_SIGNATURE) || !fact(result, type, "encoding", "binary") ||
	    !fact(result, type, "byte-order", "little-endian") ||
	    !fact(result, type, "quick3d.kind", input.data[quick3d_magic_size - 1] == 'o' ? "object" : "scene")) return false;
	if (input.size >= quick3d_magic_size + 2) {
	    unsigned char major = input.data[quick3d_magic_size], minor = input.data[quick3d_magic_size + 1];
	    if (major >= '0' && major <= '9' && minor >= '0' && minor <= '9')
		return fact(result, type, "version", std::string(1, major) + '.' + std::string(1, minor));
	} else if (!input.complete) result->incomplete = 1;
	return true;
    }
    const size_t pmx_magic_size = sizeof("PMX ") - 1;
    if (!scene_signature(input, "PMX ", pmx_magic_size, result)) return true;
    const char *type = "model/vnd.mikumikudance.pmx";
    if (!identify(registry, result, type, BU_MIME_EVIDENCE_SIGNATURE) || !fact(result, type, "encoding", "binary") ||
	!fact(result, type, "byte-order", "little-endian")) return false;
    const size_t globals_offset = pmx_magic_size + sizeof(uint32_t), globals_size = 8;
    if (input.size >= globals_offset) {
	uint32_t bits = little_u32(input.data + pmx_magic_size);
	if (!fact(result, type, "pmx.version-bits", std::to_string(bits))) return false;
	/* Compare IEEE binary32 declarations without host float assumptions.
	 * Unfamiliar words remain observable without inventing a version. */
	if (bits == 0x40000000 && !fact(result, type, "version", "2.0")) return false;
	if (bits == 0x40066666 && !fact(result, type, "version", "2.1")) return false;
    }
    if (input.size <= globals_offset) {
	if (!input.complete) result->incomplete = 1;
	return true;
    }
    if (!fact(result, type, "pmx.header-size", std::to_string(input.data[globals_offset]))) return false;
    if (input.data[globals_offset] < globals_size) return true;
    size_t available = std::min(input.size - globals_offset - 1, globals_size);
    const unsigned char *globals = input.data + globals_offset + 1;
    if (available >= 1 && globals[0] <= 1 && !fact(result, type, "pmx.text-encoding", globals[0] ? "utf-8" : "utf-16le")) return false;
    if (available >= 2 && !fact(result, type, "pmx.additional-uv", std::to_string(globals[1]))) return false;
    const char *index_keys[] = {"pmx.vertex-index-size", "pmx.texture-index-size", "pmx.material-index-size",
	"pmx.bone-index-size", "pmx.morph-index-size", "pmx.rigidbody-index-size"};
    std::set<std::string> widths;
    for (size_t i = 2; i < available; ++i) {
	std::string width = std::to_string(globals[i]);
	if (!fact(result, type, index_keys[i - 2], width)) return false;
	widths.insert(width);
    }
    if (available == globals_size && !set_fact(result, type, "pmx.index-widths", {widths.begin(), widths.end()})) return false;
    if (input.size - globals_offset - 1 < input.data[globals_offset] && !input.complete) result->incomplete = 1;
    return true;
}

bool iff_scene_profiles(bu_mime_registry *registry, const bu_mime_probe_input &input, bu_mime_result *result)
{
    const size_t form_header_size = 3 * sizeof(uint32_t);
    const size_t chunk_header_size = 2 * sizeof(uint32_t);
    const size_t version_size = 2 * sizeof(uint32_t);
    if (!scene_signature(input, "FORM", sizeof("FORM") - 1, result)) return true;
    if (input.size < form_header_size) {
	if (!input.complete) result->incomplete = 1;
	return true;
    }
    uint32_t form_size = big_u32(input.data + sizeof(uint32_t));
    if (form_size && form_size < sizeof(uint32_t)) return true;
    std::string form(reinterpret_cast<const char *>(input.data + 2 * sizeof(uint32_t)), sizeof(uint32_t));
    const char *type = form == "LXOB" ? "model/vnd.foundry.lxo" :
	(form == "LWOB" || form == "LWO2" || form == "LWO3") ? "image/x-lwo" : NULL;
    if (!type) return true;
    if (!identify(registry, result, type, BU_MIME_EVIDENCE_SIGNATURE) || !fact(result, type, "iff.form", form) ||
	!fact(result, type, "encoding", "binary") || !fact(result, type, "byte-order", "big-endian")) return false;
    if (form != "LXOB") return true;
    /* Zero is the LXO convention for a FORM larger than its 32-bit size.
     * Revision words can be observed before the application string ends. */
    size_t available = input.size;
    if (form_size && uint64_t(form_size) + chunk_header_size < available)
	available = static_cast<size_t>(form_size) + chunk_header_size;
    size_t at = form_header_size;
    while (available - at >= chunk_header_size) {
	uint32_t size = big_u32(input.data + at + sizeof(uint32_t));
	const unsigned char *tag = input.data + at;
	at += chunk_header_size;
	if (!memcmp(tag, "VRSN", sizeof(uint32_t)) && size >= version_size && available - at >= version_size)
	    return fact(result, type, "version", std::to_string(big_u32(input.data + at)) + '.' +
		std::to_string(big_u32(input.data + at + sizeof(uint32_t))));
	uint64_t padded_size = uint64_t(size) + (size & 1);
	if (padded_size > available - at) break;
	at += static_cast<size_t>(padded_size);
    }
    if (!input.complete) result->incomplete = 1;
    return true;
}

bool conitec_scene_profiles(bu_mime_registry *registry, const bu_mime_probe_input &input, bu_mime_result *result)
{
    const struct { const char *magic; const char *versions; const char *type; } formats[] = {
	{"MDL", "23457", "model/vnd.conitec.mdl"}, {"HMP", "457", "image/vnd.conitec.hmp"}};
    const size_t tag_size = 4;
    for (const auto &format : formats) {
	if (!scene_signature(input, format.magic, tag_size - 1, result)) continue;
	if (input.size < tag_size) {
	    if (!input.complete) result->incomplete = 1;
	    return true;
	}
	if (!input.data[tag_size - 1] || !strchr(format.versions, input.data[tag_size - 1])) return true;
	/* GameStudio's format revision is part of the magic tag. Its next
	 * word is not the Quake/Valve version field and is ignored by the SDK. */
	return identify(registry, result, format.type, BU_MIME_EVIDENCE_SIGNATURE) &&
	    fact(result, format.type, "version", std::string(1, input.data[tag_size - 1])) &&
	    fact(result, format.type, "encoding", "binary") && fact(result, format.type, "byte-order", "little-endian");
    }
    return true;
}

bool container_scene_profiles(bu_mime_registry *registry, const bu_mime_probe_input &input, bu_mime_result *result)
{
    enum container_kind { NENDO, SILO, CALIGARI };
    const struct {
	const char *magic;
	size_t magic_size;
	size_t header_size;
	container_kind kind;
	const char *type;
    } formats[] = {
	{"nendo ", sizeof("nendo ") - 1, sizeof("nendo 1.0") - 1, NENDO, "model/vnd.izware.nendo"},
	{"HEAD", sizeof("HEAD") - 1, 3 * sizeof(uint32_t), SILO, "model/vnd.nevercenter.sib"},
	{"Caligari V", sizeof("Caligari V") - 1, sizeof("Caligari V00.01ALH") - 1, CALIGARI, "model/vnd.caligari.cob"}
    };
    for (const auto &format : formats) {
	if (!scene_signature(input, format.magic, format.magic_size, result)) continue;
	if (input.size < format.header_size) {
	    if (!input.complete) result->incomplete = 1;
	    continue;
	}
	std::string version;
	const char *encoding = "binary";
	const char *order = "little-endian";
	bu_mime_evidence evidence = BU_MIME_EVIDENCE_SIGNATURE;
	if (format.kind == NENDO) {
	    version.assign(reinterpret_cast<const char *>(input.data + format.magic_size), sizeof("1.0") - 1);
	    if (version[1] != '.') continue;
	    order = "big-endian";
	} else if (format.kind == SILO) {
	    uint32_t chunk_size = little_u32(input.data + sizeof(uint32_t));
	    const size_t chunk_header_size = 2 * sizeof(uint32_t);
	    if (chunk_size < sizeof(uint32_t) || (input.file_size_known &&
		(input.file_size < chunk_header_size || chunk_size > input.file_size - chunk_header_size))) continue;
	    version = std::to_string(little_u32(input.data + chunk_header_size));
	    /* HEAD is a generic chunk name, so it cannot establish a signature
	     * with the same confidence as a format-specific magic string. */
	    evidence = BU_MIME_EVIDENCE_HEURISTIC;
	} else {
	    const size_t encoding_offset = sizeof("Caligari V00.01") - 1;
	    const size_t byte_order_offset = encoding_offset + 1;
	    const size_t handedness_offset = byte_order_offset + 1;
	    version.assign(reinterpret_cast<const char *>(input.data + format.magic_size), sizeof("00.01") - 1);
	    if (version[2] != '.' || input.data[handedness_offset] != 'H') continue;
	    if (input.data[encoding_offset] == 'A') encoding = "ascii";
	    else if (input.data[encoding_offset] != 'B') continue;
	    if (input.data[byte_order_offset] == 'B') order = "big-endian";
	    else if (input.data[byte_order_offset] != 'L') continue;
	}
	int comparison = 0;
	if (!bu_mime_version_compare(version.c_str(), version.c_str(), &comparison)) continue;
	if (!identify(registry, result, format.type, evidence) || !fact(result, format.type, "version", version) ||
	    !fact(result, format.type, "encoding", encoding) || !fact(result, format.type, "byte-order", order)) return false;
    }
    return true;
}

bool sun_profile(bu_mime_registry *registry, const bu_mime_probe_input &input, bu_mime_result *result)
{
    if (input.size < sizeof(mime_sun::signature)) {
	if (!input.complete && input.size && !memcmp(input.data, mime_sun::signature, input.size))
	    result->incomplete = 1;
	return true;
    }
    const mime_sun::header header(input.data, input.size);
    if (!header.identified()) return true;
    const char *type = "image/x-sun-raster";
    bu_mime_traits traits = BU_MIME_TRAITS_INIT_ZERO;
    const bool valid = identify(registry, result, type, BU_MIME_EVIDENCE_SIGNATURE) &&
	header.append_traits(traits) && bu_mime_result_add_traits(result, type, &traits);
    bu_mime_traits_free(&traits);
    if (!header.complete() && !input.complete) result->incomplete = 1;
    return valid;
}

template<typename Profile>
bool publish_profile(bu_mime_registry *registry, bu_mime_result *result, const char *type,
    const Profile &profile, bu_mime_evidence evidence, unsigned int priority = BU_MIME_DEFAULT_PRIORITY)
{
    bu_mime_traits traits = BU_MIME_TRAITS_INIT_ZERO;
    const bool valid = identify(registry, result, type, evidence, priority) &&
	profile.append_traits(traits) && bu_mime_result_add_traits(result, type, &traits);
    bu_mime_traits_free(&traits);
    return valid;
}

bool raster_container_profiles(bu_mime_registry *registry, const bu_mime_probe_input &input, bu_mime_result *result)
{
    std::istringstream label_input(std::string(reinterpret_cast<const char *>(input.data), input.size));
    const auto label = mime_raster_label::inspect(label_input, input.complete != 0);
    if (label.identified && !publish_profile(registry, result, label.type(), label, BU_MIME_EVIDENCE_HEURISTIC)) return false;
    if (label.incomplete) result->incomplete = 1;
    const auto iris = mime_iris::inspect(input);
    if (iris.incomplete) result->incomplete = 1;
    if (iris.identified && !publish_profile(registry, result, mime_iris::type, iris, BU_MIME_EVIDENCE_HEURISTIC)) return false;
    if (iris.product_view() && !publish_profile(registry, result, iris.product_view(), iris,
	BU_MIME_EVIDENCE_HEURISTIC, mime_iris::product_view_priority)) return false;
    const auto noaa = mime_noaa_grid::inspect(input.data, input.size);
    if (noaa.identified && !publish_profile(registry, result, mime_noaa_grid::type, noaa, BU_MIME_EVIDENCE_HEURISTIC)) return false;
    const auto nsidc = mime_nsidc::inspect(input.data, input.size);
    if (nsidc.identified && !publish_profile(registry, result, mime_nsidc::type, nsidc, BU_MIME_EVIDENCE_HEURISTIC)) return false;
    const auto hf2 = mime_hf2::inspect(input.data, input.size, input.complete != 0);
    if (hf2.identified && !publish_profile(registry, result, mime_hf2::type, hf2, BU_MIME_EVIDENCE_SIGNATURE)) return false;
    if (hf2.incomplete) result->incomplete = 1;
    const auto sigdem = mime_sigdem::inspect(input.data, input.size, input.complete != 0);
    if (sigdem.identified && !publish_profile(registry, result, mime_sigdem::type, sigdem, BU_MIME_EVIDENCE_SIGNATURE)) return false;
    if (sigdem.incomplete) result->incomplete = 1;
    const auto byn = mime_byn::inspect(input.data, input.size);
    if (byn.identified && !publish_profile(registry, result, mime_byn::type, byn, BU_MIME_EVIDENCE_HEURISTIC)) return false;
    std::istringstream isg_input(std::string(reinterpret_cast<const char *>(input.data), input.size));
    const auto isg = mime_isg::inspect(isg_input, input.complete != 0, input.complete != 0);
    if (isg.identified && !publish_profile(registry, result, mime_isg::type, isg, BU_MIME_EVIDENCE_HEURISTIC)) return false;
    if (isg.incomplete) result->incomplete = 1;
    scene_signature(input, mime_lerc::lerc1_signature, sizeof(mime_lerc::lerc1_signature) - 1, result);
    scene_signature(input, mime_lerc::lerc2_signature, sizeof(mime_lerc::lerc2_signature) - 1, result);
    const auto leveller = mime_leveller::inspect(input.data, input.size, input.complete != 0);
    if (leveller.identified && !publish_profile(registry, result, mime_leveller::type, leveller, BU_MIME_EVIDENCE_SIGNATURE)) return false;
    if (leveller.incomplete) result->incomplete = 1;
    const auto lerc = mime_lerc::inspect(input.data, input.size, input.complete != 0);
    if (lerc.identified && !publish_profile(registry, result, mime_lerc::type, lerc, BU_MIME_EVIDENCE_SIGNATURE)) return false;
    if (lerc.incomplete) result->incomplete = 1;
    const auto tar = mime_tar::inspect(input.data, input.size, input.complete != 0);
    if (tar.identified && !publish_profile(registry, result, mime_tar::type, tar, BU_MIME_EVIDENCE_SIGNATURE)) return false;
    if (tar.incomplete) result->incomplete = 1;
    const auto esric = mime_esric::inspect(input.data, input.size, input.complete != 0);
    if (esric.identified && !publish_profile(registry, result, mime_esric::json_type, esric, BU_MIME_EVIDENCE_HEURISTIC)) return false;
    if (esric.incomplete) result->incomplete = 1;
    const auto stac = mime_stac::inspect(input.data, input.size, input.complete != 0);
    if (stac.identified && !publish_profile(registry, result, mime_stac::type, stac, BU_MIME_EVIDENCE_HEURISTIC)) return false;
    if (stac.incomplete) result->incomplete = 1;
    const auto zarr = mime_zarr::inspect(input.data, input.size, input.complete != 0);
    if (zarr.identified) {
	bu_mime_traits traits = BU_MIME_TRAITS_INIT_ZERO;
	const bool valid = identify(registry, result, mime_zarr::type, BU_MIME_EVIDENCE_HEURISTIC) &&
	    zarr.append_traits(traits, "metadata") && bu_mime_result_add_traits(result, mime_zarr::type, &traits);
	bu_mime_traits_free(&traits);
	if (!valid) return false;
    }
    if (zarr.incomplete) result->incomplete = 1;
    const auto vicar = mime_vicar::inspect(input.data, input.size, input.complete != 0);
    if (vicar.identified && !publish_profile(registry, result, mime_vicar::type, vicar, BU_MIME_EVIDENCE_HEURISTIC)) return false;
    if (vicar.incomplete) result->incomplete = 1;
    const auto cals = mime_cals::inspect(input.data, input.size);
    if (cals.identified) {
	if (!publish_profile(registry, result, mime_cals::type, cals,
	    cals.type_one() ? BU_MIME_EVIDENCE_SIGNATURE : BU_MIME_EVIDENCE_HEURISTIC)) return false;
	if (!cals.complete && !input.complete) result->incomplete = 1;
    }
    scene_signature(input, mime_bmp::signature, sizeof(mime_bmp::signature), result);
    const auto bitmap = mime_bmp::inspect(input.data, input.size);
    if (bitmap.identified) {
	if (!publish_profile(registry, result, "image/bmp", bitmap,
	    bitmap.size_known && bitmap.known_layout() ? BU_MIME_EVIDENCE_SIGNATURE : BU_MIME_EVIDENCE_HEURISTIC)) return false;
	if (bitmap.incomplete && !input.complete) result->incomplete = 1;
    }
    scene_signature(input, "NITF", mime_nitf::signature_size, result);
    scene_signature(input, "NSIF", mime_nitf::signature_size, result);
    const auto nitf = mime_nitf::inspect(input.data, input.size);
    if (nitf.identified) {
	if (!publish_profile(registry, result, "application/vnd.nitf", nitf,
	    nitf.complete ? BU_MIME_EVIDENCE_SIGNATURE : BU_MIME_EVIDENCE_HEURISTIC)) return false;
	if (!nitf.complete && !input.complete) result->incomplete = 1;
    }
    const bool sqlite = scene_signature(input, mime_geopackage::sqlite_signature,
	sizeof(mime_geopackage::sqlite_signature), result);
    const auto geopackage = mime_geopackage::inspect(input.data, input.size);
    if (geopackage.identified && !publish_profile(registry, result, "application/geopackage+sqlite3",
	geopackage, BU_MIME_EVIDENCE_SIGNATURE)) return false;
    if (sqlite && input.size < mime_geopackage::header_size && !input.complete) result->incomplete = 1;
    scene_signature(input, mime_grib::signature, sizeof(mime_grib::signature), result);
    const auto grib = mime_grib::inspect(input.data, input.size);
    if (grib.identified) {
	if (!publish_profile(registry, result, "application/grib", grib,
	    grib.header_known && !grib.offset ? BU_MIME_EVIDENCE_SIGNATURE : BU_MIME_EVIDENCE_HEURISTIC)) return false;
	if (grib.incomplete && !input.complete) result->incomplete = 1;
    }
    return true;
}

bool stl(bu_mime_registry *registry, const bu_mime_probe_input &input, bu_mime_result *result)
{
    struct mime_stl_profile profile = MIME_STL_PROFILE_INIT;
    bu_mime_evidence evidence = BU_MIME_EVIDENCE_HEURISTIC;
    if (mime_stl_binary_matches(&input)) {
	profile.kind = MIME_STL_BINARY;
	if (mime_stl_count(&input)) evidence = BU_MIME_EVIDENCE_SIGNATURE;
    } else {
	struct mime_stl_text text = MIME_STL_TEXT_INIT;
	mime_stl_text_feed(&text, input.data, input.size);
	if (mime_stl_text_identified(&text, input.complete)) {
	    profile.kind = MIME_STL_ASCII;
	    profile.bom = text.bom;
	    profile.ascii = input.complete;
	    for (size_t at = 0; profile.ascii && at < input.size; ++at)
		if (input.data[at] > 0x7f) profile.ascii = 0;
	} else if (!input.complete && text.phase < MIME_STL_DONE) result->incomplete = 1;
    }
    if (profile.kind == MIME_STL_UNKNOWN) return true;
    bu_mime_traits traits = BU_MIME_TRAITS_INIT_ZERO;
    const bool valid = identify(registry, result, "model/stl", evidence) && mime_stl_append(&profile, &traits) &&
	bu_mime_result_add_traits(result, "model/stl", &traits);
    bu_mime_traits_free(&traits);
    return valid;
}

bool step(bu_mime_registry *registry, bu_mime_result *result, const std::string &text, const bu_mime_probe_input &input)
{
    std::istringstream stream(text);
    auto header = mime_p21::inspect(stream, input.complete != 0);
    if (header.incomplete) result->incomplete = 1;
    if (!header.complete) return true;
    bu_mime_traits traits = BU_MIME_TRAITS_INIT_ZERO;
    bool valid = header.append_traits(traits) && bu_mime_result_add_traits(result, "application/p21", &traits);
    bu_mime_traits_free(&traits);
    if (!valid) return false;
    if (!header.step_model()) return true;
    const char *type = "model/step";
    valid = identify(registry, result, type, BU_MIME_EVIDENCE_SIGNATURE) &&
	header.append_step_traits(traits) && bu_mime_result_add_traits(result, type, &traits);
    bu_mime_traits_free(&traits);
    if (!valid) return false;
    return true;
}

std::string line(const std::string &text, size_t &at, bool complete)
{
    size_t end = text.find('\n', at);
    if (end == std::string::npos && !complete) {
	at = text.size();
	return "";
    }
    std::string value = trim(text.substr(at, end == std::string::npos ? end : end - at));
    at = end == std::string::npos ? text.size() : end + 1;
    return value;
}
}

static int
asc_profile(bu_mime_registry *registry, const bu_mime_probe_input *input, bu_mime_result *result)
{
    std::string text(reinterpret_cast<const char *>(input->data), input->size);
    if (text.find('\0') != std::string::npos) return 1;
    size_t at = 0;
    while (at < text.size()) {
	std::string record = trim(line(text, at, input->complete));
	if (record.empty() || record[0] == '#') continue;
	std::istringstream fields(record);
	std::string command;
	fields >> command;
	const char *syntax = NULL;
	if (command == "title" || command == "units" || command == "put" || command == "attr" || command == "color")
	    syntax = "tcl-commands";
	if (command == "I") {
	    unsigned int units = 0;
	    std::string version;
	    if (fields >> units >> version && version == "v4") syntax = "legacy-records";
	}
	if (!syntax) return 1;
	return identify(registry, result, "application/vnd.brlcad.script", BU_MIME_EVIDENCE_HEURISTIC) &&
	    fact(result, "application/vnd.brlcad.script", "asc.syntax", syntax);
    }
    return 1;
}

static bool
fastgen_profile(bu_mime_registry *registry, const bu_mime_probe_input &input, bu_mime_result *result)
{
    std::string text(reinterpret_cast<const char *>(input.data), input.size);
    if (text.find('\0') != std::string::npos) return true;
    size_t at = 0;
    while (at < text.size()) {
	std::string record = line(text, at, input.complete);
	const size_t field_size = 8;
	if (record.compare(0, field_size, "SECTION ")) continue;
	/* GRID and ENDDATA also occur in Nastran. A SECTION with three
	 * fixed-width integer fields and a valid plate/volume mode is a
	 * conservative indication of the FASTGEN4 card dialect. */
	std::vector<std::string> fields;
	for (size_t field = 1; field <= 3 && field * field_size < record.size(); ++field) {
	    std::string value = decimal_field(trim(record.substr(field * field_size, field_size)));
	    if (value.empty()) break;
	    fields.push_back(value);
	}
	if (fields.size() != 3 || (fields[2] != "1" && fields[2] != "2")) continue;
	const char *type = "model/vnd.fastgen";
	return identify(registry, result, type, BU_MIME_EVIDENCE_HEURISTIC) &&
	    fact(result, type, "fastgen.dialect", "4") && fact(result, type, "encoding", "ascii");
    }
    return true;
}

static bool
off_profile(bu_mime_registry *registry, const bu_mime_probe_input &input, bu_mime_result *result)
{
    std::istringstream stream(std::string(reinterpret_cast<const char *>(input.data), input.size));
    const auto profile = mime_off::inspect(stream, input.complete);
    if (profile.identified && profile.incomplete) result->incomplete = 1;
    return !profile.identified || publish_profile(registry, result, mime_off::type,
	profile, BU_MIME_EVIDENCE_SIGNATURE);
}

static bool
terrain_profiles(bu_mime_registry *registry, const bu_mime_probe_input &input, bu_mime_result *result)
{
    const auto publish = [&](const mime_terrain::profile &profile, bu_mime_evidence evidence) {
	if (profile.incomplete) result->incomplete = 1;
	return !profile.type || publish_profile(registry, result, profile.type, profile, evidence);
    };
    const std::string data(reinterpret_cast<const char *>(input.data), input.size);
    const auto inspect = [&](auto reader, bu_mime_evidence evidence) {
	std::istringstream stream(data);
	return publish(reader(stream, input.complete != 0), evidence);
    };
    return publish(mime_terrain::binary(input), BU_MIME_EVIDENCE_SIGNATURE) &&
	inspect(mime_terrain::dted, BU_MIME_EVIDENCE_SIGNATURE) &&
	inspect([](std::istream &stream, bool whole) {return mime_terrain::surfer_text(stream, whole, true);}, BU_MIME_EVIDENCE_SIGNATURE) &&
	inspect(mime_terrain::grass, BU_MIME_EVIDENCE_HEURISTIC) &&
	inspect(mime_terrain::ascii_grid, BU_MIME_EVIDENCE_HEURISTIC) &&
	inspect([](std::istream &stream, bool whole) {return mime_terrain::gxf(stream, whole);}, BU_MIME_EVIDENCE_HEURISTIC) &&
	inspect([](std::istream &stream, bool whole) {return mime_terrain::zmap(stream, whole);}, BU_MIME_EVIDENCE_HEURISTIC);
}

static bool
sdk_header_profiles(bu_mime_registry *registry, const bu_mime_probe_input &input, bu_mime_result *result)
{
    std::istringstream obj_stream(std::string(reinterpret_cast<const char *>(input.data), input.size));
    const auto obj = mime_obj::inspect(obj_stream, input.complete);
    if (obj.incomplete) result->incomplete = 1;
    if (obj.identified && !publish_profile(registry, result, mime_obj::type, obj, BU_MIME_EVIDENCE_HEURISTIC)) return false;
    std::istringstream opengex_stream(std::string(reinterpret_cast<const char *>(input.data), input.size));
    const auto opengex = mime_opengex::inspect(opengex_stream, input.complete);
    if (opengex.incomplete) result->incomplete = 1;
    if (opengex.identified && !publish_profile(registry, result, mime_opengex::type, opengex, BU_MIME_EVIDENCE_HEURISTIC)) return false;
    std::istringstream fbx_stream(std::string(reinterpret_cast<const char *>(input.data), input.size));
    const auto fbx = mime_fbx::inspect(fbx_stream, input.complete);
    if (fbx.incomplete) result->incomplete = 1;
    if (fbx.identified && !publish_profile(registry, result, mime_fbx::type, fbx,
	fbx.serialization == "binary" ? BU_MIME_EVIDENCE_SIGNATURE : BU_MIME_EVIDENCE_HEURISTIC)) return false;
    const auto three_ds = mime_3ds::inspect(input);
    if (three_ds.incomplete) result->incomplete = 1;
    /* The short main-chunk ID also starts big-endian TIFF. A plausible
     * chunk length must not outrank that format's longer signature. */
    if (three_ds.identified && !publish_profile(registry, result, mime_3ds::type, three_ds, BU_MIME_EVIDENCE_HEURISTIC)) return false;
    const auto binary = mime_assbin::inspect(input);
    if (binary.incomplete) result->incomplete = 1;
    if (binary.identified && !publish_profile(registry, result, mime_assbin::type, binary, BU_MIME_EVIDENCE_SIGNATURE)) return false;
    std::istringstream stream(std::string(reinterpret_cast<const char *>(input.data), input.size));
    const auto x = mime_x::inspect(stream, input.complete);
    if (x.incomplete) result->incomplete = 1;
    if (x.identified && !publish_profile(registry, result, mime_x::type, x, BU_MIME_EVIDENCE_SIGNATURE)) return false;
    std::istringstream ply_stream(std::string(reinterpret_cast<const char *>(input.data), input.size));
    const auto ply = mime_ply::inspect(ply_stream, input.complete);
    if (ply.incomplete) result->incomplete = 1;
    return !ply.identified || publish_profile(registry, result, mime_ply::type, ply, BU_MIME_EVIDENCE_SIGNATURE);
}

static bool
scene_text_profile(bu_mime_registry *registry, const bu_mime_probe_input &input, bu_mime_result *result)
{
    std::istringstream stream(std::string(reinterpret_cast<const char *>(input.data), input.size));
    const auto profile = mime_scene_text::inspect(stream, input.complete);
    if (profile.incomplete) result->incomplete = 1;
    return profile.type.empty() || publish_profile(registry, result, profile.type.c_str(),
	profile, BU_MIME_EVIDENCE_SIGNATURE);
}

static bool
blender_profile(bu_mime_registry *registry, const bu_mime_probe_input &input, bu_mime_result *result)
{
    const auto profile = mime_blender::inspect(input);
    if (profile.incomplete) result->incomplete = 1;
    return !profile.identified || publish_profile(registry, result, mime_blender::type,
	profile, BU_MIME_EVIDENCE_SIGNATURE);
}

static bool
dxf_profile(bu_mime_registry *registry, const bu_mime_probe_input &input, bu_mime_result *result)
{
    std::istringstream stream(std::string(reinterpret_cast<const char *>(input.data), input.size));
    const auto profile = mime_dxf::inspect(stream, input.complete);
    if (profile.incomplete) result->incomplete = 1;
    return !profile.identified || publish_profile(registry, result, mime_dxf::type,
	profile, BU_MIME_EVIDENCE_SIGNATURE);
}

int
mime_inspect_builtin(bu_mime_registry *registry, const char *source_filename, const bu_mime_probe_input *input, bu_mime_result *result)
{
    if (!input->size)
	return 1;
    if (!mime_inspect_gltf(registry, input, result) || !cad_header_profiles(registry, *input, result) ||
	!pnm_profile(registry, *input, result) || !off_profile(registry, *input, result) ||
	!dxf_profile(registry, *input, result) || !blender_profile(registry, *input, result) ||
	!scene_text_profile(registry, *input, result) || !terrain_profiles(registry, *input, result) || !sdk_header_profiles(registry, *input, result) ||
	!jpeg_profile(registry, *input, result) ||
	!binary_scene_profiles(registry, *input, result) ||
	!model_header_profiles(registry, *input, result) ||
	!conitec_scene_profiles(registry, *input, result) ||
	!iff_scene_profiles(registry, *input, result) ||
	!container_scene_profiles(registry, *input, result) ||
	!sun_profile(registry, *input, result) || !raster_container_profiles(registry, *input, result) || !stl(registry, *input, result) ||
	!asc_profile(registry, input, result) || !fastgen_profile(registry, *input, result))
	return 0;
    std::string text(reinterpret_cast<const char *>(input->data), input->size);
    {
	std::string hint = source_filename ? source_filename : "";
	for (char &c : hint) if (c >= 'A' && c <= 'Z') c += 'a' - 'A';
	mime_unreal::files component(hint);
	if (component.part == "data" || component.part == "animation") {
	    auto profile = mime_unreal::inspect_binary(component.part, *input);
	    if (profile.valid) {
		bu_mime_traits traits = BU_MIME_TRAITS_INIT_ZERO;
		bool valid = identify(registry, result, mime_unreal::mesh_type, BU_MIME_EVIDENCE_HEURISTIC) &&
		    profile.append(traits) && bu_mime_result_add_traits(result, mime_unreal::mesh_type, &traits);
		bu_mime_traits_free(&traits);
		if (!valid) return 0;
	    }
	}
	std::istringstream stream(text);
	auto script = mime_unreal::inspect_script(stream, input->complete != 0);
	if (script.recognized && script.textual) {
	    bu_mime_traits traits = BU_MIME_TRAITS_INIT_ZERO;
	    bool valid = identify(registry, result, mime_unreal::script_type, BU_MIME_EVIDENCE_HEURISTIC) &&
		script.append(traits) && bu_mime_result_add_traits(result, mime_unreal::script_type, &traits);
	    bu_mime_traits_free(&traits);
	    if (!valid) return 0;
	    if (!script.complete) result->incomplete = 1;
	}
    }
    {
	std::istringstream stream(text);
	auto profile = mime_nff::inspect(stream, input->complete != 0);
	if (!profile.type.empty()) {
	    bu_mime_traits traits = BU_MIME_TRAITS_INIT_ZERO;
	    bool valid = identify(registry, result, profile.type.c_str(), profile.type == "model/vnd.sense8.nff" ?
		BU_MIME_EVIDENCE_SIGNATURE : BU_MIME_EVIDENCE_HEURISTIC) && profile.append_traits(traits) &&
		bu_mime_result_add_traits(result, profile.type.c_str(), &traits);
	    bu_mime_traits_free(&traits);
	    if (!valid) return 0;
	    if (!input->complete) result->incomplete = 1;
	}
    }
    {
	std::istringstream stream(text);
	auto profile = mime_skeletal::inspect(stream, input->complete != 0);
	if (!profile.type.empty()) {
	    if (!identify(registry, result, profile.type.c_str(), profile.type == "model/vnd.idsoftware.md5" ?
		BU_MIME_EVIDENCE_SIGNATURE : BU_MIME_EVIDENCE_HEURISTIC)) return 0;
	    for (const auto &value : profile.traits())
		if (!fact(result, profile.type.c_str(), value.first.c_str(), value.second)) return 0;
	    if (!input->complete) result->incomplete = 1;
	}
    }
    {
	size_t at = 0;
	std::string header = line(text, at, input->complete);
	std::string version;
	const char *type = NULL;
	if (header == "LWSC" || header == "LWMO") {
	    type = header == "LWSC" ? "image/x-lws" : "model/vnd.newtek.motion";
	    if (!identify(registry, result, type, BU_MIME_EVIDENCE_SIGNATURE) || !fact(result, type, "encoding", "ascii")) return 0;
	    version = decimal_field(trim(line(text, at, input->complete)));
	    if (version.empty() && !input->complete) result->incomplete = 1;
	}
	if (type && !version.empty() && version.find_first_not_of("0123456789") == std::string::npos) {
	    if (!identify(registry, result, type, BU_MIME_EVIDENCE_SIGNATURE) ||
		!fact(result, type, "version", version) || !fact(result, type, "encoding", "ascii"))
		return 0;
	}
    }
    if (text.compare(0, 6, "#VRML ") == 0) {
	size_t at = 0;
	std::istringstream declaration(line(text, at, input->complete));
	std::string magic, version, charset;
	int comparison = 0;
	if (declaration >> magic >> version >> charset && magic == "#VRML" &&
	    version.size() > 1 && version[0] == 'V' &&
	    bu_mime_version_compare(version.c_str() + 1, version.c_str() + 1, &comparison)) {
	    std::string encoding = charset;
	    if (BU_STR_EQUIV(charset.c_str(), "ascii")) encoding = "ascii";
	    if (BU_STR_EQUIV(charset.c_str(), "utf8")) encoding = "utf-8";
	    if (!identify(registry, result, "model/vrml", BU_MIME_EVIDENCE_SIGNATURE) ||
		!fact(result, "model/vrml", "version", version.substr(1)) ||
		!fact(result, "model/vrml", "encoding", encoding))
		return 0;
	}
    }
    const char vtk_header[] = "# vtk DataFile Version ";
    if (text.compare(0, sizeof(vtk_header) - 1, vtk_header) == 0) {
	if (!identify(registry, result, "application/vnd.kitware.vtk", BU_MIME_EVIDENCE_SIGNATURE))
	    return 0;
	size_t at = 0;
	line(text, at, input->complete);
	line(text, at, input->complete);
	std::string encoding = upper(line(text, at, input->complete));
	if (encoding == "ASCII" || encoding == "BINARY")
	    if (!fact(result, "application/vnd.kitware.vtk", "encoding", encoding == "ASCII" ? "ascii" : "binary"))
		return 0;
	std::istringstream dataset(line(text, at, input->complete));
	std::string keyword, kind;
	if (dataset >> keyword >> kind && keyword == "DATASET")
	    if (!fact(result, "application/vnd.kitware.vtk", "vtk.dataset", kind))
		return 0;
    }
    {
	std::istringstream stream(text);
	auto profile = mime_xml::inspect(stream);
	const char *type = profile.type();
	if (type) {
	    bu_mime_traits traits = BU_MIME_TRAITS_INIT_ZERO;
	    const bool raster_label = BU_STR_EQUAL(type, mime_raster_label::isce_label) ||
		BU_STR_EQUAL(type, mime_raster_label::photomod_label) || BU_STR_EQUAL(type, mime_esric::xml_type);
	    const bu_mime_evidence evidence = BU_STR_EQUAL(type, "model/amf") || raster_label ?
		BU_MIME_EVIDENCE_HEURISTIC : BU_MIME_EVIDENCE_SIGNATURE;
	    bool valid = identify(registry, result, type, evidence) &&
		profile.append_traits(traits) && (!raster_label || bu_mime_traits_add(&traits, "raster.entrypoint", "label")) &&
		bu_mime_result_add_traits(result, type, &traits);
	    bu_mime_traits_free(&traits);
	    if (!valid) return 0;
	} else if (profile.plausible && !input->complete) result->incomplete = 1;
    }
    if (text.compare(0, sizeof(mime_p21::signature) - 1, mime_p21::signature) == 0 && !step(registry, result, text, *input))
	return 0;
    /* Snapshot candidate names: identification may reorder/reallocate rows. */
    std::vector<std::string> names;
    for (size_t i = 0; i < result->count; ++i)
	names.emplace_back(result->candidates[i].name);
    mime_png::inspector png_header;
    if (std::find(names.begin(), names.end(), "image/png") != names.end() ||
	std::find(names.begin(), names.end(), "image/apng") != names.end()) png_header.feed(input->data, input->size);
    if (std::find(names.begin(), names.end(), "video/x-msvideo") != names.end()) {
	std::istringstream stream(text);
	const auto avi = mime_avi::inspect(stream);
	if (avi.identified) {
	    bu_mime_traits traits = BU_MIME_TRAITS_INIT_ZERO;
	    const bool valid = avi.append_traits(traits) && bu_mime_result_add_traits(result, "video/x-msvideo", &traits);
	    bu_mime_traits_free(&traits);
	    if (!valid) return 0;
	    if (!avi.complete && !input->complete) result->incomplete = 1;
	}
    }
    for (const auto &name : names) {
	if ((name == "image/png" || name == "image/apng") && png_header.facts.header_known) {
	    bu_mime_traits traits = BU_MIME_TRAITS_INIT_ZERO;
	    const bool valid = png_header.facts.append_traits(traits) && bu_mime_result_add_traits(result, name.c_str(), &traits);
	    bu_mime_traits_free(&traits);
	    if (!valid) return 0;
	}
	if (name == "image/rle") {
	    const auto rle = mime_rle::inspect(input->data, input->size);
	    if (rle.identified) {
		bu_mime_traits traits = BU_MIME_TRAITS_INIT_ZERO;
		const bool valid = rle.append_traits(traits) && bu_mime_result_add_traits(result, name.c_str(), &traits);
		bu_mime_traits_free(&traits);
		if (!valid) return 0;
		if (!rle.header_known && !input->complete) result->incomplete = 1;
	    }
	}
    }
    const char *version = NULL;
    /* Native V5 has an eight-byte header object, including empty databases.
     * The V4 ident granule is 128 bytes, with the version at offset two. */
    static const unsigned char v5_header[] = {0x76, 0x01, 0, 0, 0, 0, 0x01, 0x35};
    constexpr size_t v4_ident_bytes = 128;
    if (input->size >= v4_ident_bytes && input->data[0] == 'I' &&
	input->data[2] == 'v' && input->data[3] == '4' && !input->data[4]) version = "4";
    if (input->size >= sizeof(v5_header) && !memcmp(input->data, v5_header, sizeof(v5_header))) version = "5";
    if (version && (!identify(registry, result, "model/vnd.brlcad", BU_MIME_EVIDENCE_SIGNATURE) ||
	!fact(result, "model/vnd.brlcad", "version", version)))
	return 0;
    {
	std::istringstream fields(text);
	std::string first, second, name, brace;
	if (fields >> first >> second >> name >> brace && first == "HIERARCHY" && second == "ROOT" && brace == "{") {
	    if (!identify(registry, result, "model/vnd.biovision.bvh", BU_MIME_EVIDENCE_HEURISTIC) ||
		!fact(result, "model/vnd.biovision.bvh", "encoding", "ascii")) return 0;
	}
    }
    {
	std::istringstream lines(text);
	std::string raw, keyword, filename;
	bool order = false, points = false;
	if (std::getline(lines, raw)) {
	    std::istringstream header(raw);
	    if (header >> keyword >> filename && keyword == "$Filename") {
		while (std::getline(lines, raw)) {
		    std::istringstream declaration(raw);
		    if (!(declaration >> keyword)) continue;
		    order = order || keyword == "$Order";
		    points = points || keyword == "$Points";
		    if (order && points) break;
		}
		if (order && points && (!identify(registry, result, "model/vnd.autodesk.csm", BU_MIME_EVIDENCE_HEURISTIC) ||
		    !fact(result, "model/vnd.autodesk.csm", "encoding", "ascii"))) return 0;
	    }
	}
    }
    /* Headerless triangle tables need a RAW association or explicit reader
     * assertion. Do not reinterpret records belonging to another format. */
    bool raw_hint = false, identified = false;
    for (size_t i = 0; i < result->count; ++i) {
	raw_hint = raw_hint || mime_raw::is_type(result->candidates[i].name);
	identified = identified || result->candidates[i].evidence >= BU_MIME_EVIDENCE_HEURISTIC;
    }
    if (raw_hint && !identified) {
	std::istringstream stream(text);
	auto profile = mime_raw::inspect(stream, input->complete != 0);
	if (!profile.type.empty()) {
	    bu_mime_traits traits = BU_MIME_TRAITS_INIT_ZERO;
	    bool valid = identify(registry, result, profile.type.c_str(), BU_MIME_EVIDENCE_HEURISTIC) &&
		profile.append_traits(traits) && bu_mime_result_add_traits(result, profile.type.c_str(), &traits);
	    if (valid && profile.shared_named_subset)
		valid = identify(registry, result, "model/vnd.iabg.raw", BU_MIME_EVIDENCE_HEURISTIC) &&
		    bu_mime_result_add_traits(result, "model/vnd.iabg.raw", &traits);
	    bu_mime_traits_free(&traits);
	    if (!valid) return 0;
	    if (!input->complete) result->incomplete = 1;
	}
    }
    return 1;
}
