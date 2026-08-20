/*                         S T E P U N I T . H
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 */

#ifndef CONV_STEP_STEPUNIT_H
#define CONV_STEP_STEPUNIT_H

#include "common.h"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace brlcad {
namespace step {

/** Schema-neutral retention of a STEP unit graph.
 *
 * kind is one of "si", "context", "conversion", "derived", or "named".
 * A conversion unit has exactly one component: the conversion factor's unit.
 * A derived unit has one component per exponent.  Keeping this structure in
 * addition to the convenient normalized property-unit label avoids losing
 * external conversion factors and conversion-based units nested in derived
 * units.
 */
struct UnitStructure {
    int64_t entity_id = 0;
    std::string kind;
    std::string subtype;
    std::string name;
    std::string prefix;
    bool has_dimensions = false;
    std::array<double, 7> dimension_exponents = {{0.0, 0.0, 0.0, 0.0,
	0.0, 0.0, 0.0}};
    bool has_conversion_value = false;
    double conversion_value = 0.0;
    std::string conversion_value_type;
    std::vector<UnitStructure> components;
    std::vector<double> exponents;

    bool empty() const { return kind.empty(); }
};

/** Return the dimensional exponents required by the EXPRESS valid_units
 * rule for a supported constrained measure type.  False identifies generic
 * measure types whose dimensions are deliberately supplied by their unit. */
inline bool
ConstrainedMeasureDimensions(const std::string &value_type,
    std::array<double, 7> &dimensions)
{
    dimensions = {{0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0}};
    if (value_type == "length_measure" ||
	value_type == "non_negative_length_measure" ||
	value_type == "positive_length_measure") dimensions[0] = 1.0;
    else if (value_type == "area_measure") dimensions[0] = 2.0;
    else if (value_type == "volume_measure") dimensions[0] = 3.0;
    else if (value_type == "mass_measure") dimensions[1] = 1.0;
    else if (value_type == "time_measure") dimensions[2] = 1.0;
    else if (value_type == "electric_current_measure") dimensions[3] = 1.0;
    else if (value_type == "thermodynamic_temperature_measure" ||
	value_type == "celsius_temperature_measure") dimensions[4] = 1.0;
    else if (value_type == "amount_of_substance_measure") dimensions[5] = 1.0;
    else if (value_type == "luminous_intensity_measure") dimensions[6] = 1.0;
    else if (value_type != "plane_angle_measure" &&
	value_type != "positive_plane_angle_measure" &&
	value_type != "solid_angle_measure" &&
	value_type != "ratio_measure" &&
	value_type != "positive_ratio_measure") return false;
    return true;
}

inline bool
MeasureDimensionsEqual(const std::array<double, 7> &left,
    const std::array<double, 7> &right, double tolerance = 1.0e-12)
{
    for (size_t i = 0; i < left.size(); ++i) {
	if (left[i] < right[i] - tolerance || left[i] > right[i] + tolerance)
	    return false;
    }
    return true;
}

} // namespace step
} // namespace brlcad

#endif /* CONV_STEP_STEPUNIT_H */
