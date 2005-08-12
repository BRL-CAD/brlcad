/*                  W D B _ C O M B _ S T D . C
 * BRL-CAD
 *
 * Copyright (C) 1997-2005 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */

/** \defgroup wdb WriteDatabase
 * \ingroup librt */

/*@{*/
/** @file wdb_comb_std.c
 *	Code to implement the database objects "c" command.
 *
 *  Author -
 *      John R. Anderson
 *
 *  Source -
 *      The U. S. Army Research Laboratory
 *      Aberdeen Proving Ground, Maryland  21005-5068  USA
 */
/*@}*/

#ifndef lint
static const char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "common.h"

#include <stdio.h>
#ifdef HAVE_STRING_H
#  include <string.h>
#else
#  include <strings.h>
#endif

#include "tcl.h"
#include "machine.h"
#include "bu.h"
#include "vmath.h"
#include "raytrace.h"
#include "rtgeom.h"


#define	PRINT_USAGE Tcl_AppendResult(interp, "c: usage 'c [-cr] comb_name [bool_expr]'\n",\
				     (char *)NULL)

struct tokens {
	struct bu_list		l;
	short			type;
	union tree		*tp;
};

/* token types */
#define	WDB_TOK_NULL	0
#define	WDB_TOK_LPAREN	1
#define	WDB_TOK_RPAREN	2
#define	WDB_TOK_UNION	3
#define	WDB_TOK_INTER	4
#define	WDB_TOK_SUBTR	5
#define	WDB_TOK_TREE	6

HIDDEN void
wdb_free_tokens(struct bu_list *hp)
{
	struct tokens *tok;

	BU_CK_LIST_HEAD(hp);

	while (BU_LIST_WHILE(tok, tokens, hp)) {
		BU_LIST_DEQUEUE(&tok->l);
		if (tok->type == WDB_TOK_TREE) {
			if (tok->tp)
				db_free_tree(tok->tp, &rt_uniresource);
		}
	}
}

HIDDEN void
wdb_append_union(struct bu_list *hp)
{
	struct tokens *tok;

	BU_CK_LIST_HEAD( hp );

	tok = (struct tokens *)bu_malloc(sizeof(struct tokens), "tok");
	tok->type = WDB_TOK_UNION;
	tok->tp = (union tree *)NULL;
	BU_LIST_INSERT(hp, &tok->l);
}

HIDDEN void
wdb_append_inter(struct bu_list *hp)
{
	struct tokens *tok;

	BU_CK_LIST_HEAD(hp);

	tok = (struct tokens *)bu_malloc(sizeof(struct tokens), "tok");
	tok->type = WDB_TOK_INTER;
	tok->tp = (union tree *)NULL;
	BU_LIST_INSERT( hp, &tok->l );
}

HIDDEN void
wdb_append_subtr(struct bu_list *hp)
{
	struct tokens *tok;

	BU_CK_LIST_HEAD(hp);

	tok = (struct tokens *)bu_malloc(sizeof(struct tokens), "tok");
	tok->type = WDB_TOK_SUBTR;
	tok->tp = (union tree *)NULL;
	BU_LIST_INSERT( hp, &tok->l );
}

HIDDEN void
wdb_append_lparen(struct bu_list *hp)
{
	struct tokens *tok;

	BU_CK_LIST_HEAD(hp);

	tok = (struct tokens *)bu_malloc(sizeof(struct tokens), "tok");
	tok->type = WDB_TOK_LPAREN;
	tok->tp = (union tree *)NULL;
	BU_LIST_INSERT( hp, &tok->l );
}

HIDDEN void
wdb_append_rparen(struct bu_list *hp)
{
	struct tokens *tok;

	BU_CK_LIST_HEAD(hp);

	tok = (struct tokens *)bu_malloc(sizeof(struct tokens), "tok");
	tok->type = WDB_TOK_RPAREN;
	tok->tp = (union tree *)NULL;
	BU_LIST_INSERT(hp, &tok->l);
}

