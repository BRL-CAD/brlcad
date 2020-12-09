/** \file pretty_expr.c
 * functions dealing with expressions
 * split out of exppp.c 9/21/13
 */

#include <stdio.h>
#include <sc_stdbool.h>
#include <stdlib.h>

#include "exppp.h"
#include "pp.h"

#include "pretty_expr.h"


/** print array bounds */
void EXPRbounds_out( TypeBody tb ) {
    if( !tb->upper ) {
        return;
    }

    wrap( " [" );
    EXPR_out( tb->lower, 0 );
    wrap( " : " );
    EXPR_out( tb->upper, 0 );
    raw( "]" );
}

/**
 * if paren == 1, parens are usually added to prevent possible rebind by
 *    higher-level context.  If op is similar to previous op (and
 *    precedence/associativity is not a problem) parens may be omitted.
 * if paren == 0, then parens may be omitted without consequence
 */
void EXPR__out( Expression e, int paren, unsigned int previous_op ) {
    int i;  /* trusty temporary */

    switch( TYPEis( e->type ) ) {
        case integer_:
            if( e == LITERAL_INFINITY ) {
                wrap( "?" );
            } else {
                wrap( "%d", e->u.integer );
            }
            break;
        case real_:
            if( e == LITERAL_PI ) {
                wrap( "PI" );
            } else if( e == LITERAL_E ) {
                wrap( "E" );
            } else {
                wrap( real2exp( e->u.real ) );
            }
            break;
        case binary_:
            wrap( "%%%s", e->u.binary ); /* put "%" back */
            break;
        case logical_:
        case boolean_:
            switch( e->u.logical ) {
                case Ltrue:
                    wrap( "TRUE" );
                    break;
                case Lfalse:
                    wrap( "FALSE" );
                    break;
                default:
                    wrap( "UNKNOWN" );
                    break;
            }
            break;
        case string_:
            if( TYPEis_encoded( e->type ) ) {
                wrap( "\"%s\"", e->symbol.name );
            } else {
                breakLongStr( e->symbol.name );
            }
            break;
        case entity_:
        case identifier_:
        case attribute_:
        case enumeration_:
            wrap( "%s", e->symbol.name );
            break;
        case query_:
            wrap( "QUERY ( %s <* ", e->u.query->local->name->symbol.name );
            EXPR_out( e->u.query->aggregate, 1 );
            wrap( " | " );
            EXPR_out( e->u.query->expression, 1 );
            raw( " )" );
            break;
        case self_:
            wrap( "SELF" );
            break;
        case funcall_:
            wrap( "%s( ", e->symbol.name );
            i = 0;
            LISTdo( e->u.funcall.list, arg, Expression )
            i++;
            if( i != 1 ) {
                raw( ", " );
            }
            EXPR_out( arg, 0 );
            LISTod
            raw( " )" );
            break;
        case op_:
            EXPRop__out( &e->e, paren, previous_op );
            break;
        case aggregate_:
            wrap( "[" );
            i = 0;
            LISTdo( e->u.list, arg, Expression ) {
                bool repeat = arg->type->u.type->body->flags.repeat;
                /* if repeat is true, the previous Expression repeats and this one is the count */
                i++;
                if( i != 1 ) {
                    if( repeat ) {
                        raw( " : " );
                    } else {
                        raw( ", " );
                    }
                }
                EXPR_out( arg, 0 );
            } LISTod
            raw( "]" );
            break;
        case oneof_: {
            int old_indent = indent2;
            wrap( "ONEOF ( " );

            if( exppp_linelength == indent2 ) {
                exppp_linelength += exppp_continuation_indent;
            }
            indent2 += exppp_continuation_indent;

            i = 0;
            LISTdo( e->u.list, arg, Expression )
            i++;
            if( i != 1 ) {
                raw( ", " );
            }
            EXPR_out( arg, 1 );
            LISTod

            if( exppp_linelength == indent2 ) {
                exppp_linelength = old_indent;
            }
            indent2 = old_indent;

            raw( " )" );
            break;
        }
        default:
            fprintf( stderr, "%s:%d: ERROR - unknown expression, type %d", e->symbol.filename, e->symbol.line, TYPEis( e->type ) );
            abort();
    }
}

