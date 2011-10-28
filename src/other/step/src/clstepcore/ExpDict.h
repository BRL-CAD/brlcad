#ifndef EXPDICT_H
#define EXPDICT_H

/*
* NIST STEP Core Class Library
* clstepcore/ExpDict.h
* April, 1997
* K. C. Morris
* David Sauder

* Development of this software was funded by the United States Government,
* and is not subject to copyright.
*/

/* $Id: ExpDict.h,v 3.0.1.12 1998/02/17 19:19:15 sauderd DP3.1 $  */ 

#ifdef __O3DB__
#include <OpenOODB.h>
#endif

#include <sdai.h>
//class SCLP23(Application_instance);

typedef  SCLP23(Application_instance) * (* Creator) () ;
//class StringAggregate;

enum AttrType_Enum {
	AttrType_Explicit = 0,
	AttrType_Inverse,
	AttrType_Deriving,
	AttrType_Redefining
    };

#include <SingleLinkList.h>

#include <baseType.h>
//#include <typeDefs.h>
#include <dictdefs.h>
//#include <sdaiDefs.h> 
#include <Str.h>
#include <scl_char_str_list.h>

/*
**  I tried these variations on the TypeDescriptor to get them to be
*   initialized globally.  I couldn\'t do it.  They are now initialized
*   in the Registry constructor (in Registry.inline.cc

extern const TypeDescriptor t_INTEGER_TYPE;
extern const TypeDescriptor t_REAL_TYPE;
extern const TypeDescriptor t_NUMBER_TYPE;
extern const TypeDescriptor t_STRING_TYPE;
extern const TypeDescriptor t_BINARY_TYPE;
extern const TypeDescriptor t_BOOLEAN_TYPE;
extern const TypeDescriptor t_LOGICAL_TYPE;

#define t_INTEGER_TYPE &_t_INTEGER_TYPE
#define t_REAL_TYPE  &_t_REAL_TYPE
#define t_NUMBER_TYPE &_t_NUMBER_TYPE
#define t_STRING_TYPE &_t_STRING_TYPE
#define t_BINARY_TYPE &_t_BINARY_TYPE
#define t_BOOLEAN_TYPE &_t_BOOLEAN_TYPE
#define t_LOGICAL_TYPE &_t_LOGICAL_TYPE

extern const TypeDescriptor * const t_INTEGER_TYPE;
extern const TypeDescriptor * const t_REAL_TYPE;
extern const TypeDescriptor * const t_NUMBER_TYPE;
extern const TypeDescriptor * const t_STRING_TYPE;
extern const TypeDescriptor * const t_BINARY_TYPE;
extern const TypeDescriptor * const t_BOOLEAN_TYPE;
extern const TypeDescriptor * const t_LOGICAL_TYPE;
*/

/*
extern const TypeDescriptor *  t_INTEGER_TYPE;
extern const TypeDescriptor *  t_REAL_TYPE;
extern const TypeDescriptor *  t_NUMBER_TYPE;
extern const TypeDescriptor *  t_STRING_TYPE;
extern const TypeDescriptor *  t_BINARY_TYPE;
extern const TypeDescriptor *  t_BOOLEAN_TYPE;
extern const TypeDescriptor *  t_LOGICAL_TYPE;
*/

// defined and created in Registry.inline.cc
extern const TypeDescriptor *  t_sdaiINTEGER;
extern const TypeDescriptor *  t_sdaiREAL;
extern const TypeDescriptor *  t_sdaiNUMBER;
extern const TypeDescriptor *  t_sdaiSTRING;
extern const TypeDescriptor *  t_sdaiBINARY;
extern const TypeDescriptor *  t_sdaiBOOLEAN;
extern const TypeDescriptor *  t_sdaiLOGICAL;

///////////////////////////////////////////////////////////////////////////////
// Dictionary_instance
///////////////////////////////////////////////////////////////////////////////

class Dictionary_instance {

  protected:
    Dictionary_instance() {}
    Dictionary_instance(const Dictionary_instance&) {}

    virtual ~Dictionary_instance();
};

///////////////////////////////////////////////////////////////////////////////

class TypeDescLinkNode : public SingleLinkNode {
  private:
  protected:
    TypeDescriptor *_typeDesc;
  public:
    TypeDescLinkNode();
    virtual ~TypeDescLinkNode();

    const TypeDescriptor *TypeDesc() const { return _typeDesc; }
    void TypeDesc(TypeDescriptor *td) { _typeDesc = td; }
};

class TypeDescriptorList : public SingleLinkList {
  private:
  protected:
  public:
    TypeDescriptorList();
    virtual ~TypeDescriptorList();

    virtual SingleLinkNode * NewNode () { return new TypeDescLinkNode; }

    TypeDescLinkNode * AddNode (TypeDescriptor * td) {
	TypeDescLinkNode *node = (TypeDescLinkNode *) NewNode();
	node->TypeDesc(td);
	SingleLinkList::AppendNode(node);
	return node;
    }    
};

class TypeDescItr
{
  protected:
    const TypeDescriptorList &tdl;
    const TypeDescLinkNode *cur;

  public:
    TypeDescItr (const TypeDescriptorList &tdList);
    virtual ~TypeDescItr();

    void ResetItr() { cur = (TypeDescLinkNode *)( tdl.GetHead() ); }

    const TypeDescriptor * NextTypeDesc();
};

///////////////////////////////////////////////////////////////////////////////

class EntityDescLinkNode : public SingleLinkNode {

  private:
  protected:
    EntityDescriptor * _entityDesc;

  public:
    EntityDescLinkNode();
    virtual ~EntityDescLinkNode();

    EntityDescriptor *EntityDesc() const { return _entityDesc; }
    void EntityDesc(EntityDescriptor *ed) { _entityDesc = ed; }
};

class EntityDescriptorList : public SingleLinkList {

  private:
  protected:
  public:
    EntityDescriptorList();
    virtual ~EntityDescriptorList();

    virtual SingleLinkNode * NewNode () { return new EntityDescLinkNode; }

    EntityDescLinkNode * AddNode (EntityDescriptor * ed) { 
	EntityDescLinkNode *node = (EntityDescLinkNode *) NewNode();
	node->EntityDesc(ed);
	SingleLinkList::AppendNode(node);
	return node;
    }
};

typedef EntityDescriptorList * Entity__set_ptr;
typedef Entity__set_ptr Entity__set_var;

class EntityDescItr
{
  protected:
    const EntityDescriptorList &edl;
    const EntityDescLinkNode *cur;

  public:
    EntityDescItr(const EntityDescriptorList &edList);
    virtual ~EntityDescItr();

    void ResetItr() { cur = (EntityDescLinkNode *)( edl.GetHead() ); }

    const EntityDescriptor * NextEntityDesc();
};


///////////////////////////////////////////////////////////////////////////////


///////////////////////////////////////////////////////////////////////////////
// Interfaced_item
///////////////////////////////////////////////////////////////////////////////

class Interfaced_item : public Dictionary_instance {
  protected:
    Interfaced_item();
    Interfaced_item(const Interfaced_item&);
    Interfaced_item(const char *foreign_schema);
    virtual ~Interfaced_item();
  public:
   Express_id _foreign_schema;

    const Express_id foreign_schema_();
//  private:
    void foreign_schema_(const Express_id&);
};

///////////////////////////////////////////////////////////////////////////////
// Explicit_item_id
///////////////////////////////////////////////////////////////////////////////

