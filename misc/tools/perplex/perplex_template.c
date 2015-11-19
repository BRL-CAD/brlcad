/*              P E R P L E X _ T E M P L A T E . C
 * BRL-CAD
 *
 * Copyright (c) 1990-2011 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * Copyright (c) 1990 The Regents of the University of California.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following
 * disclaimer in the documentation and/or other materials provided
 * with the distribution.
 *
 * 3. The name of the author may not be used to endorse or promote
 * products derived from this software without specific prior written
 * permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
/*  Parts of this file are based on sources from the flex project.
 *
 *  This code is derived from software contributed to Berkeley by
 *  Vern Paxson.
 *
 *  The United States Government has rights in this work pursuant
 *  to contract no. DE-AC03-76SF00098 between the United States
 *  Department of Energy and the University of California.
 */
/** @file perplex_template.c
 *
 * template for generated scanners
 *
 */
#include <stdio.h>
#include <stdlib.h>

#define YYEOF -1

struct Buf {
    void   *elts;	/* elements. */
    int     nelts;	/* number of elements. */
    size_t  elt_size;	/* in bytes. */
    int     nmax;	/* max capacity of elements. */
};

/* scanner data */
typedef struct perplex {
    void *extra;        /* application data */
    FILE *inFile;
    struct Buf *buffer;
    char *tokenText;
    char *tokenStart;
    char *cursor;
    char *marker;
    char *null;
    int atEOI;
    int condition;
} *perplex_t;

perplex_t perplexFileScanner(FILE *input);
perplex_t perplexStringScanner(char *firstChar, size_t numChars);
void perplexFree(perplex_t scanner);

void perplexUnput(perplex_t scanner, char c);
void perplexSetExtra(perplex_t scanner, void *extra);
void* perplexGetExtra(perplex_t scanner);

#ifndef PERPLEX_LEXER
#define PERPLEX_LEXER yylex
#endif

#define PERPLEX_PUBLIC_LEXER  PERPLEX_LEXER(perplex_t scanner)
#define PERPLEX_PRIVATE_LEXER PERPLEX_LEXER_private(perplex_t scanner)

#ifndef PERPLEX_ON_ENTER
#define PERPLEX_ON_ENTER /* do nothing */
#endif

int PERPLEX_PUBLIC_LEXER;

%%
/*              S C A N N E R _ T E M P L A T E . C
 * BRL-CAD
 *
 * Copyright (c) 1990-2011 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * Copyright (c) 1990 The Regents of the University of California.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following
 * disclaimer in the documentation and/or other materials provided
 * with the distribution.
 *
 * 3. The name of the author may not be used to endorse or promote
 * products derived from this software without specific prior written
 * permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
/*  Parts of this file are based on sources from the flex project.
 *
 *  This code is derived from software contributed to Berkeley by
 *  Vern Paxson.
 *
 *  The United States Government has rights in this work pursuant
 *  to contract no. DE-AC03-76SF00098 between the United States
 *  Department of Energy and the University of California.
 */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* --- from flex's flexdef.h --- */
void buf_init(struct Buf * buf, size_t elem_size);
void buf_destroy(struct Buf * buf);
struct Buf *buf_append(struct Buf * buf, const void *ptr, int n_elem);
struct Buf *buf_concat(struct Buf* dest, const struct Buf* src);
struct Buf *buf_strappend(struct Buf *, const char *str);
struct Buf *buf_strnappend(struct Buf *, const char *str, int nchars);
struct Buf *buf_strdefine(struct Buf * buf, const char *str, const char *def);
struct Buf *buf_prints(struct Buf *buf, const char *fmt, const char* s);
struct Buf *buf_m4_define(struct Buf *buf, const char* def, const char* val);
struct Buf *buf_m4_undefine(struct Buf *buf, const char* def);
struct Buf *buf_print_strings(struct Buf * buf, FILE* out);
struct Buf *buf_linedir(struct Buf *buf, const char* filename, int lineno);

