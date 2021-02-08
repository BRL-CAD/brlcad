

/** **********************************************************************
** Module:  Error \file error.c
** This module implements the ERROR abstraction
************************************************************************/

/*
 * Development of this code was funded by the United States Government,
 * and is not subject to copyright.
 *
 * $Log: error.c,v $
 * Revision 1.13  1997/10/23 21:41:44  sauderd
 * I am backing out to version 1.10 before the changes related to va_list and
 * __STDC__ etc. I don't have time to finish fixing all this.
 *
 * Revision 1.12  1997/10/23 21:35:31  sauderd
 * Changed more mess with compiler directives for __STDC__ and HAVE_STDARG_H
 * but am getting deeper into problems because of how PROTO is defined for
 * dealing with prototypes based on standard C or not. PROTO is broken as well
 * so I'm not going down this road further. Next I will back out but am this
 * far ahead for later if we fix this.
 *
 * Revision 1.11  1997/10/22 16:10:26  sauderd
 * This would #include stdarg.h if __STDC__ was defined otherwise it would
 * #include vararg.h. I changed it to check the configure generated config file
 * named sc_cf.h (if HAVE_CONFIG_H is defined - it's also defined by
 * configure) to see if HAVE_STDARG_H is defined. If it is it #includes stdarg.h
 * otherwise it #includes vararg.h. If HAVE_CONFIG_H isn't defined then it works
 * like it used to.
 *
 * Revision 1.10  1997/01/21 19:19:51  dar
 * made C++ compatible
 *
 * Revision 1.9  1994/05/11  19:51:46  libes
 * numerous fixes
 *
 * Revision 1.8  1993/10/15  18:49:55  libes
 * CADDETC certified
 *
 * Revision 1.6  1993/02/22  21:44:34  libes
 * ANSI compat fixes
 *
 * Revision 1.5  1993/01/19  22:45:07  libes
 * *** empty log message ***
 *
 * Revision 1.4  1992/08/18  17:16:22  libes
 * rm'd extraneous error messages
 *
 * Revision 1.3  1992/06/08  18:08:05  libes
 * prettied up interface to print_objects_when_running
 */

#include <stdio.h>
#include <stdlib.h>
#include <setjmp.h>
#include <signal.h>
#include <string.h>
#include <stdarg.h>

#include "sc_memmgr.h"
#include "express/express.h"
#include "express/error.h"
#include "express/info.h"
#include "express/linklist.h"

#if defined(_MSC_VER) && _MSC_VER < 1900
#  include "sc_stdio.h"
#  define vsnprintf c99_vsnprintf
#endif

