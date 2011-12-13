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

static void
writeRuleClose(appData_t *appData)
{
    /* prevent fall-through to next rule */
    fprintf(appData->out, "\n    IGNORE_TOKEN;\n}\n");
}

static void
writeString(appData_t *appData, char *string)
{
    fprintf(appData->out, "%s", string);
    free(string);
}

static void
writeDefinitions(appData_t *appData)
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

    fprintf(outFile, "\n");
    while ((c = fgetc(templateFile)) != EOF) {
	if (c == '%') {
	    if ((c = fgetc(templateFile)) == '%') {
		/* found %%, ignore */
		continue;
	    } else {
		fprintf(outFile, "%%");
	    }
	}
	fprintf(outFile, "%c", c);
    }
    if (appData->usingConditions) {
	fprintf(outFile, "re2c:condenumprefix = \"\";\n");
	fprintf(outFile, "re2c:cond:goto = \"continue;\";\n");
	fprintf(outFile, "re2c:define:YYCONDTYPE = int;\n");
	fprintf(outFile, "re2c:define:YYGETCONDITION:naked = 1;\n");
    }
    fprintf(outFile, "\n");
    fclose(templateFile);
}
}

%token_type {YYSTYPE}
%extra_argument {appData_t *appData}

%type string {char*}
%type word {char*}

/* later tokens have greater precedence */
%nonassoc EMPTY_RULE_LIST.
%nonassoc TOKEN_WORD.
%nonassoc TOKEN_CODE_END.
%nonassoc TOKEN_CODE_START.
%right TOKEN_EMPTY_COND.
%right TOKEN_CONDITION.

/* suppress compiler warning about unused variable */
%destructor file {ParseARG_STORE;}

/* three section perplex input file */
file ::= def_section rule_section code_section.

/* DEFINITIONS SECTION */
def_section ::= string TOKEN_SEPARATOR.
{
    writeDefinitions(appData);
    fprintf(appData->out, "\n/* start rules */\n");
}

/* RULES SECTION */
rule_section ::= named_defs rule_list.

named_defs ::= /* empty */.
named_defs ::= named_defs word(NAME) TOKEN_DEFINITION(DEF).
{
     writeString(appData, NAME);
     writeString(appData, DEF.string);
}

rule_list ::= /* empty */. [EMPTY_RULE_LIST]
rule_list ::= rule_list rules. [TOKEN_CODE_END]
rule_list ::= rule_list start_scope rules end_scope.

rules ::= rule.
rules ::= rules rule.

start_scope ::= TOKEN_START_SCOPE(C).
{
    appData->conditions = C.string;
}

end_scope ::= TOKEN_END_SCOPE.
{
    free(appData->conditions);
    appData->conditions = NULL;
}

rule ::= opt_condition pattern opt_cond_change opt_code.
rule ::= empty_condition opt_cond_change opt_code.

opt_condition ::= /* empty */. [TOKEN_WORD]
{
    if (appData->conditions) {
	fprintf(appData->out, appData->conditions);
    }
}
opt_condition ::= TOKEN_CONDITION(C). [TOKEN_WORD]
{
    writeString(appData, C.string);
}

empty_condition ::= TOKEN_EMPTY_COND.
{
    fprintf(appData->out, "<>");
}

pattern ::= word(W). {
    writeString(appData, W);
}

opt_cond_change ::= /* empty */.
opt_cond_change ::= TOKEN_COND_CHANGE(C) word(W).
{
    writeString(appData, C.string);
    writeString(appData, W);
}

opt_code ::= /* empty */. [TOKEN_CODE_END]
{
    fprintf(appData->out, " { IGNORE_TOKEN; }\n");
}
opt_code ::= code_start string TOKEN_CODE_END.
{
    fprintf(appData->out, "IGNORE_TOKEN;\n}\n");
}

code_start ::= TOKEN_CODE_START.
{
    fprintf(appData->out, "{\n");
}

/* CODE SECTION */
code_section ::= /* empty */.
{
    /* close re2c block, while block, and scanner routine */
    fprintf(appData->out, "\n*/\n    }\n}\n");
}

code_section ::= code_section_start string.

code_section_start ::= TOKEN_SEPARATOR.
{
    /* close re2c block, while block, and scanner routine */
    fprintf(appData->out, "\n*/\n    }\n}\n");

    fprintf(appData->out, "\n/* start code */\n");
}

/* strings */
string ::= /* empty */.
string ::= string word(W).
{
    writeString(appData, W);
}

/* A word is a character sequence that ends in a whitespace character,
 * which does not contain any whitespace chracters except inside quotes.
 * Example words include:
 * abc
 * "abc def"'\n'?
 * abc[ \t\n]
 */
word(A) ::= TOKEN_WORD(B). { A = B.string; }
