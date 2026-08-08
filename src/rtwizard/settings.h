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

enum rtwizard_fb_transport {
    RTWIZARD_FB_AUTO = 0,
    RTWIZARD_FB_IPC,
    RTWIZARD_FB_TCP
};

/* Private application settings shared by the command-line parser and the
 * native render engine.  This is deliberately not an installed API. */
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
    int fb_transport;
    struct bu_vls *log_file;
    struct bu_vls *pid_file;

    struct bu_vls *render_spec;
    struct bu_vls *animation_file;
    struct bu_vls *animation_preset;
    struct bu_vls *frame_dir;
    double animation_duration;
    int animation_frames;
    int animation_plays;
    int animation_cyclic;
    int resume;

    double orbit_angle;
    vect_t orbit_axis;
    vect_t orbit_center;
    double orbit_elevation;
    double orbit_radius;

    struct bu_vls *turntable_object;
    double turntable_angle;
    vect_t turntable_axis;
    vect_t turntable_center;

    struct bu_vls *save_view_keyframe;
    double keyframe_time;
    int replace_keyframe;

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
