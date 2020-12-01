/** \file pretty_subtype.c
 * split out of exppp.c 9/21/13
 */

#include <sc_memmgr.h>

#include "exppp.h"

#include "pp.h"
#include "pretty_expr.h"
#include "pretty_subtype.h"

void SUBTYPEout( Expression e ) {
    /* language insists on having parens around entity names */
    /* even if there is only one, but if the expression is */
    /* complex, EXPRout will add on its own parens */
    /*  if (TYPEis_expression(e->type)) {*/
    raw( "( " );
    /*  }*/

    EXPR_out( e, 0 );

    /*  if (TYPEis_expression(e->type)) {*/
    raw( " )" );
    /*  }*/
}

char * SUBTYPEto_string( Expression e ) {
    if( prep_string() ) {
        return placeholder;
    }
    EXPR_out( e, 0 );
    return ( finish_string() );
}
