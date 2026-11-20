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
#include <memory>
#include <string>
#include <vector>

#include "bio.h"
#include "bu.h"
#include "icv.h"


/* ---- shared option-parsing helpers ------------------------------------ */

int
file_stat(struct bu_vls *msg, size_t argc, const char **argv, void *set_var)
{
    char **file_set = (char **)set_var;

    BU_OPT_CHECK_ARGV0(msg, argc, argv, "input file");

    if (!bu_file_exists(argv[0], NULL)) {
	if (msg) bu_vls_sprintf(msg, "Error - file %s does not exist!\n", argv[0]);
	return -1;
    }

    if (file_set)(*file_set) = bu_strdup(argv[0]);

    return 1;
}

int
file_null(struct bu_vls *msg, size_t argc, const char **argv, void *set_var)
{
    char **file_set = (char **)set_var;

    BU_OPT_CHECK_ARGV0(msg, argc, argv, "output file");

    if (bu_file_exists(argv[0], NULL)) {
	if (msg) bu_vls_sprintf(msg, "Error - file %s already exists!\n", argv[0]);
	return -1;
    }

    if (file_set)(*file_set) = bu_strdup(argv[0]);

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


static int codec_plugin_option(struct bu_vls *, size_t, const char **, void *);

static bool
load_codec_plugins(struct icv_context *context, const std::vector<std::string> &plugins)
{
    struct bu_vls messages = BU_VLS_INIT_ZERO;
    bool success = context != NULL;
    for (const auto &plugin : plugins)
	if (!icv_load_plugin(context, plugin.c_str(), &messages)) {
	    success = false;
	    break;
	}
    if (bu_vls_strlen(&messages))
	bu_log("%s", bu_vls_cstr(&messages));
    bu_vls_free(&messages);
    return success;
}

static int
inspect_formats(struct icv_context *context, int list, const char *filename, struct bu_vls *messages)
{
    struct bu_mime_result *result = NULL;
    struct icv_codec_info *codecs = NULL;
    size_t count = 0;
    int status = 0;
    if (!list) {
	enum bu_mime_status detected = icv_detect_file(context, filename, &result, messages);
	bu_mime_result_describe(result, messages);
	status = detected == BU_MIME_MATCH ? 0 : 1;
    }
    if (!icv_list_codecs(context, NULL, &codecs, &count)) {
	status = 1;
    } else {
	for (size_t i = 0; i < count; ++i) {
	    bool show = list;
	    for (size_t j = 0; !show && result && j < result->count; ++j)
		show = BU_STR_EQUAL(codecs[i].mime_type, result->candidates[j].name);
	    if (show) {
		bu_vls_printf(messages, "%s  %s%s%s%s%s%s%s  %s (priority %u)", codecs[i].mime_type,
		    codecs[i].operations & ICV_CODEC_READ ? "read " : "",
		    codecs[i].operations & ICV_CODEC_READ_MEM ? "read-memory " : "",
		    codecs[i].operations & ICV_CODEC_WRITE ? "write " : "",
		    codecs[i].operations & ICV_CODEC_WRITE_MEM ? "write-memory " : "",
		    codecs[i].operations & ICV_CODEC_METADATA ? "metadata " : "",
		    codecs[i].operations & ICV_CODEC_READ_SEQUENCE ? "read-sequence " : "",
		    codecs[i].operations & ICV_CODEC_WRITE_SEQUENCE ? "write-sequence " : "",
		    codecs[i].name, codecs[i].priority);
		bu_mime_traits_describe(&codecs[i].traits, messages);
		bu_vls_putc(messages, '\n');
	    }
	}
    }
    icv_codec_list_free(codecs, count);
    bu_mime_result_free(result);
    return status;
}

/* ---- icv info subcommand ---------------------------------------------- */

static int
cmd_info(int ac, const char **av)
{
    std::vector<std::string> plugins;
    struct bu_vls messages = BU_VLS_INIT_ZERO;
    int decode = 0;
    struct bu_vls input_format = BU_VLS_INIT_ZERO;
    struct bu_vls input_codec = BU_VLS_INIT_ZERO;
    struct bu_mime_traits input_traits = BU_MIME_TRAITS_INIT_ZERO;
    struct bu_opt_desc options[] = {
	{"", "input-format", "type", bu_opt_vls, &input_format, "Input MIME type or extension."},
	{"", "input-codec", "name", bu_opt_vls, &input_codec, "Select an input codec."},
	{"", "input-trait", "key=value", bu_opt_mime_trait, &input_traits, "Assert an input format property."},
	{"", "decode", "", NULL, &decode, "Decode pixels and report embedded render information."},
	{"", "plugin", "library", &codec_plugin_option, &plugins, "Load an image codec plugin."},
	BU_OPT_DESC_NULL
    };
    ac = bu_opt_parse(&messages, ac, av, options);
    if (ac < 0)
	bu_log("%s", bu_vls_cstr(&messages));
    bu_vls_free(&messages);

    if (ac < 1) {
	bu_log("Usage: icv info [--decode] [--plugin library] filename [filename ...]\n");
	bu_vls_free(&input_format);
	bu_vls_free(&input_codec);
	bu_mime_traits_free(&input_traits);
	return 1;
    }

    std::unique_ptr<icv_context, decltype(&icv_context_destroy)> context(icv_context_create(NULL), icv_context_destroy);
    if (!load_codec_plugins(context.get(), plugins)) {
	bu_vls_free(&input_format);
	bu_vls_free(&input_codec);
	bu_mime_traits_free(&input_traits);
	return 1;
    }
    const char *requested_type = bu_vls_strlen(&input_format) ? bu_vls_cstr(&input_format) : NULL;
    const char *requested_codec = bu_vls_strlen(&input_codec) ? bu_vls_cstr(&input_codec) : NULL;
    int ret = 0;
    for (int i = 0; i < ac; i++) {
	const char *path = av[i];

	if (!bu_file_exists(path, NULL)) {
	    bu_log("%s: file not found\n", path);
	    ret = 1;
	    continue;
	}

	struct bu_vls resolved = BU_VLS_INIT_ZERO;
	int selected = icv_resolve_variant(context.get(), decode ? ICV_CODEC_READ : ICV_CODEC_METADATA,
	    path, requested_type, requested_codec, &input_traits, &resolved, NULL, NULL, NULL);
	/* A readable codec may legitimately lack header metadata. */
	if (!selected && !decode)
	    selected = icv_resolve_variant(context.get(), ICV_CODEC_READ, path,
		requested_type, requested_codec, &input_traits, &resolved, NULL, NULL, NULL);
	std::string type = bu_vls_cstr(&resolved);
	bu_vls_free(&resolved);
	if (!selected) {
	    bu_log("%s: no unambiguous compatible image codec\n", path);
	    ret = 1;
	    continue;
	}
	if (!decode) {
	    icv_image_metadata metadata = {};
	    icv_metadata_status ms = icv_metadata_file_variant(context.get(), path,
		type.c_str(), requested_codec, &input_traits, &metadata, NULL);
	    bu_log("%s: format: %s", path, type.c_str());
	    if (ms == ICV_METADATA_MATCH) {
		bu_log("  %zux%zu", metadata.width, metadata.height);
		if (metadata.channels)
		    bu_log("  channels: %zu (%s)", metadata.channels,
			metadata.color_space == ICV_METADATA_COLOR_GRAY ? "grayscale" :
			metadata.color_space == ICV_METADATA_COLOR_RGB ? "RGB" :
			metadata.color_space == ICV_METADATA_COLOR_CMYK ? "CMYK" : "unknown");
	    } else {
		bu_log("  properties: %s", ms == ICV_METADATA_INCOMPLETE ? "incomplete header" :
		    ms == ICV_METADATA_ERROR ? "invalid header" : "unavailable");
		if (ms == ICV_METADATA_ERROR)
		    ret = 1;
	    }
	    bu_log("\n");
	    continue;
	}
	icv_image_t *img = icv_read_variant(context.get(), path, type.c_str(), requested_codec, &input_traits, 0, 0, NULL);
	if (!img) {
	    bu_log("%s: unable to read image\n", path);
	    ret = 1;
	    continue;
	}

	const char *fmt_str = type.c_str();
	bu_log("%s: %zux%zu  format: %s  channels: %zu (%s)",
	       path,
	       img->width, img->height,
	       fmt_str,
	       img->channels,
	       img->color_space == ICV_COLOR_SPACE_RGB ? "RGB" : "grayscale");

	if (img->render_info) {
	    struct icv_render_info *ri = img->render_info;
	    bu_log("  render-info: db=%s objects=%s",
		   ri->db_filename ? ri->db_filename : "(none)",
		   ri->objects     ? ri->objects     : "(none)");
	}
	bu_log("\n");

	icv_destroy(img);
    }
    bu_vls_free(&input_format);
    bu_vls_free(&input_codec);
    bu_mime_traits_free(&input_traits);
    return ret;
}


/* ---- icv diff subcommand ---------------------------------------------- */

static int
cmd_diff(int ac, const char **av)
{
    std::vector<std::string> plugins;
    int need_help = 0;
    int ret = 0;
    const char *out_path_str = NULL;
    struct bu_vls out_type = BU_VLS_INIT_ZERO;
    struct bu_vls in_type_1 = BU_VLS_INIT_ZERO;
    struct bu_vls in_type_2 = BU_VLS_INIT_ZERO;
    size_t width1 = 0, height1 = 0;
    size_t width2 = 0, height2 = 0;

    struct bu_vls parse_msgs = BU_VLS_INIT_ZERO;

    struct bu_opt_desc diff_opt_desc[] = {
	{"", "plugin", "library", &codec_plugin_option, &plugins, "Load an image codec plugin."},
	{"h", "help",          "",       NULL,         &need_help,            "Print help and exit."                             },
	{"?", "",              "",       NULL,         &need_help,            "",                                                },
	{"o", "output",        "file",   &bu_opt_str, (void *)&out_path_str, "Output diff image file."                         },
	{"",  "output-format", "format", &bu_opt_vls, (void *)&out_type,     "File format for output image."                   },
	{"",  "format-img1",   "format", &bu_opt_vls, (void *)&in_type_1,    "File format of first input image."               },
	{"",  "format-img2",   "format", &bu_opt_vls, (void *)&in_type_2,    "File format of second input image."              },
	{"",  "width-img1",    "#",      &positive_size, (void *)&width1,    "Width of first input image (raw formats only)."  },
	{"",  "height-img1",   "#",      &positive_size, (void *)&height1,   "Height of first input image (raw formats only)." },
	{"",  "width-img2",    "#",      &positive_size, (void *)&width2,    "Width of second input image (raw formats only)." },
	{"",  "height-img2",   "#",      &positive_size, (void *)&height2,   "Height of second input image (raw formats only)."},
	BU_OPT_DESC_NULL
    };

    int uac = bu_opt_parse(&parse_msgs, ac, av, diff_opt_desc);
    if (uac == -1) {
	bu_log("Parsing error: %s\n", bu_vls_addr(&parse_msgs));
	bu_vls_free(&parse_msgs);
	bu_vls_free(&in_type_1);
	bu_vls_free(&in_type_2);
	bu_vls_free(&out_type);

	return 1;
    }
    bu_vls_free(&parse_msgs);

    if (need_help || uac < 2) {
	char *help = bu_opt_describe(diff_opt_desc, NULL);
	bu_log("Usage: icv diff [options] img1 img2\n\n"
	       "Compare two images and report pixel differences.  With no -o option the\n"
	       "difference image is written as raw PIX data to stdout (compatible with\n"
	       "pixdiff).  Use -o to write the diff image to a named file; the output\n"
	       "format is inferred from the file extension, defaulting to PIX for\n"
	       "unassociated names.\n\nOptions:\n%s\n", help);
	bu_free(help, "help str");
	bu_vls_free(&in_type_1);
	bu_vls_free(&in_type_2);
	bu_vls_free(&out_type);

	return need_help ? 0 : 1;
    }

    const char *img_path_1 = av[uac - 2];
    const char *img_path_2 = av[uac - 1];

    std::unique_ptr<icv_context, decltype(&icv_context_destroy)> context(icv_context_create(NULL), icv_context_destroy);
    if (!load_codec_plugins(context.get(), plugins)) {
	bu_vls_free(&in_type_1);
	bu_vls_free(&in_type_2);
	bu_vls_free(&out_type);

	return 1;
    }
    icv_image_t *img1 = icv_read_with_context(context.get(), img_path_1,
	bu_vls_strlen(&in_type_1) ? bu_vls_cstr(&in_type_1) : NULL, NULL, width1, height1, NULL);
    icv_image_t *img2 = icv_read_with_context(context.get(), img_path_2,
	bu_vls_strlen(&in_type_2) ? bu_vls_cstr(&in_type_2) : NULL, NULL, width2, height2, NULL);

    if (!img1 || !img2) {
	bu_log("icv diff: error reading input image(s)\n");
	icv_destroy(img1);
	icv_destroy(img2);
	bu_vls_free(&in_type_1);
	bu_vls_free(&in_type_2);
	bu_vls_free(&out_type);

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
	    if (out_path_str || !isatty(fileno(stdout))) {
		const char *format = bu_vls_strlen(&out_type) ? bu_vls_cstr(&out_type) : NULL;
		bu_mime_result *formats = NULL;
		bu_vls messages = BU_VLS_INIT_ZERO;
		bu_mime_status identified = out_path_str && !format ?
		    bu_mime_detect(icv_mime_registry(context.get()), out_path_str, NULL, 0, 0, &formats) : BU_MIME_UNKNOWN_RESULT;
		/* Preserve the pixdiff-compatible default for stdout and
		 * unassociated names; registered suffixes use their writer. */
		if (!format && identified == BU_MIME_UNKNOWN_RESULT)
		    format = "image/vnd.brlcad.pix";
		if (identified == BU_MIME_ERROR ||
		    icv_write_with_context(context.get(), oimg, out_path_str, format, NULL, &messages)) {
		    bu_log("icv diff: cannot write difference image to %s\n%s",
			out_path_str ? out_path_str : "stdout", bu_vls_cstr(&messages));
		    ret = 1;
		}
		bu_vls_free(&messages);
		bu_mime_result_free(formats);
	    }
	    icv_destroy(oimg);
	}
    }

    icv_destroy(img1);
    icv_destroy(img2);
    bu_vls_free(&in_type_1);
    bu_vls_free(&in_type_2);
    bu_vls_free(&out_type);

    return ret;
}


