/*                   J P E G _ P R O F I L E S . C P P
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

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#define JPEG_INTERNAL_OPTIONS
#include <jpeglib.h>

#include "bu/app.h"
#include "bu/file.h"
#include "bu/log.h"
#include "bu/malloc.h"
#include "bu/str.h"
#include "icv/codec.h"
#include "icv/colorspace.h"
#include "vmath.h"

#define JPEG_CHECK(condition) do { \
    if (!(condition)) { \
	bu_log("JPEG profile check failed at line %d: %s\n", __LINE__, #condition); \
	return 1; \
    } \
} while (0)

struct jpeg_case {
    J_COLOR_SPACE color;
    bool progressive;
    bool arithmetic;
    bool lossless;
    int depth;
    bool padded;
    jpeg_case(J_COLOR_SPACE c, bool p = false, bool a = false, bool l = false, int d = 8, bool pad = false)
	: color(c), progressive(p), arithmetic(a), lossless(l), depth(d), padded(pad) {}
};

static int
jpeg_components(J_COLOR_SPACE color)
{
    return color == JCS_GRAYSCALE ? 1 : color == JCS_CMYK || color == JCS_YCCK ? 4 : 3;
}

static std::vector<unsigned char>
jpeg_fixture(const jpeg_case &spec)
{
    jpeg_compress_struct encoder;
    jpeg_error_mgr errors;
    encoder.err = jpeg_std_error(&errors);
    jpeg_create_compress(&encoder);
    unsigned char *bytes = NULL;
    unsigned long size = 0;
    jpeg_mem_dest(&encoder, &bytes, &size);
    const size_t side = 2;
    encoder.image_width = encoder.image_height = side;
    encoder.input_components = jpeg_components(spec.color);
    encoder.in_color_space = spec.color == JCS_YCbCr && !spec.lossless ? JCS_RGB :
	spec.color == JCS_YCCK ? JCS_CMYK : spec.color;
    encoder.data_precision = spec.depth;
    jpeg_set_defaults(&encoder);
    jpeg_set_colorspace(&encoder, spec.color);
    if (spec.progressive) jpeg_simple_progression(&encoder);
    encoder.arith_code = spec.arithmetic;
#if defined(LIBJPEG_TURBO_VERSION_NUMBER) && LIBJPEG_TURBO_VERSION_NUMBER >= 3000000
    if (spec.lossless) jpeg_enable_lossless(&encoder, 1, 0);
#endif
    jpeg_start_compress(&encoder, TRUE);
    if (spec.padded) {
	/* Two legal comment segments put SOF beyond the default scan limit. */
	std::vector<unsigned char> comment(BU_MIME_DEFAULT_SCAN_LIMIT / 2 + 1, 'x');
	for (int i = 0; i < 2; ++i) jpeg_write_marker(&encoder, JPEG_COM, comment.data(), comment.size());
    }
    std::vector<JSAMPLE> row(side * encoder.input_components, 255);
    while (encoder.next_scanline < encoder.image_height) {
#if defined(LIBJPEG_TURBO_VERSION_NUMBER) && LIBJPEG_TURBO_VERSION_NUMBER >= 3000000
	if (spec.depth == 12) {
	    std::vector<J12SAMPLE> wide(side * encoder.input_components, 4095);
	    J12SAMPROW pointer = wide.data();
	    jpeg12_write_scanlines(&encoder, &pointer, 1);
	    continue;
	}
#endif
	JSAMPROW pointer = row.data();
	jpeg_write_scanlines(&encoder, &pointer, 1);
    }
    jpeg_finish_compress(&encoder);
    jpeg_destroy_compress(&encoder);
    std::vector<unsigned char> result(bytes, bytes + size);
    std::free(bytes);
    return result;
}

