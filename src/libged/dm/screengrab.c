/*                         S C R E E N G R A B . C
 * BRL-CAD
 *
 * Copyright (c) 2008-2023 United States Government as represented by
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
/** @file libged/screengrab.c
 *
 * The screengrab command.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include "icv.h"
#include "dm.h"

#include "../ged_private.h"

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
	if (msg) {
	    bu_vls_sprintf(msg, "Error - unknown geometry file type: %s \n", argv[0]);
	}
	return -1;
    }
    if (set_type) {
	(*set_type) = type;
    }
    return 1;
}

int
ged_screen_grab_core(struct ged *gedp, int argc, const char *argv[])
{
    struct dm *dmp = NULL;
    int i;
    int print_help = 0;
    int bytes_per_pixel = 0;
    int bytes_per_line = 0;
    int grab_fb = 0;
    unsigned char **rows = NULL;
    unsigned char *idata = NULL;
    struct icv_image *bif = NULL;	/**< icv image container for saving images */
    struct fb *fbp = NULL;
    struct bu_vls dm_name = BU_VLS_INIT_ZERO;
    bu_mime_image_t type = BU_MIME_IMAGE_AUTO;
    static char usage[] = "Usage: screengrab [-h] [-F] [-D name] [--format fmt] [file.img]\n";

    struct bu_opt_desc d[5];
    BU_OPT(d[0], "h", "help",           "",     NULL,             &print_help,       "Print help and exit");
    BU_OPT(d[1], "F", "fb",             "",     NULL,             &grab_fb,          "screengrab framebuffer instead of scene display");
    BU_OPT(d[2], "D", "dm",             "name", &bu_opt_vls,      &dm_name,          "name of DM to screengrab");
    BU_OPT(d[3], "",  "format",         "fmt",  &image_mime,      &type,             "output image file format");
    BU_OPT_NULL(d[4]);

    GED_CHECK_VIEW(gedp, BRLCAD_ERROR);
    GED_CHECK_DRAWABLE(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    argc-=(argc>0); argv+=(argc>0); /* done with command name argv[0] */

    int opt_ret = bu_opt_parse(NULL, argc, argv, d);

    if (print_help) {
	_ged_cmd_help(gedp, usage, d);
	return GED_HELP;
    }

    argc = opt_ret;

    struct dm *cdmp = (gedp->ged_gvp) ? (struct dm *)gedp->ged_gvp->dmp : NULL;

    if (bu_vls_strlen(&dm_name) && gedp->ged_gvp) {
	// We have a name - see if we can match it.
	struct bu_ptbl *views = bv_set_views(&gedp->ged_views);
	for (size_t j = 0; j < BU_PTBL_LEN(views); j++) {
	    if (dmp)
		break;
	    struct bview *gdvp = (struct bview *)BU_PTBL_GET(views, j);
	    struct dm *ndmp = (struct dm *)gdvp->dmp;
	    if (!bu_vls_strcmp(dm_get_pathname(ndmp), &dm_name))
		dmp = ndmp;
	}
	if (!dmp) {
	    bu_vls_sprintf(gedp->ged_result_str, "DM %s specified, but not found in active set\n", bu_vls_cstr(&dm_name));
	    bu_vls_free(&dm_name);
	    return BRLCAD_ERROR;
	}
    }
    bu_vls_free(&dm_name);

    if (!dmp)
	dmp = cdmp;

    if (!dmp) {
	bu_vls_printf(gedp->ged_result_str, ": no current display manager set and no valid name specified\n");
	return BRLCAD_ERROR;
    }

    if (grab_fb) {
	fbp = dm_get_fb(dmp);
	if (!fbp) {
	    bu_vls_printf(gedp->ged_result_str, ": display manager does not have a framebuffer");
	    return BRLCAD_ERROR;
	}
    }

    /* must be wanting help */
    if (!argc) {
	_ged_cmd_help(gedp, usage, d);
	return GED_HELP;
    }

    /* create image file */
    if (!grab_fb) {

	bytes_per_pixel = 3;
	bytes_per_line = dm_get_width(dmp) * bytes_per_pixel;

	dm_get_display_image(dmp, &idata, 1, 0);
	if (!idata) {
	    bu_vls_printf(gedp->ged_result_str, "%s: display manager did not return image data.", argv[1]);
	    return BRLCAD_ERROR;
	}
	bif = icv_create(dm_get_width(dmp), dm_get_height(dmp), ICV_COLOR_SPACE_RGB);
	if (bif == NULL) {
	    bu_vls_printf(gedp->ged_result_str, ": could not create icv_image write structure.");
	    return BRLCAD_ERROR;
	}
	rows = (unsigned char **)bu_calloc(dm_get_height(dmp), sizeof(unsigned char *), "rows");
	for (i = 0; i < dm_get_height(dmp); ++i) {
	    rows[i] = (unsigned char *)(idata + ((dm_get_height(dmp)-i-1)*bytes_per_line));
	    /* TODO : Add double type data to maintain resolution */
	    icv_writeline(bif, i, rows[i], ICV_DATA_UCHAR);
	}
	bu_free(rows, "rows");
	bu_free(idata, "image data");

    } else {
	bif = fb_write_icv(fbp, 0, 0, fb_getwidth(fbp), fb_getheight(fbp));
	if (bif == NULL) {
	    bu_vls_printf(gedp->ged_result_str, ": could not create icv_image from framebuffer.");
	    return BRLCAD_ERROR;
	}
    }

    icv_write(bif, argv[0], type);
    icv_destroy(bif);

    return BRLCAD_OK;
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
