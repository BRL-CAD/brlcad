/* C O D E C . C
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

#include <string.h>

#include "bu/app.h"
#include "bu/file.h"
#include "bu/log.h"
#include "bu/malloc.h"
#include "bu/str.h"
#include "icv.h"

#define ICV_CODEC_CHECK(condition) do { \
    if (!(condition)) { \
	bu_log("ICV codec check failed at line %d: %s\n", __LINE__, #condition); \
	return 1; \
    } \
} while (0)

static int
codec_roundtrip(struct icv_context *context, icv_image_t *image, const char *type)
{
    unsigned char *buffer = NULL;
    size_t size = 0;
    struct bu_vls messages = BU_VLS_INIT_ZERO;
    icv_image_t *decoded;
    ICV_CODEC_CHECK(!icv_write_mem_with_context(context, image, &buffer, &size, type, NULL, &messages));
    ICV_CODEC_CHECK(buffer && size);
    if (icv_supports(context, type, ICV_CODEC_METADATA)) {
	struct icv_image_metadata metadata;
	struct bu_mime_probe_input input = {buffer, size, 1, size, 1};
	ICV_CODEC_CHECK(icv_metadata(context, &input, type, NULL, &metadata, &messages) == ICV_METADATA_MATCH);
	ICV_CODEC_CHECK(metadata.width == image->width && metadata.height == image->height);
	input.size = 1;
	input.complete = 0;
	ICV_CODEC_CHECK(icv_metadata(context, &input, type, NULL, &metadata, &messages) == ICV_METADATA_INCOMPLETE);
	ICV_CODEC_CHECK(!metadata.width && !metadata.height && !metadata.channels);
    }
    decoded = icv_read_mem_with_context(context, buffer, size, NULL, NULL, 0, 0, &messages);
    if (!decoded)
	bu_log("%s", bu_vls_cstr(&messages));
    ICV_CODEC_CHECK(decoded && decoded->width == image->width && decoded->height == image->height);
    ICV_CODEC_CHECK(!memcmp(decoded->data, image->data, image->channels * sizeof(double)));
    icv_destroy(decoded);
    {
	char filename[MAXPATHLEN];
	FILE *fp = bu_temp_file(filename, sizeof(filename));
	ICV_CODEC_CHECK(fp);
	ICV_CODEC_CHECK(fwrite(buffer, 1, size, fp) == size);
	ICV_CODEC_CHECK(!fclose(fp));
	decoded = icv_read_with_context(context, filename, NULL, NULL, 0, 0, &messages);
	ICV_CODEC_CHECK(decoded && decoded->width == image->width);
	icv_destroy(decoded);
	ICV_CODEC_CHECK(bu_file_delete(filename));
    }
    bu_free(buffer, "encoded image");
    bu_vls_free(&messages);
    return 0;
}

static int
codec_raw_profile_test(struct icv_context *context)
{
    const size_t side = 2;
    const unsigned char byte_one[] = {255};
    const unsigned char double_one[] = {0x3f, 0xf0, 0, 0, 0, 0, 0, 0};
    const struct {
	const char *type;
	const unsigned char *sample;
	size_t sample_size;
	size_t channels;
	const char *depth;
    } layouts[] = {
	{"image/vnd.brlcad.pix", byte_one, sizeof(byte_one), 3, "8"},
	{"image/vnd.brlcad.bw", byte_one, sizeof(byte_one), 1, "8"},
	{"image/vnd.brlcad.dpix", double_one, sizeof(double_one), 3, "64"}
    };
    const enum icv_codec_operation operations[] = {
	ICV_CODEC_READ, ICV_CODEC_READ_MEM, ICV_CODEC_WRITE, ICV_CODEC_WRITE_MEM, ICV_CODEC_METADATA};
    icv_image_t *white = icv_create(side, side, ICV_COLOR_SPACE_RGB);
    ICV_CODEC_CHECK(white);
    for (size_t i = 0; i < side * side * white->channels; ++i) white->data[i] = 1.0;
    for (size_t i = 0; i < sizeof(layouts) / sizeof(layouts[0]); ++i) {
	const struct bu_mime_trait depth = BU_MIME_TRAIT_STRING("bit-depth", layouts[i].depth);
	const struct bu_mime_traits request = {&depth, 1};
	const struct bu_mime_trait wrong[] = {
	    BU_MIME_TRAIT_STRING("bit-depth", "16"), BU_MIME_TRAIT_STRING("encoding", "ascii"),
	    BU_MIME_TRAIT_STRING("channels", "4"), BU_MIME_TRAIT_STRING("sample-format", "signed-integer"),
	    BU_MIME_TRAIT_STRING("byte-order", "little-endian"), BU_MIME_TRAIT_STRING("color-space", "cmyk")};
	size_t size = side * side * layouts[i].channels * layouts[i].sample_size;
	unsigned char *expected = (unsigned char *)bu_malloc(size, "raw layout fixture");
	unsigned char *encoded = NULL;
	size_t encoded_size = 0;
	icv_image_t *decoded;
	struct icv_image_metadata metadata;
	struct bu_mime_probe_input input = {expected, size, 1, size, 1};
	struct bu_mime_traits resolved = BU_MIME_TRAITS_INIT_ZERO;
	char filename[MAXPATHLEN];
	FILE *stream = bu_temp_file(filename, sizeof(filename));
	for (size_t at = 0; at < size; at += layouts[i].sample_size)
	    memcpy(expected + at, layouts[i].sample, layouts[i].sample_size);
	ICV_CODEC_CHECK(stream && fwrite(expected, 1, size, stream) == size && !fclose(stream));
	ICV_CODEC_CHECK(icv_select_variant(context, ICV_CODEC_READ, layouts[i].type, NULL, NULL, NULL, NULL, NULL, &resolved, NULL));
	ICV_CODEC_CHECK(!resolved.count);
	bu_mime_traits_free(&resolved);
	ICV_CODEC_CHECK(icv_metadata_variant(context, &input, layouts[i].type, NULL, &request, &metadata, NULL) == ICV_METADATA_MATCH);
	ICV_CODEC_CHECK(metadata.width == side && metadata.height == side && metadata.channels == layouts[i].channels);
	ICV_CODEC_CHECK(metadata.color_space == (layouts[i].channels == 1 ? ICV_METADATA_COLOR_GRAY : ICV_METADATA_COLOR_RGB));
	decoded = icv_read_mem_variant(context, expected, size, layouts[i].type, NULL, &request, side, side, NULL);
	ICV_CODEC_CHECK(decoded && decoded->channels == layouts[i].channels);
	for (size_t sample = 0; sample < side * side * decoded->channels; ++sample)
	    ICV_CODEC_CHECK(EQUAL(decoded->data[sample], 1.0));
	icv_destroy(decoded);
	decoded = icv_read_variant(context, filename, layouts[i].type, NULL, &request, 0, 0, NULL);
	ICV_CODEC_CHECK(decoded && decoded->width == side && decoded->height == side && decoded->channels == layouts[i].channels);
	icv_destroy(decoded);
	ICV_CODEC_CHECK(!icv_write_mem_variant(context, white, &encoded, &encoded_size, layouts[i].type, NULL, &request, NULL));
	ICV_CODEC_CHECK(encoded_size == size && !memcmp(encoded, expected, size));
	bu_free(encoded, "raw layout output");
	ICV_CODEC_CHECK(!icv_write_variant(context, white, filename, layouts[i].type, NULL, &request, NULL));
	for (size_t j = 0; j < sizeof(wrong) / sizeof(wrong[0]); ++j) {
	    const struct bu_mime_traits invalid = {&wrong[j], 1};
	    for (size_t op = 0; op < sizeof(operations) / sizeof(operations[0]); ++op)
		ICV_CODEC_CHECK(!icv_select_variant(context, operations[op], layouts[i].type, NULL, NULL, &invalid, NULL, NULL, NULL, NULL));
	    ICV_CODEC_CHECK(!icv_read_mem_variant(context, expected, size, layouts[i].type, NULL, &invalid, side, side, NULL));
	    ICV_CODEC_CHECK(!icv_read_variant(context, filename, layouts[i].type, NULL, &invalid, side, side, NULL));
	    ICV_CODEC_CHECK(icv_metadata_variant(context, &input, layouts[i].type, NULL, &invalid, &metadata, NULL) != ICV_METADATA_MATCH);
	    ICV_CODEC_CHECK(!metadata.width && !metadata.height && !metadata.channels);
	    ICV_CODEC_CHECK(icv_write_variant(context, white, filename, layouts[i].type, NULL, &invalid, NULL));
	}
	stream = fopen(filename, "rb");
	unsigned char *preserved = (unsigned char *)bu_malloc(size, "raw destination");
	ICV_CODEC_CHECK(stream && fread(preserved, 1, size, stream) == size && fgetc(stream) == EOF && !fclose(stream));
	ICV_CODEC_CHECK(!memcmp(preserved, expected, size));
	bu_free(preserved, "raw destination");
	bu_free(expected, "raw layout fixture");
	ICV_CODEC_CHECK(bu_file_delete(filename));
    }
    icv_destroy(white);
    return 0;
}

static int
codec_ppm_input_test(struct icv_context *context)
{
    const char *type = "image/x-portable-pixmap";
    const size_t padding = BU_MIME_DEFAULT_SCAN_LIMIT + 1;
    const unsigned char plain[] = "65535 0 0\n";
    const unsigned char binary[] = {255, 255, 0, 0, 0, 0};
    const struct { const unsigned char *raster; size_t size; const char *encoding; } samples[] = {
	{plain, sizeof(plain) - 1, "ascii"}, {binary, sizeof(binary), "binary"}
    };
    const char suffix[] = "\n1 1\n65535\n";
    for (size_t i = 0; i < sizeof(samples) / sizeof(samples[0]); ++i) {
	const struct bu_mime_trait facts[] = {
	    BU_MIME_TRAIT_STRING("encoding", samples[i].encoding), BU_MIME_TRAIT_STRING("ppm.maxval", "65535")};
	const struct bu_mime_traits request = {facts, sizeof(facts) / sizeof(facts[0])};
	const struct bu_mime_trait unsupported[] = {
	    BU_MIME_TRAIT_STRING("encoding", i ? "ascii" : "binary"),
	    BU_MIME_TRAIT_STRING("ppm.maxval", "255"), BU_MIME_TRAIT_STRING("bit-depth", "8"),
	    BU_MIME_TRAIT_STRING("byte-order", "little-endian")};
	const size_t prefix_size = sizeof("P3\n#") - 1;
	size_t size = prefix_size + padding + sizeof(suffix) - 1 + samples[i].size;
	unsigned char *buffer = (unsigned char *)bu_malloc(size, "PPM large header fixture");
	char filename[MAXPATHLEN];
	FILE *stream = bu_temp_file(filename, sizeof(filename));
	struct icv_image_metadata metadata;
	icv_image_t *image;
	memcpy(buffer, i ? "P6\n#" : "P3\n#", prefix_size);
	memset(buffer + prefix_size, 'x', padding);
	memcpy(buffer + prefix_size + padding, suffix, sizeof(suffix) - 1);
	memcpy(buffer + size - samples[i].size, samples[i].raster, samples[i].size);
	ICV_CODEC_CHECK(stream && fwrite(buffer, 1, size, stream) == size && !fclose(stream));
	ICV_CODEC_CHECK(icv_metadata_file_variant(context, filename, type, NULL, &request, &metadata, NULL) == ICV_METADATA_INCOMPLETE);
	ICV_CODEC_CHECK(!metadata.width && !metadata.height);
	image = icv_read_mem_variant(context, buffer, size, type, NULL, &request, 0, 0, NULL);
	ICV_CODEC_CHECK(image && EQUAL(image->data[0], 1.0) && ZERO(image->data[1]) && ZERO(image->data[2]));
	icv_destroy(image);
	image = icv_read_variant(context, filename, type, NULL, &request, 0, 0, NULL);
	ICV_CODEC_CHECK(image && EQUAL(image->data[0], 1.0) && ZERO(image->data[1]) && ZERO(image->data[2]));
	icv_destroy(image);
	for (size_t j = 0; j < sizeof(unsupported) / sizeof(unsupported[0]); ++j) {
	    const struct bu_mime_traits invalid = {&unsupported[j], 1};
	    ICV_CODEC_CHECK(!icv_read_mem_variant(context, buffer, size, type, NULL, &invalid, 0, 0, NULL));
	    ICV_CODEC_CHECK(!icv_read_variant(context, filename, type, NULL, &invalid, 0, 0, NULL));
	}
	ICV_CODEC_CHECK(bu_file_delete(filename));
	bu_free(buffer, "PPM large header fixture");
    }
    return 0;
}

static int
codec_ppm_profile_test(struct icv_context *context, icv_image_t *image)
{
    const char *type = "image/x-portable-pixmap";
    struct bu_mime_trait binary = BU_MIME_TRAIT_STRING("encoding", "binary");
    const struct bu_mime_traits request = {&binary, 1};
    const struct bu_mime_trait unsupported[] = {
	BU_MIME_TRAIT_STRING("encoding", "ascii"),
	BU_MIME_TRAIT_STRING("bit-depth", "16"),
	BU_MIME_TRAIT_STRING("ppm.maxval", "65535")
    };
    const unsigned char plain[] = "P3\n1 1\n65535\n65535 0 0\n";
    const char expected_header[] = "P6\n1 1\n255\n";
    const unsigned char sentinel[] = "preserved destination";
    unsigned char *buffer = NULL;
    size_t size = 0;
    char filename[MAXPATHLEN];
    FILE *stream;
    icv_image_t *decoded;

    ICV_CODEC_CHECK(image->width == 1 && image->height == 1);
    ICV_CODEC_CHECK(!icv_write_mem_variant(context, image, &buffer, &size, type, NULL, &request, NULL));
    ICV_CODEC_CHECK(size >= sizeof(expected_header) - 1);
    ICV_CODEC_CHECK(!memcmp(buffer, expected_header, sizeof(expected_header) - 1));
    decoded = icv_read_mem_with_context(context, buffer, size, type, NULL, 0, 0, NULL);
    ICV_CODEC_CHECK(decoded && decoded->width == 1 && decoded->height == 1);
    icv_destroy(decoded);
    bu_free(buffer, "PPM profile output");

    decoded = icv_read_mem_with_context(context, plain, sizeof(plain) - 1, type, NULL, 0, 0, NULL);
    ICV_CODEC_CHECK(decoded && decoded->width == 1 && decoded->height == 1);
    ICV_CODEC_CHECK(EQUAL(decoded->data[0], 1.0) && ZERO(decoded->data[1]) && ZERO(decoded->data[2]));
    icv_destroy(decoded);

    stream = bu_temp_file(filename, sizeof(filename));
    ICV_CODEC_CHECK(stream && fwrite(sentinel, 1, sizeof(sentinel), stream) == sizeof(sentinel) && !fclose(stream));
    for (size_t i = 0; i < sizeof(unsupported) / sizeof(unsupported[0]); ++i) {
	const struct bu_mime_traits invalid = {&unsupported[i], 1};
	unsigned char preserved[sizeof(sentinel)];
	buffer = NULL;
	size = 0;
	ICV_CODEC_CHECK(icv_write_mem_variant(context, image, &buffer, &size, type, NULL, &invalid, NULL));
	ICV_CODEC_CHECK(!buffer && !size);
	ICV_CODEC_CHECK(icv_write_variant(context, image, filename, type, NULL, &invalid, NULL));
	stream = fopen(filename, "rb");
	ICV_CODEC_CHECK(stream && fread(preserved, 1, sizeof(preserved), stream) == sizeof(preserved));
	ICV_CODEC_CHECK(fgetc(stream) == EOF && !ferror(stream) && !fclose(stream));
	ICV_CODEC_CHECK(!memcmp(preserved, sentinel, sizeof(sentinel)));
    }
    ICV_CODEC_CHECK(bu_file_delete(filename));
    return 0;
}

static int
codec_sun_test(struct icv_context *context, icv_image_t *image)
{
    const char *type = "image/x-sun-raster";
    struct bu_mime_trait depth[] = {BU_MIME_TRAIT_STRING("bit-depth", "8")};
    const struct bu_mime_traits request = {depth, sizeof(depth) / sizeof(depth[0])};
    unsigned char *buffer = NULL;
    size_t size = 0;
    struct bu_mime_result *detected = NULL;
    struct icv_image_metadata metadata;
    struct bu_mime_probe_input input;
    char filename[MAXPATHLEN];
    ICV_CODEC_CHECK(icv_supports(context, type, ICV_CODEC_WRITE | ICV_CODEC_WRITE_MEM | ICV_CODEC_METADATA));
    ICV_CODEC_CHECK(!icv_supports(context, type, ICV_CODEC_READ));
    ICV_CODEC_CHECK(!icv_supports(context, type, ICV_CODEC_READ_MEM));
    ICV_CODEC_CHECK(!icv_write_mem_variant(context, image, &buffer, &size, type, NULL, &request, NULL));
    input.data = buffer;
    input.size = input.file_size = size;
    input.complete = input.file_size_known = 1;
    ICV_CODEC_CHECK(icv_detect(context, NULL, &input, &detected, NULL) == BU_MIME_MATCH);
    ICV_CODEC_CHECK(detected->count && BU_STR_EQUAL(detected->candidates[0].name, type));
    ICV_CODEC_CHECK(BU_STR_EQUAL(bu_mime_traits_get(&detected->candidates[0].traits, "bit-depth"), "8"));
    ICV_CODEC_CHECK(BU_STR_EQUAL(bu_mime_traits_get(&detected->candidates[0].traits, "sun.raster-type"), "1"));
    ICV_CODEC_CHECK(BU_STR_EQUAL(bu_mime_traits_get(&detected->candidates[0].traits, "sun.map-type"), "1"));
    bu_mime_result_free(detected);
    ICV_CODEC_CHECK(icv_metadata(context, &input, type, NULL, &metadata, NULL) == ICV_METADATA_MATCH);
    ICV_CODEC_CHECK(metadata.width == image->width && metadata.height == image->height);
    ICV_CODEC_CHECK(icv_metadata_variant(context, &input, type, NULL, &request, &metadata, NULL) == ICV_METADATA_MATCH);
    const struct bu_mime_trait unsupported[] = {
	BU_MIME_TRAIT_STRING("version", "1"), BU_MIME_TRAIT_STRING("encoding", "ascii"),
	BU_MIME_TRAIT_STRING("bit-depth", "16"), BU_MIME_TRAIT_STRING("sun.map-type", "0")};
    for (size_t i = 0; i < sizeof(unsupported) / sizeof(unsupported[0]); ++i) {
	const struct bu_mime_traits invalid = {&unsupported[i], 1};
	ICV_CODEC_CHECK(icv_metadata_variant(context, &input, type, NULL, &invalid, &metadata, NULL) != ICV_METADATA_MATCH);
	ICV_CODEC_CHECK(!metadata.width && !metadata.height && !metadata.channels);
    }
    input.size = 1;
    input.complete = 0;
    ICV_CODEC_CHECK(icv_metadata(context, &input, type, NULL, &metadata, NULL) == ICV_METADATA_INCOMPLETE);
    ICV_CODEC_CHECK(!metadata.width && !metadata.height && !metadata.channels);
    ICV_CODEC_CHECK(!icv_read_mem_with_context(context, buffer, size, type, NULL, 0, 0, NULL));
    FILE *stream = bu_temp_file(filename, sizeof(filename));
    ICV_CODEC_CHECK(stream && !fclose(stream));
    ICV_CODEC_CHECK(!icv_write_variant(context, image, filename, type, NULL, &request, NULL));
    stream = fopen(filename, "rb");
    unsigned char *written = (unsigned char *)bu_malloc(size, "Sun output");
    ICV_CODEC_CHECK(stream && fread(written, 1, size, stream) == size);
    ICV_CODEC_CHECK(fgetc(stream) == EOF && !ferror(stream) && !fclose(stream));
    ICV_CODEC_CHECK(!memcmp(buffer, written, size));
    ICV_CODEC_CHECK(icv_metadata_file_variant(context, filename, type, NULL, &request, &metadata, NULL) == ICV_METADATA_MATCH);
    for (size_t i = 0; i < sizeof(unsupported) / sizeof(unsupported[0]); ++i) {
	const struct bu_mime_traits invalid = {&unsupported[i], 1};
	ICV_CODEC_CHECK(icv_metadata_file_variant(context, filename, type, NULL, &invalid, &metadata, NULL) != ICV_METADATA_MATCH);
	ICV_CODEC_CHECK(!metadata.width && !metadata.height && !metadata.channels);
    }
    ICV_CODEC_CHECK(bu_file_delete(filename));
    bu_free(written, "Sun output");
    bu_free(buffer, "Sun encoded image");
    buffer = NULL;
    size = 0;
    depth[0].value = "16";
    ICV_CODEC_CHECK(icv_write_mem_variant(context, image, &buffer, &size, type, NULL, &request, NULL));
    ICV_CODEC_CHECK(!buffer && !size);
    return 0;
}

static int
codec_metadata_test(struct icv_context *context, icv_image_t *image)
{
    unsigned char *buffer = NULL;
    size_t size = 0;
    struct icv_image_metadata metadata;
    struct bu_mime_probe_input input;
    ICV_CODEC_CHECK(!icv_write_mem_with_context(context, image, &buffer, &size, "image/jpeg", NULL, NULL));
    input.data = buffer;
    input.size = size;
    input.complete = input.file_size_known = 1;
    input.file_size = size;
    ICV_CODEC_CHECK(icv_metadata(context, &input, NULL, NULL, &metadata, NULL) == ICV_METADATA_MATCH);
    ICV_CODEC_CHECK(metadata.width == image->width && metadata.height == image->height && metadata.channels == 3);
    for (size_t prefix = 0; prefix < size; ++prefix) {
	input.size = prefix;
	input.complete = 0;
	enum icv_metadata_status status = icv_metadata(context, &input, "image/jpeg", NULL, &metadata, NULL);
	ICV_CODEC_CHECK(status == ICV_METADATA_MATCH || status == ICV_METADATA_INCOMPLETE);
	if (status == ICV_METADATA_INCOMPLETE)
	    ICV_CODEC_CHECK(!metadata.width && !metadata.height && !metadata.channels);
    }
    bu_free(buffer, "JPEG metadata input");
    ICV_CODEC_CHECK(!icv_write_mem_with_context(context, image, &buffer, &size, "image/png", NULL, NULL));
    {
	unsigned char header[33];
	ICV_CODEC_CHECK(size >= sizeof(header));
	memcpy(header, buffer, sizeof(header));
	/* Huge dimensions are header properties, requiring no pixel storage. */
	memset(header + 16, 0xff, 8);
	header[16] = header[20] = 0x7f;
	input.data = header;
	input.size = sizeof(header);
	input.complete = 0;
	ICV_CODEC_CHECK(icv_metadata(context, &input, "image/png", NULL, &metadata, NULL) == ICV_METADATA_MATCH);
	ICV_CODEC_CHECK(metadata.width == INT32_MAX && metadata.height == INT32_MAX);
	memset(header + 16, 0, 4);
	ICV_CODEC_CHECK(icv_metadata(context, &input, "image/png", NULL, &metadata, NULL) == ICV_METADATA_ERROR);
	ICV_CODEC_CHECK(!metadata.width && !metadata.height && !metadata.channels);
    }
    bu_free(buffer, "PNG metadata input");
    return 0;
}

