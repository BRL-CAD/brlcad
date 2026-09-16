/*                 MeasureValue.cpp
 * BRL-CAD
 *
 * Copyright (c) 1994-2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file step/MeasureValue.cpp
 *
 * Routines to convert STEP "MeasureValue" to BRL-CAD BREP
 * structures.
 *
 */

#include "STEPWrapper.h"
#include "STEPGeneratedAPI.h"
#include "ap_schema.h"
#include "Factory.h"

#include "MeasureValue.h"

#define CLASSNAME "MeasureValue"
#define ENTITYNAME "Measure_Value"
string MeasureValue::entityname = Factory::RegisterClass(ENTITYNAME, (FactoryMethod)MeasureValue::Create);

const char *measure_type_names[] = {
    "AMOUNT_OF_SUBSTANCE_MEASURE",
    "AREA_MEASURE",
    "CELSIUS_TEMPERATURE_MEASURE",
    "CONTEXT_DEPENDENT_MEASURE",
    "COUNT_MEASURE",
    "DESCRIPTIVE_MEASURE",
    "ELECTRIC_CURRENT_MEASURE",
    "LENGTH_MEASURE",
    "LUMINOUS_INTENSITY_MEASURE",
    "MASS_MEASURE",
    "NUMERIC_MEASURE",
    "PARAMETER_VALUE",
    "PLANE_ANGLE_MEASURE",
    "POSITIVE_LENGTH_MEASURE",
    "POSITIVE_PLANE_ANGLE_MEASURE",
    "POSITIVE_RATIO_MEASURE",
    "RATIO_MEASURE",
    "SOLID_ANGLE_MEASURE",
    "THERMODYNAMIC_TEMPERATURE_MEASURE",
    "TIME_MEASURE",
    "VOLUME_MEASURE",
    "MEASURE_VALUE_UNKNOWN",
    NULL
};

MeasureValue::MeasureValue()
{
    step = NULL;
    id = 0;
    type = MeasureValue::MEASURE_VALUE_UNKNOWN;
    ivalue = 0;
    rvalue = 0.0;
}

MeasureValue::MeasureValue(STEPWrapper *sw, int step_id)
{
    step = sw;
    id = step_id;
    type = MeasureValue::MEASURE_VALUE_UNKNOWN;
    ivalue = 0;
    rvalue = 0.0;
}

MeasureValue::~MeasureValue()
{
}

double
MeasureValue::GetLengthMeasure()
{
    if (type != LENGTH_MEASURE) {
	std::cerr << CLASSNAME << ":Error: Not a length measure." << std::endl;
	return 1.0;
    }
    return rvalue;
}

double
MeasureValue::GetPlaneAngleMeasure()
{
    if (type != PLANE_ANGLE_MEASURE) {
	std::cerr << CLASSNAME << ":Error: Not a plane angle measure." << std::endl;
	return 1.0;
    }
    return rvalue;
}

double
MeasureValue::GetSolidAngleMeasure()
{
    if (type != SOLID_ANGLE_MEASURE) {
	std::cerr << CLASSNAME << ":Error: Not a solid angle measure." << std::endl;
	return 1.0;
    }
    return rvalue;
}


bool
MeasureValue::Load(STEPWrapper *sw, SDAI_Application_instance *sse)
{
    step = sw;
    id = sse->STEPfile_id;

    SDAI_Select *select = step->getSelectAttribute(sse, "value_component");
    return Load(sw, select);
}


