/*                        M A I N . C
 * BRL-CAD
 *
 * Copyright (c) 1986-2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file rtwizard/main.c
 *
 */

#include "common.h"
#include <string.h>

#ifdef HAVE_WINDOWS_H
#  include <direct.h> /* For chdir */
#endif

#if defined(_WIN32) && !defined(__CYGWIN__)
__declspec(dllimport) void * __stdcall GetStdHandle(unsigned long);
__declspec(dllimport) int __stdcall SetHandleInformation(void *, unsigned long, unsigned long);
#  define RTWIZARD_STD_INPUT_HANDLE  ((unsigned long)-10)
#  define RTWIZARD_STD_OUTPUT_HANDLE ((unsigned long)-11)
#  define RTWIZARD_STD_ERROR_HANDLE  ((unsigned long)-12)
#  define RTWIZARD_HANDLE_FLAG_INHERIT 0x00000001UL
#endif

#include "tcl.h"

#include "vmath.h"
#include "bu/app.h"
#include "bu/color.h"
#include "bu/cmdschema.h"
#include "bu/file.h"
#include "bu/mime.h"
#include "bu/malloc.h"
#include "bu/log.h"
#include "bu/path.h"
#include "bu/ptbl.h"
#include "bu/str.h"
#include "pkg.h"
#include "tclcad.h"

#define RTWIZARD_HAVE_GUI 0

#define RTWIZARD_SIZE_DEFAULT 512

#define RTWIZARD_MAGIC 0x72747769 /**< rtwi */


struct rtwizard_settings {
    uint32_t magic;

    int use_gui;
    int no_gui;
    int verbose;

    struct bu_ptbl *color;
    struct bu_ptbl *ghost;
    struct bu_ptbl *line;

    struct bu_vls *input_file;
    struct bu_vls *output_file;
    struct bu_vls *fb_dev;
    int port;
    struct bu_vls *log_file;
    struct bu_vls *pid_file;

    size_t width;
    int width_set;
    size_t height;
    int height_set;
    size_t size; /* Assumes square - width and height - overridden by width and height */
    int size_set;

    struct bu_color *bkg_color;
    struct bu_color *line_color;
    struct bu_color *non_line_color;

    double ghost_intensity;
    int occlusion;
    int benchmark;
    int cpus;

    /* View model */
    double viewsize;
    quat_t orientation;
    vect_t eye_pt;

    /* View settings */
    double az, el, tw;
    double perspective;
    double zoom;
    vect_t center;

};


struct rtwizard_dimensions {
    size_t width;
    int width_set;
    size_t height;
    int height_set;
    size_t size;
    int size_set;
};


/* This is deliberately a value-owning parse record rather than a second
 * descriptor table.  The application state predates native schemas and owns
 * several pointers; parsing into this compact record keeps the schema's
 * storage bindings direct and lets command parsing stay reentrant. */
struct rtwizard_cli {
    int help;
    int help_dev;
    char type;
    struct bu_vls input_file;
    struct bu_vls output_file;
    struct rtwizard_dimensions dimensions;
    struct bu_ptbl color;
    struct bu_ptbl ghost;
    struct bu_ptbl line;
    fastf_t az;
    fastf_t el;
    fastf_t tw;
    fastf_t zoom;
    vect_t center;
    vect_t eye_pt;
    fastf_t viewsize;
    quat_t orientation;
    fastf_t perspective;
    struct bu_color bkg_color;
    struct bu_color line_color;
    struct bu_color non_line_color;
    fastf_t ghost_intensity;
    int occlusion;
    int use_gui;
    int no_gui;
    struct bu_vls fb_dev;
    int port;
    int benchmark;
    int cpus;
    struct bu_vls pid_file;
    struct bu_vls log_file;
    int verbose;
};

static int
rtwizard_setup_ipc(Tcl_Interp *interp)
{
    char ipc_addr[MAXPATHLEN] = {0};

    if (pkg_ipc_addr(ipc_addr, sizeof(ipc_addr), "brlcad-rtwizard") != 0)
	return 0;

    (void)Tcl_SetVar2(interp, "::RtWizard::wizard_state", "fbserv_ipc_addr",
		      ipc_addr, TCL_GLOBAL_ONLY);

    return 1;
}


struct rtwizard_settings *
rtwizard_settings_create(void)
{
    struct rtwizard_settings *s;
    unsigned char white[3] = {255, 255, 255};
    unsigned char black[3] = {0, 0, 0};
    BU_GET(s, struct rtwizard_settings);
    s->magic = RTWIZARD_MAGIC;
    BU_GET(s->color, struct bu_ptbl);
    BU_GET(s->ghost, struct bu_ptbl);
    BU_GET(s->line,  struct bu_ptbl);
    bu_ptbl_init(s->color, 8, "color init");
    bu_ptbl_init(s->ghost, 8, "ghost init");
    bu_ptbl_init(s->line, 8, "line init");

    BU_GET(s->input_file, struct bu_vls);
    bu_vls_init(s->input_file);
    BU_GET(s->output_file, struct bu_vls);
    bu_vls_init(s->output_file);
    BU_GET(s->fb_dev, struct bu_vls);
    bu_vls_init(s->fb_dev);
    BU_GET(s->log_file, struct bu_vls);
    bu_vls_init(s->log_file);
    BU_GET(s->pid_file, struct bu_vls);
    bu_vls_init(s->pid_file);

    BU_GET(s->bkg_color, struct bu_color);
    (void)bu_color_from_rgb_chars(s->bkg_color, white);
    BU_GET(s->line_color, struct bu_color);
    (void)bu_color_from_rgb_chars(s->line_color, black);
    BU_GET(s->non_line_color, struct bu_color);
    (void)bu_color_from_rgb_chars(s->non_line_color, black);
    s->benchmark = 0;
    s->port = -1;
    s->cpus = 0;

    s->az = DBL_MAX;
    s->el = DBL_MAX;
    s->tw = DBL_MAX;
    s->perspective = DBL_MAX;
    s->zoom = DBL_MAX;
    VSETALL(s->center, DBL_MAX);

    s->viewsize = DBL_MAX;
    s->orientation[0] = DBL_MAX;
    s->orientation[1] = DBL_MAX;
    s->orientation[2] = DBL_MAX;
    s->orientation[3] = DBL_MAX;
    VSETALL(s->eye_pt, DBL_MAX);

    s->occlusion = 1;
    s->ghost_intensity = 6.0;
    s->width = RTWIZARD_SIZE_DEFAULT;
    s->width_set = 0;
    s->height = RTWIZARD_SIZE_DEFAULT;
    s->height_set = 0;
    s->size = RTWIZARD_SIZE_DEFAULT;
    s->size_set = 0;

    s->use_gui = 0;
    s->no_gui = 0;

    s->verbose = 0;
    return s;
}


