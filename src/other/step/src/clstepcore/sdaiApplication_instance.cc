
/*
* NIST STEP Core Class Library
* clstepcore/sdaiApplication_instance.cc
* July, 1997
* K. C. Morris
* David Sauder

* Development of this software was funded by the United States Government,
* and is not subject to copyright.
*/

/* $Id: sdaiApplication_instance.cc,v 1.5 1998/02/17 18:56:53 sauderd DP3.1 $ */

//static char rcsid[] ="$Id: sdaiApplication_instance.cc,v 1.5 1998/02/17 18:56:53 sauderd DP3.1 $";

#include <sdai.h>
//#include <STEPentity.h>
#include <instmgr.h>
#include <STEPcomplex.h>
#include <STEPattribute.h>

SCLP23(Application_instance) NilSTEPentity;

/******************************************************************
**	  Functions for manipulating entities

**  KNOWN BUGs:  the SCLP23(Application_instance) is not aware of the STEPfile;
**    therefore it can not read comments which may be embedded in the instance.
**    The following are known problems:
**    -- does not handle comments embedded in an instance ==> bombs
**    -- ignores embedded entities ==> does not bomb 
**    -- error reporting does not include line number information  
**/

SCLP23(Application_instance)::SCLP23_NAME(Application_instance) ()  
:  _cur(0), STEPfile_id(0), p21Comment(0), headMiEntity(0), nextMiEntity(0), 
   _complex(0)
{
}	

SCLP23(Application_instance)::SCLP23_NAME(Application_instance) (int fileid, int complex)
:  _cur(0), STEPfile_id(fileid), p21Comment(0), 
   headMiEntity(0), nextMiEntity(0), _complex (complex)
{
}	

SCLP23(Application_instance)::~SCLP23_NAME(Application_instance) ()  
{
  STEPattribute * next =0;
  ResetAttributes ();
  while (next = NextAttribute ())  
    delete next;

  if(MultipleInheritance())
  {
      delete nextMiEntity;
  }

  delete p21Comment;
  
/*
// this is not necessary since each will call delete on its 
// own nextMiEntity - DAS
  SCLP23(Application_instance) * nextMI = nextMiEntity;
  SCLP23(Application_instance) * nextMItrail = 0;
  while(nextMI)
  {
      nextMItrail = nextMI;
      nextMI = nextMI->nextMiEntity;
	// this is ok since STEPattribute doesn't explicitly delete its attr
      delete nextMItrail;
  }
*/
}

#ifdef PART26
//SCLP26(Application_instance_ptr) 
IDL_Application_instance_ptr 
SCLP23(Application_instance)::create_TIE()
{
    cout << "ERROR SCLP23(Application_instance)::create_TIE() called." << endl;
    return 0;
}
#endif

SCLP23(Application_instance) * 
SCLP23(Application_instance)::Replicate()
{
    char errStr[BUFSIZ];
    if(IsComplex())
    {
	cerr << "STEPcomplex::Replicate() should be called:  " << __FILE__ 
	  <<  __LINE__ << "\n" << _POC_ "\n";
	sprintf(errStr, 
		"SCLP23(Application_instance)::Replicate(): %s - entity #%d.\n",
	       "Programming ERROR - STEPcomplex::Replicate() should be called",
		STEPfile_id);
	_error.AppendToDetailMsg(errStr);
	_error.AppendToUserMsg(errStr);
	_error.GreaterSeverity(SEVERITY_BUG);
	return S_ENTITY_NULL;
    }
    else
    {
#ifdef __OSTORE__
	SCLP23(Application_instance) *seNew = eDesc->NewSTEPentity(os_database::of(this));
#else
	SCLP23(Application_instance) *seNew = eDesc->NewSTEPentity();
#endif
	seNew -> CopyAs (this);
	return seNew;
    }
}

void SCLP23(Application_instance)::AddP21Comment(const char *s, int replace)
{
    if(replace)
    {
	delete p21Comment;
	p21Comment = 0;
    }
    if(s)
    {
      if(!p21Comment) 
      {
        p21Comment = new std::string("");
      }
      else
      {
        p21Comment->clear();
      }
    p21Comment->append(s);
    }
}

void 
SCLP23(Application_instance)::AddP21Comment(std::string &s, int replace) 
{
    if(replace)
    {
	delete p21Comment;
	p21Comment = 0;
    }
    if(!s.empty())
    {
	if(!p21Comment) 
	{
          p21Comment = new std::string("");
	}
	else
	    p21Comment->clear();
	p21Comment->append(const_cast<char *>(s.c_str()));
    }
}

void 
SCLP23(Application_instance)::STEPwrite_reference (ostream& out)
{  
  out << "#" << STEPfile_id; 
}

const char * 
SCLP23(Application_instance)::STEPwrite_reference (std::string& buf)
{  
    char tmp[64];
    sprintf ( tmp,"#%d", STEPfile_id);
    buf = tmp;
    return const_cast<char *>(buf.c_str());
}