bool
MeasureValue::Load(STEPWrapper *sw, SDAI_Select *sse)
{
    step = sw;

    const SDAI_Select *v = sse;
    const SDAI_Real *real_value = brlcad::step::SelectedReal(v);
    const SDAI_Integer *integer_value = brlcad::step::SelectedInteger(v);

    if (brlcad::step::SelectIs(v, "LENGTH_MEASURE")) {
	type = LENGTH_MEASURE;
	rvalue = real_value ? *real_value : 0.0;
    } else if (brlcad::step::SelectIs(v, "MASS_MEASURE")) {
	type = MASS_MEASURE;
	rvalue = real_value ? *real_value : 0.0;
    } else if (brlcad::step::SelectIs(v, "PLANE_ANGLE_MEASURE")) {
	type = PLANE_ANGLE_MEASURE;
	rvalue = real_value ? *real_value : 0.0;
    } else if (brlcad::step::SelectIs(v, "SOLID_ANGLE_MEASURE")) {
	type = SOLID_ANGLE_MEASURE;
	rvalue = real_value ? *real_value : 0.0;
    } else if (brlcad::step::SelectIs(v, "AREA_MEASURE")) {
	type = AREA_MEASURE;
	rvalue = real_value ? *real_value : 0.0;
    } else if (brlcad::step::SelectIs(v, "VOLUME_MEASURE")) {
	type = VOLUME_MEASURE;
	rvalue = real_value ? *real_value : 0.0;
    } else if (brlcad::step::SelectIs(v, "PARAMETER_VALUE")) {
	type = PARAMETER_VALUE;
	rvalue = real_value ? *real_value : 0.0;
    } else if (brlcad::step::SelectIs(v, "CONTEXT_DEPENDENT_MEASURE")) {
	type = CONTEXT_DEPENDENT_MEASURE;
	rvalue = real_value ? *real_value : 0.0;
    } else if (brlcad::step::SelectIs(v, "DESCRIPTIVE_MEASURE")) {
	const SDAI_String *string_value = brlcad::step::SelectedString(v);
	type = DESCRIPTIVE_MEASURE;
	svalue = string_value ? string_value->c_str() : "";
    } else if (brlcad::step::SelectIs(v, "POSITIVE_LENGTH_MEASURE")) {
	type = POSITIVE_LENGTH_MEASURE;
	rvalue = real_value ? *real_value : 0.0;
    } else if (brlcad::step::SelectIs(v, "POSITIVE_PLANE_ANGLE_MEASURE")) {
	type = PLANE_ANGLE_MEASURE;
	rvalue = real_value ? *real_value : 0.0;
    } else if (brlcad::step::SelectIs(v, "COUNT_MEASURE")) {
	type = COUNT_MEASURE;
	ivalue = integer_value ? static_cast<int>(*integer_value) :
	    (real_value ? static_cast<int>(*real_value) : 0);
#if defined(AP203e2) || defined(AP242)
    } else if (brlcad::step::SelectIs(v, "AMOUNT_OF_SUBSTANCE_MEASURE")) {
	type = AMOUNT_OF_SUBSTANCE_MEASURE;
	rvalue = real_value ? *real_value : 0.0;
    } else if (brlcad::step::SelectIs(v, "CELSIUS_TEMPERATURE_MEASURE")) {
	type = CELSIUS_TEMPERATURE_MEASURE;
	rvalue = real_value ? *real_value : 0.0;
    } else if (brlcad::step::SelectIs(v, "ELECTRIC_CURRENT_MEASURE")) {
	type = ELECTRIC_CURRENT_MEASURE;
	rvalue = real_value ? *real_value : 0.0;
    } else if (brlcad::step::SelectIs(v, "LUMINOUS_INTENSITY_MEASURE")) {
	type = LUMINOUS_INTENSITY_MEASURE;
	rvalue = real_value ? *real_value : 0.0;
    } else if (brlcad::step::SelectIs(v, "NUMERIC_MEASURE")) {
	type = NUMERIC_MEASURE;
	ivalue = integer_value ? static_cast<int>(*integer_value) :
	    (real_value ? static_cast<int>(*real_value) : 0);
    } else if (brlcad::step::SelectIs(v, "POSITIVE_RATIO_MEASURE")) {
	type = POSITIVE_RATIO_MEASURE;
	rvalue = real_value ? *real_value : 0.0;
    } else if (brlcad::step::SelectIs(v, "RATIO_MEASURE")) {
	type = RATIO_MEASURE;
	rvalue = real_value ? *real_value : 0.0;
    } else if (brlcad::step::SelectIs(v, "THERMODYNAMIC_TEMPERATURE_MEASURE")) {
	type = THERMODYNAMIC_TEMPERATURE_MEASURE;
	rvalue = real_value ? *real_value : 0.0;
    } else if (brlcad::step::SelectIs(v, "TIME_MEASURE")) {
	type = TIME_MEASURE;
	rvalue = real_value ? *real_value : 0.0;
#endif
    }

    return true;
}

void
MeasureValue::Print(int level)
{
    TAB(level);
    std::cout << CLASSNAME << ":" << "(";
    std::cout << "ID:" << STEPid() << ")" << std::endl;

    TAB(level);
    std::cout << "Attributes:" << std::endl;
    if (type == DESCRIPTIVE_MEASURE) {
	TAB(level + 1);
	std::cout << "Type:" << measure_type_names[type] << " Value:" << svalue << std::endl;
    } else if ((type == COUNT_MEASURE) || (type == NUMERIC_MEASURE)) {
	TAB(level + 1);
	std::cout << "Type:" << measure_type_names[type] << " Value:" << ivalue << std::endl;
    } else {
	TAB(level + 1);
	std::cout << "Type:" << measure_type_names[type] << " Value:" << rvalue << std::endl;
    }
}

STEPEntity *
MeasureValue::GetInstance(STEPWrapper *sw, int id)
{
    return new MeasureValue(sw, id);
}

STEPEntity *
MeasureValue::Create(STEPWrapper *sw, SDAI_Application_instance *sse)
{
    return STEPEntity::CreateEntity(sw, sse, GetInstance, CLASSNAME);
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
