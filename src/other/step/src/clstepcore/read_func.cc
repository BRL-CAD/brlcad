#ifdef __O3DB__
#include <OpenOODB.h>
#endif

#include <errordesc.h>
#include <stdio.h>
#include <sdai.h>
#include <read_func.h>
#include <STEPattribute.h>

const int RealNumPrecision = REAL_NUM_PRECISION;

// print Error information for debugging purposes
void 
PrintErrorState(ErrorDescriptor &err)
{
    cout << "** severity: ";
    switch(err.severity())
    {
      case SEVERITY_NULL :
	cout << "\n  Null\n";
	break;
      case SEVERITY_USERMSG :
	cout << "\n  User Message\n";
	break;
      case SEVERITY_INCOMPLETE :
	cout << "\n  Incomplete\n";
	break;
      case SEVERITY_WARNING :
	cout << "\n  Warning\n";
	break;
      case SEVERITY_INPUT_ERROR :
	cout << "\n  Input Error\n";
	break;
      default:
	cout << "\n  Other\n";
	break;
    }
    cout << err.DetailMsg() << "\n";
}

// print istream error information for debugging purposes
void IStreamState(istream &in)
{
    if( in.good())
    {
	cerr << "istream GOOD\n" << flush;
    }
    if( in.fail() )
    {
	cerr << "istream FAIL\n" << flush;
    }
    if( in.eof() )
    {
	cerr << "istream EOF\n" << flush;
    }
}

///////////////////////////////////////////////////////////////////////////////
//  ReadInteger
// * This function reads an integer if possible
// * If an integer is read it is assigned to val and 1 (true) is returned.
// * If an integer is not read because of an error then val is left unchanged 
//   and 0 (false) is returned.
// * If there is an error then the ErrorDescriptor err is set accordingly with
//   a severity level and error message (no error MESSAGE is set for severity
//   incomplete).
// * tokenList contains characters that terminate reading the value.
// * If tokenList is not zero then the istream will be read until a character 
//   is found matching a character in tokenlist.  All values read up to the 
//   terminating character (delimiter) must be valid or err will be set with an
//   appropriate error message.  A valid value may still have been assigned 
//   but it may be followed by garbage thus an error will result.  White
//   space between the value and the terminating character is not considered 
//   to be invalid.  If tokenList is null then the value must not be followed
//   by any characters other than white space (i.e. EOF must happen)
//   
///////////////////////////////////////////////////////////////////////////////

int
ReadInteger(SCLP23(Integer) &val, istream &in, ErrorDescriptor *err, 
	    char *tokenList)
{
    SCLP23(Integer) i = 0;
    in >> ws;
    in >> i;

    int valAssigned = 0;

//    IStreamState(in);

    if(!in.fail())
    {
	valAssigned = 1;
	val = i;
    }
    Severity s = CheckRemainingInput(in, err, "Integer", tokenList);
    return valAssigned;
}

///////////////////////////////////////////////////////////////////////////////
// same as above but reads from a const char *
///////////////////////////////////////////////////////////////////////////////

int
ReadInteger(SCLP23(Integer) &val, const char *s, ErrorDescriptor *err, 
	    char *tokenList)
{
    istringstream in((char *)s);
    return ReadInteger(val, in, err, tokenList);
}

///////////////////////////////////////////////////////////////////////////////
// * attrValue is validated.
// * err is set if there is an error
// * If optional is 1 then a missing value will be valid otherwise severity
//   incomplete will be set.
// * If you don\'t know if the value may be optional then set it false and 
//   check to see if SEVERITY_INCOMPLETE is set. You can change it later to
//   SEVERITY_NULL if it is valid for the value to be missing.  No error 
//   'message' will be associated with the value being missing so you won\'t 
//   have to worry about undoing an error message.
// * tokenList contains characters that terminate the expected value.
// * If tokenList is not zero then the value is expected to terminate before 
//   a character found in tokenlist.  All values read up to the 
//   terminating character (delimiter) must be valid or err will be set with an
//   appropriate error message.  White space between the value and the 
//   terminating character is not considered to be invalid.  If tokenList is 
//   null then attrValue must only contain a valid value and nothing else 
//   following.
///////////////////////////////////////////////////////////////////////////////

