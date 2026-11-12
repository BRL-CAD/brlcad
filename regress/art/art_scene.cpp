/*              A R T _ S C E N E . C P P
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
#include <cmath>
#include <exception>
#include <memory>
#include <string>

#include "bu/app.h"
#include "bu/log.h"
#include "bu/str.h"
#include "icv.h"
#include "ged.h"
#include "raytrace.h"
#include "wdb.h"
#include "art_test_mesh.h"

static int
create_scene(const char *path, const char *shader, const char *output)
{
    struct rt_wdb *db = wdb_fopen(path);
    if (!db)
	return 1;

    point_t center = {-15, 0, 0};
    point_t box_min = {5, -10, -10};
    point_t box_max = {25, 10, 10};
    point_t bore_base = {15, -15, 0};
    vect_t bore_height = {0, 30, 0};
    const unsigned char red[] = {255, 32, 16};
    const unsigned char blue[] = {32, 64, 255};
    const unsigned char green[] = {32, 255, 64};
    struct wmember members;
    bool success = mk_sph(db, "ball.s", center, 10) == 0 &&
	mk_rpp(db, "box.s", box_min, box_max) == 0 &&
	mk_rcc(db, "bore.s", bore_base, bore_height, 4) == 0;

    BU_LIST_INIT(&members.l);
    success = success && mk_addmember("ball.s", &members.l, NULL, WMOP_UNION) &&
	mk_lcomb(db, "ball.r", &members, 1, "plastic", "", red, 0) == 0;
    BU_LIST_INIT(&members.l);
    success = success && mk_addmember("box.s", &members.l, NULL, WMOP_UNION) &&
	mk_addmember("bore.s", &members.l, NULL, WMOP_SUBTRACT) &&
	mk_lcomb(db, "box.r", &members, 1, "plastic", "", NULL, 0) == 0;
    BU_LIST_INIT(&members.l);
    success = success && mk_addmember("box.r", &members.l, NULL, WMOP_UNION) &&
	mk_lcomb(db, "blue.g", &members, 0, NULL, NULL, blue, 1) == 0;
    BU_LIST_INIT(&members.l);
    success = success && mk_addmember("ball.r", &members.l, NULL, WMOP_UNION) &&
	mk_addmember("blue.g", &members.l, NULL, WMOP_UNION) &&
	mk_lcomb(db, "all.g", &members, 0, NULL, NULL, NULL, 0) == 0;
    success = success && art_test::make_mesh(db, "mesh.s", art_test::MeshUV::Authored) == 0;
    BU_LIST_INIT(&members.l);
    success = success && mk_addmember("mesh.s", &members.l, NULL, WMOP_UNION) &&
	mk_lcomb(db, "mesh.r", &members, 1, "plastic", "", green, 0) == 0;
    success = success && art_test::make_mesh(db, "smooth_mesh.s", art_test::MeshUV::Authored,
	art_test::MeshNormals::Authored) == 0;
    BU_LIST_INIT(&members.l);
    success = success && mk_addmember("smooth_mesh.s", &members.l, NULL, WMOP_UNION) &&
	mk_lcomb(db, "smooth_mesh.r", &members, 1, "plastic", "", green, 0) == 0;
    const vect_t ell_a = {8, 0, 0}, ell_b = {0, 10, 0}, ell_c = {0, 0, 6};
    success = success && mk_ell(db, "ellipsoid.s", center, ell_a, ell_b, ell_c) == 0;
    BU_LIST_INIT(&members.l);
    success = success && mk_addmember("ellipsoid.s", &members.l, NULL, WMOP_UNION) &&
	mk_lcomb(db, "ellipsoid.r", &members, 1, "plastic", "", red, 0) == 0;
    // Two paths to one region must keep their transforms and inherited colors.
    mat_t translated;
    MAT_IDN(translated);
    translated[MDX] = 30;
    BU_LIST_INIT(&members.l);
    success = success && mk_addmember("ball.r", &members.l, NULL, WMOP_UNION) &&
	mk_lcomb(db, "instance_left.g", &members, 0, NULL, NULL, red, 1) == 0;
    BU_LIST_INIT(&members.l);
    success = success && mk_addmember("ball.r", &members.l, translated, WMOP_UNION) &&
	mk_lcomb(db, "instance_right.g", &members, 0, NULL, NULL, blue, 1) == 0;
    BU_LIST_INIT(&members.l);
    success = success && mk_addmember("instance_left.g", &members.l, NULL, WMOP_UNION) &&
	mk_addmember("instance_right.g", &members.l, NULL, WMOP_UNION) &&
	mk_lcomb(db, "instances.g", &members, 0, NULL, NULL, NULL, 0) == 0;

    // Repeated arcs in one combination need their instance index as well.
    BU_LIST_INIT(&members.l);
    success = success && mk_addmember("ball.r", &members.l, NULL, WMOP_UNION) &&
	mk_addmember("ball.r", &members.l, translated, WMOP_UNION) &&
	mk_lcomb(db, "arcs.g", &members, 0, NULL, NULL, NULL, 0) == 0;

    if (shader) {
	struct bu_attribute_value_set empty, optical;
	bu_avs_init_empty(&empty);
	bu_avs_init_empty(&optical);
	bu_avs_add(&optical, "OSL", shader);
	if (output)
	    bu_avs_add(&optical, "OSL.output", output);
	success = success && mk_material(db, "osl.material", "OSL test material", "", "",
	    &empty, &empty, &optical, &empty) == 0;
	success = success && db5_update_attribute("ball.r", "material_name", "osl.material", db->dbip) == 0 &&
	    db5_update_attribute("box.r", "material_name", "osl.material", db->dbip) == 0 &&
	    db5_update_attribute("mesh.r", "material_name", "osl.material", db->dbip) == 0 &&
	    db5_update_attribute("smooth_mesh.r", "material_name", "osl.material", db->dbip) == 0 &&
	    db5_update_attribute("ellipsoid.r", "material_name", "osl.material", db->dbip) == 0;
	bu_avs_free(&optical);
	bu_avs_free(&empty);
    }
    wdb_close(db);
    return success ? 0 : 1;
}

static int
edit_material_property(const char *path, const char *key, const char *value)
{
    struct ged *editor = ged_open("db", path, 0);
    if (!editor)
	return 1;
    const char *arguments[] = {"material", value ? "set" : "remove", "osl.material", "optical", key, value};
    const int argument_count = sizeof(arguments) / sizeof(arguments[0]) - (value ? 0 : 1);
    const int status = ged_exec_material(editor, argument_count, arguments);
    if (status != BRLCAD_OK)
	bu_log("Cannot edit material property '%s': %s\n", key, bu_vls_cstr(editor->ged_result_str));
    ged_close(editor);
    return status == BRLCAD_OK ? 0 : 1;
}

static constexpr double MIN_GEOMETRY_COVERAGE = 0.01;
using ImagePtr = std::unique_ptr<icv_image_t, decltype(&icv_destroy)>;

static ImagePtr
read_image(const char *path, size_t width, size_t height)
{
    ImagePtr image(icv_read(path, BU_MIME_IMAGE_AUTO, width, height), icv_destroy);
    if (image && (image->width != width || image->height != height || image->channels < 3)) {
	bu_log("Unexpected image dimensions or channel count: %s\n", path);
	image.reset();
    }
    return image;
}

enum class TexturePattern { Checker, Black, Gray };

static int
create_texture(const char *path, TexturePattern pattern)
{
    // A one-texel checker must minify in the smaller regression renders.
    constexpr size_t CHECKER_SIZE = 512;
    // Constant fixtures also serve as 2:1 latitude-longitude environments.
    constexpr size_t ENVIRONMENT_WIDTH = 2;
    constexpr size_t ENVIRONMENT_HEIGHT = 1;
    const size_t width = pattern == TexturePattern::Checker ? CHECKER_SIZE : ENVIRONMENT_WIDTH;
    const size_t height = pattern == TexturePattern::Checker ? CHECKER_SIZE : ENVIRONMENT_HEIGHT;
    ImagePtr image(icv_create(width, height, ICV_COLOR_SPACE_RGB), icv_destroy);
    if (!image)
	return 1;
    for (size_t y = 0; y < image->height; ++y) {
	for (size_t x = 0; x < image->width; ++x) {
	    const double value = pattern == TexturePattern::Checker ? static_cast<double>((x + y) % 2) :
		pattern == TexturePattern::Gray ? 0.5 : 0.0;
	    std::fill_n(image->data + (y * image->width + x) * image->channels, image->channels, value);
	}
    }
    return icv_write(image.get(), path, BU_MIME_IMAGE_AUTO) == 0 ? 0 : 1;
}

enum class ImageCheck {
    Regions, Mesh, RepeatedArcs, VaryingRG, FlatNormals, GreenDiagnostic, RedDiagnostic
};

static int
check_image(const char *path, size_t width, size_t height, ImageCheck check)
{
    auto image = read_image(path, width, height);
    if (!image)
	return 1;

    const double COLOR_DOMINANCE = 2.0;
    const double ASPECT_TOLERANCE = 0.2;
    // The separated arc instances leave a central band free of geometry.
    const double PAIR_GAP_HALF_FRACTION = 0.05;
    const size_t gap_half = static_cast<size_t>(width * PAIR_GAP_HALF_FRACTION);
    const size_t min_color_pixels = static_cast<size_t>(width * height * MIN_GEOMETRY_COVERAGE);
    size_t min_x = width, min_y = height, max_x = 0, max_y = 0;
    size_t red_pixels = 0, blue_pixels = 0, green_pixels = 0;
    size_t pair_left = 0, pair_right = 0, pair_center = 0;
    double min_r = INFINITY, min_g = INFINITY, max_r = 0, max_g = 0;
    size_t diagnostic_pixels = 0, black_pixels = 0;
    const double BLACK_PIXEL_LIMIT = 0.02;
    const double RG_BACKGROUND_LIMIT = 0.05;
    const double RG_VISIBLE_LIMIT = 0.2;
    const double MIN_RG_CHANNEL_SPAN = 0.15;
    bool finite = true;
    for (size_t y = 0; y < height; ++y) {
	for (size_t x = 0; x < width; ++x) {
	    const double *pixel = image->data + (y * width + x) * image->channels;
	    finite = finite && std::isfinite(pixel[0]) && std::isfinite(pixel[1]) && std::isfinite(pixel[2]);
	    if (std::max({pixel[0], pixel[1], pixel[2]}) < BLACK_PIXEL_LIMIT)
		++black_pixels;
	    if (pixel[2] < RG_BACKGROUND_LIMIT && std::max(pixel[0], pixel[1]) > RG_VISIBLE_LIMIT) {
		++diagnostic_pixels;
		min_r = std::min(min_r, pixel[0]); max_r = std::max(max_r, pixel[0]);
		min_g = std::min(min_g, pixel[1]); max_g = std::max(max_g, pixel[1]);
	    }
	    if (pixel[0] > COLOR_DOMINANCE * pixel[1] && pixel[0] > COLOR_DOMINANCE * pixel[2]) {
		++red_pixels;
		if (x < width / 2 - gap_half) ++pair_left;
		else if (x >= width / 2 + gap_half) ++pair_right;
		else ++pair_center;
		min_x = std::min(min_x, x);
		min_y = std::min(min_y, y);
		max_x = std::max(max_x, x);
		max_y = std::max(max_y, y);
	    }
	    if (pixel[2] > COLOR_DOMINANCE * pixel[0] && pixel[2] > COLOR_DOMINANCE * pixel[1])
		++blue_pixels;
	    if (pixel[1] > COLOR_DOMINANCE * pixel[0] && pixel[1] > COLOR_DOMINANCE * pixel[2])
		++green_pixels;
	}
    }
    image.reset();
    if (check == ImageCheck::GreenDiagnostic || check == ImageCheck::RedDiagnostic) {
	const bool positive = check == ImageCheck::GreenDiagnostic;
	const size_t expected = positive ? green_pixels : red_pixels;
	const size_t unexpected = positive ? red_pixels : green_pixels;
	if (finite && expected > min_color_pixels && unexpected == 0 && blue_pixels == 0)
	    return 0;
	bu_log("Unexpected %s diagnostic in %s: green=%zu red=%zu blue=%zu\n",
	    positive ? "green" : "red", path, green_pixels, red_pixels, blue_pixels);
	return 1;
    }
    if (check == ImageCheck::FlatNormals) {
	if (finite && black_pixels > min_color_pixels && diagnostic_pixels == 0)
	    return 0;
	bu_log("Facet normals differ in %s: black=%zu differing=%zu\n", path, black_pixels, diagnostic_pixels);
	return 1;
    }
    if (check == ImageCheck::VaryingRG) {
	if (finite && diagnostic_pixels > min_color_pixels && max_r - min_r > MIN_RG_CHANNEL_SPAN &&
	    max_g - min_g > MIN_RG_CHANNEL_SPAN)
	    return 0;
	bu_log("Missing varying red/green diagnostic channels in %s: pixels=%zu ranges=(%g,%g)\n",
	    path, diagnostic_pixels, max_r - min_r, max_g - min_g);
	return 1;
    }
    if (check == ImageCheck::RepeatedArcs) {
	if (finite && pair_left > min_color_pixels && pair_right > min_color_pixels && !pair_center)
	    return 0;
	bu_log("Expected two separated red arc instances in %s: left=%zu right=%zu center=%zu\n",
	    path, pair_left, pair_right, pair_center);
	return 1;
    }
    if (check == ImageCheck::Mesh) {
	if (finite && green_pixels > min_color_pixels)
	    return 0;
	bu_log("Missing finite green BOT geometry: %s\n", path);
	return 1;
    }
    if (!finite || red_pixels <= min_color_pixels || blue_pixels <= min_color_pixels) {
	bu_log("Missing finite red and inherited blue geometry: %s\n", path);
	return 1;
    }

    // A sphere must stay round when the frame aspect or projection changes.
    const double span_x = static_cast<double>(max_x - min_x + 1);
    const double span_y = static_cast<double>(max_y - min_y + 1);
    if (std::abs(span_x - span_y) > ASPECT_TOLERANCE * std::max(span_x, span_y)) {
	bu_log("Distorted sphere in %s: %g x %g pixels\n", path, span_x, span_y);
	return 1;
    }
    return 0;
}

enum class ImageComparison { Matching, Aliased, Changed };

static int
check_image_difference(const char *path, const char *reference_path,
    size_t width, size_t height, ImageComparison comparison)
{
    auto image = read_image(path, width, height);
    auto reference = read_image(reference_path, width, height);
    if (!image || !reference)
	return 1;
    // Use the same display pipeline for the texture and its independent
    // reference. A reference corner identifies the sky background.
    constexpr double MIN_GEOMETRY_CONTRAST = 0.02;
    constexpr double MAX_MATCHING_RMS = 0.02;
    constexpr double MAX_MATCHING_ERROR = 0.05;
    constexpr double MIN_ALIASED_RMS = 0.1;
    constexpr double MIN_CHANGED_RMS = 0.01;
    double squared_error = 0, max_error = 0;
    size_t geometry_pixels = 0;
    const size_t pixel_count = width * height;
    for (size_t pixel = 0; pixel < pixel_count; ++pixel) {
	const double *actual = image->data + pixel * image->channels;
	const double *expected = reference->data + pixel * reference->channels;
	double background_contrast = 0;
	for (size_t channel = 0; channel < 3; ++channel)
	    background_contrast = std::max(background_contrast,
		std::abs(expected[channel] - reference->data[channel]));
	if (background_contrast > MIN_GEOMETRY_CONTRAST)
	    ++geometry_pixels;
	for (size_t channel = 0; channel < 3; ++channel) {
	    if (!std::isfinite(actual[channel]) || !std::isfinite(expected[channel])) {
		bu_log("Nonfinite texture comparison pixel: %s\n", path);
		return 1;
	    }
	    const double error = std::abs(actual[channel] - expected[channel]);
	    squared_error += error * error;
	    max_error = std::max(max_error, error);
	}
    }
    const double rms = std::sqrt(squared_error / (pixel_count * 3));
    const bool matching = comparison == ImageComparison::Matching;
    const double min_difference = comparison == ImageComparison::Aliased ? MIN_ALIASED_RMS : MIN_CHANGED_RMS;
    const bool difference_matches = matching ?
	rms <= MAX_MATCHING_RMS && max_error <= MAX_MATCHING_ERROR : rms >= min_difference;
    const bool enough_geometry = geometry_pixels > static_cast<size_t>(pixel_count * MIN_GEOMETRY_COVERAGE);
    bu_log("Image comparison %s: rms=%g max=%g geometry=%zu matching=%d\n",
	path, rms, max_error, geometry_pixels, matching);
    return difference_matches && enough_geometry ? 0 : 1;
}

int
main(int argc, char **argv)
{
    bu_setprogname(argv[0]);
    if (argc == 3 && BU_STR_EQUAL(argv[1], "create"))
	return create_scene(argv[2], nullptr, nullptr);
    if (argc == 3 && BU_STR_EQUAL(argv[1], "create-checker-texture"))
	return create_texture(argv[2], TexturePattern::Checker);
    if (argc == 3 && BU_STR_EQUAL(argv[1], "create-black-texture"))
	return create_texture(argv[2], TexturePattern::Black);
    if (argc == 3 && BU_STR_EQUAL(argv[1], "create-gray-texture"))
	return create_texture(argv[2], TexturePattern::Gray);
    if ((argc == 4 || argc == 5) && BU_STR_EQUAL(argv[1], "create-material"))
	return create_scene(argv[2], argv[3], argc == 5 ? argv[4] : nullptr);
    if (argc == 4 && BU_STR_EQUAL(argv[1], "remove-material"))
	return edit_material_property(argv[2], argv[3], nullptr);
    if (argc == 5 && BU_STR_EQUAL(argv[1], "set-material"))
	return edit_material_property(argv[2], argv[3], argv[4]);
    try {
	if (argc == 6 && (BU_STR_EQUAL(argv[1], "check-filtered-texture") ||
	    BU_STR_EQUAL(argv[1], "check-aliased-texture") || BU_STR_EQUAL(argv[1], "check-matching-image") ||
	    BU_STR_EQUAL(argv[1], "check-different-image"))) {
	    const ImageComparison comparison = BU_STR_EQUAL(argv[1], "check-aliased-texture") ? ImageComparison::Aliased :
		BU_STR_EQUAL(argv[1], "check-different-image") ? ImageComparison::Changed : ImageComparison::Matching;
	    return check_image_difference(argv[2], argv[3], std::stoul(argv[4]), std::stoul(argv[5]), comparison);
	}
	if (argc == 5) {
	    const struct { const char *name; ImageCheck check; } checks[] = {
		{"check", ImageCheck::Regions}, {"check-mesh", ImageCheck::Mesh},
		{"check-pair", ImageCheck::RepeatedArcs}, {"check-uv", ImageCheck::VaryingRG},
		{"check-normals", ImageCheck::VaryingRG}, {"check-flat-normals", ImageCheck::FlatNormals},
		{"check-footprints", ImageCheck::GreenDiagnostic}, {"check-zero-footprints", ImageCheck::RedDiagnostic},
		{"check-parameters", ImageCheck::GreenDiagnostic}, {"check-default-parameters", ImageCheck::RedDiagnostic},
		{"check-material-red", ImageCheck::RedDiagnostic}, {"check-texture-fallback", ImageCheck::GreenDiagnostic}
	    };
	    for (const auto &check : checks)
		if (BU_STR_EQUAL(argv[1], check.name))
		    return check_image(argv[2], std::stoul(argv[3]), std::stoul(argv[4]), check.check);
	}
    } catch (const std::exception &error) {
	bu_log("Image check failed: %s\n", error.what());
	return 1;
    }
    return 1;
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
