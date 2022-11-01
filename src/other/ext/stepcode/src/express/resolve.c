

/*
 * This software was developed by U.S. Government employees as part of
 * their official duties and is not subject to copyright.
 *
 * $Log: resolve.c,v $
 * Revision 1.14  1997/01/21 19:19:51  dar
 * made C++ compatible
 *
 * Revision 1.13  1995/06/08  22:59:59  clark
 * bug fixes
 *
 * Revision 1.12  1995/05/17  14:28:07  libes
 * Fixed bug in WHEREresolve return value.
 *
 * Revision 1.11  1995/04/08  20:54:18  clark
 * WHERE rule resolution bug fixes
 *
 * Revision 1.11  1995/04/08  20:49:50  clark
 * WHERE
 *
 * Revision 1.10  1995/03/09  18:44:02  clark
 * various fixes for caddetc - before interface clause changes
 *
 * Revision 1.9  1994/11/22  18:32:39  clark
 * Part 11 IS; group reference
 *
 * Revision 1.8  1994/11/10  19:20:03  clark
 * Update to IS
 *
 * Revision 1.7  1994/05/11  19:51:24  libes
 * numerous fixes
 *
 * Revision 1.6  1993/10/15  18:48:48  libes
 * CADDETC certified
 *
 * Revision 1.4  1993/02/16  03:24:37  libes
 * fixed numerous type botches (see comment in expparse.y)
 * fixed statement handling botches
 * completed implicit sub/supertypes
 * misc other fixeds
 *
 * Revision 1.3  1993/01/19  22:17:27  libes
 * *** empty log message ***
 *
 * Revision 1.2  1992/09/16  18:23:08  libes
 * fixed bug in TYPEresolve connecting reference types back to using types
 *
 * Revision 1.1  1992/08/18  17:13:43  libes
 * Initial revision
 *
 * Revision 1.4  1992/06/08  18:06:57  libes
 * prettied up interface to print_objects_when_running
 *
 */

#include <assert.h>
#include <stdlib.h>

#include "express/resolve.h"
#include "express/schema.h"
#include "express/express.h"

int print_objects_while_running = 0;

static Type self = 0;   /**< always points to current value of SELF or 0 if none */

static bool found_self;  /**< remember whether we've seen a SELF in a WHERE clause */

/***********************/
/* function prototypes */
/***********************/

extern void VAR_resolve_types( Variable v );

/** Initialize the Fed-X second pass. */
void RESOLVEinitialize( void ) {
}

/** Clean up the Fed-X second pass */
void RESOLVEcleanup( void ) {
}

/**
** Retrieve the aggregate type from the underlying types of the select type t_select
** \param t_select the select type to retrieve the aggregate type from
** \param t_agg the current aggregate type
** \return the aggregate type
*/
Type TYPE_retrieve_aggregate( Type t_select, Type t_agg ) {
    if( TYPEis_select( t_select ) ) {
        /* parse the underlying types */
        LISTdo_links( t_select->u.type->body->list, link )
        /* the current underlying type */
        Type t = ( Type ) link->data;
        if( TYPEis_select( t ) ) {
            t_agg = TYPE_retrieve_aggregate( t, t_agg );
        } else if( TYPEis_aggregate( t ) ) {
            if( t_agg ) {
                if( t_agg != t->u.type->body->base ) {
                    /* 2 underlying types do not have to the same base */
                    return 0;
                }
            } else {
                t_agg = t->u.type->body->base;
            }
        } else {
            /* the underlying type is neither a select nor an aggregate */
            return 0;
        }

        LISTod;
    }

    return t_agg;
}