static struct Error_ LibErrors[] = {
    /* dict.c */
    [DUPLICATE_DECL] = {SEVERITY_ERROR, "Redeclaration of %s.  Previous declaration was on line %d.", NULL, false},
    [DUPLICATE_DECL_DIFF_FILE] = {SEVERITY_ERROR, "Redeclaration of %s.  Previous declaration was on line %d in file %s.", NULL, false},
    /* error.c */
    [SUBORDINATE_FAILED] = {SEVERITY_ERROR, "A subordinate failed.", NULL, false},
    [SYNTAX_EXPECTING] = {SEVERITY_EXIT, "%s, expecting %s in %s %s", NULL, false},
    /* expr.c */
    [INTEGER_EXPRESSION_EXPECTED] = {SEVERITY_WARNING, "Integer expression expected", NULL, false},
    [INTERNAL_UNRECOGNISED_OP_IN_EXPRESOLVE] = {SEVERITY_ERROR, "Opcode unrecognized while trying to resolve expression", NULL, false},
    [ATTRIBUTE_REF_ON_AGGREGATE] = {SEVERITY_ERROR, "Attribute %s cannot be referenced from an aggregate", NULL, false},
    [ATTRIBUTE_REF_FROM_NON_ENTITY] = {SEVERITY_ERROR, "Attribute %s cannot be referenced from a non-entity", NULL, false},
    [INDEXING_ILLEGAL] = {SEVERITY_ERROR, "Indexing is only permitted on aggregates", NULL, false},
    [WARN_INDEXING_MIXED] = {SEVERITY_WARNING, "non-aggregates) and/or different aggregation types.", "indexing", false},
    [ENUM_NO_SUCH_ITEM] = {SEVERITY_ERROR, "Enumeration type %s does not contain item %s", NULL, false},
    [GROUP_REF_NO_SUCH_ENTITY] = {SEVERITY_ERROR, "Group reference failed to find entity %s", NULL, false},
    [GROUP_REF_UNEXPECTED_TYPE] = {SEVERITY_ERROR, "Group reference of unusual expression %s", NULL, false},
    [IMPLICIT_DOWNCAST] = {SEVERITY_WARNING, "Implicit downcast to %s.", "downcast", false},
    [AMBIG_IMPLICIT_DOWNCAST] = {SEVERITY_WARNING, "Possibly ambiguous implicit downcast (%s?).", "downcast", false},
    /* express.c */
    [BAIL_OUT] = {SEVERITY_DUMP, "Aborting due to internal error(s)", NULL, false},
    [SYNTAX] = {SEVERITY_EXIT, "%s in %s %s", NULL, false},
    [REF_NONEXISTENT] = {SEVERITY_ERROR, "USE/REF of non-existent object (%s in schema %s)", NULL, false},
    [TILDE_EXPANSION_FAILED] = {SEVERITY_ERROR, "Tilde expansion for %s failed in EXPRESS_PATH environment variable", NULL, false},
    [SCHEMA_NOT_IN_OWN_SCHEMA_FILE] = {SEVERITY_ERROR, "Schema %s was not found in its own schema file (%s)", NULL, false},
    [UNLABELLED_PARAM_TYPE] = {SEVERITY_ERROR, "Return type or local variable requires type label in `%s'", NULL, false},
    [FILE_UNREADABLE] = {SEVERITY_ERROR, "Could not read file %s: %s", NULL, false},
    [FILE_UNWRITABLE] = {SEVERITY_ERROR, "Could not write file %s: %s", NULL, false},
    [WARN_UNSUPPORTED_LANG_FEAT] = {SEVERITY_WARNING, "Unsupported language feature (%s) at %s:%d", "unsupported", false},
    [WARN_SMALL_REAL] = {
        SEVERITY_WARNING, "REALs with extremely small magnitude may be interpreted as zero by other EXPRESS parsers "
        "(IEEE 754 float denormals are sometimes rounded to zero) - fabs(%f) <= FLT_MIN.", "limits", false
    },
    /* lexact.c */
    [INCLUDE_FILE] = {SEVERITY_ERROR, "Could not open include file `%s'.", NULL, false},
    [UNMATCHED_CLOSE_COMMENT] = {SEVERITY_ERROR, "unmatched close comment", NULL, false},
    [UNMATCHED_OPEN_COMMENT] = {SEVERITY_ERROR, "unmatched open comment", NULL, false},
    [UNTERMINATED_STRING] = {SEVERITY_ERROR, "unterminated string literal", NULL, false},
    [ENCODED_STRING_BAD_DIGIT] = {SEVERITY_ERROR, "non-hex digit (%c) in encoded string literal", NULL, false},
    [ENCODED_STRING_BAD_COUNT] = {SEVERITY_ERROR, "number of digits (%d) in encoded string literal is not divisible by 8", NULL, false},
    [BAD_IDENTIFIER] = {SEVERITY_ERROR, "identifier (%s) cannot start with underscore", NULL, false},
    [UNEXPECTED_CHARACTER] = {SEVERITY_ERROR, "character (%c) is not a valid lexical element by itself", NULL, false},
    [NONASCII_CHAR] = {SEVERITY_ERROR, "character (0x%x) is not in the EXPRESS character set", NULL, false},
    /* linklist.c */
    [EMPTY_LIST] = {SEVERITY_ERROR, "Empty list in %s.", NULL, false},
    /* resolve.c */
    [UNDEFINED] = {SEVERITY_ERROR, "Reference to undefined object %s.", NULL, false},
    [UNDEFINED_ATTR] = {SEVERITY_ERROR, "Reference to undefined attribute %s.", NULL, false},
    [UNDEFINED_TYPE] = {SEVERITY_ERROR, "Reference to undefined type %s.", NULL, false},
    [UNDEFINED_SCHEMA] = {SEVERITY_ERROR, "Reference to undefined schema %s.", NULL, false},
    [UNKNOWN_ATTR_IN_ENTITY] = {SEVERITY_ERROR, "Unknown attribute %s in entity %s.", NULL, false},
    [UNKNOWN_SUBTYPE] = {SEVERITY_EXIT, "Unknown subtype %s for entity %s.", "unknown_subtype", false},
    [UNKNOWN_SUPERTYPE] = {SEVERITY_ERROR, "Unknown supertype %s for entity %s.", NULL, false},
    [CIRCULAR_REFERENCE] = {SEVERITY_ERROR, "Circularity: definition of %s references itself.", NULL, false},
    [SUBSUPER_LOOP] = {SEVERITY_ERROR, "Entity %s is a subtype of itself", "circular_subtype", false},
    [SUBSUPER_CONTINUATION] = {SEVERITY_ERROR, "  (via supertype entity %s)", NULL, false},
    [SELECT_LOOP] = {SEVERITY_ERROR, "Select type %s selects itself", "circular_select", false},
    [SELECT_CONTINUATION] = {SEVERITY_ERROR, "  (via select type %s)", NULL, false},
    [SUPERTYPE_RESOLVE] = {SEVERITY_ERROR, "Supertype %s is not an entity (line %d).", NULL, false},
    [SUBTYPE_RESOLVE] = {SEVERITY_ERROR, "Subtype %s resolves to non-entity %s on line %d.", NULL, false},
    [NOT_A_TYPE] = {SEVERITY_ERROR, "Expected a type (or entity) but %s is %s.", NULL, false},
    [FUNCALL_NOT_A_FUNCTION] = {SEVERITY_ERROR, "Function call of %s which is not a function.", NULL, false},
    [UNDEFINED_FUNC] = {SEVERITY_ERROR, "Function %s undefined.", NULL, false},
    [EXPECTED_PROC] = {SEVERITY_ERROR, "%s is used as a procedure call but is not defined as one (line %d).", NULL, false},
    [NO_SUCH_PROCEDURE] = {SEVERITY_ERROR, "No such procedure as %s.", NULL, false},
    [WRONG_ARG_COUNT] = {SEVERITY_WARNING, "Call to %s uses %d arguments, but expected %d.", NULL, false},
    [QUERY_REQUIRES_AGGREGATE] = {SEVERITY_ERROR, "Query expression source must be an aggregate.", NULL, false},
    [SELF_IS_UNKNOWN] = {SEVERITY_ERROR, "SELF is not within an entity declaration.", NULL, false},
    [INVERSE_BAD_ENTITY] = {SEVERITY_ERROR, "Attribute %s is referenced from non-entity-inheriting type.", NULL, false},
    [INVERSE_BAD_ATTR] = {SEVERITY_ERROR, "Unknown attribute %s in entity %s in inverse.", NULL, false},
    [MISSING_SUPERTYPE] = {SEVERITY_ERROR, "Entity %s missing from supertype list for subtype %s.", NULL, false},
    [TYPE_IS_ENTITY] = {SEVERITY_ERROR, "An entity (%s) is not acceptable as an underlying type.", "entity_as_type", false},
    [AMBIGUOUS_ATTR] = {SEVERITY_WARNING, "Ambiguous attribute reference %s.", NULL, false},
    [AMBIGUOUS_GROUP] = {SEVERITY_WARNING, "Ambiguous group reference %s.", NULL, false},
    [OVERLOADED_ATTR] = {SEVERITY_ERROR, "Attribute %s already inherited via supertype %s.", NULL, false},
    [REDECL_NO_SUCH_ATTR] = {SEVERITY_ERROR, "Redeclared attribute %s not declared in supertype %s.", NULL, false},
    [REDECL_NO_SUCH_SUPERTYPE] = {SEVERITY_ERROR, "No such supertype %s for redeclaration of attribute %s.", NULL, false},
    [MISSING_SELF] = {SEVERITY_ERROR, "Domain rule %s must refer to SELF or attribute.", NULL, false},
    [FN_SKIP_BRANCH] = {SEVERITY_WARNING, "IF statement condition is always %s. Ignoring branch that is never taken.", "invariant_condition", false},
    [CASE_SKIP_LABEL] = {SEVERITY_WARNING, "CASE label %s cannot be matched. Ignoring its statements.", "invalid_case", false},
    [UNIQUE_QUAL_REDECL] = {SEVERITY_WARNING, "Possibly unnecessary qualifiers on redeclared attr '%s' in a uniqueness rule of entity '%s'.", "unnecessary_qualifiers", false},
    /* type.c */
    [CORRUPTED_TYPE] = {SEVERITY_DUMP, "Corrupted type in %s", NULL, false},
    [UNDEFINED_TAG] = {SEVERITY_ERROR, "Undefined type tag %s", NULL, false},
    /* exppp.c */
    [SELECT_EMPTY] = {SEVERITY_ERROR, "select type %s has no members", NULL, false},
};