void rtwizard_settings_destroy(struct rtwizard_settings *s) {
    bu_ptbl_free(s->color);
    bu_ptbl_free(s->ghost);
    bu_ptbl_free(s->line);
    /* TODO - loop over and free table contents */
    BU_PUT(s->color, struct bu_ptbl);
    BU_PUT(s->ghost, struct bu_ptbl);
    BU_PUT(s->line,  struct bu_ptbl);

    BU_PUT(s->bkg_color, struct bu_color);
    BU_PUT(s->line_color, struct bu_color);
    BU_PUT(s->non_line_color, struct bu_color);

    bu_vls_free(s->fb_dev);
    BU_PUT(s->fb_dev, struct bu_vls);
    bu_vls_free(s->input_file);
    BU_PUT(s->input_file, struct bu_vls);
    bu_vls_free(s->output_file);
    BU_PUT(s->output_file, struct bu_vls);
    bu_vls_free(s->log_file);
    BU_PUT(s->log_file, struct bu_vls);
    bu_vls_free(s->pid_file);
    BU_PUT(s->pid_file, struct bu_vls);


    BU_PUT(s, struct rtwizard_settings);
}


static void
rtwizard_cli_init(struct rtwizard_cli *args, const struct rtwizard_settings *settings)
{
    if (!args || !settings)
	return;

    memset(args, 0, sizeof(*args));
    bu_vls_init(&args->input_file);
    bu_vls_init(&args->output_file);
    bu_vls_init(&args->fb_dev);
    bu_vls_init(&args->pid_file);
    bu_vls_init(&args->log_file);
    bu_ptbl_init(&args->color, 8, "rtwizard color arguments");
    bu_ptbl_init(&args->ghost, 8, "rtwizard ghost arguments");
    bu_ptbl_init(&args->line, 8, "rtwizard line arguments");

    args->dimensions.width = settings->width;
    args->dimensions.width_set = settings->width_set;
    args->dimensions.height = settings->height;
    args->dimensions.height_set = settings->height_set;
    args->dimensions.size = settings->size;
    args->dimensions.size_set = settings->size_set;
    args->az = settings->az;
    args->el = settings->el;
    args->tw = settings->tw;
    args->zoom = settings->zoom;
    VMOVE(args->center, settings->center);
    VMOVE(args->eye_pt, settings->eye_pt);
    QMOVE(args->orientation, settings->orientation);
    args->viewsize = settings->viewsize;
    args->perspective = settings->perspective;
    args->bkg_color = *settings->bkg_color;
    args->line_color = *settings->line_color;
    args->non_line_color = *settings->non_line_color;
    args->ghost_intensity = settings->ghost_intensity;
    args->occlusion = settings->occlusion;
    args->use_gui = settings->use_gui;
    args->no_gui = settings->no_gui;
    args->port = settings->port;
    args->benchmark = settings->benchmark;
    args->cpus = settings->cpus;
    args->verbose = settings->verbose;
}


static void
rtwizard_cli_free(struct rtwizard_cli *args)
{
    struct bu_ptbl *lists[3] = {NULL, NULL, NULL};
    size_t list_i;

    if (!args)
	return;
    lists[0] = &args->color;
    lists[1] = &args->ghost;
    lists[2] = &args->line;
    for (list_i = 0; list_i < 3; list_i++) {
	struct bu_ptbl *list = lists[list_i];
	size_t i;
	for (i = 0; i < BU_PTBL_LEN(list); i++)
	    bu_free((void *)BU_PTBL_GET(list, i), "rtwizard option object");
	bu_ptbl_free(list);
    }
    bu_vls_free(&args->input_file);
    bu_vls_free(&args->output_file);
    bu_vls_free(&args->fb_dev);
    bu_vls_free(&args->pid_file);
    bu_vls_free(&args->log_file);
}


static void
rtwizard_cli_apply(struct rtwizard_settings *settings, struct rtwizard_cli *args)
{
    if (!settings || !args)
	return;

    bu_vls_sprintf(settings->input_file, "%s", bu_vls_addr(&args->input_file));
    bu_vls_sprintf(settings->output_file, "%s", bu_vls_addr(&args->output_file));
    bu_vls_sprintf(settings->fb_dev, "%s", bu_vls_addr(&args->fb_dev));
    bu_vls_sprintf(settings->pid_file, "%s", bu_vls_addr(&args->pid_file));
    bu_vls_sprintf(settings->log_file, "%s", bu_vls_addr(&args->log_file));
    settings->width = args->dimensions.width;
    settings->width_set = args->dimensions.width_set;
    settings->height = args->dimensions.height;
    settings->height_set = args->dimensions.height_set;
    settings->size = args->dimensions.size;
    settings->size_set = args->dimensions.size_set;
    settings->az = args->az;
    settings->el = args->el;
    settings->tw = args->tw;
    settings->zoom = args->zoom;
    VMOVE(settings->center, args->center);
    VMOVE(settings->eye_pt, args->eye_pt);
    QMOVE(settings->orientation, args->orientation);
    settings->viewsize = args->viewsize;
    settings->perspective = args->perspective;
    *settings->bkg_color = args->bkg_color;
    *settings->line_color = args->line_color;
    *settings->non_line_color = args->non_line_color;
    settings->ghost_intensity = args->ghost_intensity;
    settings->occlusion = args->occlusion;
    settings->use_gui = args->use_gui;
    settings->no_gui = args->no_gui;
    settings->port = args->port;
    settings->benchmark = args->benchmark;
    settings->cpus = args->cpus;
    settings->verbose = args->verbose;

    bu_ptbl_cat(settings->color, &args->color);
    bu_ptbl_cat(settings->ghost, &args->ghost);
    bu_ptbl_cat(settings->line, &args->line);
    bu_ptbl_reset(&args->color);
    bu_ptbl_reset(&args->ghost);
    bu_ptbl_reset(&args->line);
}


#define RTW_TERR_MSG(_t, _name, _opt) "Error: picture type _t specified, but no _name objects listed.\nPlease specify _name objects using the _opt option\n"

int
rtwizard_info_sufficient(struct bu_vls *msg, struct rtwizard_settings *s, char type)
{
    int ret = 1;
    if (!bu_vls_strlen(s->input_file)) {
	bu_vls_printf(msg, "Error: No input Geometry Database (.g) file specified.\n");
	ret = 0;
    }
    switch (type) {
	case 'A':
	    if (BU_PTBL_LEN(s->color) == 0) {
		bu_vls_printf(msg, "%s", RTW_TERR_MSG(type, "color", "-c"));
		ret = 0;
	    }
	    break;
	case 'B':
	    if (BU_PTBL_LEN(s->line) == 0) {
		bu_vls_printf(msg, "%s", RTW_TERR_MSG(type, "line", "-l"));
		ret = 0;
	    }
	    break;
	case 'C':
	case 'D':
	    if (BU_PTBL_LEN(s->color) == 0 || BU_PTBL_LEN(s->line) == 0) {
		if (BU_PTBL_LEN(s->line) == 0) {
		    bu_vls_printf(msg, "%s", RTW_TERR_MSG(type, "line", "-l"));
		    ret = 0;
		}
		if (BU_PTBL_LEN(s->color) == 0) {
		    bu_vls_printf(msg, "%s", RTW_TERR_MSG(type, "color", "-c"));
		    ret = 0;
		}
	    }
	    break;
	case 'E':
	    if (BU_PTBL_LEN(s->color) == 0 || BU_PTBL_LEN(s->ghost) == 0) {
		if (BU_PTBL_LEN(s->ghost) == 0) {
		    bu_vls_printf(msg, "%s", RTW_TERR_MSG(type, "ghost", "-g"));
		    ret = 0;
		}
		if (BU_PTBL_LEN(s->color) == 0) {
		    bu_vls_printf(msg, "%s", RTW_TERR_MSG(type, "color", "-c"));
		    ret = 0;
		}
	    }
	    break;
	case 'F':
	    if (BU_PTBL_LEN(s->color) == 0 || BU_PTBL_LEN(s->line) == 0 || BU_PTBL_LEN(s->ghost) == 0) {
		if (BU_PTBL_LEN(s->ghost) == 0) {
		    bu_vls_printf(msg, "%s", RTW_TERR_MSG(type, "ghost", "-g"));
		    ret = 0;
		}
		if (BU_PTBL_LEN(s->color) == 0) {
		    bu_vls_printf(msg, "%s", RTW_TERR_MSG(type, "color", "-c"));
		    ret = 0;
		}
		if (BU_PTBL_LEN(s->line) == 0) {
		    bu_vls_printf(msg, "%s", RTW_TERR_MSG(type, "line", "-l"));
		    ret = 0;
		}
	    }
	    break;
	default:
	    /* If we don't have a type, make sure we've got *some* object in at
	     * least one of the object lists */
	    if (BU_PTBL_LEN(s->color) == 0 && BU_PTBL_LEN(s->line) == 0 && BU_PTBL_LEN(s->ghost) == 0) {
		bu_vls_printf(msg, "Error: please specify at least one color, line, or ghost object.\n");
		ret = 0;
	    }
	    break;
    }

    return ret;
}