static int
jpeg_input_test(icv_context *context, const jpeg_case &spec)
{
    std::vector<unsigned char> bytes = jpeg_fixture(spec);
    const char *features[3];
    size_t feature_count = 0;
    if (spec.progressive) features[feature_count++] = "progressive";
    if (spec.arithmetic) features[feature_count++] = "arithmetic";
    if (spec.lossless) features[feature_count++] = "lossless";
    const char *color = spec.color == JCS_GRAYSCALE ? "gray" : spec.color == JCS_RGB ? "rgb" :
	spec.color == JCS_CMYK ? "cmyk" : spec.color == JCS_YCCK ? "ycck" : "ycbcr";
    const bu_mime_trait properties[] = {
	BU_MIME_TRAIT_STRING("bit-depth", spec.depth == 8 ? "8" : "12"),
	BU_MIME_TRAIT_STRING("color-space", color), BU_MIME_TRAIT_SET("jpeg.frame-features", features, feature_count)};
    const bu_mime_traits requested = {properties, sizeof(properties) / sizeof(properties[0])};
    bool supported = spec.depth == 8 && jpeg_components(spec.color) <= 3 && (!spec.lossless || spec.color == JCS_RGB);
#ifndef D_PROGRESSIVE_SUPPORTED
    if (spec.progressive) supported = false;
#endif
#ifndef D_ARITH_CODING_SUPPORTED
    if (spec.arithmetic) supported = false;
#endif
#ifndef D_LOSSLESS_SUPPORTED
    if (spec.lossless) supported = false;
#endif
    const bu_mime_probe_input input = {bytes.data(), bytes.size(), 1, bytes.size(), 1};
    icv_image_metadata metadata;
    char filename[MAXPATHLEN];
    FILE *stream = bu_temp_file(filename, sizeof(filename));
    JPEG_CHECK(stream && fwrite(bytes.data(), 1, bytes.size(), stream) == bytes.size() && !fclose(stream));
    icv_metadata_status status = icv_metadata_variant(context, &input, "image/jpeg", NULL, &requested, &metadata, NULL);
    JPEG_CHECK(status == (spec.padded ? ICV_METADATA_INCOMPLETE : ICV_METADATA_MATCH));
    if (!spec.padded) {
	JPEG_CHECK(metadata.width == 2 && metadata.height == 2);
	JPEG_CHECK(metadata.channels == static_cast<size_t>(jpeg_components(spec.color)));
	for (size_t prefix = 0; prefix < bytes.size(); ++prefix) {
	    bu_mime_probe_input truncated = {bytes.data(), prefix, 0, bytes.size(), 1};
	    status = icv_metadata_variant(context, &truncated, "image/jpeg", NULL, NULL, &metadata, NULL);
	    JPEG_CHECK(status == ICV_METADATA_MATCH || status == ICV_METADATA_INCOMPLETE);
	}
    }
    icv_image_t *image = icv_read_mem_variant(context, bytes.data(), bytes.size(), "image/jpeg", NULL, &requested, 0, 0, NULL);
    if (supported && !image)
	bu_log("Failed JPEG input: color=%s depth=%d progressive=%d arithmetic=%d lossless=%d padded=%d\n",
	    color, spec.depth, spec.progressive, spec.arithmetic, spec.lossless, spec.padded);
    JPEG_CHECK(supported ? image != NULL : image == NULL);
    if (image) {
	JPEG_CHECK(image->channels == 3 && image->color_space == ICV_COLOR_SPACE_RGB);
	for (size_t i = 0; i < image->width * image->height * image->channels; ++i)
	    JPEG_CHECK(NEAR_EQUAL(image->data[i], 1.0, 1.0 / 255.0));
	icv_destroy(image);
    }
    image = icv_read_variant(context, filename, "image/jpeg", NULL, &requested, 0, 0, NULL);
    JPEG_CHECK(supported ? image != NULL : image == NULL);
    if (image) icv_destroy(image);
    const bu_mime_trait wrong = BU_MIME_TRAIT_STRING("color-space", spec.color == JCS_GRAYSCALE ? "rgb" : "gray");
    const bu_mime_traits invalid = {&wrong, 1};
    JPEG_CHECK(!icv_read_mem_variant(context, bytes.data(), bytes.size(), "image/jpeg", NULL, &invalid, 0, 0, NULL));
    JPEG_CHECK(!icv_read_variant(context, filename, "image/jpeg", NULL, &invalid, 0, 0, NULL));
    JPEG_CHECK(bu_file_delete(filename));
    return 0;
}

