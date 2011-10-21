/** @file expprint.c
 *
 * Routines for examining express parser state.
 *
 */

#include "express/expbasic.h"
#include "express/dict.h"
#include "express/linklist.h"
#include "express/symbol.h"

#include "express/expprint.h"
#include "expparse.h"

static void
printIndent(int indent)
{
    int i;
    for (i = 0; i < indent; i++) {
	printf("    ");
    }
}

static void
printStart(const char *structName, int indent)
{
    printIndent(indent);
    printf("%s {\n", structName);
}

static void
printEnd(int indent)
{
    printIndent(indent);
    printf("}\n");
}

/* basic types */

static void
printChar(const char *name, char c, int indent)
{
    printIndent(indent);
    printf("%s = %c\n", name, c);
}

static void
printString(const char *name, char *str, int indent)
{
    printIndent(indent);
    printf("%s = %s\n", name, str);
}

printInt(const char *name, int n, int indent)
{
    printIndent(indent);
    printf("%s = %d\n", name, n);
}

static void
printShort(const char *name, short s, int indent)
{
    printIndent(indent);
    printf("%s = %hd\n", name, s);
}

static void
printLong(const char *name, long l, int indent)
{
    printIndent(indent);
    printf("%s = %ld\n", name, l);
}

/* express types */

static void
printSymbol(const char *name, Symbol *s, int indent)
{
    printStart(name, indent++);

    printString("name", s->name, indent);
    printString("filename", s->filename, indent);
    printShort("line", s->line, indent);
    /* printChar("resolved", s->resolved, indent); */
    printChar("resolved", '?', indent);

    printEnd(--indent);
}

static void
printElement(const char *name, Element e, int indent)
{
    printStart(name, indent++);

    printString("key", e->key, indent);
    /* printString("data", e->data, indent); */
    printString("data", "???", indent);

    if (e->next != NULL) {
	printElement("next", e->next, indent);
    }

    printSymbol("symbol", e->symbol, indent);
    printChar("type", e->type, indent);

    printEnd(--indent);
}

static void
printDictionary(const char *name, Dictionary d, int indent)
{
    int i, j, numSegs, numKeys;
    Segment seg;
    char entryString[64] = {0};

    printStart(name, indent++);

    printShort("p", d->p, indent);
    printShort("maxp", d->maxp, indent);
    printLong("KeyCount", d->KeyCount, indent);
    printShort("SegmentCount", d->SegmentCount, indent);
    printShort("MinLoadFactor", d->MinLoadFactor, indent);
    printShort("MaxLoadFactor", d->MaxLoadFactor, indent);

    numSegs = d->SegmentCount;
    numKeys = d->KeyCount;

    for (i = 0; i < numSegs; i++) {
	seg = d->Directory[i];

	for (j = 0; j < SEGMENT_SIZE; j++) {
	    if (seg[j] != NULL) {
		sprintf(entryString, "Directory[%d][%d]", i, j);
		printElement(entryString, seg[j], indent);
	    }
	}
    }

    printEnd(--indent);
}

