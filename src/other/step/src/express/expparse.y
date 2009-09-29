%{

static char rcsid[] = "$Id: expparse.y,v 1.23 1997/11/14 17:09:04 libes Exp $";

/*
 * YACC grammar for Express parser.
 *
 * This software was developed by U.S. Government employees as part of
 * their official duties and is not subject to copyright.
 *
 * $Log: expparse.y,v $
 * Revision 1.23  1997/11/14 17:09:04  libes
 * allow multiple group references
 *
 * Revision 1.22  1997/01/21 19:35:43  dar
 * made C++ compatible
 *
 * Revision 1.21  1995/06/08  22:59:59  clark
 * bug fixes
 *
 * Revision 1.20  1995/04/08  21:06:07  clark
 * WHERE rule resolution bug fixes, take 2
 *
 * Revision 1.19  1995/04/08  20:54:18  clark
 * WHERE rule resolution bug fixes
 *
 * Revision 1.19  1995/04/08  20:49:08  clark
 * WHERE
 *
 * Revision 1.18  1995/04/05  13:55:40  clark
 * CADDETC preval
 *
 * Revision 1.17  1995/03/09  18:43:47  clark
 * various fixes for caddetc - before interface clause changes
 *
 * Revision 1.16  1994/11/29  20:55:58  clark
 * fix inline comment bug
 *
 * Revision 1.15  1994/11/22  18:32:39  clark
 * Part 11 IS; group reference
 *
 * Revision 1.14  1994/11/10  19:20:03  clark
 * Update to IS
 *
 * Revision 1.13  1994/05/11  19:50:00  libes
 * numerous fixes
 *
 * Revision 1.12  1993/10/15  18:47:26  libes
 * CADDETC certified
 *
 * Revision 1.10  1993/03/19  20:53:57  libes
 * one more, with feeling
 *
 * Revision 1.9  1993/03/19  20:39:51  libes
 * added unique to parameter types
 *
 * Revision 1.8  1993/02/16  03:17:22  libes
 * reorg'd alg bodies to not force artificial begin/ends
 * added flag to differentiate parameters in scopes
 * rewrote query to fix scope handling
 * added support for Where type
 *
 * Revision 1.7  1993/01/19  22:44:17  libes
 * *** empty log message ***
 *
 * Revision 1.6  1992/08/27  23:36:35  libes
 * created fifo for new schemas that are parsed
 * connected entity list to create of oneof
 *
 * Revision 1.5  1992/08/18  17:11:36  libes
 * rm'd extraneous error messages
 *
 * Revision 1.4  1992/06/08  18:05:20  libes
 * prettied up interface to print_objects_when_running
 *
 * Revision 1.3  1992/05/31  23:31:13  libes
 * implemented ALIAS resolution
 *
 * Revision 1.2  1992/05/31  08:30:54  libes
 * multiple files
 *
 * Revision 1.1  1992/05/28  03:52:25  libes
 * Initial revision
 */

#include "express/symbol.h"
#include "express/linklist.h"
#include "stack.h"
#include "express/express.h"
#include "express/schema.h"
#include "express/entity.h"
#include "express/resolve.h"

extern int print_objects_while_running;

int tag_count;	/* use this to count tagged GENERIC types in the formal */
		/* argument lists.  Gross, but much easier to do it this */
		/* way then with the 'help' of yacc. */
		/* Set it to -1 to indicate that tags cannot be defined, */
		/* only used (outside of formal parameter list, i.e. for */
		/* return types and local variables).  Hey, as long as */
		/* there's a gross hack sitting around, we might as well */
		/* milk it for all it's worth!  -snc */

/*SUPPRESS 61*/
/* Type current_type;*/	/* pass type placeholder down */
		/* this allows us to attach a dictionary to it only when */
		/* we decide we absolutely need one */

Express yyexpresult;	/* hook to everything built by parser */

Symbol *interface_schema;	/* schema of interest in use/ref clauses */
void (*interface_func)();	/* func to attach rename clauses */

/* record schemas found in a single parse here, allowing them to be */
/* differentiated from other schemas parsed earlier */
Linked_List PARSEnew_schemas;

extern int	yylineno;

static int	PARSEchunk_start;

static void	yyerror PROTO((char*));
static void	yyerror2 PROTO((char CONST *));

Boolean		yyeof = False;

#ifdef FLEX
extern char	*yytext;
#else /* LEX */
extern char	yytext[];
#endif /*FLEX*/

#define MAX_SCOPE_DEPTH	20	/* max number of scopes that can be nested */

static struct scope {
	struct Scope_ *this_;
	char type;	/* one of OBJ_XXX */
	struct scope *pscope;	/* pointer back to most recent scope */
				/* that has a printable name - for better */
				/* error messages */
} scopes[MAX_SCOPE_DEPTH], *scope;
#define CURRENT_SCOPE (scope->this_)
#define PREVIOUS_SCOPE ((scope-1)->this_)
#define CURRENT_SCHEMA (scope->this_->u.schema)
#define CURRENT_SCOPE_NAME		(OBJget_symbol(scope->pscope->this_,scope->pscope->type)->name)
#define CURRENT_SCOPE_TYPE_PRINTABLE	(OBJget_type(scope->pscope->type))

/* ths = new scope to enter */
/* sym = name of scope to enter into parent.  Some scopes (i.e., increment) */
/*       are not named, in which case sym should be 0 */
/*	 This is useful for when a diagnostic is printed, an earlier named */
/* 	 scoped can be used */
/* typ = type of scope */
#define PUSH_SCOPE(ths,sym,typ) \
	if (sym) DICTdefine(scope->this_->symbol_table,(sym)->name,(Generic)ths,sym,typ);\
	ths->superscope = scope->this_; \
	scope++;		\
	scope->type = typ;	\
	scope->pscope = (sym?scope:(scope-1)->pscope); \
	scope->this_ = ths; \
	if (sym) { \
		ths->symbol = *(sym); \
	}
#define POP_SCOPE() scope--

/* PUSH_SCOPE_DUMMY just pushes the scope stack with nothing actually on it */
/* Necessary for situations when a POP_SCOPE is unnecessary but inevitable */
#define PUSH_SCOPE_DUMMY() scope++

/* normally the superscope is added by PUSH_SCOPE, but some things (types) */
/* bother to get pushed so fix them this way */
#define SCOPEadd_super(ths) ths->superscope = scope->this_;

#define ERROR(code)	ERRORreport(code, yylineno)

/* structured types for parse node decorations */

%}

%type <case_item>	case_action		case_otherwise

%type <entity_body>	entity_body

%type <expression>	aggregate_init_element	aggregate_initializer
			assignable
			attribute_decl
			by_expression
			constant
			expression
			function_call
			general_ref		group_ref
			identifier		initializer
			interval		literal
			local_initializer	precision_spec
			query_expression
			simple_expression
			unary_expression
			supertype_expression
			until_control		while_control

%type <iVal>	function_header		rule_header		procedure_header

%type <list>	action_body		actual_parameters	aggregate_init_body
		explicit_attr_list	case_action_list	case_block
		case_labels		where_clause_list	derive_decl
		explicit_attribute	expression_list
		formal_parameter	formal_parameter_list	formal_parameter_rep
		id_list			defined_type_list
		nested_id_list		/*repeat_control_list*/	statement_rep
		subtype_decl
		where_rule		where_rule_OPT
		supertype_expression_list
		labelled_attrib_list_list
		labelled_attrib_list
		inverse_attr_list
		inverse_clause
		attribute_decl_list
		derived_attribute_rep
		unique_clause
		rule_formal_parameter_list
		qualified_attr_list	/* remove labelled_attrib_list if this works */

%type <op_code>		rel_op

%type <type_flags>	optional_or_unique	optional_fixed	optional	var	unique

%type <qualified_attr>	qualified_attr

%type <qualifier>	qualifier

%type <statement>	alias_statement
			assignment_statement	case_statement
			compound_statement
			escape_statement	if_statement
			proc_call_statement	repeat_statement
			return_statement	skip_statement
			statement

%type <subsuper_decl>	subsuper_decl

%type <subtypes>	supertype_decl
			supertype_factor

%type <symbol>		function_id	procedure_id

%type <type>		attribute_type	defined_type
			parameter_type	generic_type

%type <typebody>	basic_type
			select_type		aggregate_type
			aggregation_type	array_type
			bag_type		conformant_aggregation
			list_type
			set_type

%type <type_either>	set_or_bag_of_entity	type

%type <upper_lower>	cardinality_op	index_spec		limit_spec

%type <variable>	inverse_attr	derived_attribute
			rule_formal_parameter

%type <where>		where_clause

%left	TOK_EQUAL		TOK_GREATER_EQUAL	TOK_GREATER_THAN
	TOK_IN			TOK_INST_EQUAL		TOK_INST_NOT_EQUAL
	TOK_LESS_EQUAL		TOK_LESS_THAN		TOK_LIKE
	TOK_NOT_EQUAL

%left	TOK_MINUS		TOK_PLUS
	TOK_OR			TOK_XOR

%left	TOK_DIV			TOK_MOD			TOK_REAL_DIV
	TOK_TIMES
	TOK_AND			TOK_ANDOR
	TOK_CONCAT_OP	/*direction doesn't really matter, just priority */

%right	TOK_EXP

%left	TOK_NOT


%nonassoc TOK_DOT TOK_BACKSLASH TOK_LEFT_BRACKET

