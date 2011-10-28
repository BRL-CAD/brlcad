#ifndef LEX_ACTIONS_H
#define LEX_ACTIONS_H

/* $Id: lexact.h,v 1.5 1995/04/05 13:55:40 clark dec96 $ */

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

#include <ctype.h>
#include "basic.h"
#include "error.h"

/************/
/* typedefs */
/************/

#define SCAN_BUFFER_SIZE	1024
#define SCAN_NESTING_DEPTH	6
#define SCAN_ESCAPE		'\001'

typedef struct Scan_Buffer {
	char	text[SCAN_BUFFER_SIZE + 1];
#ifdef keep_nul
	int	numRead;
#endif
	char*	savedPos;
	FILE*	file;
	char	*filename;
	Boolean	readEof;
	int	lineno;
	int	bol;
} Scan_Buffer;

/********************/
/* global variables */
/********************/

extern Scan_Buffer	SCAN_buffers[SCAN_NESTING_DEPTH];
extern int		SCAN_current_buffer;
extern char*		SCANcurrent;

extern Error		ERROR_include_file;
extern Error		ERROR_unmatched_close_comment;
extern Error		ERROR_unmatched_open_comment;
extern Error		ERROR_unterminated_string;
extern Error		ERROR_encoded_string_bad_digit;
extern Error		ERROR_encoded_string_bad_count;
extern Error		ERROR_bad_identifier;
extern Error		ERROR_unexpected_character;
extern Error		ERROR_nonascii_char;

/******************************/
/* macro function definitions */
/******************************/

#define SCANbuffer	SCAN_buffers[SCAN_current_buffer]
#define SCANbol		SCANbuffer.bol

#ifdef keep_nul
# define SCANtext_ready	(SCANbuffer.numRead != 0)
#else
# define SCANtext_ready	(*SCANcurrent != '\0')
#endif

/***********************/
/* function prototypes */
/***********************/

extern int	yylex PROTO((void));	/* the scanner */

extern void	SCANinitialize PROTO((void));
extern int	SCANprocess_real_literal PROTO((void));
extern int	SCANprocess_integer_literal PROTO((void));
extern int	SCANprocess_binary_literal PROTO((void));
extern int	SCANprocess_logical_literal PROTO((char *));
extern int	SCANprocess_identifier_or_keyword PROTO((void));
extern int	SCANprocess_string PROTO((void));
extern int	SCANprocess_encoded_string PROTO((void));
extern int	SCANprocess_semicolon PROTO((int));
extern void	SCANsave_comment PROTO((void));
extern Boolean	SCANread PROTO((void));
#if macros_bit_the_dust
extern void	SCANdefine_macro PROTO((char *, char *));
#endif
extern void	SCANinclude_file PROTO((char *));
void		SCANlowerize PROTO((char *));
void		SCANupperize PROTO((char *));
extern char *	SCANstrdup PROTO((char *));
extern long	SCANtell PROTO((void));

#endif /* LEX_ACTIONS_H */
