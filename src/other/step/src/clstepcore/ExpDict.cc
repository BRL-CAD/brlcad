
/*
* NIST STEP Core Class Library
* clstepcore/ExpDict.cc
* April 1997
* K. C. Morris
* David Sauder

* Development of this software was funded by the United States Government,
* and is not subject to copyright.
*/

/* $Id: ExpDict.cc,v 3.0.1.7 1998/02/17 19:19:15 sauderd DP3.1 $  */ 
#ifdef HAVE_CONFIG_H
# include <scl_cf.h>
#endif

#include <memory.h>
#include <math.h>
#include <stdio.h>

// to help ObjectCenter
#ifndef HAVE_MEMMOVE
extern "C"
{
void * memmove(void *__s1, const void *__s2, size_t __n);
}
#endif

#include <ExpDict.h> 
#include <STEPaggregate.h> 

/*
const TypeDescriptor * const t_INTEGER_TYPE = & TypeDescriptor
                       ("INTEGER",     // Name
		       INTEGER_TYPE, // FundamentalType
		       "INTEGER");   // Description
extern const TypeDescriptor _t_INTEGER_TYPE
                       ("INTEGER",     // Name
		       INTEGER_TYPE, // FundamentalType
		       "INTEGER");   // Description
		       */
/*const TypeDescriptor * const t_INTEGER_TYPE = &_t_INTEGER_TYPE;*/

/*extern const TypeDescriptor _t_REAL_TYPE ("REAL", REAL_TYPE, "Real");*/
/*const TypeDescriptor * const t_REAL_TYPE = &_t_REAL_TYPE;*/

/*extern const TypeDescriptor _t_STRING_TYPE ("STRING", STRING_TYPE, "String");*/
/*const TypeDescriptor * const t_STRING_TYPE = &_t_STRING_TYPE;*/

/*extern const TypeDescriptor _t_BINARY_TYPE ("BINARY", BINARY_TYPE, "Binary") ;*/
/*const TypeDescriptor * const t_BINARY_TYPE = &_t_BINARY_TYPE;*/

/*extern const TypeDescriptor _t_BOOLEAN_TYPE ("BOOLEAN", BOOLEAN_TYPE, "Boolean") ;*/
/*const TypeDescriptor * const t_BOOLEAN_TYPE = &_t_BOOLEAN_TYPE;*/

/*extern const TypeDescriptor _t_LOGICAL_TYPE ("LOGICAL", LOGICAL_TYPE, "Logical") ;*/
/*const TypeDescriptor * const t_LOGICAL_TYPE = &_t_LOGICAL_TYPE;*/
 
/*extern const TypeDescriptor _t_NUMBER_TYPE ("NUMBER", NUMBER_TYPE, "Number") ;*/
/*const TypeDescriptor * const t_NUMBER_TYPE = &_t_NUMBER_TYPE;*/

/*extern const TypeDescriptor _t_GENERIC_TYPE ("GENERIC", GENERIC_TYPE, "Generic") ;*/
/*const TypeDescriptor * const t_GENERIC_TYPE = &_t_GENERIC_TYPE;*/

Explicit_item_id__set::Explicit_item_id__set (int defaultSize) {
    _bufsize = defaultSize;
    _buf = new Explicit_item_id_ptr[_bufsize];
    _count = 0;
}

Explicit_item_id__set::~Explicit_item_id__set () { delete [] _buf; }

void Explicit_item_id__set::Check (int index) {
    Explicit_item_id_ptr* newbuf;

    if (index >= _bufsize) {
        _bufsize = (index+1) * 2;
        newbuf = new Explicit_item_id_ptr[_bufsize];
        memmove(newbuf, _buf, _count*sizeof(Explicit_item_id_ptr));
        delete _buf;
        _buf = newbuf;
    }
}

void Explicit_item_id__set::Insert (Explicit_item_id_ptr v, int index) {
    Explicit_item_id_ptr* spot;
    index = (index < 0) ? _count : index;

    if (index < _count) {
        Check(_count+1);
        spot = &_buf[index];
        memmove(spot+1, spot, (_count - index)*sizeof(Explicit_item_id_ptr));

    } else {
        Check(index);
        spot = &_buf[index];
    }
    *spot = v;
    ++_count;
}

void Explicit_item_id__set::Append (Explicit_item_id_ptr v) {
    int index = _count;
    Explicit_item_id_ptr* spot;

    if (index < _count) {
        Check(_count+1);
        spot = &_buf[index];
        memmove(spot+1, spot, (_count - index)*sizeof(Explicit_item_id_ptr));

    } else {
        Check(index);
        spot = &_buf[index];
    }
    *spot = v;
    ++_count;
}

void Explicit_item_id__set::Remove (int index) {
    if (0 <= index && index < _count) {
        --_count;
        Explicit_item_id_ptr* spot = &_buf[index];
        memmove(spot, spot+1, (_count - index)*sizeof(Explicit_item_id_ptr));
    }
}

int Explicit_item_id__set::Index (Explicit_item_id_ptr v) {
    for (int i = 0; i < _count; ++i) {
        if (_buf[i] == v) {
            return i;
        }
    }
    return -1;
}

Explicit_item_id_ptr& Explicit_item_id__set::operator[] (int index) {
    Check(index);
//    _count = max(_count, index+1);
    _count = ( (_count > index+1) ? _count : (index+1) );
    return _buf[index];
}

int 
Explicit_item_id__set::Count()
{
    return _count; 
}

void 
Explicit_item_id__set::Clear()
{
    _count = 0; 
}

///////////////////////////////////////////////////////////////////////////////

Implicit_item_id__set::Implicit_item_id__set (int defaultSize) {
    _bufsize = defaultSize;
    _buf = new Implicit_item_id_ptr[_bufsize];
    _count = 0;
}

Implicit_item_id__set::~Implicit_item_id__set () { delete _buf; }

void Implicit_item_id__set::Check (int index) {
    Implicit_item_id_ptr* newbuf;

    if (index >= _bufsize) {
        _bufsize = (index+1) * 2;
        newbuf = new Implicit_item_id_ptr[_bufsize];
        memmove(newbuf, _buf, _count*sizeof(Implicit_item_id_ptr));
        delete _buf;
        _buf = newbuf;
    }
}

void Implicit_item_id__set::Insert (Implicit_item_id_ptr v, int index) {
    Implicit_item_id_ptr* spot;
    index = (index < 0) ? _count : index;

    if (index < _count) {
        Check(_count+1);
        spot = &_buf[index];
        memmove(spot+1, spot, (_count - index)*sizeof(Implicit_item_id_ptr));

    } else {
        Check(index);
        spot = &_buf[index];
    }
    *spot = v;
    ++_count;
}

void Implicit_item_id__set::Append (Implicit_item_id_ptr v) {
    int index = _count;
    Implicit_item_id_ptr* spot;

    if (index < _count) {
        Check(_count+1);
        spot = &_buf[index];
        memmove(spot+1, spot, (_count - index)*sizeof(Implicit_item_id_ptr));

    } else {
        Check(index);
        spot = &_buf[index];
    }
    *spot = v;
    ++_count;
}

void Implicit_item_id__set::Remove (int index) {
    if (0 <= index && index < _count) {
        --_count;
        Implicit_item_id_ptr* spot = &_buf[index];
        memmove(spot, spot+1, (_count - index)*sizeof(Implicit_item_id_ptr));
    }
}

