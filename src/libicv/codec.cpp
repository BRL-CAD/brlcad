/* C O D E C . C P P
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
#include <exception>
#include <sys/stat.h>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "bu/app.h"
#include "bu/dylib.h"
#include "bu/cv.h"
#include "bu/malloc.h"
#include "bu/str.h"
#include "icv_private.h"
#include "../libbu/mime_png.h"
#include "../libbu/mime_rle.h"
#include "../libbu/mime_sun.h"
#include "../libbu/mime_select.h"

struct icv_owned_codec {
    std::string name;
    std::vector<bu_mime_capability> formats;
    icv_codec codec;

    explicit icv_owned_codec(const icv_codec &source) : name(source.name), codec(source)
    {
	codec.name = name.c_str();
	/* Dispatch uses the copied canonical names, never the caller's array. */
	codec.formats = NULL;
    }
    ~icv_owned_codec() { for (auto &format : formats) bu_mime_capability_free(&format); }
};

struct icv_context {
    bu_mime_registry *mime;
    bool owns_mime;
    size_t spool_limit = 0;
    std::vector<std::unique_ptr<icv_owned_codec>> codecs;
    std::vector<bu_mime_token> contributions;
    std::vector<void *> handles;
};

enum class icv_operation { read, read_mem, write, write_mem, metadata, read_sequence, write_sequence };

class icv_traits_scope {
public:
    bu_mime_traits value = BU_MIME_TRAITS_INIT_ZERO;
    ~icv_traits_scope() { bu_mime_traits_free(&value); }
};

class icv_result_scope {
public:
    bu_mime_result *value = NULL;
    ~icv_result_scope() { bu_mime_result_free(value); }
};

static const bu_mime_traits *
icv_observations(const bu_mime_result *result, const char *type)
{
    for (size_t i = 0; result && i < result->count; ++i)
	if (BU_STR_EQUAL(type, result->candidates[i].name))
	    return &result->candidates[i].traits;
    return NULL;
}

static unsigned int icv_codec_operations(const icv_codec &codec);

static unsigned int
icv_operation_bit(icv_operation operation)
{
    switch (operation) {
	case icv_operation::read: return ICV_CODEC_READ;
	case icv_operation::read_mem: return ICV_CODEC_READ_MEM;
	case icv_operation::write: return ICV_CODEC_WRITE;
	case icv_operation::write_mem: return ICV_CODEC_WRITE_MEM;
	case icv_operation::metadata: return ICV_CODEC_METADATA;
	case icv_operation::read_sequence: return ICV_CODEC_READ_SEQUENCE;
	case icv_operation::write_sequence: return ICV_CODEC_WRITE_SEQUENCE;
    }
    return 0;
}

static bool
icv_format_operation(const bu_mime_capability &format, unsigned int operations)
{
    return !format.operations || (format.operations & operations) == operations;
}

static bool
icv_has_type(const icv_owned_codec &codec, const std::string &type)
{
    for (const auto &format : codec.formats)
	if (type == format.mime_type)
	    return true;
    return false;
}

static bool
icv_raw_request_matches(const bu_mime_capability &layout, const bu_mime_traits *observed,
    const bu_mime_traits *requested)
{
    if (!requested) return true;
    const bu_mime_capability facts = BU_MIME_CAPABILITY(layout.mime_type,
	observed ? observed->items : NULL, observed ? observed->count : 0, 0);
    for (size_t i = 0; i < requested->count; ++i) {
	const bu_mime_traits assertion = {&requested->items[i], 1};
	/* Raw requests must describe an available interpretation or a property
	 * actually observed by the plugin; they cannot supply unknown facts. */
	if (!bu_mime_capability_match(&layout, &assertion, BU_MIME_OUTPUT) &&
	    !bu_mime_capability_match(&facts, &assertion, BU_MIME_OUTPUT))
	    return false;
    }
    return true;
}

static int
icv_codec_error(bu_vls *messages, const char *message)
{
    if (messages)
	bu_vls_printf(messages, "%s\n", message);
    return BRLCAD_ERROR;
}

extern "C" int
icv_register_plugin(icv_context *context, const icv_plugin *plugin, bu_vls *messages)
{
    if (!context || !plugin || plugin->abi_version != ICV_PLUGIN_ABI_VERSION ||
	plugin->struct_size < sizeof(*plugin) ||
	(plugin->definition_count && !plugin->definitions) || !plugin->codecs || !plugin->codec_count) {
	icv_codec_error(messages, "Invalid image plugin descriptor");
	return 0;
    }
    std::vector<bu_mime_token> tokens;
    bool valid = true;
    for (size_t i = 0; i < plugin->definition_count; ++i) {
	bu_mime_token token = bu_mime_register(context->mime, &plugin->definitions[i]);
	if (!token) {
	    valid = false;
	    break;
	}
	tokens.push_back(token);
    }
    std::vector<std::unique_ptr<icv_owned_codec>> prepared;
    for (size_t i = 0; valid && i < plugin->codec_count; ++i) {
	const icv_codec &codec = plugin->codecs[i];
	if (!codec.name || !codec.name[0] || !codec.formats || !codec.format_count ||
	    codec.priority > BU_MIME_MAX_PRIORITY ||
	    !icv_codec_operations(codec) || (codec.raw_pixel_size && (codec.read_sequence || codec.write_sequence))) {
	    valid = false;
	    break;
	}
	for (const auto &existing : context->codecs)
	    if (existing->name == codec.name)
		valid = false;
	for (const auto &existing : prepared)
	    if (existing->name == codec.name)
		valid = false;
	if (!valid)
	    break;
	std::unique_ptr<icv_owned_codec> owned(new icv_owned_codec(codec));
	bu_vls canonical = BU_VLS_INIT_ZERO;
	for (size_t j = 0; j < codec.format_count; ++j) {
	    bu_mime_capability source = codec.formats[j];
	    if ((source.operations & ~icv_codec_operations(codec)) ||
		!bu_mime_canonical(context->mime, source.mime_type, &canonical)) {
		valid = false;
		break;
	    }
	    source.mime_type = bu_vls_cstr(&canonical);
	    bu_mime_capability copy = {};
	    if (!bu_mime_capability_copy(&copy, &source)) { valid = false; break; }
	    owned->formats.push_back(copy);
	}
	bu_vls_free(&canonical);
	prepared.push_back(std::move(owned));
    }
    if (!valid) {
	for (bu_mime_token token : tokens)
	    bu_mime_unregister(context->mime, token);
	icv_codec_error(messages, "Invalid or conflicting MIME/codec metadata");
	return 0;
    }
    context->contributions.insert(context->contributions.end(), tokens.begin(), tokens.end());
    for (auto &codec : prepared)
	context->codecs.push_back(std::move(codec));
    return 1;
}

static bool
icv_codec_supports(const icv_codec &codec, icv_operation operation)
{
    switch (operation) {
	case icv_operation::read: return codec.read != NULL;
	case icv_operation::read_mem: return codec.read_mem != NULL;
	case icv_operation::write: return codec.write != NULL;
	case icv_operation::write_mem: return codec.write_mem != NULL;
	case icv_operation::metadata: return codec.metadata != NULL || codec.raw_pixel_size != 0;
	case icv_operation::read_sequence: return codec.read_sequence != NULL;
	case icv_operation::write_sequence: return codec.write_sequence != NULL;
    }
    return false;
}

