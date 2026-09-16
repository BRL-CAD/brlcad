/*              S T E P W R A P P E R A T T R I B U T E S . C P P
 * BRL-CAD
 *
 * Copyright (c) 1994-2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 */

/** @file step/STEPWrapperAttributes.cpp
 *
 * STEP entity lookup, complex-supertype traversal, and typed attribute access.
 */

#include "common.h"

#include <cctype>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>

#include "STEPWrapper.h"
#ifdef HAVE_STEPCODE_LAZY
#  include "STEPLazySession.h"
#endif

namespace {

bool
step_attribute_logical_name(const std::string &actual, const char *requested)
{
    if (!requested) return false;
    if (actual == requested) return true;
    const std::string suffix = std::string(".") + requested;
    return actual.size() > suffix.size() &&
	actual.compare(actual.size() - suffix.size(), suffix.size(), suffix) == 0;
}

/* A redeclared EXPRESS attribute is represented twice by generated STEPcode:
 * the inherited slot is derived and the narrowed slot has a qualified name
 * such as "representation.items".  Prefer the explicit narrowed slot while
 * retaining ordinary unqualified lookup for every older schema. */
STEPattribute *
step_explicit_attribute(SDAI_Application_instance *instance,
    const char *requested)
{
    if (!instance || !requested) return NULL;
    STEPattribute *qualified = NULL;
    STEPattribute *fallback = NULL;
    instance->ResetAttributes();
    for (STEPattribute *attribute = instance->NextAttribute(); attribute;
	 attribute = instance->NextAttribute()) {
	const std::string actual = attribute->Name();
	if (!step_attribute_logical_name(actual, requested)) continue;
	if (actual == requested && !fallback) fallback = attribute;
	if (attribute->IsDerived()) continue;
	if (actual != requested) qualified = attribute;
    }
    return qualified ? qualified : fallback;
}

int
step_entity_reference_id(const std::string &value)
{
    const size_t marker = value.find('#');
    if (marker == std::string::npos) return 0;
    char *end = NULL;
    const long id = std::strtol(value.c_str() + marker + 1, &end, 10);
    return end != value.c_str() + marker + 1 && id > 0 ?
	static_cast<int>(id) : 0;
}

SDAI_Application_instance *
step_select_entity(STEPWrapper *wrapper, SDAI_Select *select)
{
    if (!wrapper || !select || !select->exists()) return NULL;
    std::string value;
    select->STEPwrite(value);
    return wrapper->getEntity(step_entity_reference_id(value));
}

} // namespace

SDAI_Application_instance *
STEPWrapper::getEntity(int STEPid)
{
    if (STEPid <= 0)
	return NULL;
#ifdef HAVE_STEPCODE_LAZY
    if (lazy_session) {
	SDAI_Application_instance *instance = lazy_batch ? lazy_batch->Get(STEPid) : NULL;
	if (instance) return instance;
	for (std::vector<std::unique_ptr<brlcad::step::STEPLazyBatch> >::const_iterator batch =
		 lazy_supplemental_batches.begin(); batch != lazy_supplemental_batches.end(); ++batch) {
	    instance = (*batch)->Get(STEPid);
	    if (instance) return instance;
	}
	std::unique_ptr<brlcad::step::STEPLazyBatch> supplemental(
	    new brlcad::step::STEPLazyBatch(lazy_session->LoadBatch(static_cast<uint64_t>(STEPid))));
	instance = supplemental->Get(STEPid);
	if (instance) lazy_supplemental_batches.push_back(std::move(supplemental));
	return instance;
    }
#endif
    if (!instance_list) return NULL;
    MgrNode *node = instance_list->FindFileId(STEPid);
    return node ? node->GetSTEPentity() : NULL;
}


SDAI_Application_instance *
STEPWrapper::getEntity(int STEPid, const char *name)
{
    SDAI_Application_instance *se = getEntity(STEPid);

    if (se && se->IsComplex()) {
	se = getSuperType(STEPid, name);
    }
    return se;
}


SDAI_Application_instance *
STEPWrapper::getEntity(SDAI_Application_instance *sse, const char *name)
{
    if (sse && sse->IsComplex()) {
	sse = getSuperType(sse, name);
    }
    return sse;
}


STEPattribute *
STEPWrapper::getAttribute(int STEPid, const char *name)
{
    STEPattribute *retValue = NULL;
    SDAI_Application_instance *sse = getEntity(STEPid);

    sse->ResetAttributes();

    STEPattribute *attr;
    while ((attr = sse->NextAttribute()) != NULL) {
	std::string attrname = attr->Name();
	if (attrname.compare(name) == 0) {
	    retValue = attr;
	    break;
	}
    }

    return retValue;
}