void 
SCLP23(Application_instance)::AppendMultInstance(SCLP23(Application_instance) *se)
{
    if(nextMiEntity == 0)
      nextMiEntity = se;
    else
    {
	SCLP23(Application_instance) *link = nextMiEntity;
	SCLP23(Application_instance) *linkTrailing = 0;
	while(link)
	{
	    linkTrailing = link;
	    link = link->nextMiEntity;
	}
	linkTrailing->nextMiEntity = se;
    }
}

// BUG implement this

SCLP23(Application_instance) *
SCLP23(Application_instance)::GetMiEntity(char *EntityName)
{
  std::string s1, s2;

    const EntityDescLinkNode *edln = 0;
    const EntityDescriptor *ed = eDesc;

    // compare up the *leftmost* parent path
    while(ed)
      {
	if( !strcmp( StrToLower(ed->Name(),s1), StrToLower(EntityName,s2) ) )
	    return this; // return this parent path
	edln = (EntityDescLinkNode *)( ed->Supertypes().GetHead() );
	if(edln)
	  ed = edln->EntityDesc();
	else
	  ed = 0;
      }
    // search alternate parent path since didn't find it in this one.
    if(nextMiEntity)
      return nextMiEntity->GetMiEntity(EntityName);
    return 0;
}


#ifdef __OSTORE__
void Application_instance_access_hook_in(void *object, 
	enum os_access_reason reason, void *user_data, 
	void *start_range, void *end_range)
{
    cout << "ObjectStore called non-member function Application_instance Access_hook" 
      << endl;
    SCLP23(Application_instance) *ent = (SCLP23(Application_instance) *)object;
    cout << "STEPfile_id: " << ent->STEPfile_id << endl;
    ent->Access_hook_in(object, reason, user_data, start_range, end_range);

//    SdaiEb_person *ent = (SdaiEb_person *)object;
//    SdaiEb_person_access_hook_in(object, reason, user_data, 
//				 start_range, end_range);
}

void 
SCLP23(Application_instance)::Access_hook_in(void *object, 
			   enum os_access_reason reason, void *user_data, 
			   void *start_range, void *end_range)
{
    SCLP23(Application_instance) *ent = (SCLP23(Application_instance) *)object;
    cout << "****WARNING: virtual SCLP23(Application_instance)::Access_hook_in() called" << endl;
    ent->Access_hook_in(object, reason, user_data, start_range, end_range);
}
#endif

STEPattribute * 
SCLP23(Application_instance)::GetSTEPattribute (const char * nm)
{
  if (!nm) return 0;
  STEPattribute *a =0;

  ResetAttributes();	
  while ((a = NextAttribute () )
	 && strcmp (nm, a ->Name()))
    ;  // keep going until no more attributes or attribute is found

  return a;
}

STEPattribute * 
SCLP23(Application_instance)::MakeRedefined (STEPattribute *redefiningAttr, const char * nm)
{
    // find the attribute being redefined
    STEPattribute * a = GetSTEPattribute (nm);

    // assign its pointer to the redefining attribute
    if (a)  a->RedefiningAttr(redefiningAttr);
    return a;
}

STEPattribute * 
SCLP23(Application_instance)::MakeDerived (const char * nm)
{
  STEPattribute * a = GetSTEPattribute (nm);
  if (a)  a ->Derive ();
  return a;
}

void
SCLP23(Application_instance)::CopyAs (SCLP23(Application_instance) * other)  
{
    int numAttrs = AttributeCount();
    ResetAttributes();	
    other -> ResetAttributes();	

    STEPattribute *this_attr = 0;
    STEPattribute *other_attr = 0;
    while((this_attr = NextAttribute()) && numAttrs)
    {
	other_attr = other -> NextAttribute();
	this_attr -> ShallowCopy(other_attr);
	numAttrs--;
    }
}


const char *
SCLP23(Application_instance)::EntityName( const char *schnm) const
{
    return eDesc->Name( schnm );
}

/*****************************************************************
  Checks if a given SCLP23(Application_instance) is the same type as this one
  ****************************************************************/

const EntityDescriptor* SCLP23(Application_instance)::IsA(const EntityDescriptor *ed) const
{
    return (eDesc->IsA(ed));
}

/******************************************************************
// Checks the validity of the current attribute values for the entity
 ******************************************************************/

Severity SCLP23(Application_instance)::ValidLevel(ErrorDescriptor *error, InstMgr *im, 
				     int clearError)
{
    ErrorDescriptor err;
    if(clearError)
	ClearError();
    int n = attributes.list_length();
    std::string tmp;
    for (int i = 0 ; i < n; i++) {
	if( !(attributes[i].aDesc->AttrType() == AttrType_Redefining) )
	  error->GreaterSeverity(attributes[i].ValidLevel(
				       attributes[i].asStr(tmp), &err, im, 0));
    }
    return error->severity();
}

/******************************************************************
	// clears all attr's errors
 ******************************************************************/
void SCLP23(Application_instance)::ClearAttrError()
{
    int n = attributes.list_length();
    for (int i = 0 ; i < n; i++) {
	attributes[i].Error().ClearErrorMsg();
    }
}

/******************************************************************
	// clears entity's error and optionally all attr's errors
 ******************************************************************/

void SCLP23(Application_instance)::ClearError(int clearAttrs)
{
    _error.ClearErrorMsg();
    if(clearAttrs)
	ClearAttrError();
} 