static const icv_codec *
icv_select_codec(icv_context *context, icv_operation operation, const char *mime_type,
    const char *codec_name, const bu_mime_result *result, const bu_mime_traits *requested,
    std::string &resolved, bu_mime_traits &resolved_traits, bu_vls *messages)
{
    if (!bu_mime_traits_valid(requested, 1)) {
	icv_codec_error(messages, "Invalid or conflicting requested image traits");
	return NULL;
    }
    std::vector<std::string> types;
    if (mime_type) {
	bu_mime_result *names = NULL;
	if (bu_mime_resolve_candidates(context->mime, mime_type, &names) != BU_MIME_ERROR && names)
	    for (size_t i = 0; i < names->count; ++i)
		types.emplace_back(names->candidates[i].name);
	bu_mime_result_free(names);
    } else if (result && result->count) {
	for (size_t i = 0; i < result->count; ++i) {
	    const auto &candidate = result->candidates[i];
	    if (candidate.evidence != result->candidates[0].evidence || candidate.priority != result->candidates[0].priority)
		break;
	    types.emplace_back(candidate.name);
	}
    }
    if (types.empty()) {
	icv_codec_error(messages, "Unrecognized image type; specify a MIME type for raw data");
	return NULL;
    }
    if (mime_type) mime_selection::prefer_observed(types, result);
    bu_mime_direction direction = operation == icv_operation::write || operation == icv_operation::write_mem ||
	operation == icv_operation::write_sequence ? BU_MIME_OUTPUT : BU_MIME_INPUT;
    unsigned int operation_bit = icv_operation_bit(operation);
    const icv_codec *selected = NULL;
    unsigned int best_priority = 0;
    std::string selected_type;
    bool ambiguous = false;
    for (const auto &type : types) {
	const bu_mime_traits *observed = icv_observations(result, type.c_str());
	icv_traits_scope traits;
	if (!bu_mime_traits_resolve(&traits.value, observed, requested)) {
	    icv_codec_error(messages, "Requested image traits contradict observed properties");
	    continue;
	}
	for (const auto &owned : context->codecs) {
	    const icv_codec &codec = owned->codec;
	    if (!icv_codec_supports(codec, operation) || (codec_name && owned->name != codec_name))
		continue;
	    bool compatible = false;
	    icv_traits_scope available;
	    for (const auto &format : owned->formats) {
		bu_mime_capability constraint = format;
		/* Headerless data uses the selected layout even for size metadata;
		 * encoded metadata cannot promise later full-reader validation. */
		if (operation == icv_operation::metadata && !codec.raw_pixel_size)
		    constraint.flags &= ~BU_MIME_DEFER_INPUT_VALIDATION;
		if (type == format.mime_type && icv_format_operation(format, operation_bit) &&
		    (!codec.raw_pixel_size || direction != BU_MIME_INPUT || icv_raw_request_matches(constraint, observed, requested)) &&
		    bu_mime_capability_resolve(&constraint, &traits.value, direction, &available.value)) {
		    compatible = true;
		    break;
		}
	    }
	    if (!compatible)
		continue;
	    if (selected && selected_type != type) {
		icv_codec_error(messages, "Ambiguous image types; specify a MIME type");
		return NULL;
	    }
	    if (!selected || codec.priority > best_priority) {
		selected = &codec;
		selected_type = type;
		best_priority = codec.priority;
		ambiguous = false;
		bu_mime_traits_copy(&resolved_traits, &available.value);
	    } else if (codec.priority == best_priority) {
		ambiguous = true;
	    }
	}
    }
    if (!selected || ambiguous) {
	icv_codec_error(messages, ambiguous ? "Ambiguous image codecs; specify a codec name" :
	    "Recognized type has no available codec compatible with this operation and its traits");
	if (messages) {
	    bu_mime_result_describe(result, messages);
	    bu_vls_strcat(messages, "Requested traits:");
	    bu_mime_traits_describe(requested, messages);
	    bu_vls_putc(messages, '\n');
	    for (const auto &type : types)
		for (const auto &owned : context->codecs)
		    if (icv_codec_supports(owned->codec, operation) && icv_has_type(*owned, type))
			for (const auto &format : owned->formats)
			    if (type == format.mime_type && icv_format_operation(format, operation_bit)) {
				bu_vls_printf(messages, "  %s: %s (priority %u)", type.c_str(), owned->name.c_str(), owned->codec.priority);
				bu_mime_traits traits = {format.traits, format.trait_count};
				bu_mime_traits_describe(&traits, messages);
				bu_vls_putc(messages, '\n');
			    }
	}
	return NULL;
    }
    resolved = selected_type;
    return selected;
}

static bool
icv_requires_traits(icv_context *context, const char *mime_type, const char *codec_name, icv_operation operation)
{
    if (!mime_type)
	return true;
    bu_vls canonical = BU_VLS_INIT_ZERO;
    const bu_mime_status status = bu_mime_resolve(context->mime, mime_type, &canonical);
    if (status != BU_MIME_MATCH) {
	bu_vls_free(&canonical);
	return status == BU_MIME_AMBIGUOUS;
    }
    bool required = false;
    for (const auto &owned : context->codecs)
	if ((!codec_name || owned->name == codec_name) && icv_codec_supports(owned->codec, operation))
	    for (const auto &format : owned->formats)
		if (format.trait_count && BU_STR_EQUAL(format.mime_type, bu_vls_cstr(&canonical)) &&
		    icv_format_operation(format, icv_operation_bit(operation)))
		    required = true;
    bu_vls_free(&canonical);
    return required;
}

static bool
icv_rle_profile_matches(const unsigned char *buffer, size_t size, const bu_mime_traits *traits, const bu_mime_traits *observations)
{
    const auto profile = mime_rle::inspect(buffer, size);
    /* read_rgb returns interleaved RGB(A), rather than arbitrary planes. */
    if (!profile.header_known || profile.depth != 8 || profile.colors != 3) return false;
#ifdef STRICT_RLE_ENDIAN
    if (profile.big_endian) return false;
#endif
    return profile.matches(traits, observations);
}

static icv_image_t *
icv_rle_stream_reader(FILE *fp, size_t, size_t, const char *, const bu_mime_traits *traits, const bu_mime_traits *observations)
{
    fpos_t position;
    if (fgetpos(fp, &position)) return NULL;
    unsigned char header[mime_rle::fixed_header_size];
    const size_t size = fread(header, 1, sizeof(header), fp);
    if (fsetpos(fp, &position) || ferror(fp) || !icv_rle_profile_matches(header, size, traits, observations)) return NULL;
    return rle_read(fp);
}

static icv_image_t *
icv_rle_memory_reader(const unsigned char *buffer, size_t size, size_t, size_t,
    const char *, const bu_mime_traits *traits, const bu_mime_traits *observations)
{
    return icv_rle_profile_matches(buffer, size, traits, observations) ? rle_read_mem(buffer, size) : NULL;
}

template<icv_image_t *(*Read)(FILE *, size_t, size_t)>
static icv_image_t *
icv_raw_reader(FILE *fp, size_t width, size_t height, const char *, const bu_mime_traits *, const bu_mime_traits *)
{
    return Read(fp, width, height);
}

template<icv_image_t *(*Read)(const unsigned char *, size_t, size_t, size_t)>
static icv_image_t *
icv_raw_memory_reader(const unsigned char *buffer, size_t size, size_t width, size_t height, const char *, const bu_mime_traits *, const bu_mime_traits *)
{
    return Read(buffer, size, width, height);
}

template<int (*Write)(icv_image_t *, FILE *)>
static int
icv_stream_writer(icv_image_t *image, FILE *fp, const char *, const bu_mime_traits *)
{
    return Write(image, fp);
}

template<size_t PixelSize, size_t Channels, icv_metadata_color Color>
static icv_metadata_status
icv_raw_metadata(const bu_mime_probe_input *input, const char *, icv_image_metadata *info,
    const bu_mime_traits *, const bu_mime_traits *)
{
    if (!input->file_size_known || input->file_size > SIZE_MAX ||
	!icv_raw_size(NULL, 0, static_cast<size_t>(input->file_size), PixelSize, &info->width, &info->height))
	return ICV_METADATA_UNKNOWN;
    info->channels = Channels;
    info->color_space = Color;
    return ICV_METADATA_MATCH;
}

template<int (*Write)(icv_image_t *, unsigned char **, size_t *)>
static int
icv_memory_writer(icv_image_t *image, unsigned char **buffer, size_t *size, const char *, const bu_mime_traits *)
{
    return Write(image, buffer, size);
}

static const int icv_jpeg_quality = 90;

template<icv_image_t *(*Read)(FILE *, const bu_mime_traits *, const bu_mime_traits *)>
static icv_image_t *
icv_profile_stream_reader(FILE *fp, size_t, size_t, const char *, const bu_mime_traits *traits, const bu_mime_traits *observations)
{
    return Read(fp, traits, observations);
}