class Explicit_item_id : public Interfaced_item{
  protected:
    Explicit_item_id();
    Explicit_item_id(const Explicit_item_id&);
    Explicit_item_id(const char *foreign_schema, TypeDescriptor *ld, 
		     const char *oi, const char *ni) 
      : Interfaced_item(foreign_schema), _local_definition(ld), _original_id(oi), _new_id(ni) {}
    virtual ~Explicit_item_id();
  public:
    // definition in the local schema. The TypeDescriptor (or subtype) is not
    // implemented quite right - the name in it is the original (foreign 
    // schema) name. The USE or REFERENCED renames are listed in 
    // TypeDescriptor's altNames member variable.
    // Warning: This is currently a null ptr for objects other than 
    // types and entities - that is - if this is a USEd FUNCTION or PROCEDURE
    // this ptr will be null.
    const TypeDescriptor * _local_definition;

    // name in originating schema - only exists if it has been renamed.
    Express_id _original_id; 

    Express_id _new_id; // original or renamed name via USE or REFERENCE (non-SDAI)

    const TypeDescriptor *local_definition_() const 
	{ return _local_definition; }

    const Express_id original_id_() const
	{ return _original_id; }

    // non-sdai, renamed name
    const Express_id new_id_() const
	{ return _new_id; }

    // return string "USE" or "REFERENCE"
    virtual const char * EXPRESS_type()=0;

//  private:
    void local_definition_(const TypeDescriptor *td)
	{ _local_definition = td; }
    void original_id_(const Express_id& ei)
	{ _original_id = ei; }

    // non-sdai
    void new_id_(const Express_id& ni)
	{ _new_id = ni; }
};

typedef Explicit_item_id * Explicit_item_id_ptr;

class Used_item : public Explicit_item_id {
  public:
    Used_item() {}
    Used_item(const char *foreign_schema, TypeDescriptor *ld, 
	      const char *oi, const char *ni) 
      : Explicit_item_id(foreign_schema, ld, oi, ni) {}
    virtual ~Used_item();

    const char * EXPRESS_type() { return "USE"; }
};

typedef Used_item * Used_item_ptr;

class Referenced_item : public Explicit_item_id {
  public:
    Referenced_item() {}
    Referenced_item(const char *foreign_schema, TypeDescriptor *ld, 
		    const char *oi, const char *ni) 
      : Explicit_item_id(foreign_schema, ld, oi, ni) {}
    virtual ~Referenced_item();

    const char * EXPRESS_type() { return "REFERENCE"; }
};

typedef Referenced_item * Referenced_item_ptr;

class Explicit_item_id__set {
public:
    Explicit_item_id__set(int = 16);
    ~Explicit_item_id__set();

    Explicit_item_id_ptr& operator[](int index);
    void Insert(Explicit_item_id_ptr, int index);
    void Append(Explicit_item_id_ptr);
    void Remove(int index);
    int Index(Explicit_item_id_ptr);

    int Count();
    void Clear();
private:
    void Check(int index);
private:
    Explicit_item_id_ptr* _buf;
    int _bufsize;
    int _count;
};

typedef Explicit_item_id__set* Explicit_item_id__set_ptr;
typedef Explicit_item_id__set_ptr Explicit_item_id__set_var;

///////////////////////////////////////////////////////////////////////////////
// Implicit_item_id
///////////////////////////////////////////////////////////////////////////////

class Implicit_item_id : public Interfaced_item {
  protected:
    Implicit_item_id();
    Implicit_item_id(Implicit_item_id&);
    virtual ~Implicit_item_id();
  public:
    const TypeDescriptor * _local_definition;

    const TypeDescriptor *local_definition_() const 
	{ return _local_definition; }

//  private:
    void local_definition_(const TypeDescriptor *td)
	{ _local_definition = td; }
};

typedef Implicit_item_id * Implicit_item_id_ptr;

///////////////////////////////////////////////////////////////////////////////
// Implicit_item_id__set
///////////////////////////////////////////////////////////////////////////////

class Implicit_item_id__set {
public:
    Implicit_item_id__set(int = 16);
    ~Implicit_item_id__set();

    Implicit_item_id_ptr& operator[](int index);
    void Insert(Implicit_item_id_ptr, int index);
    void Append(Implicit_item_id_ptr);
    void Remove(int index);
    int Index(Implicit_item_id_ptr);

    int Count();
    void Clear();
private:
    void Check(int index);
private:
    Implicit_item_id_ptr* _buf;
    int _bufsize;
    int _count;
};

typedef Implicit_item_id__set* Implicit_item_id__set_ptr;
typedef Implicit_item_id__set_ptr Implicit_item_id__set_var;

///////////////////////////////////////////////////////////////////////////////
// Interface_spec
///////////////////////////////////////////////////////////////////////////////

class Interface_spec : public Dictionary_instance {
  public:
    Express_id _current_schema_id; // schema containing the USE/REF stmt
	// set of objects from USE/REFERENCE stmt(s)
    Explicit_item_id__set_var _explicit_items; 
    Implicit_item_id__set_var _implicit_items; //not yet initialized for schema

    // non-SDAI, not useful for SDAI use of Interface_spec (it would need to 
    // be a list).
    // schema that defined the USE/REFd objects 
    Express_id _foreign_schema_id;

    // non-SDAI, not useful for SDAI use of Interface_spec (it would need to 
    // be a list of ints).
    // schema USEs or REFERENCEs all objects from foreign schema
    int _all_objects;

    Interface_spec();
    Interface_spec(Interface_spec &); // not tested
    Interface_spec(const char * cur_sch_id, const char * foreign_sch_id,
		   int all_objects=0);
    virtual ~Interface_spec();

    Express_id current_schema_id_() { return _current_schema_id; }
    Express_id foreign_schema_id_() { return _foreign_schema_id; }

    Explicit_item_id__set_var explicit_items_()
      { return _explicit_items; }

    // this is not yet initialized for the schema
    Implicit_item_id__set_var implicit_items_()
      { return _implicit_items; }

//  private:
    void current_schema_id_(const Express_id &ei) { _current_schema_id = ei; }
    void foreign_schema_id_(const Express_id &fi) { _foreign_schema_id = fi; }

    int all_objects_() { return _all_objects; }
    void all_objects_(int ao) { _all_objects = ao; }
};

typedef Interface_spec * Interface_spec_ptr;

class Interface_spec__set {
public:
    Interface_spec__set(int = 16);
    ~Interface_spec__set();

    Interface_spec_ptr& operator[](int index);
    void Insert(Interface_spec_ptr, int index);
    void Append(Interface_spec_ptr);
    void Remove(int index);
    int Index(Interface_spec_ptr);

    int Count();
    void Clear();
private:
    void Check(int index);
private:
    Interface_spec_ptr* _buf;
    int _bufsize;
    int _count;
};

typedef Interface_spec__set* Interface_spec__set_ptr;
typedef Interface_spec__set_ptr Interface_spec__set_var;


class Type_or_rule : public Dictionary_instance {
  public:
    Type_or_rule();
    Type_or_rule(const Type_or_rule&);
    virtual ~Type_or_rule();
};

typedef Type_or_rule* Type_or_rule_ptr;
typedef Type_or_rule_ptr Type_or_rule_var;

class Where_rule : public Dictionary_instance {
  public:
    Express_id _label;
    Type_or_rule_var _type_or_rule;

    // non-SDAI
    std::string _comment; // Comment contained in the EXPRESS.
			// Should be properly formatted to include (* *)
			// Will be written to EXPRESS as-is (w/out formatting)

