/*                          A D R T . C
 * BRL-CAD / ADRT
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
/** @file adrt.c
 *
 * Standalone, headless front end to the ADRT rendering library.  It loads a
 * BRL-CAD geometry database into the TIE ray-tracing engine, renders a single
 * frame with one of libadrt's shaders (the Monte-Carlo "path" tracer by
 * default), and writes the result to an image file.
 *
 * Unlike adrt_master / adrt_slave (which form a networked, observer-driven
 * renderer) this program does everything in one process and needs no GUI,
 * which makes it convenient for scripted and documentation rendering.
 *
 * Example:
 *   adrt -s 512 -H 128 -a 35 -e 25 -o jeep.png toyjeep.g toyjeep
 */

#include "common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <limits.h>

#include "bu/app.h"
#include "bu/arg.h"
#include "bu/color.h"
#include "bu/log.h"
#include "bu/malloc.h"
#include "bu/mime.h"
#include "bu/parallel.h"
#include "bu/path.h"
#include "bu/str.h"
#include "bu/datetime.h"
#include "bu/vls.h"
#include "bn/mat.h"
#include "vmath.h"
#include "icv.h"

#include "rt/tie.h"
#include "adrt.h"
#include "adrt_struct.h"
#include "camera.h"


struct adrt_vector_arg {
    point_t value;
    int set;
};

struct adrt_args {
    int help;
    const char *output;
    int size;
    int width;
    int height;
    const char *mode;
    int samples;
    const char *shader_arg;
    double spall_angle;
    double azimuth;
    double elevation;
    struct adrt_vector_arg eye;
    struct adrt_vector_arg look;
    double fov;
    struct bu_color sky;
    int threads;
};

static int
adrt_vector_consume(struct bu_vls *msg, size_t argc, const char **argv,
	void *storage)
{
    struct adrt_vector_arg *vector_arg = (struct adrt_vector_arg *)storage;

    if (bu_arg_vector3_consume(msg, argc, argv,
	    vector_arg ? vector_arg->value : NULL) < 0)
	return -1;
    if (vector_arg)
	vector_arg->set = 1;
    return 0;
}

static const struct bu_arg_value_keyword adrt_modes[] = {
    {"component", NULL, "Component-color visualization"},
    {"cut", NULL, "Cutaway visualization"},
    {"depth", NULL, "Depth visualization"},
    {"flat", NULL, "Flat shading"},
    {"flos", NULL, "Flow-line-of-sight visualization"},
    {"grid", NULL, "Grid visualization"},
    {"normal", NULL, "Surface-normal visualization"},
    {"path", NULL, "Monte Carlo path tracing"},
    {"phong", NULL, "Phong shading"},
    {"spall", NULL, "Spall-cone visualization"},
    {"surfel", NULL, "Surface-element visualization"},
    {NULL, NULL, NULL}
};

static const struct bu_arg_variant adrt_vector_variants[] = {
    BU_ARG_VARIANT("packed", "x/y/z", 1, "One packed XYZ vector"),
    BU_ARG_VARIANT("components", "x y z", 3, "Three XYZ components"),
    BU_ARG_VARIANT_NULL
};

static const struct bu_arg_shape adrt_vector_shape = BU_ARG_SHAPE(
    BU_ARG_SHAPE_VECTOR3, 1, 3, "packed XYZ or three components", NULL,
    adrt_vector_variants);