/* return 0 if there's no conflict (all user or all low level or defaults
 * only), 1 otherwise */
int
rtwizard_view_opts_check(struct bu_vls *msg, struct rtwizard_settings *s)
{
    int high_level = 0;
    int low_level = 0;
    if (s->az < DBL_MAX || s->el < DBL_MAX || s->tw < DBL_MAX || s->perspective < DBL_MAX
	|| s->zoom < DBL_MAX || s->center[0] < DBL_MAX) {
	high_level = 1;
    }
    if (s->viewsize < DBL_MAX || s->orientation[0] < DBL_MAX || s->eye_pt[0] < DBL_MAX) {
	low_level = 1;
    }

    if (low_level > 0) {
	/* We've got a potential conflict.  If we have a complete low level specification,
	 * that overrides the high level options.  Otherwise, it's the other way around. */
	if (high_level > 0 && s->viewsize < DBL_MAX && s->orientation[0] < DBL_MAX && s->eye_pt[0] < DBL_MAX) {
	    if (msg) bu_vls_printf(msg, "Warning - user level view modifiers supplied, but a complete low level view specification is present - overriding the following options:");
	    if (s->az < DBL_MAX) {
		if (msg) bu_vls_printf(msg, " azimuth ");
		s->az = DBL_MAX;
	    }
	    if (s->el < DBL_MAX) {
		if (msg) bu_vls_printf(msg, " elevation ");
		s->el = DBL_MAX;
	    }
	    if (s->tw < DBL_MAX) {
		if (msg) bu_vls_printf(msg, " twist ");
		s->tw = DBL_MAX;
	    }
	    if (s->perspective < DBL_MAX) {
		if (msg) bu_vls_printf(msg, " perspective ");
		s->perspective = DBL_MAX;
	    }
	    if (s->zoom < DBL_MAX) {
		if (msg) bu_vls_printf(msg, " zoom ");
		s->zoom = DBL_MAX;
	    }
	    if (s->center[0] < DBL_MAX) {
		if (msg) bu_vls_printf(msg, " center ");
		s->center[0] = DBL_MAX;
		s->center[1] = DBL_MAX;
		s->center[2] = DBL_MAX;
	    }
	    return 1;
	} else {
	    if (!(s->viewsize < DBL_MAX && s->orientation[0] < DBL_MAX && s->eye_pt[0] < DBL_MAX)) {
		if (msg) bu_vls_printf(msg, "Warning - low level view modifiers supplied, but a complete low level specification (viewsize, orientation, and eye_pt) was not present.  The following options will have no effect:");
		if (s->viewsize < DBL_MAX) {
		    if (msg) bu_vls_printf(msg, " viewsize ");
		    s->viewsize = DBL_MAX;
		}
		if (s->orientation[0] < DBL_MAX) {
		    if (msg) bu_vls_printf(msg, " orientation ");
		    s->orientation[0] = DBL_MAX;
		    s->orientation[1] = DBL_MAX;
		    s->orientation[2] = DBL_MAX;
		    s->orientation[3] = DBL_MAX;
		}
		if (s->eye_pt[0] < DBL_MAX) {
		    if (msg) bu_vls_printf(msg, " eye_pt ");
		    s->eye_pt[0] = DBL_MAX;
		    s->eye_pt[1] = DBL_MAX;
		    s->eye_pt[2] = DBL_MAX;
		}
		return 1;
	    }
	}
    }

    return 0;
}


static int
rtwizard_dimension_from_str(size_t *value, const char *arg)
{
    int parsed;

    if (!value || !bu_cmd_integer_from_str(&parsed, arg) || parsed < 0)
	return 0;
    *value = (size_t)parsed;
    return 1;
}


static int
rtwizard_width_parse(struct bu_vls *msg, const char *arg, void *storage)
{
    struct rtwizard_dimensions *dimensions = (struct rtwizard_dimensions *)storage;
    size_t width;

    if (!rtwizard_dimension_from_str(&width, arg)) {
	if (msg)
	    bu_vls_printf(msg, "invalid image width: %s\n", arg ? arg : "");
	return -1;
    }
    if (dimensions) {
	dimensions->width = width;
	dimensions->width_set = 1;
    }
    return 0;
}


static int
rtwizard_height_parse(struct bu_vls *msg, const char *arg, void *storage)
{
    struct rtwizard_dimensions *dimensions = (struct rtwizard_dimensions *)storage;
    size_t height;

    if (!rtwizard_dimension_from_str(&height, arg)) {
	if (msg)
	    bu_vls_printf(msg, "invalid image height: %s\n", arg ? arg : "");
	return -1;
    }
    if (dimensions) {
	dimensions->height = height;
	dimensions->height_set = 1;
    }
    return 0;
}


static int
rtwizard_size_parse(struct bu_vls *msg, const char *arg, void *storage)
{
    struct rtwizard_dimensions *dimensions = (struct rtwizard_dimensions *)storage;
    size_t size;

    if (!rtwizard_dimension_from_str(&size, arg)) {
	if (msg)
	    bu_vls_printf(msg, "invalid image size: %s\n", arg ? arg : "");
	return -1;
    }
    if (dimensions) {
	dimensions->size = size;
	dimensions->size_set = 1;
	if (!dimensions->width_set)
	    dimensions->width = size;
	if (!dimensions->height_set)
	    dimensions->height = size;
    }
    return 0;
}


static int
rtwizard_objects_parse(struct bu_vls *msg, const char *arg, void *storage)
{
    size_t i = 0;
    size_t count;
    char *objects;
    char **words;
    struct bu_ptbl *table = (struct bu_ptbl *)storage;

    if (!arg || !arg[0]) {
	if (msg)
	    bu_vls_strcat(msg, "object list expected\n");
	return -1;
    }
    objects = bu_strdup(arg);
    while (objects[i]) {
	if (objects[i] == ',' || objects[i] == ';' || objects[i] == '\'' || objects[i] == '\"') {
	    if (i == 0 || objects[i - 1] != '\\')
		objects[i] = ' ';
	}
	i++;
    }
    words = (char **)bu_calloc(strlen(objects) + 1, sizeof(char *), "rtwizard object words");
    count = bu_argv_from_string(words, strlen(objects), objects);
    if (!count && msg)
	bu_vls_strcat(msg, "object list expected\n");
    if (table) {
	for (i = 0; i < count; i++)
	    bu_ptbl_ins(table, (long *)bu_strdup(words[i]));
    }
    bu_free(words, "rtwizard object words");
    bu_free(objects, "rtwizard object string");
    return count ? 0 : -1;
}


