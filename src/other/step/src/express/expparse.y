/** Lemon grammar for Express parser, based on SCL's expparse.y.  */
%include {
#include <assert.h>
#include "token_type.h"

int yyerrstatus = 0;
#define yyerrok (yyerrstatus = 0)

int yylval;

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

    static void	yyerror PROTO((char *));
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

void parserInitState()
{
    scope = scopes;
    /* no need to define scope->this */
    scope->this_ = yyexpresult;
    scope->pscope = scope;
    scope->type = OBJ_EXPRESS;
    yyexpresult->symbol.name = yyexpresult->u.express->filename;
    yyexpresult->symbol.filename = yyexpresult->u.express->filename;
    yyexpresult->symbol.line = 1;
}
} /* include */

%type case_action			{ Case_Item }
%type case_otherwise			{ Case_Item }
%type entity_body			{ struct entity_body }

%type aggregate_init_element		{ Expression }
%type aggregate_initializer		{ Expression }
%type assignable			{ Expression }
%type attribute_decl			{ Expression }
%type by_expression			{ Expression }
%type constant				{ Expression }
%type expression			{ Expression }
%type function_call			{ Expression }
%type general_ref			{ Expression }
%type group_ref				{ Expression }
%type identifier			{ Expression }
%type initializer			{ Expression }
%type interval				{ Expression }
%type literal				{ Expression }
%type local_initializer			{ Expression }
%type precision_spec			{ Expression }
%type query_expression			{ Expression }
%type query_start			{ Expression }
%type simple_expression			{ Expression }
%type unary_expression			{ Expression }
%type supertype_expression		{ Expression }
%type until_control			{ Expression }
%type while_control			{ Expression }

%type function_header			{ Integer }
%type fh_lineno				{ Integer }
%type rule_header			{ Integer }
%type rh_start				{ Integer }
%type rh_get_line			{ Integer }
%type procedure_header			{ Integer }
%type ph_get_line			{ Integer }

%type action_body			{ Linked_List }
%type actual_parameters			{ Linked_List }
%type aggregate_init_body		{ Linked_List }
%type explicit_attr_list		{ Linked_List }
%type case_action_list			{ Linked_List }
%type case_block			{ Linked_List }
%type case_labels			{ Linked_List }
%type where_clause_list			{ Linked_List }
%type derive_decl			{ Linked_List }
%type explicit_attribute		{ Linked_List }
%type expression_list			{ Linked_List }
%type formal_parameter			{ Linked_List }
%type formal_parameter_list		{ Linked_List }
%type formal_parameter_rep		{ Linked_List }
%type id_list				{ Linked_List }
%type defined_type_list			{ Linked_List }
%type nested_id_list			{ Linked_List }
/*repeat_control_list*/
%type statement_rep			{ Linked_List }
%type subtype_decl			{ Linked_List }
%type where_rule			{ Linked_List }
%type where_rule_OPT			{ Linked_List }
%type supertype_expression_list		{ Linked_List }
%type labelled_attrib_list_list		{ Linked_List }
%type labelled_attrib_list		{ Linked_List }
%type inverse_attr_list			{ Linked_List }

%type inverse_clause			{ Linked_List }
%type attribute_decl_list		{ Linked_List }
%type derived_attribute_rep		{ Linked_List }
%type unique_clause			{ Linked_List }
%type rule_formal_parameter_list	{ Linked_List }
%type qualified_attr_list		{ Linked_List }
/* remove labelled_attrib_list if this works */

%type rel_op			{ Op_Code }

%type optional_or_unique	{ struct type_flags }
%type optional_fixed		{ struct type_flags }
%type optional			{ struct type_flags }
%type var			{ struct type_flags }
%type unique			{ struct type_flags }

%type qualified_attr		{ Qualified_Attr* }

%type qualifier			{ struct qualifier }

%type alias_statement		{ Statement }
%type assignment_statement	{ Statement }
%type case_statement		{ Statement }
%type compound_statement	{ Statement }
%type escape_statement		{ Statement }
%type if_statement		{ Statement }
%type proc_call_statement	{ Statement }
%type repeat_statement		{ Statement }
%type return_statement		{ Statement }
%type skip_statement		{ Statement }
%type statement			{ Statement }

%type subsuper_decl		{ struct subsuper_decl }

%type supertype_decl		{ struct subtypes }
%type supertype_factor		{ struct subtypes }

%type function_id		{ Symbol* }
%type procedure_id		{ Symbol* }

%type attribute_type		{ Type }
%type defined_type		{ Type }
%type parameter_type		{ Type }
%type generic_type		{ Type }

%type basic_type		{ TypeBody }
%type select_type		{ TypeBody }
%type aggregate_type		{ TypeBody }
%type aggregation_type		{ TypeBody }
%type array_type		{ TypeBody }
%type bag_type			{ TypeBody }
%type conformant_aggregation	{ TypeBody }
%type list_type			{ TypeBody }
%type set_type			{ TypeBody }

%type set_or_bag_of_entity	{ struct type_either }
%type type			{ struct type_either }

%type cardinality_op		{ struct upper_lower }
%type index_spec		{ struct upper_lower }
%type limit_spec		{ struct upper_lower }

%type inverse_attr		{ Variable }
%type derived_attribute		{ Variable }
%type rule_formal_parameter	{ Variable }

%type where_clause		{ Where }


%left	TOK_EQUAL
	TOK_GREATER_EQUAL
	TOK_GREATER_THAN
	TOK_IN
	TOK_INST_EQUAL
	TOK_INST_NOT_EQUAL
	TOK_LESS_EQUAL
	TOK_LESS_THAN
	TOK_LIKE TOK_NOT_EQUAL.  

%left	TOK_MINUS
	TOK_PLUS
	TOK_OR
	TOK_XOR.

%left	TOK_DIV
	TOK_MOD
	TOK_REAL_DIV
	TOK_TIMES
	TOK_AND
	TOK_ANDOR
	TOK_CONCAT_OP.

%right TOK_EXP.

%left TOK_NOT.

%nonassoc	TOK_DOT
		TOK_BACKSLASH
		TOK_LEFT_BRACKET.

%start_symbol express_file

%token_type { YYSTYPE }

action_body(A) ::= action_body_item_rep statement_rep(B).
{
    A = B;
}

/* this should be rewritten to force order, see comment on next production */
action_body_item(A) ::= declaration(B).
{
    A = B;
}
action_body_item(A) ::= constant_decl(B).
{
    A = B;
}
action_body_item(A) ::= local_decl(B).
{
    A = B;
}

/* this corresponds to 'algorithm_head' in N14-ese but it should be rewritten
 * to force declarationsfollowed by constants followed by local_decls
 */
action_body_item_rep ::= /* NULL item */.
action_body_item_rep(A) ::= action_body_item(B) action_body_item_rep.
{
    A = B;
}

/* NOTE: can actually go to NULL, but it's a semantic problem */
/* to disambiguate this from a simple identifier, so let a call */
/* with no parameters be parsed as an identifier for now. */

/* another problem is that x() is always an entity. func/proc calls
 * with no args are called as "x".  By the time resolution occurs
 * and we find out whether or not x is an entity or func/proc, we will
 * no longer have the information about whether there were parens!
 */

/* EXPRESS is simply wrong to handle these two cases differently. */

actual_parameters(A) ::= TOK_LEFT_PAREN expression_list(B) TOK_RIGHT_PAREN.
{
    A = B;
}
actual_parameters(A) ::= TOK_LEFT_PAREN TOK_RIGHT_PAREN.
{
    A = 0;
}

/* I give up - why does 2nd parm of AGGR_LIT have to be non-null?  */
aggregate_initializer(A) ::= TOK_LEFT_BRACKET TOK_RIGHT_BRACKET.
{
    A = EXPcreate(Type_Aggregate);
    A->u.list = LISTcreate();
}
aggregate_initializer(A) ::= TOK_LEFT_BRACKET aggregate_init_body(B)
TOK_RIGHT_BRACKET.
{
    A = EXPcreate(Type_Aggregate);
    A->u.list = B;
}

aggregate_init_element(A) ::= expression(B).
{
    A = B;
}

