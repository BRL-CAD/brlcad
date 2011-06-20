
//#include <Enum.h>
//#include <Enumeration.h>

#include <sdai.h>

/*
* NIST STEP Core Class Library
* clstepcore/Enumeration.cc
* April 1997
* K. C. Morris
* David Sauder

* Development of this software was funded by the United States Government,
* and is not subject to copyright.
*/

/* $Id: sdaiEnum.cc,v 1.5 1997/11/05 21:59:14 sauderd DP3.1 $  */

//static char rcsid[] ="$Id: sdaiEnum.cc,v 1.5 1997/11/05 21:59:14 sauderd DP3.1 $";

#include <sstream>

//#ifndef SCLP23(TRUE)
//#ifndef SCLP23(FALSE)
// Josh L, 3/31/95
// These constants have to be initialized outside the SDAI struct.  They
// are initialized here instead of in the header file in order to avoid
// multiple inclusions when building SCL applications.
/*
const SCLP23(BOOLEAN) SCLP23(TRUE)( BTrue );
const SCLP23(BOOLEAN) SCLP23(FALSE)( BFalse );
const SCLP23(BOOLEAN) SCLP23(UNSET)( BUnset );
const SCLP23(LOGICAL) SCLP23(UNKNOWN)( LUnknown );
*/
//#endif 
//#endif 


///////////////////////////////////////////////////////////////////////////////
// class Logical
///////////////////////////////////////////////////////////////////////////////

#ifdef __OSTORE__
SCLP23(LOGICAL) * create_LOGICAL(os_database *db) 
{
    return new (db, SCLP23(LOGICAL)::get_os_typespec()) SCLP23(LOGICAL);
}
#endif

SCLP23(LOGICAL)::SCLP23_NAME(LOGICAL) (char * val)
{
    set_value (val);
}

SCLP23(LOGICAL)::SCLP23_NAME(LOGICAL) (Logical state)
{
    set_value (state);
}

SCLP23(LOGICAL)::SCLP23_NAME(LOGICAL) (const SCLP23(LOGICAL)& source) 
{
    set_value (source.asInt());
}

SCLP23(LOGICAL)::SCLP23_NAME(LOGICAL) (int i)  
{
    if (i == 0)
      v =  LFalse ;
    else
      v =  LTrue ;
}

/*
SCLP23(LOGICAL)::SCLP23_NAME(LOGICAL) (const BOOLEAN& boo) 
{
    v = boo.asInt();
}
*/

SCLP23(LOGICAL)::~SCLP23_NAME(LOGICAL) () 
{
}

#ifdef __OSTORE__
void 
SCLP23(LOGICAL)::Access_hook_in(void *object, 
				enum os_access_reason reason, void *user_data, 
				void *start_range, void *end_range)
{
    cout << "ObjectStore called LOGICAL::Access_hook_in()" 
      << endl;
}
#endif

const char * 
SCLP23(LOGICAL)::Name() const
{
    return "Logical";
}

int 
SCLP23(LOGICAL)::no_elements () const
{
    return 3;
}

const char * 
SCLP23(LOGICAL)::element_at (int n) const
{
  switch (n)  {
  case  LUnknown :  return "U";
  case  LFalse : return "F";
  case  LTrue : return "T";
  default: return "UNSET";
  }
}

int 
SCLP23(LOGICAL)::exists() const // return 0 if unset otherwise return 1
{
    return !(v == 2);
}

void 
SCLP23(LOGICAL)::nullify() // change the receiver to an unset status
{
    v = 2;
}

#if 0

SCLP23(LOGICAL)::operator int () const  {
 // anything other than BFalse should return 1 according to Part 23
  if (v ==  LFalse ) return 0;
  //#endif
  else return 1;
/*
  switch (v) {
  case LFalse: return 0;
  case LTrue: return 1;

  case LUnknown:
  case LUnset:  // BUnset or anything other than t or f should set error
		// sdaiVA_NSET i.e. value unset
  default: return 1;
}
*/
}

#endif

