/*                 ContextDependentShapeRepresentation.cpp
 * BRL-CAD
 *
 * Copyright (c) 1994-2014 United States Government as represented by
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
/** @file step/ContextDependentShapeRepresentation.cpp
 *
 * Routines to convert STEP "ContextDependentShapeRepresentation" to BRL-CAD BREP
 * structures.
 *
 */

#include "STEPWrapper.h"
#include "Factory.h"

#include "RepresentationRelationship.h"
#include "RepresentationRelationshipWithTransformation.h"
#include "ShapeRepresentationRelationship.h"
#include "ProductDefinitionShape.h"
#include "ContextDependentShapeRepresentation.h"

#define CLASSNAME "ContextDependentShapeRepresentation"
#define ENTITYNAME "Context_Dependent_Shape_Representation"
string ContextDependentShapeRepresentation::entityname = Factory::RegisterClass(ENTITYNAME, (FactoryMethod) ContextDependentShapeRepresentation::Create);

ContextDependentShapeRepresentation::ContextDependentShapeRepresentation()
{
    step = NULL;
    id = 0;
    represented_product_relation = NULL;
}

ContextDependentShapeRepresentation::ContextDependentShapeRepresentation(STEPWrapper *sw, int step_id)
{
    step = sw;
    id = step_id;
    represented_product_relation = NULL;
}

ContextDependentShapeRepresentation::~ContextDependentShapeRepresentation()
{
    if (representation_relation.size() > 1) {
	LIST_OF_REPRESENTATION_RELATIONSHIPS::iterator ic = representation_relation.begin();

	while (ic != representation_relation.end()) {
	    delete(*ic);
	    ic = representation_relation.erase(ic);
	}
    } else {
	representation_relation.clear();
    }
    // created through factory will be deleted there.
    represented_product_relation = NULL;
}

string ContextDependentShapeRepresentation::ClassName()
{
    return entityname;
}

Representation *
ContextDependentShapeRepresentation::GetRepresentationRelationshipRep_1()
{
    if (!representation_relation.empty()) {
	LIST_OF_REPRESENTATION_RELATIONSHIPS::iterator irr;
	if (representation_relation.size() == 1) { // just have an SSR
	    irr = representation_relation.begin();
	    if (irr != representation_relation.end()) {
		if (dynamic_cast<ShapeRepresentationRelationship *>(*irr) != NULL) {
		    RepresentationRelationship *rr = dynamic_cast<RepresentationRelationship *>(*irr);

		    return rr->GetRepresentationRelationshipRep_1();
		}
	    }
	} else {
	    for (irr = representation_relation.begin(); irr != representation_relation.end(); ++irr) {
		if ( (dynamic_cast<RepresentationRelationshipWithTransformation*>(*irr) == NULL) &&
			(dynamic_cast<ShapeRepresentationRelationship *>(*irr) == NULL) &&
			(dynamic_cast<RepresentationRelationship *>(*irr) != NULL) ) {
		    RepresentationRelationship *rr = dynamic_cast<RepresentationRelationship *>(*irr);

		    return rr->GetRepresentationRelationshipRep_1();
		}
	    }
	}
    }
    return NULL;
}

Representation *
ContextDependentShapeRepresentation::GetRepresentationRelationshipRep_2()
{
    if (!representation_relation.empty()) {
	LIST_OF_REPRESENTATION_RELATIONSHIPS::iterator irr;
	if (representation_relation.size() == 1) { // just have an SSR
	    irr = representation_relation.begin();
	    if (irr != representation_relation.end()) {
		if (dynamic_cast<ShapeRepresentationRelationship *>(*irr) != NULL) {
		    RepresentationRelationship *rr = dynamic_cast<RepresentationRelationship *>(*irr);

		    return rr->GetRepresentationRelationshipRep_2();
		}
	    }
	} else {
	    for (irr = representation_relation.begin(); irr != representation_relation.end(); ++irr) {
		if ( (dynamic_cast<RepresentationRelationshipWithTransformation*>(*irr) == NULL) &&
			(dynamic_cast<ShapeRepresentationRelationship *>(*irr) == NULL) &&
			(dynamic_cast<RepresentationRelationship *>(*irr) != NULL) ) {
		    RepresentationRelationship *rr = dynamic_cast<RepresentationRelationship *>(*irr);

		    return rr->GetRepresentationRelationshipRep_2();
		}
	    }
	}
    }
    return NULL;
}

