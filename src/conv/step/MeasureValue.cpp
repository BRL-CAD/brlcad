/*                 MeasureValue.cpp
 * BRL-CAD
 *
 * Copyright (c) 1994-2011 United States Government as represented by
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
#include "Factory.h"

#include "MeasureValue.h"

#define CLASSNAME "MeasureValue"
#define ENTITYNAME "Measure_Value"
string MeasureValue::entityname = Factory::RegisterClass(ENTITYNAME,(FactoryMethod)MeasureValue::Create);

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
	NULL
	};

MeasureValue::MeasureValue() {
	step = NULL;
	id = 0;
}

MeasureValue::MeasureValue(STEPWrapper *sw,int step_id) {
	step = sw;
	id = step_id;
}

MeasureValue::~MeasureValue() {
}

double
MeasureValue::GetLengthMeasure() {
	if (type != LENGTH_MEASURE) {
		std::cerr << CLASSNAME << ":Error: Not a length measure." << std::endl;
		return 1.0;
	}
	return rvalue;
}

double
MeasureValue::GetPlaneAngleMeasure() {
	if (type != PLANE_ANGLE_MEASURE) {
		std::cerr << CLASSNAME << ":Error: Not a plane angle measure." << std::endl;
		return 1.0;
	}
	return rvalue;
}

double
MeasureValue::GetSolidAngleMeasure() {
	if (type != SOLID_ANGLE_MEASURE) {
		std::cerr << CLASSNAME << ":Error: Not a solid angle measure." << std::endl;
		return 1.0;
	}
	return rvalue;
}

bool
MeasureValue::Load(STEPWrapper *sw,SCLP23(Select) *sse) {
	step=sw;
//	id = sse->STEPfile_id;

	//std::cout << sse->UnderlyingTypeName() << std::endl;
	SdaiMeasure_value *v = (SdaiMeasure_value *)sse;

	if ( v->IsLength_measure()) {
		type = LENGTH_MEASURE;
		rvalue = (double)*v;
	} else if (v->IsMass_measure()) {
		type = MASS_MEASURE;
		rvalue = (double)*v;
	} else if (v->IsPlane_angle_measure()) {
		type = PLANE_ANGLE_MEASURE;
		rvalue = (double)*v;
	} else if (v->IsSolid_angle_measure()) {
		type = SOLID_ANGLE_MEASURE;
		rvalue = (double)*v;
	} else if (v->IsArea_measure()) {
		type = AREA_MEASURE;
		rvalue = (double)*v;
	} else if (v->IsVolume_measure()) {
		type = VOLUME_MEASURE;
		rvalue = (double)*v;
	} else if (v->IsParameter_value()) {
		type = PARAMETER_VALUE;
		rvalue = (double)*v;
	} else if (v->IsContext_dependent_measure()) {
		type = CONTEXT_DEPENDENT_MEASURE;
		rvalue = (double)*v;
	} else if (v->IsDescriptive_measure()) {
		SdaiDescriptive_measure dm = *v;
		type = DESCRIPTIVE_MEASURE;
		svalue = dm;
	} else if (v->IsPositive_length_measure()) {
		type = POSITIVE_LENGTH_MEASURE;
		rvalue = (double)*v;
	} else if (v->IsPositive_plane_angle_measure()) {
		type = PLANE_ANGLE_MEASURE;
		rvalue = (double)*v;
	} else if (v->IsCount_measure()) {
		type = COUNT_MEASURE;
		ivalue = (int)*v;
#ifdef AP203e2
	} else if (v->IsAmount_of_substance_measure()) {
		type = AMOUNT_OF_SUBSTANCE_MEASURE;
		rvalue = (double)*v;
	} else if (v->IsCelsius_temperature_measure()) {
		type = CELSIUS_TEMPERATURE_MEASURE;
		rvalue = (double)*v;
	} else if (v->IsElectric_current_measure()) {
		type = ELECTRIC_CURRENT_MEASURE;
		rvalue = (double)*v;
	} else if (v->IsLuminous_intensity_measure()) {
		type = LUMINOUS_INTENSITY_MEASURE;
		rvalue = (double)*v;
	} else if (v->IsNumeric_measure()) {
		type = NUMERIC_MEASURE;
		ivalue = (int)*v;
	} else if (v->IsPositive_ratio_measure()) {
		type = POSITIVE_RATIO_MEASURE;
		rvalue = (double)*v;
	} else if (v->IsRatio_measure()) {
		type = RATIO_MEASURE;
		rvalue = (double)*v;
	} else if (v->IsThermodynamic_temperature_measure()) {
		type = THERMODYNAMIC_TEMPERATURE_MEASURE;
		rvalue = (double)*v;
	} else if (v->IsTime_measure()) {
		type = TIME_MEASURE;
		rvalue = (double)*v;
#endif
	}

	return true;
}

void
MeasureValue::Print(int level) {
	TAB(level); std::cout << CLASSNAME << ":" << "(";
	std::cout << "ID:" << STEPid() << ")" << std::endl;

	TAB(level); std::cout << "Attributes:" << std::endl;
	if (type == DESCRIPTIVE_MEASURE) {
		TAB(level+1); std::cout << "Type:" << measure_type_names[type] << " Value:" << svalue << std::endl;
	} else if ((type == COUNT_MEASURE) || (type == NUMERIC_MEASURE)) {
		TAB(level+1); std::cout << "Type:" << measure_type_names[type] << " Value:" << ivalue << std::endl;
	} else {
		TAB(level+1); std::cout << "Type:" << measure_type_names[type] << " Value:" << rvalue << std::endl;
	}
}
STEPEntity *
MeasureValue::Create(STEPWrapper *sw, SCLP23(Application_instance) *sse) {
	Factory::OBJECTS::iterator i;
	if ((i = Factory::FindObject(sse->STEPfile_id)) == Factory::objects.end()) {
		MeasureValue *object = new MeasureValue(sw,sse->STEPfile_id);

		Factory::AddObject(object);

		if (!object->Load(sw, (SCLP23(Select) *)sse)) {
			std::cerr << CLASSNAME << ":Error loading class in ::Create() method." << std::endl;
			delete object;
			return NULL;
		}
		return static_cast<STEPEntity *>(object);
	} else {
		return (*i).second;
	}
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