SCLP23(LOGICAL)::operator  Logical () const  {
  switch (v) {
  case  LFalse : return  LFalse ;
  case  LTrue : return  LTrue ;
  case  LUnknown :  return  LUnknown ;
  case  LUnset :
  default: return  LUnset ;
}}



SCLP23(LOGICAL)& 
SCLP23(LOGICAL)::operator= (const SCLP23(LOGICAL)& t)
{
    set_value (t.asInt());
    return *this;
}

SCLP23(LOGICAL) SCLP23(LOGICAL)::operator ==( const SCLP23(LOGICAL)& t ) const
{
  if( v == t.asInt() )
      return  LTrue ;
  return  LFalse ;
}

int
SCLP23(LOGICAL)::set_value (const int i)  {  
    if (i > no_elements() + 1)  {
	v = 2;
	return v;
   }
    const char *tmp = element_at( i );
    if ( tmp[0] != '\0' )  return (v =i);
    // otherwise 
    cerr << "(OLD Warning:) invalid enumeration value " << i
	<< " for " <<  Name () << "\n";
    DebugDisplay ();
    return  no_elements() + 1 ;
//    return  ENUM_NULL ;
    
}

int
SCLP23(LOGICAL)::set_value (const char * n)  {  
    //  assigns the appropriate value based on n
//    if  ( !n || (!strcmp (n, "")) )  return set_value (ENUM_NULL);
    if  ( !n || (!strcmp (n, "")) ) { nullify(); return asInt(); }
	
    int i =0;
    SCLstring tmp;
    while ((i < (no_elements() + 1))  &&  
	   (strcmp ( (char *)StrToUpper( n, tmp ),  element_at (i)) != 0 ) )
	++i;
    if ( (no_elements() + 1) == i)  //  exhausted all the possible values 
    {
	nullify();
	return v;
//	return set_value (ENUM_NULL);
    }
    v = i;	
    return v;
    
}


Severity 
SCLP23(LOGICAL)::ReadEnum(istream& in, ErrorDescriptor *err, int AssignVal,
			  int needDelims)
{
    if(AssignVal)
	set_null();

    SCLstring str;
    char c;
    char messageBuf[512];
    messageBuf[0] = '\0';

    int validDelimiters = 1;

    in >> ws; // skip white space

    if( in.good() )
    {
	in.get(c);
	if( c == '.' || isalpha(c))
	{
	    if( c == '.' )
	    {
		in.get(c); // push past the delimiter
		// since found a valid delimiter it is now invalid until the 
		//   matching ending delim is found
		validDelimiters = 0;
	    }

	    // look for UPPER
	    if( in.good() && ( isalpha(c) || c == '_' ) )
	    {
		str.Append(c);
		in.get(c);
	    }

	    // look for UPPER or DIGIT
	    while( in.good() && ( isalnum(c) || c == '_' ) )
	    {
		str.Append(c);
		in.get(c);
	    }
	    // if character is not the delimiter unread it
	    if(in.good() && (c != '.') )
		in.putback(c);

	    // a value was read
	    if(str.Length() > 0)
	    {
		int i =0;
		const char *strval = str.chars();
		SCLstring tmp;
		while( (i < (no_elements()+1))  &&  
		       (strcmp( (char *)StrToUpper( strval, tmp ), 
					     element_at (i) ) != 0) )
		    ++i;
		if ( (no_elements()+1) == i)
		{	//  exhausted all the possible values 
		    err->GreaterSeverity(SEVERITY_WARNING);
		    err->AppendToDetailMsg("Invalid Enumeration value.\n");
		    err->AppendToUserMsg("Invalid Enumeration value.\n");
		}
		else
		{
		    if(AssignVal)
			v = i;
		}

		// now also check the delimiter situation
		if(c == '.') // if found ending delimiter
		{
		    // if expecting delim (i.e. validDelimiter == 0)
		    if(!validDelimiters) 
		    {
			validDelimiters = 1; // everything is fine
		    }
		    else // found ending delimiter but no initial delimiter
		    {
			validDelimiters = 0;
		    }
		}
		// didn't find any delimiters at all and need them.
		else if(needDelims) 
		{
		    validDelimiters = 0;
		}

		if (!validDelimiters)
		{	
		    err->GreaterSeverity(SEVERITY_WARNING);
		    if(needDelims)
			sprintf(messageBuf, 
			  "Enumerated value has invalid period delimiters.\n");
		    else
			sprintf(messageBuf, 
			   "Mismatched period delimiters for enumeration.\n");
		    err->AppendToDetailMsg(messageBuf);
		    err->AppendToUserMsg(messageBuf);
		}
		return err->severity();
	    }
	    // found valid or invalid delimiters with no associated value 
	    else if( (c == '.') || !validDelimiters)
	    {
		err->GreaterSeverity(SEVERITY_WARNING);
		err->AppendToDetailMsg(
	   "Enumerated has valid or invalid period delimiters with no value.\n"
				      );
		err->AppendToUserMsg(
	   "Enumerated has valid or invalid period delimiters with no value.\n"
				      );
		return err->severity();
	    }
	    else // no delims and no value
		err->GreaterSeverity(SEVERITY_INCOMPLETE);

	}
	else if( (c == ',') || (c == ')') )
	{
	    in.putback(c);
	    err->GreaterSeverity(SEVERITY_INCOMPLETE);
	}
	else
	{
	    in.putback(c);
	    err->GreaterSeverity(SEVERITY_WARNING);
	    sprintf(messageBuf, "Invalid enumeration value.\n");
	    err->AppendToDetailMsg(messageBuf);
	    err->AppendToUserMsg(messageBuf);
	}
    }
    else // hit eof (assuming there was no error state for istream passed in)
    {
	err->GreaterSeverity(SEVERITY_INCOMPLETE);
    }
    return err->severity();
}

