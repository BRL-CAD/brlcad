/*                         I M G D I F F . C P P
 * BRL-CAD
 *
 * Copyright (c) 2015-2026 United States Government as represented by
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
/** @file imgdiff.cpp
 *
 * Compare two images.
 *
 */

#include "common.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/stat.h>
#include <inttypes.h>

#include "bu.h"
#include "bu/cmdschema.h"
#include "icv.h"

struct imgdiff_args {
    int need_help;
    const char *out_path;
    const char *nirt_prefix;
    int width1;
    int height1;
    int width2;
    int height2;
    bu_mime_image_t in_type_1;
    bu_mime_image_t in_type_2;
    bu_mime_image_t out_type;
    int approx_diff;
    int meta_diff;
};

static int
imgdiff_new_output_parse(struct bu_vls *msg, const char *arg, void *storage)
{
    if (!arg || !arg[0]) {
	if (msg) bu_vls_printf(msg, "output file is required\n");
	return -1;
    }
    if (bu_file_exists(arg, NULL)) {
	if (msg) bu_vls_printf(msg, "output file already exists: %s\n", arg);
	return -1;
    }
    if (storage)
	*(const char **)storage = arg;
    return 0;
}

static int
imgdiff_image_mime_parse(struct bu_vls *msg, const char *arg, void *storage)
{
    int type_int = arg ? bu_file_mime(arg, BU_MIME_IMAGE) : -1;
    bu_mime_image_t type = (type_int < 0) ? BU_MIME_IMAGE_UNKNOWN : (bu_mime_image_t)type_int;
    if (type == BU_MIME_IMAGE_UNKNOWN) {
	if (msg) bu_vls_printf(msg, "unknown image format: %s\n", arg ? arg : "");
	return -1;
    }
    if (storage)
	*(bu_mime_image_t *)storage = type;
    return 0;
}

static const struct bu_cmd_option imgdiff_options[] = {
    BU_CMD_FLAG("h", "help", struct imgdiff_args, need_help, "Print help and exit"),
    BU_CMD_ALIAS_SHORT("?", "help", 1),
    BU_CMD_CUSTOM("o", "output", struct imgdiff_args, out_path, imgdiff_new_output_parse,
	"file", "Output diff image file"),
    BU_CMD_STRING("n", "nirt-output", struct imgdiff_args, nirt_prefix, "prefix",
	"Write NIRT shotline scripts for differing pixels"),
    BU_CMD_INTEGER_VALIDATE(NULL, "width-img1", struct imgdiff_args, width1,
	bu_cmd_nonnegative_integer_validate, "pixels", "Image width of first image"),
    BU_CMD_INTEGER_VALIDATE(NULL, "height-img1", struct imgdiff_args, height1,
	bu_cmd_nonnegative_integer_validate, "pixels", "Image height of first image"),
    BU_CMD_INTEGER_VALIDATE(NULL, "width-img2", struct imgdiff_args, width2,
	bu_cmd_nonnegative_integer_validate, "pixels", "Image width of second image"),
    BU_CMD_INTEGER_VALIDATE(NULL, "height-img2", struct imgdiff_args, height2,
	bu_cmd_nonnegative_integer_validate, "pixels", "Image height of second image"),
    BU_CMD_CUSTOM(NULL, "format-img1", struct imgdiff_args, in_type_1, imgdiff_image_mime_parse,
	"format", "File format of first input file"),
    BU_CMD_CUSTOM(NULL, "format-img2", struct imgdiff_args, in_type_2, imgdiff_image_mime_parse,
	"format", "File format of second input file"),
    BU_CMD_CUSTOM(NULL, "output-format", struct imgdiff_args, out_type, imgdiff_image_mime_parse,
	"format", "File format of output file"),
    BU_CMD_FLAG("A", "approximate", struct imgdiff_args, approx_diff,
	"Calculate approximate difference metric"),
    BU_CMD_FLAG("m", "metadata", struct imgdiff_args, meta_diff,
	"Compare embedded render metadata (PNG only)"),
    BU_CMD_OPTION_NULL
};

static const struct bu_cmd_operand imgdiff_operands[] = {
    BU_CMD_OPERAND("img1", BU_CMD_VALUE_FILE, 1, 1, "First input image", NULL),
    BU_CMD_OPERAND("img2", BU_CMD_VALUE_FILE, 1, 1, "Second input image", NULL),
    BU_CMD_OPERAND("output", BU_CMD_VALUE_FILE, 0, 1, "Optional output image", NULL),
    BU_CMD_OPERAND_NULL
};

static const struct bu_cmd_schema imgdiff_schema = {
    "imgdiff", "Compare two images", imgdiff_options, imgdiff_operands,
    BU_CMD_PARSE_INTERSPERSED, BU_CMD_SCHEMA_CONSTRAINTS(NULL, NULL)
};


