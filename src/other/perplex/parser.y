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
    }

    /* write implementation file from template */
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

rules_section ::= rules.
{
    /* close re2c comment and scanner routine */
    fprintf(appData->out, "*/\n    }\n}\n");
}

rules ::= rule.
rules ::= rules rule.

rule ::= pattern action.
{
    /* prevent fall-through to next rule */
    fprintf(appData->out, "\n    CONTINUE;\n}\n");
}

pattern ::= TOKEN_PATTERN(text).
{
    fprintf(appData->out, "%s ", text.string);
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
