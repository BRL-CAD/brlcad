
#include <sdai.h>
#include "sc_memmgr.h"

/////////    SDAI_Model_contents_instances

SDAI_Model_contents_instances::SDAI_Model_contents_instances( ) {
}

SDAI_Model_contents_instances ::~SDAI_Model_contents_instances() {
}

/////////    SDAI_Model_contents

SDAI_Model_contents::SDAI_Model_contents( ) {
}

SDAI_Model_contents ::~SDAI_Model_contents() {
}

//    const Entity_instance__set_var instances() const;
//const SDAIAGGRH(Set, EntityInstanceH) Instances() const;
SDAI_Model_contents_instances_ptr
SDAI_Model_contents::instances_() {
    return &_instances;
}

SDAI_Model_contents_instances_ptr
SDAI_Model_contents::instances_() const {
    return ( const SDAI_Model_contents_instances_ptr ) &_instances;
}

SDAI_Entity_extent__set_var
SDAI_Model_contents::folders_() {
    return &_folders;
}

SDAI_Entity_extent__set_var
SDAI_Model_contents::folders_() const {
    return ( const SDAI_Entity_extent__set_var )&_folders;
}

SDAI_Entity_extent__set_var
SDAI_Model_contents::populated_folders_() {
    return &_populated_folders;
}

SDAI_Entity_extent__set_var
SDAI_Model_contents::populated_folders_() const {
    return ( const SDAI_Entity_extent__set_var )&_populated_folders;
}

SDAI_PID_DA_ptr SDAI_Model_contents::get_object_pid( const SDAI_DAObject_ptr & d ) const {
    std::cerr << __FILE__ << ":" << __LINE__ << " - SDAI_Model_contents::get_object_pid() unimplemented!" << std::endl;
    (void) d; //unused
    return 0;
}

SDAI_DAObject_ptr SDAI_Model_contents::lookup( const SDAI_PID_DA_ptr & p ) const {
    std::cerr << __FILE__ << ":" << __LINE__ << " - SDAI_Model_contents::lookup() unimplemented!" << std::endl;
    (void) p; //unused
    return 0;
}

SDAI_DAObject_ptr SDAI_Model_contents::CreateEntityInstance( const char * Type ) {
    std::cerr << __FILE__ << ":" << __LINE__ << " - SDAI_Model_contents::CreateEntityInstance() unimplemented!" << std::endl;
    (void) Type; //unused
    return 0;
}

void SDAI_Model_contents::AddInstance( const SDAI_DAObject_SDAI_ptr & appInst ) {
    _instances.contents_()->Append( appInst );
}

void SDAI_Model_contents::RemoveInstance( SDAI_DAObject_SDAI_ptr & appInst ) {
    _instances.contents_()->Remove( _instances.contents_()->Index( appInst ) );
}


#ifdef SDAI_CPP_LATE_BINDING
#if 0 // for now

Any_var
SDAI_Model_contents::GetEntity_extent( const std::string & entityName ) {
}

const Any_var
SDAI_Model_contents::GetEntity_extent( const std::string & entityName ) const {
}

Any_var
SDAI_Model_contents::GetEntity_extent( const Entity_ptr & ep ) {
}

const Any_var
SDAI_Model_contents::GetEntity_extent( const Entity_ptr & ep ) const {
}

#endif
/* Function:
   The GetEntity_extent function shall retrieve an entity folder from
   the folders attribute within the contents attribute of the receiver.
   This folder shall contain all of the instances of a particular type
   and its subtypes within the model. The type may be specified
   indirectly by the entityName parameter or directly by the entity.
   Subsequent access to the returned aggregate shall be restricted to
   read-only functionality.

   Output:
   This function shall return an aggregate handle used as an argument
   for subsequent SDAI function calls. Otherwise, this function shall
   return a null handle.

   Possible error indicators:
   sdaiSS_NOPN         // Session not open
   sdaiRP_NOPN         // Repository not open
   sdaiMO_NEXS         // SDAI-model does not exist
   sdaiED_NDEF         // Entity definition unknown in this model
   sdaiSY_ERR          // Underlying system error

   Origin: Convenience function
   */

#endif
////////     END SDAI_Model_contents
