#ifndef PERPLEX_H
#define PERPLEX_H

#include <stdio.h>

/* tokens are typically ints defined starting from 0,
 * thus -1 is least likely to conflict with another token
 */
#define YYEOF -1

enum {
    TOKEN_DEFINITIONS,
    TOKEN_RULES
};

/* support for start conditions */
typedef enum YYCONDTYPE {
    DEFINITIONS,
    RULES,
    CODE
} condition_t;

/* scanner data */
typedef struct perplex_data_t {
    union {
	FILE *file;
	char *string;
    } in;
    char *cursor;
    char *marker;
    char *null;
    char *bufLast;
    char *buffer;
    char *yytext;
    condition_t condition;
} *perplex_t;

perplex_t perplexFileScanner(FILE *input);
perplex_t perplexStringScanner(char *input);
void perplexFree(perplex_t scanner);

#endif