static int
rtwizard_type_parse(struct bu_vls *msg, const char *arg, void *storage)
{
    char type;

    if (!arg || strlen(arg) != 1 || strchr("ABCDEF", arg[0]) == NULL) {
	if (msg)
	    bu_vls_printf(msg, "invalid letter specifier for rtwizard type: %s\n", arg ? arg : "");
	return -1;
    }
    type = arg[0];
    if (storage)
	*((char *)storage) = type;
    return 0;
}


static size_t
rtwizard_quaternion_tokens(size_t available, const char **argv)
{
    size_t i;

    if (!available || !argv || !argv[0])
	return 0;
    if (strchr(argv[0], ',') || strchr(argv[0], '/'))
	return 1;
    if (available < 4)
	return 1;
    for (i = 0; i < 4; i++) {
	fastf_t value;
	if (!bu_cmd_number_from_str(&value, argv[i]))
	    return 1;
    }
    return 4;
}


static int
rtwizard_quaternion_consume(struct bu_vls *msg, size_t argc, const char **argv, void *storage)
{
    fastf_t values[4];
    const char *words[4];
    char *packed = NULL;
    size_t i;

    if (!argc || !argv || !argv[0])
	return -1;
    if (argc == 1) {
	packed = bu_strdup(argv[0]);
	for (i = 0; packed[i]; i++) {
	    if (packed[i] == ',' || packed[i] == '/')
		packed[i] = ' ';
	}
	if (bu_argv_from_string((char **)words, 4, packed) != 4) {
	    if (msg)
		bu_vls_printf(msg, "no valid quaternion found: %s\n", argv[0]);
	    bu_free(packed, "rtwizard packed quaternion");
	    return -1;
	}
	argv = words;
	argc = 4;
    }
    if (argc != 4) {
	if (msg)
	    bu_vls_printf(msg, "no valid quaternion found: %s\n", argv[0]);
	bu_free(packed, "rtwizard packed quaternion");
	return -1;
    }
    for (i = 0; i < 4; i++) {
	if (!bu_cmd_number_from_str(&values[i], argv[i])) {
	    if (msg)
		bu_vls_printf(msg, "not a number: %s\n", argv[i]);
	    bu_free(packed, "rtwizard packed quaternion");
	    return -1;
	}
    }
    if (storage) {
	quat_t *quaternion = (quat_t *)storage;
	for (i = 0; i < 4; i++)
	    (*quaternion)[i] = values[i];
    }
    bu_free(packed, "rtwizard packed quaternion");
    return 0;
}


static const struct bu_cmd_arg_shape rtwizard_quaternion_shape = {
    BU_CMD_ARG_SHAPE_CUSTOM, 1, 4, "quaternion", rtwizard_quaternion_tokens
};


static const struct bu_cmd_option rtwizard_options[] = {
    BU_CMD_FLAG("h", "help", struct rtwizard_cli, help, "Print options help and exit."),
    BU_CMD_ALIAS_SHORT("?", "help", 1),
    BU_CMD_FLAG(NULL, "help-dev", struct rtwizard_cli, help_dev, "Print development and programmatic options."),
    BU_CMD_VLS_APPEND("i", "input-file", struct rtwizard_cli, input_file, "filename", "Input .g database file."),
    BU_CMD_VLS_APPEND("o", "output-file", struct rtwizard_cli, output_file, "filename", "Image output file name."),
    BU_CMD_CUSTOM("s", "size", struct rtwizard_cli, dimensions, rtwizard_size_parse, "pixels", "Output width and height for a square image."),
    BU_CMD_CUSTOM("w", "width", struct rtwizard_cli, dimensions, rtwizard_width_parse, "pixels", "Output image width; overrides --size."),
    BU_CMD_CUSTOM("n", "height", struct rtwizard_cli, dimensions, rtwizard_height_parse, "pixels", "Output image height; overrides --size."),
    BU_CMD_CUSTOM("t", "type", struct rtwizard_cli, type, rtwizard_type_parse, "A|B|C|D|E|F", "Specify RtWizard picture type."),
    BU_CMD_CUSTOM("c", "color-objects", struct rtwizard_cli, color, rtwizard_objects_parse, "object-list", "List of color objects."),
    BU_CMD_CUSTOM("l", "line-objects", struct rtwizard_cli, line, rtwizard_objects_parse, "object-list", "List of line objects."),
    BU_CMD_CUSTOM("g", "ghost-objects", struct rtwizard_cli, ghost, rtwizard_objects_parse, "object-list", "List of ghost objects."),
    BU_CMD_NUMBER("a", "azimuth", struct rtwizard_cli, az, "number", "Set azimuth."),
    BU_CMD_NUMBER("e", "elevation", struct rtwizard_cli, el, "number", "Set elevation."),
    BU_CMD_NUMBER(NULL, "twist", struct rtwizard_cli, tw, "number", "Set twist."),
    BU_CMD_NUMBER("z", "zoom", struct rtwizard_cli, zoom, "number", "Set zoom."),
    BU_CMD_VECTOR3(NULL, "center", struct rtwizard_cli, center, "point", "Set view center."),
    BU_CMD_VECTOR3(NULL, "eye_pt", struct rtwizard_cli, eye_pt, "point", "Set eye point."),
    BU_CMD_NUMBER(NULL, "viewsize", struct rtwizard_cli, viewsize, "number", "Set view size."),
    BU_CMD_OPTION_SHAPED(NULL, "orientation", "orientation", struct rtwizard_cli, orientation,
	BU_CMD_VALUE_CUSTOM, "quaternion", "Set view orientation.", BU_CMD_ARG_REQUIRED,
	&rtwizard_quaternion_shape, rtwizard_quaternion_consume),
    BU_CMD_NUMBER("P", "perspective", struct rtwizard_cli, perspective, "number", "Set perspective."),
    BU_CMD_COLOR_COMPAT("C", "background-color", struct rtwizard_cli, bkg_color, "color", "Background image color."),
    BU_CMD_COLOR_COMPAT(NULL, "line-color", struct rtwizard_cli, line_color, "color", "Color used for line rendering."),
    BU_CMD_COLOR_COMPAT(NULL, "non-line-color", struct rtwizard_cli, non_line_color, "color", "Color used for non-line rendering."),
    BU_CMD_NUMBER("G", "ghost-intensity", struct rtwizard_cli, ghost_intensity, "number", "Intensity of ghost objects."),
    BU_CMD_INTEGER("O", "occlusion", struct rtwizard_cli, occlusion, "mode", "Occlusion mode (1 to 3)."),
    BU_CMD_FLAG(NULL, "gui", struct rtwizard_cli, use_gui, "Force use of GUI."),
    BU_CMD_FLAG(NULL, "no-gui", struct rtwizard_cli, no_gui, "Do not use GUI."),
    BU_CMD_VLS_APPEND("d", "fbserv-device", struct rtwizard_cli, fb_dev, "device", "Device for framebuffer viewing."),
    BU_CMD_INTEGER("p", "fbserv-port", struct rtwizard_cli, port, "port", "Framebuffer port."),
    BU_CMD_FLAG(NULL, "benchmark", struct rtwizard_cli, benchmark, "Benchmark mode."),
    BU_CMD_INTEGER(NULL, "cpu-count", struct rtwizard_cli, cpus, "count", "Specify the number of CPUs to use."),
    BU_CMD_VLS_APPEND(NULL, "pid-file", struct rtwizard_cli, pid_file, "filename", "File used for tracking PID numbers."),
    BU_CMD_VLS_APPEND(NULL, "log-file", struct rtwizard_cli, log_file, "filename", "Log debugging output to this file."),
    BU_CMD_INTEGER("v", "verbose", struct rtwizard_cli, verbose, "level", "Verbosity."),
    BU_CMD_OPTION_NULL
};