Severity 
IntValidLevel(const char *attrValue, ErrorDescriptor *err,
              int clearError, int optional, char *tokenList)
{
    if(clearError)
	err->ClearErrorMsg();

    istringstream in((char *)attrValue);
    in >> ws; // skip white space
    char c = in.peek();
    if(in.eof())
    {
	if(!optional)
	    err->GreaterSeverity(SEVERITY_INCOMPLETE);
    }
    else if(c == '$')
    {
	if(!optional)
	    err->GreaterSeverity(SEVERITY_INCOMPLETE);
	in >> c;
	CheckRemainingInput(in, err, "integer", tokenList);
	return err->severity();
    }
    else
    {
	SCLP23(Integer) val = 0;
	int valAssigned = ReadInteger(val, in, err, tokenList);
	if(!valAssigned && !optional)
	    err->GreaterSeverity(SEVERITY_INCOMPLETE);
    }
    return err->severity();
}

char * 
WriteReal(SCLP23(Real) val, std::string &s)
{

    char rbuf[64];

//        out << form("%.*G", (int) Real_Num_Precision,tmp);
    // replace the above line with this code so that writing the '.' is
    // guaranteed for reals. If you use e or E then you get many 
    // unnecessary trailing zeros. g and G truncates all trailing zeros
    // to save space but when no non-zero precision exists it also 
    // truncates the decimal. The decimal is required by Part 21. 
    // Also use G instead of g since G writes uppercase E (E instead of e 
    // is also required by Part 21) when scientific notation is used - DAS

    sprintf(rbuf, "%.*G", (int) RealNumPrecision,val);
    if(!strchr(rbuf, '.'))
    {
	if(strchr(rbuf, 'E') || strchr(rbuf, 'e'))
	{
	    char *expon = strchr(rbuf, 'E');

	    if( !expon)
	    {
		expon = strchr(rbuf, 'e');
	    }
	    *expon = '\0';
	    s = rbuf;
	    s.append(".");
	    s.append("E");
	    expon++;
	    s += expon;
	}
	else 
	{
	    int rindex = strlen(rbuf);
	    rbuf[rindex] = '.';
	    rbuf[rindex+1] = '\0';
	    s = rbuf;
	}
    }
    else
      s = rbuf;
    return const_cast<char *>(s.c_str());
}

void
WriteReal(SCLP23(Real) val, ostream &out)
{
    std::string s;

    out << WriteReal(val,s);
#if 0
    char rbuf[64];

//        out << form("%.*G", (int) Real_Num_Precision,tmp);
    // replace the above line with this code so that writing the '.' is
    // guaranteed for reals. If you use e or E then you get many 
    // unnecessary trailing zeros. g and G truncates all trailing zeros
    // to save space but when no non-zero precision exists it also 
    // truncates the decimal. The decimal is required by Part 21. 
    // Also use G instead of g since G writes uppercase E (E instead of e 
    // is also required by Part 21) when scientific notation is used - DAS

    sprintf(rbuf, "%.*G", (int) RealNumPrecision,val);
    if(!strchr(rbuf, '.'))
    {
	if(strchr(rbuf, 'E') || strchr(rbuf, 'e'))
	{
	    char *expon = strchr(rbuf, 'E');

	    if( !expon)
	    {
		expon = strchr(rbuf, 'e');
	    }
	    *expon = '\0';
	    s = rbuf;
	    s.Append('.');
	    s.Append('E');
	    expon++;
	    s.Append(expon);
	}
	else 
	{
	    int rindex = strlen(rbuf);
	    rbuf[rindex] = '.';
	    rbuf[rindex+1] = '\0';
	    s = rbuf;
	}
    }
    else
      s = rbuf;

    out << (char *)(s.chars());
#endif
}

///////////////////////////////////////////////////////////////////////////////
//  ReadReal
// * This function reads a real if possible
// * If a real is read it is assigned to val and 1 (true) is returned.
// * If a real is not read because of an error then val is left unchanged 
//   and 0 (false) is returned.
// * If there is an error then the ErrorDescriptor err is set accordingly with
//   a severity level and error message (no error MESSAGE is set for severity
//   incomplete).
// * tokenList contains characters that terminate reading the value.
// * If tokenList is not zero then the istream will be read until a character 
//   is found matching a character in tokenlist.  All values read up to the 
//   terminating character (delimiter) must be valid or err will be set with an
//   appropriate error message.  A valid value may still have been assigned 
//   but it may be followed by garbage thus an error will result.  White
//   space between the value and the terminating character is not considered 
//   to be invalid.  If tokenList is null then the value must not be followed
//   by any characters other than white space (i.e. EOF must happen)