    Where_rule();
    Where_rule(const Where_rule&);
    Where_rule(const char * label, Type_or_rule_var tor=0)
      : _label(label), _type_or_rule(tor) { }
    virtual ~Where_rule();

    Express_id label_() const { return _label; }
    const Type_or_rule_var parent_item() const { return _type_or_rule; }
    std::string comment_() const { return _comment; }

    void label_(const Express_id& ei) { _label = ei; }
    void parent_item(const Type_or_rule_var& tor) { _type_or_rule = tor; }
    void comment_(const char* c) { _comment = c; }
};

typedef Where_rule * Where_rule_ptr;

class Where_rule__list {
public:
    Where_rule__list(int = 16);
    ~Where_rule__list();

    Where_rule_ptr& operator[](int index);
    void Insert(Where_rule_ptr, int index);
    void Append(Where_rule_ptr);
    void Remove(int index);
    int Index(Where_rule_ptr);

    int Count();
    void Clear();
private:
    void Check(int index);
private:
    Where_rule_ptr* _buf;
    int _bufsize;
    int _count;
};

typedef Where_rule__list* Where_rule__list_ptr;
typedef Where_rule__list_ptr Where_rule__list_var;

class Global_rule : public Dictionary_instance {
  public:
    Express_id _name;
    Entity__set_var _entities; // not implemented
    Where_rule__list_var _where_rules;
    Schema_ptr _parent_schema;
    std::string _rule_text; // non-SDAI

    Global_rule();
    Global_rule(const char *n, Schema_ptr parent_sch, const char * rt);
    Global_rule(Global_rule&); // not fully implemented
    virtual ~Global_rule();

    Express_id name_() const { return _name; }
    const Entity__set_var entities_() const { return _entities; }
    const Where_rule__list_var where_rules_() const { return _where_rules; }
    const Schema_ptr parent_schema_() const { return _parent_schema; }
    const char * rule_text_() { return const_cast<char *>(_rule_text.c_str()); }

    void name_(Express_id& n) { _name = n; }
    void entities_(const Entity__set_var &e); // not implemented
    void where_rules_(const Where_rule__list_var &wrl); // not implemented
    void parent_schema_(const Schema_ptr &s) { _parent_schema = s; }
    void rule_text_(const char * rt) { _rule_text = rt; }

};

typedef Global_rule * Global_rule_ptr;

class Global_rule__set {
public:
    Global_rule__set(int = 16);
    ~Global_rule__set();

    Global_rule_ptr& operator[](int index);
    void Insert(Global_rule_ptr, int index);
    void Append(Global_rule_ptr);
    void Remove(int index);
    int Index(Global_rule_ptr);

    int Count();
    void Clear();
private:
    void Check(int index);
private:
    Global_rule_ptr* _buf;
    int _bufsize;
    int _count;
};

typedef Global_rule__set* Global_rule__set_ptr;
typedef Global_rule__set_ptr Global_rule__set_var;

class Uniqueness_rule : public Dictionary_instance {
  public:
    Express_id _label;
    const EntityDescriptor * _parent_entity;

    // non-SDAI
    std::string _comment; // Comment contained in the EXPRESS.
			// Should be properly formatted to include (* *)
			// Will be written to EXPRESS as-is (w/out formatting)

    Uniqueness_rule();
    Uniqueness_rule(const Uniqueness_rule&);
    Uniqueness_rule(const char * label, EntityDescriptor *pe=0)
      : _label(label), _parent_entity(pe) { }
    virtual ~Uniqueness_rule();

    Express_id label_() const { return _label; }
    const EntityDescriptor * parent_() const { return _parent_entity; }
    std::string &comment_() { return _comment; }

    void label_(const Express_id& ei) { _label = ei; }
    void parent_(const EntityDescriptor * pe) { _parent_entity = pe; }
    void comment_(const char* c) { _comment = c; }

};

typedef Uniqueness_rule * Uniqueness_rule_ptr;

class Uniqueness_rule__set {
public:
    Uniqueness_rule__set(int = 16);
    ~Uniqueness_rule__set();

    Uniqueness_rule_ptr& operator[](int index);
    void Insert(Uniqueness_rule_ptr, int index);
    void Append(Uniqueness_rule_ptr);
    void Remove(int index);
    int Index(Uniqueness_rule_ptr);

    int Count();
    void Clear();
private:
    void Check(int index);
private:
    Uniqueness_rule_ptr* _buf;
    int _bufsize;
    int _count;
};

typedef Uniqueness_rule__set* Uniqueness_rule__set_ptr;
typedef Uniqueness_rule__set_ptr Uniqueness_rule__set_var;

///////////////////////////////////////////////////////////////////////////////
// Schema (was SchemaDescriptor) - a class of this type is generated and 
// contains schema info.
///////////////////////////////////////////////////////////////////////////////


typedef  SCLP23(Model_contents_ptr) (* ModelContentsCreator) () ;

class Schema : public Dictionary_instance { 

  protected:
    const char *  _name ;
    EntityDescriptorList _entList; // list of entities in the schema 
    TypeDescriptorList _typeList; // list of types in the schema 
    Interface_spec _interface; // list of USE and REF interfaces  (SDAI)

    // non-SDAI lists
    Interface_spec__set_var _use_interface_list; // list of USE interfaces
    Interface_spec__set_var _ref_interface_list; // list of REFERENCE interfaces
    scl_char_str__list_var _function_list; // of EXPRESS functions
    scl_char_str__list_var _procedure_list; // of EXPRESS procedures

    Global_rule__set_var _global_rules;

  public:  
    ModelContentsCreator CreateNewModelContents;

    Schema (const char *schemaName );
    virtual ~Schema ();

    void AssignModelContentsCreator(ModelContentsCreator f = 0)
    {
	CreateNewModelContents = f;
    }

    const char * Name() const	{ return _name; }
    void Name (const char *  n)  { _name = n; }

    Interface_spec& interface_() { return _interface; }

    Interface_spec__set_var use_interface_list_() { return
    _use_interface_list; }

    Interface_spec__set_var ref_interface_list_()
    { return _ref_interface_list; }

    scl_char_str__list_var function_list_()
    { return _function_list; }

    void AddFunction(const char * f);

    Global_rule__set_var global_rules_() // const
    { return _global_rules; }

    void AddGlobal_rule(Global_rule_ptr gr);

    void global_rules_(Global_rule__set_var &grs); // not implemented

    scl_char_str__list_var procedure_list_()
    { return _procedure_list; }

    void AddProcedure(const char * p);

    EntityDescLinkNode * AddEntity (EntityDescriptor * ed)
    { return _entList.AddNode(ed); }

    TypeDescLinkNode * AddType (TypeDescriptor * td)
    { return _typeList.AddNode(td); }

    // the whole schema
    void GenerateExpress(ostream& out) const;

    // USE, REFERENCE definitions
    void GenerateUseRefExpress(ostream& out) const;

    // TYPE definitions
    void GenerateTypesExpress(ostream& out) const;

    // Entity definitions
    void GenerateEntitiesExpress(ostream& out) const;
};

typedef Schema SchemaDescriptor;

///////////////////////////////////////////////////////////////////////////////

class AttrDescLinkNode : public SingleLinkNode {
  private:
  protected:
    AttrDescriptor *_attrDesc;
  public:
    AttrDescLinkNode();
    virtual ~AttrDescLinkNode();

    const AttrDescriptor *AttrDesc() const { return _attrDesc; }
    void AttrDesc(AttrDescriptor *ad) { _attrDesc = ad; }
};

