#ifndef PRETTY_EXPR_H
#define PRETTY_EXPR_H

#include "../express/expbasic.h"
#include "../express/express.h"


#define EXPR_out(e,p) EXPR__out(e,p,OP_UNKNOWN)
#define EXPRop2_out(oe,string,paren,pad) \
EXPRop2__out(oe,string,paren,pad,OP_UNKNOWN)
#define EXPRop_out(oe,paren) EXPRop__out(oe,paren,OP_UNKNOWN)

void EXPRop__out( struct Op_Subexpression * oe, int paren, unsigned int previous_op );
void EXPRop_string( char * buffer, struct Op_Subexpression * oe );
void EXPRop1_out( struct Op_Subexpression * eo, char * opcode, int paren );
void EXPRop2__out( struct Op_Subexpression * eo, char * opcode, int paren, int pad, unsigned int previous_op );
void EXPR__out( Expression e, int paren, unsigned int previous_op );
void EXPRbounds_out( TypeBody tb );
int EXPRlength( Expression e );

#endif /* PRETTY_EXPR_H */

/*
char * EXPRto_string( Expression e );
void EXPR__out( Expression e, int paren, unsigned int previous_op );
void EXPRbounds_out( TypeBody tb );
int EXPRlength( Expression e );
void EXPRop1_out( struct Op_Subexpression * eo, char * opcode, int paren );
void EXPRop2__out( struct Op_Subexpression * eo, char * opcode, int paren, int pad, unsigned int previous_op );
void EXPRop__out( struct Op_Subexpression * oe, int paren, unsigned int previous_op );
int EXPRop_length( struct Op_Subexpression * oe );
void EXPRop_string( char * buffer, struct Op_Subexpression * oe );
void EXPRout( Expression e );
void EXPRstring( char * buffer, Expression e );
int EXPRto_buffer( Expression e, char * buffer, int length );
*/
