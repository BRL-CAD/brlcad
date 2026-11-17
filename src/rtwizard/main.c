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
#include "bu/hash.h"
#include "bu/mime.h"
#include "bu/malloc.h"
#include "bu/log.h"
#include "bu/path.h"
#include "bu/ptbl.h"
#include "bu/opt.h"
#include "bu/str.h"
#include "icv.h"
#include "icv/anim.h"
#include "pkg.h"
#include "tclcad.h"

#include "animation.h"
#include "settings.h"

#define RTWIZARD_HAVE_GUI 0

#define RTWIZARD_SIZE_DEFAULT 512

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
    BU_GET(s->render_spec, struct bu_vls);
    bu_vls_init(s->render_spec);
    BU_GET(s->animation_file, struct bu_vls);
    bu_vls_init(s->animation_file);
    BU_GET(s->animation_preset, struct bu_vls);
    bu_vls_init(s->animation_preset);
    BU_GET(s->frame_dir, struct bu_vls);
    bu_vls_init(s->frame_dir);
    BU_GET(s->turntable_object, struct bu_vls);
    bu_vls_init(s->turntable_object);
    BU_GET(s->save_view_keyframe, struct bu_vls);
    bu_vls_init(s->save_view_keyframe);

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
    /* Zero means "take the value from an animation track file".  Presets
     * receive their user-facing defaults after option parsing. */
    s->animation_fps = 0;
    s->animation_duration = 0.0;
    s->animation_frames = 0;
    s->animation_plays = -1;
    s->animation_cyclic = -1;
    s->resume = 0;
    s->orbit_angle = 360.0;
    VSET(s->orbit_axis, 0.0, 0.0, 1.0);
    VSETALL(s->orbit_center, DBL_MAX);
    s->orbit_elevation = DBL_MAX;
    s->orbit_radius = DBL_MAX;
    s->turntable_angle = 360.0;
    VSET(s->turntable_axis, 0.0, 0.0, 1.0);
    VSETALL(s->turntable_center, DBL_MAX);
    s->keyframe_time = DBL_MAX;
    s->replace_keyframe = 0;
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
    s->image_type = '\0';

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
    bu_vls_free(s->render_spec);
    BU_PUT(s->render_spec, struct bu_vls);
    bu_vls_free(s->animation_file);
    BU_PUT(s->animation_file, struct bu_vls);
    bu_vls_free(s->animation_preset);
    BU_PUT(s->animation_preset, struct bu_vls);
    bu_vls_free(s->frame_dir);
    BU_PUT(s->frame_dir, struct bu_vls);
    bu_vls_free(s->turntable_object);
    BU_PUT(s->turntable_object, struct bu_vls);
    bu_vls_free(s->save_view_keyframe);
    BU_PUT(s->save_view_keyframe, struct bu_vls);


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


static int
opt_size_t(struct bu_vls *msg, size_t argc, const char **argv, void *set_size)
{
    size_t value;

    BU_OPT_CHECK_ARGV0(msg, argc, argv, "non-negative integer");

    if (!bu_opt_scan_size_t(argv[0], &value, NULL)) {
	if (msg)
	    bu_vls_sprintf(msg, "Invalid non-negative integer: %s\n", argv[0]);
	return -1;
    }

    if (set_size)
	*(size_t *)set_size = value;

    return 1;
}


int
opt_width(struct bu_vls *msg, size_t argc, const char **argv, void *settings)
{
    struct rtwizard_settings *s = (struct rtwizard_settings *)settings;
    if (s) {
	int ret = opt_size_t(msg, argc, argv, (void *)&s->width);
	if (ret != -1)
	    s->width_set = 1;
	return ret;
    } else {
	return -1;
    }
}


int
opt_height(struct bu_vls *msg, size_t argc, const char **argv, void *settings)
{
    struct rtwizard_settings *s = (struct rtwizard_settings *)settings;
    if (s) {
	int ret = opt_size_t(msg, argc, argv, (void *)&s->height);
	if (ret != -1)
	    s->height_set = 1;
	return ret;
    } else {
	return -1;
    }
}


int
opt_size(struct bu_vls *msg, size_t argc, const char **argv, void *settings)
{
    struct rtwizard_settings *s = (struct rtwizard_settings *)settings;
    if (s) {
	int ret = opt_size_t(msg, argc, argv, (void *)&s->size);
	if (ret != -1) {
	    s->size_set = 1;
	    if (!s->width_set)
		s->width = s->size;
	    if (!s->height_set)
		s->height = s->size;
	}
	return ret;
    } else {
	return -1;
    }
}


int
opt_objs(struct bu_vls *msg, size_t argc, const char **argv, void *obj_tbl)
{
    /* argv[0] should be either an object or a list. */
    size_t i = 0;
    char *objs = NULL;
    size_t acnum = 0;
    char **avnum;
    struct bu_ptbl *t = (struct bu_ptbl *)obj_tbl;

    BU_OPT_CHECK_ARGV0(msg, argc, argv, "opt_objs");

    objs = bu_strdup(argv[0]);

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
	if (t) {
	    bu_ptbl_ins(t, (long *)bu_strdup(avnum[i]));
	}
    }
    bu_free(objs, "string dup");
    bu_free(avnum, "array memory");

    return (acnum > 0) ? 1 : -1;
}


int
opt_letter(struct bu_vls *msg, size_t argc, const char **argv, void *l)
{
    char *letter = (char *)l;
    BU_OPT_CHECK_ARGV0(msg, argc, argv, "bu_opt_int");

    if (strlen(argv[0]) != 1) {
	if (msg)
	    bu_vls_printf(msg, "Invalid letter specifier for rtwizard type: %s\n", argv[0]);
	return -1;
    }

    if (argv[0][0] != 'A' && argv[0][0] != 'B' && argv[0][0] != 'C' && argv[0][0] != 'D' && argv[0][0] != 'E' && argv[0][0] != 'F') {
	if (msg)
	    bu_vls_printf(msg, "Invalid letter specifier for rtwizard type: %c\n", argv[0][0]);
	return -1;
    }

    if (letter) {
	(*letter) = argv[0][0];
    }

    return 1;
}


