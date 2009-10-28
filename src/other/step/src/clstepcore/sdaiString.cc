
/*
* NIST STEP Core Class Library
* clstepcore/sdaiString.cc
* April 1997
* KC Morris

* Development of this software was funded by the United States Government,
* and is not subject to copyright.
*/

/* $Id: sdaiString.cc,v 1.4 1997/11/05 21:59:17 sauderd DP3.1 $ */

#include <sdai.h>
#include <sstream>


SCLP23(String)& 
SCLP23(String)::operator= (const char* s)
{
    SCLstring::operator= (s);
    return *this;
}

void 
SCLP23(String)::STEPwrite (ostream& out) const
{
    const char *str = 0;
// strings that exist but do not contain any chars should be written as '',
// not $ --Josh L, 4/28/95
//    if (is_null ())
    if (is_undefined ())
	out << "$";
    else
    {
	out << "\'";
	str = chars ();
	while (*str)
	{
	    if(*str == STRING_DELIM)
		out << STRING_DELIM;
	    out << *str;
	    str++;
	}
	out << "\'";
    }
}

void 
SCLP23(String)::STEPwrite (SCLstring &s) const
{
    const char *str = 0;
// null strings should be represented by '', not $ --Josh L, 4/28/95
//    if (is_null ())
    if (is_undefined ())
    {
//	s.set_null(); // this would free up space? nope
	s = "$";
    }
    else
    {
	s = "\'";
	str = chars ();
	while (*str)
	{
	    if(*str == STRING_DELIM)
		s.Append(STRING_DELIM);
	    s.Append(*str);
	    str++;
	}
	s.Append(STRING_DELIM);
    }
}

Severity
SCLP23(String)::StrToVal (const char * s)
{
  operator= (s);
  if (! strcmp (chars (),  s))  return SEVERITY_NULL ; 
  else return SEVERITY_INPUT_ERROR; 
}

//  STEPread reads a string in exchange file format
//  starting with a single quote
Severity 
SCLP23(String)::STEPread (istream& in, ErrorDescriptor *err)
{
    int foundEndQuote = 0; // need so this string is not ok: 'hi''
    set_null ();  // clear the old string
    char c;
    in >> ws; // skip white space
    in >> c;

	// remember the current format state to restore the previous settings
#if defined(__GNUC__) && (__GNUC__ > 2)
    ios_base::fmtflags flags = in.flags();
#else
    ios::fmtflags flags = in.flags();
#endif
    in.unsetf(ios::skipws);

    if (c == STRING_DELIM)
    {
	Append("\0"); // this will make sure that this will not be undefined.
	   // It will handle the case where '' is read. It will be an
	   // empty string rather than one that is undefined.

	while( (c != '\0') && in.good() && in.get(c) )
	{
	    if (c == STRING_DELIM)   {
		in.get (c);
		if( ! in.good() )
		{	// it is the final quote and no extra char was read
		    foundEndQuote = 1;
		    c = '\0';
		}
		else if( !(c == STRING_DELIM) )
		{	// it is the final quote and extra char was read
		    in.putback (c); // put back non-quote extra char
		    foundEndQuote = 1;
		    c = '\0';
		}
		// else { ; } // do nothing it is an embedded quote
	    } 
	    Append (c);
	}
	Append ('\0');

	if(foundEndQuote)
	    return SEVERITY_NULL;
	else
	{    // non-recoverable error
	    err->AppendToDetailMsg("Missing closing quote on string value.\n");
	    err->AppendToUserMsg("Missing closing quote on string value.\n");
	    err->GreaterSeverity(SEVERITY_INPUT_ERROR);
	    return SEVERITY_INPUT_ERROR;
	}
    }
    //  otherwise there was not a quote
    in.putback (c);
    in.flags(flags); // set the format state back to previous settings

    set_undefined();

    return err -> GreaterSeverity (SEVERITY_INCOMPLETE);  
}

Severity 
SCLP23(String)::STEPread (const char *s, ErrorDescriptor *err)
{
    istringstream in((char *)s);
    return STEPread (in, err);
}