//   skip any leading whitespace characters
//   read: optional sign, at least one decimal digit, required decimal point,
//   zero or more decimal digits, optional letter e or E (but lower case e is 
//   an error), optional sign, at least one decimal digit if there is an E.

///////////////////////////////////////////////////////////////////////////////

int
ReadReal(SCLP23(Real) &val, istream &in, ErrorDescriptor *err, 
	 char *tokenList)
{
    SCLP23(Real) d = 0;

    // Read the real's value into a string so we can make sure it is properly
    // formatted. e.g. a decimal point is present. If you use the stream to 
    // read the real, it won't complain if the decimal place is missing.
    char buf[64];
    int i = 0;
    char c;
    ErrorDescriptor e;

    in >> ws; // skip white space

    // read optional sign
    c = in.peek();
    if(c == '+' || c == '-') { in.get(buf[i++]); c = in.peek(); }

    // check for required initial decimal digit
    if(!isdigit(c)) 
    {
	e.severity(SEVERITY_WARNING);
	e.DetailMsg("Real must have an initial digit.\n");
    }
    // read one or more decimal digits
    while(isdigit(c))
    {
	in.get(buf[i++]);
	c = in.peek();
    }

    // read Part 21 required decimal point
    if(c == '.')
    {
	in.get(buf[i++]);
	c = in.peek();
    }
    else
    {
	// It may be the number they wanted but it is incompletely specified 
	// without a decimal and thus it is an error 
	e.GreaterSeverity(SEVERITY_WARNING);
	e.AppendToDetailMsg("Reals are required to have a decimal point.\n");
    }

    // read optional decimal digits
    while(isdigit(c))
    {
	in.get(buf[i++]);
	c = in.peek();
    }

    // try to read an optional E for scientific notation 
    if( (c == 'e') || (c == 'E') )
    {
	if(c == 'e')
	{
	 // this is incorrectly specified and thus is an error 
	    e.GreaterSeverity(SEVERITY_WARNING);
//	    e.GreaterSeverity(SEVERITY_USERMSG); // not flagged as an error
	    e.AppendToDetailMsg(
		  "Reals using scientific notation must use upper case E.\n");
	}
	in.get(buf[i++]); // read the E
	c = in.peek();

	// read optional sign
	if(c == '+' || c == '-')
	{ in.get(buf[i++]); c = in.peek(); }

	// read required decimal digit (since it has an E)
	if(!isdigit(c)) 
	{
	    e.GreaterSeverity(SEVERITY_WARNING);
	    e.AppendToDetailMsg(
				 "Real must have at least one digit following E for scientific notation.\n");
	}
	// read one or more decimal digits
	while(isdigit(c))
	{
	    in.get(buf[i++]);
	    c = in.peek();
	}
    }
    buf[i] = '\0';

/*
    int success = 0;
    success = sscanf(buf," %G", &d);
*/
    istringstream in2((char *)buf);

    // now that we have the real the stream will be able to salvage reading 
    // whatever kind of format was used to represent the real.
    in2 >> d;
//    cout << "buffer: " << buf << endl << "value:  " << d << endl << endl;

    int valAssigned = 0;
//    PrintErrorState(err);

//    if(success > 0)
    if(!in2.fail())
    {
	valAssigned = 1;
	val = d;
	err->GreaterSeverity(e.severity());
	err->AppendToDetailMsg(e.DetailMsg());
    }
    else
      val = S_REAL_NULL;

    Severity s = CheckRemainingInput(in, err, "Real", tokenList);
    return valAssigned;

/* old way - much easier but not thorough enough */
/*
    in >> d;

//    IStreamState(in);

    int valAssigned = 0;
//    PrintErrorState(err);

    if(!in.fail())
    {
	valAssigned = 1;
	val = d;
    }
    Severity s = CheckRemainingInput(in, err, "Real", tokenList);
    return valAssigned;
*/
}

