/* $Header$ */
/* $NoKeywords: $ */
/*
//
// Copyright (c) 1993-2001 Robert McNeel & Associates. All rights reserved.
// Rhinoceros is a registered trademark of Robert McNeel & Assoicates.
//
// THIS SOFTWARE IS PROVIDED "AS IS" WITHOUT EXPRESS OR IMPLIED WARRANTY.
// ALL IMPLIED WARRANTIES OF FITNESS FOR ANY PARTICULAR PURPOSE AND OF
// MERCHANTABILITY ARE HEREBY DISCLAIMED.
//				
// For complete openNURBS copyright information see <http://www.opennurbs.org>.
//
////////////////////////////////////////////////////////////////
*/

#include "opennurbs.h"

// wide char (unicode) <-> char (ascii) converter
static int w2c_size( int, const wchar_t* ); // gets minimum "c_count" arg for w2c().
static int w2c( int,            // w_count = number of wide chars to convert
                const wchar_t*, // source wide char string
                int,            // c_count, 
                char*           // array of at least c_count+1 characters
                );
static int c2w( int,           // c_count = number of chars to convert
                const char*,   // source byte char string
                int,           // w_count, 
                wchar_t*       // array of at least c_count+1 wide characters
                );
static wchar_t c2w( char );

static int w2c_size( int w_count, const wchar_t* w )
{
  // returns number of bytes used in wide conversion.  Does not
  // include NULL terminator.
  int rc = 0;
  if ( w ) {
	  rc = on_WideCharToMultiByte(w, w_count, NULL, 0);
    if ( rc < 0 )
      rc = 0;
  }
  return rc;
}

static int w2c( int w_count, 
                const wchar_t* w, 
                int c_count, 
                char* c // array of at least c_count+1 characters
                )
{
  int rc = 0;
  if ( c ) 
    c[0] = 0;
  // returns length of converted c[]
  if ( c_count > 0 && c ) {
    c[0] = 0;
    if ( w ) {
	    rc = on_WideCharToMultiByte(w, w_count, c, c_count);
      if ( rc > 0 && rc <= c_count )
        c[rc] = 0;
      else {
        c[c_count] = 0;
        rc = 0;
      }
    }
  }
	return rc;
}

static wchar_t c2w( char c )
{
  wchar_t w[2] = {0,0};
  c2w(1,&c,1,w);
  return w[0];
}

static int c2w( int c_count, 
                const char* c, 
                int w_count, 
                wchar_t* w // array of at least c_count+1 wide characters
                )
{
  int rc = 0;
  if ( w ) 
    w[0] = 0;
  // returns length of converted c[]
  if ( w_count > 0 && w && c_count > 0 && c && c[0] ) {
    w[0] = 0;
    if ( c ) {
	    rc = on_MultiByteToWideChar(c, c_count, w, w_count);
      if ( rc > 0 && rc <= w_count )
        w[rc] = 0;
      else {
        w[w_count] = 0;
        rc = 0;
      }
    }
  }
	return rc;
}


void ON_String::CopyToArray( int w_count, const wchar_t* w )
{
  // convert UNICODE string to ASCII string
  int c_count = w2c_size( w_count, w );
  char* c = (char*)onmalloc(c_count+1);
  memset( c, 0, c_count+1 );
  const int c_length = w2c( w_count, w, c_count, c );
  c[c_length] = 0;
  CopyToArray( c_count, c );
  onfree(c);
}



/////////////////////////////////////////////////////////////////////////////
// Empty strings point at empty_wstring

static struct {
  ON_wStringHeader header;
  wchar_t           s;  
} empty_wstring = { {-1, 0, 0}, 0 }; // ref_count=-1, length=0, capacity=0, s=0 
static ON_wStringHeader* pEmptyStringHeader = &empty_wstring.header;
static const wchar_t* pEmptywString = &empty_wstring.s;

//////////////////////////////////////////////////////////////////////////////
// protected helpers

void ON_wString::Create()
{
  m_s = (wchar_t*)pEmptywString;
}

ON_wStringHeader* ON_wString::Header() const
{
  ON_wStringHeader* p = (ON_wStringHeader*)m_s;
  if (p)
    p--;
  else
    p = pEmptyStringHeader;
  return p;
}

void ON_wString::CreateArray( int capacity )
{
  Destroy();
  if ( capacity > 0 ) {
		ON_wStringHeader* p =
			(ON_wStringHeader*)onmalloc( sizeof(ON_wStringHeader) + (capacity+1)*sizeof(*m_s) );
		p->ref_count = 1;
		p->string_length = 0;
		p->string_capacity = capacity;
		m_s = p->string_array();
    memset( m_s, 0, (capacity+1)*sizeof(*m_s) );
  }
}

void ON_wString::Destroy()
{
  ON_wStringHeader* p = Header();
  if ( p != pEmptyStringHeader && p->ref_count > 0 ) {
    p->ref_count--;
		if ( p->ref_count == 0 )
			onfree(p);
  }
	Create();
}

void ON_wString::Empty()
{
  ON_wStringHeader* p = Header();
  if ( p != pEmptyStringHeader ) {
    if ( p->ref_count > 1 ) {
      // string memory is shared
      p->ref_count--;
	    Create();
    }
    else if ( p->ref_count == 1 ) {
      // string memory is not shared - reuse it
      if (m_s && p->string_capacity>0)
        *m_s = 0;
      p->string_length = 0;
    }
    else {
      // should not happen
      ON_ERROR("ON_wString::Empty() encountered invalid header - fixed.");
      Create();
    }
  }
  else {
    // initialized again
	  Create();
  }
}

