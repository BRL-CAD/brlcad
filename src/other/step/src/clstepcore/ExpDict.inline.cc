
/*
* NIST STEP Core Class Library
* clstepcore/ExpDict.inline.cc
* April 1997
* K. C. Morris
* David Sauder

* Development of this software was funded by the United States Government,
* and is not subject to copyright.
*/

/* $Id: ExpDict.inline.cc,v 3.0.1.5 1997/11/05 21:59:20 sauderd DP3.1 $  */ 

#include <ExpDict.h>

Dictionary_instance::~Dictionary_instance() 
{}


Schema::Schema (const char *schemaName)
 : _use_interface_list(new Interface_spec__set),
   _ref_interface_list(new Interface_spec__set), 
   _function_list(0), _procedure_list(0), _global_rules(0)
{
    _name = schemaName; 
}

Schema::~Schema ()
{
    if(_use_interface_list != 0)
      delete _use_interface_list;
    if(_ref_interface_list != 0)
      delete _ref_interface_list;
    if(_function_list != 0)
      delete _function_list;
    if(_procedure_list != 0)
      delete _procedure_list;
    if(_global_rules != 0)
      delete _global_rules;
}

Interfaced_item::Interfaced_item()
{
}

Interfaced_item::Interfaced_item(const Interfaced_item &ii)
{
    _foreign_schema = ii._foreign_schema;
}

Interfaced_item::Interfaced_item(const char *foreign_schema)
: _foreign_schema(foreign_schema)
{
}

Interfaced_item::~Interfaced_item()
{
}

const Express_id 
Interfaced_item::foreign_schema_()
{
    return _foreign_schema;
}

void 
Interfaced_item::foreign_schema_(const Express_id& fs)
{
    _foreign_schema = fs;
}

Explicit_item_id::Explicit_item_id()
{
    _local_definition = 0;
}

Explicit_item_id::Explicit_item_id(const Explicit_item_id& eii)
: Interfaced_item(eii) 
{
    _local_definition = eii._local_definition;
    _original_id = eii._original_id;
    _new_id = eii._new_id;
}

Explicit_item_id::~Explicit_item_id()
{
    _local_definition = 0;
}

Used_item::~Used_item()
{
}

Referenced_item::~Referenced_item()
{
}

Implicit_item_id::Implicit_item_id()
{
    _local_definition = 0;
}

Implicit_item_id::Implicit_item_id(Implicit_item_id& iii)
: Interfaced_item(iii) 
{
    _local_definition = iii._local_definition;
}

Implicit_item_id::~Implicit_item_id()
{
    _local_definition = 0;
}

EntityDescLinkNode::EntityDescLinkNode() 
{
    _entityDesc = 0;
}

EntityDescLinkNode::~EntityDescLinkNode() 
{
}

EntityDescriptorList::EntityDescriptorList()
{
}

EntityDescriptorList::~EntityDescriptorList()
{
}

EntityDescItr::EntityDescItr(const EntityDescriptorList &edList) : edl(edList)
{
    cur = (EntityDescLinkNode *)( edl.GetHead() );
}
EntityDescItr::~EntityDescItr()
{
}

AttrDescLinkNode::AttrDescLinkNode()
{
    _attrDesc = 0; 
}

AttrDescLinkNode::~AttrDescLinkNode()
{
}

AttrDescItr::AttrDescItr(const AttrDescriptorList &adList) : adl(adList)
{
    cur = (AttrDescLinkNode *)( adl.GetHead() );
}

AttrDescItr::~AttrDescItr()
{
}

Inverse_attributeLinkNode::Inverse_attributeLinkNode()
{
    _invAttr = 0;
}

Inverse_attributeLinkNode::~Inverse_attributeLinkNode()
{
}

Inverse_attributeList::Inverse_attributeList()
{
}

Inverse_attributeList::~Inverse_attributeList()
{
}

Inverse_attributeLinkNode * 
Inverse_attributeList::AddNode (Inverse_attribute * ad)
{
    Inverse_attributeLinkNode *node = (Inverse_attributeLinkNode *) NewNode();
    node->Inverse_attr(ad);
    SingleLinkList::AppendNode(node);
    return node;
}    

InverseAItr::InverseAItr (const Inverse_attributeList &iaList) 
	: ial(iaList)
{
    cur = (Inverse_attributeLinkNode *)( ial.GetHead() );
}

InverseAItr::~InverseAItr()
{
}