///////////////////////////////////////////////////////////////////////////////
// same as above but reads from a const char *
///////////////////////////////////////////////////////////////////////////////

int
ReadReal(SCLP23(Real) &val, const char *s, ErrorDescriptor *err, 
	 char *tokenList)
{
    istringstream in((char *)s);
    return ReadReal(val, in, err, tokenList);
}

///////////////////////////////////////////////////////////////////////////////
// * attrValue is validated.
// * err is set if there is an error
// * If optional is 1 then a missing value will be valid otherwise severity
//   incomplete will be set.
// * If you don\'t know if the value may be optional then set it false and 
//   check to see if SEVERITY_INCOMPLETE is set. You can change it later to
//   SEVERITY_NULL if it is valid for the value to be missing.  No error 
//   'message' will be associated with the value being missing so you won\'t 
//   have to worry about undoing an error message.
// * tokenList contains characters that terminate the expected value.
// * If tokenList is not zero then the value is expected to terminate before 
//   a character found in tokenlist.  All values read up to the 
//   terminating character (delimiter) must be valid or err will be set with an
//   appropriate error message.  White space between the value and the 
//   terminating character is not considered to be invalid.  If tokenList is 
//   null then attrValue must only contain a valid value and nothing else 
//   following.
///////////////////////////////////////////////////////////////////////////////

Severity 
RealValidLevel(const char *attrValue, ErrorDescriptor *err,
               int clearError, int optional, char *tokenList)
{
    if(clearError)
	err->ClearErrorMsg();

    istringstream in((char *)attrValue);
    in >> ws; // skip white space
    char c = in.peek();
    if(in.eof())
    {
	if(!optional)
	    err->GreaterSeverity(SEVERITY_INCOMPLETE);
    }
    else if(c == '$')
    {
	if(!optional)
	    err->GreaterSeverity(SEVERITY_INCOMPLETE);
	in >> c;
	CheckRemainingInput(in, err, "real", tokenList);
	return err->severity();
    }
    else
    {
	SCLP23(Real) val = 0;
	int valAssigned = ReadReal(val, in, err, tokenList);
	if(!valAssigned && !optional)
	    err->GreaterSeverity(SEVERITY_INCOMPLETE);
    }
    return err->severity();
}

///////////////////////////////////////////////////////////////////////////////
//  ReadNumber - read as a real number
// * This function reads a number if possible
// * If a number is read it is assigned to val and 1 (true) is returned.
// * If a number is not read because of an error then val is left unchanged 
//   and 0 (false) is returned.
// * If there is an error then the ErrorDescriptor err is set accordingly with
//   a severity level and error message (no error MESSAGE is set for severity
//   incomplete).
// * tokenList contains characters that terminate reading the value.
// * If tokenList is not zero then the istream will be read until a character 
//   is found matching a character in tokenlist.  All values read up to the 
//   terminating character (delimiter) must be valid or err will be set with an
//   appropriate error message.  A valid value may still have been assigned 
//   but it may be followed by garbage thus an error will result.  White
//   space between the value and the terminating character is not considered 
//   to be invalid.  If tokenList is null then the value must not be followed
//   by any characters other than white space (i.e. EOF must happen)
///////////////////////////////////////////////////////////////////////////////

int
ReadNumber(SCLP23(Real) &val, istream &in, ErrorDescriptor *err, 
	   char *tokenList)
{
    SCLP23(Real) d = 0;
    in >> ws;
    in >> d;

//    IStreamState(in);

    int valAssigned = 0;
    if(!in.fail())
    {
	valAssigned = 1;
	val = d;
    }
    Severity s = CheckRemainingInput(in, err, "Number", tokenList);
    return valAssigned;
}

///////////////////////////////////////////////////////////////////////////////
// same as above but reads from a const char *
///////////////////////////////////////////////////////////////////////////////

int
ReadNumber(SCLP23(Real) &val, const char *s, ErrorDescriptor *err, 
	   char *tokenList)
{
    istringstream in((char *)s);
    return ReadNumber(val, in, err, tokenList);
}