template<icv_image_t *(*Read)(const unsigned char *, size_t, const bu_mime_traits *, const bu_mime_traits *)>
static icv_image_t *
icv_profile_memory_reader(const unsigned char *buffer, size_t size, size_t, size_t, const char *, const bu_mime_traits *traits, const bu_mime_traits *observations)
{
    return Read(buffer, size, traits, observations);
}

static int
icv_jpeg_stream_writer(icv_image_t *image, FILE *fp, const char *, const bu_mime_traits *)
{
    return jpeg_write(image, fp, icv_jpeg_quality);
}

static int
icv_jpeg_memory_writer(icv_image_t *image, unsigned char **buffer, size_t *size, const char *, const bu_mime_traits *)
{
    return jpeg_write_mem(image, buffer, size, icv_jpeg_quality);
}

static int
icv_sun_stream_writer(icv_image_t *image, FILE *fp, const char *, const bu_mime_traits *)
{
    return sun_write(image, fp, false);
}

static int
icv_sun_memory_writer(icv_image_t *image, unsigned char **buffer, size_t *size, const char *, const bu_mime_traits *)
{
    return sun_write_mem(image, buffer, size, false);
}

static icv_metadata_status
icv_header_metadata(const bu_mime_probe_input *input, const char *type, icv_image_metadata *info, const bu_mime_traits *traits, const bu_mime_traits *observations)
{
    const unsigned char *data = input->data;
    size_t size = input->size;
    icv_metadata_status short_header = input->complete ? ICV_METADATA_ERROR : ICV_METADATA_INCOMPLETE;
    if (BU_STR_EQUAL(type, "image/x-sun-raster")) {
	if (size < mime_sun::fixed_header_size)
	    return short_header;
	const mime_sun::header header(data, size);
	if (!header.matches(traits, observations))
	    return ICV_METADATA_ERROR;
	info->width = header.word(mime_sun::width);
	info->height = header.word(mime_sun::height);
	/* Channel layout depends on the palette and raster representation. */
	info->channels = 0;
	info->color_space = ICV_METADATA_COLOR_UNKNOWN;
	return info->width && info->height ? ICV_METADATA_MATCH : ICV_METADATA_ERROR;
    }
    if (BU_STR_EQUAL(type, "image/png") || BU_STR_EQUAL(type, "image/apng")) {
	const size_t ihdr_end = 33;
	if (size < ihdr_end)
	    return short_header;
	mime_png::inspector header;
	header.feed(data, size);
	const auto &facts = header.facts;
	if (!facts.standard_header())
	    return ICV_METADATA_ERROR;
	if (BU_STR_EQUAL(type, "image/apng") && !facts.animated)
	    return input->complete || facts.animation_known ? ICV_METADATA_ERROR : ICV_METADATA_INCOMPLETE;
	if (!facts.matches(traits, observations))
	    return input->complete ? ICV_METADATA_ERROR : ICV_METADATA_INCOMPLETE;
	switch (facts.color) {
	    case mime_png::grayscale: info->channels = 1; break;
	    case mime_png::truecolor: case mime_png::indexed: info->channels = 3; break;
	    case mime_png::grayscale_alpha: info->channels = 2; break;
	    case mime_png::truecolor_alpha: info->channels = 4; break;
	    default: return ICV_METADATA_ERROR;
	}
	info->width = facts.width;
	info->height = facts.height;
	info->color_space = facts.color == mime_png::grayscale || facts.color == mime_png::grayscale_alpha ? ICV_METADATA_COLOR_GRAY : ICV_METADATA_COLOR_RGB;
	return ICV_METADATA_MATCH;
    }
    return ICV_METADATA_UNKNOWN;
}

