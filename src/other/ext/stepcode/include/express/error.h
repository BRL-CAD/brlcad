#ifndef ERROR_H
#define ERROR_H

/** **********************************************************************
** Module:  Error \file error.h
** Description: This module implements the ERROR abstraction.
************************************************************************/

/*
 * This work was supported by the United States Government, and is
 * not subject to copyright.
 *
 * $Log: error.h,v $
 * Revision 1.8  1997/01/21 19:16:55  dar
 * made C++ compatible
 * ,.
 *
 * Revision 1.7  1993/10/15  18:49:23  libes
 * CADDETC certified
 *
 * Revision 1.5  1993/02/22  21:44:34  libes
 * ANSI compat fixes
 *
 * Revision 1.4  1992/08/18  17:15:40  libes
 * rm'd extraneous error messages
 *
 * Revision 1.3  1992/06/08  18:07:35  libes
 * prettied up interface to print_objects_when_running
 */

#include <setjmp.h>

#include "sc_export.h"
#include "basic.h"  /* get basic definitions */

/*****************/
/* packages used */
/*****************/

#include "alloc.h"
#include "symbol.h"

enum ErrorCode {
    /* dict.c */
    DUPLICATE_DECL = 1,
    DUPLICATE_DECL_DIFF_FILE,
    /* error.c */
    SUBORDINATE_FAILED,
    SYNTAX_EXPECTING,
    /* expr.c */
    INTEGER_EXPRESSION_EXPECTED,
    INTERNAL_UNRECOGNISED_OP_IN_EXPRESOLVE,
    ATTRIBUTE_REF_ON_AGGREGATE,
    ATTRIBUTE_REF_FROM_NON_ENTITY,
    INDEXING_ILLEGAL,
    WARN_INDEXING_MIXED,
    ENUM_NO_SUCH_ITEM,
    GROUP_REF_NO_SUCH_ENTITY,
    GROUP_REF_UNEXPECTED_TYPE,
    IMPLICIT_DOWNCAST,
    AMBIG_IMPLICIT_DOWNCAST,
    /* express.c */
    BAIL_OUT,
    SYNTAX,
    REF_NONEXISTENT,
    TILDE_EXPANSION_FAILED,
    SCHEMA_NOT_IN_OWN_SCHEMA_FILE,
    UNLABELLED_PARAM_TYPE,
    FILE_UNREADABLE,
    FILE_UNWRITABLE,
    WARN_UNSUPPORTED_LANG_FEAT,
    WARN_SMALL_REAL,
    /* lexact.c */
    INCLUDE_FILE,
    UNMATCHED_CLOSE_COMMENT,
    UNMATCHED_OPEN_COMMENT,
    UNTERMINATED_STRING,
    ENCODED_STRING_BAD_DIGIT,
    ENCODED_STRING_BAD_COUNT,
    BAD_IDENTIFIER,
    UNEXPECTED_CHARACTER,
    NONASCII_CHAR,
    /* linklist.c */
    EMPTY_LIST,
    /* resolve.c */
    UNDEFINED,
    UNDEFINED_ATTR,
    UNDEFINED_TYPE,
    UNDEFINED_SCHEMA,
    UNKNOWN_ATTR_IN_ENTITY,
    UNKNOWN_SUBTYPE,
    UNKNOWN_SUPERTYPE,
    CIRCULAR_REFERENCE,
    SUBSUPER_LOOP,
    SUBSUPER_CONTINUATION,
    SELECT_LOOP,
    SELECT_CONTINUATION,
    SUPERTYPE_RESOLVE,
    SUBTYPE_RESOLVE,
    NOT_A_TYPE,
    FUNCALL_NOT_A_FUNCTION,
    UNDEFINED_FUNC,
    EXPECTED_PROC,
    NO_SUCH_PROCEDURE,
    WRONG_ARG_COUNT,
    QUERY_REQUIRES_AGGREGATE,
    SELF_IS_UNKNOWN,
    INVERSE_BAD_ENTITY,
    INVERSE_BAD_ATTR,
    MISSING_SUPERTYPE,
    TYPE_IS_ENTITY,
    AMBIGUOUS_ATTR,
    AMBIGUOUS_GROUP,
    OVERLOADED_ATTR,
    REDECL_NO_SUCH_ATTR,
    REDECL_NO_SUCH_SUPERTYPE,
    MISSING_SELF,
    FN_SKIP_BRANCH,
    CASE_SKIP_LABEL,
    UNIQUE_QUAL_REDECL,
    /* type.c */
    CORRUPTED_TYPE,
    UNDEFINED_TAG,
    /* exppp.c */
    SELECT_EMPTY,
};

