/////////    ENTITY entity_extent

//#include <EntityExtent.h>

#include <sdai.h>
#include "sc_memmgr.h"

SDAI_Entity_extent::SDAI_Entity_extent( )
    : _definition( 0 ), _definition_name( 0 ), _owned_by( 0 ) {
    /*
        _definition = 0;
        _definition_name = 0;
        _owned_by = 0;
    */
}

/*
SDAI_Entity_extent::SDAI_Entity_extent(const SDAI_Entity_extent& ee)
{
//  {  CopyAs((STEPentityH) &e);    }
    _definition = 0;
    _owned_by = 0;
}
*/

SDAI_Entity_extent::~SDAI_Entity_extent() {
    delete _definition_name;
}

Entity_ptr
SDAI_Entity_extent ::definition_() const {
    return _definition;
}

/*
const SDAI_Entity_name
SDAI_Entity_extent::definition_name_() const
{
    return _definition_name;
}
*/

void
SDAI_Entity_extent::definition_( const Entity_ptr & ep ) {
    _definition = ep;
}

void
SDAI_Entity_extent::definition_name_( const SDAI_Entity_name & en ) {
    _definition_name = new char[strlen( en ) + 1];
    strncpy( _definition_name, en, strlen( en ) + 1 );
}

void SDAI_Entity_extent::owned_by_( SDAI_Model_contents__list_var& mclv ) {
    _owned_by = *mclv;
}

SDAI_Model_contents__list_var SDAI_Entity_extent ::owned_by_() const {
    return ( const SDAI_Model_contents__list_var ) &_owned_by;
}

/*
SDAI_DAObject__set_var instances_()
{
    return &_instances;
}

const
SDAI_DAObject__set_var instances_() const
{
    return (const SDAI_DAObject__set_var)&_instances;
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
SDAI_Entity_extent::AddInstance( const SDAI_DAObject_ptr & appInst ) {
    _instances.Append( appInst );
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
SDAI_Entity_extent::RemoveInstance( const SDAI_DAObject_ptr & appInst ) {
    _instances.Remove( _instances.Index( appInst ) );
}

/////////    END_ENTITY SDAI_Entity_extent