printOpCode(const char *name, Op_Code o, int indent)
{
    printStart(name, indent++);

    switch (o) {
	case OP_AND: printf("OP_AND"); break;
	case OP_ANDOR: printf("OP_ANDOR"); break;
	case OP_ARRAY_ELEMENT: printf("OP_ARRAY_ELEMENT"); break;
	case OP_CONCAT: printf("OP_CONCAT"); break;
	case OP_DIV: printf("OP_DIV"); break;
	case OP_DOT: printf("OP_DOT"); break;
	case OP_EQUAL: printf("OP_EQUAL"); break;
	case OP_EXP: printf("OP_EXP"); break;
	case OP_GREATER_EQUAL: printf("OP_GREATER_EQUAL"); break;
	case OP_GREATER_THAN: printf("OP_GREATER_THAN"); break;
	case OP_GROUP: printf("OP_GROUP,"); break;
	case OP_IN: printf("OP_IN"); break;
	case OP_INST_EQUAL: printf("OP_INST_EQUAL"); break;
	case OP_INST_NOT_EQUAL: printf("OP_INST_NOT_EQUAL"); break;
	case OP_LESS_EQUAL: printf("OP_LESS_EQUAL"); break;
	case OP_LESS_THAN: printf("OP_LESS_THAN"); break;
	case OP_LIKE: printf("OP_LIKE"); break;
	case OP_MINUS: printf("OP_MINUS"); break;
	case OP_MOD: printf("OP_MOD"); break;
	case OP_NEGATE: printf("OP_NEGATE"); break;
	case OP_NOT: printf("OP_NOT"); break;
	case OP_NOT_EQUAL: printf("OP_NOT_EQUAL"); break;
	case OP_OR: printf("OP_OR"); break;
	case OP_PLUS: printf("OP_PLUS"); break;
	case OP_REAL_DIV: printf("OP_REAL_DIV"); break;
	case OP_SUBCOMPONENT: printf("OP_SUBCOMPONENT"); break;
	case OP_TIMES: printf("OP_TIMES"); break;
	case OP_XOR: printf("OP_XOR"); break;
	case OP_UNKNOWN: printf("OP_UNKNOWN"); break;
	case OP_LAST: printf("OP_LAST"); break;
    }

    printEnd(--indent);
}

static void printExpression(const char *name, Expression e, int indent);

printSubexpression(const char *name, struct Op_Subexpression *e, int indent)
{
    printStart(name, indent++);

    printOpCode("op_code", e->op_code, indent);
    printExpression("op1", e->op1, indent);
    printExpression("op2", e->op1, indent);
    printExpression("op3", e->op1, indent);

    printEnd(--indent);
}

static void
printExpression(const char *name, Expression e, int indent)
{
    printStart(name, indent++);

    printSymbol("symbol", &e->symbol, indent);
    printScope("type", e->type, indent);
    printScope("return_type", e->return_type, indent);

    if (&e->e != NULL) {
	printSubexpression("e", &e->e, indent);
    }

    /* printUnion("expr_union", e->u, indent); */

    printEnd(--indent);
#if 0
    struct Expression_ {
	Symbol symbol;		/* contains things like funcall names */
				/* string names, binary values, */
				/* enumeration names */
	Type type;
	Type return_type;	/* type of value returned by expression */
		/* The difference between 'type' and 'return_type' is */
		/* illustrated by "func(a)".  Here, 'type' is Type_Function */
		/* while 'return_type'  might be Type_Integer (assuming func */
		/* returns an integer). */
	struct Op_Subexpression e;
	union expr_union u;
}

union expr_union {
	int integer;
	float real;
/*	char *string;		find string name in symbol in Expression */
	char *attribute;	/* inverse .... for 'attr' */
	char *binary;
	int logical;
	Boolean boolean;
	struct Query_ *query;
	struct Funcall funcall;

	/* if etype == aggregate, list of expressions */
	/* if etype == funcall, 1st element of list is funcall or entity */
	/*	remaining elements are parameters */
	Linked_List list;	/* aggregates (bags, lists, sets, arrays) */
				/* or lists for oneof expressions */
	Expression expression;	/* derived value in derive attrs, or*/
				/* initializer in local vars, or */
				/* enumeration tags */
				/* or oneof value */
	struct Scope_ *entity;	/* used by subtype exp, group expr */
				/* and self expr, some funcall's and any */
				/* expr that results in an entity */
	Variable variable;	/* attribute reference */
};
#endif
}

static void
printStatement(Statement structp, int indent)
{
}

