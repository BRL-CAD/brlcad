/*           I C V _ P N G _ J S O N _ T E S T . C P P
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
/** @file icv_png_json_test.cpp
 *
 * Verifies that the BRL-CAD scene metadata embedded in PNG tEXt chunks
 * conforms to the JSON schema documented in png.cpp.
 *
 * Tests performed:
 *   1. Single tEXt chunk with key "BRL-CAD-scene" is written.
 *   2. The chunk value is valid JSON.
 *   3. The JSON has the expected top-level keys and types (version, db,
 *      objects array, camera object with viewrotscale/eye_model/viewsize/
 *      aspect/perspective).
 *   4. Double values round-trip exactly (full-precision).
 *   5. Objects are serialised as a JSON array and deserialised back to the
 *      original space-separated string.
 *   6. Images written without render_info produce no "BRL-CAD-scene" chunk.
 *
 * The helper png_read_text_chunk() reads tEXt chunks by interpreting the PNG
 * binary format directly (PNG spec section 11.3), without linking against
 * libpng.  This keeps the test self-contained with only libicv + libbu.
 */

#include "common.h"

#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "bu/app.h"
#include "bu/log.h"
#include "bu/malloc.h"
#include "bu/str.h"
#include "bu/vls.h"
#include "bu/file.h"
#include "vmath.h"
#include "icv.h"

#include "../../libbu/json.hpp"

/* -------------------------------------------------------------------------- */
/* Simple pass/fail accounting                                                 */
/* -------------------------------------------------------------------------- */
static int tests_run    = 0;
static int tests_passed = 0;

#define CHECK(cond, msg) do { \
    tests_run++; \
    if (cond) { \
	tests_passed++; \
    } else { \
	bu_log("FAIL [%s:%d]: %s\n", __FILE__, __LINE__, msg); \
    } \
} while (0)

/*
 * Bitwise exact double equality - avoids -Wfloat-equal while correctly
 * testing that a full-precision serialisation round-trip is lossless.
 */
static inline bool
dbl_eq(double a, double b)
{
    uint64_t ua, ub;
    memcpy(&ua, &a, sizeof(ua));
    memcpy(&ub, &b, sizeof(ub));
    return ua == ub;
}


/* -------------------------------------------------------------------------- */
/* Helper: create a small solid-colour RGB image                              */
/* -------------------------------------------------------------------------- */
static icv_image_t *
make_solid(size_t w, size_t h, double r, double g, double b)
{
    icv_image_t *img = icv_create(w, h, ICV_COLOR_SPACE_RGB);
    if (!img) return NULL;
    for (size_t i = 0; i < w * h; i++) {
img->data[i*3+0] = r;
img->data[i*3+1] = g;
img->data[i*3+2] = b;
    }
    return img;
}


/* -------------------------------------------------------------------------- */
/* Minimal PNG tEXt reader                                                    */
/*                                                                             */
/* PNG binary layout (RFC 2083 / ISO 15948):                                  */
/*   8-byte signature                                                          */
/*   Chunks: [4 length][4 type][<length> data][4 CRC]                         */
/*                                                                             */
/* tEXt chunk data: <keyword>\0<text>   (both Latin-1 strings)               */
/*                                                                             */
/* We locate all tEXt chunks and return the text for the requested keyword.  */
/* -------------------------------------------------------------------------- */
static uint32_t
read_be32(const unsigned char *p)
{
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16)
 | ((uint32_t)p[2] << 8)  |  (uint32_t)p[3];
}

