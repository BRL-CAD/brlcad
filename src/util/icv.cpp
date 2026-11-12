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
#include "bu/arg.h"
#include "icv.h"


/* ---- shared option-parsing helpers ------------------------------------ */

static int
file_stat(struct bu_vls *msg, const char *arg, void *storage)
{
    if (!bu_file_exists(arg, NULL)) {
	if (msg)
	    bu_vls_sprintf(msg, "input file %s does not exist\n", arg);
	return -1;
    }

    if (storage)
	*(const char **)storage = arg;

    return 0;
}

static int
file_null(struct bu_vls *msg, const char *arg, void *storage)
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
image_mime(struct bu_vls *msg, const char *arg, void *storage)
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

struct icv_diff_args {
    int print_help;
    const char *output_path;
    bu_mime_image_t output_type;
    bu_mime_image_t input_type1;
    bu_mime_image_t input_type2;
    int width1;
    int height1;
    int width2;
    int height2;
};

static const struct bu_arg_option icv_diff_options[] = {
    BU_ARG_FLAG("h", "help", struct icv_diff_args, print_help, "Print help and exit"),
    BU_ARG_ALIAS("?", NULL, "help", 1),
    BU_ARG_VALUE("o", "output", struct icv_diff_args, output_path,
	BU_ARG_VALUE_FILE, BU_ARG_REQUIRED, "file", "Output diff image file"),
    BU_ARG_CUSTOM(NULL, "output-format", struct icv_diff_args, output_type,
	image_mime, "format", "File format for output image"),
    BU_ARG_CUSTOM(NULL, "format-img1", struct icv_diff_args, input_type1,
	image_mime, "format", "File format of first input image"),
    BU_ARG_CUSTOM(NULL, "format-img2", struct icv_diff_args, input_type2,
	image_mime, "format", "File format of second input image"),
    BU_ARG_INTEGER_RANGE(NULL, "width-img1", struct icv_diff_args, width1,
	1, 0, 0, 0, "pixels", "Width of first input image (raw formats only)"),
    BU_ARG_INTEGER_RANGE(NULL, "height-img1", struct icv_diff_args, height1,
	1, 0, 0, 0, "pixels", "Height of first input image (raw formats only)"),
    BU_ARG_INTEGER_RANGE(NULL, "width-img2", struct icv_diff_args, width2,
	1, 0, 0, 0, "pixels", "Width of second input image (raw formats only)"),
    BU_ARG_INTEGER_RANGE(NULL, "height-img2", struct icv_diff_args, height2,
	1, 0, 0, 0, "pixels", "Height of second input image (raw formats only)"),
    BU_ARG_OPTION_NULL
};

static const struct bu_arg_operand icv_diff_operands[] = {
    BU_ARG_OPERAND("img1", BU_ARG_VALUE_FILE, 1, 1, "First image", NULL),
    BU_ARG_OPERAND("img2", BU_ARG_VALUE_FILE, 1, 1, "Second image", NULL),
    BU_ARG_OPERAND_NULL
};

static const struct bu_arg_schema icv_diff_schema = BU_ARG_SCHEMA(
    "icv diff",
    "Compare two images and report pixel differences. With no -o option the "
    "difference image is written as raw PIX data to stdout.",
    icv_diff_options, icv_diff_operands, BU_ARG_PARSE_INTERSPERSED,
    BU_ARG_SCHEMA_META_HELP(NULL, NULL, NULL, NULL, NULL));

static void
icv_diff_help(void)
{
    char *help = bu_arg_schema_help(&icv_diff_schema, "icv diff");
    if (help)
	bu_log("%s", help);
    bu_free(help, "help str");
}

