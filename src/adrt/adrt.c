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

#include "bu/app.h"
#include "bu/getopt.h"
#include "bu/log.h"
#include "bu/malloc.h"
#include "bu/mime.h"
#include "bu/parallel.h"
#include "bu/path.h"
#include "bu/str.h"
#include "bu/time.h"
#include "bu/vls.h"
#include "bn/mat.h"
#include "vmath.h"
#include "icv.h"

#include "rt/tie.h"
#include "adrt.h"
#include "adrt_struct.h"
#include "camera.h"


static void
usage(const char *name)
{
    bu_log("Usage: %s [options] model.g object [object ...]\n", name);
    bu_log("  -o file     output image (extension picks format; default adrt.png)\n");
    bu_log("  -s N        square image size in pixels (default 512)\n");
    bu_log("  -w N        image width  (overrides -s)\n");
    bu_log("  -n N        image height (overrides -s)\n");
    bu_log("  -m mode     shader: path, phong, normal, depth, flat (default path)\n");
    bu_log("  -H N        path-tracer samples per pixel (default 32)\n");
    bu_log("  -a az       view azimuth in degrees (default 35)\n");
    bu_log("  -e el       view elevation in degrees (default 25)\n");
    bu_log("  -E x,y,z    explicit eye point in model units (overrides -a/-e)\n");
    bu_log("  -L x,y,z    explicit look-at point in model units (default: center)\n");
    bu_log("  -p fov      perspective half-angle in degrees (default 25)\n");
    bu_log("  -C r/g/b    environment/sky color, 0-255 (default 230/237/255)\n");
    bu_log("  -P N        number of render threads (default: all cpus)\n");
}


