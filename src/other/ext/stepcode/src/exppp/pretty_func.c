/** \file pretty_func.c
 * split out of exppp.c 9/21/13
 */

#include <exppp/exppp.h>
#include "pp.h"
#include "pretty_alg.h"
#include "pretty_type.h"
#include "pretty_stmt.h"
#include "pretty_func.h"

void FUNC_out( Function fn, int level ) {
    if( fn->u.func->builtin ) {
        return;
    }

    first_newline();
    exppp_ref_info( &fn->symbol );

    raw( "%*sFUNCTION %s", level, "", fn->symbol.name );
    if( fn->u.func->parameters ) {
        unsigned int param_indent = level + strlen( "FUNCTION     " );
        raw( "(\n" );
        ALGargs_out( fn->u.func->parameters, param_indent );
        raw( "\n%*s)", param_indent - exppp_continuation_indent, "" );
    }
    raw( " :" );

    indent2 = curpos + exppp_continuation_indent;
    TYPE_head_out( fn->u.func->return_type, NOLEVEL );
    raw( ";\n" );

    ALGscope_out( fn, level + exppp_nesting_indent );
    STMTlist_out( fn->u.proc->body, level + exppp_nesting_indent );

    raw( "\n%*sEND_FUNCTION;", level, "" );
    tail_comment( fn->symbol.name );
}

char * FUNCto_string( Function f ) {
    if( prep_string() ) {
        return placeholder;
    }
    FUNC_out( f, 0 );
    return ( finish_string() );
}

/** return length of buffer used */
int FUNCto_buffer( Function e, char * buffer, int length ) {
    if( prep_buffer( buffer, length ) ) {
        return -1;
    }
    FUNC_out( e, 0 );
    return( finish_buffer() );
}

void FUNCout( Function f ) {
    prep_file();
    FUNC_out( f, 0 );
    finish_file();
}

