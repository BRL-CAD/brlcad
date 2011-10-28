#ifndef SDAIMODEL_CONTENTS_H
#define SDAIMODEL_CONTENTS_H 1

//SCLP23_NAME(Model_contents)

/*
   7.3.2  SCLP23_NAME(Model_contents)
*/

/*
//#include <sdaiDefs.h>

// for Application_instance__set
//#include <STEPentity.h>
//#include <ApplInstanceSet.h>

// for Entity_extent__set
//#include <EntityExtent.h>
//#include <EntityExtentSet.h>

// for supertype Session_instance
//#include <SessionInstance.h>
*/

// The class SCLP23_NAME(Model_contents)_instances shall implement convenience functions by 
// SCLP23_NAME(Model_contents) in this part of ISO 10303

class SCLP23_NAME(Model_contents_instances) : public SCLP23_NAME(DAObject) 
{
  public:

    SCLP23_NAME(DAObject__set) _instances;

    SCLP23_NAME(Model_contents_instances)();
    virtual ~SCLP23_NAME(Model_contents_instances)();

    // This function shall return the set of DAObjects contained in 
    // the receiver.
    SCLP23_NAME(DAObject__set_var) contents_() { return &_instances; }
    const SCLP23_NAME(DAObject__set_var) contents_() const 
	{ return (const SCLP23_NAME(DAObject__set_var)) &_instances; };

};

typedef SCLP23_NAME(Model_contents_instances) *
				SCLP23_NAME(Model_contents_instances_ptr);
typedef SCLP23_NAME(Model_contents_instances_ptr) 
				SCLP23_NAME(Model_contents_instances_var);

// Model_contents_ptr def pushed ahead of #include for Entity_extent
//class SCLP23_NAME(Model_contents);
//typedef SCLP23_NAME(Model_contents) SCLP23_NAME(Model_contents_ptr);

class SCLP23_NAME(Model_contents) : public SCLP23_NAME(Session_instance) { 

//friend class SCLP23_NAME(Model);

  /*
     NOTE -   Model is a friend so that Model may access the contents of 
     the Entity_extents folders.

     DATA MEMBERS
   */
  public:
//  protected: // for now
// contains Application_instances (i.e. STEPentities)
    SCLP23_NAME(Model_contents_instances) _instances;

    SCLP23_NAME(Entity_extent__set) _folders;	  // of entity_extent

    SCLP23_NAME(Entity_extent__set) _populated_folders;  // of entity_extent

//  EntityAggregate _instances;	  // of Entity_instance
//  EntityAggregate _folders;     // of entity_extent
//  EntityAggregate _populated_folders;  // of entity_extent

  /*
     Constructor declarations
   */
  public: // for now at least
//  private:
    SCLP23_NAME(Model_contents)();
//    SCLP23_NAME(Model_contents)(const SCLP23_NAME(Model_contents)& mc);
    ~SCLP23_NAME(Model_contents)();
    /*
       Access function declarations
       */
  public:
    SCLP23_NAME(Model_contents_instances_ptr) instances_();
    const SCLP23_NAME(Model_contents_instances_ptr) instances_() const;

    SCLP23_NAME(Entity_extent__set_var) folders_();
    const SCLP23_NAME(Entity_extent__set_var) folders_() const;

    //Boolean TestFolders() const;

    const SCLP23_NAME(Entity_extent__set_var) populated_folders_() const;
    SCLP23_NAME(Entity_extent__set_var) populated_folders_();
    //Boolean TestPopulated_folders() const;

//    static SCLP23_NAME(Model_contents_ptr) 
//				_duplicate(SCLP23_NAME(Model_contents_ptr));
//    static SCLP23_NAME(Model_contents_ptr) _narrow(Object_ptr);
//    static SCLP23_NAME(Model_contents_ptr) _nil();

    SCLP23_NAME(PID_DA_ptr) 
		get_object_pid(const SCLP23_NAME(DAObject_ptr &d)) const;

    SCLP23_NAME(DAObject_ptr) lookup(const SCLP23_NAME(PID_DA_ptr &p)) const;

    /*
       SDAI operation declarations

       7.3.2.1 SDAI operation declarations

       7.3.2.1.1 Add instance
     */
//  private:
  public: // for now at least
    SCLP23_NAME(DAObject_ptr) 
	CreateEntityInstance(const char * Type);

    // until we find out what this should really be in the spec
    void AddInstance(const SCLP23_NAME(DAObject_SDAI_ptr)& appInst);
//    void AddInstance(const Entity_instance_ptr& entityHandle);
    //void AddInstance(EntityInstanceH& entityHandle);
    /* Function:
       The AddInstance function shall add the entity instance entity handle 
       to the instances set of the receiver. This function shall add the 
       entity instance entityHandle to each Entity_extent in the folders 
       attribute of the receiver for which the type or a subtype of the
       type contained in the folder corresponds to the type of entityHandle.
       This function shall add the entity instance entityHandle to each 
       Entity_extent in the populated_folders attribute of the receiver for 
       which the type or a subtype of the type contained in the folder 
       corresponds to the type of entityHandle.

       Possible error indicators:
       sdaiSS_NOPN         // Session not open 
       sdaiRP_NOPN         // Repository not open 
       sdaiMO_NEXS         // SDAI-model does not exist 
       sdaiMX_NRW          // SDAI-model access not read-write 
       sdaiSY_ERR          // Underlying system error

       Origin: Convenience function
     */

    // until we find out what this should really be in the spec
    void RemoveInstance(SCLP23_NAME(DAObject_SDAI_ptr)& appInst);  
//    void RemoveInstance(Entity_instance_ptr& entityHandle);  
    //void RemoveInstance(EntityInstanceH& entityHandle);  
    /* Function
       The RemoveInstance function shall remove the entity instance entity 
       handle from the instances set of the receiver. This function shall 
       remove the entity instance entityHandle from each Entity_extent in 
       the folders attribute of the receiver for which the type or a 
       subtype of the type contained in the folder corresponds to the 
       type of entityHandle. This function shall remove the entity 
       instance entityHandle from each Entity_extent in the 
       populated_folders attribute of the receiver for which the type or 
       a subtype of the type contained in the folder corresponds to the 
       type of entityHandle.

       Possible error indicators
       sdaiSS_NOPN         // Session not open 
       sdaiRP_NOPN         // Repository not open 
       sdaiMO_NEXS         // SDAI-model does not exist 
       sdaiMX_NRW          // SDAI-model access not read-write 
       sdaiSY_ERR          // Underlying system error

       Origin: Convenience function
       */
  public:
    
    /*
       7.3.2.1.3 Get entity extent
     */
#ifdef SDAI_CPP_LATE_BINDING
#if 0 // for now
    Any_var GetEntity_extent(const std::string& entityName);
    const Any_var GetEntity_extent(const std::string& entityName) const;
    Any_var GetEntity_extent(const Entity_ptr& ep);
    const Any_var GetEntity_extent(const Entity_ptr& ep) const;
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

//    void instances (SCLP23_NAME(Model_contents_instances_ptr) x);
//    void folders (Entity_extent__set x);
//    void populated_folders (Entity_extent__set x);

};

#endif
