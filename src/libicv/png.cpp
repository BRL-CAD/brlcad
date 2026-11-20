/*                      P N G . C P P
 * BRL-CAD
 *
 * Copyright (c) 2007-2026 United States Government as represented by
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

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <new>
#include <sstream>
#include <string>
#include <vector>
#include "png.h"

#include "bio.h"

#include "bu/str.h"
#include "bu/file.h"
#include "bu/log.h"
#include "bu/malloc.h"
#include "bu/vls.h"
#include "vmath.h"
#include "icv_private.h"

#include "../libbu/json.hpp"
#include "../libbu/mime_png.h"

/*
 * PNG tEXt chunk key used for the BRL-CAD scene/camera JSON record.
 *
 * Schema (ICV_PNG_SCENE_VERSION 1):
 * {
 *   "version":  <int>          // schema version; increment on incompatible changes
 *   "db":       "<string>"     // path to .g geometry database (optional)
 *   "objects":  ["<s>", ...]   // top-level objects to render (optional)
 *   "camera": {
 *     "viewrotscale": [m0..m15] // Viewrotscale 4x4 matrix, row-major
 *     "eye_model":    [x, y, z] // eye position in model space (mm)
 *     "viewsize":     <double>  // view width in model units (mm)
 *     "aspect":       <double>  // pixel aspect ratio (width/height)
 *     "perspective":  <double>  // half-angle in degrees; 0 = orthographic
 *   }
 * }
 *
 * All double values use the full precision of
 * std::numeric_limits<double>::max_digits10 (17 significant digits),
 * which guarantees exact round-trip through text serialisation.
 */
#define ICV_PNG_KEY_SCENE    "BRL-CAD-scene"
#define ICV_PNG_SCENE_VERSION 1


/**
 * Serialise an icv_render_info into the JSON schema above.
 * Returns an std::string containing the JSON text.
 */
static std::string
render_info_to_json(const struct icv_render_info *info)
{
    using json = nlohmann::json;

    json j;
    j["version"] = ICV_PNG_SCENE_VERSION;

    if (info->db_filename)
	j["db"] = info->db_filename;

    if (info->objects) {
	/* Split the space-separated object list into a proper JSON array */
	json obj_array = json::array();
	const char *p = info->objects;
	while (*p) {
	    while (*p == ' ' || *p == '\t') p++;
	    if (!*p) break;
	    const char *start = p;
	    while (*p && *p != ' ' && *p != '\t') p++;
	    if (p > start)
		obj_array.push_back(std::string(start, (size_t)(p - start)));
	}
	if (!obj_array.empty())
	    j["objects"] = obj_array;
    }

    json cam;
    {
	json vrs = json::array();
	for (int i = 0; i < 16; i++)
	    vrs.push_back(info->viewrotscale[i]);
	cam["viewrotscale"] = vrs;
    }
    {
	json em = json::array();
	for (int i = 0; i < 3; i++)
	    em.push_back(info->eye_model[i]);
	cam["eye_model"] = em;
    }
    cam["viewsize"]    = info->viewsize;
    cam["aspect"]      = info->aspect;
    cam["perspective"] = info->perspective;
    j["camera"] = cam;

    return j.dump();
}


/**
 * Parse the BRL-CAD scene JSON text into an icv_render_info.
 * Returns 1 on success, 0 on any parse or validation failure.
 */
