#ifndef TYPE_H
#define TYPE_H

/** **********************************************************************
** Module:  Type \file type.h
** Description: This module implements the type abstraction.  It is
**  rather unpleasant, since this abstraction is quite well suited
**  to an object-oriented environment with inheritance.
**
************************************************************************/

/*
 * This software was developed by U.S. Government employees as part of
 * their official duties and is not subject to copyright.
 *
 * $Log: type.h,v $
 * Revision 1.14  1997/09/19 18:25:10  dar
 * small fix - removed multiple include
 *
 * Revision 1.13  1997/01/21 19:17:11  dar
 * made C++ compatible
 *
 * Revision 1.12  1994/11/10  19:20:03  clark
 * Update to IS
 *
 * Revision 1.11  1994/05/11  19:51:05  libes
 * numerous fixes
 *
 * Revision 1.10  1993/10/15  18:48:24  libes
 * CADDETC certified
 *
 * Revision 1.8  1993/03/19  20:43:45  libes
 * add is_enum macro
 *
 * Revision 1.7  1993/01/21  19:48:25  libes
 * fix bug in TYPEget_base_type
 *
 * Revision 1.6  1993/01/19  22:16:09  libes
 * *** empty log message ***
 *
 * Revision 1.5  1992/09/16  18:23:45  libes
 * added some functions for easier access to base types
 *
 * Revision 1.4  1992/08/18  17:12:41  libes
 * rm'd extraneous error messages
 *
 * Revision 1.3  1992/06/08  18:06:24  libes
 * prettied up interface to print_objects_when_running
 */

/*************/
/* constants */
/*************/

#define TYPE_NULL       (Type)0

/** since we can't evaluate true size of many aggregates,
 * prepare to grow them by chunks of this size */
#define AGGR_CHUNK_SIZE 30

/** these are all orthogonal types */
enum type_enum {
    unknown_ = 0,   /**< 0 catches uninit. errors */
    special_,   /**< placeholder, given meaning by it's owner, such as Type_Dont_Care, Type_Bad, Type_User_Def */
    runtime_,   /**< cannot be further determined until runtime */
    integer_,
    real_,
    string_,
    binary_,
    boolean_,
    logical_,

    /* formals-only */
    number_,
    generic_,

    /* aggregates */
    aggregate_, /**< as a formal */
    array_,
    bag_,
    set_,
    list_,
    last_aggregate_,/**< not real, just for easier computation */

    oneof_,

    /** while they are really used for different purposes, it might be worth considering collapsing entity_ and entity_list_ */
    entity_,    /**< single entity */
    entity_list_,   /**< linked list of entities */
    enumeration_,
    select_,
    reference_, /**< something only defined by a base type, i.e., a type reference */
    query_,
    op_,        /**< something with an operand */
    inverse_,   /**< is? an inverse */

    identifier_,    /**< simple identifier in an expression */
    attribute_, /**< attribute reference (i.e., expr->u.variable) */
    derived_,   /**< ?*/
    funcall_,   /**< a function call and actual parameters */

    self_
};

/*****************/
/* packages used */
/*****************/

#include <sc_export.h>
#include "expbasic.h"   /* get basic definitions */
#include "symbol.h"
#include "object.h"

/************/
/* typedefs */
/************/

typedef struct TypeHead_ *TypeHead;
typedef struct TypeBody_ *TypeBody;
typedef enum type_enum  TypeType;

/* provide a replacement for Class */
typedef enum type_enum  Class_Of_Type;
typedef enum type_enum  Class;
#define OBJget_class(typ)   ((typ)->u.type->body->type)
#define CLASSinherits_from  TYPEinherits_from

/****************/
/* modules used */
/****************/

#include "dict.h"
#include "entity.h"
#include "expr.h"
#include "scope.h"

/***************************/
/* hidden type definitions */
/***************************/

struct TypeHead_ {
    Type head;          /**< if we are a defined type this is who we point to */
    struct TypeBody_ *body;     /**< true type, ignoring defined types */
#if 0
    /* if we are concerned about memory (over time) uncomment this and */
    /* other references to refcount in parser and TYPEresolve.  It is */
    /* explained in TYPEresolve. */
    int refcount;
#endif
};