static std::string
png_read_text_chunk(const char *filename, const char *key)
{
    FILE *fp = fopen(filename, "rb");
    if (!fp) return std::string();

    /* Read entire file into memory */
    if (fseek(fp, 0, SEEK_END) != 0) { fclose(fp); return std::string(); }
    long fsz = ftell(fp);
    if (fsz <= 0) { fclose(fp); return std::string(); }
    rewind(fp);

    std::vector<unsigned char> buf((size_t)fsz);
    if (fread(buf.data(), 1, (size_t)fsz, fp) != (size_t)fsz) {
fclose(fp); return std::string();
    }
    fclose(fp);

    /* Verify PNG signature */
    static const unsigned char PNG_SIG[8] = {137,80,78,71,13,10,26,10};
    if (buf.size() < 8 || memcmp(buf.data(), PNG_SIG, 8) != 0)
return std::string();

    size_t pos = 8;
    while (pos + 12 <= buf.size()) {
uint32_t length = read_be32(buf.data() + pos);
/* chunk type is 4 ASCII bytes */
const unsigned char *type = buf.data() + pos + 4;
const unsigned char *data = buf.data() + pos + 8;

if (pos + 12 + length > buf.size())
    break;

/* tEXt chunk type = 0x74455874 ("tEXt") */
if (type[0]=='t' && type[1]=='E' && type[2]=='X' && type[3]=='t'
    && length > 0)
{
    /* keyword ends at the first NUL byte */
    const unsigned char *nul = (const unsigned char *)memchr(data, 0, length);
    if (nul) {
size_t klen = (size_t)(nul - data);
std::string kw(reinterpret_cast<const char *>(data), klen);
if (kw == key) {
    /* text follows the NUL */
    size_t tlen = length - klen - 1;
    return std::string(reinterpret_cast<const char *>(nul + 1), tlen);
}
    }
}

/* IEND chunk signals end */
if (type[0]=='I' && type[1]=='E' && type[2]=='N' && type[3]=='D')
    break;

pos += 12 + length;
    }

    return std::string();
}


/* -------------------------------------------------------------------------- */
/* Test 1 – JSON chunk structure                                               */
/* -------------------------------------------------------------------------- */
static void
test_json_chunk_structure(const char *tmpdir)
{
    bu_log("\n=== Test 1: JSON chunk structure ===\n");

    icv_image_t *img = make_solid(4, 4, 0.5, 0.3, 0.1);
    CHECK(img != NULL, "created test image");
    if (!img) return;

    struct icv_render_info *ri = icv_render_info_create();
    ri->db_filename = bu_strdup("/project/models/tank.g");
    ri->objects     = bu_strdup("hull.r turret.r");

    MAT_IDN(ri->viewrotscale);
    VSET(ri->eye_model, 100.1, -200.2, 300.3);
    ri->viewsize    = 5000.0;
    ri->aspect      = 1.333;
    ri->perspective = 30.0;

    icv_image_set_render_info(img, ri);

    struct bu_vls fname = BU_VLS_INIT_ZERO;
    bu_vls_printf(&fname, "%s/json_struct_test.png", tmpdir);

    int wret = icv_write(img, bu_vls_cstr(&fname), BU_MIME_IMAGE_PNG);
    CHECK(wret == BRLCAD_OK, "wrote PNG with render metadata");
    icv_destroy(img);

    /* Read the raw tEXt chunk without libpng */
    std::string raw = png_read_text_chunk(bu_vls_cstr(&fname), "BRL-CAD-scene");
    CHECK(!raw.empty(), "BRL-CAD-scene tEXt chunk is present");

    if (!raw.empty()) {
bu_log("  raw JSON: %s\n", raw.c_str());

using json = nlohmann::json;
bool valid_json = true;
json j;
try {
    j = json::parse(raw);
} catch (...) {
    valid_json = false;
}
CHECK(valid_json, "BRL-CAD-scene value is valid JSON");

if (valid_json) {
    /* version */
    CHECK(j.contains("version") && j["version"].is_number_integer(),
  "JSON has integer 'version' field");
    if (j.contains("version"))
bu_log("  version: %d\n", j["version"].get<int>());

    /* db */
    CHECK(j.contains("db") && j["db"].is_string(),
  "JSON has string 'db' field");
    if (j.contains("db") && j["db"].is_string())
CHECK(j["db"].get<std::string>() == "/project/models/tank.g",
      "db path value is correct");

    /* objects – must be an array */
    CHECK(j.contains("objects") && j["objects"].is_array(),
  "JSON has array 'objects' field");
    if (j.contains("objects") && j["objects"].is_array()) {
CHECK(j["objects"].size() == 2u, "objects array has 2 elements");
if (j["objects"].size() >= 2u) {
    CHECK(j["objects"][0].get<std::string>() == "hull.r",
  "objects[0] == 'hull.r'");
    CHECK(j["objects"][1].get<std::string>() == "turret.r",
  "objects[1] == 'turret.r'");
}
    }

    /* camera */
    CHECK(j.contains("camera") && j["camera"].is_object(),
  "JSON has object 'camera' field");
    if (j.contains("camera") && j["camera"].is_object()) {
const json &cam = j["camera"];

CHECK(cam.contains("viewrotscale") && cam["viewrotscale"].is_array()
      && cam["viewrotscale"].size() == 16u,
      "camera.viewrotscale is array of 16");

CHECK(cam.contains("eye_model") && cam["eye_model"].is_array()
      && cam["eye_model"].size() == 3u,
      "camera.eye_model is array of 3");

CHECK(cam.contains("viewsize")    && cam["viewsize"].is_number(),
      "camera.viewsize is number");
CHECK(cam.contains("aspect")      && cam["aspect"].is_number(),
      "camera.aspect is number");
CHECK(cam.contains("perspective") && cam["perspective"].is_number(),
      "camera.perspective is number");

if (cam.contains("viewsize") && cam["viewsize"].is_number())
    CHECK(dbl_eq(cam["viewsize"].get<double>(), 5000.0),
  "camera.viewsize == 5000.0");
if (cam.contains("perspective") && cam["perspective"].is_number())
    CHECK(dbl_eq(cam["perspective"].get<double>(), 30.0),
  "camera.perspective == 30.0");
    }
}
    }

    bu_file_delete(bu_vls_cstr(&fname));
    bu_vls_free(&fname);
}


