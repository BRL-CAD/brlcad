/* BRL-CAD
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include "common.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <limits>
#include <locale>
#include <map>
#include <memory>
#include <stdexcept>
#include <utility>

#include "brep.h"
#include "bu/avs.h"
#include "bu/log.h"
#include "rt/geom.h"
#include "rt/primitives/brep.h"

#include "iges_orientation.h"
#include "iges_report.h"
#include "iges_runtime.h"

namespace brlcad {
namespace iges {
namespace {

// Tolerances apply after translating to the shell box center and scaling
// by its longest side.  They do not depend on model units or placement.
constexpr double ABSOLUTE_ERROR = 1.0e-10;
constexpr double RELATIVE_ERROR = 1.0e-7;
constexpr double SIGN_MARGIN = 16.0;
constexpr double FLUX_TOLERANCE = 1.0e-5;
constexpr double MINIMUM_VOLUME = 1.0e-12;
constexpr int MAX_SUBDIVISION = 12;
constexpr size_t FACE_EVALUATION_LIMIT = 1000000;
constexpr size_t MAX_SPANS = 100000;
constexpr double PARAMETER_ROUNDOFF = 64.0 * std::numeric_limits<double>::epsilon();

// Volume followed by the three components of the oriented area integral.
// The latter must cancel on a geometrically closed shell.
using Moments = std::array<double, 4>;
struct Integral {
    Moments value{};
    Moments error{};

    void add(const Integral &other, double weight = 1.0)
    {
	for (size_t i = 0; i < value.size(); ++i) {
	    value[i] += weight * other.value[i];
	    error[i] += std::abs(weight) * other.error[i];
	}
    }
};

struct NumericalFailure : std::runtime_error {
    using std::runtime_error::runtime_error;
};

// Gauss 7 / Kronrod 15 abscissae and weights on [-1,1] (QUADPACK dqk15).
// This is an error estimate, not an interval-arithmetic bound.
template <typename Function>
Integral
integrate(const Function &function, double a, double b, double tolerance, int depth = 0)
{
    constexpr double nodes[] = {0.9914553711208126, 0.9491079123427585,
	0.8648644233597691, 0.7415311855993944, 0.5860872354676911,
	0.4058451513773972, 0.2077849550078985, 0.0};
    constexpr double kronrod[] = {0.02293532201052922, 0.06309209262997855,
	0.1047900103222502, 0.1406532597155259, 0.1690047266392679,
	0.1903505780647854, 0.2044329400752989, 0.2094821410847278};
    constexpr double gauss[] = {0.1294849661688697, 0.2797053914892767,
	0.3818300505051189, 0.4179591836734694};
    const double half = (b - a) * 0.5;
    const double center = a + half;
    Integral result, coarse;
    Moments magnitude{};
    const auto accumulate = [&](const Integral &sample, size_t node) {
	result.add(sample, kronrod[node]);
	if (node % 2)
	    coarse.add(sample, gauss[node / 2]);
	for (size_t i = 0; i < magnitude.size(); ++i)
	    magnitude[i] += kronrod[node] * std::abs(sample.value[i]);
    };
    accumulate(function(center), 7);
    for (size_t i = 0; i < 7; ++i) {
	accumulate(function(center - half * nodes[i]), i);
	accumulate(function(center + half * nodes[i]), i);
    }
    bool converged = true;
    for (size_t i = 0; i < result.value.size(); ++i) {
	result.error[i] = half * (result.error[i] + std::abs(result.value[i] - coarse.value[i]) +
	    SIGN_MARGIN * std::numeric_limits<double>::epsilon() * magnitude[i]);
	result.value[i] *= half;
	if (!std::isfinite(result.value[i]) || !std::isfinite(result.error[i]))
	    throw NumericalFailure("non-finite surface integral");
	converged = converged && result.error[i] <= tolerance + RELATIVE_ERROR * std::abs(result.value[i]);
    }
    if (converged)
	return result;
    if (depth >= MAX_SUBDIVISION || !(center > a && center < b))
	throw NumericalFailure("surface integration did not converge within the subdivision limit");
    result = integrate(function, a, center, tolerance * 0.5, depth + 1);
    result.add(integrate(function, center, b, tolerance * 0.5, depth + 1));
    return result;
}

std::vector<double>
curve_spans(const ON_Curve &curve)
{
    const int count = curve.SpanCount();
    if (count < 1 || static_cast<size_t>(count) > MAX_SPANS)
	throw NumericalFailure("unsupported trim spans");
    std::vector<double> knots(static_cast<size_t>(count) + 1);
    if (!curve.GetSpanVector(knots.data()))
	throw NumericalFailure("cannot read trim spans");
    return knots;
}

int
loop_direction(const ON_BrepLoop &loop, size_t &evaluations, size_t limit)
{
    ON_BoundingBox box;
    for (int t = 0; t < loop.TrimCount(); ++t)
	box.Union(loop.Trim(t)->BoundingBox());
    const double scale = std::max(box.Diagonal().x, box.Diagonal().y);
    if (!box.IsValid() || !std::isfinite(scale) || !(scale > 0.0))
	throw NumericalFailure("degenerate parameter-loop bounds");
    Integral area;
    for (int t = 0; t < loop.TrimCount(); ++t) {
	std::unique_ptr<ON_Curve> curve(loop.Trim(t)->DuplicateCurve());
	if (!curve || !curve->Translate(ON_3dPoint::Origin - box.Center()) || !curve->Scale(1.0 / scale))
	    throw NumericalFailure("cannot normalize parameter loop");
	const auto knots = curve_spans(*curve);
	for (size_t span = 1; span < knots.size(); ++span) {
	    const double a = std::max(knots[span - 1], curve->Domain().Min());
	    const double b = std::min(knots[span], curve->Domain().Max());
	    if (!(b > a))
		continue;
	    area.add(integrate([&](double parameter) {
		if (evaluations >= limit)
		    throw NumericalFailure("integration evaluation budget exhausted");
		++evaluations;
		ON_3dPoint point;
		ON_3dVector tangent;
		if (!curve->Ev1Der(parameter, point, tangent) || !point.IsValid() || !tangent.IsValid())
		    throw NumericalFailure("cannot evaluate parameter loop");
		Integral value;
		value.value[0] = (point.x * tangent.y - point.y * tangent.x) * 0.5;
		return value;
	    }, a, b, ABSOLUTE_ERROR / loop.TrimCount() / knots.size()));
	}
    }
    if (!std::isfinite(area.value[0]) || !std::isfinite(area.error[0]) ||
	std::abs(area.value[0]) <= SIGN_MARGIN * area.error[0])
	return 0;
    return area.value[0] > 0.0 ? 1 : -1;
}

struct Shell {
    std::vector<int> faces;
    std::vector<bool> flips;
    ON_BoundingBox box;
    bool closed = true;
    bool consistent = true;
    bool orientable = true;
};

std::vector<Shell>
shells(const ON_Brep &brep)
{
    std::vector<std::vector<std::pair<int, bool>>> adjacent(brep.m_F.Count());
    std::vector<bool> closed(brep.m_F.Count(), true);
    for (int e = 0; e < brep.m_E.Count(); ++e) {
	const auto &edge = brep.m_E[e];
	for (int t = 0; t < edge.m_ti.Count(); ++t) {
	    const int face = brep.m_T[edge.m_ti[t]].Face()->m_face_index;
	    if (edge.m_ti.Count() != 2)
		closed[face] = false;
	    if (t) {
		const auto &first_trim = brep.m_T[edge.m_ti[0]];
		const auto &trim = brep.m_T[edge.m_ti[t]];
		const int first = first_trim.Face()->m_face_index;
		const bool relation = !(first_trim.m_bRev3d ^ first_trim.Face()->m_bRev ^
		    trim.m_bRev3d ^ trim.Face()->m_bRev);
		adjacent[first].emplace_back(face, relation);
		adjacent[face].emplace_back(first, relation);
	    }
	}
    }
    std::vector<Shell> result;
    std::vector<int> flips(brep.m_F.Count(), -1);
    for (int seed = 0; seed < brep.m_F.Count(); ++seed) {
	if (flips[seed] >= 0)
	    continue;
	Shell shell;
	shell.faces.push_back(seed);
	flips[seed] = 0;
	for (size_t i = 0; i < shell.faces.size(); ++i) {
	    const int face = shell.faces[i];
	    shell.closed = shell.closed && closed[face];
	    shell.flips.push_back(flips[face] != 0);
	    shell.consistent = shell.consistent && !flips[face];
	    shell.box.Union(brep.m_F[face].BoundingBox());
	    for (const auto &neighbor : adjacent[face]) {
		const int required = flips[face] ^ neighbor.second;
		if (flips[neighbor.first] < 0) {
		    flips[neighbor.first] = required;
		    shell.faces.push_back(neighbor.first);
		} else if (flips[neighbor.first] != required)
		    shell.orientable = false;
	    }
	}
	result.push_back(std::move(shell));
    }
    return result;
}

bool
overlaps(const ON_BoundingBox &a, const ON_BoundingBox &b)
{
    if (!a.IsValid() || !b.IsValid())
	return true;
    for (int axis = 0; axis < 3; ++axis)
	if (a.m_max[axis] < b.m_min[axis] || b.m_max[axis] < a.m_min[axis])
	    return false;
    return true;
}

bool
strictly_contains(const ON_BoundingBox &outer, const ON_BoundingBox &inner)
{
    if (!outer.IsValid() || !inner.IsValid())
	return false;
    for (int axis = 0; axis < 3; ++axis)
	if (!(outer.m_min[axis] < inner.m_min[axis] && outer.m_max[axis] > inner.m_max[axis]))
	    return false;
    return true;
}

class FaceIntegral {
public:
    FaceIntegral(const ON_BrepFace &face, const ON_3dPoint &origin, double scale,
	size_t &evaluations, size_t limit) :
	face_(face), surface_(face.DuplicateSurface()), evaluations_(evaluations),
	limit_(limit), face_start_(evaluations)
    {
	// Normalize control geometry before evaluation, not evaluated points:
	// subtraction afterward cannot recover digits lost far from the origin.
	if (!surface_ || !surface_->Translate(ON_3dPoint::Origin - origin) || !surface_->Scale(1.0 / scale))
	    throw NumericalFailure("cannot normalize surface for integration");
	for (int axis = 0; axis < 2; ++axis)
	    if (surface_->Domain(axis) != face_.Domain(axis))
		throw NumericalFailure("normalizing the surface changed its parameter domain");
	const int spans = surface_->SpanCount(0);
	if (spans < 1 || static_cast<size_t>(spans) > MAX_SPANS)
	    throw NumericalFailure("unsupported surface spans");
	spans_.resize(static_cast<size_t>(spans) + 1);
	if (!surface_->GetSpanVector(0, spans_.data()))
	    throw NumericalFailure("cannot read surface spans");
    }

    Integral evaluate(double tolerance)
    {
	Integral result;
	size_t trim_count = 0;
	for (int l = 0; l < face_.LoopCount(); ++l)
	    trim_count += face_.Loop(l)->TrimCount();
	if (!trim_count)
	    throw NumericalFailure("face has no integration boundary");
	for (int l = 0; l < face_.LoopCount(); ++l) {
	    const auto &loop = *face_.Loop(l);
	    if (loop.m_type != ON_BrepLoop::outer && loop.m_type != ON_BrepLoop::inner)
		throw NumericalFailure("unsupported face loop type");
	    const int direction = loop_direction(loop, evaluations_, std::min(limit_, face_start_ + FACE_EVALUATION_LIMIT));
	    if (direction != (loop.m_type == ON_BrepLoop::outer ? 1 : -1))
		throw NumericalFailure("parameter loop orientation is incorrect or uncertain");
	    for (int t = 0; t < loop.TrimCount(); ++t) {
		const auto &trim = *loop.Trim(t);
		const auto knots = curve_spans(trim);
		const size_t count = knots.size() - 1;
		const double trim_tolerance = tolerance / trim_count;
		for (size_t span = 0; span < count; ++span) {
		    const double a = std::max(knots[span], trim.Domain().Min());
		    const double b = std::min(knots[span + 1], trim.Domain().Max());
		    if (!(b > a))
			continue;
		    const auto boundary = [&](double parameter) {
			consume_evaluation();
			ON_3dPoint uv;
			ON_3dVector tangent;
			if (!trim.Ev1Der(parameter, uv, tangent) || !uv.IsValid() || !tangent.IsValid())
			    throw NumericalFailure("cannot evaluate parameter trim");
			const auto u_domain = face_.Domain(0);
			for (int axis = 0; axis < 2; ++axis) {
			    const auto domain = face_.Domain(axis);
			    const double roundoff = PARAMETER_ROUNDOFF * std::max({1.0, std::abs(domain.Min()), std::abs(domain.Max())});
			    if (uv[axis] < domain.Min() - roundoff || uv[axis] > domain.Max() + roundoff)
				throw NumericalFailure("parameter trim is outside its surface domain");
			    uv[axis] = std::clamp(uv[axis], domain.Min(), domain.Max());
			}
			Integral value;
			// Horizontal parameter edges contribute zero to integral Q dv.
			if (!(std::abs(tangent.y) > 0.0))
			    return value;
			const double inner_tolerance = trim_tolerance /
			    (SIGN_MARGIN * std::max(1.0, std::abs(tangent.y) * (b - a)));
			for (size_t s = 1; s < spans_.size(); ++s) {
			    const double left = std::max(spans_[s - 1], u_domain.Min());
			    const double right = std::min({spans_[s], u_domain.Max(), uv.x});
			    if (right > left)
				value.add(integrate([&](double u) { return surface(u, uv.y); },
				    left, right, inner_tolerance / spans_.size()));
			}
			Integral weighted;
			weighted.add(value, tangent.y);
			return weighted;
		    };
		    result.add(integrate(boundary, a, b, trim_tolerance / count), face_.m_bRev ? -1.0 : 1.0);
		}
	    }
	}
	return result;
    }

private:
    void consume_evaluation()
    {
	if (evaluations_ >= limit_ || evaluations_ - face_start_ >= FACE_EVALUATION_LIMIT)
	    throw NumericalFailure("integration evaluation budget exhausted");
	++evaluations_;
    }

    Integral surface(double u, double v)
    {
	consume_evaluation();
	ON_3dPoint point;
	ON_3dVector du, dv;
	if (!surface_->Ev1Der(u, v, point, du, dv) || !point.IsValid() || !du.IsValid() || !dv.IsValid())
	    throw NumericalFailure("cannot evaluate surface derivatives");
	const ON_3dVector normal = ON_CrossProduct(du, dv);
	const ON_3dVector radius(point);
	Integral result;
	result.value = {{ON_DotProduct(radius, normal) / 3.0, normal.x, normal.y, normal.z}};
	return result;
    }

    const ON_BrepFace &face_;
    std::unique_ptr<ON_Surface> surface_;
    size_t &evaluations_;
    const size_t limit_;
    const size_t face_start_;
    std::vector<double> spans_;
};

const char *
orientation_name(ShellOrientation orientation)
{
    switch (orientation) {
	case ShellOrientation::Outward: return "outward";
	case ShellOrientation::Inward: return "inward";
	case ShellOrientation::Inconsistent: return "inconsistent";
	case ShellOrientation::Indeterminate: return "indeterminate";
    }
    return "indeterminate";
}

const char *
context_name(ShellContext context)
{
    switch (context) {
	case ShellContext::Isolated: return "isolated";
	case ShellContext::PossibleCavity: return "possible_cavity";
	case ShellContext::Overlapping: return "nested_or_overlapping";
	case ShellContext::Unresolved: return "unresolved";
    }
    return "nested_or_overlapping";
}

struct Statistics {
    size_t outward = 0, inward = 0, inconsistent = 0, indeterminate = 0;
    size_t isolated_inward = 0, possible_cavities = 0, unchecked_objects = 0;
    size_t corrected_shells = 0, corrected_faces = 0;
};

Statistics
statistics(const OrientationReport &report)
{
    Statistics result;
    for (const auto &object : report.objects) {
	result.unchecked_objects += object.shells.empty();
	for (const auto &shell : object.shells) {
	    result.outward += shell.orientation == ShellOrientation::Outward;
	    result.inward += shell.orientation == ShellOrientation::Inward;
	    result.inconsistent += shell.orientation == ShellOrientation::Inconsistent;
	    result.indeterminate += shell.orientation == ShellOrientation::Indeterminate;
	    result.isolated_inward += shell.orientation == ShellOrientation::Inward && shell.context == ShellContext::Isolated;
	    result.possible_cavities += shell.context == ShellContext::PossibleCavity;
	    if (shell.corrected) {
		++result.corrected_shells;
		result.corrected_faces += shell.faces_to_flip.size();
	    }
	}
    }
    return result;
}

} // namespace

int
parameter_loop_direction(const ON_BrepLoop &loop)
{
    size_t evaluations = 0;
    try {
	return loop_direction(loop, evaluations, FACE_EVALUATION_LIMIT);
    } catch (const NumericalFailure &) {
	return 0;
    }
}

BrepOrientationResult
check_brep_orientation(const ON_Brep &brep, size_t evaluation_limit,
    const std::function<void(size_t, size_t)> &progress)
{
    BrepOrientationResult result;
    if (!brep.IsValid() || brep.m_F.Count() < 1) {
	result.detail = "invalid or empty B-Rep; orientation not evaluated";
	return result;
    }
    const auto components = shells(brep);
    size_t evaluations = 0, completed = 0;
    for (size_t component = 0; component < components.size(); ++component) {
	const auto &shell = components[component];
	ShellOrientationResult checked;
	checked.first_face = shell.faces.front();
	checked.faces = shell.faces.size();
	if (!shell.closed)
	    checked.context = ShellContext::Unresolved;
	for (size_t other = 0; shell.closed && other < components.size(); ++other) {
	    if (other == component || !overlaps(shell.box, components[other].box))
		continue;
	    if (strictly_contains(components[other].box, shell.box))
		checked.context = ShellContext::PossibleCavity;
	    else if (checked.context == ShellContext::Isolated)
		checked.context = ShellContext::Overlapping;
	}
	const size_t start = evaluations;
	try {
	    if (!shell.closed)
		throw NumericalFailure("open or non-manifold shell");
	    if (!shell.orientable)
		throw NumericalFailure("shell has contradictory orientation constraints");
	    if (!shell.box.IsValid())
		throw NumericalFailure("invalid shell bounding box");
	    const ON_3dVector diagonal = shell.box.Diagonal();
	    const double scale = std::max({diagonal.x, diagonal.y, diagonal.z});
	    if (!std::isfinite(scale) || !(scale > 0.0))
		throw NumericalFailure("degenerate shell bounding box");
	    Integral integral;
	    for (size_t f = 0; f < shell.faces.size(); ++f) {
		checked.failed_face = shell.faces[f];
		if (progress)
		    progress(completed + f, brep.m_F.Count());
		// Divergence gives V = integral (S-origin).(Su x Sv)/3 du dv.
		// Green's theorem reduces each trimmed face to integral Q dv,
		// where Q(u,v) is the u-antiderivative of the surface integrand.
		// Inner loops and pole trims participate without a triangle mesh.
		FaceIntegral face(brep.m_F[shell.faces[f]], shell.box.Center(), scale, evaluations, evaluation_limit);
		integral.add(face.evaluate(ABSOLUTE_ERROR / shell.faces.size()), shell.flips[f] ? -1.0 : 1.0);
	    }
	    checked.failed_face = -1;
	    for (size_t i = 0; i < integral.value.size(); ++i)
		if (!std::isfinite(integral.value[i]) || !std::isfinite(integral.error[i]))
		    throw NumericalFailure("non-finite accumulated surface integral");
	    checked.normalized_volume = integral.value[0];
	    checked.normalized_error = integral.error[0];
	    const double flux = ON_3dVector(integral.value[1], integral.value[2], integral.value[3]).Length();
	    if (!std::isfinite(flux))
		throw NumericalFailure("non-finite oriented area integral");
	    checked.normalized_flux = flux;
	    const double volume = ((integral.value[0] * scale) * scale) * scale;
	    if (std::isfinite(volume) && std::abs(volume) > 0.0)
		checked.signed_volume_mm3 = volume;
	    const double flux_error = ON_3dVector(integral.error[1], integral.error[2], integral.error[3]).Length();
	    if (!std::isfinite(flux_error) ||
		checked.normalized_flux + SIGN_MARGIN * flux_error > FLUX_TOLERANCE)
		throw NumericalFailure("oriented area does not cancel; geometric closure is uncertain");
	    if (std::abs(checked.normalized_volume) <= std::max(MINIMUM_VOLUME, SIGN_MARGIN * checked.normalized_error))
		throw NumericalFailure("signed volume is too small or uncertain to classify");
	    const bool inward = checked.normalized_volume < 0.0;
	    checked.orientation = shell.consistent ?
		(inward ? ShellOrientation::Inward : ShellOrientation::Outward) : ShellOrientation::Inconsistent;
	    if (checked.context == ShellContext::Isolated) {
		for (size_t f = 0; f < shell.faces.size(); ++f)
		    if (shell.flips[f] != inward)
			checked.faces_to_flip.push_back(shell.faces[f]);
	    }
	} catch (const NumericalFailure &error) {
	    checked.detail = error.what();
	}
	checked.evaluations = evaluations - start;
	completed += shell.faces.size();
	result.shells.push_back(std::move(checked));
    }
    if (progress)
	progress(brep.m_F.Count(), brep.m_F.Count());
    return result;
}

BrepOrientationResult
repair_brep_orientation(ON_Brep &brep, const std::function<void(size_t, size_t)> &progress)
{
    auto result = check_brep_orientation(brep, ORIENTATION_EVALUATION_LIMIT, progress);
    bool changed = false;
    for (const auto &shell : result.shells) {
	for (int face : shell.faces_to_flip) {
	    brep.FlipFace(brep.m_F[face]);
	    changed = true;
	}
    }
    if (!changed)
	return result;
    if (!brep.IsValid()) {
	for (auto &shell : result.shells) {
	    for (int face : shell.faces_to_flip)
		brep.FlipFace(brep.m_F[face]);
	    if (!shell.faces_to_flip.empty())
		shell.detail = "orientation correction failed validation; original face senses restored";
	}
	return result;
    }
    for (auto &shell : result.shells) {
	if (shell.faces_to_flip.empty())
	    continue;
	shell.corrected = true;
	shell.orientation = ShellOrientation::Outward;
	shell.normalized_volume = std::abs(shell.normalized_volume);
	if (shell.signed_volume_mm3)
	    shell.signed_volume_mm3 = std::abs(*shell.signed_volume_mm3);
    }
    return result;
}

OrientationReport
check_database_orientation(struct db_i *database, ProgressReporter *progress)
{
    if (!database)
	throw std::invalid_argument("orientation check requires a database");
    std::vector<struct directory *> entries;
    struct directory *entry;
    FOR_ALL_DIRECTORY_START(entry, database) {
	if (!(entry->d_flags & (RT_DIR_COMB | RT_DIR_NON_GEOM)) &&
	    entry->d_major_type == DB5_MAJORTYPE_BRLCAD && entry->d_minor_type == ID_BREP)
	    entries.push_back(entry);
    } FOR_ALL_DIRECTORY_END;
    std::sort(entries.begin(), entries.end(), [](const auto *a, const auto *b) {
	return std::string(a->d_namep) < b->d_namep;
    });
    OrientationReport report;
    for (size_t index = 0; index < entries.size(); ++index) {
	entry = entries[index];
	const auto update = [&](size_t face, size_t total) {
	    if (progress) {
		const std::string activity = std::string(entry->d_namep) + " (face " +
		    std::to_string(face) + "/" + std::to_string(total) + ")";
		progress->update("orientation", activity.c_str(), index, entries.size());
	    }
	};
	update(0, 0);
	BrepOrientationResult result;
	Internal internal;
	if (rt_db_get_internal(&internal.value, entry, database, nullptr) < 0 ||
	    internal.value.idb_type != ID_BREP || !internal.value.idb_ptr) {
	    result.detail = "cannot read B-Rep";
	    ++report.read_failures;
	} else {
	    const auto *geometry = static_cast<const struct rt_brep_internal *>(internal.value.idb_ptr);
	    const char *status = bu_avs_get(&internal.value.idb_avs, "iges.import_status");
	    const char *invalid = bu_avs_get(&internal.value.idb_avs, RT_BREP_INVALID_SOLID_ATTRIBUTE);
	    if ((status && std::string(status) == "invalid_solid") || (invalid && std::string(invalid) == "1"))
		result.detail = "preserved incomplete IGES solid; orientation not evaluated";
	    else if (geometry->brep)
		result = check_brep_orientation(*geometry->brep, ORIENTATION_EVALUATION_LIMIT, update);
	    else
		result.detail = "empty B-Rep";
	}
	result.object = entry->d_namep;
	report.objects.push_back(std::move(result));
    }
    if (progress)
	progress->update("orientation", "check complete", entries.size(), entries.size());
    return report;
}

void
log_orientation_report(const OrientationReport &report)
{
    const auto counts = statistics(report);
    struct Issue {
	size_t count = 0;
	std::string object;
	int face = -1;
    };
    std::map<std::string, Issue> issues;
    const auto append = [&](const std::string &message, const std::string &object, int face) {
	auto &issue = issues[message];
	if (!issue.count++) {
	    issue.object = object;
	    issue.face = face;
	}
    };
    for (const auto &object : report.objects) {
	if (!object.detail.empty())
	    append(object.detail, object.object, -1);
	for (const auto &shell : object.shells) {
	    if (!shell.corrected && shell.orientation == ShellOrientation::Outward && shell.context == ShellContext::Isolated)
		continue;
	    append(std::string(orientation_name(shell.orientation)) + ", " + context_name(shell.context) +
		(shell.corrected ? "; corrected" : "") + (shell.detail.empty() ? "" : "; " + shell.detail),
		object.object, shell.failed_face >= 0 ? shell.failed_face : shell.first_face);
	}
    }
    for (const auto &item : issues)
	bu_log("IGES orientation %s: %zu occurrences, first %s face %d\n", item.first.c_str(),
	    item.second.count, item.second.object.c_str(), item.second.face);
    bu_log("IGES orientation: %zu B-Reps; shells: %zu outward, %zu inward (%zu isolated), "
	"%zu inconsistent, %zu indeterminate; %zu possible cavities; %zu unchecked objects; %zu read failures; "
	"corrected %zu shells (%zu faces)\n",
	report.objects.size(), counts.outward, counts.inward, counts.isolated_inward,
	counts.inconsistent, counts.indeterminate, counts.possible_cavities, counts.unchecked_objects, report.read_failures,
	counts.corrected_shells, counts.corrected_faces);
}

void
write_orientation_json(std::ostream &output, const OrientationReport &report)
{
    const auto counts = statistics(report);
    output.imbue(std::locale::classic());
    output << std::setprecision(std::numeric_limits<double>::max_digits10)
	<< "{\"geometry_modified\":" << (counts.corrected_faces ? "true" : "false")
	<< ",\"scope\":\"brep_object_coordinates\","
	<< "\"statistics\":{\"breps\":" << report.objects.size()
	<< ",\"outward\":" << counts.outward << ",\"inward\":" << counts.inward
	<< ",\"inconsistent\":" << counts.inconsistent
	<< ",\"isolated_inward\":" << counts.isolated_inward << ",\"indeterminate\":" << counts.indeterminate
	<< ",\"possible_cavities\":" << counts.possible_cavities << ",\"unchecked_objects\":" << counts.unchecked_objects
	<< ",\"read_failures\":" << report.read_failures << ",\"corrected_shells\":" << counts.corrected_shells
	<< ",\"corrected_faces\":" << counts.corrected_faces << "},\"objects\":[";
    bool first_object = true;
    for (const auto &object : report.objects) {
	output << (first_object ? "\n" : ",\n") << "{\"object\":\"" << json_escape(object.object)
	    << "\",\"detail\":\"" << json_escape(object.detail) << "\",\"shells\":[";
	first_object = false;
	bool first_shell = true;
	for (const auto &shell : object.shells) {
	    output << (first_shell ? "" : ",") << "{\"first_face\":" << shell.first_face << ",\"failed_face\":" << shell.failed_face
		<< ",\"faces\":" << shell.faces
		<< ",\"orientation\":\"" << orientation_name(shell.orientation) << "\",\"context\":\"" << context_name(shell.context)
		<< "\",\"detail\":\"" << json_escape(shell.detail) << "\",\"corrected\":" << (shell.corrected ? "true" : "false")
		<< ",\"repairable_faces\":" << shell.faces_to_flip.size() << ",\"signed_volume_mm3\":";
	    if (shell.signed_volume_mm3)
		output << *shell.signed_volume_mm3;
	    else
		output << "null";
	    output << ",\"normalized_volume\":" << shell.normalized_volume << ",\"normalized_error\":" << shell.normalized_error
		<< ",\"normalized_flux\":" << shell.normalized_flux << ",\"evaluations\":" << shell.evaluations
		<< ",\"face_reversals\":[";
	    for (size_t i = 0; i < shell.faces_to_flip.size(); ++i)
		output << (i ? "," : "") << shell.faces_to_flip[i];
	    output << "]}";
	    first_shell = false;
	}
	output << "]}";
    }
    output << "]}";
}

bool
write_orientation_report(const std::string &path, const OrientationReport &report)
{
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output)
	return false;
    write_orientation_json(output, report);
    output << '\n';
    output.close();
    return !output.fail();
}

} // namespace iges
} // namespace brlcad

/*
 * Local Variables:
 * mode: C++
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
