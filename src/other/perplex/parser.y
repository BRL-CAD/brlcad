%include {
#include <assert.h>
#include "token_type.h"
}

%token_type {YYSTYPE}

file ::= definitions TOKEN_DEFINITIONS rules TOKEN_RULES code.

definitions ::= /* empty */.
definitions ::= definitions TOKEN_ANY(s).
{
    printf("%s", s.string);
}

rules ::= /* empty */.
rules ::= rules rule.

rule ::= TOKEN_PATTERN(pattern) TOKEN_ACTION(action).
{
    printf("%s", pattern.string);
    printf("\n{\n");
    printf("%s", action.string);
    printf("\nCONTINUE;\n");
    printf("\n}\n");
}

code ::= /* empty */.
code ::= code TOKEN_ANY(s).
{
    printf("%s", s.string);
}