extern "C" icv_context *
icv_context_create(bu_mime_registry *registry)
{
    std::unique_ptr<icv_context> context(new icv_context);
    context->mime = registry ? registry : bu_mime_registry_create(0);
    context->owns_mime = !registry;
    static const bu_mime_trait binary[] = {BU_MIME_TRAIT_STRING("encoding", "binary")};
    static const bu_mime_trait png_output[] = {
	BU_MIME_TRAIT_STRING("encoding", "binary"), BU_MIME_TRAIT_STRING("bit-depth", "8"),
	BU_MIME_TRAIT_STRING("png.compression-method", "0"), BU_MIME_TRAIT_STRING("png.filter-method", "0"),
	BU_MIME_TRAIT_STRING("png.interlace-method", "0"), BU_MIME_TRAIT_STRING("png.animated", "false")
    };
    static const bu_mime_capability png_types[] = {
	BU_MIME_CAPABILITY("image/png", NULL, 0, ICV_CODEC_METADATA),
	{"image/png", binary, 1, ICV_CODEC_READ | ICV_CODEC_READ_MEM, BU_MIME_DEFER_INPUT_VALIDATION},
	BU_MIME_CAPABILITY("image/png", png_output, sizeof(png_output) / sizeof(png_output[0]), ICV_CODEC_WRITE | ICV_CODEC_WRITE_MEM)
    };
    static const bu_mime_capability apng_types[] = {
	BU_MIME_CAPABILITY("image/apng", NULL, 0, ICV_CODEC_METADATA),
	{"image/apng", binary, 1, ICV_CODEC_READ | ICV_CODEC_READ_MEM, BU_MIME_DEFER_INPUT_VALIDATION}
    };
    size_t jpeg_type_count;
    const bu_mime_capability *jpeg_types = jpeg_capabilities(&jpeg_type_count);
    static const bu_mime_trait ppm_output[] = {
	BU_MIME_TRAIT_STRING("encoding", "binary"),
	BU_MIME_TRAIT_STRING("bit-depth", "8"),
	BU_MIME_TRAIT_STRING("ppm.maxval", "255")
    };
    static const bu_mime_trait ppm_ascii[] = {BU_MIME_TRAIT_STRING("encoding", "ascii")};
    static const bu_mime_trait ppm_binary[] = {BU_MIME_TRAIT_STRING("encoding", "binary")};
    static const bu_mime_capability ppm_types[] = {
	BU_MIME_CAPABILITY("image/x-portable-pixmap", NULL, 0, ICV_CODEC_METADATA),
	{"image/x-portable-pixmap", ppm_ascii, 1, ICV_CODEC_READ | ICV_CODEC_READ_MEM, BU_MIME_DEFER_INPUT_VALIDATION},
	{"image/x-portable-pixmap", ppm_binary, 1, ICV_CODEC_READ | ICV_CODEC_READ_MEM, BU_MIME_DEFER_INPUT_VALIDATION},
	BU_MIME_CAPABILITY("image/x-portable-pixmap", ppm_output, sizeof(ppm_output) / sizeof(ppm_output[0]),
	    ICV_CODEC_WRITE | ICV_CODEC_WRITE_MEM)
    };
    static const bu_mime_trait pix_layout[] = {
	BU_MIME_TRAIT_STRING("encoding", "binary"), BU_MIME_TRAIT_STRING("bit-depth", "8"),
	BU_MIME_TRAIT_STRING("sample-format", "unsigned-integer"), BU_MIME_TRAIT_STRING("channels", "3"),
	BU_MIME_TRAIT_STRING("color-space", "rgb")};
    static const bu_mime_trait bw_layout[] = {
	BU_MIME_TRAIT_STRING("encoding", "binary"), BU_MIME_TRAIT_STRING("bit-depth", "8"),
	BU_MIME_TRAIT_STRING("sample-format", "unsigned-integer"), BU_MIME_TRAIT_STRING("channels", "1"),
	BU_MIME_TRAIT_STRING("color-space", "gray")};
    static const bu_mime_trait dpix_layout[] = {
	BU_MIME_TRAIT_STRING("encoding", "binary"), BU_MIME_TRAIT_STRING("bit-depth", "64"),
	BU_MIME_TRAIT_STRING("sample-format", "ieee754"), BU_MIME_TRAIT_STRING("channels", "3"),
	BU_MIME_TRAIT_STRING("color-space", "rgb"), BU_MIME_TRAIT_STRING("byte-order", "big-endian")};
    static const unsigned int raw_inputs = ICV_CODEC_READ | ICV_CODEC_READ_MEM | ICV_CODEC_METADATA;
    static const unsigned int raw_outputs = ICV_CODEC_WRITE | ICV_CODEC_WRITE_MEM;
    static const bu_mime_capability pix_types[] = {
	{"image/vnd.brlcad.pix", pix_layout, sizeof(pix_layout) / sizeof(pix_layout[0]), raw_inputs, BU_MIME_DEFER_INPUT_VALIDATION},
	BU_MIME_CAPABILITY("image/vnd.brlcad.pix", pix_layout, sizeof(pix_layout) / sizeof(pix_layout[0]), raw_outputs)};
    static const bu_mime_capability bw_types[] = {
	{"image/vnd.brlcad.bw", bw_layout, sizeof(bw_layout) / sizeof(bw_layout[0]), raw_inputs, BU_MIME_DEFER_INPUT_VALIDATION},
	BU_MIME_CAPABILITY("image/vnd.brlcad.bw", bw_layout, sizeof(bw_layout) / sizeof(bw_layout[0]), raw_outputs)};
    static const bu_mime_capability dpix_types[] = {
	{"image/vnd.brlcad.dpix", dpix_layout, sizeof(dpix_layout) / sizeof(dpix_layout[0]), raw_inputs, BU_MIME_DEFER_INPUT_VALIDATION},
	BU_MIME_CAPABILITY("image/vnd.brlcad.dpix", dpix_layout, sizeof(dpix_layout) / sizeof(dpix_layout[0]), raw_outputs)};
    /* The first four layout facts also constrain the reader. */
    static constexpr size_t rle_input_trait_count = 4;
    static const bu_mime_trait rle_layout[] = {
	BU_MIME_TRAIT_STRING("rle.variant", "utah"), BU_MIME_TRAIT_STRING("encoding", "binary"),
	BU_MIME_TRAIT_STRING("bit-depth", "8"), BU_MIME_TRAIT_STRING("rle.color-channels", "3"),
	BU_MIME_TRAIT_STRING("byte-order", "little-endian"), BU_MIME_TRAIT_STRING("rle.colormap-channels", "0"),
	BU_MIME_TRAIT_STRING("rle.colormap-length-bits", "0")};
    static const bu_mime_capability rle_types[] = {
	{"image/rle", rle_layout, rle_input_trait_count,
	    ICV_CODEC_READ | ICV_CODEC_READ_MEM, BU_MIME_DEFER_INPUT_VALIDATION},
	BU_MIME_CAPABILITY("image/rle", rle_layout, sizeof(rle_layout) / sizeof(rle_layout[0]),
	    ICV_CODEC_WRITE | ICV_CODEC_WRITE_MEM)};
    static const bu_mime_trait sun_output[] = {
	BU_MIME_TRAIT_STRING("encoding", "binary"),
	BU_MIME_TRAIT_STRING("bit-depth", "8"),
	BU_MIME_TRAIT_STRING("sun.raster-type", "1"),
	BU_MIME_TRAIT_STRING("sun.map-type", "1")
    };
    static const bu_mime_capability sun_types[] = {
	BU_MIME_CAPABILITY("image/x-sun-raster", NULL, 0, ICV_CODEC_METADATA),
	BU_MIME_CAPABILITY("image/x-sun-raster", sun_output, sizeof(sun_output) / sizeof(sun_output[0]),
	    ICV_CODEC_WRITE | ICV_CODEC_WRITE_MEM)
    };
    static const icv_codec codecs[] = {
	{"PNG", png_types, sizeof(png_types) / sizeof(png_types[0]), BU_MIME_DEFAULT_PRIORITY, png_read,
	 png_read_mem, icv_stream_writer<png_write>, icv_memory_writer<png_write_mem>, NULL, 0, icv_header_metadata, NULL, NULL, NULL},
	{"APNG", apng_types, sizeof(apng_types) / sizeof(apng_types[0]), BU_MIME_DEFAULT_PRIORITY, png_read,
	 png_read_mem, NULL, NULL, NULL, 0, icv_header_metadata, NULL, NULL, NULL},
	{"JPEG", jpeg_types, jpeg_type_count, BU_MIME_DEFAULT_PRIORITY, icv_profile_stream_reader<jpeg_read>,
	 icv_profile_memory_reader<jpeg_read_mem>, icv_jpeg_stream_writer, icv_jpeg_memory_writer, NULL, 0, jpeg_metadata, jpeg_inspect, NULL, NULL},
	{"PPM", ppm_types, sizeof(ppm_types) / sizeof(ppm_types[0]), BU_MIME_DEFAULT_PRIORITY, icv_profile_stream_reader<ppm_read>,
	 icv_profile_memory_reader<ppm_read_mem>, icv_stream_writer<ppm_write>, icv_memory_writer<ppm_write_mem>, NULL, 0, ppm_metadata, NULL, NULL, NULL},
	{"SUN", sun_types, sizeof(sun_types) / sizeof(sun_types[0]), BU_MIME_DEFAULT_PRIORITY, NULL, NULL,
	 icv_sun_stream_writer, icv_sun_memory_writer, NULL, 0, icv_header_metadata, NULL, NULL, NULL},
	{"PIX", pix_types, sizeof(pix_types) / sizeof(pix_types[0]), BU_MIME_DEFAULT_PRIORITY,
	 icv_raw_reader<pix_read>, icv_raw_memory_reader<pix_read_mem>, icv_stream_writer<pix_write>, icv_memory_writer<pix_write_mem>,
	 NULL, 3, icv_raw_metadata<3, 3, ICV_METADATA_COLOR_RGB>, NULL, NULL, NULL},
	{"BW", bw_types, sizeof(bw_types) / sizeof(bw_types[0]), BU_MIME_DEFAULT_PRIORITY,
	 icv_raw_reader<bw_read>, icv_raw_memory_reader<bw_read_mem>, icv_stream_writer<bw_write>, icv_memory_writer<bw_write_mem>,
	 NULL, 1, icv_raw_metadata<1, 1, ICV_METADATA_COLOR_GRAY>, NULL, NULL, NULL},
	{"DPIX", dpix_types, sizeof(dpix_types) / sizeof(dpix_types[0]), BU_MIME_DEFAULT_PRIORITY,
	 icv_raw_reader<dpix_read>, icv_raw_memory_reader<dpix_read_mem>, icv_stream_writer<dpix_write>, icv_memory_writer<dpix_write_mem>,
	 NULL, 3 * SIZEOF_NETWORK_DOUBLE, icv_raw_metadata<3 * SIZEOF_NETWORK_DOUBLE, 3, ICV_METADATA_COLOR_RGB>, NULL, NULL, NULL},
	{"RLE", rle_types, sizeof(rle_types) / sizeof(rle_types[0]), BU_MIME_DEFAULT_PRIORITY, icv_rle_stream_reader,
	 icv_rle_memory_reader, icv_stream_writer<rle_write>, icv_memory_writer<rle_write_mem>, NULL, 0, NULL, NULL, NULL, NULL}
    };
    static const icv_plugin builtins = {ICV_PLUGIN_ABI_VERSION, sizeof(icv_plugin), NULL, 0,
	codecs, sizeof(codecs) / sizeof(codecs[0])};
    if (!icv_register_plugin(context.get(), &builtins, NULL) || !icv_register_plugin(context.get(), icv_anim_plugin(), NULL)) {
	if (context->owns_mime)
	    bu_mime_registry_destroy(context->mime);
	return NULL;
    }
    return context.release();
}

extern "C" void
icv_context_destroy(icv_context *context)
{
    if (!context)
	return;
    context->codecs.clear();
    for (bu_mime_token token : context->contributions)
	bu_mime_unregister(context->mime, token);
    if (context->owns_mime)
	bu_mime_registry_destroy(context->mime);
    for (auto it = context->handles.rbegin(); it != context->handles.rend(); ++it)
	bu_dlclose(*it);
    delete context;
}