%token	TOK_ABSTRACT
	TOK_AGGREGATE		TOK_ALIAS
	TOK_ALL_IN		TOK_ARRAY
	TOK_AS			TOK_ASSIGNMENT
	TOK_BACKSLASH
	TOK_BAG			TOK_BEGIN		TOK_BINARY
	TOK_BOOLEAN
	TOK_BY			TOK_CASE		TOK_COLON
	TOK_COMMA		TOK_CONSTANT		TOK_CONTEXT
	TOK_E			TOK_DERIVE
	TOK_DOT			TOK_ELSE		TOK_END
	TOK_END_ALIAS
	TOK_END_CASE		TOK_END_CONSTANT	TOK_END_CONTEXT
	TOK_END_ENTITY		TOK_END_FUNCTION
	TOK_END_IF		TOK_END_LOCAL		TOK_END_MODEL
	TOK_END_PROCEDURE	TOK_END_REPEAT		TOK_END_RULE
	TOK_END_SCHEMA		TOK_END_TYPE		TOK_ENTITY
	TOK_ENUMERATION		TOK_ESCAPE
	TOK_FIXED		TOK_FOR			TOK_FROM
	TOK_FUNCTION		TOK_GENERIC		TOK_IF
	TOK_INCLUDE
	TOK_INTEGER
	TOK_INVERSE
	TOK_LEFT_BRACKET	TOK_LEFT_CURL
	TOK_LEFT_PAREN		TOK_LIST		TOK_LOCAL
	TOK_LOGICAL		TOK_MODEL
	TOK_NUMBER		TOK_OF			TOK_ONEOF
	TOK_OPTIONAL		TOK_OTHERWISE		TOK_PI
	TOK_PROCEDURE		TOK_QUERY
	TOK_QUESTION_MARK	TOK_REAL		TOK_REFERENCE
	TOK_REPEAT		TOK_RETURN
	TOK_RIGHT_BRACKET	TOK_RIGHT_CURL		TOK_RIGHT_PAREN
	TOK_RULE		TOK_SCHEMA
	TOK_SELECT		TOK_SET
	TOK_SKIP		TOK_STRING		TOK_SUBTYPE
	TOK_SUCH_THAT		TOK_SUPERTYPE		TOK_THEN
	TOK_TO			TOK_TYPE		TOK_UNIQUE
	TOK_UNTIL		TOK_USE
	TOK_VAR			TOK_WHERE
	TOK_WHILE

%token <string>		TOK_STRING_LITERAL	TOK_STRING_LITERAL_ENCODED
%token <symbol>		TOK_BUILTIN_FUNCTION	TOK_BUILTIN_PROCEDURE
			TOK_IDENTIFIER		TOK_SELF
%token <iVal>		TOK_INTEGER_LITERAL
%token <rVal>		TOK_REAL_LITERAL
%token <logical>	TOK_LOGICAL_LITERAL
%token <binary>		TOK_BINARY_LITERAL

%token <string>		TOK_SEMICOLON

%start express_file

%union {
    /* simple (single-valued) node types */

    Binary		binary;
    Case_Item		case_item;
    Expression		expression;
    Integer		iVal;
    Linked_List		list;
    Logical		logical;
    Op_Code		op_code;
    Qualified_Attr *	qualified_attr;
    Real		rVal;
    Statement		statement;
    Symbol *		symbol;
    char *		string;
    Type		type;
    TypeBody		typebody;
    Variable		variable;
    Where		where;

    struct type_either {
	Type	 type;
	TypeBody body;
    } type_either;	/* either one of these can be returned */

    struct {
	unsigned optional:1;
	unsigned unique:1;
	unsigned fixed:1;
	unsigned var:1;		/* when formal is "VAR" */
    } type_flags;

    struct {
	Linked_List	attributes;
	Linked_List	unique;
	Linked_List	where;
    } entity_body;

    struct {
	Expression	subtypes;
	Linked_List	supertypes;
	Boolean		abstract;
    } subsuper_decl;

    struct {
	Expression	subtypes;
	Boolean		abstract;
    } subtypes;

    struct {
	Expression	lower_limit;
	Expression	upper_limit;
    } upper_lower;

    struct {
	Expression	expr;	/* overall expression for qualifier */
	Expression	first;	/* first [syntactic] qualifier (if more */
				/* than one is contained in this */
				/* [semantic] qualifier - used for */
				/* group_ref + attr_ref - see rule for */
				/* qualifier) */
    } qualifier;
}

%%

action_body		: action_body_item_rep statement_rep
			  { $$ = $2; }
			;

/* this should be rewritten to force order, see comment on next production */
action_body_item	: declaration
			| constant_decl
			| local_decl
/*			| error */
			;

/* this corresponds to 'algorithm_head' in N14-ese
   but it should be rewritten to force
	declarations	followed by
	constants	followed by
	local_decls
*/
action_body_item_rep	: /* NULL item */
			| action_body_item action_body_item_rep
			;

/* NOTE: can actually go to NULL, but it's a semantic problem */
/* to disambiguate this from a simple identifier, so let a call */
/* with no parameters be parsed as an identifier for now. */

/* another problem is that x() is always an entity.  func/proc calls */
/* with no args are called as "x".  By the time resolution occurs */
/* and we find out whether or not x is an entity or func/proc, we will */
/* no longer have the information about whether there were parens! */
/* EXPRESS is simply wrong to handle these two cases differently. */

actual_parameters	: TOK_LEFT_PAREN expression_list TOK_RIGHT_PAREN
			  { $$ = $2; }
			| TOK_LEFT_PAREN TOK_RIGHT_PAREN
			  { $$ = 0; /*LISTcreate();*/
				/* NULL would do, except that we'll end */
				/* up stuff the call name in as the 1st */
				/* arg, so we might as well create the */
				/* list now */
			    }
			;

/* I give up - why does 2nd parm of AGGR_LIT have to be non-null? */
aggregate_initializer	: TOK_LEFT_BRACKET TOK_RIGHT_BRACKET
			  {  $$ = EXPcreate(Type_Aggregate);
			     $$->u.list = LISTcreate();}
			| TOK_LEFT_BRACKET aggregate_init_body TOK_RIGHT_BRACKET
			  {  $$ = EXPcreate(Type_Aggregate);
			     $$->u.list = $2;}
			;

aggregate_init_element	: expression
			  { $$ = $1; }
/*
			| expression TOK_COLON expression
			  { $$ = 
			  { $$ = EXPRESSION_NULL;
			    ERRORreport_with_line(ERROR_unimplemented_feature,
						  yylineno,
						  "`:' repetition in aggregate initializer"); }
*/
			;

aggregate_init_body	: aggregate_init_element
			  { $$ = LISTcreate();
			    LISTadd($$, (Generic)$1); }
			| aggregate_init_element TOK_COLON expression
			  { $$ = LISTcreate();
			    LISTadd($$, (Generic)$1);
			    LISTadd($$, (Generic)$3);
			    $1->type->u.type->body->flags.repeat = 1; }
			| aggregate_init_body TOK_COMMA aggregate_init_element
			  { $$ = $1;
			    LISTadd_last($$, (Generic)$3); }
			| aggregate_init_body TOK_COMMA aggregate_init_element TOK_COLON expression
			  { $$ = $1;
			    LISTadd_last($$, (Generic)$3);
			    LISTadd_last($$, (Generic)$5);
			    $3->type->u.type->body->flags.repeat = 1;}
			;

aggregate_type		: TOK_AGGREGATE TOK_OF parameter_type
			  { $$ = TYPEBODYcreate(aggregate_);
			    $$->base = $3;
			    if (tag_count < 0) {
				Symbol sym;
				sym.line = yylineno;
				sym.filename = current_filename;
				ERRORreport_with_symbol(ERROR_unlabelled_param_type, &sym, CURRENT_SCOPE_NAME);
			    }
			  }
			| TOK_AGGREGATE TOK_COLON TOK_IDENTIFIER TOK_OF
			  parameter_type
			  { Type t = TYPEcreate_user_defined_tag($5,CURRENT_SCOPE,$3);
			    if (t) {
				SCOPEadd_super(t);
				$$ = TYPEBODYcreate(aggregate_);
				$$->tag = t;
				$$->base = $5;
			    }
			  }
			;

aggregation_type	: array_type
			  { $$ = $1; }
			| bag_type
			  { $$ = $1; }
			| list_type
			  { $$ = $1; }
			| set_type
			  { $$ = $1; }
			;

alias_statement		: TOK_ALIAS TOK_IDENTIFIER TOK_FOR general_ref semicolon
			  { struct Scope_ *s = SCOPEcreate_tiny(OBJ_ALIAS);
			/* scope doesn't really have/need a name */
			/* I suppose that naming it by the alias is fine */
				/*SUPPRESS 622*/
			    PUSH_SCOPE(s,(Symbol *)0,OBJ_ALIAS);}
				statement_rep
			  TOK_END_ALIAS semicolon
			  { Expression e = EXPcreate_from_symbol(Type_Attribute,$2);
			    Variable v = VARcreate(e,Type_Unknown);
			    v->initializer = $4;
			    DICTdefine(CURRENT_SCOPE->symbol_table,$2->name,
				      (Generic)v,$2,OBJ_VARIABLE);
			    $$ = ALIAScreate(CURRENT_SCOPE,v,$7);
			    POP_SCOPE(); }
			;

array_type		: TOK_ARRAY index_spec TOK_OF optional_or_unique
			  attribute_type
			  { $$ = TYPEBODYcreate(array_);
			    $$->flags.optional = $4.optional;
			    $$->flags.unique = $4.unique;
			    $$->upper = $2.upper_limit;
			    $$->lower = $2.lower_limit;
			    $$->base = $5;}
			;

