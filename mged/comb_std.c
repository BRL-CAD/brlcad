/*			C O M B _ S T D . C
 *
 *	Code to impliement the "c" command
 *
 *  Author -
 *      John R. Anderson
 *
 *  Source -
 *      The U. S. Army Research Laboratory
 *      Aberdeen Proving Ground, Maryland  21005-5068  USA
 *
 *  Distribution Notice -
 *      Re-distribution of this software is restricted, as described in
 *      your "Statement of Terms and Conditions for the Release of
 *      The BRL-CAD Pacakge" agreement.
 *
 *  Copyright Notice -
 *      This software is Copyright (C) 1997 by the United States Army
 *      in all countries except the USA.  All rights reserved.
 */

#ifndef lint
static char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "conf.h"

#include <stdio.h>
#ifdef USE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif

#include "machine.h"
#include "vmath.h"
#include "raytrace.h"
#include "rtgeom.h"
#include "./ged.h"

#define	PRINT_USAGE Tcl_AppendResult(interp, "c: usage 'c [-gr] comb_name [bool_expr]'\n",\
				     (char *)NULL)

struct tokens
{
	struct bu_list		l;
	short			type;
	union tree		*tp;
};

/* token types */
#define	TOK_NULL	0
#define	TOK_LPAREN	1
#define	TOK_RPAREN	2
#define	TOK_UNION	3
#define	TOK_INTER	4
#define	TOK_SUBTR	5
#define	TOK_TREE	6

HIDDEN void
Free_tokens( hp )
struct bu_list *hp;
{
	struct tokens *tok;

	BU_CK_LIST_HEAD( hp );

	while( BU_LIST_WHILE( tok, tokens, hp ) )
	{
		BU_LIST_DEQUEUE( &tok->l );
		if( tok->type == TOK_TREE )
		{
			if( tok->tp )
				db_free_tree( tok->tp );
		}
	}
}

HIDDEN void
Append_union( hp )
struct bu_list *hp;
{
	struct tokens *tok;

	BU_CK_LIST_HEAD( hp );

	tok = (struct tokens *)bu_malloc( sizeof( struct tokens ), "tok" );
	tok->type = TOK_UNION;
	tok->tp = (union tree *)NULL;
	BU_LIST_INSERT( hp, &tok->l );
}

HIDDEN void
Append_inter( hp )
struct bu_list *hp;
{
	struct tokens *tok;

	BU_CK_LIST_HEAD( hp );

	tok = (struct tokens *)bu_malloc( sizeof( struct tokens ), "tok" );
	tok->type = TOK_INTER;
	tok->tp = (union tree *)NULL;
	BU_LIST_INSERT( hp, &tok->l );
}

HIDDEN void
Append_subtr( hp )
struct bu_list *hp;
{
	struct tokens *tok;

	BU_CK_LIST_HEAD( hp );

	tok = (struct tokens *)bu_malloc( sizeof( struct tokens ), "tok" );
	tok->type = TOK_SUBTR;
	tok->tp = (union tree *)NULL;
	BU_LIST_INSERT( hp, &tok->l );
}

HIDDEN void
Append_lparen( hp )
struct bu_list *hp;
{
	struct tokens *tok;

	BU_CK_LIST_HEAD( hp );

	tok = (struct tokens *)bu_malloc( sizeof( struct tokens ), "tok" );
	tok->type = TOK_LPAREN;
	tok->tp = (union tree *)NULL;
	BU_LIST_INSERT( hp, &tok->l );
}

HIDDEN void
Append_rparen( hp )
struct bu_list *hp;
{
	struct tokens *tok;

	BU_CK_LIST_HEAD( hp );

	tok = (struct tokens *)bu_malloc( sizeof( struct tokens ), "tok" );
	tok->type = TOK_RPAREN;
	tok->tp = (union tree *)NULL;
	BU_LIST_INSERT( hp, &tok->l );
}

HIDDEN int
Add_operator( hp, ch, last_tok )
struct bu_list *hp;
char ch;
short *last_tok;
{
	char illegal[2];

	BU_CK_LIST_HEAD( hp );

	switch( ch )
	{
		case 'u':
			Append_union( hp );
			*last_tok = TOK_UNION;
			break;
		case '+':
			Append_inter( hp );
			*last_tok = TOK_INTER;
			break;
		case '-':
			Append_subtr( hp );
			*last_tok = TOK_SUBTR;
			break;
		default:
			illegal[0] = ch;
			illegal[1] = '\0';
			Tcl_AppendResult(interp, "Illegal operator: ", illegal,
				", aborting\n", (char *)NULL );
			Free_tokens( hp );
			return TCL_ERROR;
	}
	return TCL_OK;
}

