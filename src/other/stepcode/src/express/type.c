

/** **********************************************************************
** Module:  Type \file type.c
This module implements the type abstraction.  It is
**  rather unpleasant, since this abstraction is quite well suited
**  to an object-oriented environment with inheritance.
** Constants:
**  TYPE_AGGREGATE      - generic general aggregate
**  TYPE_BINARY         - binary type
**  TYPE_BOOLEAN        - boolean type
**  TYPE_GENERIC        - generic type
**  TYPE_INTEGER        - integer type with default precision
**  TYPE_LOGICAL        - logical type
**  TYPE_NULL           - the null type
**  TYPE_NUMBER         - number type
**  TYPE_REAL           - real type with default precision
**  TYPE_SET_OF_GENERIC - type for unconstrained set of generic items
**  TYPE_STRING         - string type with default precision
**
************************************************************************/

/*
 * This code was developed with the support of the United States Government,
 * and is not subject to copyright.
 *
 * $Log: type.c,v $
 * Revision 1.12  1997/01/21 19:19:51  dar
 * made C++ compatible
 *
 * Revision 1.11  1995/03/09  18:44:45  clark
 * various fixes for caddetc - before interface clause changes
 *
 * Revision 1.10  1994/11/10  19:20:03  clark
 * Update to IS
 *
 * Revision 1.9  1994/05/11  19:51:24  libes
 * numerous fixes
 *
 * Revision 1.8  1993/10/15  18:48:48  libes
 * CADDETC certified
 *
 * Revision 1.6  1993/01/19  22:16:09  libes
 * *** empty log message ***
 *
 * Revision 1.5  1992/09/16  18:23:45  libes
 * added some functions for easier access to base types
 *
 * Revision 1.4  1992/08/18  17:13:43  libes
 * rm'd extraneous error messages
 *
 * Revision 1.3  1992/06/08  18:06:57  libes
 * prettied up interface to print_objects_when_running
 *
 * Revision 1.2  1992/05/31  08:35:51  libes
 * multiple files
 *
 * Revision 1.1  1992/05/28  03:55:04  libes
 * Initial revision
 *
 * Revision 1.9  1992/05/14  10:14:06  libes
 * don't remember
 *
 * Revision 1.8  1992/05/10  06:03:18  libes
 * cleaned up OBJget_symbol
 *
 * Revision 1.7  1992/05/10  01:42:27  libes
 * does enums and repeat properly
 *
 * Revision 1.6  1992/05/05  19:52:11  libes
 * final alpha
 *
 * Revision 1.5  1992/02/19  15:48:46  libes
 * changed types to enums & flags
 *
 * Revision 1.4  1992/02/17  14:33:41  libes
 * lazy ref/use evaluation now working
 *
 * Revision 1.3  1992/02/12  07:05:45  libes
 * do sub/supertype
 *
 * Revision 1.2  1992/02/09  00:50:20  libes
 * does ref/use correctly
 *
 * Revision 1.1  1992/02/05  08:34:24  libes
 * Initial revision
 *
 * Revision 1.0.1.1  1992/01/22  02:47:57  libes
 * copied from ~pdes
 *
 * Revision 4.8  1991/11/14  07:37:50  libes
 * added TYPEget/put_original_type
 *
 * Revision 4.7  1991/09/16  23:13:12  libes
 * added print functions
 *
 * Revision 4.6  1991/06/14  20:48:16  libes
 * added binary type
 *
 * Revision 4.5  1991/01/24  22:20:36  silver
 * merged changes from NIST and SCRA
 * SCRA changes are due to DEC ANSI C compiler tests.
 *
 * Revision 4.4  91/01/08  18:56:05  pdesadmn
 * Initial - Beta checkin at SCRA
 *
 * Revision 4.3  90/09/14  16:02:28  clark
 * Initial checkin at SCRA
 *
 * Revision 4.3  90/09/14  16:02:28  clark
 * initial checkin at SCRA
 *
 * Revision 4.3  90/09/14  16:02:28  clark
 * Reintroduce ENT_TYPEget_entity
 *
 * Revision 4.2  90/09/14  09:33:20  clark
 * Add Class_{Boolean,Generic,Logical,Number}_Type
 * Fix TYPE_equal
 *
 * Revision 4.1  90/09/13  15:13:21  clark
 * BPR 2.1 alpha
 *
 */