void ON_wString::EmergencyDestroy()
{
	Create();
}

void ON_wString::CopyArray()
{
  // If 2 or more string are using array, it is duplicated.
  // Call CopyArray() before modifying array contents.
  ON_wStringHeader* p = Header();
  if ( p != pEmptyStringHeader && p && p->ref_count > 1 ) {
    const wchar_t* s = m_s;
    Destroy();
    Create();
    CopyToArray( p->string_capacity, s );
  }
}

void ON_wString::ReserveArray( size_t array_capacity )
{
  ON_wStringHeader* p = Header();
  const int capacity = (int)array_capacity; // for 64 bit compiler
  if ( p == pEmptyStringHeader ) 
  {
		CreateArray(capacity);
  }
  else if ( p->ref_count > 1 ) 
  {
		CreateArray(capacity);
    ON_wStringHeader* p1 = Header();
    const int size = (capacity < p->string_length) ? capacity : p->string_length;
    if ( size > 0 ) 
    {
      memcpy( p1->string_array(), p->string_array(), size*sizeof(*m_s) );
      p1->string_length = size;
    }
  }
	else if ( capacity > p->string_capacity ) 
  {
		p = (ON_wStringHeader*)onrealloc( p, sizeof(ON_wStringHeader) + (capacity+1)*sizeof(*m_s) );
    m_s = p->string_array();
    memset( &m_s[p->string_capacity], 0, (1+capacity-p->string_capacity)*sizeof(*m_s) );
    p->string_capacity = capacity;
	}
}

void ON_wString::ShrinkArray()
{
  ON_wStringHeader* p = Header();
  if ( p != pEmptyStringHeader ) {
    if ( p->string_length < 1 ) {
      Destroy();
    }
    else if ( p->ref_count > 1 ) {
      // shared string
      CreateArray(p->string_length);
		  ON_wStringHeader* p1 = Header();
      memcpy( m_s, p->string_array(), p->string_length*sizeof(*m_s));
      p1->string_length = p->string_length;
      m_s[p1->string_length] = 0;
    }
	  else if ( p->string_length < p->string_capacity ) {
      // onrealloc string
		  p = (ON_wStringHeader*)onrealloc( p, sizeof(ON_wStringHeader) + (p->string_length+1)*sizeof(*m_s) );
      p->string_capacity = p->string_length;
      m_s = p->string_array();
      m_s[p->string_length] = 0;
	  }
  }
}

void ON_wString::CopyToArray( const ON_wString& s )
{
  CopyToArray( s.Length(), s.Array() );
}

void ON_wString::CopyToArray( int size, const char* s )
{
  if ( size > 0 && s && s[0] ) {
	  ReserveArray(size);
    Header()->string_length = c2w( size, s, Header()->string_capacity, m_s );
    m_s[Header()->string_length] = 0;
  }
  else {
    if ( Header()->ref_count > 1 )
      Destroy();
    else {
      Header()->string_length = 0;
      m_s[0] = 0;
    }
  }
}

void ON_wString::CopyToArray( int size, const unsigned char* s )
{
  CopyToArray( size, ((char*)s) );
}

void ON_wString::CopyToArray( int size, const wchar_t* s )
{
  if ( size > 0 && s && s[0] ) {
	  ReserveArray(size);
	  memcpy(m_s, s, size*sizeof(*m_s));
	  Header()->string_length = size;
    m_s[Header()->string_length] = 0;
  }
  else {
    if ( Header()->ref_count != 1 )
      Destroy();
    else {
      Header()->string_length = 0;
      m_s[0] = 0;
    }
  }
}

void ON_wString::AppendToArray( const ON_wString& s )
{
  AppendToArray( s.Length(), s.Array() );
}

void ON_wString::AppendToArray( int size, const char* s )
{
  if ( size > 0 && s && s[0] ) {
	  ReserveArray(size + Header()->string_length );
    Header()->string_length += c2w( size, s, Header()->string_capacity-Header()->string_length, &m_s[Header()->string_length] );
    m_s[Header()->string_length] = 0;
  }
}

void ON_wString::AppendToArray( int size, const unsigned char* s )
{
  AppendToArray( size, ((char*)s) );
}

void ON_wString::AppendToArray( int size, const wchar_t* s )
{
  if ( size > 0 && s && s[0] ) {
	  ReserveArray(size + Header()->string_length );
	  memcpy(&m_s[Header()->string_length], s, size*sizeof(*m_s));
	  Header()->string_length += size;
    m_s[Header()->string_length] = 0;
  }
}


int ON_wString::Length( const char* s )
{
  int n = (int)((s) ? strlen(s) : 0); // the (int) cast is for 64 bit size_t conversion
  if ( n < 0 )
    n = 2147483646;
  return n;
}

int ON_wString::Length( const unsigned char* s )
{
  return ON_wString::Length((const char*)s);
}

int ON_wString::Length( const wchar_t* s )
{
  int n = (int)((s) ? wcslen(s) : 0); // the (int) cast is for 64 bit size_t conversion
  if ( n < 0 )
    n = 2147483646;
  return n;
}

//////////////////////////////////////////////////////////////////////////////
// Construction/Destruction

ON_wString::ON_wString()
{
	Create();
}