LIST_OF_STRINGS *
STEPWrapper::getAttributes(int STEPid)
{
    LIST_OF_STRINGS *l = new LIST_OF_STRINGS;
    SDAI_Application_instance *sse = getEntity(STEPid);

    sse->ResetAttributes();

    STEPattribute *attr;
    while ((attr = sse->NextAttribute()) != NULL) {
	std::string name = attr->Name();

	l->push_back(name);
    }

    return l;
}


Boolean
STEPWrapper::getBooleanAttribute(int STEPid, const char *name)
{
    Boolean retValue = BUnset;
    SDAI_Application_instance *sse = getEntity(STEPid);

    sse->ResetAttributes();

    STEPattribute *attr;
    while ((attr = sse->NextAttribute()) != NULL) {
	std::string attrname = attr->Name();

	if (attrname.compare(name) == 0) {
	    retValue = (Boolean)(*attr->ptr.e).asInt();
	    if (retValue > BUnset) {
		retValue = BUnset;
	    }
	    break;
	}
    }
    return retValue;
}


std::string
STEPWrapper::getEnumAttributeName(int STEPid, const char *name)
{
    return getEnumAttributeName(getEntity(STEPid), name);
}


Logical
STEPWrapper::getLogicalAttribute(int STEPid, const char *name)
{
    Logical retValue = LUnknown;
    SDAI_Application_instance *sse = getEntity(STEPid);

    sse->ResetAttributes();

    STEPattribute *attr;
    while ((attr = sse->NextAttribute()) != NULL) {
	std::string attrname = attr->Name();

	if (attrname.compare(name) == 0) {
	    retValue = (Logical)(*attr->ptr.e).asInt();
	    if (retValue > LUnknown) {
		retValue = LUnknown;
	    }
	    break;
	}
    }
    return retValue;
}


std::string
STEPWrapper::getLogicalString(Logical v)
{
    std::string retValue = "Unknown";

    switch (v) {
	case LFalse:
	    retValue = "LFalse";
	    break;
	case LTrue:
	    retValue = "LTrue";
	    break;
	case LUnset:
	    retValue = "LUnset";
	    break;
	case LUnknown:
	    retValue = "LUnknown";
	    break;
    }
    return retValue;
}


std::string
STEPWrapper::getBooleanString(Boolean v)
{
    std::string retValue = "Unknown";

    switch (v) {
	case BFalse:
	    retValue = "BFalse";
	    break;
	case BTrue:
	    retValue = "BTrue";
	    break;
	case BUnset:
	    retValue = "BUnset";
	    break;
    }
    return retValue;
}


SDAI_Application_instance *
STEPWrapper::getEntityAttribute(int STEPid, const char *name)
{
    return getEntityAttribute(getEntity(STEPid), name);
}


int
STEPWrapper::getIntegerAttribute(int STEPid, const char *name)
{
    int retValue = 0;
    SDAI_Application_instance *sse = getEntity(STEPid);

    sse->ResetAttributes();

    STEPattribute *attr;
    while ((attr = sse->NextAttribute()) != NULL) {
	std::string attrname = attr->Name();

	if (attrname.compare(name) == 0) {
	    retValue = *attr->ptr.i;
	    break;
	}
    }
    return retValue;
}


double
STEPWrapper::getRealAttribute(int STEPid, const char *name)
{
    double retValue = 0.0;
    SDAI_Application_instance *sse = getEntity(STEPid);

    sse->ResetAttributes();

    STEPattribute *attr;
    while ((attr = sse->NextAttribute()) != NULL) {
	std::string attrname = attr->Name();

	if (attrname.compare(name) == 0) {
	    retValue = *attr->ptr.r;
	    break;
	}
    }
    return retValue;
}


LIST_OF_ENTITIES *
STEPWrapper::getListOfEntities(int STEPid, const char *name)
{
    return getListOfEntities(getEntity(STEPid), name);
}