///////////////////////////////////////////////////////////////////////////////
// class BOOLEAN  Jan 97
///////////////////////////////////////////////////////////////////////////////

#ifdef __OSTORE__
SCLP23(BOOLEAN) * 
create_BOOLEAN(os_database *db) 
{
    return new (db, SCLP23(BOOLEAN)::get_os_typespec()) SCLP23(BOOLEAN); 
}
#endif

const char * 
SCLP23(BOOLEAN)::Name() const
{
    return "Bool";
}

SCLP23(BOOLEAN)::SCLP23_NAME(BOOLEAN) (char * val)
{
    set_value (val);
}

SCLP23(BOOLEAN)::SCLP23_NAME(BOOLEAN) (Boolean state)
{
    set_value (state);
}

SCLP23(BOOLEAN)::SCLP23_NAME(BOOLEAN) (const SCLP23_NAME(BOOLEAN)& source)
{
    set_value (source.asInt());
}

SCLP23(BOOLEAN)::~SCLP23_NAME(BOOLEAN)()
{
}

int 
SCLP23(BOOLEAN)::no_elements () const
{
    return 2;
}

SCLP23(BOOLEAN)::SCLP23_NAME(BOOLEAN) (int i)
{
  if (i == 0)
    v =  BFalse ;
  else
    v =  BTrue ;
}

SCLP23(BOOLEAN)::SCLP23_NAME(BOOLEAN) (const SCLP23(LOGICAL)& val)  {
  if (val.asInt() == LUnknown) 
  { // this should set error code sdaiVT_NVLD i.e. Invalid value type.
      v = BUnset; return; 
  }
  set_value (val);
}

#ifdef __OSTORE__
void 
SCLP23(BOOLEAN)::Access_hook_in(void *object, 
				enum os_access_reason reason, void *user_data, 
				void *start_range, void *end_range)
{
    cout << "ObjectStore called BOOLEAN::Access_hook_in()" 
      << endl;
}
#endif

SCLP23(BOOLEAN)::operator  Boolean () const  {
  switch (v) {
  case  BFalse : return  BFalse ;
  case  BTrue : return  BTrue ;
  case  BUnset :
  default: return  BUnset ;
}}