ON_wString::~ON_wString()
{
  Destroy();
}

ON_wString::ON_wString(const ON_wString& src)
{
	if ( src.Header()->ref_count > 0 )	{
		m_s = src.m_s;
    src.Header()->ref_count++;
	}
	else {
		Create();
		*this = src.m_s; // use operator=(const wchar_t*) to copy
	}
}

ON_wString::ON_wString(const ON_String& src)
{
	Create();
	*this = src;
}

ON_wString::ON_wString( const char* s )
{
	Create();
  if ( s && s[0] ) 
  {
    CopyToArray( (int)strlen(s), s ); // the (int) is for 64 bit size_t conversion
  }
}

ON_wString::ON_wString( const char* s, int length )
{
	Create();
  if ( s && length > 0 ) {
    CopyToArray(length,s);
	}
}

ON_wString::ON_wString( char c, int repeat_count )
{
  Create();
  if ( repeat_count > 0 ) {
    char* s = (char*)onmalloc((repeat_count+1)*sizeof(*s));
    s[repeat_count] = 0;
    memset( s, c, repeat_count*sizeof(*s) );
    CopyToArray( repeat_count, s );
    onfree(s);
    m_s[repeat_count] = 0;
    Header()->string_length = repeat_count;
  }
}

ON_wString::ON_wString( const unsigned char* s )
{
	Create();
  if ( s && s[0] ) {
    CopyToArray( (int)strlen((const char*)s), (const char*)s ); // the (int) is for 64 bit size_t conversion
  }
}

ON_wString::ON_wString( const unsigned char* s, int length )
{
	Create();
  if ( s && length > 0 ) {
    CopyToArray(length,s);
	}
}

ON_wString::ON_wString( unsigned char c, int repeat_count )
{
  Create();
  if ( repeat_count > 0 ) {
    char* s = (char*)onmalloc((repeat_count+1)*sizeof(*s));
    s[repeat_count] = 0;
    memset( s, c, repeat_count*sizeof(*s) );
    CopyToArray( repeat_count, s );
    onfree(s);
    m_s[repeat_count] = 0;
    Header()->string_length = repeat_count;
  }
}


ON_wString::ON_wString( const wchar_t* s )
{
	Create();
  if ( s && s[0] ) {
    CopyToArray( (int)wcslen(s), s ); // the (int) is for 64 bit size_t conversion
  }
}

ON_wString::ON_wString( const wchar_t* s, int length )
{
	Create();
  if ( s && length > 0 ) {
    CopyToArray( length, s );
	}
}

ON_wString::ON_wString( wchar_t c, int repeat_count )
{
  int i;
  Create();
  if ( repeat_count > 0 ) {
    ReserveArray(repeat_count);
    for (i=0;i<repeat_count;i++)
      m_s[i] = c;
    m_s[repeat_count] = 0;
    Header()->string_length = repeat_count;
  }
}

#if defined(ON_OS_WINDOWS)
bool ON_wString::LoadResourceString(HINSTANCE instance, UINT id )
{
  bool rc = false;
  wchar_t s[2048]; // room for 2047 characters
  int length;

  Destroy();
  length = ::LoadStringW( instance, id, s, 2047 );
  if ( length > 0 && length < 2048 ) {
    CopyToArray( length, s );
    rc = TRUE;
  }
  return rc;
}
#endif

int ON_wString::Length() const
{
  return Header()->string_length;
}

wchar_t& ON_wString::operator[](int i)
{
  return m_s[i];
}

wchar_t ON_wString::operator[](int i) const
{
  return m_s[i];
}

bool ON_wString::IsEmpty() const
{
  return (Header()->string_length <= 0 ) ? TRUE : FALSE;
}

const ON_wString& ON_wString::operator=(const ON_wString& src)
{
	if (m_s != src.m_s)	
  {
    if ( src.IsEmpty() ) 
    {
      Destroy();
      Create();
    }
    else if ( src.Header()->ref_count > 0 ) 
    {
      Destroy();
      src.Header()->ref_count++;
      m_s = src.m_s;
    }
    else 
    {
      ReserveArray(src.Length());
      memcpy( m_s, src.Array(), src.Length()*sizeof(*m_s));
      Header()->string_length = src.Length();
    }
  }
	return *this;
}

const ON_wString& ON_wString::operator=(const ON_String& src)
{
  *this = src.Array();
  return *this;
}

const ON_wString& ON_wString::operator=( char c )
{
	CopyToArray( 1, &c );
	return *this;
}

const ON_wString& ON_wString::operator=( const char* s )
{
  if ( (void*)s != (void*)m_s )
	  CopyToArray( Length(s), s);
	return *this;
}

const ON_wString& ON_wString::operator=( unsigned char c )
{
	CopyToArray( 1, &c );
	return *this;
}

const ON_wString& ON_wString::operator=( const unsigned char* s )
{
  if ( (void*)s != (void*)m_s )
	  CopyToArray( Length(s), s);
	return *this;
}

const ON_wString& ON_wString::operator=( wchar_t c )
{
	CopyToArray( 1, &c );
	return *this;
}

const ON_wString& ON_wString::operator=( const wchar_t* s )
{
  if ( (void*)s != (void*)m_s )
	  CopyToArray( Length(s), s);
	return *this;
}

ON_wString ON_wString::operator+(const ON_wString& s2) const
{
	ON_wString s(*this);
  s.AppendToArray( s2 );
	return s;
}