HIDDEN int
wdb_add_operator(Tcl_Interp *interp, struct bu_list *hp, char ch, short int *last_tok)
{
	char illegal[2];

	BU_CK_LIST_HEAD(hp);

	switch( ch )
	{
		case 'u':
			wdb_append_union(hp);
			*last_tok = WDB_TOK_UNION;
			break;
		case '+':
			wdb_append_inter(hp);
			*last_tok = WDB_TOK_INTER;
			break;
		case '-':
			wdb_append_subtr(hp);
			*last_tok = WDB_TOK_SUBTR;
			break;
		default:
			illegal[0] = ch;
			illegal[1] = '\0';
			Tcl_AppendResult(interp, "Illegal operator: ", illegal,
				", aborting\n", (char *)NULL );
			wdb_free_tokens(hp);
			return TCL_ERROR;
	}
	return TCL_OK;
}

HIDDEN int
wdb_add_operand(Tcl_Interp *interp, struct bu_list *hp, char *name)
{
	char *ptr_lparen;
	char *ptr_rparen;
	int name_len;
	union tree *node;
	struct tokens *tok;

	BU_CK_LIST_HEAD(hp);

	ptr_lparen = strchr(name, '(');
	ptr_rparen = strchr(name, ')');

	RT_GET_TREE( node, &rt_uniresource );
	node->magic = RT_TREE_MAGIC;
	node->tr_op = OP_DB_LEAF;
	node->tr_l.tl_mat = (matp_t)NULL;
	if (ptr_lparen || ptr_rparen) {
		int tmp1,tmp2;

		if (ptr_rparen)
			tmp1 = ptr_rparen - name;
		else
			tmp1 = (-1);
		if (ptr_lparen)
			tmp2 = ptr_lparen - name;
		else
			tmp2 = (-1);

		if (tmp2 == (-1) && tmp1 > 0)
			name_len = tmp1;
		else if (tmp1 == (-1) && tmp2 > 0)
			name_len = tmp2;
		else if(tmp1 > 0 && tmp2 > 0) {
			if (tmp1 < tmp2)
				name_len = tmp1;
			else
				name_len = tmp2;
		}
		else {
			Tcl_AppendResult(interp, "Cannot determine length of operand name: ",
				name, ", aborting\n", (char *)NULL);
			return (0);
		}
	} else
		name_len = strlen( name );

	node->tr_l.tl_name = (char *)bu_malloc(name_len + 1, "node name");
	strncpy(node->tr_l.tl_name, name, name_len);
	node->tr_l.tl_name[name_len] = '\0';
	tok = (struct tokens *)bu_malloc(sizeof(struct tokens), "tok");
	tok->type = WDB_TOK_TREE;
	tok->tp = node;
	BU_LIST_INSERT(hp, &tok->l);
	return (name_len);
}

HIDDEN void
wdb_do_inter(struct bu_list *hp)
{
	struct tokens *tok;

	for (BU_LIST_FOR(tok, tokens, hp )) {
		struct tokens *prev, *next;
		union tree *tp;

		if (tok->type != WDB_TOK_INTER)
			continue;

		prev = BU_LIST_PREV( tokens, &tok->l );
		next = BU_LIST_NEXT( tokens, &tok->l );

		if (prev->type !=WDB_TOK_TREE || next->type != WDB_TOK_TREE)
			continue;

		/* this is an eligible intersection operation */
		tp = (union tree *)bu_malloc( sizeof( union tree ), "tp" );
		tp->magic = RT_TREE_MAGIC;
		tp->tr_b.tb_op = OP_INTERSECT;
		tp->tr_b.tb_regionp = (struct region *)NULL;
		tp->tr_b.tb_left = prev->tp;
		tp->tr_b.tb_right = next->tp;
		BU_LIST_DEQUEUE( &tok->l );
		bu_free( (char *)tok, "tok" );
		BU_LIST_DEQUEUE( &prev->l );
		bu_free( (char *)prev, "prev" );
		next->tp = tp;
		tok = next;
	}
}