///////////////////////////////////////////////////////////////////////////////
// * attrValue is validated.
// * err is set if there is an error
// * If optional is 1 then a missing value will be valid otherwise severity
//   incomplete will be set.
// * If you don\'t know if the value may be optional then set it false and 
//   check to see if SEVERITY_INCOMPLETE is set. You can change it later to
//   SEVERITY_NULL if it is valid for the value to be missing.  No error 
//   'message' will be associated with the value being missing so you won\'t 
//   have to worry about undoing an error message.
// * tokenList contains characters that terminate the expected value.
// * If tokenList is not zero then the value is expected to terminate before 
//   a character found in tokenlist.  All values read up to the 
//   terminating character (delimiter) must be valid or err will be set with an
//   appropriate error message.  White space between the value and the 
//   terminating character is not considered to be invalid.  If tokenList is 
//   null then attrValue must only contain a valid value and nothing else 
//   following.
///////////////////////////////////////////////////////////////////////////////

Severity 
NumberValidLevel(const char *attrValue, ErrorDescriptor *err,
                 int clearError, int optional, char *tokenList)
{
    if(clearError)
	err->ClearErrorMsg();

    istringstream in((char *)attrValue);
    in >> ws; // skip white space
    char c = in.peek();
    if(in.eof())
    {
	if(!optional)
	    err->GreaterSeverity(SEVERITY_INCOMPLETE);
    }
    else if(c == '$')
    {
	if(!optional)
	    err->GreaterSeverity(SEVERITY_INCOMPLETE);
	in >> c;
	CheckRemainingInput(in, err, "number", tokenList);
	return err->severity();
    }
    else
    {
	SCLP23(Real) val = 0;
	int valAssigned = ReadNumber(val, in, err, tokenList);
	if(!valAssigned && !optional)
	    err->GreaterSeverity(SEVERITY_INCOMPLETE);
    }
    return err->severity();
}


// return 1 if 'in' points at a quoted quote otherwise return 0

int
QuoteInString(istream& in)
{
    char c1, c2;
    if( in.good() )
    {
	in >> c1;
	in >> c2;
	in.putback(c2);
	in.putback(c1);
	if (c1 == STRING_DELIM)
	{
	    if(c2 == STRING_DELIM)
		return 1;
	}
    }
    return 0;
}    

// assign 's' so that it contains an exchange file format string read from 
// 'in'.  

void
PushPastString(istream& in, std::string &s, ErrorDescriptor *err)
{
    char messageBuf[BUFSIZ];
    messageBuf[0] = '\0';

    char c;
    in >> ws; // skip whitespace
    in >> c;
    if(c == STRING_DELIM)
    {
	s += c;
	while(QuoteInString(in)) // to handle a string like '''hi'
	{
	    in.get(c);
	    s += c;
	    in.get(c);
	    s += c;
	}
//	int sk = in.skip(0);
	while( in.get(c) && (c != STRING_DELIM) )
	{
	    s += c;
	    while(QuoteInString(in))
	    {	// to handle a string like 'hi'''
		in.get(c);
		s += c;
		in.get(c);
		s += c;
	    }
	}
//	in.skip(sk);
	
	if(c != STRING_DELIM)
	{
	    err->GreaterSeverity(SEVERITY_INPUT_ERROR);
	    sprintf(messageBuf, "Invalid string value.\n");
	    err->AppendToDetailMsg(messageBuf);
	    s.append("\'");
	}
	else
	    s += c;
    }
}

// assign 's' so that it contains an exchange file format aggregate read from 
// 'in'.  
// This is used to read aggregates that are part of multidimensional 
// aggregates.

void
PushPastImbedAggr(istream& in, std::string &s, ErrorDescriptor *err)
{
    char messageBuf[BUFSIZ];
    messageBuf[0] = '\0';

    char c;
    in >> ws;
    in.get(c);

    if(c == '(')
    {
	s += c;
	in.get(c);
	while( in.good() && (c != ')') )
	{
	    if ( c == '(' )  
	    {
		in.putback(c);
		PushPastImbedAggr(in, s, err);
	    }
	    else if (c == STRING_DELIM)
	    {
		in.putback(c);
		PushPastString(in, s, err);
	    }
	    else
		s += c;
	    in.get(c);
	}    
	if(c != ')')
	{
	    err->GreaterSeverity(SEVERITY_INPUT_ERROR);
	    sprintf(messageBuf, "Invalid aggregate value.\n");
	    err->AppendToDetailMsg(messageBuf);
	    s.append(")");
	}
	else
	    s += c;
    }
}