/* --- from flex's misc.c --- */
static void*
allocate_array(int size, size_t element_size)
{
    return malloc(element_size * size);
}

static void*
reallocate_array(void *array, int size, size_t element_size)
{
    return realloc(array, element_size * size);
}

/* --- from flex's buf.c --- */
/* Take note: The buffer object is sometimes used as a String buffer (one
 * continuous string), and sometimes used as a list of strings, usually line by
 * line.
 * 
 * The type is specified in buf_init by the elt_size. If the elt_size is
 * sizeof(char), then the buffer should be treated as string buffer. If the
 * elt_size is sizeof(char*), then the buffer should be treated as a list of
 * strings.
 *
 * Certain functions are only appropriate for one type or the other. 
 */

struct Buf*
buf_print_strings(struct Buf * buf, FILE* out)
{
    int i;

    if(!buf || !out) {
        return buf;
    }

    for (i = 0; i < buf->nelts; i++) {
        const char *s = ((char**)buf->elts)[i];
        if(s) {
            fprintf(out, "%s", s);
	}
    }
    return buf;
}

/* Append a "%s" formatted string to a string buffer */
struct Buf*
buf_prints(struct Buf *buf, const char *fmt, const char *s)
{
    char *t;

    t = (char*)malloc(strlen(fmt) + strlen(s) + 1);
    sprintf(t, fmt, s);
    buf = buf_strappend(buf, t);
    free(t);
    return buf;
}

int numDigits(int n)
{
    int digits;

    /* take absolute value of n */
    n = n >= 0 ? n : -n;

    if (n == 0) {
	return 1;
    }

    for (digits = 0; n > 0; digits++) {
	n /= 10;
    }

    return digits;
}

/** Append a line directive to the string buffer.
 * @param buf A string buffer.
 * @param filename file name
 * @param lineno line number
 * @return buf
 */
struct Buf*
buf_linedir(struct Buf *buf, const char* filename, int lineno)
{
    char *t;
    const char fmt[] = "#line %d \"%s\"\n";
    
    t = (char*)malloc(strlen(fmt) + strlen(filename) + numDigits(lineno) + 1);
    sprintf(t, fmt, lineno, filename);
    buf = buf_strappend(buf, t);
    free(t);
    return buf;
}


/** Append the contents of @a src to @a dest.
 * @param @a dest the destination buffer
 * @param @a dest the source buffer
 * @return @a dest
 */
struct Buf*
buf_concat(struct Buf* dest, const struct Buf* src)
{
    buf_append(dest, src->elts, src->nelts);
    return dest;
}


/* Appends n characters in str to buf. */
struct Buf*
buf_strnappend(struct Buf *buf, const char *str, int n)
{
    buf_append(buf, str, n + 1);

    /* "undo" the '\0' character that buf_append() already copied. */
    buf->nelts--;

    return buf;
}

/* Appends characters in str to buf. */
struct Buf*
buf_strappend(struct Buf *buf, const char *str)
{
    return buf_strnappend(buf, str, strlen(str));
}

/* appends "#define str def\n" */
struct Buf*
buf_strdefine(struct Buf *buf, const char *str, const char *def)
{
    buf_strappend(buf, "#define ");
    buf_strappend(buf, " ");
    buf_strappend(buf, str);
    buf_strappend(buf, " ");
    buf_strappend(buf, def);
    buf_strappend(buf, "\n");
    return buf;
}

/** Pushes "m4_define( [[def]], [[val]])m4_dnl" to end of buffer.
 * @param buf A buffer as a list of strings.
 * @param def The m4 symbol to define.
 * @param val The definition; may be NULL.
 * @return buf
 */