static int
codec_animation_test(struct icv_context *context, icv_image_t *image)
{
    const icv_anim_format_t formats[] = {ICV_ANIM_APNG, ICV_ANIM_MJPG};
    for (size_t i = 0; i < sizeof(formats) / sizeof(formats[0]); ++i) {
	char filename[MAXPATHLEN];
	FILE *fp = bu_temp_file(filename, sizeof(filename));
	struct bu_mime_result *result = NULL;
	icv_anim_t *animation = icv_anim_create(formats[i], 1, 1, 10);
	icv_anim_t *decoded;
	ICV_CODEC_CHECK(fp && !fclose(fp) && animation);
	ICV_CODEC_CHECK(!icv_anim_add_frame(animation, image));
	ICV_CODEC_CHECK(!icv_anim_write(animation, filename));
	ICV_CODEC_CHECK(icv_detect_file(context, filename, &result, NULL) == BU_MIME_MATCH);
	ICV_CODEC_CHECK(BU_STR_EQUAL(result->candidates[0].name,
	    formats[i] == ICV_ANIM_APNG ? "image/apng" : "video/x-msvideo"));
	bu_mime_result_free(result);
	decoded = icv_anim_read(filename);
	ICV_CODEC_CHECK(decoded);
	icv_anim_destroy(decoded);
	icv_anim_destroy(animation);
	if (formats[i] == ICV_ANIM_MJPG) {
	    unsigned char prefix[BU_MIME_DEFAULT_SCAN_LIMIT];
	    fp = fopen(filename, "r+b");
	    ICV_CODEC_CHECK(fp);
	    size_t count = fread(prefix, 1, sizeof(prefix), fp);
	    size_t at;
	    for (at = 0; at + 8 <= count; ++at)
		if (!memcmp(prefix + at, "vidsMJPG", 8))
		    break;
	    ICV_CODEC_CHECK(at + 8 <= count);
	    ICV_CODEC_CHECK(!fseek(fp, (long)(at + 4), SEEK_SET));
	    ICV_CODEC_CHECK(fwrite("DIB ", 1, 4, fp) == 4 && !fclose(fp));
	    ICV_CODEC_CHECK(!icv_anim_read(filename));
	} else {
	    ICV_CODEC_CHECK(!icv_write_with_context(context, image, filename, "image/png", NULL, NULL));
	    ICV_CODEC_CHECK(!icv_anim_read(filename));
	}
	ICV_CODEC_CHECK(bu_file_delete(filename));
    }
    return 0;
}