/* ---- icv anim subcommand ---------------------------------------------- */

static int
cmd_anim(int ac, const char **av)
{
    std::vector<std::string> plugins;
    struct bu_vls messages = BU_VLS_INIT_ZERO;
    struct bu_opt_desc options[] = {
	{"", "plugin", "library", codec_plugin_option, &plugins, "Load an image or sequence codec plugin."},
	BU_OPT_DESC_NULL
    };
    ac = bu_opt_parse(&messages, ac, av, options);
    if (ac < 0) bu_log("%s", bu_vls_cstr(&messages));
    bu_vls_free(&messages);
    if (ac < 2) {
	bu_log("Usage:\n"
	       "  icv anim [--plugin library] add <anim_file> <img1> [<img2> ...]\n"
	       "  icv anim extract <anim_file> <out_prefix>\n"
	       "  icv anim replace <anim_file> <index> <img_file>\n"
	       "  icv anim insert <anim_file> <index> <img_file>\n"
	       "  icv anim remove <anim_file> <index>\n"
	       "  icv anim set-fps <anim_file> <fps>\n"
	       "  icv anim set-delay <anim_file> <index> <delay_usec>\n"
	       "Note: <index> is 1-based.\n"
	       "When creating a new animation via 'add', a registered sequence writer is selected from the filename.\n");
	return 1;
    }

    const char *action = av[0];
    const char *anim_file = av[1];
    std::unique_ptr<icv_context, decltype(&icv_context_destroy)> context(icv_context_create(NULL), icv_context_destroy);
    if (!context || !load_codec_plugins(context.get(), plugins)) return 1;
    const auto read_sequence = [&]() {
	return icv_anim_read_variant(context.get(), anim_file, NULL, NULL, NULL, NULL);
    };
    const auto write_sequence = [&](const icv_anim_t *animation) {
	return icv_anim_write_variant(context.get(), animation, anim_file, icv_anim_mime_type(animation), NULL, NULL, NULL);
    };

    if (BU_STR_EQUAL(action, "add")) {
	if (ac < 3) return 1;

	icv_anim_t *anim = bu_file_exists(anim_file, NULL) ? read_sequence() : NULL;
	if (!anim) {
	    if (bu_file_exists(anim_file, NULL) || !icv_resolve_variant(context.get(), ICV_CODEC_WRITE_SEQUENCE,
		anim_file, NULL, NULL, NULL, NULL, NULL, NULL, NULL)) {
		bu_log("Cannot select animation input/output for '%s'\n", anim_file);
		return 1;
	    }
	    anim = icv_anim_create(ICV_ANIM_UNKNOWN, 0, 0, 10);
	    if (!anim) return 1;
	}

	for (int i = 2; i < ac; ++i) {
	    icv_image_t *img = icv_read_with_context(context.get(), av[i], NULL, NULL, 0, 0, NULL);
	    if (!img) {
		bu_log("Failed to read image '%s'\n", av[i]);
		continue;
	    }
	    icv_anim_add_frame(anim, img);
	    icv_destroy(img);
	}

	const int status = write_sequence(anim);
	icv_anim_destroy(anim);
	return status ? 1 : 0;

    } else if (BU_STR_EQUAL(action, "extract")) {
	if (ac < 3) return 1;
	const char *out_prefix = av[2];

	icv_anim_t *anim = read_sequence();
	if (!anim) {
	    bu_log("Failed to read animation '%s'\n", anim_file);
	    return 1;
	}

	size_t num = icv_anim_num_frames(anim);
	int status = 0;
	for (size_t i = 0; i < num; ++i) {
	    icv_image_t *img = icv_anim_get_frame(anim, i);
	    if (img) {
		char out_name[1024];
		snprintf(out_name, sizeof(out_name), "%s%03zu_ctrl.png", out_prefix, i + 1);
		status = icv_write_with_context(context.get(), img, out_name, "image/png", NULL, NULL);
		icv_destroy(img);
		if (status) {
		    bu_log("Failed to write frame '%s'\n", out_name);
		    break;
		}
	    }
	}
	icv_anim_destroy(anim);
	return status ? 1 : 0;

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

	icv_anim_t *anim = read_sequence();
	if (!anim) {
	    bu_log("Failed to read animation '%s'\n", anim_file);
	    return 1;
	}

	size_t i = (size_t)(idx - 1);
	icv_image_t *img = NULL;
	if (!BU_STR_EQUAL(action, "remove")) {
	    img = icv_read_with_context(context.get(), av[3], NULL, NULL, 0, 0, NULL);
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

	const int status = write_sequence(anim);
	icv_anim_destroy(anim);
	return status ? 1 : 0;

    } else if (BU_STR_EQUAL(action, "set-fps") || BU_STR_EQUAL(action, "set-delay")) {
	if (BU_STR_EQUAL(action, "set-fps") && ac < 3) return 1;
	if (BU_STR_EQUAL(action, "set-delay") && ac < 4) return 1;

	icv_anim_t *anim = read_sequence();
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

	if (write_sequence(anim) != 0) {
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
	   "using input contents and output extensions (or --input-format /\n"
	   "--output-format).\n");
}

static int
codec_plugin_option(struct bu_vls *messages, size_t argc, const char **argv, void *data)
{
    BU_OPT_CHECK_ARGV0(messages, argc, argv, "plugin library");
    static_cast<std::vector<std::string> *>(data)->emplace_back(argv[0]);
    return 1;
}

static bool
codec_format(struct icv_context *context, struct bu_vls *format, struct bu_vls *messages)
{
    if (!bu_vls_strlen(format))
	return true;
    std::string requested = bu_vls_cstr(format);
    struct bu_mime_result *names = NULL;
    enum bu_mime_status status = bu_mime_resolve_candidates(icv_mime_registry(context), requested.c_str(), &names);
    if (status == BU_MIME_MATCH)
	bu_vls_strcpy(format, names->candidates[0].name);
    bu_mime_result_free(names);
    if (status == BU_MIME_MATCH || status == BU_MIME_AMBIGUOUS)
	return true;
    bu_vls_printf(messages, "Unknown image format: %s\n", requested.c_str());
    return false;
}

/* ---- conversion ------------------------------ */

int
main(int ac, const char **av)
{
    int uac = 0;
    int ret = 0;
    size_t width = 0;
    size_t height = 0;
    std::unique_ptr<icv_context, decltype(&icv_context_destroy)> context(NULL, icv_context_destroy);
    std::vector<std::string> plugins;
    char *in_path_str = NULL;
    char *out_path_str = NULL;
    int need_help = 0;
    int list_formats = 0;
    char *identify = NULL;
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
    struct bu_mime_traits input_traits = BU_MIME_TRAITS_INIT_ZERO;
    struct bu_mime_traits output_traits = BU_MIME_TRAITS_INIT_ZERO;
    struct bu_vls input_codec = BU_VLS_INIT_ZERO;
    struct bu_vls output_codec = BU_VLS_INIT_ZERO;

    struct bu_opt_desc icv_opt_desc[] = {
	{"", "list-formats", "", NULL, &list_formats, "List registered formats and codecs."},
	{"", "identify", "file", &bu_opt_str, &identify, "Inspect file type and available codecs."},
	{"h", "help",          "",       NULL, &need_help, "Print help and exit."},
	{"?", "",              "",       NULL, &need_help, ""},
	{"i", "input",         "file",   &file_stat, &in_path_str, "Input file."},
	{"o", "output",        "file",   &file_null, &out_path_str, "Output file."},
	{"w", "width",         "#",      &positive_size, &width, "Image width."},
	{"n", "height",        "#",      &positive_size, &height, "Image height."},
	{"", "input-format",   "format", &bu_opt_vls, &in_format, "Input MIME name or extension."},
	{"", "output-format",  "format", &bu_opt_vls, &out_format, "Output MIME name or extension."},
	{"", "plugin",         "library", &codec_plugin_option, &plugins, "Load an image codec plugin (repeatable)."},
	{"", "input-trait", "key=value", &bu_opt_mime_trait, &input_traits, "Required input format trait (repeatable)."},
	{"", "output-trait", "key=value", &bu_opt_mime_trait, &output_traits, "Requested output format trait (repeatable)."},
	{"", "input-codec",    "name", &bu_opt_vls, &input_codec, "Input codec implementation."},
	{"", "output-codec",   "name", &bu_opt_vls, &output_codec, "Output codec implementation."},
	BU_OPT_DESC_NULL
    };

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

    uac = bu_opt_parse(&parse_msgs, ac, av, icv_opt_desc);

    if (uac == -1) {
	bu_log("Parsing error: %s\n", bu_vls_addr(&parse_msgs));
	ret = 1;
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

    context.reset(icv_context_create(NULL));
    if (!context) {
	ret = 1;
	goto cleanup;
    }
    if (!load_codec_plugins(context.get(), plugins)) {
	ret = 1;
	goto cleanup;
    }
    if (list_formats || identify) {
	ret = inspect_formats(context.get(), list_formats, identify, &slog);
	goto cleanup;
    }
    if (!codec_format(context.get(), &in_format, &slog) ||
	!codec_format(context.get(), &out_format, &slog)) {
	ret = 1;
	goto cleanup;
    }
    if (in_path_str) {
	bu_vls_strcpy(&in_path, in_path_str);
	skip_in = 1;
    }
    if (out_path_str) {
	bu_vls_strcpy(&out_path, out_path_str);
	skip_out = 1;
    }
    if (skip_in && !skip_out && uac > 0)
	bu_vls_strcpy(&out_path, av[uac - 1]);
    if (!skip_in && skip_out && uac > 0)
	bu_vls_strcpy(&in_path, av[uac - 1]);
    if (!skip_in && !skip_out && uac > 0) {
	bu_vls_strcpy(&in_path, av[uac > 1 ? uac - 2 : uac - 1]);
	if (uac > 1)
	    bu_vls_strcpy(&out_path, av[uac - 1]);
    }

    /* Make sure we have distinct input and output paths */
    if (bu_vls_strlen(&in_path) > 0 && BU_STR_EQUAL(bu_vls_addr(&in_path), bu_vls_addr(&out_path))) {
	bu_vls_printf(&slog, "Error: identical path specified for both input and output: %s\n", bu_vls_addr(&out_path));
	ret = 1;
    }

    if (ret)
	goto cleanup;
    img = icv_read_variant(context.get(), bu_vls_strlen(&in_path) ? bu_vls_cstr(&in_path) : NULL,
				bu_vls_strlen(&in_format) ? bu_vls_cstr(&in_format) : NULL,
				bu_vls_strlen(&input_codec) ? bu_vls_cstr(&input_codec) : NULL, &input_traits, width, height, &slog);
    if (!img || icv_write_variant(context.get(), img,
				       bu_vls_strlen(&out_path) ? bu_vls_cstr(&out_path) : NULL,
				       bu_vls_strlen(&out_format) ? bu_vls_cstr(&out_format) : NULL,
				       bu_vls_strlen(&output_codec) ? bu_vls_cstr(&output_codec) : NULL, &output_traits, &slog))
	ret = 1;

    /* Clean up */
cleanup:

    if (bu_vls_strlen(&slog) > 0)
	bu_log("%s", bu_vls_addr(&slog));
    if (img)
	icv_destroy(img);
    bu_free(in_path_str, "input path option");
    bu_free(out_path_str, "output path option");
    bu_vls_free(&parse_msgs);
    bu_mime_traits_free(&input_traits);
    bu_mime_traits_free(&output_traits);
    bu_vls_free(&input_codec);
    bu_vls_free(&output_codec);
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