int Implicit_item_id__set::Index (Implicit_item_id_ptr v) {
    for (int i = 0; i < _count; ++i) {
        if (_buf[i] == v) {
            return i;
        }
    }
    return -1;
}

Implicit_item_id_ptr& Implicit_item_id__set::operator[] (int index) {
    Check(index);
//    _count = max(_count, index+1);
    _count = ( (_count > index+1) ? _count : (index+1) );
    return _buf[index];
}

int 
Implicit_item_id__set::Count()
{
    return _count; 
}

void 
Implicit_item_id__set::Clear()
{
    _count = 0; 
}

///////////////////////////////////////////////////////////////////////////////

Interface_spec__set::Interface_spec__set (int defaultSize) {
    _bufsize = defaultSize;
    _buf = new Interface_spec_ptr[_bufsize];
    _count = 0;
}

Interface_spec__set::~Interface_spec__set () { delete [] _buf; }

void Interface_spec__set::Check (int index) {
    Interface_spec_ptr* newbuf;

    if (index >= _bufsize) {
        _bufsize = (index+1) * 2;
        newbuf = new Interface_spec_ptr[_bufsize];
        memmove(newbuf, _buf, _count*sizeof(Interface_spec_ptr));
        delete _buf;
        _buf = newbuf;
    }
}

void Interface_spec__set::Insert (Interface_spec_ptr v, int index) {
    Interface_spec_ptr* spot;
    index = (index < 0) ? _count : index;

    if (index < _count) {
        Check(_count+1);
        spot = &_buf[index];
        memmove(spot+1, spot, (_count - index)*sizeof(Interface_spec_ptr));

    } else {
        Check(index);
        spot = &_buf[index];
    }
    *spot = v;
    ++_count;
}

void Interface_spec__set::Append (Interface_spec_ptr v) {
    int index = _count;
    Interface_spec_ptr* spot;

    if (index < _count) {
        Check(_count+1);
        spot = &_buf[index];
        memmove(spot+1, spot, (_count - index)*sizeof(Interface_spec_ptr));

    } else {
        Check(index);
        spot = &_buf[index];
    }
    *spot = v;
    ++_count;
}

void Interface_spec__set::Remove (int index) {
    if (0 <= index && index < _count) {
        --_count;
        Interface_spec_ptr* spot = &_buf[index];
        memmove(spot, spot+1, (_count - index)*sizeof(Interface_spec_ptr));
    }
}

int Interface_spec__set::Index (Interface_spec_ptr v) {
    for (int i = 0; i < _count; ++i) {
        if (_buf[i] == v) {
            return i;
        }
    }
    return -1;
}

Interface_spec_ptr& Interface_spec__set::operator[] (int index) {
    Check(index);
//    _count = max(_count, index+1);
    _count = ( (_count > index+1) ? _count : (index+1) );
    return _buf[index];
}

int 
Interface_spec__set::Count()
{
    return _count; 
}

void 
Interface_spec__set::Clear()
{
    _count = 0; 
}

///////////////////////////////////////////////////////////////////////////////

Interface_spec::Interface_spec() 
 : _explicit_items(new Explicit_item_id__set), 
   _implicit_items(0), _all_objects(0)
{
}

// not tested
Interface_spec::Interface_spec(Interface_spec &is) 
{
    _explicit_items = new Explicit_item_id__set;
    int count = is._explicit_items->Count();
    int i;
    for (i = 0;i < count; i++)
    {
	(*_explicit_items)[i] = 
	  (*(is._explicit_items))[i];
    }
    _current_schema_id = is._current_schema_id;
    _foreign_schema_id = is._foreign_schema_id;
    _all_objects = is._all_objects;
    _implicit_items = 0;
}

Interface_spec::Interface_spec(const char * cur_sch_id, 
			       const char * foreign_sch_id, int all_objects) 
 : _current_schema_id(cur_sch_id), _explicit_items(new Explicit_item_id__set), 
   _implicit_items(0),_foreign_schema_id(foreign_sch_id), 
   _all_objects(all_objects)
{
}

Interface_spec::~Interface_spec()
{
    delete _explicit_items;
    delete _implicit_items;
}

//////////////////////////////////////////////////////////////////////////////
void 
Schema::AddFunction(const char * f)
{
    if(_function_list == 0)
      _function_list = new scl_char_str__list;
    _function_list->Append((char *)f);
}

void 
Schema::AddGlobal_rule(Global_rule_ptr gr)
{
    if(_global_rules == 0) _global_rules = new Global_rule__set;
    _global_rules->Append(gr);
}

 // not implemented
void 
Schema::global_rules_(Global_rule__set_var &grs)
{
}

void 
Schema::AddProcedure(const char * p)
{
    if(_procedure_list == 0)
      _procedure_list = new scl_char_str__list;
    _procedure_list->Append((char *)p);
}

// the whole schema
void 
Schema::GenerateExpress(ostream& out) const
{
  std::string tmp;
    out << endl << "(* Generating: " << Name() << " *)" << endl;
    out << endl << "SCHEMA " << StrToLower(Name(),tmp) << ";" << endl;
    GenerateUseRefExpress(out);
    // print TYPE definitions
    out << endl << "(* ////////////// TYPE Definitions *)" << endl;
    GenerateTypesExpress(out);

    // print Entity definitions
    out << endl << "(* ////////////// ENTITY Definitions *)" << endl;
    GenerateEntitiesExpress(out);

    int count, i;
    if(_global_rules != 0)
    {
	out << endl << "(* *************RULES************* *)" << endl;
	count = _global_rules->Count();
	for(i = 0; i <  count; i++)
	{
	    out << endl << (*_global_rules)[i]->rule_text_() << endl; 
	}
    }
    if(_function_list != 0)
    {
	out << "(* *************FUNCTIONS************* *)" << endl;
	count = _function_list->Count();
	for(i = 0; i <  count; i++)
	{
	    out << endl << (*_function_list)[i] << endl; 
	}
    }
    if(_procedure_list != 0)
    {
	out << "(* *************PROCEDURES************* *)" << endl;
	count = _procedure_list->Count();
	for(i = 0; i <  count; i++)
	{
	    out << endl << (*_procedure_list)[i] << endl; 
	}
    }
    out << endl << "END_SCHEMA;" << endl;
}