static const struct bu_cmd_operand rtwizard_operands[] = {
    BU_CMD_OPERAND("argument", BU_CMD_VALUE_RAW, 0, BU_CMD_COUNT_UNLIMITED,
	"Geometry, image output, or color object.", NULL),
    BU_CMD_OPERAND_NULL
};


static const struct bu_cmd_schema rtwizard_schema = {
    "rtwizard", "Set up or launch an RtWizard rendering session.", rtwizard_options,
    rtwizard_operands, BU_CMD_PARSE_INTERSPERSED, BU_CMD_SCHEMA_CONSTRAINTS(NULL, NULL)
};


void print_rtwizard_state(struct rtwizard_settings *s) {
    size_t i = 0;
    struct bu_vls slog = BU_VLS_INIT_ZERO;

    bu_vls_printf(&slog, "use_gui: %d\n", s->use_gui);
    bu_vls_printf(&slog, "no_gui: %d\n", s->no_gui);
    bu_vls_printf(&slog, "verbose: %d\n", s->verbose);


    bu_vls_printf(&slog, "color objs:");
    for (i = 0; i < BU_PTBL_LEN(s->color); i++) {
	bu_vls_printf(&slog, " %s", (const char *)BU_PTBL_GET(s->color, i));
    }
    bu_vls_printf(&slog, "\nghost objs:");
    for (i = 0; i < BU_PTBL_LEN(s->ghost); i++) {
	bu_vls_printf(&slog, " %s", (const char *)BU_PTBL_GET(s->ghost, i));
    }
    bu_vls_printf(&slog, "\nline objs:");
    for (i = 0; i < BU_PTBL_LEN(s->line); i++) {
	bu_vls_printf(&slog, " %s", (const char *)BU_PTBL_GET(s->line, i));
    }
    bu_vls_printf(&slog, "\n\n");

    bu_vls_printf(&slog, "input_file: %s\n", bu_vls_addr(s->input_file));
    bu_vls_printf(&slog, "output_file: %s\n", bu_vls_addr(s->output_file));
    bu_vls_printf(&slog, "fb_dev: %s\n", bu_vls_addr(s->fb_dev));
    bu_vls_printf(&slog, "port: %d\n", s->port);
    bu_vls_printf(&slog, "log_file: %s\n", bu_vls_addr(s->log_file));
    bu_vls_printf(&slog, "pid_file: %s\n", bu_vls_addr(s->pid_file));

    bu_vls_printf(&slog, "width(%d): %zu\n", s->width_set, s->width);
    bu_vls_printf(&slog, "height(%d): %zu\n", s->height_set, s->height);
    bu_vls_printf(&slog, "size(%d): %zu\n", s->size_set, s->size);
    bu_vls_printf(&slog, "bkg_color: %d, %d, %d\n", (int)s->bkg_color->buc_rgb[0], (int)s->bkg_color->buc_rgb[1], (int)s->bkg_color->buc_rgb[2]);
    bu_vls_printf(&slog, "line_color: %d, %d, %d\n", (int)s->line_color->buc_rgb[0], (int)s->line_color->buc_rgb[1], (int)s->line_color->buc_rgb[2]);
    bu_vls_printf(&slog, "non_line_color: %d, %d, %d\n", (int)s->non_line_color->buc_rgb[0], (int)s->non_line_color->buc_rgb[1], (int)s->non_line_color->buc_rgb[2]);

    bu_vls_printf(&slog, "\nghost intensity: %f\n", s->ghost_intensity);
    bu_vls_printf(&slog, "occlusion: %d\n", s->occlusion);
    bu_vls_printf(&slog, "benchmark: %d\n", s->benchmark);
    bu_vls_printf(&slog, "cpus: %d\n", s->cpus);

    bu_vls_printf(&slog, "\nviewsize: %f\n", s->viewsize);
    bu_vls_printf(&slog, "quat: %f, %f, %f, %f\n", s->orientation[0], s->orientation[1], s->orientation[2], s->orientation[3]);
    bu_vls_printf(&slog, "eye_pt: %f, %f, %f\n", s->eye_pt[0], s->eye_pt[1], s->eye_pt[2]);

    bu_vls_printf(&slog, "\naz, el, tw: %f, %f, %f\n", s->az, s->el, s->tw);
    bu_vls_printf(&slog, "perspective: %f\n", s->perspective);
    bu_vls_printf(&slog, "zoom: %f\n", s->zoom);
    bu_vls_printf(&slog, "center: %f, %f, %f\n", s->center[0], s->center[1], s->center[2]);

    bu_log("%s", bu_vls_addr(&slog));
    bu_vls_free(&slog);
}


int rtwizard_imgformat_supported(int fmt) {
    if (fmt == BU_MIME_IMAGE_DPIX)
	return 1;
    if (fmt == BU_MIME_IMAGE_PIX)
	return 1;
    if (fmt == BU_MIME_IMAGE_PNG)
	return 1;
    if (fmt == BU_MIME_IMAGE_PPM)
	return 1;
    if (fmt == BU_MIME_IMAGE_BW)
	return 1;
    return 0;
}


#if defined(_WIN32) && !defined(__CYGWIN__)
static void
rtwizard_disable_std_handle_inheritance(void)
{
    void *h = NULL;

    h = GetStdHandle(RTWIZARD_STD_INPUT_HANDLE);
    if (h)
	(void)SetHandleInformation(h, RTWIZARD_HANDLE_FLAG_INHERIT, 0);

    h = GetStdHandle(RTWIZARD_STD_OUTPUT_HANDLE);
    if (h)
	(void)SetHandleInformation(h, RTWIZARD_HANDLE_FLAG_INHERIT, 0);

    h = GetStdHandle(RTWIZARD_STD_ERROR_HANDLE);
    if (h)
	(void)SetHandleInformation(h, RTWIZARD_HANDLE_FLAG_INHERIT, 0);
}
#endif


static void
rtwizard_set_state(Tcl_Interp *interp, const char *key, const char *value)
{
    (void)Tcl_SetVar2(interp, "::RtWizard::wizard_state", key, value, TCL_GLOBAL_ONLY);
}


