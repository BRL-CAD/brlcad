#ifndef LEX_ACTIONS_H
#define LEX_ACTIONS_H

/*
 * This software was developed by U.S. Government employees as part of
 * their official duties and is not subject to copyright.
 *
 * $Log: lexact.h,v $
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

#include <sc_export.h>
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
    const char  *  filename;
    bool readEof;
    int lineno;
    int bol;
} Scan_Buffer;

/********************/
/* global variables */
/********************/

extern SC_EXPRESS_EXPORT Scan_Buffer  SCAN_buffers[SCAN_NESTING_DEPTH];
extern SC_EXPRESS_EXPORT int      SCAN_current_buffer;
extern SC_EXPRESS_EXPORT char    *    SCANcurrent;

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

#ifndef static_inline
# define static_inline static inline
#endif

/***********************/
/* function prototypes */
/***********************/

extern SC_EXPRESS_EXPORT void SCANinitialize( void );
extern SC_EXPRESS_EXPORT void SCANcleanup( void );
extern SC_EXPRESS_EXPORT int  SCANprocess_real_literal( const char * );
extern SC_EXPRESS_EXPORT int  SCANprocess_integer_literal( const char * );
extern SC_EXPRESS_EXPORT int  SCANprocess_binary_literal( const char * );
extern SC_EXPRESS_EXPORT int  SCANprocess_logical_literal( char * );
extern SC_EXPRESS_EXPORT int  SCANprocess_identifier_or_keyword( const char * );
extern SC_EXPRESS_EXPORT int  SCANprocess_string( const char * );
extern SC_EXPRESS_EXPORT int  SCANprocess_encoded_string( const char * );
extern SC_EXPRESS_EXPORT int  SCANprocess_semicolon( const char *, int );
extern SC_EXPRESS_EXPORT void SCANsave_comment( const char * );
extern SC_EXPRESS_EXPORT bool SCANread( void );
extern SC_EXPRESS_EXPORT void SCANinclude_file( char * );
       SC_EXPRESS_EXPORT void SCANlowerize( char * );
       SC_EXPRESS_EXPORT void SCANupperize( char * );
extern SC_EXPRESS_EXPORT char  *  SCANstrdup( const char * );
extern SC_EXPRESS_EXPORT long SCANtell( void );

#endif /* LEX_ACTIONS_H */