SCLP23(BOOLEAN)& 
SCLP23(BOOLEAN)::operator= (const SCLP23(LOGICAL)& t)
{
    set_value (t.asInt());
    return *this;
}

SCLP23(BOOLEAN)& 
SCLP23(BOOLEAN)::operator= (const  Boolean t)
{
    v = t;
    return *this;
}

#if 0

SCLP23(BOOLEAN)::operator int () const  {
 // anything other than BFalse should return 1 according to Part 23
  switch (v) {
  case  BFalse : return 0;
  case  BTrue : return 1;
  case  BUnset :  // BUnset or anything other than t or f should set 
		// error sdaiVA_NSET i.e. value unset
  default: return 1;
}}

SCLP23(BOOLEAN)::operator  Logical () const  {
  switch (v) {
  case  BFalse : return  LFalse ;
  case  BTrue : return  LTrue ;
  case  BUnset : return  LUnset ;
  default: return  LUnknown ;
}}

#endif

const char * 
SCLP23(BOOLEAN)::element_at (int n)  const {
  switch (n)  {
  case  BFalse : return "F";
  case  BTrue : return "T";
  default: return "UNSET";
  }
}

SCLP23(LOGICAL) SCLP23(BOOLEAN)::operator ==( const SCLP23(LOGICAL)& t ) const
{
  if( v == t.asInt() )
      return  LTrue ;
  return  LFalse ;
}

///////////////////////////////////////////////////////////////////////////////

SCLP23(Enum)::SCLP23_NAME(Enum)()
{
    v = 0;
}

SCLP23(Enum)::~SCLP23_NAME(Enum)()
{ 
}

#ifdef __OSTORE__
void 
SCLP23(Enum)::Access_hook_in(void *object, 
				enum os_access_reason reason, void *user_data, 
				void *start_range, void *end_range)
{
    cout << "ObjectStore called Enum::Access_hook_in()" 
      << endl;
}
#endif

int 
SCLP23(Enum)::put(int val)
{
    return set_value (val);
}

int 
SCLP23(Enum)::put(const char * n)
{
    return set_value (n);
}

int 
SCLP23(Enum)::exists() const // return 0 if unset otherwise return 1
{
    return ! (v > no_elements());
}

void 
SCLP23(Enum)::nullify() // change the receiver to an unset status
		// unset is generated to be 1 greater than last element
{
    set_value (no_elements()+1);
}

/******************************************************************
 ** Procedure:  DebugDisplay
 ** Parameters:  ostream& out
 ** Returns:  
 ** Description:  prints out some information on the enumerated 
 **               item for debugging purposes
 ** Side Effects:  
 ** Status:  ok 2/1/91
 ******************************************************************/
void
SCLP23(Enum)::DebugDisplay (ostream& out) const 
{
    SCLstring tmp;
    out << "Current " << Name() << " value: " << endl 
	<< "  cardinal: " <<  v  << endl 
	<< "  string: " << asStr(tmp) << endl
	<< "  Part 21 file format: ";
    STEPwrite (out);
    out << endl;

    out << "Valid values are: " << endl;
    int i =0;
    while (i < (no_elements() + 1))
      {
	  out << i << " " << element_at (i) << endl;
	  i++;
      }
    out << "\n";
}	

// Read an Enumeration value 
// ENUMERATION = "." UPPER { UPPER | DIGIT } "."
// *note* UPPER is defined as alpha or underscore.
// returns: Severity of the error.
// error message and error Severity is written to ErrorDescriptor *err.
// int AssignVal is: 
// true => value is assigned to the SCLP23(Enum); 
// true or false => value is read and appropriate error info is set and 
// 	returned.
// int needDelims is: 
// false => absence of the period delimiters is not an error; 
// true => delimiters must be valid; 
// true or false => non-matching delimiters are flagged as an error