class icv_context_scope {
    std::unique_ptr<icv_context, decltype(&icv_context_destroy)> owned{NULL, icv_context_destroy};
public:
    explicit icv_context_scope(icv_context *&context)
    {
	if (!context) {
	    owned.reset(icv_context_create(NULL));
	    context = owned.get();
	}
    }
};

extern "C" bu_mime_registry *
icv_mime_registry(icv_context *context)
{
    return context ? context->mime : NULL;
}

static unsigned int
icv_codec_operations(const icv_codec &codec)
{
    return (codec.read ? ICV_CODEC_READ : 0) |
	(codec.read_mem ? ICV_CODEC_READ_MEM : 0) |
	(codec.write ? ICV_CODEC_WRITE : 0) |
	(codec.write_mem ? ICV_CODEC_WRITE_MEM : 0) |
	((codec.metadata || codec.raw_pixel_size) ? ICV_CODEC_METADATA : 0) |
	(codec.read_sequence ? ICV_CODEC_READ_SEQUENCE : 0) |
	(codec.write_sequence ? ICV_CODEC_WRITE_SEQUENCE : 0);
}

extern "C" void
icv_codec_list_free(icv_codec_info *info, size_t count)
{
    for (size_t i = 0; i < count; ++i) {
	bu_free(const_cast<char *>(info[i].name), "codec name");
	bu_free(const_cast<char *>(info[i].mime_type), "codec MIME type");
	bu_mime_traits_free(&info[i].traits);
    }
    bu_free(info, "codec metadata");
}

extern "C" int
icv_list_codecs(icv_context *context, const char *mime_type, icv_codec_info **info, size_t *count)
{
    if (!info || !count)
	return 0;
    *info = NULL;
    *count = 0;
    icv_context_scope scope(context);
    if (!context)
	return 0;
    bu_vls canonical = BU_VLS_INIT_ZERO;
    if (mime_type && !bu_mime_canonical(context->mime, mime_type, &canonical)) {
	bu_vls_free(&canonical);
	return 1;
    }
    std::vector<icv_codec_info> rows;
    for (const auto &owned : context->codecs)
	for (const auto &format : owned->formats)
	    if (!mime_type || BU_STR_EQUAL(format.mime_type, bu_vls_cstr(&canonical)))
		rows.push_back({owned->name.c_str(), format.mime_type,
		    format.operations ? format.operations : icv_codec_operations(owned->codec), owned->codec.priority,
		    owned->codec.raw_pixel_size, format.flags, {format.traits, format.trait_count}});
    bu_vls_free(&canonical);
    std::stable_sort(rows.begin(), rows.end(), [](const icv_codec_info &a, const icv_codec_info &b) {
	int type_order = bu_strcmp(a.mime_type, b.mime_type);
	return type_order ? type_order < 0 : bu_strcmp(a.name, b.name) < 0;
    });
    if (rows.empty())
	return 1;
    *info = static_cast<icv_codec_info *>(bu_calloc(rows.size(), sizeof(icv_codec_info), "codec metadata"));
    *count = rows.size();
    for (size_t i = 0; i < rows.size(); ++i) {
	(*info)[i] = rows[i];
	(*info)[i].name = bu_strdup(rows[i].name);
	(*info)[i].mime_type = bu_strdup(rows[i].mime_type);
	(*info)[i].traits = {NULL, 0};
	bu_mime_traits_copy(&(*info)[i].traits, &rows[i].traits);
    }
    return 1;
}

extern "C" int
icv_supports(icv_context *context, const char *mime_type, unsigned int operations)
{
    const unsigned int valid = ICV_CODEC_READ | ICV_CODEC_READ_MEM | ICV_CODEC_WRITE | ICV_CODEC_WRITE_MEM | ICV_CODEC_METADATA |
	ICV_CODEC_READ_SEQUENCE | ICV_CODEC_WRITE_SEQUENCE;
    if (!operations || (operations & ~valid))
	return 0;
    icv_codec_info *info = NULL;
    size_t count = 0;
    if (!icv_list_codecs(context, mime_type, &info, &count))
	return 0;
    bool supported = false;
    for (size_t i = 0; i < count && !supported; ++i) {
	unsigned int available = 0;
	for (size_t j = 0; j < count; ++j)
	    if (BU_STR_EQUAL(info[i].name, info[j].name) && BU_STR_EQUAL(info[i].mime_type, info[j].mime_type))
		available |= info[j].operations;
	supported = (available & operations) == operations;
    }
    icv_codec_list_free(info, count);
    return supported;
}

extern "C" int
icv_image_size_with_context(icv_context *context, const char *label, size_t dpi,
    size_t data_size, const char *mime_type, size_t *width, size_t *height)
{
    icv_codec_info *info = NULL;
    size_t count = 0;
    if (!mime_type || !icv_list_codecs(context, mime_type, &info, &count))
	return 0;
    size_t pixel_size = count ? info[0].raw_pixel_size : 0;
    for (size_t i = 1; i < count; ++i)
	if (pixel_size != info[i].raw_pixel_size)
	    pixel_size = 0;
    icv_codec_list_free(info, count);
    return pixel_size && icv_raw_size(label, dpi, data_size, pixel_size, width, height);
}

extern "C" int
icv_load_plugin(icv_context *context, const char *path, bu_vls *messages)
{
    if (!context || !path)
	return 0;
    void *handle = bu_dlopen(path, BU_RTLD_NOW);
    if (!handle) {
	icv_codec_error(messages, "Cannot load image plugin library");
	return 0;
    }
    auto info = reinterpret_cast<const icv_plugin *(*)(void)>(bu_dlsym(handle, "icv_plugin_info"));
    if (!info || !icv_register_plugin(context, info(), messages)) {
	if (!info)
	    icv_codec_error(messages, "Image plugin has no icv_plugin_info entry point");
	bu_dlclose(handle);
	return 0;
    }
    context->handles.push_back(handle);
    return 1;
}

static bool
icv_apply_probes(icv_context *context, const bu_mime_probe_input &input,
    bu_mime_result *result, bu_vls *messages)
{
    for (const auto &owned : context->codecs) {
	const icv_codec &codec = owned->codec;
	if ((!codec.probe && !codec.inspect) || (!codec.read && !codec.read_mem && !codec.metadata && !codec.raw_pixel_size && !codec.read_sequence))
	    continue;
	std::set<std::string> checked;
	for (const auto &format : owned->formats) {
	    const char *type = format.mime_type;
	    if (!checked.insert(type).second)
		continue;
	    if (codec.probe) {
		bu_mime_probe_status status = codec.probe(&input, type);
		if (status == BU_MIME_PROBE_INCOMPLETE) { result->incomplete = 1; continue; }
		if (status == BU_MIME_PROBE_NO_MATCH)
		    continue;
		if ((status != BU_MIME_PROBE_HEURISTIC && status != BU_MIME_PROBE_SIGNATURE) ||
		    bu_mime_result_add(context->mime, result, type,
			status == BU_MIME_PROBE_SIGNATURE ? BU_MIME_EVIDENCE_SIGNATURE : BU_MIME_EVIDENCE_HEURISTIC,
			BU_MIME_DEFAULT_PRIORITY) == BU_MIME_ERROR) {
		    icv_codec_error(messages, "Image identification probe failed");
		    return false;
		}
	    }
	    bool identified = false;
	    for (size_t i = 0; i < result->count; ++i)
		identified = identified || BU_STR_EQUAL(type, result->candidates[i].name);
	    if (codec.inspect && identified) {
		icv_traits_scope traits;
		if (!codec.inspect(&input, type, &traits.value) || !bu_mime_result_add_traits(result, type, &traits.value)) {
		    icv_codec_error(messages, "Image trait inspection failed");
		    return false;
		}
	    }
	}
    }
    return true;
}