/**
** \param expr expression to resolve
** \param scope scope in which to resolve
** \param typecheck type to verify against (?)
**
** Resolve all references in an expression.
** \note the macro 'EXPresolve' calls this function after checking if expr is already resolved
*/
void EXP_resolve( Expression expr, Scope scope, Type typecheck ) {
    Function f = 0;
    Symbol * sym;
    void *x;
    Entity e;
    Type t;
    bool func_args_checked = false;

    /*  if (expr == EXPRESSION_NULL)
            return;
    */
    switch( expr->type->u.type->body->type ) {
        case funcall_:
            /* functions with no arguments get handled elsewhere */
            /* because the parser sees them just like attributes */

            x = SCOPEfind( scope, expr->symbol.name,
                           SCOPE_FIND_FUNCTION | SCOPE_FIND_ENTITY );
            if( !x ) {
                ERRORreport_with_symbol(UNDEFINED_FUNC, &expr->symbol, expr->symbol.name );
                resolve_failed( expr );
                break;
            }
            if( ( DICT_type != OBJ_FUNCTION ) && ( DICT_type != OBJ_ENTITY ) ) {
                sym = OBJget_symbol( x, DICT_type );
                ERRORreport_with_symbol(FUNCALL_NOT_A_FUNCTION, &expr->symbol, sym->name );
                resolve_failed( expr );
                break;
            }
            /* original code accepted rules, too? */

            /* entities are treated like implicit constructor functions */
            if( DICT_type == OBJ_ENTITY ) {
                Type self_old = self; /* save previous in the unlikely but possible case that
                                       * SELF is in a derived initialization of an entity */
                e = ( Entity )x;
                self = e->u.entity->type;
                /* skip parameter resolution for now */
                /*          ARGresolve();*/
                expr->return_type = e->u.entity->type;
                self = self_old;    /* restore old SELF */
            } else {
                f = ( Function )x;
                expr->return_type = f->u.func->return_type;

                /* do argument typechecking here if requested */
                /* currently, we just check arg count; necessary */
                /* to NVL code later which assumes args are present */
                if( LISTget_length( expr->u.funcall.list ) !=
                        f->u.func->pcount ) {
                    ERRORreport_with_symbol(WRONG_ARG_COUNT, &expr->symbol, expr->symbol.name,
                                             LISTget_length( expr->u.funcall.list ),
                                             f->u.func->pcount );
                }

#ifdef future_work
                if( EXPRESS_lint ) {
                    /* verify parameters match function call */
                }
#endif

                /* should make this data-driven! */
                if( f == FUNC_NVL ) {
                    EXPresolve( ( Expression )LISTget_first( expr->u.funcall.list ), scope, typecheck );
                    EXPresolve( ( Expression )LISTget_second( expr->u.funcall.list ), scope, typecheck );
                    func_args_checked = true;
                }

                /* why is this here?  (snc) */
                if( f == FUNC_USEDIN ) {
                    expr->return_type = Type_Bag_Of_Generic;
                }
            }
            if( !func_args_checked ) {
                LISTdo( expr->u.funcall.list, param, Expression )
                EXPresolve( param, scope, Type_Dont_Care );
                if( is_resolve_failed( param ) ) {
                    resolve_failed( expr );
                    break;
                }
                LISTod;
            }

#if 0
            /* add function or entity as first element of list */
            LISTadd_first( expr->u.list, x );
#endif
            expr->u.funcall.function = ( struct Scope_ * ) x;

            resolved_all( expr );
            break;
        case aggregate_:
            LISTdo( expr->u.list, elt, Expression )
            EXPresolve( elt, scope, Type_Dont_Care );
            if( is_resolve_failed( elt ) ) {
                resolve_failed( expr );
                break;
            }
            LISTod;

            /* may have to do more work here! */
            expr->return_type = expr->type;
            resolved_all( expr );
            break;
        case identifier_:

            x = 0;

            /* assume it's a variable/attribute */
            if( !x ) {
                x = VARfind( scope, expr->symbol.name, 0 );
            }
            /* if not found as a variable, try as function, etc ... */
            if( !x ) {
                x = SCOPEfind( scope, expr->symbol.name,
                               SCOPE_FIND_ANYTHING );
            }
            /* Not all enums have `typecheck->u.type->body->type` == `enumeration_` - ?! */
            if( !x ) {
                Scope enumscope = scope;
                while( 1 ) {
                    /* look up locally, then go through the superscopes */
                    x = DICTlookup( enumscope->enum_table, expr->symbol.name );
                    if( x ) {
                        break;
                    }
                    if( enumscope->type == OBJ_SCHEMA ) {
                        /* if we get here, this means that we've looked through all scopes */
                        x = 0;
                        break;
                    }
                    enumscope = enumscope->superscope;
                }
            }

            if( !x ) {
                if( typecheck == Type_Unknown ) {
                    return;
                } else {
                    ERRORreport_with_symbol(UNDEFINED, &expr->symbol, expr->symbol.name );
                    resolve_failed( expr );
                    break;
                }
            }
            switch( DICT_type ) {
                case OBJ_VARIABLE:
                    expr->u.variable = ( Variable )x;
#if 0
                    /* gee, I don't see what variables have to go through this right here */
                    VARresolve_expressions( expr->u.variable, scope );
                    if( is_resolve_failed( expr->u.variable->name ) ) {
                        resolve_failed( expr );
                        break;
                    }
#endif
                    /* Geez, don't wipe out original type! */
                    expr->return_type = expr->u.variable->type;
                    if( expr->u.variable->flags.attribute ) {
                        found_self = true;
                    }
                    resolved_all( expr );
                    break;
                case OBJ_ENTITY:
                    expr->return_type = expr->type = ( ( Entity )x )->u.entity->type;
                    /* entity may not actually be resolved by now */
                    /* but I don't think that's a problem */
                    resolved_all( expr );
                    break;
                case OBJ_EXPRESSION:
                    /* so far only enumerations get returned this way */
                    expr->u.expression = ( Expression )x;
                    expr->type = expr->return_type = ( ( Expression )x )->type;
                    resolved_all( expr );
                    break;
                case OBJ_FUNCTION:
                    /* functions with no args end up here because the */
                    /* parser doesn't know any better */
                    expr->u.list = LISTcreate();
                    LISTadd_last( expr->u.list, x );
                    expr->type = Type_Funcall;
                    expr->return_type = ( ( Function )x )->u.func->return_type;
                    /* function may not actually be resolved by now */
                    /* but I don't think that's a problem */

                    if( ( ( Function )x )->u.func->pcount != 0 ) {
                        ERRORreport_with_symbol(WRONG_ARG_COUNT, &expr->symbol, expr->symbol.name, 0,
                                                 f->u.func->pcount );
                        resolve_failed( expr );
                    } else {
                        resolved_all( expr );
                    }
                    break;
                case OBJ_TYPE:
                    /* enumerations can appear here, I don't know about others */
                    expr->type = ( Type )x;
                    expr->return_type = ( Type )x;
                    expr->symbol.resolved = expr->type->symbol.resolved;
                    break;
                default:
                    fprintf( stderr, "ERROR: unexpected type in EXPresolve.\n" );
                    break;
            }
            break;
        case op_:
            expr->return_type = ( *EXPop_table[expr->e.op_code].resolve )( expr, scope );
            break;
        case entity_:           /* only 'self' is seen this way */
        case self_:
            if( self ) {
                expr->return_type = self;
                /* we can't really call ourselves resolved, but we */
                /* will be by the time we return, and besides, */
                /* there's no way this will be accessed if the true */
                /* entity fails resolution */
                found_self = true;
                resolved_all( expr );
            } else {
                ERRORreport_with_symbol(SELF_IS_UNKNOWN, &scope->symbol );
                resolve_failed( expr );
            }
            break;
        case query_:
            EXPresolve( expr->u.query->aggregate, expr->u.query->scope, Type_Dont_Care );
            expr->return_type = expr->u.query->aggregate->return_type;

            /* verify that it's an aggregate */
            if( is_resolve_failed( expr->u.query->aggregate ) ) {
                resolve_failed( expr );
                break;
            }
            if( TYPEis_aggregate( expr->return_type ) ) {
                t = expr->u.query->aggregate->return_type->u.type->body->base;
            } else if( TYPEis_select( expr->return_type ) ) {
                /* retrieve the common aggregate type */
                t = TYPE_retrieve_aggregate( expr->return_type, 0 );
                if( !t ) {
                    ERRORreport_with_symbol(QUERY_REQUIRES_AGGREGATE, &expr->u.query->aggregate->symbol );
                    resolve_failed( expr );
                    break;
                }
            } else if( TYPEis_runtime( expr->return_type ) ) {
                t = Type_Runtime;
            } else {
                ERRORreport_with_symbol(QUERY_REQUIRES_AGGREGATE, &expr->u.query->aggregate->symbol );
                resolve_failed( expr );
                break;
            }
            expr->u.query->local->type = t;
            expr->u.query->local->name->return_type = t;
            EXPresolve( expr->u.query->expression, expr->u.query->scope, Type_Dont_Care );
            expr->symbol.resolved = expr->u.query->expression->symbol.resolved;
            break;
        case integer_:
        case real_:
        case string_:
        case binary_:
        case boolean_:
        case logical_:
        case number_:
            expr->return_type = expr->type;
            resolved_all( expr );
            break;
        case attribute_:
            expr->return_type = expr->type;
            resolved_all( expr );
            break;
        default:
            fprintf( stderr, "ERROR: unexpected type in EXPresolve.\n" );
    }
}