bool __ERROR_buffer_errors = false;
const char *current_filename = "stdin";

/* flag to remember whether non-warning errors have occurred */
bool ERRORoccurred = false;

Error experrc = ERROR_none;
Error ERROR_subordinate_failed = ERROR_none;
Error ERROR_syntax_expecting = ERROR_none;

/* all of these are 1 if true, 0 if false switches */
/* for debugging fedex */
int ERRORdebugging = 0;
/* for debugging malloc during resolution */
int malloc_debug_resolve = 0;
/* for debugging yacc/lex */
int debug = 0;

void (*ERRORusage_function)(void);

#define ERROR_MAX_ERRORS    100 /**< max line-numbered errors */
#define ERROR_MAX_SPACE     4000 /**< max space for line-numbered errors */
#define ERROR_MAX_STRLEN    200 /**< assuming all error messages are less than this,
* if we have less than this much space remaining
* in the error string buffer, call it a day and
* dump the buffer */

static struct heap_element {
    int line;
    char *msg;
} heap[ERROR_MAX_ERRORS + 1]; /**< NOTE!  element 0 is purposely ignored, and
                                * an additional element is at the end.  This
                                * allows the later heap calculations to be
                                * much simpler */

static int ERROR_with_lines = 0;    /**< number of warnings & errors that have occurred with a line number */
static char *ERROR_string;
static char *ERROR_string_base;
static char *ERROR_string_end;