ON_wString ON_wString::operator+(const ON_String& s2) const
{
	ON_wString s(*this);
  s.AppendToArray( s2.Length(), s2.Array() );
	return s;
}

ON_wString ON_wString::operator+(char s2 ) const
{
	ON_wString s(*this);
  s.AppendToArray( 1, &s2 );
	return s;
}

ON_wString ON_wString::operator+(unsigned char s2 ) const
{
	ON_wString s(*this);
  s.AppendToArray( 1, &s2 );
	return s;
}

ON_wString ON_wString::operator+( wchar_t s2 ) const
{
	ON_wString s(*this);
  s.AppendToArray( 1, &s2 );
	return s;
}

ON_wString ON_wString::operator+(const char* s2) const
{
	ON_wString s(*this);
  s.AppendToArray( ON_wString::Length(s2), s2 );
	return s;
}

ON_wString ON_wString::operator+(const unsigned char* s2) const
{
	ON_wString s(*this);
  s.AppendToArray( ON_wString::Length(s2), s2 );
	return s;
}

ON_wString ON_wString::operator+(const wchar_t* s2) const
{
	ON_wString s(*this);
  s.AppendToArray( ON_wString::Length(s2), s2 );
	return s;
}

//////////////////////////////////////////////////////////////////////////////
// operator+=()

void ON_wString::Append( const char* s , int count )
{
  // append specified number of characters
  if ( s && count > 0 )
    AppendToArray(count,s);
}

void ON_wString::Append( const unsigned char* s , int count )
{
  // append specified number of characters
  if ( s && count > 0 )
    AppendToArray(count,s);
}


void ON_wString::Append( const wchar_t* s, int count )
{
  // append specified number of characters
  if ( s && count > 0 )
    AppendToArray(count,s);
}

const ON_wString& ON_wString::operator+=(const ON_wString& s)
{
  AppendToArray(s);
	return *this;
}

const ON_wString& ON_wString::operator+=(const ON_String& s)
{
  AppendToArray( s.Length(), s.Array() );
	return *this;
}

const ON_wString& ON_wString::operator+=( char s )
{
  AppendToArray(1,&s);
	return *this;
}

const ON_wString& ON_wString::operator+=( unsigned char s )
{
  AppendToArray(1,&s);
	return *this;
}

const ON_wString& ON_wString::operator+=( wchar_t s )
{
  AppendToArray(1,&s);
	return *this;
}

const ON_wString& ON_wString::operator+=( const char* s )
{
  AppendToArray(Length(s),s);
	return *this;
}

const ON_wString& ON_wString::operator+=( const unsigned char* s )
{
  AppendToArray(Length(s),s);
	return *this;
}

const ON_wString& ON_wString::operator+=( const wchar_t* s )
{
  AppendToArray(Length(s),s);
	return *this;
}

void ON_wString::SetLength(size_t string_length)
{
  int length = (int)string_length; // for 64 bit compilers
  if ( length >= Header()->string_capacity ) {
    ReserveArray(length);
  }
  if ( length >= 0 && length <= Header()->string_capacity ) {
    CopyArray();
    Header()->string_length = length;
    m_s[length] = 0;
  }
}

wchar_t* ON_wString::Array()
{
  return ( Header()->string_capacity > 0 ) ? m_s : 0;
}

const wchar_t* ON_wString::Array() const
{
  return ( Header()->string_capacity > 0 ) ? m_s : 0;
}

int ON_wString::Compare( const char* s ) const
{
  int rc = 0;
  if ( s && s[0] ) {
    if ( IsEmpty() ) {
      rc = -1;
    }
    else {
      int c_count = w2c_size( Length(m_s), m_s );
      char* c = (char*)onmalloc((c_count+1)*sizeof(*c));
      w2c( Length(m_s), m_s, c_count, c );
      c[c_count] = 0;
      rc = strcmp( c, s );
      onfree(c);
    }
  }
  else {
    rc = IsEmpty() ? 0 : 1;
  }
  return rc;
}

int ON_wString::Compare( const unsigned char* s) const
{
  return ON_wString::Compare((const char*)s);
}

int ON_wString::Compare( const wchar_t* s ) const
{
  int rc = 0;
  if ( s && s[0] ) {
    if ( IsEmpty() ) {
      rc = -1;
    }
    else {
      rc = wcscmp( m_s, s );
    }
  }
  else {
    rc = IsEmpty() ? 0 : 1;
  }
  return rc;
}

int ON_wString::CompareNoCase( const char* s) const
{
  int rc = 0;
  if ( s && s[0] ) {
    if ( IsEmpty() ) {
      rc = -1;
    }
    else {
      int c_count = w2c_size( Length(m_s), m_s );
      char* c = (char*)onmalloc((c_count+1)*sizeof(*c));
      w2c( Length(m_s), m_s, c_count, c );
      c[c_count] = 0;
      rc = on_stricmp( c, s );
      onfree(c);
    }
  }
  else {
    rc = IsEmpty() ? 0 : 1;
  }
  return rc;
}

int ON_wString::CompareNoCase( const unsigned char* s) const
{
  return ON_wString::CompareNoCase((const char*)s);
}

int ON_wString::CompareNoCase( const wchar_t* s) const
{
  int rc = 0;
  if ( s && s[0] ) {
    if ( IsEmpty() ) {
      rc = -1;
    }
    else {
      rc = on_wcsicmp( m_s, s );
    }
  }
  else {
    rc = IsEmpty() ? 0 : 1;
  }
  return rc;
}