/* -------------------------------------------------------------------------- */
/* Test 2 – Full-precision double round-trip                                  */
/* -------------------------------------------------------------------------- */
static void
test_double_precision(const char *tmpdir)
{
    bu_log("\n=== Test 2: double precision round-trip ===\n");

    icv_image_t *img = make_solid(2, 2, 0.0, 0.0, 0.0);
    CHECK(img != NULL, "created test image");
    if (!img) return;

    struct icv_render_info *ri = icv_render_info_create();
    ri->db_filename = bu_strdup("precision.g");

    /* Use values that are not exactly representable in fewer digits */
    const double precise_x  =  123.456789012345678;
    const double precise_y  = -987.654321098765432;
    const double precise_vs =  1234.5678901234567;

    MAT_IDN(ri->viewrotscale);
    VSET(ri->eye_model, precise_x, precise_y, 0.5);
    ri->viewsize    = precise_vs;
    ri->aspect      = 1.0;
    ri->perspective = 0.0;

    icv_image_set_render_info(img, ri);

    struct bu_vls fname = BU_VLS_INIT_ZERO;
    bu_vls_printf(&fname, "%s/json_precision_test.png", tmpdir);
    int wret = icv_write(img, bu_vls_cstr(&fname), BU_MIME_IMAGE_PNG);
    CHECK(wret == BRLCAD_OK, "wrote PNG");
    icv_destroy(img);

    /* Read back via public API */
    icv_image_t *img2 = icv_read(bu_vls_cstr(&fname), BU_MIME_IMAGE_PNG, 0, 0);
    CHECK(img2 != NULL, "read PNG back");

    if (img2) {
const struct icv_render_info *ri2 = icv_image_get_render_info(img2);
CHECK(ri2 != NULL, "render_info present after read-back");
if (ri2) {
    /* Exact bit-for-bit equality expected after full-precision serialisation */
    CHECK(dbl_eq(ri2->eye_model[0], precise_x),
  "eye_model[0] round-trips exactly");
    CHECK(dbl_eq(ri2->eye_model[1], precise_y),
  "eye_model[1] round-trips exactly");
    CHECK(dbl_eq(ri2->viewsize, precise_vs),
  "viewsize round-trips exactly");
    bu_log("  eye_model[0]: %.17g (orig %.17g) match=%d\n",
   ri2->eye_model[0], precise_x,
   dbl_eq(ri2->eye_model[0], precise_x));
    bu_log("  eye_model[1]: %.17g (orig %.17g) match=%d\n",
   ri2->eye_model[1], precise_y,
   dbl_eq(ri2->eye_model[1], precise_y));
    bu_log("  viewsize:     %.17g (orig %.17g) match=%d\n",
   ri2->viewsize, precise_vs,
   dbl_eq(ri2->viewsize, precise_vs));
}
icv_destroy(img2);
    }

    bu_file_delete(bu_vls_cstr(&fname));
    bu_vls_free(&fname);
}


