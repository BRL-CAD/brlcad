/** \file pretty_where.c
 * functions for pretty-printing where rules
 * split out of exppp.c 9/21/13
 */

#include <express/alg.h>
#include <exppp/exppp.h>

#include "pp.h"
#include "pretty_expr.h"
#include "pretty_where.h"

char * WHEREto_string( Linked_List w ) {
    if( prep_string() ) {
        return placeholder;
    }
    WHERE_out( w, 0 );
    return ( finish_string() );
}

/** return length of buffer used */
int WHEREto_buffer( Linked_List w, char * buffer, int length ) {
    if( prep_buffer( buffer, length ) ) {
        return -1;
    }
    WHERE_out( w, 0 );
    return( finish_buffer() );
}

void WHEREout( Linked_List w ) {
    prep_file();
    WHERE_out( w, 0 );
    finish_file();
}

void WHERE_out( Linked_List wheres, int level ) {
    size_t max_indent;
    if( !wheres ) {
        return;
    }

    raw( "%*s%s", level, "", "WHERE\n" );
    level += exppp_nesting_indent;

    /* pass 1: calculate length of longest label */
    max_indent = 0;
    LISTdo( wheres, w, Where ) {
        if( w->label ) {
            if( strlen( w->label->name ) > max_indent ) {
                max_indent = strlen( w->label->name );
            }
        }
    } LISTod

    if( max_indent > 10 ) {
        /* don't bother indenting completely for
         * labels that are ridiculously long */
        max_indent = 4;
    }
    indent2 = level + max_indent + strlen( ": " ) + exppp_continuation_indent;

    /* pass 2: now print labels and exprs */
    LISTdo( wheres, w, Where ) {
        if( w->label ) {
            raw( "%*s%-*s: ", level, "", max_indent, w->label->name );
        } else {
            /* no label */
            raw( "%*s%-*s  ", level, "", max_indent, "" );
        }
        EXPR_out( w->expr, max_indent );
        raw( ";\n" );
    } LISTod
}

