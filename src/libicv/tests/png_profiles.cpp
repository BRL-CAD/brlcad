/* P N G _ P R O F I L E S . C P P
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
#include <cstdio>
#include <cstring>
#include <iterator>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>
#include <zlib.h>
#include "bu/app.h"
#include "bu/file.h"
#include "bu/log.h"
#include "bu/malloc.h"
#include "bu/str.h"
#include "icv/codec.h"
#include "icv/colorspace.h"
#include "vmath.h"

namespace {
using bytes = std::vector<unsigned char>;
using image_ptr = std::unique_ptr<icv_image_t, decltype(&icv_destroy)>;
constexpr size_t signature_size = 8;
constexpr size_t chunk_overhead = 12;
constexpr size_t ihdr_end = 33;

void require(bool condition, const char *message)
{
    if (!condition) throw std::runtime_error(message);
}

uint32_t word(const unsigned char *data)
{
    return (uint32_t(data[0]) << 24) | (uint32_t(data[1]) << 16) |
	(uint32_t(data[2]) << 8) | uint32_t(data[3]);
}

void append_word(bytes &data, uint32_t value)
{
    for (int shift = 24; shift >= 0; shift -= 8) data.push_back(static_cast<unsigned char>(value >> shift));
}

void append_chunk(bytes &data, const char *type, const bytes &payload)
{
    append_word(data, static_cast<uint32_t>(payload.size()));
    const size_t start = data.size();
    data.insert(data.end(), type, type + sizeof(uint32_t));
    data.insert(data.end(), payload.begin(), payload.end());
    append_word(data, static_cast<uint32_t>(crc32(0, data.data() + start, static_cast<uInt>(data.size() - start))));
}

bytes png_bytes(icv_context *context, bool green)
{
    image_ptr image(icv_create_with_channels(1, 1, ICV_COLOR_SPACE_RGB, 4), icv_destroy);
    require(image != nullptr, "cannot create PNG pixels");
    image->data[0] = green ? 0 : 1;
    image->data[1] = green ? 1 : 0;
    image->data[2] = 0;
    image->data[3] = 1;
    unsigned char *encoded = NULL;
    size_t size = 0;
    require(!icv_write_mem_with_context(context, image.get(), &encoded, &size, "image/png", NULL, NULL), "cannot encode PNG fixture");
    bytes data(encoded, encoded + size);
    bu_free(encoded, "PNG fixture");
    require(data.size() >= ihdr_end && data[25] == 6, "fixture is not RGBA PNG");
    return data;
}

bytes image_data(const bytes &png)
{
    bytes data;
    for (size_t at = signature_size; at + chunk_overhead <= png.size();) {
	const size_t length = word(png.data() + at);
	require(length <= png.size() - at - chunk_overhead, "invalid fixture chunk");
	if (!memcmp(png.data() + at + sizeof(uint32_t), "IDAT", sizeof(uint32_t)))
	    data.insert(data.end(), png.begin() + at + signature_size, png.begin() + at + signature_size + length);
	at += length + chunk_overhead;
    }
    require(!data.empty(), "fixture has no image data");
    return data;
}

bytes apng_bytes(icv_context *context, bool default_is_frame, bool padded)
{
    const bytes red = png_bytes(context, false);
    bytes data(red.begin(), red.begin() + ihdr_end);
    if (padded) {
	bytes text = {'C', 'o', 'm', 'm', 'e', 'n', 't', 0};
	text.resize(BU_MIME_DEFAULT_SCAN_LIMIT + 1, 'x');
	append_chunk(data, "tEXt", text);
    }
    bytes control;
    append_word(control, 1); // One animation frame, with unlimited repetitions.
    append_word(control, 0);
    append_chunk(data, "acTL", control);
    bytes frame;
    for (uint32_t value : {0, 1, 1, 0, 0}) append_word(frame, value); // Sequence, dimensions, offsets.
    const unsigned char delay_and_operations[] = {0, 1, 0, 10, 0, 0};
    frame.insert(frame.end(), std::begin(delay_and_operations), std::end(delay_and_operations));
    if (default_is_frame) append_chunk(data, "fcTL", frame);
    append_chunk(data, "IDAT", image_data(red));
    if (!default_is_frame) {
	append_chunk(data, "fcTL", frame);
	bytes payload;
	append_word(payload, 1);
	const bytes green = image_data(png_bytes(context, true));
	payload.insert(payload.end(), green.begin(), green.end());
	append_chunk(data, "fdAT", payload);
    }
    append_chunk(data, "IEND", {});
    return data;
}

void check_default(const icv_image_t *image)
{
    require(image && image->width == 1 && image->height == 1 && image->channels == 4 &&
	EQUAL(image->data[0], 1) && ZERO(image->data[1]) && ZERO(image->data[2]) && EQUAL(image->data[3], 1),
	"still decoder did not return the red default image");
}

icv_metadata_status annotation_metadata(const bu_mime_probe_input *, const char *,
    icv_image_metadata *, const bu_mime_traits *, const bu_mime_traits *)
{
    return ICV_METADATA_UNKNOWN;
}

int annotate_header(const bu_mime_probe_input *input, const char *, bu_mime_traits *traits)
{
    if (input->size < signature_size || memcmp(input->data, "\x89PNG\r\n\x1a\n", signature_size)) return 1;
    const char *members[] = {"signature", "png"};
    return bu_mime_traits_add(traits, "fixture.png-header", "present") &&
	bu_mime_traits_add_set(traits, "fixture.png-signatures", members, sizeof(members) / sizeof(members[0]));
}

void register_annotation(icv_context *context)
{
    const bu_mime_capability formats[] = {
	BU_MIME_CAPABILITY("image/png", NULL, 0, ICV_CODEC_METADATA),
	BU_MIME_CAPABILITY("image/apng", NULL, 0, ICV_CODEC_METADATA)
    };
    /* Observation-only metadata must not replace native PNG metadata. */
    constexpr unsigned int annotation_priority = 0;
    const icv_codec annotation = {"PNG header annotation", formats, sizeof(formats) / sizeof(formats[0]),
	annotation_priority, NULL, NULL, NULL, NULL, NULL, 0, annotation_metadata, annotate_header, NULL, NULL};
    const icv_plugin plugin = {ICV_PLUGIN_ABI_VERSION, sizeof(icv_plugin), NULL, 0, &annotation, 1};
    require(icv_register_plugin(context, &plugin, NULL) != 0, "cannot register independent PNG observations");
}

