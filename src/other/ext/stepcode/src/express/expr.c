

/** **********************************************************************
** Module:  Expression \file expr.c
** This module implements the Expression abstraction.  Several
**  types of expressions are supported: identifiers, literals,
**  operations (arithmetic, logical, array indexing, etc.), and
**  function calls.  Every expression is marked with a type.
** Constants:
**  EXPRESSION_NULL     - the null expression
**  LITERAL_E       - a real literal with the value 2.7182...
**  LITERAL_EMPTY_SET   - a set literal representing the empty set
**  LITERAL_INFINITY    - a numeric literal representing infinity
**  LITERAL_PI      - a real literal with the value 3.1415...
**  LITERAL_ZERO        - an integer literal representing 0
**
************************************************************************/

/*
 * This software was developed by U.S. Government employees as part of
 * their official duties and is not subject to copyright.
 *
 * $Log: expr.c,v $
 * Revision 1.6  1997/01/21 19:19:51  dar
 * made C++ compatible
 *
 * Revision 1.5  1994/11/22  18:32:39  clark
 * Part 11 IS; group reference
 *
 * Revision 1.4  1994/11/10  19:20:03  clark
 * Update to IS
 *
 * Revision 1.3  1994/06/02  14:56:06  libes
 * made plus-like ops check both args
 *
 * Revision 1.2  1993/10/15  18:48:48  libes
 * CADDETC certified
 *
 * Revision 1.9  1993/02/22  21:46:00  libes
 * ANSI compat fixes
 *
 * Revision 1.8  1993/02/16  03:21:31  libes
 * fixed numerous confusions of type with return type
 * fixed implicit loop variable type declarations
 * improved errors
 *
 * Revision 1.7  1993/01/19  22:44:17  libes
 * *** empty log message ***
 *
 * Revision 1.6  1992/09/16  18:20:40  libes
 * made expression resolution routines search through references
 *
 * Revision 1.5  1992/08/18  17:13:43  libes
 * rm'd extraneous error messages
 *
 * Revision 1.4  1992/06/08  18:06:57  libes
 * prettied up interface to print_objects_when_running
 *
 * Revision 1.3  1992/05/31  23:32:26  libes
 * implemented ALIAS resolution
 *
 * Revision 1.2  1992/05/31  08:35:51  libes
 * multiple files
 *
 * Revision 1.1  1992/05/28  03:55:04  libes
 * Initial revision
 *
 * Revision 4.1  90/09/13  15:12:48  clark
 * BPR 2.1 alpha
 *
 */

#include <limits.h>
#include <assert.h>

#include "express/expr.h"
#include "express/resolve.h"


struct EXPop_entry EXPop_table[OP_LAST];

Expression  LITERAL_E = EXPRESSION_NULL;
Expression  LITERAL_INFINITY = EXPRESSION_NULL;
Expression  LITERAL_PI = EXPRESSION_NULL;
Expression  LITERAL_ZERO = EXPRESSION_NULL;
Expression  LITERAL_ONE;

void EXPop_init();

static inline int OPget_number_of_operands(Op_Code op)
{
    if((op == OP_NEGATE) || (op == OP_NOT)) {
        return 1;
    } else if(op == OP_SUBCOMPONENT) {
        return 3;
    } else {
        return 2;
    }
}