TypeDescLinkNode::TypeDescLinkNode()
{
    _typeDesc = 0; 
}

TypeDescLinkNode::~TypeDescLinkNode()
{
}

TypeDescriptorList::TypeDescriptorList()
{
}

TypeDescriptorList::~TypeDescriptorList()
{
}

TypeDescItr::TypeDescItr (const TypeDescriptorList &tdList) : tdl(tdList)
{
    cur = (TypeDescLinkNode *)( tdl.GetHead() );
}

TypeDescItr::~TypeDescItr()
{
}

///////////////////////////////////////////////////////////////////////////////
// TypeDescriptor functions
///////////////////////////////////////////////////////////////////////////////

AttrDescriptorList::AttrDescriptorList()
{
}

AttrDescriptorList::~AttrDescriptorList() 
{
}

AttrDescLinkNode * 
AttrDescriptorList::AddNode (AttrDescriptor * ad)
{
    AttrDescLinkNode *node = (AttrDescLinkNode *) NewNode();
    node->AttrDesc(ad);
    SingleLinkList::AppendNode(node);
    return node;
}    

TypeDescriptor::TypeDescriptor( ) 
: _name (0), altNames(0), _fundamentalType(UNKNOWN_TYPE), 
  _originatingSchema(0), _referentType (0), _description (0), _where_rules(0)
{
}

TypeDescriptor::TypeDescriptor
(const char * nm, PrimitiveType ft, Schema *origSchema, 
 const char * d)
:  _name (nm), altNames (0), _fundamentalType (ft), 
   _originatingSchema(origSchema), _referentType (0), _description (d), 
   _where_rules(0)

{
}

const char *
TypeDescriptor::Name( const char *schnm ) const
    /*
     * Determines the current name of this.  Normally, this is simply _name.
     * If "schnm" is set to a value, however, then this function becomes a
     * request for our name when referenced by schnm.  (This will be diff from
     * our original name if schnm USEs or REFERENCEs us and renames us (e.g.,
     * "USE (xx as yy)").)  In such a case, this function searches through our
     * altNames list to see if schema schnm refers to us with a different name
     * and returns the new name if found.  (See header comments to function
     * SchRename::rename().)
     */
{
    if ( !schnm ) {
	return _name;
    }
    if ( altNames && altNames->rename( schnm, (char *)_altname ) ) {
	// If our altNames list has an alternate for schnm, copy it into
	// _altname, and return it:
	return _altname;
    }
    return _name;
}

const char * 
TypeDescriptor::BaseTypeName ()  const
{
  return BaseTypeDescriptor () ?  BaseTypeDescriptor () -> Name () : 0;
}

const TypeDescriptor * 
TypeDescriptor::BaseTypeIsA (const TypeDescriptor * td) const
{
  switch (NonRefType ()) {
  case AGGREGATE_TYPE:
    return AggrElemTypeDescriptor () -> IsA (td);
  case ENTITY_TYPE:
  case SELECT_TYPE: 
  default:
    return IsA (td);
  }

}

int
TypeDescriptor::CurrName(const char *other, const char *schNm) const
    /*
     * Check if our "current" name = other.  CurrName may be different from
     * _name if schNm tells us the current schema is a different one from the
     * one in which we're defined.  If so, we may have an alternate name for
     * that schema (it may be listed in our altNames list).  This would be the
     * case if schNm USEs or REFERENCEs type and renames it in the process
     * (e.g., "USE (X as Y)".
     */
{
    if ( !schNm || *schNm == '\0' ) {
	// If there's no current schema, accept any possible name of this.
	// (I.e., accept its actual name or any substitute):
	return ( PossName( other ) );
    }
    if ( altNames && altNames->rename( schNm, (char *)_altname ) ) {
	// If we have a different name when the current schema = schNm, then
	// other better = the alt name.
	return ( !StrCmpIns( _altname, other ) );
    } else {
	// If we have no desginated alternate name when the current schema =
	// schNm, other must = our _name.
	return ( OurName( other ) );
    }
}

int
TypeDescriptor::PossName( const char *nm ) const
    /*
     * return TRUE if nm is either our name or one of the possible alternates.
     */
{
    return ( OurName( nm ) || AltName( nm ) );
}

int
TypeDescriptor::OurName( const char *nm ) const
{
    return !StrCmpIns( nm, _name );
}

int
TypeDescriptor::AltName( const char *nm ) const
{
    if ( altNames ) {
	return ( altNames->choice( nm ) );
    }
    return 0;
}