/******************************************************************
 ******************************************************************/

/*
void SCLP23(Application_instance)::EnforceOptionality(int on) 
{
    Enforcement e;
    if(on) e = ENFORCE_OPTIONALITY;
    else   e = ENFORCE_OFF;

    Error().enforcement(e); 
    int n = attributes.list_length();
    for (int i = 0 ; i < n; i++) {
	attributes[i].Error().enforcement(e); 
    }
}
*/
/******************************************************************
 ** Procedure:  beginSTEPwrite
 ** Parameters:  ostream& out -- stream to write to
 ** Returns:  
 ** Side Effects:  writes out the SCOPE section for an entity
 ** Status:  stub 
 ******************************************************************/

void SCLP23(Application_instance)::beginSTEPwrite(ostream& out)
{
    out << "begin STEPwrite ... \n" ;
    out.flush();
    
    int n = attributes.list_length();
    for (int i = 0 ; i < n; i++) {
	if (attributes[i].Type () == ENTITY_TYPE
	    && *(attributes[i].ptr.c) != S_ENTITY_NULL)
	    (*(attributes[i].ptr.c)) -> STEPwrite();
    }
}

/******************************************************************
 ** Procedure:  STEPwrite
 ** Parameters:  ostream& out -- stream to write to
 ** Returns:  
 ** Side Effects:  writes out the data associated with an instance 
                   in STEP format
 ** Problems:  does not print out the SCOPE section of an entity
 **
 ******************************************************************/

void
SCLP23(Application_instance)::STEPwrite(ostream& out, const char *currSch, 
					int writeComments)
{
  std::string tmp;
    if(writeComments && p21Comment && !p21Comment->empty() )
	out << p21Comment->c_str();
    out << "#" << STEPfile_id << "=" << StrToUpper(EntityName( currSch ), tmp)
        << "(";
    int n = attributes.list_length();

    for (int i = 0 ; i < n; i++) {
	if( !(attributes[i].aDesc->AttrType() == AttrType_Redefining) )
	{
	    if (i > 0) out << ",";
	    (attributes[i]).STEPwrite( out, currSch );
	}
    }
    out << ");\n";
}

void SCLP23(Application_instance)::endSTEPwrite(ostream& out)
{
    out << "end STEPwrite ... \n" ;
    out.flush();
}

void
SCLP23(Application_instance)::WriteValuePairs(ostream& out, 
					      const char *currSch, 
					      int writeComments, int mixedCase)
{
/*
    const EntityDescriptorList &edl = eDesc->Supertypes();
    EntityDescItr &edi((EntityDescriptorList &)(eDesc->Supertypes()));

    int count = 1;
    const EntityDescriptor * ed = 0;
    while(ed = edi.NextEntityDesc())
    {
	if(count > 1)
	{
	    out << "&";
	}
	out << ed->Name();
	count++;
    }
*/

//    const AttrDescriptorList& adl = ed->ExplicitAttr();

/*
    out << "Attributes are: " << endl;
    edi.ResetItr();
    AttrDescItr *adi = 0;
    const AttrDescriptor * ad;
    while(ed = edi.NextEntityDesc())
    {
	adi = new AttrDescItr((AttrDescriptorList&)ed->ExplicitAttr());
	ad = 0;
	while(ad = adi->NextAttrDesc())
	{
	    out << ed->Name() << "." << ad->Name() << endl;
	}
	delete adi;
    }
    out << endl;
    out << "finished writing attribute names." << endl;
    edi.ResetItr();
*/
  std::string s;
//    out << eDesc->QualifiedName(s) << endl;

  std::string tmp, tmp2;
    if(writeComments && p21Comment && !p21Comment->empty() )
	out << p21Comment->c_str();
    if(mixedCase)
    {
	out << "#" << STEPfile_id << " " 
	  << eDesc->QualifiedName(s) << endl;
    }
    else
    {
	out << "#" << STEPfile_id << " " 
	  << StrToUpper(eDesc->QualifiedName(s),tmp) << endl;
    }
    int n = attributes.list_length();

    for (int i = 0 ; i < n; i++) {
	if( !(attributes[i].aDesc->AttrType() == AttrType_Redefining) )
	{
//	    if (i > 0) out << ",";
	    if(mixedCase)
	    {
		out << "\t" 
		 << attributes[i].aDesc->Owner().Name(s)
		 << "." << attributes[i].aDesc->Name() << " ";
	    }
	    else
	    {
		out << "\t" 
		 << StrToUpper(attributes[i].aDesc->Owner().Name(s),tmp)
		 << "." << StrToUpper(attributes[i].aDesc->Name(),tmp2) << " ";
	    }
	    (attributes[i]).STEPwrite( out, currSch );
	    out << endl;
	}
    }
    out << endl;
}


/******************************************************************
 ** Procedure:  STEPwrite
 ** Problems:  does not print out the SCOPE section of an entity
 **
 ******************************************************************/