/* this is anything that can be assigned to */
assignable		: assignable qualifier
			  { $2.first->e.op1 = $1;
			    $$ = $2.expr;}
			| identifier
			  { $$ = $1; }
			;

assignment_statement	: assignable TOK_ASSIGNMENT expression semicolon
			  { $$ = ASSIGNcreate($1,$3);}
			;

attribute_type		: aggregation_type
			  { $$ = TYPEcreate_from_body_anonymously($1);
			    SCOPEadd_super($$);}
			| basic_type
			  { $$ = TYPEcreate_from_body_anonymously($1);
			    SCOPEadd_super($$);}
			| defined_type
			  { $$ = $1; }
			;

explicit_attr_list	: /* NULL body */
			  { $$ = LISTcreate();}
			| explicit_attr_list explicit_attribute
			  { $$ = $1;
			    LISTadd_last($$, (Generic)$2); }
			;

bag_type		: TOK_BAG limit_spec TOK_OF attribute_type
			  { $$ = TYPEBODYcreate(bag_);
			    $$->base = $4;
			    $$->upper = $2.upper_limit;
			    $$->lower = $2.lower_limit;}
			| TOK_BAG TOK_OF attribute_type
			  { $$ = TYPEBODYcreate(bag_);
			    $$->base = $3; }
			;

basic_type		: TOK_BOOLEAN
			  { $$ = TYPEBODYcreate(boolean_);}
			| TOK_INTEGER precision_spec
			  { $$ = TYPEBODYcreate(integer_);
			    $$->precision = $2;}
			| TOK_REAL precision_spec
			  { $$ = TYPEBODYcreate(real_);
			    $$->precision = $2;}
			| TOK_NUMBER
			  { $$ = TYPEBODYcreate(number_);}
			| TOK_LOGICAL
			  { $$ = TYPEBODYcreate(logical_);}
			| TOK_BINARY precision_spec optional_fixed
			  { $$ = TYPEBODYcreate(binary_);
			    $$->precision = $2;
			    $$->flags.fixed = $3.fixed;}
			| TOK_STRING precision_spec optional_fixed
			  { $$ = TYPEBODYcreate(string_);
			    $$->precision = $2;
			    $$->flags.fixed = $3.fixed;}
			;

block_list		: /*NULL*/
			| block_list block_member
			;

block_member		: declaration
			| include_directive
			| rule_decl
			;

by_expression		: /* NULL by_expression */
			  { $$ = LITERAL_ONE;}
			| TOK_BY expression
			  { $$ = $2; }
			;

cardinality_op		: TOK_LEFT_CURL expression TOK_COLON expression TOK_RIGHT_CURL
			  { $$.lower_limit = $2;
			    $$.upper_limit = $4; }
			;

case_action		: case_labels TOK_COLON statement
			  { $$ = CASE_ITcreate($1, $3);
			    SYMBOLset($$);}
			;

case_action_list	: /* no case_actions */
			  { $$ = LISTcreate();}
			| case_action_list case_action
			  { yyerrok;
			    $$ = $1;
			    LISTadd_last($$, (Generic)$2); }
/*			| error
			  { ERROR(ERROR_empty_case);
			    $$ = LISTcreate(); }*/
			;

case_block		: case_action_list case_otherwise
			  { $$ = $1;
			    if ($2) LISTadd_last($$, (Generic)$2); }
			;

case_labels		: expression
			  { $$ = LISTcreate();
			    LISTadd_last($$, (Generic)$1); }
			| case_labels TOK_COMMA expression
			  { yyerrok;
			    $$ = $1;
			    LISTadd_last($$, (Generic)$3); }
/*			| case_labels error
			  { ERROR(ERROR_comma_expected);
			    $$ = $1; }
			| case_labels error
			  { ERROR(ERROR_comma_expected); }
			  expression
			  { yyerrok;
			    $$ = $1; }
			| case_labels TOK_COMMA error
			  { ERROR(ERROR_case_labels);
			    $$ = $1; }*/
			;

case_otherwise		: /* no otherwise clause */
			  { $$ = (Case_Item)0; }
			| TOK_OTHERWISE TOK_COLON statement
			  { $$ = CASE_ITcreate(LIST_NULL, $3);
			    SYMBOLset($$);}
			;

case_statement		: TOK_CASE expression TOK_OF case_block TOK_END_CASE
			  semicolon
			  { $$ = CASEcreate($2, $4);}
			;

compound_statement	: TOK_BEGIN statement_rep TOK_END semicolon
			  { $$ = COMP_STMTcreate($2);}
			;

constant		: TOK_PI
			  { $$ = LITERAL_PI; }
			| TOK_E
			  { $$ = LITERAL_E; }
			;

			/* package this up as an attribute */
constant_body		: identifier TOK_COLON attribute_type TOK_ASSIGNMENT
				expression semicolon
			  { Variable v;
			    $1->type = $3;
			    v = VARcreate($1,$3);
			    v->initializer = $5;
			    v->flags.constant = 1;
			    DICTdefine(CURRENT_SCOPE->symbol_table,
				$1->symbol.name,
				(Generic)v,&$1->symbol,OBJ_VARIABLE);
			}
			;

constant_body_list	: /* NULL */
			| constant_body constant_body_list
			;

constant_decl		:
			  TOK_CONSTANT constant_body_list TOK_END_CONSTANT
			  	semicolon
			;

declaration		: entity_decl
			| function_decl
			| procedure_decl
			| type_decl
			;

derive_decl		: /* NULL body */
			  { $$ = LISTcreate(); }
			| TOK_DERIVE derived_attribute_rep
			  { $$ = $2; }
			;

derived_attribute	: attribute_decl TOK_COLON attribute_type initializer semicolon
			  { $$ = VARcreate($1,$3);
			    $$->initializer = $4;
			    $$->flags.attribute = True;
			  }
			;

derived_attribute_rep	: derived_attribute
			  { $$ = LISTcreate();
			    LISTadd_last($$, (Generic)$1); }
			| derived_attribute_rep derived_attribute
			  { $$ = $1;
			    LISTadd_last($$, (Generic)$2); }
/*			| error
			  { ERROR(ERROR_missing_derive);
			    $$ = LISTcreate(); }
			| derived_attribute_rep error
			  { ERROR(ERROR_derived_attribute);
			    $$ = $1; }*/
			;

entity_body		: explicit_attr_list derive_decl inverse_clause
			  unique_clause where_rule_OPT
			  { $$.attributes = $1;
			    /* this is flattened out in entity_decl - DEL */
			    LISTadd_last($$.attributes, (Generic)$2);
			    if ($3 != LIST_NULL) {
				    LISTadd_last($$.attributes, (Generic)$3);
			    }
			    $$.unique = $4;
			    $$.where = $5; }
			;

entity_decl		: entity_header subsuper_decl semicolon
			  entity_body TOK_END_ENTITY semicolon
			  { CURRENT_SCOPE->u.entity->subtype_expression = $2.subtypes;
			    CURRENT_SCOPE->u.entity->supertype_symbols = $2.supertypes;
			    LISTdo ($4.attributes, l, Linked_List)
				LISTdo (l, a, Variable)
				    ENTITYadd_attribute(CURRENT_SCOPE, a);
				LISTod;
			    LISTod;
			    CURRENT_SCOPE->u.entity->abstract = $2.abstract;
			    CURRENT_SCOPE->u.entity->unique = $4.unique;
			    CURRENT_SCOPE->where = $4.where;
			    POP_SCOPE();}
/*			| entity_header error TOK_END_ENTITY semicolon
			  { POP_SCOPE();}*/
			;


entity_header		: TOK_ENTITY TOK_IDENTIFIER
			  { Entity e = ENTITYcreate($2);
			    if (print_objects_while_running & OBJ_ENTITY_BITS){
				fprintf(stdout,"parse: %s (entity)\n",$2->name);
			    }
			    PUSH_SCOPE(e,$2,OBJ_ENTITY);}
			;

enumeration_type	: TOK_ENUMERATION TOK_OF nested_id_list
			  { int value = 0; Expression x; Symbol *tmp;
			    TypeBody tb;
			    tb = TYPEBODYcreate(enumeration_);
			    CURRENT_SCOPE->u.type->head = 0;
			    CURRENT_SCOPE->u.type->body = tb;
			    tb->list = $3;
			    if (!CURRENT_SCOPE->symbol_table) {
				CURRENT_SCOPE->symbol_table = DICTcreate(25);
			    }
			    LISTdo_links($3, id)
				tmp = (Symbol *)id->data;
				id->data = (Generic)(x = EXPcreate(CURRENT_SCOPE));
				x->symbol = *(tmp);
				x->u.integer = ++value;
				/* define both in enum scope and scope of */
				/* 1st visibility */
				DICT_define(CURRENT_SCOPE->symbol_table,
					x->symbol.name,
					(Generic)x,&x->symbol,OBJ_EXPRESSION);
				DICTdefine(PREVIOUS_SCOPE->symbol_table,x->symbol.name,
					(Generic)x,&x->symbol,OBJ_EXPRESSION);
			        SYMBOL_destroy(tmp);
			    LISTod;
		          }
			;

escape_statement	: TOK_ESCAPE semicolon
			  { $$ = STATEMENT_ESCAPE; }
			;

