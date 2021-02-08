#include <sc_memmgr.h>
#include <stdlib.h>
#include <stdio.h>
#include <memory.h>
#include <errno.h>
#include <stdarg.h>
#include <float.h>
#include <assert.h>
#include <locale.h>

#include <express/expbasic.h>
#include <express/express.h>
#include "pp.h"
#include "exppp.h"


#if defined(_MSC_VER) && _MSC_VER < 1900
#  include "sc_stdio.h"
#  define snprintf c99_snprintf
#endif

/* PP_SMALL_BUF_SZ is a macro used in a few places where const int causes
 * "warning: ISO C90 forbids variable length array 'buf' [-Wvla]"
 *
 * The name was chosen to be unique to exppp, so it should not be defined
 * already. Define it, use it, and undefine it so it can be set to another
 * value elsewhere.
 */
#ifdef PP_SMALL_BUF_SZ
#  error "PP_SMALL_BUF_SZ already defined"
#endif

void ALGscope_out(Scope s, int level);
void ENTITYattrs_out(Linked_List attributes, int derived, int level);
void ENTITY_out(Entity e, int level);
void ENTITYinverse_out(Linked_List attrs, int level);
void ENTITYunique_out(Linked_List u, int level);
void FUNC_out(Function fn, int level);
void PROC_out(Procedure p, int level);
void REFout(Dictionary refdict, Linked_List reflist, char *type, int level);
void RULE_out(Rule r, int level);
void SCOPEalgs_out(Scope s, int level);
void SCOPEconsts_out(Scope s, int level);
void SCOPEentities_out(Scope s, int level);
void SCOPElocals_out(Scope s, int level);
void SCOPEtypes_out(Scope s, int level);
void STMT_out(Statement s, int level);
void STMTlist_out(Linked_List stmts, int level);
void TYPE_out(Type t, int level);
void TYPE_head_out(Type t, int level);
void TYPE_body_out(Type t, int level);
void WHERE_out(Linked_List wheres, int level);

Error ERROR_select_empty;

const int exppp_nesting_indent = 2;       /* default nesting indent */
const int exppp_continuation_indent = 4;  /* default nesting indent for continuation lines */
int exppp_linelength = 130;                /* leave some room for closing parens.
                                           * '\n' is not included in this count either */
int indent2;                              /* where continuation lines start */
int curpos;                               /* current line position (1 is first position) */
const int NOLEVEL = -1;


Symbol error_sym;                           /* only used when printing errors */


bool exppp_output_filename_reset = true;    /* if true, force output filename */
bool exppp_print_to_stdout = false;
bool exppp_alphabetize = true;
bool exppp_aggressively_wrap_consts = false;
bool exppp_terse = false;
bool exppp_reference_info = false;   /* if true, add commentary about where things came from */
bool exppp_tail_comment = false;

FILE *exppp_fp = NULL;          /* output file */
char *exppp_buf = 0;            /* output buffer */
int exppp_maxbuflen = 0;        /* size of expppbuf */
unsigned int exppp_buflen = 0;  /* remaining space in expppbuf */
char *exppp_bufp = 0;          /* pointer to write position in expppbuf,
                                 * should usually be pointing to a "\0" */

/** used to print a comment containing the name of a  structure at the
 * end of the structure's declaration, if exppp_tail_comment (-t) is true
 *
 * prints a newline regardless
 */
void tail_comment(const char *name)
{
    if(exppp_tail_comment) {
        raw(" -- %s", name);
    }
    raw("\n");
}

/** count newlines in a string */
int count_newlines(char *s)
{
    int count = 0;
    for(; *s; s++) {
        if(*s == '\n') {
            count++;
        }
    }
    return count;
}

/** true if last char through exp_output was a space */
static bool printedSpaceLast = false;

void exp_output(char *buf, unsigned int len)
{
    FILE *fp = (exppp_fp ? exppp_fp : stdout);

    error_sym.line += count_newlines(buf);
    printedSpaceLast = (*(buf + len - 1) == ' ');
    if(exppp_buf) {
        /* output to string */
        if(len > exppp_buflen) {
            /* should provide flag to enable complaint */
            /* for now, just ignore */
            return;
        }
        memcpy(exppp_bufp, buf, len + 1);
        exppp_bufp += len;
        exppp_buflen -= len;
    } else {
        /* output to file */
        size_t out = fwrite(buf, 1, len, fp);
        if(out != len) {
            const char *err = "%s:%u - ERROR: write operation on output file failed. Wanted %u bytes, wrote %u.";
            fprintf(stderr, err, __FILE__, __LINE__, len, out);
            abort();
        }
    }
}