int
opt_cut_direction(struct bu_vls *msg, size_t argc, const char **argv, void *settings)
{
    struct rtwizard_settings *s = (struct rtwizard_settings *)settings;
    int ret;

    if (!s)
	return -1;

    ret = bu_opt_vect_t(msg, argc, argv, (void *)&s->cut_direction);
    if (ret != -1)
	s->cut_direction_set = 1;
    return ret;
}


int
opt_quat(struct bu_vls *msg, size_t argc, const char **argv, void *inq)
{
    size_t i = 0;
    size_t acnum = 0;
    char *str1 = NULL;
    char *avnum[6] = {NULL, NULL, NULL, NULL, NULL, NULL};

    quat_t *q = (quat_t *)inq;
    BU_OPT_CHECK_ARGV0(msg, argc, argv, "bu_opt_int");


    /* First, see if the first string converts to a quat_t*/
    str1 = bu_strdup(argv[0]);
    while (str1[i]) {
	/* If we have a separator, replace with a space */
	if (str1[i] == ',' || str1[i] == '/') str1[i] = ' ';
	i++;
    }
    acnum = bu_argv_from_string(avnum, 5, str1);
    if (acnum == 4) {
	/* We might have four numbers - find out */
	fastf_t q1, q2, q3, q4;
	int have_four = 1;
	if (bu_opt_fastf_t(msg, 1, (const char **)&avnum[0], &q1) == -1) {
	    if (msg)
		bu_vls_sprintf(msg, "Not a number: %s.\n", avnum[0]);
	    have_four = 0;
	}
	if (bu_opt_fastf_t(msg, 1, (const char **)&avnum[1], &q2) == -1) {
	    if (msg)
		bu_vls_sprintf(msg, "Not a number: %s.\n", avnum[1]);
	    have_four = 0;
	}
	if (bu_opt_fastf_t(msg, 1, (const char **)&avnum[2], &q3) == -1) {
	    if (msg) bu_vls_sprintf(msg, "Not a number: %s.\n", avnum[2]);
	    have_four = 0;
	}
	if (bu_opt_fastf_t(msg, 1, (const char **)&avnum[3], &q4) == -1) {
	    if (msg) bu_vls_sprintf(msg, "Not a number: %s.\n", avnum[3]);
	    have_four = 0;
	}
	bu_free(str1, "free tmp str");
	/* If we got here, we do have four numbers */
	if (have_four) {
	    if (q) {
		(*q)[0] = q1;
		(*q)[1] = q2;
		(*q)[2] = q3;
		(*q)[3] = q4;
	    }
	    return 1;
	}
    } else {
	/* Can't be just the first arg */
	bu_free(str1, "free tmp str");
    }
    /* First string didn't have the numbers - maybe we have 4 args ? */
    if (argc >= 4) {
	/* We might have four numbers - find out */
	fastf_t q1, q2, q3, q4;
	if (bu_opt_fastf_t(msg, 1, &argv[0], &q1) == -1) {
	    if (msg)
		bu_vls_sprintf(msg, "Not a number: %s.\n", argv[0]);
	    return -1;
	}
	if (bu_opt_fastf_t(msg, 1, &argv[1], &q2) == -1) {
	    if (msg)
		bu_vls_sprintf(msg, "Not a number: %s.\n", argv[1]);
	    return -1;
	}
	if (bu_opt_fastf_t(msg, 1, &argv[2], &q3) == -1) {
	    if (msg)
		bu_vls_sprintf(msg, "Not a number: %s.\n", argv[2]);
	    return -1;
	}
	if (bu_opt_fastf_t(msg, 1, &argv[3], &q4) == -1) {
	    if (msg)
		bu_vls_sprintf(msg, "Not a number: %s.\n", argv[3]);
	    return -1;
	}
	if (q) {
	    (*q)[0] = q1;
	    (*q)[1] = q2;
	    (*q)[2] = q3;
	    (*q)[3] = q4;
	}
	return 1;
    } else {
	if (msg)
	    bu_vls_sprintf(msg, "No valid quaternion found: %s\n", argv[0]);
	return -1;
    }
}


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
    int width, height, fps, plays;
    int i;

    if (objc < 7) {
	Tcl_SetObjResult(interp, Tcl_NewStringObj(
		"usage: rtwizard_anim_write output width height fps plays frame1 ?frame2 ...?", -1));
	return TCL_ERROR;
    }

    output = Tcl_GetString(objv[1]);
    if (Tcl_GetIntFromObj(interp, objv[2], &width) != TCL_OK ||
	Tcl_GetIntFromObj(interp, objv[3], &height) != TCL_OK ||
	Tcl_GetIntFromObj(interp, objv[4], &fps) != TCL_OK ||
	Tcl_GetIntFromObj(interp, objv[5], &plays) != TCL_OK)
	return TCL_ERROR;

    if (width <= 0 || height <= 0 || fps <= 0 || plays < 0) {
	Tcl_SetObjResult(interp, Tcl_NewStringObj(
		"animation width, height, and fps must be positive and plays must be nonnegative", -1));
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

    (void)icv_anim_set_plays(anim, (uint32_t)plays);

    for (i = 6; i < objc; i++) {
	const char *frame = Tcl_GetString(objv[i]);
	icv_image_t *img = icv_read(frame, BU_MIME_IMAGE_AUTO, (size_t)width, (size_t)height);
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


static int
rtwizard_image_valid_cmd(ClientData UNUSED(client_data), Tcl_Interp *interp, int objc, Tcl_Obj *const objv[])
{
    int width, height;
    icv_image_t *img;
    int valid = 0;
    if (objc != 4 || Tcl_GetIntFromObj(interp, objv[2], &width) != TCL_OK ||
	Tcl_GetIntFromObj(interp, objv[3], &height) != TCL_OK) {
	Tcl_SetObjResult(interp, Tcl_NewStringObj("usage: rtwizard_image_valid file width height", -1));
	return TCL_ERROR;
    }
    img = icv_read(Tcl_GetString(objv[1]), BU_MIME_IMAGE_AUTO, (size_t)width, (size_t)height);
    if (img) {
	valid = (img->width == (size_t)width && img->height == (size_t)height);
	icv_destroy(img);
    }
    Tcl_SetObjResult(interp, Tcl_NewBooleanObj(valid));
    return TCL_OK;
}


static int
rtwizard_file_hash_cmd(ClientData UNUSED(client_data), Tcl_Interp *interp, int objc, Tcl_Obj *const objv[])
{
    static const size_t buffer_size = 64 * 1024;
    unsigned char *buffer;
    struct bu_data_hash128_state *state;
    bu_h128_t digest;
    FILE *stream;
    size_t bytes_read;

    if (objc != 2) {
	Tcl_SetObjResult(interp, Tcl_NewStringObj("usage: rtwizard_file_hash file", -1));
	return TCL_ERROR;
    }
    stream = fopen(Tcl_GetString(objv[1]), "rb");
    if (!stream) {
	Tcl_SetObjResult(interp, Tcl_ObjPrintf("unable to open '%s' for hashing", Tcl_GetString(objv[1])));
	return TCL_ERROR;
    }
    buffer = (unsigned char *)bu_malloc(buffer_size, "file hash buffer");
    state = bu_data_hash128_create();
    while ((bytes_read = fread(buffer, 1, buffer_size, stream)) > 0)
	bu_data_hash128_update(state, buffer, bytes_read);
    if (ferror(stream)) {
	bu_data_hash128_destroy(state);
	bu_free(buffer, "file hash buffer");
	fclose(stream);
	Tcl_SetObjResult(interp, Tcl_ObjPrintf("unable to read '%s' for hashing", Tcl_GetString(objv[1])));
	return TCL_ERROR;
    }
    digest = bu_data_hash128_val(state);
    bu_data_hash128_destroy(state);
    bu_free(buffer, "file hash buffer");
    fclose(stream);
    Tcl_SetObjResult(interp, Tcl_ObjPrintf("%016llx%016llx",
	(unsigned long long)digest.w[1], (unsigned long long)digest.w[0]));
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

    bu_vls_sprintf(&tcl_cmd, "set ::RtWizard::wizard_state(animation_duration) %.17g", s->animation_duration);
    (void)Tcl_Eval(interp, bu_vls_addr(&tcl_cmd));
    bu_vls_sprintf(&tcl_cmd, "set ::RtWizard::wizard_state(animation_frames) %d", s->animation_frames);
    (void)Tcl_Eval(interp, bu_vls_addr(&tcl_cmd));
    bu_vls_sprintf(&tcl_cmd, "set ::RtWizard::wizard_state(animation_plays) %d", s->animation_plays);
    (void)Tcl_Eval(interp, bu_vls_addr(&tcl_cmd));
    bu_vls_sprintf(&tcl_cmd, "set ::RtWizard::wizard_state(animation_cyclic) %d", s->animation_cyclic);
    (void)Tcl_Eval(interp, bu_vls_addr(&tcl_cmd));
    bu_vls_sprintf(&tcl_cmd, "set ::RtWizard::wizard_state(resume_animation) %d", s->resume);
    (void)Tcl_Eval(interp, bu_vls_addr(&tcl_cmd));

    if (bu_vls_strlen(s->render_spec))
	rtwizard_set_state(interp, "render_spec", bu_vls_addr(s->render_spec));
    if (bu_vls_strlen(s->animation_file)) {
	rtwizard_set_state(interp, "animation_file", bu_vls_addr(s->animation_file));
	(void)Tcl_SetVar2(interp, "::RtWizard::wizard_state", "make_animation", "1", TCL_GLOBAL_ONLY);
    }
    if (bu_vls_strlen(s->animation_preset)) {
	rtwizard_set_state(interp, "animation_preset", bu_vls_addr(s->animation_preset));
	(void)Tcl_SetVar2(interp, "::RtWizard::wizard_state", "make_animation", "1", TCL_GLOBAL_ONLY);
    }
    if (bu_vls_strlen(s->frame_dir))
	rtwizard_set_state(interp, "frame_dir", bu_vls_addr(s->frame_dir));
    if (bu_vls_strlen(s->save_view_keyframe)) {
	rtwizard_set_state(interp, "save_view_keyframe", bu_vls_addr(s->save_view_keyframe));
	bu_vls_sprintf(&tcl_cmd, "set ::RtWizard::wizard_state(keyframe_time) %.17g", s->keyframe_time);
	(void)Tcl_Eval(interp, bu_vls_addr(&tcl_cmd));
	bu_vls_sprintf(&tcl_cmd, "set ::RtWizard::wizard_state(replace_keyframe) %d", s->replace_keyframe);
	(void)Tcl_Eval(interp, bu_vls_addr(&tcl_cmd));
    }

    bu_vls_sprintf(&tcl_cmd, "set ::RtWizard::wizard_state(orbit_angle) %.17g", s->orbit_angle);
    (void)Tcl_Eval(interp, bu_vls_addr(&tcl_cmd));
    bu_vls_sprintf(&tcl_cmd, "set ::RtWizard::wizard_state(orbit_axis) {%.17g %.17g %.17g}", V3ARGS(s->orbit_axis));
    (void)Tcl_Eval(interp, bu_vls_addr(&tcl_cmd));
    if (s->orbit_center[0] < DBL_MAX) {
	bu_vls_sprintf(&tcl_cmd, "set ::RtWizard::wizard_state(orbit_center) {%.17g %.17g %.17g}", V3ARGS(s->orbit_center));
	(void)Tcl_Eval(interp, bu_vls_addr(&tcl_cmd));
    }
    if (s->orbit_elevation < DBL_MAX) {
	bu_vls_sprintf(&tcl_cmd, "set ::RtWizard::wizard_state(orbit_elevation) %.17g", s->orbit_elevation);
	(void)Tcl_Eval(interp, bu_vls_addr(&tcl_cmd));
    }
    if (s->orbit_radius < DBL_MAX) {
	bu_vls_sprintf(&tcl_cmd, "set ::RtWizard::wizard_state(orbit_radius) %.17g", s->orbit_radius);
	(void)Tcl_Eval(interp, bu_vls_addr(&tcl_cmd));
    }

    if (bu_vls_strlen(s->turntable_object))
	rtwizard_set_state(interp, "turntable_object", bu_vls_addr(s->turntable_object));
    bu_vls_sprintf(&tcl_cmd, "set ::RtWizard::wizard_state(turntable_angle) %.17g", s->turntable_angle);
    (void)Tcl_Eval(interp, bu_vls_addr(&tcl_cmd));
    bu_vls_sprintf(&tcl_cmd, "set ::RtWizard::wizard_state(turntable_axis) {%.17g %.17g %.17g}", V3ARGS(s->turntable_axis));
    (void)Tcl_Eval(interp, bu_vls_addr(&tcl_cmd));
    if (s->turntable_center[0] < DBL_MAX) {
	bu_vls_sprintf(&tcl_cmd, "set ::RtWizard::wizard_state(turntable_center) {%.17g %.17g %.17g}", V3ARGS(s->turntable_center));
	(void)Tcl_Eval(interp, bu_vls_addr(&tcl_cmd));
    }

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


/* Help message printed when -h option is supplied */
void
rtwizard_help(struct bu_opt_desc *d)
{
    struct bu_opt_desc_opts settings = BU_OPT_DESC_OPTS_INIT_ZERO;
    struct bu_vls str = BU_VLS_INIT_ZERO;
    struct bu_vls filtered = BU_VLS_INIT_ZERO;
    char *option_help;

    bu_vls_sprintf(&str, "\nUsage: rtwizard [options]\n\n");

    /* I/O options */
    bu_vls_sprintf(&filtered, "h help-dev i o s w n render-spec A animate animation-spec frame-dir resume");
    settings.accept = bu_vls_addr(&filtered);
    option_help = bu_opt_describe(d, &settings);
    if (option_help) {
	bu_vls_printf(&str, "Input/Output Options:\n%s\n", option_help);
    }
    bu_free(option_help, "help str");

    /* Visualization options */
    bu_vls_sprintf(&filtered, "t c l g");
    settings.accept = bu_vls_addr(&filtered);
    option_help = bu_opt_describe(d, &settings);
    if (option_help) {
	bu_vls_printf(&str, "Visualization Options:\n%s\n", option_help);
    }
    bu_free(option_help, "help str");

    /* View setup options */
    bu_vls_sprintf(&filtered, "a e twist z center eye_pt viewsize orientation P");
    settings.accept = bu_vls_addr(&filtered);
    option_help = bu_opt_describe(d, &settings);
    if (option_help) {
	bu_vls_printf(&str, "View Setup Options:\n%s\n", option_help);
    }
    bu_free(option_help, "help str");

    /* Style options */
    bu_vls_sprintf(&filtered, "C line-color non-line-color G O ao-samples ao-radius");
    settings.accept = bu_vls_addr(&filtered);
    option_help = bu_opt_describe(d, &settings);
    if (option_help) {
	bu_vls_printf(&str, "Style Options:\n%s\n", option_help);
    }
    bu_free(option_help, "help str");

    /* Display options */
    bu_vls_sprintf(&filtered, "gui no-gui d p v");
    settings.accept = bu_vls_addr(&filtered);
    option_help = bu_opt_describe(d, &settings);
    if (option_help) {
	bu_vls_printf(&str, "Display Options:\n%s\n", option_help);
    }
    bu_free(option_help, "help str");

    bu_log("%s", bu_vls_addr(&str));
    bu_vls_free(&str);
    bu_vls_free(&filtered);
}


/* Help message printed when --help-dev option is supplied */
void
rtwizard_help_dev(struct bu_opt_desc *d)
{
    struct bu_opt_desc_opts settings = BU_OPT_DESC_OPTS_INIT_ZERO;
    struct bu_vls str = BU_VLS_INIT_ZERO;
    struct bu_vls filtered = BU_VLS_INIT_ZERO;
    char *option_help = NULL;
    const char *devopts = "benchmark cpu-count pid-file log-file animation-duration animation-fps animation-frames animation-plays animation-cyclic cut-steps cut-direction save-view-keyframe time replace-keyframe";

    bu_vls_sprintf(&str, "\nUsage: rtwizard [options]\n\n");

    bu_vls_sprintf(&filtered, "%s", devopts);
    settings.accept = bu_vls_addr(&filtered);
    option_help = bu_opt_describe(d, &settings);
    if (option_help) {
	bu_vls_printf(&str, "Options for developers:\n%s\n", option_help);
    }
    bu_free(option_help, "help str");

    bu_log("%s", bu_vls_addr(&str));
    bu_vls_free(&str);
    bu_vls_free(&filtered);
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


static int
rtwizard_argv_has_option(int argc, char **argv, const char *shortopt, const char *longopt)
{
    int i;
    size_t llen = strlen(longopt);
    for (i = 1; i < argc; i++) {
	if ((shortopt && BU_STR_EQUAL(argv[i], shortopt)) || BU_STR_EQUAL(argv[i], longopt) ||
	    (bu_strncmp(argv[i], longopt, llen) == 0 && argv[i][llen] == '='))
	    return 1;
    }
    return 0;
}


static void
rtwizard_obj_table_clear(struct bu_ptbl *table)
{
    size_t i;
    for (i = 0; i < BU_PTBL_LEN(table); i++)
	bu_free((char *)BU_PTBL_GET(table, i), "rtwizard object name");
    bu_ptbl_reset(table);
}


static void
rtwizard_cli_overlay_prepare(int argc, char **argv, struct rtwizard_settings *s)
{
    if (rtwizard_argv_has_option(argc, argv, "-c", "--color-objects")) rtwizard_obj_table_clear(s->color);
    if (rtwizard_argv_has_option(argc, argv, "-g", "--ghost-objects")) rtwizard_obj_table_clear(s->ghost);
    if (rtwizard_argv_has_option(argc, argv, "-l", "--line-objects")) rtwizard_obj_table_clear(s->line);

    if (rtwizard_argv_has_option(argc, argv, "-i", "--input-file")) bu_vls_trunc(s->input_file, 0);
    if (rtwizard_argv_has_option(argc, argv, "-o", "--output-file")) bu_vls_trunc(s->output_file, 0);
    if (rtwizard_argv_has_option(argc, argv, "-d", "--fbserv-device")) bu_vls_trunc(s->fb_dev, 0);
    if (rtwizard_argv_has_option(argc, argv, NULL, "--log-file")) bu_vls_trunc(s->log_file, 0);
    if (rtwizard_argv_has_option(argc, argv, NULL, "--pid-file")) bu_vls_trunc(s->pid_file, 0);
    if (rtwizard_argv_has_option(argc, argv, NULL, "--frame-dir")) bu_vls_trunc(s->frame_dir, 0);

    if (rtwizard_argv_has_option(argc, argv, NULL, "--cut-steps")) {
	bu_vls_trunc(s->animation_file, 0);
	bu_vls_strcpy(s->animation_preset, "cut");
    }
    if (rtwizard_argv_has_option(argc, argv, NULL, "--gui")) s->no_gui = 0;
    if (rtwizard_argv_has_option(argc, argv, NULL, "--no-gui")) s->use_gui = 0;
}


static void
rtwizard_load_specs(int *argcp, char ***argvp, struct rtwizard_settings *s)
{
    int argc = *argcp;
    char **argv = *argvp;
    const char *spec = NULL;
    const char *animate = NULL;
    const char *animation_spec = NULL;
    int i;
    char *errmsg = NULL;
    int filtered_argc = 0;

    for (i = 1; i < argc; i++) {
	if (BU_STR_EQUAL(argv[i], "--render-spec")) {
	    if (spec) bu_exit(EXIT_FAILURE, "ERROR: --render-spec may be specified only once.\n");
	    if (i + 1 >= argc)
		bu_exit(EXIT_FAILURE, "ERROR: --render-spec requires a filename.\n");
	    spec = argv[i+1];
	    i++;
	    continue;
	}
	if (bu_strncmp(argv[i], "--render-spec=", 14) == 0) {
	    if (spec) bu_exit(EXIT_FAILURE, "ERROR: --render-spec may be specified only once.\n");
	    spec = argv[i] + 14;
	    if (!spec[0]) bu_exit(EXIT_FAILURE, "ERROR: --render-spec requires a filename.\n");
	    continue;
	}
	if (BU_STR_EQUAL(argv[i], "-A") || BU_STR_EQUAL(argv[i], "--animate")) {
	    if (animate) bu_exit(EXIT_FAILURE, "ERROR: --animate may be specified only once.\n");
	    if (i + 1 >= argc) bu_exit(EXIT_FAILURE, "ERROR: --animate requires cut, orbit, or turntable.\n");
	    animate = argv[++i];
	    continue;
	}
	if (bu_strncmp(argv[i], "--animate=", 10) == 0) {
	    if (animate) bu_exit(EXIT_FAILURE, "ERROR: --animate may be specified only once.\n");
	    animate = argv[i] + 10;
	    if (!animate[0]) bu_exit(EXIT_FAILURE, "ERROR: --animate requires cut, orbit, or turntable.\n");
	    continue;
	}
	if (BU_STR_EQUAL(argv[i], "--animation-spec")) {
	    if (animation_spec) bu_exit(EXIT_FAILURE, "ERROR: --animation-spec may be specified only once.\n");
	    if (i + 1 >= argc) bu_exit(EXIT_FAILURE, "ERROR: --animation-spec requires a filename.\n");
	    animation_spec = argv[++i];
	    continue;
	}
	if (bu_strncmp(argv[i], "--animation-spec=", 17) == 0) {
	    if (animation_spec) bu_exit(EXIT_FAILURE, "ERROR: --animation-spec may be specified only once.\n");
	    animation_spec = argv[i] + 17;
	    if (!animation_spec[0]) bu_exit(EXIT_FAILURE, "ERROR: --animation-spec requires a filename.\n");
	}
    }
    if (spec) {
	if (rtwizard_spec_apply(spec, s, &errmsg) != 0)
	    bu_exit(EXIT_FAILURE, "ERROR: %s\n", errmsg ? errmsg : "unable to load render specification");
	if (errmsg) bu_free(errmsg, "render spec error");
	errmsg = NULL;
    }
    if (animate) {
	bu_vls_strcpy(s->animation_preset, animate);
	bu_vls_trunc(s->animation_file, 0);
    }
    if (animation_spec) {
	if (rtwizard_animation_spec_apply(animation_spec, s, &errmsg) != 0)
	    bu_exit(EXIT_FAILURE, "ERROR: %s\n", errmsg ? errmsg : "unable to load animation specification");
	if (errmsg) bu_free(errmsg, "animation spec error");
    }
    rtwizard_cli_overlay_prepare(argc, argv, s);

    argv[filtered_argc++] = argv[0];
    for (i = 1; i < argc; i++) {
	if (BU_STR_EQUAL(argv[i], "--render-spec") || BU_STR_EQUAL(argv[i], "-A") ||
	    BU_STR_EQUAL(argv[i], "--animate") || BU_STR_EQUAL(argv[i], "--animation-spec")) {
	    i++;
	    continue;
	}
	if (bu_strncmp(argv[i], "--render-spec=", 14) == 0 ||
	    bu_strncmp(argv[i], "--animate=", 10) == 0 ||
	    bu_strncmp(argv[i], "--animation-spec=", 17) == 0) {
	    continue;
	}
	argv[filtered_argc++] = argv[i];
    }
    argv[filtered_argc] = NULL;
    *argcp = filtered_argc;
}


int
main(int argc, char **argv)
{
    char *av0;
    int i = 0;
    int need_help = 0;
    int need_help_dev = 0;
    int uac = 0;

    struct bu_vls optparse_msg = BU_VLS_INIT_ZERO;
    struct bu_vls info_msg = BU_VLS_INIT_ZERO;
    struct rtwizard_settings *s = rtwizard_settings_create();
    struct bu_opt_desc d[52];

    BU_OPT(d[0],  "h", "help",          "",             NULL,            &need_help,     "Print options help and exit");
    BU_OPT(d[1],  "",  "help-dev",      "",             NULL,            &need_help_dev, "Print development and programmatic options.");

    /* I/O FILE OPTIONS */
    BU_OPT(d[2],  "i", "input-file",    "<filename>",   &bu_opt_vls,     s->input_file,  "Input .g database file");
    BU_OPT(d[3],  "o", "output-file",   "<filename>",   &bu_opt_vls,     s->output_file, "Image output file name");
    BU_OPT(d[4],  "s", "size",          "#",            &opt_size,       s,              "Output width & height (for square image)");
    BU_OPT(d[5],  "w", "width",         "#",            &opt_width,      s,              "Output image width (overrides -s)");
    BU_OPT(d[6],  "n", "height",        "#",            &opt_height,     s,              "Output image height (overrides -s)");

    /* VISUALIZATION OPTIONS */
    BU_OPT(d[7],  "t", "type",          "A|B|C|D|E|F",  &opt_letter,     &s->image_type, "Specify RtWizard picture type");
    BU_OPT(d[8],  "c", "color-objects", "<obj_list>",   &opt_objs,       s->color,       "List of color objects (e.g., -c obj1,obj2)");
    BU_OPT(d[9],  "l", "line-objects",  "<obj_list>",   &opt_objs,       s->line,        "List of line objects");
    BU_OPT(d[10], "g", "ghost-objects", "<obj_list>",   &opt_objs,       s->ghost,       "List of ghost objects");

    /* VIEW SETUP OPTIONS */
    BU_OPT(d[11], "a", "azimuth",       "<float>",      &bu_opt_fastf_t, &s->az,         "Set azimuth");
    BU_OPT(d[12], "e", "elevation",     "<float>",      &bu_opt_fastf_t, &s->el,         "Set elevation");
    BU_OPT(d[13], "",  "twist",         "<float>",      &bu_opt_fastf_t, &s->tw,         "Set twist");
    BU_OPT(d[14], "z", "zoom",          "<float>",      &bu_opt_fastf_t, &s->zoom,       "Set zoom");
    BU_OPT(d[15], "",  "center",        "<point>",      &bu_opt_vect_t,  &s->center,     "Set view center (e.g., -center 1.2/3.4/5)");
    BU_OPT(d[16], "",  "eye_pt",        "<point>",      &bu_opt_vect_t,  &s->eye_pt,     "Set eye point (e.g., -eye_pt 1.0/2.0/3.0)");
    BU_OPT(d[17], "",  "viewsize",      "<float>",      &bu_opt_fastf_t, &s->viewsize,   "Set view size");
    BU_OPT(d[18], "",  "orientation",   "<quat>",       &opt_quat,       &s->orientation, "Set view orientation (e.g., -orientation 1/2/3/4)");
    BU_OPT(d[19], "P", "perspective",   "<float>",      &bu_opt_fastf_t, &s->perspective, "Set perspective");

    /* STYLE OPTIONS */
    BU_OPT(d[20], "C", "background-color", "<color>",   &bu_opt_color,   s->bkg_color,   "Background image color (e.g., -C 255/0/0)");
    BU_OPT(d[21], "",  "line-color",       "<color>",   &bu_opt_color,   s->line_color,  "Color used for line rendering");
    BU_OPT(d[22], "",  "non-line-color",   "<color>",   &bu_opt_color,   s->non_line_color, "Color used for non-line rendering");
    BU_OPT(d[23], "G", "ghost-intensity",  "<float>",   &bu_opt_fastf_t, &s->ghost_intensity,"Intensity of ghost objects");
    BU_OPT(d[24], "O", "occlusion",     "#",            &bu_opt_int,     &s->occlusion,  "Occlusion mode (1 to 3, e.g., -O 1)");

    /* RUNTIME BEHAVIOR (dev and non-dev) */
    BU_OPT(d[25],  "",  "gui",           "",             NULL,            &s->use_gui,    "Force use of GUI.");
    BU_OPT(d[26],  "",  "no-gui",        "",             NULL,            &s->no_gui,     "Do not use GUI, even if available information is insufficient to generate image.");
    BU_OPT(d[27], "d", "fbserv-device", "<device>",     &bu_opt_vls,     s->fb_dev,      "Device for framebuffer viewing (e.g., -d /dev/wgl)");
    BU_OPT(d[28], "p", "fbserv-port",   "#",            &bu_opt_int,     &s->port,       "Port # for framebuffer");
    BU_OPT(d[29], "",  "benchmark",     "",             NULL,            &s->benchmark,  "Benchmark mode (no randomness)");
    BU_OPT(d[30], "",  "cpu-count",     "#",            &bu_opt_int,     &s->cpus,       "Specify the number of CPUs to use");
    BU_OPT(d[31], "",  "pid-file",      "<filename>",   &bu_opt_vls,     s->pid_file,    "File used for tracking PID numbers");
    BU_OPT(d[32], "",  "log-file",      "<filename>",   &bu_opt_vls,     s->log_file,    "Log debugging output to this file");
    BU_OPT(d[33], "v", "verbose",       "#",            &bu_opt_int,     &s->verbose,    "Verbosity");
    BU_OPT(d[34], "",  "cut-steps",     "#",            &bu_opt_int,     &s->cut_steps,  "Generate a cutting-plane animation with this many frames");
    BU_OPT(d[35], "",  "cut-direction", "<vector>",     &opt_cut_direction, s,            "Model-space direction in which the cutting plane advances");
    BU_OPT(d[36], "",  "animation-fps", "#",            &bu_opt_int,     &s->animation_fps, "Animation frames per second");
    BU_OPT(d[37], "",  "ao-samples",    "#",            &bu_opt_int,     &s->ao_samples, "Ambient occlusion samples per ray");
    BU_OPT(d[38], "",  "ao-radius",     "<float>",      &bu_opt_fastf_t, &s->ao_radius,  "Ambient occlusion maximum radius");
    BU_OPT(d[39], "",  "render-spec",   "<file.json>",  &bu_opt_vls,     s->render_spec, "Load a complete declarative render specification");
    BU_OPT(d[40], "",  "animation-duration", "<seconds>", &bu_opt_fastf_t, &s->animation_duration, "Animation duration in seconds");
    BU_OPT(d[41], "",  "animation-frames", "#",          &bu_opt_int,     &s->animation_frames, "Exact animation frame count");
    BU_OPT(d[42], "",  "animation-plays", "#",           &bu_opt_int,     &s->animation_plays, "APNG play count (0 means indefinite)");
    BU_OPT(d[43], "",  "frame-dir",     "<directory>",   &bu_opt_vls,     s->frame_dir, "Preserve numbered PNG animation frames");
    BU_OPT(d[44], "",  "resume",        "",              NULL,            &s->resume, "Resume using valid frames in --frame-dir");
    BU_OPT(d[45], "",  "save-view-keyframe", "<file.json>", &bu_opt_vls, s->save_view_keyframe, "Create or append a camera view keyframe");
    BU_OPT(d[46], "",  "time",          "<seconds>",     &bu_opt_fastf_t, &s->keyframe_time, "Keyframe time for --save-view-keyframe");
    BU_OPT(d[47], "",  "replace-keyframe", "",           NULL,            &s->replace_keyframe, "Replace an existing camera keyframe at --time");
    BU_OPT(d[48], "",  "animation-cyclic", "0|1",        &bu_opt_int,     &s->animation_cyclic, "Exclude a duplicate animation endpoint");
    BU_OPT(d[49], "A", "animate",       "cut|orbit|turntable", &bu_opt_vls, s->animation_preset, "Apply a standard animation preset");
    BU_OPT(d[50], "",  "animation-spec", "<file.json>",  &bu_opt_vls,     s->animation_file, "Customize animation using an animation-only JSON specification");
    BU_OPT_NULL(d[51]);

    /* initialize progname for run-time resource finding */
    bu_setprogname(argv[0]);
    av0 = argv[0];

    rtwizard_load_specs(&argc, &argv, s);

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

    uac = bu_opt_parse(&optparse_msg, argc, (const char **)argv, d);

    if (uac == -1) {
	bu_exit(EXIT_FAILURE, "%s", bu_vls_addr(&optparse_msg));
    }
    bu_vls_free(&optparse_msg);

    if (need_help) {
	rtwizard_help((struct bu_opt_desc *)&d);
	bu_exit(EXIT_SUCCESS, NULL);
    }

    if (need_help_dev) {
	rtwizard_help_dev((struct bu_opt_desc *)&d);
	bu_exit(EXIT_SUCCESS, NULL);
    }

    {
	int stop = 0;
	for (i = 0; i < uac; i++) {
	    if (argv[i][0] == '-' && argv[i][1] == '?') {
		need_help=1;
	    } else if (argv[i][0] == '-') {
		bu_log("ERROR: unknown option %s.\n", argv[i]);
		stop++;
	    }
	}
	if (stop && !need_help)
	    bu_exit(EXIT_FAILURE, "Halting.  Unknown options encountered.\n");
    }

    if (need_help) {
	rtwizard_help((struct bu_opt_desc *)&d);
	bu_exit(EXIT_SUCCESS, NULL);
    }

    if (s->image_type != '\0') {
	bu_log("Image type: %c\n", s->image_type);
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

    if (s->cut_steps && !bu_vls_strlen(s->animation_preset) && !bu_vls_strlen(s->animation_file))
	bu_vls_strcpy(s->animation_preset, "cut");
    if (bu_vls_strlen(s->animation_preset) && bu_vls_strlen(s->animation_file))
	bu_exit(EXIT_FAILURE, "ERROR: a render specification may not contain both a preset and tracks.\n");
    if (bu_vls_strlen(s->animation_preset) &&
	!BU_STR_EQUAL(bu_vls_addr(s->animation_preset), "cut") &&
	!BU_STR_EQUAL(bu_vls_addr(s->animation_preset), "orbit") &&
	!BU_STR_EQUAL(bu_vls_addr(s->animation_preset), "turntable"))
	bu_exit(EXIT_FAILURE, "ERROR: animation preset must be cut, orbit, or turntable.\n");
    /* Presets have simple defaults.  For track files, zero remains an
     * intentional sentinel so timing is read from the JSON specification. */
    if (bu_vls_strlen(s->animation_preset)) {
	if (s->animation_fps == 0) s->animation_fps = 24;
	if (NEAR_ZERO(s->animation_duration, SMALL_FASTF)) s->animation_duration = 5.0;
    }

    if (s->cut_steps != 0 && s->cut_steps < 2)
	bu_exit(EXIT_FAILURE, "ERROR: --cut-steps must be at least 2.\n");
    if (s->animation_fps < 0)
	bu_exit(EXIT_FAILURE, "ERROR: --animation-fps must be positive when specified.\n");
    if (s->animation_duration < 0.0 || !isfinite(s->animation_duration))
	bu_exit(EXIT_FAILURE, "ERROR: --animation-duration must be positive and finite when specified.\n");
    if (s->animation_frames < 0 || s->animation_frames == 1)
	bu_exit(EXIT_FAILURE, "ERROR: --animation-frames must be zero or at least 2.\n");
    if (s->animation_plays < -1)
	bu_exit(EXIT_FAILURE, "ERROR: --animation-plays may not be negative (except the unset value).\n");
    if (s->animation_cyclic < -1 || s->animation_cyclic > 1)
	bu_exit(EXIT_FAILURE, "ERROR: --animation-cyclic must be 0 or 1.\n");
    if (bu_vls_strlen(s->save_view_keyframe) && (!(s->keyframe_time < DBL_MAX) || s->keyframe_time < 0.0))
	bu_exit(EXIT_FAILURE, "ERROR: --save-view-keyframe requires a nonnegative --time.\n");
    if (s->ao_samples < 0 || s->ao_radius < 0.0)
	bu_exit(EXIT_FAILURE, "ERROR: ambient occlusion samples and radius may not be negative.\n");
    if (s->cut_direction_set && MAGNITUDE(s->cut_direction) <= SQRT_SMALL_FASTF)
	bu_exit(EXIT_FAILURE, "ERROR: --cut-direction must be a non-zero vector.\n");
    if (!isfinite(s->orbit_angle) || !isfinite(s->turntable_angle))
	bu_exit(EXIT_FAILURE, "ERROR: animation angles must be finite.\n");
    if (MAGNITUDE(s->orbit_axis) <= SQRT_SMALL_FASTF || MAGNITUDE(s->turntable_axis) <= SQRT_SMALL_FASTF)
	bu_exit(EXIT_FAILURE, "ERROR: orbit and turntable axes must be non-zero vectors.\n");
    if (!isfinite(s->orbit_axis[0]) || !isfinite(s->orbit_axis[1]) || !isfinite(s->orbit_axis[2]) ||
	!isfinite(s->turntable_axis[0]) || !isfinite(s->turntable_axis[1]) || !isfinite(s->turntable_axis[2]))
	bu_exit(EXIT_FAILURE, "ERROR: orbit and turntable axes must be finite.\n");
    if (s->orbit_radius < DBL_MAX && (!(s->orbit_radius > 0.0) || !isfinite(s->orbit_radius)))
	bu_exit(EXIT_FAILURE, "ERROR: orbit radius must be positive and finite.\n");
    if (s->orbit_elevation < DBL_MAX && !isfinite(s->orbit_elevation))
	bu_exit(EXIT_FAILURE, "ERROR: orbit elevation must be finite.\n");
    {
	int have_animation = bu_vls_strlen(s->animation_preset) || bu_vls_strlen(s->animation_file);
	if (have_animation && !bu_vls_strlen(s->output_file) && !bu_vls_strlen(s->frame_dir) && !s->use_gui)
	    bu_exit(EXIT_FAILURE, "ERROR: an animation requires -o or --frame-dir.\n");
	if (have_animation && bu_vls_strlen(s->output_file) &&
	!rtwizard_anim_path_supported(bu_vls_addr(s->output_file)))
	    bu_exit(EXIT_FAILURE, "ERROR: animation output must use .apng, .png, .avi, or .mjpg.\n");
    if (!have_animation && !s->use_gui && bu_vls_strlen(s->output_file) &&
	rtwizard_anim_only_path(bu_vls_addr(s->output_file)))
	bu_exit(EXIT_FAILURE, "ERROR: .apng, .avi, and .mjpg outputs require an animation option.\n");
    }

    if (!s->use_gui && !rtwizard_info_sufficient(&info_msg, s, s->image_type)) {
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
	(void)Tcl_CreateObjCommand(interp, "rtwizard_anim_write",
		rtwizard_anim_write_cmd, NULL, NULL);
	(void)Tcl_CreateObjCommand(interp, "rtwizard_image_valid",
		rtwizard_image_valid_cmd, NULL, NULL);
	(void)Tcl_CreateObjCommand(interp, "rtwizard_file_hash",
		rtwizard_file_hash_cmd, NULL, NULL);
	rtwizard_animation_init(interp);

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
	if (!rtwizard_info_sufficient(NULL, s, s->image_type)) {
	    /* If we *can* launch the GUI in this situation, do it */
	    if (s->no_gui) {
		bu_exit(EXIT_FAILURE, "ERROR: Image type %c specified, but supplied inputs are insufficient for generating a type %c image.\n", s->image_type, s->image_type);
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
