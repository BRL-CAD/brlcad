#ifndef SCLPREFIXES_H
#define SCLPREFIXES_H

/* **********************

The macro SCLP23_NAME is used in header files and for constructor/destructor
class function names in corresponding .cc files.

The macro SCLP23 is used everywhere else.

********************* */

// *note* Currently the IDL generated code is not in a namespace.
//#undef NO_P26_NAMESPACE
//#define NO_P26_NAMESPACE 1

// P stands for Part (yeah, it's just to make it short & unique - hopefully)
// as in Part 23 or Part 26.

// Orbix-generated software interface.

// This is the namespace for SCL's Part 23 SDAI.
#define P23_NAMESPACE SDAI
#define P23_NAMESPACE_F(x) SDAI##x

// This was used by the ITI code. It holds the same meaning as P23_NAMESPACE
#define IMP_NAMESPACE P23_NAMESPACE
#define IMP_NAMESPACE_F(x) P23_NAMESPACE_F(x)
// as defined by original ITI code
// #define IMP_NAMESPACE SDAI

// This would be the namespace for your application schema.
// This is for the ITI-developed code. It really shouldn't go here - DAS
#define APP_NAMESPACE Simple_geometry


#define CC_NO_NESTED_CLASSES 1
#ifdef CC_NO_NESTED_CLASSES

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

// SCLP23_NAME is used in header files and for constructor/destructor
// class function names in corresponding .cc files.
// Use this macro to enclose declarations of implementation types.
#define SCLP23_NAME(x) x

// SCLP23 is used everywhere else.
// Use this macro to enclose uses of Part 23 based (implementation) types.
#define SCLP23(x) P23_NAMESPACE_F(::##x)
// as defined by ITI
// #define SCLP23(x) IMP_NAMESPACE_F(::imp##x)

// Use this macro to enclose references to implementation types
// whose names have already been qualified
// I don't understand this macros use - DAS
#define SCLP23_SPACE(x) IMP_NAMESPACE_F(::x)

// INTEGRATION appmacros here...
#define SCLP22_SIMPLE_GEOMETRY(X) SDAI::Simple_geometry::##X
#define SCLP23_SIMPLE_GEOMETRY_NAME(x) imp##x
#define SCLP23_SIMPLE_GEOMETRY(x) APP_NAMESPACE::imp##x
/////////////////////////////////////

#endif

#endif