Axis2Placement3D *
ContextDependentShapeRepresentation::GetTransformItem_1()
{
    if (!representation_relation.empty()) {
	LIST_OF_REPRESENTATION_RELATIONSHIPS::iterator irr;
	RepresentationRelationshipWithTransformation *rrwt = NULL;
	for (irr = representation_relation.begin(); irr != representation_relation.end(); ++irr) {
	    if ( (rrwt=dynamic_cast<RepresentationRelationshipWithTransformation*>(*irr)) != NULL) {
		return rrwt->GetTransformItem_1();
	    }
	}
    }
    return NULL;
}

Axis2Placement3D *
ContextDependentShapeRepresentation::GetTransformItem_2()
{
    if (!representation_relation.empty()) {
	LIST_OF_REPRESENTATION_RELATIONSHIPS::iterator irr;
	RepresentationRelationshipWithTransformation *rrwt = NULL;
	for (irr = representation_relation.begin(); irr != representation_relation.end(); ++irr) {
	    if ( (rrwt=dynamic_cast<RepresentationRelationshipWithTransformation*>(*irr)) != NULL) {
		return rrwt->GetTransformItem_2();
	    }
	}
    }
    return NULL;
}

ProductDefinition *
ContextDependentShapeRepresentation::GetRelatingProductDefinition()
{
    ProductDefinition *ret = NULL;

    if (represented_product_relation) {
	ret = represented_product_relation->GetRelatingProductDefinition();
    }

    return ret;
}

ProductDefinition *
ContextDependentShapeRepresentation::GetRelatedProductDefinition()
{
    ProductDefinition *ret = NULL;

    if (represented_product_relation) {
	ret = represented_product_relation->GetRelatedProductDefinition();
    }

    return ret;
}

bool ContextDependentShapeRepresentation::Load(STEPWrapper *sw, SDAI_Application_instance *sse)
{
    step = sw;
    id = sse->STEPfile_id;

    // need to do this for local attributes to makes sure we have
    // the actual entity and not a complex/supertype parent
    sse = step->getEntity(sse, ENTITYNAME);

    if (representation_relation.empty()) {
	SDAI_Application_instance *entity = step->getEntityAttribute(sse, "representation_relation");
	if (entity) {
	    /*
	     * can't use factory here because complex sub_entities each take same step_id TODO revisit cleanup complex types so they too can use factory
	     */
	    if (entity->IsComplex()) {
		SDAI_Application_instance *sub_entity = step->getEntity(entity, "Representation_Relationship_With_Transformation");
		if (sub_entity) {
		    RepresentationRelationshipWithTransformation *aRRWT = new RepresentationRelationshipWithTransformation();

		    representation_relation.push_back(aRRWT);
		    if (!aRRWT->Load(step, sub_entity)) {
			std::cout << CLASSNAME << ":Error loading RepresentationRelationshipWithTransformation" << std::endl;
			return false;
		    }
		}

		sub_entity = step->getEntity(entity, "Shape_Representation_Relationship");
		if (sub_entity) {
		    ShapeRepresentationRelationship *aSRR = new ShapeRepresentationRelationship();

		    representation_relation.push_back(aSRR);
		    if (!aSRR->Load(step, sub_entity)) {
			std::cout << CLASSNAME << ":Error loading ShapeRepresentationRelationship" << std::endl;
			return false;
		    }
		}

		sub_entity = step->getEntity(entity, "Representation_Relationship");
		if (sub_entity) {
		    RepresentationRelationship *aRR = new RepresentationRelationship();

		    representation_relation.push_back(aRR);
		    if (!aRR->Load(step, sub_entity)) {
			std::cout << CLASSNAME << ":Error loading RepresentationRelationship" << std::endl;
			return false;
		    }
		}
	    } else {
		ShapeRepresentationRelationship *aSRR = dynamic_cast<ShapeRepresentationRelationship *>(Factory::CreateObject(sw, entity));
		if (aSRR != NULL) {
		    representation_relation.push_back(aSRR);
		} else {
		    std::cout << CLASSNAME << ":Error loading ShapeRepresentationRelationship" << std::endl;
		    return false;
		}
	    }
	}
    }

    // currently don't use/need this when loading geometry from STEP
    if (represented_product_relation == NULL) {
	SDAI_Application_instance *entity = step->getEntityAttribute(sse, "represented_product_relation");
	if (entity) { //this attribute is optional
	    represented_product_relation = dynamic_cast<ProductDefinitionShape *>(Factory::CreateObject(sw, entity));
	} else {
	    std::cout << CLASSNAME << ":Error loading attribute 'represented_product_relation'." << std::endl;
	    return false;
	}
    }

    return true;
}