static bool ERROR_unsafe = false;
static jmp_buf ERROR_safe_env;


#define error_file stderr /**< message buffer file */

static int ERROR_vprintf(const char *format, va_list ap)
{
    int result = vsnprintf(ERROR_string, ERROR_string_end - ERROR_string, format, ap);

    if(result < 0) {
        ERROR_string = ERROR_string_end;
    } else if(result > (ERROR_string_end - ERROR_string)) {
        ERROR_string = ERROR_string_end;
    } else {
        ERROR_string = ERROR_string + result;
    }
    return result;
}

static int ERROR_printf(const char *format, ...)
{
    int result;
    va_list ap;
    va_start(ap, format);
    result = ERROR_vprintf(format, ap);
    va_end(ap);
    return result;
}

static void ERROR_nexterror()
{
    if(ERROR_string == ERROR_string_end) {
        return;
    }
    ERROR_string++;
}

/** Initialize the Error module */
void ERRORinitialize(void)
{
    ERROR_string_base = (char *)sc_malloc(ERROR_MAX_SPACE);
    ERROR_string_end = ERROR_string_base + ERROR_MAX_SPACE;
    ERROR_start_message_buffer();

#ifdef SIGQUIT
    signal(SIGQUIT, ERRORabort);
#endif
#ifdef SIGBUS
    signal(SIGBUS, ERRORabort);
#endif
#ifdef SIGSEGV
    signal(SIGSEGV, ERRORabort);
#endif
#ifdef SIGABRT
    signal(SIGABRT, ERRORabort);
#endif
}

/** Clean up the Error module */
void ERRORcleanup(void)
{
    sc_free(ERROR_string_base);
}

