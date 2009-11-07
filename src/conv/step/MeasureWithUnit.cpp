/*                 MeasureWithUnit.cpp
 * BRL-CAD
 *
 * Copyright (c) 1994-2009 United States Government as represented by
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
/** @file MeasureWithUnit.cpp
 *
 * Routines to convert STEP "MeasureWithUnit" to BRL-CAD BREP
 * structures.
 *
 */

#include "STEPWrapper.h"
#include "Factory.h"

#include "MeasureValue.h"
#include "Unit.h"
#include "LengthSiUnit.h"

#include "MeasureWithUnit.h"

#define CLASSNAME "MeasureWithUnit"
#define ENTITYNAME "Measure_With_Unit"
string MeasureWithUnit::entityname = Factory::RegisterClass(ENTITYNAME,(FactoryMethod)MeasureWithUnit::Create);

MeasureWithUnit::MeasureWithUnit() {
	step = NULL;
	id = 0;
	unit_component = NULL;
}

MeasureWithUnit::MeasureWithUnit(STEPWrapper *sw,int STEPid) {
	step = sw;
	id = STEPid;
	unit_component = NULL;
}

MeasureWithUnit::~MeasureWithUnit() {
	unit_component=NULL;
}

double
MeasureWithUnit::GetLengthConversionFactor() {
	double sifactor;
	double mfactor;
	SiUnit *si = dynamic_cast<SiUnit *>(unit_component);
	if (si != NULL) {
		//found SI length unit
		sifactor = si->GetLengthConversionFactor();
	}
	mfactor = value_component.GetLengthMeasure();

	return mfactor * sifactor;
}

double
MeasureWithUnit::GetPlaneAngleConversionFactor() {
	double sifactor;
	double mfactor;
	SiUnit *si = dynamic_cast<SiUnit *>(unit_component);
	if (si != NULL) {
		//found SI length unit
		sifactor = si->GetPlaneAngleConversionFactor();
	}
	mfactor = value_component.GetPlaneAngleMeasure();

	return mfactor * sifactor;
}

double
MeasureWithUnit::GetSolidAngleConversionFactor() {
	double sifactor;
	double mfactor;
	SiUnit *si = dynamic_cast<SiUnit *>(unit_component);
	if (si != NULL) {
		//found SI length unit
		sifactor = si->GetSolidAngleConversionFactor();
	}
	mfactor = value_component.GetSolidAngleMeasure();

	return mfactor * sifactor;
}

bool
MeasureWithUnit::Load(STEPWrapper *sw,SCLP23(Application_instance) *sse) {
	step=sw;
	id = sse->STEPfile_id;

	// need to do this for local attributes to makes sure we have
	// the actual entity and not a complex/supertype parent
	sse = step->getEntity(sse,ENTITYNAME);

	SCLP23(Select) *select = step->getSelectAttribute(sse,"value_component");
	if (select) {
		if (!value_component.Load(step,select)) {
			cout << CLASSNAME << ":Error loading MeasureValue." << endl;
			return false;
		}
	} else {
		return false;
	}

	if (unit_component == NULL) {
		// select-select
		select = step->getSelectAttribute(sse,"unit_component");
		if (select) {
			SdaiUnit *u = (SdaiUnit *)select;
			if ( u->IsNamed_unit() ) {
				SdaiNamed_unit *nu = *u;
				unit_component = (Unit*)Factory::CreateObject(sw,(SCLP23(Application_instance) *)nu);
	#ifdef AP203e
			} else if (u->IsDerived_unit()) {
				SdaiDerived_unit *du = *u;
				unit_component = (Unit*)Factory::CreateObject(sw,(SCLP23(Application_instance) *)du);
	#endif
			} else {
				cerr << CLASSNAME << ": Unknown 'Unit' type from select." << endl;
				return false;
			}
		} else {
			return false;
		}
	}
	return true;
}

void
MeasureWithUnit::Print(int level) {
	TAB(level); cout << CLASSNAME << ":" << "(";
	cout << "ID:" << STEPid() << ")" << endl;

	TAB(level); cout << "Attributes:" << endl;
	TAB(level+1); cout << "value_component:" << endl;
	value_component.Print(level+1);

	TAB(level+1); cout << "unit_component:" << endl;
	unit_component->Print(level+1);
}

STEPEntity *
MeasureWithUnit::Create(STEPWrapper *sw, SCLP23(Application_instance) *sse) {
	Factory::OBJECTS::iterator i;
	if ((i = Factory::FindObject(sse->STEPfile_id)) == Factory::objects.end()) {
		MeasureWithUnit *object = new MeasureWithUnit(sw,sse->STEPfile_id);

		Factory::AddObject(object);

		if (!object->Load(sw, sse)) {
			cerr << CLASSNAME << ":Error loading class in ::Create() method." << endl;
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
