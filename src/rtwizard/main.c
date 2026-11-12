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
#include "bu/file.h"
#include "bu/mime.h"
#include "bu/malloc.h"
#include "bu/log.h"
#include "bu/path.h"
#include "bu/ptbl.h"
#include "bu/arg.h"
#include "bu/str.h"
#include "bv/vlist.h"
#include "icv.h"
#include "icv/anim.h"
#include "pkg.h"
#include "raytrace.h"
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

    /* Cutting plane animation */
    int cut_steps;
    int animation_fps;
    vect_t cut_direction;
    int cut_direction_set;

    /* Raytrace enhancements */
    int ao_samples;
    double ao_radius;

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
    s->cut_steps = 0;
    s->animation_fps = 10;
    VSETALL(s->cut_direction, 0.0);
    s->cut_direction_set = 0;
    s->ao_samples = 0;
    s->ao_radius = 0.0;

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


struct rtwizard_args {
    int help;
    int help_dev;
    struct bu_vls input_file;
    struct bu_vls output_file;
    size_t size;
    size_t width;
    size_t height;
    char type;
    struct bu_ptbl *color;
    struct bu_ptbl *line;
    struct bu_ptbl *ghost;
    fastf_t azimuth;
    fastf_t elevation;
    fastf_t twist;
    fastf_t zoom;
    vect_t center;
    vect_t eye_pt;
    fastf_t viewsize;
    quat_t orientation;
    fastf_t perspective;
    struct bu_color background_color;
    struct bu_color line_color;
    struct bu_color non_line_color;
    fastf_t ghost_intensity;
    int occlusion;
    int use_gui;
    int no_gui;
    struct bu_vls fb_device;
    int port;
    int benchmark;
    int cpus;
    struct bu_vls pid_file;
    struct bu_vls log_file;
    int verbose;
    int cut_steps;
    vect_t cut_direction;
    int animation_fps;
    int ao_samples;
    fastf_t ao_radius;
};


static int
rtwizard_size_from_str(struct bu_vls *msg, const char *arg, void *storage)
{
    size_t value;
    size_t *target = storage ? (size_t *)storage : &value;
    return bu_arg_size_t_from_str(msg, target, arg, 0) ? 0 : -1;
}


static int
rtwizard_objects_from_str(struct bu_vls *msg, const char *arg, void *storage)
{
    size_t i = 0;
    char *objs = NULL;
    size_t acnum = 0;
    char **avnum;
    struct bu_ptbl *table = storage ? *(struct bu_ptbl **)storage : NULL;

    if (!arg || !arg[0]) {
	if (msg)
	    bu_vls_printf(msg, "expected an object name or object list\n");
	return -1;
    }

    objs = bu_strdup(arg);

    while (objs[i]) {
	/* If we have a separator or a quote, replace with a space */
	if (objs[i] == ',' || objs[i] == ';' || objs[i] == '\'' || objs[i] == '\"') {
	    if (i == 0 || objs[i-1] != '\\')
		objs[i] = ' ';
	}
	i++;
    }

    avnum = (char **)bu_calloc(strlen(objs) + 1, sizeof(char *), "breakout array");
    acnum = bu_argv_from_string(avnum, strlen(objs), objs);

    /* TODO - use quote/unquote routines to scrub names... */

    for (i = 0; i < acnum; i++) {
	if (table)
	    bu_ptbl_ins(table, (long *)bu_strdup(avnum[i]));
    }
    bu_free(objs, "string dup");
    bu_free(avnum, "array memory");

    if (!acnum && msg)
	bu_vls_printf(msg, "expected at least one object name\n");
    return acnum > 0 ? 0 : -1;
}


static int
rtwizard_type_from_str(struct bu_vls *msg, const char *arg, void *storage)
{
    if (!arg || strlen(arg) != 1) {
	if (msg)
	    bu_vls_printf(msg, "invalid RtWizard picture type: %s\n", arg ? arg : "");
	return -1;
    }

    if (arg[0] < 'A' || arg[0] > 'F') {
	if (msg)
	    bu_vls_printf(msg, "invalid RtWizard picture type: %c\n", arg[0]);
	return -1;
    }

    if (storage)
	*(char *)storage = arg[0];
    return 0;
}


static int
rtwizard_quaternion_packed(struct bu_vls *msg, const char *arg, fastf_t *quaternion)
{
    char *copy;
    char *values[6] = {NULL, NULL, NULL, NULL, NULL, NULL};
    fastf_t parsed[4];
    size_t count;
    size_t i;

    if (!arg || !arg[0])
	return 0;

    copy = bu_strdup(arg);
    for (i = 0; copy[i]; i++) {
	if (copy[i] == ',' || copy[i] == '/')
	    copy[i] = ' ';
    }
    count = bu_argv_from_string(values, 5, copy);
    if (count != 4) {
	bu_free(copy, "quaternion parse buffer");
	return 0;
    }

    for (i = 0; i < 4; i++) {
	if (!bu_arg_number_from_str(msg, &parsed[i], values[i])) {
	    bu_free(copy, "quaternion parse buffer");
	    return 0;
	}
    }
    if (quaternion)
	HMOVE(quaternion, parsed);
    bu_free(copy, "quaternion parse buffer");
    return 1;
}


static size_t
rtwizard_quaternion_count(size_t available, const char **argv)
{
    if (!available || !argv || !argv[0])
	return 0;
    if (rtwizard_quaternion_packed(NULL, argv[0], NULL))
	return 1;
    return available < 4 ? available : 4;
}


