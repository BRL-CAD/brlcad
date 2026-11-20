/* R L E _ P R O F I L E S . C P P
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
#include <cstring>
#include <stdexcept>
#include <vector>
#include "bio.h"
#include "bu/app.h"
#include "bu/file.h"
#include "bu/log.h"
#include "bu/malloc.h"
#include "bu/str.h"
#include "icv/codec.h"
#include "vmath.h"

namespace {
using bytes = std::vector<unsigned char>;
constexpr size_t fixed_header_size = 15;
constexpr size_t color_channels_offset = 11;
constexpr size_t pixel_bits_offset = 12;

void require(bool condition, const char *message)
{
    if (!condition) throw std::runtime_error(message);
}

void word(bytes &data, unsigned value, bool big)
{
    data.push_back(static_cast<unsigned char>(big ? value >> 8 : value));
    data.push_back(static_cast<unsigned char>(big ? value : value >> 8));
}

/* Hand-written one-pixel Utah headers and SetColor/Run/EOF records keep
 * profile and pixel checks independent of the production encoder. */
bytes fixture(bool big, bool alpha, bool mapped)
{
    bytes data;
    word(data, 0xcc52, big);
    word(data, 0, big); word(data, 0, big);
    word(data, 1, big); word(data, 1, big);
    data.insert(data.end(), {static_cast<unsigned char>(alpha ? 6 : 2), 3, 8,
	static_cast<unsigned char>(mapped ? 3 : 0), 0, 0});
    const unsigned colors[] = {255, 128, 0};
    if (mapped)
	for (unsigned color : colors) word(data, color << 8, big);
    for (unsigned channel = 0; channel < 3 + unsigned(alpha); ++channel) {
	data.push_back(2); data.push_back(channel == 3 ? 255 : channel);
	data.push_back(6); data.push_back(0);
	word(data, channel == 3 ? 64 : (mapped ? 0 : colors[channel]), big);
    }
    data.insert(data.end(), {7, 0});
    return data;
}

void check_image(icv_image_t *image, bool alpha)
{
    require(image && image->width == 1 && image->height == 1 &&
	image->channels == (alpha ? 4u : 3u), "RLE decode/layout failed");
    require(EQUAL(image->data[0], 1.0) && EQUAL(image->data[1], 128.0 / 255.0) &&
	ZERO(image->data[2]), "RLE colors changed");
    if (alpha) require(EQUAL(image->data[3], 64.0 / 255.0), "RLE alpha changed");
    icv_destroy(image);
}

void read_profiles(icv_context *context, bool big, bool alpha, bool mapped)
{
    bytes data = fixture(big, alpha, mapped);
    bu_mime_trait fields[] = {
	BU_MIME_TRAIT_STRING("rle.variant", "utah"),
	BU_MIME_TRAIT_STRING("encoding", "binary"),
	BU_MIME_TRAIT_STRING("byte-order", big ? "big-endian" : "little-endian"),
	BU_MIME_TRAIT_STRING("bit-depth", "8"),
	BU_MIME_TRAIT_STRING("rle.color-channels", "3"),
	BU_MIME_TRAIT_STRING("rle.colormap-channels", mapped ? "3" : "0"),
	BU_MIME_TRAIT_STRING("rle.colormap-length-bits", "0"),
	BU_MIME_TRAIT_STRING("rle.alpha", alpha ? "true" : "false")};
    const bu_mime_traits requested = {fields, sizeof(fields) / sizeof(fields[0])};
    check_image(icv_read_mem_variant(context, data.data(), data.size(), "image/rle",
	NULL, &requested, 0, 0, NULL), alpha);
    char filename[MAXPATHLEN];
    FILE *stream = bu_temp_file(filename, sizeof(filename));
    require(stream && fwrite(data.data(), 1, data.size(), stream) == data.size() && !fclose(stream),
	"cannot write RLE fixture");
    check_image(icv_read_variant(context, filename, "image/rle", NULL, &requested, 0, 0, NULL), alpha);
    check_image(icv_read_with_context(context, filename, NULL, NULL, 0, 0, NULL), alpha);
    for (const char *key : {"byte-order", "bit-depth", "rle.variant", "rle.alpha", "version"}) {
	bu_mime_trait bad = BU_MIME_TRAIT_STRING(key, "unsupported");
	bu_mime_traits wrong = {&bad, 1};
	require(!icv_read_variant(context, filename, "image/rle", NULL, &wrong, 0, 0, NULL),
	    "RLE file accepted an unestablished assertion");
	require(!icv_read_mem_variant(context, data.data(), data.size(), "image/rle", NULL, &wrong, 0, 0, NULL),
	    "RLE memory accepted an unestablished assertion");
    }
    require(bu_file_delete(filename), "cannot delete RLE fixture");
}

void observations(icv_context *context)
{
    for (bool big : {false, true}) {
	bytes data = fixture(big, false, false);
	data[pixel_bits_offset] = 16;
	const bu_mime_probe_input input = {data.data(), data.size(), 1, data.size(), 1};
	bu_mime_result *result = NULL;
	require(icv_detect(context, NULL, &input, &result, NULL) == BU_MIME_MATCH && result && result->count,
	    "Utah RLE signature not recognized");
	require(BU_STR_EQUAL(result->candidates[0].name, "image/rle") &&
	    BU_STR_EQUAL(bu_mime_traits_get(&result->candidates[0].traits, "bit-depth"), "16"),
	    "RLE observation was limited to decoder support");
	bu_mime_result_free(result);
	require(!icv_read_mem_with_context(context, data.data(), data.size(), "image/rle", NULL, 0, 0, NULL),
	    "unsupported RLE sample size decoded");
	data[pixel_bits_offset] = 8;
	data[color_channels_offset] = 1;
	require(!icv_read_mem_with_context(context, data.data(), data.size(), "image/rle", NULL, 0, 0, NULL),
	    "unsupported RLE color-channel layout decoded");
	for (size_t size = 0; size < fixed_header_size; ++size)
	    require(!icv_read_mem_with_context(context, data.data(), size, "image/rle", NULL, 0, 0, NULL),
		"truncated RLE header decoded");
    }
}