int ENTITYresolve_subtype_expression( Expression expr, Entity ent/*was scope*/, Linked_List * flat ) {
    Entity ent_ref;
    int i = UNRESOLVED;

    if( !expr ) {
        return ( RESOLVED );
    } else if( TYPEis_expression( expr->type ) ) {
        i = ENTITYresolve_subtype_expression( expr->e.op1, ent, flat );
        i |= ENTITYresolve_subtype_expression( expr->e.op2, ent, flat );
    } else if( TYPEis_oneof( expr->type ) ) {
        LISTdo( expr->u.list, sel, Expression )
        i |= ENTITYresolve_subtype_expression( sel, ent, flat );
        LISTod;
    } else {
        /* must be a simple entity reference */
        ent_ref = ( Entity )SCOPEfind( ent->superscope, expr->symbol.name, SCOPE_FIND_ENTITY );
        if( !ent_ref ) {
            ERRORreport_with_symbol(UNKNOWN_SUBTYPE, &ent->symbol,
                                     expr->symbol.name, ent->symbol.name );
            i = RESOLVE_FAILED;
        } else if( DICT_type != OBJ_ENTITY ) {
            Symbol * sym = OBJget_symbol( ent_ref, DICT_type );
            /* line number should really be on supertype name, */
            /* but all we have easily is the entity line number */
            ERRORreport_with_symbol(SUBTYPE_RESOLVE, &ent->symbol,
                                     expr->symbol.name, sym->line );
            i = RESOLVE_FAILED;
        } else {
            bool found = false;

            /* link in to flat list */
            if( !*flat ) {
                *flat = LISTcreate();
            }

            LISTdo( *flat, sub, Entity )
            if( sub == ent_ref ) {
                found = true;
                break;
            }
            LISTod

            if( !found ) {
                LISTadd_last( *flat, ent_ref );
            }

            /* link in to expression */
            expr->type = ent_ref->u.entity->type;
            i = RESOLVED;

#if 0
            /* If the user said there was a subtype relationship but */
            /* did not mention the reverse supertype relationship, */
            /* complain (IS p. 44) */
            LISTdo( ent_ref->u.entity->supertypes, sup, Entity )
            if( sup == ent ) {
                found = true;
                break;
            }
            LISTod
            if( !found ) {
                if( !ent_ref->u.entity->supertypes ) {
                    ent_ref->u.entity->supertypes = LISTcreate();
                }
                LISTadd_last( ent_ref->u.entity->supertypes, ent );
            }
#endif
        }
    }
    return( i );
}