void wrap(const char *fmt, ...)
{
    char buf[10000];
    char *p, * start = buf;
    int len;
    va_list args;

    va_start(args, fmt);
    vsprintf(buf, fmt, args);
    va_end(args);

    len = strlen(buf);

    /* eliminate leading whitespace */
    while((*start == ' ') && ((printedSpaceLast) || (*(start + 1) == ' '))) {
        start++;
        len--;
    }

    /* 1st condition checks if string can't fit into current line
     * 2nd condition checks if string can fit into any line
     * I.e., if we still can't fit after indenting, don't bother to
     * go to newline, just print a long line
     * 3rd condition: if exppp_linelength == indent2 and curpos > indent2, always newline
     * to use #3: temporarily change exppp_linelength; it doesn't make sense to change indent2
     */
    if((((curpos + len) > exppp_linelength) && ((indent2 + len) < exppp_linelength))
            || ((exppp_linelength == indent2) && (curpos > indent2))) {
        /* move to new continuation line */
        char line[1000];
        sprintf(line, "\n%*s", indent2, "");
        exp_output(line, 1 + indent2);

        curpos = indent2;       /* reset current position */
    }

    /* eliminate leading whitespace  - again */
    while((*start == ' ') && ((printedSpaceLast) || (*(start + 1) == ' '))) {
        start++;
        len--;
    }
    exp_output(start, len);

    if(len) {
        /* reset cur position based on last newline seen */
        if(0 == (p = strrchr(start, '\n'))) {
            curpos += len;
        } else {
            curpos = len + start - p;
        }
    }
}

void raw(const char *fmt, ...)
{
    char *p;
    char buf[10000];
    int len;
    va_list args;

    va_start(args, fmt);
    vsprintf(buf, fmt, args);
    va_end(args);

    len = strlen(buf);

    exp_output(buf, len);

    if(len) {
        /* reset cur position based on last newline seen */
        if(0 == (p = strrchr(buf, '\n'))) {
            curpos += len;
        } else {
            curpos = len + buf - p;
        }
    }
}

void exppp_init()
{
    static bool first_time = true;

    if(!first_time) {
        return;
    }
    first_time = false;
}


void exppp_ref_info(Symbol *s)
{
    if(exppp_reference_info) {
        raw("--info %s %s %d\n", s->name, s->filename, s->line);
    }
}

/** normally all non-schema objects start out by printing a newline
 * however, this is undesirable when printing out single objects
 * use this variable to avoid it
 */
bool first_line = true;       /* if first line */

void first_newline()
{
    if(first_line) {
        first_line = false;
    } else {
        raw("\n");
    }
}

int minimum(int a, int b, int c)
{
    if(a < b) {
        return ((a < c) ? a : c);
    } else {
        return ((b < c) ? b : c);
    }
}

/** convert a real into our preferred form compatible with 10303-11
 * (i.e. decimal point is required; no trailing zeros)
 * uses a static buffer, so NOT thread safe
 * \param r the real to convert
 * \returns const char pointer to static buffer containing ascii representation of real
 */
const char *real2exp(double r)
{
#define PP_SMALL_BUF_SZ 80
    static char result[PP_SMALL_BUF_SZ] = { 0 };
    char *pos = result, * lcNumeric = setlocale(LC_NUMERIC, NULL);

    /* the following ensures that PP_SMALL_BUF_SZ is at least
     * as big as the largest possible string:
     *   max number of decimal digits a double can represent
     *   + max number of exponent digits + '.' + 'E' + NULL
     * start at 2 to include '-' and last digit
     *
     * the number of digits in the mantissa is DBL_DIG
     * exponentDigits must be calculated from DBL_MAX_10_EXP
     *
     * I (MP) expect this check to always succeed on
     * non-exotic platforms.
     */
    unsigned int exponentDigits = 2, expMax = DBL_MAX_10_EXP;
    while(expMax >= 10) {
        exponentDigits++;
        expMax /= 10;
    }
    if(!((DBL_DIG + exponentDigits + 3) < PP_SMALL_BUF_SZ)) {
        fprintf(stderr, "ERROR: buffer undersized at %s:%d\n", __FILE__, __LINE__);
        abort();
    }

    if(strcmp("C", lcNumeric)) {
        fprintf(stderr, "WARNING: locale has been set to \"%s\", not \"C\" %s", lcNumeric,
                "(are you calling exppp from Qt?). Incorrect formatting is possible.\n");
        setlocale(LC_NUMERIC, "C");
    }
    snprintf(result, PP_SMALL_BUF_SZ, "%#.*g", DBL_DIG, r);

    /* eliminate trailing zeros in the mantissa */
    assert(strlen(result) < PP_SMALL_BUF_SZ - 1);
    while((*pos != '.') && (*pos != '\0')) {
        /* search for '.' */
        pos++;
    }
    if(*pos != '\0') {
        char *firstUnnecessaryDigit = NULL;  /* this will be the first zero of the trailing zeros in the mantissa */
        pos++;
        while(isdigit(*pos)) {
            if((*pos == '0') && (firstUnnecessaryDigit == NULL)) {
                firstUnnecessaryDigit = pos;
            } else if(*pos != '0') {
                firstUnnecessaryDigit = NULL;
            }
            pos++;
        }
        if((firstUnnecessaryDigit != NULL) && (firstUnnecessaryDigit < pos)) {
            if((*(firstUnnecessaryDigit - 1) == '.') && (*pos == '\0')) {
                /* no exponent, nothing after decimal point - remove decimal point */
                *(firstUnnecessaryDigit - 1) = '\0';
            } else {
                /* copy exponent (or \0) immediately after the decimal point */
                memmove(firstUnnecessaryDigit, pos, strlen(pos) + 1);
            }
        }
    }
    assert(strlen(result) < PP_SMALL_BUF_SZ - 1);
    return result;
#undef PP_SMALL_BUF_SZ
}

