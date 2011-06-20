/*                 MeasureValue.h
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
/** @file step/MeasureValue.h
 *
 * Class definition used to convert STEP "MeasureValue" to BRL-CAD BREP
 * structures.
 *
 */

#ifndef MEASUREVALUE_H_
#define MEASUREVALUE_H_

#include "STEPEntity.h"

enum measure_type {
	AMOUNT_OF_SUBSTANCE_MEASURE,
	AREA_MEASURE,
	CELSIUS_TEMPERATURE_MEASURE,
	CONTEXT_DEPENDENT_MEASURE,
	COUNT_MEASURE,
	DESCRIPTIVE_MEASURE,
	ELECTRIC_CURRENT_MEASURE,
	LENGTH_MEASURE,
	LUMINOUS_INTENSITY_MEASURE,
	MASS_MEASURE,
	NUMERIC_MEASURE,
	PARAMETER_VALUE,
	PLANE_ANGLE_MEASURE,
	POSITIVE_LENGTH_MEASURE,
	POSITIVE_PLANE_ANGLE_MEASURE,
	POSITIVE_RATIO_MEASURE,
	RATIO_MEASURE,
	SOLID_ANGLE_MEASURE,
	THERMODYNAMIC_TEMPERATURE_MEASURE,
	TIME_MEASURE,
	VOLUME_MEASURE
	};

class MeasureValue : public STEPEntity {
private:
	static string entityname;

protected:
	int ivalue;
	double rvalue;
	string svalue;
	measure_type type;

public:
	MeasureValue();
	virtual ~MeasureValue();
	MeasureValue(STEPWrapper *sw,int step_id);
	double GetLengthMeasure();
	double GetPlaneAngleMeasure();
	double GetSolidAngleMeasure();
	bool Load(STEPWrapper *sw,SCLP23(Select) *sse);
	virtual void Print(int level);

	//static methods
	static STEPEntity *Create(STEPWrapper *sw,SCLP23(Application_instance) *sse);
};

#endif /* MEASUREVALUE_H_ */

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