// USE, REFERENCE definitions
void 
Schema::GenerateUseRefExpress(ostream& out) const
{
    int i,k;
    int intf_count;
    int count;
    Interface_spec_ptr is;
    int first_time;
    std::string tmp;

    /////////////////////// print USE statements

    intf_count = _use_interface_list->Count();
    if(intf_count) // there is at least 1 USE interface to a foreign schema
    {
	for (i = 0; i < intf_count; i++) // print out each USE interface
	{
	    is = (*_use_interface_list)[i]; // the 1st USE interface

	    // count is # of USE items in interface
	    count = is->explicit_items_()->Count(); 

	    //Explicit_item_id__set_var eiis = is->explicit_items_();
	    
	    if(count > 0)
	    {
		out << endl << "    USE FROM " 
		   << StrToLower(is->foreign_schema_id_().c_str(),tmp) << endl;
		out << "       (";

		first_time = 1;
		for(k = 0; k < count; k++) // print out each USE item
		{
		    if(first_time) first_time = 0;
		    else out << "," << endl << "\t";
		    if( !((*(is->explicit_items_()))[k]->original_id_().size()) )
		    { // not renamed
			out << (*(is->explicit_items_()))[k]->new_id_().c_str();
		    } else { // renamed
			out << (*(is->explicit_items_()))[k]->original_id_().c_str();
			out << " AS " << (*(is->explicit_items_()))[k]->new_id_().c_str();
		    }
		}
		out << ");" << endl;
	    } else if (is->all_objects_()) {
		out << endl << "    USE FROM " 
		    << StrToLower(is->foreign_schema_id_().c_str(),tmp) << ";" 
		    << endl;
	    }
	}
    }

    /////////////////////// print REFERENCE stmts

    intf_count = _ref_interface_list->Count();
    if(intf_count)//there is at least 1 REFERENCE interface to a foreign schema
    {
	for (i = 0; i < intf_count; i++) // print out each REFERENCE interface
	{
	    is = (*_ref_interface_list)[i]; // the 1st REFERENCE interface

	    // count is # of REFERENCE items in interface
	    count = is->explicit_items_()->Count(); 

	    
	    if(count > 0)
	    {
		
//		out << "    REFERENCE FROM " << (*(is->explicit_items_()))[0]->foreign_schema_().chars() << endl;
		out << endl << "    REFERENCE FROM " 
		   << StrToLower(is->foreign_schema_id_().c_str(),tmp) << endl;
		out << "       (";

		first_time = 1;
		for(k = 0; k < count; k++) // print out each REFERENCE item
		{
		    if(first_time) first_time = 0;
		    else out << "," << endl << "\t";
		    if( (!(*(is->explicit_items_()))[k]->original_id_().size()) )
		    { // not renamed
			out << (*(is->explicit_items_()))[k]->new_id_().c_str();
		    } else { // renamed
			out << (*(is->explicit_items_()))[k]->original_id_().c_str();
			out << " AS " 
			  << (*(is->explicit_items_()))[k]->new_id_().c_str();
		    }
		}
		out << ");" << endl;
	    } else if (is->all_objects_()) {
		out << endl << "    REFERENCE FROM " 
		    << StrToLower(is->foreign_schema_id_().c_str(),tmp) << ";" 
		    << endl;
	    }
	}
    }
}

// TYPE definitions
void 
Schema::GenerateTypesExpress(ostream& out) const
{
    TypeDescItr tdi(_typeList);
    tdi.ResetItr();
    std::string tmp;

    const TypeDescriptor *td = tdi.NextTypeDesc();
    while (td)
    {
	out << endl << td->GenerateExpress(tmp);
	td = tdi.NextTypeDesc();
    }
}

// Entity definitions
void 
Schema::GenerateEntitiesExpress(ostream& out) const
{
    EntityDescItr edi(_entList);
    edi.ResetItr();
    std::string tmp;

    const EntityDescriptor *ed = edi.NextEntityDesc();
    while (ed)
    {
	out << endl << ed->GenerateExpress(tmp);
	ed = edi.NextEntityDesc();
    }
}

///////////////////////////////////////////////////////////////////////////////

#ifdef __OSTORE__
EnumAggregate * create_EnumAggregate(os_database *db)
{
    return new (db, EnumAggregate::get_os_typespec()) EnumAggregate; 
}
#else
EnumAggregate * create_EnumAggregate()
{
    return new EnumAggregate; 
}
#endif

#ifdef __OSTORE__
GenericAggregate * create_GenericAggregate(os_database *db)
{ 
    return new (db, GenericAggregate::get_os_typespec()) GenericAggregate; 
}
#else
GenericAggregate * create_GenericAggregate()
{ 
    return new GenericAggregate; 
}
#endif

#ifdef __OSTORE__
EntityAggregate * create_EntityAggregate(os_database *db)
{
    return new (db, EntityAggregate::get_os_typespec()) EntityAggregate; 
}
#else
EntityAggregate * create_EntityAggregate()
{
    return new EntityAggregate; 
}
#endif

#ifdef __OSTORE__
SelectAggregate * create_SelectAggregate(os_database *db)
{
    return new (db, SelectAggregate::get_os_typespec()) SelectAggregate;
}
#else
SelectAggregate * create_SelectAggregate()
{
    return new SelectAggregate; 
}
#endif

#ifdef __OSTORE__
StringAggregate * create_StringAggregate(os_database *db)
{
    return new (db, StringAggregate::get_os_typespec()) StringAggregate;
}
#else
StringAggregate * create_StringAggregate()
{
    return new StringAggregate; 
}
#endif

#ifdef __OSTORE__
BinaryAggregate * create_BinaryAggregate(os_database *db)
{
    return new (db, BinaryAggregate::get_os_typespec()) BinaryAggregate;
}
#else
BinaryAggregate * create_BinaryAggregate()
{
    return new BinaryAggregate; 
}
#endif

#ifdef __OSTORE__
RealAggregate * create_RealAggregate(os_database *db)
{
    return new (db, RealAggregate::get_os_typespec()) RealAggregate;
}
#else
RealAggregate * create_RealAggregate()
{
    return new RealAggregate; 
}
#endif

#ifdef __OSTORE__
IntAggregate * create_IntAggregate(os_database *db)
{
    return new (db, IntAggregate::get_os_typespec()) IntAggregate;
}
#else
IntAggregate * create_IntAggregate()
{
    return new IntAggregate; 
}
#endif

const EntityDescriptor * 
EntityDescItr::NextEntityDesc()
{
    if(cur)
    {
	const EntityDescriptor *ed = cur->EntityDesc();
	cur = (EntityDescLinkNode *)( cur->NextNode() );
	return ed;
    }
    return 0;
} 

const AttrDescriptor * 
AttrDescItr::NextAttrDesc()
{
    if(cur)
    {
	const AttrDescriptor *ad = cur->AttrDesc();
	cur = (AttrDescLinkNode *)( cur->NextNode() );
	return ad;
    }
    return 0;
} 

const Inverse_attribute * 
InverseAItr::NextInverse_attribute()
{
    if(cur)
    {
	const Inverse_attribute *ia = cur->Inverse_attr();
	cur = (Inverse_attributeLinkNode *)( cur->NextNode() );
	return ia;
    }
    return 0;
} 

const TypeDescriptor * 
TypeDescItr::NextTypeDesc()
{
    if(cur)
    {
	const TypeDescriptor *td = cur->TypeDesc();
	cur = (TypeDescLinkNode *)( cur->NextNode() );
	return td;
    }
    return 0;
} 

///////////////////////////////////////////////////////////////////////////////
// AttrDescriptor functions
///////////////////////////////////////////////////////////////////////////////

const char *
AttrDescriptor::AttrExprDefStr(std::string & s) const
{
  std::string buf;

  s = Name ();
  s.append (" : ");
  if(_optional.asInt() == LTrue)
    s.append( "OPTIONAL ");
  if(DomainType())
	s.append (DomainType()->AttrTypeName(buf));
  return const_cast<char *>(s.c_str());
}    

const PrimitiveType 
AttrDescriptor::BaseType() const
{
    if(_domainType)
	return _domainType->BaseType();
    return UNKNOWN_TYPE;
}

int 
AttrDescriptor::IsAggrType() const
{
    return ReferentType()->IsAggrType();
}

const PrimitiveType 
AttrDescriptor::AggrElemType() const
{
    if(IsAggrType())
    {
	return ReferentType()->AggrElemType();
    }
    return UNKNOWN_TYPE;
}

const TypeDescriptor *
AttrDescriptor::AggrElemTypeDescriptor() const
{
    if(IsAggrType())
    {
	return ReferentType()->AggrElemTypeDescriptor();
    }
    return 0;
}