struct Buf*
buf_m4_define(struct Buf *buf, const char* def, const char* val)
{
    const char *fmt = "m4_define( [[%s]], [[%s]])m4_dnl\n";
    char *str;

    val = val ? val : "";
    str = (char*)malloc(strlen(fmt) + strlen(def) + strlen(val) + 2);

    sprintf(str, fmt, def, val);
    buf_append(buf, &str, 1);
    return buf;
}

/** Pushes "m4_undefine([[def]])m4_dnl" to end of buffer.
 * @param buf A buffer as a list of strings.
 * @param def The m4 symbol to undefine.
 * @return buf
 */
struct Buf*
buf_m4_undefine(struct Buf *buf, const char* def)
{
    const char *fmt = "m4_undefine( [[%s]])m4_dnl\n";
    char *str;

    str = (char*)malloc(strlen(fmt) + strlen(def) + 2);

    sprintf(str, fmt, def);
    buf_append(buf, &str, 1);
    return buf;
}

/* create buf with 0 elements, each of size elem_size. */
void
buf_init(struct Buf *buf, size_t elem_size)
{
    buf->elts = (void*)0;
    buf->nelts = 0;
    buf->elt_size = elem_size;
    buf->nmax = 0;
}

/* frees memory */
void
buf_destroy(struct Buf *buf)
{
    if (buf && buf->elts) {
	free(buf->elts);
    }
    if (buf)
	buf->elts = (void*)0;
}

/* appends ptr[] to buf, grow if necessary.
 * n_elem is number of elements in ptr[], NOT bytes.
 * returns buf.
 * We grow by mod(512) boundaries.
 */
struct Buf*
buf_append(struct Buf *buf, const void *ptr, int n_elem)
{
    int n_alloc = 0;

    if (!ptr || n_elem == 0) {
	return buf;
    }

    /* May need to alloc more. */
    if (n_elem + buf->nelts > buf->nmax) {
	/* exact amount needed... */
	n_alloc = (n_elem + buf->nelts) * buf->elt_size;

	/* ...plus some extra */
	if (((n_alloc * buf->elt_size) % 512) != 0 && buf->elt_size < 512) {
	    n_alloc += (512 - ((n_alloc * buf->elt_size) % 512)) / buf->elt_size;
	}
	if (!buf->elts) {
	    buf->elts = allocate_array(n_alloc, buf->elt_size);
	} else {
	    buf->elts = reallocate_array(buf->elts, n_alloc, buf->elt_size);
	}
	buf->nmax = n_alloc;
    }
    memcpy((char*)buf->elts + buf->nelts * buf->elt_size, ptr,
	n_elem * buf->elt_size);

    buf->nelts += n_elem;

    return buf;
}

/* --- */
/* input buffering support
 * note that these routines assume buf->elt_size == sizeof(char)
 */

/* get pointer to the start of the first element */
static char*
bufferFirstElt(struct Buf *buf)
{
    return (char*)buf->elts;
}

/* get pointer to the start of the last element */
static char*
bufferLastElt(struct Buf *buf)
{
    if (buf->nelts < 1) {
	return NULL;
    }
    return bufferFirstElt(buf) + buf->nelts - 1;
}

static void
bufferAppendChar(struct Buf *buf, char c)
{
    char *cp = &c;
    buf_append(buf, cp, 1);
}

/* Copy up to n input characters to the end of scanner buffer. If EOF is
 * encountered before n characters are copied, scanner->atEOI flag is set.
 */
