

/** **********************************************************************
** Module:  Variable \file variable.c
** This module implements the Variable abstraction.  A
**  Variable consists of a name, a type, a reference class, and
**  some flags, e.g. 'optional', 'variable'.  It is used to represent
**  variable attributes, variables, and formal parameters.
** Constants:
**  VARIABLE_NULL   - the null variable
**
************************************************************************/

/*
 * This code was developed with the support of the United States Government,
 * and is not subject to copyright.
 *
 * $Log: variable.c,v $
 * Revision 1.7  1997/01/21 19:19:51  dar
 * made C++ compatible
 *
 * Revision 1.6  1994/05/11  19:51:24  libes
 * numerous fixes
 *
 * Revision 1.5  1993/10/15  18:48:48  libes
 * CADDETC certified
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
 * Revision 1.5  1992/05/10  06:03:51  libes
 * cleaned up OBJget_symbol
 *
 * Revision 1.4  1992/02/17  14:34:01  libes
 * lazy ref/use evaluation now working
 *
 * Revision 1.3  1992/02/12  07:06:10  libes
 * do sub/supertype
 *
 * Revision 1.2  1992/02/09  00:50:45  libes
 * does ref/use correctly
 *
 * Revision 1.1  1992/02/05  08:34:24  libes
 * Initial revision
 *
 * Revision 1.0.1.1  1992/01/22  02:47:57  libes
 * copied from ~pdes
 *
 * Revision 4.6  1991/09/16  23:13:12  libes
 * added print functionsalgorithm.c
 *
 * Revision 4.5  1991/07/21  05:22:17  libes
 * added VARget and put_inverse, initialized private ->inverse
 *
 * Revision 4.4  1991/06/14  20:42:02  libes
 * Remove reference_class from variable, and added support for true name
 * of variable in another schema to which variable corresponds.
 *
 * Revision 4.3  1991/01/24  22:20:36  silver
 * merged changes from NIST and SCRA
 * SCRA changes are due to DEC ANSI C compiler tests.
 *
 * Revision 4.2  91/01/08  18:56:07  pdesadmn
 * Initial - Beta checkin at SCRA
 *
 * Revision 4.1  90/09/06  11:40:50  clark
 * Initial checkin at SCRA
 *
 * Revision 4.1  90/09/06  11:40:50  clark
 * initial checkin at SCRA
 *
 * Revision 4.1  90/09/06  11:40:50  clark
 * BPR 2.1 alpha
 *
 */

#include <sc_memmgr.h>
#include <stdlib.h>
#include "express/variable.h"
#include "express/object.h"
char * opcode_print( Op_Code o );

struct freelist_head VAR_fl;

Symbol * VAR_get_symbol( Generic v ) {
    return( &( ( Variable )v )->name->symbol );
}

/** Initialize the Variable module. */
void VARinitialize() {
    MEMinitialize( &VAR_fl, sizeof( struct Variable_ ), 100, 50 );
    /*  OBJcreate(OBJ_VARIABLE,VAR_get_symbol,"variable",OBJ_UNUSED_BITS);*/
    OBJcreate( OBJ_VARIABLE, VAR_get_symbol, "variable", OBJ_VARIABLE_BITS );
}

/** VARget_simple_name
 * returns simple name of variable
 * for example, if var is named SELF\xxx.yyy, return yyy
 */
extern char * VARget_simple_name( Variable v ) {
    Expression e = VARget_name( v );

    while( TYPEis_expression( EXPget_type( e ) ) ) {
        switch( e->e.op_code ) {
            case OP_DOT:
            case OP_GROUP:
                e = e->e.op2;
                break;
            default:
                fprintf( stderr, "unexpected op_code (%s) encountered in variable name expression\n", opcode_print( e->e.op_code ) );
                abort();
        }
    }
    return EXPget_name( e );
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
