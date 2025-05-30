/*                         O V E R L A Y . C
 * BRL-CAD
 *
 * Copyright (c) 2008-2025 United States Government as represented by
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

#include "bu/path.h"
#include "bu/mime.h"
#include "bv/vlist.h"
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
ged_overlay_core(struct ged *gedp, int argc, const char *argv[])
{
    bu_mime_image_t type = BU_MIME_IMAGE_UNKNOWN;
    double size = 0.0;
    int clear = 0;
    int height = 0;  /* may need to specify for some formats (such as PIX) */
    int inverse = 0;
    int print_help = 0;
    int ret = BRLCAD_OK;
    int scr_xoff=0;
    int scr_yoff=0;
    int square = 0; /* may need to specify for some formats (such as PIX) */
    int verbose = 0;
    int width = 0; /* may need to specify for some formats (such as PIX) */
    int write_fb = 0;
    int zoom = 0;
    struct dm *dmp = NULL;
    struct fb *fbp = NULL;
    struct bu_vls vname = BU_VLS_INIT_ZERO;
    struct bu_list *vlfree = &rt_vlfree;

    static char usage[] = "Usage: overlay [options] file\n";

    struct bu_opt_desc d[15];
    BU_OPT(d[0],  "h", "help",           "",     NULL,            &print_help,       "Print help and exit");
    BU_OPT(d[1],  "F", "fb",             "",     NULL,            &write_fb,         "Overlay image on framebuffer");
    BU_OPT(d[2],  "s", "size",           "#",    &bu_opt_fastf_t, &size,             "[Plot] Character size for plot drawing");
    BU_OPT(d[3],  "N", "view-obj",       "name", &bu_opt_vls,     &vname,            "[Plot] Name of view object");
    BU_OPT(d[4],  "i", "inverse",        "",     NULL,            &inverse,          "[Fb]   Draw upside-down");
    BU_OPT(d[5],  "c", "clear",          "",     NULL,            &clear,            "[Fb]   Clear framebuffer before drawing");
    BU_OPT(d[6],  "v", "verbose",        "",     NULL,            &verbose,          "[Fb]   Verbose reporting");
    BU_OPT(d[7],  "z", "zoom",           "",     NULL,            &zoom,             "[Fb]   Zoom image to fill screen");
    BU_OPT(d[8],  "X", "scr_xoff",       "#",    &bu_opt_int,     &scr_xoff,         "[Fb]   X drawing offset in framebuffer");
    BU_OPT(d[9],  "Y", "scr_yoff",       "#",    &bu_opt_int,     &scr_yoff,         "[Fb]   Y drawing offset in framebuffer");
    BU_OPT(d[10],  "w", "width",          "#",    &bu_opt_int,     &width,            "[Fb]   image width");
    BU_OPT(d[11], "n", "height",         "#",    &bu_opt_int,     &height,           "[Fb]   image height");
    BU_OPT(d[12], "S", "square",         "#",    &bu_opt_int,     &square,           "[Fb]   image width/height (for square image)");
    BU_OPT(d[13], "",  "format",         "fmt",  &image_mime,     &type,             "[Fb]   image file format");
    BU_OPT_NULL(d[14]);

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
	_ged_cmd_help(gedp, usage, d);
	bu_vls_free(&vname);
	return GED_HELP;
    }

    argc-=(argc>0); argv+=(argc>0); /* done with command name argv[0] */

    int opt_ret = bu_opt_parse(NULL, argc, argv, d);

    if (print_help) {
	_ged_cmd_help(gedp, usage, d);
	bu_vls_free(&vname);
	return GED_HELP;
    }

    if (!write_fb && NEAR_ZERO(size, VUNITIZE_TOL)) {
	if (!gedp->ged_gvp) {
	    bu_vls_printf(gedp->ged_result_str, ": no character size specified, and could not determine default value");
	    bu_vls_free(&vname);
	    return BRLCAD_ERROR;
	}
	size = gedp->ged_gvp->gv_scale * 0.01;
    }

    argc = opt_ret;

    if (write_fb) {
	fbp = dm_get_fb(dmp);
	if (!fbp) {
	    bu_vls_printf(gedp->ged_result_str, ": display manager does not have a framebuffer");
	    bu_vls_free(&vname);
	    return BRLCAD_ERROR;
	}
    }

    /* must be wanting help */
    if (!argc) {
	_ged_cmd_help(gedp, usage, d);
	bu_vls_free(&vname);
	return GED_HELP;
    }
    /* check arg cnt */
    if (argc > 2) {
	_ged_cmd_help(gedp, usage, d);
	bu_vls_free(&vname);
	return GED_HELP;
    }

    /* Second arg, if present, is view obj name */
    if (argc == 2) {
	bu_vls_sprintf(&vname, "%s", argv[1]);
    } else {
	if (!bu_vls_strlen(&vname))
	    bu_vls_sprintf(&vname, "_PLOT_OVERLAY_");
    }

    if (!write_fb) {
	struct bv_vlblock*vbp;

	struct bu_vls nroot = BU_VLS_INIT_ZERO;
	if (!BU_STR_EQUAL(bu_vls_cstr(&vname), "_PLOT_OVERLAY_")) {
	    bu_vls_sprintf(&nroot, "overlay::%s", bu_vls_cstr(&vname));
	} else {
	    bu_path_component(&nroot, argv[0], BU_PATH_BASENAME_EXTLESS);
	    bu_vls_simplify(&nroot, NULL, NULL, NULL);
	    bu_vls_prepend(&nroot, "overlay::");
	}

	FILE *fp = fopen(argv[0], "rb");

	/* If we don't have an exact filename match, see if we got a pattern -
	 * it is practical to plot many plot files simultaneously, so that may
	 * be what was specified. */
	if (fp == NULL) {
	    char **files = NULL;
	    size_t count = bu_file_list(".", argv[0], &files);
	    if (count <= 0) {
		bu_vls_printf(gedp->ged_result_str, "ged_overlay_core: failed to open file - %s\n", argv[1]);
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
		ret = rt_uplot_to_vlist(vbp, fp, size, gedp->i->ged_gdp->gd_uplotOutputMode);
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
	    ret = rt_uplot_to_vlist(vbp, fp, size, gedp->i->ged_gdp->gd_uplotOutputMode);
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

	if (!bu_file_exists(argv[0], NULL)) {
	    bu_vls_printf(gedp->ged_result_str, ": file %s not found", argv[0]);
	    bu_vls_free(&vname);
	    return BRLCAD_ERROR;
	}

	const char *file_name = argv[0];

	/* Find out what input file type we are dealing with */
	if (type == BU_MIME_IMAGE_UNKNOWN) {
	    struct bu_vls c = BU_VLS_INIT_ZERO;
	    if (bu_path_component(&c, file_name, BU_PATH_EXT)) {
		int itype = bu_file_mime(bu_vls_cstr(&c), BU_MIME_IMAGE);
		type = (bu_mime_image_t)itype;
	    } else {
		bu_vls_printf(gedp->ged_result_str, "no input file image type specified - need either a specified input image type or a path that provides MIME information.\n");
		bu_vls_free(&c);
		bu_vls_free(&vname);
		return BRLCAD_ERROR;
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
		bu_vls_printf(gedp->ged_result_str, "unable to stat input file");
		bu_vls_free(&vname);
		return BRLCAD_ERROR;
	    }
	    size_t lwidth, lheight;
	    if (!icv_image_size(NULL, 0, (size_t)sbuf.st_size, type, &lwidth, &lheight)) {
		bu_vls_printf(gedp->ged_result_str, "input image type does not have dimension information encoded, and libicv was not able to deduce a size.  Please specify image width in pixels with the \"-w\" option and image height in pixels with the \"-n\" option.\n");
		bu_vls_free(&vname);
		return BRLCAD_ERROR;
	    } else {
		width = (int)lwidth;
		height = (int)lheight;
	    }
	}

	icv_image_t *img = icv_read(file_name, type, width, height);

	if (!img) {
	    if (!argc) {
		bu_vls_printf(gedp->ged_result_str, "icv_read failed to read from stdin.\n");
	    } else {
		bu_vls_printf(gedp->ged_result_str, "icv_read failed to read %s.\n", file_name);
	    }
	    icv_destroy(img);
	    bu_vls_free(&vname);
	    return BRLCAD_ERROR;
	}

	ret = fb_read_icv(fbp, img, 0, 0, 0, 0,	scr_xoff, scr_yoff, clear, zoom, inverse, 0, 0, gedp->ged_result_str);

	(void)dm_draw_begin(dmp);
	fb_refresh(fbp, 0, 0, fb_getwidth(fbp), fb_getheight(fbp));
	(void)dm_draw_end(dmp);

	icv_destroy(img);
	bu_vls_free(&vname);
	return ret;

    }
}


#ifdef GED_PLUGIN
#include "../include/plugin.h"
struct ged_cmd_impl overlay_cmd_impl = {
    "overlay",
    ged_overlay_core,
    GED_CMD_DEFAULT
};

const struct ged_cmd overlay_cmd = { &overlay_cmd_impl };
const struct ged_cmd *overlay_cmds[] = { &overlay_cmd, NULL };

static const struct ged_plugin pinfo = { GED_API,  overlay_cmds, 1 };

COMPILER_DLLEXPORT const struct ged_plugin *ged_plugin_info(void)
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
