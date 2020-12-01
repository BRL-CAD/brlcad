#include "express/schema.h"
#include "express/type.h"
#include "express/expr.h"

#include "express/dict.h"

/* TODO: use enum? */
Type Type_Bad;
Type Type_Unknown;
Type Type_Dont_Care;
Type Type_Runtime;
Type Type_Binary;
Type Type_Boolean;
Type Type_Enumeration;
Type Type_Expression;
Type Type_Aggregate;
Type Type_Repeat;
Type Type_Integer;
Type Type_Number;
Type Type_Real;
Type Type_String;
Type Type_String_Encoded;
Type Type_Logical;
Type Type_Set;
Type Type_Attribute;
Type Type_Entity;
Type Type_Funcall;
Type Type_Generic;
Type Type_Identifier;
Type Type_Oneof;
Type Type_Query;
Type Type_Self;
Type Type_Set_Of_String;
Type Type_Set_Of_Generic;
Type Type_Bag_Of_Generic;

void FACTORYinitialize() {
    /* Very commonly-used read-only types */
    Type_Unknown = TYPEcreate( unknown_ );
    Type_Dont_Care = TYPEcreate( special_ );
    Type_Bad = TYPEcreate( special_ );
    Type_Runtime = TYPEcreate( runtime_ );

    Type_Enumeration = TYPEcreate( enumeration_ );
    Type_Enumeration->u.type->body->flags.shared = 1;
    resolved_all( Type_Enumeration );

    Type_Expression = TYPEcreate( op_ );
    Type_Expression->u.type->body->flags.shared = 1;

    Type_Aggregate = TYPEcreate( aggregate_ );
    Type_Aggregate->u.type->body->flags.shared = 1;
    Type_Aggregate->u.type->body->base = Type_Runtime;

    Type_Integer = TYPEcreate( integer_ );
    Type_Integer->u.type->body->flags.shared = 1;
    resolved_all( Type_Integer );

    Type_Real = TYPEcreate( real_ );
    Type_Real->u.type->body->flags.shared = 1;
    resolved_all( Type_Real );

    Type_Number = TYPEcreate( number_ );
    Type_Number->u.type->body->flags.shared = 1;
    resolved_all( Type_Number );

    Type_String = TYPEcreate( string_ );
    Type_String->u.type->body->flags.shared = 1;
    resolved_all( Type_String );

    Type_String_Encoded = TYPEcreate( string_ );
    Type_String_Encoded->u.type->body->flags.shared = 1;
    Type_String_Encoded->u.type->body->flags.encoded = 1;
    resolved_all( Type_String );

    Type_Logical = TYPEcreate( logical_ );
    Type_Logical->u.type->body->flags.shared = 1;
    resolved_all( Type_Logical );

    Type_Binary = TYPEcreate( binary_ );
    Type_Binary->u.type->body->flags.shared = 1;
    resolved_all( Type_Binary );

    Type_Number = TYPEcreate( number_ );
    Type_Number->u.type->body->flags.shared = 1;
    resolved_all( Type_Number );

    Type_Boolean = TYPEcreate( boolean_ );
    Type_Boolean->u.type->body->flags.shared = 1;
    resolved_all( Type_Boolean );

    Type_Generic = TYPEcreate( generic_ );
    Type_Generic->u.type->body->flags.shared = 1;
    resolved_all( Type_Generic );

    Type_Set_Of_String = TYPEcreate( set_ );
    Type_Set_Of_String->u.type->body->flags.shared = 1;
    Type_Set_Of_String->u.type->body->base = Type_String;

    Type_Set_Of_Generic = TYPEcreate( set_ );
    Type_Set_Of_Generic->u.type->body->flags.shared = 1;
    Type_Set_Of_Generic->u.type->body->base = Type_Generic;

    Type_Bag_Of_Generic = TYPEcreate( bag_ );
    Type_Bag_Of_Generic->u.type->body->flags.shared = 1;
    Type_Bag_Of_Generic->u.type->body->base = Type_Generic;

    Type_Attribute = TYPEcreate( attribute_ );
    Type_Attribute->u.type->body->flags.shared = 1;

    Type_Entity = TYPEcreate( entity_ );
    Type_Entity->u.type->body->flags.shared = 1;

    Type_Funcall = TYPEcreate( funcall_ );
    Type_Funcall->u.type->body->flags.shared = 1;

    Type_Generic = TYPEcreate( generic_ );
    Type_Generic->u.type->body->flags.shared = 1;

    Type_Identifier = TYPEcreate( identifier_ );
    Type_Identifier->u.type->body->flags.shared = 1;

    Type_Repeat = TYPEcreate( integer_ );
    Type_Repeat->u.type->body->flags.shared = 1;
    Type_Repeat->u.type->body->flags.repeat = 1;

    Type_Oneof = TYPEcreate( oneof_ );
    Type_Oneof->u.type->body->flags.shared = 1;

    Type_Query = TYPEcreate( query_ );
    Type_Query->u.type->body->flags.shared = 1;

    Type_Self = TYPEcreate( self_ );
    Type_Self->u.type->body->flags.shared = 1;
}