/** Description: Initialize the Expression module. */
void EXPinitialize(void)
{
#ifdef does_not_appear_to_be_necessary_or_even_make_sense
    LITERAL_EMPTY_SET = EXPcreate_simple(Type_Set);
    LITERAL_EMPTY_SET->u.list = LISTcreate();
    resolved_all(LITERAL_EMPTY_SET);
#endif

    /* E and PI might come out of math.h */

    LITERAL_E = EXPcreate_simple(Type_Real);
#ifndef M_E
#define M_E     2.7182818284590452354
#endif
    LITERAL_E->u.real = M_E;
    resolved_all(LITERAL_E);

    LITERAL_PI = EXPcreate_simple(Type_Real);
#ifndef M_PI
#define M_PI    3.14159265358979323846
#endif
    LITERAL_PI->u.real = M_PI;
    resolved_all(LITERAL_PI);

    LITERAL_INFINITY = EXPcreate_simple(Type_Integer);
    LITERAL_INFINITY->u.integer = INT_MAX;
    resolved_all(LITERAL_INFINITY);

    LITERAL_ZERO = EXPcreate_simple(Type_Integer);
    LITERAL_ZERO->u.integer = 0;
    resolved_all(LITERAL_ZERO);

    LITERAL_ONE = EXPcreate_simple(Type_Integer);
    LITERAL_ONE->u.integer = 1;
    resolved_all(LITERAL_ONE);

    EXPop_init();
}

void EXPcleanup(void)
{
}

/**
 * \param selection the Type to look in (i.e. an enum)
 * \param sref the Symbol to be found
 * \param e set to the Expression found, when an enum is found
 * \param v set to the Variable found, when a variable is found
 * \param dt set to DICT_type when a match is found (use to determine whether to use e or v)
 * \param where used by ENTITYfind_inherited_attribute, not sure of purpose
 * \param s_id the search id, a parameter to avoid colliding with ENTITYfind...
 * there will be no ambiguities, since we're looking at (and marking)
 * only types, and it's marking only entities
 */
static int EXP_resolve_op_dot_fuzzy(Type selection, Symbol sref, Expression *e,
                                    Variable *v, char *dt, struct Symbol_ **where, int s_id)
{
    Expression item;
    Variable tmp;
    int options = 0;
    struct Symbol_ *w = NULL;

    if(selection->search_id == s_id) {
        return 0;
    }

    switch(selection->u.type->body->type) {
        case entity_:
            /* goes through supertypes and their subtypes (!!) */
            tmp = ENTITYfind_inherited_attribute(selection->u.type->body->entity, sref.name, &w);
            if(tmp) {
                if(w != NULL) {
                    *where = w;
                }
                *v = tmp;
                *dt = DICT_type;
                return 1;
            } else {
                return 0;
            }
        case select_: {
            Linked_List supert = LISTcreate();
            Linked_List subt = LISTcreate();
            Linked_List uniqSubs = LISTcreate();
            selection->search_id = s_id;
            LISTdo(selection->u.type->body->list, t, Type) {
                int nr = EXP_resolve_op_dot_fuzzy(t, sref, e, v, dt, &w, s_id);
                if(nr) {
                    if(w != NULL) {
                        /* only ever set due to ENTITYfind_inherited_attribute in case entity_.
                         * it is set to a subtype of one of the current type's supertypes. not
                         * sure of the circumstances in which this is beneficial.
                         */
                        *where = w;
                        LISTadd_last(subt, w);
                    } else {
                        LISTadd_last(supert, t);
                    }
                    options += nr;
                }
            }
            LISTod
            /* go through supertypes and subtypes, comparing. for any subtypes in supertypes, remove item from subtypes
             * would be possible to delete items from subt while going through the list... worth the effort?
             */
            LISTdo(subt, s, Symbol *) {
                bool found = false;
                LISTdo_n(supert, t, Type, b) {
                    if(0 == strcmp(s->name, t->symbol.name)) {
                        found = true;
                        break;
                    }
                }
                LISTod
                if(!found) {
                    LISTadd_last(uniqSubs, s);
                }
            }
            LISTod
            if((LISTget_length(uniqSubs) == 0) && (LISTget_length(supert) == 1) && (options > 1)) {
                options = 1;
                /* this ensures that v is set correctly and wasn't overwritten */
                EXP_resolve_op_dot_fuzzy((Type) LISTget_first(supert), sref, e, v, dt, &w, s_id);
            }
            if(options > 1) {
                /* found more than one, so ambiguous */
                *v = VARIABLE_NULL;
            }
            LISTfree(supert);
            LISTfree(subt);
            LISTfree(uniqSubs);
            return options;
        }
        case enumeration_:
            item = (Expression)DICTlookup(TYPEget_enum_tags(selection), sref.name);
            if(item) {
                *e = item;
                *dt = DICT_type;
                return 1;
            } else {
                return 0;
            }
        default:
            return 0;
    }
}