const TypeDescriptor * 
AttrDescriptor::NonRefTypeDescriptor() const
{
    if(_domainType)
	return _domainType->NonRefTypeDescriptor();
    return 0;
}

const PrimitiveType 
AttrDescriptor::NonRefType() const
{
    if(_domainType)
	return _domainType->NonRefType();
    return UNKNOWN_TYPE;
}

const PrimitiveType 
AttrDescriptor::Type() const
{
    if(_domainType)
	return _domainType->Type();
    return UNKNOWN_TYPE;
}

	// right side of attr def
// NOTE this returns a \'const char * \' instead of an std::string
const char *
AttrDescriptor::TypeName() const
{
  std::string buf;

    if(_domainType)
	return _domainType->AttrTypeName(buf);
    else
	return "";
}

	// an expanded right side of attr def
const char *
AttrDescriptor::ExpandedTypeName(std::string & s) const
{
    s.clear();
    if (Derived() == LTrue) s = "DERIVE  ";
    if(_domainType)
    {
      std::string tmp;
	return const_cast<char *>((s.append (_domainType->TypeString(tmp)).c_str()));
    }
    else
	return 0;
}

const char * 
AttrDescriptor::GenerateExpress (std::string &buf) const
{
    char tmp[BUFSIZ];
    std::string sstr;
    buf = AttrExprDefStr(sstr);
    buf.append(";\n");

/*
//    buf = "";
    buf.Append(StrToLower(Name(),tmp));
    buf.Append(" : ");
    if(_optional == LTrue) {
	buf.Append("OPTIONAL ");
    }
    buf.Append(TypeName());
    buf.Append(";\n");
*/
    return const_cast<char *>(buf.c_str());
}

///////////////////////////////////////////////////////////////////////////////
// Derived_attribute functions
///////////////////////////////////////////////////////////////////////////////

const char *
Derived_attribute::AttrExprDefStr(std::string & s) const
{
  std::string buf;

  s.clear();
  if(Name() && strchr(Name(),'.'))
  {
      s = "SELF\\";
  }
  s.append(Name ());
  s.append (" : ");
/*
  if(_optional.asInt() == LTrue)
    s.append( "OPTIONAL ");
*/
  if(DomainType())
	s.append (DomainType()->AttrTypeName(buf));
  if(_initializer) // this is supposed to exist for a derived attribute.
  {
      s.append(" \n\t\t:= ");
      s.append(_initializer);
  }
  return const_cast<char *>(s.c_str());
}    

///////////////////////////////////////////////////////////////////////////////
// Inverse_attribute functions
///////////////////////////////////////////////////////////////////////////////

const char *
Inverse_attribute::AttrExprDefStr(std::string & s) const
{
  std::string buf;

  s = Name ();
  s.append (" : ");
  if(_optional.asInt() == LTrue)
    s.append( "OPTIONAL ");
  if(DomainType())
	s.append (DomainType()->AttrTypeName(buf));
  s.append(" FOR ");
  s.append(_inverted_attr_id);
  return const_cast<char *>(s.c_str());
}    

///////////////////////////////////////////////////////////////////////////////
// EnumDescriptor functions
///////////////////////////////////////////////////////////////////////////////

EnumTypeDescriptor::EnumTypeDescriptor (const char * nm, PrimitiveType ft, 
					Schema *origSchema, 
					const char * d, EnumCreator f)
: TypeDescriptor (nm, ft, origSchema, d), CreateNewEnum(f)
{
}

#ifdef __OSTORE__
SCLP23(Enum) *
EnumTypeDescriptor::CreateEnum(os_database *db)
{
    if(CreateNewEnum)
      return CreateNewEnum(db);
    else
      return 0;
}
#else
SCLP23(Enum) *
EnumTypeDescriptor::CreateEnum()
{
    if(CreateNewEnum)
      return CreateNewEnum();
    else
      return 0;
}
#endif

const char * 
EnumTypeDescriptor::GenerateExpress (std::string &buf) const
{
    char tmp[BUFSIZ];
    buf = "TYPE ";
    buf.append(StrToLower(Name(),tmp));
    buf.append(" = ENUMERATION OF \n  (");
    const char *desc = Description();
    const char *ptr = &(desc[16]);
    int count;
    Where_rule_ptr wr;
    int i;
    int all_comments = 1;
    
    while(*ptr != '\0') 
    {
	if( *ptr == ',')
	{
	    buf.append(",\n  ");
	} else if (isupper(*ptr)){
	    buf += (char)tolower(*ptr);
	} else {
	    buf += *ptr;
	}
	ptr++;
    }
    buf.append(";\n");
///////////////
    // count is # of WHERE rules
    if(_where_rules != 0)
    {
	count = _where_rules->Count(); 
	for(i = 0; i < count; i++) // print out each UNIQUE rule
	{
	    if( !(*(_where_rules))[i]->_label.size() )
	      all_comments = 0;
	}

	if(all_comments)
	    buf.append("  (* WHERE *)\n");
	else
	    buf.append("  WHERE\n");

	for(i = 0; i < count; i++) // print out each WHERE rule
	{
	    if( !(*(_where_rules))[i]->_comment.empty() )
	    {
		buf.append("    ");
		buf.append( (*(_where_rules))[i]->comment_() );
	    }
	    if( (*(_where_rules))[i]->_label.size() )
	    {
		buf.append("    ");
		buf.append( (*(_where_rules))[i]->label_() );
	    }
	}
    }

    buf.append("END_TYPE;\n");
    return const_cast<char *>(buf.c_str());
}

///////////////////////////////////////////////////////////////////////////////
// EntityDescriptor functions
///////////////////////////////////////////////////////////////////////////////

EntityDescriptor::EntityDescriptor ( ) 
: _abstractEntity(LUnknown), _extMapping(LUnknown),
  _uniqueness_rules((Uniqueness_rule__set_var)0), NewSTEPentity(0)
{
//    _derivedAttr = new StringAggregate;
/*
    _subtypes = 0;
    _supertypes = 0;
    _explicitAttr = 0;
    _inverseAttr = 0;
*/
}

EntityDescriptor::EntityDescriptor (const char * name, // i.e. char *
				    Schema *origSchema, 
				    Logical abstractEntity, // F U or T
				    Logical extMapping,
				    Creator f
/*
				    EntityDescriptorList *subtypes,
				    EntityDescriptorList *supertypes,
				    AttrDescriptorList *explicitAttr,
				    StringAggregate *derivedAttr,
				    Inverse_attributeList *inverseAttr
*/
				    ) 
: TypeDescriptor (name, ENTITY_TYPE, origSchema, name),
  _abstractEntity(abstractEntity), _extMapping(extMapping), 
  _uniqueness_rules((Uniqueness_rule__set_var)0), NewSTEPentity(f)
{
/*
    _subtypes = subtypes;
    _supertypes = supertypes;
    _explicitAttr = explicitAttr;
    _derivedAttr = derivedAttr;
    _inverseAttr = inverseAttr;
*/
}

EntityDescriptor::~EntityDescriptor ()  
{
    delete _uniqueness_rules;
}