static int
rtwizard_quaternion_consume(struct bu_vls *msg, size_t argc, const char **argv, void *storage)
{
    fastf_t parsed[4];
    fastf_t *quaternion = storage ? (fastf_t *)storage : parsed;
    size_t i;

    if (argc == 1 && rtwizard_quaternion_packed(msg, argv[0], quaternion))
	return 0;
    if (argc != 4) {
	if (msg)
	    bu_vls_printf(msg, "expected a quaternion as qx/qy/qz/qw or four numbers\n");
	return -1;
    }
    for (i = 0; i < 4; i++) {
	if (!bu_arg_number_from_str(msg, &quaternion[i], argv[i]))
	    return -1;
    }
    return 0;
}


static const struct bu_arg_variant rtwizard_quaternion_variants[] = {
    BU_ARG_VARIANT("packed", "qx/qy/qz/qw", 1, "Packed quaternion"),
    BU_ARG_VARIANT("components", "qx qy qz qw", 4, "Four quaternion components"),
    BU_ARG_VARIANT_NULL
};

static const struct bu_arg_shape rtwizard_quaternion_shape = BU_ARG_SHAPE(
    BU_ARG_SHAPE_CUSTOM, 1, 4, "a quaternion", rtwizard_quaternion_count,
    rtwizard_quaternion_variants);


static const struct bu_arg_option rtwizard_options[] = {
    BU_ARG_FLAG("h", "help", struct rtwizard_args, help,
	"Print options help and exit"),
    BU_ARG_ALIAS("?", NULL, "help", 1),
    BU_ARG_FLAG(NULL, "help-dev", struct rtwizard_args, help_dev,
	"Print development and programmatic options"),
    BU_ARG_VALUE("i", "input-file", struct rtwizard_args, input_file,
	BU_ARG_VALUE_VLS, BU_ARG_REQUIRED, "filename", "Input .g database file"),
    BU_ARG_VALUE("o", "output-file", struct rtwizard_args, output_file,
	BU_ARG_VALUE_VLS, BU_ARG_REQUIRED, "filename", "Image output file name"),
    BU_ARG_CUSTOM("s", "size", struct rtwizard_args, size,
	rtwizard_size_from_str, "pixels", "Output width and height for a square image"),
    BU_ARG_CUSTOM("w", "width", struct rtwizard_args, width,
	rtwizard_size_from_str, "pixels", "Output image width (overrides --size)"),
    BU_ARG_CUSTOM("n", "height", struct rtwizard_args, height,
	rtwizard_size_from_str, "pixels", "Output image height (overrides --size)"),
    BU_ARG_CUSTOM("t", "type", struct rtwizard_args, type,
	rtwizard_type_from_str, "A|B|C|D|E|F", "Specify RtWizard picture type"),
    BU_ARG_CUSTOM("c", "color-objects", struct rtwizard_args, color,
	rtwizard_objects_from_str, "object-list", "List of color objects"),
    BU_ARG_CUSTOM("l", "line-objects", struct rtwizard_args, line,
	rtwizard_objects_from_str, "object-list", "List of line objects"),
    BU_ARG_CUSTOM("g", "ghost-objects", struct rtwizard_args, ghost,
	rtwizard_objects_from_str, "object-list", "List of ghost objects"),
    BU_ARG_VALUE("a", "azimuth", struct rtwizard_args, azimuth,
	BU_ARG_VALUE_NUMBER, BU_ARG_REQUIRED, "degrees", "Set azimuth"),
    BU_ARG_VALUE("e", "elevation", struct rtwizard_args, elevation,
	BU_ARG_VALUE_NUMBER, BU_ARG_REQUIRED, "degrees", "Set elevation"),
    BU_ARG_VALUE(NULL, "twist", struct rtwizard_args, twist,
	BU_ARG_VALUE_NUMBER, BU_ARG_REQUIRED, "degrees", "Set twist"),
    BU_ARG_VALUE("z", "zoom", struct rtwizard_args, zoom,
	BU_ARG_VALUE_NUMBER, BU_ARG_REQUIRED, "factor", "Set zoom"),
    BU_ARG_VECTOR3(NULL, "center", struct rtwizard_args, center, "x/y/z",
	"Set view center"),
    BU_ARG_VECTOR3(NULL, "eye_pt", struct rtwizard_args, eye_pt, "x/y/z",
	"Set eye point"),
    BU_ARG_VALUE(NULL, "viewsize", struct rtwizard_args, viewsize,
	BU_ARG_VALUE_NUMBER, BU_ARG_REQUIRED, "size", "Set view size"),
    BU_ARG_OPTION_SHAPED(NULL, "orientation", "orientation",
	struct rtwizard_args, orientation, BU_ARG_VALUE_VECTOR, "quaternion",
	"Set view orientation", BU_ARG_REQUIRED, &rtwizard_quaternion_shape,
	rtwizard_quaternion_consume),
    BU_ARG_VALUE("P", "perspective", struct rtwizard_args, perspective,
	BU_ARG_VALUE_NUMBER, BU_ARG_REQUIRED, "degrees", "Set perspective"),
    BU_ARG_COLOR_COMPAT("C", "background-color", struct rtwizard_args,
	background_color, "color", "Background image color"),
    BU_ARG_COLOR_COMPAT(NULL, "line-color", struct rtwizard_args, line_color,
	"color", "Color used for line rendering"),
    BU_ARG_COLOR_COMPAT(NULL, "non-line-color", struct rtwizard_args,
	non_line_color, "color", "Color used for non-line rendering"),
    BU_ARG_VALUE("G", "ghost-intensity", struct rtwizard_args, ghost_intensity,
	BU_ARG_VALUE_NUMBER, BU_ARG_REQUIRED, "value", "Intensity of ghost objects"),
    BU_ARG_VALUE("O", "occlusion", struct rtwizard_args, occlusion,
	BU_ARG_VALUE_INTEGER, BU_ARG_REQUIRED, "mode", "Occlusion mode (1 to 3)"),
    BU_ARG_FLAG(NULL, "gui", struct rtwizard_args, use_gui, "Force use of GUI"),
    BU_ARG_FLAG(NULL, "no-gui", struct rtwizard_args, no_gui,
	"Do not use the GUI"),
    BU_ARG_VALUE("d", "fbserv-device", struct rtwizard_args, fb_device,
	BU_ARG_VALUE_VLS, BU_ARG_REQUIRED, "device", "Framebuffer device"),
    BU_ARG_VALUE("p", "fbserv-port", struct rtwizard_args, port,
	BU_ARG_VALUE_INTEGER, BU_ARG_REQUIRED, "port", "Framebuffer port"),
    BU_ARG_FLAG(NULL, "benchmark", struct rtwizard_args, benchmark,
	"Benchmark mode (no randomness)"),
    BU_ARG_VALUE(NULL, "cpu-count", struct rtwizard_args, cpus,
	BU_ARG_VALUE_INTEGER, BU_ARG_REQUIRED, "count", "Number of CPUs to use"),
    BU_ARG_VALUE(NULL, "pid-file", struct rtwizard_args, pid_file,
	BU_ARG_VALUE_VLS, BU_ARG_REQUIRED, "filename", "PID tracking file"),
    BU_ARG_VALUE(NULL, "log-file", struct rtwizard_args, log_file,
	BU_ARG_VALUE_VLS, BU_ARG_REQUIRED, "filename", "Debug log file"),
    BU_ARG_VALUE("v", "verbose", struct rtwizard_args, verbose,
	BU_ARG_VALUE_INTEGER, BU_ARG_REQUIRED, "level", "Verbosity"),
    BU_ARG_VALUE(NULL, "cut-steps", struct rtwizard_args, cut_steps,
	BU_ARG_VALUE_INTEGER, BU_ARG_REQUIRED, "count", "Cutting-plane animation frame count"),
    BU_ARG_VECTOR3(NULL, "cut-direction", struct rtwizard_args, cut_direction,
	"x/y/z", "Direction in which the cutting plane advances"),
    BU_ARG_VALUE(NULL, "animation-fps", struct rtwizard_args, animation_fps,
	BU_ARG_VALUE_INTEGER, BU_ARG_REQUIRED, "fps", "Animation frames per second"),
    BU_ARG_VALUE(NULL, "ao-samples", struct rtwizard_args, ao_samples,
	BU_ARG_VALUE_INTEGER, BU_ARG_REQUIRED, "count", "Ambient occlusion samples per ray"),
    BU_ARG_VALUE(NULL, "ao-radius", struct rtwizard_args, ao_radius,
	BU_ARG_VALUE_NUMBER, BU_ARG_REQUIRED, "radius", "Ambient occlusion maximum radius"),
    BU_ARG_OPTION_NULL
};