void check_independent_observations()
{
    std::unique_ptr<icv_context, decltype(&icv_context_destroy)> context(icv_context_create(NULL), icv_context_destroy);
    require(context != nullptr, "cannot create annotated image context");
    const bytes data = png_bytes(context.get(), false);
    char filename[MAXPATHLEN];
    FILE *file = bu_temp_file(filename, sizeof(filename));
    require(file != nullptr, "cannot create annotated PNG fixture");
    const size_t written = fwrite(data.data(), 1, data.size(), file);
    const int closed = fclose(file);
    require(written == data.size() && !closed, "cannot save annotated PNG fixture");
    register_annotation(context.get());
    const bu_mime_probe_input input = {data.data(), data.size(), 1, data.size(), 1};
    bu_mime_result *observed = NULL;
    require(icv_detect(context.get(), NULL, &input, &observed, NULL) == BU_MIME_MATCH, "cannot inspect annotated PNG");
    bu_mime_traits resolved = BU_MIME_TRAITS_INIT_ZERO;
    const bool selected = icv_select_variant(context.get(), ICV_CODEC_READ_MEM, "image/png", NULL,
	observed, NULL, NULL, NULL, &resolved, NULL) != 0;
    const bool retained = BU_STR_EQUAL(bu_mime_traits_get(&resolved, "fixture.png-header"), "present");
    bu_mime_traits_free(&resolved);
    bu_mime_result_free(observed);
    require(selected && retained, "selection discarded an independent PNG observation");
    const bu_mime_trait binary = BU_MIME_TRAIT_STRING("encoding", "binary");
    const bu_mime_traits supported = {&binary, 1};
    const bu_mime_traits empty = BU_MIME_TRAITS_INIT_ZERO;
    const char *members[] = {"png", "signature"};
    const bu_mime_trait annotation_fields[] = {BU_MIME_TRAIT_STRING("fixture.png-header", "present"),
	BU_MIME_TRAIT_SET("fixture.png-signatures", members, sizeof(members) / sizeof(members[0]))};
    const bu_mime_traits annotated = {annotation_fields, sizeof(annotation_fields) / sizeof(annotation_fields[0])};
    const bu_mime_traits *requests[] = {NULL, &empty, &supported, &annotated};
    for (const auto *requested : requests) {
	image_ptr memory(icv_read_mem_variant(context.get(), data.data(), data.size(), "image/png", NULL, requested, 0, 0, NULL), icv_destroy);
	require(memory != nullptr, "independent observations became PNG memory-reader assertions");
	check_default(memory.get());
	image_ptr stream(icv_read_variant(context.get(), filename, "image/png", NULL, requested, 0, 0, NULL), icv_destroy);
	require(stream != nullptr, "independent observations became PNG file-reader assertions");
	check_default(stream.get());
	icv_image_metadata metadata = {};
	require(icv_metadata_variant(context.get(), &input, "image/png", NULL, requested, &metadata, NULL) == ICV_METADATA_MATCH &&
	    metadata.width == 1 && metadata.height == 1 && metadata.channels == 4, "independent observations became PNG metadata assertions");
	require(icv_metadata_file_variant(context.get(), filename, "image/png", NULL, requested, &metadata, NULL) == ICV_METADATA_MATCH &&
	    metadata.width == 1 && metadata.height == 1 && metadata.channels == 4, "independent observations became file metadata assertions");
    }
    const bu_mime_trait unsupported[] = {BU_MIME_TRAIT_STRING("encoding", "ascii"),
	BU_MIME_TRAIT_STRING("fixture.png-header", "absent"), BU_MIME_TRAIT_SET("fixture.png-signatures", NULL, 0),
	BU_MIME_TRAIT_STRING("fixture.unestablished", "value")};
    for (const auto &field : unsupported) {
	const bu_mime_traits requested = {&field, 1};
	image_ptr memory(icv_read_mem_variant(context.get(), data.data(), data.size(), "image/png", NULL, &requested, 0, 0, NULL), icv_destroy);
	require(!memory, "PNG memory reader ignored an explicit unsupported assertion");
	image_ptr stream(icv_read_variant(context.get(), filename, "image/png", NULL, &requested, 0, 0, NULL), icv_destroy);
	require(!stream, "PNG file reader ignored an explicit unsupported assertion");
	icv_image_metadata metadata = {};
	require(icv_metadata_variant(context.get(), &input, "image/png", NULL, &requested, &metadata, NULL) != ICV_METADATA_MATCH &&
	    !metadata.width && !metadata.height, "PNG metadata ignored an explicit unsupported assertion");
	require(icv_metadata_file_variant(context.get(), filename, "image/png", NULL, &requested, &metadata, NULL) != ICV_METADATA_MATCH &&
	    !metadata.width && !metadata.height, "file metadata ignored an explicit unsupported assertion");
    }
    std::unique_ptr<icv_context, decltype(&icv_context_destroy)> isolated(icv_context_create(NULL), icv_context_destroy);
    require(isolated != nullptr, "cannot create isolated PNG context");
    image_ptr rejected(icv_read_mem_variant(isolated.get(), data.data(), data.size(), "image/png", NULL, &annotated, 0, 0, NULL), icv_destroy);
    require(!rejected, "PNG observations escaped their context");
    require(bu_file_delete(filename) != 0, "cannot remove annotated PNG fixture");
}

