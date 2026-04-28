/*                           I C V . C P P
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
/** @file icv.cpp
 *
 * Image converter and processing utility.
 *
 * Subcommands:
 *   icv info filename [...]    - report image size, format, and other info
 *   icv diff [opts] img1 img2  - compare two images (see pixdiff)
 *
 * Without a subcommand, acts as an image format converter.
 *
 */

#include "common.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/stat.h>

#include "bio.h"
#include "bu.h"
#include "icv.h"


/* ---- shared option-parsing helpers ------------------------------------ */

int
file_stat(struct bu_vls *msg, size_t argc, const char **argv, void *set_var)
{
    char **file_set = (char **)set_var;

    BU_OPT_CHECK_ARGV0(msg, argc, argv, "input file");

    if (!bu_file_exists(argv[0], NULL)){
	if (msg) bu_vls_sprintf(msg, "Error - file %s does not exist!\n", argv[0]);
	return -1;
    }

    if (file_set) (*file_set) = bu_strdup(argv[0]);

    return 1;
}

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


/* ---- internal utilities ----------------------------------------------- */

/* Derive a MIME image type from a file's extension. */
static bu_mime_image_t
mime_from_path(const char *path)
{
    struct bu_vls c = BU_VLS_INIT_ZERO;
    bu_mime_image_t t = BU_MIME_IMAGE_UNKNOWN;
    if (path && bu_path_component(&c, path, BU_PATH_EXT)) {
	int ti = bu_file_mime(bu_vls_addr(&c), BU_MIME_IMAGE);
	t = (ti < 0) ? BU_MIME_IMAGE_UNKNOWN : (bu_mime_image_t)ti;
    }
    bu_vls_free(&c);
    return t;
}

/*
 * Read an image, auto-deducing dimensions for raw formats (pix/bw) from the
 * file size when width/height are both zero.  Returns NULL on failure.
 */
static icv_image_t *
icv_read_auto(const char *path, bu_mime_image_t fmt, size_t width, size_t height)
{
    if (!width && !height && (fmt == BU_MIME_IMAGE_PIX || fmt == BU_MIME_IMAGE_BW)) {
	struct stat sbuf;
	if (stat(path, &sbuf) < 0) {
	    bu_log("Error: unable to stat '%s'\n", path);
	    return NULL;
	}
	if (sbuf.st_size <= 0) {
	    bu_log("Error: '%s' has zero or invalid size\n", path);
	    return NULL;
	}
	if (!icv_image_size(NULL, 0, (size_t)sbuf.st_size, fmt, &width, &height)) {
	    bu_log("Error: unable to determine dimensions of '%s';\n"
		   "  use -w/-n to specify width/height explicitly.\n", path);
	    return NULL;
	}
    }
    return icv_read(path, fmt, width, height);
}


/* ---- icv info subcommand ---------------------------------------------- */

static int
cmd_info(int ac, const char **av)
{
    if (ac < 1) {
	bu_log("Usage: icv info filename [filename ...]\n");
	return 1;
    }

    int ret = 0;
    for (int i = 0; i < ac; i++) {
	const char *path = av[i];

	if (!bu_file_exists(path, NULL)) {
	    bu_log("%s: file not found\n", path);
	    ret = 1;
	    continue;
	}

	bu_mime_image_t fmt = mime_from_path(path);
	icv_image_t *img = icv_read_auto(path, fmt, 0, 0);
	if (!img) {
	    bu_log("%s: unable to read image\n", path);
	    ret = 1;
	    continue;
	}

	const char *fmt_str = bu_file_mime_str((int)fmt, BU_MIME_IMAGE);
	bu_log("%s: %zux%zu  format: %s  channels: %zu (%s)",
	       path,
	       img->width, img->height,
	       fmt_str ? fmt_str : "unknown",
	       img->channels,
	       img->color_space == ICV_COLOR_SPACE_RGB ? "RGB" : "grayscale");

	if (img->render_info) {
	    struct icv_render_info *ri = img->render_info;
	    bu_log("  render-info: db=%s objects=%s",
		   ri->db_filename ? ri->db_filename : "(none)",
		   ri->objects     ? ri->objects     : "(none)");
	}
	bu_log("\n");

	bu_free((char *)fmt_str, "fmt str");
	icv_destroy(img);
    }
    return ret;
}