int
main(int argc, char **argv)
{
    struct tie_s tie;
    struct adrt_mesh_s *meshes = NULL;
    render_camera_t camera;
    camera_tile_t tile;
    tienet_buffer_t buffer;
    struct bu_vls shaderbuf = BU_VLS_INIT_ZERO;
    icv_image_t *img;

    const char *output = "adrt.png";
    const char *mode = "path";
    int size = 512, width = 0, height = 0, samples = 32, threads = 0;
    double az = 35.0, el = 25.0, fov = 25.0;
    double sky[3] = { 230.0/255.0, 237.0/255.0, 255.0/255.0 };
    double eye[3], look[3];
    int have_eye = 0, have_look = 0;
    int c;

    vect_t dir;
    fastf_t dist;
    int64_t t_start, t_end;
    size_t i, j;
    uint8_t *px;
    bu_mime_image_t fmt;

    bu_setprogname(argv[0]);

    while ((c = bu_getopt(argc, argv, "o:s:w:n:m:H:a:e:E:L:p:C:P:h?")) != -1) {
	switch (c) {
	    case 'o': output = bu_optarg; break;
	    case 's': size = atoi(bu_optarg); break;
	    case 'w': width = atoi(bu_optarg); break;
	    case 'n': height = atoi(bu_optarg); break;
	    case 'm': mode = bu_optarg; break;
	    case 'H': samples = atoi(bu_optarg); break;
	    case 'a': az = atof(bu_optarg); break;
	    case 'e': el = atof(bu_optarg); break;
	    case 'E':
		if (sscanf(bu_optarg, "%lf,%lf,%lf", &eye[0], &eye[1], &eye[2]) == 3)
		    have_eye = 1;
		break;
	    case 'L':
		if (sscanf(bu_optarg, "%lf,%lf,%lf", &look[0], &look[1], &look[2]) == 3)
		    have_look = 1;
		break;
	    case 'p': fov = atof(bu_optarg); break;
	    case 'P': threads = atoi(bu_optarg); break;
	    case 'C': {
		int r = 0, g = 0, b = 0;
		if (sscanf(bu_optarg, "%d/%d/%d", &r, &g, &b) == 3) {
		    sky[0] = r / 255.0;
		    sky[1] = g / 255.0;
		    sky[2] = b / 255.0;
		}
		break;
	    }
	    default:
		usage(bu_getprogname());
		return (c == 'h' || c == '?') ? EXIT_SUCCESS : EXIT_FAILURE;
	}
    }
    argc -= bu_optind;
    argv += bu_optind;

    if (argc < 2) {
	usage(bu_getprogname());
	return EXIT_FAILURE;
    }

    if (width <= 0)
	width = size;
    if (height <= 0)
	height = size;
    if (samples < 1)
	samples = 1;

    /* Load the requested objects into a freshly prepared TIE engine. */
    memset(&tie, 0, sizeof(tie));
    bu_log("Loading %s ...\n", argv[0]);
    t_start = bu_gettime();
    if (load_g(&tie, argv[0], argc - 1, (const char **)(argv + 1), &meshes) != 0) {
	bu_log("ERROR: failed to load geometry from %s\n", argv[0]);
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
    render_camera_init(&camera, (size_t)(threads > 0 ? threads : bu_avail_cpus()));
    camera.type = RENDER_CAMERA_PERSPECTIVE;
    camera.fov = fov;
    camera.w = (uint16_t)width;
    camera.h = (uint16_t)height;

    /*
     * Position the camera.  load_g scales geometry from millimeters to
     * meters, so tie.mid/tie.radius (and hence explicit eye/look points,
     * which are given in model millimeters) are handled in that space.
     */
    if (have_look) {
	VSCALE(camera.focus, look, 0.001);
    } else {
	VMOVE(camera.focus, tie.mid);
    }
    if (have_eye) {
	VSCALE(camera.pos, eye, 0.001);
    } else {
	/* Frame the model's bounding sphere from the requested az/el. */
	bn_vec_ae(dir, az * DEG2RAD, el * DEG2RAD);
	dist = tie.radius / tan(fov * DEG2RAD) * 1.15;
	VJOIN1(camera.pos, camera.focus, dist, dir);
    }

    /* Select the shader. */
    if (BU_STR_EQUAL(mode, "path")) {
	bu_vls_sprintf(&shaderbuf, "%d,%.4f,%.4f,%.4f",
		       samples, sky[0], sky[1], sky[2]);
	if (render_shader_init(&camera.render, "path", bu_vls_cstr(&shaderbuf)) != 0)
	    return EXIT_FAILURE;
    } else {
	if (render_shader_init(&camera.render, mode, NULL) != 0)
	    return EXIT_FAILURE;
    }

    render_camera_prep(&camera);

    /* One tile covering the whole image, 24-bit RGB. */
    tile.orig_x = 0;
    tile.orig_y = 0;
    tile.size_x = (uint16_t)width;
    tile.size_y = (uint16_t)height;
    tile.format = RENDER_CAMERA_BIT_DEPTH_24;
    tile.frame = 0;

    TIENET_BUFFER_INIT(buffer);

    bu_log("Rendering %dx%d, mode=%s%s ...\n", width, height, mode,
	   BU_STR_EQUAL(mode, "path") ? "" : "");
    t_start = bu_gettime();
    render_camera_render(&camera, &tie, &tile, &buffer);
    t_end = bu_gettime();
    bu_log("Rendered in %.2f s\n", (t_end - t_start) / 1.0e6);

    /* Copy the 24-bit RGB tile into an icv image and write it out.  The
     * renderer produces scanlines top-to-bottom; icv's origin is the lower
     * left, so flip vertically as we copy. */
    px = buffer.data + sizeof(camera_tile_t);
    img = icv_create((size_t)width, (size_t)height, ICV_COLOR_SPACE_RGB);
    for (j = 0; j < (size_t)height; j++) {
	size_t src = j * (size_t)width * 3;
	size_t dst = ((size_t)height - 1 - j) * (size_t)width * 3;
	for (i = 0; i < (size_t)width * 3; i++)
	    img->data[dst + i] = px[src + i] / 255.0;
    }

    fmt = BU_MIME_IMAGE_PNG;
    {
	struct bu_vls ext = BU_VLS_INIT_ZERO;
	if (bu_path_component(&ext, output, BU_PATH_EXT)) {
	    int t = bu_file_mime(bu_vls_cstr(&ext), BU_MIME_IMAGE);
	    if (t >= 0)
		fmt = (bu_mime_image_t)t;
	}
	bu_vls_free(&ext);
    }

    if (icv_write(img, output, fmt) != 0) {
	bu_log("ERROR: failed to write %s\n", output);
	icv_destroy(img);
	return EXIT_FAILURE;
    }
    bu_log("Wrote %s\n", output);

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
