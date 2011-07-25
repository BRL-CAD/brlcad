
/*
* NIST STEP Core Class Library
* clstepcore/STEPaggregate.cc
* April 1997
* K. C. Morris
* David Sauder

* Development of this software was funded by the United States Government,
* and is not subject to copyright.
*/

/* $Id: STEPaggregate.cc,v 3.0.1.7 1997/11/05 21:59:28 sauderd DP3.1 $ */

#include <stdio.h> 

#include <read_func.h>
#include <STEPaggregate.h>
#include <STEPattribute.h>
//#include <STEPentity.h>
#include <instmgr.h>
#include <ExpDict.h>

const int Real_Num_Precision = REAL_NUM_PRECISION; // from STEPattribute.h

#define STRING_DELIM '\''

static char rcsid[] = "$Id: STEPaggregate.cc,v 3.0.1.7 1997/11/05 21:59:28 sauderd DP3.1 $";

/******************************************************************
**	  Functions for manipulating aggregate attributes

**  KNOWN BUGs:  
**     -- treatment of aggregates of reals or ints is inconsistent with
**        other aggregates (there's no classes for these)
**     -- no two- dimensional aggregates are implemented
**/

STEPaggregate NilSTEPaggregate;


///////////////////////////////////////////////////////////////////////////////
// STEPaggregate
///////////////////////////////////////////////////////////////////////////////

STEPaggregate::STEPaggregate  ()  
{
    _null = 1;
}

STEPaggregate::~STEPaggregate  ()  
{
}

STEPaggregate& 
STEPaggregate::ShallowCopy (const STEPaggregate& a) 
{
  cerr << "Internal error:  " << __FILE__ << ": " <<  __LINE__
       << "\n" << _POC_ "\n";
  cerr << "function:  STEPaggregate::ShallowCopy \n" << "\n";
  return *this;
}

    // do not require exchange file format
Severity 
STEPaggregate::AggrValidLevel(const char *value, ErrorDescriptor *err, 
			      const TypeDescriptor *elem_type, InstMgr *insts, 
			      int optional, char *tokenList, int addFileId, 
			      int clearError)
{
    std::string buf;
    if(clearError)
	err->ClearErrorMsg();

    istringstream in ((char *)value); // sz defaults to length of s

    ReadValue(in, err, elem_type, insts, addFileId, 0, 0);
    CheckRemainingInput(in, err, elem_type->AttrTypeName(buf), tokenList);
    if( optional && (err->severity() == SEVERITY_INCOMPLETE) )
	err->severity(SEVERITY_NULL);
    return err->severity();
}

    // require exchange file format
Severity 
STEPaggregate::AggrValidLevel(istream &in, ErrorDescriptor *err, 
			      const TypeDescriptor *elem_type, InstMgr *insts, 
			      int optional, char *tokenList, int addFileId, 
			      int clearError)
{
    std::string buf;
    if(clearError)
	err->ClearErrorMsg();

    ReadValue(in, err, elem_type, insts, addFileId, 0, 1);
    CheckRemainingInput(in, err, elem_type->AttrTypeName(buf), tokenList);
    if( optional && (err->severity() == SEVERITY_INCOMPLETE) )
	err->severity(SEVERITY_NULL);
    return err->severity();
}

// if exchangeFileFormat == 1 then paren delims are required.

Severity 
STEPaggregate::ReadValue(istream &in, ErrorDescriptor *err,
			 const TypeDescriptor *elem_type, InstMgr *insts,
			 int addFileId, int assignVal, int exchangeFileFormat,
			 const char *)
{
    ErrorDescriptor errdesc;
    char errmsg[BUFSIZ];
    int value_cnt = 0;
    std::string buf;

    if(assignVal)
	Empty ();  // read new values and discard existing ones

    char c;
    int validDelims = 1;

    in >> ws; // skip white space

    c = in.peek(); // does not advance input

    if(in.eof() || c == '$')
    {
	_null = 1;
	err->GreaterSeverity(SEVERITY_INCOMPLETE);
	return SEVERITY_INCOMPLETE;
    }

    if(c == '(')
    {
	in.get(c);
	validDelims = 0; // signal expectation for end paren delim
    }
    else if(exchangeFileFormat)
    {	// error did not find opening delim
	// cannot recover so give up and let STEPattribute recover
	err->GreaterSeverity(SEVERITY_INPUT_ERROR);
	return SEVERITY_INPUT_ERROR;
    }
    else if(!in.good())
    {// this should actually have been caught by skipping white space above
	err->GreaterSeverity(SEVERITY_INCOMPLETE);
	return SEVERITY_INCOMPLETE;
    }

    STEPnode * item = 0;

    in >> ws;
    // take a peek to see if there are any elements before committing to an 
    // element
    c = in.peek(); // does not advance input
    if(c == ')')
    {
	in.get(c);
    }
    // if not assigning values only need one node. So only one node is created.
    // It is used to read the values
    else if(!assignVal)
	item = (STEPnode*)NewNode();

	// ')' is the end of the aggregate
    while (in.good() && (c != ')') )
    {
	value_cnt++;
	if(assignVal) // create a new node each time through the loop
	    item = (STEPnode*)NewNode();

	errdesc.ClearErrorMsg();

	if(exchangeFileFormat)
	    item->STEPread(in, &errdesc);
	else
	    item->StrToVal(in, &errdesc);

	// read up to the next delimiter and set errors if garbage is
	// found before specified delims (i.e. comma and quote)
	CheckRemainingInput(in, &errdesc, elem_type->AttrTypeName(buf), ",)");

	if (errdesc.severity() < SEVERITY_INCOMPLETE)
	{
	    sprintf (errmsg, "  index:  %d\n", value_cnt );
	    errdesc.PrependToDetailMsg(errmsg);
	    err->AppendFromErrorArg(&errdesc);
	}
	if(assignVal) // pass the node to STEPaggregate
	    AddNode (item);

	in >> ws; // skip white space (although should already be skipped)
	in.get(c); // read delim

	// CheckRemainingInput should have left the input right at the delim
	// so that it would be read in in.get() above.  Since it did not find
	// the delim this does not know how to find it either!
	if( (c != ',') && (c != ')') )
	{
	    // cannot recover so give up and let STEPattribute recover
	    err->GreaterSeverity(SEVERITY_INPUT_ERROR);
	    return SEVERITY_INPUT_ERROR;
/*
	    // error read until you find a delimiter
	    std::string tmp;
	    while(in.good() && !strchr(",)", c) )
	    {
		in.get(c);
		tmp.Append(c);
	    }
	    // BUG could overwrite the error message buffer
	    sprintf(errmsg, "ERROR aggr. elem. followed by \'%s\' garbage.\n",
		    tmp.chars());
	    err->AppendToDetailMsg(errmsg);
	    err->AppendToUserMsg(errmsg);
	    err->GreaterSeverity(SEVERITY_WARNING);
	    if(!in.good())
		return err->severity();
*/
	}
    }
    if(c == ')')
    {
	_null = 0;
//	validDelims = 1; // expectation for end paren delim is met
    }
    else // expectation for end paren delim has not been met
    {
	err->GreaterSeverity(SEVERITY_INPUT_ERROR);
	err->AppendToUserMsg("Missing close paren for aggregate value");
	return SEVERITY_INPUT_ERROR;
    }
    return err->severity();
}