aggregate_init_body(A) ::= aggregate_init_element(B).
{
    A = LISTcreate();
    LISTadd(A, (Generic)B);
}
aggregate_init_body(A) ::= aggregate_init_element(B) TOK_COLON expression(C).
{
    A = LISTcreate();
    LISTadd(A, (Generic)B);

    LISTadd(A, (Generic)C);

    B->type->u.type->body->flags.repeat = 1;
}
aggregate_init_body(A) ::= aggregate_init_body(B) TOK_COMMA
			    aggregate_init_element(C).
{ 
    A = B;

    LISTadd_last(A, (Generic)C);

}
aggregate_init_body(A) ::= aggregate_init_body(B) TOK_COMMA
			    aggregate_init_element(C) TOK_COLON expression(D).
{
    A = B;

    LISTadd_last(A, (Generic)C);
    LISTadd_last(A, (Generic)D);

    C->type->u.type->body->flags.repeat = 1;
}

aggregate_type(A) ::= TOK_AGGREGATE TOK_OF parameter_type(B).
{
    A = TYPEBODYcreate(aggregate_);
    A->base = B;

    if (tag_count < 0) {
        Symbol sym;
        sym.line = yylineno;
        sym.filename = current_filename;
        ERRORreport_with_symbol(ERROR_unlabelled_param_type, &sym,
	    CURRENT_SCOPE_NAME);
    }
}
aggregate_type(A) ::= TOK_AGGREGATE TOK_COLON TOK_IDENTIFIER(B) TOK_OF
		       parameter_type(C).
{
    Type t = TYPEcreate_user_defined_tag(C, CURRENT_SCOPE, B.symbol);

    if (t) {
        SCOPEadd_super(t);
        A = TYPEBODYcreate(aggregate_);
        A->tag = t;
        A->base = C;
    }
}

aggregation_type(A) ::= array_type(B).
{
    A = B;
}
aggregation_type(A) ::= bag_type(B).
{
    A = B;
}
aggregation_type(A) ::= list_type(B).
{
    A = B;
}
aggregation_type(A) ::= set_type(B).
{
    A = B;
}

alias_statement(A) ::= TOK_ALIAS TOK_IDENTIFIER(B) TOK_FOR general_ref(C)
			semicolon alias_push_scope statement_rep(D)
			TOK_END_ALIAS semicolon.
{
    Expression e = EXPcreate_from_symbol(Type_Attribute, B.symbol);
    Variable v = VARcreate(e, Type_Unknown);

    v->initializer = C; 

    DICTdefine(CURRENT_SCOPE->symbol_table, B.symbol->name, (Generic)v,
	    B.symbol, OBJ_VARIABLE);
    A = ALIAScreate(CURRENT_SCOPE, v, D);

    POP_SCOPE();
}

alias_push_scope ::= /* subroutine */.
{
    struct Scope_ *s = SCOPEcreate_tiny(OBJ_ALIAS);
    PUSH_SCOPE(s, (Symbol *)0, OBJ_ALIAS);
}

array_type(A) ::= TOK_ARRAY index_spec(B) TOK_OF optional_or_unique(C)
		   attribute_type(D).
{
    A = TYPEBODYcreate(array_);

    A->flags.optional = C.optional;
    A->flags.unique = C.unique;
    A->upper = B.upper_limit;
    A->lower = B.lower_limit;
    A->base = D;
}

/* this is anything that can be assigned to */
assignable(A) ::= assignable(B) qualifier(C).
{
    C.first->e.op1 = B;
    A = C.expr;
}
assignable(A) ::= identifier(B).
{
    A = B;
}

assignment_statement(A) ::= assignable(B) TOK_ASSIGNMENT expression(C)
			    semicolon.
{ 
    A = ASSIGNcreate(B, C);
}

attribute_type(A) ::= aggregation_type(B).
{
    A = TYPEcreate_from_body_anonymously(B);
    SCOPEadd_super(A);
}
attribute_type(A) ::= basic_type(B).
{
    A = TYPEcreate_from_body_anonymously(B);
    SCOPEadd_super(A);
}
attribute_type(A) ::= defined_type(B).
{
    A = B;
}

explicit_attr_list(A) ::= /* NULL body */.
{
    A = LISTcreate();
}
explicit_attr_list(A) ::= explicit_attr_list(B) explicit_attribute(C).
{
    A = B;
    LISTadd_last(A, (Generic)C); 
}

bag_type(A) ::= TOK_BAG limit_spec(B) TOK_OF attribute_type(C).
{
    A = TYPEBODYcreate(bag_);
    A->base = C;
    A->upper = B.upper_limit;
    A->lower = B.lower_limit;
}
bag_type(A) ::= TOK_BAG TOK_OF attribute_type(B).
{
    A = TYPEBODYcreate(bag_);
    A->base = B;
}

basic_type(A) ::= TOK_BOOLEAN.
{
    A = TYPEBODYcreate(boolean_);
}
basic_type(A) ::= TOK_INTEGER precision_spec(B).
{
    A = TYPEBODYcreate(integer_);
    A->precision = B;
}
basic_type(A) ::= TOK_REAL precision_spec(B).
{
    A = TYPEBODYcreate(real_);
    A->precision = B;
}
basic_type(A) ::= TOK_NUMBER.
{
    A = TYPEBODYcreate(number_);
}
basic_type(A) ::= TOK_LOGICAL.
{
    A = TYPEBODYcreate(logical_);
}
basic_type(A) ::= TOK_BINARY precision_spec(B) optional_fixed(C).
{
    A = TYPEBODYcreate(binary_);
    A->precision = B;
    A->flags.fixed = C.fixed;
}
basic_type(A) ::= TOK_STRING precision_spec(B) optional_fixed(C).
{
    A = TYPEBODYcreate(string_);
    A->precision = B;
    A->flags.fixed = C.fixed;
}

block_list ::= /* NULL */.
block_list(A) ::= block_list(B) block_member.
{
    A = B;
}

block_member(A) ::= declaration(B).
{
    A = B;
}
block_member(A) ::= include_directive(B).
{
    A = B;
}
block_member(A) ::= rule_decl(B).
{
    A = B;
}

by_expression(A) ::= /* NULL by_expression */.
{
    A = LITERAL_ONE;
}
by_expression(A) ::= TOK_BY expression(B).
{
    A = B;
}

cardinality_op(A) ::= TOK_LEFT_CURL expression(B) TOK_COLON expression(C)
		       TOK_RIGHT_CURL.
{
    A.lower_limit = B;
    A.upper_limit = C;
}

case_action(A) ::= case_labels(B) TOK_COLON statement(C).
{
    A = CASE_ITcreate(B, C);
    SYMBOLset(A);
}

case_action_list(A) ::= /* no case_actions */.
{
    A = LISTcreate();
}
case_action_list(A) ::= case_action_list(B) case_action(C).
{
    yyerrok;

    A = B;

    LISTadd_last(A, (Generic)C);
}

case_block(A) ::= case_action_list(B) case_otherwise(C).
{
    A = B;

    if (C) {
        LISTadd_last(A,
        (Generic)C);
    }
}

case_labels(A) ::= expression(B).
{
    A = LISTcreate();

    LISTadd_last(A, (Generic)B);
}
case_labels(A) ::= case_labels(B) TOK_COMMA expression(C).
{
    yyerrok;

    A = B;
    LISTadd_last(A, (Generic)C);
}

case_otherwise(A) ::= /* no otherwise clause */.
{
    A = (Case_Item)0;
}
case_otherwise(A) ::= TOK_OTHERWISE TOK_COLON statement(B).
{
    A = CASE_ITcreate(LIST_NULL, B);
    SYMBOLset(A);
}

case_statement(A) ::= TOK_CASE expression(B) TOK_OF case_block(C) TOK_END_CASE
		      semicolon.
{
    A = CASEcreate(B, C);
}

compound_statement(A) ::= TOK_BEGIN statement_rep(B) TOK_END semicolon.
{
    A = COMP_STMTcreate(B);
}

constant(A) ::= TOK_PI.
{ 
    A = LITERAL_PI;
}

constant(A) ::= TOK_E.
{ 
    A = LITERAL_E;
}

/* package this up as an attribute */
constant_body ::= identifier(A) TOK_COLON attribute_type(B) TOK_ASSIGNMENT
    expression(C) semicolon.
{
    Variable v;

    A->type = B;
    v = VARcreate(A, B);
    v->initializer = C;
    v->flags.constant = 1;
    DICTdefine(CURRENT_SCOPE->symbol_table, A->symbol.name, (Generic)v,
	&A->symbol, OBJ_VARIABLE);
}

constant_body_list ::= /* NULL */.
constant_body_list(A) ::= constant_body(B) constant_body_list.
{
    A = B;
}

constant_decl(A) ::= TOK_CONSTANT(B) constant_body_list TOK_END_CONSTANT
		     semicolon.
{
    A = B;
}