// assign 's' so that it contains an exchange file format aggregate read from 
// 'in'.  
// This is used to read a single dimensional aggregate (i.e. it is not allowed
// to contain an aggregate as an element.

void
PushPastAggr1Dim(istream& in, std::string &s, ErrorDescriptor *err)
{
    char messageBuf[BUFSIZ];
    messageBuf[0] = '\0';

    char c;
    in >> ws;
    in.get(c);

    if(c == '(')
    {
	s += c;
	in.get(c);
	while( in.good() && (c != ')') )
	{
	    if ( c == '(' )  
	    {
		err->GreaterSeverity(SEVERITY_WARNING);
		sprintf(messageBuf, "Invalid aggregate value.\n");
		err->AppendToDetailMsg(messageBuf);
	    }

	    if (c == STRING_DELIM)
	    {
		in.putback(c);
		PushPastString(in, s, err);
	    }
	    else
		s += c;
	    in.get(c);
	}    
	if(c != ')')
	{
	    err->GreaterSeverity(SEVERITY_INPUT_ERROR);
	    sprintf(messageBuf, "Invalid aggregate value.\n");
	    err->AppendToDetailMsg(messageBuf);
	    s.append(")");
	}
	else
	    s += c;
    }
}

/*************************** 
* FindStartOfInstance reads to the beginning of an instance marked by #  
* it copies what is read to the std::string inst.  It leaves the # on the 
* istream.
***************************/

Severity 
FindStartOfInstance(istream& in, std::string&  inst)
{
    char c =0;
    ErrorDescriptor errs;
    SCLP23(String) tmp;
//    std::string tmp;

    while (in.good ())
    {
	in >> c;
	switch (c)  {
	  case '#':  //  found char looking for.
	    in.putback(c);
	    return SEVERITY_NULL;

	  case '\'':  // get past the string
	    in.putback (c);
	    tmp.STEPread (in, &errs);
	    inst.append (tmp);

//	    PushPastString(in, tmp, &errs);
//	    inst.Append( tmp->chars() );
	    break;

	  case '\0':  // problem in input ?
	    return SEVERITY_INPUT_ERROR;

	  default:
	    inst += c;
	}
    }
    return SEVERITY_INPUT_ERROR;
}

/*************************** 
* SkipInstance reads in an instance terminated with ;.  it copies
* what is read to the std::string inst.  
***************************/

Severity
SkipInstance(istream& in, std::string&  inst)
{
    char c =0;
    ErrorDescriptor errs;
    SCLP23(String) tmp;
//    std::string tmp;

    while (in.good ())
    {
	in >> c;
	switch (c)  {
	  case ';':  //  end of instance reached
	    return SEVERITY_NULL;

	  case '\'':  // get past the string
	    in.putback (c);
	    tmp.STEPread (in, &errs);
	    inst.append (tmp);

//	    PushPastString(in, tmp, &errs);
//	    inst.Append( tmp->chars() );
	    break;

	  case '\0':  // problem in input ?
	    return SEVERITY_INPUT_ERROR;

	  default:
	    inst += c;
	}
    }
    return SEVERITY_INPUT_ERROR;
}


/*************************** 
// This reads a simple record.  It is used when parsing externally mapped 
// entities to skip to the next entity type name part of the externally mapped
// record.  It does not expect a semicolon at the end since the pieces in an
// external mapping don't have them.  If you are reading a simple record in the
// form of an internal mapping you will have to read the semicolon.
***************************/

const char *
SkipSimpleRecord(istream &in, std::string &buf, ErrorDescriptor *err)
{
    char c;
    std::string s;

    in >> ws;
    in.get(c);
    if(c == '(') // beginning of record
    {
	buf += c;
	while(in.get(c) && (c != ')') && (err->severity() > SEVERITY_INPUT_ERROR) )
	{
	    if(c == '\'')
	    {
		in.putback(c);
		s.clear();
		PushPastString(in, s, err);
		buf.append(s.c_str());
	    }
	    else if(c == '(')
	    {
		in.putback(c);
		s.clear();
		PushPastImbedAggr(in, s, err);
		buf.append(s.c_str());
	    }
	    else
		buf += c;
	}
	if( !in.good() )
	{
	    err->GreaterSeverity(SEVERITY_INPUT_ERROR);
	    err->DetailMsg("File problems reading simple record.\n");
	}
	buf.append(")");
    }
    else
	in.putback(c); // put back open paren
    return const_cast<char *>(buf.c_str());
}