class AttrDescriptorList : public SingleLinkList {
  private:
  protected:
  public:
    AttrDescriptorList();
    virtual ~AttrDescriptorList();

    virtual SingleLinkNode * NewNode () { return new AttrDescLinkNode; }

    AttrDescLinkNode * AddNode (AttrDescriptor * ad);
};

class AttrDescItr
{
  protected:
    const AttrDescriptorList &adl;
    const AttrDescLinkNode *cur;

  public:
    AttrDescItr(const AttrDescriptorList &adList);
    virtual ~AttrDescItr();

    void ResetItr() { cur = (AttrDescLinkNode *)( adl.GetHead() ); }

    const AttrDescriptor * NextAttrDesc();
};

///////////////////////////////////////////////////////////////////////////////

class Inverse_attributeLinkNode : public  SingleLinkNode {
  private:
  protected:
    Inverse_attribute *_invAttr;
  public:
    Inverse_attributeLinkNode();
    virtual ~Inverse_attributeLinkNode();

    const Inverse_attribute *Inverse_attr() const { return _invAttr; }
    void Inverse_attr(Inverse_attribute *ia) { _invAttr = ia; }
};

class Inverse_attributeList : public  SingleLinkList {
  private:
  protected:
  public:
    Inverse_attributeList();
    virtual ~Inverse_attributeList();

    virtual SingleLinkNode * NewNode () { return new Inverse_attributeLinkNode; }
    Inverse_attributeLinkNode * AddNode (Inverse_attribute * ia);
};

class InverseAItr
{
  protected:
    const Inverse_attributeList &ial;
    const Inverse_attributeLinkNode *cur;

  public:
    InverseAItr (const Inverse_attributeList &iaList);
    virtual ~InverseAItr();

    void ResetItr() 
	{ cur = (Inverse_attributeLinkNode *)( ial.GetHead() ); }

    const Inverse_attribute * NextInverse_attribute();
};

///////////////////////////////////////////////////////////////////////////////
// AttrDescriptor
// An instance of this class will be generated for each attribute for
// an Entity.  They will be pointed to by the EntityTypeDescriptors.
///////////////////////////////////////////////////////////////////////////////

class AttrDescriptor { 

  protected:
	const char *  _name ;	// the attributes name
		// this defines the domain of the attribute
	const TypeDescriptor * _domainType ;
	SCLP23(LOGICAL) _optional;
	SCLP23(LOGICAL) _unique;
	AttrType_Enum _attrType; // former attribute _derived
	
#ifdef __O3DB__
	const EntityDescriptor * _owner ;  // the owning entityDescriptor
#else
	const EntityDescriptor & _owner ;  // the owning entityDescriptor
#endif
  public:  

	AttrDescriptor(
		       const char * name,		// i.e. char *
		       const TypeDescriptor *domainType, 
		       Logical optional,	// i.e. F U or T
		       Logical unique,	// i.e. F U or T
		       AttrType_Enum at,// AttrType_Explicit, AttrType_Inverse,
				       // AttrType_Deriving,AttrType_Redefining
		       const EntityDescriptor & owner 
		   );
	virtual ~AttrDescriptor ();

	const char * GenerateExpress (std::string &buf) const;

		// the attribute Express def
	virtual const char *AttrExprDefStr(std::string & s) const;

		// left side of attr def
	const char * Name()	const	   { return _name; }
	void     Name (const char *  n) { _name = n; }

	   // BaseType() is the underlying type of this attribute. 
	   // NonRefType() is the first non REFERENCE_TYPE type
	   // e.g. Given attributes of each of the following types 
	   // TYPE count = INTEGER;
	   // TYPE ref_count = count;
	   // TYPE count_set = SET OF ref_count;
	   //  BaseType() will return INTEGER_TYPE for an attr of each type.
	   //  BaseTypeDescriptor() returns the TypeDescriptor for Integer
	   //  NonRefType() will return INTEGER_TYPE for the first two. For an
	   //    attribute of type count_set NonRefType() would return 
	   //    AGGREGATE_TYPE
	   //  NonRefTypeDescriptor() returns the TypeDescriptor for Integer
	   //     for the first two and a TypeDescriptor for an
	   //     aggregate for the last.

	const PrimitiveType BaseType() const;
	const TypeDescriptor *BaseTypeDescriptor() const;

	   // the first PrimitiveType that is not REFERENCE_TYPE (the first 
	   // TypeDescriptor *_referentType that does not have REFERENCE_TYPE 
	   // for it's fundamentalType variable).  This would return the same 
	   // as BaseType() for fundamental types.  An aggregate type
	   // would return AGGREGATE_TYPE then you could find out the type of
	   // an element by calling AggrElemType().  Select types
	   // would work the same?

	const PrimitiveType NonRefType() const;
	const TypeDescriptor *NonRefTypeDescriptor() const;

	int   IsAggrType() const;
	const PrimitiveType	AggrElemType() const;
	const TypeDescriptor *AggrElemTypeDescriptor() const;

		// The type of the attributes TypeDescriptor
	const PrimitiveType Type() const;
	const char * TypeName() const;	// right side of attr def

			// an expanded right side of attr def
	const char *ExpandedTypeName(std::string & s) const;

	int RefersToType() const	{ return !(_domainType == 0); }

	const TypeDescriptor * ReferentType() const { return _domainType; }
	const TypeDescriptor * DomainType() const { return _domainType; }
	void DomainType (const TypeDescriptor *td)  { _domainType = td; }
	void ReferentType(const TypeDescriptor *td) { _domainType = td; }

	const SCLP23(LOGICAL) & Optional() const { return _optional; }
	void Optional (SCLP23(LOGICAL) &opt)	{ _optional.put(opt.asInt()); }

	void Optional (Logical opt)	{ _optional.put(opt); }
	void Optional (const char *opt) { _optional.put(opt); }

	const SCLP23(LOGICAL) & Unique() const { return _unique; }
	void Unique (SCLP23(LOGICAL) uniq)	{ _unique.put(uniq.asInt()); }
	void Unique (Logical uniq)	{ _unique.put(uniq); }
	void Unique (const char *uniq)	{ _unique.put(uniq); }

	void AttrType(enum AttrType_Enum ate) { _attrType = ate; }
	enum AttrType_Enum AttrType() const { return _attrType; }

	Logical Explicit() const;
	Logical Inverse() const;
	Logical Redefining() const;
	Logical Deriving() const;

	//outdated functions, use AttrType func above, new support of redefined
	Logical Derived() const { return Deriving(); }
	void Derived (Logical x);     // outdated DAS
	void Derived (SCLP23(LOGICAL) x); // outdated DAS
	void Derived (const char *x); // outdated DAS

	const SCLP23(LOGICAL) & Optionality() const { return _optional; }
	void Optionality (SCLP23(LOGICAL) &opt) { _optional.put(opt.asInt()); }
	void Optionality (Logical opt)	   { _optional.put(opt); }
	void Optionality (const char *opt) { _optional.put(opt); }

	const SCLP23(LOGICAL) & Uniqueness() const	{ return _unique; }
	void Uniqueness (SCLP23(LOGICAL) uniq)	{ _unique.put(uniq.asInt()); }
	void Uniqueness (Logical uniq)		{ _unique.put(uniq); }
	void Uniqueness (const char *uniq)	{ _unique.put(uniq); }

#ifdef __O3DB__
	const EntityDescriptor  & Owner() const	{ return *_owner; }
#else
	const EntityDescriptor  & Owner() const	{ return _owner; }
#endif
};