HIDDEN void
wdb_do_union_subtr(struct bu_list *hp)
{
	struct tokens *tok;

	for(BU_LIST_FOR(tok, tokens, hp)) {
		struct tokens *prev, *next;
		union tree *tp;

		if (tok->type != WDB_TOK_UNION && tok->type != WDB_TOK_SUBTR)
			continue;

		prev = BU_LIST_PREV( tokens, &tok->l );
		next = BU_LIST_NEXT( tokens, &tok->l );

		if (prev->type !=WDB_TOK_TREE || next->type != WDB_TOK_TREE)
			continue;

		/* this is an eligible operation */
		tp = (union tree *)bu_malloc( sizeof( union tree ), "tp" );
		tp->magic = RT_TREE_MAGIC;
		if (tok->type == WDB_TOK_UNION)
			tp->tr_b.tb_op = OP_UNION;
		else
			tp->tr_b.tb_op = OP_SUBTRACT;
		tp->tr_b.tb_regionp = (struct region *)NULL;
		tp->tr_b.tb_left = prev->tp;
		tp->tr_b.tb_right = next->tp;
		BU_LIST_DEQUEUE(&tok->l);
		bu_free((char *)tok, "tok");
		BU_LIST_DEQUEUE(&prev->l);
		bu_free((char *)prev, "prev");
		next->tp = tp;
		tok = next;
	}
}

HIDDEN int
wdb_do_paren(struct bu_list *hp)
{
	struct tokens *tok;

	for (BU_LIST_FOR(tok, tokens, hp)) {
		struct tokens *prev, *next;

		if (tok->type != WDB_TOK_TREE)
			continue;

		prev = BU_LIST_PREV( tokens, &tok->l );
		next = BU_LIST_NEXT( tokens, &tok->l );

		if (prev->type !=WDB_TOK_LPAREN || next->type != WDB_TOK_RPAREN)
			continue;

		/* this is an eligible operand surrounded by parens */
		BU_LIST_DEQUEUE(&next->l);
		bu_free((char *)next, "next");
		BU_LIST_DEQUEUE(&prev->l);
		bu_free((char *)prev, "prev");
	}

	if (hp->forw == hp->back && hp->forw != hp)
		return 1;	/* done */
	else if (BU_LIST_IS_EMPTY(hp))
		return -1;	/* empty tree!!!! */
	else
		return 0;	/* more to do */
		
}

HIDDEN union tree *
wdb_eval_bool(struct bu_list *hp)
{
	int done=0;
	union tree *final_tree;
	struct tokens *tok;

	while (done != 1) {
		wdb_do_inter(hp);
		wdb_do_union_subtr(hp);
		done = wdb_do_paren(hp);
	}

	if (done == 1) {
		tok = BU_LIST_NEXT(tokens, hp);
		final_tree = tok->tp;
		BU_LIST_DEQUEUE(&tok->l);
		bu_free((char *)tok, "tok");
		return(final_tree);
	}

        return (union tree *)NULL;
}

HIDDEN int
wdb_check_syntax(Tcl_Interp *interp, struct db_i *dbip, struct bu_list *hp, char *comb_name, struct directory *dp)
{
	struct tokens *tok;
	int paren_count=0;
	int paren_error=0;
	int missing_exp=0;
	int missing_op=0;
	int op_count=0;
	int arg_count=0;
	int circular_ref=0;
	int errors=0;
	short last_tok=WDB_TOK_NULL;

	if(dbip == DBI_NULL)
	  return 0;

	for( BU_LIST_FOR( tok, tokens, hp ) )
	{
		switch( tok->type )
		{
			case WDB_TOK_LPAREN:
				paren_count++;
				if( last_tok == WDB_TOK_RPAREN )
					missing_op++;
				break;
			case WDB_TOK_RPAREN:
				paren_count--;
				if( last_tok == WDB_TOK_LPAREN )
					missing_exp++;
				break;
			case WDB_TOK_UNION:
			case WDB_TOK_SUBTR:
			case WDB_TOK_INTER:
				op_count++;
				break;
			case WDB_TOK_TREE:
				arg_count++;
				if( !dp && !strcmp( comb_name, tok->tp->tr_l.tl_name ) )
					circular_ref++;
				else if( db_lookup( dbip, tok->tp->tr_l.tl_name, LOOKUP_QUIET ) == DIR_NULL )
					Tcl_AppendResult(interp, "WARNING: '",
						tok->tp->tr_l.tl_name,
						"' does not actually exist\n", (char *)NULL );
				break;
		}
		if( paren_count < 0 )
			paren_error++;
		last_tok = tok->type;
	}

	if( paren_count || paren_error )
	{
		Tcl_AppendResult(interp, "ERROR: unbalanced parenthesis\n", (char *)NULL );
		errors++;
	}

	if( missing_exp )
	{
		Tcl_AppendResult(interp, "ERROR: empty parenthesis (missing expression)\n", (char *)NULL );
		errors++;
	}

	if( missing_op )
	{
		Tcl_AppendResult(interp, "ERROR: must have operator between ')('\n", (char *)NULL );
		errors++;
	}

	if( op_count != arg_count-1 )
	{
		Tcl_AppendResult(interp, "ERROR: mismatch of operators and operands\n", (char *)NULL );
		errors++;
	}

	if( circular_ref )
	{
		Tcl_AppendResult(interp, "ERROR: combination cannot reference itself during initial creation\n", (char *)NULL );
		errors++;
	}

	if( errors )
	{
		Tcl_AppendResult(interp, "\t---------aborting!\n", (char *)NULL );
		return( 1 );
	}

	return( 0 );
}