/**
** \param typeaddr type to resolve
** \returns true type
**
** Resolve all references in a type.
*/
void TYPE_resolve( Type * typeaddr /*, Scope scope*/ ) {
    Type type = *typeaddr;
    Type ref_type;
    TypeBody body = type->u.type->body;
    Scope scope = type->superscope;

    if( body ) {
        /* complex type definition such as aggregates, enums, ... */

        resolve_in_progress( type );

        if( TYPEis_aggregate( type ) ) {
            TYPEresolve( &body->base );
            /* only really critical failure point for future use */
            /* of this type is the base type, ignore others (above) */
            type->symbol.resolved = body->base->symbol.resolved;
        } else if( TYPEis_select( type ) ) {
            LISTdo_links( body->list, link )
            TYPEresolve( ( Type * )&link->data );
            if( is_resolve_failed( ( Type )link->data ) ) {
                resolve_failed( type );
                break;
            }
            LISTod;
        }
    } else if( type->u.type->head ) {
        /* simple type definition such as "TYPE T = U" */
        resolve_in_progress( type );

        TYPEresolve( &type->u.type->head );

        if( !is_resolve_failed( type->u.type->head ) ) {
            if( ERRORis_enabled( TYPE_IS_ENTITY ) ) {
                if( TYPEis_entity( type->u.type->head ) ) {
                    ERRORreport_with_symbol(TYPE_IS_ENTITY, &type->symbol, type->u.type->head->u.type->body->entity->symbol.name );
                    resolve_failed( type );
                }
            }
            /* allow type ref's to be bypassed by caching true type */
            type->u.type->body = type->u.type->head->u.type->body;
        }
    } else {
        /* simple type reference such as "T" */
        /* this really is a hack.  masking out only variables from */
        /* the search is to support the (allowed) circumstance of */
        /* an attribute or formal parameter whose name is the same */
        /* as its type, i.e. "foo : foo".  unfortunately, babys like */
        /* local variables get thrown out with the bathwater.  -snc */
        ref_type = ( Type )SCOPEfind( scope, type->symbol.name,
                                      SCOPE_FIND_ANYTHING ^ SCOPE_FIND_VARIABLE );
        /*              SCOPE_FIND_TYPE | SCOPE_FIND_ENTITY);*/
        if( !ref_type ) {
            ERRORreport_with_symbol(UNDEFINED_TYPE, &type->symbol, type->symbol.name );
            *typeaddr = Type_Bad; /* just in case */
            resolve_failed( type );
        } else if( DICT_type == OBJ_TYPE ) {
            /* due to declarations of multiple attributes off of a */
            /* single type ref, we have to use reference counts */
            /* to safely deallocate the TypeHead.  It's trivial to do */
            /* but gaining back the memory isn't worth the CPU time. */
            /* if (type->refcount--) TYPE_destroy(type); */

            type = *typeaddr = ref_type;
            TYPEresolve( typeaddr ); /* addr doesn't matter here */
            /* it will not be written through */
        } else if( DICT_type == OBJ_ENTITY ) {
            /* if (type->refcount--) TYPE_destroy(type); see above */

            type = *typeaddr = ( ( Entity )ref_type )->u.entity->type;
        } else {
            ERRORreport_with_symbol(NOT_A_TYPE, &type->symbol, type->symbol.name,
                                     OBJget_type( DICT_type ) );
            resolve_failed( type );
        }
    }

    if( !is_resolve_failed( type ) ) {
        resolved_all( type );
    }
    return;
}

/**
** \param v variable to resolve
** \param entity entity in which to resolve
**
** Resolve all references in a variable definition.
*/
void VAR_resolve_expressions( Variable v, Entity entity /* was scope */ ) {
    EXPresolve( v->name, entity, Type_Dont_Care ); /* new!! */

    if( v->initializer ) {
        EXPresolve( v->initializer, entity, v->type );

        if( is_resolve_failed( v->initializer ) ) {
            resolve_failed( v->name );
        }
    }
}

/**
** \param v variable to resolve
**
** Resolve all references in a variable definition.
*/
void VAR_resolve_types( Variable v ) {
    int failed = 0;

    TYPEresolve( &v->type );
    failed = is_resolve_failed( v->type );

    if( v->inverse_symbol && ( !v->inverse_attribute ) ) {
        /* resolve inverse */
        Variable attr;
        Type type = v->type;

        if( TYPEis_aggregate( type ) ) {
            /* pull entity out of aggregate type defn for ... */
            /* inverse var: set (or bag) of entity for ...; */
            type = type->u.type->body->base;
        }
        if( type->u.type->body->type != entity_ ) {
            ERRORreport_with_symbol(INVERSE_BAD_ENTITY,
                                     &v->name->symbol, v->inverse_symbol->name );
        } else {
            attr = VARfind( type->u.type->body->entity, v->inverse_symbol->name, 1 );
            if( attr ) {
                v->inverse_attribute = attr;
                failed |= is_resolve_failed( attr->name );
            } else {
                ERRORreport_with_symbol(INVERSE_BAD_ATTR,
                                         v->inverse_symbol, v->inverse_symbol->name, type->u.type->body->entity->symbol.name );
            }
        }
        /* symbol is no longer used here and could be gc'd */
        /* but keep around anyway for ease in later reconstruction */
    }

    if( failed ) {
        resolve_failed( v->name );
    }

    /* note: cannot set resolved bit since it has to be resolved again */
    /* by VAR_resolve_expressions later on */
#if 0
    else {
        resolved_all( v->name );
    }
#endif
}

