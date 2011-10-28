
/*
* NIST Utils Class Library
* clutils/Str.cc
* April 1997
* K. C. Morris
* David Sauder

* Development of this software was funded by the United States Government,
* and is not subject to copyright.
*/

/* $Id: Str.cc,v 3.0.1.3 1997/11/05 22:33:52 sauderd DP3.1 $  */ 

#include <Str.h>

/******************************************************************
 ** Procedure:  string functions
 ** Description:  These functions take a character or a string and return
 **	a temporary copy of the string with the function applied to it.
 ** Parameters:  
 ** Returns:  temporary copy of characters
 ** Side Effects:  
 ** Status:  complete
 ******************************************************************/

char
ToLower (const char c)
{
    if (isupper (c)) return (tolower (c));
    else return (c);

}

char
ToUpper  (const char c)
{
    if (islower (c)) return (toupper (c));
    else return (c);
}

char *
StrToLower (const char *strOld, char *strNew)
    /*
     * Place in strNew a lowercase version of strOld.
     */
{
    int i = 0;

    while ( strOld[i] != '\0' ) {
	strNew[i] = ToLower( strOld[i] );
	i++;
    }
    strNew[i] = '\0';
    return strNew;
}

const char *
StrToLower (const char * word, std::string &s)
{
    char newword [BUFSIZ];
    int i =0;
//    char ToLower (char c);
    
    while (word [i] != '\0') {
	newword [i] = ToLower (word [i]);
	++i;	
    }
    newword [i] = '\0';
    s = newword;
    return const_cast<char *>(s.c_str());
}

const char * 
StrToUpper (const char * word, std::string &s)
{
    char newword [BUFSIZ];
    int i =0;
//    char ToUpper (char c);
    
    while (word [i] != '\0') {
	newword [i] = ToUpper (word [i]);
	++i;
	
    }
    newword [i] = '\0';
    s = newword;
    return const_cast<char *>(s.c_str());
}

const char * 
StrToConstant (const char * word, std::string &s)
{
    char newword [BUFSIZ];
    int i =0;
//    char ToUpper (char c);
    
    while (word [i] != '\0') {
	if (word [i] == '/' || word [i] == '.') newword [i] = '_';
	else newword [i] = ToUpper (word [i]);
	++i;
	
    }
    newword [i] = '\0';
    s = newword;
    return const_cast<char *>(s.c_str());
}

/******************************************************************
 ** Procedure:  PrettyName (char * oldname)
 ** Parameters:  oldname
 ** Returns:  a new capitalized name 
 ** Description:  creates a new name with first character's in caps
 ** Side Effects:  PrettyNewName allocates memory for the new name
                   PrettyTmpName returns new name in a static buffer
 ** Status:   OK  7-Oct-1992 kcm 
 ******************************************************************/
const char *
PrettyTmpName (const char * oldname)
{
    int i= 0;
    static char newname [BUFSIZ];
    newname [0] = '\0';
    while (( oldname [i] != '\0') && (i < BUFSIZ)) {
        newname [i] = ToLower (oldname [i]);
        if (oldname [i] == '_')  /*  character is '_'   */  
	  {
	      ++i;
	      newname [i] = ToUpper (oldname [i]);  
	  }
        if (oldname [i] != '\0') ++i;
    }

    newname [0] = ToUpper (oldname [0]);
    newname [i] = '\0';
    return newname;
}

char *
PrettyNewName (const char * oldname)
{
    
    char * name = new char [strlen (oldname) +1];
    strcpy (name, PrettyTmpName (oldname));
    return name;
}

int
StrCmpIns( const char *strA, const char *strB )
    /*
     * An insensitive string compare function.  Used most often to compare
     * names of objects where case is not known and not significant.
     *
     * NOTE - cygnus does not define strcmpi/stricmp.  I'm sure there's a nifty
     * way to add preprocessor commands to check if it exists, and if so to
     * use it, but I didn't bother.
     */
{
    char str1[BUFSIZ], str2[BUFSIZ];

    strncpy( str1, PrettyTmpName( strA ), BUFSIZ-1 );
    strncpy( str2, PrettyTmpName( strB ), BUFSIZ-1 );
    return ( strcmp( str1, str2 ) );
}

// This function is used to check an input stream following a read.  It writes
// error messages in the 'ErrorDescriptor &err' argument as appropriate.
// 'const char *tokenList' argument contains a string made up of delimiters
// that are used to move the file pointer in the input stream to the end of
// the value you are reading (i.e. the ending marked by the presence of the
// delimiter).  The file pointer is moved just prior to the delimiter.  If the 
// tokenList argument is a null pointer then this function expects to find EOF.
//
// If input is being read from a stream then a tokenList should be provided so
// this function can push the file pointer up to but not past the delimiter
// (i.e. not removing the delimiter from the input stream).  If you have a
// string containing a single value and you expect the whole string to contain
// a valid value, you can change the string to an istrstream, read the value 
// then send the istrstream to this function with tokenList set to null 
// and this function will set an error for you if any input remains following
// the value.

