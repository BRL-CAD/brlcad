#ifndef SCLPREFIXES_H
#define	SCLPREFIXES_H

/* DAS
   What I had in mind here is that there could be a different prefix or 
   namespace for some types (like the ones defined in CORBA or some other 
   software package). e.g. the Boolean and Logical enums are problematic 
   with various other software packages. They will be defined by the IDL 
   generated C++. In that case they should not be defined in SCL. In either
   case their SCL uses are always prefixed with macros defined in this file
   to help adapt the code as necessary.
*/

/* **********************
If you define NO_BOOLS_LOGS when you build the SCL then SCL will not define
Bool or Logical C++ enums. You will need to:
 1. Define Bool and Logical yourself somewhere and #include the name of the 
    header file that defines them in a corbaIncludes.h that can be seen by 
    SCL Makefiles (currently found in sdaiOrbix/corbaIncludes.h)
 2. Define SCLBOOL, SCLBOOL_H, SCLLOG, and SCLLOG_H to match their non-SCL
    name space or name prefix.

If you have NO_BOOLS_LOGS turned off (the default) when building SCL
then the definitions for Bool and Logical will be defined in SCL in 
clstepcore/sdaiEnum.h 

You should use the parameterized macro definition of SCLBOOL or SCLLOG below 
when using Bool or Logical.

The macro SCLP23_NAME is used in header files and for constructor/destructor
class function names in corresponding .cc files.

The macro SCLP23 is used everywhere else.

********************* */

 // *note* Currently the IDL generated code is not in a namespace.
//#undef NO_P26_NAMESPACE
//#define NO_P26_NAMESPACE 1

// P stands for Part (yeah, it's just to make it short & unique - hopefully)
// as in Part 23 or Part 26.

#ifdef PART26 // SCL's Part 23 is providing the implementation for the Part 26 
              // Orbix-generated software interface.

// The namespace is specified to be SDAI by Part 26 and Part 23
 // Since Part 23 and Part 26 defined objects are suppose to have the same
 // names and are supposed to be defined in a namespace called SDAI, define
 // the namespaces names for Part 23 and 26 to be different (to avoid conflict)

 // This is the namespace for SCL's Part 23 SDAI.
 #define P23_NAMESPACE P23
 // Whenever you have something like P23_NAMESPACE##_name the preprocessor 
 // does the concatenation first then substitutes things. Thus you get 
 // P23_NAMESPACE_name instead of <whatever P23_NAMESPACE is>_name. If you
 // call P23_NAMESPACE_F() like P23_NAMESPACE_F(_name) you get 
 // whatever P23_NAMESPACE is>_name like you want. The user never has to
 // use this macro. It is used in the definition of macros the user uses like
 // SCLP23() and SCLP23_NAME().
 #define P23_NAMESPACE_F(x) P23##x

 // This was used by the ITI code. It holds the same meaning as P23_NAMESPACE
 #define IMP_NAMESPACE P23_NAMESPACE
 #define IMP_NAMESPACE_F(x) P23_NAMESPACE_F(x)
 // as defined by original ITI code
 // #define IMP_NAMESPACE P26_SDAI

