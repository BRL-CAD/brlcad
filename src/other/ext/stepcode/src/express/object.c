

/*
 * This software was developed by U.S. Government employees as part of
 * their official duties and is not subject to copyright.
 *
 * $Log: object.c,v $
 * Revision 1.7  1997/01/21 19:19:51  dar
 * made C++ compatible
 *
 * Revision 1.6  1993/10/15  18:49:55  libes
 * CADDETC certified
 *
 * Revision 1.5  1993/02/22  21:48:18  libes
 * added arg to ERRORabort
 *
 * Revision 1.4  1992/08/18  17:16:22  libes
 * rm'd extraneous error messages
 *
 * Revision 1.3  1992/06/08  18:08:05  libes
 * prettied up interface to print_objects_when_running
 */

#include "express/object.h"

#include "express/scope.h"
#include "express/variable.h"
#include "express/alg.h"
#include "express/schema.h"
#include "express/type.h"
#include "express/expr.h"

Symbol * SCOPE_get_symbol( void *s );
Symbol * EXPRESS_get_symbol( void *e );
Symbol * RENAME_get_symbol( void *r );
Symbol * TYPE_get_symbol( void *t );
Symbol * EXP_get_symbol( void *e );
Symbol * WHERE_get_symbol( void *w );
Symbol * VAR_get_symbol( void *v );
Symbol * UNK_get_symbol( void *x );

/* global Object type array */
struct Object OBJ[] = {
    [0] = {UNK_get_symbol, "of unknown type", 0},
    
    [OBJ_VARIABLE] = {VAR_get_symbol, "variable", OBJ_VARIABLE_BITS},
    [OBJ_ENTITY] = {SCOPE_get_symbol, "entity", OBJ_ENTITY_BITS},
    
    [OBJ_EXPRESSION] = {EXP_get_symbol, "expression", OBJ_EXPRESSION_BITS},
    [OBJ_AMBIG_ENUM] = {EXP_get_symbol, "ambiguous enumeration", OBJ_UNUSED_BITS},
    
    [OBJ_RULE] = {SCOPE_get_symbol, "rule", OBJ_UNUSED_BITS},
    [OBJ_PROCEDURE] = {SCOPE_get_symbol, "procedure", OBJ_PROCEDURE_BITS},
    [OBJ_FUNCTION] = {SCOPE_get_symbol, "function", OBJ_FUNCTION_BITS},
    [OBJ_WHERE] = {WHERE_get_symbol, "where", OBJ_WHERE_BITS},
    
    [OBJ_SCHEMA] = {SCOPE_get_symbol, "schema", OBJ_SCHEMA_BITS},
    /* TODO: PASS should also have a symbol */
    [OBJ_PASS] = {UNK_get_symbol, "pass", OBJ_PASS_BITS},
    [OBJ_EXPRESS] = {EXPRESS_get_symbol, "express file", OBJ_UNUSED_BITS},
    [OBJ_RENAME] = {RENAME_get_symbol, "rename clause", OBJ_UNUSED_BITS},
    
    [OBJ_TYPE] = {TYPE_get_symbol, "type", OBJ_TYPE_BITS},
    [OBJ_TAG] = {TYPE_get_symbol, "tag", OBJ_TYPE_BITS},
    
    [OBJ_ALIAS] = {SCOPE_get_symbol, "alias scope", OBJ_UNUSED_BITS },
    [OBJ_INCREMENT] = {SCOPE_get_symbol, "increment scope", OBJ_UNUSED_BITS },

    {0}
};

Symbol * UNK_get_symbol( void *x ) {
    (void) x; /* quell unused param warning; it appears that the prototype must match other functions */
    fprintf( stderr, "OBJget_symbol called on object of unknown type\n" );
    ERRORabort( 0 );
    return 0;
}

Symbol * VAR_get_symbol( void *v ) {
    return &( ( Variable )v )->name->symbol;
}

Symbol * SCOPE_get_symbol( void *s ) {
    return &( ( Scope )s )->symbol;
}

Symbol * EXP_get_symbol( void *e ) {
    return &( ( Expression )e )->symbol;
}

Symbol * WHERE_get_symbol( void *w ) {
    return ( ( Where )w )->label;
}

Symbol * EXPRESS_get_symbol( void *e ) {
    return &( ( Express )e )->symbol;
}

Symbol * RENAME_get_symbol( void *r ) {
    return ( ( Rename * )r )->old;
}

Symbol * TYPE_get_symbol( void *t ) {
    return &( ( Type )t )->symbol;
}