static int
jpeg_output_test(icv_context *context)
{
    icv_image_t *image = icv_create(1, 1, ICV_COLOR_SPACE_RGB);
    JPEG_CHECK(image);
    const bu_mime_trait baseline = BU_MIME_TRAIT_SET("jpeg.frame-features", NULL, 0);
    const bu_mime_traits request = {&baseline, 1};
    unsigned char *bytes = NULL;
    size_t size = 0;
    JPEG_CHECK(!icv_write_mem_variant(context, image, &bytes, &size, "image/jpeg", NULL, &request, NULL));
    bu_mime_result *detected = NULL;
    const bu_mime_probe_input input = {bytes, size, 1, size, 1};
    JPEG_CHECK(icv_detect(context, NULL, &input, &detected, NULL) == BU_MIME_MATCH);
    const bu_mime_traits &facts = detected->candidates[0].traits;
    JPEG_CHECK(BU_STR_EQUAL(bu_mime_traits_get(&facts, "bit-depth"), "8"));
    JPEG_CHECK(BU_STR_EQUAL(bu_mime_traits_get(&facts, "color-space"), "ycbcr"));
    JPEG_CHECK(BU_STR_EQUAL(bu_mime_traits_get(&facts, "jpeg.sampling-factors"), "1x1,1x1,1x1"));
    const char *const *members = NULL;
    size_t count = 0;
    JPEG_CHECK(bu_mime_traits_get_set(&facts, "jpeg.frame-features", &members, &count) && !count);
    bu_mime_result_free(detected);
    unsigned char *expected = bytes;
    size_t expected_size = size;
    char filename[MAXPATHLEN];
    FILE *stream = bu_temp_file(filename, sizeof(filename));
    JPEG_CHECK(stream && !fclose(stream));
    JPEG_CHECK(!icv_write_variant(context, image, filename, "image/jpeg", NULL, &request, NULL));
    const char *progressive[] = {"progressive"};
    const bu_mime_trait unsupported[] = {
	BU_MIME_TRAIT_STRING("bit-depth", "12"), BU_MIME_TRAIT_STRING("channels", "1"),
	BU_MIME_TRAIT_STRING("color-space", "rgb"), BU_MIME_TRAIT_SET("jpeg.frame-features", progressive, 1),
	BU_MIME_TRAIT_STRING("jpeg.sampling-factors", "2x2,1x1,1x1")};
    for (const auto &trait : unsupported) {
	const bu_mime_traits invalid = {&trait, 1};
	bytes = NULL;
	size = 0;
	JPEG_CHECK(icv_write_mem_variant(context, image, &bytes, &size, "image/jpeg", NULL, &invalid, NULL));
	JPEG_CHECK(!bytes && !size);
	JPEG_CHECK(icv_write_variant(context, image, filename, "image/jpeg", NULL, &invalid, NULL));
    }
    std::vector<unsigned char> preserved(expected_size);
    stream = fopen(filename, "rb");
    JPEG_CHECK(stream && fread(preserved.data(), 1, preserved.size(), stream) == preserved.size());
    JPEG_CHECK(fgetc(stream) == EOF && !ferror(stream) && !fclose(stream));
    JPEG_CHECK(!memcmp(expected, preserved.data(), preserved.size()));
    bu_free(expected, "JPEG output");
    JPEG_CHECK(bu_file_delete(filename));
    icv_destroy(image);
    return 0;
}

int
main(int argc, const char **argv)
{
    bu_setprogname(argv[0]);
    icv_context *context = icv_context_create(NULL);
    JPEG_CHECK(context);
    if (argc == 2) {
	if (BU_STR_EQUAL(argv[1], "--emit")) {
	    std::vector<unsigned char> bytes = jpeg_fixture(jpeg_case(JCS_YCbCr, false, false, false, 8, true));
	    JPEG_CHECK(fwrite(bytes.data(), 1, bytes.size(), stdout) == bytes.size() && !fflush(stdout));
	} else {
	    bool reject = BU_STR_EQUAL(argv[1], "--stdin-bad");
	    JPEG_CHECK(reject || BU_STR_EQUAL(argv[1], "--stdin"));
	    const bu_mime_trait color = BU_MIME_TRAIT_STRING("color-space", reject ? "rgb" : "ycbcr");
	    const bu_mime_traits request = {&color, 1};
	    icv_image_t *image = icv_read_variant(context, NULL, "image/jpeg", NULL, &request, 0, 0, NULL);
	    JPEG_CHECK(reject ? image == NULL : image != NULL);
	    if (image) {
		JPEG_CHECK(image->width == 2 && image->height == 2 && image->channels == 3);
		icv_destroy(image);
	    }
	}
	icv_context_destroy(context);
	return 0;
    }
    JPEG_CHECK(argc == 1);
    std::vector<jpeg_case> cases = {jpeg_case(JCS_GRAYSCALE), jpeg_case(JCS_RGB), jpeg_case(JCS_YCbCr),
	jpeg_case(JCS_CMYK), jpeg_case(JCS_YCCK),
	jpeg_case(JCS_YCbCr, false, false, false, 8, true)};
#ifdef C_PROGRESSIVE_SUPPORTED
    cases.emplace_back(JCS_GRAYSCALE, true);
    cases.emplace_back(JCS_YCbCr, true);
#endif
#ifdef C_ARITH_CODING_SUPPORTED
    cases.emplace_back(JCS_GRAYSCALE, false, true);
#ifdef C_PROGRESSIVE_SUPPORTED
    cases.emplace_back(JCS_YCbCr, true, true);
#endif
#endif
#if defined(LIBJPEG_TURBO_VERSION_NUMBER) && LIBJPEG_TURBO_VERSION_NUMBER >= 3000000
#ifdef C_LOSSLESS_SUPPORTED
    cases.emplace_back(JCS_GRAYSCALE, false, false, true);
    cases.emplace_back(JCS_RGB, false, false, true);
    cases.emplace_back(JCS_YCbCr, false, false, true);
#endif
    cases.emplace_back(JCS_YCbCr, false, false, false, 12);
#endif
    int status = jpeg_output_test(context);
    for (const auto &spec : cases) status = status || jpeg_input_test(context, spec);
    icv_context_destroy(context);
    return status;
}