Type EXPresolve_op_dot(Expression expr, Scope scope)
{
    Expression op1 = expr->e.op1;
    Expression op2 = expr->e.op2;
    Variable v = 0;
    Expression item;
    Type op1type;
    bool all_enums = true; /* used by 'case select_' */

    /* stuff for dealing with select_ */
    int options = 0;
    char dt;
    struct Symbol_ *where = NULL;

    /* op1 is entity expression, op2 is attribute */
    /* could be very impossible to determine except */
    /* at run-time, .... */
    EXPresolve(op1, scope, Type_Dont_Care);
    if(is_resolve_failed(op1)) {
        resolve_failed(expr);
        return(Type_Bad);
    }
    op1type = op1->return_type;

    switch(op1type->u.type->body->type) {
        case generic_:
        case runtime_:
            /* defer */
            return(Type_Runtime);
        case select_:
            __SCOPE_search_id++;
            /* don't think this actually actually catches anything on the first go-round, but let's be consistent */
            op1type->search_id = __SCOPE_search_id;
            LISTdo(op1type->u.type->body->list, t, Type) {
                /* this used to increment options by 1 if EXP_resolve_op_dot_fuzzy found 1 or more possibilities.
                 * thus the code for handling ambiguities was only used if the ambig was in the immediate type
                 * and not a supertype. don't think that's right...
                 */
                options += EXP_resolve_op_dot_fuzzy(t, op2->symbol, &item, &v, &dt, &where, __SCOPE_search_id);
            }
            LISTod;
            switch(options) {
                case 0:
                    LISTdo(op1type->u.type->body->list, t, Type) {
                        if(t->u.type->body->type != enumeration_) {
                            all_enums = false;
                        }
                    }
                    LISTod;

                    if(all_enums) {
                        ERRORreport_with_symbol(CASE_SKIP_LABEL, &op2->symbol, op2->symbol.name);
                    } else {
                        /* no possible resolutions */
                        ERRORreport_with_symbol(UNDEFINED_ATTR, &op2->symbol, op2->symbol.name);
                    }
                    resolve_failed(expr);
                    return(Type_Bad);
                case 1:
                    /* only one possible resolution */
                    if(dt == OBJ_VARIABLE) {
                        if(where) {
                            ERRORreport_with_symbol(IMPLICIT_DOWNCAST, &op2->symbol, where->name);
                        }

                        if(v == VARIABLE_NULL) {
                            fprintf(stderr, "EXPresolve_op_dot: nonsense value for Variable\n");
                            ERRORabort(0);
                        }
                        op2->u.variable = v;
                        op2->return_type = v->type;
                        resolved_all(expr);
                        return(v->type);
                    } else if(dt == OBJ_ENUM) {
                        op2->u.expression = item;
                        op2->return_type = item->type;
                        resolved_all(expr);
                        return(item->type);
                    } else {
                        fprintf(stderr, "EXPresolved_op_dot: attribute not an attribute?\n");
                        ERRORabort(0);
                        return(Type_Bad);
                    }
                default:
                    /* compile-time ambiguous */
                    if(where) {
                        /* this is actually a warning, not an error */
                        ERRORreport_with_symbol(AMBIG_IMPLICIT_DOWNCAST, &op2->symbol, where->name);
                    }
                    return(Type_Runtime);
            }
        case attribute_:
            v = ENTITYresolve_attr_ref(op1->u.variable->type->u.type->body->entity, (struct Symbol_ *)0, &op2->symbol);

            if(!v) {
                /*      reported by ENTITYresolve_attr_ref */
                /*      ERRORreport_with_symbol(ERROR_undefined_attribute,*/
                /*              &expr->symbol,op2->symbol.name);*/
                resolve_failed(expr);
                return(Type_Bad);
            }
            if(DICT_type != OBJ_VARIABLE) {
                fprintf(stderr, "EXPresolved_op_dot: attribute not an attribute?\n");
                ERRORabort(0);
            }

            op2->u.variable = v;
            op2->return_type = v->type;
            resolved_all(expr);
            return(v->type);
        case entity_:
        case op_:   /* (op1).op2 */
            v = ENTITYresolve_attr_ref(op1type->u.type->body->entity,
                                       (struct Symbol_ *)0, &op2->symbol);
            if(!v) {
                /*      reported by ENTITYresolve_attr_ref */
                /*      ERRORreport_with_symbol(ERROR_undefined_attribute,*/
                /*              &expr->symbol,op2->symbol.name);*/
                resolve_failed(expr);
                return(Type_Bad);
            }
            if(DICT_type != OBJ_VARIABLE) {
                fprintf(stderr, "ERROR: EXPresolved_op_dot: attribute not an attribute?\n");
            }

            op2->u.variable = v;
            /* changed to set return_type */
            op2->return_type = op2->u.variable->type;
            resolved_all(expr);
            return(op2->return_type);
        case enumeration_:
            /* enumerations within a select will be handled by `case select_` above,
             * which calls EXP_resolve_op_dot_fuzzy(). */
            item = (Expression)DICTlookup(TYPEget_enum_tags(op1type), op2->symbol.name);
            if(!item) {
                ERRORreport_with_symbol(ENUM_NO_SUCH_ITEM, &op2->symbol,
                                        op1type->symbol.name, op2->symbol.name);
                resolve_failed(expr);
                return(Type_Bad);
            }

            op2->u.expression = item;
            op2->return_type = item->type;
            resolved_all(expr);
            return(item->type);
        case aggregate_:
        case array_:
        case bag_:
        case list_:
        case set_:
            ERRORreport_with_symbol(ATTRIBUTE_REF_ON_AGGREGATE,
                                    &op2->symbol, op2->symbol.name);
        /*FALLTHRU*/
        case unknown_:  /* unable to resolved operand */
            /* presumably error has already been reported */
            resolve_failed(expr);
            return(Type_Bad);
        default:
            ERRORreport_with_symbol(ATTRIBUTE_REF_FROM_NON_ENTITY,
                                    &op2->symbol, op2->symbol.name);
            resolve_failed(expr);
            return(Type_Bad);
    }
}