const char * 
SCLP23(Application_instance)::STEPwrite(std::string& buf, const char *currSch)
{
    buf.clear();

    char instanceInfo[BUFSIZ];
    
    std::string tmp;
    sprintf(instanceInfo, "#%d=%s(", STEPfile_id, 
	    (char *)StrToUpper( EntityName( currSch ), tmp ) );
    buf.append(instanceInfo);

    int n = attributes.list_length();

    for (int i = 0 ; i < n; i++) {
	if( !(attributes[i].aDesc->AttrType() == AttrType_Redefining) )
	{
	    if (i > 0) buf.append(",");
	    attributes[i].asStr( tmp, currSch ) ;
	    buf.append (tmp);
	}
    }    
    buf.append( ");" );
    return const_cast<char *>(buf.c_str());
}

void 
SCLP23(Application_instance)::PrependEntityErrMsg()
{
    char errStr[BUFSIZ];
    errStr[0] = '\0';

    if(_error.severity() == SEVERITY_NULL)
    {  //  if there is not an error already
	sprintf(errStr, "\nERROR:  ENTITY #%d %s\n", GetFileId(), 
		EntityName());
	_error.PrependToDetailMsg(errStr);
    }
}

/******************************************************************
 ** Procedure:  SCLP23(Application_instance)::STEPread_error
 ** Parameters:  char c --  character which caused error
 **     int i --  index of attribute which caused error
 **     istream& in  --  input stream for recovery
 ** Returns:  
 ** Description:  reports the error found, reads until it finds the end of an
 **     instance. i.e. a close quote followed by a semicolon optionally having
 **     whitespace between them.
 ******************************************************************/
void
SCLP23(Application_instance)::STEPread_error(char c, int i, istream& in)
{
    char errStr[BUFSIZ];
    errStr[0] = '\0';

    if(_error.severity() == SEVERITY_NULL)
    {  //  if there is not an error already
	sprintf(errStr, "\nERROR:  ENTITY #%d %s\n", GetFileId(), 
		EntityName());
	_error.PrependToDetailMsg(errStr);
    }

/*    sprintf(errStr, " for instance #%d : %s\n", STEPfile_id, EntityName());*/
/*    _error.AppendToDetailMsg(errStr);*/

    if ( (i >= 0) && (i < attributes.list_length()))  // i is an attribute 
    {
	Error ().GreaterSeverity (SEVERITY_WARNING);
	sprintf(errStr, "  invalid data before type \'%s\'\n", 
		attributes[i].TypeName()); 
	_error.AppendToDetailMsg(errStr);
    }
    else
    {
	Error ().GreaterSeverity (SEVERITY_INPUT_ERROR);
	_error.AppendToDetailMsg("  No more attributes were expected.\n");
    }

    std::string tmp;
    STEPwrite (tmp);  // STEPwrite writes to a static buffer inside function
    sprintf(errStr, 
	    "  The invalid instance to this point looks like :\n%s\n", 
	    tmp.c_str() );
    _error.AppendToDetailMsg(errStr);
    
//    _error.AppendToDetailMsg("  data lost looking for end of entity:");

	//  scan over the rest of the instance and echo it
//    cerr << "  ERROR Trying to find the end of the ENTITY to recover...\n";
//    cerr << "  skipping the following input:\n";

#ifdef OBSOLETE
    in.clear();
    int foundEnd = 0;
    tmp = "";

    // Search until a close paren is found followed by (skipping optional 
    // whitespace) a semicolon
    while( in.good() && !foundEnd )
    {
	while ( in.good() && (c != ')') )  
	{
	    in.get(c);
	    tmp.Append(c);
//	    cerr << c;
	}
	if(in.good() && (c == ')') )
	{
	    in >> ws; // skip whitespace
	    in.get(c);
	    tmp.Append(c);
//	    cerr << c;
//	    cerr << "\n";
	    if(c == ';')
	    {
		foundEnd = 1;
	    }
	}
    }
    _error.AppendToDetailMsg( tmp.chars() );
#endif
    sprintf (errStr, "\nfinished reading #%d\n", STEPfile_id);
    _error.AppendToDetailMsg(errStr);
    return;
}

/******************************************************************
 ** Procedure:  STEPread
 ** Returns:    Severity, error information
 **             SEVERITY_NULL - no errors
 **             SEVERITY_USERMSG - checked as much as possible, could still 
 **			be error - e.g. entity didn't match base entity type.
 **             SEVERITY_INCOMPLETE - data is missing and required.
 **             SEVERITY_WARNING - errors, but can recover
 **             <= SEVERITY_INPUT_ERROR - fatal error, can't recover
 ** Description:  reads the values for an entity from an input stream
 **               in STEP file format starting at the open paren and
 **               ending with the semi-colon
 ** Parameters:  int id
 **              int idIncrement
 **              InstMgr instances
 **              istream& in
 ** Side Effects:  gobbles up input stream
 ** Status:
 ******************************************************************/

