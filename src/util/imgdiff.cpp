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
#include <inttypes.h>
#include <memory>

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

static int
positive_size(struct bu_vls *msg, size_t argc, const char **argv, void *set_size)
{
    size_t value;

    BU_OPT_CHECK_ARGV0(msg, argc, argv, "positive integer");

    if (!bu_opt_scan_size_t_range(argv[0], &value, 1, SIZE_MAX, NULL)) {
	if (msg)
	    bu_vls_sprintf(msg, "Invalid positive integer: %s\n", argv[0]);
	return -1;
    }

    if (set_size)
	*(size_t *)set_size = value;

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
    int meta_diff = 0;
    fastf_t apret = 0;
    std::unique_ptr<icv_context, decltype(&icv_context_destroy)> context(icv_context_create(NULL), icv_context_destroy);
    struct bu_vls in_type_1 = BU_VLS_INIT_ZERO;
    struct bu_vls in_type_2 = BU_VLS_INIT_ZERO;
    struct bu_vls out_type = BU_VLS_INIT_ZERO;
    const char *out_path = NULL;
    const char *nirt_prefix = NULL;
    int need_help = 0;
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
    struct bu_vls plugin = BU_VLS_INIT_ZERO;

    const char *diff_usage = "imgdiff [options] img1 img2";
    struct bu_opt_desc icv_opt_desc[] = {
	{"", "plugin", "library", &bu_opt_vls, &plugin, "Load an image codec plugin."},
	{"h", "help",             "",           NULL,         &need_help,             "Print help and exit."                      },
	{"?", "",                 "",           NULL,         &need_help,             "",                                         },
	{"o", "output",           "file",       &file_null,   (void *)&out_path,      "Output diff image file.",                  },
	{"n", "nirt-output",      "prefix",     &bu_opt_str,  (void *)&nirt_prefix,   "Write nirt shotline script(s) for differing pixels. One file is produced per image that has PNG render metadata: <prefix>_img1.nirt and/or <prefix>_img2.nirt." },
	{"",  "width-img1",       "#",          &positive_size, (void *)&width1,     "Image width of first image.",              },
	{"",  "height-img1",      "#",          &positive_size, (void *)&height1,    "Image height of first image.",             },
	{"",  "width-img2",       "#",          &positive_size, (void *)&width2,     "Image width of second image.",             },
	{"",  "height-img2",      "#",          &positive_size, (void *)&height2,    "Image height of second image.",            },
	{"",  "format-img1",      "format",     &bu_opt_vls,  (void *)&in_type_1,     "File format of first input file.",         },
	{"",  "format-img2",      "format",     &bu_opt_vls,  (void *)&in_type_2,     "File format of second input file.",        },
	{"",  "output-format",    "format",     &bu_opt_vls,  (void *)&out_type,      "File format of output file."               },
	{"A", "approximate",      "",           NULL,         (void *)&approx_diff,   "Calculate approximate difference metric."  },
	{"m", "metadata",         "",           NULL,         (void *)&meta_diff,     "Compare embedded render metadata (PNG only)." },
	BU_OPT_DESC_NULL
    };

    ac-=(ac>0); av+=(ac>0); /* skip program name argv[0] if present */

    uac = bu_opt_parse(&parse_msgs, ac, av, icv_opt_desc);

    if (uac == -1) {
	bu_log("Parsing error: %s\n", bu_vls_addr(&parse_msgs));
	ret = 1;
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

    if (bu_vls_strlen(&plugin) && !icv_load_plugin(context.get(), bu_vls_cstr(&plugin), &slog)) {
	ret = 1;
	goto cleanup;
    }
    img1 = icv_read_with_context(context.get(), img_path_1,
	bu_vls_strlen(&in_type_1) ? bu_vls_cstr(&in_type_1) : NULL, NULL, width1, height1, &slog);
    img2 = icv_read_with_context(context.get(), img_path_2,
	bu_vls_strlen(&in_type_2) ? bu_vls_cstr(&in_type_2) : NULL, NULL, width2, height2, &slog);
    if (!img1 || !img2) {
	ret = 1;
	goto cleanup;
    }

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
	if (!oimg || icv_write_with_context(context.get(), oimg, out_path,
	    bu_vls_strlen(&out_type) ? bu_vls_cstr(&out_type) : NULL, NULL, &slog))
	    ret = 1;
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
    bu_vls_free(&plugin);
    icv_destroy(img1);
    icv_destroy(img2);
    if (bu_vls_strlen(&slog) > 0)
	bu_log("%s", bu_vls_addr(&slog));
    bu_vls_free(&in_type_1);
    bu_vls_free(&in_type_2);
    bu_vls_free(&out_type);
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

