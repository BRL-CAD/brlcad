/*                      S E T T I N G S . H
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 */

#ifndef RTWIZARD_SETTINGS_H
#define RTWIZARD_SETTINGS_H

#include "common.h"

#include "vmath.h"
#include "bu/color.h"
#include "bu/ptbl.h"
#include "bu/vls.h"

#define RTWIZARD_SIZE_DEFAULT 512
#define RTWIZARD_MAGIC 0x72747769

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
    struct bu_vls *fb_target;
    int port;
    struct bu_vls *log_file;
    struct bu_vls *pid_file;

    size_t width;
    int width_set;
    size_t height;
    int height_set;
    size_t size;
    int size_set;

    struct bu_color *bkg_color;
    struct bu_color *line_color;
    struct bu_color *non_line_color;

    double ghost_intensity;
    int occlusion;
    int benchmark;
    int cpus;
    int process_timeout;

    int cut_steps;
    int animation_fps;
    vect_t cut_direction;
    int cut_direction_set;

    int ao_samples;
    double ao_radius;

    double viewsize;
    quat_t orientation;
    vect_t eye_pt;

    double az, el, tw;
    double perspective;
    double zoom;
    vect_t center;
};

#endif /* RTWIZARD_SETTINGS_H */