/*************/
/* constants */
/*************/

#define ERROR_none      (Error)NULL
#define ERROR_MAX       100

/************/
/* typedefs */
/************/

enum Severity {
    SEVERITY_WARNING = 0,
    SEVERITY_ERROR,
    SEVERITY_EXIT,
    SEVERITY_DUMP,
    SEVERITY_MAX
};

/***************************/
/* hidden type definitions */
/***************************/

struct Error_ {
    enum Severity severity;
    const char *message;
    const char *name;
    bool override;
};

typedef struct Error_ *Error;

/****************/
/* modules used */
/****************/

/********************/
/* global variables */
/********************/

extern SC_EXPRESS_EXPORT bool __ERROR_buffer_errors;
extern SC_EXPRESS_EXPORT const char * current_filename;

/* flag to remember whether non-warning errors have occurred */
extern SC_EXPRESS_EXPORT bool ERRORoccurred;


/* all of these are 1 if true, 0 if false switches */
/* for debugging fedex */
extern SC_EXPRESS_EXPORT int ERRORdebugging;
/* for debugging malloc during resolution */
extern SC_EXPRESS_EXPORT int malloc_debug_resolve;
/* for debugging yacc/lex */
extern SC_EXPRESS_EXPORT int debug;

extern SC_EXPRESS_EXPORT void ( *ERRORusage_function )( void );

/***********************/
/* function prototypes */
/***********************/

extern SC_EXPRESS_EXPORT void ERROR_start_message_buffer( void );
extern SC_EXPRESS_EXPORT void ERROR_flush_message_buffer( void );

static inline void ERRORbuffer_messages( bool flag ) {
    __ERROR_buffer_errors = flag;
    if( __ERROR_buffer_errors ) {
        ERROR_start_message_buffer();
    } else {
        ERROR_flush_message_buffer();
    }
}

static inline void ERRORflush_messages( void ) {
    if( __ERROR_buffer_errors ) {
        ERROR_flush_message_buffer();
        ERROR_start_message_buffer();
    }
}


/***********************/
/* function prototypes */
/***********************/

extern SC_EXPRESS_EXPORT void ERRORinitialize( void );
extern SC_EXPRESS_EXPORT void ERRORcleanup( void );
extern SC_EXPRESS_EXPORT void ERRORnospace( void );
extern SC_EXPRESS_EXPORT void ERRORabort( int );
extern SC_EXPRESS_EXPORT void ERRORreport( enum ErrorCode, ... );

struct Symbol_; /* mention Symbol to avoid warning on following line */
extern SC_EXPRESS_EXPORT void ERRORreport_with_symbol( enum ErrorCode, struct Symbol_ *, ... );
extern SC_EXPRESS_EXPORT void ERRORreport_with_line( enum ErrorCode, int, ... );

extern SC_EXPRESS_EXPORT void ERRORset_warning( char *, bool );
extern SC_EXPRESS_EXPORT void ERRORset_all_warnings( bool );
extern SC_EXPRESS_EXPORT void ERRORsafe( jmp_buf env );
extern SC_EXPRESS_EXPORT void ERRORunsafe( void );

extern char * ERRORget_warnings_help(const char* prefix, const char *eol);
extern bool ERRORis_enabled(enum ErrorCode errnum);

#endif /* ERROR_H */