static icv_image_t *
codec_reject(const unsigned char *UNUSED(buffer), size_t UNUSED(size), size_t UNUSED(width), size_t UNUSED(height), const char *UNUSED(type), const struct bu_mime_traits *UNUSED(traits), const struct bu_mime_traits *UNUSED(observations))
{
    return NULL;
}

static enum bu_mime_probe_status
codec_header_probe(const struct bu_mime_probe_input *input, const char *UNUSED(type))
{
    const size_t header_size = 3;
    if (input->size < header_size)
	return input->complete ? BU_MIME_PROBE_NO_MATCH : BU_MIME_PROBE_INCOMPLETE;
    return memcmp(input->data, "HDR", header_size) ? BU_MIME_PROBE_NO_MATCH : BU_MIME_PROBE_SIGNATURE;
}

static int
codec_raw_inspect(const struct bu_mime_probe_input *input, const char *UNUSED(type), struct bu_mime_traits *traits)
{
    return !input->size || input->data[0] != 'A' || bu_mime_traits_add(traits, "example.first-byte", "a");
}

static enum icv_metadata_status
codec_header_metadata(const struct bu_mime_probe_input *input, const char *type, struct icv_image_metadata *metadata, const struct bu_mime_traits *UNUSED(traits), const struct bu_mime_traits *UNUSED(observations))
{
    if (!BU_STR_EQUAL(type, "image/vnd.example.header") || codec_header_probe(input, type) != BU_MIME_PROBE_SIGNATURE)
	return ICV_METADATA_ERROR;
    metadata->width = 2;
    metadata->height = 3;
    metadata->channels = 1;
    metadata->color_space = ICV_METADATA_COLOR_GRAY;
    return ICV_METADATA_MATCH;
}