declaration(A) ::= entity_decl(B).
{
    A = B;
}
declaration(A) ::= function_decl(B).
{
    A = B;
}
declaration(A) ::= procedure_decl(B).
{
    A = B;
}
declaration(A) ::= type_decl(B).
{
    A = B;
}

derive_decl(A) ::= /* NULL body */.
{
    A = LISTcreate();
}
derive_decl(A) ::= TOK_DERIVE derived_attribute_rep(B).
{
    A = B;
}

derived_attribute(A) ::= attribute_decl(B) TOK_COLON attribute_type(C)
			 initializer(D) semicolon.
{
    A = VARcreate(B, C);
    A->initializer = D;
    A->flags.attribute = True;
}

derived_attribute_rep(A) ::= derived_attribute(B).
{
    A = LISTcreate();
    LISTadd_last(A, (Generic)B);
}
derived_attribute_rep(A) ::= derived_attribute_rep(B) derived_attribute(C).
{
    A = B;
    LISTadd_last(A, (Generic)C);
}

entity_body(A) ::= explicit_attr_list(B) derive_decl(C) inverse_clause(D)
		   unique_clause(E) where_rule_OPT(F).
{
    A.attributes = B;
    /* this is flattened out in entity_decl - DEL */
    LISTadd_last(A.attributes, (Generic)C);

    if (D != LIST_NULL) {
	LISTadd_last(A.attributes,
        (Generic)D);
    }

    A.unique = E;
    A.where = F;
}

entity_decl ::= entity_header subsuper_decl(A) semicolon entity_body(B)
		TOK_END_ENTITY semicolon.
{
    CURRENT_SCOPE->u.entity->subtype_expression = A.subtypes;
    CURRENT_SCOPE->u.entity->supertype_symbols = A.supertypes;
    LISTdo (B.attributes, l, Linked_List)
    LISTdo (l, a, Variable)
    ENTITYadd_attribute(CURRENT_SCOPE, a);
    LISTod;
    LISTod;
    CURRENT_SCOPE->u.entity->abstract = A.abstract;
    CURRENT_SCOPE->u.entity->unique = B.unique;
    CURRENT_SCOPE->where = B.where;
    POP_SCOPE();
}

entity_header ::= TOK_ENTITY TOK_IDENTIFIER(A).
{
    Entity e = ENTITYcreate(A.symbol);

    if (print_objects_while_running & OBJ_ENTITY_BITS) {
        fprintf(stdout, "parse: %s (entity)\n", A.symbol->name);
    }

    PUSH_SCOPE(e, A.symbol, OBJ_ENTITY);
}

enumeration_type ::= TOK_ENUMERATION TOK_OF nested_id_list(A).
{
    int value = 0;
    Expression x;
    Symbol *tmp;
    TypeBody tb;
    tb = TYPEBODYcreate(enumeration_);
    CURRENT_SCOPE->u.type->head = 0;
    CURRENT_SCOPE->u.type->body = tb;
    tb->list = A;

    if (!CURRENT_SCOPE->symbol_table) {
        CURRENT_SCOPE->symbol_table = DICTcreate(25);
    }

    LISTdo_links(A, id) 

    tmp = (Symbol *)id->data;
    id->data = (Generic)(x = EXPcreate(CURRENT_SCOPE));
    x->symbol = *(tmp);
    x->u.integer = ++value;

    /* define both in enum scope and scope of */
    /* 1st visibility */
    DICT_define(CURRENT_SCOPE->symbol_table, x->symbol.name,
	(Generic)x, &x->symbol, OBJ_EXPRESSION);
    DICTdefine(PREVIOUS_SCOPE->symbol_table, x->symbol.name,
	(Generic)x, &x->symbol, OBJ_EXPRESSION);
    SYMBOL_destroy(tmp);
    LISTod;
}

escape_statement(A) ::= TOK_ESCAPE semicolon.
{
    A = STATEMENT_ESCAPE;
}

attribute_decl(A) ::= TOK_IDENTIFIER(B).
{
    A = EXPcreate(Type_Attribute);
    A->symbol = *B.symbol;
    SYMBOL_destroy(B.symbol);
}
attribute_decl(A) ::= TOK_SELF TOK_BACKSLASH TOK_IDENTIFIER(B) TOK_DOT
		      TOK_IDENTIFIER(C).
{
    A = EXPcreate(Type_Expression);
    A->e.op1 = EXPcreate(Type_Expression);
    A->e.op1->e.op_code = OP_GROUP;
    A->e.op1->e.op1 = EXPcreate(Type_Self);
    A->e.op1->e.op2 = EXPcreate_from_symbol(Type_Entity, B.symbol);
    SYMBOL_destroy(B.symbol);

    A->e.op_code = OP_DOT;
    A->e.op2 = EXPcreate_from_symbol(Type_Attribute, C.symbol);
    SYMBOL_destroy(C.symbol);
}

attribute_decl_list(A) ::= attribute_decl(B).
{
    A = LISTcreate();
    LISTadd_last(A, (Generic)B);

}
attribute_decl_list(A) ::= attribute_decl_list(B) TOK_COMMA
			   attribute_decl(C).
{
    A = B;
    LISTadd_last(A, (Generic)C);
}

optional(A) ::= /*NULL*/.
{
    A.optional = 0;
}
optional(A) ::= TOK_OPTIONAL.
{
    A.optional = 1;
}

explicit_attribute ::= attribute_decl_list(A) TOK_COLON optional(B)
		       attribute_type(C) semicolon.
{
    Variable v;

    LISTdo_links (A, attr)
    v = VARcreate((Expression)attr->data, C);
    v->flags.optional = B.optional;
    v->flags.attribute = True;
    attr->data = (Generic)v;
    LISTod;
}

express_file ::= schema_decl_list.

schema_decl_list(A) ::= schema_decl(B).
{
    A = B;
}
schema_decl_list(A) ::= schema_decl_list(B) schema_decl.
{
    A = B;
}

expression(A) ::= simple_expression(B).
{
    A = B;
}
expression(A) ::= expression(B) TOK_AND expression(C).
{
    yyerrok;

    A = BIN_EXPcreate(OP_AND, B, C);
}
expression(A) ::= expression(B) TOK_OR expression(C).
{
    yyerrok;

    A = BIN_EXPcreate(OP_OR, B, C);
}
expression(A) ::= expression(B) TOK_XOR expression(C).
{
    yyerrok;

    A = BIN_EXPcreate(OP_XOR, B, C);
}
expression(A) ::= expression(B) TOK_LESS_THAN expression(C).
{
    yyerrok;

    A = BIN_EXPcreate(OP_LESS_THAN, B, C);
}
expression(A) ::= expression(B) TOK_GREATER_THAN expression(C).
{
    yyerrok;

    A = BIN_EXPcreate(OP_GREATER_THAN, B, C);
}
expression(A) ::= expression(B) TOK_EQUAL expression(C).
{
    yyerrok;

    A = BIN_EXPcreate(OP_EQUAL, B, C);
}
expression(A) ::= expression(B) TOK_LESS_EQUAL expression(C).
{
    yyerrok;

    A = BIN_EXPcreate(OP_LESS_EQUAL, B, C);
}
expression(A) ::= expression(B) TOK_GREATER_EQUAL expression(C).
{
    yyerrok;

    A = BIN_EXPcreate(OP_GREATER_EQUAL, B, C);
}
expression(A) ::= expression(B) TOK_NOT_EQUAL expression(C).
{
    yyerrok;

    A = BIN_EXPcreate(OP_NOT_EQUAL, B, C);
}
expression(A) ::= expression(B) TOK_INST_EQUAL expression(C).
{
    yyerrok;

    A = BIN_EXPcreate(OP_INST_EQUAL, B, C);
}
expression(A) ::= expression(B) TOK_INST_NOT_EQUAL expression(C).
{
    yyerrok;

    A = BIN_EXPcreate(OP_INST_NOT_EQUAL, B, C);
}
expression(A) ::= expression(B) TOK_IN expression(C).
{
    yyerrok;

    A = BIN_EXPcreate(OP_IN, B, C);
}
expression(A) ::= expression(B) TOK_LIKE expression(C).
{
    yyerrok;

    A = BIN_EXPcreate(OP_LIKE, B, C);
}
expression ::= simple_expression cardinality_op simple_expression.
{
    yyerrok;
}

