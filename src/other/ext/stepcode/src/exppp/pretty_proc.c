/** \file pretty_proc.c
 * split out of exppp.c 9/21/13
 */

#include <exppp/exppp.h>
#include "pp.h"
#include "pretty_alg.h"
#include "pretty_stmt.h"
#include "pretty_proc.h"

char * PROCto_string( Procedure p ) {
    if( prep_string() ) {
        return placeholder;
    }
    PROC_out( p, 0 );
    return ( finish_string() );
}

/** return length of buffer used */
int PROCto_buffer( Procedure e, char * buffer, int length ) {
    if( prep_buffer( buffer, length ) ) {
        return -1;
    }
    PROC_out( e, 0 );
    return( finish_buffer() );
}

void PROCout( Procedure p ) {
    prep_file();
    PROC_out( p, 0 );
    finish_file();
}

void PROC_out( Procedure p, int level ) {
    if( p->u.proc->builtin ) {
        return;
    }

    first_newline();
    exppp_ref_info( &p->symbol );

    raw( "%*sPROCEDURE %s(\n", level, "", p->symbol.name );

    ALGargs_out( p->u.proc->parameters, level + strlen( "PROCEDURE     " ) );

    raw( "%*s);\n", level + exppp_nesting_indent, "" );

    ALGscope_out( p, level + exppp_nesting_indent );
    STMTlist_out( p->u.proc->body, level + exppp_nesting_indent );

    raw( "\n%*sEND_PROCEDURE;", level, "" );
    tail_comment( p->symbol.name );
}

