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
#define RTWIZARD_ANIMATION_FPS_DEFAULT 24
#define RTWIZARD_ANIMATION_DURATION_DEFAULT 5.0
#define RTWIZARD_MAGIC 0x72747769

struct rtwizard_animation_options {
    double duration;
    int frames;
    int plays;
    int cyclic;
    int resume;
    int first_frame;
    int last_frame;
    double orbit_angle;
    vect_t orbit_axis;
    vect_t orbit_center;
    double orbit_elevation;
    double orbit_radius;
    double turntable_angle;
    vect_t turntable_axis;
    vect_t turntable_center;
};

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
    struct rtwizard_animation_options animation;
    struct bu_vls *animation_preset;
    struct bu_vls *animation_tracks;
    struct bu_vls *frame_dir;
    int preview_worker;
    struct bu_vls *turntable_object;
    struct bu_vls *save_view_keyframe;
    double keyframe_time;
    int replace_keyframe;
    char image_type;

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

__BEGIN_DECLS
int rtwizard_is_animated(const struct rtwizard_settings *s);
__END_DECLS

#endif /* RTWIZARD_SETTINGS_H */