Severity 
SCLP23(Enum)::ReadEnum(istream& in, ErrorDescriptor *err, int AssignVal,
			  int needDelims)
{
    if(AssignVal)
	set_null();

    SCLstring str;
    char c;
    char messageBuf[512];
    messageBuf[0] = '\0';

    int validDelimiters = 1;

    in >> ws; // skip white space

    if( in.good() )
    {
	in.get(c);
	if( c == '.' || isalpha(c))
	{
	    if( c == '.' )
	    {
		in.get(c); // push past the delimiter
		// since found a valid delimiter it is now invalid until the 
		//   matching ending delim is found
		validDelimiters = 0;
	    }

	    // look for UPPER
	    if( in.good() && ( isalpha(c) || c == '_' ) )
	    {
		str.Append(c);
		in.get(c);
	    }

	    // look for UPPER or DIGIT
	    while( in.good() && ( isalnum(c) || c == '_' ) )
	    {
		str.Append(c);
		in.get(c);
	    }
	    // if character is not the delimiter unread it
	    if(in.good() && (c != '.') )
		in.putback(c);

	    // a value was read
	    if(str.Length() > 0)
	    {
		int i =0;
		const char *strval = str.chars();
		SCLstring tmp;
		while( (i < no_elements ())  &&  
		       (strcmp( (char *)StrToUpper( strval, tmp ), 
					     element_at (i) ) != 0) )
		    ++i;
		if ( no_elements () == i)
		{	//  exhausted all the possible values 
		    err->GreaterSeverity(SEVERITY_WARNING);
		    err->AppendToDetailMsg("Invalid Enumeration value.\n");
		    err->AppendToUserMsg("Invalid Enumeration value.\n");
		}
		else
		{
		    if(AssignVal)
			v = i;
		}

		// now also check the delimiter situation
		if(c == '.') // if found ending delimiter
		{
		    // if expecting delim (i.e. validDelimiter == 0)
		    if(!validDelimiters) 
		    {
			validDelimiters = 1; // everything is fine
		    }
		    else // found ending delimiter but no initial delimiter
		    {
			validDelimiters = 0;
		    }
		}
		// didn't find any delimiters at all and need them.
		else if(needDelims) 
		{
		    validDelimiters = 0;
		}

		if (!validDelimiters)
		{	
		    err->GreaterSeverity(SEVERITY_WARNING);
		    if(needDelims)
			sprintf(messageBuf, 
			  "Enumerated value has invalid period delimiters.\n");
		    else
			sprintf(messageBuf, 
			   "Mismatched period delimiters for enumeration.\n");
		    err->AppendToDetailMsg(messageBuf);
		    err->AppendToUserMsg(messageBuf);
		}
		return err->severity();
	    }
	    // found valid or invalid delimiters with no associated value 
	    else if( (c == '.') || !validDelimiters)
	    {
		err->GreaterSeverity(SEVERITY_WARNING);
		err->AppendToDetailMsg(
	   "Enumerated has valid or invalid period delimiters with no value.\n"
				      );
		err->AppendToUserMsg(
	   "Enumerated has valid or invalid period delimiters with no value.\n"
				      );
		return err->severity();
	    }
	    else // no delims and no value
		err->GreaterSeverity(SEVERITY_INCOMPLETE);

	}
	else if( (c == ',') || (c == ')') )
	{
	    in.putback(c);
	    err->GreaterSeverity(SEVERITY_INCOMPLETE);
	}
	else
	{
	    in.putback(c);
	    err->GreaterSeverity(SEVERITY_WARNING);
	    sprintf(messageBuf, "Invalid enumeration value.\n");
	    err->AppendToDetailMsg(messageBuf);
	    err->AppendToUserMsg(messageBuf);
	}
    }
    else // hit eof (assuming there was no error state for istream passed in)
    {
	err->GreaterSeverity(SEVERITY_INCOMPLETE);
    }
    return err->severity();
}

/*
Severity 
SCLP23(Enum)::StrToVal (const char * s)
{
    put (s);
    return SEVERITY_NULL;
}
*/

