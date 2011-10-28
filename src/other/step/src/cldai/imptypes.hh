//
//  File: imptypes.hh
//
//  Description: Type definitions for SDAI implementation classes
//
// This software was developed by NIST and the Industrial Technology Institute
//

// NOTE: This header file will be included within a namespace. Do not
//       #include any headers that contain class declarations or functions
//       you don't want included in the namespace.
// You should probably only #include sdai.h which #includes this one
//

#ifndef IMPTYPES_HH
#define IMPTYPES_HH

class SCLP23_NAME(Object);
typedef SCLP23_NAME(Object)* SCLP23_NAME(Object_ptr);
typedef SCLP23_NAME(Object_ptr) SCLP23_NAME(Object_var);

class SCLP23_NAME(Session_instance);
typedef SCLP23_NAME(Session_instance)* SCLP23_NAME(Session_instance_ptr);
typedef SCLP23_NAME(Session_instance_ptr) SCLP23_NAME(Session_instance_var);

typedef char * SCLP23_NAME(DAObjectID);

class SCLP23_NAME(PID);
typedef SCLP23_NAME(PID)* SCLP23_NAME(PID_ptr);
typedef SCLP23_NAME(PID_ptr) SCLP23_NAME(PID_var);

class SCLP23_NAME(PID_DA);
typedef SCLP23_NAME(PID_DA)* SCLP23_NAME(PID_DA_ptr);
typedef SCLP23_NAME(PID_DA_ptr) SCLP23_NAME(PID_DA_var);

class SCLP23_NAME(PID_SDAI);
typedef SCLP23_NAME(PID_SDAI)* SCLP23_NAME(PID_SDAI_ptr);
typedef SCLP23_NAME(PID_SDAI_ptr) SCLP23_NAME(PID_SDAI_var);

class SCLP23_NAME(DAObject); 
typedef SCLP23_NAME(DAObject)* SCLP23_NAME(DAObject_ptr);
typedef SCLP23_NAME(DAObject_ptr) SCLP23_NAME(DAObject_var);

class SCLP23_NAME(DAObject__set);
typedef SCLP23_NAME(DAObject__set)* SCLP23_NAME(DAObject__set_ptr);
typedef SCLP23_NAME(DAObject__set_ptr) SCLP23_NAME(DAObject__set_var);

class SCLP23_NAME(DAObject_SDAI);
typedef SCLP23_NAME(DAObject_SDAI)* SCLP23_NAME(DAObject_SDAI_ptr);
typedef SCLP23_NAME(DAObject_SDAI_ptr) SCLP23_NAME(DAObject_SDAI_var);

class SCLP23_NAME(Model_contents_instances);
typedef SCLP23_NAME(Model_contents_instances) *
				SCLP23_NAME(Model_contents_instances_ptr);
typedef SCLP23_NAME(Model_contents_instances_ptr) 
				SCLP23_NAME(Model_contents_instances_var);

class SCLP23_NAME(Model_contents);
typedef SCLP23_NAME(Model_contents) * SCLP23_NAME(Model_contents_ptr);
typedef SCLP23_NAME(Model_contents_ptr) SCLP23_NAME(Model_contents_var);

class SCLP23_NAME(Model_contents__list);
typedef SCLP23_NAME(Model_contents__list)* 
				SCLP23_NAME(Model_contents__list_ptr);
typedef SCLP23_NAME(Model_contents__list_ptr) 
				SCLP23_NAME(Model_contents__list_var);

class SCLP23_NAME(Entity_extent);
typedef SCLP23_NAME(Entity_extent) * SCLP23_NAME(Entity_extent_ptr);
typedef SCLP23_NAME(Entity_extent_ptr) SCLP23_NAME(Entity_extent_var);

class SCLP23_NAME(Entity_extent__set);
typedef SCLP23_NAME(Entity_extent__set)* SCLP23_NAME(Entity_extent__set_ptr);
typedef SCLP23_NAME(Entity_extent__set_ptr) SCLP23_NAME(Entity_extent__set_var);

class SCLP23_NAME(Application_instance);
// current style of CORBA handles for Part 23
typedef SCLP23_NAME(Application_instance)* SCLP23_NAME(Application_instance_ptr);
typedef SCLP23_NAME(Application_instance_ptr) SCLP23_NAME(Application_instance_var);

class SCLP23_NAME(Application_instance__set);
typedef SCLP23_NAME(Application_instance__set) *
				SCLP23_NAME(Application_instance__set_ptr);
typedef SCLP23_NAME(Application_instance__set_ptr) 
				SCLP23_NAME(Application_instance__set_var);

/*
   typedef SCLP23_NAME(Application_instance__list) * 
                                SCLP23_NAME(Application_instance__list_var);
*/
#endif // IMPTYPES_HH