Severity
STEPaggregate::StrToVal(const char *s, ErrorDescriptor *err,
			const TypeDescriptor *elem_type, InstMgr *insts,
			int addFileId)
{
    istringstream in((char *)s);
    return ReadValue(in, err, elem_type, insts, addFileId, 1, 0);
}

///////////////////////////////////////////////////////////////////////////////

Severity
STEPaggregate::STEPread(istream& in, ErrorDescriptor *err,
			const TypeDescriptor *elem_type, InstMgr *insts,
			int addFileId, const char *currSch)
{
    return ReadValue(in, err, elem_type, insts, addFileId, 1, 1, currSch);
}

const char *
STEPaggregate::asStr(std::string & s) const
{
    s.clear();

    if(!_null)
    {
	s = "(";
	STEPnode * n = (STEPnode *) head;
	std::string tmp;
	while (n)
	{
	    s.append( n->STEPwrite(tmp) );
	    if (n = (STEPnode *) n -> NextNode ())
		s.append(",");
	}
	s.append(")");
    }
    return const_cast<char *>(s.c_str());
}

void
STEPaggregate::STEPwrite(ostream& out, const char *currSch) const
{
    if(!_null)
    {
	out << '(';
	STEPnode * n = (STEPnode *)head;
	std::string s;
	while (n)  
	{
	    out << n->STEPwrite (s, currSch);
	    if ( n = (STEPnode *)(n -> NextNode ()) )
		out <<  ',';
	}
	out << ')';
    }
    else
	out << '$';
}

SingleLinkNode *
STEPaggregate::NewNode () 
{
  cerr << "Internal error:  " << __FILE__ << ": " <<  __LINE__ << "\n" ;
  cerr << "function:  STEPaggregate::NewNode \n" << _POC_ << "\n";
  return 0;
}

void
STEPaggregate::AddNode(SingleLinkNode *n)
{
    SingleLinkList::AppendNode(n);
    _null = 0;
}

void
STEPaggregate::Empty()
{
    SingleLinkList::Empty();
    _null = 1;
}


///////////////////////////////////////////////////////////////////////////////
// STEPnode
///////////////////////////////////////////////////////////////////////////////

Severity
STEPnode::StrToVal(const char *s, ErrorDescriptor *err)
{
    // defined in subtypes
  cerr << "Internal error:  " << __FILE__ << ": " <<  __LINE__ << "\n" ;
    err->AppendToDetailMsg(
	" function: STEPnode::StrToVal() called instead of virtual function.\n"
			   );
    err->AppendToDetailMsg("Aggr. attr value: '\n");
    err->AppendToDetailMsg("not assigned.\n");
    err->AppendToDetailMsg(_POC_);
    err->GreaterSeverity(SEVERITY_BUG);
    return SEVERITY_BUG;
}

Severity
STEPnode::StrToVal(istream &in, ErrorDescriptor *err)
{
    // defined in subtypes
  cerr << "Internal error:  " << __FILE__ << ": " <<  __LINE__ << "\n" ;
    err->AppendToDetailMsg(
	" function: STEPnode::StrToVal() called instead of virtual function.\n"
			   );
    err->AppendToDetailMsg("Aggr. attr value: '\n");
    err->AppendToDetailMsg("not assigned.\n");
    err->AppendToDetailMsg(_POC_);
    err->GreaterSeverity(SEVERITY_BUG);
    return SEVERITY_BUG;
}

Severity 
STEPnode::STEPread(const char *s, ErrorDescriptor *err)
{
    //  defined in subclasses
  cerr << "Internal error:  " << __FILE__ << ": " <<  __LINE__ << "\n" ;
  cerr << "function:  STEPnode::STEPread called instead of virtual function.\n"
       << _POC_ << "\n";

    err->AppendToDetailMsg(
	" function: STEPnode::STEPread() called instead of virtual function.\n"
			   );
    err->AppendToDetailMsg(_POC_);
    err->GreaterSeverity(SEVERITY_BUG);

    return SEVERITY_BUG;
}

Severity 
STEPnode::STEPread(istream &in, ErrorDescriptor *err)
{
  cerr << "Internal error:  " << __FILE__ << ": " <<  __LINE__ << "\n" ;
  cerr << "function:  STEPnode::STEPread called instead of virtual function.\n"
       << _POC_ << "\n";

    err->AppendToDetailMsg(
	" function: STEPnode::STEPread() called instead of virtual function.\n"
			   );
    err->AppendToDetailMsg(_POC_);
    err->GreaterSeverity(SEVERITY_BUG);
    return SEVERITY_BUG;
}

const char *
STEPnode::asStr(std::string &s)
{
    //  defined in subclasses
  cerr << "Internal error:  " << __FILE__ << ": " <<  __LINE__ << "\n" ;
  cerr << "function:  STEPnode::asStr called instead of virtual function.\n" 
       << _POC_ << "\n";

  return "";
}

const char *
STEPnode::STEPwrite (std::string &s, const char *currSch)
    /*
     * NOTE - second argument will contain name of current schema.  We may need
     * this if STEPnode belongs to an aggregate of selects.  If so, each node
     * will be written out by calling STEPwrite on its select, and to write a
     * select out, the name of its underlying type will be written.  Finally,
     * the underlying type's name depends on the current schema.  This is be-
     * cause e.g. type X may be defined in schema A, and may be USEd in schema
     * B and renamed to Y (i.e., "USE from A (X as Y)").  Thus, if currSch = B,
     * Y will have to be written out rather than X.  Actually, this concern
     * only applies for SelectNode.  To accomodate those cases, all the signa-
     * tures of STEPwrite(std::string) contain currSch.  (As an additional note,
     * 2D aggregates should make use of currSch in case they are 2D aggrs of
     * selects.  But since currently (3/27/97) the SCL handles 2D+ aggrs using
     * SCLundefined's, this is not implemented.)
     */
{
  cerr << "Internal error:  " << __FILE__ << ": " <<  __LINE__ << "\n" ;
  cerr << "function:  STEPnode::STEPwrite called instead of virtual function.\n"
       << _POC_ << "\n";

 return "";
}

void 
STEPnode::STEPwrite (ostream& out)
{
  cerr << "Internal error:  " << __FILE__ << ": " <<  __LINE__ << "\n" ;
 cerr << "function:  STEPnode::STEPwrite called instead of virtual function.\n"
       << _POC_ << "\n";
}

///////////////////////////////////////////////////////////////////////////////
// GenericAggregate
///////////////////////////////////////////////////////////////////////////////

GenericAggregate::GenericAggregate() 
{
}

GenericAggregate::~GenericAggregate() 
{
}