void ContextDependentShapeRepresentation::Print(int level)
{
    TAB(level);
    std::cout << CLASSNAME << ":" << "(";
    std::cout << "ID:" << STEPid() << ")" << std::endl;

    TAB(level);
    std::cout << "Attributes:" << std::endl;
    TAB(level + 1);
    std::cout << "representation_relations:" << std::endl;
    LIST_OF_REPRESENTATION_RELATIONSHIPS::iterator irr;
    for (irr = representation_relation.begin(); irr != representation_relation.end(); ++irr) {
	(*irr)->Print(level + 1);
    }
    TAB(level + 1);
    std::cout << "represented_product_relation:" << std::endl;
    represented_product_relation->Print(level + 2);
}

STEPEntity *
ContextDependentShapeRepresentation::GetInstance(STEPWrapper *sw, int id)
{
    return new ContextDependentShapeRepresentation(sw, id);
}

STEPEntity *
ContextDependentShapeRepresentation::Create(STEPWrapper *sw, SDAI_Application_instance *sse)
{
    return STEPEntity::CreateEntity(sw, sse, GetInstance, CLASSNAME);
}

bool ContextDependentShapeRepresentation::LoadONBrep(ON_Brep *brep)
{
    std::cerr << "Error: ::LoadONBrep(ON_Brep *brep<" << std::hex << brep << std::dec << ">) not implemented for " << entityname << std::endl;
    return false;
}

double ContextDependentShapeRepresentation::GetLengthConversionFactor()
{
    LIST_OF_REPRESENTATION_RELATIONSHIPS::iterator irr;
    double retVal = 1.0;
    if ((irr = representation_relation.begin()) != representation_relation.end()) {
	retVal = (*irr)->GetLengthConversionFactor();
    }
    return retVal;
}

double ContextDependentShapeRepresentation::GetPlaneAngleConversionFactor()
{
    LIST_OF_REPRESENTATION_RELATIONSHIPS::iterator irr;
    double retVal = 1.0;
    if ((irr = representation_relation.begin()) != representation_relation.end()) {
	retVal = (*irr)->GetPlaneAngleConversionFactor();
    }
    return retVal;
}

double ContextDependentShapeRepresentation::GetSolidAngleConversionFactor()
{
    LIST_OF_REPRESENTATION_RELATIONSHIPS::iterator irr;
    double retVal = 1.0;
    if ((irr = representation_relation.begin()) != representation_relation.end()) {
	retVal = (*irr)->GetSolidAngleConversionFactor();
    }
    return retVal;
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
