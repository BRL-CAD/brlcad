#ifndef ENTITYEXTENT_H
#define ENTITYEXTENT_H 1

/*
//#include <sdaiDefs.h>
//#include <ApplInstanceSet.h>
//#include <SessionInstance.h>
//#include <ModelContents.h>
*/

/* 
   7.3.3  Entity extent
*/

class SCLP23_NAME(Entity_extent);
typedef SCLP23_NAME(Entity_extent) * SCLP23_NAME(Entity_extent_ptr);
typedef SCLP23_NAME(Entity_extent_ptr) SCLP23_NAME(Entity_extent_var);

class SCLP23_NAME(Entity_extent) : public SCLP23_NAME(Session_instance) { 

friend class SCLP23_NAME(Model_contents);
    /*
       NOTE - The ModelContent class is a friend so that ModelContent may add 
       instances to Entity_extent instances.
     */

  public: //for now
//  protected:
    Entity_ptr _definition ;
    SCLP23_NAME(Entity_name) _definition_name ;
    SCLP23_NAME(DAObject__set) _instances ;	  //  of  Application_instance
//    Entity_instance__set _instances ;	  //  of  entity_instance
//    EntityAggregate _instances ;	  //  of  entity_instance
// Express in part 22 - INVERSE owned_by : sdai_model_contents FOR folders;
    SCLP23_NAME(Model_contents__list) _owned_by ;  //  ADDED in Part 22

//  private:
    SCLP23_NAME(Entity_extent)();
//    SCLP23_NAME(Entity_extent)(const SCLP23_NAME(Entity_extent)& ee); 
    ~SCLP23_NAME(Entity_extent)(); // not in part 23

  public:

    SCLP23_NAME(Entity_name) definition_name_() const 
			{ return _definition_name; }

    const Entity_ptr definition_() const;
#ifdef SDAI_CPP_LATE_BINDING
//    const Entity_ptr definition_() const;
#endif

    SCLP23_NAME(DAObject__set_var) instances_() { return &_instances; }
    SCLP23_NAME(DAObject__set_var) instances_() const
		{ return (const SCLP23_NAME(DAObject__set_var))&_instances; }

// need to implement Model_contents__list
    const SCLP23_NAME(Model_contents__list_var) owned_by_() const;

//    static SCLP23_NAME(Entity_extent_ptr) 
//			_duplicate(SCLP23_NAME(Entity_extent_ptr) eep);
//    static SCLP23_NAME(Entity_extent_ptr) _narrow(Object_ptr op);
//    static SCLP23_NAME(Entity_extent_ptr) _nil();

//  private:
    void definition_(const Entity_ptr& ep);
#ifdef SDAI_CPP_LATE_BINDING
//    void definition_(const Entity_ptr& ep);
#endif
    void definition_name_(const SCLP23_NAME(Entity_name)& ep);
    void owned_by_(SCLP23_NAME(Model_contents__list_var)& mcp);

    /*
       7.3.3.1  SDAI operation declarations

       7.3.3.1.1  AddInstance
     */

    // this is no longer in Part 23
    void AddInstance(const SCLP23_NAME(DAObject_ptr)& appInst);

    /*
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

    // this is no longer in Part 23
#ifndef __OSTORE__
    void RemoveInstance(const SCLP23_NAME(DAObject_ptr)& appInst);
#endif

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

#ifdef __OSTORE__
    static os_typespec* get_os_typespec();
#endif

};

#endif
