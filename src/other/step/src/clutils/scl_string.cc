
/*
* NIST Utils Class Library
* clutils/scl_string.cc
* April 1997
* K. C. Morris
* David Sauder

* Development of this software was funded by the United States Government,
* and is not subject to copyright.
*/

/* $Id: scl_string.cc,v 3.0.1.2 1997/11/05 22:33:51 sauderd DP3.1 $ */

#include <scl_string.h>
#include <stdio.h>
/*******************************************************************/
void 
SCLstring::PrintContents(ostream &out) const
{
    if(_strBuf)
    {
	out << "_strBuf in parens" << endl << "(";
	out << _strBuf << ")" << endl;
    }
    else
      out << "_strBuf is a null pointer" << endl;
    out << "_strBufSize: " << _strBufSize << endl;
    out << "_max_len: " << _max_len << endl;
}

int SCLstring::newBufSize (int l) const
{
  int rval =  (((l + 1)/32) +1) * 32;

  // if there\'s a max and rval > max return max +1
  return (MaxLength () && (rval > MaxLength ())) ?  MaxLength ()+1 : rval;
}

//constructor
SCLstring::SCLstring (const char * str, int lim)
:  _max_len (lim)
{

  if (!str) { _strBufSize = 0; _strBuf = 0;  return;  }

  // check the length
  int l;
  if (lim && (strlen (str) > lim)) l = lim;
  else l = strlen (str);

  _strBufSize = newBufSize (l);
#ifdef __OSTORE__
  _strBuf = new (os_database::of(this), os_typespec::get_char(), _strBufSize) 
		char[_strBufSize];
#else
  _strBuf = new char[_strBufSize];
#endif
  strncpy (_strBuf, str, l);
  _strBuf [l] = '\0';

}	

SCLstring::SCLstring (const SCLstring& s)  
:  _max_len (s.MaxLength ())
{
  if (s.is_undefined ()) { _strBufSize = 0; _strBuf = 0;  return;  }
//  int len = newBufSize (s.Length ());
//  _strBuf = new char [len];
//  strncpy (_strBuf, s, len);
//  _strBuf [len] = '\0';
  _strBufSize = newBufSize (s.Length ());

#ifdef __OSTORE__
  _strBuf = new (os_database::of(this), os_typespec::get_char(), _strBufSize) 
		char[_strBufSize];
#else
  _strBuf = new char[_strBufSize];
#endif
  strncpy (_strBuf, s, _strBufSize);
  _strBuf [_strBufSize-1] = '\0';
}  

//destructor
SCLstring::~SCLstring()
{
    if(_strBufSize)
    {
	delete [] _strBuf;
//	_strbuf = 0;
    }
}

//operators

SCLstring& SCLstring::operator= (const char* str) 
{
  if (!str) { 
    _strBufSize = 0; delete [] _strBuf; _strBuf =0;  
  }
  else {
    int slen = strlen (str);
    if(!_strBuf || (StrBufSize () < newBufSize(slen+1) ) )  {
      // make more room
      // first clean up the space
      if (_strBuf)  delete [] _strBuf;
      _strBufSize = newBufSize (slen+1);
#ifdef __OSTORE__
      _strBuf = new (os_database::of(this), os_typespec::get_char(), 
		     _strBufSize) char[_strBufSize];
#else
      _strBuf = new char[_strBufSize];
#endif
      }
    strncpy (_strBuf, str, _strBufSize -1);
    _strBuf [_strBufSize -1] = '\0';
  }
    return *this;
}

// Josh L, 4/11/95
SCLstring& SCLstring::operator= (const SCLstring& s)
{
    // changed from sending s.chars() so undefined will be set correctly - DAS
//  _max_len = s.MaxLength(); // Do we want this to happen? - DAS
  operator=(s.rep());
//  _strBufSize = s.StrBufSize(); // this seems wrong - DAS
  return *this;
}

SCLstring::operator const char * ()  const
{
    if (_strBuf) return _strBuf;
    else return "";
}

SCLstring& 
SCLstring::Append (const char * s)  
{
  if (!s) return *this;
  int oldlen = Length ();
  int slen = strlen (s);
  int l = oldlen + slen +1;
  if (_strBuf && (l < _strBufSize))  strncat (_strBuf, s, slen+1);
  else {  // make more space
    int sz = newBufSize (l);

#ifdef __OSTORE__
    char * tmp = new (os_database::of(this), os_typespec::get_char(), sz) 
			char[sz];
#else
    char * tmp = new char [sz];
#endif
    tmp [0] ='\0';

    _strBufSize = sz;
    if (_strBuf) strcpy (tmp, _strBuf );
    l = (( slen > (sz-oldlen) ) ? sz-oldlen : slen);
    strncat (tmp, s, l);
    *(tmp+oldlen+l) = '\0';  
    delete [] _strBuf;
    _strBuf = tmp;
    }
  return *this;
}