const char * 
EntityDescriptor::GenerateExpress (std::string &buf) const
{
    char tmp[BUFSIZ];
    std::string sstr;
    int count;
    Where_rule_ptr wr;
    int i;
    int all_comments = 1;

    buf = "ENTITY ";
    buf.append(StrToLower(Name(),sstr));

    if(strlen(_supertype_stmt.c_str()) > 0)
      buf.append("\n  ");
    buf.append(_supertype_stmt.c_str());

    const EntityDescriptor * ed = 0;
/*
    EntityDescItr edi_sub(_subtypes);
    edi_sub.ResetItr();
    ed = edi_sub.NextEntityDesc();
    int subtypes = 0;
    if(ed) {
	buf.Append("\n  SUPERTYPE OF (ONEOF(");
	buf.Append(StrToLower(ed->Name(),sstr));
	subtypes = 1;
    }
    ed = edi_sub.NextEntityDesc();
    while (ed) {
	buf.Append(",\n\t\t");
	buf.Append(StrToLower(ed->Name(),sstr));
	ed = edi_sub.NextEntityDesc();
    }
    if(subtypes) {
	buf.Append("))");
    }
*/

    EntityDescItr edi_super(_supertypes);
    edi_super.ResetItr();
    ed = edi_super.NextEntityDesc();
    int supertypes = 0;
    if(ed) {
	buf.append("\n  SUBTYPE OF (");
	buf.append(StrToLower(ed->Name(),sstr));
	supertypes = 1;
    }
    ed = edi_super.NextEntityDesc();
    while (ed) {
	buf.append(",\n\t\t");
	buf.append(StrToLower(ed->Name(),sstr));
	ed = edi_super.NextEntityDesc();
    }
    if(supertypes) {
	buf.append(")");
    }

    buf.append(";\n");

    AttrDescItr adi(_explicitAttr);

    adi.ResetItr();
    const AttrDescriptor *ad = adi.NextAttrDesc();

    while (ad) {
	if(ad->AttrType() == AttrType_Explicit)
	{
	    buf.append("    ");
	    buf.append(ad->GenerateExpress(sstr));
	}
	ad = adi.NextAttrDesc();
    }

    adi.ResetItr();
    ad = adi.NextAttrDesc();

    count = 1;
    while (ad) {
	if(ad->AttrType() == AttrType_Deriving)
	{
	    if(count == 1)
	      buf.append("  DERIVE\n");
	    buf.append("    ");
	    buf.append(ad->GenerateExpress(sstr));
	    count++;
	}
	ad = adi.NextAttrDesc();
    }
/////////

    InverseAItr iai(_inverseAttr);

    iai.ResetItr();
    const Inverse_attribute *ia = iai.NextInverse_attribute();

    if(ia)
      buf.append("  INVERSE\n");

    while (ia) {
	buf.append("    ");
	buf.append(ia->GenerateExpress(sstr));
	ia = iai.NextInverse_attribute();
    }
///////////////
    // count is # of UNIQUE rules
    if(_uniqueness_rules != 0)
    {
	count = _uniqueness_rules->Count(); 
	for(i = 0; i < count; i++) // print out each UNIQUE rule
	{
	    if( !(*(_uniqueness_rules))[i]->_label.size() )
	      all_comments = 0;
	}

	if(all_comments)
	    buf.append("  (* UNIQUE *)\n");
	else
	    buf.append("  UNIQUE\n");
	for(i = 0; i < count; i++) // print out each UNIQUE rule
	{
	    if( !(*(_uniqueness_rules))[i]->_comment.empty() )
	    {
		buf.append("    ");
		buf.append( (*(_uniqueness_rules))[i]->comment_() );
		buf.append("\n");
	    }
	    if( (*(_uniqueness_rules))[i]->_label.size() )
	    {
		buf.append("    ");
		buf.append( (*(_uniqueness_rules))[i]->label_() );
		buf.append("\n");
	    }
	}
    }

///////////////
    // count is # of WHERE rules
    if(_where_rules != 0)
    {
	all_comments = 1;
	count = _where_rules->Count(); 
	for(i = 0; i < count; i++) // print out each UNIQUE rule
	{
	    if( !(*(_where_rules))[i]->_label.size() )
	      all_comments = 0;
	}

	if(!all_comments)
	  buf.append("  WHERE\n");
	else
	  buf.append("  (* WHERE *)\n");
	for(i = 0; i < count; i++) // print out each WHERE rule
	{
	    if( !(*(_where_rules))[i]->_comment.empty() )
	    {
		buf.append("    ");
		buf.append( (*(_where_rules))[i]->comment_() );
		buf.append("\n");
	    }
	    if( (*(_where_rules))[i]->_label.size() )
	    {
		buf.append("    ");
		buf.append( (*(_where_rules))[i]->label_() );
		buf.append("\n");
	    }
	}
    }

    buf.append("END_ENTITY;\n");
   
    return const_cast<char *>(buf.c_str());
}

const char *
EntityDescriptor::QualifiedName(std::string &s) const
{
    s.clear();
    EntityDescItr edi(_supertypes);

    int count = 1;
    const EntityDescriptor * ed = 0;
    while(ed = edi.NextEntityDesc())
    {
	if(count > 1)
	{
	    s.append("&");
	}
	s.append(ed->Name());
	count++;
    }
    if(count > 1) s.append("&");
    s.append(Name());
    return const_cast<char *>(s.c_str());
}

const TypeDescriptor * 
EntityDescriptor::IsA (const TypeDescriptor * td) const 
{ if (td -> NonRefType () == ENTITY_TYPE)
    return IsA ((EntityDescriptor *) td);
  else return 0;
}

const EntityDescriptor * 
EntityDescriptor::IsA (const EntityDescriptor * other)  const {
  const EntityDescriptor * found =0;
  const EntityDescLinkNode * link = (const EntityDescLinkNode *) (GetSupertypes ().GetHead());

  if (this == other) return other;
  else {
    while (link && ! found)  {
      found = link -> EntityDesc() -> IsA (other);
      link = (EntityDescLinkNode *) link -> NextNode ();
    }
  }
  return found;
}

/*
EntityDescriptor::FindLongestAttribute()
{
    AttrDescLinkNode *attrPtr = 
		  (AttrDescLinkNode *)(ed->ExplicitAttr().GetHead());
    while( attrPtr != 0)
    {
	if(attrPtr->AttrDesc()->IsEntityType())
	    maxAttrLen = max(maxAttrLen, 
		    (strlen(attrPtr->AttrDesc()->EntityType()->Name()) +
		      strlen(attrPtr->AttrDesc()->Name()) + 3
		     )
		    );
	else
	    maxAttrLen = max(maxAttrLen, 
	      (strlen(attrPtr->AttrDesc()->DomainType()->NameOrDescription()) +
	       strlen(attrPtr->AttrDesc()->Name()) + 3
	      )
		    );
	attrPtr = (AttrDescLinkNode *)attrPtr->NextNode();
    }
}
*/

Type_or_rule::Type_or_rule()
{
}

Type_or_rule::Type_or_rule(const Type_or_rule&tor)
{
}

Type_or_rule::~Type_or_rule()
{
}


Where_rule::Where_rule() 
{
    _type_or_rule = 0;
}


Where_rule::Where_rule(const Where_rule& wr) 
{
    _label = wr._label;
    _type_or_rule = wr._type_or_rule;
}

Where_rule::~Where_rule() 
{
}

///////////////////////////////////////////////////////////////////////////////

Where_rule__list::Where_rule__list (int defaultSize) {
    _bufsize = defaultSize;
    _buf = new Where_rule_ptr[_bufsize];
    _count = 0;
}

Where_rule__list::~Where_rule__list () { delete _buf; }