static int
codec_pipe_test(int emitting)
{
    struct icv_context *context = icv_context_create(NULL);
    struct bu_vls messages = BU_VLS_INIT_ZERO;
    icv_image_t *image;
    if (emitting) {
	const unsigned char raw[] = {255, 0, 0};
	image = icv_read_mem_with_context(context, raw, sizeof(raw), "image/vnd.brlcad.pix", NULL, 1, 1, &messages);
	ICV_CODEC_CHECK(image);
	if (emitting == 2) {
	    unsigned char *encoded = NULL;
	    size_t size = 0;
	    const size_t png_header_size = 33;
	    ICV_CODEC_CHECK(!icv_write_mem_with_context(context, image, &encoded, &size, "image/png", NULL, &messages));
	    ICV_CODEC_CHECK(size >= png_header_size && fwrite(encoded, 1, png_header_size, stdout) == png_header_size);
	    bu_free(encoded, "PNG header fixture");
	} else {
	    ICV_CODEC_CHECK(!icv_write_with_context(context, image, NULL, "image/png", NULL, &messages));
	}
    } else {
	image = icv_read_with_context(context, NULL, NULL, NULL, 0, 0, &messages);
	if (!image)
	    bu_log("%s", bu_vls_cstr(&messages));
	ICV_CODEC_CHECK(image && image->width == 1 && image->height == 1 && image->channels == 3);
	{
	    unsigned char *pixel = icv_data2uchar(image);
	    ICV_CODEC_CHECK(pixel && pixel[0] == 255 && pixel[1] == 0 && pixel[2] == 0);
	    bu_free(pixel, "decoded pixel");
	}
    }
    icv_destroy(image);
    icv_context_destroy(context);
    bu_vls_free(&messages);
    return 0;
}

static const char *
codec_variant_encoding(const struct bu_mime_traits *traits)
{
    return bu_mime_traits_get(traits, "encoding");
}

static int
codec_variant_input_matches(const unsigned char *buffer, size_t size,
    const char *type, const struct bu_mime_traits *requested, const struct bu_mime_traits *observations)
{
    if (!BU_STR_EQUAL(type, "image/vnd.example.variant") || size != 1 ||
	(buffer[0] != 'A' && buffer[0] != 'B'))
	return 0;
    const struct bu_mime_trait encoding = BU_MIME_TRAIT_STRING("encoding", buffer[0] == 'A' ? "ascii" : "binary");
    const struct bu_mime_traits observed = {&encoding, 1};
    return bu_mime_traits_match(&observed, observations, requested);
}

static icv_image_t *
codec_variant_read_memory(const unsigned char *buffer, size_t size,
    size_t UNUSED(width), size_t UNUSED(height), const char *type, const struct bu_mime_traits *traits, const struct bu_mime_traits *observations)
{
    if (!codec_variant_input_matches(buffer, size, type, traits, observations))
	return NULL;
    return icv_create(1, 1, ICV_COLOR_SPACE_RGB);
}

static icv_image_t *
codec_variant_read_stream(FILE *fp, size_t width, size_t height,
    const char *type, const struct bu_mime_traits *traits, const struct bu_mime_traits *observations)
{
    unsigned char value;
    return fread(&value, 1, 1, fp) == 1 ? codec_variant_read_memory(&value, 1, width, height, type, traits, observations) : NULL;
}

static int
codec_variant_write_memory(icv_image_t *UNUSED(image), unsigned char **buffer, size_t *size,
    const char *type, const struct bu_mime_traits *traits)
{
    const char *encoding = codec_variant_encoding(traits);
    if (!BU_STR_EQUAL(type, "image/vnd.example.variant") || !encoding)
	return BRLCAD_ERROR;
    *buffer = (unsigned char *)bu_malloc(1, "variant image");
    (*buffer)[0] = BU_STR_EQUAL(encoding, "ascii") ? 'A' : 'B';
    *size = 1;
    return 0;
}

static int
codec_variant_write_stream(icv_image_t *image, FILE *fp,
    const char *type, const struct bu_mime_traits *traits)
{
    unsigned char *buffer = NULL;
    size_t size = 0;
    if (codec_variant_write_memory(image, &buffer, &size, type, traits))
	return BRLCAD_ERROR;
    int status = fwrite(buffer, 1, size, fp) == size ? 0 : BRLCAD_ERROR;
    bu_free(buffer, "variant image");
    return status;
}