static int
json_to_render_info(const char *text, struct icv_render_info *info)
{
    using json = nlohmann::json;

    try {
	json j = json::parse(text);

	/* Version check – warn on unknown future versions but still attempt parse */
	if (j.contains("version") && j["version"].is_number_integer()) {
	    int ver = j["version"].get<int>();
	    if (ver > ICV_PNG_SCENE_VERSION)
		bu_log("png_read: WARNING: scene metadata version %d (expected <= %d),"
		       " attempting to read anyway\n", ver, ICV_PNG_SCENE_VERSION);
	}

	if (j.contains("db") && j["db"].is_string()) {
	    if (info->db_filename) bu_free(info->db_filename, "ri db_filename");
	    info->db_filename = bu_strdup(j["db"].get<std::string>().c_str());
	}

	if (j.contains("objects") && j["objects"].is_array()) {
	    /* Rebuild the space-separated string expected by the API */
	    std::string objs;
	    for (const auto &o : j["objects"]) {
		if (o.is_string()) {
		    if (!objs.empty()) objs += ' ';
		    objs += o.get<std::string>();
		}
	    }
	    if (!objs.empty()) {
		if (info->objects) bu_free(info->objects, "ri objects");
		info->objects = bu_strdup(objs.c_str());
	    }
	}

	if (j.contains("camera") && j["camera"].is_object()) {
	    const json &cam = j["camera"];

	    if (cam.contains("viewrotscale") && cam["viewrotscale"].is_array()
		&& cam["viewrotscale"].size() == 16) {
		for (int i = 0; i < 16; i++)
		    info->viewrotscale[i] = cam["viewrotscale"][i].get<double>();
	    }

	    if (cam.contains("eye_model") && cam["eye_model"].is_array()
		&& cam["eye_model"].size() == 3) {
		for (int i = 0; i < 3; i++)
		    info->eye_model[i] = cam["eye_model"][i].get<double>();
	    }

	    if (cam.contains("viewsize") && cam["viewsize"].is_number())
		info->viewsize = cam["viewsize"].get<double>();

	    if (cam.contains("aspect") && cam["aspect"].is_number())
		info->aspect = cam["aspect"].get<double>();

	    if (cam.contains("perspective") && cam["perspective"].is_number())
		info->perspective = cam["perspective"].get<double>();
	}

    } catch (const json::exception &e) {
	bu_log("png_read: WARNING: failed to parse BRL-CAD scene JSON: %s\n", e.what());
	return 0;
    }

    return 1;
}


extern "C" int
png_write(icv_image_t *bif, FILE *fp)
{
    if (UNLIKELY(!bif))
	return BRLCAD_ERROR;
    if (UNLIKELY(!fp))
	return BRLCAD_ERROR;

    if (bif->channels > 4 || bif->channels == 0) {
	bu_log("png_write : Invalid number of channels (%d)\n", (int)bif->channels);
	return BRLCAD_ERROR;
    }

    /* An error jump must not invalidate modified C++ string state. */
    const std::string scene_json = bif->render_info ? render_info_to_json(bif->render_info) : std::string();

    unsigned char *data = icv_data2uchar(bif);

    png_structp png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (UNLIKELY(png_ptr == NULL)) {
	bu_free(data, "png write uchar data");
	return BRLCAD_ERROR;
    }

    png_infop info_ptr = png_create_info_struct(png_ptr);
    if (info_ptr == NULL || setjmp(png_jmpbuf(png_ptr))) {
	png_destroy_write_struct(&png_ptr, info_ptr ? &info_ptr : NULL);
	bu_log("ERROR: Unable to create png header\n");
	bu_free(data, "png write uchar data");
	return BRLCAD_ERROR;
    }
    int png_color_type;
    switch (bif->color_space) {
	case ICV_COLOR_SPACE_GRAY:
	    png_color_type = (bif->channels == 2) ? PNG_COLOR_TYPE_GRAY_ALPHA : PNG_COLOR_TYPE_GRAY;
	    break;
	default:
	    png_color_type = (bif->channels == 4) ? PNG_COLOR_TYPE_RGBA : PNG_COLOR_TYPE_RGB;
    }

    png_init_io(png_ptr, fp);
    png_set_IHDR(png_ptr, info_ptr, (unsigned)bif->width, (unsigned)bif->height, 8, png_color_type,
		 PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT,
		 PNG_FILTER_TYPE_DEFAULT);

    /* Embed render metadata as a single JSON tEXt chunk when available */
    if (bif->render_info) {
	png_text text_chunk;
	memset(&text_chunk, 0, sizeof(text_chunk));
	text_chunk.compression = PNG_TEXT_COMPRESSION_NONE;
	text_chunk.key  = (png_charp)ICV_PNG_KEY_SCENE;
	text_chunk.text = (png_charp)scene_json.c_str();
	/* For PNG_TEXT_COMPRESSION_NONE, text_length must be zero;
	 * libpng computes the length from the NUL-terminated text. */
	text_chunk.text_length = 0;

	/* scene_json must remain valid until after png_write_info() because
		 * libpng may store a pointer to the text rather than copying it. */
	png_set_text(png_ptr, info_ptr, &text_chunk, 1);
    }

    png_write_info(png_ptr, info_ptr);

    if (bif->height > 0) {
	for (size_t i = bif->height-1; i > 0; --i) {
	    png_write_row(png_ptr, (png_bytep)(data + bif->width*bif->channels*i));
	}
	png_write_row(png_ptr, (png_bytep)(data + 0));
    }
    png_write_end(png_ptr, info_ptr);

    png_destroy_write_struct(&png_ptr, &info_ptr);
    bu_free(data, "png write uchar data");

    return BRLCAD_OK;
}

