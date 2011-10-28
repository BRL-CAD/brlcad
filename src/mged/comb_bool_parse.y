/*               C O M B _ B O O L _ P A R S E . Y
 * BRL-CAD
 *
 * Copyright (c) 2008-2011 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file comb_bool_parse.y
 *
 *	YACC(1) specifiation to parse Boolean expressions for
 *			the 'c' command
 *
 *	This grammar recognizes the classical infix notation that allows
 *	parenthesization and assigns INTERSECTION a higher precedence than
 *	UNION or DIFFERENCE, which are evaluated left-to-right.
 *
 */

%{

#include "common.h"

#include <stdio.h>
#include <string.h>

#include "../mged/comb_bool.h"


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

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
