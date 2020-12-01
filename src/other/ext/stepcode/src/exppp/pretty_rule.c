/** \file pretty_rule.c
 * split out of exppp.c 9/21/13
 */

#include "pp.h"
#include "exppp.h"
#include "pretty_alg.h"
#include "pretty_stmt.h"
#include "pretty_where.h"
#include "pretty_rule.h"

char * RULEto_string( Rule r ) {
    if( prep_string() ) {
        return placeholder;
    }
    RULE_out( r, 0 );
    return ( finish_string() );
}

/** return length of buffer used */
int RULEto_buffer( Rule e, char * buffer, int length ) {
    if( prep_buffer( buffer, length ) ) {
        return -1;
    }
    RULE_out( e, 0 );
    return( finish_buffer() );
}

void RULEout( Rule r ) {
    prep_file();
    RULE_out( r, 0 );
    finish_file();
}

void RULE_out( Rule r, int level ) {
    int i = 0;
    first_newline();
    exppp_ref_info( &r->symbol );

    raw( "%*sRULE %s FOR ( ", level, "", r->symbol.name );

    LISTdo( r->u.rule->parameters, p, Variable )
    i++;
    if( i != 1 ) {
        raw( ", " );
    }
    wrap( p->name->symbol.name );
    LISTod;
    raw( " );\n" );

    ALGscope_out( r, level + exppp_nesting_indent );
    STMTlist_out( r->u.rule->body, level + exppp_nesting_indent );
    raw( "\n" );
    WHERE_out( RULEget_where( r ), level );

    raw( "\n%*sEND_RULE;", level, "" );
    tail_comment( r->symbol.name );
}