/**
** \param statement statement to resolve
** \param scope scope in which to resolve
**
** Resolve all references in a statement.
*/

void STMTlist_resolve( Linked_List list, Scope scope ) {
    LISTdo( list, s, Statement )
    STMTresolve( s, scope );
    LISTod;
}

/**
 * \param item case item to resolve
 * \param scope scope in which to resolve
 * \param statement the CASE statement (for return type, etc)
 *
 * Resolve all references in a case item
 */
void CASE_ITresolve( Case_Item item, Scope scope, Statement statement ) {
    int validLabels = 0;
    LISTdo( item->labels, e, Expression ) {
        EXPresolve( e, scope, statement->u.Case->selector->return_type );
        if( e->return_type != Type_Bad ) {
            validLabels++;
        }
    }
    LISTod;
    if( validLabels ) {
        STMTresolve( item->action, scope );
    }
}

void STMTresolve( Statement statement, Scope scope ) {
    /* scope is always the function/procedure/rule from SCOPEresolve_expressions_statements(); */
    Scope proc;

    if( !statement ) {
        return;    /* could be null statement */
    }

    switch( statement->type ) {
        case STMT_ALIAS:
            EXPresolve( statement->u.alias->variable->initializer, scope, Type_Dont_Care );
            statement->u.alias->variable->type =
                statement->u.alias->variable->initializer->type;
            if( !is_resolve_failed( statement->u.alias->variable->initializer ) ) {
                STMTlist_resolve( statement->u.alias->statements, statement->u.alias->scope );
            }
            break;
        case STMT_ASSIGN:
            EXPresolve( statement->u.assign->lhs, scope, Type_Dont_Care );
            EXPresolve( statement->u.assign->rhs, scope, statement->u.assign->lhs->type );
            break;
        case STMT_CASE:
            EXPresolve( statement->u.Case->selector, scope, Type_Dont_Care );
            LISTdo( statement->u.Case->cases, c, Case_Item ) {
                CASE_ITresolve( c, scope, statement );
            }
            LISTod;
            break;
        case STMT_COMPOUND:
            STMTlist_resolve( statement->u.compound->statements, scope );
            break;
        case STMT_COND:
            EXPresolve( statement->u.cond->test, scope, Type_Dont_Care );
            STMTlist_resolve( statement->u.cond->code, scope );
            if( statement->u.cond->otherwise ) {
                STMTlist_resolve( statement->u.cond->otherwise, scope );
            }
            break;
        case STMT_PCALL:
#define proc_name statement->symbol.name
            proc = ( Scope )SCOPEfind( scope, proc_name,
                                       SCOPE_FIND_PROCEDURE );
            if( proc ) {
                if( DICT_type != OBJ_PROCEDURE ) {
                    Symbol * newsym = OBJget_symbol( proc, DICT_type );
                    ERRORreport_with_symbol(EXPECTED_PROC, &statement->symbol, proc_name, newsym->line );
                } else {
                    statement->u.proc->procedure = proc;
                }
            } else {
                ERRORreport_with_symbol(NO_SUCH_PROCEDURE, &statement->symbol, proc_name );
            }
            LISTdo( statement->u.proc->parameters, e, Expression )
            EXPresolve( e, scope, Type_Dont_Care );
            LISTod;
            break;
        case STMT_LOOP:
            if( statement->u.loop->scope ) {
                /* resolve increment with old scope */
                EXPresolve( statement->u.loop->scope->u.incr->init, scope, Type_Dont_Care );
                EXPresolve( statement->u.loop->scope->u.incr->end, scope, Type_Dont_Care );
                EXPresolve( statement->u.loop->scope->u.incr->increment, scope, Type_Dont_Care );
                /* resolve others with new scope! */
                scope = statement->u.loop->scope;
            }
            if( statement->u.loop->while_expr ) {
                EXPresolve( statement->u.loop->while_expr, scope, Type_Dont_Care );
            }

            if( statement->u.loop->until_expr ) {
                EXPresolve( statement->u.loop->until_expr, scope, Type_Dont_Care );
            }

            STMTlist_resolve( statement->u.loop->statements, scope );
            break;
        case STMT_RETURN:
            if( statement->u.ret->value ) {
                EXPresolve( statement->u.ret->value, scope, Type_Dont_Care );
            }
            break;
        case STMT_SKIP:
        case STMT_ESCAPE:
            /* do nothing */ /* WARNING should we really *not* do anything for these? */
            ;
    }
}

static Variable ENTITY_get_local_attribute( Entity e, char * name ) {
    LISTdo( e->u.entity->attributes, a, Variable )
    if( !strcmp( VARget_simple_name( a ), name ) ) {
        return a;
    }
    LISTod;
    return 0;
}