// This is the namespace used by the IDL generated code implementing Part 26.
//
// define NO_P26_NAMESPACE if you are not going to define an encapsulating
// module in the idl. If you define a module named SDAI then you will have to 
// refer to the class for interface PID (e.g.) as SDAI::PID and you will have 
// to refer to the macros as DEF_TIE_SDAI_PID() and TIE_SDAI_PID() instead of
// DEF_TIE_PID() and TIE_PID(). DEF_TIE_IDL_MODULE() and TIE_IDL_MODULE() 
// handle this problem in the code.
// You should define P26_NAMESPACE the same as the name you use for the idl
//  module. Without a module you should define NO_P26_NAMESPACE
//
// DEF_TIE_IDL_MODULE handles the DEF_TIE macro depending on whether a module
// name was used in the idl. e.g. for class/interface named PID
//#define SDAI_DEF_TIE_PID(X) DEF_TIE_IDL_MODULE(PID,X) 
// replaces either of these two stmts in your code depending on whether a 
// module name SDAI was used.
//#define SDAI_DEF_TIE_PID(X) DEF_TIE_PID(X)
//#define SDAI_DEF_TIE_PID(X) DEF_TIE_SDAI_PID(X)
//
// TIE_IDL_MODULE handles the TIE macro depending on whether a module
// name was used in the idl. e.g. for class/interface named PID
//#define _SDAI_TIE_PID_(X) TIE_IDL_MODULE(PID,X)
// replaces either of these two stmts in your code depending on whether a 
// module name SDAI was used.
//#define _SDAI_TIE_PID_(X) TIE_PID(X)
//#define _SDAI_TIE_PID_(X) TIE_SDAI_PID(X)

 #ifdef NO_P26_NAMESPACE
   #define P26_NAMESPACE 
   #define P26_NAMESPACE_F(x) x
   #define DEF_TIE_IDL_MODULE(x,y) DEF_TIE_##x(y)
   #define TIE_IDL_MODULE(x,y) TIE_##x(y)
 #else
   // edit "SDAI" here to match the name of your module
   #define P26_NAMESPACE SDAI
   #define P26_NAMESPACE_F(x) SDAI##x
   #define DEF_TIE_IDL_MODULE(x,y) DEF_TIE_SDAI_##x(y)
   #define TIE_IDL_MODULE(x,y) TIE_SDAI_##x(y)
 #endif

 // Define the namespace that contains the Bool and Logical enum definitions.
 // If PART26 is defined, Bool and Logical should be in the IDL-generated 
 // code (and namespace).
 // *note* Currently the IDL generated code is not in a namespace.
 #define BOOL_LOG_NAMESPACE P26_NAMESPACE
 #define BOOL_LOG_NAMESPACE_F(x) P26_NAMESPACE_F(x)

 // This would be the namespace for your application schema. 
 // This is for the ITI-developed code. It really shouldn't go here - DAS
 #define APP_NAMESPACE P26_Simple_geometry

#else // SCL's Part 23 is not providing the implementation for the Part 26 
              // Orbix-generated software interface.

 // This is the namespace for SCL's Part 23 SDAI.
 #define P23_NAMESPACE SDAI
 #define P23_NAMESPACE_F(x) SDAI##x

 // This was used by the ITI code. It holds the same meaning as P23_NAMESPACE
 #define IMP_NAMESPACE P23_NAMESPACE
 #define IMP_NAMESPACE_F(x) P23_NAMESPACE_F(x)
 // as defined by original ITI code
 // #define IMP_NAMESPACE SDAI

 // This is the namespace used by the IDL generated code implementing Part 26.
 // **Note this should never be used if PART26 is not defined.
 #define P26_NAMESPACE 
 #define P26_NAMESPACE_F(x) x

 // Define the namespace that contains the Bool and Logical enum definitions.
 // NO_BOOLS_LOGS is used to turn off the SCL definitions of Bool and Logical
 #ifndef NO_BOOLS_LOGS
   // Unless otherwise necessary, if not defined by IDL generated code use the
   // SCL namespace definitions.
   #define BOOL_LOG_NAMESPACE P23_NAMESPACE
   #define BOOL_LOG_NAMESPACE_F(x) P23_NAMESPACE_F(x)
 #else 
   // if you turn the definitions off you need to define how they are used.
   #define BOOL_LOG_NAMESPACE 
   #define BOOL_LOG_NAMESPACE_F(x) x
 #endif

 // This would be the namespace for your application schema. 
 // This is for the ITI-developed code. It really shouldn't go here - DAS
 #define APP_NAMESPACE Simple_geometry

#endif