int
wdb_comb_std_cmd(struct rt_wdb	*wdbp,
		Tcl_Interp	*interp,
		int		argc,
		char 		**argv)
{
	char				*comb_name;
	int				ch;
	int				region_flag = -1;
	register struct directory	*dp;
    	struct rt_db_internal		intern;
	struct rt_comb_internal		*comb = NULL;
	struct tokens			tok_hd;
	struct tokens			*tok;
	short				last_tok;
	int				i;
	union tree			*final_tree;

	if (wdbp->dbip->dbi_read_only) {
		Tcl_AppendResult(interp, "Database is read-only!\n", (char *)NULL);
		return TCL_ERROR;
	}

	if (argc < 3 || RT_MAXARGS < argc) {
	  struct bu_vls vls;

	  bu_vls_init(&vls);
	  bu_vls_printf(&vls, "helplib_alias wdb_comb_std %s", argv[0]);
	  Tcl_Eval(interp, bu_vls_addr(&vls));
	  bu_vls_free(&vls);
	  return TCL_ERROR;
	}

	/* Parse options */
	bu_optind = 1;	/* re-init bu_getopt() */
	while ((ch = bu_getopt(argc, argv, "cgr?")) != EOF) {
		switch (ch) {
		case 'c':
		case 'g':
			region_flag = 0;
			break;
		case 'r':
			region_flag = 1;
			break;
		/* XXX How about -p and -v for FASTGEN? */
		case '?':
		default:
			PRINT_USAGE;
			return TCL_OK;
		}
	}
	argc -= (bu_optind + 1);
	argv += bu_optind;

	comb_name = *argv++;
	if (argc == -1) {
		PRINT_USAGE;
		return TCL_OK;
	}

	if ((region_flag != -1) && (argc == 0)) {
		/*
		 *	Set/Reset the REGION flag of an existing combination
		 */
		if ((dp = db_lookup(wdbp->dbip, comb_name, LOOKUP_NOISY)) == DIR_NULL)
			return TCL_ERROR;

		if (!(dp->d_flags & DIR_COMB)) {
			Tcl_AppendResult(interp, comb_name, " is not a combination\n", (char *)0 );
			return TCL_ERROR;
		}

		if (rt_db_get_internal(&intern, dp, wdbp->dbip, (fastf_t *)NULL, &rt_uniresource) < 0) {
			Tcl_AppendResult(interp, "Database read error, aborting\n", (char *)NULL);	
			return TCL_ERROR;
		}
		comb = (struct rt_comb_internal *)intern.idb_ptr;
		RT_CK_COMB(comb);

		if (region_flag) {
			if( !comb->region_flag ) {
				/* assign values from the defaults */
				comb->region_id = wdbp->wdb_item_default++;
				comb->aircode = wdbp->wdb_air_default;
				comb->GIFTmater = wdbp->wdb_mat_default;
				comb->los = wdbp->wdb_los_default;
			}
			comb->region_flag = 1;
		}
		else
			comb->region_flag = 0;

		if (rt_db_put_internal(dp, wdbp->dbip, &intern, &rt_uniresource) < 0) {
			rt_db_free_internal(&intern, &rt_uniresource);
			Tcl_AppendResult(interp, "Database write error, aborting\n", (char *)NULL);
			return TCL_ERROR;
		}

		return TCL_OK;
	}
	/*
	 *	At this point, we know we have a Boolean expression.
	 *	If the combination already existed and region_flag is -1,
	 *	then leave its region_flag alone.
	 *	If the combination didn't exist yet,
	 *	then pretend region_flag was 0.
	 *	Otherwise, make sure to set its c_flags according to region_flag.
	 */

	dp = db_lookup( wdbp->dbip, comb_name, LOOKUP_QUIET );
	if (dp != DIR_NULL) {
		Tcl_AppendResult(interp, "ERROR: ", comb_name, " already exists\n", (char *)0 );
		return TCL_ERROR;
	}

	/* parse Boolean expression */
	BU_LIST_INIT(&tok_hd.l);
	tok_hd.type = WDB_TOK_NULL;

	last_tok = WDB_TOK_LPAREN;
	for (i=0 ; i<argc ; i++) {
		char *ptr;

		ptr = argv[i];
		while (*ptr) {
			while (*ptr == '(' || *ptr == ')') {
				switch (*ptr) {
				case '(':
					wdb_append_lparen( &tok_hd.l );
					last_tok = WDB_TOK_LPAREN;
					break;
				case ')':
					wdb_append_rparen( &tok_hd.l );
					last_tok = WDB_TOK_RPAREN;
					break;
				}
				ptr++;
			}

			if (*ptr == '\0')
				continue;

			if (last_tok == WDB_TOK_RPAREN) {
				/* next token MUST be an operator */
				if (wdb_add_operator(interp, &tok_hd.l, *ptr, &last_tok) == TCL_ERROR) {
					wdb_free_tokens(&tok_hd.l);
					if (dp != DIR_NULL)
						rt_db_free_internal(&intern, &rt_uniresource);
					return TCL_ERROR;
				}
				ptr++;
			} else if (last_tok == WDB_TOK_LPAREN) {
				/* next token MUST be an operand */
				int name_len;

				name_len = wdb_add_operand(interp, &tok_hd.l, ptr );
				if (name_len < 1) {
					wdb_free_tokens(&tok_hd.l);
					if (dp != DIR_NULL)
						rt_db_free_internal(&intern, &rt_uniresource);
					return TCL_ERROR;
				}
				last_tok = WDB_TOK_TREE;
				ptr += name_len;
			} else if (last_tok == WDB_TOK_TREE) {
				/* must be an operator */
				if (wdb_add_operator(interp, &tok_hd.l, *ptr, &last_tok) == TCL_ERROR) {
					wdb_free_tokens(&tok_hd.l);
					if (dp != DIR_NULL)
						rt_db_free_internal(&intern, &rt_uniresource);
					return TCL_ERROR;
				}
				ptr++;
			} else if (last_tok == WDB_TOK_UNION ||
				   last_tok == WDB_TOK_INTER ||
				   last_tok == WDB_TOK_SUBTR) {
				/* must be an operand */
				int name_len;

				name_len = wdb_add_operand(interp, &tok_hd.l, ptr );
				if (name_len < 1) {
					wdb_free_tokens(&tok_hd.l);
					if (dp != DIR_NULL)
						rt_db_free_internal(&intern, &rt_uniresource);
					return TCL_ERROR;
				}
				last_tok = WDB_TOK_TREE;
				ptr += name_len;
			}
		}
	}

	if (wdb_check_syntax(interp, wdbp->dbip, &tok_hd.l, comb_name, dp)) {
		wdb_free_tokens(&tok_hd.l);
		return TCL_ERROR;
	}

	/* replace any occurences of comb_name with existing tree */
	if (dp != DIR_NULL) {
		for (BU_LIST_FOR(tok, tokens, &tok_hd.l)) {
			struct rt_db_internal intern1;
			struct rt_comb_internal *comb1;

			switch (tok->type) {
			case WDB_TOK_LPAREN:
			case WDB_TOK_RPAREN:
			case WDB_TOK_UNION:
			case WDB_TOK_INTER:
			case WDB_TOK_SUBTR:
				break;
			case WDB_TOK_TREE:
				if (!strcmp(tok->tp->tr_l.tl_name, comb_name)) {
					db_free_tree( tok->tp, &rt_uniresource );
					if (rt_db_get_internal(&intern1, dp, wdbp->dbip, (fastf_t *)NULL, &rt_uniresource) < 0) {
						Tcl_AppendResult(interp, "Cannot get records for ", comb_name, "\n" );
						Tcl_AppendResult(interp, "Database read error, aborting\n", (char *)NULL);
						return TCL_ERROR;
					}
					comb1 = (struct rt_comb_internal *)intern1.idb_ptr;
					RT_CK_COMB(comb1);

					tok->tp = comb1->tree;
					comb1->tree = (union tree *)NULL;
					rt_db_free_internal(&intern1, &rt_uniresource);
				}
				break;
			default:
				Tcl_AppendResult(interp, "ERROR: Unrecognized token type\n", (char *)NULL);
				wdb_free_tokens(&tok_hd.l);
				return TCL_ERROR;
			}
		}
	}

	final_tree = wdb_eval_bool(&tok_hd.l);

	if (dp == DIR_NULL) {
		int flags;

		flags = DIR_COMB;
		BU_GETSTRUCT(comb, rt_comb_internal);
		comb->magic = RT_COMB_MAGIC;
		comb->tree = final_tree;
		bu_vls_init(&comb->shader);
		bu_vls_init(&comb->material);
		comb->region_id = -1;
		if (region_flag == (-1))
			comb->region_flag = 0;
		else
			comb->region_flag = region_flag;

		if (comb->region_flag) {
			struct bu_vls tmp_vls;

			comb->region_flag = 1;
			comb->region_id = wdbp->wdb_item_default++;;
			comb->aircode = wdbp->wdb_air_default;
			comb->los = wdbp->wdb_los_default;
			comb->GIFTmater = wdbp->wdb_mat_default;
			bu_vls_init(&tmp_vls);
			bu_vls_printf(&tmp_vls,
				"Creating region id=%d, air=%d, los=%d, GIFTmaterial=%d\n",
				comb->region_id, comb->aircode, comb->los, comb->GIFTmater);
			Tcl_AppendResult(interp, bu_vls_addr(&tmp_vls), (char *)NULL);
			bu_vls_free(&tmp_vls);

			flags |= DIR_REGION;
		}

		RT_INIT_DB_INTERNAL(&intern);
		intern.idb_major_type = DB5_MAJORTYPE_BRLCAD;
		intern.idb_type = ID_COMBINATION;
		intern.idb_meth = &rt_functab[ID_COMBINATION];
		intern.idb_ptr = (genptr_t)comb;

		if ((dp=db_diradd(wdbp->dbip, comb_name, -1L, 0, flags, (genptr_t)&intern.idb_type)) == DIR_NULL) {
			Tcl_AppendResult(interp, "Failed to add ", comb_name,
					 " to directory, aborting\n" , (char *)NULL);
			return TCL_ERROR;
		}

		if (rt_db_put_internal(dp, wdbp->dbip, &intern, &rt_uniresource) < 0) {
			Tcl_AppendResult(interp, "Failed to write ", dp->d_namep, (char *)NULL );
			return TCL_ERROR;
		}
	} else {
		db_delete(wdbp->dbip, dp);

		dp->d_len = 0;
		dp->d_un.file_offset = -1;
		db_free_tree(comb->tree, &rt_uniresource);
		comb->tree = final_tree;

		if (rt_db_put_internal(dp, wdbp->dbip, &intern, &rt_uniresource) < 0) {
			Tcl_AppendResult(interp, "Failed to write ", dp->d_namep, (char *)NULL );
			return TCL_ERROR;
		}
	}

	return TCL_OK;
}

/*
 * Input a combination in standard set-theoretic notation.
 *
 * Usage:
 *        procname c [-gr] comb_name boolean_expr
 *
 * NON-PARALLEL because of rt_uniresource
 */
int
wdb_comb_std_tcl(ClientData	clientData,
		 Tcl_Interp	*interp,
		 int     	argc,
		 char    	**argv)
{
	struct rt_wdb *wdbp = (struct rt_wdb *)clientData;

	return wdb_comb_std_cmd(wdbp, interp, argc-1, argv+1);
}

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