static void
printProcedure(const char *name, struct Procedure_ *p, int indent)
{
    Expression e;
    Statement s;

    printStart(name, indent);
    indent++;

    printInt("pcount", p->pcount, indent);
    printInt("tag_count", p->tag_count, indent);

    printString("parameters", "", indent);
    LISTdo(p->parameters, e, Expression);
	printExpression("", e, indent + 1);
    LISTod;

    printString("body", "", indent);
    LISTdo(p->body, s, Statement);
	printStatement(s, indent + 1);
    LISTod;

#if 0
struct Procedure_ {
	Linked_List parameters;
	Linked_List body;
	struct FullText text;
	int builtin;	/* builtin if true */
};
#endif

    indent--;
    printEnd(indent);
}

static void
printFunction(struct Function_ *structp)
{
#if 0
struct Function_ {
	int pcount;	/* # of parameters */
	int tag_count;	/* # of different parameter/return value tags */
	Linked_List parameters;
	Linked_List body;
	Type return_type;
	struct FullText text;
	int builtin;	/* builtin if true */
};
#endif
}

static void
printRule(struct Rule_ *structp)
{
#if 0
struct Rule_ {
	Linked_List parameters;
	Linked_List body;
	struct FullText text;
};
#endif
}

static void
printEntity(struct Entity_ *structp)
{
#if 0
struct Entity_ {
	Linked_List	supertype_symbols; /* linked list of original symbols*/
				/* as generated by parser */
	Linked_List	supertypes;	/* linked list of supertypes (as entities) */
	Linked_List	subtypes;	/* simple list of subtypes */
			/* useful for simple lookups */
	Expression	subtype_expression;	/* DAG of subtypes, with complete */
			/* information including, OR, AND, and ONEOF */
	Linked_List	attributes;	/* explicit attributes */
	int		inheritance;	/* total number of attributes */
					/* inherited from supertypes */
	int		attribute_count;
	Linked_List	unique;	/* list of identifiers that are unique */
	Linked_List	instances;	/* hook for applications */
	int		mark;	/* usual hack - prevent traversing sub/super */
				/* graph twice */
	Boolean		abstract;/* is this an abstract supertype? */
	Type		type;	/* type pointing back to ourself */
				/* Useful to have when evaluating */
				/* expressions involving entities */
};
#endif
}

static void
printSchema(struct Schema_ *structp)
{
#if 0
struct Schema_ {
	Linked_List rules;
	Linked_List reflist;
	Linked_List uselist;
	/* dictionarys into which are entered renames for each specific */
	/* object specified in a rename clause (even if it uses the same */
	/* name */
	Dictionary refdict;
	Dictionary usedict;
	/* lists of schemas that are fully ref/use'd */
	/* entries can be 0 if schemas weren't found during RENAMEresolve */
	Linked_List use_schemas;
	Linked_List ref_schemas;
};
#endif
}

static void
printIncrement(struct Increment_ *structp)
{
#if 0
/* this is an element in the optional Loop scope */
struct Increment_ {
    Expression init;
    Expression end;
    Expression increment;
};
#endif
}

static void
printExpress(const char *name, struct Express_ *e, int indent)
{
    printStart(name, indent);
    indent++;

    printInt("file", e->file, indent);
    printString("filename", e->filename, indent);
    printString("basename", e->basename, indent);

    indent--;
    printEnd(indent);
}

static void
printTypeHead(TypeHead t, int indent)
{
}