SingleLinkNode *
GenericAggregate::NewNode () 
{
#ifdef __OSTORE__
    return new (os_segment::of(this),
		GenericAggrNode::get_os_typespec()) GenericAggrNode();
#else
    return new GenericAggrNode();
#endif
}

STEPaggregate& 
GenericAggregate::ShallowCopy (const STEPaggregate& a)
{
    Empty();
    
    SingleLinkNode* next = a.GetHead();
    SingleLinkNode* copy;

    while (next) 
    {
#ifdef __OSTORE__
	copy = new (os_segment::of(this), 
		    GenericAggrNode::get_os_typespec()) 
				GenericAggrNode (*(GenericAggrNode*)next);
#else
	copy = new GenericAggrNode (*(GenericAggrNode*)next);
#endif
	AddNode(copy);
	next = next->NextNode();
    }
    if(head)
	_null = 0;
    else
	_null = 1;
    return *this;
    
}

///////////////////////////////////////////////////////////////////////////////
// GenericAggrNode
///////////////////////////////////////////////////////////////////////////////

GenericAggrNode::GenericAggrNode (const char *str)
{  
    value = str;
}

GenericAggrNode::GenericAggrNode (GenericAggrNode& gan)
{  
    value = gan.value;
}

GenericAggrNode::GenericAggrNode()
{
}

GenericAggrNode::~GenericAggrNode()
{
}

SingleLinkNode *
GenericAggrNode::NewNode () 
{
#ifdef __OSTORE__
    return new (os_segment::of(this), 
		GenericAggrNode::get_os_typespec()) GenericAggrNode();
#else
    return new GenericAggrNode();
#endif
}

Severity 
GenericAggrNode::StrToVal(const char *s, ErrorDescriptor *err)
{
    return value.STEPread(s, err);
}

//TODO
Severity 
GenericAggrNode::StrToVal(istream &in, ErrorDescriptor *err)
{
    return value.STEPread(in, err);
}

Severity 
GenericAggrNode::STEPread(const char *s, ErrorDescriptor *err)
{
    istringstream in((char *) s);
    return value.STEPread(in, err);
}

Severity 
GenericAggrNode::STEPread(istream &in, ErrorDescriptor *err)
{
    return value.STEPread(in, err);
}

const char *
GenericAggrNode::asStr(std::string &s)  
{
    s.clear();
    value.asStr(s);
    return const_cast<char *>(s.c_str());
}

const char *
GenericAggrNode::STEPwrite(std::string &s, const char *currSch)
{
    return value.STEPwrite(s);
/*
// CHECK do we write dollar signs for nulls within an aggregate? DAS
    if(_value)
	return (const char *)_value;
    else
	return "$";
*/
}

void
GenericAggrNode::STEPwrite (ostream& out)
{
    value.STEPwrite(out);
}

///////////////////////////////////////////////////////////////////////////////
// EntityAggregate
///////////////////////////////////////////////////////////////////////////////
 
EntityAggregate::EntityAggregate () 
{
}

EntityAggregate::~EntityAggregate ()
{
//    delete v;
}


// if exchangeFileFormat == 1 then delims are required.

Severity
EntityAggregate::ReadValue(istream &in, ErrorDescriptor *err,
			   const TypeDescriptor *elem_type, InstMgr *insts,
			   int addFileId, int assignVal,
			   int exchangeFileFormat, const char *)
{
    ErrorDescriptor errdesc;
    char errmsg[BUFSIZ];
    int value_cnt = 0;
    std::string buf;

    if(assignVal)
	Empty ();  // read new values and discard existing ones

    char c;
    int validDelims = 1;

    in >> ws; // skip white space

    c = in.peek(); // does not advance input

    if(in.eof() || (c == '$') )
    {
	_null = 1;
	err->GreaterSeverity(SEVERITY_INCOMPLETE);
	return SEVERITY_INCOMPLETE;
    }

    if(c == '(')
    {
	in.get(c);
	validDelims = 0; // signal expectation for end delim
    }
    else if(exchangeFileFormat)
    {	// error did not find opening delim
	// give up because you do not know where to stop reading.
	err->GreaterSeverity(SEVERITY_INPUT_ERROR);
	return SEVERITY_INPUT_ERROR;
    }
    else if(!in.good())
    {// this should actually have been caught by skipping white space above
	err->GreaterSeverity(SEVERITY_INCOMPLETE);
	return SEVERITY_INCOMPLETE;
    }

    EntityNode * item = 0;

    in >> ws;
    // take a peek to see if there are any elements before committing to an 
    // element
    c = in.peek(); // does not advance input
    if(c == ')')
    {
	in.get(c);
    }
    // if not assigning values only need one node. So only one node is created.
    // It is used to read the values
    else if(!assignVal) 
      // OSTORE note - since this is temporary and deleted anyway don't worry
      // about persistent new
	item = new EntityNode();

    while (in.good() && (c != ')') )
    {
	value_cnt++;
	if(assignVal) // create a new node each time through the loop
#ifdef __OSTORE__
	    item = new (os_segment::of(this), 
			EntityNode::get_os_typespec()) EntityNode();
#else
	    item = new EntityNode();
#endif

	errdesc.ClearErrorMsg();

	if(exchangeFileFormat)
	    item->STEPread(in, &errdesc, elem_type, insts, addFileId);
	else
	    item->StrToVal(in, &errdesc, elem_type, insts, addFileId);

	// read up to the next delimiter and set errors if garbage is
	// found before specified delims (i.e. comma and quote)
	CheckRemainingInput(in, &errdesc, elem_type->AttrTypeName(buf), ",)");

	if (errdesc.severity() < SEVERITY_INCOMPLETE)
	{
	    sprintf (errmsg, "  index:  %d\n", value_cnt );
	    errdesc.PrependToDetailMsg(errmsg);
	    err->AppendFromErrorArg(&errdesc);
	}
	if(assignVal)
	    AddNode (item);

	in >> ws; // skip white space (although should already be skipped)
	in.get(c); // read delim

	// CheckRemainingInput should have left the input right at the delim
	// so that it would be read in in.get() above.  Since it did not find
	// the delim this does not know how to find it either!
	if( (c != ',') && (c != ')') )
	{
	    // cannot recover so give up and let STEPattribute recover
	    err->GreaterSeverity(SEVERITY_INPUT_ERROR);
	    return SEVERITY_INPUT_ERROR;
/*
	    // error read until you find a delimiter
	    std::string tmp;
	    while(in.good() && !strchr(",)", c) )
	    {
		in.get(c);
		tmp.Append(c);
	    }
	    // BUG could overwrite the error message buffer
	    sprintf(errmsg, "ERROR aggr. elem. followed by \'%s\' garbage.\n",
		    tmp.chars());
	    err->AppendToDetailMsg(errmsg);
	    err->AppendToUserMsg(errmsg);
	    err->GreaterSeverity(SEVERITY_WARNING);
	    if(!in.good())
		return err->severity();
*/
	}
    }
    if(c == ')')
    {
	_null = 0;
//	validDelims = 1; // expectation for end paren delim is met
    }
    else // expectation for end paren delim has not been met
    {
	err->GreaterSeverity(SEVERITY_INPUT_ERROR);
	err->AppendToUserMsg("Missing close paren for aggregate value");
	return SEVERITY_INPUT_ERROR;
    }
    return err->severity();
}


