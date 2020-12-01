/** \file pp.h
 * for functions that are internal to exppp, not shared
 */
#ifndef PP_H
#define PP_H

#include <sc_stdbool.h>
#include <express/symbol.h>

extern int indent2;                             /**< where continuation lines start */
extern int curpos;                              /**< current line position (1 is first position) */
extern const int NOLEVEL;                       /**< unused-level indicator */

extern Symbol error_sym;                        /**< only used when printing errors */
extern Error ERROR_select_empty;
extern FILE * exppp_fp;
extern bool first_line;

/** output a string, exactly as provided
 * \sa wrap()
 */
void raw( const char * fmt, ... );

/** output a string, insert newlines to keep line length down
 * TODO list globals this func uses
 * \sa raw()
 */
void wrap( const char * fmt, ... );

/** convert a real into our preferred form compatible with 10303-11
 * (i.e. decimal point is required; no trailing zeros)
 * uses a static buffer, so NOT thread safe
 * \param r the real to convert
 * \returns const char pointer to static buffer containing ascii representation of real
 */
const char * real2exp( double r );

/** Break a long un-encoded string up, enclose in '', output via raw()
 * if short, don't insert line breaks
 * \param in the input string
 *
 * side effects: output via raw()
 * reads globals indent2 and curpos
 */
void breakLongStr( const char * in );

int finish_buffer();
int minimum( int a, int b, int c );
int prep_buffer( char * buf, int len );
int prep_string();
void finish_file();
void first_newline();
void prep_file();
char * finish_string();
const char * real2exp( double r );
void exp_output( char * buf, unsigned int len );
void exppp_init();
void exppp_ref_info( Symbol * s );
extern char * placeholder;


#endif /* PP_H */