attribute_decl		: TOK_IDENTIFIER
			  { $$ = EXPcreate(Type_Attribute);
			    $$->symbol = *$1;SYMBOL_destroy($1);
			  }
			| TOK_SELF TOK_BACKSLASH TOK_IDENTIFIER TOK_DOT TOK_IDENTIFIER
			  { $$ = EXPcreate(Type_Expression);
			    $$->e.op1 = EXPcreate(Type_Expression);
			    $$->e.op1->e.op_code = OP_GROUP;
			    $$->e.op1->e.op1 = EXPcreate(Type_Self);
			    $$->e.op1->e.op2 = EXPcreate_from_symbol(Type_Entity,$3);
			    SYMBOL_destroy($3);
			    $$->e.op_code = OP_DOT;
			    $$->e.op2 = EXPcreate_from_symbol(Type_Attribute,$5);
			    SYMBOL_destroy($5);
			  }
			;

attribute_decl_list	: attribute_decl
			  { $$ = LISTcreate();
			    LISTadd_last($$, (Generic)$1); }
			| attribute_decl_list TOK_COMMA attribute_decl
			  { $$ = $1;
			    LISTadd_last($$, (Generic)$3); }
			;

optional		: /*NULL*/
			  { $$.optional = 0;}
			| TOK_OPTIONAL
			  { $$.optional = 1;}
			;

explicit_attribute	: attribute_decl_list TOK_COLON optional
			  attribute_type semicolon
			  { Variable v;
			    LISTdo_links ($1, attr)
				v = VARcreate((Expression)attr->data,$4);
				v->flags.optional = $3.optional;
			        v->flags.attribute = True;
				attr->data = (Generic)v;
			    LISTod;
			  }
			;
/*
				if (OBJis_kind_of(attr,Class_Binary_Expression)) {
					x = BIN_EXPget_second_operand(attr);
				} else  x = attr;
				if (!OBJis_kind_of(x,Class_Identifier)) {
					yyerror("illegal attribute name");
				}
				v = EXPcreate(...);
				v->symbol = SYMBOLget_name(
					IDENTget_identifier(x)),$4,&experrc);
				VARput_optional(v, true);
				VARput_reference(v,attr);
				LISTadd_last(vlist,(Generic)v);
*/


express_file		: { scope = scopes;
			    /* no need to define scope->this */
			    scope->this_ = yyexpresult;
			    scope->pscope = scope;
			    scope->type = OBJ_EXPRESS;
			    yyexpresult->symbol.name = yyexpresult->u.express->filename;
			    yyexpresult->symbol.filename = yyexpresult->u.express->filename;
			    yyexpresult->symbol.line = 1;
			  }
			  schema_decl_list
			;

schema_decl_list	: schema_decl
			| schema_decl_list schema_decl
			;

expression		: simple_expression
			  { $$ = $1; }
			| expression TOK_AND expression
			  { yyerrok;
			    $$ = BIN_EXPcreate(OP_AND, $1, $3);}
			| expression TOK_OR expression
			  { yyerrok;
			    $$ = BIN_EXPcreate(OP_OR, $1, $3);}
			| expression TOK_XOR expression
			  { yyerrok;
			    $$ = BIN_EXPcreate(OP_XOR, $1, $3);}
			| expression TOK_LESS_THAN expression
			  { yyerrok;
			    $$ = BIN_EXPcreate(OP_LESS_THAN, $1, $3);}
			| expression TOK_GREATER_THAN expression
			  { yyerrok;
			    $$ = BIN_EXPcreate(OP_GREATER_THAN, $1, $3);}
			| expression TOK_EQUAL expression
			  { yyerrok;
			    $$ = BIN_EXPcreate(OP_EQUAL, $1, $3);}
			| expression TOK_LESS_EQUAL expression
			  { yyerrok;
			    $$ = BIN_EXPcreate(OP_LESS_EQUAL, $1, $3);}
			| expression TOK_GREATER_EQUAL expression
			  { yyerrok;
			    $$ = BIN_EXPcreate(OP_GREATER_EQUAL, $1, $3);}
			| expression TOK_NOT_EQUAL expression
			  { yyerrok;
			    $$ = BIN_EXPcreate(OP_NOT_EQUAL, $1, $3);}
			| expression TOK_INST_EQUAL expression
			  { yyerrok;
			    $$ = BIN_EXPcreate(OP_INST_EQUAL, $1, $3);}
			| expression TOK_INST_NOT_EQUAL expression
			  { yyerrok;
			    $$ = BIN_EXPcreate(OP_INST_NOT_EQUAL, $1, $3);}
			| expression TOK_IN expression
			  { yyerrok;
			    $$ = BIN_EXPcreate(OP_IN, $1, $3);}
			| expression TOK_LIKE expression
			  { yyerrok;
			    $$ = BIN_EXPcreate(OP_LIKE, $1, $3);}
			| simple_expression cardinality_op simple_expression
			  { yyerrok; }
/*			| expression error 
			  { ERROR;
			    $$ = EXPRESSION_NULL; }*/
			;

simple_expression	: unary_expression
			  { $$ = $1; }
/*			| unary_expression group_ref
			  { $2->e.op1 = $1;
			    $$ = $2; }*/
			| simple_expression TOK_CONCAT_OP simple_expression
			  { yyerrok;
			    $$ = BIN_EXPcreate(OP_CONCAT, $1, $3);}
			| simple_expression TOK_EXP simple_expression
			  { yyerrok;
			    $$ = BIN_EXPcreate(OP_EXP, $1, $3);}
			| simple_expression TOK_TIMES simple_expression
			  { yyerrok;
			    $$ = BIN_EXPcreate(OP_TIMES, $1, $3);}
			| simple_expression TOK_DIV simple_expression
			  { yyerrok;
			    $$ = BIN_EXPcreate(OP_DIV, $1, $3);}
			| simple_expression TOK_REAL_DIV simple_expression
			  { yyerrok;
			    $$ = BIN_EXPcreate(OP_REAL_DIV, $1, $3);}
			| simple_expression TOK_MOD simple_expression
			  { yyerrok;
			    $$ = BIN_EXPcreate(OP_MOD, $1, $3);}
			| simple_expression TOK_PLUS simple_expression
			  { yyerrok;
			    $$ = BIN_EXPcreate(OP_PLUS, $1, $3);}
			| simple_expression TOK_MINUS simple_expression
			  { yyerrok;
			    $$ = BIN_EXPcreate(OP_MINUS, $1, $3);}
/* this would be for string concatenation, but the parser can't tell whether
something is a string or not, so let it be tagged as just arithmetic plus.
			| simple_expression TOK_PLUS simple_expression
			  { yyerrok;
			    $$ = BIN_EXPcreate(OP_CONCAT, $1, $3);}
*/
/*			| simple_expression error
			  { ERROR;
			    $$ = EXPRESSION_NULL; }*/
			;

expression_list		: expression
			  { $$ = LISTcreate();
			    LISTadd_last($$, (Generic)$1); }
			| expression_list TOK_COMMA expression
			  { $$ = $1;
			    LISTadd_last($$, (Generic)$3); }
			;

var			: /*NULL*/
			  { $$.var = 1;}
			| TOK_VAR
			  { $$.var = 0;}
			;

formal_parameter	: var id_list TOK_COLON parameter_type
			  { Symbol *tmp; Expression e; Variable v;
			    $$ = $2;
			    LISTdo_links($$, param)
				tmp = (Symbol *)param->data;
/*				e = EXPcreate_from_symbol($4,tmp);*/
				e = EXPcreate_from_symbol(Type_Attribute,tmp);
			        v = VARcreate(e,$4);
			        v->flags.optional = $1.var;
				v->flags.parameter = True;
				param->data = (Generic)v;

				/* link it in to the current scope's dict */
				DICTdefine(CURRENT_SCOPE->symbol_table,
					tmp->name,(Generic)v,
					tmp,OBJ_VARIABLE);

				/* see explanation of this in TYPEresolve func */
				/* $4->refcount++; */
			    LISTod;
			  }
			;

formal_parameter_list	: /* no parameters */
			  { $$ = LIST_NULL; }
			| TOK_LEFT_PAREN formal_parameter_rep TOK_RIGHT_PAREN
			  { $$ = $2; }
			;

formal_parameter_rep	: formal_parameter
			  { $$ = $1; }
			| formal_parameter_rep semicolon formal_parameter
			  { $$ = $1;
			    LISTadd_all($$, $3); }
			;

parameter_type		: basic_type
			  { $$ = TYPEcreate_from_body_anonymously($1);
			    SCOPEadd_super($$);}
			| conformant_aggregation
			  { $$ = TYPEcreate_from_body_anonymously($1);
			    SCOPEadd_super($$);}
			| defined_type
			  { $$ = $1; }
			| generic_type
			  { $$ = $1; }
			;

function_call		: function_id actual_parameters
			  { $$ = EXPcreate(Type_Funcall);
			    $$->symbol = *$1; SYMBOL_destroy($1);
			    $$->u.funcall.list = $2;}
			;

function_decl		: function_header action_body TOK_END_FUNCTION
			  semicolon
			  { FUNCput_body(CURRENT_SCOPE, $2);
			    ALGput_full_text(CURRENT_SCOPE, $1, SCANtell());
			    POP_SCOPE();}
/*			| function_header error TOK_END_FUNCTION semicolon
			  { POP_SCOPE();}*/
			;

function_header		: TOK_FUNCTION
			  { $<iVal>$ = SCANtell(); }
			  TOK_IDENTIFIER
			  { Function f = ALGcreate(OBJ_FUNCTION);
			    tag_count = 0;
			    if (print_objects_while_running
				& OBJ_FUNCTION_BITS)
				fprintf(stdout, "parse: %s (function)\n",
					$3->name);
			    PUSH_SCOPE(f, $3, OBJ_FUNCTION);}
			  formal_parameter_list
			  { Function f = CURRENT_SCOPE;
			    f->u.func->parameters = $5;
			    f->u.func->pcount = LISTget_length($5);
			    f->u.func->tag_count = tag_count;
			    tag_count = -1;	/* done with parameters, no
						   new tags can be defined */
			  }
			  TOK_COLON parameter_type semicolon
			  { Function f = CURRENT_SCOPE;
			    f->u.func->return_type = $8;
			    $$ = $<iVal>2;
			  }
			;

