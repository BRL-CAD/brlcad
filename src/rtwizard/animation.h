/*                     A N I M A T I O N . H
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 */

#ifndef RTWIZARD_ANIMATION_H
#define RTWIZARD_ANIMATION_H

#ifdef __cplusplus
#  include <array>
#  include <string>
#  include <vector>

namespace rtwizard {

struct AnimationFrame {
    double time = 0.0;
    double view_size = 0.0;
    std::array<double, 4> orientation{{0.0, 0.0, 0.0, 1.0}};
    std::array<double, 3> eye{{0.0, 0.0, 0.0}};
    double perspective = 0.0;
    std::string cut_plane;
    std::vector<std::string> commands;
};

struct AnimationPlan {
    double duration = 0.0;
    int fps = 0;
    bool cyclic = false;
    int plays = 1;
    std::vector<AnimationFrame> frames;
};

/* Evaluate a track-based version 1 animation without involving Tcl. */
bool evaluate_animation(const std::string &path, double base_view_size,
    const std::array<double, 4> &base_orientation,
    const std::array<double, 3> &base_eye, double base_perspective,
    double duration_override, int fps_override, int frames_override,
    int plays_override, int cyclic_override, double local_to_base,
    AnimationPlan &plan, std::string &error);

/* Safely update the camera track in a render specification. */
bool save_view_keyframe(const std::string &path, double time, bool replace,
    const std::array<double, 3> &eye,
    const std::array<double, 4> &orientation,
    double view_size, double perspective, double database_local_to_base,
    std::string &error);

} // namespace rtwizard
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* Convert a declarative render specification into ordinary rtwizard options.
 * General animation tracks retain the specification path for later typed
 * evaluation.  The caller owns the returned vector. */
int rtwizard_spec_to_argv(const char *path, int *argc, char ***argv, char **errmsg);
void rtwizard_spec_argv_free(int argc, char **argv);

#ifdef __cplusplus
}
#endif

#endif /* RTWIZARD_ANIMATION_H */