/** Create and return an empty scope inside a parent scope. */
Scope SCOPEcreate( char type ) {
    Scope d = SCOPE_new();
    d->symbol_table = DICTcreate( 50 );
    d->type = type;
    return d;
}

Scope SCOPEcreate_tiny( char type ) {
    Scope d = SCOPE_new();
    d->symbol_table = DICTcreate( 1 );
    d->type = type;
    return d;
}

void SCOPEdestroy( Scope scope ) {
    SCOPE_destroy( scope );
}

/**
 * create a scope without a symbol table
 * used for simple types
 */
Scope SCOPEcreate_nostab( char type ) {
    Scope d = SCOPE_new();
    d->type = type;
    return d;
}

/** Create and return a schema. */
Schema SCHEMAcreate( void ) {
    Scope s = SCOPEcreate( OBJ_SCHEMA );
    s->enum_table = DICTcreate(50);
    s->u.schema = SCHEMA_new();
    return s;
}

/**
 * create a type with no symbol table
 */
Type TYPEcreate_nostab( struct Symbol_ *symbol, Scope scope, char objtype ) {
    Type t = SCOPEcreate_nostab( OBJ_TYPE );
    TypeHead th = TYPEHEAD_new();

    t->u.type = th;
    t->symbol = *symbol;
    DICTdefine( scope->symbol_table, symbol->name, t, &t->symbol, objtype );

    return t;
}

/**
 * create a type but this is just a shell, either to be completed later
 * such as enumerations (which have a symbol table added later)
 * or to be used as a type reference
 */
Type TYPEcreate_name( Symbol * symbol ) {
    Scope s = SCOPEcreate_nostab( OBJ_TYPE );
    TypeHead t = TYPEHEAD_new();

    s->u.type = t;
    s->symbol = *symbol;
    return s;
}

Type TYPEcreate( enum type_enum type ) {
    TypeBody tb = TYPEBODYcreate( type );
    Type t = TYPEcreate_from_body_anonymously( tb );
    return( t );
}

Type TYPEcreate_from_body_anonymously( TypeBody tb ) {
    Type t = SCOPEcreate_nostab( OBJ_TYPE );
    TypeHead th = TYPEHEAD_new();

    t->u.type = th;
    t->u.type->body = tb;
    t->symbol.name = 0;
    SYMBOLset( t );
    return t;
}

TypeBody TYPEBODYcreate( enum type_enum type ) {
    TypeBody tb = TYPEBODY_new();
    tb->type = type;
    return tb;
}

Symbol * SYMBOLcreate( char * name, int line, const char * filename ) {
    Symbol * sym = SYMBOL_new();
    sym->name = name;
    sym->line = line;
    sym->filename = filename; /* NOTE this used the global 'current_filename',
                               * instead of 'filename'. This func is only
                               * called in two places, and both calls use
                               * 'current_filename'. Changed this to avoid
                               * potential future headaches. (MAP, Jan 12)
                               */
    return sym;
}