Severity
SCLP23(Application_instance)::STEPread(int id,  int idIncr, 
				       InstMgr * instance_set, istream& in,
				       const char *currSch, int useTechCor)
{
    STEPfile_id = id;
    char c = '\0';
    char errStr[BUFSIZ];
    errStr[0] = '\0';
    Severity severe;
    int i = 0;

    ClearError(1);

    in >> ws;
    in >> c; // read the open paren
    if( c != '(' )
    {
	PrependEntityErrMsg();
	_error.AppendToDetailMsg(
		       "  Missing initial open paren... Trying to recover.\n");
	in.putback(c); // assume you can recover by reading 1st attr value
    }
    in >> ws;

    int n = attributes.list_length();
    if( n == 0)  // no attributes
    {
	in >> c; // look for the close paren
	if( c == ')' )
	    return _error.severity();
    }

    for (i = 0 ; i < n; i++) {
	if( attributes[i].aDesc->AttrType() == AttrType_Redefining )
	{
	    in >> ws;
	    c = in.peek();
	    if(!useTechCor) // i.e. use pre-technical corrigendum encoding
	    {
		in >> c; // read what should be the '*'
		in >> ws;
		if(c == '*')
		{
		    in >> c; // read the delimiter i.e. ',' or ')'
		}
		else
		{
		    severe = SEVERITY_INCOMPLETE;
		    PrependEntityErrMsg(); // adds entity info if necessary

		    // set the severity for this entity
		    _error.GreaterSeverity(severe);
		    sprintf (errStr, "  %s :  ", attributes[i].Name());
		    _error.AppendToDetailMsg(errStr); // add attr name
		    _error.AppendToDetailMsg(
			"Since using pre-technical corrigendum... missing asterisk for redefined attr.\n");
		    _error.AppendToUserMsg(
			"Since using pre-technical corrigendum... missing asterisk for redefined attr. ");
		}
/*
		if( ! ( (c == ',') || (c == ')') ) ) 
		{ // input is not a delimiter - an error
		    PrependEntityErrMsg();

		    _error.AppendToDetailMsg(
			"Delimiter expected after redefined asterisk attribute encoding (since using pre-technical corregendum.\n");
		    CheckRemainingInput(in, &_error, "ENTITY", ",)");
		    if(!in.good())
		      return _error.severity();
		    if(_error.severity() <= SEVERITY_INPUT_ERROR)
		    {
//			STEPread_error(c,i,in);
			return _error.severity();
		    }
		}
*/
	    }
	    else // using technical corrigendum
	    {
		// should be nothing to do except loop again unless...
		// if at end need to have read the closing paren.
		if(c == ')') // assume you are at the end so read last char
		  in >> c;
		cout << "Entity #" << STEPfile_id 
		  << " skipping redefined attribute "
		    << attributes[i].aDesc->Name() << endl << endl << flush;
	    }
	    // increment counter to read following attr since these attrs 
	    // aren't written or read => there won't be a delimiter either
//	    i++;
	}
	else
	{
	    attributes[i].STEPread(in, instance_set, idIncr, currSch);
	    in >> c; // read the , or ) following the attr read

	    severe = attributes[i].Error().severity();

	    if(severe <= SEVERITY_USERMSG)
	    {  // if there is some type of error
		PrependEntityErrMsg();

		// set the severity for this entity
		_error.GreaterSeverity(severe);
		sprintf (errStr, "  %s :  ", attributes[i].Name());
		_error.AppendToDetailMsg(errStr); // add attr name
		_error.AppendToDetailMsg((char *) // add attr error
					 attributes[i].Error().DetailMsg());
		_error.AppendToUserMsg((char*)attributes[i].Error().UserMsg());
	    }
	}

/*
	if(severe <= SEVERITY_INPUT_ERROR)
	{	// attribute\'s error is non-recoverable 
	// I believe if this error occurs then you cannot recover

	//  TODO: can you just read to the next comma and try to continue ?
	STEPread_error(c,i,in);
	return _error.severity();
	}
*/
	// if technical corrigendum redefined, input is at next attribute value
	// if pre-technical corrigendum redefined, don't process 
	if ( (!( attributes[i].aDesc->AttrType() == AttrType_Redefining ) || 
	      !useTechCor) && 
	     ! ((c == ',') || (c == ')' )) ) { //  input is not a delimiter
	    PrependEntityErrMsg();

	    _error.AppendToDetailMsg(
				"Delimiter expected after attribute value.\n");
	    if(!useTechCor)
	    {
		_error.AppendToDetailMsg(
		   "I.e. since using pre-technical corrigendum, redefined ");
		_error.AppendToDetailMsg(
		   "attribute is mapped as an asterisk so needs delimiter.\n");
	    }
	    CheckRemainingInput(in, &_error, "ENTITY", ",)");
	    if(!in.good())
	      return _error.severity();
	    if(_error.severity() <= SEVERITY_INPUT_ERROR)
	    {
//		STEPread_error(c,i,in);
		return _error.severity();
	    }
	}
	else if(c == ')')
	{
	    while (i < n-1)
	    {
		i++; // check if following attributes are redefined
		if( !(attributes[i].aDesc->AttrType() == AttrType_Redefining) )
		{
		    PrependEntityErrMsg();
		    _error.AppendToDetailMsg("Missing attribute value[s].\n");
				// recoverable error
		    _error.GreaterSeverity(SEVERITY_WARNING); 
		    return _error.severity();
		}
		i++;
	    }
	    return _error.severity();
	}
    }
    STEPread_error(c,i,in);
//  code fragment imported from STEPread_error 
//  for some currently unknown reason it was commented out of STEPread_error
    errStr[0] = '\0';
    in.clear();
    int foundEnd = 0;
    std::string tmp;
    tmp = "";

    // Search until a close paren is found followed by (skipping optional 
    // whitespace) a semicolon
    while( in.good() && !foundEnd )
    {
	while ( in.good() && (c != ')') )  
	{
	    in.get(c);
	    tmp += c;
//	    cerr << c;
	}
	if(in.good() && (c == ')') )
	{
	    in >> ws; // skip whitespace
	    in.get(c);
	    tmp += c;
//	    cerr << c;
//	    cerr << "\n";
	    if(c == ';')
	    {
		foundEnd = 1;
	    }
	}
    }
    _error.AppendToDetailMsg( tmp.c_str() );
    sprintf (errStr, "\nfinished reading #%d\n", STEPfile_id);
    _error.AppendToDetailMsg(errStr);
// end of imported code
    return _error.severity();
}