// If the input stream can be readable again then 
//	- any error states set for the the stream are cleared. 
//	- white space skipped in the input stream
//	- if EOF is encountered it returns
//	  otherwise it peeks at the next character
//	- if the tokenList argument exists (i.e. is not null)
//	  then if looks to see if the char peeked at is in the tokenList string
//	  if it is then no error is set in the ErrorDescriptor
//	  if the char peeked at is not in the tokenList string that implies 
//	     that there is garbage following the value that was successfully 
//	     or unsuccessfully read.  The garbage is read until EOF or a 
//	     delimiter in the tokenList is found.
//	     - EOF is found you did not recover -> SEVERITY_INPUT_ERROR
//	     - delimiter found you recovered successfully => SEVERITY_WARNING
//	- if tokenList does not exist then it expects to find EOF, if it does 
//	  not then it is an error but the bad chars are not read since you have
//	  no way to know when to stop.

Severity 
CheckRemainingInput(istream &in, ErrorDescriptor *err, 
		    const char *typeName, // used in error message
		    const char *tokenList) // e.g. ",)"
{
  std::string skipBuf;
    char name[64];
    name[0] = 0;

    // 1. CHECK to see if there is invalid input following what you read.
    //    good or fail means you can still read from the input stream.

    if( in.good() || in.fail() )
      // fail means that the input did not match the expected input.
    {   // check for bad input following what you read (or tried to read) but 
	// preceding a delimiter if you are expecting one.

	in.clear(); // clear the istreams error
	in >> ws; // skip whitespace
	if(in.eof()) // no input following the desired input (or following the 
	{	     // missing desired input)
	    return err->severity();
	}
	char c;
	c = in.peek();

	if( tokenList ) // are expecting a delim so read till you find it
	{

	    // 3. FIND a delimiter

	    if( strchr(tokenList, c) )
	    { // next char peeked at was delim expected => success
		return err->severity();
	    }
	    else // next character was not a delim and thus is garbage
	    { // read bad input until you find the delim
		in.get(c);
		while(in && !strchr(tokenList, c) )
		{   // this could chew up the remainder of the input unless you
		    // give it the delim to end an entity value and it knows 
		    // about strings
		    skipBuf += c;
		    in.get(c);
		}
		// ENHANCEMENT may want to add the bad data to the err msg.

		if(strchr(tokenList, c))
		{ // congratulations you recovered
		    err->GreaterSeverity(SEVERITY_WARNING);
		    sprintf(name, 
			   "\tFound invalid %s value...\n", 
			    typeName);
		    err->AppendToUserMsg(name);
		    err->AppendToDetailMsg(name);
		    err->AppendToDetailMsg(
				"\tdata lost looking for end of attribute: ");
		    err->AppendToDetailMsg( skipBuf.c_str() );
		    err->AppendToDetailMsg( "\n" );
		    in.putback(c);
			// invalid input 
			// (though a valid value may have been assigned)
		    return err->severity();
		}
		else
		{ // could not recover (of course)
		    err->GreaterSeverity(SEVERITY_INPUT_ERROR);
		    sprintf(name, 
		      "Unable to recover from input error while reading %s %s",
			    typeName, "value.\n");
		    err->AppendToUserMsg(name);
		    err->AppendToDetailMsg(name);
			// invalid input 
			// (though a valid value may have been assigned)
		    return err->severity();
		}
	    }
	}
	else if(in.good()) // found a char => error 
	  // i.e. skipping whitespace did not cause EOF so must have hit a char
	{ // remember we are not expecting more input since no tokenList

	    // or 3. since not expecting a delimiter and there is input there 
	    //       is an error

	    err->GreaterSeverity(SEVERITY_WARNING);
	    sprintf(name, "Invalid %s value.\n", typeName);
	    err->AppendToUserMsg(name);
	    err->AppendToDetailMsg(name);
			// invalid input 
			// (though a valid value may have been assigned)
	    return err->severity();
	}
    }
    else if (in.eof())
    {
	// hit EOF when reading for a value 
	// don\'t set any error
	return err->severity();
    }
    else
    { // badbit set (in.bad()) means there was a problem when reading istream
      // this is bad news... it means the input stream is hopelessly messed up
	err->GreaterSeverity(SEVERITY_INPUT_ERROR);
	sprintf(name, "Invalid %s value.\n", typeName);
	err->AppendToUserMsg(name);
	err->AppendToDetailMsg(name);
			// invalid input 
			// (though a valid value may have been assigned)
	return err->severity();
    }
    return err->severity();
}