Severity 
SCLP23(Enum)::StrToVal (const char * s, ErrorDescriptor *err, int optional)
{
    istringstream in ((char *)s); // sz defaults to length of s

    Severity sev = ReadEnum(in, err, 1, 0);
    if( (err->severity() == SEVERITY_INCOMPLETE) && optional)
	err->severity(SEVERITY_NULL);

    return err->severity();
}

// reads an enumerated value in STEP file format 
Severity
SCLP23(Enum)::STEPread (const char *s, ErrorDescriptor *err, int optional)
{
    istringstream in((char *)s);
    return STEPread (in, err, optional);
}

// reads an enumerated value in STEP file format 
Severity
SCLP23(Enum)::STEPread (istream& in, ErrorDescriptor *err, int optional)
{
    Severity sev = ReadEnum(in, err, 1, 1);
    if( (err->severity() == SEVERITY_INCOMPLETE) && optional)
	err->severity(SEVERITY_NULL);

    return err->severity();
}


const char * 
SCLP23(Enum)::asStr (SCLstring &s) const  {
//    if (v != ENUM_NULL) 
    if (exists()) 
    {
//	s = elements[v];
	return s = element_at (v);
//	return s.chars();
    }
    else return "";
}

void 
SCLP23(Enum)::STEPwrite (ostream& out)  const  {
    if( is_null() )
	out << '$';
    else
    {
	SCLstring tmp;
	out << "." <<  asStr (tmp) << ".";
    }
}

const char * 
SCLP23(Enum)::STEPwrite (SCLstring &s) const
{
    if( is_null() )
    {
	s.set_null();
    }
    else
    {
	SCLstring tmp;
	s = ".";
	s.Append(asStr(tmp));
	s.Append('.');
    }
    return s.chars();
}

//SCLP23(Enum)::SCLP23_NAME(Enum) (const char * const e)
//:  elements (e)
//{  
//}

/******************************************************************
 ** Procedure:  set_elements
 ** Parameters:  
 ** Returns:  
 ** Description:  
 ** Side Effects:  
 ** Status:  
 ******************************************************************/
#ifdef OBSOLETE
void
SCLP23(Enum)::set_elements (const char * const e [])  {
    elements = e;
}
#endif
Severity 
SCLP23(Enum)::EnumValidLevel(istream &in, ErrorDescriptor *err,
				int optional, char *tokenList, 
				int needDelims, int clearError)
{
    if(clearError)
	err->ClearErrorMsg();

    in >> ws; // skip white space
    char c = ' '; 
    c = in.peek();
    if(c == '$' || in.eof())
    {
	if(!optional)
	    err->GreaterSeverity(SEVERITY_INCOMPLETE);
	if(in)
	    in >> c;
	CheckRemainingInput(in, err, "enumeration", tokenList);
	return err->severity();
    }
    else
    {
	ErrorDescriptor error;

	ReadEnum(in, &error, 0, needDelims);
	CheckRemainingInput(in, &error, "enumeration", tokenList);

	Severity sev = error.severity();
	if(sev < SEVERITY_INCOMPLETE)
	{
	    err->AppendToDetailMsg(error.DetailMsg());
	    err->AppendToUserMsg(error.UserMsg());
	    err->GreaterSeverity(error.severity());
	}
	else if(sev == SEVERITY_INCOMPLETE && !optional)
	    err->GreaterSeverity(SEVERITY_INCOMPLETE);
    }
    return err->severity();
}

