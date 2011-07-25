#ifndef SCL_OSSCHEMA_H
#define SCL_OSSCHEMA_H

//#define  _ODI_OSSG_

/*
*/
#include <string.h>
#include <sdai.h>

#include <errordesc.h>

//#include <ExpDict.h>
#include <SingleLinkList.h>
#include <STEPattribute.h>
#include <STEPattributeList.h>
//#include <STEPentity.h>
#include <STEPaggregate.h>

#include <gennode.h>
#include <gennodearray.h>
#include <mgrnode.h>
#include <mgrnodearray.h>
#include <instmgr.h>

void scl_dummy () 
{

OS_MARK_SCHEMA_TYPE(ErrorDescriptor)

OS_MARK_SCHEMA_TYPE(std::string)
OS_MARK_SCHEMA_TYPE(SCLP23(String))
OS_MARK_SCHEMA_TYPE(SCLP23(Binary))
OS_MARK_SCHEMA_TYPE(SCLP23(Enum))
OS_MARK_SCHEMA_TYPE(SCLP23(BOOLEAN))
OS_MARK_SCHEMA_TYPE(SCLP23(LOGICAL))

OS_MARK_SCHEMA_TYPE(STEPattribute)
OS_MARK_SCHEMA_TYPE(SingleLinkNode)
OS_MARK_SCHEMA_TYPE(SingleLinkList)
OS_MARK_SCHEMA_TYPE(AttrListNode)
OS_MARK_SCHEMA_TYPE(STEPattributeList)

OS_MARK_SCHEMA_TYPE(SCLP23(sdaiObject))
OS_MARK_SCHEMA_TYPE(SCLP23(PID))
OS_MARK_SCHEMA_TYPE(SCLP23(PID_SDAI))
OS_MARK_SCHEMA_TYPE(SCLP23(PID_DA))
OS_MARK_SCHEMA_TYPE(SCLP23(DAObject))
OS_MARK_SCHEMA_TYPE(SCLP23(DAObject_SDAI))
OS_MARK_SCHEMA_TYPE(SCLP23(Application_instance))

OS_MARK_SCHEMA_TYPE(SCLP23(Select))

OS_MARK_SCHEMA_TYPE(SCLP23(DAObject__set))
OS_MARK_SCHEMA_TYPE(SCLP23(Session_instance))
OS_MARK_SCHEMA_TYPE(SCLP23(Model_contents_instances))
OS_MARK_SCHEMA_TYPE(SCLP23(Model_contents))
OS_MARK_SCHEMA_TYPE(SCLP23(Model_contents__list))
OS_MARK_SCHEMA_TYPE(SCLP23(Entity_extent))
OS_MARK_SCHEMA_TYPE(SCLP23(Entity_extent__set))

OS_MARK_SCHEMA_TYPE(STEPaggregate)
OS_MARK_SCHEMA_TYPE(GenericAggregate)
OS_MARK_SCHEMA_TYPE(EntityAggregate)
OS_MARK_SCHEMA_TYPE(SelectAggregate)
OS_MARK_SCHEMA_TYPE(StringAggregate)
OS_MARK_SCHEMA_TYPE(BinaryAggregate)
OS_MARK_SCHEMA_TYPE(EnumAggregate)
OS_MARK_SCHEMA_TYPE(LOGICALS)
OS_MARK_SCHEMA_TYPE(BOOLEANS)
OS_MARK_SCHEMA_TYPE(RealAggregate)
OS_MARK_SCHEMA_TYPE(IntAggregate)
OS_MARK_SCHEMA_TYPE(STEPnode)
OS_MARK_SCHEMA_TYPE(GenericAggrNode)
OS_MARK_SCHEMA_TYPE(EntityNode)
OS_MARK_SCHEMA_TYPE(SelectNode)
OS_MARK_SCHEMA_TYPE(StringNode)
OS_MARK_SCHEMA_TYPE(BinaryNode)
OS_MARK_SCHEMA_TYPE(EnumNode)
OS_MARK_SCHEMA_TYPE(RealNode)
OS_MARK_SCHEMA_TYPE(IntNode)

/*
//O S_MARK_SCHEMA_TYPE(GenericNode)
//O S_MARK_SCHEMA_TYPE(GenNodeArray)
//O S_MARK_SCHEMA_TYPE(MgrNode)
//O S_MARK_SCHEMA_TYPE(MgrNodeArray)
//O S_MARK_SCHEMA_TYPE(MgrNodeArraySorted)
//O S_MARK_SCHEMA_TYPE(InstMgr)
*/

}

#endif
