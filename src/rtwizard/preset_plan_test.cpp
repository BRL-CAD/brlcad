/*            P R E S E T _ P L A N _ T E S T . C P P
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 */

#include "common.h"

#include <cmath>
#include <iostream>
#include <string>

#include "bu/app.h"

#include "preset_plan.h"

namespace {

constexpr double test_tolerance = 1.0e-9;

static bool
near(double lhs, double rhs)
{
    return std::fabs(lhs-rhs) <= test_tolerance;
}

static PresetSettings
base_settings(const std::string &preset)
{
    PresetSettings settings;
    settings.preset = preset;
    settings.duration = 2.0;
    settings.fps = 2;
    settings.frame_count = 3;
    settings.plays = 1;
    settings.viewsize = 20.0;
    settings.orientation = {0.0, 0.0, 0.0, 1.0};
    settings.eye = {10.0, 0.0, 0.0};
    return settings;
}

static int
test_cut_timing()
{
    PresetSettings settings = base_settings("cut");
    settings.width = 100;
    settings.height = 100;
    settings.direction = {0.0, 0.0, -1.0};
    PresetSceneFacts facts;
    facts.sweep = {-5.0, 5.0};
    AnimationPlan plan = plan_animation_preset(settings, facts);
    if (plan.frames.size() != 3 || !near(plan.frames[0].time, 0.0) ||
	!near(plan.frames[1].time, 1.0) || !near(plan.frames[2].time, 2.0)) {
	std::cerr << "cut frame timing is incorrect\n";
	return 1;
    }
    if (plan.frames.front().cut_plane.empty() || plan.frames.back().cut_plane.empty()) {
	std::cerr << "cut frames are missing planes\n";
	return 1;
    }
    return 0;
}

static int
test_cyclic_orbit()
{
    PresetSettings settings = base_settings("orbit");
    settings.frame_count = 4;
    settings.angle = 360.0;
    settings.axis = {0.0, 0.0, 1.0};
    PresetSceneFacts facts;
    facts.center = {0.0, 0.0, 0.0};
    AnimationPlan plan = plan_animation_preset(settings, facts);
    if (!plan.cyclic || plan.frames.size() != 4 || !near(plan.frames.back().time, 1.5)) {
	std::cerr << "cyclic orbit endpoint policy is incorrect\n";
	return 1;
    }
    if (near(plan.frames.front().eye[0], plan.frames.back().eye[0]) &&
	near(plan.frames.front().eye[1], plan.frames.back().eye[1])) {
	std::cerr << "cyclic orbit duplicated its endpoint\n";
	return 1;
    }
    return 0;
}

static int
test_turntable_operation()
{
    PresetSettings settings = base_settings("turntable");
    settings.angle = 90.0;
    settings.axis = {0.0, 0.0, 1.0};
    settings.object = "model";
    PresetSceneFacts facts;
    facts.center = {1.0, 2.0, 3.0};
    AnimationPlan plan = plan_animation_preset(settings, facts);
    if (plan.frames.size() != 3 || plan.frames.back().operations.size() != 1) {
	std::cerr << "turntable operation count is incorrect\n";
	return 1;
    }
    const DatabaseOperation &operation = plan.frames.back().operations.front();
    if (operation.path != "/model" || operation.action != "lmul" || operation.values.size() != 16) {
	std::cerr << "turntable operation is incorrect\n";
	return 1;
    }
    return 0;
}

}

int
main(int argc, char **argv)
{
    bu_setprogname(argv[0]);
    if (argc != 1) return 1;
    return test_cut_timing() + test_cyclic_orbit() + test_turntable_operation();
}

/*
 * Local Variables:
 * mode: C++
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