simple_expression(A) ::= unary_expression(B).
{
    A = B;
}
simple_expression(A) ::= simple_expression(B) TOK_CONCAT_OP
			 simple_expression(C).
{
    yyerrok;

    A = BIN_EXPcreate(OP_CONCAT, B, C);
}
simple_expression(A) ::= simple_expression(B) TOK_EXP simple_expression(C).
{
    yyerrok;

    A = BIN_EXPcreate(OP_EXP, B, C);
}
simple_expression(A) ::= simple_expression(B) TOK_TIMES simple_expression(C).
{
    yyerrok;

    A = BIN_EXPcreate(OP_TIMES, B, C);
}
simple_expression(A) ::= simple_expression(B) TOK_DIV simple_expression(C).
{
    yyerrok;

    A = BIN_EXPcreate(OP_DIV, B, C);
}
simple_expression(A) ::= simple_expression(B) TOK_REAL_DIV simple_expression(C).
{
    yyerrok;

    A = BIN_EXPcreate(OP_REAL_DIV, B, C);
}
simple_expression(A) ::= simple_expression(B) TOK_MOD simple_expression(C).
{
    yyerrok;

    A = BIN_EXPcreate(OP_MOD, B, C);
}
simple_expression(A) ::= simple_expression(B) TOK_PLUS simple_expression(C).
{
    yyerrok;

    A = BIN_EXPcreate(OP_PLUS, B, C);
}
simple_expression(A) ::= simple_expression(B) TOK_MINUS simple_expression(C).
{
    yyerrok;

    A = BIN_EXPcreate(OP_MINUS, B, C);
}

expression_list(A) ::= expression(B).
{
    A = LISTcreate();
    LISTadd_last(A, (Generic)B);
}
expression_list(A) ::= expression_list(B) TOK_COMMA expression(C).
{
    A = B;
    LISTadd_last(A, (Generic)C);
}

var(A) ::= /* NULL */.
{
    A.var = 1;
}
var(A) ::= TOK_VAR.
{
    A.var = 0;
}

formal_parameter(A) ::= var(B) id_list(C) TOK_COLON parameter_type(D).
{
    Symbol *tmp;
    Expression e;
    Variable v;

    A = C;
    LISTdo_links(A, param)
    tmp = (Symbol*)param->data;

    e = EXPcreate_from_symbol(Type_Attribute, tmp);
    v = VARcreate(e, D);
    v->flags.optional = B.var;
    v->flags.parameter = True;
    param->data = (Generic)v;

    /* link it in to the current scope's dict */
    DICTdefine(CURRENT_SCOPE->symbol_table,
    tmp->name, (Generic)v, tmp, OBJ_VARIABLE);

    LISTod;
}

formal_parameter_list(A) ::= /* no parameters */.
{
    A = LIST_NULL;
}
formal_parameter_list(A) ::= TOK_LEFT_PAREN formal_parameter_rep(B)
			     TOK_RIGHT_PAREN.
{
    A = B;

}

formal_parameter_rep(A) ::= formal_parameter(B).
{
    A = B;

}
formal_parameter_rep(A) ::= formal_parameter_rep(B) semicolon
			    formal_parameter(C).
{
    A = B;
    LISTadd_all(A, C);
}

parameter_type(A) ::= basic_type(B).
{
    A = TYPEcreate_from_body_anonymously(B);
    SCOPEadd_super(A);
}
parameter_type(A) ::= conformant_aggregation(B).
{
    A = TYPEcreate_from_body_anonymously(B);
    SCOPEadd_super(A);
}
parameter_type(A) ::= defined_type(B).
{
    A = B;
}
parameter_type(A) ::= generic_type(B).
{
    A = B;
}

function_call(A) ::= function_id(B) actual_parameters(C).
{
    A = EXPcreate(Type_Funcall);
    A->symbol = *B;
    SYMBOL_destroy(B);
    A->u.funcall.list = C;
}

function_decl ::= function_header(A) action_body(B) TOK_END_FUNCTION
		  semicolon.
{
    FUNCput_body(CURRENT_SCOPE, B);
    ALGput_full_text(CURRENT_SCOPE, A, SCANtell());
    POP_SCOPE();
}

function_header(A) ::= fh_lineno(B) fh_push_scope fh_plist TOK_COLON
		       parameter_type(C) semicolon.
{ 
    Function f = CURRENT_SCOPE;

    f->u.func->return_type = C;
    A = B;
}

fh_lineno(A) ::= TOK_FUNCTION.
{
    A = SCANtell();
}

fh_push_scope ::= TOK_IDENTIFIER(A).
{
    Function f = ALGcreate(OBJ_FUNCTION);
    tag_count = 0;
    if (print_objects_while_running & OBJ_FUNCTION_BITS) {
        fprintf(stdout, "parse: %s (function)\n", A.symbol->name);
    }
    PUSH_SCOPE(f, A.symbol, OBJ_FUNCTION);
}

fh_plist ::= formal_parameter_list(A).
{
    Function f = CURRENT_SCOPE;
    f->u.func->parameters = A;
    f->u.func->pcount = LISTget_length(A);
    f->u.func->tag_count = tag_count;
    tag_count = -1;	/* done with parameters, no new tags can be defined */
}

function_id(A) ::= TOK_IDENTIFIER(B).
{
    A = B.symbol;
}
function_id(A) ::= TOK_BUILTIN_FUNCTION(B).
{
    A = B.symbol;

}

conformant_aggregation(A) ::= aggregate_type(B).
{
    A = B;

}
conformant_aggregation(A) ::= TOK_ARRAY TOK_OF optional_or_unique(B)
			      parameter_type(C).
{
    A = TYPEBODYcreate(array_);
    A->flags.optional = B.optional;
    A->flags.unique = B.unique;
    A->base = C;
}
conformant_aggregation(A) ::= TOK_ARRAY index_spec(B) TOK_OF
    optional_or_unique(C) parameter_type(D).
{
    A = TYPEBODYcreate(array_);
    A->flags.optional = C.optional;
    A->flags.unique = C.unique;
    A->base = D;
    A->upper = B.upper_limit;
    A->lower = B.lower_limit;
}
conformant_aggregation(A) ::= TOK_BAG TOK_OF parameter_type(B).
{
    A = TYPEBODYcreate(bag_);
    A->base = B;

}
conformant_aggregation(A) ::= TOK_BAG index_spec(B) TOK_OF parameter_type(C).
{
    A = TYPEBODYcreate(bag_);
    A->base = C;
    A->upper = B.upper_limit;
    A->lower = B.lower_limit;
}
conformant_aggregation(A) ::= TOK_LIST TOK_OF unique(B) parameter_type(C).
{
    A = TYPEBODYcreate(list_);
    A->flags.unique = B.unique;
    A->base = C;

}
conformant_aggregation(A) ::= TOK_LIST index_spec(B) TOK_OF unique(C)
			      parameter_type(D).
{
    A = TYPEBODYcreate(list_);
    A->base = D;
    A->flags.unique = C.unique;
    A->upper = B.upper_limit;
    A->lower = B.lower_limit;
}
conformant_aggregation(A) ::= TOK_SET TOK_OF parameter_type(B).
{
    A = TYPEBODYcreate(set_);
    A->base = B;
}
conformant_aggregation(A) ::= TOK_SET index_spec(B) TOK_OF parameter_type(C).
{
    A = TYPEBODYcreate(set_);
    A->base = C;
    A->upper = B.upper_limit;
    A->lower = B.lower_limit;
}

generic_type(A) ::= TOK_GENERIC.
{
    A = Type_Generic;

    if (tag_count < 0) {
        Symbol sym;
        sym.line = yylineno;
        sym.filename = current_filename;
        ERRORreport_with_symbol(ERROR_unlabelled_param_type, &sym,
	    CURRENT_SCOPE_NAME);
    }
}
generic_type(A) ::= TOK_GENERIC TOK_COLON TOK_IDENTIFIER(B).
{
    TypeBody g = TYPEBODYcreate(generic_);
    A = TYPEcreate_from_body_anonymously(g);

    SCOPEadd_super(A);

    if (g->tag = TYPEcreate_user_defined_tag(A, CURRENT_SCOPE, B.symbol)) {
        SCOPEadd_super(g->tag);
    }
}

id_list(A) ::= TOK_IDENTIFIER(B).
{
    A = LISTcreate();
    LISTadd_last(A, (Generic)B.symbol);

}
id_list(A) ::= id_list(B) TOK_COMMA TOK_IDENTIFIER(C).
{
    yyerrok;

    A = B;
    LISTadd_last(A, (Generic)C.symbol);
}

identifier(A) ::= TOK_SELF.
{
    A = EXPcreate(Type_Self);
}
identifier(A) ::= TOK_QUESTION_MARK.
{
    A = LITERAL_INFINITY;
}
identifier(A) ::= TOK_IDENTIFIER(B).
{
    A = EXPcreate(Type_Identifier);
    A->symbol = *(B.symbol);
    SYMBOL_destroy(B.symbol);
}