void check_apng(icv_context *context, bool default_is_frame, bool padded)
{
    const bytes data = apng_bytes(context, default_is_frame, padded);
    char filename[MAXPATHLEN];
    FILE *file = bu_temp_file(filename, sizeof(filename));
    require(file && fwrite(data.data(), 1, data.size(), file) == data.size() && !fclose(file), "cannot save APNG fixture");
    const bu_mime_trait properties[] = {
	BU_MIME_TRAIT_STRING("encoding", "binary"), BU_MIME_TRAIT_STRING("bit-depth", "8"),
	BU_MIME_TRAIT_STRING("png.color-type", "6"), BU_MIME_TRAIT_STRING("png.interlace-method", "0"),
	BU_MIME_TRAIT_STRING("png.animated", "true"), BU_MIME_TRAIT_STRING("apng.frames", "1"),
	BU_MIME_TRAIT_STRING("apng.plays", "0"), BU_MIME_TRAIT_STRING("apng.default-image", default_is_frame ? "frame" : "fallback")
    };
    const bu_mime_traits traits = {properties, sizeof(properties) / sizeof(properties[0])};
    for (const char *type : {"image/apng", "image/png", "png"}) {
	image_ptr memory(icv_read_mem_variant(context, data.data(), data.size(), type, NULL, &traits, 0, 0, NULL), icv_destroy);
	image_ptr decoded(icv_read_variant(context, filename, type, NULL, &traits, 0, 0, NULL), icv_destroy);
	check_default(memory.get());
	check_default(decoded.get());
    }
    std::unique_ptr<icv_anim_t, decltype(&icv_anim_destroy)> animation(
	icv_anim_read_variant(context, filename, "image/apng", NULL, &traits, NULL), icv_anim_destroy);
    require(animation && icv_anim_num_frames(animation.get()) == 1, "sequence reader counted the fallback as an animation frame");
    image_ptr frame(icv_anim_get_frame(animation.get(), 0), icv_destroy);
    require(frame && frame->channels == 3 && EQUAL(frame->data[0], default_is_frame ? 1 : 0) &&
	EQUAL(frame->data[1], default_is_frame ? 0 : 1) && ZERO(frame->data[2]), "sequence reader returned the wrong animation frame");
    const bu_mime_trait invalid[] = {
	BU_MIME_TRAIT_STRING("apng.default-image", default_is_frame ? "fallback" : "frame"),
	BU_MIME_TRAIT_STRING("apng.frames", "2"), BU_MIME_TRAIT_STRING("png.animated", "false"),
	BU_MIME_TRAIT_STRING("png.color-type", "2"), BU_MIME_TRAIT_STRING("fixture.unestablished", "assertion")
    };
    for (const auto &property : invalid) {
	const bu_mime_traits request = {&property, 1};
	image_ptr memory(icv_read_mem_variant(context, data.data(), data.size(), "image/apng", NULL, &request, 0, 0, NULL), icv_destroy);
	image_ptr decoded(icv_read_variant(context, filename, "image/apng", NULL, &request, 0, 0, NULL), icv_destroy);
	require(!memory && !decoded, "APNG accepted an incorrect or unobserved assertion");
    }
    icv_image_metadata metadata = {};
    const bu_mime_probe_input header = {data.data(), ihdr_end, 0, 0, 0};
    require(icv_metadata_variant(context, &header, "image/apng", NULL, &traits, &metadata, NULL) == ICV_METADATA_INCOMPLETE &&
	!metadata.width && !metadata.height, "bounded APNG metadata invented animation facts");
    require(icv_metadata_file_variant(context, filename, "image/apng", NULL, &traits, &metadata, NULL) ==
	(padded ? ICV_METADATA_INCOMPLETE : ICV_METADATA_MATCH), "APNG metadata ignored the inspection boundary");
    require(bu_file_delete(filename) != 0, "cannot remove APNG fixture");
}