///////////////////////////////////////////////////////////////////////////////
// Inverse_attribute
///////////////////////////////////////////////////////////////////////////////

class Derived_attribute  :    public AttrDescriptor  { 
  public:
    const char *_initializer;

    Derived_attribute(
		       const char * name,		// i.e. char *
		       const TypeDescriptor *domainType, 
		       Logical optional,	// i.e. F U or T
		       Logical unique,	// i.e. F U or T
		       AttrType_Enum at,// AttrType_Explicit, AttrType_Inverse,
				       // AttrType_Deriving,AttrType_Redefining
		       const EntityDescriptor & owner 
		   );
    virtual ~Derived_attribute();
    const char * AttrExprDefStr(std::string & s) const;

    const char *initializer_() { return _initializer; }
    void initializer_(const char *i) { _initializer = i; }
};

///////////////////////////////////////////////////////////////////////////////
// Inverse_attribute
///////////////////////////////////////////////////////////////////////////////

class Inverse_attribute  :    public AttrDescriptor  { 

  public:  
    const char *_inverted_attr_id;
    const char *_inverted_entity_id;
  protected: 
	AttrDescriptor * _inverted_attr ; // not implemented
  public:  

	Inverse_attribute(
		       const char * name,		// i.e. char *
		       TypeDescriptor *domainType, 
		       Logical optional,	// i.e. F U or T*/
		       Logical unique,	// i.e. F U or T
//		       AttrType_Enum at, // will always be AttrType_Inverse
		       const EntityDescriptor & owner, 
		       const char *inverted_attr_id =0
		   ) : AttrDescriptor( name, domainType, optional, unique, 
				       AttrType_Inverse, owner ), 
		       _inverted_attr_id(inverted_attr_id), 
		       _inverted_entity_id(0), _inverted_attr(0)
		{ }
	virtual ~Inverse_attribute () { }
	
	const char * AttrExprDefStr(std::string & s) const;

	const char * inverted_attr_id_() const
                { return _inverted_attr_id; } 
	
	void inverted_attr_id_(const char *iai ) 
                { _inverted_attr_id = iai; } 

	const char * inverted_entity_id_() const
                { return _inverted_entity_id; } 
	
	void inverted_entity_id_(const char *iei ) 
                { _inverted_entity_id = iei; } 

	// not implemented 
	class AttrDescriptor * inverted_attr_() 
                { return _inverted_attr; } 
	
	void inverted_attr_(AttrDescriptor *ia ) 
                { _inverted_attr = ia; } 

	  // below are obsolete (and not implemented anyway)
	class AttrDescriptor * InverseAttribute() 
                { return _inverted_attr; } 
        void InverseOf (AttrDescriptor * invAttr) 
                { _inverted_attr = invAttr; } 
};

///////////////////////////////////////////////////////////////////////////////
// SchRename is a structure which partially support the concept of USE and RE-
// FERENCE in EXPRESS.  Say schema A USEs object X from schema B and renames it
// to Y (i.e., "USE (X as Y);").  SchRename stores the name of the schema (B)
// plus the new object name for that schema (Y).  Each TypeDescriptor has a
// SchRename object (actually a linked list of SchRenames) corresponding to all
// the possible different names of itself depending on the current schema (the
// schema which is currently reading or writing this object).  (The current
// schema is determined by the file schema section of the header section of a
// part21 file (the _headerInstances of STEPfile).
///////////////////////////////////////////////////////////////////////////////

class SchRename {
  public:
    SchRename( const char *sch="\0", const char *newnm="\0" ) : next(0)
      { strcpy( schName, sch ); strcpy( newName, newnm ); }
    ~SchRename() { delete next; }
    const char *objName() const { return newName; }
    int operator<  ( SchRename &schrnm )
        { return ( strcmp( schName, schrnm.schName ) < 0 ); }
    int choice( const char *nm ) const;
            // is nm one of our possible choices?
    char *rename(const char *schm, char *newnm ) const;
            // given a schema name, returns new object name if exists
    SchRename *next;

  private:
    char schName[BUFSIZ];
    char newName[BUFSIZ];
};

///////////////////////////////////////////////////////////////////////////////
// TypeDescriptor
// This class and the classes inherited from this class are used to describe   
// all types (base types and created types).  There will be an instance of this
// class generated for each type found in the schema.  
// A TypeDescriptor will be generated in three contexts:
// 1) to describe a base type - e.g. INTEGER, REAL, STRING.  There is only one 
//	TypeDescriptor created for each Express base type. Each of these will 
//	be pointed to by several other AttrDescriptors and TypeDescriptors)
// 2) to describe a type created by an Express TYPE statement.
//	e.g. TYPE label = STRING END_TYPE;
//	These TypeDescriptors will be pointed to by other AttrDescriptors (and 
//	TypeDescriptors) representing attributes (and Express TYPEs) that are 
//	of the type created by this Express TYPE.
// 3) to describe a type created in an attribute definition
//	e.g. part_label_grouping : ARRAY [1.10] label;
//	or part_codes : ARRAY [1.10] INTEGER;
//	In this #3 context there will not be a name associated with the type.
//	The TypeDescriptor created in this case will only be pointed to by the
//	single AttrDescriptor associated with the attribute it was created for.
///////////////////////////////////////////////////////////////////////////////

///// _name is the name of the type. 
	// In the case of the TypeDescriptors representing the Express base
	// types this will be the name of the base type.
	// In the case where this TypeDescriptor is representing an Express
	// TYPE it is the LEFT side of an Express TYPE statement (i.e. label 
	// as in TYPE label = STRING END_TYPE;) This name would in turn be 
	// found on the RIGHT side of an Express attribute definition (e.g. 
	// attr defined as part_label : label; )
	// In the case where this TypeDescriptor was generated to describe a
	// type created in an attr definition, it will be a null pointer (e.g
	// attr defined as part_label_grouping : ARRAY [1..10] label)
///// _fundamentalType is the 'type' of the type being represented by 
	//  the TypeDescriptor . i.e. the following 2 stmts
	//  would cause 2 TypeDescriptors to be generated - the 1st having
	//  _fundamentalType set to STRING_TYPE and for the 2nd to 
	//  REFERENCE_TYPE.
	// TYPE label = STRING END_TYPE; 
	// TYPE part_label = label END_TYPE;
	// part_label and label would be the value of the respective
	// _name member variables for the 2 TypeDescriptors.  
///// _referentType will point at another TypeDescriptor furthur specifying 
	//  the type in all cases except when the type is directly 
	//  an enum or select.  i.e. in the following... _referentType for 
	//  the 1st type does not point at anything and for the 2nd it does:
	// TYPE color = ENUMERATION OF (red, blue); END_TYPE;
	// TYPE color_ref = color; END_TYPE;
////// _fundamentalType being REFERENCE_TYPE (as would be the case for 
	// part_label and color_ref above) means that the _referentType 
	// member variable points at a TypeDescriptor representing a type
	// that has been defined in an Express TYPE stmt.
	//  Otherwise _fundamental type reflects 
	//  the type directly as in the type label above.  type label above
	//  has a _referentType that points at a TypeDescriptor for STRING 
	//  described in the next sentence (also see #1 above).
	// A TypeDescriptor would be generated for each of the EXPRESS base 
	// types (int, string, real, etc) having _fundamentalType member 
	// variables set to match the EXPRESS base type being represented.
