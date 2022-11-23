/*
 * This software was developed by U.S. Government employees as part of
 * their official duties and is not subject to copyright.
 */

#include "express/express.h"
#include "express/schema.h"
#include "express/resolve.h"

void SCOPEresolve_subsupers( Scope scope ) {
    DictionaryEntry de;
    void *x;
    char type;
    Symbol * sym;
    Type t;

    if( print_objects_while_running & OBJ_SCOPE_BITS &
            OBJget_bits( scope->type ) ) {
        fprintf( stderr, "pass %d: %s (%s)\n", EXPRESSpass,
                 scope->symbol.name, OBJget_type( scope->type ) );
    }

    DICTdo_init( scope->symbol_table, &de );
    while( 0 != ( x = DICTdo( &de ) ) ) {
        switch( type = DICT_type ) {
            case OBJ_ENTITY:
                ENTITYresolve_supertypes( ( Entity )x );
                ENTITYresolve_subtypes( ( Entity )x );
                break;
            case OBJ_FUNCTION:
            case OBJ_PROCEDURE:
            case OBJ_RULE:
                SCOPEresolve_subsupers( ( Scope )x );
                break;
            case OBJ_TYPE:
                t = ( Type )x;
                TYPEresolve( &t );
                break;
            default:
                /* ignored everything else */
                break;
        }
        sym = OBJget_symbol( x, type );
        if( is_resolve_failed_raw( sym ) ) {
            resolve_failed( scope );
        }
    }
}

void SCOPEresolve_expressions_statements( Scope s ) {
    DictionaryEntry de;
    void *x;
    Variable v;

    if( print_objects_while_running & OBJ_SCOPE_BITS &
            OBJget_bits( s->type ) ) {
        fprintf( stderr, "pass %d: %s (%s)\n", EXPRESSpass,
                 s->symbol.name, OBJget_type( s->type ) );
    }

    DICTdo_init( s->symbol_table, &de );
    while( 0 != ( x = DICTdo( &de ) ) ) {
        switch( DICT_type ) {
            case OBJ_SCHEMA:
                if( is_not_resolvable( ( Schema )x ) ) {
                    break;
                }
                SCOPEresolve_expressions_statements( ( Scope )x );
                break;
            case OBJ_ENTITY:
                ENTITYresolve_expressions( ( Entity )x );
                break;
            case OBJ_FUNCTION:
                ALGresolve_expressions_statements( ( Scope )x, ( ( Scope )x )->u.func->body );
                break;
            case OBJ_PROCEDURE:
                ALGresolve_expressions_statements( ( Scope )x, ( ( Scope )x )->u.proc->body );
                break;
            case OBJ_RULE:
                ALGresolve_expressions_statements( ( Scope )x, ( ( Scope )x )->u.rule->body );

                WHEREresolve( RULEget_where( ( Scope )x ), ( Scope )x, 0 );
                break;
            case OBJ_VARIABLE:
                v = ( Variable )x;
                TYPEresolve_expressions( v->type, s );
                if( v->initializer ) {
                    EXPresolve( v->initializer, s, v->type );
                }
                break;
            case OBJ_TYPE:
                TYPEresolve_expressions( ( Type )x, s );
                break;
            default:
                /* ignored everything else */
                break;
        }
    }
}

void ALGresolve_expressions_statements( Scope s, Linked_List statements ) {
    int status = 0;

    if( print_objects_while_running & OBJ_ALGORITHM_BITS &
            OBJget_bits( s->type ) ) {
        fprintf( stderr, "pass %d: %s (%s)\n", EXPRESSpass,
                 s->symbol.name, OBJget_type( s->type ) );
    }

    SCOPEresolve_expressions_statements( s );
    STMTlist_resolve( statements, s );

    s->symbol.resolved = status;
}