///////////////////////////////////////////////////////////////////////////////
// read an entity reference and return a pointer to the SCLP23(Application_instance)
///////////////////////////////////////////////////////////////////////////////

SCLP23(Application_instance) *
ReadEntityRef(istream &in, ErrorDescriptor *err, char *tokenList, 
	      InstMgr * instances, int addFileId)
{
    char c;
    char errStr[BUFSIZ];
    errStr[0] = '\0';
    
    in >> ws;
    in >> c;
    switch (c)
    {
      case '@':
	    err->AppendToDetailMsg(
				"Use of @ instead of # to identify entity.\n");
	    err->GreaterSeverity(SEVERITY_WARNING);
	    // no break statement here on purpose
      case '#':
	{
	    int id = -1; 
	    int n = 0;
	    in >>  id;
	    if (in.fail ())  //  there's been an error in input
	    {
//		in.clear();
		sprintf(errStr,"Invalid entity reference value.\n");
		err->AppendToDetailMsg(errStr);
		err->AppendToUserMsg(errStr);
		err->GreaterSeverity(SEVERITY_WARNING);
		CheckRemainingInput(in, err, "Entity Reference", tokenList);
		return S_ENTITY_NULL;
	    }
	    else // found an entity id
	    {
		// check to make sure garbage does not follow the id
		CheckRemainingInput(in, err, "Entity Reference", tokenList);

		id += addFileId;
		if (!instances)
		{
		    cerr << "Internal error:  " << __FILE__ <<  __LINE__
			 << "\n" << _POC_ "\n";
		    sprintf(errStr, 
			    "STEPread_reference(): %s - entity #%d %s.\n",
			    "BUG - cannot read reference without the InstMgr", 
			     id, "is unknown");
		    err->AppendToDetailMsg(errStr);
		    err->AppendToUserMsg(errStr);
		    err->GreaterSeverity(SEVERITY_BUG);
		    return S_ENTITY_NULL;
		}
	
		//  lookup which object has id as its instance id
		SCLP23(Application_instance)* inst;
		/* If there is a ManagerNode it should have a SCLP23(Application_instance) */
		MgrNode* mn =0;
		mn = instances->FindFileId(id);
		if (mn)
		{
		    inst =  mn->GetSTEPentity() ;
		    if (inst) { return (inst); }
		    else
		    {
			cerr << "Internal error:  " << __FILE__ <<  __LINE__
			     << "\n" << _POC_ "\n";
			sprintf(errStr, 
				"STEPread_reference(): %s - entity #%d %s.\n",
			  "BUG - MgrNode::GetSTEPentity returned NULL pointer",
				id, "is unknown");
			err->AppendToDetailMsg(errStr);
			err->AppendToUserMsg(errStr);
			err->GreaterSeverity(SEVERITY_BUG);
			return S_ENTITY_NULL;
		    }
		}
		else
		{
		    sprintf(errStr,"Reference to non-existent ENTITY #%d.\n", 
			    id);
		    err->AppendToDetailMsg(errStr);
		    err->AppendToUserMsg(errStr);
		    err->GreaterSeverity(SEVERITY_WARNING);
		    return S_ENTITY_NULL;
		}
	    }
	}
      default:
	{
	    in.putback(c);
	    // read past garbage up to delim in tokenList
	    // if tokenList is null it will not read anything
	    CheckRemainingInput(in, err, "Entity Reference", tokenList);
	    return S_ENTITY_NULL;
	}
    }
}

///////////////////////////////////////////////////////////////////////////////
// same as above but reads from a const char *
///////////////////////////////////////////////////////////////////////////////

SCLP23(Application_instance) *
ReadEntityRef(const char * s, ErrorDescriptor *err, char *tokenList, 
	      InstMgr * instances, int addFileId)
{
    istringstream in((char *)s);
    return ReadEntityRef(in, err, tokenList, instances, addFileId);
}

///////////////////////////////////////////////////////////////////////////////
// return SEVERITY_NULL if se's entity type matches the supplied entity type
///////////////////////////////////////////////////////////////////////////////