void
printScope(struct Scope_ *s, int indent)
{
    printStart("Scope", indent);
    indent++;

    printSymbol("symbol", &s->symbol, indent);
    printChar("type", s->type, indent);
    printInt("search_id", s->search_id, indent);
    printDictionary("symbol_table", s->symbol_table, indent);

    if (s->superscope != NULL) {
	printString("superscope", "", indent);
	expprintExpress(s->superscope);
    }

    switch(s->type) {
	case OBJ_PROCEDURE:
	    printProcedure("u.proc", s->u.proc, indent);
	    break;
	case OBJ_FUNCTION:
	    printString("u.func", "", indent);
	    printFunction(s->u.func);
	    break;
	case OBJ_RULE:
	    printString("u.rule", "", indent);
	    printRule(s->u.rule);
	    break;
	case OBJ_ENTITY:
	    printString("u.entity", "", indent);
	    printEntity(s->u.entity);
	    break;
	case OBJ_SCHEMA:
	    printString("u.schema", "", indent);
	    printSchema(s->u.schema);
	    break;
	case OBJ_EXPRESS:
	    printExpress("u.express", s->u.express, indent);
	    break;
	case OBJ_INCREMENT:
	    printString("u.incr", "", indent);
	    printIncrement(s->u.incr);
	    break;
	case OBJ_TYPE:
	    printString("u.type", "", indent);
	    printTypeHead(s->u.type, indent + 1);
    }

    indent--;
    printEnd(indent);
} /* printExpress */

void
expprintExpress(Express s)
{
    printScope(s, 0);
}


#if 0
/* struct definitions temporarily included for reference purposes */

struct Scope_ {
    Symbol symbol;
    char type;		   /* see above */
    ClientData clientData; /* user may use this for any purpose */
    int search_id;	   /* key to avoid searching this scope twice */
    Dictionary symbol_table;
    struct Scope_ *superscope;
    union {
	struct Procedure_ *proc;
	struct Function_ *func;
	struct Rule_ *rule;
	struct Entity_ *entity;
	struct Schema_ *schema;
	struct Express_ *express;
	struct Increment_ *incr;
	struct TypeHead_ *type;
    } u;
    Linked_List where; /* optional where clause */
};

typedef void *ClientData;

typedef struct Hash_Table_ {
	short	p;		/* Next bucket to be split	*/
	short	maxp;		/* upper bound on p during expansion	*/
	long	KeyCount;	/* current # keys	*/
	short	SegmentCount;	/* current # segments	*/
	short	MinLoadFactor;
	short	MaxLoadFactor;
	Segment	Directory[DIRECTORY_SIZE];
} *Hash_Table;
typedef struct Hash_Table_	*Dictionary;


typedef struct Linked_List_ *Linked_List;

typedef struct Link_ {
    struct Link_ *	next;
    struct Link_ *	prev;
    Generic		data;
} *Link;

struct Linked_List_ {
    Link mark;
};
#endif

