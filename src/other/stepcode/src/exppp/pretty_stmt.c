/** \file pretty_stmt.c
 * split out of exppp.c 9/21/13
 */

#include <exppp/exppp.h>
#include "pp.h"
#include "pretty_expr.h"
#include "pretty_case.h"
#include "pretty_loop.h"
#include "pretty_stmt.h"

void STMT_out( Statement s, int level ) {
    bool first_time = true;

    if( !s ) {  /* null statement */
        raw( "%*s;\n", level, "" );
        return;
    }

    indent2 = level + exppp_continuation_indent;

    switch( s->type ) {
        case STMT_ASSIGN:
            raw( "%*s", level, "" );
            EXPR_out( s->u.assign->lhs, 0 );
            wrap( " := " );
            EXPR_out( s->u.assign->rhs, 0 );
            raw( ";\n", level, "" );
            break;
        case STMT_CASE:
            CASEout( s->u.Case, level );
            break;
        case STMT_COMPOUND:
            raw( "\n%*sBEGIN\n", level, "" );
            STMTlist_out( s->u.compound->statements, level + exppp_nesting_indent );
            raw( "%*sEND;\n", level, "" );
            break;
        case STMT_COND:
            raw( "%*sIF ", level, "" );
            EXPR_out( s->u.cond->test, 0 );
            wrap( " THEN\n" );
            STMTlist_out( s->u.cond->code, level + exppp_nesting_indent );
            if( s->u.cond->otherwise ) {
                raw( "%*sELSE\n", level, "" );
                STMTlist_out( s->u.cond->otherwise, level + exppp_nesting_indent );
            }
            raw( "%*sEND_IF;\n", level, "" );
            break;
        case STMT_LOOP:
            LOOPout( s->u.loop, level );
            break;
        case STMT_PCALL:
            raw( "%*s%s( ", level, "", s->symbol.name );
            LISTdo( s->u.proc->parameters, p, Expression )
            if( first_time ) {
                first_time = false;
            } else {
                raw( ", " );
            }
            EXPR_out( p, 0 );
            LISTod
            raw( " );\n" );
            break;
        case STMT_RETURN:
            raw( "%*sRETURN", level, "" );
            if( s->u.ret->value ) {
                wrap( "( " );
                EXPR_out( s->u.ret->value, 0 );
                raw( " )" );
            }
            raw( ";\n" );
            break;
        case STMT_ALIAS:
            raw( "%*sALIAS %s for %s;\n", level, "", s->symbol.name,
                 /* should be generalized reference */
                 s->u.alias->variable->name->symbol.name );
            STMTlist_out( s->u.alias->statements, level + exppp_nesting_indent );
            raw( "%*sEND_ALIAS;", level, "" );
            tail_comment( s->symbol.name );
            break;
        case STMT_SKIP:
            raw( "%*sSKIP;\n", level, "" );
            break;
        case STMT_ESCAPE:
            raw( "%*sESCAPE;\n", level, "" );
            break;
    }
}

void STMTlist_out( Linked_List stmts, int level ) {
    LISTdo( stmts, stmt, Statement )
    STMT_out( stmt, level );
    LISTod
}

char * STMTto_string( Statement s ) {
    if( prep_string() ) {
        return placeholder;
    }
    STMT_out( s, 0 );
    return ( finish_string() );
}

/* return length of buffer used */
int STMTto_buffer( Statement s, char * buffer, int length ) {
    if( prep_buffer( buffer, length ) ) {
        return -1;
    }
    STMT_out( s, 0 );
    return( finish_buffer() );
}

void STMTout( Statement s ) {
    prep_file();
    STMT_out( s, 0 );
    finish_file();
}