static enum bu_mime_probe_status
codec_variant_probe(const struct bu_mime_probe_input *input, const char *UNUSED(type))
{
    return input->size && (input->data[0] == 'A' || input->data[0] == 'B') ?
	BU_MIME_PROBE_SIGNATURE : BU_MIME_PROBE_NO_MATCH;
}

static int
codec_variant_inspect(const struct bu_mime_probe_input *input,
    const char *UNUSED(type), struct bu_mime_traits *traits)
{
    return !input->size || bu_mime_traits_add(traits, "encoding", input->data[0] == 'A' ? "ascii" : "binary");
}

static enum icv_metadata_status
codec_variant_metadata(const struct bu_mime_probe_input *input, const char *type,
    struct icv_image_metadata *metadata, const struct bu_mime_traits *traits, const struct bu_mime_traits *observations)
{
    if (!codec_variant_input_matches(input->data, input->size, type, traits, observations))
	return ICV_METADATA_UNKNOWN;
    metadata->width = 2;
    metadata->height = 3;
    metadata->channels = 1;
    metadata->color_space = ICV_METADATA_COLOR_GRAY;
    return ICV_METADATA_MATCH;
}

static int
codec_feature_test(void)
{
    struct icv_context *context = icv_context_create(NULL);
    const char *features[] = {"feature-a", "feature-b"};
    const struct bu_mime_trait supported[] = {
	BU_MIME_TRAIT_STRING("encoding", "binary"),
	BU_MIME_TRAIT_SET("example.required-features", features, 2)
    };
    const struct bu_mime_capability capability = BU_MIME_CAPABILITY("image/vnd.example.variant", supported, 2, ICV_CODEC_WRITE_MEM);
    const struct bu_mime_definition definition = {BU_MIME_ABI_VERSION, sizeof(struct bu_mime_definition),
	"image/vnd.example.variant", NULL, NULL, 0, NULL, 0, NULL, 0};
    const struct icv_codec codec = {"Feature writer", &capability, 1, BU_MIME_DEFAULT_PRIORITY,
	NULL, NULL, NULL, codec_variant_write_memory, NULL, 0, NULL, NULL, NULL, NULL};
    const struct icv_plugin plugin = {ICV_PLUGIN_ABI_VERSION, sizeof(struct icv_plugin), &definition, 1, &codec, 1};
    struct bu_mime_traits requested = BU_MIME_TRAITS_INIT_ZERO;
    struct bu_mime_traits resolved = BU_MIME_TRAITS_INIT_ZERO;
    struct bu_vls selected = BU_VLS_INIT_ZERO;
    struct icv_codec_info *snapshot = NULL;
    size_t count = 0;
    ICV_CODEC_CHECK(icv_register_plugin(context, &plugin, NULL));
    ICV_CODEC_CHECK(bu_mime_traits_add_set(&requested, "example.required-features", features, 1));
    ICV_CODEC_CHECK(icv_select_variant(context, ICV_CODEC_WRITE_MEM, definition.name, NULL, NULL,
	&requested, NULL, &selected, &resolved, NULL));
    ICV_CODEC_CHECK(BU_STR_EQUAL(bu_vls_cstr(&selected), codec.name));
    const char *const *members = NULL;
    size_t member_count = 0;
    ICV_CODEC_CHECK(bu_mime_traits_get_set(&resolved, "example.required-features", &members, &member_count) && member_count == 1);
    ICV_CODEC_CHECK(BU_STR_EQUAL(bu_mime_traits_get(&resolved, "encoding"), "binary"));
    bu_mime_traits_free(&requested);
    const char *unsupported[] = {"feature-c"};
    ICV_CODEC_CHECK(bu_mime_traits_add_set(&requested, "example.required-features", unsupported, 1));
    ICV_CODEC_CHECK(!icv_select_variant(context, ICV_CODEC_WRITE_MEM, definition.name, NULL, NULL,
	&requested, NULL, &selected, &resolved, NULL));
    ICV_CODEC_CHECK(!resolved.count && !bu_vls_strlen(&selected));
    ICV_CODEC_CHECK(icv_list_codecs(context, definition.name, &snapshot, &count) && count == 1);
    icv_context_destroy(context);
    ICV_CODEC_CHECK(bu_mime_traits_get_set(&snapshot[0].traits, "example.required-features", &members, &member_count) && member_count == 2);
    icv_codec_list_free(snapshot, count);
    bu_mime_traits_free(&requested);
    bu_mime_traits_free(&resolved);
    bu_vls_free(&selected);
    return 0;
}

static int
codec_variant_test(void)
{
    struct icv_context *context = icv_context_create(NULL);
    const char *type = "image/vnd.example.variant";
    const struct bu_mime_definition definition = {BU_MIME_ABI_VERSION, sizeof(struct bu_mime_definition),
	type, "Variant image", NULL, 0, NULL, 0, NULL, 0};
    const struct bu_mime_trait ascii[] = {BU_MIME_TRAIT_STRING("encoding", "ascii")};
    const struct bu_mime_trait binary[] = {
        BU_MIME_TRAIT_STRING("encoding", "binary"),
        BU_MIME_TRAIT_STRING("byte-order", "little-endian"),
    };
    const struct bu_mime_capability ascii_cap = BU_MIME_CAPABILITY(type, ascii, 1, 0);
    const struct bu_mime_capability binary_caps[] = {
	{type, binary, 1, ICV_CODEC_READ | ICV_CODEC_READ_MEM | ICV_CODEC_METADATA, BU_MIME_DEFER_INPUT_VALIDATION},
	BU_MIME_CAPABILITY(type, binary, 2, ICV_CODEC_WRITE_MEM)
    };
    const struct icv_codec codecs[] = {
	{"ASCII only", &ascii_cap, 1, BU_MIME_MAX_PRIORITY, codec_variant_read_stream, codec_variant_read_memory,
	 codec_variant_write_stream, codec_variant_write_memory, codec_variant_probe, 0, codec_variant_metadata, codec_variant_inspect, NULL, NULL},
	{"Binary memory writer", binary_caps, 2, BU_MIME_DEFAULT_PRIORITY, codec_variant_read_stream, codec_variant_read_memory,
	 NULL, codec_variant_write_memory, codec_variant_probe, 0, codec_variant_metadata, codec_variant_inspect, NULL, NULL}
    };
    const struct icv_plugin plugin = {ICV_PLUGIN_ABI_VERSION, sizeof(struct icv_plugin), &definition, 1, codecs, 2};
    struct bu_mime_traits request = {binary + 1, 1};
    struct bu_mime_traits ascii_request = {ascii, 1};
    unsigned char *buffer = NULL;
    size_t size = 0;
    const unsigned char bytes[] = {'B'};
    char filename[MAXPATHLEN];
    FILE *fp;
    icv_image_t *image;
    ICV_CODEC_CHECK(context && icv_register_plugin(context, &plugin, NULL));
    /* Full input may defer missing facts; a prefix-only metadata query cannot. */
    ICV_CODEC_CHECK(icv_select_variant(context, ICV_CODEC_READ_MEM, type, "Binary memory writer", NULL,
	NULL, NULL, NULL, NULL, NULL));
    ICV_CODEC_CHECK(!icv_select_variant(context, ICV_CODEC_METADATA, type, "Binary memory writer", NULL,
	NULL, NULL, NULL, NULL, NULL));

    {
	struct bu_mime_result *detected = NULL;
	struct bu_mime_probe_input input = {bytes, sizeof(bytes), 1, sizeof(bytes), 1};
	struct bu_vls resolved = BU_VLS_INIT_ZERO;
	struct bu_vls selected = BU_VLS_INIT_ZERO;
	struct bu_mime_traits traits = BU_MIME_TRAITS_INIT_ZERO;
	struct icv_image_metadata metadata;
	ICV_CODEC_CHECK(icv_detect(context, NULL, &input, &detected, NULL) == BU_MIME_MATCH);
	ICV_CODEC_CHECK(icv_select_variant(context, ICV_CODEC_READ_MEM, NULL, NULL, detected,
	    NULL, &resolved, &selected, &traits, NULL));
	ICV_CODEC_CHECK(BU_STR_EQUAL(bu_vls_cstr(&resolved), type));
	ICV_CODEC_CHECK(BU_STR_EQUAL(bu_vls_cstr(&selected), "Binary memory writer"));
	ICV_CODEC_CHECK(BU_STR_EQUAL(bu_mime_traits_get(&traits, "encoding"), "binary"));
	ICV_CODEC_CHECK(!icv_select_variant(context, ICV_CODEC_READ_MEM, NULL, "ASCII only", detected,
	    NULL, &resolved, &selected, &traits, NULL));
	ICV_CODEC_CHECK(!bu_vls_strlen(&resolved) && !bu_vls_strlen(&selected) && !traits.count);
	ICV_CODEC_CHECK(icv_select_variant(context, ICV_CODEC_WRITE_MEM, type, NULL, NULL,
	    &request, &resolved, &selected, &traits, NULL));
	ICV_CODEC_CHECK(BU_STR_EQUAL(bu_mime_traits_get(&traits, "encoding"), "binary"));
	ICV_CODEC_CHECK(!icv_select_variant(context, ICV_CODEC_WRITE, type, NULL, NULL,
	    &request, &resolved, &selected, &traits, NULL));
	ICV_CODEC_CHECK(icv_metadata_variant(context, &input, NULL, NULL, NULL, &metadata, NULL) == ICV_METADATA_MATCH);
	ICV_CODEC_CHECK(metadata.width == 2 && metadata.height == 3);
	ICV_CODEC_CHECK(icv_metadata_variant(context, &input, NULL, NULL, &ascii_request, &metadata, NULL) != ICV_METADATA_MATCH);
	ICV_CODEC_CHECK(!metadata.width && !metadata.height);
	bu_mime_result_free(detected);
	bu_vls_free(&resolved);
	bu_vls_free(&selected);
	bu_mime_traits_free(&traits);
    }

    image = icv_read_mem_variant(context, bytes, sizeof(bytes), type, NULL, NULL, 0, 0, NULL);
    ICV_CODEC_CHECK(image);
    ICV_CODEC_CHECK(!icv_read_mem_variant(context, bytes, sizeof(bytes), type, "ASCII only", NULL, 0, 0, NULL));
    ICV_CODEC_CHECK(!icv_read_mem_variant(context, bytes, sizeof(bytes), type, NULL, &ascii_request, 0, 0, NULL));
    ICV_CODEC_CHECK(!icv_read_mem_variant(context, (const unsigned char *)"U", 1, type, NULL, NULL, 0, 0, NULL));
    ICV_CODEC_CHECK(!icv_write_mem_variant(context, image, &buffer, &size, type, NULL, &request, NULL));
    ICV_CODEC_CHECK(size == 1 && buffer[0] == 'B');
    bu_free(buffer, "variant image");
    fp = bu_temp_file(filename, sizeof(filename));
    ICV_CODEC_CHECK(fp && !fclose(fp) && bu_file_delete(filename));
    ICV_CODEC_CHECK(icv_write_variant(context, image, filename, type, NULL, &request, NULL));
    ICV_CODEC_CHECK(!bu_file_exists(filename, NULL));
    ICV_CODEC_CHECK(!icv_write_variant(context, image, filename, type, NULL, &ascii_request, NULL));
    {
	icv_image_t *decoded = icv_read_variant(context, filename, type, NULL, NULL, 0, 0, NULL);
	ICV_CODEC_CHECK(decoded);
	icv_destroy(decoded);
    }
    ICV_CODEC_CHECK(bu_file_delete(filename));
    ICV_CODEC_CHECK(icv_supports(context, "image/png", ICV_CODEC_READ | ICV_CODEC_WRITE | ICV_CODEC_WRITE_MEM));
    icv_destroy(image);
    icv_context_destroy(context);
    return 0;
}