void Where_rule__list::Check (int index) {
    Where_rule_ptr* newbuf;

    if (index >= _bufsize) {
        _bufsize = (index+1) * 2;
        newbuf = new Where_rule_ptr[_bufsize];
        memmove(newbuf, _buf, _count*sizeof(Where_rule_ptr));
        delete _buf;
        _buf = newbuf;
    }
}

void Where_rule__list::Insert (Where_rule_ptr v, int index) {
    Where_rule_ptr* spot;
    index = (index < 0) ? _count : index;

    if (index < _count) {
        Check(_count+1);
        spot = &_buf[index];
        memmove(spot+1, spot, (_count - index)*sizeof(Where_rule_ptr));

    } else {
        Check(index);
        spot = &_buf[index];
    }
    *spot = v;
    ++_count;
}

void Where_rule__list::Append (Where_rule_ptr v) {
    int index = _count;
    Where_rule_ptr* spot;

    if (index < _count) {
        Check(_count+1);
        spot = &_buf[index];
        memmove(spot+1, spot, (_count - index)*sizeof(Where_rule_ptr));

    } else {
        Check(index);
        spot = &_buf[index];
    }
    *spot = v;
    ++_count;
}

void Where_rule__list::Remove (int index) {
    if (0 <= index && index < _count) {
        --_count;
        Where_rule_ptr* spot = &_buf[index];
        memmove(spot, spot+1, (_count - index)*sizeof(Where_rule_ptr));
    }
}

int Where_rule__list::Index (Where_rule_ptr v) {
    for (int i = 0; i < _count; ++i) {
        if (_buf[i] == v) {
            return i;
        }
    }
    return -1;
}

Where_rule_ptr& Where_rule__list::operator[] (int index) {
    Check(index);
//    _count = max(_count, index+1);
    _count = ( (_count > index+1) ? _count : (index+1) );
    return _buf[index];
}

int 
Where_rule__list::Count()
{
    return _count; 
}

void 
Where_rule__list::Clear()
{
    _count = 0; 
}

///////////////////////////////////////////////////////////////////////////////

Uniqueness_rule::Uniqueness_rule() 
: _parent_entity(0)
{
}


Uniqueness_rule::Uniqueness_rule(const Uniqueness_rule& ur) 
{
    _label = ur._label;
    _parent_entity = ur._parent_entity;
}

Uniqueness_rule::~Uniqueness_rule() 
{
    // don't delete _parent_entity
}

///////////////////////////////////////////////////////////////////////////////

Uniqueness_rule__set::Uniqueness_rule__set (int defaultSize) {
    _bufsize = defaultSize;
    _buf = new Uniqueness_rule_ptr[_bufsize];
    _count = 0;
}

Uniqueness_rule__set::~Uniqueness_rule__set () { delete _buf; }

void Uniqueness_rule__set::Check (int index) {
    Uniqueness_rule_ptr* newbuf;

    if (index >= _bufsize) {
        _bufsize = (index+1) * 2;
        newbuf = new Uniqueness_rule_ptr[_bufsize];
        memmove(newbuf, _buf, _count*sizeof(Uniqueness_rule_ptr));
        delete _buf;
        _buf = newbuf;
    }
}

void Uniqueness_rule__set::Insert (Uniqueness_rule_ptr v, int index) {
    Uniqueness_rule_ptr* spot;
    index = (index < 0) ? _count : index;

    if (index < _count) {
        Check(_count+1);
        spot = &_buf[index];
        memmove(spot+1, spot, (_count - index)*sizeof(Uniqueness_rule_ptr));

    } else {
        Check(index);
        spot = &_buf[index];
    }
    *spot = v;
    ++_count;
}

void Uniqueness_rule__set::Append (Uniqueness_rule_ptr v) {
    int index = _count;
    Uniqueness_rule_ptr* spot;

    if (index < _count) {
        Check(_count+1);
        spot = &_buf[index];
        memmove(spot+1, spot, (_count - index)*sizeof(Uniqueness_rule_ptr));

    } else {
        Check(index);
        spot = &_buf[index];
    }
    *spot = v;
    ++_count;
}

void Uniqueness_rule__set::Remove (int index) {
    if (0 <= index && index < _count) {
        --_count;
        Uniqueness_rule_ptr* spot = &_buf[index];
        memmove(spot, spot+1, (_count - index)*sizeof(Uniqueness_rule_ptr));
    }
}

int Uniqueness_rule__set::Index (Uniqueness_rule_ptr v) {
    for (int i = 0; i < _count; ++i) {
        if (_buf[i] == v) {
            return i;
        }
    }
    return -1;
}

Uniqueness_rule_ptr& Uniqueness_rule__set::operator[] (int index) {
    Check(index);
//    _count = max(_count, index+1);
    _count = ( (_count > index+1) ? _count : (index+1) );
    return _buf[index];
}

int 
Uniqueness_rule__set::Count()
{
    return _count; 
}

void 
Uniqueness_rule__set::Clear()
{
    _count = 0; 
}

///////////////////////////////////////////////////////////////////////////////

Global_rule::Global_rule()
: _entities(0), _where_rules(0), _parent_schema(0)
{
}

Global_rule::Global_rule(const char *n, Schema_ptr parent_sch, const char * rt)
: _name(n), _entities(0), _where_rules(0), _parent_schema(parent_sch), 
  _rule_text(rt)
{
}

// not fully implemented
Global_rule::Global_rule(Global_rule& gr)
{
    _name = gr._name;
    _parent_schema = gr._parent_schema;
}

Global_rule::~Global_rule()
{
    delete _entities;
    delete _where_rules;
}

void 
Global_rule::entities_(const Entity__set_var &e)
{
}

void 
Global_rule::where_rules_(const Where_rule__list_var &wrl)
{
}

///////////////////////////////////////////////////////////////////////////////

Global_rule__set::Global_rule__set (int defaultSize) {
    _bufsize = defaultSize;
    _buf = new Global_rule_ptr[_bufsize];
    _count = 0;
}

Global_rule__set::~Global_rule__set () { delete _buf; }

void Global_rule__set::Check (int index) {
    Global_rule_ptr* newbuf;

    if (index >= _bufsize) {
        _bufsize = (index+1) * 2;
        newbuf = new Global_rule_ptr[_bufsize];
        memmove(newbuf, _buf, _count*sizeof(Global_rule_ptr));
        delete [] _buf;
        _buf = newbuf;
    }
}

void Global_rule__set::Insert (Global_rule_ptr v, int index) {
    Global_rule_ptr* spot;
    index = (index < 0) ? _count : index;

    if (index < _count) {
        Check(_count+1);
        spot = &_buf[index];
        memmove(spot+1, spot, (_count - index)*sizeof(Global_rule_ptr));

    } else {
        Check(index);
        spot = &_buf[index];
    }
    *spot = v;
    ++_count;
}

void Global_rule__set::Append (Global_rule_ptr v) {
    int index = _count;
    Global_rule_ptr* spot;

    if (index < _count) {
        Check(_count+1);
        spot = &_buf[index];
        memmove(spot+1, spot, (_count - index)*sizeof(Global_rule_ptr));

    } else {
        Check(index);
        spot = &_buf[index];
    }
    *spot = v;
    ++_count;
}

void Global_rule__set::Remove (int index) {
    if (0 <= index && index < _count) {
        --_count;
        Global_rule_ptr* spot = &_buf[index];
        memmove(spot, spot+1, (_count - index)*sizeof(Global_rule_ptr));
    }
}

