/*   D E F A U L T _ G E O M E T R I C _ C O N T E X T . C P P
 *
 * BRL-CAD
 *
 * Copyright (c) 2013-2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file Default_Geometric_Context.cpp
 *
 */

#include "AP_Common.h"
#include "Default_Geometric_Context.h"
#include "STEPGeneratedAPI.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <string>

namespace {

const char *
si_length_prefix(double millimetres)
{
    struct Prefix {
	double millimetres;
	const char *name;
    };
    static const Prefix prefixes[] = {
	{1.0e-15, "ATTO"}, {1.0e-12, "FEMTO"}, {1.0e-9, "PICO"},
	{1.0e-6, "NANO"}, {1.0e-3, "MICRO"}, {1.0, "MILLI"},
	{10.0, "CENTI"}, {100.0, "DECI"}, {1000.0, NULL},
	{1.0e4, "DECA"}, {1.0e5, "HECTO"}, {1.0e6, "KILO"},
	{1.0e9, "MEGA"}, {1.0e12, "GIGA"}, {1.0e15, "TERA"},
	{1.0e18, "PETA"}, {1.0e21, "EXA"}
    };
    for (const Prefix &prefix : prefixes) {
	const double scale = std::max(std::fabs(millimetres),
	    std::fabs(prefix.millimetres));
	if (std::fabs(millimetres - prefix.millimetres) <= scale * 1.0e-12)
	    return prefix.name ? prefix.name : "";
    }
    return NULL;
}

STEPcomplex *
si_length_unit(AP203_Contents *sc, const char *prefix, int &instance_count)
{
    const char *types[4] = {"length_unit", "named_unit", "si_unit", "*"};
    STEPcomplex *unit = new STEPcomplex(sc->registry, types, instance_count);
    sc->instance_list->Append(static_cast<STEPentity *>(unit), completeSE);
    ++instance_count;
    STEPcomplex *component = unit->head;
    while (component) {
	if (!bu_strcmp(component->EntityName(), "Si_Unit")) {
	    if (prefix && prefix[0])
		brlcad::step::SetEnum(component, "prefix", prefix);
	    brlcad::step::SetEnum(component, "name", "METRE");
	}
	component = component->sc;
    }
    return unit;
}

STEPentity *
length_dimensions(AP203_Contents *sc)
{
    STEPentity *dimensions = brlcad::step::CreateEntity(sc->registry,
	sc->instance_list, "DIMENSIONAL_EXPONENTS");
    brlcad::step::SetReal(dimensions, "length_exponent", 1.0);
    brlcad::step::SetReal(dimensions, "mass_exponent", 0.0);
    brlcad::step::SetReal(dimensions, "time_exponent", 0.0);
    brlcad::step::SetReal(dimensions, "electric_current_exponent", 0.0);
    brlcad::step::SetReal(dimensions, "thermodynamic_temperature_exponent", 0.0);
    brlcad::step::SetReal(dimensions, "amount_of_substance_exponent", 0.0);
    brlcad::step::SetReal(dimensions, "luminous_intensity_exponent", 0.0);
    return dimensions;
}

STEPentity *
conversion_length_unit(AP203_Contents *sc, int &instance_count)
{
    STEPcomplex *metre = si_length_unit(sc, "", instance_count);
    STEPentity *factor = brlcad::step::CreateEntity(sc->registry,
	sc->instance_list, "LENGTH_MEASURE_WITH_UNIT");
    brlcad::step::SetSelectReal(factor, "value_component", "LENGTH_MEASURE",
	sc->length_unit_mm / 1000.0);
    brlcad::step::SetEntity(factor, "unit_component", metre);
    ++instance_count;

    STEPentity *dimensions = length_dimensions(sc);
    ++instance_count;
    const char *types[4] = {
	"conversion_based_unit", "length_unit", "named_unit", "*"
    };
    STEPcomplex *unit = new STEPcomplex(sc->registry, types, instance_count);
    std::string name = sc->length_unit;
    std::transform(name.begin(), name.end(), name.begin(),
	[](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    for (STEPcomplex *component = unit->head; component;
	    component = component->sc) {
	if (!bu_strcmp(component->EntityName(), "Conversion_Based_Unit")) {
	    brlcad::step::SetString(component, "name", name.c_str());
	    brlcad::step::SetEntity(component, "conversion_factor", factor);
	}
	if (!bu_strcmp(component->EntityName(), "Named_Unit"))
	    brlcad::step::SetEntity(component, "dimensions", dimensions);
    }
    sc->instance_list->Append(static_cast<STEPentity *>(unit), completeSE);
    ++instance_count;
    return unit;
}

} // namespace

STEPcomplex *
Add_Default_Geometric_Context(AP203_Contents *sc)
{

    int instance_cnt = 0;
    STEPcomplex *stepcomplex;

    /* Uncertainty measure with unit */
    STEPentity *uncertainty = brlcad::step::CreateEntity(sc->registry,
	    sc->instance_list, "UNCERTAINTY_MEASURE_WITH_UNIT");
    brlcad::step::SetString(uncertainty, "name", "DISTANCE_ACCURACY_VALUE");
    brlcad::step::SetString(uncertainty, "description",
	    "Threshold below which geometry imperfections (such as overlaps) are not considered errors.");
    brlcad::step::SetSelectReal(uncertainty, "value_component",
	    "LENGTH_MEASURE", sc->uncertainty);
    instance_cnt++;

    /** unit component of uncertainty measure with unit */
    const char *length_prefix = si_length_prefix(sc->length_unit_mm);
    STEPentity *unit_complex = length_prefix ?
	static_cast<STEPentity *>(si_length_unit(sc, length_prefix, instance_cnt)) :
	conversion_length_unit(sc, instance_cnt);
    brlcad::step::SetEntity(uncertainty, "unit_component", unit_complex);

    /* Global Unit Assigned Context */
    const char *ua_entry_1_types[4] = {"named_unit", "si_unit", "solid_angle_unit", "*"};
    STEPcomplex *ua_entry_1 = new STEPcomplex(sc->registry, (const char **)ua_entry_1_types, instance_cnt);
    stepcomplex = ua_entry_1->head;
    while (stepcomplex) {
	if (!bu_strcmp(stepcomplex->EntityName(), "Si_Unit")) {
	    brlcad::step::SetEnum(stepcomplex, "name", "STERADIAN");
	}
	stepcomplex = stepcomplex->sc;
    }
    sc->instance_list->Append((STEPentity *)ua_entry_1, completeSE);
    instance_cnt++;

    const char *ua_entry_3_types[4] = {"named_unit", "plane_angle_unit", "si_unit", "*"};
    STEPcomplex *ua_entry_3 = new STEPcomplex(sc->registry, (const char **)ua_entry_3_types, instance_cnt);
    stepcomplex = ua_entry_3->head;
    while (stepcomplex) {
	if (!bu_strcmp(stepcomplex->EntityName(), "Si_Unit")) {
	    brlcad::step::SetEnum(stepcomplex, "name", "RADIAN");
	}
	stepcomplex = stepcomplex->sc;
    }
    sc->instance_list->Append((STEPentity *)ua_entry_3, completeSE);
    instance_cnt++;

    STEPentity *plane_angle_unit = ua_entry_3;
    if (sc->plane_angle_unit == "degree") {
	STEPentity *p_ang_measure_with_unit = brlcad::step::CreateEntity(
		sc->registry, sc->instance_list, "PLANE_ANGLE_MEASURE_WITH_UNIT");
	brlcad::step::SetSelectReal(p_ang_measure_with_unit, "value_component",
		"PLANE_ANGLE_MEASURE", 0.017453292519943295);
	brlcad::step::SetEntity(p_ang_measure_with_unit, "unit_component", ua_entry_3);
	instance_cnt++;

	const char *types[4] = {
	    "conversion_based_unit", "named_unit", "plane_angle_unit", "*"
	};
	STEPcomplex *degrees = new STEPcomplex(sc->registry, types, instance_cnt);
	STEPentity *dimensional_exp = brlcad::step::CreateEntity(sc->registry,
	    sc->instance_list, "DIMENSIONAL_EXPONENTS");
	brlcad::step::SetReal(dimensional_exp, "length_exponent", 0.0);
	brlcad::step::SetReal(dimensional_exp, "mass_exponent", 0.0);
	brlcad::step::SetReal(dimensional_exp, "time_exponent", 0.0);
	brlcad::step::SetReal(dimensional_exp, "electric_current_exponent", 0.0);
	brlcad::step::SetReal(dimensional_exp, "thermodynamic_temperature_exponent", 0.0);
	brlcad::step::SetReal(dimensional_exp, "amount_of_substance_exponent", 0.0);
	brlcad::step::SetReal(dimensional_exp, "luminous_intensity_exponent", 0.0);
	for (stepcomplex = degrees->head; stepcomplex; stepcomplex = stepcomplex->sc) {
	    if (!bu_strcmp(stepcomplex->EntityName(), "Conversion_Based_Unit")) {
		brlcad::step::SetString(stepcomplex, "name", "DEGREES");
		brlcad::step::SetEntity(stepcomplex, "conversion_factor",
		    p_ang_measure_with_unit);
	    }
	    if (!bu_strcmp(stepcomplex->EntityName(), "Named_Unit"))
		brlcad::step::SetEntity(stepcomplex, "dimensions", dimensional_exp);
	}
	sc->instance_list->Append(static_cast<STEPentity *>(degrees), completeSE);
	instance_cnt++;
	plane_angle_unit = degrees;
    }

    /*
     * Now that we have the pieces, build the final complex type from four other types:
     */
    const char *entNmArr[5] = {"geometric_representation_context", "global_uncertainty_assigned_context",
			       "global_unit_assigned_context", "representation_context", "*"};
    STEPcomplex *complex_entity = new STEPcomplex(sc->registry, (const char **)entNmArr, instance_cnt);
    stepcomplex = complex_entity->head;

    while (stepcomplex) {

	if (!bu_strcmp(stepcomplex->EntityName(), "Geometric_Representation_Context")) {
	    brlcad::step::SetInteger(stepcomplex, "coordinate_space_dimension", 3);
	}

	if (!bu_strcmp(stepcomplex->EntityName(), "Global_Uncertainty_Assigned_Context")) {
	    brlcad::step::AddEntity(stepcomplex, "uncertainty", uncertainty);

	}

	if (!bu_strcmp(stepcomplex->EntityName(), "Global_Unit_Assigned_Context")) {
	    brlcad::step::AddEntity(stepcomplex, "units", unit_complex);
	    brlcad::step::AddEntity(stepcomplex, "units", plane_angle_unit);
	    brlcad::step::AddEntity(stepcomplex, "units", ua_entry_1);
	}

	if (!bu_strcmp(stepcomplex->EntityName(), "Representation_Context")) {
	    brlcad::step::SetString(stepcomplex, "context_identifier", "STANDARD");
	    brlcad::step::SetString(stepcomplex, "context_type", "3D");
	}
	stepcomplex = stepcomplex->sc;
    }

    sc->instance_list->Append((STEPentity *)complex_entity, completeSE);

    return complex_entity;
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
