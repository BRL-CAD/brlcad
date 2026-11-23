/*              A N I M A T I O N _ P L A N . H
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 */

#ifndef RTWIZARD_ANIMATION_PLAN_H
#define RTWIZARD_ANIMATION_PLAN_H

#include <string>
#include <vector>

enum class DatabaseOperationType {
    Transform,
    Material,
    Color,
    Temperature
};

struct DatabaseOperation {
    DatabaseOperationType type;
    std::string path;
    std::string action;
    std::vector<double> values;
    std::string text;
};

struct AnimationFrame {
    double time = 0.0;
    double viewsize = 0.0;
    std::vector<double> orientation;
    std::vector<double> eye;
    double perspective = 0.0;
    std::string cut_plane;
    std::vector<DatabaseOperation> operations;
};

struct AnimationPlan {
    double duration = 0.0;
    int fps = 0;
    bool cyclic = false;
    int plays = 0;
    std::vector<AnimationFrame> frames;
};

void normalize_animation_vector(std::vector<double> &vector, const std::string &where);
int animation_frame_count(double duration, int fps, int requested, bool cyclic);
std::vector<double> animation_look_at_quaternion(const std::vector<double> &direction,
    const std::vector<double> &up);
AnimationFrame make_animation_frame(double time, double viewsize,
    const std::vector<double> &orientation, const std::vector<double> &eye,
    double perspective);

#endif /* RTWIZARD_ANIMATION_PLAN_H */