/***************************
// This reads a part 21 definition of keyword.  This includes the name of 
// entity types.  To read a user-defined keyword: read the '!' then call 
// this function with skipInitWS turned off.
***************************/

const char *
ReadStdKeyword(istream& in, std::string &buf, int skipInitWS)
{
    char c;
    if(skipInitWS)
	in >> ws;

    while( in.get(c) && !isspace(c) && ( isalnum(c) || (c == '_') ) )
    {
	buf += c;
    }

    if( in.eof() || in.good() )
	in.putback(c);

    return const_cast<char *>(buf.c_str());
}

/***************************
This function returns a null terminated const char* for the
characters read from the istream up to, but not including
the first character found in the set of delimiters, or the 
whitespace character. It leaves the delimiter on the istream.

The string is returned in a static buffer, so it will change 
the next time the function is called.

Keywords are special strings of characters indicating the instance
of an entity of a specific type. They shall consist of uppercase letters, 
digits, underscore characters, and possibly an exclamation mark. 
The "!" shall appear only once, and only as the first character.
***************************/
const char* 
GetKeyword(istream& in, const char* delims, ErrorDescriptor &err)
{
    char c;
    int sz = 1;
    static std::string str;

    str = "";
    in.get(c);
    while (  !((isspace(c)) || (strchr(delims,c)) )  )
    {
	//check to see if the char is valid
	if (!( (isupper(c)) ||
	       (isdigit(c)) ||
	       (c == '_')   ||
	       (c == '-')   ||    //for reading 'ISO-10303-21'
	       ((c == '!') && (sz == 1)) ) )
	{
	    cerr << "Error: Invalid character \'" << c <<
		"\' in GetKeyword.\nkeyword was: " << str << "\n";
	    err.GreaterSeverity(SEVERITY_WARNING);
	    in.putback(c);
	    return const_cast<char *>(str.c_str());
	}
	if (!in.good()) break;  //BUG: should do something on eof()
	str += c;
	++sz;
	in.get(c);
    }
    in.putback(c);
    return const_cast<char *>(str.c_str());
}

// return 1 if found the keyword 'ENDSEC' with optional space and a ';' 
// otherwise return 0. This gobbles up the input stream until it knows 
// that it does or does not have the ENDSEC keyword (and semicolon). i.e.
// the first character that stops matching the keyword ENDSEC; (including the 
// semicolon) will be put back onto the istream, everything else will remain 
// read.  It is this way so that checking for the keywd the next time will
// start with the correct char or if it doesn't find it the non-matching char
// may be what you are looking for next (e.g. someone typed in END; and the 
// next chars are DATA; for the beginning of the data section).

int  
FoundEndSecKywd(istream& in, ErrorDescriptor &err)
{
    char c;
    in >> ws;
    in.get(c);

    if(c == 'E')
    {
        in.get(c);
	if(c == 'N')
	{
	    in.get(c);
	    if(c == 'D')
	    {
		in.get(c);
		if(c == 'S')
		{
		    in.get(c);
		    if(c == 'E')
		    {
			in.get(c);
			if(c == 'C')
			{
			    in >> ws;
			    in.get(c);
			    if(c == ';')
			    {
				return 1;
			    }
			    else
			        in.putback(c);
			}
			else
			    in.putback(c);
		    }
		    else
			in.putback(c);
		}
		else
		    in.putback(c);
	    }
	    else
		in.putback(c);
	}
	else
	    in.putback(c);
    }
    else
	in.putback(c);
    // error
    return 0;
}