function_id		: TOK_IDENTIFIER
			  { $$ = $1; }
			| TOK_BUILTIN_FUNCTION
			  { $$ = $1; }
			;

conformant_aggregation	: aggregate_type
			  { $$ = $1; }
			| TOK_ARRAY TOK_OF optional_or_unique parameter_type
			  { $$ = TYPEBODYcreate(array_);
			    $$->flags.optional = $3.optional;
			    $$->flags.unique = $3.unique;
			    $$->base = $4;}
			| TOK_ARRAY index_spec TOK_OF optional_or_unique parameter_type
			  { $$ = TYPEBODYcreate(array_);
			    $$->flags.optional = $4.optional;
			    $$->flags.unique = $4.unique;
			    $$->base = $5;
			    $$->upper = $2.upper_limit;
			    $$->lower = $2.lower_limit;}
			| TOK_BAG TOK_OF parameter_type
			  { $$ = TYPEBODYcreate(bag_);
			    $$->base = $3; }
			| TOK_BAG index_spec TOK_OF parameter_type
			  { $$ = TYPEBODYcreate(bag_);
			    $$->base = $4;
			    $$->upper = $2.upper_limit;
			    $$->lower = $2.lower_limit;}
			| TOK_LIST TOK_OF unique parameter_type
			  { $$ = TYPEBODYcreate(list_);
			    $$->flags.unique = $3.unique;
			    $$->base = $4; }
			| TOK_LIST index_spec TOK_OF unique parameter_type
			  { $$ = TYPEBODYcreate(list_);
			    $$->base = $5;
			    $$->flags.unique = $4.unique;
			    $$->upper = $2.upper_limit;
			    $$->lower = $2.lower_limit;}
			| TOK_SET TOK_OF parameter_type
			  { $$ = TYPEBODYcreate(set_);
			    $$->base = $3; }
			| TOK_SET index_spec TOK_OF parameter_type
			  { $$ = TYPEBODYcreate(set_);
			    $$->base = $4;
			    $$->upper = $2.upper_limit;
			    $$->lower = $2.lower_limit;}
			;

generic_type		: TOK_GENERIC
			  { $$ = Type_Generic;
			    if (tag_count < 0) {
				Symbol sym;
				sym.line = yylineno;
				sym.filename = current_filename;
				ERRORreport_with_symbol(ERROR_unlabelled_param_type, &sym, CURRENT_SCOPE_NAME);
			    }
			  }
			| TOK_GENERIC TOK_COLON TOK_IDENTIFIER
			  { 
			    TypeBody g = TYPEBODYcreate(generic_);
			    $$ = TYPEcreate_from_body_anonymously(g);
			    SCOPEadd_super($$);
			    if (g->tag = TYPEcreate_user_defined_tag($$,CURRENT_SCOPE,$3))
				SCOPEadd_super(g->tag);
		    	  }
			;

id_list			: TOK_IDENTIFIER
			  { $$ = LISTcreate();
			    LISTadd_last($$, (Generic)$1); }
			| id_list TOK_COMMA TOK_IDENTIFIER
			  { yyerrok;
			    $$ = $1;
			    LISTadd_last($$, (Generic)$3); }
/*			| error
			  { $$ = LISTcreate(); }*/
/*			| id_list error
			  { $$ = $1; }*/
/*			| id_list error TOK_IDENTIFIER
			  { yyerrok;
			    $$ = $1; }*/
/*			| id_list TOK_COMMA error
			  { $$ = $1; }*/
			;

identifier		: TOK_SELF
			  {$$ = EXPcreate(Type_Self);}
			| TOK_QUESTION_MARK
			  {$$ = LITERAL_INFINITY; }
			| TOK_IDENTIFIER
			  {$$ = EXPcreate(Type_Identifier);
			    $$->symbol = *($1); SYMBOL_destroy($1);}
			;

if_statement		: TOK_IF expression TOK_THEN statement_rep
			  TOK_END_IF semicolon
			  { $$ = CONDcreate($2,$4,STATEMENT_LIST_NULL);}
			| TOK_IF expression TOK_THEN statement_rep
			  TOK_ELSE statement_rep TOK_END_IF semicolon
			  { $$ = CONDcreate($2,$4,$6);}
/*			| TOK_IF expression TOK_THEN statement_rep
			  TOK_ELSE error TOK_END_IF semicolon
			  { $$ = CONDcreate($2,$4,STATEMENT_LIST_NULL);}*/
/*			| TOK_IF expression TOK_THEN error TOK_END_IF semicolon
			  { $$ = CONDcreate($2, STATEMENT_LIST_NULL, STATEMENT_LIST_NULL);*/
			;

include_directive	: TOK_INCLUDE TOK_STRING_LITERAL semicolon
			{ SCANinclude_file($2); }
			;

increment_control	: TOK_IDENTIFIER TOK_ASSIGNMENT expression TOK_TO
			  expression by_expression
			  { Increment i = INCR_CTLcreate($1, $3, $5, $6);
			/* scope doesn't really have/need a name, I suppose */
			/* naming it by the iterator variable is fine */
			PUSH_SCOPE(i,(Symbol *)0,OBJ_INCREMENT);}
			;

index_spec		: TOK_LEFT_BRACKET expression TOK_COLON expression
			  TOK_RIGHT_BRACKET
			  { $$.lower_limit = $2;
			    $$.upper_limit = $4; }
/*			| TOK_LEFT_BRACKET expression error TOK_RIGHT_BRACKET
			  { $$.lower_limit = $2;
			    $$.upper_limit = LITERAL_INFINITY; }*/
/*			| TOK_LEFT_BRACKET error TOK_RIGHT_BRACKET
			  { $$.lower_limit = LITERAL_INFINITY;
			    $$.upper_limit = LITERAL_INFINITY; }*/
			;

initializer		: TOK_ASSIGNMENT expression
			  { $$ = $2; }
			;

rename			: TOK_IDENTIFIER
			  { (*interface_func)(CURRENT_SCOPE,interface_schema,$1,$1);}
			| TOK_IDENTIFIER TOK_AS TOK_IDENTIFIER
			  { (*interface_func)(CURRENT_SCOPE,interface_schema,$1,$3);}
			;

rename_list		: rename
			| rename_list TOK_COMMA rename
			;

parened_rename_list	: TOK_LEFT_PAREN rename_list TOK_RIGHT_PAREN
			;

reference_clause	: TOK_REFERENCE TOK_FROM TOK_IDENTIFIER semicolon
			  { if (!CURRENT_SCHEMA->ref_schemas)
			      CURRENT_SCHEMA->ref_schemas = LISTcreate();
			    LISTadd(CURRENT_SCHEMA->ref_schemas,
				    (Generic)$3); }
			| TOK_REFERENCE TOK_FROM TOK_IDENTIFIER 
			  { interface_schema = $3;
			    interface_func = SCHEMAadd_reference; }
			  parened_rename_list semicolon
			;

use_clause		: TOK_USE TOK_FROM TOK_IDENTIFIER semicolon
			  { if (!CURRENT_SCHEMA->use_schemas)
			      CURRENT_SCHEMA->use_schemas = LISTcreate();
			    LISTadd(CURRENT_SCHEMA->use_schemas,
				    (Generic)$3); }
			| TOK_USE TOK_FROM TOK_IDENTIFIER
			  { interface_schema = $3;
			    interface_func = SCHEMAadd_use; }
			  parened_rename_list semicolon
			;

interface_specification	: use_clause
			| reference_clause
			;

interface_specification_list	: /*NULL*/
			| interface_specification_list interface_specification
			;

interval		: TOK_LEFT_CURL simple_expression rel_op
			  simple_expression rel_op simple_expression
			  right_curl
			  { Expression	tmp1, tmp2;

			    $$ = (Expression )0;
			    tmp1 = BIN_EXPcreate($3, $2, $4);
			    tmp2 = BIN_EXPcreate($5, $4, $6);
			    $$ = BIN_EXPcreate(OP_AND, tmp1, tmp2);
			  }
			;

/* defined_type might have to be something else since it's really an */
/* entity_ref */
set_or_bag_of_entity	: defined_type
			  { $$.type = $1;
			    $$.body = 0;}
			| TOK_SET TOK_OF defined_type
			  { $$.type = 0;
			    $$.body = TYPEBODYcreate(set_);
			    $$.body->base = $3; }
			| TOK_SET limit_spec TOK_OF defined_type
			  { $$.type = 0;
			    $$.body = TYPEBODYcreate(set_);
			    $$.body->base = $4;
			    $$.body->upper = $2.upper_limit;
			    $$.body->lower = $2.lower_limit;}
			| TOK_BAG limit_spec TOK_OF defined_type
			  { $$.type = 0;
			    $$.body = TYPEBODYcreate(bag_);
			    $$.body->base = $4;
			    $$.body->upper = $2.upper_limit;
			    $$.body->lower = $2.lower_limit;}
			| TOK_BAG TOK_OF defined_type
			  { $$.type = 0;
			    $$.body = TYPEBODYcreate(bag_);
			    $$.body->base = $3; }
			;

inverse_attr_list	: inverse_attr
			{ $$ = LISTcreate();
			  LISTadd_last($$,(Generic)$1);}
			| inverse_attr_list inverse_attr
			  { $$ = $1;
			    LISTadd_last($$, (Generic)$2); }
			;

