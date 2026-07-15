/*                         O V E R L A Y . C
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
/** @file libged/overlay.c
 *
 * The overlay command.
 *
 */

#include "common.h"
#include <sys/stat.h>

#include "bu/cmdschema.h"
#include "bu/path.h"
#include "bu/mime.h"
#include "bv/vlist.h"
#include "icv.h"
#include "dm.h"

#include "../ged_private.h"

static int
overlay_image_mime(struct bu_vls *msg, const char *arg, void *set_mime)
{
    int type_int;
    bu_mime_image_t type = BU_MIME_IMAGE_UNKNOWN;
    bu_mime_image_t *set_type = (bu_mime_image_t *)set_mime;

    if (!arg || !arg[0]) {
	if (msg)
	    bu_vls_printf(msg, "mime format is required\n");
	return -1;
    }

    type_int = bu_file_mime(arg, BU_MIME_IMAGE);
    type = (type_int < 0) ? BU_MIME_IMAGE_UNKNOWN : (bu_mime_image_t)type_int;
    if (type == BU_MIME_IMAGE_UNKNOWN) {
	if (msg) {
	    bu_vls_sprintf(msg, "Error - unknown geometry file type: %s \n", arg);
	}
	return -1;
    }
    if (set_type) {
	(*set_type) = type;
    }
    return 0;
}


struct overlay_args {
    bu_mime_image_t type;
    fastf_t size;
    const char *view_name;
    int clear;
    int height;
    int inverse;
    int print_help;
    int scr_xoff;
    int scr_yoff;
    int square;
    int verbose;
    int width;
    int write_fb;
    int zoom;
};


static int
overlay_schema_validate(const struct bu_cmd_schema *schema, size_t argc,
	const char **argv, size_t cursor_arg, struct bu_cmd_validate_result *result)
{
    struct bu_cmd_schema flat = *schema;
    int ret;

    flat.validation.custom_validate = NULL;
    ret = bu_cmd_schema_validate(&flat, argc, argv, cursor_arg, result);
    if (ret || result->state == BU_CMD_VALIDATE_INVALID)
	return ret;
    if (!bu_cmd_schema_option_present(schema, argc, argv, "help"))
	return 0;

    bu_cmd_validate_result_clear(result);
    result->state = BU_CMD_VALIDATE_VALID;
    result->token_start = cursor_arg < argc ? cursor_arg : argc;
    result->token_end = result->token_start;
    result->expected = BU_CMD_EXPECT_NONE;
    result->completion_type = BU_CMD_VALUE_FLAG;
    result->hint = "command help";
    return 0;
}


static const struct bu_cmd_option overlay_schema_options[] = {
    BU_CMD_FLAG("h", "help", struct overlay_args, print_help, "Print help and exit"),
    BU_CMD_FLAG("F", "fb", struct overlay_args, write_fb, "Overlay image on framebuffer"),
    BU_CMD_NUMBER("s", "size", struct overlay_args, size, "size",
	"Plot character size"),
    BU_CMD_STRING("N", "view-obj", struct overlay_args, view_name, "name",
	"View object name"),
    BU_CMD_FLAG("i", "inverse", struct overlay_args, inverse, "Draw upside-down"),
    BU_CMD_FLAG("c", "clear", struct overlay_args, clear,
	"Clear framebuffer before drawing"),
    BU_CMD_FLAG("v", "verbose", struct overlay_args, verbose, "Verbose reporting"),
    BU_CMD_FLAG("z", "zoom", struct overlay_args, zoom, "Zoom to fill the screen"),
    BU_CMD_INTEGER("X", "scr_xoff", struct overlay_args, scr_xoff, "offset",
	"Framebuffer X offset"),
    BU_CMD_INTEGER("Y", "scr_yoff", struct overlay_args, scr_yoff, "offset",
	"Framebuffer Y offset"),
    BU_CMD_INTEGER("w", "width", struct overlay_args, width, "pixels", "Image width"),
    BU_CMD_INTEGER("n", "height", struct overlay_args, height, "pixels", "Image height"),
    BU_CMD_INTEGER("S", "square", struct overlay_args, square, "pixels", "Square image size"),
    {NULL, "format", "format", "fmt", "Framebuffer image format", BU_CMD_VALUE_CUSTOM,
	offsetof(struct overlay_args, type), overlay_image_mime, NULL, NULL, NULL, 0, 0,
	NULL, BU_CMD_ARG_REQUIRED, NULL, NULL, NULL},
    BU_CMD_OPTION_NULL
};
static const struct bu_cmd_operand overlay_schema_operands[] = {
    BU_CMD_OPERAND("input_file", BU_CMD_VALUE_FILE, 1, 1, "Plot or image file", "ged.file_path"),
    BU_CMD_OPERAND("view_object", BU_CMD_VALUE_STRING, 0, 1, "Optional view object name", NULL),
    BU_CMD_OPERAND_NULL
};
static const struct bu_cmd_schema overlay_cmd_schema = {
    "overlay", "Overlay plot or image data", overlay_schema_options,
    overlay_schema_operands, BU_CMD_PARSE_INTERSPERSED,
    BU_CMD_SCHEMA_CONSTRAINTS(overlay_schema_validate, NULL)
};