#define CC_NO_NESTED_CLASSES 1
#ifdef CC_NO_NESTED_CLASSES

  // SCLBOOL_H and SCLLOG_H are used in the sdaiEnum.h header file where
  // the C++ enums Bool and Logical are used.

  // Use SCLBOOL_H and SCLLOG_H when Bool and Logical are used inside the 
  // namespace definition i.e. inside SCL header files.
  // This macro should not be relevant to users of SCL (other than to 
  // understand the SCL header files).
  #define SCLBOOL_H(x) BOOL_LOG_NAMESPACE_F(_##x)
  #define SCLLOG_H(x) BOOL_LOG_NAMESPACE_F(_##x)
  // or if you don't want them to use a name prefix
  //#define SCLBOOL_H(x) x
  //#define SCLLOG_H(x) x

  // Use SCLBOOL and SCLLOG in all other files where the C++ enums Bool and 
  // Logical are used. i.e. any uses outside the namespace definition.
  // SCL users would always wrap uses of Bool and Logical enums inside these.
  #define SCLBOOL(x) BOOL_LOG_NAMESPACE_F(_##x)
  #define SCLLOG(x) BOOL_LOG_NAMESPACE_F(_##x)
  // or if you don't want them to use a name prefix
  //#define SCLBOOL(x) x
  //#define SCLLOG(x) x

///////////////////////
  // Use this macro to enclose objects generated from IDL interface types.
  // i.e. Part 26 based generated types.
  // Use this with types that will ONLY ever be Part 26 types. For types
  // that can switch use PART22() macro.
  // Used with the IDL compiler generated C++ - DAS
  #define SCLP26(X) P26_NAMESPACE_F(_##X)

  // Used by ITI code to enclose objects generated from IDL interface types.
  // This is used for function arguments and return types that need to be
  // either the Part 26 version or the Part 23 version depending on whether
  // the Part 23 interface is being used standalone or to implement Orbix
  // implementation objects using the TIE approach. - DAS
  // This one is defined to use the Part 26 defined type for use with Orbix.
  #define SCLP22_NAME(X) P26_NAMESPACE_F(_##X)
  #define SCLP22(X) P26_NAMESPACE_F(_##X)
  // as defined by ITI
  //  #define SCLP22(X) SDAI_##X

  // Use this macro to enclose references to CORBA types.
  // Used with the Orbix base C++ types/classes #included via CORBA.h. 
  // i.e. not used with types/interfaces generated from a users IDL schema.
  // DAS
  #define CORBA_REF(X) CORBA_##X

  // I don't understand why this is necessary - DAS
     // Use this macro to enclose references to CORBA Object and Object_ptr
  #define SCLP22_BASE(X) CORBA_##X


  // SCLP23_NAME is used in header files and for constructor/destructor
  // class function names in corresponding .cc files.
  // Use this macro to enclose declarations of implementation types.
  #define SCLP23_NAME(x) P23_NAMESPACE_F(_##x)

  // SCLP23 is used everywhere else.
  // Use this macro to enclose uses of Part 23 based (implementation) types.
  #define SCLP23(x) P23_NAMESPACE_F(_##x)

      // Use this macro to enclose references to implementation types
      // whose names have already been qualified
      // I don't understand this macros use - DAS
  #define SCLP23_SPACE(x) x

      // INTEGRATION appmacros here...
  #define SCLP22_SIMPLE_GEOMETRY(X) SDAI_Simple_geometry_##X
  #define SCLP23_SIMPLE_GEOMETRY_NAME(x) APP_NAMESPACE##_##x
  #define SCLP23_SIMPLE_GEOMETRY(x) APP_NAMESPACE##_##x
///////////////////////////////////

#else // nested classes are allowed by the compiler
/////////////////////////////////////

  // Use SCLBOOL_H and SCLLOG_H when Bool and Logical are used inside the 
  // namespace definition i.e. inside SCL header files.
  // This macro should not be relevant to users of SCL (other than to 
  // understand the SCL header files).
  #ifndef NO_BOOLS_LOGS
    #define SCLBOOL_H(x) x
    #define SCLLOG_H(x) x
  #else
    #ifdef PART26
      #ifdef NO_P26_NAMESPACE
        #define SCLBOOL_H(x) x
        #define SCLLOG_H(x) x
      #else
        #define SCLBOOL_H(x) BOOL_LOG_NAMESPACE_F(::##x)
        #define SCLLOG_H(x) BOOL_LOG_NAMESPACE_F(::##x)
      #endif
    #else
      #define SCLBOOL_H(x) BOOL_LOG_NAMESPACE_F(::##x)
      #define SCLLOG_H(x) BOOL_LOG_NAMESPACE_F(::##x)
    #endif
  #endif

  // Use SCLBOOL and SCLLOG in all other files where the C++ enums Bool and 
  // Logical are used. i.e. any uses outside the namespace definition.
  // SCL users would always wrap uses of Bool and Logical enums inside these.
  #ifndef NO_BOOLS_LOGS
    #define SCLBOOL(x) BOOL_LOG_NAMESPACE_F(::##x)
    #define SCLLOG(x) BOOL_LOG_NAMESPACE_F(::##x)
  #else
    #ifdef PART26
      #ifdef NO_P26_NAMESPACE
        #define SCLBOOL(x) x
        #define SCLLOG(x) x
      #else
        #define SCLBOOL(x) BOOL_LOG_NAMESPACE_F(::##x)
        #define SCLLOG(x) BOOL_LOG_NAMESPACE_F(::##x)
      #endif
    #else
      #define SCLBOOL(x) BOOL_LOG_NAMESPACE_F(::##x)
      #define SCLLOG(x) BOOL_LOG_NAMESPACE_F(::##x)
    #endif
  #endif