STEPaggregate& 
EntityAggregate::ShallowCopy (const STEPaggregate& a)  
{
    const EntityNode * tmp = (const EntityNode*) a.GetHead ();
    while (tmp) 
    {
#ifdef __OSTORE__
	AddNode (new (os_segment::of(this), 
		      EntityNode::get_os_typespec()) EntityNode (tmp -> node));
#else
	AddNode (new EntityNode (tmp -> node));
#endif
	tmp = (const EntityNode*) tmp -> NextNode ();
    }
    if(head)
	_null = 0;
    else
	_null = 1;

    return *this;
}


SingleLinkNode *	
EntityAggregate::NewNode ()  
{
#ifdef __OSTORE__
    return new (os_segment::of(this), 
		EntityNode::get_os_typespec()) EntityNode();
#else
    return new EntityNode();
#endif
}

///////////////////////////////////////////////////////////////////////////////
// EntityNode
///////////////////////////////////////////////////////////////////////////////

EntityNode::EntityNode() 
{
}

EntityNode::~EntityNode() 
{
}

EntityNode::EntityNode  (SCLP23(Application_instance) * e) : node (e) 
{
}

SingleLinkNode *	
EntityNode::NewNode ()  
{
#ifdef __OSTORE__
    return new (os_segment::of(this), 
		EntityNode::get_os_typespec()) EntityNode();
#else
    return new EntityNode();
#endif
}
///////////////////////////////////////////////////////////////////////////////

Severity 
EntityNode::StrToVal(const char *s, ErrorDescriptor *err, 
		     const TypeDescriptor *elem_type,
		     InstMgr *insts, int addFileId)
{
    SCLP23(Application_instance) *se = ReadEntityRef(s, err, ",)", insts, 
						     addFileId);
//    SCLP23(Application_instance) *se = ReadEntityRef(s, err, 0, insts, 
//						     addFileId);
    if( se != S_ENTITY_NULL )
    {
	ErrorDescriptor error;
	if(EntityValidLevel(se, elem_type, &error) == SEVERITY_NULL)
	    node = se;
	else
	{
	    node = S_ENTITY_NULL;
	    err->AppendToDetailMsg(error.DetailMsg());
	    err->AppendToUserMsg(error.UserMsg());
	    err->GreaterSeverity(error.severity());
	}
    }
    else
	node = S_ENTITY_NULL;
    return err->severity();
}

Severity 
EntityNode::StrToVal(istream &in, ErrorDescriptor *err, 
		     const TypeDescriptor *elem_type,
		     InstMgr *insts, int addFileId)
{
    return STEPread(in, err, elem_type, insts, addFileId);
}

Severity 
EntityNode::STEPread(const char *s, ErrorDescriptor *err, 
		     const TypeDescriptor *elem_type, 
		     InstMgr *insts, int addFileId)
{
    istringstream in((char *)s);
    return STEPread(in, err, elem_type, insts, addFileId);
}

Severity 
EntityNode::STEPread(istream &in, ErrorDescriptor *err, 
		     const TypeDescriptor *elem_type,
		     InstMgr *insts, int addFileId)
{
    SCLP23(Application_instance) *se = ReadEntityRef(in, err, ",)", insts, 
						     addFileId);
    if( se != S_ENTITY_NULL )
    {
	ErrorDescriptor error;
	if( EntityValidLevel(se, elem_type, &error) == SEVERITY_NULL )
	    node = se;
	else
	{
	    node = S_ENTITY_NULL;
	    err->AppendToDetailMsg(error.DetailMsg());
	    err->AppendToUserMsg(error.UserMsg());
	    err->GreaterSeverity(error.severity());
	}
    }
    else
	node = S_ENTITY_NULL;
//    CheckRemainingInput(in, err, "enumeration", ",)");
    return err->severity();
}

const char *
EntityNode::asStr (std::string &s)  
{
    s.clear();
    if (!node || (node == S_ENTITY_NULL))     //  nothing
	return "";
    else // otherwise return entity id
    {
	char tmp [64];
	sprintf(tmp, "#%d", node->STEPfile_id);
	s = tmp;
    }
    return const_cast<char *>(s.c_str());
}    

const char *
EntityNode::STEPwrite(std::string &s, const char *)
{
    if (!node || (node == S_ENTITY_NULL) )     //  nothing
    {
	s = "$";
	return const_cast<char *>(s.c_str());
    }
    asStr(s);
    return const_cast<char *>(s.c_str());
}

void 
EntityNode::STEPwrite(ostream& out)
{
    if (!node || (node == S_ENTITY_NULL))     //  nothing
      out << "$";
    std::string s;
    out << asStr(s);
}


///////////////////////////////////////////////////////////////////////////////
// SelectAggregate
///////////////////////////////////////////////////////////////////////////////
 
SelectAggregate::SelectAggregate () 
{
}

SelectAggregate::~SelectAggregate ()
{
//    delete v;
}


// if exchangeFileFormat == 1 then delims are required.

