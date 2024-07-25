/*                         I M G D I F F . C P P
 * BRL-CAD
 *
 * Copyright (c) 2015-2024 United States Government as represented by
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
#include "icv.h"

int
file_null(struct bu_vls *msg, size_t argc, const char **argv, void *set_var)
{
    char **file_set = (char **)set_var;

    BU_OPT_CHECK_ARGV0(msg, argc, argv, "output file");

    if (bu_file_exists(argv[0], NULL)){
	if (msg) bu_vls_sprintf(msg, "Error - file %s already exists!\n", argv[0]);
	return -1;
    }

    if (file_set) (*file_set) = bu_strdup(argv[0]);

    return 1;
}

int
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
main(int ac, const char **av)
{
    int uac = 0;
    int ret = 0;
    size_t width1 = 0;
    size_t height1 = 0;
    size_t width2 = 0;
    size_t height2 = 0;
    int approx_diff = 0;
    uint32_t pret = 0;
    const char *in_fmt = NULL;
    const char *out_fmt = NULL;
    static bu_mime_image_t in_type_1 = BU_MIME_IMAGE_UNKNOWN;
    static bu_mime_image_t in_type_2 = BU_MIME_IMAGE_UNKNOWN;
    static bu_mime_image_t out_type = BU_MIME_IMAGE_UNKNOWN;
    const char *out_path = NULL;
    int need_help = 0;
    int matching = 0;
    int off_by_1 = 0;
    int off_by_many = 0;
    icv_image_t *img1, *img2, *oimg;
    const char *img_path_1 = NULL;
    const char *img_path_2 = NULL;
    bu_setprogname(av[0]);

    struct bu_vls parse_msgs = BU_VLS_INIT_ZERO;
    struct bu_vls slog = BU_VLS_INIT_ZERO;

    const char *diff_usage = "imgdiff [options] img1 img2";
    struct bu_opt_desc icv_opt_desc[] = {
	{"h", "help",             "",           NULL,         &need_help,             "Print help and exit."                      },
	{"?", "",                 "",           NULL,         &need_help,             "",                                         },
	{"o", "output",           "file",       &file_null,   (void *)&out_path,      "Output file.",                             },
	{"",  "width-img1",       "#",          &bu_opt_int,  (void *)&width1,        "Image width of first image.",              },
	{"",  "height-img1",      "#",          &bu_opt_int,  (void *)&height1,       "Image height of first iamge.",             },
	{"",  "width-img2",       "#",          &bu_opt_int,  (void *)&width2,        "Image width of second image.",             },
	{"",  "height-img2",      "#",          &bu_opt_int,  (void *)&height2,       "Image height of second iamge.",            },
	{"",  "format-img1",      "format",     &image_mime,  (void *)&in_type_1,     "File format of first input file.",         },
	{"",  "format-img2",      "format",     &image_mime,  (void *)&in_type_2,     "File format of second input file.",        },
	{"",  "output-format",    "format",     &image_mime,  (void *)&out_type,      "File format of output file."               },
	{"A", "approximate",      "",           NULL,         (void *)&approx_diff ,  "Calculate approximate difference metric."  },
	BU_OPT_DESC_NULL
    };

    ac-=(ac>0); av+=(ac>0); /* skip program name argv[0] if present */

    uac = bu_opt_parse(&parse_msgs, ac, av, icv_opt_desc);

    if (uac == -1) {
	bu_log("Parsing error: %s\n", bu_vls_addr(&parse_msgs));
	goto cleanup;
    }

    /* First, see if help was requested or needed */
    if (uac < 2 || uac > 3 || need_help) {
	/* Print help */
	char *help = bu_opt_describe(icv_opt_desc, NULL);
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

    if (approx_diff) {
	pret = icv_pdiff(img1, img2);
	bu_log("Hamming distance: %" PRIu32 "\n", pret);
	ret = 0;
	goto cleanup;
    } else {
	ret = icv_diff(&matching, &off_by_1, &off_by_many, img1, img2);

	bu_log("%d matching, %d off by 1, %d off by many\n", matching, off_by_1, off_by_many);

	if (out_path && (off_by_1 || off_by_many)) {
	    oimg = icv_diffimg(img1, img2);
	    icv_write(oimg, out_path, out_type);
	}
    }

    /* Clean up */
cleanup:
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