inverse_attr		: TOK_IDENTIFIER TOK_COLON set_or_bag_of_entity
				TOK_FOR TOK_IDENTIFIER semicolon
			{ Expression e = EXPcreate(Type_Attribute);
			  e->symbol = *$1;SYMBOL_destroy($1);
			  if ($3.type) $$ = VARcreate(e,$3.type);
			  else {
				  Type t = TYPEcreate_from_body_anonymously($3.body);
				  SCOPEadd_super(t);
				  $$ = VARcreate(e,t);
			  }
			  $$->flags.attribute = True;
			  $$->inverse_symbol = $5;}
			;

inverse_clause		: /*NULL*/
			  { $$ = LIST_NULL; }
			| TOK_INVERSE inverse_attr_list
			  { $$ = $2; }
			;

limit_spec		: TOK_LEFT_BRACKET expression TOK_COLON
			  expression TOK_RIGHT_BRACKET
			  { $$.lower_limit = $2;
			    $$.upper_limit = $4; }
/*			| TOK_LEFT_BRACKET expression error TOK_RIGHT_BRACKET
			  { $$.lower_limit = $2;
			    $$.upper_limit = LITERAL_INFINITY; }*/
/*			| TOK_LEFT_BRACKET error TOK_RIGHT_BRACKET
			  { $$.lower_limit = LITERAL_INFINITY; 
			    $$.upper_limit = LITERAL_INFINITY; }*/
/*			| error
			  { $$.lower_limit = LITERAL_INFINITY;
			    $$.upper_limit = LITERAL_INFINITY; }*/
			;

list_type		: TOK_LIST limit_spec TOK_OF unique attribute_type
			  { $$ = TYPEBODYcreate(list_);
			    $$->base = $5;
			    $$->flags.unique = $4.unique;
			    $$->lower = $2.lower_limit;
			    $$->upper = $2.upper_limit;}
			| TOK_LIST TOK_OF unique attribute_type
			  { $$ = TYPEBODYcreate(list_);
			    $$->base = $4;
			    $$->flags.unique = $3.unique;}
			;

literal			: TOK_INTEGER_LITERAL
			  { if ($1 == 0)
				$$ = LITERAL_ZERO;
			    else if ($1 == 1)
				$$ = LITERAL_ONE;
			    else {
				$$ = EXPcreate_simple(Type_Integer);
				/*SUPPRESS 112*/
				$$->u.integer = (int)$1;
				resolved_all($$);
			    } }
			| TOK_REAL_LITERAL
			  { if ($1 == 0.0)
				$$ = LITERAL_ZERO;
			    else {
				$$ = EXPcreate_simple(Type_Real);
				$$->u.real = $1;
				resolved_all($$);
			    } }
			| TOK_STRING_LITERAL
			  { $$ = EXPcreate_simple(Type_String);
			    $$->symbol.name = $1;
				resolved_all($$);}
			| TOK_STRING_LITERAL_ENCODED
			  { $$ = EXPcreate_simple(Type_String_Encoded);
			    $$->symbol.name = $1;
				resolved_all($$);}
			| TOK_LOGICAL_LITERAL
			  { $$ = EXPcreate_simple(Type_Logical);
			    $$->u.logical = $1;
				resolved_all($$);}
			| TOK_BINARY_LITERAL
			  { $$ = EXPcreate_simple(Type_Binary);
			    $$->symbol.name = $1;
				resolved_all($$);}
			| constant
			  { $$ = $1; }
			;

local_initializer	: TOK_ASSIGNMENT expression
			  { $$ = $2; }
			;

local_variable		: id_list TOK_COLON parameter_type semicolon
			  { Expression e; Variable v;
			    LISTdo($1, sym, Symbol *)
				/* convert symbol to name-expression */
				e = EXPcreate(Type_Attribute);
				e->symbol = *sym;SYMBOL_destroy(sym);
				v = VARcreate(e,$3);
				DICTdefine(CURRENT_SCOPE->symbol_table,
					e->symbol.name,(Generic)v,&e->symbol,OBJ_VARIABLE);
			    LISTod; LISTfree($1); }
			| id_list TOK_COLON parameter_type local_initializer semicolon
			  { Expression e; Variable v;
			    LISTdo($1, sym, Symbol *)
				e = EXPcreate(Type_Attribute);
				e->symbol = *sym;SYMBOL_destroy(sym);
				v = VARcreate(e,$3);
				v->initializer = $4;
				DICTdefine(CURRENT_SCOPE->symbol_table,
					e->symbol.name,(Generic)v,&e->symbol,OBJ_VARIABLE);
			    LISTod; LISTfree($1);}
			;

local_body		: /* no local_variables */
			| local_variable local_body
			;

local_decl		: TOK_LOCAL local_body TOK_END_LOCAL semicolon
			  { }
/*			| TOK_LOCAL error TOK_END_LOCAL semicolon
			  { }*/
			;

defined_type		: TOK_IDENTIFIER
			  { $$ = TYPEcreate_name($1);
				SCOPEadd_super($$);
			    SYMBOL_destroy($1);}
			;

defined_type_list		: defined_type
			  { $$ = LISTcreate();
			    LISTadd($$, (Generic)$1); }
			| defined_type_list TOK_COMMA defined_type
			  { $$ = $1;
			    LISTadd_last($$, (Generic)$3); }
			;

nested_id_list		: TOK_LEFT_PAREN id_list TOK_RIGHT_PAREN
			  { $$ = $2; }
			;

oneof_op		: TOK_ONEOF
			;

optional_or_unique	: /* NULL body */
			  { $$.unique = 0; $$.optional = 0;}
			| TOK_OPTIONAL
			  { $$.unique = 0; $$.optional = 1;}
			| TOK_UNIQUE
			  { $$.unique = 1; $$.optional = 0;}
			| TOK_OPTIONAL TOK_UNIQUE
			  { $$.unique = 1; $$.optional = 1;}
			| TOK_UNIQUE TOK_OPTIONAL
			  { $$.unique = 1; $$.optional = 1;}
			;

optional_fixed		: /* nuthin' */
			  { $$.fixed = 0; }
			| TOK_FIXED
			  { $$.fixed = 1; }
			;

precision_spec		: /* no precision specified */
			  { $$ = (Expression )0; }
			| TOK_LEFT_PAREN expression TOK_RIGHT_PAREN
			  { $$ = $2; }
			;

/* NOTE: actual parameters cannot go to NULL, since this causes */
/* a syntactic ambiguity (see note at actual_parameters).  hence */
/* the need for the second branch of this rule. */

proc_call_statement	: procedure_id actual_parameters semicolon
			  { $$ = PCALLcreate($2);
			    $$->symbol = *($1);}
			| procedure_id semicolon
			  { $$ = PCALLcreate((Linked_List)0);
			    $$->symbol = *($1);}
			;

procedure_decl		: procedure_header action_body TOK_END_PROCEDURE
			  semicolon
			  { PROCput_body(CURRENT_SCOPE, $2);
			    ALGput_full_text(CURRENT_SCOPE, $1, SCANtell());
			    POP_SCOPE();}

/*			| procedure_header error TOK_END_PROCEDURE semicolon
			  { POP_SCOPE();}*/
			;

procedure_header	: TOK_PROCEDURE
			  { $<iVal>$ = SCANtell(); }
			  TOK_IDENTIFIER
			  { Procedure p = ALGcreate(OBJ_PROCEDURE);
			    tag_count = 0;
			    if (print_objects_while_running
				& OBJ_PROCEDURE_BITS) {
				fprintf(stdout, "parse: %s (procedure)\n",
					$3->name);
			    }
			    PUSH_SCOPE(p, $3, OBJ_PROCEDURE); }
			  formal_parameter_list semicolon
			  { Procedure p = CURRENT_SCOPE;
			    p->u.proc->parameters = $5;
			    p->u.proc->pcount = LISTget_length($5);
			    p->u.proc->tag_count = tag_count;
			    tag_count = -1;	/* done with parameters, no
						   new tags can be defined */
			    $$ = $<iVal>2;
			  }
			;

procedure_id		: TOK_IDENTIFIER
			  { $$ = $1; }
			| TOK_BUILTIN_PROCEDURE
			  { $$ = $1; }
			;

group_ref		: TOK_BACKSLASH TOK_IDENTIFIER
			  { $$ = BIN_EXPcreate(OP_GROUP,(Expression )0,(Expression )0);
			    $$->e.op2 = EXPcreate(Type_Identifier);
			    $$->e.op2->symbol = *$2; SYMBOL_destroy($2); }
			;

qualifier		: TOK_DOT TOK_IDENTIFIER
			  { $$.expr = $$.first =
				BIN_EXPcreate(OP_DOT,(Expression )0,(Expression )0);
			    $$.expr->e.op2 = EXPcreate(Type_Identifier);
			    $$.expr->e.op2->symbol = *$2;
			    SYMBOL_destroy($2); }
			| TOK_BACKSLASH TOK_IDENTIFIER
			  { $$.expr = $$.first =
				BIN_EXPcreate(OP_GROUP,(Expression )0,(Expression )0);
			    $$.expr->e.op2 = EXPcreate(Type_Identifier);
			    $$.expr->e.op2->symbol = *$2;
			    SYMBOL_destroy($2); }
/*			| group_ref TOK_DOT TOK_IDENTIFIER
			  { */
			    /* we want op1 of the group_ref to be the */
			    /* preceding identifier, so put it in $$.first. */
			    /* the overall expression for this qualifier, */
			    /* however, is the one we create here, so it */
			    /* goes in $$.expr. */