void
TypeDescriptor::addAltName( const char *schnm, const char *newnm )
    /*
     * Creates a SchRename consisting of schnm & newnm.  Places it in alphabe-
     * tical order in this's altNames list.
     */
{
    SchRename *newpair = new SchRename( schnm, newnm ),
              *node = (SchRename *)altNames, *prev = NULL;

    while ( node && *node < *newpair ) {
	prev = node;
	node = node->next;
    }
    newpair->next = node; // node may = NULL
    if ( prev ) {
	// Will be the case if new node should not be first (and above while
	// loop was entered).
	prev->next = newpair;
    } else {
	// put newpair first
	altNames = newpair;
    }
}

int
SchRename::choice( const char *nm ) const
    /*
     * See if nm = one of our choices (either ours or that of a SchRename
     * later in the list.
     */
{
    if ( !StrCmpIns( nm, newName ) ) {
	return 1;
    }
    if ( next ) {
	return ( next->choice( nm ) );
    }
    return 0;
}

char *
SchRename::rename( const char *schnm, char *newnm ) const
    /*
     * Check if this SchRename represents the rename of its owning TypeDesc for
     * schema schnm.  (This will be the case if schnm = schName.)  If so, the
     * new name is returned and copied into newnm.  If not, ::rename is called
     * on next.  Thus, this function will tell us if this or any later SchRe-
     * name in this list provide a new name for TypeDesc for schema schnm.
     */
{
    if ( !StrCmpIns( schnm, schName ) ) {
	strcpy( newnm, newName );
	return newnm;
    }
    if ( next ) {
	return ( next->rename( schnm, newnm ) );
    }
    return NULL;
}

///////////////////////////////////////////////////////////////////////////////
// AttrDescriptor functions
///////////////////////////////////////////////////////////////////////////////

AttrDescriptor::AttrDescriptor(
		       const char * name,	   // i.e. char *
		       const TypeDescriptor *domainType, 
		       Logical optional,   // i.e. F U or T
		       Logical unique,	   // i.e. F U or T
		       AttrType_Enum at, // AttrType_Explicit,AttrType_Inverse,
				       // AttrType_Deriving,AttrType_Redefining
		       const EntityDescriptor & owner )
      : _name (name), _domainType (domainType), _optional(optional),
	_unique(unique), _attrType (at), 

#ifdef __O3DB__
_owner (&owner)
#else
_owner ((EntityDescriptor&)owner)
#endif

{
}

AttrDescriptor::~AttrDescriptor () 
{ }

Logical
AttrDescriptor::Explicit() const 
{ 
    if(_attrType == AttrType_Explicit)
      return LTrue;
    return LFalse;
}

Logical
AttrDescriptor::Inverse() const 
{ 
    if(_attrType == AttrType_Inverse)
      return LTrue;
    return LFalse;
}

Logical
AttrDescriptor::Redefining() const 
{ 
    if(_attrType == AttrType_Redefining)
      return LTrue;
    return LFalse;
}

Logical
AttrDescriptor::Deriving() const 
{ 
    if(_attrType == AttrType_Deriving)
      return LTrue;
    return LFalse;
}

// outdated function
void 
AttrDescriptor::Derived (SCLP23(LOGICAL) x)	
{ 
    if(x.asInt())
      _attrType = AttrType_Deriving;
    else
      _attrType = AttrType_Explicit;
}

// outdated function
void 
AttrDescriptor::Derived (Logical x)
{ 
    if(x == LTrue)
      _attrType = AttrType_Deriving;
    else
      _attrType = AttrType_Explicit;
}

// outdated function
void 
AttrDescriptor::Derived (const char *x)	
{ 
    if( !strcmp(x, "LTrue") || !strcmp(x, "T") )
      _attrType = AttrType_Deriving;
    else
      _attrType = AttrType_Explicit;
}


Derived_attribute::Derived_attribute(
		       const char * name,	   // i.e. char *
		       const TypeDescriptor *domainType, 
		       Logical optional,   // i.e. F U or T
		       Logical unique,	   // i.e. F U or T
		       AttrType_Enum at, // AttrType_Explicit,AttrType_Inverse,
				       // AttrType_Deriving,AttrType_Redefining
		       const EntityDescriptor & owner )
: AttrDescriptor(name, domainType, optional, unique, at, owner) 
{
    _initializer = (const char *)0;
}

Derived_attribute::~Derived_attribute()
{
}
