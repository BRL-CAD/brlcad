/*                 TrimmedCurve.cpp
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
/** @file step/TrimmedCurve.cpp
 *
 * Routines to convert STEP "TrimmedCurve" to BRL-CAD BREP
 * structures.
 *
 */

#include "STEPWrapper.h"
#include "Factory.h"
#include "LocalUnits.h"
#include "TrimmingSelect.h"

#include "TrimmedCurve.h"

#define CLASSNAME "TrimmedCurve"
#define ENTITYNAME "Trimmed_Curve"
string TrimmedCurve::entityname = Factory::RegisterClass(ENTITYNAME,(FactoryMethod)TrimmedCurve::Create);

static const char *Trimming_preference_string[] = {
    "cartesian",
    "parameter",
    "unspecified",
    "unset"
};

TrimmedCurve::TrimmedCurve() {
    step = NULL;
    id = 0;
    basis_curve = NULL;
}

TrimmedCurve::TrimmedCurve(STEPWrapper *sw,int step_id) {
    step = sw;
    id = step_id;
    basis_curve = NULL;
}

TrimmedCurve::~TrimmedCurve() {
    /*
      LIST_OF_TRIMMING_SELECT::iterator i = trim_1.begin();
      while(i != trim_1.end()) {
      delete (*i);
      i = trim_1.erase(i);
      }

      i = trim_2.begin();
      while(i != trim_2.end()) {
      delete (*i);
      i = trim_2.erase(i);
      }
    */
    basis_curve = NULL;
    trim_1.clear();
    trim_2.clear();
}

bool
TrimmedCurve::Load(STEPWrapper *sw,SCLP23(Application_instance) *sse) {
    step=sw;
    id = sse->STEPfile_id;

    if ( !BoundedCurve::Load(step,sse) ) {
	std::cout << CLASSNAME << ":Error loading base class ::BoundedCurve." << std::endl;
	return false;
    }

    // need to do this for local attributes to makes sure we have
    // the actual entity and not a complex/supertype parent
    sse = step->getEntity(sse,ENTITYNAME);

    if (basis_curve == NULL) {
	SCLP23(Application_instance) *entity = step->getEntityAttribute(sse,"basis_curve");
	if (entity) {
	    basis_curve = dynamic_cast<Curve *>(Factory::CreateObject(sw,entity)); //CreateCurveObject(sw,entity));
	} else {
	    std::cerr << CLASSNAME << ": Error loading entity attribute 'basis_curve'." << std::endl;
	    return false;
	}
    }
    if (trim_1.empty()) {
	STEPattribute *attr = step->getAttribute(sse, "trim_1");
	if (attr) {
	    STEPaggregate *sa = (STEPaggregate *) (attr->ptr.a);
	    EntityNode *sn = (EntityNode *) sa->GetHead();
	    SCLP23(Select) *p;
	    while (sn != NULL) {
		p = (SCLP23(Select) *) sn->node;
		TrimmingSelect *aTS = new TrimmingSelect();

		trim_1.push_back(aTS);

		if (!aTS->Load(step, p)) {
		    std::cout << CLASSNAME << ":Error loading TrimmingSelect from list." << std::endl;
		    return false;
		}

		sn = (EntityNode *) sn->NextNode();
	    }
	}
    }
    if (trim_2.empty()) {
	STEPattribute *attr = step->getAttribute(sse, "trim_2");
	if (attr) {
	    STEPaggregate *sa = (STEPaggregate *) (attr->ptr.a);
	    EntityNode *sn = (EntityNode *) sa->GetHead();
	    SCLP23(Select) *p;
	    while (sn != NULL) {
		p = (SCLP23(Select) *) sn->node;
		TrimmingSelect *aTS = new TrimmingSelect();

		trim_2.push_back(aTS);

		if (!aTS->Load(step, p)) {
		    std::cout << CLASSNAME << ":Error loading TrimmingSelect from list." << std::endl;
		    return false;
		}
		sn = (EntityNode *) sn->NextNode();
	    }
	}
    }

    sense_agreement = step->getBooleanAttribute(sse,"sense_agreement");
    master_representation = (Trimming_preference)step->getEnumAttribute(sse,"master_representation");

    return true;
}

const double *
TrimmedCurve::PointAtEnd() {
    return basis_curve->PointAtEnd();
}

const double *
TrimmedCurve::PointAtStart() {
    return basis_curve->PointAtStart();
}

void
TrimmedCurve::Print(int level) {
    TAB(level); std::cout << CLASSNAME << ":" << name << "(";
    std::cout << "ID:" << STEPid() << ")" << std::endl;

    TAB(level); std::cout << "Attributes:" << std::endl;
    basis_curve->Print(level+1);
    TAB(level); std::cout << "trim_1:" << std::endl;
    LIST_OF_TRIMMING_SELECT::iterator i;
    for(i=trim_1.begin();i!=trim_1.end();i++) {
	(*i)->Print(level+1);
    }

    TAB(level); std::cout << "trim_2:" << std::endl;
    for(i=trim_2.begin();i!=trim_2.end();i++) {
	(*i)->Print(level+1);
    }
    TAB(level); std::cout << "sense_agreement:" << step->getBooleanString(sense_agreement) << std::endl;
    TAB(level); std::cout << "master_representation:" << Trimming_preference_string[master_representation] << std::endl;
}
STEPEntity *
TrimmedCurve::Create(STEPWrapper *sw, SCLP23(Application_instance) *sse) {
    Factory::OBJECTS::iterator i;
    if ((i = Factory::FindObject(sse->STEPfile_id)) == Factory::objects.end()) {
	TrimmedCurve *object = new TrimmedCurve(sw,sse->STEPfile_id);

	Factory::AddObject(object);

	if (!object->Load(sw, sse)) {
	    std::cerr << CLASSNAME << ":Error loading class in ::Create() method." << std::endl;
	    delete object;
	    return NULL;
	}
	return static_cast<STEPEntity *>(object);
    } else {
	return (*i).second;
    }
}


bool
TrimmedCurve::LoadONBrep(ON_Brep *brep)
{
    if (ON_id >= 0)
	return true; // already loaded

    //TODO: pass down trimmings(protected curve attributes
    LIST_OF_TRIMMING_SELECT::iterator iter = trim_1.begin();
    TrimmingSelect *ts = (*iter);
    if (ts->IsParameterTrim()) {
	trimmed = true;
	parameter_trim = true;
	t = ts->GetParameterTrim(); // no units conversion here because we're not
	// sure what type of parameter it is until we
	// look at the entity it belongs to.
    } else {
	const double *point=ts->GetPointTrim();
	trimmed = true;
	parameter_trim = false;
	for(int i=0;i<3;i++)
	    trim_startpoint[i] = point[i] * LocalUnits::length;
    }
    iter = trim_2.begin();
    TrimmingSelect *te = (*iter);
    if (te->IsParameterTrim()) {
	trimmed = true;
	parameter_trim = true;
	s = te->GetParameterTrim(); // no units conversion here because we're not
	// sure what type of parameter it is until we
	// look at the entity it belongs to.
    } else {
	const double *point=te->GetPointTrim();
	trimmed = true;
	parameter_trim = false;
	for(int i=0;i<3;i++)
	    trim_endpoint[i] = point[i] * LocalUnits::length;
    }

    if (parameter_trim) {
	basis_curve->SetParameterTrim(t,s);
    } else {
	basis_curve->SetPointTrim(trim_startpoint,trim_endpoint);
    }
    basis_curve->LoadONBrep(brep);

    ON_id = basis_curve->GetONId();

    return true;
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