/**
 * \param s_id the search id, a parameter to avoid colliding with ENTITYfind...
 * there will be no ambiguities, since we're looking at (and marking)
 * only types, and it's marking only entities
 */
static int EXP_resolve_op_group_fuzzy(Type selection, Symbol sref, Entity *e,
                                      int s_id)
{
    Entity tmp;
    int options = 0;

    if(selection->search_id == s_id) {
        return 0;
    }

    switch(selection->u.type->body->type) {
        case entity_:
            tmp = (Entity)ENTITYfind_inherited_entity(
                      selection->u.type->body->entity, sref.name, 1);
            if(tmp) {
                *e = tmp;
                return 1;
            }

            return 0;
        case select_:
            tmp = *e;
            selection->search_id = s_id;
            LISTdo(selection->u.type->body->list, t, Type)
            if(EXP_resolve_op_group_fuzzy(t, sref, e, s_id)) {
                if(*e != tmp) {
                    tmp = *e;
                    ++options;
                }
            }
            LISTod;

            switch(options) {
                case 0:
                    return 0;
                case 1:
                    return 1;
                default:
                    /* found more than one, so ambiguous */
                    *e = ENTITY_NULL;
                    return 1;
            }
        default:
            return 0;
    }
}

Type EXPresolve_op_group(Expression expr, Scope scope)
{
    Expression op1 = expr->e.op1;
    Expression op2 = expr->e.op2;
    Entity ent_ref = ENTITY_NULL;
    Entity tmp = ENTITY_NULL;
    Type op1type;

    /* stuff for dealing with select_ */
    int options = 0;

    /* op1 is entity expression, op2 is entity */
    /* could be very impossible to determine except */
    /* at run-time, .... */
    EXPresolve(op1, scope, Type_Dont_Care);
    if(is_resolve_failed(op1)) {
        resolve_failed(expr);
        return(Type_Bad);
    }
    op1type = op1->return_type;

    switch(op1type->u.type->body->type) {
        case generic_:
        case runtime_:
        case op_:
            /* All these cases are very painful to do right */
            /* "Generic" and sometimes others require runtime evaluation */
            op2->return_type = Type_Runtime;
            return(Type_Runtime);
        case self_:
        case entity_:
            /* Get entity denoted by "X\" */
            tmp = ((op1type->u.type->body->type == self_)
                   ? scope
                   : op1type->u.type->body->entity);

            /* Now get entity denoted by "X\Y" */
            ent_ref =
                (Entity)ENTITYfind_inherited_entity(tmp, op2->symbol.name, 1);
            if(!ent_ref) {
                ERRORreport_with_symbol(GROUP_REF_NO_SUCH_ENTITY,
                                        &op2->symbol, op2->symbol.name);
                resolve_failed(expr);
                return(Type_Bad);
            }

            op2->u.entity = ent_ref;
            op2->return_type = ent_ref->u.entity->type;
            resolved_all(expr);
            return(op2->return_type);
        case select_:
            __SCOPE_search_id++;
            /* don't think this actually actually catches anything on the */
            /* first go-round, but let's be consistent */
            op1type->search_id = __SCOPE_search_id;
            LISTdo(op1type->u.type->body->list, t, Type)
            if(EXP_resolve_op_group_fuzzy(t, op2->symbol, &ent_ref,
                                          __SCOPE_search_id)) {
                if(ent_ref != tmp) {
                    tmp = ent_ref;
                    ++options;
                }
            }
            LISTod;

            switch(options) {
                case 0:
                    /* no possible resolutions */
                    ERRORreport_with_symbol(GROUP_REF_NO_SUCH_ENTITY,
                                            &op2->symbol, op2->symbol.name);
                    resolve_failed(expr);
                    return(Type_Bad);
                case 1:
                    /* only one possible resolution */
                    op2->u.entity = ent_ref;
                    op2->return_type = ent_ref->u.entity->type;
                    resolved_all(expr);
                    return(op2->return_type);
                default:
                    /* compile-time ambiguous */
                    /*      ERRORreport_with_symbol(ERROR_ambiguous_group,*/
                    /*                  &op2->symbol, op2->symbol.name);*/
                    return(Type_Runtime);
            }
        case array_:
            if(op1->type->u.type->body->type == self_) {
                return(Type_Runtime);   /* not sure if there are other cases where Type_Runtime should be returned, or not */
            } /*  else fallthrough */
        case unknown_:  /* unable to resolve operand */
            /* presumably error has already been reported */
            resolve_failed(expr);
            return(Type_Bad);
        case aggregate_:

        case bag_:
        case list_:
        case set_:
        default:
            ERRORreport_with_symbol(GROUP_REF_UNEXPECTED_TYPE,
                                    &op1->symbol);
            return(Type_Bad);
    }
}

