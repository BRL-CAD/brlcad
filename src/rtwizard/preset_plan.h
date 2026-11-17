/*                P R E S E T _ P L A N . H
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 */

#ifndef RTWIZARD_PRESET_PLAN_H
#define RTWIZARD_PRESET_PLAN_H

#include <string>
#include <utility>
#include <vector>

#include "animation_plan.h"

struct PresetSettings {
    std::string preset;
    std::string database;
    std::vector<std::string> objects;
    double duration = 0.0;
    int fps = 0;
    int frame_count = 0;
    int cyclic_override = -1;
    int plays = -1;
    double viewsize = 0.0;
    double perspective = 0.0;
    std::vector<double> orientation;
    std::vector<double> eye;
    double angle = 0.0;
    int cut_steps = 0;
    int width = 0;
    int height = 0;
    std::vector<double> direction;
    std::vector<double> axis;
    bool has_center = false;
    std::vector<double> center;
    bool has_radius = false;
    double radius = 0.0;
    bool has_elevation = false;
    double elevation = 0.0;
    std::string object;
};

struct PresetSceneFacts {
    std::pair<double, double> sweep = {0.0, 0.0};
    std::vector<double> center;
};

AnimationPlan plan_animation_preset(const PresetSettings &settings,
    const PresetSceneFacts &facts);

#endif /* RTWIZARD_PRESET_PLAN_H */
