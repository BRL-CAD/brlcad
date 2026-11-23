/*            A N I M A T I O N _ P L A N . C P P
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 */

#include "common.h"

#include <cmath>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

#include "vmath.h"
#include "bn/mat.h"
#include "bn/qmath.h"

#include "animation_plan.h"

namespace {

constexpr double vector_magnitude_tolerance = 1.0e-12;

}

int
animation_frame_count(double duration, int fps, int requested, bool cyclic)
{
    if (!(duration > 0.0) || !std::isfinite(duration) || fps <= 0)
	throw std::runtime_error("animation duration and frame rate must be positive");
    const double count = requested ? requested : std::round(duration * fps) + (cyclic ? 0 : 1);
    if (count < 2 || count > std::numeric_limits<int>::max())
	throw std::runtime_error("animation frame count is out of range");
    return static_cast<int>(count);
}

void
normalize_animation_vector(std::vector<double> &vector, const std::string &where)
{
    if (vector.size() != 3) throw std::runtime_error(where + " must contain three components");
    double magnitude = std::sqrt(vector[0]*vector[0] + vector[1]*vector[1] + vector[2]*vector[2]);
    if (!(magnitude > vector_magnitude_tolerance)) throw std::runtime_error(where + " must be nonzero");
    for (double &component : vector) component /= magnitude;
}

std::vector<double>
animation_look_at_quaternion(const std::vector<double> &direction, const std::vector<double> &up)
{
    if (direction.size() != 3 || (!up.empty() && up.size() != 3))
	throw std::runtime_error("camera direction and up vectors must contain three components");
    vect_t forward = {direction[0], direction[1], direction[2]};
    vect_t right, camera_up;
    mat_t matrix;
    quat_t quaternion;
    VUNITIZE(forward);
    if (up.empty()) {
	bn_mat_lookat(matrix, forward, 0);
    } else {
	vect_t up_vector = {up[0], up[1], up[2]};
	if (MAGNITUDE(up_vector) <= SMALL_FASTF) throw std::runtime_error("camera up vector must be nonzero");
	VUNITIZE(up_vector);
	VCROSS(right, forward, up_vector);
	if (MAGNITUDE(right) <= SMALL_FASTF) throw std::runtime_error("camera up vector is parallel to the viewing direction");
	VUNITIZE(right);
	VCROSS(camera_up, right, forward);
	MAT_IDN(matrix);
	VMOVE(&matrix[0], right);
	VMOVE(&matrix[4], camera_up);
	VREVERSE(&matrix[8], forward);
    }
    quat_mat2quat(quaternion, matrix);
    return {quaternion[0], quaternion[1], quaternion[2], quaternion[3]};
}

AnimationFrame
make_animation_frame(double time, double viewsize, const std::vector<double> &orientation,
    const std::vector<double> &eye, double perspective)
{
    AnimationFrame frame;
    frame.time = time;
    frame.viewsize = viewsize;
    frame.orientation = orientation;
    frame.eye = eye;
    frame.perspective = perspective;
    return frame;
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