bool ON_WildCardMatch(const wchar_t* s, const wchar_t* pattern)
{
  if ( !pattern || !pattern[0] ) {
    return ( !s || !s[0] ) ? TRUE : FALSE;
  }

  if ( *pattern == '*' ) {
    pattern++;
    while ( *pattern == '*' )
      pattern++;
    
    if ( !pattern[0] )
      return TRUE;

    while (*s) {
      if ( ON_WildCardMatch(s,pattern) )
        return TRUE;
      s++;
    }

    return FALSE;
  }

  while ( *pattern != '*' )
  {
    if ( *pattern == '?' ) {
      if ( *s) {
        pattern++;
        s++;
        continue;
      }
      return FALSE;
    }
    
    if ( *pattern == '\\' ) {
      switch( pattern[1] )
      {
      case '*':
      case '?':
        pattern++;
        break;
      }
    }
    if ( *pattern != *s ) {
      return FALSE;
    }

    if ( *s == 0 )
      return TRUE;

    pattern++;
    s++;
  }
  
  return ON_WildCardMatch(s,pattern);
}


bool ON_WildCardMatchNoCase(const wchar_t* s, const wchar_t* pattern)
{
  if ( !pattern || !pattern[0] ) {
    return ( !s || !s[0] ) ? TRUE : FALSE;
  }

  if ( *pattern == '*' ) {
    pattern++;
    while ( *pattern == '*' )
      pattern++;
    
    if ( !pattern[0] )
      return TRUE;

    while (*s) {
      if ( ON_WildCardMatch(s,pattern) )
        return TRUE;
      s++;
    }

    return FALSE;
  }

  while ( *pattern != '*' )
  {
    if ( *pattern == '?' ) {
      if ( *s) {
        pattern++;
        s++;
        continue;
      }
      return FALSE;
    }
    
    if ( *pattern == '\\' ) {
      switch( pattern[1] )
      {
      case '*':
      case '?':
        pattern++;
        break;
      }
    }
    if ( towupper(*pattern) != towupper(*s) ) {
      return FALSE;
    }

    if ( *s == 0 )
      return TRUE;

    pattern++;
    s++;
  }
  
  return ON_WildCardMatch(s,pattern);
}

bool ON_wString::WildCardMatch( const wchar_t* pattern ) const
{
  return ON_WildCardMatch(m_s,pattern);
}


bool ON_wString::WildCardMatchNoCase( const wchar_t* pattern ) const
{
  return ON_WildCardMatchNoCase(m_s,pattern);
}

/*
static TestReplace()
{
  int len, len1, len2, i, count, gap, k, i0, repcount, replen;
  ON_wString str;

  bool bRepeat = false;

  wchar_t s[1024], token1[1024], token2[1024];

  memset(s,     0,1024*sizeof(s[0]));
  memset(token1,0,1024*sizeof(token1[0]));
  memset(token2,0,1024*sizeof(token2[0]));

	for ( len = 1; len < 32; len++ )
  {
    for ( len1 = 1; len1 < len+1; len1++ )
    {
      if ( len1 > 0 )
        token1[0] = '<';
      for ( i = 1; i < len1-1; i++ )
        token1[i] = '-';
      if ( len1 > 1 )
        token1[len1-1] = '>';
      token1[len1] = 0;

      for ( len2 = 1; len2 < len1+5; len2++ )
      {
        if ( len2 > 0 )
          token2[0] = '+';
        for ( i = 1; i < len2-1; i++ )
          token2[i] = '=';
        if ( len2 > 1 )
          token2[len2-1] = '*';
        token2[len2] = 0;

        for ( k = 1; k*len1 <= len+1; k++ )
        {
          gap = (len/k) - len1;
          if (0 == len1 && gap < 1 )
            gap = 1;
          else if ( gap < 0 )
            gap = 0;
          bRepeat = false;
          for ( i0 = 0; i0 < 2*len1 + gap; i0++ )
          {
            for ( i = 0; i < len; i++ )
            {
              s[i] = (wchar_t)('a' + (i%26));
            }
            s[len] = 0;
            count = 0;
            for ( i = i0; i+len1 <= len; i += (gap+len1) )
            {
              memcpy(&s[i],token1,len1*sizeof(s[0]));
              count++;
            }
            str = s;
            repcount = str.Replace(token1,token2);
            replen = str.Length();
            if ( repcount != count || replen != len + count*(len2-len1) )
            {
              RhinoApp().Print(L"%s -> %s failed\n",token1,token2);
              RhinoApp().Print(L"%s (%d tokens, %d chars)\n",s,count,len);
              RhinoApp().Print(L"%s (%d tokens, %d chars)\n",str.Array(),repcount,replen);
              if ( bRepeat )
              {
                bRepeat = false;
              }
              else
              {
                bRepeat = true;
                i0--;
              }
            }
          }
          bRepeat = false;
        }
      }
    }
  }
}
*/