void check_still_and_output(icv_context *context)
{
    const bytes data = png_bytes(context, false);
    image_ptr incorrectly_typed(icv_read_mem_with_context(context, data.data(), data.size(), "image/apng", NULL, 0, 0, NULL), icv_destroy);
    require(!incorrectly_typed, "ordinary PNG satisfied an APNG identity assertion");
    icv_image_metadata metadata = {};
    const bu_mime_probe_input prefix = {data.data(), data.size(), 0, 0, 0};
    require(icv_metadata(context, &prefix, "image/apng", NULL, &metadata, NULL) == ICV_METADATA_ERROR &&
	!metadata.width && !metadata.height, "nonanimated IDAT did not disprove APNG metadata selection");
    image_ptr image(icv_read_mem_with_context(context, data.data(), data.size(), "image/png", NULL, 0, 0, NULL), icv_destroy);
    check_default(image.get());
    image_ptr by_extension(icv_read_mem_with_context(context, data.data(), data.size(), "png", NULL, 0, 0, NULL), icv_destroy);
    check_default(by_extension.get());
    char filename[MAXPATHLEN];
    const char marker[] = "Preserve output after an unsupported PNG profile";
    FILE *file = bu_temp_file(filename, sizeof(filename));
    require(file && fwrite(marker, 1, sizeof(marker), file) == sizeof(marker) && !fclose(file), "cannot save output marker");
    const bu_mime_trait unsupported[] = {
	BU_MIME_TRAIT_STRING("bit-depth", "16"), BU_MIME_TRAIT_STRING("png.interlace-method", "1"),
	BU_MIME_TRAIT_STRING("png.animated", "true"), BU_MIME_TRAIT_STRING("encoding", "ascii")};
    for (const auto &property : unsupported) {
	const bu_mime_traits request = {&property, 1};
	unsigned char *buffer = NULL;
	size_t size = 0;
	require(icv_write_variant(context, image.get(), filename, "image/png", NULL, &request, NULL) != 0 &&
	    icv_write_mem_variant(context, image.get(), &buffer, &size, "image/png", NULL, &request, NULL) != 0 &&
	    !buffer && !size, "PNG output accepted an unsupported profile");
    }
    char stored[sizeof(marker)];
    file = fopen(filename, "rb");
    require(file && fread(stored, 1, sizeof(stored), file) == sizeof(stored) && fgetc(file) == EOF && !fclose(file) &&
	!memcmp(stored, marker, sizeof(marker)), "rejected PNG output changed its destination");
    require(bu_file_delete(filename) != 0, "cannot remove output marker");
}

