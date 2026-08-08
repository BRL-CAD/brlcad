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

#include "vmath.h"
#include "bu/app.h"
#include "bu/color.h"
#include "bu/file.h"
#include "bu/mime.h"
#include "bu/malloc.h"
#include "bu/log.h"
#include "bu/path.h"
#include "bu/ptbl.h"
#include "bu/opt.h"
#include "bu/str.h"
#include "raytrace.h"

#include "animation.h"
#if RTWIZARD_HAVE_GUI
#  include "gui.h"
#endif
#include "settings.h"
#include "render.h"

#ifndef RTWIZARD_HAVE_GUI
#  define RTWIZARD_HAVE_GUI 0
#endif

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
    s->fb_transport = RTWIZARD_FB_AUTO;
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

    s->use_gui = 0;
    s->no_gui = 0;

    s->verbose = 0;
    return s;
}


void rtwizard_settings_destroy(struct rtwizard_settings *s) {
    size_t i;

    if (!s)
	return;
    for (i = 0; i < BU_PTBL_LEN(s->color); i++)
	bu_free((char *)BU_PTBL_GET(s->color, i), "rtwizard color object");
    for (i = 0; i < BU_PTBL_LEN(s->ghost); i++)
	bu_free((char *)BU_PTBL_GET(s->ghost, i), "rtwizard ghost object");
    for (i = 0; i < BU_PTBL_LEN(s->line); i++)
	bu_free((char *)BU_PTBL_GET(s->line, i), "rtwizard line object");
    bu_ptbl_free(s->color);
    bu_ptbl_free(s->ghost);
    bu_ptbl_free(s->line);
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
    if (s->az < DBL_MAX || s->el < DBL_MAX || s->tw < DBL_MAX ||
	s->zoom < DBL_MAX || s->center[0] < DBL_MAX) {
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


int
opt_width(struct bu_vls *msg, size_t argc, const char **argv, void *settings)
{
    struct rtwizard_settings *s = (struct rtwizard_settings *)settings;
    if (s) {
	int ret = bu_opt_int(msg, argc, argv, (void *)&s->width);
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
	int ret = bu_opt_int(msg, argc, argv, (void *)&s->height);
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
	int ret = bu_opt_int(msg, argc, argv, (void *)&s->size);
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
opt_fb_transport(struct bu_vls *msg, size_t argc, const char **argv, void *settings)
{
    struct rtwizard_settings *s = (struct rtwizard_settings *)settings;

    BU_OPT_CHECK_ARGV0(msg, argc, argv, "opt_fb_transport");
    if (BU_STR_EQUAL(argv[0], "auto"))
	s->fb_transport = RTWIZARD_FB_AUTO;
    else if (BU_STR_EQUAL(argv[0], "ipc"))
	s->fb_transport = RTWIZARD_FB_IPC;
    else if (BU_STR_EQUAL(argv[0], "tcp"))
	s->fb_transport = RTWIZARD_FB_TCP;
    else {
	bu_vls_printf(msg, "invalid framebuffer transport '%s' (expected auto, ipc, or tcp)\n", argv[0]);
	return -1;
    }
    return 1;
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
	    if (i == 0)
		objs[i] = ' ';
	    if (objs[i-1] != '\\')
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
    char *avnum[5] = {NULL, NULL, NULL, NULL, NULL};

    quat_t *q = (quat_t *)inq;
    BU_OPT_CHECK_ARGV0(msg, argc, argv, "bu_opt_int");


    /* First, see if the first string converts to a quat_t*/
    str1 = bu_strdup(argv[0]);
    while (str1[i]) {
	/* If we have a separator, replace with a space */
	if (str1[i] == ',' || str1[i] == '/') str1[i] = ' ';
	i++;
    }
    acnum = bu_argv_from_string(avnum, 4, str1);
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
    bu_vls_printf(&slog, "fb_transport: %d\n", s->fb_transport);
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


static int
rtwizard_anim_path_supported_native(const char *path)
{
    const char *ext = strrchr(path, '.');
    return (ext && (BU_STR_EQUIV(ext, ".apng") || BU_STR_EQUIV(ext, ".png") ||
	BU_STR_EQUIV(ext, ".avi") || BU_STR_EQUIV(ext, ".mjpg")));
}

static int
rtwizard_anim_only_path_native(const char *path)
{
    const char *ext = strrchr(path, '.');
    return (ext && (BU_STR_EQUIV(ext, ".apng") || BU_STR_EQUIV(ext, ".avi") ||
	BU_STR_EQUIV(ext, ".mjpg")));
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
    bu_vls_sprintf(&filtered, "h help-dev i o s w n render-spec animation animation-file animation-duration animation-fps animation-frames animation-plays animation-cyclic frame-dir resume cut-steps cut-direction orbit-angle orbit-axis orbit-center orbit-elevation orbit-radius turntable-object turntable-angle turntable-axis turntable-center save-view-keyframe time replace-keyframe");
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
    bu_vls_sprintf(&filtered, "gui no-gui d p fbserv-transport v");
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
    const char *devopts = "benchmark cpu-count pid-file log-file";

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


static const char *
rtwizard_short_option(const char *longopt)
{
    if (BU_STR_EQUAL(longopt, "--input-file")) return "-i";
    if (BU_STR_EQUAL(longopt, "--output-file")) return "-o";
    if (BU_STR_EQUAL(longopt, "--size")) return "-s";
    if (BU_STR_EQUAL(longopt, "--width")) return "-w";
    if (BU_STR_EQUAL(longopt, "--height")) return "-n";
    if (BU_STR_EQUAL(longopt, "--type")) return "-t";
    if (BU_STR_EQUAL(longopt, "--color-objects")) return "-c";
    if (BU_STR_EQUAL(longopt, "--line-objects")) return "-l";
    if (BU_STR_EQUAL(longopt, "--ghost-objects")) return "-g";
    if (BU_STR_EQUAL(longopt, "--azimuth")) return "-a";
    if (BU_STR_EQUAL(longopt, "--elevation")) return "-e";
    if (BU_STR_EQUAL(longopt, "--zoom")) return "-z";
    if (BU_STR_EQUAL(longopt, "--perspective")) return "-P";
    if (BU_STR_EQUAL(longopt, "--background-color")) return "-C";
    if (BU_STR_EQUAL(longopt, "--ghost-intensity")) return "-G";
    if (BU_STR_EQUAL(longopt, "--occlusion")) return "-O";
    if (BU_STR_EQUAL(longopt, "--fbserv-device")) return "-d";
    if (BU_STR_EQUAL(longopt, "--fbserv-port")) return "-p";
    if (BU_STR_EQUAL(longopt, "--verbose")) return "-v";
    return NULL;
}


static int
rtwizard_spec_option_takes_arg(const char *opt)
{
    return !(BU_STR_EQUAL(opt, "--benchmark") ||
	    BU_STR_EQUAL(opt, "--gui") ||
	    BU_STR_EQUAL(opt, "--no-gui") ||
	    BU_STR_EQUAL(opt, "--resume") ||
	    BU_STR_EQUAL(opt, "--replace-keyframe"));
}


/* Expand a render specification before normal option parsing.  Spec options
 * are placed first.  Explicit command-line instances suppress their JSON
 * counterpart because bu_opt_vls intentionally concatenates repeated string
 * options.  This also gives object role lists replace rather than append
 * semantics. */
static void
rtwizard_expand_render_spec(int *argcp, char ***argvp)
{
    int argc = *argcp;
    char **argv = *argvp;
    const char *spec = NULL;
    int spec_index = -1;
    int spec_separate = 0;
    int i;
    int sac = 0;
    char **sav = NULL;
    char *errmsg = NULL;
    char **merged;
    int mac = 0;
    int cli_animation = rtwizard_argv_has_option(argc, argv, NULL, "--animation") ||
	rtwizard_argv_has_option(argc, argv, NULL, "--animation-file") ||
	rtwizard_argv_has_option(argc, argv, NULL, "--cut-steps");
    int cli_gui = rtwizard_argv_has_option(argc, argv, NULL, "--gui");
    int cli_no_gui = rtwizard_argv_has_option(argc, argv, NULL, "--no-gui");

    for (i = 1; i < argc; i++) {
	if (BU_STR_EQUAL(argv[i], "--render-spec")) {
	    if (i + 1 >= argc)
		bu_exit(EXIT_FAILURE, "ERROR: --render-spec requires a filename.\n");
	    spec = argv[i+1];
	    spec_index = i;
	    spec_separate = 1;
	    break;
	}
	if (bu_strncmp(argv[i], "--render-spec=", 14) == 0) {
	    spec = argv[i] + 14;
	    spec_index = i;
	    break;
	}
    }
    if (!spec)
	return;
    if (rtwizard_spec_to_argv(spec, &sac, &sav, &errmsg) != 0) {
	bu_exit(EXIT_FAILURE, "ERROR: %s\n", errmsg ? errmsg : "unable to load render specification");
    }

    merged = (char **)bu_calloc((size_t)argc + (size_t)sac + 1, sizeof(char *), "merged rtwizard argv");
    merged[mac++] = argv[0];
    for (i = 0; i < sac; i++) {
	int skip = 0;
	if (bu_strncmp(sav[i], "--", 2) == 0 &&
	    rtwizard_argv_has_option(argc, argv, rtwizard_short_option(sav[i]), sav[i])) {
	    skip = 1;
	}
	if (cli_animation && (BU_STR_EQUAL(sav[i], "--animation") ||
		BU_STR_EQUAL(sav[i], "--animation-file"))) {
	    skip = 1;
	}
	/* These two flags are alternate values of one setting. */
	if ((cli_gui && BU_STR_EQUAL(sav[i], "--no-gui")) ||
	    (cli_no_gui && BU_STR_EQUAL(sav[i], "--gui")))
	    skip = 1;
	if (skip) {
	    if (rtwizard_spec_option_takes_arg(sav[i]) && i + 1 < sac) i++;
	    continue;
	}
	merged[mac++] = bu_strdup(sav[i]);
    }
    /* Retain the option itself so the resolved path is available to the
     * native renderer and view-keyframe capture. */
    merged[mac++] = bu_strdup("--render-spec");
    merged[mac++] = bu_strdup(spec);
    for (i = 1; i < argc; i++) {
	if (i == spec_index) {
	    if (spec_separate) i++;
	    continue;
	}
	merged[mac++] = argv[i];
    }
    rtwizard_spec_argv_free(sac, sav);
    if (errmsg) bu_free(errmsg, "render spec error");
    *argcp = mac;
    *argvp = merged;
}


int
main(int argc, char **argv)
{
    char *av0;
    char type = '\0';
    int i = 0;
    int need_help = 0;
    int need_help_dev = 0;
    int uac = 0;

    struct bu_vls optparse_msg = BU_VLS_INIT_ZERO;
    struct bu_vls info_msg = BU_VLS_INIT_ZERO;
    struct rtwizard_settings *s = rtwizard_settings_create();
    struct bu_opt_desc d[62];

    BU_OPT(d[0],  "h", "help",          "",             NULL,            &need_help,     "Print options help and exit");
    BU_OPT(d[1],  "",  "help-dev",      "",             NULL,            &need_help_dev, "Print development and programmatic options.");

    /* I/O FILE OPTIONS */
    BU_OPT(d[2],  "i", "input-file",    "<filename>",   &bu_opt_vls,     s->input_file,  "Input .g database file");
    BU_OPT(d[3],  "o", "output-file",   "<filename>",   &bu_opt_vls,     s->output_file, "Image output file name");
    BU_OPT(d[4],  "s", "size",          "#",            &opt_size,       s,              "Output width & height (for square image)");
    BU_OPT(d[5],  "w", "width",         "#",            &opt_width,      s,              "Output image width (overrides -s)");
    BU_OPT(d[6],  "n", "height",        "#",            &opt_height,     s,              "Output image height (overrides -s)");

    /* VISUALIZATION OPTIONS */
    BU_OPT(d[7],  "t", "type",          "A|B|C|D|E|F",  &opt_letter,     &type,          "Specify RtWizard picture type");
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
    BU_OPT(d[28], "p", "fbserv-port",   "#",            &bu_opt_int,     &s->port,       "Framebuffer service port (used by TCP fallback)");
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
    BU_OPT(d[40], "",  "animation",     "cut|orbit|turntable", &bu_opt_vls, s->animation_preset, "Select an animation preset");
    BU_OPT(d[41], "",  "animation-file","<file.json>",  &bu_opt_vls,     s->animation_file, "Load animation tracks from a render specification");
    BU_OPT(d[42], "",  "animation-duration", "<seconds>", &bu_opt_fastf_t, &s->animation_duration, "Animation duration in seconds");
    BU_OPT(d[43], "",  "animation-frames", "#",          &bu_opt_int,     &s->animation_frames, "Exact animation frame count");
    BU_OPT(d[44], "",  "animation-plays", "#",           &bu_opt_int,     &s->animation_plays, "APNG play count (0 means indefinite)");
    BU_OPT(d[45], "",  "frame-dir",     "<directory>",   &bu_opt_vls,     s->frame_dir, "Preserve numbered PNG animation frames");
    BU_OPT(d[46], "",  "resume",        "",              NULL,            &s->resume, "Resume using valid frames in --frame-dir");
    BU_OPT(d[47], "",  "orbit-angle",   "<degrees>",     &bu_opt_fastf_t, &s->orbit_angle, "Orbit angle");
    BU_OPT(d[48], "",  "orbit-axis",    "<vector>",      &bu_opt_vect_t,  &s->orbit_axis, "Orbit axis");
    BU_OPT(d[49], "",  "orbit-center",  "<point>",       &bu_opt_vect_t,  &s->orbit_center, "Orbit target center");
    BU_OPT(d[50], "",  "orbit-elevation", "<degrees>",   &bu_opt_fastf_t, &s->orbit_elevation, "Camera elevation above the orbit plane");
    BU_OPT(d[51], "",  "orbit-radius",  "<distance>",    &bu_opt_fastf_t, &s->orbit_radius, "Camera distance from orbit center");
    BU_OPT(d[52], "",  "turntable-object", "<path>",      &bu_opt_vls,     s->turntable_object, "Database path rotated by the turntable preset");
    BU_OPT(d[53], "",  "turntable-angle", "<degrees>",    &bu_opt_fastf_t, &s->turntable_angle, "Turntable rotation angle");
    BU_OPT(d[54], "",  "turntable-axis", "<vector>",      &bu_opt_vect_t,  &s->turntable_axis, "Turntable axis");
    BU_OPT(d[55], "",  "turntable-center", "<point>",     &bu_opt_vect_t,  &s->turntable_center, "Turntable pivot");
    BU_OPT(d[56], "",  "save-view-keyframe", "<file.json>", &bu_opt_vls, s->save_view_keyframe, "Create or append a camera view keyframe");
    BU_OPT(d[57], "",  "time",          "<seconds>",     &bu_opt_fastf_t, &s->keyframe_time, "Keyframe time for --save-view-keyframe");
    BU_OPT(d[58], "",  "replace-keyframe", "",           NULL,            &s->replace_keyframe, "Replace an existing camera keyframe at --time");
    BU_OPT(d[59], "",  "animation-cyclic", "0|1",        &bu_opt_int,     &s->animation_cyclic, "Exclude a duplicate animation endpoint");
    BU_OPT(d[60], "",  "fbserv-transport", "auto|ipc|tcp", &opt_fb_transport, s, "Framebuffer transport (local IPC preferred by default)");
    BU_OPT_NULL(d[61]);

    /* initialize progname for run-time resource finding */
    bu_setprogname(argv[0]);
    av0 = argv[0];
#if !RTWIZARD_HAVE_GUI
    (void)av0;
#endif

    rtwizard_expand_render_spec(&argc, &argv);

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
		    rtwizard_anim_path_supported_native(argv[i])) {
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
	bu_exit(EXIT_FAILURE, "ERROR: --animation and --animation-file are mutually exclusive.\n");
    if (bu_vls_strlen(s->animation_preset) &&
	!BU_STR_EQUAL(bu_vls_addr(s->animation_preset), "cut") &&
	!BU_STR_EQUAL(bu_vls_addr(s->animation_preset), "orbit") &&
	!BU_STR_EQUAL(bu_vls_addr(s->animation_preset), "turntable"))
	bu_exit(EXIT_FAILURE, "ERROR: --animation must be cut, orbit, or turntable.\n");
    if (BU_STR_EQUAL(bu_vls_addr(s->animation_preset), "turntable") && !bu_vls_strlen(s->turntable_object))
	bu_exit(EXIT_FAILURE, "ERROR: --animation turntable requires --turntable-object.\n");

    /* Presets have simple defaults.  For track files, zero remains an
     * intentional sentinel so timing is read from the JSON specification. */
    if (bu_vls_strlen(s->animation_preset)) {
	if (s->animation_fps == 0) s->animation_fps = 10;
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
    if (MAGNITUDE(s->orbit_axis) <= SQRT_SMALL_FASTF || MAGNITUDE(s->turntable_axis) <= SQRT_SMALL_FASTF)
	bu_exit(EXIT_FAILURE, "ERROR: orbit and turntable axes must be non-zero vectors.\n");
    {
	int have_animation = bu_vls_strlen(s->animation_preset) || bu_vls_strlen(s->animation_file);
	if (have_animation && !bu_vls_strlen(s->output_file) && !bu_vls_strlen(s->frame_dir) && !s->use_gui)
	    bu_exit(EXIT_FAILURE, "ERROR: an animation requires -o or --frame-dir.\n");
	if (have_animation && bu_vls_strlen(s->output_file) &&
	!rtwizard_anim_path_supported_native(bu_vls_addr(s->output_file)))
	    bu_exit(EXIT_FAILURE, "ERROR: animation output must use .apng, .png, .avi, or .mjpg.\n");
    if (!have_animation && !s->use_gui && bu_vls_strlen(s->output_file) &&
	rtwizard_anim_only_path_native(bu_vls_addr(s->output_file)))
	bu_exit(EXIT_FAILURE, "ERROR: .apng, .avi, and .mjpg outputs require an animation option.\n");
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

    if (s->use_gui) {
#if RTWIZARD_HAVE_GUI
	return rtwizard_gui(av0, s, type);
#else
	bu_exit(EXIT_FAILURE, "ERROR: this rtwizard build does not include Qt GUI support; supply a complete render request with --no-gui.\n");
#endif
    }

    {
	char *render_error = NULL;
	int render_status = rtwizard_render(s, type, NULL, NULL, &render_error);
	if (render_status != BRLCAD_OK) {
	    bu_log("ERROR: %s\n", render_error ? render_error : "native render failed");
	    if (render_error) bu_free(render_error, "native render error");
	    return EXIT_FAILURE;
	}
	if (render_error) bu_free(render_error, "native render error");
	return EXIT_SUCCESS;
    }

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