/*			    $$.expr = BIN_EXPcreate(OP_DOT,(Expression )0,(Expression )0);
			    $$.expr->e.op1 = $$.first = $1;
			    $$.expr->e.op2 = EXPcreate(Type_Identifier);
			    $$.expr->e.op2->symbol = *$3;
			    SYMBOL_destroy($3); }
*/
			| TOK_LEFT_BRACKET simple_expression TOK_RIGHT_BRACKET
			  { $$.expr = $$.first =
				BIN_EXPcreate(OP_ARRAY_ELEMENT,(Expression )0,(Expression )0);
			    $$.expr->e.op2 = $2; }
			| TOK_LEFT_BRACKET simple_expression TOK_COLON
					   simple_expression TOK_RIGHT_BRACKET
			  { $$.expr = $$.first =
				TERN_EXPcreate(OP_SUBCOMPONENT,(Expression )0,(Expression )0,(Expression )0);
			    $$.expr->e.op2 = $2;
			    $$.expr->e.op3 = $4; }
			;

query_expression	: TOK_QUERY TOK_LEFT_PAREN
			  TOK_IDENTIFIER TOK_ALL_IN expression
			  TOK_SUCH_THAT
			  {
			    $<expression>$ = QUERYcreate($3,$5);
			    SYMBOL_destroy($3);
			    PUSH_SCOPE($<expression>$->u.query->scope,(Symbol *)0,OBJ_QUERY);
			  }
			  expression TOK_RIGHT_PAREN
			  {
			    $$ = $<expression>7;	/* nice syntax, eh? */
			    $$->u.query->expression = $8;
			    POP_SCOPE();
			  }
			;

rel_op			: TOK_LESS_THAN
			  { $$ = OP_LESS_THAN; }
			| TOK_GREATER_THAN
			  { $$ = OP_GREATER_THAN; }
			| TOK_EQUAL
			  { $$ = OP_EQUAL; }
			| TOK_LESS_EQUAL
			  { $$ = OP_LESS_EQUAL; }
			| TOK_GREATER_EQUAL
			  { $$ = OP_GREATER_EQUAL; }
			| TOK_NOT_EQUAL
			  { $$ = OP_NOT_EQUAL; }
			| TOK_INST_EQUAL
			  { $$ = OP_INST_EQUAL; }
			| TOK_INST_NOT_EQUAL
			  { $$ = OP_INST_NOT_EQUAL; }
			;

/* repeat_statement causes a scope creation if an increment_control exists */
repeat_statement	: TOK_REPEAT increment_control
				while_control until_control semicolon
			  statement_rep TOK_END_REPEAT semicolon
			  { $$ = LOOPcreate(CURRENT_SCOPE,$3,$4,$6);
			    /* matching PUSH_SCOPE is in increment_control */
			    POP_SCOPE(); }
			| TOK_REPEAT while_control until_control semicolon
			  statement_rep TOK_END_REPEAT semicolon
			  { $$ = LOOPcreate((struct Scope_ *)0,$2,$3,$5);}
/*			| TOK_REPEAT error TOK_END_REPEAT semicolon
			  { $$ = STATEMENT_NULL; }*/
			;

return_statement	: TOK_RETURN semicolon
			  { $$ = RETcreate((Expression )0);}
			| TOK_RETURN TOK_LEFT_PAREN expression TOK_RIGHT_PAREN
			  semicolon
			  { $$ = RETcreate($3);}
			;

right_curl		: TOK_RIGHT_CURL
			  { yyerrok; }
			;

rule_decl		: rule_header action_body where_rule TOK_END_RULE semicolon
			  { RULEput_body(CURRENT_SCOPE,$2);
			    RULEput_where(CURRENT_SCOPE,$3);
			    ALGput_full_text(CURRENT_SCOPE, $1, SCANtell());
			    POP_SCOPE();}
/*			| rule_header error TOK_END_RULE semicolon
			  { POP_SCOPE();}*/
			;

rule_formal_parameter	: TOK_IDENTIFIER
			  { Expression e; Type t;
				/* it's true that we know it will be an */
				/* entity_ type later */
			    TypeBody tb = TYPEBODYcreate(set_);
			    tb->base = TYPEcreate_name($1);
			    SCOPEadd_super(tb->base);
			    t = TYPEcreate_from_body_anonymously(tb);
			    SCOPEadd_super(t);
			    e = EXPcreate_from_symbol(t,$1);
			    $$ = VARcreate(e,t);
			    $$->flags.attribute = True;
			    $$->flags.parameter = True;
			    /* link it in to the current scope's dict */
			    DICTdefine(CURRENT_SCOPE->symbol_table,
				$1->name,(Generic)$$,$1,OBJ_VARIABLE);
			}
			;

rule_formal_parameter_list	: rule_formal_parameter
			  { $$ = LISTcreate();
			    LISTadd($$, (Generic)$1); }
			| rule_formal_parameter_list TOK_COMMA rule_formal_parameter
			  { $$ = $1;
			    LISTadd_last($$, (Generic)$3); }
			;

rule_header		: TOK_RULE
			  { $<iVal>$ = SCANtell(); }
			  TOK_IDENTIFIER TOK_FOR TOK_LEFT_PAREN
			  { Rule r = ALGcreate(OBJ_RULE);
			    if (print_objects_while_running & OBJ_RULE_BITS){
				fprintf(stdout,"parse: %s (rule)\n",$3->name);
			    }
			    PUSH_SCOPE(r,$3,OBJ_RULE);
		          }
			  rule_formal_parameter_list TOK_RIGHT_PAREN semicolon
			  { CURRENT_SCOPE->u.rule->parameters = $7;
			  $$ = $<iVal>2; }
			;

schema_body		: interface_specification_list
						block_list
			| interface_specification_list constant_decl
						block_list
			;

schema_decl		: schema_header schema_body TOK_END_SCHEMA
			  semicolon
			  { POP_SCOPE();}
			| include_directive
/*			| schema_header error TOK_END_SCHEMA semicolon
			  { POP_SCOPE();}*/
			;

schema_header		: TOK_SCHEMA TOK_IDENTIFIER semicolon
			  { Schema schema = DICTlookup(CURRENT_SCOPE->symbol_table,$2->name);

			    if (print_objects_while_running & OBJ_SCHEMA_BITS){
				fprintf(stdout,"parse: %s (schema)\n",$2->name);
			    }
			    if (EXPRESSignore_duplicate_schemas && schema) {
				SCANskip_to_end_schema();
				PUSH_SCOPE_DUMMY();
			    } else {
				schema = SCHEMAcreate();
				LISTadd_last(PARSEnew_schemas,(Generic)schema);
				PUSH_SCOPE(schema,$2,OBJ_SCHEMA);
			    }
		          }
			;

select_type		: TOK_SELECT TOK_LEFT_PAREN defined_type_list
			  TOK_RIGHT_PAREN
			  { $$ = TYPEBODYcreate(select_);
			    $$->list = $3; }
			;

semicolon		: TOK_SEMICOLON
			  { yyerrok; }
			;

set_type		: TOK_SET limit_spec TOK_OF attribute_type
			  { $$ = TYPEBODYcreate(set_);
			    $$->base = $4;
			    $$->lower = $2.lower_limit;
			    $$->upper = $2.upper_limit;}
			| TOK_SET TOK_OF attribute_type
			  { $$ = TYPEBODYcreate(set_);
			    $$->base = $3;}
			;

skip_statement		: TOK_SKIP semicolon
			  { $$ = STATEMENT_SKIP; }
			;

statement		: alias_statement
			  { $$ = $1; }
			| assignment_statement
			  { $$ = $1; }
			| case_statement
			  { $$ = $1; }
			| compound_statement
			  { $$ = $1; }
			| escape_statement
			  { $$ = $1; }
			| if_statement
			  { $$ = $1; }
			| proc_call_statement
			  { $$ = $1; }
			| repeat_statement
			  { $$ = $1; }
			| return_statement
			  { $$ = $1; }
			| skip_statement
			  { $$ = $1; }
/*			| error semicolon
			  { $$ = STATEMENT_NULL; }*/
			;

statement_rep		: /* no statements */
			  { $$ = LISTcreate(); }
			| /* ignore null statement */
			  semicolon statement_rep
			  { $$ = $2; }
			| statement statement_rep
			  { $$ = $2;
			    LISTadd_first($$, (Generic)$1); }
			;

/* if the actions look backwards, remember the declaration syntax:  */
/* <entity> SUPERTYPE OF <subtype1> ... SUBTYPE OF <supertype1> ... */

subsuper_decl		: /* NULL body */
			  { $$.subtypes = EXPRESSION_NULL;
			    $$.abstract = False;
			    $$.supertypes = LIST_NULL; }
			| supertype_decl
			  { $$.subtypes = $1.subtypes;
			    $$.abstract = $1.abstract;
			    $$.supertypes = LIST_NULL; }
			| subtype_decl
			  { $$.supertypes = $1;
			    $$.abstract = False;
			    $$.subtypes = EXPRESSION_NULL; }
/*			| subtype_decl supertype_decl
			  { $$.supertypes = $1;
			    $$.abstract = $2.abstract;
			    $$.subtypes = $2.subtypes; }*/
			| supertype_decl subtype_decl
			  { $$.subtypes = $1.subtypes;
			    $$.abstract = $1.abstract;
			    $$.supertypes = $2; }
			;

subtype_decl		: TOK_SUBTYPE TOK_OF TOK_LEFT_PAREN defined_type_list
			  TOK_RIGHT_PAREN
			  { $$ = $4; }
			;

