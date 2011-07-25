
#include <sdai.h>

/////////	 SCLP23(Model_contents_instances)

SCLP23(Model_contents_instances)::SCLP23_NAME(Model_contents_instances)( ) 
{
}

/*
SCLP23(Model_contents_instances)::SCLP23_NAME(Model_contents_instances) (const SCLP23(Model_contents_instances)& e ) 
{
}
*/

SCLP23(Model_contents_instances)::~SCLP23_NAME(Model_contents_instances)()
{
}

/////////	 SCLP23(Model_contents) 

SCLP23(Model_contents)::SCLP23_NAME(Model_contents)( ) 
{
}

/*
SCLP23(Model_contents)::SCLP23_NAME(Model_contents) (const SCLP23(Model_contents)& e ) 
{
//    CopyAs((STEPentityH) &e);
}
*/

SCLP23(Model_contents)::~SCLP23_NAME(Model_contents)()
{
}

//    const Entity_instance__set_var instances() const;
    //const SDAIAGGRH(Set, EntityInstanceH) Instances() const;
SCLP23(Model_contents_instances_ptr) 
SCLP23(Model_contents)::instances_()
{
    return &_instances;
}

const SCLP23(Model_contents_instances_ptr) 
SCLP23(Model_contents)::instances_() const
{
    return (const SCLP23(Model_contents_instances_ptr)) &_instances;
}

SCLP23(Entity_extent__set_var) 
SCLP23(Model_contents)::folders_()
{
    return &_folders;
}

const SCLP23(Entity_extent__set_var) 
SCLP23(Model_contents)::folders_() const
{
    return (const SCLP23(Entity_extent__set_var))&_folders;
}

SCLP23(Entity_extent__set_var) 
SCLP23(Model_contents)::populated_folders_() 
{
    return &_populated_folders;
}

const SCLP23(Entity_extent__set_var) 
SCLP23(Model_contents)::populated_folders_() const
{
    return (const SCLP23(Entity_extent__set_var))&_populated_folders;
}

SCLP23(PID_DA_ptr) 
SCLP23(Model_contents)::get_object_pid(const SCLP23(DAObject_ptr &d)) 
				const 
{
    return 0;
}

SCLP23(DAObject_ptr) 
SCLP23(Model_contents)::lookup(const SCLP23(PID_DA_ptr &p)) const
{
    return 0;
}

SCLP23(DAObject_ptr) 
SCLP23(Model_contents)::CreateEntityInstance(const char * Type)
{
    return 0;
}

void 
SCLP23(Model_contents)::AddInstance(const SCLP23(DAObject_SDAI_ptr)& appInst)
{
    _instances.contents_()->Append(appInst);
}

#ifndef __OSTORE__
void 
SCLP23(Model_contents)::RemoveInstance(SCLP23(DAObject_SDAI_ptr)& appInst)
{
    _instances.contents_()->Remove(_instances.contents_()->Index(appInst));
}
#endif


#ifdef SDAI_CPP_LATE_BINDING
#if 0 // for now

Any_var 
SCLP23(Model_contents)::GetEntity_extent(const std::string& entityName)
{
}

const Any_var 
SCLP23(Model_contents)::GetEntity_extent(const std::string& entityName) const
{
}

Any_var 
SCLP23(Model_contents)::GetEntity_extent(const Entity_ptr& ep)
{
}

const Any_var 
SCLP23(Model_contents)::GetEntity_extent(const Entity_ptr& ep) const
{
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
////////	 END SCLP23(Model_contents) 
