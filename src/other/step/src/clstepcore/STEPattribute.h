#ifndef STEPATTRIBUTE_H
#define	STEPATTRIBUTE_H	1

/*
* NIST STEP Core Class Library
* clstepcore/STEPattribute.h
* April 1997
* K. C. Morris
* David Sauder

* Development of this software was funded by the United States Government,
* and is not subject to copyright.
*/

/* $Id: STEPattribute.h,v 3.0.1.8 1997/11/05 21:59:24 sauderd DP3.1 $ */

#include <sclprefixes.h>

#ifdef __OSTORE__
#include <ostore/ostore.hh>    // Required to access ObjectStore Class Library
#endif

#ifdef __O3DB__
#include <OpenOODB.h>
#endif

#include <stdio.h>
#include <errordesc.h>
#include <baseType.h>

#include <sdai.h>

// this is used to set a const int Real_Num_Precision 
// in STEPaggregate.cc and STEPattribute.cc
#define REAL_NUM_PRECISION 15

//typedef unsigned short BOOLEAN;
typedef double real;  

class InstMgr;
class SCLP23(Application_instance);
class STEPaggregate;
class SCLundefined;
//class SCLP23(Enum);
//class SCLP23(Select);
//class SCLP23(Binary);
//class SCLP23(String);

class TypeDescriptor;
class AttrDescriptor;
class EntityDescriptor;

#include <sstream>
//#include <ExpDict.h>

//#define s_String	char *

extern int SetErrOnNull(const char *attrValue, ErrorDescriptor *error);
////////////////////
////////////////////

extern Severity 
CheckRemainingInput(istream &in, ErrorDescriptor *err, 
		    const char *typeName, // used in error message
		    const char *tokenList); // e.g. ",)"

extern SCLP23(Application_instance) *
ReadEntityRef(istream &in, ErrorDescriptor *err, char *tokenList, 
	      InstMgr * instances, int addFileId);

extern SCLP23(Application_instance) *
ReadEntityRef(const char * s, ErrorDescriptor *err, char *tokenList, 
	      InstMgr * instances, int addFileId);

extern Severity 
EntityValidLevel(SCLP23(Application_instance) *se, 
		 const TypeDescriptor *ed, // entity type that entity se needs 
					   // to match. (this must be an
					   // EntityDescriptor)
		 ErrorDescriptor *err);

extern Severity 
EntityValidLevel(const char *attrValue, // string contain entity ref
		 const TypeDescriptor *ed, // entity type that entity in 
					   // attrValue (if it exists) needs 
					   // to match. (this must be an
					   // EntityDescriptor)
		 ErrorDescriptor *err, InstMgr *im, int clearError);

////////////////////
////////////////////

extern SCLP23(Application_instance) *STEPread_reference (const char * s, ErrorDescriptor *err, 
				       InstMgr * instances, int addFileId);
////////////////////

extern int   QuoteInString(istream& in);

extern void  AppendChar(char c, int& index, char *&s, int& sSize);

extern void 
PushPastString (istream& in, std::string& s, ErrorDescriptor *err);

extern void 
PushPastImbedAggr (istream& in, std::string& s, ErrorDescriptor *err);

extern void 
PushPastAggr1Dim(istream& in, std::string& s, ErrorDescriptor *err);

//extern  Severity ValidateEntityType(SCLP23(Application_instance) *se, 
//					const AttrDescriptor *ad, 
//					ErrorDescriptor *error);

class STEPattribute {

    friend ostream &operator<< ( ostream&, STEPattribute& );

// The CenterLine compiler couldn't handle this friend declaration.
// Here is the error
// "../../../src/clstepcore/STEPattribute.h", line 118: internal << AT&T C++ Translator 3.0.2 - ObjectCenter Version 2.1>> error: cannot find friend P23::Application_instance
// We may want to change the next two lines back at some point? DAS
//    friend class SCLP23(Application_instance);
//  protected:
  public:
    ErrorDescriptor _error;
    unsigned int _derive : 1;
    int Derive (unsigned int n =1)  { return _derive =n; }