HIDDEN int
Add_operand( hp, name )
struct bu_list *hp;
char *name;
{
	char *ptr_lparen;
	char *ptr_rparen;
	int name_len;
	union tree *node;
	struct tokens *tok;

	BU_CK_LIST_HEAD( hp );

	ptr_lparen = strchr( name, '(' );
	ptr_rparen = strchr( name, ')' );

	BU_GETUNION( node, tree );
	node->magic = RT_TREE_MAGIC;
	node->tr_op = OP_DB_LEAF;
	node->tr_l.tl_mat = (matp_t)NULL;
	if( ptr_lparen || ptr_rparen )
	{
		int tmp1,tmp2;

		if( ptr_rparen )
			tmp1 = ptr_rparen - name;
		else
			tmp1 = (-1);
		if( ptr_lparen )
			tmp2 = ptr_lparen - name;
		else
			tmp2 = (-1);

		if( tmp2 == (-1) && tmp1 > 0 )
			name_len = tmp1;
		else if( tmp1 == (-1) && tmp2 > 0 )
			name_len = tmp2;
		else if( tmp1 > 0 && tmp2 > 0 )
		{
			if( tmp1 < tmp2 )
				name_len = tmp1;
			else
				name_len = tmp2;
		}
		else
		{
			Tcl_AppendResult(interp, "Cannot determine length of operand name: ",
				name, ", aborting\n", (char *)NULL );
			return( 0 );
		}
	}
	else
		name_len = strlen( name );

	node->tr_l.tl_name = (char *)bu_malloc( name_len + 1, "node name" );
	strncpy( node->tr_l.tl_name, name, name_len );
	node->tr_l.tl_name[name_len] = '\0';
	tok = (struct tokens *)bu_malloc( sizeof( struct tokens ), "tok" );
	tok->type = TOK_TREE;
	tok->tp = node;
	BU_LIST_INSERT( hp, &tok->l );
	return( name_len );
}

HIDDEN void
Do_inter( hp )
struct bu_list *hp;
{
	struct tokens *tok;