/** print expression that has op and operands */
void EXPRop__out( struct Op_Subexpression * oe, int paren, unsigned int previous_op ) {
    const unsigned int PAD = 1, NOPAD = 0;
    switch( oe->op_code ) {
        case OP_AND:
        case OP_ANDOR:
        case OP_OR:
        case OP_CONCAT:
        case OP_EQUAL:
        case OP_PLUS:
        case OP_TIMES:
        case OP_XOR:
            EXPRop2__out( oe, ( char * )0, paren, PAD, previous_op );
            break;
        case OP_EXP:
        case OP_GREATER_EQUAL:
        case OP_GREATER_THAN:
        case OP_IN:
        case OP_INST_EQUAL:
        case OP_INST_NOT_EQUAL:
        case OP_LESS_EQUAL:
        case OP_LESS_THAN:
        case OP_LIKE:
        case OP_MOD:
        case OP_NOT_EQUAL:
            EXPRop2_out( oe, ( char * )0, paren, PAD );
            break;
        case OP_NOT:
            EXPRop1_out( oe, "NOT ", paren );
            break;
        case OP_REAL_DIV:
            EXPRop2_out( oe, "/", paren, PAD );
            break;
        case OP_DIV:
            EXPRop2_out( oe, "DIV", paren, PAD );
            break;
        case OP_MINUS:
            EXPRop2_out( oe, "-", paren, PAD );
            break;
        case OP_DOT:
            EXPRop2_out( oe, ".", paren, NOPAD );
            break;
        case OP_GROUP:
            EXPRop2_out( oe, "\\", paren, NOPAD );
            break;
        case OP_NEGATE:
            EXPRop1_out( oe, "-", paren );
            break;
        case OP_ARRAY_ELEMENT:
            EXPR_out( oe->op1, 1 );
            wrap( "[" );
            EXPR_out( oe->op2, 0 );
            raw( "]" );
            break;
        case OP_SUBCOMPONENT:
            EXPR_out( oe->op1, 1 );
            wrap( "[" );
            EXPR_out( oe->op2, 0 );
            wrap( " : " );
            EXPR_out( oe->op3, 0 );
            raw( "]" );
            break;
        default:
            wrap( "(* unknown op-expression *)" );
    }
}

void EXPRop2__out( struct Op_Subexpression * eo, char * opcode, int paren, int pad, unsigned int previous_op ) {
    if( pad && paren && ( eo->op_code != previous_op ) ) {
        wrap( "( " );
    }
    EXPR__out( eo->op1, 1, eo->op_code );
    if( pad ) {
        raw( " " );
    }
    wrap( "%s", ( opcode ? opcode : EXPop_table[eo->op_code].token ) );
    if( pad ) {
        wrap( " " );
    }
    EXPR__out( eo->op2, 1, eo->op_code );
    if( pad && paren && ( eo->op_code != previous_op ) ) {
        raw( " )" );
    }
}

/** Print out a one-operand operation.  If there were more than two of these
 * I'd generalize it to do padding, but it's not worth it.
 */
void EXPRop1_out( struct Op_Subexpression * eo, char * opcode, int paren ) {
    if( paren ) {
        wrap( "( " );
    }
    wrap( "%s", opcode );
    EXPR_out( eo->op1, 1 );
    if( paren ) {
        raw( " )" );
    }
}

int EXPRop_length( struct Op_Subexpression * oe ) {
    switch( oe->op_code ) {
        case OP_DOT:
        case OP_GROUP:
            return( 1 + EXPRlength( oe->op1 )
                    + EXPRlength( oe->op2 ) );
        default:
            fprintf( stdout, "EXPRop_length: unknown op-expression" );
    }
    return 0;
}

/** returns printable representation of expression rather than printing it
 * originally only used for general references, now being expanded to handle
 * any kind of expression
 * contains fragment of string, adds to it
 */
