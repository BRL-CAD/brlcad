/* The perplex input scanner. Converts flex-style input to re2c input.
 * Usage:
 *     int tokenID;
 *     perplex_t scanner;
 *      ...
 *     perplexFileScanner(scanner, inFile);
 *     while ((tokenID = yylex(scanner)) != YYEOF) {
 *      ...
 *     }
 *     perplexFree(scanner);
 *     fclose(inFile);
 */
#include <stdio.h>
#include <stdlib.h>
#include "perplex.h"

static void
setCondition(perplex_t scanner, condition_t cond)
{
    scanner->condition = cond;
}

static condition_t
getCondition(perplex_t scanner)
{
    return scanner->condition;
}

/*!max:re2c*/

/* Estimate of largest number of characters "in use" at once:
 * (<= YYMAXFILL input chars) + (<= YYMAXFILL backtracking chars) + '\0'
 */
#define MAX_IN_USE ((YYMAXFILL * 2 + 1) * 2)

/* Size of scanner buffer. Should be big enough to comfortably hold
 * everything from backtracking marker to null marker.
 */
static const int BUF_SIZE = (MAX_IN_USE > 128) ? MAX_IN_USE : 128;

/* Copy up to n input characters to the end of scanner buffer.
 * If EOF is encountered before n characters are read, '\0'
 * is appended to the buffer to serve as EOF indicator.
 */
void
bufferAppend(perplex_t scanner, size_t n)
{
    FILE *in = scanner->in.file;
    char *new, *end;

    new = scanner->null;
    end = new + n;

    while (new < end) {
	if ((*new = fgetc(in)) != EOF) {
	    new++;
	} else {
	    *new = '\0'; new++;
	    break;
	}
    }
    *new = '\0';
    scanner->null = new;
}

/* Appends up to n characters of input to scanner buffer.
 *
 * Buffer contents are shifted if there is insufficient room
 * at the end of the buffer.
 */
void
bufferFill(perplex_t scanner, size_t n)
{
    size_t i, shiftSize;
    char *new, *old;

    /* If appending will put null past last element,
     * then shift remaining in-use input to make room.
     */
    if ((scanner->null + n) > scanner->bufLast) {
	new = scanner->buffer;
	old = scanner->marker;
	shiftSize = old - new;

	for (i = 0; i < shiftSize; i++) {
	    new[i] = old[i];
	}
	/* update markers */
	scanner->marker  = scanner->buffer;
	scanner->cursor -= shiftSize;
	scanner->null   -= shiftSize;
    }
    bufferAppend(scanner, n);
}

static perplex_t
newScanner()
{
    perplex_t scanner;
    scanner = (perplex_t)calloc(1, sizeof(struct perplex_data_t));

    setCondition(scanner, DEFINITIONS);
    scanner->buffer = NULL;

    return scanner;
}

/* public functions */

perplex_t
perplexStringScanner(char *input)
{
    perplex_t scanner = newScanner();

    scanner->in.string = input;

    return scanner;
}

perplex_t
perplexFileScanner(FILE *input)
{
    perplex_t scanner = newScanner();

    scanner->in.file = input;

    scanner->buffer = (char*)calloc(BUF_SIZE, sizeof(char));
    scanner->bufLast = &scanner->buffer[BUF_SIZE - 1];

    scanner->null = scanner->marker = scanner->cursor = scanner->buffer;

    return scanner;
}

void
perplexFree(perplex_t scanner)
{
    if (scanner->buffer != NULL) {
	free(scanner->buffer);
    }

    free(scanner);
}

#define RETURN(id) return id
#define YYGETCONDITION getCondition(scanner)
#define YYFILL(n) bufferFill(scanner, n)

/* if it's re2c, echo it
 * otherwise, convert it to re2c
 */
int yylex(perplex_t scanner) {

    while (1) {
/*!re2c
re2c:condenumprefix = "";
re2c:define:YYCONDTYPE = condition_t;
re2c:define:YYGETCONDITION:naked = 1;

re2c:define:YYCTYPE  = char;
re2c:define:YYCURSOR = scanner->cursor;
re2c:define:YYLIMIT  = scanner->null;
re2c:define:YYMARKER = scanner->marker;

NAME = [a-zA-Z_][a-zA-Z0-9_-]*;
EOL = '\n';
EOF = '\000';
ANY = [^\000];
LINE_ANY = [^\000\n];

SEPARATOR = "%%"EOL;
FAUX_SEPARATOR = LINE_ANY"%%"EOL;

<*>EOF { RETURN(YYEOF); }

<*>FAUX_SEPARATOR {
    /* print text as it appears */
    printf("%c%%%%\n", scanner->cursor[-4]);
    continue;
}

<DEFINITIONS>SEPARATOR {
    setCondition(scanner, RULES);
    RETURN(TOKEN_DEFINITIONS);
}

<RULES>SEPARATOR {
    setCondition(scanner, CODE);
    RETURN(TOKEN_RULES);
}

<*>ANY {
    /* echo rule */
    printf("%c", scanner->cursor[-1]);
    continue;
}
*/
    }
}