static void
rtwizard_set_state_ptbl(Tcl_Interp *interp, const char *key, struct bu_ptbl *ptbl)
{
    size_t i = 0;
    Tcl_Obj *obj = Tcl_NewListObj(0, NULL);

    Tcl_IncrRefCount(obj);
    for (i = 0; i < BU_PTBL_LEN(ptbl); i++) {
	const char *item = (const char *)BU_PTBL_GET(ptbl, i);
	(void)Tcl_ListObjAppendElement(interp, obj, Tcl_NewStringObj(item, -1));
    }
    (void)Tcl_SetVar2Ex(interp, "::RtWizard::wizard_state", key, obj, TCL_GLOBAL_ONLY);
    Tcl_DecrRefCount(obj);
}


void
Init_RtWizard_Vars(Tcl_Interp *interp, struct rtwizard_settings *s)
{
    struct bu_vls tcl_cmd = BU_VLS_INIT_ZERO;

    (void)Tcl_Eval(interp, "namespace eval RtWizard {}");

    if (s->use_gui) {
	bu_vls_sprintf(&tcl_cmd, "set ::use_gui 1");
	(void)Tcl_Eval(interp, bu_vls_addr(&tcl_cmd));
    }

    if (s->no_gui) {
	bu_vls_sprintf(&tcl_cmd, "set ::disable_gui 1");
	(void)Tcl_Eval(interp, bu_vls_addr(&tcl_cmd));
    }

    if (s->verbose) {
	bu_vls_sprintf(&tcl_cmd, "set ::RtWizard::wizard_state(verbose) %d", s->verbose);
	(void)Tcl_Eval(interp, bu_vls_addr(&tcl_cmd));
    }

    if (bu_vls_strlen(s->input_file)) {
	rtwizard_set_state(interp, "dbFile", bu_vls_addr(s->input_file));
    }

    if (bu_vls_strlen(s->output_file)) {
	rtwizard_set_state(interp, "output_filename", bu_vls_addr(s->output_file));
    }

    if (bu_vls_strlen(s->fb_dev)) {
	rtwizard_set_state(interp, "fbserv_device", bu_vls_addr(s->fb_dev));
    }

    if (s->port > -1) {
	bu_vls_sprintf(&tcl_cmd, "set ::RtWizard::wizard_state(fbserv_port) %d", s->port);
	(void)Tcl_Eval(interp, bu_vls_addr(&tcl_cmd));
    }

    if (s->width_set) {
	bu_vls_sprintf(&tcl_cmd, "set ::RtWizard::wizard_state(width) %zu", s->width);
	(void)Tcl_Eval(interp, bu_vls_addr(&tcl_cmd));
    }

    if (s->height_set) {
	bu_vls_sprintf(&tcl_cmd, "set ::RtWizard::wizard_state(scanlines) %zu", s->height);
	(void)Tcl_Eval(interp, bu_vls_addr(&tcl_cmd));
    }

    if (s->size_set) {
	bu_vls_sprintf(&tcl_cmd, "set ::RtWizard::wizard_state(size) %zu", s->size);
	(void)Tcl_Eval(interp, bu_vls_addr(&tcl_cmd));
	if (!s->width_set) {
	    bu_vls_sprintf(&tcl_cmd, "set ::RtWizard::wizard_state(width) %zu", s->size);
	    (void)Tcl_Eval(interp, bu_vls_addr(&tcl_cmd));
	}
	if (!s->height_set) {
	    bu_vls_sprintf(&tcl_cmd, "set ::RtWizard::wizard_state(scanlines) %zu", s->size);
	    (void)Tcl_Eval(interp, bu_vls_addr(&tcl_cmd));
	}
    }

    rtwizard_set_state_ptbl(interp, "color_objlist", s->color);
    rtwizard_set_state_ptbl(interp, "ghost_objlist", s->ghost);
    rtwizard_set_state_ptbl(interp, "line_objlist", s->line);

    {
	unsigned char rgb[3];
	(void) bu_color_to_rgb_chars(s->bkg_color, (unsigned char *)rgb);
	bu_vls_sprintf(&tcl_cmd, "set ::RtWizard::wizard_state(bg_color) \"%d %d %d\"", (int)rgb[0], (int)rgb[1], (int)rgb[2]);
	(void)Tcl_Eval(interp, bu_vls_addr(&tcl_cmd));
    }
    {
	unsigned char rgb[3];
	(void) bu_color_to_rgb_chars(s->line_color, (unsigned char *)rgb);
	bu_vls_sprintf(&tcl_cmd, "set ::RtWizard::wizard_state(e_color) \"%d %d %d\"", (int)rgb[0], (int)rgb[1], (int)rgb[2]);
	(void)Tcl_Eval(interp, bu_vls_addr(&tcl_cmd));
    }
    {
	unsigned char rgb[3];
	(void) bu_color_to_rgb_chars(s->non_line_color, (unsigned char *)rgb);
	bu_vls_sprintf(&tcl_cmd, "set ::RtWizard::wizard_state(ne_color) \"%d %d %d\"", (int)rgb[0], (int)rgb[1], (int)rgb[2]);
	(void)Tcl_Eval(interp, bu_vls_addr(&tcl_cmd));
    }

    bu_vls_sprintf(&tcl_cmd, "set ::RtWizard::wizard_state(ghost_intensity) %0.15f", s->ghost_intensity);
    (void)Tcl_Eval(interp, bu_vls_addr(&tcl_cmd));

    bu_vls_sprintf(&tcl_cmd, "set ::RtWizard::wizard_state(occmode) %d", s->occlusion);
    (void)Tcl_Eval(interp, bu_vls_addr(&tcl_cmd));

    if (s->benchmark) {
	bu_vls_sprintf(&tcl_cmd, "set ::benchmark_mode \"-B\"");
	(void)Tcl_Eval(interp, bu_vls_addr(&tcl_cmd));
    }

    if (s->cpus) {
	bu_vls_sprintf(&tcl_cmd, "set ::RtWizard::wizard_state(cpu_count) %d", s->cpus);
	(void)Tcl_Eval(interp, bu_vls_addr(&tcl_cmd));
    }

    if (s->az < DBL_MAX) {
	bu_vls_sprintf(&tcl_cmd, "set ::RtWizard::wizard_state(init_azimuth) %0.15f", s->az);
	(void)Tcl_Eval(interp, bu_vls_addr(&tcl_cmd));
    }
    if (s->el < DBL_MAX) {
	bu_vls_sprintf(&tcl_cmd, "set ::RtWizard::wizard_state(init_elevation) %0.15f", s->el);
	(void)Tcl_Eval(interp, bu_vls_addr(&tcl_cmd));
    }
    if (s->tw < DBL_MAX) {
	bu_vls_sprintf(&tcl_cmd, "set ::RtWizard::wizard_state(init_twist) %0.15f", s->tw);
	(void)Tcl_Eval(interp, bu_vls_addr(&tcl_cmd));
    }
    if (s->perspective < DBL_MAX) {
	bu_vls_sprintf(&tcl_cmd, "set ::RtWizard::wizard_state(perspective) %0.15f", s->perspective);
	(void)Tcl_Eval(interp, bu_vls_addr(&tcl_cmd));
    }
    if (s->zoom < DBL_MAX) {
	bu_vls_sprintf(&tcl_cmd, "set ::RtWizard::wizard_state(zoom) %0.15f", s->zoom);
	(void)Tcl_Eval(interp, bu_vls_addr(&tcl_cmd));
    }
    if (s->center[0] < DBL_MAX) {
	bu_vls_sprintf(&tcl_cmd, "set ::RtWizard::wizard_state(x_center) %0.15f", s->center[0]);
	(void)Tcl_Eval(interp, bu_vls_addr(&tcl_cmd));
	bu_vls_sprintf(&tcl_cmd, "set ::RtWizard::wizard_state(y_center) %0.15f", s->center[1]);
	(void)Tcl_Eval(interp, bu_vls_addr(&tcl_cmd));
	bu_vls_sprintf(&tcl_cmd, "set ::RtWizard::wizard_state(z_center) %0.15f", s->center[2]);
	(void)Tcl_Eval(interp, bu_vls_addr(&tcl_cmd));
    }


    if (s->viewsize < DBL_MAX) {
	bu_vls_sprintf(&tcl_cmd, "set ::RtWizard::wizard_state(viewsize) %0.15f", s->viewsize);
	(void)Tcl_Eval(interp, bu_vls_addr(&tcl_cmd));
    }

    if (s->orientation[0] < DBL_MAX) {
	bu_vls_sprintf(&tcl_cmd, "set ::RtWizard::wizard_state(orientation) \"%0.15f %0.15f %0.15f %0.15f\"", s->orientation[0], s->orientation[1], s->orientation[2], s->orientation[3]);
	(void)Tcl_Eval(interp, bu_vls_addr(&tcl_cmd));
    }

    if (s->eye_pt[0] < DBL_MAX) {
	bu_vls_sprintf(&tcl_cmd, "set ::RtWizard::wizard_state(eye_pt) \"%0.15f %0.15f %0.15f\"", s->eye_pt[0], s->eye_pt[1], s->eye_pt[2]);
	(void)Tcl_Eval(interp, bu_vls_addr(&tcl_cmd));
    }

    if (bu_vls_strlen(s->log_file)) {
	rtwizard_set_state(interp, "log_file", bu_vls_addr(s->log_file));
    }

    if (bu_vls_strlen(s->pid_file)) {
	rtwizard_set_state(interp, "pid_filename", bu_vls_addr(s->pid_file));
    }


}