void EXPRstring( char * buffer, Expression e ) {
    int i;

    switch( TYPEis( e->type ) ) {
        case integer_:
            if( e == LITERAL_INFINITY ) {
                strcpy( buffer, "?" );
            } else {
                sprintf( buffer, "%d", e->u.integer );
            }
            break;
        case real_:
            if( e == LITERAL_PI ) {
                strcpy( buffer, "PI" );
            } else if( e == LITERAL_E ) {
                strcpy( buffer, "E" );
            } else {
                sprintf( buffer, "%s", real2exp( e->u.real ) );
            }
            break;
        case binary_:
            sprintf( buffer, "%%%s", e->u.binary ); /* put "%" back */
            break;
        case logical_:
        case boolean_:
            switch( e->u.logical ) {
                case Ltrue:
                    strcpy( buffer, "TRUE" );
                    break;
                case Lfalse:
                    strcpy( buffer, "FALSE" );
                    break;
                default:
                    strcpy( buffer, "UNKNOWN" );
                    break;
            }
            break;
        case string_:
            if( TYPEis_encoded( e->type ) ) {
                sprintf( buffer, "\"%s\"", e->symbol.name );
            } else {
                sprintf( buffer, "%s", e->symbol.name );
            }
            break;
        case entity_:
        case identifier_:
        case attribute_:
        case enumeration_:
            strcpy( buffer, e->symbol.name );
            break;
        case query_:
            sprintf( buffer, "QUERY ( %s <* ", e->u.query->local->name->symbol.name );
            EXPRstring( buffer + strlen( buffer ), e->u.query->aggregate );
            strcat( buffer, " | " );
            EXPRstring( buffer + strlen( buffer ), e->u.query->expression );
            strcat( buffer, " )" );
            break;
        case self_:
            strcpy( buffer, "SELF" );
            break;
        case funcall_:
            sprintf( buffer, "%s( ", e->symbol.name );
            i = 0;
            LISTdo( e->u.funcall.list, arg, Expression )
            i++;
            if( i != 1 ) {
                strcat( buffer, ", " );
            }
            EXPRstring( buffer + strlen( buffer ), arg );
            LISTod
            strcat( buffer, " )" );
            break;

        case op_:
            EXPRop_string( buffer, &e->e );
            break;
        case aggregate_:
            strcpy( buffer, "[" );
            i = 0;
            LISTdo( e->u.list, arg, Expression ) {
                bool repeat = arg->type->u.type->body->flags.repeat;
                /* if repeat is true, the previous Expression repeats and this one is the count */
                i++;
                if( i != 1 ) {
                    if( repeat ) {
                        strcat( buffer, " : " );
                    } else {
                        strcat( buffer, ", " );
                    }
                }
                EXPRstring( buffer + strlen( buffer ), arg );
            } LISTod
            strcat( buffer, "]" );
            break;
        case oneof_:
            strcpy( buffer, "ONEOF ( " );

            i = 0;
            LISTdo( e->u.list, arg, Expression ) {
                i++;
                if( i != 1 ) {
                    strcat( buffer, ", " );
                }
                EXPRstring( buffer + strlen( buffer ), arg );
            } LISTod

            strcat( buffer, " )" );
            break;
        default:
            sprintf( buffer, "EXPRstring: unknown expression, type %d", TYPEis( e->type ) );
            fprintf( stderr, "%s", buffer );
    }
}

void EXPRop_string( char * buffer, struct Op_Subexpression * oe ) {
    EXPRstring( buffer, oe->op1 );
    switch( oe->op_code ) {
        case OP_DOT:
            strcat( buffer, "." );
            break;
        case OP_GROUP:
            strcat( buffer, "\\" );
            break;
        default:
            strcat( buffer, "(* unknown op-expression *)" );
    }
    EXPRstring( buffer + strlen( buffer ), oe->op2 );
}

/** returns length of printable representation of expression w.o. printing it
 * doesn't understand as many expressions as the printing functions (!)
 * WARNING this *does* change the global 'curpos'!
 */
int EXPRlength( Expression e ) {
    char buffer[10000];
    *buffer = '\0';
    EXPRstring( buffer, e );
    return( strlen( buffer ) );
}

char * EXPRto_string( Expression e ) {
    if( prep_string() ) {
        return placeholder;
    }
    EXPR_out( e, 0 );
    return ( finish_string() );
}

/** \return length of buffer used */
int EXPRto_buffer( Expression e, char * buffer, int length ) {
    if( prep_buffer( buffer, length ) ) {
        return -1;
    }
    EXPR_out( e, 0 );
    return( finish_buffer() );
}

void EXPRout( Expression e ) {
    prep_file();
    EXPR_out( e, 0 );
    finish_file();
}