int ON_wString::Replace( const wchar_t* token1, const wchar_t* token2 )
{
  int count = 0;

  if ( 0 != token1 && 0 != token1[0] )
  {
    if ( 0 == token2 )
      token2 = L"";
    const int len1 = (int)wcslen(token1);
    if ( len1 > 0 )
    {
      const int len2 = (int)wcslen(token2);
      int len = Length();
      if ( len >= len1 )
      {
        // in-place
        ON_SimpleArray<int> n(32);
        wchar_t* s = m_s;
        int i;
        for ( i = 0; i <= len-len1; /*empty*/ )
        {
          if ( wcsncmp(s,token1,len1) )
          {
            s++;
            i++;
          }
          else
          {
            n.Append(i);
            i += len1;
            s += len1;
          }
        }

        count = n.Count();

        // reserve array space - must be done even when len2 <= len1
        // so that shared arrays are not corrupted.
        const int newlen = len + (count*(len2-len1));
        if ( 0 == newlen )
        {
          Destroy();
          return count;
        }

        // 24 August 2006 Dale Lear
        //    This used to say
        //       ReserveArray(newlen);
        //    but when newlen < len and the string had multiple
        //    references, the ReserveArray(newlen) call truncated
        //    the input array.  
        ReserveArray( (newlen<len) ? len : newlen );

        int i0, i1, ni, j;

        if ( len2 > len1 )
        {
          // copy from back to front
          i1 = newlen;
          i0 = len;
          for ( ni =0; ni < count; ni++ )
            n[ni] = n[ni] + len1;
          for ( ni = count-1; ni >= 0; ni-- )
          {
            j = n[ni];
            while ( i0 > j )
            {
              i0--;
              i1--;
              m_s[i1] = m_s[i0];
            }
            i1 -= len2;
            i0 -= len1;
            memcpy(&m_s[i1],token2,len2*sizeof(m_s[0]));
          }
        }
        else
        {
          // copy from front to back
          i0 = i1 = n[0];
          n.Append(len);
          for ( ni = 0; ni < count; ni++ )
          {
            if ( len2 > 0 )
            {
              memcpy(&m_s[i1],token2,len2*sizeof(m_s[0]));
              i1 += len2;
            }
            i0 += len1;
            j = n[ni+1];
            while ( i0 < j )
            {
              m_s[i1++] = m_s[i0++];
            }
          }
        }
        Header()->string_length = newlen;
        m_s[newlen] = 0;
      }
    }
  }

  return count;
}

int ON_wString::Replace( wchar_t token1, wchar_t token2 )
{
  int count = 0;
  int i = Length();
  while (i--)
  {
    if ( token1 == m_s[i] )
    {
      m_s[i] = token2;
      count++;
    }
  }
  return count;
}

static bool IsWhiteSpaceHelper( wchar_t c, const wchar_t* whitespace )
{
  while ( *whitespace )
  {
    if ( c == *whitespace++ )
      return true;
  }
  return false;
}

int ON_wString::ReplaceWhiteSpace( wchar_t token, const wchar_t* whitespace )
{
  wchar_t* s0;
  wchar_t* s1;
  int n;
  wchar_t c;

  if ( 0 == (s0 = m_s) )
    return 0;
  s1 = s0 + Length();
  if ( whitespace && *whitespace )
  {
    while( s0 < s1 )
    {
      if (IsWhiteSpaceHelper(*s0++,whitespace))
      {
        // need to modify this string
        n = ((int)(s0 - m_s)); // keep win64 happy with (int) cast
        CopyArray(); // may change m_s if string has multiple refs
        s0 = m_s + n;
        s1 = m_s + Length();
        s0[-1] = token;
        n = 1;
        while ( s0 < s1 )
        {
          if ( IsWhiteSpaceHelper(*s0++,whitespace) )
          {
            s0[-1] = token;
            n++;
          }
        }
        return n;
      }
    }
  }
  else
  {
    while( s0 < s1 )
    {
      c = *s0++;
      if ( (1 <= c && c <= 32) || 127 == c )
      {
        // need to modify this string
        n = ((int)(s0 - m_s)); // keep win64 happy with (int) cast
        CopyArray(); // may change m_s if string has multiple refs
        s0 = m_s + n;
        s1 = m_s + Length();
        s0[-1] = token;
        n = 1;
        while ( s0 < s1 )
        {
          c = *s0++;
          if ( (1 <= c && c <= 32) || 127 == c )
          {
            s0[-1] = token;
            n++;
          }
        }
        return n;
      }
    }
  }
  return 0;
}

int ON_wString::RemoveWhiteSpace( const wchar_t* whitespace )
{
  wchar_t* s0;
  wchar_t* s1;
  wchar_t* s;
  int n;
  wchar_t c;

  if ( 0 == (s0 = m_s) )
    return 0;
  s1 = s0 + Length();
  if ( whitespace && *whitespace )
  {
    while( s0 < s1 )
    {
      if (IsWhiteSpaceHelper(*s0++,whitespace))
      {
        // need to modify this string
        n = ((int)(s0 - m_s)); // keep win64 happy with (int) cast
        CopyArray(); // may change m_s if string has multiple refs
        s0 = m_s + n;
        s = s0-1;
        s1 = m_s + Length();
        while ( s0 < s1 )
        {
          if ( !IsWhiteSpaceHelper(*s0,whitespace) )
          {
            *s++ = *s0;
          }
          s0++;
        }
        *s = 0;
        n = ((int)(s1 - s)); // keep win64 happy with (int) cast
        Header()->string_length -= n;
        return n;
      }
    }
  }
  else
  {
    while( s0 < s1 )
    {
      c = *s0++;
      if ( (1 <= c && c <= 32) || 127 == c )
      {
        // need to modify this string
        n = ((int)(s0 - m_s)); // keep win64 happy with (int) cast
        CopyArray(); // may change m_s if string has multiple refs
        s0 = m_s + n;
        s = s0-1;
        s1 = m_s + Length();
        while ( s0 < s1 )
        {
          c = *s0;
          if ( c < 1 || (c > 32 && 127 != c) )
          {
            *s++ = *s0;
          }
          s0++;
        }
        *s = 0;
        n = ((int)(s1 - s)); // keep win64 happy with (int) cast
        Header()->string_length -= n;
        return n;
      }
    }
  }
  return 0;
}