extern "C" bu_mime_status
icv_detect(icv_context *context, const char *filename, const bu_mime_probe_input *input,
    bu_mime_result **result, bu_vls *messages)
{
    if (!result)
	return BU_MIME_ERROR;
    *result = NULL;
    icv_context_scope scope(context);
    if (!context || !input || (!input->data && input->size))
	return BU_MIME_ERROR;
    bu_mime_probe_input bounded = *input;
    bounded.size = std::min(input->size, bu_mime_scan_limit(context->mime));
    bounded.complete = input->complete && bounded.size == input->size;
    const unsigned char empty = 0;
    if (!bounded.data)
	bounded.data = &empty;
    bu_mime_status status = bu_mime_identify(context->mime, filename, &bounded, result);
    if (status == BU_MIME_ERROR)
	return status;
    if (!icv_apply_probes(context, bounded, *result, messages)) {
	bu_mime_result_free(*result);
	*result = NULL;
	return BU_MIME_ERROR;
    }
    if (!(*result)->count)
	return BU_MIME_UNKNOWN_RESULT;
    if ((*result)->count > 1 &&
	(*result)->candidates[0].evidence == (*result)->candidates[1].evidence &&
	(*result)->candidates[0].priority == (*result)->candidates[1].priority)
	return BU_MIME_AMBIGUOUS;
    return BU_MIME_MATCH;
}

struct icv_stream {
    FILE *fp = NULL;
    bool owned = false;
    std::vector<unsigned char> prefix;
    ~icv_stream() { if (owned && fp) fclose(fp); }
};

/* Inspect before buffering: unsupported pipes must not consume unbounded
 * input merely to discover that no decoder can handle it. */
static bool
icv_inspect_stream(icv_context *context, icv_stream &stream, const char *filename,
		   bu_mime_result **result, bu_vls *messages)
{
    fpos_t position;
    bool seekable = fgetpos(stream.fp, &position) == 0;
    std::vector<unsigned char> prefix(bu_mime_scan_limit(context->mime));
    size_t size = fread(prefix.data(), 1, prefix.size(), stream.fp);
    int complete = feof(stream.fp);
    if (ferror(stream.fp)) {
	icv_codec_error(messages, "Error reading image input");
	return false;
    }
    if (seekable && fsetpos(stream.fp, &position)) {
	icv_codec_error(messages, "Cannot restore image input position");
	return false;
    }
    struct stat info;
    bool known = !fstat(fileno(stream.fp), &info) && S_ISREG(info.st_mode) && info.st_size >= 0;
    bu_mime_probe_input input = {prefix.data(), size, complete,
	known ? static_cast<uint64_t>(info.st_size) : 0, known};
    if (icv_detect(context, filename, &input, result, messages) == BU_MIME_ERROR)
	return false;
    if (!seekable) {
	prefix.resize(size);
	stream.prefix = std::move(prefix);
    }
    return true;
}

/* Decoder callbacks historically expect seekable input. Replay an inspected
 * pipe only after a decoder has been selected, keeping storage on disk. */
static bool
icv_buffer_stream(icv_context *context, icv_stream &stream, bu_vls *messages)
{
    fpos_t position;
    if (fgetpos(stream.fp, &position) == 0)
	return true;
    icv_stream temporary;
    temporary.fp = bu_temp_file(NULL, 0);
    temporary.owned = true;
    if (!temporary.fp) {
	icv_codec_error(messages, "Cannot buffer nonseekable image input");
	return false;
    }
    size_t total = stream.prefix.size();
    if ((context->spool_limit && total > context->spool_limit) ||
	fwrite(stream.prefix.data(), 1, total, temporary.fp) != total) {
	icv_codec_error(messages, "Cannot buffer image prefix within stream limit");
	return false;
    }
    std::vector<unsigned char> block(bu_mime_scan_limit(context->mime));
    for (;;) {
	/* Read one excess byte to distinguish an exact limit from overflow. */
	size_t request = block.size();
	if (context->spool_limit && context->spool_limit - total < request)
	    request = context->spool_limit - total + 1;
	size_t count = fread(block.data(), 1, request, stream.fp);
	if ((context->spool_limit && count > context->spool_limit - total) ||
	    count > SIZE_MAX - total) {
	    icv_codec_error(messages, "Image input exceeds stream buffering limit");
	    return false;
	}
	if (ferror(stream.fp) || fwrite(block.data(), 1, count, temporary.fp) != count) {
	    icv_codec_error(messages, "Cannot buffer image input");
	    return false;
	}
	total += count;
	if (count < request)
	    break;
    }
    if (fseek(temporary.fp, 0, SEEK_SET)) {
	icv_codec_error(messages, "Cannot rewind buffered image input");
	return false;
    }
    if (stream.owned)
	fclose(stream.fp);
    stream.fp = temporary.fp;
    stream.owned = true;
    stream.prefix.clear();
    temporary.owned = false;
    return true;
}

extern "C" void
icv_context_set_spool_limit(icv_context *context, size_t bytes)
{
    if (context)
	context->spool_limit = bytes;
}

static bool
icv_file_prefix(icv_context *context, const char *filename, std::vector<unsigned char> &prefix,
    bu_mime_probe_input &input, bu_vls *messages)
{
    if (!context || !filename)
	return false;
    icv_stream stream;
    stream.fp = fopen(filename, "rb");
    stream.owned = true;
    struct stat info;
    if (!stream.fp || fstat(fileno(stream.fp), &info) || !S_ISREG(info.st_mode) || info.st_size < 0) {
	icv_codec_error(messages, "Inspection requires a regular image input");
	return false;
    }
    prefix.resize(bu_mime_scan_limit(context->mime));
    size_t size = fread(prefix.data(), 1, prefix.size(), stream.fp);
    if (ferror(stream.fp)) {
	icv_codec_error(messages, "Cannot read image input");
	return false;
    }
    input = {prefix.data(), size, feof(stream.fp), static_cast<uint64_t>(info.st_size), 1};
    return true;
}

extern "C" bu_mime_status
icv_detect_file(icv_context *context, const char *filename, bu_mime_result **result, bu_vls *messages)
{
    if (!result)
	return BU_MIME_ERROR;
    *result = NULL;
    icv_context_scope scope(context);
    std::vector<unsigned char> prefix;
    bu_mime_probe_input input = {};
    if (!icv_file_prefix(context, filename, prefix, input, messages))
	return BU_MIME_ERROR;
    return icv_detect(context, filename, &input, result, messages);
}

static void
icv_selection_clear(bu_vls *type, bu_vls *codec, bu_mime_traits *traits)
{
    if (type) bu_vls_trunc(type, 0);
    if (codec) bu_vls_trunc(codec, 0);
    if (traits) bu_mime_traits_free(traits);
}

static bool
icv_selection_operation(icv_codec_operation operation, icv_operation &value)
{
    switch (operation) {
	case ICV_CODEC_READ: value = icv_operation::read; return true;
	case ICV_CODEC_READ_MEM: value = icv_operation::read_mem; return true;
	case ICV_CODEC_WRITE: value = icv_operation::write; return true;
	case ICV_CODEC_WRITE_MEM: value = icv_operation::write_mem; return true;
	case ICV_CODEC_METADATA: value = icv_operation::metadata; return true;
	case ICV_CODEC_READ_SEQUENCE: value = icv_operation::read_sequence; return true;
	case ICV_CODEC_WRITE_SEQUENCE: value = icv_operation::write_sequence; return true;
    }
    return false;
}

extern "C" int
icv_select_variant(icv_context *context, icv_codec_operation operation,
    const char *mime_type, const char *codec_name, const bu_mime_result *detected,
    const bu_mime_traits *requested, bu_vls *resolved_type, bu_vls *resolved_codec,
    bu_mime_traits *resolved_traits, bu_vls *messages)
{
    icv_selection_clear(resolved_type, resolved_codec, resolved_traits);
    icv_context_scope scope(context);
    icv_operation selected_operation;
    if (!context || !icv_selection_operation(operation, selected_operation)) {
	icv_codec_error(messages, "Selection requires one image operation");
	return 0;
    }
    std::string type;
    icv_traits_scope traits;
    const icv_codec *codec = icv_select_codec(context, selected_operation, mime_type,
	codec_name, detected, requested, type, traits.value, messages);
    if (!codec)
	return 0;
    if (resolved_type) bu_vls_strcpy(resolved_type, type.c_str());
    if (resolved_codec) bu_vls_strcpy(resolved_codec, codec->name);
    if (resolved_traits) bu_mime_traits_copy(resolved_traits, &traits.value);
    return 1;
}

