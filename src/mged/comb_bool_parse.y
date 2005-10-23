/*
 *		C O M B _ B O O L _ P A R S E . Y
 *
 *	YACC(1) specifiation to parse Boolean expressions for
 *			the 'c' command
 *
 *	This grammar recognizes the classical infix notation that allows
 *	parenthesization and assigns INTERSECTION a higher precedence than
 *	UNION or DIFFERENCE, which are evaluated left-to-right.
 *
 *  Author -
 *	Paul Tanenbaum
 *
 *  Source -
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5068  USA
 */

%{
#ifndef lint
static char RCSid[] = "@(#)$Header$ (ARL)";
#endif

#include "common.h"

#include <stdio.h>
#include <string.h>

#include "../mged/comb_bool.h"

/* The following is needed to get "gettxt()" correctly on IRIX platforms */
#undef _POSIX_SOURCE

%}

/*	Token representations
 *	(i.e. terminals)
 */
%token <val_string>	TKN_OBJECT
%token <val_vanilla>	TKN_LPAREN
%token <val_vanilla>	TKN_RPAREN
%token <val_string>	TKN_UNION
%token <val_string>	TKN_DIFFERENCE
%token <val_string>	TKN_INTERSECTION

/*
 *	Nonterminals
 */
%start			start
%type <val_tree>	tree intersectree uniontree

/*
 *	Associativity and precedence
 */
%left	TKN_UNION TKN_DIFFERENCE
%left	TKN_PRODUCT_OP

%union
{
    char			*val_string;
    int				val_vanilla;
    struct bool_tree_node	*val_tree;
}

%%

/*
 *	Productions
 */
start		: uniontree
		    {
			comb_bool_tree = $1;
		    }
		;

uniontree	: intersectree
		    {
			$$ = $1;
		    }
		| uniontree TKN_UNION intersectree
		    {
			$$ = bt_create_internal(OPN_UNION, $1, $3);
		    }
		| uniontree TKN_DIFFERENCE intersectree
		    {
			$$ = bt_create_internal(OPN_DIFFERENCE, $1, $3);
		    }
		;
intersectree	: tree
		    {
			$$ = $1;
		    }
		| intersectree TKN_INTERSECTION tree
		    {
			$$ = bt_create_internal(OPN_INTERSECTION, $1, $3);
		    }
		;
tree		: TKN_OBJECT
		    {
			$$ = bt_create_leaf($1);
		    }
		| TKN_LPAREN uniontree TKN_RPAREN
		    {
			$$ = $2;
		    }
		;
%%

extern char	*bool_op_lexeme[];

/* ??? #include "../mged/comb_bool.c" */

void yyerror (s)
char *s;
{
    (void) fprintf(stderr, "Error: %s\n", s);
}