static const struct bu_arg_option adrt_options[] = {
    BU_ARG_FLAG("h", "help", struct adrt_args, help,
	"Print help and exit"),
    BU_ARG_ALIAS("?", NULL, "help", 1),
    BU_ARG_VALUE("o", "output", struct adrt_args, output,
	BU_ARG_VALUE_FILE, BU_ARG_REQUIRED, "file",
	"Write the rendered image to this file"),
    BU_ARG_INTEGER_RANGE("s", "size", struct adrt_args, size,
	1, 1, 1, UINT16_MAX, "pixels", "Set both image dimensions"),
    BU_ARG_INTEGER_RANGE("w", "width", struct adrt_args, width,
	1, 1, 1, UINT16_MAX, "pixels",
	"Set the image width, overriding --size"),
    BU_ARG_INTEGER_RANGE("n", "height", struct adrt_args, height,
	1, 1, 1, UINT16_MAX, "pixels",
	"Set the image height, overriding --size"),
    BU_ARG_KEYWORD_VALUES("m", "mode", struct adrt_args, mode, "mode",
	"Select the rendering shader", adrt_modes),
    BU_ARG_INTEGER_RANGE("H", "samples", struct adrt_args, samples,
	1, 1, 1, INT_MAX, "count", "Set path-tracer samples per pixel"),
    BU_ARG_VALUE("b", "shader-argument", struct adrt_args, shader_arg,
	BU_ARG_VALUE_STRING, BU_ARG_REQUIRED, "string",
	"Pass an initialization string to the selected shader"),
    BU_ARG_VALUE_VALIDATE("A", "spall-angle", struct adrt_args,
	spall_angle, BU_ARG_VALUE_NUMBER, bu_arg_positive_number_validate,
	"degrees", "Set the full spall-cone angle"),
    BU_ARG_VALUE("a", "azimuth", struct adrt_args, azimuth,
	BU_ARG_VALUE_NUMBER, BU_ARG_REQUIRED, "degrees", "Set the view azimuth"),
    BU_ARG_VALUE("e", "elevation", struct adrt_args, elevation,
	BU_ARG_VALUE_NUMBER, BU_ARG_REQUIRED, "degrees", "Set the view elevation"),
    BU_ARG_OPTION_SHAPED("E", "eye", "eye", struct adrt_args, eye,
	BU_ARG_VALUE_VECTOR, "x/y/z", "Set the eye point in model units",
	BU_ARG_REQUIRED, &adrt_vector_shape, adrt_vector_consume),
    BU_ARG_OPTION_SHAPED("L", "look-at", "look-at", struct adrt_args,
	look, BU_ARG_VALUE_VECTOR, "x/y/z", "Set the look-at point in model units",
	BU_ARG_REQUIRED, &adrt_vector_shape, adrt_vector_consume),
    BU_ARG_VALUE_VALIDATE("p", "field-of-view", struct adrt_args, fov,
	BU_ARG_VALUE_NUMBER, bu_arg_positive_number_validate, "degrees",
	"Set the perspective half-angle"),
    BU_ARG_RGB("C", "sky-color", struct adrt_args, sky, "r/g/b",
	"Set the environment color"),
    BU_ARG_INTEGER_RANGE("P", "threads", struct adrt_args, threads,
	1, 1, 1, UINT8_MAX, "count", "Set the rendering thread count"),
    BU_ARG_OPTION_NULL
};

static const struct bu_arg_operand adrt_operands[] = {
    BU_ARG_OPERAND("model.g", BU_ARG_VALUE_FILE, 1, 1,
	"Input BRL-CAD database", NULL),
    BU_ARG_OPERAND("object", BU_ARG_VALUE_DB_OBJECT, 1,
	BU_ARG_COUNT_UNLIMITED, "Object to render", NULL),
    BU_ARG_OPERAND_NULL
};

static const struct bu_arg_schema adrt_schema = BU_ARG_SCHEMA(
    "adrt", "Render BRL-CAD geometry with the ADRT triangle ray tracer",
    adrt_options, adrt_operands, BU_ARG_PARSE_INTERSPERSED,
    BU_ARG_SCHEMA_META_HELP(NULL, NULL, NULL, NULL, NULL));

static const struct bu_arg_grammar adrt_grammar = {
    &adrt_schema, NULL, NULL
};


