
#ifndef SDAIOBJECT_H
#define SDAIOBJECT_H 1

/*
#include <sclprefixes.h>
*/
/*
   The EXPRESS ENTITY entity_instance from the SDAI_data_type_schema from 
   clause 10 in ISO 10303-22, shall be mapped to the C++ class Entity_instance.
   The class Entity_instance shall be a subtype of the C++ class Object:
*/

class SCLP23_NAME(sdaiObject) 
{
  public:
    SCLP23_NAME(sdaiObject)();
    virtual ~SCLP23_NAME(sdaiObject)();
//    static Object_ptr _duplicate(Object_ptr);
//    static Object_ptr _nil();

#ifdef PART26
//    virtual SCLP26(Application_instance_ptr) create_TIE();
    virtual IDL_Application_instance_ptr create_TIE();
#endif

};

typedef SCLP23_NAME(sdaiObject)* SCLP23_NAME(sdaiObject_ptr);
typedef SCLP23_NAME(sdaiObject_ptr) SCLP23_NAME(sdaiObject_var);

/*
   The class Object shall be accessed through the handle types Object_ptr and 
   Object_var. For handles to the C++ class representing a EXPRESS ENTITY data 
   type A, the following implicit widening operations shall be supported: 
   A_ptr to Object_ptr and A_var to Object_ptr. An implementation need not 
   support implicit widening of A_var to Object_var.

   This binding shall support the following functions:
*/

//void release(Object_ptr obj);

/*
   The release function indicates that the caller will no longer access the 
   reference obj.
*/

//Boolean is_nil(Object_ptr obj);

/*
   The is_nil function shall return true if obj is a nil handle; otherwise it 
   returns false.

   This binding does not specify that all base classes in the application 
   schema be virtual base classes; however, it does require that no multiply 
   inherited derived class contain multiple instantiations of any of its base 
   classes. This requirement applies to all classes, even classes used to 
   instantiate complex instances.
*/

#endif
