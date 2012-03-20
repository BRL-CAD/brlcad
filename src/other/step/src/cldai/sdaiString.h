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

class SCLP23_NAME(String) : public std::string {
public:

  //constructor(s) & destructor    
  SCLP23_NAME(String) (const char * str = 0, int max =0) : std::string (str,max)
	{ }
  SCLP23_NAME(String) (const std::string& s)   : std::string (s) { }
  SCLP23_NAME(String) (const SCLP23_NAME(String)& s)  : std::string (s) { }
  ~SCLP23_NAME(String) ()  {  }

//  operators
  SCLP23_NAME(String)& operator= (const char* s);

  // format for STEP
  const char* asStr(std::string& s) const { s = c_str(); return s.c_str();  }
  void STEPwrite (ostream& out =cout)  const;
  void STEPwrite (std::string &s) const;

  Severity StrToVal (const char *s);
  Severity STEPread (istream& in, ErrorDescriptor *err);
  Severity STEPread (const char *s, ErrorDescriptor *err);

};

#endif
