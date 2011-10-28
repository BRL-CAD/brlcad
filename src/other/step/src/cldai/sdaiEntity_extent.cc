/////////	 ENTITY entity_extent 

//#include <EntityExtent.h>

#include <sdai.h>

SCLP23(Entity_extent)::SCLP23_NAME(Entity_extent)( ) 
: _definition(0), _definition_name(0), _owned_by(0)
{
/*
    _definition = 0;
    _definition_name = 0;
    _owned_by = 0;
*/
}

/*
SCLP23(Entity_extent)::SCLP23_NAME(Entity_extent)(const SCLP23(Entity_extent)& ee)
{
//	{  CopyAs((STEPentityH) &e);	}
    _definition = 0;
    _owned_by = 0;
}
*/

SCLP23(Entity_extent)::~SCLP23_NAME(Entity_extent) () 
{
    delete _definition_name;
}

const Entity_ptr 
SCLP23(Entity_extent)::definition_() const
{
    return _definition;
}

/*
const SCLP23(Entity_name) 
SCLP23(Entity_extent)::definition_name_() const
{
    return _definition_name;
}
*/

void 
SCLP23(Entity_extent)::definition_(const Entity_ptr& ep)
{
    _definition = ep;
}

void 
SCLP23(Entity_extent)::definition_name_(const SCLP23(Entity_name)& en)
{
  _definition_name = new char[strlen(en)+1];
  strncpy (_definition_name, en, strlen(en)+1);
}

void 
SCLP23(Entity_extent)::owned_by_(SCLP23_NAME(Model_contents__list_var)& mclv)
{
//    _owned_by = mcp;
}

const SCLP23(Model_contents__list_var) 
SCLP23(Entity_extent)::owned_by_() const
{
    return (const SCLP23(Model_contents__list_var)) &_owned_by;
}

/*
SCLP23(DAObject__set_var) instances_() 
{
    return &_instances;
}

const 
SCLP23(DAObject__set_var) instances_() const
{
    return (const SCLP23(DAObject__set_var))&_instances;
}
*/
    
/*
   7.3.3.1  SDAI operation declarations

   7.3.3.1.1  AddInstance
   Function:
   The AddInstance function shall add the entity instance entity 
   handle to the instances set of the receiver.

   Possible error indicators:
   sdaiSS_NOPN         // Session not open 
   sdaiRP_NOPN         // Repository not open 
   sdaiMO_NEXS         // SDAI-model does not exist 
   sdaiMX_NRW          // SDAI-model access not read-write 
   sdaiSY_ERR          // Underlying system error

   Origin: Convenience function
*/

void 
SCLP23(Entity_extent)::AddInstance(const SCLP23(DAObject_ptr)& appInst)
{
    _instances.Append(appInst);
}

/*
   7.3.3.1.2  RemoveInstance
   Function:
   The RemoveInstance function shall remove the entity instance entity 
   handle from the instances set of the receiver.

   Possible error indicators:
   sdaiSS_NOPN         // Session not open 
   sdaiRP_NOPN         // Repository not open 
   sdaiMO_NEXS         // SDAI-model does not exist 
   sdaiMX_NRW          // SDAI-model access not read-write 
   sdaiSY_ERR          // Underlying system error

   Origin: Convenience function
*/


void 
SCLP23(Entity_extent)::RemoveInstance(const SCLP23(DAObject_ptr)& appInst)
{
    _instances.Remove( _instances.Index(appInst) );
}

/////////	 END_ENTITY SCLP23(Entity_extent) 