void write_profiles(icv_context *context)
{
    icv_image_t *image = icv_create(1, 1, ICV_COLOR_SPACE_RGB);
    require(image != NULL, "cannot create RLE output image");
    image->data[0] = 1.0; image->data[1] = 128.0 / 255.0;
    bu_mime_trait fields[] = {BU_MIME_TRAIT_STRING("encoding", "binary"),
	BU_MIME_TRAIT_STRING("rle.variant", "utah"), BU_MIME_TRAIT_STRING("bit-depth", "8"),
	BU_MIME_TRAIT_STRING("byte-order", "little-endian"), BU_MIME_TRAIT_STRING("rle.color-channels", "3"),
	BU_MIME_TRAIT_STRING("rle.colormap-channels", "0"), BU_MIME_TRAIT_STRING("rle.colormap-length-bits", "0")};
    bu_mime_traits requested = {fields, sizeof(fields) / sizeof(fields[0])};
    unsigned char *data = NULL;
    size_t size = 0;
    require(!icv_write_mem_variant(context, image, &data, &size, "image/rle", NULL, &requested, NULL),
	"supported RLE output profile rejected");
    check_image(icv_read_mem_variant(context, data, size, "image/rle", NULL, &requested, 0, 0, NULL), false);
    bu_free(data, "RLE output");
    char filename[MAXPATHLEN];
    FILE *stream = bu_temp_file(filename, sizeof(filename));
    const char marker[] = "preserve";
    require(stream && fwrite(marker, 1, sizeof(marker), stream) == sizeof(marker) && !fclose(stream),
	"cannot write destination marker");
    bu_mime_trait unsupported[] = {BU_MIME_TRAIT_STRING("encoding", "ascii"),
	BU_MIME_TRAIT_STRING("byte-order", "big-endian"), BU_MIME_TRAIT_STRING("bit-depth", "16"),
	BU_MIME_TRAIT_STRING("rle.color-channels", "1"), BU_MIME_TRAIT_STRING("rle.colormap-channels", "3"),
	BU_MIME_TRAIT_STRING("rle.variant", "other"), BU_MIME_TRAIT_STRING("version", "1")};
    for (auto &field : unsupported) {
	bu_mime_traits wrong = {&field, 1};
	require(icv_write_variant(context, image, filename, "image/rle", NULL, &wrong, NULL) == BRLCAD_ERROR,
	    "unsupported RLE output profile accepted");
	require(icv_write_mem_variant(context, image, &data, &size, "image/rle", NULL, &wrong, NULL) == BRLCAD_ERROR && !data && !size,
	    "unsupported RLE memory output produced bytes");
    }
    stream = fopen(filename, "rb");
    char actual[sizeof(marker)];
    require(stream && fread(actual, 1, sizeof(actual), stream) == sizeof(actual) && !fclose(stream) &&
	!memcmp(actual, marker, sizeof(marker)), "rejected RLE write changed destination");
    require(bu_file_delete(filename), "cannot delete destination marker");
    icv_destroy(image);
}
} // namespace

int main(int argc, char **argv)
{
    bu_setprogname(argv[0]);
    try {
	if (argc > 1 && BU_STR_EQUAL(argv[1], "--emit")) {
	    setmode(fileno(stdout), O_BINARY);
	    bytes data = fixture(false, false, false);
	    return fwrite(data.data(), 1, data.size(), stdout) == data.size() ? 0 : 1;
	}
	icv_context *context = icv_context_create(NULL);
	require(context != NULL, "cannot create RLE context");
	if (argc > 1) {
	    setmode(fileno(stdin), O_BINARY);
	    bu_mime_trait field = BU_MIME_TRAIT_STRING("bit-depth", BU_STR_EQUAL(argv[1], "--stdin-bad") ? "16" : "8");
	    bu_mime_traits requested = {&field, 1};
	    icv_image_t *image = icv_read_variant(context, NULL, "image/rle", NULL, &requested, 0, 0, NULL);
	    if (BU_STR_EQUAL(argv[1], "--stdin-bad")) require(!image, "RLE pipe accepted wrong sample size");
	    else check_image(image, false);
	} else {
	    for (bool big : {false, true})
		for (bool alpha : {false, true})
		    for (bool mapped : {false, true}) read_profiles(context, big, alpha, mapped);
	    observations(context);
	    write_profiles(context);
	    bu_mime_registry *registry = bu_mime_registry_create(2);
	    icv_context *short_context = icv_context_create(registry);
	    require(short_context != NULL, "cannot create bounded RLE context");
	    read_profiles(short_context, false, false, false);
	    read_profiles(short_context, true, true, true);
	    icv_context_destroy(short_context);
	    bu_mime_registry_destroy(registry);
	}
	icv_context_destroy(context);
    } catch (const std::exception &error) {
	bu_log("RLE profile test: %s\n", error.what());
	return 1;
    }
    return 0;
}