static const struct bu_arg_operand rtwizard_operands[] = {
    BU_ARG_OPERAND("input-or-object", BU_ARG_VALUE_RAW, 0, BU_ARG_COUNT_UNLIMITED,
	"Input database, output image, or color object", NULL),
    BU_ARG_OPERAND_NULL
};

static const struct bu_arg_schema rtwizard_schema = BU_ARG_SCHEMA(
    "rtwizard", "Generate raytraced model visualizations", rtwizard_options,
    rtwizard_operands, BU_ARG_PARSE_INTERSPERSED,
    BU_ARG_SCHEMA_META_HELP(NULL, NULL, NULL, NULL, NULL));


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
    bu_vls_printf(&slog, "cut steps: %d\n", s->cut_steps);
    bu_vls_printf(&slog, "animation fps: %d\n", s->animation_fps);
    bu_vls_printf(&slog, "cut direction: %f, %f, %f\n", V3ARGS(s->cut_direction));
    bu_vls_printf(&slog, "ao samples: %d\n", s->ao_samples);
    bu_vls_printf(&slog, "ao radius: %f\n", s->ao_radius);

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


static icv_anim_format_t rtwizard_anim_format(const char *path);


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


static int
rtwizard_anim_path_supported(const char *path)
{
    return (rtwizard_anim_format(path) != ICV_ANIM_UNKNOWN);
}