if_statement(A) ::= TOK_IF expression(B) TOK_THEN statement_rep(C) TOK_END_IF
		    semicolon.
{
    A = CONDcreate(B, C, STATEMENT_LIST_NULL);
}
if_statement(A) ::= TOK_IF expression(B) TOK_THEN statement_rep(C) TOK_ELSE
    statement_rep(D) TOK_END_IF semicolon.
{
    A = CONDcreate(B, C, D);
}

include_directive ::= TOK_INCLUDE TOK_STRING_LITERAL(A) semicolon.
{
    SCANinclude_file(A.string);
}

increment_control ::= TOK_IDENTIFIER(A) TOK_ASSIGNMENT expression(B) TOK_TO
		      expression(C) by_expression(D).
{
    Increment i = INCR_CTLcreate(A.symbol, B, C, D);

    /* scope doesn't really have/need a name, I suppose */
    /* naming it by the iterator variable is fine */

    PUSH_SCOPE(i, (Symbol *)0, OBJ_INCREMENT);
}

index_spec(A) ::= TOK_LEFT_BRACKET expression(B) TOK_COLON expression(C)
		  TOK_RIGHT_BRACKET.
{
    A.lower_limit = B;
    A.upper_limit = C;
}

initializer(A) ::= TOK_ASSIGNMENT expression(B).
{
    A = B;
}

rename ::= TOK_IDENTIFIER(A).
{
    (*interface_func)(CURRENT_SCOPE, interface_schema, A, A);
}
rename ::= TOK_IDENTIFIER(A) TOK_AS TOK_IDENTIFIER(B).
{
    (*interface_func)(CURRENT_SCOPE, interface_schema, A, B);
}

rename_list(A) ::= rename(B).
{
    A = B;
}
rename_list(A) ::= rename_list(B) TOK_COMMA rename.
{
    A = B;
}

parened_rename_list ::= TOK_LEFT_PAREN rename_list TOK_RIGHT_PAREN.  

reference_clause ::= TOK_REFERENCE TOK_FROM TOK_IDENTIFIER(A) semicolon.
{
    if (!CURRENT_SCHEMA->ref_schemas) {
        CURRENT_SCHEMA->ref_schemas = LISTcreate();
    }

    LISTadd(CURRENT_SCHEMA->ref_schemas, (Generic)A.symbol);
}
reference_clause(A) ::= reference_head(B) parened_rename_list semicolon.
{
    A = B;
}

reference_head ::= TOK_REFERENCE TOK_FROM TOK_IDENTIFIER(A).
{
    interface_schema = A.symbol;
    interface_func = SCHEMAadd_reference;
}

use_clause ::= TOK_USE TOK_FROM TOK_IDENTIFIER(A) semicolon.
{
    if (!CURRENT_SCHEMA->use_schemas) {
        CURRENT_SCHEMA->use_schemas = LISTcreate();
    }

    LISTadd(CURRENT_SCHEMA->use_schemas, (Generic)A.symbol);
}
use_clause(A) ::= use_head(B) parened_rename_list semicolon.
{
    A = B;
}

use_head ::= TOK_USE TOK_FROM TOK_IDENTIFIER(A).
{
    interface_schema = A.symbol;
    interface_func = SCHEMAadd_use;
}

interface_specification(A) ::= use_clause(B).
{
    A = B;
}
interface_specification(A) ::= reference_clause(B).
{
    A = B;
}

interface_specification_list ::= /*NULL*/.
interface_specification_list(A) ::= interface_specification_list(B)
				    interface_specification.
{
    A = B;
}

interval(A) ::= TOK_LEFT_CURL simple_expression(B) rel_op(C)
		simple_expression(D) rel_op(E) simple_expression(F) right_curl.
{
    Expression	tmp1, tmp2;

    A = (Expression)0;
    tmp1 = BIN_EXPcreate(C, B, D);
    tmp2 = BIN_EXPcreate(E, D, F);
    A = BIN_EXPcreate(OP_AND, tmp1, tmp2);
}

/* defined_type might have to be something else since it's really an
 * entity_ref */
set_or_bag_of_entity(A) ::= defined_type(B).
{
    A.type = B;
    A.body = 0;
}
set_or_bag_of_entity(A) ::= TOK_SET TOK_OF defined_type(B).
{
    A.type = 0;
    A.body = TYPEBODYcreate(set_);
    A.body->base = B;

}
set_or_bag_of_entity(A) ::= TOK_SET limit_spec(B) TOK_OF defined_type(C).
{
    A.type = 0; 
    A.body = TYPEBODYcreate(set_);
    A.body->base = C;
    A.body->upper = B.upper_limit;
    A.body->lower = B.lower_limit;
}
set_or_bag_of_entity(A) ::= TOK_BAG limit_spec(B) TOK_OF defined_type(C).
{
    A.type = 0;
    A.body = TYPEBODYcreate(bag_);
    A.body->base = C;
    A.body->upper = B.upper_limit;
    A.body->lower = B.lower_limit;
}
set_or_bag_of_entity(A) ::= TOK_BAG TOK_OF defined_type(B).
{
    A.type = 0;
    A.body = TYPEBODYcreate(bag_);
    A.body->base = B;
}

inverse_attr_list(A) ::= inverse_attr(B).
{
    A = LISTcreate();
    LISTadd_last(A, (Generic)B);
}
inverse_attr_list(A) ::= inverse_attr_list(B) inverse_attr(C).
{
    A = B;
    LISTadd_last(A, (Generic)C);
}

inverse_attr(A) ::= TOK_IDENTIFIER(B) TOK_COLON set_or_bag_of_entity(C)
		    TOK_FOR TOK_IDENTIFIER(D) semicolon.
{
    Expression e = EXPcreate(Type_Attribute);

    e->symbol = *B.symbol;
    SYMBOL_destroy(B.symbol);

    if (C.type) {
        A = VARcreate(e, C.type);
    } else {
        Type t = TYPEcreate_from_body_anonymously(C.body);
        SCOPEadd_super(t);
        A = VARcreate(e, t);
    }

    A->flags.attribute = True;
    A->inverse_symbol = D.symbol;
}

inverse_clause(A) ::= /*NULL*/.
{
    A = LIST_NULL;
}
inverse_clause(A) ::= TOK_INVERSE inverse_attr_list(B).
{
    A = B;
}

limit_spec(A) ::= TOK_LEFT_BRACKET expression(B) TOK_COLON expression(C)
		  TOK_RIGHT_BRACKET.
{
    A.lower_limit = B;
    A.upper_limit = C;
}

list_type(A) ::= TOK_LIST limit_spec(B) TOK_OF unique(C) attribute_type(D).
{
    A = TYPEBODYcreate(list_);
    A->base = D;
    A->flags.unique = C.unique;
    A->lower = B.lower_limit;
    A->upper = B.upper_limit;
}
list_type(A) ::= TOK_LIST TOK_OF unique(B) attribute_type(C).
{
    A = TYPEBODYcreate(list_);
    A->base = C;
    A->flags.unique = B.unique;
}

literal(A) ::= TOK_INTEGER_LITERAL(B).
{
    if (B.iVal == 0) {
        A = LITERAL_ZERO;
    } else if (B.iVal == 1) {
	A = LITERAL_ONE;
    } else {
	A = EXPcreate_simple(Type_Integer);
	A->u.integer = (int)B.iVal;
	resolved_all(A);
    }
}
literal(A) ::= TOK_REAL_LITERAL(B).
{
    if (B.rVal == 0.0) {
	A = LITERAL_ZERO;
    } else {
	A = EXPcreate_simple(Type_Real);
	A->u.real = B.rVal;
	resolved_all(A);
    }
}
literal(A) ::= TOK_STRING_LITERAL(B).
{
    A = EXPcreate_simple(Type_String);
    A->symbol.name = B.string;
    resolved_all(A);
}
literal(A) ::= TOK_STRING_LITERAL_ENCODED(B).
{
    A = EXPcreate_simple(Type_String_Encoded);
    A->symbol.name = B.string;
    resolved_all(A);
}
literal(A) ::= TOK_LOGICAL_LITERAL(B).
{
    A = EXPcreate_simple(Type_Logical);
    A->u.logical = B.logical;
    resolved_all(A);
}
literal(A) ::= TOK_BINARY_LITERAL(B).
{
    A = EXPcreate_simple(Type_Binary);
    A->symbol.name = B.binary;
    resolved_all(A);
}
literal(A) ::= constant(B).
{
    A = B;
}