static enum icv_metadata_status
codec_observation_metadata(const struct bu_mime_probe_input *UNUSED(input), const char *UNUSED(type),
    struct icv_image_metadata *UNUSED(metadata), const struct bu_mime_traits *UNUSED(requested),
    const struct bu_mime_traits *UNUSED(observations))
{
    return ICV_METADATA_UNKNOWN;
}

static int
codec_observe_source(const struct bu_mime_probe_input *input, const char *type, struct bu_mime_traits *traits)
{
    struct bu_mime_registry *registry = bu_mime_registry_create(0);
    struct bu_mime_result *result = NULL;
    int valid = registry && bu_mime_identify(registry, NULL, input, &result) != BU_MIME_ERROR;
    for (size_t i = 0; valid && result && i < result->count; ++i) {
	const struct bu_mime_candidate *candidate = &result->candidates[i];
	if (BU_STR_EQUAL(candidate->name, type) && (candidate->evidence == BU_MIME_EVIDENCE_SIGNATURE ||
	    candidate->evidence == BU_MIME_EVIDENCE_HEURISTIC)) {
	    const char *members[] = {"content"};
	    valid = bu_mime_traits_add(traits, "fixture.source-type", type) &&
		bu_mime_traits_add_set(traits, "fixture.source-evidence", members, 1);
	}
    }
    bu_mime_result_free(result);
    bu_mime_registry_destroy(registry);
    return valid;
}

static int
codec_observation_test(icv_image_t *source)
{
    const char *types[] = {"image/png", "image/jpeg", "image/x-portable-pixmap", "image/rle", "image/x-sun-raster"};
    for (size_t i = 0; i < sizeof(types) / sizeof(types[0]); ++i) {
	struct icv_context *context = icv_context_create(NULL);
	unsigned char *data = NULL;
	size_t size = 0;
	ICV_CODEC_CHECK(context && !icv_write_mem_with_context(context, source, &data, &size, types[i], NULL, NULL));
	const struct bu_mime_probe_input input = {data, size, 1, size, 1};
	const int readable = icv_supports(context, types[i], ICV_CODEC_READ | ICV_CODEC_READ_MEM);
	const int metadata_supported = icv_supports(context, types[i], ICV_CODEC_METADATA);
	const char *members[] = {"content"};
	const struct bu_mime_trait fields[] = {BU_MIME_TRAIT_STRING("fixture.source-type", types[i]),
	    BU_MIME_TRAIT_SET("fixture.source-evidence", members, 1)};
	const struct bu_mime_traits requested = {fields, sizeof(fields) / sizeof(fields[0])};
	char filename[MAXPATHLEN];
	FILE *file = bu_temp_file(filename, sizeof(filename));
	ICV_CODEC_CHECK(file && fwrite(data, 1, size, file) == size && !fclose(file));
	const struct bu_mime_capability format = BU_MIME_CAPABILITY(types[i], NULL, 0, ICV_CODEC_METADATA);
	/* Keep metadata selection on the existing format implementation. */
	const unsigned int observation_priority = 0;
	const struct icv_codec codec = {"Source observations", &format, 1, observation_priority,
	    NULL, NULL, NULL, NULL, NULL, 0, codec_observation_metadata, codec_observe_source, NULL, NULL};
	const struct icv_plugin plugin = {ICV_PLUGIN_ABI_VERSION, sizeof(struct icv_plugin), NULL, 0, &codec, 1};
	for (int registered = 0; registered <= 1; ++registered) {
	    if (registered) ICV_CODEC_CHECK(icv_register_plugin(context, &plugin, NULL));
	    if (readable) {
		icv_image_t *image = icv_read_mem_variant(context, data, size, types[i], NULL, &requested, 0, 0, NULL);
		ICV_CODEC_CHECK((image != NULL) == registered);
		if (image) {
		    ICV_CODEC_CHECK(image->width == source->width && image->height == source->height);
		    icv_destroy(image);
		}
		image = icv_read_variant(context, filename, types[i], NULL, &requested, 0, 0, NULL);
		ICV_CODEC_CHECK((image != NULL) == registered);
		if (image) icv_destroy(image);
	    }
	    if (metadata_supported) {
		struct icv_image_metadata metadata;
		ICV_CODEC_CHECK((icv_metadata_variant(context, &input, types[i], NULL, &requested, &metadata, NULL) == ICV_METADATA_MATCH) == registered);
		ICV_CODEC_CHECK((icv_metadata_file_variant(context, filename, types[i], NULL, &requested, &metadata, NULL) == ICV_METADATA_MATCH) == registered);
		if (registered) ICV_CODEC_CHECK(metadata.width == source->width && metadata.height == source->height);
	    }
	}
	ICV_CODEC_CHECK(bu_file_delete(filename));
	bu_free(data, "observed source");
	icv_context_destroy(context);
    }
    return 0;
}