Type EXPresolve_op_relational(Expression e, Scope s)
{
    Type t = 0;
    int failed = 0;
    Type op1type;

    /* Prevent op1 from complaining if it fails */

    EXPresolve(e->e.op1, s, Type_Unknown);
    failed = is_resolve_failed(e->e.op1);
    op1type = e->e.op1->return_type;

    /* now, either op1 was resolved in which case, we use its return type */
    /* for typechecking, OR, it wasn't resolved in which case we resolve */
    /* op2 in such a way that it complains if it fails to resolved */

    if(op1type == Type_Unknown) {
        t = Type_Dont_Care;
    } else {
        t = op1type;
    }

    EXPresolve(e->e.op2, s, t);
    if(is_resolve_failed(e->e.op2)) {
        failed = 1;
    }

    /* If op1 wasn't successfully resolved, retry it now with new information */

    if((failed == 0) && !is_resolved(e->e.op1)) {
        EXPresolve(e->e.op1, s, e->e.op2->return_type);
        if(is_resolve_failed(e->e.op1)) {
            failed = 1;
        }
    }

    if(failed) {
        resolve_failed(e);
    } else {
        resolved_all(e);
    }
    return(Type_Logical);
}

void EXPresolve_op_default(Expression e, Scope s)
{
    int failed = 0;

    if(OPget_number_of_operands(e->e.op_code) == 3) {
        EXPresolve(e->e.op3, s, Type_Dont_Care);
        failed = is_resolve_failed(e->e.op3);
    }
    if(OPget_number_of_operands(e->e.op_code) == 2) {
        EXPresolve(e->e.op2, s, Type_Dont_Care);
        failed |= is_resolve_failed(e->e.op2);
    }
    EXPresolve(e->e.op1, s, Type_Dont_Care);
    if(failed || is_resolve_failed(e->e.op1)) {
        resolve_failed(e);
    } else {
        resolved_all(e);
    }
}

