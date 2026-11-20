/* F O R M A T _ P A R I T Y . C P P
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
#include <string>
#include <vector>
#ifdef HAVE_SYS_STAT_H
#  include <sys/stat.h>
#endif
#include "png.h"

#include "bu/app.h"
#include "bu/file.h"
#include "bu/log.h"
#include "bu/malloc.h"
#include "bu/str.h"
#include "vmath.h"
#include "icv/codec.h"
#include "icv/colorspace.h"

#define PARITY_CHECK(condition) do { \
    if (!(condition)) { \
	bu_log("Format parity failure at line %d: %s\n", __LINE__, #condition); \
	return 1; \
    } \
} while (0)

static int
same_image(const icv_image_t *first, const icv_image_t *second)
{
    PARITY_CHECK(first && second && first->width == second->width && first->height == second->height);
    PARITY_CHECK(first->channels == second->channels && first->color_space == second->color_space &&
	first->alpha_channel == second->alpha_channel);
    for (size_t i = 0; i < first->width * first->height * first->channels; ++i)
	PARITY_CHECK(EQUAL(first->data[i], second->data[i]));
    return 0;
}

static int
read_parity(icv_context *context, const unsigned char *bytes, size_t size,
    const char *type, size_t channels, double first_sample)
{
    char filename[MAXPATHLEN];
    FILE *stream = bu_temp_file(filename, sizeof(filename));
    PARITY_CHECK(stream && fwrite(bytes, 1, size, stream) == size && !fclose(stream));
    icv_image_t *memory = icv_read_mem_with_context(context, bytes, size, type, NULL, 0, 0, NULL);
    icv_image_t *file = icv_read_with_context(context, filename, type, NULL, 0, 0, NULL);
    PARITY_CHECK(memory && memory->channels == channels && EQUAL(memory->data[0], first_sample));
    PARITY_CHECK(!same_image(file, memory));
    icv_destroy(file);
    icv_destroy(memory);
    PARITY_CHECK(bu_file_delete(filename));
    return 0;
}

static int
ppm_parity(icv_context *context)
{
    const char *plain = "P3\n# dimensions\n2 2\n15\n0 1 2 3 4 5\n15 14 13 12 11 10\n";
    PARITY_CHECK(!read_parity(context, reinterpret_cast<const unsigned char *>(plain), strlen(plain),
	"image/x-portable-pixmap", 3, 1.0));
    const unsigned char raw[] = {'P','6','\n','1',' ','1','\n','1','5','\n',15,0,0};
    PARITY_CHECK(!read_parity(context, raw, sizeof(raw), "image/x-portable-pixmap", 3, 1.0));
    const unsigned char wide[] = {'P','6','\n','1',' ','1','\n','6','5','5','3','5','\n',255,255,128,0,0,0};
    PARITY_CHECK(!read_parity(context, wide, sizeof(wide), "image/x-portable-pixmap", 3, 1.0));
    const unsigned char whitespace_pixel[] = {'P','6','\n','1',' ','1','\n','2','5','5','\n','\n','#',' '};
    PARITY_CHECK(!read_parity(context, whitespace_pixel, sizeof(whitespace_pixel),
	"image/x-portable-pixmap", 3, 10.0 / 255.0));
    const char *invalid[] = {
	"P3\n1 1\n15\n16 0 0", "P3\n1 1\n15\n-1 0 0", "P6\n1 1\n65536\nabc",
	"P3\n184467440737095516160 1\n255\n0 0 0", "P3\n0 1\n255\n0 0 0", "P6\n1 1\n255Xabc"
    };
    for (const char *sample : invalid)
	PARITY_CHECK(!icv_read_mem_with_context(context, reinterpret_cast<const unsigned char *>(sample), strlen(sample),
	    "image/x-portable-pixmap", NULL, 0, 0, NULL));
    for (size_t size = 0; size < sizeof(wide); ++size)
	PARITY_CHECK(!icv_read_mem_with_context(context, wide, size, "image/x-portable-pixmap", NULL, 0, 0, NULL));
    return 0;
}

static int
png_fixture(icv_context *context, int color_type, int depth, int interlace)
{
    const png_uint_32 width = 3, height = 2;
    char filename[MAXPATHLEN];
    FILE *stream = bu_temp_file(filename, sizeof(filename));
    PARITY_CHECK(stream);
    png_structp writer = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    png_infop info = writer ? png_create_info_struct(writer) : NULL;
    PARITY_CHECK(writer && info);
    PARITY_CHECK(!setjmp(png_jmpbuf(writer)));
    png_init_io(writer, stream);
    png_set_IHDR(writer, info, width, height, depth, color_type, interlace,
	PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
    const png_color palette[] = {{255,0,0},{0,255,0},{0,0,255}};
    const png_byte transparency[] = {255,128,0};
    if (color_type == PNG_COLOR_TYPE_PALETTE) {
	const int palette_count = std::min(3, 1 << depth);
	png_set_PLTE(writer, info, palette, palette_count);
	png_set_tRNS(writer, info, transparency, palette_count, NULL);
    }
    png_write_info(writer, info);
    size_t encoded_channels = color_type == PNG_COLOR_TYPE_RGB ? 3 : color_type == PNG_COLOR_TYPE_RGB_ALPHA ? 4 :
	color_type == PNG_COLOR_TYPE_GRAY_ALPHA ? 2 : 1;
    size_t row_size = (width * encoded_channels * depth + 7) / 8;
    std::vector<unsigned char> pixels(row_size * height);
    for (size_t i = 0; i < pixels.size(); ++i)
	pixels[i] = color_type == PNG_COLOR_TYPE_PALETTE ? static_cast<unsigned char>(i % 3) :
	    static_cast<unsigned char>((i * 29 + 17) % 256);
    png_bytep rows[] = {pixels.data(), pixels.data() + row_size};
    png_write_image(writer, rows);
    png_write_end(writer, info);
    png_destroy_write_struct(&writer, &info);
    long length = ftell(stream);
    PARITY_CHECK(length > 0 && !fseek(stream, 0, SEEK_SET));
    std::vector<unsigned char> bytes(static_cast<size_t>(length));
    PARITY_CHECK(fread(bytes.data(), 1, bytes.size(), stream) == bytes.size() && !fclose(stream));
    size_t decoded_channels = color_type == PNG_COLOR_TYPE_PALETTE ? 4 : encoded_channels;
    double first = color_type == PNG_COLOR_TYPE_PALETTE ? 1.0 : static_cast<double>(pixels[row_size]) / 255.0;
    if (color_type == PNG_COLOR_TYPE_GRAY && depth < 8)
	first = static_cast<double>(pixels[row_size] >> (8 - depth)) / ((1u << depth) - 1);
    PARITY_CHECK(!read_parity(context, bytes.data(), bytes.size(), "image/png", decoded_channels, first));
    const std::string depth_value = std::to_string(depth), color_value = std::to_string(color_type), interlace_value = std::to_string(interlace);
    const bu_mime_trait properties[] = {
	BU_MIME_TRAIT_STRING("bit-depth", depth_value.c_str()), BU_MIME_TRAIT_STRING("png.color-type", color_value.c_str()),
	BU_MIME_TRAIT_STRING("png.interlace-method", interlace_value.c_str())
    };
    const bu_mime_traits profile = {properties, sizeof(properties) / sizeof(properties[0])};
    icv_image_t *requested = icv_read_mem_variant(context, bytes.data(), bytes.size(), "image/png", NULL, &profile, 0, 0, NULL);
    PARITY_CHECK(requested && requested->channels == decoded_channels && EQUAL(requested->data[0], first));
    icv_destroy(requested);
    PARITY_CHECK(!icv_read_mem_with_context(context, bytes.data(), bytes.size() - 1, "image/png", NULL, 0, 0, NULL));
    PARITY_CHECK(bu_file_delete(filename));
    return 0;
}

static int
write_parity(icv_context *context)
{
    icv_image_t *image = icv_create_with_channels(3, 2, ICV_COLOR_SPACE_RGB, 4);
    PARITY_CHECK(image && image->alpha_channel);
    for (size_t i = 0; i < image->width * image->height * image->channels; ++i)
	image->data[i] = static_cast<double>((i * 37) % 256) / 255.0;
    const char *types[] = {"image/png", "image/x-portable-pixmap"};
    for (const char *type : types) {
	char filename[MAXPATHLEN];
	FILE *stream = bu_temp_file(filename, sizeof(filename));
	PARITY_CHECK(stream && !fclose(stream));
	unsigned char *bytes = NULL;
	size_t size = 0;
	PARITY_CHECK(!icv_write_mem_with_context(context, image, &bytes, &size, type, NULL, NULL));
	PARITY_CHECK(!icv_write_with_context(context, image, filename, type, NULL, NULL));
	icv_image_t *memory = icv_read_mem_with_context(context, bytes, size, type, NULL, 0, 0, NULL);
	icv_image_t *file = icv_read_with_context(context, filename, type, NULL, 0, 0, NULL);
	PARITY_CHECK(!same_image(memory, file));
	if (BU_STR_EQUAL(type, "image/png"))
	    PARITY_CHECK(!same_image(image, file));
	icv_destroy(memory); icv_destroy(file);
	bu_free(bytes, "encoded image");
	PARITY_CHECK(bu_file_delete(filename));
    }
    PARITY_CHECK(!icv_create_with_channels(1, 1, ICV_COLOR_SPACE_GRAY, 3));
    PARITY_CHECK(!icv_create_with_channels(1, 1, ICV_COLOR_SPACE_RGB, 2));
    icv_destroy(image);
    return 0;
}

static int
write_failures(icv_context *context)
{
#ifdef HAVE_SYS_STAT_H
    struct stat device;
    if (stat("/dev/full", &device) || !S_ISCHR(device.st_mode))
	return 0;
    icv_image_t *image = icv_create(2, 2, ICV_COLOR_SPACE_RGB);
    PARITY_CHECK(image);
    const char *types[] = {"image/png", "image/jpeg", "image/x-portable-pixmap", "image/rle",
	"image/vnd.brlcad.pix", "image/vnd.brlcad.bw", "image/vnd.brlcad.dpix"};
    for (const char *type : types)
	PARITY_CHECK(icv_write_with_context(context, image, "/dev/full", type, NULL, NULL) == BRLCAD_ERROR);
    icv_destroy(image);
#else
    (void)context;
#endif
    return 0;
}

static int
flatten_layouts(void)
{
    const double white[] = {1.0, 1.0, 1.0};
    for (size_t channels = 1; channels <= 4; ++channels) {
	bool gray = channels <= 2;
	icv_image_t *source = icv_create_with_channels(1, 1,
	    gray ? ICV_COLOR_SPACE_GRAY : ICV_COLOR_SPACE_RGB, channels);
	PARITY_CHECK(source);
	for (size_t channel = 0; channel < channels; ++channel)
	    source->data[channel] = 0.25;
	icv_image_t *black_result = icv_flatten(source, NULL);
	icv_image_t *white_result = icv_flatten(source, white);
	PARITY_CHECK(black_result && white_result && black_result->channels == 3 && !black_result->alpha_channel);
	double alpha = source->alpha_channel ? 0.25 : 1.0;
	for (size_t channel = 0; channel < 3; ++channel) {
	    PARITY_CHECK(EQUAL(black_result->data[channel], alpha * 0.25));
	    PARITY_CHECK(EQUAL(white_result->data[channel], alpha * 0.25 + 1.0 - alpha));
	}
	PARITY_CHECK(source->channels == channels && EQUAL(source->data[0], 0.25));
	if (source->alpha_channel) {
	    source->data[channels - 1] = 2.0;
	    PARITY_CHECK(!icv_flatten(source, NULL));
	}
	icv_destroy(black_result);
	icv_destroy(white_result);
	icv_destroy(source);
    }
    const double invalid[] = {-0.1, 0.0, 0.0};
    icv_image_t *image = icv_create(1, 1, ICV_COLOR_SPACE_RGB);
    PARITY_CHECK(image && !icv_flatten(image, invalid) && !icv_flatten(NULL, NULL));
    icv_destroy(image);
    return 0;
}

int
main(int, char **argv)
{
    bu_setprogname(argv[0]);
    icv_context *context = icv_context_create(NULL);
    PARITY_CHECK(context);
    int status = ppm_parity(context) || write_parity(context) || write_failures(context) || flatten_layouts();
    const int colors[] = {PNG_COLOR_TYPE_GRAY, PNG_COLOR_TYPE_GRAY_ALPHA, PNG_COLOR_TYPE_RGB,
	PNG_COLOR_TYPE_RGB_ALPHA, PNG_COLOR_TYPE_PALETTE};

    for (int color : colors)
	for (int depth : {1,2,4,8,16})
	    if ((color == PNG_COLOR_TYPE_GRAY || color == PNG_COLOR_TYPE_PALETTE || depth >= 8) &&
		(color != PNG_COLOR_TYPE_PALETTE || depth <= 8))
		for (int interlace : {PNG_INTERLACE_NONE, PNG_INTERLACE_ADAM7})
		    status = status || png_fixture(context, color, depth, interlace);
    icv_context_destroy(context);
    return status;
}