struct TypeBody_ {
#if 1
    struct TypeHead_ *head;     /**< for debugging only */
#endif
    enum type_enum type;        /**< bits describing this type, int, real, etc */
    struct {
        unsigned unique     : 1;
        unsigned optional   : 1;
        unsigned fixed      : 1;
        unsigned shared     : 1; /**< type is shared */
        unsigned repeat     : 1; /**< Expression is the number of repetitions of the previous Expression
                                   * 10303-11:2004 production #203
                                   *   element = expression [ ':' repetition ] .
                                   * TODO exp2cxx and exp2py do not use this! Are all use cases handled by libexppp?
                                   */
        unsigned var        : 1; /** denotes variable marked 'VAR' - i.e. one where changes are propagated back to the caller */
        unsigned encoded    : 1; /**< encoded string */
    } flags;
    Type base;      /**< underlying base type if any can also contain true type if this type is a type reference */
    Type tag;       /**< optional tag */
    /* a lot of the stuff below can be unionized */
    Expression precision;
    Linked_List list;   /**< used by select_types and composed types, such as for a list of entities in an instance */
    Expression upper;
    Expression lower;
    struct Scope_ *entity;      /**< only used by entity types */
};

/********************/
/* global variables */
/********************/

/* Very commonly-used read-only types */
/* non-constant versions probably aren't necessary? */
extern SC_EXPRESS_EXPORT Type Type_Bad;
extern SC_EXPRESS_EXPORT Type Type_Unknown;
extern SC_EXPRESS_EXPORT Type Type_Dont_Care;
extern SC_EXPRESS_EXPORT Type Type_Runtime;   /**< indicates that this object can't be
                                                    calculated now but must be deferred
                                                    until (the mythical) runtime */
extern SC_EXPRESS_EXPORT Type Type_Binary;
extern SC_EXPRESS_EXPORT Type Type_Boolean;
extern SC_EXPRESS_EXPORT Type Type_Enumeration;
extern SC_EXPRESS_EXPORT Type Type_Expression;
extern SC_EXPRESS_EXPORT Type Type_Aggregate;
extern SC_EXPRESS_EXPORT Type Type_Repeat;
extern SC_EXPRESS_EXPORT Type Type_Integer;
extern SC_EXPRESS_EXPORT Type Type_Number;
extern SC_EXPRESS_EXPORT Type Type_Real;
extern SC_EXPRESS_EXPORT Type Type_String;
extern SC_EXPRESS_EXPORT Type Type_String_Encoded;
extern SC_EXPRESS_EXPORT Type Type_Logical;
extern SC_EXPRESS_EXPORT Type Type_Set;
extern SC_EXPRESS_EXPORT Type Type_Attribute;
extern SC_EXPRESS_EXPORT Type Type_Entity;
extern SC_EXPRESS_EXPORT Type Type_Funcall;
extern SC_EXPRESS_EXPORT Type Type_Generic;
extern SC_EXPRESS_EXPORT Type Type_Identifier;
extern SC_EXPRESS_EXPORT Type Type_Oneof;
extern SC_EXPRESS_EXPORT Type Type_Query;
extern SC_EXPRESS_EXPORT Type Type_Self;
extern SC_EXPRESS_EXPORT Type Type_Set_Of_String;
extern SC_EXPRESS_EXPORT Type Type_Set_Of_Generic;
extern SC_EXPRESS_EXPORT Type Type_Bag_Of_Generic;

extern SC_EXPRESS_EXPORT struct freelist_head TYPEHEAD_fl;
extern SC_EXPRESS_EXPORT struct freelist_head TYPEBODY_fl;

/******************************/
/* macro function definitions */
/******************************/

#define TYPEHEAD_new()      (struct TypeHead_ *)ALLOC_new(&TYPEHEAD_fl)
#define TYPEHEAD_destroy(x) ALLOC_destroy(&TYPEHEAD_fl,(Freelist *)x)
#define TYPEBODY_new()      (struct TypeBody_ *)ALLOC_new(&TYPEBODY_fl)
#define TYPEBODY_destroy(x) ALLOC_destroy(&TYPEBODY_fl,(Freelist *)x)