extern "C" int
icv_resolve_variant(icv_context *context, icv_codec_operation operation,
    const char *filename, const char *mime_type, const char *codec_name,
    const bu_mime_traits *requested, bu_vls *resolved_type, bu_vls *resolved_codec,
    bu_mime_traits *resolved_traits, bu_vls *messages)
{
    icv_selection_clear(resolved_type, resolved_codec, resolved_traits);
    icv_context_scope scope(context);
    if (!context || (operation != ICV_CODEC_READ && operation != ICV_CODEC_WRITE &&
	operation != ICV_CODEC_METADATA && operation != ICV_CODEC_READ_SEQUENCE && operation != ICV_CODEC_WRITE_SEQUENCE)) {
	icv_codec_error(messages, "File selection requires one file operation");
	return 0;
    }
    bu_mime_result *detected = NULL;
    bu_mime_status status = BU_MIME_UNKNOWN_RESULT;
    if (operation == ICV_CODEC_WRITE || operation == ICV_CODEC_WRITE_SEQUENCE)
	status = bu_mime_detect(context->mime, filename, NULL, 0, 0, &detected);
    else if (filename)
	status = icv_detect_file(context, filename, &detected, messages);
    if (status == BU_MIME_ERROR)
	return 0;
    int selected = icv_select_variant(context, operation, mime_type, codec_name,
	detected, requested, resolved_type, resolved_codec, resolved_traits, messages);
    bu_mime_result_free(detected);
    return selected;
}

static icv_metadata_status
icv_metadata_impl(icv_context *context, const bu_mime_probe_input &input,
    const bu_mime_result *result, const char *mime_type, const char *codec_name,
    const bu_mime_traits *requested, icv_image_metadata *metadata, bu_vls *messages)
{
    std::string resolved;
    icv_traits_scope traits;
    const icv_codec *codec = icv_select_codec(context, icv_operation::metadata,
	mime_type, codec_name, result, requested, resolved, traits.value, messages);
    if (!codec)
	return result && result->incomplete ? ICV_METADATA_INCOMPLETE : ICV_METADATA_UNKNOWN;
    icv_image_metadata info = {};
    icv_metadata_status status = ICV_METADATA_UNKNOWN;
    if (codec->metadata) {
	status = codec->metadata(&input, resolved.c_str(), &info, requested, icv_observations(result, resolved.c_str()));
    } else if (codec->raw_pixel_size && input.file_size_known && input.file_size <= SIZE_MAX) {
	if (icv_raw_size(NULL, 0, static_cast<size_t>(input.file_size), codec->raw_pixel_size,
		&info.width, &info.height))
	    status = ICV_METADATA_MATCH;
    }
    if (status == ICV_METADATA_MATCH)
	*metadata = info;
    else if (status != ICV_METADATA_UNKNOWN && status != ICV_METADATA_INCOMPLETE)
	return ICV_METADATA_ERROR;
    return status;
}

extern "C" icv_metadata_status
icv_metadata_variant(icv_context *context, const bu_mime_probe_input *input,
    const char *mime_type, const char *codec_name, const bu_mime_traits *requested,
    icv_image_metadata *metadata, bu_vls *messages)
{
    if (!metadata)
	return ICV_METADATA_ERROR;
    *metadata = {};
    icv_context_scope scope(context);
    if (!context || !input || (!input->data && input->size) || !bu_mime_traits_valid(requested, 1))
	return ICV_METADATA_ERROR;
    bu_mime_probe_input bounded = *input;
    bounded.size = std::min(input->size, bu_mime_scan_limit(context->mime));
    bounded.complete = input->complete && bounded.size == input->size;
    bu_mime_result *result = NULL;
    if (icv_detect(context, NULL, &bounded, &result, messages) == BU_MIME_ERROR)
	return ICV_METADATA_ERROR;
    icv_metadata_status status = icv_metadata_impl(context, bounded, result,
	mime_type, codec_name, requested, metadata, messages);
    bu_mime_result_free(result);
    return status;
}

extern "C" icv_metadata_status
icv_metadata_file_variant(icv_context *context, const char *filename,
    const char *mime_type, const char *codec_name, const bu_mime_traits *requested,
    icv_image_metadata *metadata, bu_vls *messages)
{
    if (!metadata)
	return ICV_METADATA_ERROR;
    *metadata = {};
    icv_context_scope scope(context);
    std::vector<unsigned char> prefix;
    bu_mime_probe_input input = {};
    if (!context || !bu_mime_traits_valid(requested, 1) ||
	!icv_file_prefix(context, filename, prefix, input, messages))
	return ICV_METADATA_ERROR;
    bu_mime_result *result = NULL;
    if (icv_detect(context, filename, &input, &result, messages) == BU_MIME_ERROR)
	return ICV_METADATA_ERROR;
    icv_metadata_status status = icv_metadata_impl(context, input, result,
	mime_type, codec_name, requested, metadata, messages);
    bu_mime_result_free(result);
    return status;
}

extern "C" icv_metadata_status
icv_metadata(icv_context *context, const bu_mime_probe_input *input, const char *mime_type,
    const char *codec_name, icv_image_metadata *metadata, bu_vls *messages)
{
    return icv_metadata_variant(context, input, mime_type, codec_name, NULL, metadata, messages);
}

extern "C" icv_metadata_status
icv_metadata_file(icv_context *context, const char *filename, const char *mime_type,
    const char *codec_name, icv_image_metadata *metadata, bu_vls *messages)
{
    return icv_metadata_file_variant(context, filename, mime_type, codec_name, NULL, metadata, messages);
}

static icv_image_t *
icv_read_context_impl(icv_context *context, const char *filename, const char *mime_type,
    const char *codec_name, const bu_mime_traits *requested, size_t width, size_t height,
    const char *fallback, bool infer_dimensions, bu_vls *messages)
{
    if (!context || !bu_mime_traits_valid(requested, 1))
	return NULL;
    icv_stream stream;
    stream.fp = filename ? fopen(filename, "rb") : stdin;
    stream.owned = filename != NULL;
    if (!stream.fp) {
	icv_codec_error(messages, "Cannot open image input");
	return NULL;
    }
    if (!filename)
	setmode(fileno(stream.fp), O_BINARY);
    icv_result_scope result;
    bool inspected = !mime_type || (requested && requested->count) || icv_requires_traits(context, mime_type, codec_name, icv_operation::read);
    if (inspected && !icv_inspect_stream(context, stream, filename, &result.value, messages))
	return NULL;
    if (!mime_type && fallback && (!result.value || !result.value->count || result.value->candidates[0].evidence == BU_MIME_EVIDENCE_EXTENSION))
	mime_type = fallback;
    std::string resolved;
    icv_traits_scope traits;
    const icv_codec *codec = icv_select_codec(context, icv_operation::read, mime_type, codec_name,
	result.value, requested, resolved, traits.value, messages);
    if (!codec || (inspected && !icv_buffer_stream(context, stream, messages)))
	return NULL;
    if (infer_dimensions && !width && !height && codec->raw_pixel_size) {
	struct stat info;
	if (!fstat(fileno(stream.fp), &info) && S_ISREG(info.st_mode) && info.st_size > 0 && static_cast<uint64_t>(info.st_size) <= SIZE_MAX)
	    icv_raw_size(NULL, 0, static_cast<size_t>(info.st_size), codec->raw_pixel_size, &width, &height);
    }
    icv_image_t *image = codec->read(stream.fp, width, height, resolved.c_str(), requested, icv_observations(result.value, resolved.c_str()));
    if (ferror(stream.fp)) {
	if (image)
	    icv_destroy(image);
	image = NULL;
    }
    if (!image)
	icv_codec_error(messages, "Image decoder failed");
    return image;
}

extern "C" icv_image_t *
icv_read_variant(icv_context *context, const char *filename, const char *mime_type,
    const char *codec_name, const bu_mime_traits *traits, size_t width, size_t height, bu_vls *messages)
{
    icv_context_scope scope(context);
    return icv_read_context_impl(context, filename, mime_type, codec_name, traits, width, height, NULL, true, messages);
}

