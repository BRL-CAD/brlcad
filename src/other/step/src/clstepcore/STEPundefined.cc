
/*
* NIST STEP Core Class Library
* clstepcore/STEPundefined.cc
* April 1997
* KC Morris
* David Sauder

* Development of this software was funded by the United States Government,
* and is not subject to copyright.
*/

/* $Id: STEPundefined.cc,v 3.0.1.2 1997/11/05 21:59:29 sauderd DP3.1 $ */

#include <stdio.h> // to get the BUFSIZ #define
#include <STEPundefined.h>
#include <STEPattribute.h>


/******************************************************************
 **    helping functions for reading unknown types		**/



Severity 
SCLundefined::StrToVal(const char *s, ErrorDescriptor *err)
{
    val = s;
    return SEVERITY_NULL;
}

Severity 
SCLundefined::StrToVal(istream &in, ErrorDescriptor *err)
{
    return STEPread(in, err);
}

Severity 
SCLundefined::STEPread(const char *s, ErrorDescriptor *err)
{
    istringstream in((char *) s);
    return STEPread(in, err);
}

Severity 
SCLundefined::STEPread(istream &in, ErrorDescriptor *err)
{
    char c = '\0';
    ostringstream ss;
    SCLstring str;

    int terminal = 0;

    in >> ws; // skip white space
    in >> c;
    if(c == '$')
    {
	val = "";
	CheckRemainingInput(in, err, "aggregate item", ",)");
    }
    else
	in.putback(c);

    while (!terminal)  
    {
	in.get(c);
	switch (c)  
	{
	  case '(':
	    in.putback(c);

	    PushPastImbedAggr(in, str, err);
	    ss << str.chars();
	    break;

	  case '\'':
	    in.putback(c);

	    PushPastString(in, str, err);
	    ss << str.chars();
	    break;

	  case ',':	
	    terminal = 1; // it's a STEPattribute separator
	    in.putback (c);
	    c = '\0';
	    break;

	  case ')':
	    in.putback (c);
	    terminal = 1; // found a valid delimiter
	    break;

	  case '\0':
	  case EOF:
	    terminal = 1; // found a valid delimiter
	    break;

	  default:
	    ss.put(c);
	    break;
	}

	if (!in.good ()) {
	    terminal =1;
	    c = '\0';  
	}
//	  if (!in.readable ()) terminal =1;
    }	  

    ss << ends;
    /*** val = ss.str(); ***/ val = &(ss.str()[0]);

    err->GreaterSeverity(SEVERITY_NULL);
    return SEVERITY_NULL;
}

const char *
SCLundefined::asStr(SCLstring & s) const
{
    s = val.chars();
    return s.chars();
}

const char *
SCLundefined::STEPwrite(SCLstring &s)
{
    if(val.rep())
    {
	s = val.chars();
    }
    else 
	s = "$";
    return s.chars();
}

void 
SCLundefined::	STEPwrite (ostream& out)
{
    if(val.rep())
	out << val.chars();
    else 
	out << "$";
}

SCLundefined& 
SCLundefined::operator= (const SCLundefined& x)  
{
    SCLstring tmp;
    val = x.asStr(tmp);
    return *this;
}

SCLundefined& 
SCLundefined::operator= (const char * str)
{
    if (!str)
	val.set_null();
    else
	val = str;
    return *this;
}

SCLundefined::SCLundefined ()  
{
}

SCLundefined::~SCLundefined ()  
{
}

int
SCLundefined::set_null ()  
{
    val = "";
    return 1;
}

int
SCLundefined::is_null ()  
{
    return (!strcmp (val.chars(), ""));
    
}


/*
int
SCLundefined::STEPread(istream& in )  
{
    char c ='\0';
    char buf [BUFSIZ];
    int i =0;
    int open_paren =0;
    int terminal = 0;
    
    while (!terminal)  
      {
	  in >> c;
	  switch (c)  
	    {
	      case '(':
		++open_paren;
		break;
	      case ')':
		if (open_paren)  {
		      --open_paren;
		      break; 
		  }
		// otherwise treat it like a comma
	      case ',':
		if (!open_paren)  {
		    terminal =1;
		    in.putback (c);
		    c = '\0';
		}
		
		break;
	      case '\0':
		terminal =1;
		break;
		
	    }		

	  if (!in)
	  {
	      terminal =1;
	      c = '\0';  
	  }
	  if (i < BUFSIZ) buf [i] = c;

	  // BUG:  read up to BUFSIZ -1 number of characters
	  // if more characters, NULL terminate and ignore the rest of input 
	  if ((++i == BUFSIZ) && !terminal)  {
	      cerr << "WARNING:  information lost -- value of undefined type is too long\n";
	      buf [i] = '\0';
	  }

      }	  
    if (i < BUFSIZ) buf [i+1] = '\0';
    val = buf;
    return i;
}
*/