Severity
SelectAggregate::ReadValue(istream &in, ErrorDescriptor *err,
			   const TypeDescriptor *elem_type, InstMgr *insts,
			   int addFileId, int assignVal,
			   int exchangeFileFormat, const char *currSch)
{
    ErrorDescriptor errdesc;
    char errmsg[BUFSIZ];
    int value_cnt = 0;
    std::string buf;

    if(assignVal)
	Empty ();  // read new values and discard existing ones

    char c;
    int validDelims = 1;

    in >> ws; // skip white space

    c = in.peek(); // does not advance input

    if(in.eof() || (c == '$') )
    {
	_null = 1;
	err->GreaterSeverity(SEVERITY_INCOMPLETE);
	return SEVERITY_INCOMPLETE;
    }

    if(c == '(')
    {
	in.get(c);
	validDelims = 0; // signal expectation for end delim
    }
    else if(exchangeFileFormat)
    {	// error did not find opening delim
	// give up because you do not know where to stop reading.
	err->GreaterSeverity(SEVERITY_INPUT_ERROR);
	return SEVERITY_INPUT_ERROR;
    }
    else if(!in.good())
    {// this should actually have been caught by skipping white space above
	err->GreaterSeverity(SEVERITY_INCOMPLETE);
	return SEVERITY_INCOMPLETE;
    }

    SelectNode * item = 0;

    in >> ws;
    // take a peek to see if there are any elements before committing to an 
    // element
    c = in.peek(); // does not advance input
    if(c == ')')
    {
	in.get(c);
    }
    // if not assigning values only need one node. So only one node is created.
    // It is used to read the values
    else if(!assignVal)
	item = (SelectNode *) NewNode ();

    while (in.good() && (c != ')') )
    {
	value_cnt++;
	if(assignVal) // create a new node each time through the loop
	  item = (SelectNode *) NewNode ();

	errdesc.ClearErrorMsg();

	if(exchangeFileFormat)
	    item->STEPread(in, &errdesc, elem_type, insts, addFileId, currSch);
	else
	    item->StrToVal(in, &errdesc, elem_type, insts, addFileId, currSch);

	// read up to the next delimiter and set errors if garbage is
	// found before specified delims (i.e. comma and quote)
	CheckRemainingInput(in, &errdesc, elem_type->AttrTypeName(buf), ",)");

	if (errdesc.severity() < SEVERITY_INCOMPLETE)
	{
	    sprintf (errmsg, "  index:  %d\n", value_cnt );
	    errdesc.PrependToDetailMsg(errmsg);
	    err->AppendFromErrorArg(&errdesc);
//	    err->AppendToDetailMsg(errdesc.DetailMsg());
//	    err->AppendToUserMsg(errdesc.UserMsg());
	}
	if(assignVal)
	    AddNode (item);

	in >> ws; // skip white space (although should already be skipped)
	in.get(c); // read delim

	// CheckRemainingInput should have left the input right at the delim
	// so that it would be read in in.get() above.  Since it did not find
	// the delim this does not know how to find it either!
	if( (c != ',') && (c != ')') )
	{
	    // cannot recover so give up and let STEPattribute recover
	    err->GreaterSeverity(SEVERITY_INPUT_ERROR);
	    return SEVERITY_INPUT_ERROR;
	}
    }
    if(c == ')')
    {
	_null = 0;
//	validDelims = 1; // expectation for end paren delim is met
    }
    else // expectation for end paren delim has not been met
    {
	err->GreaterSeverity(SEVERITY_INPUT_ERROR);
	err->AppendToUserMsg("Missing close paren for aggregate value");
	return SEVERITY_INPUT_ERROR;
    }
    return err->severity();
}


STEPaggregate& 
SelectAggregate::ShallowCopy (const STEPaggregate& a)  
{
    const SelectNode * tmp = (const SelectNode*) a.GetHead ();
    while (tmp) 
    {
#ifdef __OSTORE__
	AddNode (new (os_segment::of(this), 
		      SelectNode::get_os_typespec()) SelectNode (tmp -> node));
#else
	AddNode (new SelectNode (tmp -> node));
#endif

	tmp = (const SelectNode*) tmp -> NextNode ();
    }
    if(head)
	_null = 0;
    else
	_null = 1;

    return *this;
}


SingleLinkNode *	
SelectAggregate::NewNode ()  
{
#ifdef __OSTORE__
    return new (os_segment::of(this), 
		SelectNode::get_os_typespec()) SelectNode();
#else
    return new SelectNode();
#endif
}

///////////////////////////////////////////////////////////////////////////////
// SelectNode
///////////////////////////////////////////////////////////////////////////////

SelectNode::SelectNode(SCLP23(Select) * s) :  node (s) 
{
}

SelectNode::SelectNode() 
{
}

SelectNode::~SelectNode() 
{
}

SingleLinkNode *	
SelectNode::NewNode ()  
{
#ifdef __OSTORE__
    return new (os_segment::of(this), 
		SelectNode::get_os_typespec()) SelectNode();
#else
    return new SelectNode();
#endif
}
///////////////////////////////////////////////////////////////////////////////

Severity 
SelectNode::StrToVal(const char *s, ErrorDescriptor *err, 
		     const TypeDescriptor *elem_type,
		     InstMgr *insts, int addFileId)
{
/*
    SCLP23(Application_instance) *se = ReadEntityRef(s, err, ",)", insts, 
						     addFileId);
    if( se != S_ENTITY_NULL )
    {
	ErrorDescriptor error;
	if(SelectValidLevel(se, elem_type, &error) == SEVERITY_NULL)
	    node = se;
	else
	{
	    node = S_ENTITY_NULL;
	    err->AppendToDetailMsg(error.DetailMsg());
	    err->AppendToUserMsg(error.UserMsg());
	    err->GreaterSeverity(error.severity());
	}
    }
    else
	node = S_ENTITY_NULL;
*/
	// KC you will have to decide what to do here
    istringstream in((char*)s);
    if (err->severity( node->STEPread(in, err, insts) ) != SEVERITY_NULL)
	err->AppendToDetailMsg (node ->Error ());
    return err->severity();
}

Severity 
SelectNode::StrToVal(istream &in, ErrorDescriptor *err, 
		     const TypeDescriptor *elem_type,
		     InstMgr *insts, int addFileId, const char *currSch)
{
    return STEPread(in, err, elem_type, insts, addFileId, currSch);
}

Severity 
SelectNode::STEPread(const char *s, ErrorDescriptor *err, 
		     const TypeDescriptor *elem_type, 
		     InstMgr *insts, int addFileId)
{
    istringstream in((char *)s);
    return STEPread(in, err, elem_type, insts, addFileId);
}

Severity 
SelectNode::STEPread(istream &in, ErrorDescriptor *err, 
		     const TypeDescriptor *elem_type,
		     InstMgr *insts, int addFileId, const char *currSch)
{
  if (!node)  {
    cerr << "Internal error:  " << __FILE__ << ": " <<  __LINE__ << "\n" 
	 << _POC_ "\n";
    cerr << "function:  SelectNode::STEPread \n" << "\n";
    return SEVERITY_BUG;
  }
  err->severity( node->STEPread(in, err, insts, 0, addFileId, currSch) );
  CheckRemainingInput(in, err, "select", ",)");
  return err->severity();
}

const char *
SelectNode::asStr (std::string &s)  
{
    s.clear();
    if ( !node || (node->is_null()) )     //  nothing
	return "";
    else // otherwise return entity id
    {
      node -> STEPwrite (s);
      return const_cast<char *>(s.c_str());
    }
}    

const char *
SelectNode::STEPwrite(std::string &s, const char *currSch)
{
    s.clear();
    if ( !node || (node->is_null()) )     //  nothing
    {
	s = "$";
	return "$";
    }
    node -> STEPwrite (s, currSch);
    return const_cast<char *>(s.c_str());
}

void 
SelectNode::STEPwrite(ostream& out)
{
    if ( !node || (node->is_null()) )     //  nothing
      out << "$";
    std::string s;
    out << asStr(s);
}

///////////////////////////////////////////////////////////////////////////////
// StringAggregate
///////////////////////////////////////////////////////////////////////////////

/******************************************************************
STEPaggregate& 
StringAggregate::ShallowCopy (const STEPaggregate&);
******************************************************************/

StringAggregate::StringAggregate() 
{
}

StringAggregate::~StringAggregate() 
{
}