supertype_decl		: TOK_ABSTRACT TOK_SUPERTYPE
			  { $$.subtypes = (Expression)0;
			    $$.abstract = True; }
			| TOK_SUPERTYPE TOK_OF TOK_LEFT_PAREN
			  supertype_expression TOK_RIGHT_PAREN
			  { $$.subtypes = $4;
			    $$.abstract = False; }
			| TOK_ABSTRACT TOK_SUPERTYPE TOK_OF TOK_LEFT_PAREN
			  supertype_expression TOK_RIGHT_PAREN
			  { $$.subtypes = $5;
			    $$.abstract = True; }
			;

supertype_expression	: supertype_factor
			  { $$ = $1.subtypes; }
			| supertype_expression TOK_AND supertype_factor
			  { $$ = BIN_EXPcreate(OP_AND, $1, $3.subtypes);}
			| supertype_expression TOK_ANDOR supertype_factor
			  { $$ = BIN_EXPcreate(OP_ANDOR, $1, $3.subtypes);}
			;

supertype_expression_list	: supertype_expression
			{ $$ = LISTcreate();
			    LISTadd_last($$,(Generic)$1); }
			| supertype_expression_list TOK_COMMA supertype_expression
			    { LISTadd_last($1,(Generic)$3);
			      $$ = $1;}
			;

supertype_factor	: identifier
			  { $$.subtypes = $1; }
			| oneof_op TOK_LEFT_PAREN supertype_expression_list
				TOK_RIGHT_PAREN
			  { $$.subtypes = EXPcreate(Type_Oneof);
			    $$.subtypes->u.list = $3;}
			| TOK_LEFT_PAREN supertype_expression TOK_RIGHT_PAREN
			  { $$.subtypes = $2; }
			;

type			: aggregation_type
			  { $$.type = 0;
			    $$.body = $1; }
			| basic_type
			  { $$.type = 0;
			    $$.body = $1; }
			| defined_type
			  { $$.type = $1;
			    $$.body = 0; }
			| select_type
			  { $$.type = 0;
			    $$.body = $1; }
			;

type_item_body		: enumeration_type
			| type {
				CURRENT_SCOPE->u.type->head = $1.type;
				CURRENT_SCOPE->u.type->body = $1.body;
			}
			;

type_item		: TOK_IDENTIFIER TOK_EQUAL {
				Type t = TYPEcreate_name($1);
				PUSH_SCOPE(t,$1,OBJ_TYPE);
			} type_item_body {
/*				SYMBOL_destroy($1);*/
		        } semicolon
			;

type_decl		: TOK_TYPE type_item where_rule_OPT {
				CURRENT_SCOPE->where = $3;
				POP_SCOPE();
			  } TOK_END_TYPE semicolon
/*			| TOK_TYPE error TOK_END_TYPE semicolon*/
			;


general_ref		: assignable group_ref
			  { $2->e.op1 = $1;
			    $$ = $2;}
			| assignable
			  { $$ = $1; }
			;

unary_expression	: aggregate_initializer
			  { $$ = $1; }
			| unary_expression qualifier
			  { $2.first->e.op1 = $1;
			    $$ = $2.expr;}
			| literal
			  { $$ = $1; }
			| function_call
			  { $$ = $1; }
			| identifier
			  { $$ = $1; }
			| TOK_LEFT_PAREN expression TOK_RIGHT_PAREN
			  { $$ = $2; }
/*			| TOK_LEFT_PAREN expression error
			  { $$ = $2; }*/
			| interval
			  { $$ = $1; }
/*subsuper_init looks just like any expression */
/*			| subsuper_init
			  { $$ = $1; }*/
			| query_expression
			  { $$ = $1; }
			| TOK_NOT unary_expression
			  { $$ = UN_EXPcreate(OP_NOT, $2);}
			| TOK_PLUS unary_expression		%prec TOK_NOT
			  { $$ = $2; }
			| TOK_MINUS unary_expression		%prec TOK_NOT
			  { $$ = UN_EXPcreate(OP_NEGATE, $2);}
/*			| error
			  { /*ERROR(ERROR_expression);*/ 
/*			    $$ = EXPRESSION_NULL; }*/
			;

unique			: /* look for optional UNIQUE */
			  { $$.unique = 0;}
			| TOK_UNIQUE
			  { $$.unique = 1;}
			;

qualified_attr		: TOK_IDENTIFIER
			  { $$ = QUAL_ATTR_new();
			    $$->attribute = $1;
			  }
			| TOK_SELF TOK_BACKSLASH TOK_IDENTIFIER TOK_DOT TOK_IDENTIFIER
			  { $$ = QUAL_ATTR_new();
			    $$->entity = $3;
			    $$->attribute = $5;
			  }
			;

qualified_attr_list	: qualified_attr
			  { $$ = LISTcreate();
			    LISTadd_last($$, (Generic)$1); }
			| qualified_attr_list TOK_COMMA qualified_attr
			  { $$ = $1;
			    LISTadd_last($$, (Generic)$3); }
			;

labelled_attrib_list	: qualified_attr_list semicolon
			  { LISTadd_first($1,(Generic)EXPRESSION_NULL);
			    $$ = $1;}
			| TOK_IDENTIFIER TOK_COLON qualified_attr_list semicolon
			  { LISTadd_first($3,(Generic)$1);
			    $$ = $3;}
			;

/*
labelled_attrib_list	: /* these could be unary_expression_lists instead */
			  /* of expression_lists if there is a parse prob */
/*			  expression_list semicolon
			  { LISTadd_first($1,(Generic)EXPRESSION_NULL);
			    $$ = $1;}
			| TOK_IDENTIFIER TOK_COLON expression_list semicolon
			  { LISTadd_first($3,(Generic)$1);
			    $$ = $3;}
			;
*/

/* returns a list */
labelled_attrib_list_list	:
			  labelled_attrib_list
			  { $$ = LISTcreate();
			    LISTadd_last($$,(Generic)$1); }
			| labelled_attrib_list_list labelled_attrib_list
			    { LISTadd_last($1,(Generic)$2);
			      $$ = $1;}
			;

unique_clause		: 
			  /* unique clause is always optional */
			  { $$ = 0; }
			| TOK_UNIQUE labelled_attrib_list_list
			  { $$ = $2; }
			;

until_control		: /* NULL */
			{ $$ = 0; }
			| TOK_UNTIL expression
			  { $$ = $2;}
			;

where_clause		: expression semicolon
			  { $$ = WHERE_new();
			    $$->label = SYMBOLcreate("<unnamed>", yylineno,
						     current_filename);
			    $$->expr = $1;
		          }
			| TOK_IDENTIFIER TOK_COLON expression semicolon
			  { $$ = WHERE_new();
			    $$->label = $1;
			    $$->expr = $3;
			    if (!CURRENT_SCOPE->symbol_table) {
				CURRENT_SCOPE->symbol_table = DICTcreate(25);
			    }
			    DICTdefine(CURRENT_SCOPE->symbol_table,$1->name,
				(Generic)$$,$1,OBJ_WHERE);
		          }
			;

where_clause_list	: where_clause
			  { $$ = LISTcreate();
			    LISTadd($$, (Generic)$1); }
			| where_clause_list where_clause
			  { $$ = $1;
			    LISTadd_last($$, (Generic)$2); }
/*			| where_clause_list error
			  { $$ = $1; }*/
			;

where_rule		: TOK_WHERE where_clause_list
			  { $$ = $2; }
			;

where_rule_OPT		: /* NULL body: no where rule */
			  { $$ = LIST_NULL; }
			| where_rule
			  { $$ = $1; }
			;

while_control		: /* NULL */
			  { $$ = 0; }
			| TOK_WHILE expression
			  { $$ = $2;}
			;

%%
static void
yyerror(string)
char* string;
{
	char buf[200];
	Symbol sym;

	strcpy (buf, string);

	if (yyeof) strcat(buf, " at end of input");
	else if (yytext[0] == 0) strcat(buf, " at null character");
	else if (yytext[0] < 040 || yytext[0] >= 0177)
		sprintf(buf + strlen(buf), " before character 0%o", yytext[0]);
	else	sprintf(buf + strlen(buf), " before `%s'", yytext);

	sym.line = yylineno;
	sym.filename = current_filename;
	ERRORreport_with_symbol(ERROR_syntax, &sym, buf, CURRENT_SCOPE_TYPE_PRINTABLE,CURRENT_SCOPE_NAME);
  }

static void
yyerror2(t)
char CONST * t;	/* token or 0 to indicate no more tokens */
{
	char buf[200];
	Symbol sym;
	static int first = 1;	/* true if first suggested replacement */
	static char tokens[4000] = "";/* error message, saying */
					/* "expecting <token types>" */

	if (t) {	/* subsequent token? */
		if (first) first = 0;
		else strcat (tokens," or ");
		strcat(tokens,t);
	} else {
	  strcpy(buf,"syntax error");
	  if (yyeof)
	    strcat(buf, " at end of input");
	  else if (yytext[0] == 0)
	    strcat(buf, " at null character");
	  else if (yytext[0] < 040 || yytext[0] >= 0177)
	    sprintf(buf + strlen(buf), " near character 0%o", yytext[0]);
	  else
	    sprintf(buf + strlen(buf), " near `%s'", yytext);

	  if (0 == strlen(tokens)) yyerror("syntax error");
	  sym.line = yylineno - (yytext[0] == '\n');
	  sym.filename = current_filename;
	  ERRORreport_with_symbol(ERROR_syntax_expecting,&sym,buf,tokens
			,CURRENT_SCOPE_TYPE_PRINTABLE,CURRENT_SCOPE_NAME);
	}
}
