#ifndef ENTITYEXTENT_H
#define ENTITYEXTENT_H 1

#include <sc_export.h>

/*
//#include <sdaiDefs.h>
//#include <ApplInstanceSet.h>
//#include <SessionInstance.h>
//#include <ModelContents.h>
*/

/*
   7.3.3  Entity extent
*/

class SDAI_Entity_extent;
typedef SDAI_Entity_extent * SDAI_Entity_extent_ptr;
typedef SDAI_Entity_extent_ptr SDAI_Entity_extent_var;

class SC_DAI_EXPORT SDAI_Entity_extent : public SDAI_Session_instance {

        friend class SDAI_Model_contents;
        /*
           NOTE - The ModelContent class is a friend so that ModelContent may add
           instances to Entity_extent instances.
         */

    public: //for now
//  protected:
        Entity_ptr _definition ;
        SDAI_Entity_name _definition_name ;
        SDAI_DAObject__set _instances ;   //  of  Application_instance
//    Entity_instance__set _instances ;   //  of  entity_instance
//    EntityAggregate _instances ;    //  of  entity_instance
// Express in part 22 - INVERSE owned_by : sdai_model_contents FOR folders;
        SDAI_Model_contents__list _owned_by ;  //  ADDED in Part 22

//  private:
        SDAI_Entity_extent();
//    SDAI_Entity_extent(const SDAI_Entity_extent& ee);
        ~SDAI_Entity_extent(); // not in part 23

    public:

        SDAI_Entity_name definition_name_() const {
            return _definition_name;
        }

        Entity_ptr definition_() const;
#ifdef SDAI_CPP_LATE_BINDING
//    const Entity_ptr definition_() const;
#endif

        SDAI_DAObject__set_var instances_() {
            return &_instances;
        }
        SDAI_DAObject__set_var instances_() const {
            return ( const SDAI_DAObject__set_var )&_instances;
        }

// need to implement Model_contents__list
        SDAI_Model_contents__list_var owned_by_() const;

//    static SDAI_Entity_extent_ptr
//          _duplicate(SDAI_Entity_extent_ptr eep);
//    static SDAI_Entity_extent_ptr _narrow(Object_ptr op);
//    static SDAI_Entity_extent_ptr _nil();

//  private:
        void definition_( const Entity_ptr & ep );
#ifdef SDAI_CPP_LATE_BINDING
//    void definition_(const Entity_ptr& ep);
#endif
        void definition_name_( const SDAI_Entity_name & ep );
        void owned_by_( SDAI_Model_contents__list_var & mclv );

        /*
           7.3.3.1  SDAI operation declarations

           7.3.3.1.1  AddInstance
         */

        // this is no longer in Part 23
        void AddInstance( const SDAI_DAObject_ptr & appInst );

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
        void RemoveInstance( const SDAI_DAObject_ptr & appInst );

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

};

#endif