/* ---- icv diff subcommand ---------------------------------------------- */

static int
cmd_diff(int ac, const char **av)
{
    int need_help = 0;
    int ret = 0;
    const char *out_path_str = NULL;
    bu_mime_image_t out_type  = BU_MIME_IMAGE_UNKNOWN;
    bu_mime_image_t in_type_1 = BU_MIME_IMAGE_UNKNOWN;
    bu_mime_image_t in_type_2 = BU_MIME_IMAGE_UNKNOWN;
    size_t width1 = 0, height1 = 0;
    size_t width2 = 0, height2 = 0;

    struct bu_vls parse_msgs = BU_VLS_INIT_ZERO;

    struct bu_opt_desc diff_opt_desc[] = {
	{"h", "help",          "",       NULL,         &need_help,            "Print help and exit."                             },
	{"?", "",              "",       NULL,         &need_help,            "",                                                },
	{"o", "output",        "file",   &bu_opt_str,  (void *)&out_path_str, "Output diff image file."                         },
	{"",  "output-format", "format", &image_mime,  (void *)&out_type,     "File format for output image."                   },
	{"",  "format-img1",   "format", &image_mime,  (void *)&in_type_1,    "File format of first input image."               },
	{"",  "format-img2",   "format", &image_mime,  (void *)&in_type_2,    "File format of second input image."              },
	{"",  "width-img1",    "#",      &bu_opt_int,  (void *)&width1,       "Width of first input image (raw formats only)."  },
	{"",  "height-img1",   "#",      &bu_opt_int,  (void *)&height1,      "Height of first input image (raw formats only)." },
	{"",  "width-img2",    "#",      &bu_opt_int,  (void *)&width2,       "Width of second input image (raw formats only)." },
	{"",  "height-img2",   "#",      &bu_opt_int,  (void *)&height2,      "Height of second input image (raw formats only)."},
	BU_OPT_DESC_NULL
    };

    int uac = bu_opt_parse(&parse_msgs, ac, av, diff_opt_desc);
    if (uac == -1) {
	bu_log("Parsing error: %s\n", bu_vls_addr(&parse_msgs));
	bu_vls_free(&parse_msgs);
	return 1;
    }
    bu_vls_free(&parse_msgs);

    if (need_help || uac < 2) {
	char *help = bu_opt_describe(diff_opt_desc, NULL);
	bu_log("Usage: icv diff [options] img1 img2\n\n"
	       "Compare two images and report pixel differences.  With no -o option the\n"
	       "difference image is written as raw PIX data to stdout (compatible with\n"
	       "pixdiff).  Use -o to write the diff image to a named file; the output\n"
	       "format is inferred from the file extension.\n\nOptions:\n%s\n", help);
	bu_free(help, "help str");
	return need_help ? 0 : 1;
    }

    const char *img_path_1 = av[uac - 2];
    const char *img_path_2 = av[uac - 1];

    /* Determine input formats from file extensions when not specified */
    if (in_type_1 == BU_MIME_IMAGE_UNKNOWN)
	in_type_1 = mime_from_path(img_path_1);
    if (in_type_2 == BU_MIME_IMAGE_UNKNOWN)
	in_type_2 = mime_from_path(img_path_2);

    /* Determine output format */
    if (out_type == BU_MIME_IMAGE_UNKNOWN && out_path_str)
	out_type = mime_from_path(out_path_str);
    /* Default: raw PIX to stdout (pixdiff-compatible) */
    if (out_type == BU_MIME_IMAGE_UNKNOWN)
	out_type = BU_MIME_IMAGE_PIX;

    icv_image_t *img1 = icv_read_auto(img_path_1, in_type_1, width1, height1);
    icv_image_t *img2 = icv_read_auto(img_path_2, in_type_2, width2, height2);

    if (!img1 || !img2) {
	bu_log("icv diff: error reading input image(s)\n");
	icv_destroy(img1);
	icv_destroy(img2);
	return 1;
    }

    int matching = 0, off_by_1 = 0, off_by_many = 0;
    ret = icv_diff(&matching, &off_by_1, &off_by_many, img1, img2);
    fprintf(stderr,
	    "icv diff channels: %d matching, %d off by 1, %d off by many\n",
	    matching, off_by_1, off_by_many);

    if (off_by_1 || off_by_many) {
	icv_image_t *oimg = icv_diffimg(img1, img2);
	if (oimg) {
	    if (out_path_str) {
		/* Write diff image to named output file */
		icv_write(oimg, out_path_str, out_type);
	    } else if (!isatty(fileno(stdout))) {
		/* Pipe raw PIX diff to stdout, matching pixdiff behaviour */
		icv_write(oimg, NULL, out_type);
	    }
	    icv_destroy(oimg);
	}
    }

    icv_destroy(img1);
    icv_destroy(img2);
    return ret;
}


