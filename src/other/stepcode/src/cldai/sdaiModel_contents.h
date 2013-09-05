#ifndef SDAIMODEL_CONTENTS_H
#define SDAIMODEL_CONTENTS_H 1

#include <sc_export.h>
//SDAI_Model_contents

/*
   7.3.2  SDAI_Model_contents
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

// The class SDAI_Model_contents_instances shall implement convenience functions by
// SDAI_Model_contents in this part of ISO 10303

class SC_DAI_EXPORT SDAI_Model_contents_instances : public SDAI_DAObject {
    public:

        SDAI_DAObject__set _instances;

        SDAI_Model_contents_instances();
        virtual ~SDAI_Model_contents_instances();

        // This function shall return the set of DAObjects contained in
        // the receiver.
        SDAI_DAObject__set_var contents_() {
            return &_instances;
        }
        SDAI_DAObject__set_var contents_() const {
            return ( const SDAI_DAObject__set_var ) &_instances;
        }

};

typedef SDAI_Model_contents_instances *
SDAI_Model_contents_instances_ptr;
typedef SDAI_Model_contents_instances_ptr
SDAI_Model_contents_instances_var;

// Model_contents_ptr def pushed ahead of #include for Entity_extent

class SC_DAI_EXPORT SDAI_Model_contents : public SDAI_Session_instance {

//friend class SDAI_Model;

        /*
           NOTE -   Model is a friend so that Model may access the contents of
           the Entity_extents folders.

           DATA MEMBERS
         */
    public:
        SDAI_Model_contents_instances _instances;

        SDAI_Entity_extent__set _folders;   // of entity_extent

        SDAI_Entity_extent__set _populated_folders; // of entity_extent

        /*
           Constructor declarations
         */
    public: // for now at least
        SDAI_Model_contents();
        ~SDAI_Model_contents();
        /*
           Access function declarations
           */
    public:
        SDAI_Model_contents_instances_ptr instances_();
        SDAI_Model_contents_instances_ptr instances_() const;

        SDAI_Entity_extent__set_var folders_();
        SDAI_Entity_extent__set_var folders_() const;

        SDAI_Entity_extent__set_var populated_folders_() const;
        SDAI_Entity_extent__set_var populated_folders_();

        SDAI_PID_DA_ptr
        get_object_pid( const SDAI_DAObject_ptr & d ) const;

        SDAI_DAObject_ptr lookup( const SDAI_PID_DA_ptr & p ) const;

        /*
           SDAI operation declarations

           7.3.2.1 SDAI operation declarations

           7.3.2.1.1 Add instance
         */
//  private:
    public: // for now at least
        SDAI_DAObject_ptr
        CreateEntityInstance( const char * Type );

        // until we find out what this should really be in the spec
        void AddInstance( const SDAI_DAObject_SDAI_ptr & appInst );
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
        void RemoveInstance( SDAI_DAObject_SDAI_ptr & appInst );
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
        Any_var GetEntity_extent( const std::string & entityName );
        const Any_var GetEntity_extent( const std::string & entityName ) const;
        Any_var GetEntity_extent( const Entity_ptr & ep );
        const Any_var GetEntity_extent( const Entity_ptr & ep ) const;
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

};

#endif