/* -------------------------------------------------------------------------- */
/* Test 3 – No metadata => no scene chunk                                     */
/* -------------------------------------------------------------------------- */
static void
test_no_metadata_no_chunk(const char *tmpdir)
{
    bu_log("\n=== Test 3: no render_info => no BRL-CAD-scene chunk ===\n");

    icv_image_t *img = make_solid(2, 2, 0.25, 0.5, 0.75);
    CHECK(img != NULL, "created test image");
    if (!img) return;

    /* Do NOT attach render_info */

    struct bu_vls fname = BU_VLS_INIT_ZERO;
    bu_vls_printf(&fname, "%s/json_nometa_test.png", tmpdir);
    int wret = icv_write(img, bu_vls_cstr(&fname), BU_MIME_IMAGE_PNG);
    CHECK(wret == BRLCAD_OK, "wrote PNG without metadata");
    icv_destroy(img);

    std::string raw = png_read_text_chunk(bu_vls_cstr(&fname), "BRL-CAD-scene");
    CHECK(raw.empty(), "no BRL-CAD-scene chunk when render_info is NULL");

    bu_file_delete(bu_vls_cstr(&fname));
    bu_vls_free(&fname);
}


/* -------------------------------------------------------------------------- */
/* Test 4 – Objects with a single entry (edge case)                           */
/* -------------------------------------------------------------------------- */
static void
test_single_object(const char *tmpdir)
{
    bu_log("\n=== Test 4: single object round-trip ===\n");

    icv_image_t *img = make_solid(2, 2, 0.0, 0.0, 0.0);
    CHECK(img != NULL, "created test image");
    if (!img) return;

    struct icv_render_info *ri = icv_render_info_create();
    ri->db_filename = bu_strdup("single.g");
    ri->objects     = bu_strdup("all.r");
    MAT_IDN(ri->viewrotscale);
    VSET(ri->eye_model, 0, 0, 1000);
    ri->viewsize    = 500.0;
    ri->aspect      = 1.0;
    ri->perspective = 0.0;
    icv_image_set_render_info(img, ri);

    struct bu_vls fname = BU_VLS_INIT_ZERO;
    bu_vls_printf(&fname, "%s/json_single_obj.png", tmpdir);
    int wret = icv_write(img, bu_vls_cstr(&fname), BU_MIME_IMAGE_PNG);
    CHECK(wret == BRLCAD_OK, "wrote PNG");
    icv_destroy(img);

    /* Verify JSON array has one element */
    std::string raw = png_read_text_chunk(bu_vls_cstr(&fname), "BRL-CAD-scene");
    CHECK(!raw.empty(), "BRL-CAD-scene chunk present");
    if (!raw.empty()) {
using json = nlohmann::json;
json j;
bool valid_json = true;
try { j = json::parse(raw); } catch (...) { valid_json = false; }
CHECK(valid_json, "valid JSON");
if (valid_json && j.contains("objects") && j["objects"].is_array()) {
    CHECK(j["objects"].size() == 1u, "objects array has 1 element");
    if (!j["objects"].empty())
CHECK(j["objects"][0].get<std::string>() == "all.r",
      "objects[0] == 'all.r'");
}
    }

    /* Verify API round-trip */
    icv_image_t *img2 = icv_read(bu_vls_cstr(&fname), BU_MIME_IMAGE_PNG, 0, 0);
    CHECK(img2 != NULL, "read PNG back");
    if (img2) {
const struct icv_render_info *ri2 = icv_image_get_render_info(img2);
CHECK(ri2 && ri2->objects && BU_STR_EQUAL(ri2->objects, "all.r"),
      "objects field round-trips as 'all.r'");
icv_destroy(img2);
    }

    bu_file_delete(bu_vls_cstr(&fname));
    bu_vls_free(&fname);
}


/* -------------------------------------------------------------------------- */
/* main                                                                        */
/* -------------------------------------------------------------------------- */
int
main(int argc, char *argv[])
{
    bu_setprogname(argv[0]);

    const char *tmpdir = "/tmp";
    if (argc > 1)
tmpdir = argv[1];

    test_json_chunk_structure(tmpdir);
    test_double_precision(tmpdir);
    test_no_metadata_no_chunk(tmpdir);
    test_single_object(tmpdir);

    bu_log("\n=== Results: %d/%d tests passed ===\n", tests_passed, tests_run);

    return (tests_passed == tests_run) ? 0 : 1;
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