/** Find next '.' in null-terminated string, return number of chars
 *  If no '.' found, returns length of string
 */
int nextBreakpoint(const char *pos, const char *end)
{
    int i = 0;
    while((*pos != '.') && (*pos != '\0') && (pos < end)) {
        i++;
        pos++;
    }
    if(*pos == '.') {
        i++;
    }
    return i;
}

/** true if it makes sense to break before printing next part of the string */
bool shouldBreak(int len)
{
    if((curpos > indent2) &&
            ((curpos + len) > exppp_linelength)) {
        return true;
    }
    return false;
}

/** Insert newline if it makes sense. */
void maybeBreak(int len, bool first)
{
    if(shouldBreak(len)) {
        if(first) {
            raw("\n%*s'", indent2, "");
        } else {
            raw("'\n%*s+ '", indent2, "");
        }
    } else if(first) {
        /* staying on same line */
        raw("%s", (printedSpaceLast ? "'" : " '"));
    }
}

/** Break a long un-encoded string up for output and enclose in ''
 * if short, enclose in '' but don't insert line breaks
 * \param in the input string
 *
 * side effects: output via raw()
 * reads globals indent2 and curpos
 */
void breakLongStr(const char *in)
{
    const char *iptr = in, * end;
    unsigned int inlen = strlen(in);
    bool first = true;
    /* used to ensure that we don't overrun the input buffer */
    end = in + inlen;

    if((inlen == 0) || (((int) inlen + curpos) < exppp_linelength)) {
        /* short enough to fit on current line */
        raw("%s'%s'", (printedSpaceLast ? "" : " "), in);
        return;
    }

    /* insert newlines at dots as necessary */
    while((iptr < end) && (*iptr)) {
        int i = nextBreakpoint(iptr, end);
        maybeBreak(i, first);
        first = false;
        raw("%.*s", i, iptr);
        iptr += i;
    }
    raw("' ");
}

/* Interfacing Definitions */

#define BIGBUFSIZ   100000
static int old_curpos;
static int old_lineno;
static bool string_func_in_use = false;
static bool file_func_in_use = false;

/** return 0 if successful */
int prep_buffer(char *buf, int len)
{
    /* this should never happen */
    if(string_func_in_use) {
        fprintf(stderr, "cannot generate EXPRESS string representations recursively!\n");
        return 1;
    }
    string_func_in_use = true;

    exppp_buf = exppp_bufp = buf;
    exppp_buflen = exppp_maxbuflen = len;

    *exppp_bufp = '\0';
    old_curpos = curpos;
    curpos = 1;
    old_lineno = 1;

    first_line = true;

    return 0;
}

/** \return length of string */
int finish_buffer()
{
    exppp_buf = 0;
    curpos = old_curpos;
    error_sym.line = old_lineno;
    string_func_in_use = false;
    return 1 + exppp_maxbuflen - exppp_buflen;
}

/** \return 0 if successful */
int prep_string()
{
    /* this should never happen */
    if(string_func_in_use) {
        fprintf(stderr, "cannot generate EXPRESS string representations recursively!\n");
        return 1;
    }
    string_func_in_use = true;

    exppp_buf = exppp_bufp = (char *)sc_malloc(BIGBUFSIZ);
    if(!exppp_buf) {
        fprintf(stderr, "failed to allocate exppp buffer\n");
        return 1;
    }
    exppp_buflen = exppp_maxbuflen = BIGBUFSIZ;

    *exppp_bufp = '\0';
    old_curpos = curpos;
    old_lineno = error_sym.line;
    curpos = 1;

    first_line = true;

    return 0;
}

char *finish_string()
{
    char *b = (char *)sc_realloc(exppp_buf, 1 + exppp_maxbuflen - exppp_buflen);

    if(b == 0) {
        fprintf(stderr, "failed to reallocate exppp buffer\n");
        return 0;
    }
    exppp_buf = 0;
    curpos = old_curpos;
    error_sym.line = old_lineno;

    string_func_in_use = false;
    return b;
}

static FILE *oldfp;

void prep_file()
{
    /* this can only happen if user calls output func while suspended */
    /* inside another output func both called from debugger */
    if(file_func_in_use) {
        fprintf(stderr, "cannot print EXPRESS representations recursively!\n");
    }
    file_func_in_use = true;

    /* temporarily change file to stdout and print */
    /* This avoids messing up any printing in progress */
    oldfp = exppp_fp ? exppp_fp : stdout;
    exppp_fp = stdout;
    curpos = 1;
}

void finish_file()
{
    exppp_fp = oldfp;       /* reset back to original file */
    file_func_in_use = false;
}

char *placeholder = "placeholder";