static void
bufferAppend(perplex_t scanner, size_t n)
{
    struct Buf *buf;
    FILE *in;
    size_t i;
    int c;
    char *bufStart;
    size_t markerOffset, tokenStartOffset, cursorOffset;

    buf = scanner->buffer;
    in = scanner->inFile;

    /* save marker offsets */
    bufStart = (char*)buf->elts;
    cursorOffset = (size_t)(scanner->cursor - bufStart);
    markerOffset = (size_t)(scanner->marker - bufStart);
    tokenStartOffset = (size_t)(scanner->tokenStart - bufStart);

    /* remove last (null) element */
    buf->nelts--;

    for (i = 0; i < n; i++) {
	if ((c = fgetc(in)) == EOF) {
	    scanner->atEOI = 1;
	    break;
	}
	bufferAppendChar(buf, c);
    }

    /* (scanner->null - eltSize) should be the last input element,
     * we put a literal null after this element for debugging
     */
    bufferAppendChar(buf, '\0');
    scanner->null = bufferLastElt(buf);

    /* update markers in case append caused buffer to be reallocated */
    bufStart = (char*)buf->elts;
    scanner->cursor = bufStart + cursorOffset;
    scanner->marker = bufStart + markerOffset;
    scanner->tokenStart = bufStart + tokenStartOffset;
}

/* Appends up to n characters of input to scanner buffer. */
static void
bufferFill(perplex_t scanner, size_t n)
{
    struct Buf *buf;
    size_t totalElts, usedElts, freeElts;

    if (scanner->atEOI) {
	/* nothing to add to buffer */
	return;
    }

    buf = scanner->buffer;

    totalElts = (size_t)buf->nmax;
    usedElts = (size_t)buf->nelts;
    freeElts = totalElts - usedElts;

    /* not enough room for append, shift buffer contents to avoid realloc */
    if (n > freeElts) {
	void *bufFirst, *scannerFirst, *tokenStart, *marker, *null;
	size_t bytesInUse, shiftSize;

	tokenStart = (void*)scanner->tokenStart;
	marker = (void*)scanner->marker;
	null = (void*)scanner->null;

	bufFirst = bufferFirstElt(buf);

	/* Find first buffer element still in use by scanner. Will be
	 * tokenStart unless backtracking marker is in use.
	 */
	scannerFirst = tokenStart;
	if (marker >= bufFirst && marker < tokenStart) {
	    scannerFirst = marker;
	}

	/* bytes of input being used by scanner */
	bytesInUse = (size_t)null - (size_t)scannerFirst + 1;

	/* copy in-use elements to start of buffer */
	memmove(bufFirst, scannerFirst, bytesInUse);

	/* update number of elements */
        buf->nelts = bytesInUse / buf->elt_size;

	/* update markers */
	shiftSize = (size_t)scannerFirst - (size_t)bufFirst;
	scanner->marker     -= shiftSize;
	scanner->cursor     -= shiftSize;
	scanner->null       -= shiftSize;
	scanner->tokenStart -= shiftSize;
    }
    bufferAppend(scanner, n);
}

char*
getTokenText(perplex_t scanner)
{
    int tokenChars = scanner->cursor - scanner->tokenStart;

    if (scanner->tokenText != NULL) {
	free(scanner->tokenText);
    }

    scanner->tokenText = (char*)malloc(sizeof(char) * (tokenChars + 1));

    memcpy(scanner->tokenText, scanner->tokenStart, tokenChars);
    scanner->tokenText[tokenChars] = '\0';

    return scanner->tokenText;
}

/* scanner helpers */
#define UPDATE_START  scanner->tokenStart = scanner->cursor;
#define IGNORE_TOKEN  UPDATE_START; continue;
#define       yytext  getTokenText(scanner)
#define      yyextra  scanner->extra

static perplex_t
newScanner()
{
    perplex_t scanner;
    scanner = (perplex_t)calloc(1, sizeof(struct perplex));

    return scanner;
}

static void
initBuffer(perplex_t scanner)
{
    scanner->buffer = (struct Buf*)malloc(sizeof(struct Buf));
    buf_init(scanner->buffer, sizeof(char));
}

/* public functions */

