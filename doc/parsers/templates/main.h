#ifndef DPLOT_PARSER
#define DPLOT_PARSER

/* Defining this perplex macro allows you to access "appData" inside
 * any rule in the perplex input file.
 *
 * This and any other perplex macro must be defined before including
 * the perplex-generated header.
 */
#define PERPLEX_ON_ENTER app_data_t *appData = (app_data_t *)yyextra;

/* perplex-generated header with perplex_t definition and perplex
 * function prototypes
 */
#include "scanner.h"

/* lemon-generated header with token definitions */
#include "parser.h"

/* Contains whatever data related to the current token that you want
 * to pass from the scanner to main and the parser.
 *
 * The type name must agree with the Parse() declaration below and the
 * %token_type directive in the lemon input file.
 */
typedef struct {
} token_t;

/* Contains whatever data you want to pass between main, the scanner,
 * and the parser, which typically includes the token data, the
 * in-memory representation of the parsed data, and parser error
 * codes.
 *
 * The type name must agree with the Parse() declaration below and the
 * %extra_argument directive in the lemon input file.
 */
typedef struct {
    token_t token_data;
} app_data_t;

/* lemon prototypes */
void *ParseAlloc(void *(*mallocProc)(size_t));
void ParseFree(void *parser, void (*freeProc)(void *));
void Parse(void *yyp, int yymajor, token_t tokenData, app_data_t *data);
void ParseTrace(FILE *fp, char *s);

#endif
