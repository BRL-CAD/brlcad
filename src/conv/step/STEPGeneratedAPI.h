/*                 S T E P G E N E R A T E D A P I . H
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 */

#ifndef CONV_STEP_STEPGENERATEDAPI_H
#define CONV_STEP_STEPGENERATEDAPI_H

#include "common.h"
#include "STEPString.h"

#include "bu/str.h"

#include <cctype>
#include <string>
#include <vector>

#include <Registry.h>
#include <STEPaggregate.h>
#include <STEPattribute.h>
#include <attrDescriptor.h>
#include <instmgr.h>
#include <sdai.h>
#include <typeDescriptor.h>

namespace brlcad {
namespace step {

/** Find an explicit attribute on either an early- or late-bound instance. */
inline STEPattribute *
Attribute(SDAI_Application_instance *instance, const char *name)
{
    if (!instance || !name) return nullptr;
    instance->ResetAttributes();
    for (STEPattribute *attribute = instance->NextAttribute(); attribute;
	 attribute = instance->NextAttribute()) {
	if (attribute->Name() && BU_STR_EQUAL(attribute->Name(), name))
	    return attribute;
    }
    return nullptr;
}

/** Construct and register a schema entity without depending on a generated
 * C++ entity class.  API v2 late-bound schemas obtain both their concrete
 * descriptor and owned attribute storage from Registry::ObjCreate. */
inline STEPentity *
CreateEntity(Registry *registry, InstMgr *instances, const char *name)
{
    if (!registry || !instances || !name) return nullptr;
    STEPentity *entity = registry->ObjCreate(name);
    if (!entity || isNilSTEPentity(entity)) return nullptr;
    instances->Append(entity, completeSE);
    return entity;
}

inline bool
SetString(STEPentity *entity, const char *name, const char *value)
{
    STEPattribute *attribute = Attribute(entity, name);
    SDAI_String *target = attribute ? attribute->String() : nullptr;
    if (!target) return false;
    const std::string input = value ? value : "";
    /* The older generated setters were called with a mixture of raw strings
     * and already encoded Part 21 tokens.  Preserve encoded tokens while
     * making the descriptor-backed API safe for ordinary UTF-8 input. */
    *target = input.size() >= 2 && input.front() == '\'' && input.back() == '\'' ?
	input : encode_string(input);
    return true;
}

/** Assign a scalar, enumeration, optional marker, or aggregate from an exact
 * Part 21 token through STEPcode's descriptor-backed parser.  Entity
 * references should instead use SetEntity/AddEntity so retained source
 * instance numbers can never leak into newly authored graphs. */
inline bool
SetPart21(STEPentity *entity, const char *name, const std::string &value,
          InstMgr *instances)
{
    STEPattribute *attribute = Attribute(entity, name);
    /* STEPattribute::StrToVal does not recognize '$' for scalar integer,
     * real, or string attributes; that syntax is handled by STEPread.  Use
     * the public null operation directly so optional authored values have
     * the same meaning regardless of their primitive storage type. */
    if (attribute && value == "$")
	return attribute->Nullable() &&
	    attribute->set_null() == SEVERITY_NULL;
    return attribute &&
	attribute->StrToVal(value.c_str(), instances) > SEVERITY_WARNING;
}

inline bool
SetInteger(STEPentity *entity, const char *name, SDAI_Integer value)
{
    STEPattribute *attribute = Attribute(entity, name);
    SDAI_Integer *target = attribute ? attribute->Integer() : nullptr;
    if (!target) return false;
    *target = value;
    return true;
}

inline bool
SetReal(STEPentity *entity, const char *name, SDAI_Real value)
{
    STEPattribute *attribute = Attribute(entity, name);
    if (!attribute) return false;
    SDAI_Real *target = attribute->Real();
    if (!target) target = attribute->Number();
    if (!target) return false;
    *target = value;
    return true;
}

/** Select a named REAL/NUMBER alternative and assign its value without
 * round-tripping through Part 21 text. */
inline bool
SetSelectReal(STEPentity *entity, const char *name, const char *choice_name,
              SDAI_Real value)
{
    STEPattribute *attribute = Attribute(entity, name);
    SDAI_Select *select = attribute ? attribute->Select() : nullptr;
    const TypeDescriptor *domain = attribute && attribute->aDesc ?
	attribute->aDesc->NonRefTypeDescriptor() : nullptr;
    const TypeDescriptor *choice = domain && choice_name ?
	domain->CanBeSet(choice_name) : nullptr;
    return select && select->SetReal(choice, value);
}

inline bool
SetEnum(STEPentity *entity, const char *name, const char *value)
{
    STEPattribute *attribute = Attribute(entity, name);
    if (!attribute) return false;
    SDAI_Enum *target = attribute->Enum();
    if (!target) target = attribute->Logical();
    if (!target) target = attribute->Boolean();
    return target && target->put(value) == 0;
}

inline bool
SetLogical(STEPentity *entity, const char *name, Logical value)
{
    STEPattribute *attribute = Attribute(entity, name);
    SDAI_LOGICAL *target = attribute ? attribute->Logical() : nullptr;
    return target && target->put(static_cast<int>(value)) == 0;
}

inline bool
SetBoolean(STEPentity *entity, const char *name, Boolean value)
{
    STEPattribute *attribute = Attribute(entity, name);
    SDAI_BOOLEAN *target = attribute ? attribute->Boolean() : nullptr;
    return target && target->put(static_cast<int>(value)) == 0;
}

/** Assign either a direct entity attribute or any depth of nested SELECTs
 * ending in a compatible entity. */
inline bool
SetEntity(STEPentity *entity, const char *name,
          SDAI_Application_instance *value)
{
    STEPattribute *attribute = Attribute(entity, name);
    if (!attribute) return false;
    if (attribute->NonRefType() == ENTITY_TYPE) {
	attribute->Entity(value);
	return true;
    }
    SDAI_Select *select = attribute->Select();
    return select && select->SetEntity(value);
}

inline SDAI_Application_instance *
SelectedEntity(const SDAI_Select *select)
{
    if (!select) return nullptr;
    SDAI_Application_instance *value = select->EntityValue();
    if (value) return value;
    return SelectedEntity(select->SelectValue());
}

inline bool
EqualTypeName(const char *left, const char *right)
{
    if (!left || !right) return false;
    while (*left && *right) {
	if (std::toupper(static_cast<unsigned char>(*left)) !=
		std::toupper(static_cast<unsigned char>(*right))) return false;
	++left;
	++right;
    }
    return !*left && !*right;
}

/** Test the active choice at every level of a possibly nested SELECT. */
inline bool
SelectIs(const SDAI_Select *select, const char *type_name)
{
    if (!select || !type_name) return false;
    const TypeDescriptor *type = select->CurrentUnderlyingType();
    if (type && EqualTypeName(type->Name(), type_name)) return true;
    return SelectIs(select->SelectValue(), type_name);
}

inline const SDAI_Real *
SelectedReal(const SDAI_Select *select)
{
    if (!select) return nullptr;
    const SDAI_Real *value = select->RealValue();
    return value ? value : SelectedReal(select->SelectValue());
}

inline const SDAI_Integer *
SelectedInteger(const SDAI_Select *select)
{
    if (!select) return nullptr;
    const SDAI_Integer *value = select->IntegerValue();
    return value ? value : SelectedInteger(select->SelectValue());
}

inline const SDAI_String *
SelectedString(const SDAI_Select *select)
{
    if (!select) return nullptr;
    const SDAI_String *value = select->StringValue();
    return value ? value : SelectedString(select->SelectValue());
}

inline STEPaggregate *
SelectedAggregate(SDAI_Select *select)
{
    if (!select) return nullptr;
    STEPaggregate *value = select->AggregateValue();
    return value ? value : SelectedAggregate(select->SelectValue());
}

inline SDAI_Application_instance *
Entity(STEPentity *entity, const char *name)
{
    STEPattribute *attribute = Attribute(entity, name);
    if (!attribute) return nullptr;
    if (attribute->NonRefType() == ENTITY_TYPE) return attribute->Entity();
    return SelectedEntity(attribute->Select());
}

inline STEPaggregate *
Aggregate(STEPentity *entity, const char *name)
{
    STEPattribute *attribute = Attribute(entity, name);
    return attribute ? attribute->Aggregate() : nullptr;
}

inline bool
AddReal(STEPentity *entity, const char *name, SDAI_Real value)
{
    STEPaggregate *aggregate = Aggregate(entity, name);
    if (!aggregate) return false;
    aggregate->AddNode(new RealNode(value));
    return true;
}

inline bool
AddEntity(STEPentity *entity, const char *name,
	  SDAI_Application_instance *value)
{
    STEPaggregate *aggregate = Aggregate(entity, name);
    if (!aggregate || !value) return false;
    if (SelectAggregate *selects = dynamic_cast<SelectAggregate *>(aggregate)) {
	SelectNode *node = static_cast<SelectNode *>(selects->NewNode());
	if (!node || !node->node || !node->node->SetEntity(value)) {
	    delete node;
	    return false;
	}
	selects->AddNode(node);
	return true;
    }
    aggregate->AddNode(new EntityNode(value));
    return true;
}

inline std::vector<SDAI_Application_instance *>
Entities(STEPaggregate *aggregate)
{
    std::vector<SDAI_Application_instance *> result;
    if (!aggregate) return result;
    if (SelectAggregate *selects = dynamic_cast<SelectAggregate *>(aggregate)) {
	for (SelectNode *node = static_cast<SelectNode *>(selects->GetHead()); node;
	     node = static_cast<SelectNode *>(node->NextNode())) {
	    SDAI_Application_instance *value = SelectedEntity(node->node);
	    if (value) result.push_back(value);
	}
	return result;
    }
    for (EntityNode *node = static_cast<EntityNode *>(aggregate->GetHead()); node;
	 node = static_cast<EntityNode *>(node->NextNode()))
	if (node->node) result.push_back(node->node);
    return result;
}

inline std::vector<SDAI_Application_instance *>
Entities(STEPentity *entity, const char *name)
{
    return Entities(Aggregate(entity, name));
}

inline bool
SetAggregate(STEPentity *entity, const char *name,
             const STEPaggregate &value)
{
    STEPaggregate *target = Aggregate(entity, name);
    if (!target) return false;
    target->ShallowCopy(value);
    return true;
}

} // namespace step
} // namespace brlcad

#endif /* CONV_STEP_STEPGENERATEDAPI_H */
