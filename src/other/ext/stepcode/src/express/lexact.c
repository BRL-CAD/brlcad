

/*
 * This software was developed by U.S. Government employees as part of
 * their official duties and is not subject to copyright.
 *
 * $Log: lexact.c,v $
 * Revision 1.10  1997/09/24 20:05:38  dar
 * scanner went into infinite loop with unmatched single quote at eol
 *
 * Revision 1.10  1997/09/24 15:56:35  libes
 * scanner went into infinite loop with unmatched single quote at eol
 *
 * Revision 1.9  1997/01/21 20:07:11  dar
 * made C++ compatible
 *
 * Revision 1.8  1997/01/21  19:51:14  libes
 * add POSIX portability
 *
 * Revision 1.7  1995/04/05  13:55:40  clark
 * CADDETC preval
 *
 * Revision 1.6  1994/11/29  20:55:34  clark
 * fix inline comment bug
 *
 * Revision 1.5  1994/11/22  18:32:39  clark
 * Part 11 IS; group reference
 *
 * Revision 1.4  1994/11/10  19:20:03  clark
 * Update to IS
 *
 * Revision 1.3  1994/05/11  19:51:24  libes
 * numerous fixes
 *
 * Revision 1.2  1993/10/15  18:48:48  libes
 * CADDETC certified
 *
 * Revision 1.7  1993/02/22  21:48:00  libes
 * added decl for strdup
 *
 * Revision 1.6  1993/01/19  22:44:17  libes
 * *** empty log message ***
 *
 * Revision 1.5  1992/08/27  23:40:58  libes
 * remove connection between scanner and rest of EXPRESSinitializations
 *
 * Revision 1.4  1992/08/18  17:13:43  libes
 * rm'd extraneous error messages
 *
 * Revision 1.3  1992/06/08  18:06:57  libes
 * prettied up interface to print_objects_when_running
 */

#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include "sc_memmgr.h"
#include "express/lexact.h"
#include "express/linklist.h"
#include "stack.h"
#include "express/hash.h"
#include "express/express.h"
#include "express/dict.h"
#include "express/alloc.h"
#include "token_type.h"
#include "expparse.h"
#include "expscan.h"

extern YYSTYPE yylval;

Scan_Buffer SCAN_buffers[SCAN_NESTING_DEPTH];
int     SCAN_current_buffer = 0;
char       *SCANcurrent;

extern int      yylineno;

#define SCAN_COMMENT_LENGTH 256
static char     last_comment_[SCAN_COMMENT_LENGTH + 1] = "";
static char     *last_comment = 0;

/* keyword lookup table */

static Hash_Table   keyword_dictionary;