/* prototype for this func cannot change - it is passed as a fn pointer */
Type EXPresolve_op_unknown(Expression e, Scope s)
{
    (void) e; /* quell unused param warning */
    (void) s;
    ERRORreport(INTERNAL_UNRECOGNISED_OP_IN_EXPRESOLVE);
    return Type_Bad;
}

typedef Type(Resolve_expr_func)(Expression, Scope);

Type EXPresolve_op_logical(Expression e, Scope s)
{
    EXPresolve_op_default(e, s);
    return(Type_Logical);
}
Type EXPresolve_op_array_like(Expression e, Scope s)
{

    Type op1type;
    EXPresolve_op_default(e, s);
    op1type = e->e.op1->return_type;

    if(TYPEis_aggregate(op1type)) {
        return(op1type->u.type->body->base);
    } else if(TYPEis_string(op1type)) {
        return(op1type);
    } else if(op1type == Type_Runtime) {
        return(Type_Runtime);
    } else if(op1type->u.type->body->type == binary_) {
        ERRORreport_with_symbol(WARN_UNSUPPORTED_LANG_FEAT, &e->symbol, "indexing on a BINARY", __FILE__, __LINE__);
        return(Type_Binary);
    } else if(op1type->u.type->body->type == generic_) {
        return(Type_Generic);
    } else if(TYPEis_select(op1type)) {
        int numAggr = 0, numNonAggr = 0;
        bool sameAggrType = true;
        Type lasttype = 0;

        /* FIXME Is it possible that the base type hasn't yet been resolved?
         * If it is possible, we should signal that we need to come back later... but how? */
        assert(op1type->symbol.resolved == 1);

        /* FIXME We should check for a not...or excluding non-aggregate types in the select, such as
         * WR1: NOT('INDEX_ATTRIBUTE.COMMON_DATUM_LIST' IN TYPEOF(base)) OR (SELF\shape_aspect.of_shape = base[1]\shape_aspect.of_shape);
         * (how?)
         */

        /* count aggregates and non-aggregates, check aggregate types */
        LISTdo(op1type->u.type->body->list, item, Type) {
            if(TYPEis_aggregate(item)) {
                numAggr++;
                if(lasttype == TYPE_NULL) {
                    lasttype = item;
                } else {
                    if(lasttype->u.type->body->type != item->u.type->body->type) {
                        sameAggrType = false;
                    }
                }
            } else {
                numNonAggr++;
            }
        }
        LISTod;

        /* NOTE the following code returns the same data for every case that isn't an error.
         * It needs to be simplified or extended, depending on whether it works or not. */
        if(sameAggrType && (numAggr != 0) && (numNonAggr == 0)) {
            /*  All are the same aggregation type */
            return(lasttype->u.type->body->base);
        } else if(numNonAggr == 0) {
            /*  All aggregates, but different types */
            ERRORreport_with_symbol(WARN_INDEXING_MIXED, &e->symbol, op1type->symbol.name);
            return(lasttype->u.type->body->base);   /*  WARNING I'm assuming that any of the types is acceptable!!! */
        } else if(numAggr != 0) {
            /*  One or more aggregates, one or more nonaggregates */
            ERRORreport_with_symbol(WARN_INDEXING_MIXED, &e->symbol, op1type->symbol.name);
            return(lasttype->u.type->body->base);   /*  WARNING I'm assuming that any of the types is acceptable!!! */
        }   /*  Else, all are nonaggregates. This is an error. */
    }
    ERRORreport_with_symbol(INDEXING_ILLEGAL, &e->symbol);
    return(Type_Unknown);
}