extern "C" {

    struct icv_png_mem_ctx {
	unsigned char *buffer;
	size_t size;
	size_t capacity;
    };

    /* Custom write callback for libpng */
    static void
    icv_png_mem_write(png_structp png_ptr, png_bytep data, png_size_t length)
    {
	struct icv_png_mem_ctx *ctx = (struct icv_png_mem_ctx *)png_get_io_ptr(png_ptr);

	/* Expand buffer if needed */
	if (ctx->size + length > ctx->capacity) {
	    ctx->capacity = (ctx->capacity == 0) ? 8192 : ctx->capacity * 2;
	    if (ctx->capacity < ctx->size + length) {
		ctx->capacity = ctx->size + length;
	    }
	    ctx->buffer = (unsigned char *)bu_realloc(ctx->buffer, ctx->capacity, "png_mem buffer");
	}

	memcpy(ctx->buffer + ctx->size, data, length);
	ctx->size += length;
    }

    /* Custom flush callback (no-op for memory) */
    static void
    icv_png_mem_flush(png_structp /*png_ptr*/)
    {
	/* nothing to do */
    }

} // extern "C"

extern "C" int
png_write_mem(icv_image_t *bif, unsigned char **outbuffer, size_t *outsize)
{
    png_structp png_ptr;
    png_infop info_ptr;
    unsigned char *data;
    size_t row;
    int color_type;
    struct icv_png_mem_ctx ctx;

    if (UNLIKELY(!bif)) return BRLCAD_ERROR;
    if (UNLIKELY(!outbuffer || !outsize)) return BRLCAD_ERROR;

    *outbuffer = NULL;
    *outsize = 0;

    /* Initialize dynamic memory context */
    ctx.capacity = 8192;
    ctx.size = 0;
    ctx.buffer = (unsigned char *)bu_malloc(ctx.capacity, "png_mem buffer");

    png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png_ptr) {
	bu_log("png_write_mem: Cannot create png_struct\n");
	bu_free(ctx.buffer, "png_mem buffer");
	return BRLCAD_ERROR;
    }

    info_ptr = png_create_info_struct(png_ptr);
    if (!info_ptr) {
	bu_log("png_write_mem: Cannot create png_info\n");
	png_destroy_write_struct(&png_ptr, NULL);
	bu_free(ctx.buffer, "png_mem buffer");
	return BRLCAD_ERROR;
    }

    if (setjmp(png_jmpbuf(png_ptr))) {
	bu_log("png_write_mem: Error writing PNG to memory\n");
	png_destroy_write_struct(&png_ptr, &info_ptr);
	if (ctx.buffer) bu_free(ctx.buffer, "png_mem buffer");
	return BRLCAD_ERROR;
    }

    /* Bind libpng output to our custom memory callbacks */
    png_set_write_fn(png_ptr, &ctx, icv_png_mem_write, icv_png_mem_flush);

    switch (bif->channels) {
	case 1:
	    color_type = PNG_COLOR_TYPE_GRAY;
	    break;
	case 2:
	    color_type = PNG_COLOR_TYPE_GRAY_ALPHA;
	    break;
	case 3:
	    color_type = PNG_COLOR_TYPE_RGB;
	    break;
	case 4:
	    color_type = PNG_COLOR_TYPE_RGB_ALPHA;
	    break;
	default:
	    bu_log("png_write_mem: Unsupported number of channels (%zu)\n", bif->channels);
	    png_destroy_write_struct(&png_ptr, &info_ptr);
	    bu_free(ctx.buffer, "png_mem buffer");
	    return BRLCAD_ERROR;
    }

    png_set_IHDR(png_ptr, info_ptr, bif->width, bif->height,
		 8, color_type, PNG_INTERLACE_NONE,
		 PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);

    png_write_info(png_ptr, info_ptr);

    data = icv_data2uchar(bif);
    if (!data) {
	png_destroy_write_struct(&png_ptr, &info_ptr);
	bu_free(ctx.buffer, "png_mem buffer");
	return BRLCAD_ERROR;
    }

    /* BRL-CAD images are bottom-up, PNG wants top-down */
    for (size_t i = 0; i < (size_t)bif->height; i++) {
	row = bif->height - 1 - i;
	png_bytep row_pointer = data + row * bif->width * bif->channels;
	png_write_row(png_ptr, row_pointer);
    }

    png_write_end(png_ptr, info_ptr);
    png_destroy_write_struct(&png_ptr, &info_ptr);
    bu_free(data, "png_write_mem data");

    /* Trim allocated memory to exact final size */
    if (ctx.size > 0 && ctx.size < ctx.capacity) {
	*outbuffer = (unsigned char *)bu_realloc(ctx.buffer, ctx.size, "png_mem buffer trim");
    } else {
	*outbuffer = ctx.buffer;
    }

    *outsize = ctx.size;

    return BRLCAD_OK;
}