#include <assert.h>

#include "express/type.h"

Type TYPEcreate_user_defined_tag(Type base, Scope scope, struct Symbol_ *symbol)
{
    Type t;
    extern int tag_count;

    t = (Type)DICTlookup(scope->symbol_table, symbol->name);
    if(t) {
        if(DICT_type == OBJ_TAG) {
            return(t);
        } else {
            /* easiest to just generate the error this way!
             * following call WILL fail intentionally
             */
            DICTdefine(scope->symbol_table, symbol->name, 0, symbol, OBJ_TAG);
            return(0);
        }
    }

    /* tag is undefined
     * if we are outside a formal parameter list (hack, hack)
     * then we can only refer to existing tags, so produce an error
     */
    if(tag_count < 0) {
        ERRORreport_with_symbol(UNDEFINED_TAG, symbol,
                                symbol->name);
        return(0);
    }

    /* otherwise, we're in a formal parameter list,
     * so it's ok to define it
     */
    t = TYPEcreate_nostab(symbol, scope, OBJ_TAG);
    t->u.type->head = base;

    /* count unique type tags inside PROC and FUNC headers */
    tag_count++;

    return(t);
}

/**
 * return true if "type t" inherits from "enum type_enum"
 * may need to be implemented for more types
 */
#define TYPE_inherits_from(t,e) ((t) && TYPEinherits_from((t),(e)))

bool TYPEinherits_from(Type t, enum type_enum e)
{
    TypeBody tb = t->u.type->body;
    assert((t->type == OBJ_TYPE) && (tb) && "Not a Type!");
    switch(e) {
        case aggregate_:
            if(tb->type == aggregate_ ||
                    tb->type == array_ ||
                    tb->type == bag_ ||
                    tb->type == set_ ||
                    tb->type == list_) {
                return true;
            } else {
                return(TYPE_inherits_from(tb->base, e));
            }
        case array_:
            return((tb->type == array_) ? true : TYPE_inherits_from(tb->base, e));
        case bag_:
            return((tb->type == bag_ ||
                    tb->type == set_) ? true : TYPE_inherits_from(tb->base, e));
        case set_:
            return((tb->type == set_) ? true : TYPE_inherits_from(tb->base, e));
        case list_:
            return((tb->type == list_) ? true : TYPE_inherits_from(tb->base, e));
        default:
            break;
    }
    return (tb->type == e);
}

#if 0
case binary_:
return((t->type == binary_) ? true : TYPEinherits_from(t->base, e));
case integer_:
return((t->type == integer_) ? true : TYPEinherits_from(t->base, e));
case real_:
return((t->type == real_) ? true : TYPEinherits_from(t->base, e));
case string_:
return((t->type == string_) ? true : TYPEinherits_from(t->base, e));
case logical_:
return((t->type == logical_) ? true : TYPEinherits_from(t->base, e));
case boolean_:
return((t->type == boolean_) ? true : TYPEinherits_from(t->base, e));
default:
return(false);
}
}
#endif

/** Initialize the Type module */
void TYPEinitialize()
{
}

/** Clean up the Type module */
void TYPEcleanup(void)
{
}

/**
 * \param t type to examine
 * \return the base type of the aggregate type
 * Retrieve the base type of an aggregate.
 */
Type TYPEget_nonaggregate_base_type(Type t)
{
    while(TYPEis_aggregate(t)) {
        t = t->u.type->body->base;
    }
    return t;
}

