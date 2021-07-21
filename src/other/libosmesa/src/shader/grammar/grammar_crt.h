#ifndef GRAMMAR_CRT_H
#define GRAMMAR_CRT_H


#include <stdlib.h>
#include <malloc.h>
#include <string.h>


typedef unsigned long grammar;
typedef unsigned char byte;


#define GRAMMAR_PORT_INCLUDE 1
#include "grammar.h"
#undef GRAMMAR_PORT_INCLUDE


#endif


/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