void ENTITYresolve_expressions( Entity e ) {
    Variable v;
    int status = 0;
    DictionaryEntry de;
    char * sname;
    Entity sup;

    if( print_objects_while_running & OBJ_ENTITY_BITS ) {
        fprintf( stderr, "pass %d: %s (entity)\n", EXPRESSpass,
                 e->symbol.name );
    }

    self = e->u.entity->type;

    LISTdo( e->u.entity->attributes, attr, Variable ) {
        if( attr->name->type->u.type->body->type == op_ ) {
            /* attribute redeclaration */
            sname = attr->name->e.op1->e.op2->symbol.name;
            if( !strcmp( sname, e->symbol.name ) ||
                    !( sup = ENTITYfind_inherited_entity( e, sname, 0 ) ) ) {
                ERRORreport_with_symbol(REDECL_NO_SUCH_SUPERTYPE,
                                        &attr->name->e.op1->e.op2->symbol,
                                        attr->name->e.op1->e.op2->symbol.name,
                                        VARget_simple_name( attr ) );
                resolve_failed( attr->name );
            } else {
                sname = VARget_simple_name( attr );
                if( !ENTITY_get_local_attribute( sup, sname ) ) {
                    ERRORreport_with_symbol(REDECL_NO_SUCH_ATTR,
                                            &attr->name->e.op2->symbol,
                                            sname,
                                            sup->symbol.name );
                    resolve_failed( attr->name );
                }
                /* should be ok to share this ptr */
                attr->name->symbol.name = sname;
            }
        } else {
            /* new attribute declaration */
            LISTdo_n( e->u.entity->supertypes, supr, Entity, b ) {
                    if( ENTITYget_named_attribute( supr,
                                                attr->name->symbol.name ) ) {
                        ERRORreport_with_symbol(OVERLOADED_ATTR,
                                                &attr->name->symbol,
                                                attr->name->symbol.name,
                                                supr->symbol.name );
                        resolve_failed( attr->name );
                    }
            } LISTod;
        }
        VARresolve_expressions( attr, e );
        status |= is_resolve_failed( attr->name );
    } LISTod;

    DICTdo_type_init( e->symbol_table, &de, OBJ_VARIABLE );
    while( 0 != ( v = ( Variable )DICTdo( &de ) ) ) {
        if( !is_resolve_failed( v->name ) ) {
            TYPEresolve_expressions( v->type, e );
            if( v->initializer ) {
                EXPresolve( v->initializer, e, v->type );
                status |= is_resolve_failed( v->initializer );
            }
        } else {
            status = RESOLVE_FAILED;
        }
    }

    if( !WHEREresolve( e->where, e, 1 ) ) {
        status = RESOLVE_FAILED;
    }

    self = 0;

    e->symbol.resolved = status;
}



void ENTITYcheck_missing_supertypes( Entity ent ) {
    int found;

    /* Make sure each of my subtypes lists me as a supertype */
    LISTdo( ent->u.entity->subtypes, sub, Entity ) {
        found = false;
        LISTdo_n( sub->u.entity->supertypes, sup, Entity, b ) {
            if( sup == ent ) {
                found = true;
                break;
            }
        } LISTod;
        if( !found ) {
            ERRORreport_with_symbol(MISSING_SUPERTYPE, &sub->symbol, ent->symbol.name, sub->symbol.name );
            resolve_failed( sub );
        }
    } LISTod;
}

/** calculate number of attributes inheritance, following up superclass chain */
void ENTITYcalculate_inheritance( Entity e ) {
    e->u.entity->inheritance = 0;

    LISTdo( e->u.entity->supertypes, super, Entity ) {
        if( super->u.entity->inheritance == ENTITY_INHERITANCE_UNINITIALIZED ) {
            ENTITYcalculate_inheritance( super );
        }
        e->u.entity->inheritance += ENTITYget_size( super );
    }
    LISTod
}

/** returns 1 if entity is involved in circularity, else 0 */
int ENTITY_check_subsuper_cyclicity( Entity e, Entity enew ) {
    /* just check subtypes - this implicitly checks supertypes */
    /* as well */
    LISTdo( enew->u.entity->subtypes, sub, Entity )
    if( e == sub ) {
        ERRORreport_with_symbol(SUBSUPER_LOOP, &sub->symbol, e->symbol.name );
        return 1;
    }
    if( sub->search_id == __SCOPE_search_id ) {
        return 0;
    }
    sub->search_id = __SCOPE_search_id;
    if( ENTITY_check_subsuper_cyclicity( e, sub ) ) {
        ERRORreport_with_symbol(SUBSUPER_CONTINUATION, &sub->symbol, sub->symbol.name );
        return 1;
    }
    LISTod;
    return 0;
}

void ENTITYcheck_subsuper_cyclicity( Entity e ) {
    __SCOPE_search_id++;
    ( void ) ENTITY_check_subsuper_cyclicity( e, e );
}

/** returns 1 if select type is involved in circularity, else 0 */
int TYPE_check_select_cyclicity( TypeBody tb, Type tnew ) {
    LISTdo( tnew->u.type->body->list, item, Type )
    if( item->u.type->body->type == select_ ) {
        if( tb == item->u.type->body ) {
            ERRORreport_with_symbol(SELECT_LOOP,
                                     &item->symbol, item->symbol.name );
            return 1;
        }
        if( item->search_id == __SCOPE_search_id ) {
            return 0;
        }
        item->search_id = __SCOPE_search_id;
        if( TYPE_check_select_cyclicity( tb, item ) ) {
            ERRORreport_with_symbol(SELECT_CONTINUATION,
                                     &item->symbol, item->symbol.name );
            return 1;
        }
    }
    LISTod;
    return 0;
}

