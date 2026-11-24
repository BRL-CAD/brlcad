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
#include "bu/arg.h"
#include "icv.h"

struct imgdiff_args {
    int print_help;
    const char *output_path;
    const char *nirt_prefix;
    int width1;
    int height1;
    int width2;
    int height2;
    bu_mime_image_t input_type1;
    bu_mime_image_t input_type2;
    bu_mime_image_t output_type;
    int approximate;
    int metadata;
};

static int
output_path_parse(struct bu_vls *msg, const char *arg, void *storage)
{
    if (bu_file_exists(arg, NULL)) {
	if (msg)
	    bu_vls_sprintf(msg, "output file %s already exists\n", arg);
	return -1;
    }

    if (storage)
	*(const char **)storage = arg;

    return 0;
}

static int
image_mime_parse(struct bu_vls *msg, const char *arg, void *storage)
{
    int type_int = bu_file_mime(arg, BU_MIME_IMAGE);
    bu_mime_image_t type = (type_int < 0) ? BU_MIME_IMAGE_UNKNOWN : (bu_mime_image_t)type_int;
    if (type == BU_MIME_IMAGE_UNKNOWN) {
	if (msg)
	    bu_vls_sprintf(msg, "unknown image file type: %s\n", arg);
	return -1;
    }
    if (storage)
	*(bu_mime_image_t *)storage = type;
    return 0;
}

static const struct bu_arg_option imgdiff_options[] = {
    BU_ARG_FLAG("h", "help", struct imgdiff_args, print_help, "Print help and exit"),
    BU_ARG_ALIAS("?", NULL, "help", 1),
    BU_ARG_CUSTOM("o", "output", struct imgdiff_args, output_path, output_path_parse,
	"file", "Output diff image file"),
    BU_ARG_VALUE("n", "nirt-output", struct imgdiff_args, nirt_prefix,
	BU_ARG_VALUE_STRING, BU_ARG_REQUIRED, "prefix",
	"Write nirt shotline scripts for differing pixels"),
    BU_ARG_INTEGER_RANGE(NULL, "width-img1", struct imgdiff_args, width1,
	1, 0, 0, 0, "pixels", "Image width of first image"),
    BU_ARG_INTEGER_RANGE(NULL, "height-img1", struct imgdiff_args, height1,
	1, 0, 0, 0, "pixels", "Image height of first image"),
    BU_ARG_INTEGER_RANGE(NULL, "width-img2", struct imgdiff_args, width2,
	1, 0, 0, 0, "pixels", "Image width of second image"),
    BU_ARG_INTEGER_RANGE(NULL, "height-img2", struct imgdiff_args, height2,
	1, 0, 0, 0, "pixels", "Image height of second image"),
    BU_ARG_CUSTOM(NULL, "format-img1", struct imgdiff_args, input_type1,
	image_mime_parse, "format", "File format of first input file"),
    BU_ARG_CUSTOM(NULL, "format-img2", struct imgdiff_args, input_type2,
	image_mime_parse, "format", "File format of second input file"),
    BU_ARG_CUSTOM(NULL, "output-format", struct imgdiff_args, output_type,
	image_mime_parse, "format", "File format of output file"),
    BU_ARG_FLAG("A", "approximate", struct imgdiff_args, approximate,
	"Calculate approximate difference metric"),
    BU_ARG_FLAG("m", "metadata", struct imgdiff_args, metadata,
	"Compare embedded render metadata (PNG only)"),
    BU_ARG_OPTION_NULL
};

static const struct bu_arg_operand imgdiff_operands[] = {
    BU_ARG_OPERAND("img1", BU_ARG_VALUE_FILE, 1, 1, "First image", NULL),
    BU_ARG_OPERAND("img2", BU_ARG_VALUE_FILE, 1, 1, "Second image", NULL),
    BU_ARG_OPERAND("output", BU_ARG_VALUE_FILE, 0, 1, "Output diff image", NULL),
    BU_ARG_OPERAND_NULL
};

static const struct bu_arg_schema imgdiff_schema = BU_ARG_SCHEMA(
    "imgdiff", "Compare two images and report their differences", imgdiff_options,
    imgdiff_operands, BU_ARG_PARSE_INTERSPERSED,
    BU_ARG_SCHEMA_META_HELP(NULL, NULL, NULL, NULL, NULL));

static void
imgdiff_help(void)
{
    char *help = bu_arg_schema_help(&imgdiff_schema, bu_getprogname());
    if (help)
	bu_log("%s", help);
    bu_free(help, "help str");
}


int
main(int ac, const char **av)
{
    const struct bu_arg_grammar grammar = {&imgdiff_schema, NULL, NULL};
    struct imgdiff_args args = {};
    int ret = 0;
    size_t width1 = 0;
    size_t height1 = 0;
    size_t width2 = 0;
    size_t height2 = 0;
    int &approx_diff = args.approximate;
    int &meta_diff = args.metadata;
    fastf_t apret = 0;
    const char *in_fmt = NULL;
    const char *out_fmt = NULL;
    bu_mime_image_t &in_type_1 = args.input_type1;
    bu_mime_image_t &in_type_2 = args.input_type2;
    bu_mime_image_t &out_type = args.output_type;
    const char *&out_path = args.output_path;
    const char *&nirt_prefix = args.nirt_prefix;
    int matching = 0;
    int off_by_1 = 0;
    int off_by_many = 0;
    icv_image_t *img1 = NULL;
    icv_image_t *img2 = NULL;
    icv_image_t *oimg;
    const char *img_path_1 = NULL;
    const char *img_path_2 = NULL;
    bu_setprogname(av[0]);

    struct bu_vls parse_msgs = BU_VLS_INIT_ZERO;
    struct bu_vls slog = BU_VLS_INIT_ZERO;

    int schema_status = bu_arg_introspect(&parse_msgs, &grammar, ac - 1,
	av + 1);
    if (schema_status != 0) {
	bu_flog(schema_status > 0 ? stdout : stderr, "%s", bu_vls_cstr(&parse_msgs));
	bu_vls_free(&parse_msgs);
	bu_vls_free(&slog);
	return schema_status > 0 ? 0 : 1;
    }

    ac-=(ac>0); av+=(ac>0); /* skip program name argv[0] if present */

    int operand_index = bu_arg_schema_parse_complete(&imgdiff_schema, &args,
	&parse_msgs, ac, av);
    if (operand_index < 0) {
	bu_log("Parsing error: %s", bu_vls_addr(&parse_msgs));
	ret = 1;
	goto cleanup;
    }

    if (args.print_help) {
	imgdiff_help();
	goto cleanup;
    }

    ac -= operand_index;
    av += operand_index;
    img_path_1 = av[0];
    img_path_2 = av[1];
    if (ac == 3) {
	if (out_path) {
	    bu_log("Warning - output path was specified with -o option, not using %s\n", av[2]);
	} else {
	    out_path = av[2];
	}
    }

    width1 = (size_t)args.width1;
    height1 = (size_t)args.height1;
    width2 = (size_t)args.width2;
    height2 = (size_t)args.height2;

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

    if (approx_diff) {
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
    if (meta_diff) {
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
    bu_vls_free(&parse_msgs);
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
