/*                  S O L I D R E P L I C A . C P P
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 */

#include "STEPWrapper.h"
#include "Factory.h"

#include "CartesianTransformationOperator3D.h"
#include "LocalUnits.h"
#include "SolidReplica.h"

#include <set>

#define CLASSNAME "SolidReplica"
#define ENTITYNAME "Solid_Replica"

string SolidReplica::entityname = Factory::RegisterClass(
    ENTITYNAME, (FactoryMethod)SolidReplica::Create);

namespace {

thread_local std::set<int> loading_replicas;

class ReplicaLoadGuard
{
public:
    explicit ReplicaLoadGuard(int step_id) : id(step_id) {}
    ~ReplicaLoadGuard() { loading_replicas.erase(id); }

private:
    int id;
};

} // namespace

SolidReplica::SolidReplica()
    : parent_solid(NULL), transformation(NULL), converting(false)
{
    step = NULL;
    id = 0;
}

SolidReplica::SolidReplica(STEPWrapper *sw, int step_id)
    : parent_solid(NULL), transformation(NULL), converting(false)
{
    step = sw;
    id = step_id;
}

SolidReplica::~SolidReplica()
{
    parent_solid = NULL;
    transformation = NULL;
}

bool
SolidReplica::Load(STEPWrapper *sw, SDAI_Application_instance *sse)
{
    step = sw;
    id = sse ? sse->STEPfile_id : 0;
    if (!sse || id <= 0) {
	if (id > 0) sw->entity_status[id] = STEP_LOAD_ERROR;
	return false;
    }
    if (!loading_replicas.insert(id).second) {
	if (id > 0) sw->entity_status[id] = STEP_LOAD_ERROR;
	std::cerr << CLASSNAME << ": cyclic 'parent_solid' reference at #" << id << '.' << std::endl;
	return false;
    }
    ReplicaLoadGuard load_guard(id);
    if (!SolidModel::Load(sw, sse)) {
	if (id > 0) sw->entity_status[id] = STEP_LOAD_ERROR;
	return false;
    }

    sse = step->getEntity(sse, ENTITYNAME);
    if (!sse) {
	sw->entity_status[id] = STEP_LOAD_ERROR;
	return false;
    }

    if (!parent_solid) {
	SDAI_Application_instance *entity = step->getEntityAttribute(sse, "parent_solid");
	parent_solid = dynamic_cast<SolidModel *>(Factory::CreateObject(sw, entity));
	if (!parent_solid) {
	    std::cerr << CLASSNAME << ": error loading 'parent_solid' attribute." << std::endl;
	    sw->entity_status[id] = STEP_LOAD_ERROR;
	    return false;
	}
    }

    if (!transformation) {
	SDAI_Application_instance *entity = step->getEntityAttribute(sse, "transformation");
	transformation = dynamic_cast<CartesianTransformationOperator3D *>(
	    Factory::CreateObject(sw, entity));
	if (!transformation) {
	    std::cerr << CLASSNAME << ": error loading 'transformation' attribute." << std::endl;
	    sw->entity_status[id] = STEP_LOAD_ERROR;
	    return false;
	}
    }

    sw->entity_status[id] = STEP_LOADED;
    return true;
}

void
SolidReplica::Print(int level)
{
    TAB(level);
    std::cout << CLASSNAME << ":" << name << "(ID:" << STEPid() << ")" << std::endl;
    TAB(level + 1);
    std::cout << "parent_solid:" << std::endl;
    if (parent_solid) parent_solid->Print(level + 2);
    TAB(level + 1);
    std::cout << "transformation:" << std::endl;
    if (transformation) transformation->Print(level + 2);
    SolidModel::Print(level + 1);
}

STEPEntity *
SolidReplica::GetInstance(STEPWrapper *sw, int step_id)
{
    return new SolidReplica(sw, step_id);
}

STEPEntity *
SolidReplica::Create(STEPWrapper *sw, SDAI_Application_instance *sse)
{
    return STEPEntity::CreateEntity(sw, sse, GetInstance, CLASSNAME);
}

bool
SolidReplica::LoadONBrep(ON_Brep *brep)
{
    if (!brep || !parent_solid || !transformation || converting)
	return false;

    /* EXPRESS declares replicas acyclic, but malformed input must not turn a
     * recursive replica chain into unbounded C++ recursion. */
    class ConversionGuard {
    public:
	explicit ConversionGuard(bool &state) : value(state) { value = true; }
	~ConversionGuard() { value = false; }
    private:
	bool &value;
    } conversion_guard(converting);
    const bool parent_loaded = parent_solid->LoadONBrep(brep);
    if (!parent_loaded)
	return false;

    ON_Xform xform;
    if (!transformation->GetONTransform(xform, LocalUnits::length) ||
	!brep->Transform(xform))
	return false;

    return true;
}

/*
 * Local Variables:
 * tab-width: 8
 * mode: C++
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
