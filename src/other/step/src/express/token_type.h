#ifndef TOKEN_TYPE_H
#define TOKEN_TYPE_H
#include "express/symbol.h"
#include "express/linklist.h"
#include "stack.h"
#include "express/express.h"
#include "express/schema.h"
#include "express/entity.h"
#include "express/resolve.h"

/* structured types for parse node decorations */
struct type_either {
    Type	 type;
    TypeBody body;
};

struct type_flags {
    unsigned optional: 1;
    unsigned unique: 1;
    unsigned fixed: 1;
    unsigned var: 1;		/* when formal is "VAR" */
};

struct entity_body {
    Linked_List	attributes;
    Linked_List	unique;
    Linked_List	where;
};

struct subsuper_decl {
    Expression	subtypes;
    Linked_List	supertypes;
    Boolean		abstract;
};

struct subtypes {
    Expression	subtypes;
    Boolean		abstract;
};

struct upper_lower {
    Expression	lower_limit;
    Expression	upper_limit;
};

struct qualifier {
    Expression	expr;	/* overall expression for qualifier */
    Expression	first;	/* first [syntactic] qualifier (if more */
			    /* than one is contained in this */
			    /* [semantic] qualifier - used for */
			    /* group_ref + attr_ref - see rule for */
			    /* qualifier) */
};

typedef union YYSTYPE {
    /* simple (single-valued) node types */

    Binary		binary;
    Case_Item		case_item;
    Expression		expression;
    Integer		iVal;
    Linked_List		list;
    Logical		logical;
    Op_Code		op_code;
    Qualified_Attr 	*qualified_attr;
    Real		rVal;
    Statement		statement;
    Symbol 		*symbol;
    char 		*string;
    Type		type;
    TypeBody		typebody;
    Variable		variable;
    Where		where;

    struct type_either   type_either;
    struct type_flags    type_flags;
    struct entity_body   entity_body;
    struct subsuper_decl subsuper_decl;
    struct subtypes      subtypes;
    struct upper_lower   upper_lower;
    struct qualifier     qualifier;
} YYSTYPE;
#endif