void TYPEcheck_select_cyclicity( Type t ) {
    if( t->u.type->body->type == select_ ) {
        __SCOPE_search_id++;
        ( void ) TYPE_check_select_cyclicity( t->u.type->body, t );
    }
}

void ENTITYresolve_types( Entity e );

/** also resolves inheritance counts and sub/super consistency */
void SCOPEresolve_types( Scope s ) {
    Variable var;
    DictionaryEntry de;
    void *x;

    if( print_objects_while_running & OBJ_SCOPE_BITS &
            OBJget_bits( s->type ) ) {
        fprintf( stderr, "pass %d: %s (%s)\n", EXPRESSpass,
                 s->symbol.name, OBJget_type( s->type ) );
    }

    DICTdo_init( s->symbol_table, &de );
    while( 0 != ( x = DICTdo( &de ) ) ) {
        switch( DICT_type ) {
            case OBJ_TYPE:
                if( ERRORis_enabled( SELECT_LOOP ) ) {
                    TYPEcheck_select_cyclicity( ( Type )x );
                }
                break;
            case OBJ_VARIABLE:  /* really constants */
                var = ( Variable )x;
                /* before OBJ_BITS hack, we looked in s->superscope */
                TYPEresolve( &var->type );
                if( is_resolve_failed( var->type ) ) {
                    resolve_failed( var->name );
                    resolve_failed( s );
                }
                break;
            case OBJ_ENTITY:
                ENTITYcheck_missing_supertypes( ( Entity )x );
                ENTITYresolve_types( ( Entity )x );
                ENTITYcalculate_inheritance( ( Entity )x );
                if( ERRORis_enabled( SUBSUPER_LOOP ) ) {
                    ENTITYcheck_subsuper_cyclicity( ( Entity )x );
                }
                if( is_resolve_failed( ( Entity )x ) ) {
                    resolve_failed( s );
                }
                break;
            case OBJ_SCHEMA:
                if( is_not_resolvable( ( Schema )x ) ) {
                    break;
                }
                /*FALLTHRU*/
            case OBJ_PROCEDURE:
            case OBJ_RULE:
            case OBJ_FUNCTION:
                SCOPEresolve_types( ( Scope )x );
                if( is_resolve_failed( ( Scope )x ) ) {
                    resolve_failed( s );
                }
                break;
            default:
                break;
        }
    }
    if( s->type == OBJ_FUNCTION ) {
        TYPEresolve( &s->u.func->return_type );
    }
}

/** for each supertype, find the entity it refs to */
void ENTITYresolve_supertypes( Entity e ) {
    Entity ref_entity;

    if( print_objects_while_running & OBJ_ENTITY_BITS ) {
        fprintf( stderr, "pass %d: %s (entity)\n", EXPRESSpass,
                 e->symbol.name );
    }

    if( e->u.entity->supertype_symbols ) {
        e->u.entity->supertypes = LISTcreate();
    }
#if 0
    if( e->u.entity->supertype_symbols && !e->u.entity->supertypes ) {
        e->u.entity->supertypes = LISTcreate();
    }
#endif

    LISTdo( e->u.entity->supertype_symbols, sym, Symbol * ) {
        ref_entity = ( Entity )SCOPEfind( e->superscope, sym->name, SCOPE_FIND_ENTITY );
        if( !ref_entity ) {
            ERRORreport_with_symbol(UNKNOWN_SUPERTYPE, sym, sym->name, e->symbol.name );
            /*          ENTITY_resolve_failed = 1;*/
            resolve_failed( e );
        } else if( DICT_type != OBJ_ENTITY ) {
            Symbol * newsym = OBJget_symbol( ref_entity, DICT_type );
            ERRORreport_with_symbol(SUPERTYPE_RESOLVE, sym, sym->name, newsym->line );
            /*          ENTITY_resolve_failed = 1;*/
            resolve_failed( e );
        } else {
            bool found = false;

            LISTadd_last( e->u.entity->supertypes, ref_entity );
            if( is_resolve_failed( ref_entity ) ) {
                resolve_failed( e );
            }

            /* If the user said there was a supertype relationship but */
            /* did not mentioned the reverse subtype relationship */
            /* force it to be explicitly known by listing this entity */
            /* in the ref'd entity's subtype list */

            LISTdo_n( ref_entity->u.entity->subtypes, sub, Entity, b ) {
                if( sub == e ) {
                    found = true;
                    break;
                }
            } LISTod
            if( !found ) {
                if( !ref_entity->u.entity->subtypes ) {
                    ref_entity->u.entity->subtypes = LISTcreate();
                }
                LISTadd_last( ref_entity->u.entity->subtypes, e );
            }
        }
    } LISTod;
}

void ENTITYresolve_subtypes( Entity e ) {
    int i;

    if( print_objects_while_running & OBJ_ENTITY_BITS ) {
        fprintf( stderr, "pass %d: %s (entity)\n", EXPRESSpass,
                 e->symbol.name );
    }

    i = ENTITYresolve_subtype_expression( e->u.entity->subtype_expression, e, &e->u.entity->subtypes );
    if( i & RESOLVE_FAILED ) {
        resolve_failed( e );
    }
}