SCLstring& 
SCLstring::Append (const char c)  
{
  char tmp [2];
  tmp [0] = c;
  tmp [1] = '\0';
  return Append (tmp);
}
   
SCLstring& 
SCLstring::Append (const long int i)
{
  char tmp [BUFSIZ];
  sprintf (tmp, "%ld", i);
  return Append (tmp);
}

SCLstring& 
SCLstring::Append (const int i)
{
  char tmp [BUFSIZ];
  sprintf (tmp, "%d", i);
  return Append (tmp);
}

SCLstring& 
SCLstring::Append (const double i, const int prec)
{
    char tmp [BUFSIZ];
//  sprintf (tmp, "%.*g", prec, i);
	// replace the above line with this code so that writing the '.' is
	// guaranteed for reals. If you use e or E then you get many 
	// unnecessary trailing zeros. g and G truncates all trailing zeros
	// to save space but when no non-zero precision exists it also 
	// truncates the decimal. The decimal is required by Part 21. 
	// Also use G instead of g since G writes uppercase E (E instead of e 
	// is also required by Part 21) when scientific notation is used - DAS

    sprintf(tmp, "%.*G", prec, i);
    if(!strchr(tmp, '.'))
    {
	if(strchr(tmp, 'E') || strchr(tmp, 'e'))
	{
	    char *expon = strchr(tmp, 'E');

	    if( !expon)
	    {
		expon = strchr(tmp, 'e');
	    }
	    *expon = '\0';
	    Append(tmp);
	    Append('.');
	    Append('E');
	    expon++;
	    Append(expon);
	}
	else 
	{
	    int rindex = strlen(tmp);
	    tmp[rindex] = '.';
	    tmp[rindex+1] = '\0';
	    Append(tmp);
	}
	return *this;
    }
    return Append (tmp);
} 

SCLstring& 
SCLstring::Prepend (const char * s)  
{
  if (!_strBuf)  {  // no string 
    int sz = newBufSize (strlen (s));
#ifdef __OSTORE__
    _strBuf = new (os_database::of(this), os_typespec::get_char(), sz) 
			char[sz];
#else
    _strBuf = new char [sz];  
#endif

    sz = sz-1;
    strncpy (_strBuf, s, sz);
    _strBuf [sz-1] = '\0';
    return *this;
  }
  //  otherwise make some space
  int slen = strlen (s);
  int sz = newBufSize (slen + Length () );

#ifdef __OSTORE__
  char * nstr = new (os_database::of(this), os_typespec::get_char(), sz) 
			char[sz];
#else
  char * nstr = new char [sz];  
#endif

  strncpy (nstr, s, sz-1);
  if (slen < sz)  // copy the rest
    strncat (nstr, _strBuf, sz-slen);
  nstr [sz -1] = '\0';  // make sure it\'s null terminated
  delete [] _strBuf;
  _strBuf = nstr;
  return *this;
}
   

int 
SCLstring::operator== (const char* s) const
{
  return !strcmp (_strBuf, s);  
}

// is_null returns true if the string doesn't exist or if it is empty.
// If is_null() returns true you may also want to call is_undefined() to 
// see if it is undefined.
int 
SCLstring::is_null() const
{
     if (_strBuf) return _strBuf[0] == '\0';
     else return 1;
}

// If the buffer doesn't exist SCLstring returns true for is_null() and 
// and is_undefined().
int
SCLstring::is_undefined() const
{
  return !_strBuf;
}

// see also set_null()
void
SCLstring::set_undefined()
{
    delete [] _strBuf;
    _strBuf = 0;
    _strBufSize = 0;
}

// see also set_undefined()
int 
SCLstring::set_null() 
{
  for (int i =0; i < _strBufSize; ++i)
    *(_strBuf +i) = '\0';
  return 1;
/*
  delete [] _strBuf;
  _strBuf =0;
  _strBufSize = 0;
  return 1;
  */
}

int
SCLstring::StrBufSize() const
{ return _strBufSize; }

int
SCLstring::Length() const
{ 
  if (_strBuf) return strlen (_strBuf); 
  else return 0;
}