/**
** low-level function for type Entity
**
** \note The attribute list of a new entity is defined as an
**  empty list; all other aspects of the entity are initially
**  undefined (i.e., have appropriate NULL values).
*/
Entity ENTITYcreate( Symbol * sym ) {
    Scope s = SCOPEcreate( OBJ_ENTITY );

    s->u.entity = ENTITY_new();
    s->u.entity->attributes = LISTcreate();
    s->u.entity->inheritance = ENTITY_INHERITANCE_UNINITIALIZED;

    /* it's so useful to have a type hanging around for each entity */
    s->u.entity->type = TYPEcreate_name( sym );
    s->u.entity->type->u.type->body = TYPEBODYcreate( entity_ );
    s->u.entity->type->u.type->body->entity = s;
    return( s );
}

/**  VARcreate
** \param name name of variable to create
** \param type type of new variable
** \return the Variable created
** Create and return a new variable.
**
** \note The reference class of the variable is, by default,
**      dynamic.  Special flags associated with the variable
**      (e.g., optional) are initially false.
*/
Variable VARcreate( Expression name, Type type ) {
    Variable v = VAR_new();
    v->name = name;
    v->type = type;
    return v;
}

Expression EXPcreate( Type type ) {
    Expression e;
    e = EXP_new();
    SYMBOLset( e );
    e->type = type;
    e->return_type = Type_Unknown;
    return( e );
}

/**
 * use this when the return_type is the same as the type
 * For example, for constant integers
 */
Expression EXPcreate_simple( Type type ) {
    Expression e;
    e = EXP_new();
    SYMBOLset( e );
    e->type = e->return_type = type;
    return( e );
}

Expression EXPcreate_from_symbol( Type type, Symbol * symbol ) {
    Expression e;
    e = EXP_new();
    e->type = type;
    e->return_type = Type_Unknown;
    e->symbol = *symbol;
    return e;
}

/**
** \param op operation
** \param operand1 - first operand
** \param operand2 - second operand
** \param operand3 - third operand
** \returns Ternary_Expression  - the expression created
** Create a ternary operation Expression.
*/
Expression TERN_EXPcreate( Op_Code op, Expression operand1, Expression operand2, Expression operand3 ) {
    Expression e = EXPcreate( Type_Expression );

    e->e.op_code = op;
    e->e.op1 = operand1;
    e->e.op2 = operand2;
    e->e.op3 = operand3;
    return e;
}

/**
** \fn BIN_EXPcreate
** \param op       operation
** \param operand1 - first operand
** \param operand2 - second operand
** \returns Binary_Expression   - the expression created
** Create a binary operation Expression.
*/
Expression BIN_EXPcreate( Op_Code op, Expression operand1, Expression operand2 ) {
    Expression e = EXPcreate( Type_Expression );

    e->e.op_code = op;
    e->e.op1 = operand1;
    e->e.op2 = operand2;
    return e;
}

/**
** \param op operation
** \param operand  operand
** \returns the expression created
** Create a unary operation Expression.
*/
Expression UN_EXPcreate( Op_Code op, Expression operand ) {
    Expression e = EXPcreate( Type_Expression );

    e->e.op_code = op;
    e->e.op1 = operand;
    return e;
}

/**
** \param local local identifier for source elements
** \param aggregate source aggregate to query
** \returns the query expression created
** Create a query Expression.
** NOTE Dec 2011 - MP - function description did not match actual params. Had to guess.
*/
Expression QUERYcreate( Symbol * local, Expression aggregate ) {
    Expression e = EXPcreate_from_symbol( Type_Query, local );
    Scope s = SCOPEcreate_tiny( OBJ_QUERY );
    Expression e2 = EXPcreate_from_symbol( Type_Attribute, local );

    Variable v = VARcreate( e2, Type_Attribute );

    DICTdefine( s->symbol_table, local->name, v, &e2->symbol, OBJ_VARIABLE );
    e->u.query = QUERY_new();
    e->u.query->scope = s;
    e->u.query->local = v;
    e->u.query->aggregate = aggregate;
    return e;
}
