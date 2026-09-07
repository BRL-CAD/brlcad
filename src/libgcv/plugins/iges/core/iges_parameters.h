/* BRL-CAD
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#ifndef GCV_IGES_PARAMETERS_H
#define GCV_IGES_PARAMETERS_H

#include "common.h"
#include "iges_document.h"
#include <limits>

namespace brlcad {
namespace iges {

constexpr size_t GLOBAL_MODEL_SCALE = 12;
constexpr size_t GLOBAL_UNITS_FLAG = 13;
constexpr double DEFAULT_MODEL_SCALE = 1.0;
constexpr double DEFAULT_UNIT_TO_MM = 1.0;

inline bool
parameter_real(const ParameterList *parameters, size_t index, double &value)
{
    return parameters && index < parameters->values.size() && parameters->values[index].real(value);
}

inline bool
parameter_integer(const ParameterList *parameters, size_t index, int &value)
{
    int64_t parsed = 0;
    if (!parameters || index >= parameters->values.size() ||
	!parameters->values[index].integer(parsed) || parsed < std::numeric_limits<int>::min() ||
	parsed > std::numeric_limits<int>::max())
	return false;
    value = static_cast<int>(parsed);
    return true;
}

inline bool
parameter_entity(const ParameterList *parameters, size_t index, EntityId &value)
{
    return parameters && index < parameters->values.size() &&
	parameters->values[index].entity(value) && value.valid();
}

inline bool
parameter_string(const ParameterList *parameters, size_t index, std::string &value)
{
    return parameters && index < parameters->values.size() && parameters->values[index].string(value);
}

inline double
global_real(const GlobalSection &global, size_t index, double fallback)
{
    double value = 0.0;
    return index < global.parameters.size() && Parameter{global.parameters[index], {}}.real(value) ? value : fallback;
}

inline int
global_integer(const GlobalSection &global, size_t index, int fallback)
{
    int64_t value = 0;
    if (index >= global.parameters.size() || !Parameter{global.parameters[index], {}}.integer(value) ||
	value < std::numeric_limits<int>::min() || value > std::numeric_limits<int>::max())
	return fallback;
    return static_cast<int>(value);
}

inline double
unit_scale(const GlobalSection &global)
{
    // IGES 5.3 Global field 14 unit flags, indexed by the published value.
    constexpr double TO_MM[] = {1.0, 25.4, 1.0, 1.0, 304.8, 1609344.0, 1000.0,
	1000000.0, 0.0254, 0.001, 10.0, 0.0000254};
    const double model_scale = global_real(global, GLOBAL_MODEL_SCALE, DEFAULT_MODEL_SCALE);
    const int units = global_integer(global, GLOBAL_UNITS_FLAG, 2);
    const double conversion = units > 0 && static_cast<size_t>(units) < sizeof(TO_MM) / sizeof(TO_MM[0]) ?
	TO_MM[units] : DEFAULT_UNIT_TO_MM;
    return conversion / (model_scale > 0.0 ? model_scale : DEFAULT_MODEL_SCALE);
}

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