LIST_OF_LIST_OF_POINTS *
STEPWrapper::getListOfListOfPoints(int STEPid, const char *attrName)
{
    LIST_OF_LIST_OF_POINTS *l = new LIST_OF_LIST_OF_POINTS;

    SDAI_Application_instance *sse = getEntity(STEPid);
    sse->ResetAttributes();

    STEPattribute *attr;
    while ((attr = sse->NextAttribute()) != NULL) {
	std::string attrval;
	std::string name = attr->Name();

	if (name.compare(attrName) == 0) {
	    ErrorDescriptor errdesc;

	    //std::cout << attr->asStr(attrval) << std::endl;
	    //std::cout << attr->TypeName() << std::endl;


	    GenericAggregate_ptr gp = (GenericAggregate_ptr)attr->ptr.a;

	    STEPnode *sn = (STEPnode *)gp->GetHead();
	    //EntityAggregate *ag = new EntityAggregate();


	    const char *eaStr;

	    LIST_OF_POINTS *points;
	    while (sn != NULL) {
		//sn->STEPwrite(std::cout);
		//std::cout << std::endl;
		eaStr = sn->asStr(attrval);
		points = parseListOfPointEntities(eaStr);
		l->push_back(points);
		sn = (STEPnode *)sn->NextNode();
	    }
	    break;
	}
    }

    return l;
}


MAP_OF_SUPERTYPES *
STEPWrapper::getMapOfSuperTypes(int STEPid)
{
    MAP_OF_SUPERTYPES *m = new MAP_OF_SUPERTYPES;
    SDAI_Application_instance *sse = getEntity(STEPid);

    if (sse->IsComplex()) {
	STEPcomplex *sc = ((STEPcomplex *)sse)->head;
	while (sc) {
	    (*m)[sc->EntityName()] = sc;
	    sc = sc->sc;
	}
    }

    return m;
}


void
STEPWrapper::getSuperTypes(int STEPid, MAP_OF_SUPERTYPES &m)
{
    SDAI_Application_instance *sse = getEntity(STEPid);

    if (sse->IsComplex()) {
	STEPcomplex *sc = ((STEPcomplex *)sse)->head;
	while (sc) {
	    m[sc->EntityName()] = sc;
	    sc = sc->sc;
	}
    }
}


SDAI_Application_instance *
STEPWrapper::getSuperType(int STEPid, const char *name)
{
    SDAI_Application_instance *sse = getEntity(STEPid);

    if (sse->IsComplex()) {
	STEPcomplex *sc = ((STEPcomplex *)sse)->head;
	while (sc) {
	    std::string ename = sc->EntityName();

	    if (ename.compare(name) == 0) {
		return sc;
	    }
	    sc = sc->sc;
	}
    }
    return NULL;
}


std::string
STEPWrapper::getStringAttribute(int STEPid, const char *name)
{
    SDAI_Application_instance *sse = getEntity(STEPid);
    return getStringAttribute(sse, name);
}


STEPattribute *
STEPWrapper::getAttribute(SDAI_Application_instance *sse, const char *name)
{
    STEPattribute *retValue = NULL;

    sse->ResetAttributes();

    STEPattribute *attr;
    while ((attr = sse->NextAttribute()) != NULL) {
	std::string attrname = attr->Name();
	if (attrname.compare(name) == 0) {
	    retValue = attr;
	    break;
	}
    }

    return retValue;
}


LIST_OF_STRINGS *
STEPWrapper::getAttributes(SDAI_Application_instance *sse)
{
    LIST_OF_STRINGS *l = new LIST_OF_STRINGS;

    sse->ResetAttributes();

    STEPattribute *attr;
    while ((attr = sse->NextAttribute()) != NULL) {
	std::string name = attr->Name();

	l->push_back(name);
    }

    return l;
}


Boolean
STEPWrapper::getBooleanAttribute(SDAI_Application_instance *sse, const char *name)
{
    Boolean retValue = BUnset;

    sse->ResetAttributes();

    STEPattribute *attr;
    while ((attr = sse->NextAttribute()) != NULL) {
	std::string attrname = attr->Name();

	if (attrname.compare(name) == 0) {
	    retValue = (Boolean)(*attr->ptr.e).asInt();
	    if (retValue > BUnset) {
		retValue = BUnset;
	    }
	    break;
	}
    }
    return retValue;
}


std::string
STEPWrapper::getEnumAttributeName(SDAI_Application_instance *sse, const char *name)
{
    if (!sse || !name)
	return std::string();

    sse->ResetAttributes();
    STEPattribute *attr;
    while ((attr = sse->NextAttribute()) != NULL) {
	if (std::string(attr->Name()) != name)
	    continue;
	if (attr->is_null())
	    return std::string();
	/* Ask STEPcode for the symbolic enumeration value.  Generated C++ enum
	 * ordinals are schema-specific (KNOT_TYPE is one concrete example), so
	 * neither asInt() nor a generated enum declaration is a portable value at
	 * this schema-neutral boundary. */
	SDAI_Enum *enumeration = attr->Enum();
	if (!enumeration)
	    return std::string();
	std::string normalized;
	enumeration->asStr(normalized);
	for (size_t i = 0; i < normalized.size(); ++i)
	    normalized[i] = static_cast<char>(std::tolower(
		static_cast<unsigned char>(normalized[i])));
	return normalized;
    }
    return std::string();
}