    STEPattribute *_redefAttr; 
    void RedefiningAttr(STEPattribute *a) { _redefAttr = a; }

  public:

    const AttrDescriptor * aDesc;

    // You know which of these to use based on the return value of
    // NonRefType() - see below. BASE_TYPE is defined in baseType.h
    // This variable points to an appropriate member variable in the entity
    // class in the generated schema class library (the entity class is 
    // inherited from SCLP23(Application_instance))
    union  {
	SCLP23(String) *S;// STRING_TYPE
	SCLP23(Integer) *i;	// INTEGER_TYPE (Integer is a long int)
	SCLP23(Binary) *b;	// BINARY_TYPE
	SCLP23(Real) *r;	// REAL_TYPE and NUMBER_TYPE (Real is a double)
	SCLP23(Application_instance)* *c;	// ENTITY_TYPE
	STEPaggregate *a;	// AGGREGATE_TYPE
	SCLP23(Enum) *e;	// ENUM_TYPE, BOOLEAN_TYPE, and LOGICAL_TYPE
	SCLP23(Select) *sh;	// SELECT_TYPE
	SCLundefined *u;	// UNKNOWN_TYPE
	void *p;
	
	} ptr;

  protected:
    char SkipBadAttr(istream& in, char *StopChars);
    void AddErrorInfo();

  public:

///////////// Read, Write, Assign attr value

    Severity StrToVal(const char *s, InstMgr *instances =0, 
		      int addFileId =0);
    Severity STEPread(istream& in = cin, InstMgr *instances =0, 
		      int addFileId =0, const char * =NULL);

    const char * asStr(std::string&, const char * =0) const;
                      // return the attr value as a string
    void STEPwrite(ostream& out = cout, const char * =0);

    int ShallowCopy(STEPattribute *sa);

    Severity set_null();

////////////// Return info on attr

    int	Nullable() const; // may this attribute be null?
    int	is_null () const; // is this attribute null?
    int 	IsDerived () const  {  return _derive;  }
    STEPattribute *RedefiningAttr() { return _redefAttr; }

    const char * 	Name() const;
    const char *	TypeName() const;
    const BASE_TYPE	Type() const;
    const BASE_TYPE	NonRefType() const;
    const BASE_TYPE	BaseType() const;

    const TypeDescriptor   *ReferentType() const;

    ErrorDescriptor &Error()	{ return _error; }
    void ClearErrorMsg()	{ _error.ClearErrorMsg(); } 

    Severity ValidLevel (const char *attrValue, ErrorDescriptor *error, 
			     InstMgr *im, int clearError = 1);
#ifdef __OSTORE__
    static os_typespec* get_os_typespec();
#endif
  public:

////////////////// Constructors

   STEPattribute (const STEPattribute& a);
   STEPattribute ()  {};

   ~STEPattribute () {}; 

	   //  INTEGER
   STEPattribute (const class AttrDescriptor& d, SCLP23(Integer) *p);
	   //  BINARY
   STEPattribute (const class AttrDescriptor& d, SCLP23(Binary) *p);
	   //  STRING
   STEPattribute (const class AttrDescriptor& d, SCLP23(String) *p);
	   //  REAL & NUMBER
   STEPattribute (const class AttrDescriptor& d, SCLP23(Real) *p);
	   //  ENTITY
   STEPattribute (const class AttrDescriptor& d, SCLP23(Application_instance)* *p);
	   //  AGGREGATE
   STEPattribute (const class AttrDescriptor& d, STEPaggregate *p);
	   //  ENUMERATION  and Logical
   STEPattribute (const class AttrDescriptor& d, SCLP23(Enum) *p);
	   //  SELECT
   STEPattribute (const class AttrDescriptor& d, SCLP23(Select) *p);
	   //  UNDEFINED
   STEPattribute (const class AttrDescriptor& d, SCLundefined *p);

  friend int operator == (STEPattribute &a1, STEPattribute &a2);
};

#endif