STEPaggregate& 
StringAggregate::ShallowCopy (const STEPaggregate& a)
{
    Empty();
    
    SingleLinkNode* next = a.GetHead();
    SingleLinkNode* copy;

    while (next) 
    {
#ifdef __OSTORE__
	copy = new (os_segment::of(this), 
		    StringNode::get_os_typespec()) 
				StringNode (*(StringNode*)next);
#else
	copy = new StringNode (*(StringNode*)next);
#endif
	AddNode(copy);
	next = next->NextNode();
    }
    if(head)
	_null = 0;
    else
	_null = 1;
    return *this;
    
}

SingleLinkNode *
StringAggregate::NewNode () 
{
#ifdef __OSTORE__
    return new (os_segment::of(this), 
		StringNode::get_os_typespec()) StringNode();
#else
    return new StringNode();
#endif
}

///////////////////////////////////////////////////////////////////////////////
// StringNode
///////////////////////////////////////////////////////////////////////////////

StringNode::StringNode() 
{
    value = 0;
}

StringNode::~StringNode() 
{
}

StringNode::StringNode(StringNode& sn)
{
    value = sn.value.c_str();
}

StringNode::StringNode(const char * sStr)
{
    // value is an SCLP23(String) (the memory is copied)
    value = sStr;

/*
  // I do not think that you are expecting sStr in exchange file format
    ErrorDescriptor err;
    if(value.STEPread(sStr, &err) < SEVERITY_USERMSG)
	value.set_null();
*/
}

SingleLinkNode *
StringNode::NewNode () 
{
#ifdef __OSTORE__
    return new (os_segment::of(this), 
		StringNode::get_os_typespec()) StringNode();
#else
    return new StringNode();
#endif
}

///////////////////////////////////////////////////////////////////////////////
// non-whitespace chars following s are considered garbage and is an error.
// a valid value will still be assigned if it exists before the garbage.
///////////////////////////////////////////////////////////////////////////////

Severity 
StringNode::StrToVal(const char *s, ErrorDescriptor *err)
{
    return STEPread(s, err);
}

// this function assumes you will check for garbage following input

Severity 
StringNode::StrToVal(istream &in, ErrorDescriptor *err)
{
    return value.STEPread(in, err);
}

// non-whitespace chars following s are considered garbage and is an error.
// a valid value will still be assigned if it exists before the garbage.
Severity 
StringNode::STEPread(const char *s, ErrorDescriptor *err)
{
    istringstream in((char *)s);

    value.STEPread(in, err);
    CheckRemainingInput(in, err, "string", ",)");
    return err->severity();
}

// this function assumes you will check for garbage following input

Severity 
StringNode::STEPread(istream &in, ErrorDescriptor *err)
{
    return value.STEPread(in, err);
}

const char *
StringNode::asStr(std::string &s)
{
//    return value.asStr(); // this does not put quotes around the value

    value.asStr(s);
    return const_cast<char *>(s.c_str());
}

const char *
StringNode::STEPwrite (std::string &s, const char *)
{
    value.STEPwrite(s);
    return const_cast<char *>(s.c_str());
}

void
StringNode::STEPwrite (ostream& out)
{
    value.STEPwrite(out);
}
///////////////////////////////////////////////////////////////////////////////
// BinaryAggregate
///////////////////////////////////////////////////////////////////////////////

/******************************************************************
STEPaggregate& 
BinaryAggregate::ShallowCopy (const STEPaggregate&);
******************************************************************/

BinaryAggregate::BinaryAggregate() 
{
}

BinaryAggregate::~BinaryAggregate() 
{
}

STEPaggregate& 
BinaryAggregate::ShallowCopy (const STEPaggregate& a)
{
    Empty();
    
    SingleLinkNode* next = a.GetHead();
    SingleLinkNode* copy;

    while (next) 
    {
#ifdef __OSTORE__
	copy = new (os_segment::of(this), 
		    BinaryNode::get_os_typespec()) 
				BinaryNode (*(BinaryNode*)next);
#else
	copy = new BinaryNode (*(BinaryNode*)next);
#endif
	AddNode(copy);
	next = next->NextNode();
    }
    if(head)
	_null = 0;
    else
	_null = 1;
    return *this;
    
}

SingleLinkNode *
BinaryAggregate::NewNode () 
{
#ifdef __OSTORE__
    return new (os_segment::of(this), 
		BinaryNode::get_os_typespec()) BinaryNode();
#else
    return new BinaryNode();
#endif
}

///////////////////////////////////////////////////////////////////////////////
// BinaryNode
///////////////////////////////////////////////////////////////////////////////

BinaryNode::BinaryNode() 
{
    value = 0;
}

BinaryNode::~BinaryNode() 
{
}

BinaryNode::BinaryNode(BinaryNode& bn)
{
    value = bn.value.c_str();
}

BinaryNode::BinaryNode(const char *sStr)
{
    // value is an SCLP23(Binary) (the memory is copied)
    value = sStr;
}

SingleLinkNode *
BinaryNode::NewNode () 
{
#ifdef __OSTORE__
    return new (os_segment::of(this), 
		BinaryNode::get_os_typespec()) BinaryNode();
#else
    return new BinaryNode();
#endif
}

///////////////////////////////////////////////////////////////////////////////
// non-whitespace chars following s are considered garbage and is an error.
// a valid value will still be assigned if it exists before the garbage.
///////////////////////////////////////////////////////////////////////////////

Severity 
BinaryNode::StrToVal(const char *s, ErrorDescriptor *err)
{
    return STEPread(s, err);
}

// this function assumes you will check for garbage following input

Severity 
BinaryNode::StrToVal(istream &in, ErrorDescriptor *err)
{
    return value.STEPread(in, err);
}

// non-whitespace chars following s are considered garbage and is an error.
// a valid value will still be assigned if it exists before the garbage.

Severity 
BinaryNode::STEPread(const char *s, ErrorDescriptor *err)
{
    istringstream in((char *)s);

    value.STEPread(in, err);
    CheckRemainingInput(in, err, "binary", ",)");
    return err->severity();
}

// this function assumes you will check for garbage following input

Severity 
BinaryNode::STEPread(istream &in, ErrorDescriptor *err)
{
    return value.STEPread(in, err);
}

const char *
BinaryNode::asStr(std::string &s)
{
    s = value.c_str();
    return const_cast<char *>(s.c_str());
}

const char *
BinaryNode::STEPwrite (std::string &s, const char *)
{
    value.STEPwrite(s);
    return const_cast<char *>(s.c_str());
}

void
BinaryNode::STEPwrite (ostream& out)
{
    value.STEPwrite(out);
}

///////////////////////////////////////////////////////////////////////////////
// EnumAggregate
///////////////////////////////////////////////////////////////////////////////

// COPY
STEPaggregate& 
EnumAggregate::ShallowCopy (const STEPaggregate& a)
{
    const EnumNode * tmp = (const EnumNode *) a.GetHead();
    EnumNode * to;
    
    while (tmp) 
    {
	to = (EnumNode *) NewNode ();
	to -> node -> put (tmp -> node ->asInt());
	AddNode (to);
	tmp = (const EnumNode *) tmp -> NextNode ();
    }
    if(head)
	_null = 0;
    else
	_null = 1;

    return *this;
}

