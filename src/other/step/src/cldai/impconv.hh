//
//  File: impconv.hh
//
//  Description: Macro definitions for type conversions between interface
//               and implementation types.
//
//  Rev:         $Revision: 1.3 $
//  Created:     $Date: 1998/01/13 20:24:39 $
//  Author:      $Author: sauderd $
//
//  Copyright Industrial Technology Institute 1997 -- All Rights Reserved
//
#ifndef IMPCONV_HH
#define IMPCONV_HH

#include <sdai.h>


#ifdef PART26

#define P22toP23Ptr(T, X, Y) \
SCLP23(T) * Y; \
{ \
if ((X)) \
  { \
    Y = (SCLP23(T) *) DEREF((X)); \
  } \
else \
  { \
    Y = SCLP23(T)::_nil(); \
  } \
}

#define P23toP22Ptr(T, X, Y) \
SCLP22(T) * Y; \
{ \
if ((X)) \
  { \
    Y = SCLP22(T)::_duplicate(SCLP22(T)::_narrow((X)->tie_())); \
  } \
else \
  { \
    Y = SCLP22(T)::_nil(); \
  } \
}

/////////////////////////
#define myP23toP22Ptr(T, X, Y) \
SCLP22(T) * Y; \
{ \
if ((X)) \
  { \
    Y = SCLP22(T)::_duplicate(SCLP22(T)::_narrow((X)->tie_())); \
  } \
else \
  { \
    Y = SCLP22(T)::_nil(); \
  } \
}
#if 0
    if(X /*_containing_machine*/ != 0)
    {
	try
	{
	    const char *hostName = CORBA_REF(Orbix).myHost();
	    char markerServer[64];
	    sprintf(markerServer, "%d:%s", X /*_containing_machine*/->STEPfile_id, serverName);

	    cout << "*****" << markerServer << endl;

	    Machine_var x = Machine::_bind((const char *)markerServer,hostName);
	    Machine::_duplicate(x);

	    cout << endl << "x->_refCount(): " << x->_refCount();
	    cout << endl << "STEPfile id inside _containing_machine's get function is: " 
		 << _containing_machine->STEPfile_id << endl;
	    cout << "x's marker name in server's implementation object's attr _containing_machine's get function is: '" 
		 << x->_marker() << "'" << endl << endl;
	    return x;
	}
	catch (CORBA::SystemException &se) {
	    cerr << "Unexpected system exception in _containing_machine's get funct: " << &se;
	    throw;
	}
	catch(...) {
	    cerr << "Caught Unknown Exception in _containing_machine's get funct!" << endl;
	    throw;
	}
    }
    else
	cout << "nil object ref in attr _containing_machine's put funct" << endl;
    return Machine::_nil();
/////////////////////////
#endif

//
// Convert a Part 22 (interface) set into a Part 23 (implementation) set
//
// Note: this macro cannot be used with nested collections.
//
// T is the type of the elements in the set. The straight name of the
// type must be given, i.e., Repository instead of SCLP23(Repository).
//
// X is a variable (not a pointer to) of interface (Part 22) set type
// object. It must be declared before invoking this macro.
//
// Y is a variable of the implementation (Part 23) set type. It is
// declared inside this macro.
//
#define P22toP23Set(T, X, Y) \
SCLP23(T##__set)  Y; \
{ \
  long length = (X).length(); \
 \
  for (long i = 0; i < length_; ++i) \
   { \
      SCLP23(T) * x_el = (SCLP23(T) *) DEREF((&X)[i]); \
 \
      if (y_el != (SCLP22(T) *) NULL) \
        { \
          Y.insert(y_el); \
        } \
      else \
        { \
          reportServerError(serverTypeConversionError); \
        } \
   } \
}

//
// Convert a Part 22 (interface) list into a Part 23 (implementation) list
//
// Note: this macro cannot be used with nested collections.
//
// T is the type of the elements in the list. The straight name of the
// type must be given, i.e., Repository instead of SCLP23(Repository).
//
// X is a variable (not a pointer to) of interface (Part 22) list type 
// object. It must be declared before invoking this macro.
//
// Y is a variable of the implementation (Part 23) set type. It is
// declared inside this macro.
//
#define P22toP23List(T, X, Y) \
SCLP23(T##__list)  Y; \
{ \
  long length = (X).length(); \
 \
  for (long i = 0; i < length_; ++i) \
   { \
      SCLP23(T) * x_el = (SCLP23(T) *) DEREF((&X)[i]); \
 \
      if (y_el != (SCLP22(T) *) NULL) \
        { \
          Y.insert(y_el); \
        } \
      else \
        { \
          reportServerError(serverTypeConversionError); \
        } \
   } \
}

#define P23toP22Set(T, X, Y) \
SCLP22(T##__set) * Y = new SCLP22(T##__set); \
{ \
/*  long length = (X)->cardinality(); */\
/*  os_Cursor<SCLP23(T) *> c(*(X)); */\
  long length = (X)->_rep.cardinality(); \
 \
  Y->length(length); \
  int i = 0; \
 \
/*  for (SCLP23(T) * x_el = c.first(); c.more(); x_el = c.next())*/ \
  for (SCLP23(T) * x_el = X->first(); X->more(); x_el = X->next()) \
   { \
      SCLP22(T) * y_el = SCLP22(T)::_narrow(x_el->tie_()); \
      if (y_el != (SCLP22(T) *) NULL) \
        { \
          (*Y)[i++] = y_el; \
        } \
      else \
        { \
          reportServerError(serverTypeConversionError); \
        } \
   } \
}

#define P23toP22List(T, X, Y) \
SCLP22(T##__list) * Y = new SCLP22(T##__list); \
{ \
  long length = (X)->_rep.cardinality(); \
/*  long length = (X)->cardinality();*/ \
/*  os_Cursor<SCLP23(T) *> c(*(X));*/ \
 \
  Y->length(length); \
  int i = 0; \
 \
/*  for (SCLP23(T) * x_el = c.first(); c.more(); x_el = c.next()) */\
  for (SCLP23(T) * x_el = (X)->first(); (X)->more(); x_el = (X)->next()) \
   { \
      SCLP22(T) * y_el = SCLP22(T)::_narrow(x_el->tie_()); \
      if (y_el != (SCLP22(T) *) NULL) \
        { \
          (*Y)[i++] = y_el; \
        } \
      else \
        { \
          reportServerError(serverTypeConversionError); \
        } \
   } \
}

#else

#endif // PART26

#endif // IMPCONV_HH