int
STEPWrapper::getEnumAttributeIndex(SDAI_Application_instance *sse,
    const char *name, const char * const *symbols, size_t symbol_count,
    int fallback)
{
    const std::string value = getEnumAttributeName(sse, name);
    for (size_t i = 0; i < symbol_count; ++i) {
	if (symbols[i] && value == symbols[i])
	    return static_cast<int>(i);
    }
    return fallback;
}


SDAI_Application_instance *
STEPWrapper::getEntityAttribute(SDAI_Application_instance *sse, const char *name)
{
    STEPattribute *attr = step_explicit_attribute(sse, name);
    if (!attr || attr->is_null()) return NULL;
    if (attr->NonRefType() == SELECT_TYPE)
	return step_select_entity(this, attr->Select());
    if (attr->NonRefType() == ENTITY_TYPE)
	return attr->Entity();
    return NULL;
}


Logical
STEPWrapper::getLogicalAttribute(SDAI_Application_instance *sse, const char *name)
{
    Logical retValue = LUnknown;

    sse->ResetAttributes();

    STEPattribute *attr;
    while ((attr = sse->NextAttribute()) != NULL) {
	std::string attrname = attr->Name();

	if (attrname.compare(name) == 0) {
	    retValue = (Logical)(*attr->ptr.e).asInt();
	    if (retValue > LUnknown) {
		retValue = LUnknown;
	    }
	    break;
	}
    }
    return retValue;
}


int
STEPWrapper::getIntegerAttribute(SDAI_Application_instance *sse, const char *name)
{
    int retValue = 0;

    sse->ResetAttributes();

    STEPattribute *attr;
    while ((attr = sse->NextAttribute()) != NULL) {
	std::string attrname = attr->Name();

	if (attrname.compare(name) == 0) {
	    retValue = *attr->ptr.i;
	    break;
	}
    }
    return retValue;
}


double
STEPWrapper::getRealAttribute(SDAI_Application_instance *sse, const char *name)
{
    double retValue = 0.0;

    sse->ResetAttributes();

    STEPattribute *attr;
    while ((attr = sse->NextAttribute()) != NULL) {
	std::string attrname = attr->Name();

	if (attrname.compare(name) == 0) {
	    retValue = *attr->ptr.r;
	    break;
	}
    }
    return retValue;
}


SDAI_Select *
STEPWrapper::getSelectAttribute(SDAI_Application_instance *sse, const char *name)
{
    STEPattribute *attr = step_explicit_attribute(sse, name);
    return attr && attr->NonRefType() == SELECT_TYPE ? attr->Select() : NULL;
}


LIST_OF_ENTITIES *
STEPWrapper::getListOfEntities(SDAI_Application_instance *sse, const char *name)
{
    LIST_OF_ENTITIES *l = new LIST_OF_ENTITIES;
    STEPattribute *attr = step_explicit_attribute(sse, name);
    STEPaggregate *aggregate = attr ? attr->Aggregate() : NULL;
    if (!attr || attr->is_null() || !aggregate) return l;
    if (SelectAggregate *selects = dynamic_cast<SelectAggregate *>(aggregate)) {
	for (SelectNode *node = static_cast<SelectNode *>(selects->GetHead());
		node; node = static_cast<SelectNode *>(node->NextNode())) {
	    SDAI_Application_instance *entity = step_select_entity(this, node->node);
	    if (entity) l->push_back(entity);
	}
	return l;
    }
    if (EntityAggregate *entities = dynamic_cast<EntityAggregate *>(aggregate)) {
	for (EntityNode *node = static_cast<EntityNode *>(entities->GetHead());
		node; node = static_cast<EntityNode *>(node->NextNode()))
	    if (node->node) l->push_back(node->node);
    }
    return l;
}


LIST_OF_SELECTS *
STEPWrapper::getListOfSelects(SDAI_Application_instance *sse, const char *name)
{
    LIST_OF_SELECTS *l = new LIST_OF_SELECTS;
    std::string attrval;

    sse->ResetAttributes();
    STEPattribute *attr;

    while ((attr = sse->NextAttribute()) != NULL) {
	std::string attrname = attr->Name();

	if (attrname.compare(name) == 0) {

	    SelectAggregate *sa = (SelectAggregate *)attr->ptr.a;
	    SelectNode *sn = (SelectNode *)sa->GetHead();
	    while (sn) {
		l->push_back(sn->node);
		sn = (SelectNode *)sn->NextNode();
	    }
	    break;
	}
    }

    return l;
}