/* ---- top-level usage -------------------------------------------------- */

static void
print_usage(void)
{
    bu_log("Usage: icv [options] input output\n"
	   "       icv info filename [filename ...]\n"
	   "       icv diff [options] img1 img2\n\n"
	   "Subcommands:\n"
	   "  info   Report image size, format, and other information.\n"
	   "  diff   Compare two images (pixdiff-compatible; see 'icv diff -h').\n\n"
	   "Without a subcommand, icv converts the input image to the output format\n"
	   "as inferred from the file extensions (or specified with --input-format /\n"
	   "--output-format).\n");
}


/* ---- conversion (original functionality) ------------------------------ */

int
main(int ac, const char **av)
{
    int uac = 0;
    int ret = 0;
    size_t width = 0;
    size_t height = 0;
    const char *in_fmt = NULL;
    const char *out_fmt = NULL;
    static bu_mime_image_t in_type = BU_MIME_IMAGE_UNKNOWN;
    static bu_mime_image_t out_type = BU_MIME_IMAGE_UNKNOWN;
    static char *in_path_str = NULL;
    static char *out_path_str = NULL;
    int need_help = 0;
    int skip_in = 0;
    int skip_out = 0;
    icv_image_t *img = NULL;

    bu_setprogname(av[0]);

    struct bu_vls parse_msgs = BU_VLS_INIT_ZERO;
    struct bu_vls in_format = BU_VLS_INIT_ZERO;
    struct bu_vls in_path = BU_VLS_INIT_ZERO;
    struct bu_vls out_format = BU_VLS_INIT_ZERO;
    struct bu_vls out_path = BU_VLS_INIT_ZERO;
    struct bu_vls slog = BU_VLS_INIT_ZERO;

    struct bu_opt_desc icv_opt_desc[] = {
	{"h", "help",             "",           NULL,          &need_help,            "Print help and exit."        },
	{"?", "",                 "",           NULL,          &need_help,            "",                           },
	{"i", "input",            "file",       &file_stat,   (void *)&in_path_str,   "Input file.",                },
	{"o", "output",           "file",       &file_null,   (void *)&out_path_str,  "Output file.",               },
	{"w", "width",            "#",          &bu_opt_int,  (void *)&width,         "Image width.",               },
	{"n", "height",           "#",          &bu_opt_int,  (void *)&height,        "Image height.",              },
	{"",  "input-format",     "format",     &image_mime,  (void *)&in_type,       "File format of input file.", },
	{"",  "output-format",    "format",     &image_mime,  (void *)&out_type,      "File format of output file." },
	BU_OPT_DESC_NULL
    };

    ac-=(ac>0); av+=(ac>0); /* skip program name argv[0] if present */

    if (ac == 0) {
	print_usage();
	goto cleanup;
    }

    /* Dispatch subcommands before option parsing */
    if (BU_STR_EQUAL(av[0], "info"))
	return cmd_info(ac - 1, av + 1);
    if (BU_STR_EQUAL(av[0], "diff"))
	return cmd_diff(ac - 1, av + 1);

    uac = bu_opt_parse(&parse_msgs, ac, av, icv_opt_desc);

    if (uac == -1) {
	bu_log("Parsing error: %s\n", bu_vls_addr(&parse_msgs));
	goto cleanup;
    }

    /* First, see if help was requested or needed */
    if (need_help) {
	char *help = bu_opt_describe(icv_opt_desc, NULL);
	print_usage();
	bu_log("Conversion options:\n%s\n", help);
	bu_free(help, "help str");
	goto cleanup;
    }

    /* Did we get explicit options for an input and/or output file? */
    if (in_path_str) {
	bu_vls_sprintf(&in_path, "%s", in_path_str);
	skip_in++;
    }
    if (out_path_str) {
	bu_vls_sprintf(&out_path, "%s", out_path_str);
	skip_out++;
    }

    /* If not specified explicitly with -i or -o, the input and output paths must always
     * be the last two arguments supplied */
    if (!(skip_in && skip_out)) {
	if (skip_in && !skip_out) {
	    bu_vls_sprintf(&out_path, "%s", av[uac - 1]);
	}
	if (!skip_in && skip_out) {
	    bu_vls_sprintf(&in_path, "%s", av[uac - 1]);
	}
	if (!skip_in && !skip_out) {
	    if (ac > 1) {
		bu_vls_sprintf(&in_path, "%s", av[uac - 2]);
		bu_vls_sprintf(&out_path, "%s", av[uac - 1]);
	    } else {
		bu_vls_sprintf(&in_path, "%s", av[uac - 1]);
	    }
	}
    }

    /* Make sure we have distinct input and output paths */
    if (bu_vls_strlen(&in_path) > 0 && BU_STR_EQUAL(bu_vls_addr(&in_path), bu_vls_addr(&out_path))) {
	bu_vls_printf(&slog, "Error: identical path specified for both input and output: %s\n", bu_vls_addr(&out_path));
	ret = 1;
    }

    /* Find out what input file type we are dealing with */
    if (in_type == BU_MIME_IMAGE_UNKNOWN) {
	struct bu_vls c = BU_VLS_INIT_ZERO;
	if (!bu_vls_strlen(&in_path)) {
	    bu_vls_printf(&slog, "No input file specified - to use stdin, specify an image format for the data input stream\n");
	    ret = 1;
	} else {
	    if (bu_path_component(&c, bu_vls_addr(&in_path), BU_PATH_EXT)) {
		in_type = (bu_mime_image_t)bu_file_mime(bu_vls_addr(&c), BU_MIME_IMAGE);
	    } else {
		bu_vls_printf(&slog, "No input file image type specified - need either a specified input image type or a path that provides MIME information.\n");
		ret = 1;
	    }
	}
	bu_vls_free(&c);
    }
    if (out_type == BU_MIME_IMAGE_UNKNOWN) {
	struct bu_vls c = BU_VLS_INIT_ZERO;
	if (!bu_vls_strlen(&out_path)) {
	    bu_vls_printf(&slog, "No output file specified - to use stdout, specify an image format to use for the data stream\n");
	    ret = 1;
	} else {
	    if (bu_path_component(&c, bu_vls_addr(&out_path), BU_PATH_EXT)) {
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

    in_fmt = bu_file_mime_str((int)in_type, BU_MIME_IMAGE);
    out_fmt = bu_file_mime_str((int)out_type, BU_MIME_IMAGE);

    /* If we have no width or height specified, and we have an input format that
     * does not encode that information, make an educated guess */
    if (!width && !height && (in_type == BU_MIME_IMAGE_PIX || in_type == BU_MIME_IMAGE_BW)) {
	struct stat sbuf;
	if (stat(bu_vls_addr(&in_path), &sbuf) < 0) {
	    bu_exit(1, "Unable to stat input file");
	}
	if (!icv_image_size(NULL, 0, (size_t)sbuf.st_size, in_type, &width, &height)) {
	    bu_log("Error - input image type does not have dimension information encoded, and icv was not able to deduce a size.  Please specify image width in pixels with the \"-w\" option and image height in pixels with the \"-n\" option.\n");
	    bu_exit(1, "image dimensional information insufficient");
	}
    }

    img = icv_read(bu_vls_addr(&in_path), in_type, width, height);
    icv_write(img, bu_vls_addr(&out_path), out_type);

    /* Clean up */
cleanup:
    if (bu_vls_strlen(&slog) > 0)
	bu_log("%s", bu_vls_addr(&slog));
    bu_free((char *)in_fmt, "input format string");
    bu_free((char *)out_fmt, "output format string");
    bu_vls_free(&in_format);
    bu_vls_free(&in_path);
    bu_vls_free(&out_format);
    bu_vls_free(&out_path);
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