void ERRORset_warning(char *name, bool warn_only)
{
    Error err;
    bool found = false;

    for(unsigned int errnum = 0; errnum < (sizeof LibErrors / sizeof LibErrors[0]); errnum++) {
        err = &LibErrors[errnum];
        if(err->severity <= SEVERITY_WARNING && !strcmp(err->name, name)) {
            found = true;
            err->override = warn_only;
        }
    }

    if(!found) {
        fprintf(stderr, "unknown warning: %s\n", name);
        if(ERRORusage_function) {
            (*ERRORusage_function)();
        } else {
            EXPRESSusage(1);
        }
    }
}

void ERRORset_all_warnings(bool warn_only)
{
    Error err;

    for(unsigned int errnum = 0; errnum < (sizeof LibErrors / sizeof LibErrors[0]); errnum++) {
        err = &LibErrors[errnum];
        if(err->severity <= SEVERITY_WARNING) {
            err->override = warn_only;
        }
    }
}

char *ERRORget_warnings_help(const char *prefix, const char *eol)
{
    unsigned int sz = 2048, len, clen;
    char *buf, *nbuf;
    Error err;

    clen = strlen(prefix) + strlen(eol) + 1;

    buf = sc_malloc(sz);
    if(!buf) {
        fprintf(error_file, "failed to allocate memory for warnings help!\n");
    }
    buf[0] = '\0';

    for(unsigned int errnum = 0; errnum < (sizeof LibErrors / sizeof LibErrors[0]); errnum++) {
        err = &LibErrors[errnum];
        if(err->name) {
            len = strlen(buf) + strlen(err->name) + clen;
            if(len > sz) {
                sz *= 2;
                nbuf = sc_realloc(buf, sz);
                if(!nbuf) {
                    fprintf(error_file, "failed to reallocate / grow memory for warnings help!\n");
                }
                buf = nbuf;
            }
            strcat(buf, prefix);
            strcat(buf, err->name);
            strcat(buf, eol);
        }
    }

    return buf;
}

bool
ERRORis_enabled(enum ErrorCode errnum)
{
    Error err = &LibErrors[errnum];
    return !err->override;
}

/** \fn ERRORreport
** \param what error to report
** \param ... arguments for error string
** Print a report of an error
**
** Notes:   The second and subsequent arguments should match the
**      format fields of the message generated by 'what.'
*/
void
ERRORreport(enum ErrorCode errnum, ...)
{
    va_list args;

    va_start(args, errnum);
    Error what = &LibErrors[errnum];

    if(errnum != SUBORDINATE_FAILED && ERRORis_enabled(errnum)) {
        if(what->severity >= SEVERITY_ERROR) {
            fprintf(error_file, "ERROR PE%03d: ", errnum);
            vfprintf(error_file, what->message, args);
            fputc('\n', error_file);
            ERRORoccurred = true;
        } else {
            fprintf(error_file, "WARNING PW%03d: %d", errnum, what->severity);
            vfprintf(error_file, what->message, args);
            fputc('\n', error_file);
        }
        if(what->severity >= SEVERITY_EXIT) {
            ERROR_flush_message_buffer();
            if(what->severity >= SEVERITY_DUMP) {
                abort();
            } else {
                exit(EXPRESS_fail((Express)0));
            }
        }
    }
    va_end(args);
}

/**
** \param what error to report
** \param line line number of error
** \param ... arguments for error string
** Print a report of an error, including a line number
**
** \note The third and subsequent arguments should match the
**      format fields of the message generated by 'what.'
*/
void
ERRORreport_with_line(enum ErrorCode errnum, int line, ...)
{
    Symbol sym;
    va_list args;
    va_start(args, line);

    sym.filename = current_filename;
    sym.line = line;
    ERRORreport_with_symbol(errnum, &sym, args);
}