static void
rtwizard_help_group(struct bu_vls *out, const char *title, const char * const *options)
{
    char *option_help = bu_cmd_schema_describe_selected(&rtwizard_schema, options);

    if (option_help) {
	bu_vls_printf(out, "%s:\n%s\n", title, option_help);
	bu_free(option_help, "rtwizard help");
    }
}


/* Help message printed when -h option is supplied. */
static void
rtwizard_help(void)
{
    static const char *const input_output[] = {
	"help", "help-dev", "input-file", "output-file", "size", "width", "height", NULL
    };
    static const char *const visualization[] = {
	"type", "color-objects", "line-objects", "ghost-objects", NULL
    };
    static const char *const view_setup[] = {
	"azimuth", "elevation", "twist", "zoom", "center", "eye_pt", "viewsize", "orientation", "perspective", NULL
    };
    static const char *const style[] = {
	"background-color", "line-color", "non-line-color", "ghost-intensity", "occlusion", NULL
    };
    static const char *const display[] = {
	"gui", "no-gui", "fbserv-device", "fbserv-port", "verbose", NULL
    };
    struct bu_vls out = BU_VLS_INIT_ZERO;

    bu_vls_strcat(&out, "\nUsage: rtwizard [options]\n\n");
    rtwizard_help_group(&out, "Input/Output Options", input_output);
    rtwizard_help_group(&out, "Visualization Options", visualization);
    rtwizard_help_group(&out, "View Setup Options", view_setup);
    rtwizard_help_group(&out, "Style Options", style);
    rtwizard_help_group(&out, "Display Options", display);
    bu_log("%s", bu_vls_addr(&out));
    bu_vls_free(&out);
}


/* Help message printed when --help-dev option is supplied. */
static void
rtwizard_help_dev(void)
{
    static const char *const development[] = {
	"benchmark", "cpu-count", "pid-file", "log-file", NULL
    };
    struct bu_vls out = BU_VLS_INIT_ZERO;

    bu_vls_strcat(&out, "\nUsage: rtwizard [options]\n\n");
    rtwizard_help_group(&out, "Options for developers", development);
    bu_log("%s", bu_vls_addr(&out));
    bu_vls_free(&out);
}


#define RCFILE  ".rtwizardrc"
static int
rtwizard_rc(Tcl_Interp *interp)
{
    FILE *fp = NULL;
    char *path;
    struct bu_vls str = BU_VLS_INIT_ZERO;


    if ((path = getenv("RTWIZARD_RCFILE")) != (char *)NULL) {
	if ((fp = fopen(path, "r")) != NULL) {
	    bu_vls_strcpy(&str, path);
	}
    }

    if (!fp) {
	if ((path = getenv("HOME")) != (char *)NULL) {
	    bu_vls_strcpy(&str, path);
	    bu_vls_strcat(&str, "/");
	    bu_vls_strcat(&str, RCFILE);

	    fp = fopen(bu_vls_addr(&str), "r");
	}
    }

    if (!fp) {
	if ((fp = fopen(RCFILE, "r")) != NULL) {
	    bu_vls_strcpy(&str, RCFILE);
	}
    }

    /* At this point, if none of the above attempts panned out, give up. */

    if (!fp) {
	bu_vls_free(&str);
	return -1;
    }
    fclose(fp);

    if (Tcl_EvalFile(interp, bu_vls_addr(&str)) != TCL_OK) {
	bu_log("Error reading %s:\n%s\n", RCFILE,
	       Tcl_GetVar(interp, "errorInfo", TCL_GLOBAL_ONLY));
    }

    bu_vls_free(&str);

    return 0;
}


