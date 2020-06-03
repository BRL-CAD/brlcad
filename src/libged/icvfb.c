/*                        I C V F B . C
 * BRL-CAD
 *
 * Copyright (c) 1998-2020 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 *
 */
/** @file icvfb.c
 *
 * Logic for sending image data to/from framebuffers.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <sys/stat.h>

#include "bu/mime.h"
#include "bu/opt.h"
#include "bu/path.h"
#include "dm.h"
#include "icv.h"
#include "ged.h"
#include "./ged_private.h"

static int
image_mime(struct bu_vls *msg, size_t argc, const char **argv, void *set_mime)
{
    int type_int;
    bu_mime_image_t type = BU_MIME_IMAGE_UNKNOWN;
    bu_mime_image_t *set_type = (bu_mime_image_t *)set_mime;

    BU_OPT_CHECK_ARGV0(msg, argc, argv, "mime format");

    type_int = bu_file_mime(argv[0], BU_MIME_IMAGE);
    type = (type_int < 0) ? BU_MIME_IMAGE_UNKNOWN : (bu_mime_image_t)type_int;
    if (type == BU_MIME_IMAGE_UNKNOWN) {
        if (msg) bu_vls_sprintf(msg, "Error - unknown geometry file type: %s \n", argv[0]);
        return -1;
    }
    if (set_type) (*set_type) = type;
    return 1;
}


int
ged_icv2fb(struct ged *gedp, int argc, const char *argv[])
{
    int ret = GED_OK;

    int multiple_lines = 0;	/* Streamlined operation */

    const char *file_name;

    int print_help = 0;
    int file_xoff = 0;
    int file_yoff = 0;
    int scr_xoff=0;
    int scr_yoff=0;
    int clear = 0;
    int zoom = 0;
    int inverse = 0;		/* Draw upside-down */
    int one_line_only = 0;	/* insist on 1-line writes */
    int verbose = 0;
    int header_only = 0;
    int width = 0; /* may need to specify for some formats (such as PIX) */
    int height = 0; /* may need to specify for some formats (such as PIX) */
    int square = 0; /* may need to specify for some formats (such as PIX) */
    bu_mime_image_t type = BU_MIME_IMAGE_UNKNOWN;

    /* Don't add more gamma, by default. Particularly because on SGI, the
     * system provides gamma correction, so programs don't have to. */
    double def_screen_gamma=1.0;

    static char usage[] = "\
Usage: icv2fb [-h -H -i -c -v -z -1] [-m #lines]\n\
	[-g screen_gamma]\n\
	[-x file_xoff] [-y file_yoff] [-X scr_xoff] [-Y scr_yoff]\n\
	[-S squarescrsize] [--format fmt] [file.img]\n";

    struct bu_opt_desc d[18];
    BU_OPT(d[0],  "h", "help",           "",            NULL,      &print_help,       "Print help and exit");
    BU_OPT(d[1],  "H", "header-only",    "",            NULL,      &header_only,      "header only");
    BU_OPT(d[2],  "i", "inverse",        "",            NULL,      &inverse,          "Draw upside-down");
    BU_OPT(d[3],  "c", "clear",          "",            NULL,      &clear,            "clear");
    BU_OPT(d[4],  "v", "verbose",        "",            NULL,      &verbose,          "verbose");
    BU_OPT(d[5],  "z", "zoom",           "",            NULL,      &zoom,             "zoom");
    BU_OPT(d[6],  "1", "one-line-only",  "",            NULL,      &one_line_only,    "insist on 1-line writes");
    BU_OPT(d[7],  "m", "multiple-lines", "#",    &bu_opt_int,      &multiple_lines,   "multple lines");
    BU_OPT(d[8],  "g", "screen_gamma",   "#",    &bu_opt_fastf_t,  &def_screen_gamma, "gamma");
    BU_OPT(d[9],  "x", "file_xoff",      "#",    &bu_opt_int,      &file_xoff,        "X offset reading from file");
    BU_OPT(d[10], "y", "file_yoff",      "#",    &bu_opt_int,      &file_yoff,        "Y offset reading from file");
    BU_OPT(d[11], "X", "scr_xoff",       "#",    &bu_opt_int,      &scr_xoff,         "X drawing offset in framebuffer");
    BU_OPT(d[12], "Y", "scr_yoff",       "#",    &bu_opt_int,      &scr_yoff,         "Y drawing offset in framebuffer");
    BU_OPT(d[13], "w", "width",          "#",    &bu_opt_int,      &width,            "image width");
    BU_OPT(d[14], "n", "height",         "#",    &bu_opt_int,      &height,           "image height");
    BU_OPT(d[15], "S", "size",           "#",    &bu_opt_int,      &square,           "image width/height (for square image)");
    BU_OPT(d[16], "",  "format",         "fmt",  &image_mime,      &type,             "image file format");
    BU_OPT_NULL(d[17]);

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    if (!gedp->ged_dmp) {
	bu_vls_printf(gedp->ged_result_str, ": no display manager currently active");
	return GED_ERROR;
    }

    struct dm *dmp = (struct dm *)gedp->ged_dmp;
    struct fb *fbp = dm_get_fb(dmp);

    if (!fbp) {
	bu_vls_printf(gedp->ged_result_str, ": display manager does not have a framebuffer");
	return GED_ERROR;
    }

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	_ged_cmd_help(gedp, usage, d);
	return GED_HELP;
    }

    argc-=(argc>0); argv+=(argc>0); /* done with command name argv[0] */

    int opt_ret = bu_opt_parse(NULL, argc, argv, d);

    if (print_help) {
	_ged_cmd_help(gedp, usage, d);
	return GED_HELP;
    }

    argc = opt_ret;

    if (!argc) {
	if (isatty(fileno(stdin))) {
	    bu_vls_printf(gedp->ged_result_str, ": no file specified, expected valid stdin");
	    return GED_ERROR;
	}
	if (type == BU_MIME_IMAGE_UNKNOWN) {
	    bu_vls_printf(gedp->ged_result_str, ": reading from stdin, but no format specified");
	    return GED_ERROR;
	}
	file_name = NULL; 
    } else {

	if (argc > 1) {
	    bu_log("icvfb: Warning: excess argument(s) ignored\n");
	}
	file_name = argv[0];

	/* Find out what input file type we are dealing with */
	if (type == BU_MIME_IMAGE_UNKNOWN) {
	    struct bu_vls c = BU_VLS_INIT_ZERO;
	    if (bu_path_component(&c, file_name, BU_PATH_EXT)) {
		int itype = bu_file_mime(bu_vls_cstr(&c), BU_MIME_IMAGE);
		type = (bu_mime_image_t)itype;
	    } else {
		bu_vls_printf(gedp->ged_result_str, ":  no input file image type specified - need either a specified input image type or a path that provides MIME information.\n");
		bu_vls_free(&c);
		return GED_ERROR;
	    }
	    bu_vls_free(&c);
	}

	// If we're square, let width and height know
	if (square && !width && !height) {
	    width = square;
	    height = square;
	}

	/* If we have no width or height specified, and we have an input format that
	 * does not encode that information, make an educated guess */
	if (!width && !height && (type == BU_MIME_IMAGE_PIX || type == BU_MIME_IMAGE_BW)) {
	    struct stat sbuf;
	    if (stat(file_name, &sbuf) < 0) {
		bu_vls_printf(gedp->ged_result_str, ":  unable to stat input file");
		return GED_ERROR;
	    }
	    unsigned long lwidth, lheight;
	    if (!icv_image_size(NULL, 0, (size_t)sbuf.st_size, type, &lwidth, &lheight)) {
		bu_vls_printf(gedp->ged_result_str, ":  input image type does not have dimension information encoded, and libicv was not able to deduce a size.  Please specify image width in pixels with the \"-w\" option and image height in pixels with the \"-n\" option.\n");
		return GED_ERROR;
	    } else {
		width = (int)lwidth;
		height = (int)lheight;
	    }
	}
    }

    icv_image_t *img = icv_read(file_name, type, width, height);

    if (!img) {
	if (!argc) {
	    bu_vls_printf(gedp->ged_result_str, ":  icv_read failed to read from stdin.\n");
	} else {
	    bu_vls_printf(gedp->ged_result_str, ":  icv_read failed to read %s.\n", file_name);
	}
	return GED_ERROR;
    }

    ret = fb_read_icv(fbp, img,
		      file_xoff, file_yoff,
		      scr_xoff, scr_yoff,
		      clear, zoom, inverse,
		      one_line_only, multiple_lines,
		      gedp->ged_result_str);


    (void)dm_draw_begin(dmp);
    fb_refresh(fbp, 0, 0, fb_getwidth(fbp), fb_getheight(fbp));
    (void)dm_draw_end(dmp);

    return ret;
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