Severity 
EntityValidLevel(SCLP23(Application_instance) *se, 
		 const TypeDescriptor *ed, // entity type that entity se needs 
					   // to match. (this must be an
					   // EntityDescriptor)
		 ErrorDescriptor *err)
{
    char messageBuf [BUFSIZ];
    messageBuf[0] = '\0';

    if( !ed || (ed->NonRefType() != ENTITY_TYPE) )
    {
	err->GreaterSeverity(SEVERITY_BUG);
	sprintf(messageBuf, 
		" BUG: EntityValidLevel() called with %s", 
		"missing or invalid EntityDescriptor\n");
	err->AppendToUserMsg(messageBuf);
	err->AppendToDetailMsg(messageBuf);
	cerr << "Internal error:  " << __FILE__ <<  __LINE__
	       << "\n" << _POC_ "\n";
	return SEVERITY_BUG;
    }
    if(!se || (se == S_ENTITY_NULL))
    {
	err->GreaterSeverity(SEVERITY_BUG);
	sprintf(messageBuf, 
		" BUG: EntityValidLevel() called with null pointer %s\n", 
		"for SCLP23(Application_instance) argument.");
	err->AppendToUserMsg(messageBuf);
	err->AppendToDetailMsg(messageBuf);
	cerr << "Internal error:  " << __FILE__ <<  __LINE__
	       << "\n" << _POC_ "\n";
	return SEVERITY_BUG;
    }

    // DAVE: Can an entity be used in an Express TYPE so that this 
    // EntityDescriptor would have type REFERENCE_TYPE -- it looks like NO

    else if(se->eDesc)
    {
	// is se a descendant of ed?
	if ( se->eDesc->IsA(ed) )
	{
	    return SEVERITY_NULL;
	}
	else
	{
	    if(se->IsComplex())
	    {
	      // This way assumes that the complex has all it's parts i.e. the
	      // complete hierarchy so it should be able to find ed's part if
	      // it is an ed.
	        STEPcomplex *sc = ((STEPcomplex *)se)->sc;
		if(sc->EntityExists(ed->Name()))
		    return SEVERITY_NULL;
/*
*/

	      // This way checks to see if it is an ed based on the dictionary
	      // It is much less efficient but does not depend on the instance
	      // having all the parts it needs to be valid.
/*
	        STEPcomplex *sc = ((STEPcomplex *)se)->sc;
		while (sc)
		{
		    if( sc->eDesc->IsA(ed) )
			return SEVERITY_NULL;
		    sc = sc->sc;
		}
*/
	    }
	    err->GreaterSeverity(SEVERITY_WARNING);
	    sprintf(messageBuf,
		    " Entity #%d exists but is not a %s or descendant.\n", 
		    se->STEPfile_id, ed->Name());
	    err->AppendToUserMsg(messageBuf);
	    err->AppendToDetailMsg(messageBuf);
	    return SEVERITY_WARNING;
	}
    }
    else
    {
	err->GreaterSeverity(SEVERITY_BUG);
	sprintf(messageBuf, 
		" BUG: EntityValidLevel(): SCLP23(Application_instance) #%d has a %s", 
		se->STEPfile_id, "missing or invalid EntityDescriptor\n");
	err->AppendToUserMsg(messageBuf);
	err->AppendToDetailMsg(messageBuf);
	cerr << "Internal error:  " << __FILE__ <<  __LINE__
	       << "\n" << _POC_ "\n";
	return SEVERITY_BUG;
    }
}

///////////////////////////////////////////////////////////////////////////////
// return 1 if attrValue has the equivalent of a null value.
///////////////////////////////////////////////////////////////////////////////

int 
SetErrOnNull  (const char *attrValue, ErrorDescriptor *error)
{
// DAVE: Is this needed will sscanf return 1 if assignment suppression is used?
    char scanBuf[BUFSIZ];
    scanBuf[0] = '\0';

    int numFound = sscanf((char *)attrValue," %s", scanBuf);
    if (numFound == EOF) {
/*
	if(Nullable()) {
	    error->GreaterSeverity (SEVERITY_NULL);
	}
	else {
	    error->GreaterSeverity (SEVERITY_INCOMPLETE);
	}
*/
	error->GreaterSeverity (SEVERITY_INCOMPLETE);
	return 1;
    }
    return 0;
}

///////////////////////////////////////////////////////////////////////////////
// return SEVERITY_NULL if attrValue has a valid entity reference
// This function accepts an entity reference in two forms that is with or 
// without the # sign: e.g. either #23 or 23 will be read.
// If non-whitespace characters follow the entity reference an error is set.
///////////////////////////////////////////////////////////////////////////////