static int
cmd_diff(int ac, const char **av)
{
    struct icv_diff_args args = {};
    int ret = 0;
    const char *&out_path_str = args.output_path;
    bu_mime_image_t &out_type = args.output_type;
    bu_mime_image_t &in_type_1 = args.input_type1;
    bu_mime_image_t &in_type_2 = args.input_type2;

    struct bu_vls parse_msgs = BU_VLS_INIT_ZERO;

    int operand_index = bu_arg_schema_parse_complete(&icv_diff_schema, &args,
	&parse_msgs, ac, av);
    if (operand_index < 0) {
	bu_log("Parsing error: %s", bu_vls_addr(&parse_msgs));
	bu_vls_free(&parse_msgs);
	return 1;
    }
    bu_vls_free(&parse_msgs);

    if (args.print_help) {
	icv_diff_help();
	return 0;
    }

    av += operand_index;
    const char *img_path_1 = av[0];
    const char *img_path_2 = av[1];
    size_t width1 = (size_t)args.width1;
    size_t height1 = (size_t)args.height1;
    size_t width2 = (size_t)args.width2;
    size_t height2 = (size_t)args.height2;

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


/* ---- icv anim subcommand ---------------------------------------------- */

static int
cmd_anim(int ac, const char **av)
{
    if (ac < 2) {
	bu_log("Usage:\n"
	       "  icv anim add <anim_file> <img1> [<img2> ...]\n"
	       "  icv anim extract <anim_file> <out_prefix>\n"
	       "  icv anim replace <anim_file> <index> <img_file>\n"
	       "  icv anim insert <anim_file> <index> <img_file>\n"
	       "  icv anim remove <anim_file> <index>\n"
	       "  icv anim set-fps <anim_file> <fps>\n"
	       "  icv anim set-delay <anim_file> <index> <delay_usec>\n"
	       "Note: <index> is 1-based.\n"
	       "When creating a new animation via 'add', format is inferred from the extension (.apng or .avi).\n");
	return 1;
    }

    const char *action = av[0];
    const char *anim_file = av[1];

    if (BU_STR_EQUAL(action, "add")) {
	if (ac < 3) return 1;

	icv_anim_t *anim = icv_anim_read(anim_file);
	if (!anim) {
	    icv_anim_format_t fmt = ICV_ANIM_UNKNOWN;
	    if (strstr(anim_file, ".apng") || strstr(anim_file, ".png")) fmt = ICV_ANIM_APNG;
	    else if (strstr(anim_file, ".avi") || strstr(anim_file, ".mjpg")) fmt = ICV_ANIM_MJPG;
	    if (fmt == ICV_ANIM_UNKNOWN) {
		bu_log("Unknown animation format for '%s'. Use .apng or .avi extension.\n", anim_file);
		return 1;
	    }
	    anim = icv_anim_create(fmt, 0, 0, 10);
	}

	for (int i = 2; i < ac; ++i) {
	    bu_mime_image_t type = mime_from_path(av[i]);
	    icv_image_t *img = icv_read_auto(av[i], type, 0, 0);
	    if (!img) {
		bu_log("Failed to read image '%s'\n", av[i]);
		continue;
	    }
	    icv_anim_add_frame(anim, img);
	    icv_destroy(img);
	}

	icv_anim_write(anim, anim_file);
	icv_anim_destroy(anim);
	return 0;

    } else if (BU_STR_EQUAL(action, "extract")) {
	if (ac < 3) return 1;
	const char *out_prefix = av[2];

	icv_anim_t *anim = icv_anim_read(anim_file);
	if (!anim) {
	    bu_log("Failed to read animation '%s'\n", anim_file);
	    return 1;
	}

	size_t num = icv_anim_num_frames(anim);
	for (size_t i = 0; i < num; ++i) {
	    icv_image_t *img = icv_anim_get_frame(anim, i);
	    if (img) {
		char out_name[1024];
		snprintf(out_name, sizeof(out_name), "%s%03zu_ctrl.png", out_prefix, i + 1);
		icv_write(img, out_name, BU_MIME_IMAGE_PNG);
		icv_destroy(img);
	    }
	}
	icv_anim_destroy(anim);
	return 0;

    } else if (BU_STR_EQUAL(action, "replace") || BU_STR_EQUAL(action, "insert") || BU_STR_EQUAL(action, "remove")) {
	if (BU_STR_EQUAL(action, "remove")) {
	    if (ac < 3) return 1;
	} else {
	    if (ac < 4) return 1;
	}
	long idx = strtol(av[2], NULL, 10);
	if (idx < 1) {
	    bu_log("Invalid index %ld. Must be >= 1.\n", idx);
	    return 1;
	}

	icv_anim_t *anim = icv_anim_read(anim_file);
	if (!anim) {
	    bu_log("Failed to read animation '%s'\n", anim_file);
	    return 1;
	}

	size_t i = (size_t)(idx - 1);
	icv_image_t *img = NULL;
	if (!BU_STR_EQUAL(action, "remove")) {
	    bu_mime_image_t type = mime_from_path(av[3]);
	    img = icv_read_auto(av[3], type, 0, 0);
	    if (!img) {
		bu_log("Failed to read image '%s'\n", av[3]);
		icv_anim_destroy(anim);
		return 1;
	    }
	}

	if (BU_STR_EQUAL(action, "replace")) {
	    icv_anim_replace_frame(anim, i, img);
	} else if (BU_STR_EQUAL(action, "insert")) {
	    icv_anim_insert_frame(anim, i, img);
	} else if (BU_STR_EQUAL(action, "remove")) {
	    icv_anim_remove_frame(anim, i);
	}
	if (img) icv_destroy(img);

	icv_anim_write(anim, anim_file);
	icv_anim_destroy(anim);
	return 0;

    } else if (BU_STR_EQUAL(action, "set-fps") || BU_STR_EQUAL(action, "set-delay")) {
	if (BU_STR_EQUAL(action, "set-fps") && ac < 3) return 1;
	if (BU_STR_EQUAL(action, "set-delay") && ac < 4) return 1;

	icv_anim_t *anim = icv_anim_read(anim_file);
	if (!anim) {
	    bu_log("Failed to read animation '%s'\n", anim_file);
	    return 1;
	}

	if (BU_STR_EQUAL(action, "set-fps")) {
	    int fps = atoi(av[2]);
	    if (icv_anim_set_fps(anim, fps) != 0) {
		bu_log("Invalid FPS %d. Must be > 0.\n", fps);
		icv_anim_destroy(anim);
		return 1;
	    }
	} else {
	    long idx = strtol(av[2], NULL, 10);
	    if (idx < 1) {
		bu_log("Invalid index %ld. Must be >= 1.\n", idx);
		icv_anim_destroy(anim);
		return 1;
	    }
	    uint32_t delay = (uint32_t)strtoul(av[3], NULL, 10);
	    if (icv_anim_set_frame_delay(anim, (size_t)(idx - 1), delay) != 0) {
		bu_log("Invalid frame index %ld.\n", idx);
		icv_anim_destroy(anim);
		return 1;
	    }
	}

	if (icv_anim_write(anim, anim_file) != 0) {
	    bu_log("Failed to write animation '%s'\n", anim_file);
	    icv_anim_destroy(anim);
	    return 1;
	}
	icv_anim_destroy(anim);
	return 0;
    }

    bu_log("Unknown action '%s'\n", action);
    return 1;
}


/* ---- top-level usage -------------------------------------------------- */

static void
print_usage(void)
{
    bu_log("Usage: icv [options] input output\n"
	   "       icv info filename [filename ...]\n"
	   "       icv diff [options] img1 img2\n"
	   "       icv anim <action> <anim_file> ...\n\n"
	   "Subcommands:\n"
	   "  info   Report image size, format, and other information.\n"
	   "  diff   Compare two images (pixdiff-compatible; see 'icv diff -h').\n"
	   "  anim   Manage animation files (APNG, MJPG). Run 'icv anim' for usage.\n\n"
	   "Without a subcommand, icv converts the input image to the output format\n"
	   "as inferred from the file extensions (or specified with --input-format /\n"
	   "--output-format).\n");
}


/* ---- conversion (original functionality) ------------------------------ */

struct icv_convert_args {
    int print_help;
    const char *input_path;
    const char *output_path;
    int width;
    int height;
    bu_mime_image_t input_type;
    bu_mime_image_t output_type;
};

static const struct bu_arg_option icv_convert_options[] = {
    BU_ARG_FLAG("h", "help", struct icv_convert_args, print_help, "Print help and exit"),
    BU_ARG_ALIAS("?", NULL, "help", 1),
    BU_ARG_CUSTOM("i", "input", struct icv_convert_args, input_path,
	file_stat, "file", "Input file"),
    BU_ARG_CUSTOM("o", "output", struct icv_convert_args, output_path,
	file_null, "file", "Output file"),
    BU_ARG_INTEGER_RANGE("w", "width", struct icv_convert_args, width,
	1, 0, 0, 0, "pixels", "Image width"),
    BU_ARG_INTEGER_RANGE("n", "height", struct icv_convert_args, height,
	1, 0, 0, 0, "pixels", "Image height"),
    BU_ARG_CUSTOM(NULL, "input-format", struct icv_convert_args, input_type,
	image_mime, "format", "File format of input file"),
    BU_ARG_CUSTOM(NULL, "output-format", struct icv_convert_args, output_type,
	image_mime, "format", "File format of output file"),
    BU_ARG_OPTION_NULL
};

static const struct bu_arg_operand icv_convert_operands[] = {
    BU_ARG_OPERAND("path", BU_ARG_VALUE_FILE, 0, 2,
	"Input and output paths not supplied by options", NULL),
    BU_ARG_OPERAND_NULL
};

static const struct bu_arg_schema icv_convert_schema = BU_ARG_SCHEMA(
    "icv", "Convert images between supported formats", icv_convert_options,
    icv_convert_operands, BU_ARG_PARSE_INTERSPERSED,
    BU_ARG_SCHEMA_META_HELP(NULL, NULL, NULL, NULL, NULL));

static const struct bu_arg_form icv_forms[] = {
    BU_ARG_FORM("convert", "Convert an image", &icv_convert_schema, NULL),
    BU_ARG_FORM("diff", "Compare two images", &icv_diff_schema, NULL),
    BU_ARG_FORM_NULL
};

static const struct bu_arg_forms icv_grammar = BU_ARG_FORMS(
    "icv", "Convert, inspect, or compare images", icv_forms, NULL);

static void
icv_convert_help(void)
{
    char *help = bu_arg_schema_help(&icv_convert_schema, bu_getprogname());
    if (help)
	bu_log("%s", help);
    bu_free(help, "help str");
    bu_log("\nSubcommands:\n"
	   "  info                               Report image information\n"
	   "  diff [options] img1 img2           Compare two images\n"
	   "  anim action anim_file [args ...]   Manage animation files\n");
}

int
main(int ac, const char **av)
{
    const struct bu_arg_grammar grammar = {NULL, NULL, &icv_grammar};
    struct icv_convert_args args = {};
    int ret = 0;
    int operand_index = 0;
    size_t positional_index = 0;
    size_t width = 0;
    size_t height = 0;
    const char *in_fmt = NULL;
    const char *out_fmt = NULL;
    bu_mime_image_t &in_type = args.input_type;
    bu_mime_image_t &out_type = args.output_type;
    icv_image_t *img = NULL;

    bu_setprogname(av[0]);

    struct bu_vls parse_msgs = BU_VLS_INIT_ZERO;
    struct bu_vls in_path = BU_VLS_INIT_ZERO;
    struct bu_vls out_path = BU_VLS_INIT_ZERO;
    struct bu_vls slog = BU_VLS_INIT_ZERO;

    int schema_status = bu_arg_introspect(&parse_msgs, &grammar, ac - 1,
	av + 1);
    if (schema_status != 0) {
	bu_flog(schema_status > 0 ? stdout : stderr, "%s", bu_vls_cstr(&parse_msgs));
	bu_vls_free(&parse_msgs);
	bu_vls_free(&in_path);
	bu_vls_free(&out_path);
	bu_vls_free(&slog);
	return schema_status > 0 ? 0 : 1;
    }

    ac-=(ac>0);
    av+=(ac>0); /* skip program name argv[0] if present */

    if (ac == 0) {
	print_usage();
	goto cleanup;
    }

    /* Dispatch subcommands before option parsing */
    if (BU_STR_EQUAL(av[0], "info"))
	return cmd_info(ac - 1, av + 1);
    if (BU_STR_EQUAL(av[0], "diff"))
	return cmd_diff(ac - 1, av + 1);
    if (BU_STR_EQUAL(av[0], "anim"))
	return cmd_anim(ac - 1, av + 1);

    operand_index = bu_arg_schema_parse_complete(&icv_convert_schema, &args,
	&parse_msgs, ac, av);
    if (operand_index < 0) {
	bu_log("Parsing error: %s", bu_vls_addr(&parse_msgs));
	ret = 1;
	goto cleanup;
    }

    if (args.print_help) {
	icv_convert_help();
	goto cleanup;
    }

    ac -= operand_index;
    av += operand_index;
    if (args.input_path)
	bu_vls_sprintf(&in_path, "%s", args.input_path);
    if (args.output_path)
	bu_vls_sprintf(&out_path, "%s", args.output_path);

    if (!args.input_path && positional_index < (size_t)ac)
	bu_vls_sprintf(&in_path, "%s", av[positional_index++]);
    if (!args.output_path && positional_index < (size_t)ac)
	bu_vls_sprintf(&out_path, "%s", av[positional_index++]);
    if (positional_index != (size_t)ac) {
	bu_vls_printf(&slog, "Too many image paths supplied\n");
	ret = 1;
    }

    width = (size_t)args.width;
    height = (size_t)args.height;

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
    bu_vls_free(&in_path);
    bu_vls_free(&out_path);
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