Type EXPresolve_op_entity_constructor(Expression e, Scope s)
{
    EXPresolve_op_default(e, s);
    /* perhaps should return Type_Runtime? */
    return Type_Entity;
}

Type EXPresolve_op_int_div_like(Expression e, Scope s)
{
    EXPresolve_op_default(e, s);
    return Type_Integer;
}

Type EXPresolve_op_plus_like(Expression e, Scope s)
{
    /* i.e., Integer or Real */
    EXPresolve_op_default(e, s);
    if(is_resolve_failed(e)) {
        resolve_failed(e);
        return(Type_Unknown);
    }

    /* could produce better results with a lot of pain but the EXPRESS */
    /* spec is a little confused so what's the point.  For example */
    /* it says bag+set=bag */
    /*     and set+bag=set */
    /*     and set+list=set */
    /*     and list+set=? */

    /* crude but sufficient */
    if((TYPEis_aggregate(e->e.op1->return_type)) ||
            (TYPEis_aggregate(e->e.op2->return_type))) {
        return Type_Aggregate;
    }

    /* crude but sufficient */
    if((e->e.op1->return_type->u.type->body->type == real_) ||
            (e->e.op2->return_type->u.type->body->type == real_)) {
        return(Type_Real);
    }
    return Type_Integer;
}

Type EXPresolve_op_unary_minus(Expression e, Scope s)
{
    EXPresolve_op_default(e, s);
    return e->e.op1->return_type;
}

/** Initialize one entry in EXPop_table
 * This table's function pointers are resolved in EXP_resolve(), at approx resolve.c:520
 * \sa EXP_resolve()
 * \sa EXPop_init()
 *
 * \param token_number operator value, usually in macro form
 * \param string human-readable description
 * \param resolve_func   resolves an expression of this type
 */
void EXPop_create(int token_number, char *string, Resolve_expr_func *resolve_func)
{
    EXPop_table[token_number].token = string;
    EXPop_table[token_number].resolve = resolve_func;
}

