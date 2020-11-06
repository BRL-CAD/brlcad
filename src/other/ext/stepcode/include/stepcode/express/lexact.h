#ifndef LEX_ACTIONS_H
#define LEX_ACTIONS_H

/*
 * This software was developed by U.S. Government employees as part of
 * their official duties and is not subject to copyright.
 *
 * $Log$
 * Revision 1.5  1995/04/05 13:55:40  clark
 * CADDETC preval
 *
 * Revision 1.4  1994/11/22  18:32:39  clark
 * Part 11 IS; group reference
 *
 * Revision 1.3  1994/05/11  19:51:05  libes
 * numerous fixes
 *
 * Revision 1.2  1993/10/15  18:48:24  libes
 * CADDETC certified
 *
 * Revision 1.5  1992/08/27  23:41:58  libes
 * modified decl of SCANinitialize
 *
 * Revision 1.4  1992/08/18  17:12:41  libes
 * rm'd extraneous error messages
 *
 * Revision 1.3  1992/06/08  18:06:24  libes
 * prettied up interface to print_objects_when_running
 */

#define keep_nul

/*************/
/* constants */
/*************/

/*****************/
/* packages used */
/*****************/

#include "export.h"
#include <ctype.h>
#include "basic.h"
#include "error.h"

/************/
/* typedefs */
/************/

#define SCAN_BUFFER_SIZE    1024
#define SCAN_NESTING_DEPTH  6
#define SCAN_ESCAPE     '\001'

typedef struct Scan_Buffer {
    char    text[SCAN_BUFFER_SIZE + 1];
#ifdef keep_nul
    int numRead;
#endif
    char  * savedPos;
    FILE  * file;
    char  *  filename;
    bool readEof;
    int lineno;
    int bol;
} Scan_Buffer;

/********************/
/* global variables */
/********************/

extern STEPCODE_EXPRESS_EXPORT Scan_Buffer  SCAN_buffers[SCAN_NESTING_DEPTH];
extern STEPCODE_EXPRESS_EXPORT int      SCAN_current_buffer;
extern STEPCODE_EXPRESS_EXPORT char    *    SCANcurrent;

extern STEPCODE_EXPRESS_EXPORT Error        ERROR_include_file;
extern STEPCODE_EXPRESS_EXPORT Error        ERROR_unmatched_close_comment;
extern STEPCODE_EXPRESS_EXPORT Error        ERROR_unmatched_open_comment;
extern STEPCODE_EXPRESS_EXPORT Error        ERROR_unterminated_string;
extern STEPCODE_EXPRESS_EXPORT Error        ERROR_encoded_string_bad_digit;
extern STEPCODE_EXPRESS_EXPORT Error        ERROR_encoded_string_bad_count;
extern STEPCODE_EXPRESS_EXPORT Error        ERROR_bad_identifier;
extern STEPCODE_EXPRESS_EXPORT Error        ERROR_unexpected_character;
extern STEPCODE_EXPRESS_EXPORT Error        ERROR_nonascii_char;

/******************************/
/* macro function definitions */
/******************************/

#define SCANbuffer  SCAN_buffers[SCAN_current_buffer]
#define SCANbol     SCANbuffer.bol

#ifdef keep_nul
# define SCANtext_ready (SCANbuffer.numRead != 0)
#else
# define SCANtext_ready (*SCANcurrent != '\0')
#endif

/***********************/
/* function prototypes */
/***********************/

extern STEPCODE_EXPRESS_EXPORT void SCANinitialize PROTO( ( void ) );
extern STEPCODE_EXPRESS_EXPORT int  SCANprocess_real_literal PROTO( ( const char * ) );
extern STEPCODE_EXPRESS_EXPORT int  SCANprocess_integer_literal PROTO( ( const char * ) );
extern STEPCODE_EXPRESS_EXPORT int  SCANprocess_binary_literal PROTO( ( const char * ) );
extern STEPCODE_EXPRESS_EXPORT int  SCANprocess_logical_literal PROTO( ( char * ) );
extern STEPCODE_EXPRESS_EXPORT int  SCANprocess_identifier_or_keyword PROTO( ( const char * ) );
extern STEPCODE_EXPRESS_EXPORT int  SCANprocess_string PROTO( ( const char * ) );
extern STEPCODE_EXPRESS_EXPORT int  SCANprocess_encoded_string PROTO( ( const char * ) );
extern STEPCODE_EXPRESS_EXPORT int  SCANprocess_semicolon PROTO( ( const char *, int ) );
extern STEPCODE_EXPRESS_EXPORT void SCANsave_comment PROTO( ( const char * ) );
extern STEPCODE_EXPRESS_EXPORT bool  SCANread PROTO( ( void ) );
#ifdef macros_bit_the_dust
extern STEPCODE_EXPRESS_EXPORT void SCANdefine_macro PROTO( ( char *, char * ) );
#endif
extern STEPCODE_EXPRESS_EXPORT void SCANinclude_file PROTO( ( char * ) );
STEPCODE_EXPRESS_EXPORT void        SCANlowerize PROTO( ( char * ) );
STEPCODE_EXPRESS_EXPORT void        SCANupperize PROTO( ( char * ) );
extern STEPCODE_EXPRESS_EXPORT char  *  SCANstrdup PROTO( ( const char * ) );
extern STEPCODE_EXPRESS_EXPORT long SCANtell PROTO( ( void ) );

#endif /* LEX_ACTIONS_H */
