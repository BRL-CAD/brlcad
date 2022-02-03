/*                         P N G . C
 * BRL-CAD
 *
 * Copyright (c) 2008-2022 United States Government as represented by
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
/** @file libged/png.c
 *
 * The png command.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include "png.h"


#include "bu/getopt.h"
#include "vmath.h"
#include "bn.h"


#include "../ged_private.h"


static unsigned int size = 512;
static unsigned int half_size = 256;

static unsigned char bg_red = 255;
static unsigned char bg_green = 255;
static unsigned char bg_blue = 255;

static int
draw_png(struct ged *gedp, FILE *fp)
{
    long i;
    png_structp png_p;
    png_infop info_p;
    double out_gamma = 1.0;

    /* TODO: explain why this is size+1 */
    size_t num_bytes_per_row = (size+1) * 3;
    size_t num_bytes = num_bytes_per_row * (size+1);
    unsigned char **image = (unsigned char **)bu_malloc(sizeof(unsigned char *) * (size+1), "draw_png, image");
    unsigned char *bytes = (unsigned char *)bu_malloc(num_bytes, "draw_png, bytes");

    /* Initialize bytes using the background color */
    if (bg_red == bg_green && bg_red == bg_blue)
	memset((void *)bytes, bg_red, num_bytes);
    else {
	for (i = 0; (size_t)i < num_bytes; i += 3) {
	    bytes[i] = bg_red;
	    bytes[i+1] = bg_green;
	    bytes[i+2] = bg_blue;
	}
    }

    png_p = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png_p) {
	bu_vls_printf(gedp->ged_result_str, "Could not create PNG write structure\n");
	return BRLCAD_ERROR;
    }

    info_p = png_create_info_struct(png_p);
    if (!info_p) {
	bu_vls_printf(gedp->ged_result_str, "Could not create PNG info structure\n");
	bu_free((void *)image, "draw_png, image");
	bu_free((void *)bytes, "draw_png, bytes");

	return BRLCAD_ERROR;
    }

    png_init_io(png_p, fp);
    png_set_filter(png_p, 0, PNG_FILTER_NONE);
    png_set_compression_level(png_p, 9);
    png_set_IHDR(png_p, info_p,
		 size, size, 8,
		 PNG_COLOR_TYPE_RGB,
		 PNG_INTERLACE_NONE,
		 PNG_COMPRESSION_TYPE_DEFAULT,
		 PNG_FILTER_TYPE_DEFAULT);
    png_set_gAMA(png_p, info_p, out_gamma);
    png_write_info(png_p, info_p);

    /* Arrange the rows of data/pixels */
    for (i = size; i >= 0; --i) {
	image[i] = (unsigned char *)(bytes + ((size-i) * num_bytes_per_row));
    }

    dl_png(gedp->ged_gdp->gd_headDisplay, gedp->ged_gvp->gv_model2view, gedp->ged_gvp->gv_perspective, gedp->ged_gvp->gv_eye_pos, (size_t)size, (size_t)half_size, image);

    /* Write out pixels */
    png_write_image(png_p, image);
    png_write_end(png_p, NULL);

    bu_free((void *)image, "draw_png, image");
    bu_free((void *)bytes, "draw_png, bytes");

    return BRLCAD_OK;
}


int
ged_png_core(struct ged *gedp, int argc, const char *argv[])
{
    FILE *fp;
    int k;
    int ret;
    int r, g, b;
    static const char *usage = "[-c r/g/b] [-s size] file";

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_VIEW(gedp, BRLCAD_ERROR);
    GED_CHECK_DRAWABLE(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_HELP;
    }

    /* Process options */
    bu_optind = 1;
    while ((k = bu_getopt(argc, (char * const *)argv, "c:s:")) != -1) {
	switch (k) {
	    case 'c':
		/* parse out a delimited rgb color value */
		if (sscanf(bu_optarg, "%d%*c%d%*c%d", &r, &g, &b) != 3) {
		    bu_vls_printf(gedp->ged_result_str, "%s: bad color - %s", argv[0], bu_optarg);
		    return BRLCAD_ERROR;
		}

		/* Clamp color values */
		if (r < 0)
		    r = 0;
		else if (r > 255)
		    r = 255;

		if (g < 0)
		    g = 0;
		else if (g > 255)
		    g = 255;

		if (b < 0)
		    b = 0;
		else if (b > 255)
		    b = 255;

		bg_red = (unsigned char)r;
		bg_green = (unsigned char)g;
		bg_blue = (unsigned char)b;

		break;
	    case 's':
		if (sscanf(bu_optarg, "%u", &size) != 1) {
		    bu_vls_printf(gedp->ged_result_str, "%s: bad size - %s", argv[0], bu_optarg);
		    return BRLCAD_ERROR;
		}

		if (size < 50) {
		    bu_vls_printf(gedp->ged_result_str, "%s: bad size - %s, must be greater than or equal to 50\n", argv[0], bu_optarg);
		    return BRLCAD_ERROR;
		}

		half_size = size * 0.5;

		break;
	    default:
		bu_vls_printf(gedp->ged_result_str, "%s: Unrecognized option - %s", argv[0], argv[bu_optind-1]);
		return BRLCAD_ERROR;
	}
    }

    if ((argc - bu_optind) != 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    fp = fopen(argv[bu_optind], "wb");
    if (fp == NULL) {
	bu_vls_printf(gedp->ged_result_str, "%s: Error opening file - %s\n", argv[0], argv[bu_optind]);
	return BRLCAD_ERROR;
    }

    ret = draw_png(gedp, fp);
    fclose(fp);

    return ret;
}


#ifdef GED_PLUGIN
#include "../include/plugin.h"
struct ged_cmd_impl png_cmd_impl = {"png", ged_png_core, GED_CMD_DEFAULT};
const struct ged_cmd png_cmd = { &png_cmd_impl };

struct ged_cmd_impl pngwf_cmd_impl = {"pngwf", ged_png_core, GED_CMD_DEFAULT};
const struct ged_cmd pngwf_cmd = { &pngwf_cmd_impl };

const struct ged_cmd *png_cmds[] = { &png_cmd, &pngwf_cmd, NULL };

static const struct ged_plugin pinfo = { GED_API,  png_cmds, 2 };

COMPILER_DLLEXPORT const struct ged_plugin *ged_plugin_info()
{
    return &pinfo;
}
#endif /* GED_PLUGIN */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