perplex_t
perplexStringScanner(char *firstChar, size_t numChars)
{
    size_t i;
    struct Buf *buf;
    perplex_t scanner = newScanner();

    scanner->inFile = NULL;

    initBuffer(scanner);
    buf = scanner->buffer;

    /* copy string to buffer */
    for (i = 0; i < numChars; i++) {
	bufferAppendChar(buf, firstChar[i]);
    }
    bufferAppendChar(buf, '\0');

    scanner->marker = scanner->cursor = bufferFirstElt(buf);
    scanner->null = bufferLastElt(buf);
    scanner->atEOI = 1;

    return scanner;
}

perplex_t
perplexFileScanner(FILE *input)
{
    char *bufFirst;
    perplex_t scanner = newScanner();

    scanner->inFile = input;

    initBuffer(scanner);
    bufferAppendChar(scanner->buffer, '\0');

    bufFirst = bufferFirstElt(scanner->buffer);
    scanner->null = scanner->marker = scanner->cursor = bufFirst;

    return scanner;
}

void
perplexFree(perplex_t scanner)
{
    if (scanner->buffer != NULL) {
	buf_destroy(scanner->buffer);
	free(scanner->buffer);
    }

    free(scanner);
}

void
perplexSetExtra(perplex_t scanner, void *extra)
{
    scanner->extra = extra;
}

void*
perplexGetExtra(perplex_t scanner)
{
    return scanner->extra;
}

/* Make c the next character to be scanned.
 *
 * This performs an insert, leaving the input buffer prior to the cursor
*  unchanged.
 */
void
perplexUnput(perplex_t scanner, char c)
{
    struct Buf *buf;
    char *curr, *cursor, *bufStart;
    size_t markerOffset, tokenStartOffset, cursorOffset;

    buf = scanner->buffer;

    /* save marker offsets */
    bufStart = bufferFirstElt(buf);
    cursorOffset = (size_t)(scanner->cursor - bufStart);
    markerOffset = (size_t)(scanner->marker - bufStart);
    tokenStartOffset = (size_t)(scanner->tokenStart - bufStart);

    /* append character to create room for shift */
    bufferAppendChar(buf, '\0');
    scanner->null = bufferLastElt(buf);

    /* update markers in case append caused buffer to be reallocated */
    bufStart = bufferFirstElt(buf);
    scanner->cursor = bufStart + cursorOffset;
    scanner->marker = bufStart + markerOffset;
    scanner->tokenStart = bufStart + tokenStartOffset;

    /* input from cursor to null is shifted to the right */
    cursor = scanner->cursor;
    for (curr = scanner->null; curr != cursor; curr--) {
	curr[0] = curr[-1];
    }

    /* insert c */
    *curr = c;
}

#ifdef PERPLEX_USING_CONDITIONS
/* start-condition support */
static void
setCondition(perplex_t scanner, int cond)
{
    scanner->condition = cond;
}

static int
getCondition(perplex_t scanner)
{
    return scanner->condition;
}

/* required re2c macros */
#define YYGETCONDITION     getCondition(scanner)
#define YYSETCONDITION(c)  setCondition(scanner, c)
#endif

/* required re2c macro */
#define YYFILL(n)          bufferFill(scanner, n)

/* scanner */
static int PERPLEX_PRIVATE_LEXER;

int
PERPLEX_PUBLIC_LEXER {
    int ret;

    UPDATE_START;

    scanner->tokenText = NULL;

    ret = PERPLEX_LEXER_private(scanner);

    if (scanner->tokenText != NULL) {
	free(scanner->tokenText);
	scanner->tokenText = NULL;
    }

    return ret;
}

static int
PERPLEX_PRIVATE_LEXER {
    char yych;

    PERPLEX_ON_ENTER;

    while (1) {
	if (scanner->atEOI && scanner->cursor >= scanner->null) {
	    return YYEOF;
	}
/*!re2c
re2c:yych:emit = 0;
re2c:define:YYCTYPE  = char;
re2c:define:YYCURSOR = scanner->cursor;
re2c:define:YYLIMIT  = scanner->null;
re2c:define:YYMARKER = scanner->marker;