int
main(int argc, char **argv)
{
    char *av0;
    char type = '\0';
    int i = 0;
    int operand_index;
    int operand_argc;
    const char **operand_argv;

    struct bu_vls optparse_msg = BU_VLS_INIT_ZERO;
    struct bu_vls info_msg = BU_VLS_INIT_ZERO;
    struct rtwizard_settings *s = rtwizard_settings_create();
    struct rtwizard_cli args;

    /* initialize progname for run-time resource finding */
    bu_setprogname(argv[0]);
    av0 = argv[0];

    /* Change the working directory to BU_DIR_HOME if we are invoking
     * without any arguments. */
    if (argc == 1) {
	const char *homed = bu_dir(NULL, 0, BU_DIR_HOME, NULL);
	if (homed && chdir(homed)) {
	    bu_exit(1, "Failed to change working directory to \"%s\" ", homed);
	}
    }

    /* Skip first arg */
    argv++; argc--;

	/* Settings owns application state; args is the native schema's direct,
	 * reentrant binding record. */
    rtwizard_cli_init(&args, s);
    operand_index = bu_cmd_schema_parse(&rtwizard_schema, &args, &optparse_msg,
	argc, (const char **)argv);
    if (operand_index < 0) {
	rtwizard_cli_free(&args);
	bu_exit(EXIT_FAILURE, "%s", bu_vls_addr(&optparse_msg));
    }
    bu_vls_free(&optparse_msg);

	if (args.help) {
	rtwizard_cli_free(&args);
	rtwizard_help();
	bu_exit(EXIT_SUCCESS, NULL);
    }

	if (args.help_dev) {
	rtwizard_cli_free(&args);
	rtwizard_help_dev();
	bu_exit(EXIT_SUCCESS, NULL);
    }

    type = args.type;
    rtwizard_cli_apply(s, &args);
    rtwizard_cli_free(&args);
    operand_argc = argc - operand_index;
    operand_argv = (const char **)(argv + operand_index);

    if (type != '\0') {
	bu_log("Image type: %c\n", type);
    }

    if (s->use_gui && s->no_gui) {
	bu_log("WARNING: both -gui and -no-gui supplied - enabling gui\n");
	s->no_gui = 0;
    }

    if (bu_vls_strlen(s->input_file) && !bu_file_exists(bu_vls_addr(s->input_file), NULL)) {
	bu_exit(EXIT_FAILURE, "ERROR: Specified %s as .g file, but file does not exist.\n", bu_vls_addr(s->input_file));
    }

    /* Handle any leftover arguments per established conventions */
    for (i = 0; i < operand_argc; i++) {
	struct bu_vls c = BU_VLS_INIT_ZERO;
	/* First, see if we have an input .g file */
	if (bu_vls_strlen(s->input_file) == 0) {
	    if (bu_path_component(&c, operand_argv[i], BU_PATH_EXT)) {
		if (bu_file_mime(bu_vls_addr(&c), BU_MIME_MODEL) == BU_MIME_MODEL_VND_BRLCAD_PLUS_BINARY) {
		    if (bu_file_exists(operand_argv[i], NULL)) {
			bu_vls_sprintf(s->input_file, "%s", operand_argv[i]);
			/* This was the .g name - don't add it to the color list */
			continue;
		    } else {
			bu_exit(EXIT_FAILURE, "ERROR: Specified %s as .g file, but file does not exist.\n", operand_argv[i]);
		    }
		}
	    }
	}
	bu_vls_trunc(&c, 0);
	/* Next, see if we have an image specified as an output destination */
	if (bu_vls_strlen(s->output_file) == 0 && bu_vls_strlen(s->fb_dev) == 0) {
	    if (bu_path_component(&c, operand_argv[i], BU_PATH_EXT)) {
		if (rtwizard_imgformat_supported(bu_file_mime(bu_vls_addr(&c), BU_MIME_IMAGE))) {
		    bu_vls_sprintf(s->output_file, "%s", operand_argv[i]);
		    /* This looks like the output image name - don't add it to the color list */
		    continue;
		}
	    }
	}
	/* If it's none of the above, assume a color object in the .g file */
	bu_ptbl_ins(s->color, (long *)bu_strdup(operand_argv[i]));
    }

    if (rtwizard_view_opts_check(&info_msg, s)) {
	bu_log("%s\n", bu_vls_addr(&info_msg));
	bu_vls_trunc(&info_msg, 0);
    }

    if (!s->use_gui && !rtwizard_info_sufficient(&info_msg, s, type)) {
	if ((!s->use_gui) && (!s->no_gui)) {
	    s->use_gui = 1;
	} else {
	    bu_log("%s", bu_vls_addr(&info_msg));
	    bu_vls_free(&info_msg);
	    bu_exit(EXIT_FAILURE, "ERROR: insufficient information to generate image");
	}
    }
    bu_vls_free(&info_msg);

    /*print_rtwizard_state(s);*/

    /* For now, all roads lead to Tcl. */

    {
	int status = 0;
	struct bu_vls tlog = BU_VLS_INIT_ZERO;
	const char *rtwizard = NULL;
	const char *fullname = NULL;
	const char *result = NULL;
	Tcl_DString temp;
	Tcl_Interp *interp = Tcl_CreateInterp();


	/* The subsequent Tcl scripts will take of Tk, so at this
	 * level we need only the standard init */
	status = tclcad_init(interp, 0, &tlog);
	if (status == TCL_ERROR) {
	    bu_log("tclcad init failure:\n%s\n", bu_vls_addr(&tlog));
	}
	bu_vls_free(&tlog);

	/* Normalize .g and output image file paths, since they're to be used
	 * in Tcl scripts */
	if (bu_vls_strlen(s->input_file) > 0) {
	    Tcl_Obj *initPath, *normalPath;
	    initPath = Tcl_NewStringObj(bu_vls_addr(s->input_file), (int)bu_vls_strlen(s->input_file));
	    Tcl_IncrRefCount(initPath);
	    normalPath = Tcl_FSGetNormalizedPath(interp, initPath);
	    bu_vls_sprintf(s->input_file, "%s", Tcl_GetString(normalPath));
	    Tcl_DecrRefCount(initPath);
	}
	if (bu_vls_strlen(s->output_file) > 0) {
	    Tcl_Obj *initPath, *normalPath;
	    initPath = Tcl_NewStringObj(bu_vls_addr(s->output_file), (int)bu_vls_strlen(s->output_file));
	    Tcl_IncrRefCount(initPath);
	    normalPath = Tcl_FSGetNormalizedPath(interp, initPath);
	    bu_vls_sprintf(s->output_file, "%s", Tcl_GetString(normalPath));
	    Tcl_DecrRefCount(initPath);
	}

	/* Set a single argv so the Tcl scripts will run the main proc.  Not passing more args
	 * because they can apparently cause problems with Tcl script execution. */
	tclcad_set_argv(interp, 1, (const char **)&av0);

#if defined(_WIN32) && !defined(__CYGWIN__)
	rtwizard_disable_std_handle_inheritance();
#endif

	Init_RtWizard_Vars(interp, s);
	if (s->port < 0) {
	    (void)rtwizard_setup_ipc(interp);
	}

	/* If we have a .rtwizardrc file, get the previous size settings from it */
	rtwizard_rc(interp);

	/* We're using this path on the file system, not in Tcl: translate it
	 * to the appropriate form before doing the eval */
	Tcl_DStringInit(&temp);
	rtwizard = bu_dir(NULL, 0, BU_DIR_DATA, "tclscripts", "rtwizard", "rtwizard", NULL);
	fullname = Tcl_TranslateFileName(interp, rtwizard, &temp);
	status = Tcl_EvalFile(interp, fullname);
	Tcl_DStringFree(&temp);

	result = Tcl_GetStringResult(interp);
	if (status != TCL_OK) {
	    const char *error_info = Tcl_GetVar(interp, "errorInfo", TCL_GLOBAL_ONLY);
	    bu_log("rtwizard Tcl script returned status %d\n", status);
	    if (strlen(result) > 0)
		bu_log("%s\n", result);
	    if (error_info && strlen(error_info) > 0)
		bu_log("%s\n", error_info);
	}

	/*Tcl_DeleteInterp(interp);*/
	return status;
    }

    /* Someday, we want to do this without Tcl via library calls unless
     * the GUI is needed... */
#if 0
    /* At this point, if we know we're supposed to launch the GUI, do it */
    if (s->use_gui) {
	/* launch gui */
    } else {
	/* Check that we know enough to make an image. */
	if (!rtwizard_info_sufficient(NULL, s, type)) {
	    /* If we *can* launch the GUI in this situation, do it */
	    if (s->no_gui) {
		bu_exit(EXIT_FAILURE, "ERROR: Image type %c specified, but supplied inputs are insufficient for generating a type %c image.\n", type, type);
	    } else {
		/* Launch GUI */
	    }
	}
	/* We know enough - make our image */
    }
#endif
    return 0;
}


/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