extern "C" icv_image_t *
icv_read_with_context(icv_context *context, const char *filename, const char *mime_type,
    const char *codec_name, size_t width, size_t height, bu_vls *messages)
{
    return icv_read_variant(context, filename, mime_type, codec_name, NULL, width, height, messages);
}

extern "C" icv_image_t *
icv_read_legacy(icv_context *context, const char *filename, bu_mime_image_t format,
    int automatic, size_t width, size_t height, bu_vls *messages)
{
    const char *type = bu_mime_legacy_name(format, BU_MIME_IMAGE);
    if (!type)
	return NULL;
    return icv_read_context_impl(context, filename, automatic ? NULL : type, NULL, NULL,
	width, height, automatic ? type : NULL, false, messages);
}

extern "C" icv_image_t *
icv_read_mem_variant(icv_context *context, const unsigned char *buffer, size_t size,
    const char *mime_type, const char *codec_name, const bu_mime_traits *requested,
    size_t width, size_t height, bu_vls *messages)
{
    icv_context_scope scope(context);
    if (!context || !buffer || !size) {
	icv_codec_error(messages, "Empty or invalid image input");
	return NULL;
    }
    icv_result_scope result;
    bu_mime_probe_input input = {buffer, size, 1, size, 1};
    if (icv_detect(context, NULL, &input, &result.value, messages) == BU_MIME_ERROR)
	return NULL;
    std::string resolved;
    icv_traits_scope traits;
    const icv_codec *codec = icv_select_codec(context, icv_operation::read_mem, mime_type, codec_name,
	result.value, requested, resolved, traits.value, messages);
    if (!codec)
	return NULL;
    icv_image_t *image = codec->read_mem(buffer, size, width, height, resolved.c_str(), requested, icv_observations(result.value, resolved.c_str()));
    if (!image)
	icv_codec_error(messages, "Image decoder failed");
    return image;
}

extern "C" icv_image_t *
icv_read_mem_with_context(icv_context *context, const unsigned char *buffer, size_t size,
    const char *mime_type, const char *codec_name, size_t width, size_t height, bu_vls *messages)
{
    return icv_read_mem_variant(context, buffer, size, mime_type, codec_name, NULL, width, height, messages);
}

extern "C" int
icv_write_variant(icv_context *context, icv_image_t *image, const char *filename,
    const char *mime_type, const char *codec_name, const bu_mime_traits *requested, bu_vls *messages)
{
    icv_context_scope scope(context);
    if (!context || !ICV_IMAGE_IS_INITIALIZED(image))
	return icv_codec_error(messages, "Invalid image or codec context");
    bu_mime_result *result = NULL;
    if (!mime_type && bu_mime_detect(context->mime, filename, NULL, 0, 0, &result) == BU_MIME_ERROR)
	return BRLCAD_ERROR;
    std::string resolved;
    icv_traits_scope traits;
    const icv_codec *codec = icv_select_codec(context, icv_operation::write, mime_type, codec_name,
	result, requested, resolved, traits.value, messages);
    bu_mime_result_free(result);
    if (!codec)
	return BRLCAD_ERROR;
    FILE *fp = filename ? fopen(filename, "wb") : stdout;
    if (!fp)
	return icv_codec_error(messages, "Cannot open image output");
    if (!filename)
	setmode(fileno(fp), O_BINARY);
    int status = codec->write(image, fp, resolved.c_str(), &traits.value);
    if (fflush(fp) || ferror(fp))
	status = BRLCAD_ERROR;
    if (filename && fclose(fp))
	status = BRLCAD_ERROR;
    if (status)
	icv_codec_error(messages, "Image encoder failed");
    return status;
}

extern "C" int
icv_write_with_context(icv_context *context, icv_image_t *image, const char *filename,
    const char *mime_type, const char *codec_name, bu_vls *messages)
{
    return icv_write_variant(context, image, filename, mime_type, codec_name, NULL, messages);
}

extern "C" int
icv_write_mem_variant(icv_context *context, icv_image_t *image, unsigned char **buffer, size_t *size,
    const char *mime_type, const char *codec_name, const bu_mime_traits *requested, bu_vls *messages)
{
    icv_context_scope scope(context);
    if (!buffer || !size)
	return BRLCAD_ERROR;
    *buffer = NULL;
    *size = 0;
    if (!context || !ICV_IMAGE_IS_INITIALIZED(image) || !mime_type)
	return icv_codec_error(messages, "Memory output requires an explicit MIME type");
    std::string resolved;
    icv_traits_scope traits;
    const icv_codec *codec = icv_select_codec(context, icv_operation::write_mem, mime_type, codec_name,
	NULL, requested, resolved, traits.value, messages);
    if (!codec)
	return BRLCAD_ERROR;
    int status = codec->write_mem(image, buffer, size, resolved.c_str(), &traits.value);
    if (status) {
	bu_free(*buffer, "failed encoded image");
	*buffer = NULL;
	*size = 0;
	icv_codec_error(messages, "Image encoder failed");
    }
    return status;
}

extern "C" int
icv_write_mem_with_context(icv_context *context, icv_image_t *image, unsigned char **buffer,
    size_t *size, const char *mime_type, const char *codec_name, bu_vls *messages)
{
    return icv_write_mem_variant(context, image, buffer, size, mime_type, codec_name, NULL, messages);
}

extern "C" icv_anim_t *
icv_anim_read_variant(icv_context *context, const char *filename, const char *mime_type,
    const char *codec_name, const bu_mime_traits *requested, bu_vls *messages)
{
    icv_context_scope scope(context);
    if (!context || !filename) {
	icv_codec_error(messages, "Sequence input requires a named file and a codec context");
	return NULL;
    }
    icv_result_scope detected;
    if (icv_detect_file(context, filename, &detected.value, messages) == BU_MIME_ERROR) return NULL;
    std::string resolved;
    icv_traits_scope traits;
    const icv_codec *codec = icv_select_codec(context, icv_operation::read_sequence, mime_type, codec_name,
	detected.value, requested, resolved, traits.value, messages);
    if (!codec) return NULL;
    try {
	std::unique_ptr<icv_anim_t, decltype(&icv_anim_destroy)> animation(
	    codec->read_sequence(context, filename, resolved.c_str(), requested, icv_observations(detected.value, resolved.c_str())), icv_anim_destroy);
	if (animation && icv_anim_num_frames(animation.get())) {
	    icv_anim_set_mime_type(animation.get(), resolved.c_str());
	    return animation.release();
	}
    } catch (const std::exception &error) {
	if (messages) bu_vls_printf(messages, "Sequence decoder exception: %s\n", error.what());
    } catch (...) {
	icv_codec_error(messages, "Sequence decoder threw an exception");
    }
    icv_codec_error(messages, "Sequence decoder failed");
    return NULL;
}

extern "C" int
icv_anim_write_variant(icv_context *context, const icv_anim_t *animation, const char *filename,
    const char *mime_type, const char *codec_name, const bu_mime_traits *requested, bu_vls *messages)
{
    icv_context_scope scope(context);
    if (!context || !filename || !animation || !icv_anim_num_frames(animation))
	return icv_codec_error(messages, "Sequence output requires a named file and a nonempty animation");
    bu_mime_result *detected = NULL;
    if (!mime_type && bu_mime_detect(context->mime, filename, NULL, 0, 0, &detected) == BU_MIME_ERROR) return BRLCAD_ERROR;
    std::string resolved;
    icv_traits_scope traits;
    const icv_codec *codec = icv_select_codec(context, icv_operation::write_sequence, mime_type, codec_name,
	detected, requested, resolved, traits.value, messages);
    bu_mime_result_free(detected);
    if (!codec) return BRLCAD_ERROR;
    try {
	if (!codec->write_sequence(context, animation, filename, resolved.c_str(), &traits.value)) return 0;
    } catch (const std::exception &error) {
	if (messages) bu_vls_printf(messages, "Sequence encoder exception: %s\n", error.what());
    } catch (...) {
	icv_codec_error(messages, "Sequence encoder threw an exception");
    }
    return icv_codec_error(messages, "Sequence encoder failed");
}
