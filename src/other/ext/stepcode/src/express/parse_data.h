#ifndef PARSE_DATA
#define PARSE_DATA
#include "expscan.h"

/* TODO: factor out when regenerating the parser */
#define Generic void *

typedef struct parse_data {
    perplex_t scanner;
} parse_data_t;
#endif
