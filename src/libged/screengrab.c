/*                         S C R E E N G R A B . C
 * BRL-CAD
 *
 * Copyright (c) 2008-2014 United States Government as represented by
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

#include "bu.h"
#include "icv.h"

#include "./ged_private.h"


/* !!! FIXME: this command should not be directly utilizing LIBDM or
 * LIBFB as this breaks library encapsulation.  Generic functionality
 * should be moved out of LIBDM into LIBICV, or be handled by the
 * application logic calling this routine.
 */
int
ged_screen_grab(struct ged *gedp, int argc, const char *argv[])
{

    int i;
    int width = 0;
    int height = 0;
    int bytes_per_pixel = 0;
    int bytes_per_line = 0;
    static const char *usage = "image_name.ext";
    unsigned char **rows = NULL;
    unsigned char *idata = NULL;
    struct icv_image *bif = NULL;	/**< icv image container for saving images */

    if (gedp->ged_dmp_is_null) {
	bu_vls_printf(gedp->ged_result_str, "Bad display pointer.");
	return GED_ERROR;
    }

    if (gedp->ged_dm_get_display_image == NULL) {
	bu_vls_printf(gedp->ged_result_str, "Bad display function pointer.");
	return GED_ERROR;
    }

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_VIEW(gedp, GED_ERROR);
    GED_CHECK_DRAWABLE(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    if (argc != 2) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    width = gedp->ged_dm_width;
    height = gedp->ged_dm_height;

    if (width <= 0 || height <= 0) {
	bu_vls_printf(gedp->ged_result_str, "%s: invalid screen dimensions.", argv[1]);
	return GED_ERROR;
    }

    bytes_per_pixel = 3;
    bytes_per_line = width * bytes_per_pixel;

    /* create image file */

   if ((bif = icv_create(width, height, ICV_COLOR_SPACE_RGB)) == NULL) {
	bu_vls_printf(gedp->ged_result_str, "%s: could not create icv_image write structure.", argv[1]);
	return GED_ERROR;
    }

    rows = (unsigned char **)bu_calloc(height, sizeof(unsigned char *), "rows");

    gedp->ged_dm_get_display_image(gedp, &idata);

    for (i = 0; i < height; ++i) {
	rows[i] = (unsigned char *)(idata + ((height-i-1)*bytes_per_line));
	/* TODO : Add double type data to maintain resolution */
	icv_writeline(bif, i, rows[i], ICV_DATA_UCHAR);
    }

    if (bif != NULL) {
	icv_write(bif, argv[1], ICV_IMAGE_AUTO);
	icv_destroy(bif);
	bif = NULL;
    }

    bu_free(rows, "rows");
    bu_free(idata, "image data");

    return GED_OK;
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