static struct keyword_entry {
    char *key;
    int  token;
} keywords[] = {
    { "ABS",        TOK_BUILTIN_FUNCTION },
    { "ABSTRACT",       TOK_ABSTRACT },
    { "ACOS",       TOK_BUILTIN_FUNCTION },
    { "AGGREGATE",      TOK_AGGREGATE },
    { "ALIAS",      TOK_ALIAS },
    { "AND",        TOK_AND },
    { "ANDOR",      TOK_ANDOR },
    { "ARRAY",      TOK_ARRAY },
    { "AS",         TOK_AS },
    { "ASIN",       TOK_BUILTIN_FUNCTION },
    { "ATAN",       TOK_BUILTIN_FUNCTION },
    { "BAG",        TOK_BAG },
    { "BEGIN",      TOK_BEGIN },
    { "BINARY",     TOK_BINARY },
    { "BLENGTH",        TOK_BUILTIN_FUNCTION },
    { "BOOLEAN",        TOK_BOOLEAN },
    { "BY",         TOK_BY },
    { "CASE",       TOK_CASE },
    { "CONST_E",        TOK_E },
    { "CONSTANT",       TOK_CONSTANT },
    { "COS",        TOK_BUILTIN_FUNCTION },
    { "DERIVE",     TOK_DERIVE },
    { "DIV",        TOK_DIV },
    { "ELSE",       TOK_ELSE },
    { "END",        TOK_END },
    { "END_ALIAS",      TOK_END_ALIAS },
    { "END_CASE",       TOK_END_CASE },
    { "END_CONSTANT",   TOK_END_CONSTANT },
    { "END_ENTITY",     TOK_END_ENTITY },
    { "END_FUNCTION",   TOK_END_FUNCTION },
    { "END_IF",     TOK_END_IF },
    { "END_LOCAL",      TOK_END_LOCAL },
    { "END_PROCEDURE",  TOK_END_PROCEDURE },
    { "END_REPEAT",     TOK_END_REPEAT },
    { "END_RULE",       TOK_END_RULE },
    { "END_SCHEMA",     TOK_END_SCHEMA },
    { "END_TYPE",       TOK_END_TYPE },
    { "ENTITY",     TOK_ENTITY },
    { "ENUMERATION",    TOK_ENUMERATION },
    { "ESCAPE",     TOK_ESCAPE },
    { "EXISTS",     TOK_BUILTIN_FUNCTION },
    { "EXP",        TOK_BUILTIN_FUNCTION },
    { "FALSE",      TOK_LOGICAL_LITERAL },
    { "FIXED",      TOK_FIXED },
    { "FOR",        TOK_FOR },
    { "FORMAT",     TOK_BUILTIN_FUNCTION },
    { "FROM",       TOK_FROM },
    { "FUNCTION",       TOK_FUNCTION },
    { "GENERIC",        TOK_GENERIC },
    { "HIBOUND",        TOK_BUILTIN_FUNCTION },
    { "HIINDEX",        TOK_BUILTIN_FUNCTION },
    { "IF",         TOK_IF },
    { "IN",         TOK_IN },
    { "INCLUDE",        TOK_INCLUDE },
    { "INSERT",     TOK_BUILTIN_PROCEDURE },
    { "INTEGER",        TOK_INTEGER },
    { "INVERSE",        TOK_INVERSE },
    { "LENGTH",     TOK_BUILTIN_FUNCTION },
    { "LIKE",       TOK_LIKE },
    { "LIST",       TOK_LIST },
    { "LOBOUND",        TOK_BUILTIN_FUNCTION },
    { "LOCAL",      TOK_LOCAL },
    { "LOG",        TOK_BUILTIN_FUNCTION },
    { "LOG10",      TOK_BUILTIN_FUNCTION },
    { "LOG2",       TOK_BUILTIN_FUNCTION },
    { "LOGICAL",        TOK_LOGICAL },
    { "LOINDEX",        TOK_BUILTIN_FUNCTION },
    { "MOD",        TOK_MOD },
    { "NOT",        TOK_NOT },
    { "NUMBER",     TOK_NUMBER },
    { "NVL",        TOK_BUILTIN_FUNCTION },
    { "ODD",        TOK_BUILTIN_FUNCTION },
    { "OF",         TOK_OF },
    { "ONEOF",      TOK_ONEOF },
    { "OPTIONAL",       TOK_OPTIONAL },
    { "OR",         TOK_OR },
    { "OTHERWISE",      TOK_OTHERWISE },
    { "PI",         TOK_PI },
    { "PROCEDURE",      TOK_PROCEDURE },
    { "QUERY",      TOK_QUERY },
    { "REAL",       TOK_REAL },
    { "REFERENCE",      TOK_REFERENCE },
    { "REMOVE",     TOK_BUILTIN_PROCEDURE },
    { "REPEAT",     TOK_REPEAT },
    { "RETURN",     TOK_RETURN },
    { "ROLESOF",        TOK_BUILTIN_FUNCTION },
    { "RULE",       TOK_RULE },
    { "SCHEMA",     TOK_SCHEMA },
    { "SELECT",     TOK_SELECT },
    { "SELF",       TOK_SELF },
    { "SET",        TOK_SET },
    { "SIN",        TOK_BUILTIN_FUNCTION },
    { "SIZEOF",     TOK_BUILTIN_FUNCTION },
    { "SKIP",       TOK_SKIP },
    { "SQRT",       TOK_BUILTIN_FUNCTION },
    { "STRING",     TOK_STRING },
    { "SUBTYPE",        TOK_SUBTYPE },
    { "SUPERTYPE",      TOK_SUPERTYPE },
    { "TAN",        TOK_BUILTIN_FUNCTION },
    { "THEN",       TOK_THEN },
    { "TO",         TOK_TO },
    { "TRUE",       TOK_LOGICAL_LITERAL },
    { "TYPE",       TOK_TYPE },
    { "TYPEOF",     TOK_BUILTIN_FUNCTION },
    { "UNIQUE",     TOK_UNIQUE },
    { "UNKNOWN",        TOK_LOGICAL_LITERAL },
    { "UNTIL",      TOK_UNTIL },
    { "USE",        TOK_USE },
    { "USEDIN",     TOK_BUILTIN_FUNCTION },
    { "VALUE",      TOK_BUILTIN_FUNCTION },
    { "VALUE_IN",       TOK_BUILTIN_FUNCTION },
    { "VALUE_UNIQUE",   TOK_BUILTIN_FUNCTION },
    { "VAR",        TOK_VAR },
    { "WHERE",      TOK_WHERE },
    { "WHILE",      TOK_WHILE },
    { "XOR",        TOK_XOR },
    { 0,            0}
};