//////_referentType
	// For the TypeDescriptors describing the EXPRESS base types this will
	// be a null pointer.  For all other TypeDescriptors this will point 
	// to another TypeDescriptor which furthur describes the type. e.g.
	// TYPE part_label = label END_TYPE; TYPE label = STRING END_TYPE; 
	// part_label's _referentType will point to the TypeDescriptor for
	// label.  label's _referentType will point to the TypeDescriptor
	// for STRING. The _fundamentalType for part_label will be
	// REFERENCE_TYPE and for label will be STRING_TYPE.  
	// The _fundamentalType for the EXPRESS base type STRING's
	// TypeDescriptor will be STRING_TYPE.
	// The _referentType member variable will in most cases point to
	// a subtype of TypeDescriptor.
//////_description
	// This is the string description of the type as found in the
	// EXPRESS file. e.g. aggr of [aggr of ...] [list of ...] someType 
	// It is the RIGHT side of an Express TYPE statement 
	// (i.e. LIST OF STRING as in 
	// TYPE label_group = LIST OF STRING END_TYPE;) 
	// It is the same as _name for EXPRESS base types TypeDescriptors (with
	// the possible exception of upper or lower case differences).

class TypeDescriptor { 

  protected:

		// the name of the type (see above)
                //
                // NOTE - memory is not allocated for this, or for _description
                // below.  It is assumed that at creation, _name will be made
                // to point to a static location in memory.  The fedex_plus
                // generated code, for example, places a literal string in its
                // TypeDesc constructor calls.  This creates a location in me-
                // mory static throughout the lifetime of the calling program.
	const char *  _name ;

	        // an alternate name of type - such as one given by a different
	        // schema which USEs/ REFERENCEs this.  (A complete list of
	        // alternate names is stored in altNames below.  _altname pro-
	        // vides storage space for the currently used one.)
	char _altname[BUFSIZ];

	        // contains list of renamings of type - used by other schemas
	        // which USE/ REFERENCE this
	const SchRename * altNames;

		// the type of the type (see above). 
		// it is an enum see file clstepcore/baseType.h
//	BASE_TYPE _fundamentalType ;
	PrimitiveType _fundamentalType ;

	const Schema * _originatingSchema;

		// further describes the type (see above)
		// most often (or always) points at a subtype.
	const TypeDescriptor * _referentType ;

		// Express file description (see above)
		// e.g. the right side of an Express TYPE stmt
	        // (See note above by _name regarding memory allocation.)
	const char *  _description ;

      public:
	// a Where_rule may contain only a comment
	Where_rule__list_var _where_rules; // initially a null pointer

	Where_rule__list_var& where_rules_()
	{ return _where_rules; }

	void where_rules_(Where_rule__list * wrl)
	{ _where_rules = wrl; }

      protected:
	        // Functions used to check the current name of the type (may
	        // != _name if altNames has diff name for current schema).
	int PossName (const char *) const;
	int OurName  (const char *) const;
	int AltName  (const char *) const;
	
  public:  

	TypeDescriptor (const char * nm, PrimitiveType ft, const char * d); 
	TypeDescriptor (const char * nm, PrimitiveType ft, 
			Schema *origSchema, const char * d);
	TypeDescriptor ( ); 
	virtual ~TypeDescriptor () { /* if ( altNames ) delete altNames; */ }

	virtual const char * GenerateExpress (std::string &buf) const;

		// The name of this type.  If schnm != NULL, the name we're
	        // referred to by schema schnm (may be diff name in our alt-
	        // names list (based on schnm's USE/REF list)).
	const char * Name(const char *schnm = NULL) const;

		// The name that would be found on the right side of an 
		// attribute definition. In the case of a type defined like
		// TYPE name = STRING END_TYPE; 
		// with attribute definition   employee_name : name;
		// it would be the _name member variable. If it was a type
		// defined in an attribute it will be the _description
		// member variable since _name will be null. e.g. attr. def.
		// project_names : ARRAY [1..10] name;
	const char * AttrTypeName( std::string &buf, const char *schnm =NULL ) const;

	        // Linked link of alternate names for the type:
	const SchRename *AltNameList() const { return altNames; }

		// This is a fully expanded description of the type.
		// This returns a string like the _description member variable
		// except it is more thorough of a description where possible
		// e.g. if the description contains a TYPE name it will also
		// be explained.
	const char *TypeString(std::string & s) const;

		// This TypeDescriptor's type
	const PrimitiveType Type() const	{ return _fundamentalType; }
	void  Type(const PrimitiveType type) 	{ _fundamentalType = type; }

	   // This is the underlying Express base type of this type. It will 
	   // be the type of the last TypeDescriptor following the 
	   // _referentType member variable pointers. e.g.
	   // TYPE count = INTEGER;
	   // TYPE ref_count = count;
	   // TYPE count_set = SET OF ref_count;
	   //  each of the above will generate a TypeDescriptor and for 
	   //  each one, PrimitiveType BaseType() will return INTEGER_TYPE.
	   //  TypeDescriptor *BaseTypeDescriptor() returns the TypeDescriptor 
	   //  for Integer.
	const PrimitiveType       BaseType() const;
	const TypeDescriptor *BaseTypeDescriptor() const;
	const char * BaseTypeName () const;

	   // the first PrimitiveType that is not REFERENCE_TYPE (the first 
	   // TypeDescriptor *_referentType that does not have REFERENCE_TYPE 
	   // for it's fundamentalType variable).  This would return the same 
	   // as BaseType() for fundamental types.  An aggregate type
	   // would return AGGREGATE_TYPE then you could find out the type of
	   // an element by calling AggrElemType().  Select types
	   // would work the same?

	const PrimitiveType	NonRefType() const;
	const TypeDescriptor *NonRefTypeDescriptor() const;

	int   IsAggrType() const;
	const PrimitiveType	AggrElemType() const;
	const TypeDescriptor *AggrElemTypeDescriptor() const;

	const PrimitiveType FundamentalType() const { return _fundamentalType; }
	void FundamentalType (PrimitiveType ftype) { _fundamentalType = ftype; }

		// The TypeDescriptor for the type this type is based on 
	const TypeDescriptor * ReferentType() const { return _referentType; }
	void ReferentType (const TypeDescriptor * rtype) 
			{ _referentType = rtype; }


	const Schema * OriginatingSchema()  const
			{ return _originatingSchema; }
	void OriginatingSchema (const Schema * os)
			{ _originatingSchema = os; }
	const char *schemaName() const {
	    if ( _originatingSchema ) return _originatingSchema->Name();
	    else return "";
	}

		// A description of this type's type. Basically you
		// get the right side of a TYPE statement minus END_TYPE.
		// For base type TypeDescriptors it is the same as _name.
	const char * Description() const	{ return _description; }
	void Description (const char * desc) { _description = desc; } 

        virtual const TypeDescriptor * IsA (const TypeDescriptor *) const;
        virtual const TypeDescriptor * BaseTypeIsA (const TypeDescriptor *)
	  const;
        virtual const TypeDescriptor * IsA (const char *) const;
        virtual const TypeDescriptor * CanBe (const TypeDescriptor *n) const
                {  return TypeDescriptor::IsA (n);  }
	virtual const TypeDescriptor * CanBe (const char *n) const
		{  return TypeDescriptor::IsA (n); }
	virtual const TypeDescriptor * CanBeSet (const char *n,
						 const char *schNm =0) const
		{  return ( CurrName( n, schNm ) ? this : 0 ); }
	int CurrName( const char *, const char * =0 ) const;
	void addAltName( const char *schnm, const char *newnm );
	        // Adds an additional name, newnm, to be use when schema schnm
	        // is USE/REFERENCE'ing us (added to altNames).
};