EnumAggregate::EnumAggregate ()
{
    
}

EnumAggregate::~EnumAggregate ()
{
    
}

/******************************************************************
 ** Procedure:  EnumAggregate::NewNode
 ** Parameters:  
 ** Returns:  a new EnumNode which is of the correct derived type
 ** Description:  creates a node to put in an list of enumerated values
 **               function is virtual so that the right node will be 
 **               created
 ** Side Effects:  
 ** Status:  ok 2/91
 ******************************************************************/

/*EnumNode **/

SingleLinkNode *
EnumAggregate::NewNode ()
{
    //  defined in subclass
  cerr << "Internal error:  " << __FILE__ << ": " <<  __LINE__ << "\n" ;
  cerr << "function:  EnumAggregate::NewNode () called instead of virtual function. \n" 
       << _POC_ << "\n";
  return 0;
}

///////////////////////////////////////////////////////////////////////////////
// EnumNode
///////////////////////////////////////////////////////////////////////////////

EnumNode::EnumNode(SCLP23(Enum) * e) :  node (e) 
{
}

EnumNode::EnumNode()
{
}

EnumNode::~EnumNode()
{
}

SingleLinkNode *
EnumNode::NewNode ()
{
    //  defined in subclass
  cerr << "Internal error:  " << __FILE__ << ": " <<  __LINE__ << "\n" ;
  cerr << "function:  EnumNode::NewNode () called instead of virtual function. \n" 
       << _POC_ << "\n";
  return 0;
}

/*
// insts and addFileId are used for the EntityNode virtual definition of this
// function
Severity
EnumNode::StrToVal(const char *s, ErrorDescriptor *err)
{
    char messageBuf[BUFSIZ];
    messageBuf[0] = '\0';

    Severity sev = SEVERITY_NULL;
    int len = strlen (s);
    char *val = new char [len + 1];
    val[0] = '\0';
    char *saveForDelete = val;

    int numFound = sscanf((char *)s," %s", val);
    if(numFound != EOF)
    {
	if(val [0] == '.')  // strip the delims
	{
	    val++;
	    char * pos = strchr(val, '.');
	    if (pos) 
		*pos = '\0';
	    else 
	    {
		sev = SEVERITY_WARNING;
		err->GreaterSeverity(SEVERITY_WARNING);
		err->AppendToDetailMsg("Missing matching \'.\' delimiter.\n");
	    }
	}
	if(elem_type)
	{
	    if(elem_type->BaseType() == BOOLEAN_TYPE)
	    {
		switch(val[0])
		{
		  case 't':
		  case 'f':
		  case 'T':
		  case 'F':
		    break;
		  default:
		    sev = SEVERITY_WARNING;
		    err->GreaterSeverity(SEVERITY_WARNING);
		    sprintf(messageBuf, "Invalid Boolean value: \'%s\'.\n",
			    val);
		    err->AppendToDetailMsg(messageBuf);
		    break;
		}
	    }
	}
	    // assign based on the result of this element (the error descriptor
	    // contains the error level for the whole aggregate).
//	if(assignVal && sev > SEVERITY_WARNING)
	if(1 && sev > SEVERITY_WARNING)
	    node -> put (val);
    }
//SCLP23(Enum)::EnumValidLevel(const char *value, ErrorDescriptor *err,
//				int optional, char *tokenList, 
//				int needDelims, int clearError)

    node->EnumValidLevel((char *)val, err, 0, 0, 0, 0);
    delete [] saveForDelete;

    // an element being null shouldn't make the aggregate incomplete!!
    if(err->severity() == SEVERITY_INCOMPLETE)
	err->severity(SEVERITY_NULL);
    return err->severity();
}
*/

///////////////////////////////////////////////////////////////////////////////
// non-whitespace chars following s are considered garbage and is an error.
// a valid value will still be assigned if it exists before the garbage.
///////////////////////////////////////////////////////////////////////////////

Severity
EnumNode::StrToVal(const char *s, ErrorDescriptor *err)
{
    return STEPread(s, err);
}

// this function assumes you will check for garbage following input

Severity 
EnumNode::StrToVal(istream &in, ErrorDescriptor *err)
{
    return node->STEPread(in, err);
}

// non-whitespace chars following s are considered garbage and is an error.
// a valid value will still be assigned if it exists before the garbage.

Severity 
EnumNode::STEPread(const char *s, ErrorDescriptor *err)
{
    istringstream in((char *)s); // sz defaults to length of s

    int nullable = 0;
    node->STEPread (in, err,  nullable);
    CheckRemainingInput(in, err, "enumeration", ",)");
    return err->severity();
}

// this function assumes you will check for garbage following input

Severity 
EnumNode::STEPread(istream &in, ErrorDescriptor *err)
{
    int nullable = 0;
    node->STEPread (in, err,  nullable);
    return err->severity();
}

const char *
EnumNode::asStr (std::string &s)  
{
    node -> asStr(s);
    return const_cast<char *>(s.c_str());
}

const char *
EnumNode::STEPwrite (std::string &s, const char *)
{
    node->STEPwrite(s);
    return const_cast<char *>(s.c_str());

/*
    static char buf[BUFSIZ];
    buf[0] = '\0';
    
//    const char *str = asStr();
//    if( (strlen(str) > 0) && (str[0] != '$') )

    if(!(node->is_null()))
    {
	buf[0] = '.';
	buf[1] = '\0';
//	strcat(buf, str);
	strcat(buf, asStr());
	strcat(buf, ".");
    }
    return buf;
 */
}

void 
EnumNode::STEPwrite (ostream& out)
{
//    out << '.' << asStr() << '.';
    node->STEPwrite(out);
}

///////////////////////////////////////////////////////////////////////////////
// LOGICALS
///////////////////////////////////////////////////////////////////////////////

LOGICALS::LOGICALS() 
{
}

LOGICALS::~LOGICALS() 
{
}

//EnumNode * 
SingleLinkNode *
LOGICALS::NewNode ()  
{
#ifdef __OSTORE__
    return new (os_segment::of(this), EnumNode::get_os_typespec()) 
			EnumNode( new (os_segment::of(this), 
				       SCLP23(LOGICAL)::get_os_typespec()) SCLP23(LOGICAL) );
#else
    return new EnumNode (new SCLP23(LOGICAL));
#endif
}	

#ifdef __OSTORE__
LOGICALS * 
create_LOGICALS(os_database *db)
{
    return new (db, LOGICALS::get_os_typespec()) LOGICALS;
}
#else
LOGICALS * 
create_LOGICALS()
{
    return new LOGICALS;
}
#endif

///////////////////////////////////////////////////////////////////////////////
// BOOLEANS
///////////////////////////////////////////////////////////////////////////////

BOOLEANS::BOOLEANS() 
{
}

BOOLEANS::~BOOLEANS() 
{
}

