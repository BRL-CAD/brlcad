/** \file pretty_loop.c
 * split out of exppp.c 9/21/13
 */

#include <express/variable.h>
#include <express/dict.h>
#include <exppp/exppp.h>

#include "pp.h"
#include "pretty_expr.h"
#include "pretty_stmt.h"
#include "pretty_loop.h"

void LOOPout( struct Loop_ *loop, int level ) {
    Variable v;

    raw( "%*sREPEAT", level, "" );

    /* increment */
    /*  if (loop->scope->u.incr) {*/
    if( loop->scope ) {
        DictionaryEntry de;

        DICTdo_init( loop->scope->symbol_table, &de );
        v = ( Variable )DICTdo( &de );
        wrap( " %s := ", v->name->symbol.name );
        EXPR_out( loop->scope->u.incr->init, 0 );
        wrap( " TO " );
        EXPR_out( loop->scope->u.incr->end, 0 );
        wrap( " BY " ); /* parser always forces a "by" expr */
        EXPR_out( loop->scope->u.incr->increment, 0 );
    }

    /* while */
    if( loop->while_expr ) {
        wrap( " WHILE " );
        EXPR_out( loop->while_expr, 0 );
    }

    /* until */
    if( loop->until_expr ) {
        wrap( " UNTIL " );
        EXPR_out( loop->until_expr, 0 );
    }

    raw( ";\n" );

    STMTlist_out( loop->statements, level + exppp_nesting_indent );

    raw( "%*sEND_REPEAT;\n", level, "" );
}