	for( BU_LIST_FOR( tok, tokens, hp ) )
	{
		struct tokens *prev, *next;
		union tree *tp;

		if( tok->type != TOK_INTER )
			continue;

		prev = BU_LIST_PREV( tokens, &tok->l );
		next = BU_LIST_NEXT( tokens, &tok->l );

		if( prev->type !=TOK_TREE || next->type != TOK_TREE )
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
Do_union_subtr( hp )
struct bu_list *hp;
{
	struct tokens *tok;

	for( BU_LIST_FOR( tok, tokens, hp ) )
	{
		struct tokens *prev, *next;
		union tree *tp;

		if( tok->type != TOK_UNION && tok->type != TOK_SUBTR )
			continue;

		prev = BU_LIST_PREV( tokens, &tok->l );
		next = BU_LIST_NEXT( tokens, &tok->l );

		if( prev->type !=TOK_TREE || next->type != TOK_TREE )
			continue;

		/* this is an eligible operation */
		tp = (union tree *)bu_malloc( sizeof( union tree ), "tp" );
		tp->magic = RT_TREE_MAGIC;
		if( tok->type == TOK_UNION )
			tp->tr_b.tb_op = OP_UNION;
		else
			tp->tr_b.tb_op = OP_SUBTRACT;
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

HIDDEN int
Do_paren( hp )
struct bu_list *hp;
{
	struct tokens *tok;

	for( BU_LIST_FOR( tok, tokens, hp ) )
	{
		struct tokens *prev, *next;
		union tree *tp;

		if( tok->type != TOK_TREE )
			continue;

		prev = BU_LIST_PREV( tokens, &tok->l );
		next = BU_LIST_NEXT( tokens, &tok->l );

		if( prev->type !=TOK_LPAREN || next->type != TOK_RPAREN )
			continue;

		/* this is an eligible operand surrounded by parens */
		BU_LIST_DEQUEUE( &next->l );
		bu_free( (char *)next, "next" );
		BU_LIST_DEQUEUE( &prev->l );
		bu_free( (char *)prev, "prev" );
	}

	if( hp->forw == hp->back && hp->forw != hp )
		return( 1 );	/* done */
	else if( BU_LIST_IS_EMPTY( hp ) )
		return( -1 );	/* empty tree!!!! */
	else
		return( 0 );	/* more to do */
		
}

HIDDEN union tree *
Eval_bool( hp )
struct bu_list *hp;
{
	int done=0;
	union tree *final_tree;
	struct tokens *tok;

	while( done != 1 )
	{
		Do_inter( hp );
		Do_union_subtr( hp );
		done = Do_paren( hp );
	}

	if( done == 1 )
	{
		tok = BU_LIST_NEXT( tokens, hp );
		final_tree = tok->tp;
		BU_LIST_DEQUEUE( &tok->l );
		bu_free( (char *)tok, "tok" );
		return( final_tree );
	}
#if 0
	if( done == (-1) )
#endif
        return( (union tree *)NULL );
}

HIDDEN int
Check_syntax( hp, comb_name, dp )
struct bu_list *hp;
char *comb_name;
struct directory *dp;
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
	short last_tok=TOK_NULL;

	if(dbip == DBI_NULL)
	  return 0;

	for( BU_LIST_FOR( tok, tokens, hp ) )
	{
		switch( tok->type )
		{
			case TOK_LPAREN:
				paren_count++;
				if( last_tok == TOK_RPAREN )
					missing_op++;
				break;
			case TOK_RPAREN:
				paren_count--;
				if( last_tok == TOK_LPAREN )
					missing_exp++;
				break;
			case TOK_UNION:
			case TOK_SUBTR:
			case TOK_INTER:
				op_count++;
				break;
			case TOK_TREE:
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

/*
 *		    F _ C O M B _ S T D ( )
 *
 *	Input a combination in standard set-theoetic notation
 *
 *	Syntax: c [-gr] comb_name [boolean_expr]
 */
int
f_comb_std(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int	argc;
char	**argv;
{
	char				*comb_name;
	int				ch;
	int				region_flag = -1;
	register struct directory	*dp;
    	struct rt_db_internal		intern;
	struct rt_comb_internal		*comb;
	extern int			bu_optind;
	extern char			*bu_optarg;
	struct tokens			tok_hd;
	struct tokens			*tok;
	short				last_tok;
	int				i;
	union tree			*final_tree;

	if(dbip == DBI_NULL)
	  return TCL_OK;

	CHECK_READ_ONLY;

	if(argc < 3 || MAXARGS < argc){
	  struct bu_vls vls;

	  bu_vls_init(&vls);
	  bu_vls_printf(&vls, "help c");
	  Tcl_Eval(interp, bu_vls_addr(&vls));
	  bu_vls_free(&vls);
	  return TCL_ERROR;
	}

	/* Parse options */
	bu_optind = 1;	/* re-init bu_getopt() */
	while ((ch = bu_getopt(argc, argv, "gr?")) != EOF)
	{
		switch (ch)
		{
		  case 'g':
			region_flag = 0;
			break;
		  case 'r':
			region_flag = 1;
			break;
		  case '?':
			  default:
			PRINT_USAGE;
			return TCL_OK;
		}
	}
	argc -= (bu_optind + 1);
	argv += bu_optind;

	comb_name = *argv++;
	if (argc == -1)
	{
		PRINT_USAGE;
		return TCL_OK;
	}

	if ((region_flag != -1) && (argc == 0))
	{
		/*
		 *	Set/Reset the REGION flag of an existing combination
		 */
		if ((dp = db_lookup(dbip, comb_name, LOOKUP_NOISY)) == DIR_NULL)
			return TCL_ERROR;

		if( !(dp->d_flags & DIR_COMB) )
		{
			Tcl_AppendResult(interp, comb_name, " is not a combination\n", (char *)0 );
			return TCL_ERROR;
		}

		if( rt_db_get_internal( &intern, dp, dbip, (mat_t *)NULL ) < 0 )
			TCL_READ_ERR_return;
		comb = (struct rt_comb_internal *)intern.idb_ptr;
		RT_CK_COMB( comb );

		if( region_flag )
			comb->region_flag = 1;
		else
			comb->region_flag = 0;

		if( rt_db_put_internal( dp, dbip, &intern ) < 0 )
		{
			rt_comb_ifree( &intern );
			TCL_WRITE_ERR_return;
		}

		return TCL_OK;
	}
	/*
	 *	At this point, we know we have a Boolean expression.
	 *	If the combination already existed and region_flag is -1,
	 *	then leave its c_flags alone.
	 *	If the combination didn't exist yet,
	 *	then pretend region_flag was 0.
	 *	Otherwise, make sure to set its c_flags according to region_flag.
	 */

	dp = db_lookup( dbip, comb_name, LOOKUP_QUIET );
	if( dp != DIR_NULL )
	{
		Tcl_AppendResult(interp, "ERROR: ", comb_name, " already exists\n", (char *)0 );
		return TCL_ERROR;
	}

	/* parse Boolean expression */
	BU_LIST_INIT( &tok_hd.l );
	tok_hd.type = TOK_NULL;

	last_tok = TOK_LPAREN;
	for( i=0 ; i<argc ; i++ )
	{
		char *ptr;
		char *ptr_next;
		union tree *node;

		ptr = argv[i];
		while( *ptr )
		{
			while( *ptr == '(' || *ptr == ')' )
			{
				switch( *ptr )
				{
					case '(':
						Append_lparen( &tok_hd.l );
						last_tok = TOK_LPAREN;
						break;
					case ')':
						Append_rparen( &tok_hd.l );
						last_tok = TOK_RPAREN;
						break;
				}
				ptr++;
			}

			if( *ptr == '\0' )
				continue;

			if( last_tok == TOK_RPAREN )
			{
				/* next token MUST be an operator */
				if( Add_operator( &tok_hd.l, *ptr, &last_tok ) == TCL_ERROR )
				{
					Free_tokens( &tok_hd.l );
					if( dp != DIR_NULL )
						rt_comb_ifree( &intern );
					return TCL_ERROR;
				}
				ptr++;
			}
			else if( last_tok == TOK_LPAREN )
			{
				/* next token MUST be an operand */
				int name_len;

				name_len = Add_operand( &tok_hd.l, ptr );
				if( name_len < 1 )
				{
					Free_tokens( &tok_hd.l );
					if( dp != DIR_NULL )
						rt_comb_ifree( &intern );
					return TCL_ERROR;
				}
				last_tok = TOK_TREE;
				ptr += name_len;
			}
			else if( last_tok == TOK_TREE )
			{
				/* must be an operator */
				if( Add_operator( &tok_hd.l, *ptr, &last_tok ) == TCL_ERROR )
				{
					Free_tokens( &tok_hd.l );
					if( dp != DIR_NULL )
						rt_comb_ifree( &intern );
					return TCL_ERROR;
				}
				ptr++;
			}
			else if( last_tok == TOK_UNION ||
				 last_tok == TOK_INTER ||
				 last_tok == TOK_SUBTR )
			{
				/* must be an operand */
				int name_len;

				name_len = Add_operand( &tok_hd.l, ptr );
				if( name_len < 1 )
				{
					Free_tokens( &tok_hd.l );
					if( dp != DIR_NULL )
						rt_comb_ifree( &intern );
					return TCL_ERROR;
				}
				last_tok = TOK_TREE;
				ptr += name_len;
			}
		}
	}

#if 0
	Tcl_AppendResult(interp, "parsed tree:\n", (char *)NULL );
	for( BU_LIST_FOR( tok, tokens, &tok_hd.l ) )
	{
		switch( tok->type )
		{
			case TOK_LPAREN:
				Tcl_AppendResult(interp, " ( ", (char *)NULL );
				break;
			case TOK_RPAREN:
				Tcl_AppendResult(interp, " ) ", (char *)NULL );
				break;
			case TOK_UNION:
				Tcl_AppendResult(interp, " u ", (char *)NULL );
				break;
			case TOK_INTER:
				Tcl_AppendResult(interp, " + ", (char *)NULL );
				break;
			case TOK_SUBTR:
				Tcl_AppendResult(interp, " - ", (char *)NULL );
				break;
			case TOK_TREE:
				Tcl_AppendResult(interp, tok->tp->tr_l.tl_name, (char *)NULL );
				break;
			default:
				Tcl_AppendResult(interp, "\nUnrecognized token type\n", (char *)NULL );
				break;
		}
	}
	Tcl_AppendResult(interp, "\n", (char *)NULL );
#endif

	if( Check_syntax( &tok_hd.l, comb_name, dp ) )
	{
		Free_tokens( &tok_hd.l );
		return TCL_ERROR;
#if 0
		if( dp != DIR_NULL )
			rt_comb_ifree( &intern );
#endif
	}

	/* replace any occurences of comb_name with existing tree */
	if( dp != DIR_NULL )
	{
		for( BU_LIST_FOR( tok, tokens, &tok_hd.l ) )
		{
			struct rt_db_internal intern1;
			struct rt_comb_internal *comb1;

			switch( tok->type )
			{
				case TOK_LPAREN:
				case TOK_RPAREN:
				case TOK_UNION:
				case TOK_INTER:
				case TOK_SUBTR:
					break;
				case TOK_TREE:
					if( !strcmp( tok->tp->tr_l.tl_name, comb_name ) )
					{
						db_free_tree( tok->tp );
						if( rt_db_get_internal( &intern1, dp, dbip, (mat_t *)NULL ) < 0 )
						{
							Tcl_AppendResult(interp, "Cannot get records for ", comb_name, "\n" );
							TCL_READ_ERR_return;
						}
						comb1 = (struct rt_comb_internal *)intern1.idb_ptr;
						RT_CK_COMB( comb1 );

						tok->tp = comb1->tree;
						comb1->tree = (union tree *)NULL;
						rt_comb_ifree( &intern1 );
					}
				break;
				default:
					Tcl_AppendResult(interp, "ERROR: Unrecognized token type\n", (char *)NULL );
					Free_tokens( &tok_hd.l );
					return TCL_ERROR;
			}
		}
	}

	final_tree = Eval_bool( &tok_hd.l );

#if 0
	{
		struct bu_vls vls;

		Tcl_AppendResult(interp, "evaluated tree:\n", (char *)NULL );
		bu_vls_init( &vls );
		db_tree_describe( &vls, final_tree, 0, 0, 1,0 );
		Tcl_AppendResult(interp, bu_vls_addr( &vls ), (char *)NULL );
		bu_vls_free( &vls );
	}
#endif
	if( dp == DIR_NULL )
	{
		BU_GETSTRUCT( comb, rt_comb_internal );
		comb->magic = RT_COMB_MAGIC;
		comb->tree = final_tree;
		bu_vls_init( &comb->shader );
		bu_vls_init( &comb->material );
		comb->region_id = -1;
		if( region_flag == (-1) )
			comb->region_flag = 0;
		else
			comb->region_flag = region_flag;

		if( comb->region_flag )
		{
			struct bu_vls tmp_vls;

			comb->region_flag = 1;
			comb->region_id = item_default++;;
			comb->aircode = air_default;
			comb->los = los_default;
			comb->GIFTmater = mat_default;
			bu_vls_init(&tmp_vls);
			bu_vls_printf(&tmp_vls,
				"Creating region id=%d, air=%d, los=%d, GIFTmaterial=%d\n",
				comb->region_id, comb->aircode, comb->los, comb->GIFTmater );
			Tcl_AppendResult(interp, bu_vls_addr(&tmp_vls), (char *)NULL);
			bu_vls_free(&tmp_vls);
		}

		RT_INIT_DB_INTERNAL( &intern );
		intern.idb_type = ID_COMBINATION;
		intern.idb_ptr = (genptr_t)comb;

		if( (dp=db_diradd( dbip, comb_name, -1L, 0, DIR_COMB )) == DIR_NULL )
		{
			Tcl_AppendResult(interp, "Failed to add ", comb_name,
				" to directory, aborting\n" , (char *)NULL );
			TCL_ERROR_RECOVERY_SUGGESTION;
			return TCL_ERROR;
		}

		if( rt_db_put_internal( dp, dbip, &intern ) < 0 )
		{
			Tcl_AppendResult(interp, "Failed to write ", dp->d_namep, (char *)NULL );
			return TCL_ERROR;
		}
	}
	else
	{
		db_delete( dbip, dp );

		dp->d_len = 0;
		dp->d_un.file_offset = -1;
		db_free_tree( comb->tree );
		comb->tree = final_tree;

		if( rt_db_put_internal( dp, dbip, &intern ) < 0 )
		{
			Tcl_AppendResult(interp, "Failed to write ", dp->d_namep, (char *)NULL );
			return TCL_ERROR;
		}
	}

	return TCL_OK;
}