// Skip over everything in s until the start of a comment. If it is a well
// formatted comment append the comment (including delimiters) to 
// std::string ss.  Return a pointer in s just past what was read as a comment.
// If no well formed comment was found, a pointer to the null char in s is 
// returned.  If one is found ss is appended with it and a pointer just
// past the comment in s is returned. Note* a carraige return ('\n') is added
// after the comment that is appended.
const char * ReadComment(std::string &ss, const char *s)
{
    std::string ssTmp;
    int endComment = 0;

    if(s)
    {
	while(*s && *s != '/') s++; // skip leading everything
	if(*s == '/')
	{
	    s++;
	    if(*s == '*') // found a comment
	    {
		ssTmp.append("/*");
		s++;
		while(*s && !endComment)
		{
		    if(*s == '*')
		    {
			ssTmp += *s;
			s++;
			if(*s == '/')
			{
			    endComment = 1;
			    ssTmp += *s;
			    ssTmp.append("\n");
			}
			else
			    s--;
		    }
		    else 
			ssTmp += *s;
		    s++;
		}
	    }
	}
	if(endComment)
	    ss.append(ssTmp.c_str());
    }
    return s;
}

/***************************
 * Reads a comment. If there is a comment it is returned as 
 * char * in space provided in std::string s. 
 * If there is not a comment returns null pointer.
 * After skipping white space it expects a slash followed by
 * an asterisk (which I didn't type to avoid messing up the 
 * compiler).  It ends with asterisk followed by slash.
 * If first char from 'in' is a slash it will remain read
 * whether or not there was a comment.  If there is no comment
 * only the slash will be read from 'in'.
***************************/

const char * 
ReadComment(istream& in, std::string &s)
{
    char c = '\0';
    int commentLength = 0;
    in >> ws;
    in >> c;

    // it looks like a comment so far
    if (c == '/') // leave slash read from stream
    {
	in.get(c);	// won't skip space
	if (c == '*')   // it is a comment
	{
	    in >> ws; // skip leading comment space

	    // only to keep it from completely gobbling up input
	    while( commentLength <= MAX_COMMENT_LENGTH ) 
	    {
		in.get (c);
		if(c == '*') // looks like start of end comment
		{
		    in.get(c);
		    if(c == '/') // it is end of comment
			return const_cast<char *>(s.c_str()); // return comment as a string
		    else	// it is not end of comment
		    {   	// so store the * and put back the other char
			s.append("*");
			in.putback(c);
			commentLength++;
		    }
		}
		else
		{
		    s += c;
		    commentLength++;
		}
	    } // end while
	    cout << "ERROR comment longer than maximum comment length of " 
		 << MAX_COMMENT_LENGTH << "\n" 
		 << "Will try to recover...\n";
	    std::string tmp;
	    SkipInstance(in, tmp);
	    return const_cast<char *>(s.c_str());
	}
	// leave slash read from stream... assume caller already knew there was
	//  a slash, leave it off stream so they don't think this funct needs 
	// to be called again
	else // not a comment
	    in.putback(c); // put non asterisk char back on input stream
    }
    else // first non-white char is not a slash
	in.putback(c); // put non slash char back on input stream

    return 0; // no comment string to return
}

/***************************
 ** Read a Print Control Directive from the istream
 ** "\F\" == formfeed
 ** "\N\" == newline
 ***************************/
Severity
ReadPcd(istream& in) 
{
    char c;
    in.get(c);
    if (c == '\\') { 
	in.get(c);
	if (c == 'F' || c == 'N') 
	  {
	      in.get(c);
	      if (c == '\\') { in.get(c); return SEVERITY_NULL; }
	  }
    }
    cerr << "Print control directive expected.\n";
    return SEVERITY_WARNING;
}
 

/******************************
This function reads through token separators
from an istream. It returns when a token
separator is not the next thing on the istream.
The token separators are blanks, explicit print control directives,
and comments.
Part 21 considers the blank to be the space character, 
but this function considers blanks to be the return value of isspace(c)
******************************/
void 
ReadTokenSeparator(istream& in, std::string *comments)
{
    char c;
    std::string s; // used if need to read a comment
    const char *cstr = 0;

    if (in.eof()) 
    {
	//BUG: no error message is reported
	return;
    }

    while (in)
    {
	in >> ws; // skip white space.
	c = in.peek(); // look at next char on input stream

	switch (c) 
	{
	  case '/': // read p21 file comment
	    s.clear();
	    ReadComment(in, s);
	    if(!s.empty() && comments)
	    {
		comments->append("/*");
		comments->append(s.c_str());
		comments->append("*/\n");
	    }
	    break;

	  case '\\': // try to read a print control directive
	    ReadPcd(in);
	    break;

	  default:
	    return;
	}
    }
}