#define TYPEis(t)       ((t)->u.type->body->type)
#define TYPEis_identifier(t)    ((t)->u.type->body->type == identifier_)
#define TYPEis_logical(t)   ((t)->u.type->body->type == logical_)
#define TYPEis_boolean(t)   ((t)->u.type->body->type == boolean_)
#define TYPEis_real(t)   ((t)->u.type->body->type == real_)
#define TYPEis_integer(t)   ((t)->u.type->body->type == integer_)
#define TYPEis_string(t)    ((t)->u.type->body->type == string_)
#define TYPEis_expression(t)    ((t)->u.type->body->type == op_)
#define TYPEis_oneof(t)     ((t)->u.type->body->type == oneof_)
#define TYPEis_entity(t)    ((t)->u.type->body->type == entity_)
#define TYPEis_enumeration(t)   ((t)->u.type->body->type == enumeration_)
#define TYPEis_aggregate(t) ((t)->u.type->body->base)
#define TYPEis_aggregate_raw(t) ((t)->u.type->body->type == aggregate_)
#define TYPEis_array(t)     ((t)->u.type->body->type == array_)
#define TYPEis_select(t)    ((t)->u.type->body->type == select_)
#define TYPEis_reference(t) ((t)->u.type->body->type == reference_)
#define TYPEis_unknown(t)   ((t)->u.type->body->type == unknown_)
#define TYPEis_runtime(t)   ((t)->u.type->body->type == runtime_)
#define TYPEis_shared(t)    ((t)->u.type->body->flags.shared)
#define TYPEis_optional(t)  ((t)->u.type->body->flags.optional)
#define TYPEis_encoded(t)   ((t)->u.type->body->flags.encoded)

#define TYPEget_symbol(t)   (&(t)->symbol)

#define TYPEget_head(t)     ((t)->u.type->head)
#define TYPEput_head(t,h)   ((t)->u.type->head = (h))

#define TYPEget_body(t)     ((t)->u.type->body)
#define TYPEput_body(t,h)   ((t)->u.type->body = (h))

#define TYPEget_type(t)     ((t)->u.type->body->type)
#define TYPEget_base_type(t)    ((t)->u.type->body->base)

#define TYPEput_name(type,n)    ((type)->symbol.name = (n))
#define TYPEget_name(type)  ((type)->symbol.name)

#define TYPEget_where(t)    ((t)->where)
#define TYPEput_where(t,w)  (((t)->where) = w)

#define ENT_TYPEget_entity(t)       ((t)->u.type->body->entity)
#define ENT_TYPEput_entity(t,ent)   ((t)->u.type->body->entity = ent)

#define COMP_TYPEget_items(t)       ((t)->u.type->body->list)
#define COMP_TYPEput_items(t,lis)   ((t)->u.type->body->list = (lis))
#define COMP_TYPEadd_items(t,lis)   LISTadd_all((t)->u.type->body->list, (lis));

#define ENUM_TYPEget_items(t)       ((t)->symbol_table)
#define TYPEget_optional(t)     ((t)->u.type->body->flags.optional)
#define TYPEget_unique(t)       ((t)->u.type->body->flags.unique)

#define SEL_TYPEput_items(type,list)    COMP_TYPEput_items(type, list)
#define SEL_TYPEget_items(type)     COMP_TYPEget_items(type)

#define AGGR_TYPEget_upper_limit(t) ((t)->u.type->body->upper?(t)->u.type->body->upper:LITERAL_INFINITY)
#define AGGR_TYPEget_lower_limit(t) ((t)->u.type->body->lower?(t)->u.type->body->lower:LITERAL_ZERO)

#define TYPEget_enum_tags(t)        ((t)->symbol_table)

#define TYPEget_clientData(t)       ((t)->clientData)
#define TYPEput_clientData(t,d)     ((t)->clientData = (d))

/***********************/
/* function prototypes */
/***********************/

extern SC_EXPRESS_EXPORT Type TYPEcreate_partial(struct Symbol_ *, Scope);

extern SC_EXPRESS_EXPORT Type TYPEcreate(enum type_enum);
extern SC_EXPRESS_EXPORT Type TYPEcreate_from_body_anonymously(TypeBody);
extern SC_EXPRESS_EXPORT Type TYPEcreate_name(struct Symbol_ *);
extern SC_EXPRESS_EXPORT Type TYPEcreate_nostab(struct Symbol_ *, Scope, char);
extern SC_EXPRESS_EXPORT TypeBody TYPEBODYcreate(enum type_enum);
extern SC_EXPRESS_EXPORT void TYPEinitialize(void);
extern SC_EXPRESS_EXPORT void TYPEcleanup(void);

extern SC_EXPRESS_EXPORT bool TYPEinherits_from(Type, enum type_enum);
extern SC_EXPRESS_EXPORT Type TYPEget_nonaggregate_base_type(Type);

extern SC_EXPRESS_EXPORT Type TYPEcreate_user_defined_type(Type, Scope, struct Symbol_ *);
extern SC_EXPRESS_EXPORT Type TYPEcreate_user_defined_tag(Type, Scope, struct Symbol_ *);

#endif    /*  TYPE_H  */
