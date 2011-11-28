%include {
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include "perplex.h"
#include "token_type.h"
}

%token_type {YYSTYPE}
%extra_argument {appData_t *appData}

file ::= definitions_section rules_section code.

/* definitions section */

definitions_section ::= definitions TOKEN_SEPARATOR.
{
    FILE *templateFile = appData->template;
    char c;

    while ((c = fgetc(templateFile)) != EOF) {
	printf("%c", c);
    }
    printf("\n");
    fclose(templateFile);
}

definitions ::= /* empty */.
definitions ::= definitions TOKEN_DEFINITIONS(text).
{
    printf("%s", text.string);
    free(text.string);
}

/* rules section */

rules_section ::= rules TOKEN_SEPARATOR.
{
    /* close re2c comment and scanner routine */
    printf("*/\n    }\n}\n");
}

rules ::= rule.
rules ::= rules rule.

rule ::= pattern TOKEN_LBRACE action TOKEN_RBRACE.
{
    /* prevent fall-through to next rule */
    printf("\n    CONTINUE;\n}\n");
}

pattern ::= TOKEN_PATTERN(text).
{
    printf("%s {", text.string);
    free(text.string);
}

action ::= TOKEN_ACTION(text).
{
    printf("%s", text.string);
    free(text.string);
}

/* code section */

code ::= /* empty */.
code ::= code TOKEN_CODE(text).
{
    printf("%s", text.string);
    free(text.string);
}
