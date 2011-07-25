#ifndef P23ENUM_H
#define	P23ENUM_H 

/*
* NIST STEP Core Class Library
* clstepcore/Enum.h
* April 1997
* K. C. Morris
* David Sauder

* Development of this software was funded by the United States Government,
* and is not subject to copyright.
*/

/* $Id: sdaiEnum.h,v 1.5 1997/11/05 21:59:14 sauderd DP3.1 $ */
#if 0

#ifdef PART26
// Change the name of the include file defining the defs for Bool and Logical
// inside the file corbaIncludes.h
#include <corbaIncludes.h>
#endif

#ifdef __OSTORE__
#include <ostore/ostore.hh>    // Required to access ObjectStore Class Library
#endif

#ifdef __O3DB__
#include <OpenOODB.h>
#endif

#include <sclprefixes.h>

#include <Str.h>
#include <errordesc.h>

#endif

//#define ENUM_NULL -1


//class STEPenumeration  {
class SCLP23_NAME(Enum)  {
    friend     ostream &operator<< ( ostream&, const SCLP23_NAME(Enum)& );
  protected:
    int v;	//  integer value of enumeration instance 
	//  mapped to a symbolic value in the elements

    virtual int set_value (const char * n);
    virtual int set_value (const int n);
    SCLP23_NAME(Enum)();
    virtual ~SCLP23_NAME(Enum)();

  public:

    void PrintContents(ostream &out = cout) const 
	    { DebugDisplay(out); }
    
    virtual int no_elements () const =0;
    virtual const char * Name () const =0;
    const char * get_value_at (int n) const { return element_at (n);  }
    virtual const char * element_at (int n) const =0; 

    Severity EnumValidLevel(const char *value, ErrorDescriptor *err,
			    int optional, char *tokenList,
			    int needDelims = 0, int clearError = 1);

    Severity EnumValidLevel(istream &in, ErrorDescriptor *err, 
			    int optional, char *tokenList,
			    int needDelims = 0, int clearError = 1);

    const int asInt () const {	return v;    }
    
    const char * asStr (std::string& s) const;
    void STEPwrite (ostream& out = cout)  const;
    const char * STEPwrite (std::string& s) const;

    Severity StrToVal (const char * s, ErrorDescriptor *err, int optional = 1);
    Severity STEPread(istream& in, ErrorDescriptor *err, int optional = 1);
    Severity STEPread(const char *s, ErrorDescriptor *err, int optional = 1);

    virtual int put (int val);
    virtual int put (const char * n);
    int is_null () const
		{ return ! (exists()); }
    void set_null() { nullify(); } 
    SCLP23_NAME(Enum)& operator= (const int);
    SCLP23_NAME(Enum)& operator= (const SCLP23_NAME(Enum)&);
        
//    operator ULong() const { return v; } // return the integer equivalent

    virtual int exists() const; // return 0 if unset otherwise return 1
    virtual void nullify(); // change the receiver to an unset status
			// unset is generated to be 1 greater than last element
    void DebugDisplay (ostream& out =cout) const;

#ifdef __OSTORE__
    static os_typespec* get_os_typespec();
    virtual void Access_hook_in(void *object, 
	enum os_access_reason reason, void *user_data, 
	void *start_range, void *end_range);
#endif

  protected:
    virtual Severity ReadEnum(istream& in, ErrorDescriptor *err, 
			      int AssignVal = 1, int needDelims = 1);
};


//struct P23_NAMESPACE {

#ifndef _ODI_OSSG_
class SCLP23_NAME(LOGICAL);
class SCLP23_NAME(BOOLEAN);
#endif

// If NO_BOOLS_LOGS is defined then Boolean and Logical need to be defined 
// elsewhere. Namely they are specified by Part 26 to be defined within the 
// context of CORBA in IDL. That means they need to have code generated for 
// them by the IDL compiler so that they are understood by the compiler in the 
// IDL generated code. Of course that means they can't be defined here. It also
// means that their definition needs to be included here. That is done by 
// changing the file included above as #include <corbaIncludes.h> Go to that
// file and change the name of the file included there.
// If NO_BOOL_LOGS is not defined they will be defined here and must be 
// prefixed with  or  parameterized macro when used. (see 
// clstepcore/sclprefixes.h)
// If you need Logical and Bool to be prefixed with something else because it 
// is defined in another name space:
// 1. define NO_BOOLS_LOGS (so they are not defined here)
// 2. change the definition of the macros defining the parameterized 
//    macros (SCLBOOL, SCLBOOL_H, SCLLOG, and SCLLOG_H) in 
//    clstepcore/sclprefixes.h  
// DAS
/* #ifndef NO_BOOLS_LOGS */
/* enum SCLBOOL_H(Boolean) { SCLBOOL_H(BFalse), SCLBOOL_H(BTrue), SCLBOOL_H(BUnset) }; */
/* enum SCLLOG_H(Logical) { SCLLOG_H(LFalse), SCLLOG_H(LTrue), SCLLOG_H(LUnset), SCLLOG_H(LUnknown) }; */
/* #endif */

