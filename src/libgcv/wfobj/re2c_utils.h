/* Interface to simplify usage of re2c by providing support for:
 *  - automatic input buffer management
 *        - Just specify a FILE* and the scanner will read from the input file
 *          as needed and save its place in the input between calls to the
 *          scan function.
 *
 *  - end-of-input token
 *        - The YYEOF symbol is defined, and is returned by the scan function 
 *          when the end of the input file is reached.
 *
 *  - ignoring tokens
 *        - Use IGNORE_TOKEN in an action body to ignore the recognized token
 *          and continue scanning.
 *
 *  - (f)lex-like yytext string
 *        - char *yytext will exist as a local buffer that holds the text of the
 *          current token.
 *
 *  - (f)lex-like BEGIN statement
 *        - You can use BEGIN(condition) to set the current start condition.
 *
 * There are a few requirements to get this support:
 *  - You need to include this header in the re2c input file and link to the
 *    corresponding object file.
 *
 *  - Your scanning function needs to return int (the token id). To return the
 *    token id, you must use RETURN(id) rather than simply return id.
 *
 *  - A scanner_t* that you have initialized with initScanner(scanner_t*, FILE*)
 *    (and that you can deconstruct with freeScanner(scanner_t*)) must be
 *    available inside your scan function.
 *
 *  - You need to wrap the re2c comment block between the macros
 *    BEGIN_RE2C(scanner, cursor) and END_RE2C, where scanner is the name of
 *    a scanner_t*, and cursor is the name of the char* given to re2c via:
 *        re2c:define:YYCURSOR = cursor;
 *
 *  - To use the (f)lex-like BEGIN(condition) statement you need to define the
 *    CONDTYPE symbol before including this header:
 *        enum YYCONDTYPE {
 *            INITIAL,
 *            ...other states...
 *        };
 *        #define CONDTYPE enum YYCONDTYPE
 */

#ifndef RE2C_UTILS_H
#define RE2C_UTILS_H

#include "common.h"
#include "bu.h"

__BEGIN_DECLS

/* end of file token - since parser generators start defining token ids at 0,
 * -1 is the least likely value to be used by an existing token
 */
#define YYEOF -1

#ifndef CONDTYPE
enum YYCONDTYPE { INITIAL };
#define CONDTYPE enum YYCONDTYPE
#endif

typedef struct bu_vls bu_string;

typedef struct {
    void *extra; /* user data - user must free */
    FILE *in;    /* user must close */
    bu_string *currLine;
    bu_string *tokenText;
    char *tokenStart;
    char *marker;
    char **cursor_p;
    CONDTYPE startCondition;
} scanner_t;

void initScanner(scanner_t *scanner, FILE *in);
void freeScanner(scanner_t *scanner);

#define YYMARKER re2c_scanner->marker

/* support for (f)lex "start conditions" */
#define BEGIN(sc) setStartCondition(re2c_scanner, sc)
#define YYGETCONDITION() getStartCondition(re2c_scanner)

void setStartCondition(scanner_t *scanner, const CONDTYPE sc);
CONDTYPE getStartCondition(scanner_t *scanner);


/* text processing support */
bu_string *newString();
int getNextLine(bu_string *buf, FILE *in);
int loadInput(scanner_t *scanner);


/* support for (f)lex token string */
#define FREE_TOKEN_TEXT bu_vls_vlsfree(tokenString);
#define yytext (copyCurrTokenText(tokenString, re2c_scanner))

char *copyCurrTokenText(bu_string *tokenString, scanner_t *scanner);


/* update/save stream position */
#define UPDATE_START re2c_scanner->tokenStart = *re2c_scanner->cursor_p;


/* use inside action to skip curr token and continue scanning */
#define IGNORE_TOKEN \
    UPDATE_START; \
    continue;


/* use inside action to return tokenID to caller */
#define RETURN(tokenID) \
    FREE_TOKEN_TEXT \
    UPDATE_START; \
    return tokenID;


#define BEGIN_RE2C(scanner, yycursor) \
    /* define the stuff used by helper macros */ \
    scanner_t *re2c_scanner = scanner; \
    bu_string *tokenString; \
    char *yycursor; \
\
    if (loadInput(re2c_scanner) < 0) { \
	return YYEOF; \
    } \
\
    yycursor = re2c_scanner->tokenStart; \
    re2c_scanner->cursor_p = &yycursor; \
\
    tokenString = newString(); \
\
    /* create implicit label */ \
    while (1) {

#define END_RE2C } /* while (1) */

__END_DECLS

#endif /* RE2C_UTILS_H */
