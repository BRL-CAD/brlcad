/*                        A S C I I . C
 * BRL-CAD
 *
 * Copyright (c) 2016-2025 United States Government as represented by
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
/** @file ascii.c
 *
 * Test outputting ASCII txt from images.
 *
 */

#include "common.h"

#include <stdlib.h>

#include "bio.h"
#include "bu/app.h"
#include "bu/log.h"
#include "bu/malloc.h"
#include "bu/opt.h"
#include "bu/vls.h"
#include "icv.h"

void
_print_help(struct bu_opt_desc *d)
{
    struct bu_vls str = BU_VLS_INIT_ZERO;
    char *option_help;

    bu_vls_sprintf(&str, "icv_ascii [options] input_file.png");

    if ((option_help = bu_opt_describe(d, NULL))) {
	bu_vls_printf(&str, "Options:\n%s\n", option_help);
	bu_free(option_help, "help str");
    }

    bu_log("%s\n", bu_vls_cstr(&str));
    bu_vls_free(&str);
}


int main(int argc, const char **argv)
{
    int print_help = 0;
    struct bu_vls char_set = BU_VLS_INIT_ZERO;
    struct icv_ascii_art_params artparams = ICV_ASCII_ART_PARAMS_DEFAULT;
    fastf_t aspect_ratio = 1.0;

    struct bu_opt_desc d[7];
    BU_OPT(d[0], "h", "help",          "",                     NULL,   &print_help,                      "Print help and exit");
    BU_OPT(d[1], "?", "",              "",                     NULL,   &print_help,                      "");
    BU_OPT(d[2], "c", "colorize",      "",                     NULL,   &artparams.output_color,          "Use color when outputting text");
    BU_OPT(d[3], "I", "invert-color",  "",                     NULL,   &artparams.invert_color,          "Invert colors");
    BU_OPT(d[4], "b", "brightness",    "#",         &bu_opt_fastf_t,   &artparams.brightness_multiplier, "Scale image brightness by this factor");
    BU_OPT(d[5], "a", "aspect-ratio",  "char_set",  &bu_opt_fastf_t,   &aspect_ratio,                    "Adjust aspect ratio (used to correct for distortion of character width/height ratios - alters incoming image before conversion to let output look better on terminal.)");
    BU_OPT_NULL(d[6]);

    // Record program name
    bu_setprogname(argv[0]);

    argc-=(argc>0); argv+=(argc>0); /* done with command name argv[0] */

    /* parse standard options */
    int opt_ret = bu_opt_parse(NULL, argc, argv, d);

    argc = opt_ret;

    if (print_help) {
	_print_help(d);
	bu_vls_free(&char_set);
	return BRLCAD_ERROR;
    }

    if (!argc) {
	/* must be wanting help */
	_print_help(d);
	return BRLCAD_ERROR;
    }

    icv_image_t *bif = icv_read(argv[0], BU_MIME_IMAGE_AUTO, 0, 0);

    if (!bif) {
	bu_log("Unable to read in image %s\n", argv[0]);
	bu_vls_free(&char_set);
	return BRLCAD_ERROR;
    }

    if (!NEAR_EQUAL(aspect_ratio, 1.0, VUNITIZE_TOL)) {
	size_t width = bif->width;
	size_t height = bif->height;
	// Resize image - apparently need to do a preliminary resize before we can set
	// just the height??
	icv_resize(bif, ICV_RESIZE_BINTERP, width/aspect_ratio, height/aspect_ratio, 0);
	icv_resize(bif, ICV_RESIZE_BINTERP, width, height/aspect_ratio, 0);
    }

    char *ascii_art = icv_ascii_art(bif, &artparams);

    bu_log("%s\n", ascii_art);

    bu_free(ascii_art, "ascii_art");

    icv_destroy(bif);

    return BRLCAD_OK;
}

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