Severity 
SCLP23(Enum)::EnumValidLevel(const char *value, ErrorDescriptor *err,
				int optional, char *tokenList, 
				int needDelims, int clearError)
{
    istringstream in((char *)value);
    return EnumValidLevel (in, err, optional, tokenList, needDelims,
			   clearError);
/*

    char messageBuf[BUFSIZ];
    messageBuf[0] = '\0';

    if(attrValue)
    {
	int len = strlen (attrValue);
	char *valstart = new char [len + 1];
	char *val = valstart;

	int numFound = sscanf(attrValue," %s", val);
	if(numFound != EOF)
	{
	    int i = 0;
	    if(val [0] == '.')  // strip the delims
	    {

		val++;
		char * pos = strchr(val, '.');
		if (pos) 
		    *pos = '\0';
		else
		{
		    err->AppendToDetailMsg(
		    "Missing ending period delimiter for enumerated value.\n");
		    err->AppendToUserMsg(
		    "Missing ending period delimiter for enumerated value.\n");
		    err->GreaterSeverity(SEVERITY_WARNING);
		}
	    }

	    SCLstring tmp;
	    while((i < no_elements() ) && 
	    (strcmp( (char *)StrToUpper(val, tmp), element_at (i) ) != 0))
		++i;
	    if(no_elements() == i)	// exhausted all the possible values 
	    {
		err->GreaterSeverity(SEVERITY_WARNING);
		sprintf(messageBuf, 
			"attribute %s: Invalid enumeration value: '%s'",
			Name(), val);
		err->AppendToUserMsg(messageBuf);
		err->AppendToDetailMsg(messageBuf);
//		DebugDisplay ();
		return SEVERITY_WARNING;
	    }
	    err->GreaterSeverity(SEVERITY_NULL);
	    return SEVERITY_NULL;
	}
	delete [] valstart;
    }
    if(optional) 
    {
	err->GreaterSeverity(SEVERITY_NULL);
	return SEVERITY_NULL;
    }
    else
    {
	err->GreaterSeverity(SEVERITY_INCOMPLETE);
	return SEVERITY_INCOMPLETE;
    }
*/
}

/******************************************************************
 ** Procedure:  set_value
 ** Parameters:  char * n  OR  in i  -- value to be set
 ** Returns:  value set 
 ** Description:  sets the value of an enumerated attribute
 **     case is not important in the character based version
 **     if value is not acceptable, a warning is printed and 
 **     processing continues
 ** Side Effects:  
 ** Status:  ok 2.91
 ******************************************************************/
int
SCLP23(Enum)::set_value (const char * n)  {  
    //  assigns the appropriate value based on n
//    if  ( !n || (!strcmp (n, "")) )  return set_value (ENUM_NULL);
    if  ( !n || (!strcmp (n, "")) ) { nullify(); return asInt(); }
	
    int i =0;
    SCLstring tmp;
    while ((i < no_elements ())  &&  
	   (strcmp ( (char *)StrToUpper( n, tmp ),  element_at (i)) != 0 ) )
	++i;
    if ( no_elements () == i)  {  //  exhausted all the possible values 
	return v = no_elements() + 1; // defined as UNSET
//	return set_value (ENUM_NULL);
    }
    v = i;	
    return v;
    
}

//  set_value is the same function as put
int
SCLP23(Enum)::set_value (const int i)  {  
/*
    if (i == ENUM_NULL)  {
	v = ENUM_NULL;
	return ENUM_NULL;
    }
*/
    if (i > no_elements())  {
	v = no_elements() + 1;
	return v;
    }
    const char *tmp = element_at( i );
    if ( tmp[0] != '\0' )  return (v =i);
    // otherwise 
    cerr << "(OLD Warning:) invalid enumeration value " << i
	<< " for " <<  Name () << "\n";
    DebugDisplay ();
    return  no_elements() + 1 ;
//    return  ENUM_NULL ;
    
}

SCLP23(Enum)& 
SCLP23(Enum)::operator= (const int i)
{
    put (i);
    return *this;
}

SCLP23(Enum)&
SCLP23(Enum)::operator= (const SCLP23(Enum)& Senum)
{
    put (Senum.asInt());
    return *this;
}

ostream &operator<< ( ostream& out, const SCLP23(Enum)& a )
{
    SCLstring tmp;
    out << a.asStr( tmp );
    return out;

}


#ifdef pojoldStrToValNstepRead