local_initializer(A) ::= TOK_ASSIGNMENT expression(B).
{
    A = B;
}

local_variable ::= id_list(A) TOK_COLON parameter_type(B) semicolon.
{
    Expression e;
    Variable v;
    LISTdo(A, sym, Symbol *)

    /* convert symbol to name-expression */

    e = EXPcreate(Type_Attribute);
    e->symbol = *sym; SYMBOL_destroy(sym);
    v = VARcreate(e, B);
    DICTdefine(CURRENT_SCOPE->symbol_table, e->symbol.name, (Generic)v,
	&e->symbol, OBJ_VARIABLE);
    LISTod;
    LISTfree(A);
}
local_variable ::= id_list(A) TOK_COLON parameter_type(B) local_initializer(C)
		   semicolon.
{
    Expression e;
    Variable v;
    LISTdo(A, sym, Symbol *)
    e = EXPcreate(Type_Attribute);
    e->symbol = *sym; SYMBOL_destroy(sym);
    v = VARcreate(e, B);
    v->initializer = C;
    DICTdefine(CURRENT_SCOPE->symbol_table, e->symbol.name, (Generic)v,
	&e->symbol, OBJ_VARIABLE);
    LISTod;
    LISTfree(A);
}

local_body ::= /* no local_variables */.
local_body(A) ::= local_variable(B) local_body.
{
    A = B;
}

local_decl ::= TOK_LOCAL local_body TOK_END_LOCAL semicolon.

defined_type(A) ::= TOK_IDENTIFIER(B).
{
    A = TYPEcreate_name(B.symbol);
    SCOPEadd_super(A);
    SYMBOL_destroy(B.symbol);
}

defined_type_list(A) ::= defined_type(B).
{
    A = LISTcreate();
    LISTadd(A, (Generic)B);

}
defined_type_list(A) ::= defined_type_list(B) TOK_COMMA defined_type(C).
{
    A = B;
    LISTadd_last(A,
    (Generic)C);
}

nested_id_list(A) ::= TOK_LEFT_PAREN id_list(B) TOK_RIGHT_PAREN.
{
    A = B;
}

oneof_op(A) ::= TOK_ONEOF(B).
{
    A = B;
}

optional_or_unique(A) ::= /* NULL body */.
{
    A.unique = 0;
    A.optional = 0;
}
optional_or_unique(A) ::= TOK_OPTIONAL.
{
    A.unique = 0;
    A.optional = 1;
}
optional_or_unique(A) ::= TOK_UNIQUE.
{
    A.unique = 1;
    A.optional = 0;
}
optional_or_unique(A) ::= TOK_OPTIONAL TOK_UNIQUE.
{
    A.unique = 1;
    A.optional = 1;
}
optional_or_unique(A) ::= TOK_UNIQUE TOK_OPTIONAL.
{
    A.unique = 1;
    A.optional = 1;
}

optional_fixed(A) ::= /* nuthin' */.
{
    A.fixed = 0;
}
optional_fixed(A) ::= TOK_FIXED.
{
    A.fixed = 1;
}

precision_spec(A) ::= /* no precision specified */.
{
    A = (Expression)0;
}
precision_spec(A) ::= TOK_LEFT_PAREN expression(B) TOK_RIGHT_PAREN.
{
    A = B;
}

/* NOTE: actual parameters cannot go to NULL, since this causes
 * a syntactic ambiguity (see note at actual_parameters).  hence
 * the need for the second branch of this rule.
 */

proc_call_statement(A) ::= procedure_id(B) actual_parameters(C) semicolon.
{
    A = PCALLcreate(C);
    A->symbol = *(B);
}
proc_call_statement(A) ::= procedure_id(B) semicolon.
{
    A = PCALLcreate((Linked_List)0);
    A->symbol = *(B);
}

procedure_decl ::= procedure_header(A) action_body(B) TOK_END_PROCEDURE
		   semicolon.
{
    PROCput_body(CURRENT_SCOPE, B);
    ALGput_full_text(CURRENT_SCOPE, A, SCANtell());
    POP_SCOPE();
}

procedure_header(A) ::= TOK_PROCEDURE ph_get_line(B) ph_push_scope
			formal_parameter_list(C) semicolon.
{
    Procedure p = CURRENT_SCOPE;
    p->u.proc->parameters = C;
    p->u.proc->pcount = LISTget_length(C);
    p->u.proc->tag_count = tag_count;
    tag_count = -1;	/* done with parameters, no new tags can be defined */
    A = B;
}

ph_push_scope ::= TOK_IDENTIFIER(A).
{
    Procedure p = ALGcreate(OBJ_PROCEDURE);
    tag_count = 0;

    if (print_objects_while_running & OBJ_PROCEDURE_BITS) {
	fprintf(stdout, "parse: %s (procedure)\n", A.symbol->name);
    }

    PUSH_SCOPE(p, A.symbol, OBJ_PROCEDURE);
}

ph_get_line(A) ::= /* subroutine */.
{
    A = SCANtell();
}

procedure_id(A) ::= TOK_IDENTIFIER(B).
{
    A = B.symbol;
}
procedure_id(A) ::= TOK_BUILTIN_PROCEDURE(B).
{
    A = B.symbol;
}

group_ref(A) ::= TOK_BACKSLASH TOK_IDENTIFIER(B).
{
    A = BIN_EXPcreate(OP_GROUP, (Expression)0, (Expression)0);
    A->e.op2 = EXPcreate(Type_Identifier);
    A->e.op2->symbol = *B.symbol;
    SYMBOL_destroy(B.symbol);
}

qualifier(A) ::= TOK_DOT TOK_IDENTIFIER(B).
{
    A.expr = A.first = BIN_EXPcreate(OP_DOT, (Expression)0, (Expression)0);
    A.expr->e.op2 = EXPcreate(Type_Identifier);
    A.expr->e.op2->symbol = *B.symbol;
    SYMBOL_destroy(B.symbol);
}
qualifier(A) ::= TOK_BACKSLASH TOK_IDENTIFIER(B). [TOK_NOT]
{
    A.expr = A.first = BIN_EXPcreate(OP_GROUP, (Expression)0, (Expression)0);
    A.expr->e.op2 = EXPcreate(Type_Identifier);
    A.expr->e.op2->symbol = *B.symbol;
    SYMBOL_destroy(B.symbol);
}
qualifier(A) ::= TOK_LEFT_BRACKET simple_expression(B) TOK_RIGHT_BRACKET.
{
    A.expr = A.first = BIN_EXPcreate(OP_ARRAY_ELEMENT, (Expression)0,
	(Expression)0);
    A.expr->e.op2 = B;
}
qualifier(A) ::= TOK_LEFT_BRACKET simple_expression(B) TOK_COLON
		 simple_expression(C) TOK_RIGHT_BRACKET.
{
    A.expr = A.first = TERN_EXPcreate(OP_SUBCOMPONENT, (Expression)0,
	(Expression)0, (Expression)0);
    A.expr->e.op2 = B;
    A.expr->e.op3 = C;
}

query_expression(A) ::= query_start(B) expression(C) TOK_RIGHT_PAREN.
{
    A = B;
    A->u.query->expression = C;
    POP_SCOPE();
}

query_start(A) ::= TOK_QUERY TOK_LEFT_PAREN TOK_IDENTIFIER(B) TOK_ALL_IN
		   expression(C) TOK_SUCH_THAT.
{
    A = QUERYcreate(B.symbol, C);
    SYMBOL_destroy(B.symbol);
    PUSH_SCOPE(A->u.query->scope, (Symbol *)0, OBJ_QUERY);
}

rel_op(A) ::= TOK_LESS_THAN.
{
    A = OP_LESS_THAN;
}
rel_op(A) ::= TOK_GREATER_THAN.
{
    A = OP_GREATER_THAN;
}
rel_op(A) ::= TOK_EQUAL.
{
    A = OP_EQUAL;
}
rel_op(A) ::= TOK_LESS_EQUAL.
{
    A = OP_LESS_EQUAL;
}
rel_op(A) ::= TOK_GREATER_EQUAL.
{
    A = OP_GREATER_EQUAL;
}
rel_op(A) ::= TOK_NOT_EQUAL.
{
    A = OP_NOT_EQUAL;
}
rel_op(A) ::= TOK_INST_EQUAL.
{
    A = OP_INST_EQUAL;
}
rel_op(A) ::= TOK_INST_NOT_EQUAL.
{
    A = OP_INST_NOT_EQUAL;
}