void check_unimplemented_headers(icv_context *context)
{
    const bytes data = png_bytes(context, false);
    const struct {const char *key; size_t offset; unsigned char value;} fields[] = {
	{"bit-depth", 24, 12}, {"png.color-type", 25, 7}, {"png.compression-method", 26, 1},
	{"png.filter-method", 27, 1}, {"png.interlace-method", 28, 2}
    };
    for (const auto &field : fields) {
	bytes changed(data.begin(), data.begin() + ihdr_end);
	changed[field.offset] = field.value;
	bu_mime_result *result = NULL;
	require(bu_mime_detect(icv_mime_registry(context), NULL, changed.data(), changed.size(), 0, &result) == BU_MIME_MATCH,
	    "unimplemented PNG header lost its identity");
	const std::string value = std::to_string(field.value);
	const bool observed = result->count && BU_STR_EQUAL(result->candidates[0].name, "image/png") &&
	    BU_STR_EQUAL(bu_mime_traits_get(&result->candidates[0].traits, field.key), value.c_str());
	bu_mime_result_free(result);
	require(observed, "recognition discarded an unimplemented IHDR field value");
	const bu_mime_probe_input prefix = {changed.data(), changed.size(), 0, 0, 0};
	icv_image_metadata metadata = {};
	require(icv_metadata(context, &prefix, "image/png", NULL, &metadata, NULL) == ICV_METADATA_ERROR &&
	    !metadata.width && !metadata.height, "metadata accepted an unimplemented IHDR layout");
    }
}
}

int
main(int argc, char **argv)
{
    bu_setprogname(argv[0]);
    try {
	std::unique_ptr<icv_context, decltype(&icv_context_destroy)> context(icv_context_create(NULL), icv_context_destroy);
	require(context != nullptr, "cannot create image context");
	if (argc == 2 && (BU_STR_EQUAL(argv[1], "--emit") || BU_STR_EQUAL(argv[1], "--emit-observed"))) {
	    const bytes data = apng_bytes(context.get(), false, BU_STR_EQUAL(argv[1], "--emit"));
	    require(fwrite(data.data(), 1, data.size(), stdout) == data.size(), "cannot emit APNG fixture");
	} else if (argc == 2 && (BU_STR_EQUAL(argv[1], "--stdin") || BU_STR_EQUAL(argv[1], "--stdin-bad") || BU_STR_EQUAL(argv[1], "--stdin-observed"))) {
	    register_annotation(context.get());
	    const bool reject = BU_STR_EQUAL(argv[1], "--stdin-bad");
	    const char *members[] = {"png", "signature"};
	    const bu_mime_trait properties[] = {BU_MIME_TRAIT_STRING("apng.default-image", reject ? "frame" : "fallback"),
		BU_MIME_TRAIT_STRING("fixture.png-header", "present"),
		BU_MIME_TRAIT_SET("fixture.png-signatures", members, sizeof(members) / sizeof(members[0]))};
	    const bu_mime_traits traits = {properties, BU_STR_EQUAL(argv[1], "--stdin-observed") ? sizeof(properties) / sizeof(properties[0]) : 1};
	    image_ptr image(icv_read_variant(context.get(), NULL, "image/apng", NULL, &traits, 0, 0, NULL), icv_destroy);
	    if (reject) require(!image, "APNG pipe accepted a contradictory default-image assertion");
	    else check_default(image.get());
	} else {
	    require(argc == 1, "unexpected PNG test argument");
	    check_independent_observations();
	    for (bool default_is_frame : {false, true})
		for (bool padded : {false, true}) check_apng(context.get(), default_is_frame, padded);
	    check_still_and_output(context.get());
	    check_unimplemented_headers(context.get());
	}
    } catch (const std::exception &error) {
	bu_log("PNG profile test: %s\n", error.what());
	return 1;
    }
    return 0;
}