static void SCANpush_buffer(char *filename, FILE *fp)
{
    SCANbuffer.savedPos = SCANcurrent;
    SCANbuffer.lineno = yylineno;
    yylineno = 1;
    ++SCAN_current_buffer;
#ifdef keep_nul
    SCANbuffer.numRead = 0;
#else
    *(SCANcurrent = SCANbuffer.text) = '\0';
#endif
    SCANbuffer.readEof = false;
    SCANbuffer.file = fp;
    SCANbuffer.filename = current_filename = filename;
}

static void SCANpop_buffer()
{
    if(SCANbuffer.file != NULL) {
        fclose(SCANbuffer.file);
    }
    --SCAN_current_buffer;
    SCANcurrent = SCANbuffer.savedPos;
    yylineno = SCANbuffer.lineno + 1;   /* DEL */
    current_filename = SCANbuffer.filename;
}

void SCANinitialize(void)
{
    struct keyword_entry *k;

    keyword_dictionary = HASHcreate(100);   /* not exact */
    for(k = keywords; k->key; k++) {
        DICTdefine(keyword_dictionary, k->key, k, NULL, OBJ_UNKNOWN);
        /* not "unknown", but certainly won't be looked up by type! */
    }
}

/** Clean up the Scan module */
void SCANcleanup(void)
{
}

int SCANprocess_real_literal(const char *yytext)
{
    sscanf(yytext, "%lf", &(yylval.rVal));
    return TOK_REAL_LITERAL;
}

int SCANprocess_integer_literal(const char *yytext)
{
    sscanf(yytext, "%d", &(yylval.iVal));
    return TOK_INTEGER_LITERAL;
}

int SCANprocess_binary_literal(const char *yytext)
{
    yylval.binary = SCANstrdup(yytext + 1);   /* drop '%' prefix */
    return TOK_BINARY_LITERAL;
}

int SCANprocess_logical_literal(char *string)
{
    switch(string[0]) {
        case 'T':
            yylval.logical = Ltrue;
            break;
        case 'F':
            yylval.logical = Lfalse;
            break;
        default:
            yylval.logical = Lunknown;
            break;
            /* default will actually be triggered by 'UNKNOWN' keyword */
    }
    sc_free(string);
    return TOK_LOGICAL_LITERAL;
}

int SCANprocess_identifier_or_keyword(const char *yytext)
{
    char *test_string, * dest;
    const char *src;
    struct keyword_entry *k;
    int len;

    /* make uppercase copy */
    len = strlen(yytext);
    dest = test_string = (char *)sc_malloc(len + 1);
    for(src = yytext; *src; src++, dest++) {
        *dest = (islower(*src) ? toupper(*src) : *src);
    }
    *dest = '\0';

    /* check for language keywords */
    k = (struct keyword_entry *)DICTlookup(keyword_dictionary, test_string);
    if(k) {
        switch(k->token) {
            case TOK_BUILTIN_FUNCTION:
            case TOK_BUILTIN_PROCEDURE:
                break;
            case TOK_LOGICAL_LITERAL:
                return SCANprocess_logical_literal(test_string);
            default:
                sc_free(test_string);
                return k->token;
        }
    }
    /* now we have an identifier token */
    yylval.symbol = SYMBOLcreate(test_string, yylineno, current_filename);
    if(k) {
        /* built-in function/procedure */
        return(k->token);
    } else {
        /* plain identifier */
        /* translate back to lower-case */
        SCANlowerize(test_string);
        return TOK_IDENTIFIER;
    }
}