///////////////////////////////////////////////////////////////////////////////

ON_wString::operator const wchar_t*() const
{
  return ( m_s == pEmptywString ) ? NULL : m_s;
}

int ON_wString::Find( char c ) const
{
	// find first single character
  char s[2];
  s[0] = c;
  s[1] = 0;
  return Find( s );
}

int ON_wString::Find( unsigned char c ) const
{
  return Find( (char)c );
}

int ON_wString::Find( wchar_t c ) const
{
	// find first single character
  wchar_t s[2];
  s[0] = c;
  s[1] = 0;
  return Find( s );
}

int ON_wString::ReverseFind( char c ) const
{
  char s[2];
  wchar_t w[3];
  s[0] = c;
  s[1] = 0;
  w[0] = 0;
  w[1] = 0;
  w[2] = 0;
  c2w(1,s,2,w);
  return ReverseFind( w[0] );
}

int ON_wString::ReverseFind( unsigned char c ) const
{
  return ReverseFind( (char)c );
}

int ON_wString::ReverseFind( wchar_t c ) const
{
	// find first single character
  if ( IsEmpty() )
    return -1;
  int i;
  const int length = Length();
  for ( i = length-1; i >= 0; i-- ) {
    if ( c == m_s[i] )
      return i;
  }
  return -1;
}

int ON_wString::Find( const char* s ) const
{
  int rc = -1;
  if ( s && s[0] && !IsEmpty() ) {
    const int s_count = (int)strlen(s); // the (int) is for 64 bit size_t conversion
    wchar_t* w = (wchar_t*)onmalloc( (s_count+2)*sizeof(w[0]) );
    c2w( s_count, s, s_count+1, w );
    const wchar_t* p;
    p = wcsstr( m_s, w );
    if ( p )
    {
      rc = ((int)(p-m_s)); // the (int) cast is for 64 bit compilers
    }
    onfree( w );
  }
  return rc;
}

int ON_wString::Find( const unsigned char* s ) const
{
  return Find( (const char*)s );
}

int ON_wString::Find( const wchar_t* s ) const
{
  int rc = -1;
  if ( s && s[0] && !IsEmpty() ) {
    const wchar_t* p;
    p = wcsstr( m_s, s );
    if ( p )
    {
      rc = ((int)(p-m_s)); // the (int) is for 64 bit size_t conversion
    }
  }
  return rc;
}

void ON_wString::MakeReverse()
{
  if ( !IsEmpty() ) {
  	CopyArray();
    on_wcsrev(m_s);
  }
}

void ON_wString::TrimLeft(const wchar_t* s)
{
  wchar_t c;
  const wchar_t* sc;
  wchar_t* dc;
  int i;
  if ( !IsEmpty() ) {
    if ( !s )
      s = L" \t\n";
    for ( i = 0; 0 != (c=m_s[i]); i++ )
    {
      for (sc = s;*sc;sc++) {
        if ( *sc == c )
          break;
      }
      if (!(*sc))
        break;
    }
    if ( i > 0 ) {
      if ( m_s[i] ) {
        CopyArray();
        dc = m_s;
        sc = m_s+i;
        while( (*dc++ = *sc++) );
        Header()->string_length -= i;
      }
      else
        Destroy();
    }
  }
}

void ON_wString::TrimRight(const wchar_t* s)
{
  wchar_t c;
  const wchar_t* sc;
  int i = Header()->string_length;
  if ( i > 0 ) {
    if ( !s )
      s = L" \t\n";
    for (i--; i >= 0 && 0 != (c=m_s[i]); i-- )
    {
      for (sc = s;*sc;sc++) {
        if ( *sc == c )
          break;
      }
      if (!(*sc))
        break;
    }
    if ( i < 0 )
      Destroy();
    else if ( m_s[i+1] ) {
      CopyArray();
      m_s[i+1] = 0;
      Header()->string_length = i+1;
    }
  }
}

void ON_wString::TrimLeftAndRight(const wchar_t* s)
{
  TrimRight(s);
  TrimLeft(s);
}


int ON_wString::Remove( wchar_t c)
{
  wchar_t* s0;
  wchar_t* s1;
  wchar_t* s;
  int n;

  if ( 0 == (s0 = m_s) )
    return 0;
  s1 = s0 + Length();
  while( s0 < s1 )
  {
    if (c == *s0++)
    {
      // need to modify this string
      n = ((int)(s0 - m_s)); // keep win64 happy with (int) cast
      CopyArray(); // may change m_s if string has multiple refs
      s0 = m_s + n;
      s = s0-1;
      s1 = m_s + Length();
      while ( s0 < s1 )
      {
        if ( c != *s0 )
        {
          *s++ = *s0;
        }
        s0++;
      }
      *s = 0;
      n = ((int)(s1 - s)); // keep win64 happy with (int) cast
      Header()->string_length -= n;
      return n;
    }
  }
  return 0;
}

