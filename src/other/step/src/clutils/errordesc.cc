
/*
* NIST Utils Class Library
* clutils/errordesc.cc
* February, 1993
* David Sauder
* K. C. Morris

* Development of this software was funded by the United States Government,
* and is not subject to copyright.
*/

/* $Id: errordesc.cc,v 3.0.1.2 1997/11/05 22:33:46 sauderd DP3.1 $  */ 

#include <errordesc.h>
#include <Str.h>

DebugLevel ErrorDescriptor::_debug_level = DEBUG_OFF;
ostream *  ErrorDescriptor::_out = 0;

void 
ErrorDescriptor::PrintContents(ostream &out) const
{
    std::string s;
    out << "Severity: " << severity(s) << endl;
    if(_userMsg)
    {
	out << "User message in parens:" << endl << "(";
	out << UserMsg() << ")" << endl;
    }
    if(_detailMsg)
    {
	out << "Detailed message in parens:" << endl << "(";
	out << DetailMsg() << ")" << endl;
    }
}

const char *
ErrorDescriptor::severity(std::string &s) const
{
    s.clear();
    switch(severity())
    {
      case SEVERITY_NULL :
      {
	  s.assign("SEVERITY_NULL");
	  break;
      }
      case SEVERITY_USERMSG :
      {
	  s.assign("SEVERITY_USERMSG");
	  break;
      }
      case SEVERITY_INCOMPLETE :
      {
	  s.assign("SEVERITY_INCOMPLETE");
	  break;
      }
      case SEVERITY_WARNING :
      {
	  s.assign("SEVERITY_WARNING");
	  break;
      }
      case SEVERITY_INPUT_ERROR :
      {
	  s.assign("SEVERITY_INPUT_ERROR");
	  break;
      }
      case SEVERITY_BUG :
      {
	  s.assign("SEVERITY_BUG");
	  break;
      }
      case SEVERITY_EXIT :
      {
	  s.assign("SEVERITY_EXIT");
	  break;
      }
      case SEVERITY_DUMP :
      {
	  s.assign("SEVERITY_DUMP");
	  break;
      }
      case SEVERITY_MAX :
      {
	  s.assign("SEVERITY_MAX");
	  break;
      }
    }
    return s.c_str();
}


Severity 
ErrorDescriptor::GetCorrSeverity(const char *s)
{
//    cout << "s is (" << s << ") \n";
    if(s && s[0] != 0)
    {
      std::string s2;
	StrToUpper(s,s2);
//	cout << "s after if is (" << s << ") \n" << "s2 is (" << s2.chars() << ")\n";
	if(!strcmp(s2.c_str(),"SEVERITY_NULL"))
	{
//	    cout << "SEVERITY_NULL" << endl;
	    return SEVERITY_NULL;
	}
	if(!strcmp(s2.c_str(),"SEVERITY_USERMSG"))
	{
//	    cout << "SEVERITY_USERMSG" << endl;
	    return SEVERITY_USERMSG;
	}
	if(!strcmp(s2.c_str(),"SEVERITY_INCOMPLETE"))
	{
//	    cout << "SEVERITY_INCOMPLETE" << endl;
	    return SEVERITY_INCOMPLETE;
	}
	if(!strcmp(s2.c_str(),"SEVERITY_WARNING"))
	{
//	    cout << "SEVERITY_WARNING" << endl;
	    return SEVERITY_WARNING;
	}
	if(!strcmp(s2.c_str(),"SEVERITY_INPUT_ERROR"))
	{
//	    cout << "SEVERITY_INPUT_ERROR" << endl;
	    return SEVERITY_INPUT_ERROR;
	}
	if(!strcmp(s2.c_str(),"SEVERITY_BUG"))
	{
//	    cout << "SEVERITY_BUG" << endl;
	    return SEVERITY_BUG;
	}
	if(!strcmp(s2.c_str(),"SEVERITY_EXIT"))
	{
//	    cout << "SEVERITY_EXIT" << endl;
	    return SEVERITY_EXIT;
	}
	if(!strcmp(s2.c_str(),"SEVERITY_DUMP"))
	{
//	    cout << "SEVERITY_DUMP" << endl;
	    return SEVERITY_DUMP;
	}
	if(!strcmp(s2.c_str(),"SEVERITY_MAX"))
	{
//	    cout << "SEVERITY_MAX" << endl;
	    return SEVERITY_MAX;
	}
    }
    cerr << "Internal error:  " << __FILE__ <<  __LINE__
      << "\n" << _POC_ "\n";
    cerr << "Calling ErrorDescriptor::GetCorrSeverity() with null string\n";
    return SEVERITY_BUG;
}

ErrorDescriptor::ErrorDescriptor ( Severity s,  DebugLevel d)
:     _severity (s), 
      _userMsg (0),
      _detailMsg (0)
{
  if (d  != DEBUG_OFF) 
    _debug_level = d;
}

/*
ErrorDescriptor::ErrorDescriptor ( Severity s,  Enforcement e,  DebugLevel d)
:     _severity (s), 
      _enforcement_level (e), 
      _userMsg (0),
      _detailMsg (0)
{
  if (d  != DEBUG_OFF) 
    _debug_level = d;
}
*/
 
const char *
ErrorDescriptor::UserMsg () const
{
    if(_userMsg)
	return _userMsg->c_str();
    else
	return "";
}

void
ErrorDescriptor::UserMsg ( const char * msg)  
{
    if(!_userMsg)
	_userMsg = new std::string;
    _userMsg->assign(msg);
}

void  
ErrorDescriptor::PrependToUserMsg ( const char * msg)  
{
    if(!_userMsg)
	_userMsg = new std::string;
    _userMsg->insert(0, msg);
}

void 
ErrorDescriptor::AppendToUserMsg ( const char c)
{
    if(!_userMsg) 
	_userMsg = new std::string;
    _userMsg->append(&c);
}    

void  
ErrorDescriptor::AppendToUserMsg ( const char * msg)  
{
    if(!_userMsg)
	_userMsg = new std::string;
    _userMsg->append(msg);
}

 
const char *
ErrorDescriptor::DetailMsg ()  const
{
    if(_detailMsg)
	return _detailMsg->c_str();
    else
	return "";
}

void
ErrorDescriptor::DetailMsg ( const char * msg)  
{
    if(!_detailMsg)
	_detailMsg = new std::string;
    _detailMsg->assign(msg);
}

void
ErrorDescriptor::PrependToDetailMsg (const char * msg)  
{
    if(!_detailMsg)
	_detailMsg = new std::string;
    _detailMsg->insert(0, msg);
}    

void 
ErrorDescriptor::AppendToDetailMsg ( const char c)
{
    if(!_detailMsg)
	_detailMsg = new std::string;
    _detailMsg->append(&c);
}    

void
ErrorDescriptor::AppendToDetailMsg (const char * msg)  
{
    if(!_detailMsg)
	_detailMsg = new std::string;
    _detailMsg->append(msg);
}    