Severity 
EntityValidLevel(const char *attrValue, // string contain entity ref
		 const TypeDescriptor *ed, // entity type that entity in 
					   // attrValue (if it exists) needs 
					   // to match. (this must be an
					   // EntityDescriptor)
		   ErrorDescriptor *err, InstMgr *im, int clearError)
{
    char tmp [BUFSIZ];
    tmp[0] = '\0';
    char messageBuf [BUFSIZ];
    messageBuf[0] = '\0';

    if(clearError)
	err->ClearErrorMsg();

/*
  // the problem with doing this is that it will require having a # in front
  // of the entity ref.
    SCLP23(Application_instance) se = ReadEntityRef(attrValue, err, 0, im, 0);
    return EntityValidLevel(se, ed, err);
*/

    int fileId;
    MgrNode *mn = 0;

    // check for both forms:  #id or id
    int found1 = sscanf((char *)attrValue, " #%d %s", &fileId, tmp);
    int found2 = sscanf((char *)attrValue, " %d %s", &fileId, tmp);

    if( (found1 > 0) || (found2 > 0) )
    {
	if ( (found1 == 2) || (found2 == 2) )
	{
	    sprintf(messageBuf, 
		   " Attribute's Entity Reference %s is %s data \'%s\'.\n",
		    attrValue, "followed by invalid", tmp);
	    err->AppendToUserMsg(messageBuf);
	    err->AppendToDetailMsg(messageBuf);
	    err->GreaterSeverity(SEVERITY_WARNING);
	}
	mn = im->FindFileId(fileId);
	if(mn)
	{
	    SCLP23(Application_instance) *se = mn->GetSTEPentity();
	    return EntityValidLevel(se, ed, err);
	}
	else { 
	    sprintf(messageBuf, 
		   " Attribute's Entity Reference %s does not exist.\n",
		    attrValue);
	    err->AppendToUserMsg(messageBuf);
	    err->AppendToDetailMsg(messageBuf);
	    err->GreaterSeverity(SEVERITY_WARNING);
	    return SEVERITY_WARNING;
	}
    }

    // if the attrValue contains no value return
    if (SetErrOnNull (attrValue, err))
	return err->severity();

    sprintf(messageBuf, "Invalid attribute entity reference value: '%s'.\n", 
	    attrValue);
    err->AppendToUserMsg(messageBuf);
    err->AppendToDetailMsg(messageBuf);
    err->GreaterSeverity(SEVERITY_WARNING);
    return SEVERITY_WARNING;
}

/******************************************************************
 ** Procedure:  NextAttribute
 ** Parameters:  
 ** Returns:  reference to an attribute pointer
 ** Description:  used to cycle through the list of attributes
 ** Side Effects:  increments the current position in the attribute list
 ** Status:  untested 7/31/90
 ******************************************************************/

STEPattribute *
SCLP23(Application_instance)::NextAttribute ()  {
    int i = AttributeCount ();
    ++_cur;
    if (i < _cur) return 0;
    return &attributes [_cur-1];

}
    
int 
SCLP23(Application_instance)::AttributeCount ()  {
    return  attributes.list_length ();
}

#ifdef OBSOLETE
Severity 
SCLP23(Application_instance)::ReadAttrs(int id, int addFileId, 
		       class InstMgr * instance_set, istream& in)
{
    char c ='\0';
    char errStr[BUFSIZ];
    errStr[0] = '\0';
    Severity severe;

    ClearError(1);

    int n = attributes.list_length();
    for (int i = 0 ; i < n; i++) {
        attributes[i].STEPread(in, instance_set, addFileId);

	severe = attributes[i].Error().severity();

        if(severe <= SEVERITY_USERMSG)
	{  // if there\'s some type of error
	    if(_error.severity() == SEVERITY_NULL)
	    {  //  if there is not an error already
		sprintf(errStr, "\nERROR:  ENTITY #%d %s\n", GetFileId(), 
			EntityName());
		_error.PrependToDetailMsg(errStr);
	    }
		// set the severity for this entity
	    sprintf (errStr, "  %s :  ", attributes[i].Name());
	    _error.AppendToDetailMsg(errStr);
	    _error.GreaterSeverity(severe);
	    _error.AppendToDetailMsg((char *)
				     attributes[i].Error().DetailMsg());
	    _error.AppendToUserMsg((char *)attributes[i].Error().UserMsg());

	}
/*
	if(severe <= SEVERITY_INPUT_ERROR)
	{	// attribute\'s error is non-recoverable 
		// I believe if this error occurs then you cannot recover

	  //  TODO: can you just read to the next comma and try to continue ?
	    STEPread_error(c,i,in);
	    return _error.severity();
	}
*/
	in >> c;
	if ( ! ((c == ',') || (c == ')' ))) { //  input is not a delimiter
	    if(_error.severity() == SEVERITY_NULL)
	    {  //  if there is not an error already
		sprintf(errStr, "\nERROR:  ENTITY #%d %s\n", GetFileId(), 
			EntityName());
		_error.PrependToDetailMsg(errStr);
	    }
	    _error.AppendToDetailMsg(
				"delimiter expected after attribute value.\n");
	    CheckRemainingInput(in, &_error, "ENTITY", ",)");
	    if(!in.good())
		return _error.severity();
	    if(_error.severity() <= SEVERITY_INPUT_ERROR)
	    {
		STEPread_error(c,i,in);
		return _error.severity();
	    }
	}
	else if(c == ')')
	{
	    in >> ws;
	    char z = in.peek();
	    if (z == ';')
	    {
		in.get(c);
		return _error.severity();
	    }
	}
    }
    if(c != ')' )
    {
	STEPread_error(c,i,in);
	return _error.severity();
    }
    return SEVERITY_NULL;
}
#endif