int Global_rule__set::Index (Global_rule_ptr v) {
    for (int i = 0; i < _count; ++i) {
        if (_buf[i] == v) {
            return i;
        }
    }
    return -1;
}

Global_rule_ptr& Global_rule__set::operator[] (int index) {
    Check(index);
//    _count = max(_count, index+1);
    _count = ( (_count > index+1) ? _count : (index+1) );
    return _buf[index];
}

int 
Global_rule__set::Count()
{
    return _count; 
}

void 
Global_rule__set::Clear()
{
    _count = 0; 
}


///////////////////////////////////////////////////////////////////////////////
// TypeDescriptor functions
///////////////////////////////////////////////////////////////////////////////

const char * 
TypeDescriptor::AttrTypeName(std::string &buf, const char *schnm ) const 
{
  std::string sstr;
    buf = Name(schnm) ? StrToLower(Name(schnm),sstr) : _description;
    return const_cast<char *>(buf.c_str());
}	    

const char * 
TypeDescriptor::GenerateExpress (std::string &buf) const
{
    char tmp[BUFSIZ];
    buf = "TYPE ";
    buf.append(StrToLower(Name(),tmp));
    buf.append(" = ");
    const char *desc = Description();
    const char *ptr = desc;
    int count;
    Where_rule_ptr wr;
    int i;
    int all_comments = 1;
    
    while(*ptr != '\0') 
    {
	if( *ptr == ',')
	{
	    buf.append(",\n  ");
	} else if( *ptr == '(') {
	    buf.append("\n  (");
	} else if (isupper(*ptr)){
	    buf += (char)tolower(*ptr);
	} else {
	    buf += *ptr;
	}
	ptr++;
    }
    buf.append(";\n");
///////////////
    // count is # of WHERE rules
    if(_where_rules != 0)
    {
	count = _where_rules->Count(); 
	for(i = 0; i < count; i++) // print out each UNIQUE rule
	{
	    if( !(*(_where_rules))[i]->_label.size() )
	      all_comments = 0;
	}

	if(all_comments)
	    buf.append("  (* WHERE *)\n");
	else
	    buf.append("    WHERE\n");

	for(i = 0; i < count; i++) // print out each WHERE rule
	{
	    if( !(*(_where_rules))[i]->_comment.empty() )
	    {
		buf.append("    ");
		buf.append( (*(_where_rules))[i]->comment_() );
	    }
	    if( (*(_where_rules))[i]->_label.size() )
	    {
		buf.append("      ");
		buf.append( (*(_where_rules))[i]->label_() );
	    }
	}
    }

    buf.append("END_TYPE;\n");
    return const_cast<char *>(buf.c_str());

//    buf.append(Description());
//    buf.append("\nEND_TYPE;\n");
}

	///////////////////////////////////////////////////////////////////////
	// This is a fully expanded description of the type.
	// This returns a string like the _description member variable
	// except it is more thorough of a description where possible
	// e.g. if the description contains a TYPE name it will also
	// be explained.
	///////////////////////////////////////////////////////////////////////
const char *
TypeDescriptor::TypeString(std::string & s) const
{
    switch(Type())
    {
	  case REFERENCE_TYPE:
		if(Name())
		{
		  s.append ( "TYPE ");
		  s.append (Name());
		  s.append ( " = ");
		}
		if(Description())
		  s.append ( Description ());
		if(ReferentType())
		{
		  s.append ( " -- ");
      std::string tmp;
		  s.append ( ReferentType()->TypeString(tmp));
		}
		return const_cast<char *>(s.c_str());

	  case INTEGER_TYPE:
		s.clear();
		if(_referentType != 0)
		{
		    s = "TYPE ";
		    s.append (Name());
		    s.append ( " = ");
		}
		s.append("Integer");
		break;

	  case STRING_TYPE:
		s.clear();
		if(_referentType != 0)
		{
		    s = "TYPE ";
		    s.append (Name());
		    s.append ( " = ");
		}
		s.append("String");
		break;

	  case REAL_TYPE:
		s.clear();
		if(_referentType != 0)
		{
		    s = "TYPE ";
		    s.append (Name());
		    s.append ( " = ");
		}
		s.append("Real");
		break;

	  case ENUM_TYPE:
		s = "Enumeration: ";
		if(Name())
		{
		  s.append ( "TYPE ");
		  s.append (Name());
		  s.append ( " = ");
		}
		if(Description())
		  s.append ( Description ());
		break;

	  case BOOLEAN_TYPE:
		s.clear();
		if(_referentType != 0)
		{
		    s = "TYPE ";
		    s.append (Name());
		    s.append ( " = ");
		}
		s.append("Boolean: F, T");
		break;
	  case LOGICAL_TYPE:
		s.clear();
		if(_referentType != 0)
		{
		    s = "TYPE ";
		    s.append (Name());
		    s.append ( " = ");
		}
		s.append("Logical: F, T, U");
		break;
	  case NUMBER_TYPE:
		s.clear();
		if(_referentType != 0)
		{
		    s = "TYPE ";
		    s.append (Name());
		    s.append ( " = ");
		}
		s.append("Number");
		break;
	  case BINARY_TYPE:
		s.clear();
		if(_referentType != 0)
		{
		    s = "TYPE ";
		    s.append (Name());
		    s.append ( " = ");
		}
		s.append("Binary");
		break;
	  case ENTITY_TYPE:
		s = "Entity: ";
		if(Name())
		  s.append (Name());
		break;
	  case AGGREGATE_TYPE:
	  case ARRAY_TYPE:		// DAS
	  case BAG_TYPE:		// DAS
	  case SET_TYPE:		// DAS
	  case LIST_TYPE:		// DAS
		s = Description();
		if(ReferentType())
		{
		  s.append ( " -- ");
      std::string tmp;
		  s.append ( ReferentType()->TypeString(tmp));
		}
		break;
	  case SELECT_TYPE:
		  s.append ( Description ());
		break;
	  case GENERIC_TYPE:
	  case UNKNOWN_TYPE:
		s = "Unknown";
		break;
    } // end switch
  return const_cast<char *>(s.c_str());

}

const TypeDescriptor * 
TypeDescriptor::IsA (const TypeDescriptor * other)  const {
  if (this == other)  return other;
  return 0;
}

const TypeDescriptor * 
TypeDescriptor::IsA (const char * other) const  {
  if (!Name())  return 0;
  if ( !StrCmpIns( _name, other ) ) {  // this is the type
      return this;
  }
  return (ReferentType () ? ReferentType () -> IsA (other) : 0);
}

	///////////////////////////////////////////////////////////////////////
	// the first PrimitiveType that is not REFERENCE_TYPE (the first 
	// TypeDescriptor *_referentType that does not have REFERENCE_TYPE 
	// for it's fundamentalType variable).  This would return the same 
	// as BaseType() for fundamental types.  An aggregate type
	// would return AGGREGATE_TYPE then you could find out the type of
	// an element by calling AggrElemType().  Select types
	// would work the same?
	///////////////////////////////////////////////////////////////////////

const PrimitiveType 
TypeDescriptor::NonRefType() const
{
    const TypeDescriptor *td = NonRefTypeDescriptor();
    if(td)
	return td->FundamentalType();
    return UNKNOWN_TYPE;
}