static void
overlay_show_help(struct ged *gedp)
{
    char *option_help = bu_cmd_schema_describe(&overlay_cmd_schema);

    bu_vls_printf(gedp->ged_result_str, "Usage: overlay [options] file [view_object]");
    if (option_help) {
	bu_vls_printf(gedp->ged_result_str, "\nOptions:\n%s", option_help);
	bu_free(option_help, "overlay native option help");
    }
}

int
ged_overlay_core(struct ged *gedp, int argc, const char *argv[])
{
    struct overlay_args args = {BU_MIME_IMAGE_UNKNOWN, 0.0, NULL, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0};
    int operand_count;
    const char **operands;
    int ret = BRLCAD_OK;
    struct dm *dmp = NULL;
    struct fb *fbp = NULL;
    struct bu_vls vname = BU_VLS_INIT_ZERO;
    struct bu_list *vlfree = &rt_vlfree;

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_DRAWABLE(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    if (!gedp->ged_gvp) {
	bu_vls_printf(gedp->ged_result_str, ": no current view set\n");
	bu_vls_free(&vname);
	return BRLCAD_ERROR;
    }

    dmp = (struct dm *)gedp->ged_gvp->dmp;
    if (!dmp) {
	bu_vls_printf(gedp->ged_result_str, ": no display manager currently active");
	bu_vls_free(&vname);
	return BRLCAD_ERROR;
    }

    /* must be wanting help */
    if (argc == 1) {
	overlay_show_help(gedp);
	bu_vls_free(&vname);
	return GED_HELP;
    }

    int operand_index = bu_cmd_schema_parse_complete(&overlay_cmd_schema, &args,
	gedp->ged_result_str, argc - 1, argv + 1);
    if (operand_index < 0) {
	overlay_show_help(gedp);
	bu_vls_free(&vname);
	return BRLCAD_ERROR;
    }
    operand_count = argc - 1 - operand_index;
    operands = argv + operand_index + 1;

    if (args.print_help) {
	overlay_show_help(gedp);
	bu_vls_free(&vname);
	return GED_HELP;
    }

    if (!args.write_fb && NEAR_ZERO(args.size, VUNITIZE_TOL)) {
	if (!gedp->ged_gvp) {
	    bu_vls_printf(gedp->ged_result_str, ": no character size specified, and could not determine default value");
	    bu_vls_free(&vname);
	    return BRLCAD_ERROR;
	}
	args.size = gedp->ged_gvp->gv_scale * 0.01;
    }

    if (args.write_fb) {
	fbp = dm_get_fb(dmp);
	if (!fbp) {
	    bu_vls_printf(gedp->ged_result_str, ": display manager does not have a framebuffer");
	    bu_vls_free(&vname);
	    return BRLCAD_ERROR;
	}
    }

    if (args.view_name)
	bu_vls_sprintf(&vname, "%s", args.view_name);
    if (operand_count == 2)
	bu_vls_sprintf(&vname, "%s", operands[1]);
    if (!bu_vls_strlen(&vname)) {
	bu_vls_sprintf(&vname, "_PLOT_OVERLAY_");
    }

    if (!args.write_fb) {
	struct bv_vlblock*vbp;

	struct bu_vls nroot = BU_VLS_INIT_ZERO;
	if (!BU_STR_EQUAL(bu_vls_cstr(&vname), "_PLOT_OVERLAY_")) {
	    bu_vls_sprintf(&nroot, "overlay::%s", bu_vls_cstr(&vname));
	} else {
	    bu_path_component(&nroot, operands[0], BU_PATH_BASENAME_EXTLESS);
	    bu_vls_simplify(&nroot, NULL, NULL, NULL);
	    bu_vls_prepend(&nroot, "overlay::");
	}

	FILE *fp = fopen(operands[0], "rb");

	/* If we don't have an exact filename match, see if we got a pattern -
	 * it is practical to plot many plot files simultaneously, so that may
	 * be what was specified. */
	if (fp == NULL) {
	    char **files = NULL;
	    size_t count = bu_file_list(".", operands[0], &files);
	    if (count <= 0) {
		bu_vls_printf(gedp->ged_result_str, "ged_overlay_core: failed to open file - %s\n", operands[0]);
		bu_vls_free(&nroot);
		bu_vls_free(&vname);
		return BRLCAD_ERROR;
	    }
	    vbp = bv_vlblock_init(vlfree, 32);
	    for (size_t i = 0; i < count; i++) {
		if ((fp = fopen(files[i], "rb")) == NULL) {
		    bu_vls_printf(gedp->ged_result_str, "ged_overlay_core: failed to open file - %s\n", files[i]);
		    bu_argv_free(count, files);
		    bu_vls_free(&nroot);
		    bu_vls_free(&vname);
		    return BRLCAD_ERROR;
		}
		ret = rt_uplot_to_vlist(vbp, fp, args.size, gedp->i->ged_gdp->gd_uplotOutputMode);
		fclose(fp);
		if (ret < 0) {
		    bv_vlblock_free(vbp);
		    bu_argv_free(count, files);
		    bu_vls_free(&nroot);
		    bu_vls_free(&vname);
		    return BRLCAD_ERROR;
		}
	    }
	    bu_argv_free(count, files);
	} else {
	    vbp = bv_vlblock_init(vlfree, 32);
	    ret = rt_uplot_to_vlist(vbp, fp, args.size, gedp->i->ged_gdp->gd_uplotOutputMode);
	    fclose(fp);
	    if (ret < 0) {
		bv_vlblock_free(vbp);
		bu_vls_free(&nroot);
		bu_vls_free(&vname);
		return BRLCAD_ERROR;
	    }
	}

	if (gedp->new_cmd_forms) {
	    struct bview *v = gedp->ged_gvp;
	    bv_vlblock_obj(vbp, v, bu_vls_cstr(&nroot));
	} else {
	    _ged_cvt_vlblock_to_solids(gedp, vbp, bu_vls_cstr(&vname), 0);
	}

	bv_vlblock_free(vbp);
	bu_vls_free(&nroot);
	bu_vls_free(&vname);

	return BRLCAD_OK;

    } else {

	if (!bu_file_exists(operands[0], NULL)) {
	    bu_vls_printf(gedp->ged_result_str, ": file %s not found", operands[0]);
	    bu_vls_free(&vname);
	    return BRLCAD_ERROR;
	}

	const char *file_name = operands[0];

	/* Find out what input file type we are dealing with */
	if (args.type == BU_MIME_IMAGE_UNKNOWN) {
	    struct bu_vls c = BU_VLS_INIT_ZERO;
	    if (bu_path_component(&c, file_name, BU_PATH_EXT)) {
		int itype = bu_file_mime(bu_vls_cstr(&c), BU_MIME_IMAGE);
		args.type = (bu_mime_image_t)itype;
	    } else {
		bu_vls_printf(gedp->ged_result_str, "no input file image type specified - need either a specified input image type or a path that provides MIME information.\n");
		bu_vls_free(&c);
		bu_vls_free(&vname);
		return BRLCAD_ERROR;
	    }
	    bu_vls_free(&c);
	}

	// If we're square, let width and height know
	if (args.square && !args.width && !args.height) {
	    args.width = args.square;
	    args.height = args.square;
	}

	/* If we have no width or height specified, and we have an input format that
	 * does not encode that information, make an educated guess */
	if (!args.width && !args.height &&
		(args.type == BU_MIME_IMAGE_PIX || args.type == BU_MIME_IMAGE_BW)) {
	    struct stat sbuf;
	    if (stat(file_name, &sbuf) < 0) {
		bu_vls_printf(gedp->ged_result_str, "unable to stat input file");
		bu_vls_free(&vname);
		return BRLCAD_ERROR;
	    }
	    size_t lwidth, lheight;
	    if (!icv_image_size(NULL, 0, (size_t)sbuf.st_size, args.type, &lwidth, &lheight)) {
		bu_vls_printf(gedp->ged_result_str, "input image type does not have dimension information encoded, and libicv was not able to deduce a size.  Please specify image width in pixels with the \"-w\" option and image height in pixels with the \"-n\" option.\n");
		bu_vls_free(&vname);
		return BRLCAD_ERROR;
	    } else {
		args.width = (int)lwidth;
		args.height = (int)lheight;
	    }
	}

	icv_image_t *img = icv_read(file_name, args.type, args.width, args.height);

	if (!img) {
	    if (!operand_count) {
		bu_vls_printf(gedp->ged_result_str, "icv_read failed to read from stdin.\n");
	    } else {
		bu_vls_printf(gedp->ged_result_str, "icv_read failed to read %s.\n", file_name);
	    }
	    icv_destroy(img);
	    bu_vls_free(&vname);
	    return BRLCAD_ERROR;
	}

	ret = fb_read_icv(fbp, img, 0, 0, 0, 0, args.scr_xoff, args.scr_yoff,
		args.clear, args.zoom, args.inverse, 0, 0, gedp->ged_result_str);

	(void)dm_draw_begin(dmp);
	fb_refresh(fbp, 0, 0, fb_getwidth(fbp), fb_getheight(fbp));
	(void)dm_draw_end(dmp);

	icv_destroy(img);
	bu_vls_free(&vname);
	return ret;

    }
}


#include "../include/plugin.h"

#define GED_OVERLAY_COMMANDS(X, XID) \
    X(overlay, ged_overlay_core, GED_CMD_DEFAULT, &overlay_cmd_schema) \

GED_DECLARE_COMMAND_SET_WITH_NATIVE_SCHEMA(GED_OVERLAY_COMMANDS)
GED_DECLARE_PLUGIN_MANIFEST_WITH_NATIVE_SCHEMA("libged_overlay", 1, GED_OVERLAY_COMMANDS)

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
