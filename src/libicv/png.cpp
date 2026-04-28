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
&& cam["viewrotscale"].size() == 16)
    {
for (int i = 0; i < 16; i++)
    info->viewrotscale[i] = cam["viewrotscale"][i].get<double>();
    }

    if (cam.contains("eye_model") && cam["eye_model"].is_array()
&& cam["eye_model"].size() == 3)
    {
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

    static int png_color_type;
    std::string scene_json;

    switch (bif->color_space) {
case ICV_COLOR_SPACE_GRAY:
    png_color_type = PNG_COLOR_TYPE_GRAY;
    break;
default:
    png_color_type = PNG_COLOR_TYPE_RGB;
    }

    unsigned char *data = icv_data2uchar(bif);

    png_structp png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (UNLIKELY(png_ptr == NULL)) {
bu_free(data, "png write uchar data");
return BRLCAD_ERROR;
    }

    png_infop info_ptr = png_create_info_struct(png_ptr);
    if (info_ptr == NULL || setjmp(png_jmpbuf(png_ptr))) {
png_destroy_read_struct(&png_ptr, info_ptr ? &info_ptr : NULL, NULL);
bu_log("ERROR: Unable to create png header\n");
bu_free(data, "png write uchar data");
return BRLCAD_ERROR;
    }

    png_init_io(png_ptr, fp);
    png_set_IHDR(png_ptr, info_ptr, (unsigned)bif->width, (unsigned)bif->height, 8, png_color_type,
 PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT,
 PNG_FILTER_TYPE_DEFAULT);

    /* Embed render metadata as a single JSON tEXt chunk when available */
    if (bif->render_info) {
scene_json = render_info_to_json(bif->render_info);

png_text text_chunk;
memset(&text_chunk, 0, sizeof(text_chunk));
text_chunk.compression = PNG_TEXT_COMPRESSION_NONE;
text_chunk.key  = (png_charp)ICV_PNG_KEY_SCENE;
text_chunk.text = (png_charp)scene_json.c_str();
text_chunk.text_length = scene_json.size();

/* scene_json must remain valid until after png_write_info() because
	 * libpng may store a pointer to the text rather than copying it. */
png_set_text(png_ptr, info_ptr, &text_chunk, 1);
    }

    png_write_info(png_ptr, info_ptr);

    for (size_t i = bif->height-1; i > 0; --i) {
png_write_row(png_ptr, (png_bytep) (data + bif->width*bif->channels*i));
    }
    png_write_row(png_ptr, (png_bytep) (data + 0));
    png_write_end(png_ptr, info_ptr);

    png_destroy_write_struct(&png_ptr, &info_ptr);
    bu_free(data, "png write uchar data");

    return BRLCAD_OK;
}

extern "C" icv_image_t *
png_read(FILE *fp)
{
    if (UNLIKELY(!fp))
return NULL;

    char header[8];
    if (fread(header, 8, 1, fp) != 1) {
bu_log("png-pix: ERROR: Failed while reading file header!!!\n");
return NULL;
    }

    if (png_sig_cmp((png_bytep)header, 0, 8)) {
bu_log("png-pix: This is not a PNG file!!!\n");
return NULL;
    }

    png_structp png_p = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png_p) {
bu_log("png-pix: png_create_read_struct() failed!!\n");
return NULL;
    }

    png_infop info_p = png_create_info_struct(png_p);
    if (!info_p) {
bu_log("png-pix: png_create_info_struct() failed!!\n");
return NULL;
    }

    icv_image_t *bif;
    BU_ALLOC(bif, struct icv_image);
    ICV_IMAGE_INIT(bif);

    png_init_io(png_p, fp);
    png_set_sig_bytes(png_p, 8);
    png_read_info(png_p, info_p);
    int color_type = png_get_color_type(png_p, info_p);
    if (color_type == PNG_COLOR_TYPE_GRAY || color_type == PNG_COLOR_TYPE_GRAY_ALPHA) {
png_set_gray_to_rgb(png_p);
    }
    png_set_expand(png_p);
    int bit_depth = png_get_bit_depth(png_p, info_p);
    if (bit_depth == 16) png_set_strip_16(png_p);

    bif->width = png_get_image_width(png_p, info_p);
    bif->height = png_get_image_height(png_p, info_p);

    /* Read the BRL-CAD scene JSON tEXt chunk (must be done after png_read_info) */
    {
png_textp text_ptr = NULL;
int num_text = 0;
if (png_get_text(png_p, info_p, &text_ptr, &num_text) > 0 && num_text > 0) {
    for (int i = 0; i < num_text; i++) {
if (!text_ptr[i].key || !text_ptr[i].text)
    continue;
if (BU_STR_EQUAL(text_ptr[i].key, ICV_PNG_KEY_SCENE)) {
    struct icv_render_info *ri = icv_render_info_create();
    if (json_to_render_info(text_ptr[i].text, ri)) {
bif->render_info = ri;
    } else {
icv_render_info_free(ri);
    }
    break;
}
    }
}
    }

    png_color_16p input_backgrd;
    if (png_get_bKGD(png_p, info_p, &input_backgrd)) {
png_set_background(png_p, input_backgrd, PNG_BACKGROUND_GAMMA_FILE, 1, 1.0);
    } else {
png_color_16 def_backgrd={ 0, 0, 0, 0, 0 };
png_set_background(png_p, &def_backgrd, PNG_BACKGROUND_GAMMA_FILE, 0, 1.0);
    }

    double gammaval;
    if (png_get_gAMA(png_p, info_p, &gammaval)) {
png_set_gAMA(png_p, info_p, gammaval);
    }

    png_read_update_info(png_p, info_p);


    /* allocate memory for image */
    unsigned char *image = (unsigned char *)bu_calloc(1, bif->width*bif->height*3, "image");

    /* create rows array */
    unsigned char **rows = (unsigned char **)bu_calloc(bif->height, sizeof(unsigned char *), "rows");
    for (size_t i = 0; i < bif->height; i++)
rows[bif->height - 1 - i] = image+(i * bif->width * 3);

    png_read_image(png_p, rows);

    bif->data = icv_uchar2double(image, 3 * bif->width * bif->height);
    bu_free(image, "png_read : unsigned char data");
    bif->magic = ICV_IMAGE_MAGIC;
    bif->channels = 3;
    bif->color_space = ICV_COLOR_SPACE_RGB;

    png_destroy_read_struct(&png_p, &info_p, NULL);
    bu_free(rows, "png rows");

    return bif;
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