struct png_source {
    FILE *stream;
    const unsigned char *buffer;
    size_t size;
    size_t offset;
    mime_png::inspector *header;
};

static void
user_read_data(png_structp png_ptr, png_bytep data, png_size_t length)
{
    png_voidp io_ptr = png_get_io_ptr(png_ptr);
    if (!io_ptr) {
	png_error(png_ptr, "PNG input source is NULL");
	return;
    }

    auto *src = static_cast<png_source *>(io_ptr);
    if (src->stream) {
	if (fread(data, 1, length, src->stream) != length)
	    png_error(png_ptr, "Cannot read PNG input");
    } else {
	if (src->offset > src->size || length > src->size - src->offset)
	    png_error(png_ptr, "Read beyond end of PNG buffer");
	memcpy(data, src->buffer + src->offset, length);
	src->offset += length;
    }
    src->header->feed(data, length);
}

struct png_decode_state {
    icv_image_t *image = NULL;
    unsigned char *pixels = NULL;
    png_bytep *rows = NULL;
    mime_png::inspector header;
};

static icv_image_t *
png_decode(FILE *stream, const unsigned char *buffer, size_t size, const char *type, const bu_mime_traits *traits, const bu_mime_traits *observations)
{
    png_structp png = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png)
	return NULL;
    png_infop info = png_create_info_struct(png);
    if (!info) {
	png_destroy_read_struct(&png, NULL, NULL);
	return NULL;
    }
    /* libpng errors use longjmp. Keep changing ownership state on the heap
     * so cleanup does not inspect indeterminate automatic variables. */
    auto *state = new (std::nothrow) png_decode_state;
    if (!state) {
	png_destroy_read_struct(&png, &info, NULL);
	return NULL;
    }
    if (setjmp(png_jmpbuf(png))) {
	png_destroy_read_struct(&png, &info, NULL);
	icv_destroy(state->image);
	bu_free(state->pixels, "PNG pixels");
	bu_free(state->rows, "PNG rows");
	delete state;
	return NULL;
    }
    struct png_source source = {stream, buffer, size, 0, &state->header};
    png_set_read_fn(png, &source, user_read_data);
    png_read_info(png, info);
    if (!state->header.facts.matches(traits, observations) ||
	(BU_STR_EQUAL(type, "image/apng") && !state->header.facts.animated))
	png_error(png, "Requested PNG traits do not match the encoded header");
    int color_type = png_get_color_type(png, info);
    int bit_depth = png_get_bit_depth(png, info);
    if (color_type == PNG_COLOR_TYPE_PALETTE)
	png_set_palette_to_rgb(png);
    if (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8)
	png_set_expand_gray_1_2_4_to_8(png);
    if (png_get_valid(png, info, PNG_INFO_tRNS))
	png_set_tRNS_to_alpha(png);
    if (bit_depth == 16)
	png_set_strip_16(png);
    png_set_interlace_handling(png);
    png_read_update_info(png, info);

    size_t width = png_get_image_width(png, info);
    size_t height = png_get_image_height(png, info);
    size_t channels = png_get_channels(png, info);
    size_t row_size = png_get_rowbytes(png, info);
    if (!width || !height || !channels || channels > 4 ||
	width > SIZE_MAX / channels || row_size != width * channels ||
	height > SIZE_MAX / row_size || height > SIZE_MAX / sizeof(png_bytep))
	png_error(png, "Invalid or excessive PNG dimensions");
    state->image = icv_create_with_channels(width, height,
	channels <= 2 ? ICV_COLOR_SPACE_GRAY : ICV_COLOR_SPACE_RGB, channels);
    if (!state->image)
	png_error(png, "Cannot allocate PNG image");
    state->pixels = static_cast<unsigned char *>(bu_malloc(row_size * height, "PNG pixels"));
    state->rows = static_cast<png_bytep *>(bu_malloc(height * sizeof(png_bytep), "PNG rows"));
    for (size_t row = 0; row < height; ++row)
	state->rows[height - row - 1] = state->pixels + row * row_size;
    png_read_image(png, state->rows);
    png_read_end(png, info);
    for (size_t i = 0; i < row_size * height; ++i)
	state->image->data[i] = static_cast<double>(state->pixels[i]) / 255.0;

    png_textp text = NULL;
    int text_count = 0;
    if (png_get_text(png, info, &text, &text_count) > 0)
	for (int i = 0; i < text_count; ++i) {
	    if (!text[i].key || !text[i].text || !BU_STR_EQUAL(text[i].key, ICV_PNG_KEY_SCENE))
		continue;
	    struct icv_render_info *render_info = icv_render_info_create();
	    if (json_to_render_info(text[i].text, render_info))
		state->image->render_info = render_info;
	    else
		icv_render_info_destroy(render_info);
	    break;
	}
    icv_image_t *image = state->image;
    png_destroy_read_struct(&png, &info, NULL);
    bu_free(state->pixels, "PNG pixels");
    bu_free(state->rows, "PNG rows");
    delete state;
    return image;
}

extern "C" icv_image_t *
png_read(FILE *stream, size_t, size_t, const char *type, const bu_mime_traits *traits, const bu_mime_traits *observations)
{
    return stream ? png_decode(stream, NULL, 0, type, traits, observations) : NULL;
}

extern "C" icv_image_t *
png_read_mem(const unsigned char *buffer, size_t size, size_t, size_t, const char *type, const bu_mime_traits *traits, const bu_mime_traits *observations)
{
    return buffer && size ? png_decode(NULL, buffer, size, type, traits, observations) : NULL;
}



/*
 * Local Variables:
 * mode: C++
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