enum Boolean { BFalse, BTrue, BUnset };
enum Logical { LFalse, LTrue, LUnset, LUnknown };

// old SCL definition
//enum LOGICAL { sdaiFALSE, sdaiTRUE, sdaiUNKNOWN };

class SCLP23_NAME(LOGICAL)  :
public SCLP23_NAME(Enum)  {
  public:
    const char * Name() const;

    SCLP23_NAME(LOGICAL) (char * val =0);
    SCLP23_NAME(LOGICAL) (Logical state);
    SCLP23_NAME(LOGICAL) (const SCLP23_NAME(LOGICAL)& source);
    SCLP23_NAME(LOGICAL) (int i);

// this is to avoid a bug? in the ossg utility for ObjectStore
#ifndef __OSTORE__
#ifndef _ODI_OSSG_
// this causes an error with sun C++ because SCLP23_NAME(BOOLEAN) is not a completely 
// specified type. (this is supposed to be SDAI?) DAS
//    SCLP23_NAME(LOGICAL) (const SCLP23_NAME(BOOLEAN)& boo);
#endif
#endif
    virtual ~SCLP23_NAME(LOGICAL) ();

    virtual int no_elements () const;
    virtual const char * element_at (int n) const;

//    operator int () const;
    operator Logical () const;
    SCLP23_NAME(LOGICAL)& operator=(const SCLP23_NAME(LOGICAL)& t);

    SCLP23_NAME(LOGICAL) operator==( const SCLP23_NAME(LOGICAL)& t ) const;

    // these 2 are redefined because LUnknown has cardinal value > LUnset
    int exists() const; // return 0 if unset otherwise return 1
    void nullify(); // change the receiver to an unset status

#ifdef __OSTORE__
    static os_typespec* get_os_typespec();
    virtual void Access_hook_in(void *object, 
	enum os_access_reason reason, void *user_data, 
	void *start_range, void *end_range);
#endif

  protected:
    virtual int set_value (const int n);
    virtual int set_value (const char * n);
    virtual Severity ReadEnum(istream& in, ErrorDescriptor *err, 
			      int AssignVal = 1, int needDelims = 1);

}
;

class SCLP23_NAME(BOOLEAN)  :
public SCLP23_NAME(Enum)  {
  public:
    const char * Name() const;

    SCLP23_NAME(BOOLEAN) (char * val = 0);
    SCLP23_NAME(BOOLEAN) (Boolean state);
    SCLP23_NAME(BOOLEAN) (const SCLP23_NAME(BOOLEAN)& source);
    SCLP23_NAME(BOOLEAN) (int i);
    SCLP23_NAME(BOOLEAN) (const SCLP23_NAME(LOGICAL)& val);
    virtual ~SCLP23_NAME(BOOLEAN) ();

    virtual int no_elements () const;
    virtual const char * element_at (int n) const;

    operator Boolean() const;
    SCLP23_NAME(BOOLEAN)& operator=(const SCLP23_NAME(LOGICAL)& t);

    SCLP23_NAME(BOOLEAN)& operator=(const Boolean t);
//    operator int () const;
//    operator Logical () const;
    SCLP23_NAME(LOGICAL) operator==( const SCLP23_NAME(LOGICAL)& t ) const;

#ifdef __OSTORE__
    static os_typespec* get_os_typespec();
    virtual void Access_hook_in(void *object, 
	enum os_access_reason reason, void *user_data, 
	void *start_range, void *end_range);
#endif

}
;

  static const SCLP23_NAME(BOOLEAN) SCLP23_NAME(TRUE);
  static const SCLP23_NAME(BOOLEAN) SCLP23_NAME(FALSE);
  static const SCLP23_NAME(BOOLEAN) SCLP23_NAME(UNSET);
  static const SCLP23_NAME(LOGICAL) SCLP23_NAME(UNKNOWN);

//}; // end struct P23_NAMESPACE

#if 0
// *********** NOTE ****************
// These are now defined in sdai.h

#ifdef __OSTORE__
SCLP23(BOOLEAN) *create_BOOLEAN(os_database *db);
#else
inline SCLP23(BOOLEAN) *create_BOOLEAN() { return new SCLP23(BOOLEAN) ; }
#endif

#ifdef __OSTORE__
SCLP23(LOGICAL) *create_LOGICAL(os_database *db);
#else
inline SCLP23(LOGICAL) *create_LOGICAL() { return new SCLP23(LOGICAL) ; }
#endif

#endif

#endif 