const TypeDescriptor *
TypeDescriptor::NonRefTypeDescriptor() const
{  
    const TypeDescriptor *td = this;

    while ( td->ReferentType() ) {
      if (td->Type() != REFERENCE_TYPE) 
	  return td;
      td = td->ReferentType();
    }

    return td;
}

	///////////////////////////////////////////////////////////////////////
	// This returns the PrimitiveType of the first non-aggregate element of 
	// an aggregate
	///////////////////////////////////////////////////////////////////////

int TypeDescriptor::IsAggrType() const
{
    switch(NonRefType())
    {
      case AGGREGATE_TYPE:
      case ARRAY_TYPE:		// DAS
      case BAG_TYPE:		// DAS
      case SET_TYPE:		// DAS
      case LIST_TYPE:		// DAS
	return 1;

      default:
	return 0;
    }
}

const PrimitiveType 
TypeDescriptor::AggrElemType() const
{
    const TypeDescriptor *aggrElemTD = AggrElemTypeDescriptor();
    if(aggrElemTD)
    {
	return aggrElemTD->Type();
    }
    return UNKNOWN_TYPE;
}

const TypeDescriptor *
TypeDescriptor::AggrElemTypeDescriptor() const
{
    const TypeDescriptor *aggrTD = NonRefTypeDescriptor();
    const TypeDescriptor *aggrElemTD = aggrTD->ReferentType();
    if(aggrElemTD)
    {
	aggrElemTD = aggrElemTD->NonRefTypeDescriptor();
    }
    return aggrElemTD;
}

	////////////////////////////////////////////////////////////
	// This is the underlying type of this type. For instance:
	// TYPE count = INTEGER;
	// TYPE ref_count = count;
	// TYPE count_set = SET OF ref_count;
	//  each of the above will generate a TypeDescriptor and for 
	//  each one, PrimitiveType BaseType() will return INTEGER_TYPE
	//  TypeDescriptor *BaseTypeDescriptor() returns the TypeDescriptor 
	//  for Integer
	////////////////////////////////////////////////////////////

const PrimitiveType
TypeDescriptor::BaseType() const
{
    const TypeDescriptor *td = BaseTypeDescriptor();
    if(td)
	return td->FundamentalType();
    else
	return ENTITY_TYPE;
}

const TypeDescriptor *
TypeDescriptor::BaseTypeDescriptor() const
{
    const TypeDescriptor *td = this;

    while (td -> ReferentType ()) td = td->ReferentType ();
    return td;
}
#ifdef NOT_YET
///////////////////////////////////////////////////////////////////////////////
// EnumerationTypeDescriptor functions
///////////////////////////////////////////////////////////////////////////////
EnumerationTypeDescriptor::EnumerationTypeDescriptor( ) 
{
    _elements = new StringAggregate; 
}
#endif
///////////////////////////////////////////////////////////////////////////////
// SelectTypeDescriptor functions
///////////////////////////////////////////////////////////////////////////////

#ifdef __OSTORE__
SCLP23(Select) *
SelectTypeDescriptor::CreateSelect(os_database *db)
{
    if(CreateNewSelect)
      return CreateNewSelect(db);
    else
      return 0;
}
#else
SCLP23(Select) *
SelectTypeDescriptor::CreateSelect()
{
    if(CreateNewSelect)
      return CreateNewSelect();
    else
      return 0;
}
#endif

const TypeDescriptor *
SelectTypeDescriptor::IsA (const TypeDescriptor * other) const
{  return TypeDescriptor::IsA (other);  }

// returns the td among the choices of tds describing elements of this select
// type but only at this unexpanded level. The td ultimately describing the
// type may be an element of a td for a select that is returned.

const TypeDescriptor *
SelectTypeDescriptor::CanBe (const TypeDescriptor * other) const
{
//  const TypeDescriptor * found =0;
  TypeDescItr elements (GetElements()) ;
  const TypeDescriptor * td =0;

  if (this == other) return other;
  while (td = elements.NextTypeDesc ())  {
//    if (found = (td -> CanBe (other))) return found;
    if (td -> CanBe (other)) return td;
  }
  return 0;
}

// returns the td among the choices of tds describing elements of this select
// type but only at this unexpanded level. The td ultimately describing the
// type may be an element of a td for a select that is returned.

const TypeDescriptor *
SelectTypeDescriptor::CanBe (const char * other) const
{
  const TypeDescriptor * found =0;
  TypeDescItr elements (GetElements()) ;
  const TypeDescriptor * td =0;

  // see if other is the select
  if ( !StrCmpIns( _name, other ) )
    return this;

  // see if other is one of the elements
  while (td = elements.NextTypeDesc ())  {
    if (td -> CanBe (other)) return td;
//    if (found = (td -> CanBe (other))) return found;
  }
  return 0;
}

// A modified CanBe, used to determine if "other", a string we have just read,
// is a possible type-choice of this.  (I.e., our select "CanBeSet" to this
// choice.)  This deals with the following issue, based on the Tech Corrigendum
// to Part 21:  Say our select ("selP") has an item which is itself a select
// ("selQ").  Say it has another select item which is a redefinition of another
// select ("TYPE selR = selS;").  According to the T.C., if selP is set to one
// of the members of selQ, "selQ(...)" may not appear in the instantiation.
// If, however, selP is set to a member of selR, "selR(...)" must appear first.
// The code below checks if "other" = one of our possible choices.  If one of
// our choices is a select like selQ, we recurse to see if other matches a
// member of selQ (and don't look for "selQ").  If we have a choice like selR,
// we check if other = "selR", but do not look at selR's members.  This func-
// tion also takes into account schNm, the name of the current schema.  If
// schNm does not = the schema in which this type was declared, it's possible
// that it should be referred to with a different name.  This would be the case
// if schNm = a schema which USEs or REFERENCEs this and renames it (e.g., "USE
// from XX (A as B)").

const TypeDescriptor *
SelectTypeDescriptor::CanBeSet (const char * other, const char *schNm) const
{
  TypeDescItr elements (GetElements()) ;
  const TypeDescriptor * td =0;

  while (td = elements.NextTypeDesc ()) {
      if ( td->Type() == REFERENCE_TYPE && td->NonRefType() == sdaiSELECT ) {
	  // Just look at this level, don't look at my items (see intro).
	  if ( td->CurrName( other, schNm ) ) {
	      return td;
	  }
      } else if ( td->CanBeSet( other, schNm ) ) {
	  return td;
      }
  }
  return 0;
}

///////////////////////////////////////////////////////////////////////////////
// AggrTypeDescriptor functions
///////////////////////////////////////////////////////////////////////////////

#ifdef __OSTORE__
STEPaggregate *
AggrTypeDescriptor::CreateAggregate(os_database *db)
{
    if(CreateNewAggr)
      return CreateNewAggr(db);
    else
      return 0;
}
#else
STEPaggregate *
AggrTypeDescriptor::CreateAggregate()
{
    if(CreateNewAggr)
      return CreateNewAggr();
    else
      return 0;
}
#endif


AggrTypeDescriptor::AggrTypeDescriptor( ) : 
		_uniqueElements("UNKNOWN_TYPE")
{
    _bound1 = -1;
    _bound2 = -1;
    _aggrDomainType = 0;
}

AggrTypeDescriptor::AggrTypeDescriptor(SCLP23(Integer) b1, 
				       SCLP23(Integer) b2, 
				       Logical uniqElem, 
				       TypeDescriptor *aggrDomType)
	      : _bound1(b1), _bound2(b2), _uniqueElements(uniqElem)
{
    _aggrDomainType = aggrDomType;
}

AggrTypeDescriptor::~AggrTypeDescriptor()
{
}
