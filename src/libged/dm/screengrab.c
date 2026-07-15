/*                         S C R E E N G R A B . C
 * BRL-CAD
 *
 * Copyright (c) 2008-2026 United States Government as represented by
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
#include "bu/cmdschema.h"
#include "icv.h"
#include "dm.h"

#include "../ged_private.h"

struct screengrab_args {
    int print_help;
    int grab_fb;
    struct bu_vls dm_name;
    bu_mime_image_t type;
};

static int
screengrab_image_mime_parse(struct bu_vls *msg, const char *arg, void *set_mime)
{
    int type_int;
    bu_mime_image_t type = BU_MIME_IMAGE_UNKNOWN;
    bu_mime_image_t *set_type = (bu_mime_image_t *)set_mime;

    if (!arg || !arg[0]) {
	if (msg)
	    bu_vls_printf(msg, "image format is required\n");
	return -1;
    }

    type_int = bu_file_mime(arg, BU_MIME_IMAGE);
    type = (type_int < 0) ? BU_MIME_IMAGE_UNKNOWN : (bu_mime_image_t)type_int;
    if (type == BU_MIME_IMAGE_UNKNOWN) {
	if (msg) {
	    bu_vls_sprintf(msg, "Error - unknown image file type: %s\n", arg);
	}
	return -1;
    }
    if (set_type) {
	(*set_type) = type;
    }
    return 0;
}

static const struct bu_cmd_option screengrab_schema_options[] = {
    BU_CMD_FLAG("h", "help", struct screengrab_args, print_help,
	"Print command help"),
    BU_CMD_FLAG("F", "fb", struct screengrab_args, grab_fb,
	"Capture the framebuffer instead of the scene display"),
    BU_CMD_VLS_APPEND("D", "dm", struct screengrab_args, dm_name, "name",
	"Display manager to capture"),
    BU_CMD_CUSTOM(NULL, "format", struct screengrab_args, type,
	screengrab_image_mime_parse, "format", "Output image format"),
    BU_CMD_OPTION_NULL
};
static const struct bu_cmd_operand screengrab_schema_operands[] = {
    BU_CMD_OPERAND("output_file", BU_CMD_VALUE_FILE, 0, 1,
	"Optional image output file", "ged.file_path"),
    BU_CMD_OPERAND_NULL
};
const struct bu_cmd_schema ged_screengrab_schema = {
    "screengrab", "Capture a display manager image", screengrab_schema_options,
    screengrab_schema_operands, BU_CMD_PARSE_INTERSPERSED,
    BU_CMD_SCHEMA_CONSTRAINTS(NULL, NULL)
};
const struct bu_cmd_schema ged_screen_grab_schema = {
    "screen_grab", "Capture a display manager image", screengrab_schema_options,
    screengrab_schema_operands, BU_CMD_PARSE_INTERSPERSED,
    BU_CMD_SCHEMA_CONSTRAINTS(NULL, NULL)
};

static void
screengrab_show_help(struct ged *gedp, const char *command)
{
    char *help = bu_cmd_schema_describe(&ged_screengrab_schema);

    bu_vls_printf(gedp->ged_result_str,
	"Usage: %s [-F] [-D name] [--format format] image_file", command);
    if (help) {
	bu_vls_printf(gedp->ged_result_str, "\nOptions:\n%s", help);
	bu_free(help, "screengrab native help");
    }
}

int
ged_screen_grab_core(struct ged *gedp, int argc, const char *argv[])
{
    struct dm *dmp = NULL;
    int i;
    int operand_index;
    int ret = BRLCAD_ERROR;
    int bytes_per_pixel = 0;
    int bytes_per_line = 0;
    unsigned char **rows = NULL;
    unsigned char *idata = NULL;
    struct icv_image *bif = NULL;	/**< icv image container for saving images */
    struct fb *fbp = NULL;
    const char *output_file = NULL;
    struct screengrab_args args = {0, 0, BU_VLS_INIT_ZERO, BU_MIME_IMAGE_AUTO};

    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    operand_index = bu_cmd_schema_parse_complete(&ged_screengrab_schema, &args,
	gedp->ged_result_str, argc - 1, argv + 1);
    if (operand_index < 0) {
	screengrab_show_help(gedp, argv[0]);
	goto done;
    }
    if (args.print_help || operand_index == argc - 1) {
	screengrab_show_help(gedp, argv[0]);
	ret = GED_HELP;
	goto done;
    }
    output_file = argv[operand_index + 1];

    if (gedp->ged_gvp == GED_VIEW_NULL) {
	bu_vls_trunc(gedp->ged_result_str, 0);
	bu_vls_printf(gedp->ged_result_str, "A view does not exist.");
	goto done;
    }
    if (!ged_dl(gedp)) {
	bu_vls_trunc(gedp->ged_result_str, 0);
	bu_vls_printf(gedp->ged_result_str, "A drawable does not exist.");
	goto done;
    }

    struct dm *cdmp = (gedp->ged_gvp) ? (struct dm *)gedp->ged_gvp->dmp : NULL;


    if (bu_vls_strlen(&args.dm_name) && gedp->ged_gvp) {
	// We have a name - see if we can match it.
	struct bu_ptbl *views = bv_set_views(&gedp->ged_views);
	for (size_t j = 0; j < BU_PTBL_LEN(views); j++) {
	    if (dmp)
		break;
	    struct bview *gdvp = (struct bview *)BU_PTBL_GET(views, j);
	    struct dm *ndmp = (struct dm *)gdvp->dmp;
	    if (!bu_vls_strcmp(dm_get_pathname(ndmp), &args.dm_name))
		dmp = ndmp;
	}
	if (!dmp) {
	    bu_vls_sprintf(gedp->ged_result_str, "DM %s specified, but not found in active set\n", bu_vls_cstr(&args.dm_name));
	    goto done;
	}
    }

    if (!dmp)
	dmp = cdmp;

    if (!dmp) {
	bu_vls_printf(gedp->ged_result_str, ": no current display manager set and no valid name specified\n");
	goto done;
    }


    if (args.grab_fb) {
	fbp = dm_get_fb(dmp);
	if (!fbp) {
	    bu_vls_printf(gedp->ged_result_str, ": display manager does not have a framebuffer");
	    goto done;
	}
    }

    /* create image file */
    if (!args.grab_fb) {

	bytes_per_pixel = 3;
	bytes_per_line = dm_get_width(dmp) * bytes_per_pixel;

	dm_get_display_image(dmp, &idata, 1, 0);
	if (!idata) {
	    bu_vls_printf(gedp->ged_result_str, "%s: display manager did not return image data.", output_file);
	    goto done;
	}
	bif = icv_create(dm_get_width(dmp), dm_get_height(dmp), ICV_COLOR_SPACE_RGB);
	if (bif == NULL) {
	    bu_vls_printf(gedp->ged_result_str, ": could not create icv_image write structure.");
	    goto done;
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
	    goto done;
	}
    }

    icv_write(bif, output_file, args.type);
    icv_destroy(bif);

    ret = BRLCAD_OK;
done:
    bu_vls_free(&args.dm_name);
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