Severity 
SCLP23(Enum)::StrToVal (const char * s, ErrorDescriptor *err, int optional)
{
    const char *sPtr = s;
    while(isspace(*sPtr)) sPtr++;
    if(*sPtr == '\0')
    {
	if(optional) 
	{
	    err->GreaterSeverity(SEVERITY_NULL);
	    return SEVERITY_NULL;
	}
	else
	{
	    err->GreaterSeverity(SEVERITY_INCOMPLETE);
	    return SEVERITY_INCOMPLETE;
	}
    }
    else if(*sPtr == '.') // look for initial period delimiter
    {
	return STEPread(sPtr, err);
    }
    else
    {
		// look for ending period delimiter (an error)
	char *periodLoc = strchr(sPtr, '.');
	if (periodLoc)
	{	// found an ending period w/out initial period
	    char *tmp = new char[strlen(sPtr) + 1];
	    strcpy(tmp, sPtr);
	    tmp[periodLoc - sPtr] = '\0'; // write over ending period
	    err->GreaterSeverity(SEVERITY_WARNING);
	    err->AppendToDetailMsg(
		"Ending period delimiter without initial period delimiter.\n");
	    err->AppendToUserMsg(
		"Ending period delimiter without initial period delimiter.\n");
	    delete [] tmp;
	    if( ValidLevel(sPtr, err, optional) )
	    { // remaining value is valid so assign it
		put(tmp);
		return SEVERITY_WARNING;
	    }
	    else
	    {
		err->AppendToDetailMsg("Invalid Enumerated value.\n");
		err->AppendToUserMsg("Invalid Enumerated value.\n");
		return SEVERITY_WARNING;
	    }
	}
	// no surrounding delimiters
	else if( ValidLevel(sPtr, err, optional) )
	{ // value is valid so assign it
	    put (sPtr);  
	    return SEVERITY_NULL;
	}
	else
	{
	    err->AppendToDetailMsg("Invalid Enumerated value.\n");
	    err->AppendToUserMsg("Invalid Enumerated value.\n");
	    return SEVERITY_WARNING;
	}
    }
}


Severity
SCLP23(Enum)::STEPread (istream& in, ErrorDescriptor *err, int optional)
{
    char enumValue [BUFSIZ];
    char c;
    char errStr[BUFSIZ];
    errStr[0] = '\0';

    err->severity(SEVERITY_NULL); // assume ok until error happens
    in >> c;
    switch (c)  
      {
	case '.':
	  in.getline (enumValue, BUFSIZ, '.');// reads but does not store the .
/*
  // gcc 2.3.3 - It does and should read the . It doesn't store it DAS 4/27/93
	  char * pos = index(enumValue, '.');
	  if (pos) *pos = '\0';
	  //  NON-STANDARD (GNUism)  getline should not retrieve .
	  //  function gcount is unavailable
*/
	  if(in.fail())
	  {
	      err->GreaterSeverity(SEVERITY_WARNING);
	      err->AppendToUserMsg(
		    "Missing ending period delimiter for enumerated value.\n");
	      err->AppendToDetailMsg(
		    "Missing ending period delimiter for enumerated value.\n");
	  }
	  if(ValidLevel(enumValue, err, optional) == SEVERITY_NULL)
	      set_value (enumValue);
	  else
	  {
	      err->AppendToDetailMsg("Invalid enumerated value.\n");
	      err->GreaterSeverity(SEVERITY_WARNING);
	      set_value(ENUM_NULL);
	  }	      
	  break;
	  
	case ',':	// for next attribute or next aggregate value?
	case ')':	// for end of aggregate value?
	default:
	  in.putback (c);
	  set_value (ENUM_NULL);
	  if(optional) err->GreaterSeverity(SEVERITY_NULL);
	  else	       err->GreaterSeverity(SEVERITY_INCOMPLETE);
	  break;
	  
/*
	default:
	  set_value (ENUM_NULL);
		// read didn't know what to do
	  err->GreaterSeverity(SEVERITY_INPUT_ERROR); 
	  sprintf(errStr,
	       "SCLP23(Enum)::STEPread(): warning : poorly delimited %s %s",
	        Name(), "enumerated value was ignored.");
	  err->AppendToDetailMsg(errStr);
*/
      }  
    return err->severity();
}

#endif