typedef  SCLP23(Enum) * (* EnumCreator) () ;

class EnumTypeDescriptor  :    public TypeDescriptor  { 
  public:
    EnumCreator CreateNewEnum;

    const char * GenerateExpress (std::string &buf) const;

    void AssignEnumCreator(EnumCreator f = 0)
    {
	CreateNewEnum = f;
    }

    SCLP23(Enum) *CreateEnum();

    EnumTypeDescriptor ( ) { }
    EnumTypeDescriptor (const char * nm, PrimitiveType ft, 
			Schema *origSchema, const char * d, 
			EnumCreator f =0 ); 

    virtual ~EnumTypeDescriptor () { }
};


///////////////////////////////////////////////////////////////////////////////
// EntityDescriptor
// An instance of this class will be generated for each entity type
// found in the schema.  This should probably be derived from the
// CreatorEntry class (see sdaiApplicaton_instance.h).  Then the binary tree 
// that the current software  builds up containing the entities in the schema
// will be building the same thing but using the new schema info.
// nodes (i.e. EntityDesc nodes) for each entity.
///////////////////////////////////////////////////////////////////////////////

class EntityDescriptor  :    public TypeDescriptor  { 

  protected:
//	const Schema * _originatingSchema;
	SCLP23(LOGICAL) _abstractEntity;
	SCLP23(LOGICAL) _extMapping;
	  // does external mapping have to be used to create an instance of
	  // us (see STEP Part 21, sect 11.2.5.1)

	EntityDescriptorList _subtypes;   // OPTIONAL
	EntityDescriptorList _supertypes; // OPTIONAL

	AttrDescriptorList	  _explicitAttr; // OPTIONAL
//	StringAggregate		 * _derivedAttr;  // OPTIONAL
	Inverse_attributeList _inverseAttr;  // OPTIONAL

	std::string _supertype_stmt;
  public:
	Uniqueness_rule__set_var _uniqueness_rules; // initially a null pointer

       // pointer to a function that will create a new instance of a SCLP23(Application_instance)
        Creator NewSTEPentity;

	EntityDescriptor ( );
	EntityDescriptor (const char * name, // i.e. char *
			  Schema *origSchema,
			  Logical abstractEntity, // i.e. F U or T
			  Logical extMapping,
			  Creator f =0
			  );

	virtual ~EntityDescriptor ();

	const char * GenerateExpress (std::string &buf) const;

	const char * QualifiedName(std::string &s) const;

	const SCLP23(LOGICAL) & AbstractEntity() const 
					{ return _abstractEntity;}
	const SCLP23(LOGICAL) & ExtMapping() const   { return _extMapping; }
	void AbstractEntity (SCLP23(LOGICAL) &ae)
					{ _abstractEntity.put(ae.asInt()); }
	void ExtMapping (SCLP23(LOGICAL) &em)
				        { _extMapping.put(em.asInt());     }
	void AbstractEntity (Logical ae) { _abstractEntity.put(ae); }
	void ExtMapping     (Logical em) { _extMapping.put(em);     }
	void ExtMapping     (const char *em)    { _extMapping.put(em);     }

	const EntityDescriptorList& Subtypes() const
	  { return _subtypes; } 

	const EntityDescriptorList& Supertypes() const
	  { return _supertypes; }

	const EntityDescriptorList& GetSupertypes()  const 
	  { return _supertypes; }

	const AttrDescriptorList& ExplicitAttr() const
	  { return _explicitAttr; }

//	StringAggregate  & DerivedAttr()	{ return *_derivedAttr; }

	const Inverse_attributeList& InverseAttr() const
	        { return _inverseAttr; }

        virtual const EntityDescriptor * IsA (const EntityDescriptor *) const;
        virtual const TypeDescriptor * IsA (const TypeDescriptor * td) const;
        virtual const TypeDescriptor * IsA (const char * n) const
                {  return TypeDescriptor::IsA (n);  }
        virtual const TypeDescriptor * CanBe (const TypeDescriptor *o) const
                {  return o -> IsA (this);  }

	virtual const TypeDescriptor * CanBe (const char *n) const
		{  return TypeDescriptor::CanBe (n); }

    // The following will be used by schema initialization functions

	void AddSubtype(EntityDescriptor *ed) 
		{ _subtypes.AddNode(ed); }
	void AddSupertype_Stmt(const char *s) { _supertype_stmt = s; }
	const char * Supertype_Stmt() { return const_cast<char *>(_supertype_stmt.c_str()); }
	std::string& supertype_stmt_() { return _supertype_stmt; }

	void AddSupertype(EntityDescriptor *ed) 
		{ _supertypes.AddNode(ed); }

	void AddExplicitAttr(AttrDescriptor *ad)
		{ _explicitAttr.AddNode(ad); }

	void AddInverseAttr(Inverse_attribute *ia)
		{ _inverseAttr.AddNode(ia); }
	void uniqueness_rules_(Uniqueness_rule__set *urs)
	  { _uniqueness_rules = urs; }
	Uniqueness_rule__set_var& uniqueness_rules_()
	  { return _uniqueness_rules; }

};


///////////////////////////////////////////////////////////////////////////////
// EnumerationTypeDescriptor
///////////////////////////////////////////////////////////////////////////////
#ifdef NOT_YET
class EnumerationTypeDescriptor  :    public TypeDescriptor  { 

  protected:
	StringAggregate  *_elements ;	  //  of  (null)

  public:  
	EnumerationTypeDescriptor ( );
	virtual ~EnumerationTypeDescriptor () { } 


	StringAggregate & Elements() { return *_elements; }
//	void Elements (StringAggregate  e);
};
#endif

class STEPaggregate;
class EnumAggregate;
class GenericAggregate;
class EntityAggregate;
class SelectAggregate;
class StringAggregate;
class BinaryAggregate;
class RealAggregate;
class IntAggregate;

typedef  STEPaggregate * (* AggregateCreator) () ;
typedef  EnumAggregate * (* EnumAggregateCreator) () ;
typedef  GenericAggregate * (* GenericAggregateCreator) () ;
typedef  EntityAggregate * (* EntityAggregateCreator) () ;
typedef  SelectAggregate * (* SelectAggregateCreator) () ;
typedef  StringAggregate * (* StringAggregateCreator) () ;
typedef  BinaryAggregate * (* BinaryAggregateCreator) () ;
typedef  RealAggregate * (* RealAggregateCreator) () ;
typedef  IntAggregate * (* IntAggregateCreator) () ;

EnumAggregate * create_EnumAggregate();

GenericAggregate * create_GenericAggregate();

EntityAggregate * create_EntityAggregate();

SelectAggregate * create_SelectAggregate();

StringAggregate * create_StringAggregate();

BinaryAggregate * create_BinaryAggregate();

RealAggregate * create_RealAggregate();

IntAggregate * create_IntAggregate();

///////////////////////////////////////////////////////////////////////////////
// AggrTypeDescriptor
// I think we decided on a simplistic representation of aggr. types for now?
// i.e. just have one AggrTypeDesc for Array of [list of] [set of] someType
// the inherited variable _referentType will point to the TypeDesc for someType
// So I don't believe this class was necessary.  If we were to retain
// info for each of the [aggr of]'s in the example above then there would be
// one of these for each [aggr of] above and they would be strung
// together by the _aggrDomainType variables.  If you can make this
// work then go for it.
///////////////////////////////////////////////////////////////////////////////