int
main(int argc, char **argv)
{
    static const unsigned char default_sky[] = {230, 237, 255};
    struct tie_s tie;
    struct adrt_mesh_s *meshes = NULL;
    render_camera_t camera;
    camera_tile_t tile;
    tienet_buffer_t buffer;
    struct bu_vls shaderbuf = BU_VLS_INIT_ZERO;
    icv_image_t *img;

    struct adrt_args args = {0};
    struct bu_vls msg = BU_VLS_INIT_ZERO;
    fastf_t sky[3];
    int introspection;
    int operand_index;

    vect_t dir;
    fastf_t dist;
    int64_t t_start, t_end;
    size_t i, j;
    uint8_t *px;
    bu_mime_image_t fmt;

    bu_setprogname(argv[0]);
    args.output = "adrt.png";
    args.size = 512;
    args.mode = "path";
    args.samples = 32;
    args.spall_angle = 30.0;
    args.azimuth = 35.0;
    args.elevation = 25.0;
    args.fov = 25.0;
    (void)bu_color_from_rgb_chars(&args.sky, default_sky);

    introspection = bu_arg_introspect(&msg, &adrt_grammar, argc - 1,
	(const char * const *)(argv + 1));
    if (introspection != 0) {
	bu_flog(introspection > 0 ? stdout : stderr, "%s", bu_vls_cstr(&msg));
	bu_vls_free(&msg);
	return introspection > 0 ? EXIT_SUCCESS : EXIT_FAILURE;
    }
    operand_index = bu_arg_schema_parse_complete(&adrt_schema, &args, &msg,
	argc - 1, (const char **)(argv + 1));
    if (operand_index < 0) {
	bu_flog(stderr, "%s", bu_vls_cstr(&msg));
	bu_vls_free(&msg);
	return EXIT_FAILURE;
    }
    bu_vls_free(&msg);
    if (args.help) {
	char *help = bu_arg_schema_help(&adrt_schema, "adrt");
	bu_flog(stdout, "%s", help ? help : "");
	bu_free(help, "adrt help");
	return EXIT_SUCCESS;
    }
    if (args.fov >= 90.0) {
	bu_flog(stderr, "field-of-view must be less than 90 degrees\n");
	return EXIT_FAILURE;
    }
    if (!args.width)
	args.width = args.size;
    if (!args.height)
	args.height = args.size;
    if (!bu_color_to_rgb_floats(&args.sky, sky)) {
	bu_flog(stderr, "failed to convert sky color\n");
	return EXIT_FAILURE;
    }

    /* Load the requested objects into a freshly prepared TIE engine. */
    memset(&tie, 0, sizeof(tie));
    bu_log("Loading %s ...\n", argv[operand_index + 1]);
    t_start = bu_gettime();
    if (load_g(&tie, argv[operand_index + 1], argc - operand_index - 2,
	    (const char **)(argv + operand_index + 2), &meshes) != 0) {
	bu_log("ERROR: failed to load geometry from %s\n", argv[operand_index + 1]);
	return EXIT_FAILURE;
    }
    t_end = bu_gettime();
    bu_log("Loaded %u triangles in %.2f s\n",
	   tie.tri_num, (t_end - t_start) / 1.0e6);

    if (tie.tri_num == 0) {
	bu_log("ERROR: no renderable geometry found (only BoT/NMG and\n");
	bu_log("       tessellatable solids are supported by adrt)\n");
	return EXIT_FAILURE;
    }

    /* Configure the camera. */
    render_camera_init(&camera, (size_t)(args.threads > 0 ?
	    (size_t)args.threads : bu_avail_cpus()));
    camera.type = RENDER_CAMERA_PERSPECTIVE;
    camera.fov = args.fov;
    camera.w = (uint16_t)args.width;
    camera.h = (uint16_t)args.height;

    /*
     * Position the camera.  load_g scales geometry from millimeters to
     * meters, so tie.mid/tie.radius (and hence explicit eye/look points,
     * which are given in model millimeters) are handled in that space.
     */
    if (args.look.set) {
	VSCALE(camera.focus, args.look.value, 0.001);
    } else {
	VMOVE(camera.focus, tie.mid);
    }
    if (args.eye.set) {
	VSCALE(camera.pos, args.eye.value, 0.001);
    } else {
	/* Frame the model's bounding sphere from the requested az/el. */
	bn_vec_ae(dir, args.azimuth * DEG2RAD, args.elevation * DEG2RAD);
	dist = tie.radius / tan(args.fov * DEG2RAD) * 1.15;
	VJOIN1(camera.pos, camera.focus, dist, dir);
    }

    /* Select the shader. */
    if (BU_STR_EQUAL(args.mode, "path")) {
	bu_vls_sprintf(&shaderbuf, "%d,%.4f,%.4f,%.4f",
		       args.samples, sky[0], sky[1], sky[2]);
	if (render_shader_init(&camera.render, "path", bu_vls_cstr(&shaderbuf)) != 0)
	    return EXIT_FAILURE;
    } else if (BU_STR_EQUAL(args.mode, "spall")) {
	/*
	 * The spall shader wants "(pos) (dir) angle".  Use an explicit -b
	 * string if given; otherwise synthesize a shot from the current view
	 * through the model center so the cutaway cone is visible.  Coordinates
	 * are in the tie's (meter) space.
	 */
	if (args.shader_arg) {
	    if (render_shader_init(&camera.render, "spall", args.shader_arg) != 0)
		return EXIT_FAILURE;
	} else {
	    /* Fire the shot horizontally across the model (90 degrees off the
	     * view azimuth) so the cone is seen from the side rather than down
	     * its axis. */
	    vect_t sdir;
	    bn_vec_ae(sdir, (args.azimuth + 90.0) * DEG2RAD, 0.0);
	    bu_vls_sprintf(&shaderbuf, "(%.6f %.6f %.6f) (%.6f %.6f %.6f) %f",
			   V3ARGS(camera.focus), V3ARGS(sdir), args.spall_angle);
	    if (render_shader_init(&camera.render, "spall", bu_vls_cstr(&shaderbuf)) != 0)
		return EXIT_FAILURE;
	}
    } else {
	/* shader_arg may be NULL; shaders that need an argument (cut, flos)
	 * accept one via -b. */
	if (render_shader_init(&camera.render, args.mode, args.shader_arg) != 0)
	    return EXIT_FAILURE;
    }

    render_camera_prep(&camera);

    /* One tile covering the whole image, 24-bit RGB. */
    tile.orig_x = 0;
    tile.orig_y = 0;
    tile.size_x = (uint16_t)args.width;
    tile.size_y = (uint16_t)args.height;
    tile.format = RENDER_CAMERA_BIT_DEPTH_24;
    tile.frame = 0;

    TIENET_BUFFER_INIT(buffer);

    bu_log("Rendering %dx%d, mode=%s ...\n", args.width, args.height, args.mode);
    t_start = bu_gettime();
    render_camera_render(&camera, &tie, &tile, &buffer);
    t_end = bu_gettime();
    bu_log("Rendered in %.2f s\n", (t_end - t_start) / 1.0e6);

    /* Copy the 24-bit RGB tile into an icv image and write it out.  The
     * renderer produces scanlines top-to-bottom; icv's origin is the lower
     * left, so flip vertically as we copy. */
    px = buffer.data + sizeof(camera_tile_t);
    img = icv_create((size_t)args.width, (size_t)args.height, ICV_COLOR_SPACE_RGB);
    for (j = 0; j < (size_t)args.height; j++) {
	size_t src = j * (size_t)args.width * 3;
	size_t dst = ((size_t)args.height - 1 - j) * (size_t)args.width * 3;
	for (i = 0; i < (size_t)args.width * 3; i++)
	    img->data[dst + i] = px[src + i] / 255.0;
    }

    fmt = BU_MIME_IMAGE_PNG;
    {
	struct bu_vls ext = BU_VLS_INIT_ZERO;
	if (bu_path_component(&ext, args.output, BU_PATH_EXT)) {
	    int t = bu_file_mime(bu_vls_cstr(&ext), BU_MIME_IMAGE);
	    if (t >= 0)
		fmt = (bu_mime_image_t)t;
	}
	bu_vls_free(&ext);
    }

    if (icv_write(img, args.output, fmt) != 0) {
	bu_log("ERROR: failed to write %s\n", args.output);
	icv_destroy(img);
	return EXIT_FAILURE;
    }
    bu_log("Wrote %s\n", args.output);

    icv_destroy(img);
    TIENET_BUFFER_FREE(buffer);
    bu_vls_free(&shaderbuf);

    return EXIT_SUCCESS;
}

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