wchar_t ON_wString::GetAt( int i ) const
{
  return m_s[i];
}


void ON_wString::SetAt( int i, char c )
{
  if ( i >= 0 && i < Header()->string_length ) {
	  CopyArray();
	  m_s[i] = c2w(c);
  }
}

void ON_wString::SetAt( int i, unsigned char c )
{
  SetAt( i, (char)c );
}

void ON_wString::SetAt( int i, wchar_t c )
{
  if ( i >= 0 && i < Header()->string_length ) {
	  CopyArray();
	  m_s[i] = c;
  }
}

ON_wString ON_wString::Mid(int i, int count) const
{
  ON_wString(s);
  if ( i >= 0 && i < Length() && count > 0 ) {
    if ( count > Length() - i )
      count = Length() - i;
    s.CopyToArray( count, &m_s[i] );
  }
  return s;
}

ON_wString ON_wString::Mid(int i) const
{
  return Mid( i, Length() - i );
}

ON_wString ON_wString::Left(int count) const
{
  ON_wString s;
  if ( count > Length() )
    count = Length();
  if ( count > 0 ) {
    s.CopyToArray( count, m_s );
  }
  return s;
}

ON_wString ON_wString::Right(int count) const
{
  ON_wString s;
  if ( count > Length() )
    count = Length();
  if ( count > 0 ) {
    s.CopyToArray( count, &m_s[Length()-count] );
  }
  return s;
}

void ON_VARG_CDECL ON_wString::Format( const char* sFormat, ...)
{
#define MAX_MSG_LENGTH 2048
  char sMessage[MAX_MSG_LENGTH];
  va_list args;

  /* put formatted message in sMessage */
  memset(sMessage,0,sizeof(sMessage));
  if (sFormat) {
    va_start(args, sFormat);
    on_vsnprintf(sMessage, MAX_MSG_LENGTH-1, sFormat, args);
    sMessage[MAX_MSG_LENGTH-1] = 0;
    va_end(args);
  }
  const int len = Length(sMessage);
  if ( len < 1 ) {
    Destroy();
    Create();
  }
  else {
    ReserveArray( len );
    CopyToArray( len, sMessage );
  }
}

void ON_VARG_CDECL ON_wString::Format( const unsigned char* sFormat, ...)
{
#define MAX_MSG_LENGTH 2048
  char sMessage[MAX_MSG_LENGTH];
  va_list args;

  /* put formatted message in sMessage */
  memset(sMessage,0,sizeof(sMessage));
  if (sFormat) {
    va_start(args, sFormat);
    on_vsnprintf(sMessage, MAX_MSG_LENGTH-1, (const char*)sFormat, args);
    sMessage[MAX_MSG_LENGTH-1] = 0;
    va_end(args);
  }
  const int len = Length(sMessage);
  if ( len < 1 ) {
    Destroy();
    Create();
  }
  else {
    ReserveArray( len );
    CopyToArray( len, sMessage );
  }
}

void ON_VARG_CDECL ON_wString::Format( const wchar_t* sFormat, ...)
{
#define MAX_MSG_LENGTH 2048
  wchar_t sMessage[MAX_MSG_LENGTH];
  va_list args;

  /* put formatted message in sMessage */
  memset(sMessage,0,sizeof(sMessage));
  if (sFormat) {
    va_start(args, sFormat);
    on_vsnwprintf(sMessage, MAX_MSG_LENGTH-1, sFormat, args);
    sMessage[MAX_MSG_LENGTH-1] = 0;
    va_end(args);
  }
  const int len = Length(sMessage);
  if ( len < 1 ) {
    Destroy();
    Create();
  }
  else {
    ReserveArray( len );
    CopyToArray( len, sMessage );
  }
}

///////////////////////////////////////////////////////////////////////////////

bool ON_wString::operator==(const ON_wString& s2) const
{
  return (Compare(s2) == 0) ? TRUE : FALSE;
}

bool ON_wString::operator==(const wchar_t* s2) const
{
  return (Compare(s2) == 0) ? TRUE : FALSE;
}

bool ON_wString::operator!=(const ON_wString& s2) const
{
  return (Compare(s2) != 0) ? TRUE : FALSE;
}

bool ON_wString::operator!=(const wchar_t* s2) const
{
  return (Compare(s2) != 0) ? TRUE : FALSE;
}

bool ON_wString::operator<(const ON_wString& s2) const
{
  return (Compare(s2) < 0) ? TRUE : FALSE;
}

bool ON_wString::operator<(const wchar_t* s2) const
{
  return (Compare(s2) < 0) ? TRUE : FALSE;
}

bool ON_wString::operator>(const ON_wString& s2) const
{
  return (Compare(s2) > 0) ? TRUE : FALSE;
}

bool ON_wString::operator>(const wchar_t* s2) const
{
  return (Compare(s2) > 0) ? TRUE : FALSE;
}

bool ON_wString::operator<=(const ON_wString& s2) const
{
  return (Compare(s2) <= 0) ? TRUE : FALSE;
}

bool ON_wString::operator<=(const wchar_t* s2) const
{
  return (Compare(s2) <= 0) ? TRUE : FALSE;
}

bool ON_wString::operator>=(const ON_wString& s2) const
{
  return (Compare(s2) >= 0) ? TRUE : FALSE;
}

bool ON_wString::operator>=(const wchar_t* s2) const
{
  return (Compare(s2) >= 0) ? TRUE : FALSE;
}