class AggrTypeDescriptor  :    public TypeDescriptor  { 

  protected:

    SCLP23(Integer)  _bound1 ;
    SCLP23(Integer)  _bound2 ;
    SCLP23(LOGICAL) _uniqueElements ;
    TypeDescriptor * _aggrDomainType ;
    AggregateCreator CreateNewAggr;

  public:  

    void AssignAggrCreator(AggregateCreator f = 0)
    {
	CreateNewAggr = f;
    }

    STEPaggregate *CreateAggregate();

    AggrTypeDescriptor ( ); 
    AggrTypeDescriptor(SCLP23(Integer) b1, SCLP23(Integer) b2, 
		       Logical uniqElem, 
		       TypeDescriptor *aggrDomType);
    AggrTypeDescriptor (const char * nm, PrimitiveType ft, 
			Schema *origSchema, const char * d, 
			AggregateCreator f =0 )
	: TypeDescriptor (nm, ft, origSchema, d), CreateNewAggr(f) { }
    virtual ~AggrTypeDescriptor ();


    SCLP23(Integer) & Bound1()	        { return _bound1; }
    void Bound1 (SCLP23(Integer)  b1)   { _bound1 = b1; } 

    SCLP23(Integer) & Bound2() 		{ return _bound2; }
    void Bound2 (SCLP23(Integer)  b2)   { _bound2 = b2; } 

    SCLP23(LOGICAL)& UniqueElements()	{ return _uniqueElements; } 
    void UniqueElements (SCLP23(LOGICAL) &ue) 
					{ _uniqueElements.put(ue.asInt()); } 
    void UniqueElements (Logical ue)     { _uniqueElements.put(ue); }
    void UniqueElements (const char *ue) { _uniqueElements.put(ue); }

    class TypeDescriptor * AggrDomainType()    { return _aggrDomainType; } 
    void AggrDomainType (TypeDescriptor * adt) { _aggrDomainType = adt; }
};

///////////////////////////////////////////////////////////////////////////////
// ArrayTypeDescriptor
///////////////////////////////////////////////////////////////////////////////

class ArrayTypeDescriptor  :    public AggrTypeDescriptor  { 

  protected:
	SCLP23(LOGICAL) _optionalElements ;
  public:  

    ArrayTypeDescriptor ( ) : _optionalElements((char *)"UNKNOWN_TYPE") { } 
    ArrayTypeDescriptor (Logical optElem) : _optionalElements(optElem)
      { }
    ArrayTypeDescriptor (const char * nm, PrimitiveType ft, 
			 Schema *origSchema, const char * d, 
			 AggregateCreator f =0 )
	: AggrTypeDescriptor (nm, ft, origSchema, d, f),
	  _optionalElements((char *)"UNKNOWN_TYPE") 
    { }

    virtual ~ArrayTypeDescriptor () {}


    SCLP23(LOGICAL)& OptionalElements()       { return _optionalElements; } 
    void OptionalElements (SCLP23(LOGICAL) &oe) 
				     { _optionalElements.put(oe.asInt()); } 
    void OptionalElements (Logical oe)     { _optionalElements.put(oe); }
    void OptionalElements (const char *oe) { _optionalElements.put(oe); }
};

class ListTypeDescriptor  :    public AggrTypeDescriptor  { 

  protected:
  public:  

/*    void AssignAggrCreator(ListAggregateCreator f = 0)
    {
	CreateNewAggr = f;
    }

    STEPaggregate *CreateListAggregate()
    {
	if(CreateNewAggr)
	    return CreateNewAggr();
	else
	    return 0;
    }
    */
    ListTypeDescriptor ( ) { }
    ListTypeDescriptor (const char * nm, PrimitiveType ft, 
			Schema *origSchema, const char * d, 
			AggregateCreator f =0 )
		: AggrTypeDescriptor (nm, ft, origSchema, d, f) { }
    virtual ~ListTypeDescriptor () { }

};

class SetTypeDescriptor  :    public AggrTypeDescriptor  { 

  protected:
  public:  

    SetTypeDescriptor ( ) { } 
    SetTypeDescriptor (const char * nm, PrimitiveType ft, 
		       Schema *origSchema, const char * d, 
		       AggregateCreator f =0 )
		: AggrTypeDescriptor (nm, ft, origSchema, d, f) { }
    virtual ~SetTypeDescriptor () { }

};

class BagTypeDescriptor  :    public AggrTypeDescriptor  { 

  protected:
  public:  

    BagTypeDescriptor ( ) { }
    BagTypeDescriptor (const char * nm, PrimitiveType ft, 
		       Schema *origSchema, const char * d, 
		       AggregateCreator f =0 )
		: AggrTypeDescriptor (nm, ft, origSchema, d, f) { }
    virtual ~BagTypeDescriptor () { }

};

typedef  SCLP23(Select) * (* SelectCreator) () ;

class SelectTypeDescriptor  :    public TypeDescriptor  { 

  protected:
	TypeDescriptorList _elements ;	  //  of  TYPE_DESCRIPTOR
	int _unique_elements;

  public:  

	SelectCreator CreateNewSelect;

	void AssignSelectCreator(SelectCreator f = 0)
	{
	    CreateNewSelect = f;
	}

	SCLP23(Select) *CreateSelect();

        SelectTypeDescriptor (int b, const char * nm, PrimitiveType ft, 
			      Schema *origSchema, 
			      char * d, SelectCreator f =0 ) 
          : TypeDescriptor (nm, ft, origSchema, d), 
	  _unique_elements (b), CreateNewSelect(f)
		{ }
	virtual ~SelectTypeDescriptor () { }

	TypeDescriptorList& Elements() { return _elements; }
	const TypeDescriptorList& GetElements() const { return _elements; }
//	void Elements (TypeDescriptorList x);
        int UniqueElements () const {  return _unique_elements; }
        virtual const TypeDescriptor * IsA (const TypeDescriptor *) const;
        virtual const TypeDescriptor * IsA (const char * n) const  
                {  return TypeDescriptor::IsA (n);  }
        virtual const TypeDescriptor * CanBe (const TypeDescriptor *) const;
	virtual const TypeDescriptor * CanBe (const char *n) const;
	virtual const TypeDescriptor * CanBeSet (const char *, const char *)
	  const;
};

class StringTypeDescriptor  :    public TypeDescriptor  { 

  protected:
	SCLP23(Integer)  _width ;    //  OPTIONAL
	SCLP23(LOGICAL) _fixedSize ;
  public:  

	StringTypeDescriptor ( ) : _fixedSize((char *)"UNKNOWN_TYPE") { _width = 0; }
	virtual ~StringTypeDescriptor () { }


	SCLP23(Integer) Width()		{ return _width; }
	void Width (SCLP23(Integer)  w)	{ _width = w; }

	SCLP23(LOGICAL)& FixedSize()		{ return _fixedSize; }
	void FixedSize (SCLP23(LOGICAL) fs)	{ _fixedSize.put(fs.asInt()); }
	void FixedSize (Logical fs)	{ _fixedSize.put(fs); }
};

class RealTypeDescriptor  :    public TypeDescriptor  { 

  protected:
	SCLP23(Integer)  _precisionSpec ;    //  OPTIONAL
  public:  

	RealTypeDescriptor ( ) { _precisionSpec = 0; }
	virtual ~RealTypeDescriptor () { }

	SCLP23(Integer) PrecisionSpec()		 { return _precisionSpec; }
	void PrecisionSpec (SCLP23(Integer)  ps) { _precisionSpec = ps; }
};


#endif
