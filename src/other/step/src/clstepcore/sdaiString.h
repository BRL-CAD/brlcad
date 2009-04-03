#ifndef STEPSTRING_H
#define	STEPSTRING_H  1

/*
* NIST STEP Core Class Library
* clstepcore/sdaiString.h
* April 1997
* KC Morris

* Development of this software was funded by the United States Government,
* and is not subject to copyright.
*/

/* $Id: sdaiString.h,v 1.4 1997/11/05 21:59:16 sauderd DP3.1 $ */

/*
#ifdef __OSTORE__
#include <ostore/ostore.hh>    // Required to access ObjectStore Class Library
#endif

#ifdef __O3DB__
#include <OpenOODB.h>
#endif

class ErrorDescriptor;
#include <scl_string.h>
#include <errordesc.h>

#ifndef STRING_DELIM
#define STRING_DELIM '\''
#endif
*/

class SCLP23_NAME(String) : public SCLstring {
public:

  //constructor(s) & destructor    
  SCLP23_NAME(String) (const char * str = 0, int max =0) : SCLstring (str,max)
	{ }
  SCLP23_NAME(String) (const SCLstring& s)   : SCLstring (s) { }
  SCLP23_NAME(String) (const SCLP23_NAME(String)& s)  : SCLstring (s) { }
  ~SCLP23_NAME(String) ()  {  }

//  operators
  SCLP23_NAME(String)& operator= (const char* s);

  // format for STEP
  const char * asStr (SCLstring & s) const  {  return s = chars ();  }
  void STEPwrite (ostream& out =cout)  const;
  void STEPwrite (SCLstring &s) const;

  Severity StrToVal (const char *s);
  Severity STEPread (istream& in, ErrorDescriptor *err);
  Severity STEPread (const char *s, ErrorDescriptor *err);

#ifdef __OSTORE__
    static os_typespec* get_os_typespec();
#endif

};

#endif