/* repeat_statement causes a scope creation if an increment_control exists */
repeat_statement(A) ::= TOK_REPEAT increment_control while_control(B)
			until_control(C) semicolon statement_rep(D)
			TOK_END_REPEAT semicolon.
{
    A = LOOPcreate(CURRENT_SCOPE, B, C, D);

    /* matching PUSH_SCOPE is in increment_control */
    POP_SCOPE();
}
repeat_statement(A) ::= TOK_REPEAT while_control(B) until_control(C) semicolon
			statement_rep(D) TOK_END_REPEAT semicolon.
{
    A = LOOPcreate((struct Scope_ *)0, B, C, D);
}

return_statement(A) ::= TOK_RETURN semicolon.
{
    A = RETcreate((Expression)0);
}
return_statement(A) ::= TOK_RETURN TOK_LEFT_PAREN expression(B) TOK_RIGHT_PAREN
			semicolon.
{
    A = RETcreate(B);
}

right_curl ::= TOK_RIGHT_CURL.
{
    yyerrok;
}

rule_decl ::= rule_header(A) action_body(B) where_rule(C) TOK_END_RULE
	      semicolon.
{
    RULEput_body(CURRENT_SCOPE, B);
    RULEput_where(CURRENT_SCOPE, C);
    ALGput_full_text(CURRENT_SCOPE, A, SCANtell());
    POP_SCOPE();
}

rule_formal_parameter(A) ::= TOK_IDENTIFIER(B).
{
    Expression e;
    Type t;

    /* it's true that we know it will be an entity_ type later */
    TypeBody tb = TYPEBODYcreate(set_);
    tb->base = TYPEcreate_name(B.symbol);
    SCOPEadd_super(tb->base);
    t = TYPEcreate_from_body_anonymously(tb);
    SCOPEadd_super(t);
    e = EXPcreate_from_symbol(t, B.symbol);
    A = VARcreate(e, t);
    A->flags.attribute = True;
    A->flags.parameter = True;

    /* link it in to the current scope's dict */
    DICTdefine(CURRENT_SCOPE->symbol_table, B.symbol->name, (Generic)A,
	B.symbol, OBJ_VARIABLE);
}

rule_formal_parameter_list(A) ::= rule_formal_parameter(B).
{
    A = LISTcreate();
    LISTadd(A, (Generic)B); 
}
rule_formal_parameter_list(A) ::= rule_formal_parameter_list(B) TOK_COMMA
				  rule_formal_parameter(C).
{
    A = B;
    LISTadd_last(A, (Generic)C);
}

rule_header(A) ::= rh_start(B) rule_formal_parameter_list(C) TOK_RIGHT_PAREN
		   semicolon.
{
    CURRENT_SCOPE->u.rule->parameters = C;

    A = B;
}

rh_start(A) ::= TOK_RULE rh_get_line(B) TOK_IDENTIFIER(C) TOK_FOR
		TOK_LEFT_PAREN.
{
    Rule r = ALGcreate(OBJ_RULE);

    if (print_objects_while_running & OBJ_RULE_BITS) {
	fprintf(stdout, "parse: %s (rule)\n", C.symbol->name);
    }

    PUSH_SCOPE(r, C.symbol, OBJ_RULE);

    A = B;
}

rh_get_line(A) ::= /* subroutine */.
{
    A = SCANtell();
}

schema_body(A) ::= interface_specification_list(B) block_list.
{
    A = B;
}
schema_body(A) ::= interface_specification_list(B) constant_decl block_list.
{
    A = B;
}

schema_decl ::= schema_header schema_body TOK_END_SCHEMA semicolon.
{
    POP_SCOPE();
}
schema_decl(A) ::= include_directive(B).
{
    A = B;
}

schema_header ::= TOK_SCHEMA TOK_IDENTIFIER(A) semicolon.
{
    Schema schema = DICTlookup(CURRENT_SCOPE->symbol_table, A.symbol->name);

    if (print_objects_while_running & OBJ_SCHEMA_BITS) {
	fprintf(stdout, "parse: %s (schema)\n", A.symbol->name);
    }

    if (EXPRESSignore_duplicate_schemas && schema) {
	SCANskip_to_end_schema();
	PUSH_SCOPE_DUMMY();
    } else {
	schema = SCHEMAcreate();
	LISTadd_last(PARSEnew_schemas, (Generic)schema);
	PUSH_SCOPE(schema, A.symbol, OBJ_SCHEMA);
    }
}

select_type(A) ::= TOK_SELECT TOK_LEFT_PAREN defined_type_list(B)
		   TOK_RIGHT_PAREN.
{
    A = TYPEBODYcreate(select_);
    A->list = B;
}

semicolon ::= TOK_SEMICOLON.
{
    yyerrok;
}

set_type(A) ::= TOK_SET limit_spec(B) TOK_OF attribute_type(C).
{
    A = TYPEBODYcreate(set_);
    A->base = C;
    A->lower = B.lower_limit;
    A->upper = B.upper_limit;
}
set_type(A) ::= TOK_SET TOK_OF attribute_type(B).
{
    A = TYPEBODYcreate(set_);
    A->base = B;
}

skip_statement(A) ::= TOK_SKIP semicolon.
{
    A = STATEMENT_SKIP;
}

statement(A) ::= alias_statement(B).
{
    A = B;
}
statement(A) ::= assignment_statement(B).
{
    A = B;
}
statement(A) ::= case_statement(B).
{
    A = B;
}
statement(A) ::= compound_statement(B).
{
    A = B;
}
statement(A) ::= escape_statement(B).
{
    A = B;
}
statement(A) ::= if_statement(B).
{
    A = B;
}
statement(A) ::= proc_call_statement(B).
{
    A = B;
}
statement(A) ::= repeat_statement(B).
{
    A = B;
}
statement(A) ::= return_statement(B).
{
    A = B;
}
statement(A) ::= skip_statement(B).
{
    A = B;
}

statement_rep(A) ::= /* no statements */.
{
    A = LISTcreate();
}
statement_rep(A) ::= /* ignore null statement */ semicolon statement_rep(B).
{
    A = B;
}
statement_rep(A) ::= statement(B) statement_rep(C).
{
    A = C;
    LISTadd_first(A, (Generic)B); 
}

/* if the actions look backwards, remember the declaration syntax:
 * <entity> SUPERTYPE OF <subtype1> ...  SUBTYPE OF <supertype1> ...
 */

subsuper_decl(A) ::= /* NULL body */.
{
    A.subtypes = EXPRESSION_NULL;
    A.abstract = False;
    A.supertypes = LIST_NULL;
}
subsuper_decl(A) ::= supertype_decl(B).
{
    A.subtypes = B.subtypes;
    A.abstract = B.abstract;
    A.supertypes = LIST_NULL;
}
subsuper_decl(A) ::= subtype_decl(B).
{
    A.supertypes = B;
    A.abstract = False;
    A.subtypes = EXPRESSION_NULL;
}
subsuper_decl(A) ::= supertype_decl(B) subtype_decl(C).
{
    A.subtypes = B.subtypes;
    A.abstract = B.abstract;
    A.supertypes = C;
}

subtype_decl(A) ::= TOK_SUBTYPE TOK_OF TOK_LEFT_PAREN defined_type_list(B)
    TOK_RIGHT_PAREN.
{
    A = B;
}

supertype_decl(A) ::= TOK_ABSTRACT TOK_SUPERTYPE.
{
    A.subtypes = (Expression)0;
    A.abstract = True;
}
supertype_decl(A) ::= TOK_SUPERTYPE TOK_OF TOK_LEFT_PAREN
		      supertype_expression(B) TOK_RIGHT_PAREN.
{
    A.subtypes = B;
    A.abstract = False;
}
supertype_decl(A) ::= TOK_ABSTRACT TOK_SUPERTYPE TOK_OF TOK_LEFT_PAREN
		      supertype_expression(B) TOK_RIGHT_PAREN.
{
    A.subtypes = B;
    A.abstract = True;
}

supertype_expression(A) ::= supertype_factor(B).
{
    A = B.subtypes;
}
supertype_expression(A) ::= supertype_expression(B) TOK_AND supertype_factor(C).
{
    A = BIN_EXPcreate(OP_AND, B, C.subtypes);
}
supertype_expression(A) ::= supertype_expression(B) TOK_ANDOR
			    supertype_factor(C).
{
    A = BIN_EXPcreate(OP_ANDOR, B, C.subtypes);
}

supertype_expression_list(A) ::= supertype_expression(B).
{
    A = LISTcreate();
    LISTadd_last(A, (Generic)B);
}
supertype_expression_list(A) ::= supertype_expression_list(B) TOK_COMMA
				 supertype_expression(C).
{
    LISTadd_last(B, (Generic)C);
    A = B;
}

