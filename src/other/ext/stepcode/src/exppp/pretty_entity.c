/** \file pretty_entity.c
 * functions dealing with entities
 * split out of exppp.c 9/21/13
 */

#include <assert.h>
#include <stdlib.h>

#include "exppp.h"
#include "pp.h"
#include "pretty_expr.h"
#include "pretty_subtype.h"
#include "pretty_where.h"
#include "pretty_type.h"
#include "pretty_entity.h"

void ENTITY_out( Entity e, int level ) {
    const unsigned int EXPLICIT = 0, DERIVED = 1;
    int linelen = exppp_linelength;
    bool first_time = true;

    first_newline();
    exppp_ref_info( &e->symbol );

    raw( "%*sENTITY %s", level, "", e->symbol.name );

    level += exppp_nesting_indent;
    indent2 = level + exppp_continuation_indent;

    exppp_linelength = indent2; /* force newlines */
    if( ENTITYget_abstract( e ) ) {
        if( e->u.entity->subtype_expression ) {
            raw( "\n%*sABSTRACT SUPERTYPE OF ", level, "" );
            SUBTYPEout( e->u.entity->subtype_expression );
        } else {
            raw( "\n%*sABSTRACT SUPERTYPE", level, "" );
        }
    } else {
        if( e->u.entity->subtype_expression ) {
            raw( "\n%*sSUPERTYPE OF ", level, "" );
            SUBTYPEout( e->u.entity->subtype_expression );
        }
    }
    exppp_linelength = linelen;

    if( e->u.entity->supertype_symbols ) {
        raw( "\n%*sSUBTYPE OF ( ", level, "" );

        LISTdo( e->u.entity->supertype_symbols, s, Symbol * )
        if( first_time ) {
            first_time = false;
        } else {
            raw( ", " );
        }
        wrap( s->name );
        LISTod
        raw( " )" );
    }

    raw( ";\n" );

#if 0
    /* add a little more space before entities if sub or super appears */
    if( e->u.entity->supertype_symbols || e->u.entity->subtype_expression ) {
        raw( "\n" );
    }
#endif

    ENTITYattrs_out( e->u.entity->attributes, EXPLICIT, level );
    ENTITYattrs_out( e->u.entity->attributes, DERIVED, level );
    ENTITYinverse_out( e->u.entity->attributes, level );
    ENTITYunique_out( e->u.entity->unique, level );
    WHERE_out( TYPEget_where( e ), level );

    level -= exppp_nesting_indent;
    raw( "%*sEND_ENTITY;", level, "" );
    tail_comment( e->symbol.name );
}

void ENTITYunique_out( Linked_List u, int level ) {
    int i;
    int max_indent;
    Symbol * sym;

    if( !u ) {
        return;
    }

    raw( "%*sUNIQUE\n", level, "" );

    /* pass 1 */
    max_indent = 0;
    LISTdo( u, list, Linked_List ) {
        if( 0 != ( sym = ( Symbol * )LISTget_first( list ) ) ) {
            int length;
            length = strlen( sym->name );
            if( length > max_indent ) {
                max_indent = length;
            }
        }
    } LISTod

    level += exppp_nesting_indent;
    indent2 = level + max_indent + strlen( ": " ) + exppp_continuation_indent;

    LISTdo( u, list, Linked_List ) {
        i = 0;
        LISTdo_n( list, e, Expression, b ) {
            i++;
            if( i == 1 ) {
                /* print label if present */
                if( e ) {
                    raw( "%*s%-*s : ", level, "", max_indent, ( ( Symbol * )e )->name );
                } else {
                    raw( "%*s%-*s   ", level, "", max_indent, "" );
                }
            } else {
                if( i > 2 ) {
                    raw( ", " );
                }
                EXPR_out( e, 0 );
            }
        } LISTod
        raw( ";\n" );
    } LISTod
}

void ENTITYinverse_out( Linked_List attrs, int level ) {
    int max_indent;

    /* pass 1: calculate length of longest attr name */
    max_indent = 0;
    LISTdo( attrs, v, Variable ) {
        if( v->inverse_symbol ) {
            int length;
            length = strlen( v->name->symbol.name );
            if( length > max_indent ) {
                max_indent = length;
            }
        }
    } LISTod

    if( max_indent == 0 ) {
        return;
    }
    raw( "%*sINVERSE\n", level, "" );
    level += exppp_nesting_indent;
    indent2 = level + max_indent + strlen( ": " ) + exppp_continuation_indent;

    /* pass 2: print them */
    LISTdo( attrs, v, Variable ) {
        if( v->inverse_symbol ) {
            /* print attribute name */
            raw( "%*s", level, "" );
            EXPR_out( v->name, 0 );
            raw( "%-*s :", ( ( ( max_indent - curpos ) > 0 ) ? max_indent - curpos  : 0 ), "" );

            /* print attribute type */
            if( VARget_optional( v ) ) {
                wrap( " OPTIONAL" );
            }
            TYPE_head_out( v->type, NOLEVEL );

            raw( " FOR " );

            wrap( v->inverse_attribute->name->symbol.name );

            raw( ";\n" );
        }
    } LISTod
}

void ENTITYattrs_out( Linked_List attrs, int derived, int level ) {
    int max_indent;

    /* pass 1: calculate length of longest attr name */
    max_indent = 0;
    LISTdo( attrs, v, Variable ) {
        if( v->inverse_symbol ) {
            continue;
        }
        if( ( derived && v->initializer ) ||
                ( !derived && !v->initializer ) ) {
            int length;
            length = EXPRlength( v->name );
            if( length > max_indent ) {
                max_indent = length;
            }
        }
    } LISTod

    if( max_indent == 0 ) {
        return;
    }
    if( derived ) {
        raw( "%*sDERIVE\n", level, "" );
    }
    level += exppp_nesting_indent;
    if( level + max_indent > exppp_linelength / 3 ) {
        max_indent = ( exppp_linelength / 3 ) - level;
    }
    indent2 = level + max_indent + strlen( ": " ) + exppp_continuation_indent;

    /* pass 2: print them */
    LISTdo( attrs, v, Variable ) {
        if( v->inverse_symbol ) {
            continue;
        }
        if( ( derived && v->initializer ) || ( !derived && !v->initializer ) ) {
            int spaces;
            /* print attribute name */
            raw( "%*s", level, "" );
            EXPR_out( v->name, 0 );
            spaces = level + max_indent + 2 - curpos;
            if( spaces < 0 ) {
                spaces = 0;
            }
            raw( "%*s :", spaces, "" );

            /* print attribute type */
            if( VARget_optional( v ) ) {
                wrap( " OPTIONAL" );
            }
            TYPE_head_out( v->type, NOLEVEL );

            if( derived && v->initializer ) {
                wrap( " := " );
                EXPR_out( v->initializer, 0 );
            }

            raw( ";\n" );
        }
    } LISTod
}

char * ENTITYto_string( Entity e ) {
    if( prep_string() ) {
        return placeholder;
    }
    ENTITY_out( e, 0 );
    return ( finish_string() );
}

/** return length of buffer used */
int ENTITYto_buffer( Entity e, char * buffer, int length ) {
    if( prep_buffer( buffer, length ) ) {
        return -1;
    }
    ENTITY_out( e, 0 );
    return( finish_buffer() );
}

void ENTITYout( Entity e ) {
    prep_file();
    ENTITY_out( e, 0 );
    finish_file();
}