int SCANprocess_string(const char *yytext)
{
    char *s, *d;    /* source, destination */

    /* strip off quotes */
    yylval.string = SCANstrdup(yytext + 1);   /* remove 1st single quote */

    /* change pairs of quotes to single quotes */
    for(s = d = yylval.string; *s;) {
        if(*s != '\'') {
            *d++ = *s++;
        } else if(0 == strncmp(s, "''", 2)) {
            *d++ = '\'';
            s += 2;
        } else if(*s == '\'') {
            /* trailing quote */
            *s = '\0';
            /* if string was unterminated, there will be no */
            /* quote to remove in which case the scanner has */
            /* already complained about it */
        }
    }
    *d = '\0';

    return TOK_STRING_LITERAL;
}

int SCANprocess_encoded_string(const char *yytext)
{
    char *s;    /* source */
    int count;

    /* strip off quotes */
    yylval.string = SCANstrdup(yytext + 1);   /* remove 1st double quote */

    s = strrchr(yylval.string, '"');
    if(s) {
        *s = '\0';    /* remove last double quote */
    }
    /* if string was unterminated, there will be no quote to remove */
    /* in which case the scanner has already complained about it */

    count = 0;
    for(s = yylval.string; *s; s++, count++) {
        if(!isxdigit(*s)) {
            ERRORreport_with_line(ENCODED_STRING_BAD_DIGIT, yylineno, *s);
        }
    }

    if(0 != (count % 8)) {
        ERRORreport_with_line(ENCODED_STRING_BAD_COUNT, yylineno, count);
    }

    return TOK_STRING_LITERAL_ENCODED;
}

int SCANprocess_semicolon(const char *yytext, int commentp)
{

    if(commentp && yytext) {
        strncpy(last_comment_, strchr(yytext, '-'), SCAN_COMMENT_LENGTH);
        yylval.string = last_comment_;
    } else {
        yylval.string = last_comment;
    }

    if(last_comment) {
        last_comment = 0;
    }

    return TOK_SEMICOLON;
}

void SCANsave_comment(const char *yytext)
{
    strncpy(last_comment_, yytext, SCAN_COMMENT_LENGTH - 1);
    last_comment = last_comment_;
}

bool SCANread(void)
{
    int     numRead;
    bool done;

    do {
        /* this loop is guaranteed to terminate, since buffer[0] is on yyin */
        while(SCANbuffer.file == NULL) {
            SCANpop_buffer();
            if(SCANtext_ready) {
                return true;
            }
        }

        /* now we have a file buffer */

        /* check for more stuff already buffered */
        if(SCANtext_ready) {
            return true;
        }

        /* check whether we've seen eof on this file */
        if(!SCANbuffer.readEof) {
            numRead = fread(SCANbuffer.text, sizeof(char),
                            SCAN_BUFFER_SIZE, SCANbuffer.file);
            if(numRead < SCAN_BUFFER_SIZE) {
                SCANbuffer.readEof = true;
            }
#ifdef keep_nul
            SCANbuffer.numRead = numRead;
#else
            SCANbuffer.text[numRead] = '\0';
#endif
            SCANcurrent = SCANbuffer.text;
        }

        if(!(done = SCANtext_ready)) {
            if(SCAN_current_buffer == 0) {
                done = true;
                fclose(SCANbuffer.file);   /* close yyin */
                SCANbuffer.file = NULL;
            } else {
                SCANpop_buffer();
            }
        }
    } while(!done);
    return SCANtext_ready;
}


void SCANinclude_file(char *filename)
{
    extern int print_objects_while_running;
    FILE *fp;

    if((fp = fopen(filename, "r")) == NULL) {
        ERRORreport_with_line(INCLUDE_FILE, yylineno);
    } else {
        if(print_objects_while_running & OBJ_SCHEMA_BITS) {
            fprintf(stderr, "parse: including %s at line %d of %s\n",
                    filename, yylineno, SCANbuffer.filename);
        }
        SCANpush_buffer(filename, fp);
    }
}

void SCANlowerize(char *s)
{
    for(; *s; s++) {
        if(isupper(*s)) {
            *s = tolower(*s);
        }
    }
}

void SCANupperize(char *s)
{
    for(; *s; s++) {
        if(islower(*s)) {
            *s = toupper(*s);
        }
    }
}

char *SCANstrdup(const char *s)
{
    char *s2 = (char *)sc_malloc(strlen(s) + 1);
    if(!s2) {
        return 0;
    }

    strcpy(s2, s);
    return s2;
}

long SCANtell()
{
    return yylineno;
}
