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

#include "bu/arg.h"
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


static int
overlay_format_validate(struct bu_vls *msg, const char *arg)
{
    return overlay_image_mime(msg, arg, NULL);
}


struct overlay_args {
    bu_mime_image_t type;
    const char *format;
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


static const struct bu_arg_option overlay_options[] = {
    BU_ARG_FLAG("h", "help", struct overlay_args, print_help,
	"Print help and exit"),
    BU_ARG_FLAG("F", "fb", struct overlay_args, write_fb,
	"Overlay image on framebuffer"),
    BU_ARG_VALUE("s", "size", struct overlay_args, size, BU_ARG_VALUE_NUMBER,
	BU_ARG_REQUIRED, "size", "Plot character size"),
    {"N", "view-obj", "view-obj", "name", "View object name",
	BU_ARG_VALUE_STRING, BU_ARG_STORAGE_OFFSET(struct overlay_args, view_name),
	NULL, NULL, "ged.view", NULL, 0, 0, NULL, BU_ARG_REQUIRED, NULL, NULL,
	NULL, BU_ARG_VALUE_RANGE_NONE, BU_ARG_SHAPE_SCALAR},
    BU_ARG_FLAG("i", "inverse", struct overlay_args, inverse, "Draw upside-down"),
    BU_ARG_FLAG("c", "clear", struct overlay_args, clear,
	"Clear framebuffer before drawing"),
    BU_ARG_FLAG("v", "verbose", struct overlay_args, verbose, "Verbose reporting"),
    BU_ARG_FLAG("z", "zoom", struct overlay_args, zoom, "Zoom to fill the screen"),
    BU_ARG_VALUE("X", "scr_xoff", struct overlay_args, scr_xoff,
	BU_ARG_VALUE_INTEGER, BU_ARG_REQUIRED, "offset", "Framebuffer X offset"),
    BU_ARG_VALUE("Y", "scr_yoff", struct overlay_args, scr_yoff,
	BU_ARG_VALUE_INTEGER, BU_ARG_REQUIRED, "offset", "Framebuffer Y offset"),
    BU_ARG_VALUE("w", "width", struct overlay_args, width,
	BU_ARG_VALUE_INTEGER, BU_ARG_REQUIRED, "pixels", "Image width"),
    BU_ARG_VALUE("n", "height", struct overlay_args, height,
	BU_ARG_VALUE_INTEGER, BU_ARG_REQUIRED, "pixels", "Image height"),
    BU_ARG_VALUE("S", "square", struct overlay_args, square,
	BU_ARG_VALUE_INTEGER, BU_ARG_REQUIRED, "pixels", "Square image size"),
    BU_ARG_VALUE_VALIDATE(NULL, "format", struct overlay_args, format,
	BU_ARG_VALUE_STRING, overlay_format_validate, "fmt",
	"Framebuffer image format"),
    BU_ARG_OPTION_NULL
};
static const struct bu_arg_operand overlay_help_operands[] = {
    BU_ARG_OPERAND("raw_arguments", BU_ARG_VALUE_RAW, 0,
	BU_ARG_COUNT_UNLIMITED, "Arguments ignored when requesting help", NULL),
    BU_ARG_OPERAND_NULL
};
static const struct bu_arg_operand overlay_operands[] = {
    BU_ARG_OPERAND("input_file", BU_ARG_VALUE_FILE, 1, 1,
	"Input plot or image file", "ged.file_path"),
    BU_ARG_OPERAND("view_object", BU_ARG_VALUE_STRING, 0, 1,
	"Optional view object name", "ged.view"),
    BU_ARG_OPERAND_NULL
};
static const char * const overlay_help_option[] = {"help", NULL};
static const struct bu_arg_schema_case overlay_cases[] = {
    BU_ARG_SCHEMA_CASE("help", "Display command help",
	BU_ARG_CONDITION_ANY_OPTION_PRESENT, overlay_help_option,
	overlay_help_operands, NULL),
    BU_ARG_SCHEMA_CASE_DEFAULT("input",
	"Read an input file and optional view object", overlay_operands, NULL),
    BU_ARG_SCHEMA_CASE_NULL
};
static const struct bu_arg_schema overlay_cmd_schema =
    BU_ARG_SCHEMA_CASED("overlay", "Overlay plot or image data",
	overlay_options, BU_ARG_PARSE_INTERSPERSED, overlay_cases);


static void
overlay_show_help(struct ged *gedp)
{
    char *help = ged_cmd_help("overlay", "overlay");

    if (help) {
	bu_vls_strcat(gedp->ged_result_str, help);
	bu_free(help, "overlay standard help");
    }
}

int
ged_overlay_core(struct ged *gedp, int argc, const char *argv[])
{
    struct overlay_args args = {BU_MIME_IMAGE_UNKNOWN, NULL, 0.0, NULL, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0};
    int operand_count;
    int operand_index;
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

    argc--; argv++;
    operand_index = bu_arg_schema_parse_complete(&overlay_cmd_schema, &args,
	gedp->ged_result_str, argc, argv);
    if (operand_index < 0) {
	overlay_show_help(gedp);
	bu_vls_free(&vname);
	return BRLCAD_ERROR;
    }
    operand_count = argc - operand_index;
    operands = argv + operand_index;

    if (args.print_help) {
	overlay_show_help(gedp);
	bu_vls_free(&vname);
	return GED_HELP;
    }

    if (args.format)
	(void)overlay_image_mime(NULL, args.format, &args.type);

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