/** resolve the 'unique' list
 * these are lists of lists
 * the sublists are: label, ref'd_attr, ref'd attr, ref'd attr, etc.
 * where ref'd_attrs are either simple ids or SELF\entity.attr
 * where "entity" represents a supertype (only, I believe)
*/
void ENTITYresolve_uniques( Entity e ) {
    Variable attr, attr2 = 0;
    int failed = 0;
    LISTdo( e->u.entity->unique, unique, Linked_List ) {
        int i = 0;
        LISTdo_links( unique, reflink ) {
            Type old_self = self;
            Expression expr;
            /* skip first which is always the label (or NULL if no label) */
            i++;
            if( i == 1 ) {
                continue;
            }
            expr = ( Expression ) reflink->data;
            assert( expr );

            self = e->u.entity->type;
            EXPresolve( expr, e, Type_Dont_Care );
            self = old_self;

            /* SELF\entity.attr, or just an attr name? */
            if( ( expr->e.op_code == OP_DOT ) &&
                    ( expr->e.op1->e.op_code == OP_GROUP ) &&
                    ( expr->e.op1->e.op1->type == Type_Self ) ) {
                attr = ENTITYresolve_attr_ref( e, &( expr->e.op1->e.op2->symbol ), &( expr->e.op2->symbol ) );
                attr2 = ENTITYresolve_attr_ref( e, 0, &( expr->e.op2->symbol ) );
            } else {
                attr = ENTITYresolve_attr_ref( e, 0, &( expr->symbol ) );
            }
            if( ( attr2 ) && ( attr != attr2 ) && ( ENTITYdeclares_variable( e, attr2 ) ) ) {
                /* attr exists in type + supertype - it's a redeclaration.
                 * in this case, qualifiers are unnecessary; print a warning */
                ERRORreport_with_symbol(UNIQUE_QUAL_REDECL, &( expr->e.op2->symbol ), expr->e.op2->symbol.name, e->symbol.name );
            }
            if( !attr ) {
                /*      ERRORreport_with_symbol(ERROR_unknown_attr_in_entity,*/
                /*                  aref->attribute, aref->attribute->name,*/
                /*                  e->symbol.name);*/
                failed = RESOLVE_FAILED;
                continue;
            }
            if( ENTITYdeclares_variable( e, attr ) ) {
                attr->flags.unique = 1;
            }
        } LISTod;
    } LISTod;
    e->symbol.resolved |= failed;
}

void ENTITYresolve_types( Entity e ) {
    int failed = 0;

    if( print_objects_while_running & OBJ_ENTITY_BITS ) {
        fprintf( stderr, "pass %d: %s (entity)\n", EXPRESSpass,
                 e->symbol.name );
    }

    LISTdo( e->u.entity->attributes, att, Variable ) {
        /* resolve in context of superscope to allow "X : X;" */
        VARresolve_types( att );
        failed |= is_resolve_failed( att->name );
    } LISTod;

    /*
     * resolve the 'unique' list
     */
    ENTITYresolve_uniques( e );

    /* don't wipe out any previous failure stat */
    e->symbol.resolved |= failed;
}

/** resolve all expressions in type definitions */
void TYPEresolve_expressions( Type t, Scope s ) {
    TypeBody body;

    /* meaning of self in a type declaration refers to the type itself, so */
    /* temporary redirect "self" to ourselves to allow this */
    Type self_old = self;
    self = t;

    /* recurse through base types */
    for( ;; t = body->base ) {
        if( t->where ) {
            ( void )WHEREresolve( t->where, s, 1 );
        }

        /* reached an indirect type definition, resolved elsewhere */
        if( t->u.type->head ) {
            break;
        }

        if( !TYPEis_aggregate( t ) ) {
            break;
        }

        body = t->u.type->body;
        if( body->upper ) {
            EXPresolve( body->upper, s, Type_Dont_Care );
        }
        if( body->lower ) {
            EXPresolve( body->lower, s, Type_Dont_Care );
        }
        if( body->precision ) {
            EXPresolve( body->precision, s, Type_Dont_Care );
        }
    }

    self = self_old;
}

int WHEREresolve( Linked_List list, Scope scope, int need_self ) {
    int status = 0;

    LISTdo( list, w, Where )
    /* check if we've been here before */
    /* i'm not sure why, but it happens */
    status |= w->label->resolved;
    if( w->label->resolved & ( RESOLVED | RESOLVE_FAILED ) ) {
        break;
    }

    found_self = false;
    EXPresolve( w->expr, scope, Type_Dont_Care );
    if( need_self && ! found_self ) {
        ERRORreport_with_symbol(MISSING_SELF,
                                 w->label,
                                 w->label->name );
        w->label->resolved = RESOLVE_FAILED;
    } else {
        w->label->resolved = RESOLVED;
    }
    status |= w->label->resolved;
    LISTod
    if( status == RESOLVE_FAILED ) {
        return 0;
    } else {
        return 1;
    }
}

struct tag * TAGcreate_tags() {
    extern int tag_count;

    return( ( struct tag * )calloc( tag_count, sizeof( struct tag ) ) );
}