supertype_factor(A) ::= identifier(B).
{
    A.subtypes = B;
}
supertype_factor(A) ::= oneof_op TOK_LEFT_PAREN supertype_expression_list(B)
			TOK_RIGHT_PAREN.
{
    A.subtypes = EXPcreate(Type_Oneof);
    A.subtypes->u.list = B;
}
supertype_factor(A) ::= TOK_LEFT_PAREN supertype_expression(B) TOK_RIGHT_PAREN.
{
    A.subtypes = B;
}

type(A) ::= aggregation_type(B).
{
    A.type = 0;
    A.body = B;
}
type(A) ::= basic_type(B).
{
    A.type = 0;
    A.body = B;
}
type(A) ::= defined_type(B).
{
    A.type = B;
    A.body = 0;
}
type(A) ::= select_type(B).
{
    A.type = 0;
    A.body = B;
}

type_item_body(A) ::= enumeration_type(B).
{
    A = B;
}
type_item_body ::= type(A).
{
    CURRENT_SCOPE->u.type->head = A.type;
    CURRENT_SCOPE->u.type->body = A.body;
}

type_item ::= ti_start type_item_body semicolon.

ti_start ::= TOK_IDENTIFIER(A) TOK_EQUAL.
{
    Type t = TYPEcreate_name(A.symbol);
    PUSH_SCOPE(t, A.symbol, OBJ_TYPE);
}

type_decl(A) ::= td_start(B) TOK_END_TYPE semicolon.
{
    A = B;
}

td_start(A) ::= TOK_TYPE(B) type_item where_rule_OPT(C).
{
    CURRENT_SCOPE->where = C;
    POP_SCOPE();
    A = B;
}

general_ref(A) ::= assignable(B) group_ref(C).
{
    C->e.op1 = B;
    A = C;
}
general_ref(A) ::= assignable(B).
{
    A = B;
}

unary_expression(A) ::= aggregate_initializer(B).
{
    A = B;
}
unary_expression(A) ::= unary_expression(B) qualifier(C).
{
    C.first->e.op1 = B;
    A = C.expr;
}
unary_expression(A) ::= literal(B).
{
    A = B;
}
unary_expression(A) ::= function_call(B).
{
    A = B;
}
unary_expression(A) ::= identifier(B).
{
    A = B;
}
unary_expression(A) ::= TOK_LEFT_PAREN expression(B) TOK_RIGHT_PAREN.
{
    A = B;
}
unary_expression(A) ::= interval(B).
{
    A = B;
}
unary_expression(A) ::= query_expression(B).
{
    A = B;
}
unary_expression(A) ::= TOK_NOT unary_expression(B).
{
    A = UN_EXPcreate(OP_NOT, B);
}
unary_expression(A) ::= TOK_PLUS unary_expression(B).  [TOK_NOT]
{
    A = B;
}
unary_expression(A) ::= TOK_MINUS unary_expression(B).  [TOK_NOT]
{
    A = UN_EXPcreate(OP_NEGATE, B);
}

unique(A) ::= /* look for optional UNIQUE */.
{
    A.unique = 0;
}
unique(A) ::= TOK_UNIQUE.
{
    A.unique = 1;
}

qualified_attr(A) ::= TOK_IDENTIFIER(B).
{
    A = QUAL_ATTR_new();
    A->attribute = B.symbol;
}
qualified_attr(A) ::= TOK_SELF TOK_BACKSLASH TOK_IDENTIFIER(B) TOK_DOT
		      TOK_IDENTIFIER(C).
{
    A = QUAL_ATTR_new();
    A->entity = B.symbol;
    A->attribute = C.symbol;
}

qualified_attr_list(A) ::= qualified_attr(B).
{
    A = LISTcreate();
    LISTadd_last(A, (Generic)B);
}
qualified_attr_list(A) ::= qualified_attr_list(B) TOK_COMMA qualified_attr(C).
{
    A = B;
    LISTadd_last(A, (Generic)C);
}

labelled_attrib_list(A) ::= qualified_attr_list(B) semicolon.
{
    LISTadd_first(B, (Generic)EXPRESSION_NULL);
    A = B;
}
labelled_attrib_list(A) ::= TOK_IDENTIFIER(B) TOK_COLON qualified_attr_list(C)
			    semicolon.
{
    LISTadd_first(C, (Generic)B.symbol); 
    A = C;
}

/* returns a list */
labelled_attrib_list_list(A) ::= labelled_attrib_list(B).
{
    A = LISTcreate();
    LISTadd_last(A, (Generic)B);
}
labelled_attrib_list_list(A) ::= labelled_attrib_list_list(B)
				 labelled_attrib_list(C).
{
    LISTadd_last(A, (Generic)C);
    A = B;
}

unique_clause(A) ::= /* unique clause is always optional */.
{
    A = 0;
}
unique_clause(A) ::= TOK_UNIQUE labelled_attrib_list_list(B).
{
    A = B;
}

until_control(A) ::= /* NULL */.
{
    A = 0;
}
until_control(A) ::= TOK_UNTIL expression(B).
{
    A = B;
}

where_clause(A) ::= expression(B) semicolon.
{
    A = WHERE_new();
    A->label = SYMBOLcreate("<unnamed>", yylineno, current_filename);
    A->expr = B;
}
where_clause(A) ::= TOK_IDENTIFIER(B) TOK_COLON expression(C) semicolon.
{
    A = WHERE_new();
    A->label = B.symbol;
    A->expr = C;

    if (!CURRENT_SCOPE->symbol_table) {
	CURRENT_SCOPE->symbol_table = DICTcreate(25);
    }

    DICTdefine(CURRENT_SCOPE->symbol_table, B.symbol->name, (Generic)A,
	B.symbol, OBJ_WHERE);
}

where_clause_list(A) ::= where_clause(B).
{
    A = LISTcreate();
    LISTadd(A, (Generic)B);
}
where_clause_list(A) ::= where_clause_list(B) where_clause(C).
{
    A = B;
    LISTadd_last(A, (Generic)C);
}

where_rule(A) ::= TOK_WHERE where_clause_list(B).
{
    A = B;
}

where_rule_OPT(A) ::= /* NULL body: no where rule */.
{
    A = LIST_NULL;
}
where_rule_OPT(A) ::= where_rule(B).
{
    A = B;
}

while_control(A) ::= /* NULL */.
{
    A = 0;
}
while_control(A) ::= TOK_WHILE expression(B).
{
    A = B;
}

%syntax_error {
    yyerrstatus++;
}

%include {
static void
yyerror(string)
char *string;
{
    char buf[200];
    Symbol sym;

    strcpy (buf, string);

    if (yyeof) {
	strcat(buf, " at end of input");
    } else if (yytext[0] == 0) {
	strcat(buf, " at null character");
    } else if (yytext[0] < 040 || yytext[0] >= 0177) {
	sprintf(buf + strlen(buf), " before character 0%o", yytext[0]);
    } else {
	sprintf(buf + strlen(buf), " before `%s'", yytext);
    }

    sym.line = yylineno;
    sym.filename = current_filename;
    ERRORreport_with_symbol(ERROR_syntax, &sym, buf,
	CURRENT_SCOPE_TYPE_PRINTABLE, CURRENT_SCOPE_NAME);
}

static void
yyerror2(t)
char CONST *t;	/* token or 0 to indicate no more tokens */
{
    char buf[200];
    Symbol sym;
    static int first = 1;	/* true if first suggested replacement */
    static char tokens[4000] = "";/* error message, saying */
    /* "expecting <token types>" */

    if (t) {	/* subsequent token? */
	if (first) {
	    first = 0;
	} else {
	    strcat (tokens, " or ");
	}
	strcat(tokens, t);
    } else {
	strcpy(buf, "syntax error");
	if (yyeof) {
	    strcat(buf, " at end of input");
	} else if (yytext[0] == 0) {
	    strcat(buf, " at null character");
	} else if (yytext[0] < 040 || yytext[0] >= 0177) {
	    sprintf(buf + strlen(buf), " near character 0%o", yytext[0]);
	} else {
	    sprintf(buf + strlen(buf), " near `%s'", yytext);
	}

	if (0 == strlen(tokens)) {
	    yyerror("syntax error");
	}
	sym.line = yylineno - (yytext[0] == '\n');
	sym.filename = current_filename;
	ERRORreport_with_symbol(ERROR_syntax_expecting, &sym, buf, tokens,
	    CURRENT_SCOPE_TYPE_PRINTABLE, CURRENT_SCOPE_NAME);
    }
}
}
