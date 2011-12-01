%include {
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include "perplex.h"
#include "token_type.h"

static void
writeHeader(FILE *templateFile, FILE *headerFile)
{
    char c;
    while ((c = fgetc(templateFile)) != EOF) {
	if (c == '%') {
	    if ((c = fgetc(templateFile)) == '%') {
		/* found %%, end of header section */
		return;
	    } else {
		fprintf(headerFile, "%c", '%');
	    }
	}
	fprintf(headerFile, "%c", c);
    }
}
}

%token_type {YYSTYPE}
%extra_argument {appData_t *appData}

/* suppress compiler warning about unused variable */
%destructor file {ParseARG_STORE;}

file ::= definitions_section TOKEN_SEPARATOR rules_section.
file ::= definitions_section TOKEN_SEPARATOR rules_section TOKEN_SEPARATOR code_section.

/* definitions section */

definitions_section ::= definitions.
{
    FILE *templateFile = appData->scanner_template;
    FILE *headerFile = appData->header;
    FILE *outFile = appData->out;
    char c;

    /* write header file from template */
    if (headerFile != NULL) {
	fprintf(headerFile, "#ifndef PERPLEX_H\n");
	fprintf(headerFile, "#define PERPLEX_H\n");

	writeHeader(templateFile, headerFile);
	fprintf(headerFile, "#endif\n");
	fclose(headerFile);
    }

    /* write implementation file from template */
    if (appData->usingConditions) {
	fprintf(outFile, "#define PERPLEX_USING_CONDITIONS\n");
    }

    while ((c = fgetc(templateFile)) != EOF) {
	if (c == '%') {
	    if ((c = fgetc(templateFile)) == '%') {
		/* found %%, ignore */
		continue;
	    } else {
		fprintf(outFile, "%c", '%');
	    }
	}
	fprintf(outFile, "%c", c);
    }
    fprintf(outFile, "\n");
    fclose(templateFile);
}

definitions ::= /* empty */.
definitions ::= definitions TOKEN_DEFINITIONS(text).
{
    fprintf(appData->out, "%s", text.string);
    free(text.string);
}

/* rules section */

rules_section ::= rule_list.
{
    /* close re2c comment and scanner routine */
    fprintf(appData->out, "*/\n    }\n}\n");
}

rule_list ::= rule.
rule_list ::= rule_list rule.
rule_list ::= start_scope scoped_rules end_scope.
rule_list ::= rule_list start_scope scoped_rules end_scope.

scoped_rules ::= scoped_rule.
scoped_rules ::= scoped_rules scoped_rule.

rule ::= conditions pattern action.
{
    /* prevent fall-through to next rule */
    fprintf(appData->out, "\n    IGNORE_TOKEN;\n}\n");
}

scoped_rule ::= scoped_pattern action.
{
    /* prevent fall-through to next rule */
    fprintf(appData->out, "\n    IGNORE_TOKEN;\n}\n");
}

conditions ::= /* null */.
conditions ::= TOKEN_CONDITION(conds).
{
    fprintf(appData->out, "%s", conds.string);
    free(conds.string);
}

start_scope ::= TOKEN_START_CONDITION_SCOPE(conds).
{
    appData->conditions = conds.string;
}

end_scope ::= TOKEN_END_CONDITION_SCOPE.
{
    free(appData->conditions);
    appData->conditions = NULL;
}

pattern ::= TOKEN_PATTERN(text).
{
    fprintf(appData->out, "%s ", text.string);
    free(text.string);
}

scoped_pattern ::= TOKEN_SCOPED_PATTERN(text).
{
    fprintf(appData->out, "%s%s ", appData->conditions, text.string);
    free(text.string);
}

action ::= TOKEN_ACTION(text).
{
    fprintf(appData->out, "%s", text.string);
    free(text.string);
}

/* code section */

code_section ::= code.

code ::= /* empty */.
code ::= code TOKEN_CODE(text).
{
    fprintf(appData->out, "%s", text.string);
    free(text.string);
}