int
main(int argc, const char **argv)
{
    struct bu_mime_registry *registry;
    struct icv_context *context;
    struct bu_vls messages = BU_VLS_INIT_ZERO;
    struct bu_vls canonical = BU_VLS_INIT_ZERO;
    struct icv_codec_info *snapshot = NULL;
    size_t snapshot_count = 0;
    icv_image_t *image;
    unsigned char raw[] = {255, 0, 0};
    bu_setprogname(argv[0]);
    ICV_CODEC_CHECK(argc == 2);
    ICV_CODEC_CHECK(!codec_feature_test() && !codec_variant_test());
    if (BU_STR_EQUAL(argv[1], "--emit"))
	return codec_pipe_test(1);
    if (BU_STR_EQUAL(argv[1], "--emit-header"))
	return codec_pipe_test(2);
    if (BU_STR_EQUAL(argv[1], "--stdin"))
	return codec_pipe_test(0);
    if (BU_STR_EQUAL(argv[1], "--stdin-ppm") || BU_STR_EQUAL(argv[1], "--stdin-ppm-bad")) {
	int reject = BU_STR_EQUAL(argv[1], "--stdin-ppm-bad");
	const struct bu_mime_trait fact = BU_MIME_TRAIT_STRING("ppm.maxval", reject ? "255" : "65535");
	const struct bu_mime_traits request = {&fact, 1};
	context = icv_context_create(NULL);
	image = icv_read_variant(context, NULL, "image/x-portable-pixmap", NULL, &request, 0, 0, &messages);
	if (reject) {
	    ICV_CODEC_CHECK(!image);
	} else {
	    ICV_CODEC_CHECK(image && image->width == 1 && image->height == 1);
	    ICV_CODEC_CHECK(EQUAL(image->data[0], 1.0) && ZERO(image->data[1]) && ZERO(image->data[2]));
	    icv_destroy(image);
	}
	icv_context_destroy(context);
	bu_vls_free(&messages);
	return 0;
    }
    if (BU_STR_EQUAL(argv[1], "--emit-unsupported")) {
	unsigned char buffer[BU_MIME_DEFAULT_SCAN_LIMIT + 1] = {0};
	memcpy(buffer, "GIF89a", 6);
	buffer[BU_MIME_DEFAULT_SCAN_LIMIT] = 77;
	ICV_CODEC_CHECK(fwrite(buffer, 1, sizeof(buffer), stdout) == sizeof(buffer));
	return 0;
    }
    if (BU_STR_EQUAL(argv[1], "--reject-unsupported") || BU_STR_EQUAL(argv[1], "--stdin-limit")) {
	context = icv_context_create(NULL);
	if (BU_STR_EQUAL(argv[1], "--stdin-limit"))
	    icv_context_set_spool_limit(context, 8);
	image = icv_read_with_context(context, NULL, NULL, NULL, 0, 0, &messages);
	ICV_CODEC_CHECK(!image);
	if (BU_STR_EQUAL(argv[1], "--reject-unsupported")) {
	    ICV_CODEC_CHECK(strstr(bu_vls_cstr(&messages), "no available codec"));
	    ICV_CODEC_CHECK(fgetc(stdin) == 77);
	} else {
	    ICV_CODEC_CHECK(strstr(bu_vls_cstr(&messages), "limit"));
	}
	icv_context_destroy(context);
	bu_vls_free(&messages);
	return 0;
    }
    registry = bu_mime_registry_create(0);
    context = icv_context_create(registry);
    ICV_CODEC_CHECK(context);
    ICV_CODEC_CHECK(!codec_raw_profile_test(context));
    ICV_CODEC_CHECK(icv_supports(context, "image/apng", ICV_CODEC_READ | ICV_CODEC_METADATA));
    ICV_CODEC_CHECK(!icv_supports(context, "image/apng", ICV_CODEC_WRITE));
    ICV_CODEC_CHECK(!icv_load_plugin(context, "missing-image-plugin", &messages));
    ICV_CODEC_CHECK(icv_load_plugin(context, argv[1], &messages));
    ICV_CODEC_CHECK(!icv_load_plugin(context, argv[1], &messages));
    {
	const char *type = "image/vnd.example.header";
	const struct bu_mime_definition definition = {BU_MIME_ABI_VERSION, sizeof(struct bu_mime_definition),
	    type, NULL, NULL, 0, NULL, 0, NULL, 0};
	const struct bu_mime_capability capability = BU_MIME_CAPABILITY(type, NULL, 0, 0);
	const struct icv_codec codec = {"Header only", &capability, 1, BU_MIME_DEFAULT_PRIORITY,
	    NULL, NULL, NULL, NULL, codec_header_probe, 0, codec_header_metadata, NULL, NULL, NULL};
	const struct icv_plugin plugin = {ICV_PLUGIN_ABI_VERSION, sizeof(struct icv_plugin), &definition, 1, &codec, 1};
	const unsigned char header[] = "HDR";
	const struct bu_mime_probe_input input = {header, sizeof(header) - 1, 1, sizeof(header) - 1, 1};
	struct icv_image_metadata metadata;
	ICV_CODEC_CHECK(icv_register_plugin(context, &plugin, &messages));
	ICV_CODEC_CHECK(icv_supports(context, type, ICV_CODEC_METADATA));
	ICV_CODEC_CHECK(!icv_supports(context, type, ICV_CODEC_READ));
	ICV_CODEC_CHECK(icv_metadata(context, &input, NULL, NULL, &metadata, &messages) == ICV_METADATA_MATCH);
	ICV_CODEC_CHECK(metadata.width == 2 && metadata.height == 3 && metadata.channels == 1);
    }
    image = icv_read_mem_with_context(context, raw, sizeof(raw), "image/vnd.brlcad.pix", NULL, 1, 1, &messages);
    ICV_CODEC_CHECK(image);
    ICV_CODEC_CHECK(!icv_read_mem_with_context(context, raw, sizeof(raw), NULL, NULL, 1, 1, &messages));
    ICV_CODEC_CHECK(!codec_roundtrip(context, image, "image/png"));
    ICV_CODEC_CHECK(!codec_observation_test(image));
    ICV_CODEC_CHECK(!codec_animation_test(context, image));
    ICV_CODEC_CHECK(!codec_metadata_test(context, image) && !codec_sun_test(context, image));
    {
	char path[MAXPATHLEN];
	FILE *temporary = bu_temp_file(path, sizeof(path));
	struct bu_vls filename = BU_VLS_INIT_ZERO;
	struct bu_vls resolved = BU_VLS_INIT_ZERO;
	struct icv_image_metadata metadata;
	ICV_CODEC_CHECK(temporary && !fclose(temporary) && bu_file_delete(path));
	bu_vls_printf(&filename, "%s.pix", path);
	ICV_CODEC_CHECK(!icv_write_with_context(context, image, bu_vls_cstr(&filename), NULL, NULL, NULL));
	ICV_CODEC_CHECK(icv_resolve_variant(context, ICV_CODEC_METADATA, bu_vls_cstr(&filename),
	    NULL, NULL, NULL, &resolved, NULL, NULL, NULL));
	ICV_CODEC_CHECK(BU_STR_EQUAL(bu_vls_cstr(&resolved), "image/vnd.brlcad.pix"));
	ICV_CODEC_CHECK(icv_metadata_file(context, bu_vls_cstr(&filename), NULL, NULL, &metadata, NULL) == ICV_METADATA_MATCH);
	ICV_CODEC_CHECK(metadata.width == 1 && metadata.height == 1);
	ICV_CODEC_CHECK(bu_file_delete(bu_vls_cstr(&filename)));
	bu_vls_free(&filename);
	bu_vls_free(&resolved);
    }

    ICV_CODEC_CHECK(!codec_roundtrip(context, image, "image/x-portable-pixmap"));
    ICV_CODEC_CHECK(!codec_ppm_profile_test(context, image));
    ICV_CODEC_CHECK(!codec_ppm_input_test(context));
    ICV_CODEC_CHECK(!codec_roundtrip(context, image, "image/vnd.example.codec"));
    ICV_CODEC_CHECK(!codec_roundtrip(context, image, "image/x-example-codec2"));
    {
	struct icv_codec_info *info = NULL;
	size_t count = 0;
	size_t width = 0, height = 0;
	ICV_CODEC_CHECK(icv_list_codecs(context, "image/x-example-codec2", &info, &count));
	ICV_CODEC_CHECK(count == 1 && BU_STR_EQUAL(info[0].mime_type, "image/vnd.example.codec2"));
	ICV_CODEC_CHECK(info[0].operations == (ICV_CODEC_READ | ICV_CODEC_READ_MEM | ICV_CODEC_WRITE | ICV_CODEC_WRITE_MEM | ICV_CODEC_METADATA));
	ICV_CODEC_CHECK(icv_supports(context, "image/x-example-codec2", ICV_CODEC_READ | ICV_CODEC_WRITE_MEM));
	ICV_CODEC_CHECK(!icv_supports(context, "image/unsupported", ICV_CODEC_WRITE));
	ICV_CODEC_CHECK(!icv_supports(context, "image/png", 0));
	icv_codec_list_free(info, count);
	ICV_CODEC_CHECK(icv_image_size_with_context(context, NULL, 0, 12, "image/vnd.brlcad.pix", &width, &height));
	ICV_CODEC_CHECK(width == 2 && height == 2);
	ICV_CODEC_CHECK(!icv_image_size_with_context(context, NULL, 0, 13, "image/vnd.brlcad.pix", &width, &height));
	ICV_CODEC_CHECK(!icv_image_size_with_context(context, NULL, 0, 12, "image/png", &width, &height));
	ICV_CODEC_CHECK(icv_named_size("Letter", 72, &width, &height) && width == 612 && height == 792);
	ICV_CODEC_CHECK(!icv_raw_size(NULL, 0, 12, 0, &width, &height));
	ICV_CODEC_CHECK(!icv_named_size("A0", SIZE_MAX, &width, &height));
	ICV_CODEC_CHECK(!icv_raw_size(NULL, 0, SIZE_MAX, 1, &width, &height));
	ICV_CODEC_CHECK(!icv_raw_size(NULL, 0, 12, 3, NULL, &height));
    }
    {
	unsigned char *buffer = NULL;
	size_t size = 0;
	icv_image_t *decoded;
	char filename[MAXPATHLEN];
	FILE *fp = bu_temp_file(filename, sizeof(filename));
	ICV_CODEC_CHECK(fp && !fclose(fp));
	ICV_CODEC_CHECK(!icv_write_with_context(context, image, filename, "image/vnd.example.codec", NULL, &messages));
	decoded = icv_read_with_context(context, filename, NULL, NULL, 0, 0, &messages);
	ICV_CODEC_CHECK(decoded);
	icv_destroy(decoded);
	ICV_CODEC_CHECK(bu_file_delete(filename));
	ICV_CODEC_CHECK(!icv_write_mem_with_context(context, image, &buffer, &size, "image/png", NULL, &messages));
	{
	    const struct bu_mime_capability types[] = {BU_MIME_CAPABILITY("image/png", NULL, 0, 0)};
	    struct icv_codec codec = {"Other PNG codec", types, 1, BU_MIME_DEFAULT_PRIORITY,
		NULL, codec_reject, NULL, NULL, NULL, 0, NULL, NULL, NULL, NULL};
	    struct icv_plugin plugin = {ICV_PLUGIN_ABI_VERSION, sizeof(struct icv_plugin), NULL, 0, &codec, 1};
	    ICV_CODEC_CHECK(icv_register_plugin(context, &plugin, &messages));
	    ICV_CODEC_CHECK(!icv_read_mem_with_context(context, buffer, size, NULL, NULL, 0, 0, &messages));
	    decoded = icv_read_mem_with_context(context, buffer, size, NULL, "PNG", 0, 0, &messages);
	    ICV_CODEC_CHECK(decoded);
	    icv_destroy(decoded);
	}
	bu_free(buffer, "encoded image");
    }
    {
	const char *type = "image/vnd.example.raw";
	const char *extension = "mimeraw";
	struct bu_mime_definition definition = {BU_MIME_ABI_VERSION, sizeof(struct bu_mime_definition),
	    type, "Two-byte raw pixels", NULL, 0, &extension, 1, NULL, 0};
	const struct bu_mime_trait layout = BU_MIME_TRAIT_STRING("bit-depth", "16");
	const struct bu_mime_capability capability = {type, &layout, 1,
	    ICV_CODEC_READ_MEM | ICV_CODEC_METADATA, BU_MIME_DEFER_INPUT_VALIDATION};
	struct icv_codec codec = {"Raw fixture", &capability, 1, BU_MIME_DEFAULT_PRIORITY,
	    NULL, codec_reject, NULL, NULL, NULL, 2, NULL, codec_raw_inspect, NULL, NULL};
	struct icv_plugin plugin = {ICV_PLUGIN_ABI_VERSION, sizeof(struct icv_plugin), &definition, 1, &codec, 1};
	size_t width = 0, height = 0;
	plugin.abi_version = ICV_PLUGIN_ABI_VERSION - 1;
	ICV_CODEC_CHECK(!icv_register_plugin(context, &plugin, &messages));
	plugin.abi_version = ICV_PLUGIN_ABI_VERSION;
	ICV_CODEC_CHECK(icv_register_plugin(context, &plugin, &messages));
	ICV_CODEC_CHECK(icv_image_size_with_context(context, NULL, 0, 8, type, &width, &height));
	ICV_CODEC_CHECK(width == 2 && height == 2);
	{
	    const unsigned char bytes[] = {'A', 0, 0, 0};
	    const struct bu_mime_probe_input input = {bytes, sizeof(bytes), 1, sizeof(bytes), 1};
	    const struct bu_mime_trait property = BU_MIME_TRAIT_STRING("example.first-byte", "a");
	    const struct bu_mime_traits request = {&property, 1};
	    struct bu_mime_result *detected = NULL;
	    struct bu_mime_traits resolved = BU_MIME_TRAITS_INIT_ZERO;
	    ICV_CODEC_CHECK(icv_detect(context, "fixture.mimeraw", &input, &detected, NULL) == BU_MIME_MATCH);
	    ICV_CODEC_CHECK(icv_select_variant(context, ICV_CODEC_READ_MEM, type, NULL, detected,
		NULL, NULL, NULL, &resolved, NULL));
	    ICV_CODEC_CHECK(!bu_mime_traits_get(&resolved, "bit-depth"));
	    ICV_CODEC_CHECK(BU_STR_EQUAL(bu_mime_traits_get(&resolved, property.key), property.value));
	    ICV_CODEC_CHECK(icv_select_variant(context, ICV_CODEC_METADATA, type, NULL, detected,
		&request, NULL, NULL, NULL, NULL));
	    ICV_CODEC_CHECK(!icv_select_variant(context, ICV_CODEC_METADATA, type, NULL, NULL,
		&request, NULL, NULL, NULL, NULL));
	    bu_mime_traits_free(&resolved);
	    bu_mime_result_free(detected);
	}
    }
    ICV_CODEC_CHECK(icv_list_codecs(context, "image/x-example-codec2", &snapshot, &snapshot_count));
    icv_context_destroy(context);
    ICV_CODEC_CHECK(snapshot_count == 1 && BU_STR_EQUAL(snapshot[0].mime_type, "image/vnd.example.codec2"));
    icv_codec_list_free(snapshot, snapshot_count);
    ICV_CODEC_CHECK(!bu_mime_canonical(registry, "image/vnd.example.codec", &canonical));
    /* Images own their storage independently of the codec context. */
    icv_destroy(image);
    bu_mime_registry_destroy(registry);
    bu_vls_free(&canonical);
    bu_vls_free(&messages);
    return 0;
}