//EnumNode * 
SingleLinkNode *
BOOLEANS::NewNode ()  
{
#ifdef __OSTORE__
    return new (os_segment::of(this), EnumNode::get_os_typespec()) 
			EnumNode( new (os_segment::of(this), 
				       SCLP23(BOOLEAN)::get_os_typespec()) SCLP23(BOOLEAN) );
#else
    return new EnumNode (new SCLP23(BOOLEAN));
#endif
}	

#ifdef __OSTORE__
BOOLEANS * 
create_BOOLEANS(os_database *db)
{
    return new (db, BOOLEANS::get_os_typespec()) BOOLEANS;
}
#else
BOOLEANS * 
create_BOOLEANS()
{
    return new BOOLEANS ; 
}
#endif

///////////////////////////////////////////////////////////////////////////////
// RealAggregate
///////////////////////////////////////////////////////////////////////////////

RealAggregate::RealAggregate() 
{
}

RealAggregate::~RealAggregate() 
{
}

SingleLinkNode *
RealAggregate::NewNode ()  
{
#ifdef __OSTORE__
    return new (os_segment::of(this), 
		RealNode::get_os_typespec()) RealNode();
#else
    return new RealNode();
#endif
}	

// COPY
STEPaggregate& 
RealAggregate::ShallowCopy (const STEPaggregate& a)
{
    const RealNode * tmp = (const RealNode *) a.GetHead();
    RealNode * to;
    
    while (tmp) 
    {
	to = (RealNode *) NewNode ();
	to -> value = tmp -> value;
	AddNode (to);
	tmp = (const RealNode *) tmp -> NextNode ();
    }
    if(head)
	_null = 0;
    else
	_null = 1;
    return *this;
}

///////////////////////////////////////////////////////////////////////////////
// IntAggregate
///////////////////////////////////////////////////////////////////////////////

IntAggregate::IntAggregate() 
{
}

IntAggregate::~IntAggregate() 
{
}

SingleLinkNode *
IntAggregate::NewNode ()  
{
#ifdef __OSTORE__
    return new (os_segment::of(this), 
		IntNode::get_os_typespec()) IntNode();
#else
    return new IntNode();
#endif
}	

// COPY
STEPaggregate& 
IntAggregate::ShallowCopy (const STEPaggregate& a)
{
    const IntNode * tmp = (const IntNode *) a.GetHead();
    IntNode * to;
    
    while (tmp) 
    {
	to = (IntNode *) NewNode ();
	to -> value = tmp -> value;
	AddNode (to);
	tmp = (const IntNode *) tmp -> NextNode ();
    }
    if(head)
	_null = 0;
    else
	_null = 1;
    return *this;
}

///////////////////////////////////////////////////////////////////////////////
// RealNode
///////////////////////////////////////////////////////////////////////////////

RealNode::RealNode()
{
    value = S_REAL_NULL; 
}

RealNode::~RealNode()
{
}

SingleLinkNode *
RealNode::NewNode ()  
{
#ifdef __OSTORE__
    return new (os_segment::of(this), 
		RealNode::get_os_typespec()) RealNode();
#else
    return new RealNode();
#endif
}	

Severity 
RealNode::StrToVal(const char *s, ErrorDescriptor *err)
{
    if( ReadReal(value, s, err, ",)") ) // returns true if value is assigned
	_null = 0;
    else
    {
	set_null();
	value = S_REAL_NULL;
    }
    return err->severity ();
}

Severity 
RealNode::StrToVal(istream &in, ErrorDescriptor *err)
{
    if( ReadReal(value, in, err, ",)") ) // returns true if value is assigned
	_null = 0;
    else
    {
	set_null();
	value = S_REAL_NULL;
    }
    return err->severity ();
}


Severity 
RealNode::STEPread(const char *s, ErrorDescriptor *err)
{
    if( ReadReal(value, s, err, ",)") ) // returns true if value is assigned
	_null = 0;
    else
    {
	set_null();
	value = S_REAL_NULL;
    }
    return err->severity ();
}

Severity 
RealNode::STEPread(istream &in, ErrorDescriptor *err)
{
    if( ReadReal(value, in, err, ",)") ) // returns true if value is assigned
	_null = 0;
    else
    {
	set_null();
	value = S_REAL_NULL;
    }
    return err->severity ();
}

const char *
RealNode::asStr(std::string &s)
{
    STEPwrite(s);
    return const_cast<char *>(s.c_str());
}

const char *
RealNode::STEPwrite(std::string &s, const char *)
{
    char tmp[BUFSIZ];
    if(value != S_REAL_NULL)
    {
//	sprintf(tmp, "%.15g", value);
//	sprintf(tmp, "%.*g", Real_Num_Precision, value);
//	s = tmp;
	WriteReal(value,s);
    }
    else
	s.clear();
    return const_cast<char *>(s.c_str());
}

void 
RealNode::STEPwrite(ostream& out)
{
    std::string s;
    out << STEPwrite(s);
}

///////////////////////////////////////////////////////////////////////////////
// IntNode
///////////////////////////////////////////////////////////////////////////////

IntNode::IntNode()
{
    value = S_INT_NULL;
}

IntNode::~IntNode()
{
}

SingleLinkNode *
IntNode::NewNode ()  
{
#ifdef __OSTORE__
    return new (os_segment::of(this), 
		IntNode::get_os_typespec()) IntNode();
#else
    return new IntNode();
#endif
}	

Severity 
IntNode::StrToVal(const char *s, ErrorDescriptor *err)
{
    if( ReadInteger(value, s, err, ",)") ) // returns true if value is assigned
	_null = 0;
    else
    {
	set_null();
	value = S_INT_NULL;
    }
    return err->severity ();
}

Severity 
IntNode::StrToVal(istream &in, ErrorDescriptor *err)
{
    if( ReadInteger(value, in, err, ",)") )// returns true if value is assigned
	_null = 0;
    else
    {
	set_null();
	value = S_INT_NULL;
    }
    return err->severity ();
}

Severity 
IntNode::STEPread(const char *s, ErrorDescriptor *err)
{
    if( ReadInteger(value, s, err, ",)") ) // returns true if value is assigned
	_null = 0;
    else
    {
	set_null();
	value = S_INT_NULL;
    }
    return err->severity ();
}

Severity 
IntNode::STEPread(istream &in, ErrorDescriptor *err)
{
    if( ReadInteger(value, in, err, ",)") ) // returns true if value is assigned
	_null = 0;
    else
    {
	set_null();
	value = S_INT_NULL;
    }
    return err->severity ();
}

const char *
IntNode::asStr(std::string &s)
{
    STEPwrite(s);
    return const_cast<char *>(s.c_str());
}

const char *
IntNode::STEPwrite(std::string &s, const char *)
{
    char tmp[BUFSIZ];
    if(value != S_INT_NULL)
    {
	sprintf(tmp, "%d", value);
	s = tmp;
    }
    else
	s.clear();
    return const_cast<char *>(s.c_str());
}

void 
IntNode::STEPwrite(ostream& out)
{
    std::string s;
    out << STEPwrite(s);
}
