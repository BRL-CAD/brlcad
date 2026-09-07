/* BRL-CAD
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#ifndef GCV_IGES_ORIENTATION_H
#define GCV_IGES_ORIENTATION_H

#include "common.h"

#include <cstddef>
#include <functional>
#include <iosfwd>
#include <optional>
#include <string>
#include <vector>

class ON_Brep;
class ON_BrepLoop;
struct db_i;

namespace brlcad {
namespace iges {

class ProgressReporter;

enum class ShellOrientation { Outward, Inward, Inconsistent, Indeterminate };
enum class ShellContext { Isolated, PossibleCavity, Overlapping, Unresolved };

struct ShellOrientationResult {
    int first_face = -1;
    int failed_face = -1;
    size_t faces = 0;
    ShellOrientation orientation = ShellOrientation::Indeterminate;
    ShellContext context = ShellContext::Isolated;
    std::optional<double> signed_volume_mm3;
    double normalized_volume = 0.0;
    double normalized_error = 0.0;
    double normalized_flux = 0.0;
    size_t evaluations = 0;
    std::vector<int> faces_to_flip;
    bool corrected = false;
    std::string detail;
};

struct BrepOrientationResult {
    std::string object;
    std::string detail;
    std::vector<ShellOrientationResult> shells;
};

struct OrientationReport {
    std::vector<BrepOrientationResult> objects;
    size_t read_failures = 0;
};

// Bound numerical work per B-Rep, independently of the database size.
constexpr size_t ORIENTATION_EVALUATION_LIMIT = 20000000;

/** Bounded parameter-space area test: +1 counterclockwise, -1 clockwise,
 * zero when direction cannot be established.  Does not modify the loop. */
int parameter_loop_direction(const ON_BrepLoop &loop);

/** Diagnostic estimates, not a solid-validity certificate.  No geometry,
 * face sense, provenance, or cached solid orientation is changed. */
BrepOrientationResult check_brep_orientation(const ON_Brep &brep,
    size_t evaluation_limit = ORIENTATION_EVALUATION_LIMIT,
    const std::function<void(size_t, size_t)> &progress = {});

/** Apply only the isolated closed-shell corrections established by the
 * check above.  Open, non-orientable, nested, and uncertain shells stay as-is. */
BrepOrientationResult repair_brep_orientation(ON_Brep &brep,
    const std::function<void(size_t, size_t)> &progress = {});

/** Visit stored B-Rep primitives once, in object coordinates.  No Boolean
 * evaluation, instance expansion, tessellation, or ray preparation. */
OrientationReport check_database_orientation(struct db_i *database,
    ProgressReporter *progress = nullptr);
void log_orientation_report(const OrientationReport &report);
void write_orientation_json(std::ostream &output, const OrientationReport &report);
bool write_orientation_report(const std::string &path, const OrientationReport &report);

} // namespace iges
} // namespace brlcad
#endif

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