void EXPop_init()
{
    EXPop_create(OP_AND, "AND",      EXPresolve_op_logical);
    EXPop_create(OP_ANDOR, "ANDOR",      EXPresolve_op_logical);
    EXPop_create(OP_ARRAY_ELEMENT, "[array element]", EXPresolve_op_array_like);
    EXPop_create(OP_CONCAT, "||",        EXPresolve_op_entity_constructor);
    EXPop_create(OP_DIV, "/ (INTEGER)",  EXPresolve_op_int_div_like);
    EXPop_create(OP_DOT, ".",        EXPresolve_op_dot);
    EXPop_create(OP_EQUAL, "=",      EXPresolve_op_relational);
    EXPop_create(OP_EXP, "**",       EXPresolve_op_plus_like);
    EXPop_create(OP_GREATER_EQUAL, ">=", EXPresolve_op_relational);
    EXPop_create(OP_GREATER_THAN, ">",   EXPresolve_op_relational);
    EXPop_create(OP_GROUP, "\\",     EXPresolve_op_group);
    EXPop_create(OP_IN, "IN",        EXPresolve_op_relational);
    EXPop_create(OP_INST_EQUAL, ":=:",   EXPresolve_op_relational);
    EXPop_create(OP_INST_NOT_EQUAL, ":<>:",  EXPresolve_op_relational);
    EXPop_create(OP_LESS_EQUAL, "<=",    EXPresolve_op_relational);
    EXPop_create(OP_LESS_THAN, "<",      EXPresolve_op_relational);
    EXPop_create(OP_LIKE, "LIKE",        EXPresolve_op_relational);
    EXPop_create(OP_MINUS, "- (MINUS)",  EXPresolve_op_plus_like);
    EXPop_create(OP_MOD, "MOD",      EXPresolve_op_int_div_like);
    EXPop_create(OP_NEGATE, "- (NEGATE)",    EXPresolve_op_unary_minus);
    EXPop_create(OP_NOT, "NOT",      EXPresolve_op_logical);
    EXPop_create(OP_NOT_EQUAL, "<>",     EXPresolve_op_relational);
    EXPop_create(OP_OR, "OR",        EXPresolve_op_logical);
    EXPop_create(OP_PLUS, "+",       EXPresolve_op_plus_like);
    EXPop_create(OP_REAL_DIV, "/ (REAL)",    EXPresolve_op_plus_like);
    EXPop_create(OP_SUBCOMPONENT, "[:]", EXPresolve_op_array_like);
    EXPop_create(OP_TIMES, "*",      EXPresolve_op_plus_like);
    EXPop_create(OP_XOR, "XOR",      EXPresolve_op_logical);
    EXPop_create(OP_UNKNOWN, "UNKNOWN OP",   EXPresolve_op_unknown);
}

/**
** \param expression  expression to evaluate
** \param experrc buffer for error code
** \returns value of expression
** Compute the value of an integer expression.
*/
int EXPget_integer_value(Expression expression)
{
    /* TODO: why is this treated differently than a type error below? */
    if(expression == EXPRESSION_NULL) {
        return 0;
    }
    if(expression->return_type->u.type->body->type == integer_) {
        return INT_LITget_value(expression);
    } else {
        ERRORreport(INTEGER_EXPRESSION_EXPECTED);
        return 0;
    }
}

char *opcode_print(Op_Code o)
{
    switch(o) {
        case OP_AND:
            return("OP_AND");
        case OP_ANDOR:
            return("OP_ANDOR");
        case OP_ARRAY_ELEMENT:
            return("OP_ARRAY_ELEMENT");
        case OP_CONCAT:
            return("OP_CONCAT");
        case OP_DIV:
            return("OP_DIV");
        case OP_DOT:
            return("OP_DOT");
        case OP_EQUAL:
            return("OP_EQUAL");
        case OP_EXP:
            return("OP_EXP");
        case OP_GREATER_EQUAL:
            return("OP_GREATER_EQUAL");
        case OP_GREATER_THAN:
            return("OP_GREATER_THAN");
        case OP_GROUP:
            return("OP_GROUP");
        case OP_IN:
            return("OP_IN");
        case OP_INST_EQUAL:
            return("OP_INST_EQUAL");
        case OP_INST_NOT_EQUAL:
            return("OP_INST_NOT_EQUAL");
        case OP_LESS_EQUAL:
            return("OP_LESS_EQUAL");
        case OP_LESS_THAN:
            return("OP_LESS_THAN");
        case OP_LIKE:
            return("OP_LIKE");
        case OP_MINUS:
            return("OP_MINUS");
        case OP_MOD:
            return("OP_MOD");
        case OP_NEGATE:
            return("OP_NEGATE");
        case OP_NOT:
            return("OP_NOT");
        case OP_NOT_EQUAL:
            return("OP_NOT_EQUAL");
        case OP_OR:
            return("OP_OR");
        case OP_PLUS:
            return("OP_PLUS");
        case OP_REAL_DIV:
            return("OP_REAL_DIV");
        case OP_SUBCOMPONENT:
            return("OP_SUBCOMPONENT");
        case OP_TIMES:
            return("OP_TIMES");
        case OP_XOR:
            return("OP_XOR");
        case OP_UNKNOWN:
            return("OP_UNKNOWN");
        default:
            return("no such op");
    }
}