///////////////////////
  // Use this macro to enclose objects generated from IDL interface types.
  // i.e. Part 26 based generated types.
  // Used with the IDL compiler generated C++ - DAS
  #ifdef NO_P26_NAMESPACE
    #define SCLP26(X) X
  #else
    #define SCLP26(X) P26_NAMESPACE_F(::##X)
  #endif

  // Used by ITI code to enclose objects generated from IDL interface types.
  // This is used for function arguments and return types that need to be
  // either the Part 26 version or the Part 23 version depending on whether
  // the Part 23 interface is being used standalone or to implement Orbix
  // implementation objects using the TIE approach. - DAS
  // This one is defined to use the Part 26 defined type for use with Orbix.
  #ifdef NO_P26_NAMESPACE
    #define SCLP22_NAME(X) X
    #define SCLP22(X) X
  #else
    #define SCLP22_NAME(X) P26_NAMESPACE_F(::##X)
    #define SCLP22(X) P26_NAMESPACE_F(::##X)
  #endif
  // as defined by ITI
  //  #define SCLP22(X) SDAI::##X

  // Use this macro to enclose references to CORBA types.
  // Used with the Orbix base C++ types/classes #included via CORBA.h. 
  // i.e. not used with types/interfaces generated from a users IDL schema.
  // DAS
  #define CORBA_REF(X) CORBA::##X

  // I don't understand why this is necessary - DAS
     // Use this macro to enclose references to CORBA Object and Object_ptr
  #define SCLP22_BASE(X) CORBA::##X

  // SCLP23_NAME is used in header files and for constructor/destructor
  // class function names in corresponding .cc files.
  // Use this macro to enclose declarations of implementation types.
  #ifdef PART26
    #define SCLP23_NAME(x) P23_NAMESPACE_F(_imp##x)
//    #define SCLP23_NAME(x) imp##x
  #else
    #define SCLP23_NAME(x) x
  #endif

  // SCLP23 is used everywhere else.
  // Use this macro to enclose uses of Part 23 based (implementation) types.
  #ifdef PART26
    #define SCLP23(x) P23_NAMESPACE_F(_imp##x)
//    #define SCLP23(x) P23_NAMESPACE_F(::##imp##x)
  #else
    #define SCLP23(x) P23_NAMESPACE_F(::##x)
  #endif
  // as defined by ITI
  // #define SCLP23(x) IMP_NAMESPACE_F(::imp##x)

      // Use this macro to enclose references to implementation types
      // whose names have already been qualified
      // I don't understand this macros use - DAS
  #ifdef PART26
    #define SCLP23_SPACE(x) P23_NAMESPACE_F(_imp##x)
//    #define SCLP23_SPACE(x) P23_NAMESPACE_F(::##imp##x)
  #else
    #define SCLP23_SPACE(x) IMP_NAMESPACE_F(::x)
  #endif

      // INTEGRATION appmacros here...
  #define SCLP22_SIMPLE_GEOMETRY(X) SDAI::Simple_geometry::##X
  #define SCLP23_SIMPLE_GEOMETRY_NAME(x) imp##x
  #define SCLP23_SIMPLE_GEOMETRY(x) APP_NAMESPACE::imp##x
/////////////////////////////////////

#endif

#endif