LIST_OF_LIST_OF_PATCHES *
STEPWrapper::getListOfListOfPatches(SDAI_Application_instance *sse, const char *attrName)
{
    LIST_OF_LIST_OF_PATCHES *l = new LIST_OF_LIST_OF_PATCHES;

    sse->ResetAttributes();

    STEPattribute *attr;
    while ((attr = sse->NextAttribute()) != NULL) {
	std::string attrval;
	std::string name = attr->Name();

	if (name.compare(attrName) == 0) {
	    ErrorDescriptor errdesc;

	    //std::cout << attr->asStr(attrval) << std::endl;
	    //std::cout << attr->TypeName() << std::endl;


	    GenericAggregate_ptr gp = (GenericAggregate_ptr)attr->ptr.a;

	    STEPnode *sn = (STEPnode *)gp->GetHead();
	    //EntityAggregate *ag = new EntityAggregate();


	    const char *eaStr;

	    LIST_OF_PATCHES *patches;
	    while (sn != NULL) {
		//sn->STEPwrite(std::cout);
		//std::cout << std::endl;
		eaStr = sn->asStr(attrval);
		patches = parseListOfPatchEntities(eaStr);
		l->push_back(patches);
		sn = (STEPnode *)sn->NextNode();
	    }
	    break;
	}
    }

    return l;
}


LIST_OF_LIST_OF_POINTS *
STEPWrapper::getListOfListOfPoints(SDAI_Application_instance *sse, const char *attrName)
{
    LIST_OF_LIST_OF_POINTS *l = new LIST_OF_LIST_OF_POINTS;

    sse->ResetAttributes();

    STEPattribute *attr;
    while ((attr = sse->NextAttribute()) != NULL) {
	std::string attrval;
	std::string name = attr->Name();

	if (name.compare(attrName) == 0) {
	    ErrorDescriptor errdesc;

	    //std::cout << attr->asStr(attrval) << std::endl;
	    //std::cout << attr->TypeName() << std::endl;


	    GenericAggregate_ptr gp = (GenericAggregate_ptr)attr->ptr.a;

	    STEPnode *sn = (STEPnode *)gp->GetHead();
	    //EntityAggregate *ag = new EntityAggregate();


	    const char *eaStr;

	    LIST_OF_POINTS *points;
	    while (sn != NULL) {
		//sn->STEPwrite(std::cout);
		//std::cout << std::endl;
		eaStr = sn->asStr(attrval);
		points = parseListOfPointEntities(eaStr);
		l->push_back(points);
		sn = (STEPnode *)sn->NextNode();
	    }
	    break;
	}
    }

    return l;
}


MAP_OF_SUPERTYPES *
STEPWrapper::getMapOfSuperTypes(SDAI_Application_instance *sse)
{
    MAP_OF_SUPERTYPES *m = new MAP_OF_SUPERTYPES;

    if (sse->IsComplex()) {
	STEPcomplex *sc = ((STEPcomplex *)sse)->head;
	while (sc) {
	    (*m)[sc->EntityName()] = sc;
	    sc = sc->sc;
	}
    }

    return m;
}


void
STEPWrapper::getSuperTypes(SDAI_Application_instance *sse, MAP_OF_SUPERTYPES &m)
{
    if (sse->IsComplex()) {
	STEPcomplex *sc = ((STEPcomplex *)sse)->head;
	while (sc) {
	    m[sc->EntityName()] = sc;
	    sc = sc->sc;
	}
    }
}


SDAI_Application_instance *
STEPWrapper::getSuperType(SDAI_Application_instance *sse, const char *name)
{
    if (sse->IsComplex()) {
	STEPcomplex *sc = ((STEPcomplex *)sse)->head;
	while (sc) {
	    std::string ename = sc->EntityName();

	    if (ename.compare(name) == 0) {
		return sc;
	    }
	    sc = sc->sc;
	}
    }
    return NULL;
}


std::string
STEPWrapper::getStringAttribute(SDAI_Application_instance *sse, const char *name)
{
    std::string retValue = "";

    sse->ResetAttributes();

    STEPattribute *attr;
    while ((attr = sse->NextAttribute()) != NULL) {
	std::string attrval;
	std::string attrname = attr->Name();

	if (attrname.compare(name) == 0) {
	    const char *str = attr->asStr(attrval);
	    if (str != NULL) {
		retValue = str;
	    }
	    break;
	}
    }
    return retValue;
}



// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