int
main(int ac, const char **av)
{
    int uac = 0;
    int ret = 0;
    size_t width1 = 0;
    size_t height1 = 0;
    size_t width2 = 0;
    size_t height2 = 0;
    struct imgdiff_args args = {};
    fastf_t apret = 0;
    const char *in_fmt = NULL;
    const char *out_fmt = NULL;
    bu_mime_image_t in_type_1 = BU_MIME_IMAGE_UNKNOWN;
    bu_mime_image_t in_type_2 = BU_MIME_IMAGE_UNKNOWN;
    bu_mime_image_t out_type = BU_MIME_IMAGE_UNKNOWN;
    const char *out_path = NULL;
    const char *nirt_prefix = NULL;
    int matching = 0;
    int off_by_1 = 0;
    int off_by_many = 0;
    icv_image_t *img1 = NULL;
    icv_image_t *img2 = NULL;
    icv_image_t *oimg;
    const char *img_path_1 = NULL;
    const char *img_path_2 = NULL;
    bu_setprogname(av[0]);

    struct bu_vls slog = BU_VLS_INIT_ZERO;

    const char *diff_usage = "imgdiff [options] img1 img2";
    ac-=(ac>0); av+=(ac>0); /* skip program name argv[0] if present */

    uac = bu_cmd_schema_parse(&imgdiff_schema, &args, &slog, ac, av);

    if (uac == -1) {
	bu_log("Parsing error: %s\n", bu_vls_addr(&slog));
	goto cleanup;
    }

    uac = ac - uac;
    av += ac - uac;
    width1 = (size_t)args.width1;
    height1 = (size_t)args.height1;
    width2 = (size_t)args.width2;
    height2 = (size_t)args.height2;
    in_type_1 = args.in_type_1;
    in_type_2 = args.in_type_2;
    out_type = args.out_type;
    out_path = args.out_path;
    nirt_prefix = args.nirt_prefix;

    /* First, see if help was requested or needed */
    if (uac < 2 || uac > 3 || args.need_help) {
	/* Print help */
	char *help = bu_cmd_schema_describe(&imgdiff_schema);
	bu_log("%s\nOptions:\n", diff_usage);
	bu_log("%s\n", help);
	bu_free(help, "help str");
	goto cleanup;
    }

    img_path_1 = av[uac - 2];
    img_path_2 = av[uac - 1];
    if (uac == 3) {
	img_path_1 = av[uac - 3];
	img_path_2 = av[uac - 2];
	if (out_path) {
	    bu_log("Warning - output path was specified with -o option, not using %s\n", av[uac - 2]);
	} else {
	    out_path = av[uac - 1];
	}
    }

    /* Find out what input file type we are dealing with */
    if (in_type_1 == BU_MIME_IMAGE_UNKNOWN) {
	struct bu_vls c = BU_VLS_INIT_ZERO;
	if (bu_path_component(&c, img_path_1, BU_PATH_EXT)) {
	    in_type_1 = (bu_mime_image_t)bu_file_mime(bu_vls_addr(&c), BU_MIME_IMAGE);
	} else {
	    bu_vls_printf(&slog, "No file image type specified for first image - need either a specified input image type or a path that provides MIME information.\n");
	    ret = 1;
	}
	bu_vls_free(&c);
    }

    if (in_type_2 == BU_MIME_IMAGE_UNKNOWN) {
	struct bu_vls c = BU_VLS_INIT_ZERO;
	if (bu_path_component(&c, img_path_2, BU_PATH_EXT)) {
	    in_type_2 = (bu_mime_image_t)bu_file_mime(bu_vls_addr(&c), BU_MIME_IMAGE);
	} else {
	    bu_vls_printf(&slog, "No file image type specified for second image - need either a specified input image type or a path that provides MIME information.\n");
	    ret = 1;
	}
	bu_vls_free(&c);
    }

    if (out_type == BU_MIME_IMAGE_UNKNOWN) {
	struct bu_vls c = BU_VLS_INIT_ZERO;
	if (out_path) {
	    if (bu_path_component(&c, out_path, BU_PATH_EXT)) {
		out_type = (bu_mime_image_t)bu_file_mime(bu_vls_addr(&c), BU_MIME_IMAGE);
	    } else {
		bu_vls_printf(&slog, "No output file image type specified - need either a specified output image type or a path that provides MIME information.\n");
		ret = 1;
	    }
	}
	bu_vls_free(&c);
    }

    /* If everything isn't OK, we're done - report and clean up memory */
    if (ret == 1) goto cleanup;

    /* If we have no width or height specified, and we have an input format that
     * does not encode that information, make an educated guess */
    if (!width1 && !height1 && (in_type_1 == BU_MIME_IMAGE_PIX || in_type_1 == BU_MIME_IMAGE_BW)) {
	struct stat sbuf;
	if (stat(img_path_1, &sbuf) < 0) {
	    bu_exit(1, "Unable to stat input file");
	}
	if (!icv_image_size(NULL, 0, (size_t)sbuf.st_size, in_type_1, &width1, &height1)) {
	    bu_log("Error - first input image type does not have dimension information encoded, and icv was not able to deduce a size.  Please specify image width in pixels with the \"-width-img1\" option and image height in pixels with the \"-height-img1\" option.\n");
	    bu_exit(1, "image dimensional information insufficient");
	}
    }
    if (!width2 && !height2 && (in_type_2 == BU_MIME_IMAGE_PIX || in_type_2 == BU_MIME_IMAGE_BW)) {
	struct stat sbuf;
	if (stat(img_path_2, &sbuf) < 0) {
	    bu_exit(1, "Unable to stat input file");
	}
	if (!icv_image_size(NULL, 0, (size_t)sbuf.st_size, in_type_2, &width2, &height2)) {
	    bu_log("Error - second input image type does not have dimension information encoded, and icv was not able to deduce a size.  Please specify image width in pixels with the \"-width-img2\" option and image height in pixels with the \"-height-img2\" option.\n");
	    bu_exit(1, "image dimensional information insufficient");
	}
    }

    img1 = icv_read(img_path_1, in_type_1, width1, height1);
    img2 = icv_read(img_path_2, in_type_2, width2, height2);

    if (args.approx_diff) {
	apret = icv_adiff(img1, img2, ICV_DIFF_PHASH);
	bu_log("Hamming distance: %g\n", apret);
	apret = icv_adiff(img1, img2, ICV_DIFF_SSIM);
	bu_log("SSIM: %g\n", apret);
	ret = 0;
	goto cleanup;
    }

    /* Pixel diff */
    ret = icv_diff(&matching, &off_by_1, &off_by_many, img1, img2);
    bu_log("icv_diff channels: %d matching, %d off by 1, %d off by many\n",
	   matching, off_by_1, off_by_many);

    if (out_path && (off_by_1 || off_by_many)) {
	oimg = icv_diffimg(img1, img2);
	icv_write(oimg, out_path, out_type);
	icv_destroy(oimg);
    }

    /* Optional metadata comparison */
    if (args.meta_diff) {
	struct bu_vls meta_report = BU_VLS_INIT_ZERO;
	int mret = icv_diff_render_info(img1, img2, &meta_report);
	if (mret == -1) {
	    bu_log("metadata: neither image contains embedded render metadata\n");
	} else if (mret == 0) {
	    bu_log("metadata: identical\n%s", bu_vls_cstr(&meta_report));
	} else {
	    bu_log("metadata: DIFFERS\n%s", bu_vls_cstr(&meta_report));
	    ret = 1;
	}
	bu_vls_free(&meta_report);
    }

    /* Optional nirt shotline generation */
    if (nirt_prefix) {
	struct bu_vls path1 = BU_VLS_INIT_ZERO;
	struct bu_vls path2 = BU_VLS_INIT_ZERO;
	bu_vls_printf(&path1, "%s_img1.nirt", nirt_prefix);
	bu_vls_printf(&path2, "%s_img2.nirt", nirt_prefix);

	FILE *fp1 = NULL;
	FILE *fp2 = NULL;

	if (img1->render_info) {
	    fp1 = fopen(bu_vls_cstr(&path1), "w");
	    if (!fp1)
		bu_log("ERROR: cannot open '%s' for writing nirt shotlines\n", bu_vls_cstr(&path1));
	}
	if (img2->render_info) {
	    fp2 = fopen(bu_vls_cstr(&path2), "w");
	    if (!fp2)
		bu_log("ERROR: cannot open '%s' for writing nirt shotlines\n", bu_vls_cstr(&path2));
	}

	if (fp1 || fp2) {
	    int nshots = icv_diff_nirt_shots(img1, img2, fp1, fp2);
	    if (fp1) {
		fclose(fp1);
		if (nshots >= 0)
		    bu_log("nirt shotlines (img1): %d differing pixel(s) written to '%s'\n",
			   nshots, bu_vls_cstr(&path1));
	    }
	    if (fp2) {
		fclose(fp2);
		if (nshots >= 0)
		    bu_log("nirt shotlines (img2): %d differing pixel(s) written to '%s'\n",
			   nshots, bu_vls_cstr(&path2));
	    }
	    if (nshots < 0)
		bu_log("nirt shotlines: failed (see above)\n");
	} else {
	    bu_log("nirt shotlines: neither image has embedded render metadata\n");
	}

	bu_vls_free(&path1);
	bu_vls_free(&path2);
    }

    /* Clean up */
cleanup:
    icv_destroy(img1);
    icv_destroy(img2);
    if (bu_vls_strlen(&slog) > 0)
	bu_log("%s", bu_vls_addr(&slog));
    bu_free((char *)in_fmt, "input format string");
    bu_free((char *)out_fmt, "output format string");
    bu_vls_free(&slog);
    return ret;
}


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