static int
rtwizard_anim_only_path(const char *path)
{
    const char *ext = strrchr(path, '.');
    return (ext && (BU_STR_EQUIV(ext, ".apng") || BU_STR_EQUIV(ext, ".avi") ||
	BU_STR_EQUIV(ext, ".mjpg")));
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


static icv_anim_format_t
rtwizard_anim_format(const char *path)
{
    const char *ext = strrchr(path, '.');

    if (!ext)
	return ICV_ANIM_UNKNOWN;
    if (BU_STR_EQUIV(ext, ".apng") || BU_STR_EQUIV(ext, ".png"))
	return ICV_ANIM_APNG;
    if (BU_STR_EQUIV(ext, ".avi") || BU_STR_EQUIV(ext, ".mjpg"))
	return ICV_ANIM_MJPG;

    return ICV_ANIM_UNKNOWN;
}


/* Tcl bridge used by the orchestration script after all PIX frames have
 * been rendered.  Keeping encoding here lets rtwizard use its linked libicv
 * directly rather than repeatedly invoking the icv command line tool. */
static int
rtwizard_anim_write_cmd(ClientData UNUSED(client_data), Tcl_Interp *interp, int objc, Tcl_Obj *const objv[])
{
    icv_anim_t *anim = NULL;
    icv_anim_format_t fmt;
    const char *output;
    int width, height, fps;
    int i;

    if (objc < 6) {
	Tcl_SetObjResult(interp, Tcl_NewStringObj(
		"usage: rtwizard_anim_write output width height fps frame1 ?frame2 ...?", -1));
	return TCL_ERROR;
    }

    output = Tcl_GetString(objv[1]);
    if (Tcl_GetIntFromObj(interp, objv[2], &width) != TCL_OK ||
	Tcl_GetIntFromObj(interp, objv[3], &height) != TCL_OK ||
	Tcl_GetIntFromObj(interp, objv[4], &fps) != TCL_OK)
	return TCL_ERROR;

    if (width <= 0 || height <= 0 || fps <= 0) {
	Tcl_SetObjResult(interp, Tcl_NewStringObj("animation width, height, and fps must be positive", -1));
	return TCL_ERROR;
    }

    fmt = rtwizard_anim_format(output);
    if (fmt == ICV_ANIM_UNKNOWN) {
	Tcl_SetObjResult(interp, Tcl_NewStringObj(
		"animation output must use .apng, .png, .avi, or .mjpg", -1));
	return TCL_ERROR;
    }

    anim = icv_anim_create(fmt, (uint32_t)width, (uint32_t)height, fps);
    if (!anim) {
	Tcl_SetObjResult(interp, Tcl_NewStringObj("failed to create libicv animation", -1));
	return TCL_ERROR;
    }

    for (i = 5; i < objc; i++) {
	const char *frame = Tcl_GetString(objv[i]);
	icv_image_t *img = icv_read(frame, BU_MIME_IMAGE_PIX, (size_t)width, (size_t)height);
	if (!img || icv_anim_add_frame(anim, img) != 0) {
	    if (img)
		icv_destroy(img);
	    icv_anim_destroy(anim);
	    Tcl_SetObjResult(interp, Tcl_ObjPrintf("failed to add animation frame '%s'", frame));
	    return TCL_ERROR;
	}
	icv_destroy(img);
    }

    if (icv_anim_write(anim, output) != 0) {
	icv_anim_destroy(anim);
	(void)bu_file_delete(output);
	Tcl_SetObjResult(interp, Tcl_ObjPrintf("failed to write animation '%s'", output));
	return TCL_ERROR;
    }

    icv_anim_destroy(anim);
    return TCL_OK;
}


struct rtwizard_bound_data {
    point_t bmin;
    point_t bmax;
    int have_bounds;
    int error;
};


static union tree *
rtwizard_bound_leaf(struct db_tree_state *tsp, const struct db_full_path *UNUSED(pathp), struct rt_db_internal *ip, void *client_data)
{
    struct rtwizard_bound_data *bd = (struct rtwizard_bound_data *)client_data;
    point_t bmin, bmax;
    int bret = -1;
    union tree *tp;
    struct soltab *stp;
    struct soltab st_init = RT_SOLTAB_INIT_ZERO;

    VSETALL(bmin, INFINITY);
    VSETALL(bmax, -INFINITY);
    if (ip->idb_meth->ft_bbox)
	bret = ip->idb_meth->ft_bbox(ip, &bmin, &bmax, tsp->ts_tol);
    if (bret < 0 && ip->idb_meth->ft_plot) {
	struct bu_list vhead;
	BU_LIST_INIT(&vhead);
	if (ip->idb_meth->ft_plot(&vhead, ip, tsp->ts_ttol, tsp->ts_tol, NULL) >= 0 &&
	    bv_vlist_bbox(&vhead, &bmin, &bmax, NULL, NULL) == 0)
	    bret = 0;
    }
    if (bret < 0) {
	bd->error = 1;
	return TREE_NULL;
    }

    BU_GET(stp, struct soltab);
    *stp = st_init;
    stp->l.magic = RT_SOLTAB_MAGIC;
    stp->l2.magic = RT_SOLTAB2_MAGIC;
    stp->st_aradius = 1.0;
    VMOVE(stp->st_min, bmin);
    VMOVE(stp->st_max, bmax);

    BU_GET(tp, union tree);
    RT_TREE_INIT(tp);
    tp->tr_op = OP_SOLID;
    tp->tr_a.tu_stp = stp;
    return tp;
}


static int
rtwizard_bound_tree(const union tree *tp, point_t bmin, point_t bmax)
{
    point_t rmin, rmax;

    VSETALL(bmin, INFINITY);
    VSETALL(bmax, -INFINITY);
    VSETALL(rmin, INFINITY);
    VSETALL(rmax, -INFINITY);
    if (!tp)
	return -1;

    switch (tp->tr_op) {
	case OP_SOLID:
	    VMOVE(bmin, tp->tr_a.tu_stp->st_min);
	    VMOVE(bmax, tp->tr_a.tu_stp->st_max);
	    return 0;
	case OP_UNION:
	case OP_XOR:
	    if (rtwizard_bound_tree(tp->tr_b.tb_left, bmin, bmax) < 0 ||
		rtwizard_bound_tree(tp->tr_b.tb_right, rmin, rmax) < 0)
		return -1;
	    VMIN(bmin, rmin);
	    VMAX(bmax, rmax);
	    return 0;
	case OP_INTERSECT:
	    if (rtwizard_bound_tree(tp->tr_b.tb_left, bmin, bmax) < 0 ||
		rtwizard_bound_tree(tp->tr_b.tb_right, rmin, rmax) < 0)
		return -1;
	    VMAX(bmin, rmin);
	    VMIN(bmax, rmax);
	    return 0;
	case OP_SUBTRACT:
	    return rtwizard_bound_tree(tp->tr_b.tb_left, bmin, bmax);
	case OP_GUARD:
	case OP_XNOP:
	    return rtwizard_bound_tree(tp->tr_b.tb_left, bmin, bmax);
	case OP_NOT:
	    VSETALL(bmin, -INFINITY);
	    VSETALL(bmax, INFINITY);
	    return 0;
	case OP_NOP:
	default:
	    return -1;
    }
}


static void
rtwizard_bound_tree_clear(union tree *tp, int keep_root)
{
    if (!tp)
	return;

    switch (tp->tr_op) {
	case OP_SOLID:
	    if (tp->tr_a.tu_stp)
		BU_PUT(tp->tr_a.tu_stp, struct soltab);
	    break;
	case OP_UNION:
	case OP_XOR:
	case OP_INTERSECT:
	case OP_SUBTRACT:
	    rtwizard_bound_tree_clear(tp->tr_b.tb_left, 0);
	    rtwizard_bound_tree_clear(tp->tr_b.tb_right, 0);
	    break;
	case OP_GUARD:
	case OP_XNOP:
	case OP_NOT:
	    rtwizard_bound_tree_clear(tp->tr_b.tb_left, 0);
	    break;
	default:
	    break;
    }

    if (keep_root) {
	RT_TREE_INIT(tp);
	tp->tr_op = OP_NOP;
    } else {
	BU_PUT(tp, union tree);
    }
}


static union tree *
rtwizard_bound_region_end(struct db_tree_state *UNUSED(tsp), const struct db_full_path *UNUSED(pathp), union tree *curtree, void *client_data)
{
    struct rtwizard_bound_data *bd = (struct rtwizard_bound_data *)client_data;
    point_t bmin, bmax;

    if (rtwizard_bound_tree(curtree, bmin, bmax) == 0) {
	if (!bd->have_bounds) {
	    VMOVE(bd->bmin, bmin);
	    VMOVE(bd->bmax, bmax);
	    bd->have_bounds = 1;
	} else {
	    VMIN(bd->bmin, bmin);
	    VMAX(bd->bmax, bmax);
	}
    }
    rtwizard_bound_tree_clear(curtree, 1);
    return curtree;
}


/* Return the min/max projection of the requested object trees along a model
 * space direction.  Applying the direction-alignment rotation as the tree's
 * initial matrix avoids the loose diagonal projection of an axis-aligned
 * model bounding box. */
static int
rtwizard_cut_bounds_cmd(ClientData UNUSED(client_data), Tcl_Interp *interp, int objc, Tcl_Obj *const objv[])
{
    struct db_i *dbip;
    struct db_tree_state ts = RT_DBTS_INIT_IDN;
    struct bg_tess_tol ttol = BG_TESS_TOL_INIT_ZERO;
    struct bn_tol tol = BN_TOL_INIT_TOL;
    struct rtwizard_bound_data bd;
    vect_t dir, ref, xaxis, yaxis;
    mat_t rmat;
    const char **objects;
    int ret;
    int i;

    if (objc < 4) {
	Tcl_SetObjResult(interp, Tcl_NewStringObj(
		"usage: rtwizard_cut_bounds database direction object ?object ...?", -1));
	return TCL_ERROR;
    }
    if (bn_decode_vect(dir, Tcl_GetString(objv[2])) != 3 ||
	MAGNITUDE(dir) <= SQRT_SMALL_FASTF) {
	Tcl_SetObjResult(interp, Tcl_NewStringObj("cutting direction must be a non-zero XYZ vector", -1));
	return TCL_ERROR;
    }
    VUNITIZE(dir);

    dbip = db_open(Tcl_GetString(objv[1]), DB_OPEN_READONLY);
    if (!dbip || db_dirbuild(dbip) < 0) {
	if (dbip)
	    db_close(dbip);
	Tcl_SetObjResult(interp, Tcl_NewStringObj("unable to open geometry database for animation bounds", -1));
	return TCL_ERROR;
    }

    if (fabs(dir[Z]) < 0.9)
	VSET(ref, 0.0, 0.0, 1.0);
    else
	VSET(ref, 0.0, 1.0, 0.0);
    VCROSS(xaxis, ref, dir);
    VUNITIZE(xaxis);
    VCROSS(yaxis, dir, xaxis);
    VUNITIZE(yaxis);
    MAT_IDN(rmat);
    VMOVE(&rmat[0], xaxis);
    VMOVE(&rmat[4], yaxis);
    VMOVE(&rmat[8], dir);

    memset(&bd, 0, sizeof(bd));
    VSETALL(bd.bmin, INFINITY);
    VSETALL(bd.bmax, -INFINITY);
    ts.ts_dbip = dbip;
    ts.ts_ttol = &ttol;
    ts.ts_tol = &tol;
    MAT_COPY(ts.ts_mat, rmat);

    objects = (const char **)bu_calloc((size_t)objc - 3, sizeof(char *), "animation bound objects");
    for (i = 3; i < objc; i++)
	objects[i - 3] = Tcl_GetString(objv[i]);
    ret = db_walk_tree(dbip, objc - 3, objects, 1, &ts, NULL,
	rtwizard_bound_region_end, rtwizard_bound_leaf, &bd);
    bu_free(objects, "animation bound objects");
    db_close(dbip);

    if (ret < 0 || bd.error || !bd.have_bounds || !isfinite(bd.bmin[Z]) || !isfinite(bd.bmax[Z])) {
	Tcl_SetObjResult(interp, Tcl_NewStringObj(
		"cutting-plane animation requires finite rendered-object bounds", -1));
	return TCL_ERROR;
    }

    Tcl_Obj *bounds = Tcl_NewListObj(0, NULL);
    Tcl_ListObjAppendElement(interp, bounds, Tcl_NewDoubleObj(bd.bmin[Z]));
    Tcl_ListObjAppendElement(interp, bounds, Tcl_NewDoubleObj(bd.bmax[Z]));
    Tcl_SetObjResult(interp, bounds);
    return TCL_OK;
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

    if (s->cut_steps) {
	bu_vls_sprintf(&tcl_cmd, "set ::RtWizard::wizard_state(cut_steps) %d", s->cut_steps);
	(void)Tcl_Eval(interp, bu_vls_addr(&tcl_cmd));
	(void)Tcl_SetVar2(interp, "::RtWizard::wizard_state", "make_animation", "1", TCL_GLOBAL_ONLY);
    }

    bu_vls_sprintf(&tcl_cmd, "set ::RtWizard::wizard_state(animation_fps) %d", s->animation_fps);
    (void)Tcl_Eval(interp, bu_vls_addr(&tcl_cmd));

    if (s->cut_direction_set) {
	bu_vls_sprintf(&tcl_cmd, "set ::RtWizard::wizard_state(cut_direction) \"%0.15f %0.15f %0.15f\"", V3ARGS(s->cut_direction));
	(void)Tcl_Eval(interp, bu_vls_addr(&tcl_cmd));
    }

    bu_vls_sprintf(&tcl_cmd, "set ::RtWizard::wizard_state(ao_samples) %d", s->ao_samples);
    (void)Tcl_Eval(interp, bu_vls_addr(&tcl_cmd));
    bu_vls_sprintf(&tcl_cmd, "set ::RtWizard::wizard_state(ao_radius) %0.15f", s->ao_radius);
    (void)Tcl_Eval(interp, bu_vls_addr(&tcl_cmd));

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


#define RTWIZARD_HELP_GROUP_MAX 16

static void
rtwizard_help_group(struct bu_vls *output, const char *heading,
	const char * const *names)
{
    struct bu_arg_option selected[RTWIZARD_HELP_GROUP_MAX];
    struct bu_arg_schema view = rtwizard_schema;
    size_t count = 0;
    size_t i;
    char *description;

    for (i = 0; names[i]; i++) {
	size_t option_index;
	for (option_index = 0; bu_arg_option_is_valid(&rtwizard_options[option_index]);
		option_index++) {
	    const char *canonical = bu_arg_option_canonical(&rtwizard_options[option_index]);
	    if (canonical && BU_STR_EQUAL(canonical, names[i])) {
		if (count + 1 >= RTWIZARD_HELP_GROUP_MAX)
		    bu_bomb("RtWizard help group exceeds its fixed capacity");
		selected[count++] = rtwizard_options[option_index];
		break;
	    }
	}
    }
    selected[count] = (struct bu_arg_option)BU_ARG_OPTION_NULL;
    view.options = selected;
    view.operands = NULL;
    description = bu_arg_schema_describe(&view);
    if (description) {
	bu_vls_printf(output, "%s:\n%s\n", heading, description);
	bu_free(description, "RtWizard option help");
    }
}


static void
rtwizard_help(void)
{
    static const char * const io_options[] = {
	"help", "help-dev", "input-file", "output-file", "size", "width",
	"height", "cut-steps", "cut-direction", "animation-fps", NULL
    };
    static const char * const visualization_options[] = {
	"type", "color-objects", "line-objects", "ghost-objects", NULL
    };
    static const char * const view_options[] = {
	"azimuth", "elevation", "twist", "zoom", "center", "eye_pt",
	"viewsize", "orientation", "perspective", NULL
    };
    static const char * const style_options[] = {
	"background-color", "line-color", "non-line-color", "ghost-intensity",
	"occlusion", "ao-samples", "ao-radius", NULL
    };
    static const char * const display_options[] = {
	"gui", "no-gui", "fbserv-device", "fbserv-port", "verbose", NULL
    };
    struct bu_vls output = BU_VLS_INIT_ZERO;
    char *usage = bu_arg_schema_usage(&rtwizard_schema, "rtwizard");

    bu_vls_printf(&output, "%s\n", usage ? usage : "");
    if (usage)
	bu_free(usage, "RtWizard usage");
    rtwizard_help_group(&output, "Input/Output Options", io_options);
    rtwizard_help_group(&output, "Visualization Options", visualization_options);
    rtwizard_help_group(&output, "View Setup Options", view_options);
    rtwizard_help_group(&output, "Style Options", style_options);
    rtwizard_help_group(&output, "Display Options", display_options);
    bu_log("%s", bu_vls_cstr(&output));
    bu_vls_free(&output);
}


static void
rtwizard_help_dev(void)
{
    static const char * const developer_options[] = {
	"benchmark", "cpu-count", "pid-file", "log-file", NULL
    };
    struct bu_vls output = BU_VLS_INIT_ZERO;
    char *usage = bu_arg_schema_usage(&rtwizard_schema, "rtwizard");

    bu_vls_printf(&output, "%s\n", usage ? usage : "");
    if (usage)
	bu_free(usage, "RtWizard developer usage");
    rtwizard_help_group(&output, "Options for developers", developer_options);
    bu_log("%s", bu_vls_cstr(&output));
    bu_vls_free(&output);
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


static void
rtwizard_args_init(struct rtwizard_args *args, const struct rtwizard_settings *settings)
{
    memset(args, 0, sizeof(*args));
    bu_vls_init(&args->input_file);
    bu_vls_init(&args->output_file);
    bu_vls_init(&args->fb_device);
    bu_vls_init(&args->pid_file);
    bu_vls_init(&args->log_file);
    bu_vls_vlscat(&args->input_file, settings->input_file);
    bu_vls_vlscat(&args->output_file, settings->output_file);
    bu_vls_vlscat(&args->fb_device, settings->fb_dev);
    bu_vls_vlscat(&args->pid_file, settings->pid_file);
    bu_vls_vlscat(&args->log_file, settings->log_file);
    args->size = settings->size;
    args->width = settings->width;
    args->height = settings->height;
    args->color = settings->color;
    args->line = settings->line;
    args->ghost = settings->ghost;
    args->azimuth = settings->az;
    args->elevation = settings->el;
    args->twist = settings->tw;
    args->zoom = settings->zoom;
    VMOVE(args->center, settings->center);
    VMOVE(args->eye_pt, settings->eye_pt);
    args->viewsize = settings->viewsize;
    HMOVE(args->orientation, settings->orientation);
    args->perspective = settings->perspective;
    args->background_color = *settings->bkg_color;
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
    args->cut_steps = settings->cut_steps;
    VMOVE(args->cut_direction, settings->cut_direction);
    args->animation_fps = settings->animation_fps;
    args->ao_samples = settings->ao_samples;
    args->ao_radius = settings->ao_radius;
}


static void
rtwizard_args_apply(struct rtwizard_settings *settings,
	const struct rtwizard_args *args, int size_set, int width_set,
	int height_set, int cut_direction_set)
{
    bu_vls_strcpy(settings->input_file, bu_vls_cstr(&args->input_file));
    bu_vls_strcpy(settings->output_file, bu_vls_cstr(&args->output_file));
    bu_vls_strcpy(settings->fb_dev, bu_vls_cstr(&args->fb_device));
    bu_vls_strcpy(settings->pid_file, bu_vls_cstr(&args->pid_file));
    bu_vls_strcpy(settings->log_file, bu_vls_cstr(&args->log_file));
    if (size_set) {
	settings->size = args->size;
	settings->size_set = 1;
	if (!width_set)
	    settings->width = args->size;
	if (!height_set)
	    settings->height = args->size;
    }
    if (width_set) {
	settings->width = args->width;
	settings->width_set = 1;
    }
    if (height_set) {
	settings->height = args->height;
	settings->height_set = 1;
    }
    settings->az = args->azimuth;
    settings->el = args->elevation;
    settings->tw = args->twist;
    settings->zoom = args->zoom;
    VMOVE(settings->center, args->center);
    VMOVE(settings->eye_pt, args->eye_pt);
    settings->viewsize = args->viewsize;
    HMOVE(settings->orientation, args->orientation);
    settings->perspective = args->perspective;
    *settings->bkg_color = args->background_color;
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
    settings->cut_steps = args->cut_steps;
    VMOVE(settings->cut_direction, args->cut_direction);
    settings->cut_direction_set = cut_direction_set;
    settings->animation_fps = args->animation_fps;
    settings->ao_samples = args->ao_samples;
    settings->ao_radius = args->ao_radius;
}


static void
rtwizard_args_free(struct rtwizard_args *args)
{
    bu_vls_free(&args->input_file);
    bu_vls_free(&args->output_file);
    bu_vls_free(&args->fb_device);
    bu_vls_free(&args->pid_file);
    bu_vls_free(&args->log_file);
}


int
main(int argc, char **argv)
{
    const struct bu_arg_grammar grammar = {&rtwizard_schema, NULL, NULL};
    char *av0;
    char type = '\0';
    int i = 0;
    int uac = 0;
    int operand_index;
    int size_set;
    int width_set;
    int height_set;
    int cut_direction_set;

    struct bu_vls optparse_msg = BU_VLS_INIT_ZERO;
    struct bu_vls info_msg = BU_VLS_INIT_ZERO;

    /* initialize progname for run-time resource finding */
    bu_setprogname(argv[0]);
    av0 = argv[0];

    int schema_status = bu_arg_introspect(&optparse_msg, &grammar, argc - 1,
	(const char * const *)(argv + 1));
    if (schema_status != 0) {
	bu_flog(schema_status > 0 ? stdout : stderr, "%s", bu_vls_cstr(&optparse_msg));
	bu_vls_free(&optparse_msg);
	return schema_status > 0 ? EXIT_SUCCESS : EXIT_FAILURE;
    }

    struct rtwizard_settings *s = rtwizard_settings_create();
    struct rtwizard_args args;
    rtwizard_args_init(&args, s);

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

    size_set = bu_arg_schema_option_present(&rtwizard_schema, argc,
	(const char **)argv, "size");
    width_set = bu_arg_schema_option_present(&rtwizard_schema, argc,
	(const char **)argv, "width");
    height_set = bu_arg_schema_option_present(&rtwizard_schema, argc,
	(const char **)argv, "height");
    cut_direction_set = bu_arg_schema_option_present(&rtwizard_schema, argc,
	(const char **)argv, "cut-direction");
    operand_index = bu_arg_schema_parse(&rtwizard_schema, &args, &optparse_msg,
	argc, (const char **)argv);
    if (operand_index < 0) {
	bu_exit(EXIT_FAILURE, "%s", bu_vls_cstr(&optparse_msg));
    }
    bu_vls_free(&optparse_msg);

    if (args.help) {
	rtwizard_help();
	bu_exit(EXIT_SUCCESS, NULL);
    }
    if (args.help_dev) {
	rtwizard_help_dev();
	bu_exit(EXIT_SUCCESS, NULL);
    }

    type = args.type;
    rtwizard_args_apply(s, &args, size_set, width_set, height_set,
	cut_direction_set);
    rtwizard_args_free(&args);
    argv += operand_index;
    uac = argc - operand_index;

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
    for (i = 0; i < uac; i++) {
	struct bu_vls c = BU_VLS_INIT_ZERO;
	/* First, see if we have an input .g file */
	if (bu_vls_strlen(s->input_file) == 0) {
	    if (bu_path_component(&c, argv[i], BU_PATH_EXT)) {
		if (bu_file_mime(bu_vls_addr(&c), BU_MIME_MODEL) == BU_MIME_MODEL_VND_BRLCAD_PLUS_BINARY) {
		    if (bu_file_exists(argv[i], NULL)) {
			bu_vls_sprintf(s->input_file, "%s", argv[i]);
			/* This was the .g name - don't add it to the color list */
			continue;
		    } else {
			bu_exit(EXIT_FAILURE, "ERROR: Specified %s as .g file, but file does not exist.\n", argv[i]);
		    }
		}
	    }
	}
	bu_vls_trunc(&c, 0);
	/* Next, see if we have an image specified as an output destination */
	if (bu_vls_strlen(s->output_file) == 0 && bu_vls_strlen(s->fb_dev) == 0) {
	    if (bu_path_component(&c, argv[i], BU_PATH_EXT)) {
		if (rtwizard_imgformat_supported(bu_file_mime(bu_vls_addr(&c), BU_MIME_IMAGE)) ||
		    rtwizard_anim_path_supported(argv[i])) {
		    bu_vls_sprintf(s->output_file, "%s", argv[i]);
		    /* This looks like the output image name - don't add it to the color list */
		    continue;
		}
	    }
	}
	/* If it's none of the above, assume a color object in the .g file */
	bu_ptbl_ins(s->color, (long *)bu_strdup(argv[i]));
    }

    if (rtwizard_view_opts_check(&info_msg, s)) {
	bu_log("%s\n", bu_vls_addr(&info_msg));
	bu_vls_trunc(&info_msg, 0);
    }

    if (s->cut_steps != 0 && s->cut_steps < 2)
	bu_exit(EXIT_FAILURE, "ERROR: --cut-steps must be at least 2.\n");
    if (s->animation_fps <= 0)
	bu_exit(EXIT_FAILURE, "ERROR: --animation-fps must be positive.\n");
    if (s->ao_samples < 0 || s->ao_radius < 0.0)
	bu_exit(EXIT_FAILURE, "ERROR: ambient occlusion samples and radius may not be negative.\n");
    if (s->cut_direction_set && MAGNITUDE(s->cut_direction) <= SQRT_SMALL_FASTF)
	bu_exit(EXIT_FAILURE, "ERROR: --cut-direction must be a non-zero vector.\n");
    if (s->cut_steps && !bu_vls_strlen(s->output_file) && !s->use_gui)
	bu_exit(EXIT_FAILURE, "ERROR: a file output is required for a cutting-plane animation.\n");
    if (s->cut_steps && bu_vls_strlen(s->output_file) &&
	!rtwizard_anim_path_supported(bu_vls_addr(s->output_file)))
	bu_exit(EXIT_FAILURE, "ERROR: animation output must use .apng, .png, .avi, or .mjpg.\n");
    if (!s->cut_steps && !s->use_gui && bu_vls_strlen(s->output_file) &&
	rtwizard_anim_only_path(bu_vls_addr(s->output_file)))
	bu_exit(EXIT_FAILURE, "ERROR: .apng, .avi, and .mjpg outputs require --cut-steps.\n");

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
	(void)Tcl_CreateObjCommand(interp, "rtwizard_anim_write",
		rtwizard_anim_write_cmd, NULL, NULL);
	(void)Tcl_CreateObjCommand(interp, "rtwizard_cut_bounds",
		rtwizard_cut_bounds_cmd, NULL, NULL);

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