void
expprintToken(int tokenID)
{
    switch (tokenID) {
	case TOK_EQUAL: puts("TOK_EQUAL"); break;
	case TOK_GREATER_EQUAL: puts("TOK_GREATER_EQUAL"); break;
	case TOK_GREATER_THAN: puts("TOK_GREATER_THAN"); break;
	case TOK_IN: puts("TOK_IN"); break;
	case TOK_INST_EQUAL: puts("TOK_INST_EQUAL"); break;
	case TOK_INST_NOT_EQUAL: puts("TOK_INST_NOT_EQUAL"); break;
	case TOK_LESS_EQUAL: puts("TOK_LESS_EQUAL"); break;
	case TOK_LESS_THAN: puts("TOK_LESS_THAN"); break;
	case TOK_LIKE: puts("TOK_LIKE"); break;
	case TOK_NOT_EQUAL: puts("TOK_NOT_EQUAL"); break;
	case TOK_MINUS: puts("TOK_MINUS"); break;
	case TOK_PLUS: puts("TOK_PLUS"); break;
	case TOK_OR: puts("TOK_OR"); break;
	case TOK_XOR: puts("TOK_XOR"); break;
	case TOK_DIV: puts("TOK_DIV"); break;
	case TOK_MOD: puts("TOK_MOD"); break;
	case TOK_REAL_DIV: puts("TOK_REAL_DIV"); break;
	case TOK_TIMES: puts("TOK_TIMES"); break;
	case TOK_AND: puts("TOK_AND"); break;
	case TOK_ANDOR: puts("TOK_ANDOR"); break;
	case TOK_CONCAT_OP: puts("TOK_CONCAT_OP"); break;
	case TOK_EXP: puts("TOK_EXP"); break;
	case TOK_NOT: puts("TOK_NOT"); break;
	case TOK_DOT: puts("TOK_DOT"); break;
	case TOK_BACKSLASH: puts("TOK_BACKSLASH"); break;
	case TOK_LEFT_BRACKET: puts("TOK_LEFT_BRACKET"); break;
	case TOK_LEFT_PAREN: puts("TOK_LEFT_PAREN"); break;
	case TOK_RIGHT_PAREN: puts("TOK_RIGHT_PAREN"); break;
	case TOK_RIGHT_BRACKET: puts("TOK_RIGHT_BRACKET"); break;
	case TOK_COLON: puts("TOK_COLON"); break;
	case TOK_COMMA: puts("TOK_COMMA"); break;
	case TOK_AGGREGATE: puts("TOK_AGGREGATE"); break;
	case TOK_OF: puts("TOK_OF"); break;
	case TOK_IDENTIFIER: puts("TOK_IDENTIFIER"); break;
	case TOK_ALIAS: puts("TOK_ALIAS"); break;
	case TOK_FOR: puts("TOK_FOR"); break;
	case TOK_END_ALIAS: puts("TOK_END_ALIAS"); break;
	case TOK_ARRAY: puts("TOK_ARRAY"); break;
	case TOK_ASSIGNMENT: puts("TOK_ASSIGNMENT"); break;
	case TOK_BAG: puts("TOK_BAG"); break;
	case TOK_BOOLEAN: puts("TOK_BOOLEAN"); break;
	case TOK_INTEGER: puts("TOK_INTEGER"); break;
	case TOK_REAL: puts("TOK_REAL"); break;
	case TOK_NUMBER: puts("TOK_NUMBER"); break;
	case TOK_LOGICAL: puts("TOK_LOGICAL"); break;
	case TOK_BINARY: puts("TOK_BINARY"); break;
	case TOK_STRING: puts("TOK_STRING"); break;
	case TOK_BY: puts("TOK_BY"); break;
	case TOK_LEFT_CURL: puts("TOK_LEFT_CURL"); break;
	case TOK_RIGHT_CURL: puts("TOK_RIGHT_CURL"); break;
	case TOK_OTHERWISE: puts("TOK_OTHERWISE"); break;
	case TOK_CASE: puts("TOK_CASE"); break;
	case TOK_END_CASE: puts("TOK_END_CASE"); break;
	case TOK_BEGIN: puts("TOK_BEGIN"); break;
	case TOK_END: puts("TOK_END"); break;
	case TOK_PI: puts("TOK_PI"); break;
	case TOK_E: puts("TOK_E"); break;
	case TOK_CONSTANT: puts("TOK_CONSTANT"); break;
	case TOK_END_CONSTANT: puts("TOK_END_CONSTANT"); break;
	case TOK_DERIVE: puts("TOK_DERIVE"); break;
	case TOK_END_ENTITY: puts("TOK_END_ENTITY"); break;
	case TOK_ENTITY: puts("TOK_ENTITY"); break;
	case TOK_ENUMERATION: puts("TOK_ENUMERATION"); break;
	case TOK_ESCAPE: puts("TOK_ESCAPE"); break;
	case TOK_SELF: puts("TOK_SELF"); break;
	case TOK_OPTIONAL: puts("TOK_OPTIONAL"); break;
	case TOK_VAR: puts("TOK_VAR"); break;
	case TOK_END_FUNCTION: puts("TOK_END_FUNCTION"); break;
	case TOK_FUNCTION: puts("TOK_FUNCTION"); break;
	case TOK_BUILTIN_FUNCTION: puts("TOK_BUILTIN_FUNCTION"); break;
	case TOK_LIST: puts("TOK_LIST"); break;
	case TOK_SET: puts("TOK_SET"); break;
	case TOK_GENERIC: puts("TOK_GENERIC"); break;
	case TOK_QUESTION_MARK: puts("TOK_QUESTION_MARK"); break;
	case TOK_IF: puts("TOK_IF"); break;
	case TOK_THEN: puts("TOK_THEN"); break;
	case TOK_END_IF: puts("TOK_END_IF"); break;
	case TOK_ELSE: puts("TOK_ELSE"); break;
	case TOK_INCLUDE: puts("TOK_INCLUDE"); break;
	case TOK_STRING_LITERAL: puts("TOK_STRING_LITERAL"); break;
	case TOK_TO: puts("TOK_TO"); break;
	case TOK_AS: puts("TOK_AS"); break;
	case TOK_REFERENCE: puts("TOK_REFERENCE"); break;
	case TOK_FROM: puts("TOK_FROM"); break;
	case TOK_USE: puts("TOK_USE"); break;
	case TOK_INVERSE: puts("TOK_INVERSE"); break;
	case TOK_INTEGER_LITERAL: puts("TOK_INTEGER_LITERAL"); break;
	case TOK_REAL_LITERAL: puts("TOK_REAL_LITERAL"); break;
	case TOK_STRING_LITERAL_ENCODED: puts("TOK_STRING_LITERAL_ENCODED"); break;
	case TOK_LOGICAL_LITERAL: puts("TOK_LOGICAL_LITERAL"); break;
	case TOK_BINARY_LITERAL: puts("TOK_BINARY_LITERAL"); break;
	case TOK_LOCAL: puts("TOK_LOCAL"); break;
	case TOK_END_LOCAL: puts("TOK_END_LOCAL"); break;
	case TOK_ONEOF: puts("TOK_ONEOF"); break;
	case TOK_UNIQUE: puts("TOK_UNIQUE"); break;
	case TOK_FIXED: puts("TOK_FIXED"); break;
	case TOK_END_PROCEDURE: puts("TOK_END_PROCEDURE"); break;
	case TOK_PROCEDURE: puts("TOK_PROCEDURE"); break;
	case TOK_BUILTIN_PROCEDURE: puts("TOK_BUILTIN_PROCEDURE"); break;
	case TOK_QUERY: puts("TOK_QUERY"); break;
	case TOK_ALL_IN: puts("TOK_ALL_IN"); break;
	case TOK_SUCH_THAT: puts("TOK_SUCH_THAT"); break;
	case TOK_REPEAT: puts("TOK_REPEAT"); break;
	case TOK_END_REPEAT: puts("TOK_END_REPEAT"); break;
	case TOK_RETURN: puts("TOK_RETURN"); break;
	case TOK_END_RULE: puts("TOK_END_RULE"); break;
	case TOK_RULE: puts("TOK_RULE"); break;
	case TOK_END_SCHEMA: puts("TOK_END_SCHEMA"); break;
	case TOK_SCHEMA: puts("TOK_SCHEMA"); break;
	case TOK_SELECT: puts("TOK_SELECT"); break;
	case TOK_SEMICOLON: puts("TOK_SEMICOLON"); break;
	case TOK_SKIP: puts("TOK_SKIP"); break;
	case TOK_SUBTYPE: puts("TOK_SUBTYPE"); break;
	case TOK_ABSTRACT: puts("TOK_ABSTRACT"); break;
	case TOK_SUPERTYPE: puts("TOK_SUPERTYPE"); break;
	case TOK_END_TYPE: puts("TOK_END_TYPE"); break;
	case TOK_TYPE: puts("TOK_TYPE"); break;
	case TOK_UNTIL: puts("TOK_UNTIL"); break;
	case TOK_WHERE: puts("TOK_WHERE"); break;
	case TOK_WHILE: puts("TOK_WHILE"); break;
	default: printf("UNRECOGNIZED (%d)\n", tokenID);
    }
}