void
ERRORreport_with_symbol(enum ErrorCode errnum, Symbol *sym, ...)
{
    va_list args;
    va_start(args, sym);
    Error what = &LibErrors[errnum];

    if(errnum != SUBORDINATE_FAILED && ERRORis_enabled(errnum)) {
        if(__ERROR_buffer_errors) {
            int child, parent;

            /*
             * add an element to the heap
             * by (logically) storing the new value
             * at the end of the array and bubbling
             * it up as necessary
             */

            child = ++ERROR_with_lines;
            parent = child / 2;
            while(parent) {
                if(sym->line < heap[parent].line) {
                    heap[child] = heap[parent];
                } else {
                    break;
                }
                child = parent;
                parent = child / 2;
            }
            heap[child].line = sym->line;
            heap[child].msg = ERROR_string;

            if(what->severity >= SEVERITY_ERROR) {
                ERROR_printf("%s:%d: --ERROR PE%03d: ", sym->filename, sym->line, errnum);
                ERROR_vprintf(what->message, args);
                ERROR_nexterror();
                ERRORoccurred = true;
            } else {
                ERROR_printf("%s:%d: WARNING PW%03d: ", sym->filename, sym->line, errnum);
                ERROR_vprintf(what->message, args);
                ERROR_nexterror();
            }
            if(what->severity >= SEVERITY_EXIT ||
                    ERROR_string + ERROR_MAX_STRLEN > ERROR_string_base + ERROR_MAX_SPACE ||
                    ERROR_with_lines == ERROR_MAX_ERRORS) {
                ERROR_flush_message_buffer();
                if(what->severity >= SEVERITY_DUMP) {
                    abort();
                } else {
                    exit(EXPRESS_fail((Express)0));
                }
            }
        } else {
            if(what->severity >= SEVERITY_ERROR) {
                fprintf(error_file, "%s:%d: --ERROR PE%03d: ", sym->filename, sym->line, errnum);
                vfprintf(error_file, what->message, args);
                fprintf(error_file, "\n");
                ERRORoccurred = true;
            } else {
                fprintf(error_file, "%s:%d: WARNING PW%03d: ", sym->filename, sym->line, errnum);
                vfprintf(error_file, what->message, args);
                fprintf(error_file, "\n");
            }
            if(what->severity >= SEVERITY_EXIT) {
                if(what->severity >= SEVERITY_DUMP) {
                    abort();
                } else {
                    exit(EXPRESS_fail((Express)0));
                }
            }
        }
    }
    va_end(args);
}

void ERRORnospace()
{
    fprintf(stderr, "%s: out of space\n", EXPRESSprogram_name);
    ERRORabort(0);
}

/** \fn ERRORbuffer_messages
** \param flag    - to buffer or not to buffer
** Selects buffering of error messages
** \note this function is inlined in error.h
*/

/** \fn ERRORflush_messages
** Flushes the error message buffer to standard output.
** \note this function is inlined in error.h
**
** \note The error messages are sorted by line number (which appears in the third column).
*/

void ERROR_start_message_buffer(void)
{
    ERROR_string = ERROR_string_base;
    ERROR_with_lines = 0;
}

void ERROR_flush_message_buffer(void)
{
    if(!__ERROR_buffer_errors) {
        return;
    }

    while(ERROR_with_lines) {
        struct heap_element *replace;
        int parent, child;

        /* pop off the top of the heap */
        fprintf(stderr, "%s", heap[1].msg);

        replace = &heap[ERROR_with_lines--];

        child = 1;
        while(1) {
            parent = child;
            child = 2 * parent;
            if(child > ERROR_with_lines) {
                break;
            }
            if(child + 1 <= ERROR_with_lines) {
                if(heap[child].line > heap[child + 1].line) {
                    child++;
                }
            }
            if(replace->line <= heap[child].line) {
                break;
            }
            heap[parent] = heap[child];
        }
        heap[parent] = *replace;
    }
}

void ERRORabort(int sig)
{
    (void) sig; /* quell unused param warning */

    /* TODO: rework - fprintf is not atomic
     * so ERRORflush_messages() is unsafe if __ERROR_buffer_errors is set
     */

    /* NOTE: signals can be caught in gdb,
     * no need for special treatment of debugging scenario */
    if(ERROR_unsafe) {
        longjmp(ERROR_safe_env, 1);
    }
#ifdef SIGABRT
    signal(SIGABRT, SIG_DFL);
#endif
    /* TODO: library shouldn't abort an application? */
    abort();
}

void ERRORsafe(jmp_buf env)
{
    memcpy(ERROR_safe_env, env, sizeof(jmp_buf));
}

void ERRORunsafe()
{
    ERROR_unsafe = true;
}
