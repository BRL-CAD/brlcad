/*                        S C A N N E R . R E
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
/** @file scanner.re
 *
 * The perplex input scanner. Converts flex-style input to re2c input.
 * Usage:
 *     int tokenID;
 *     perplex_t scanner;
 *      ...
 *     perplexFileScanner(scanner, inFile);
 *     while ((tokenID = yylex(scanner)) != YYEOF) {
 *      ...
 *     }
 *     perplexFree(scanner);
 *     fclose(inFile);
 */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "perplex.h"
#include "token_type.h"

static void*
copyString(const char *str)
{
    void *copy;
    size_t numChars = strlen(str) + 1;

    copy = malloc(numChars * sizeof(char));
    strncpy(copy, str, numChars);

    return copy;
}
#include <math.h>
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
        const char * s = ((char**)buf->elts)[i];
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

    t = malloc(strlen(fmt) + strlen(s) + 1);
    sprintf(t, fmt, s);
    buf = buf_strappend(buf, t);
    free(t);
    return buf;
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
    char   *t, *fmt = "#line %d \"%s\"\n";
    
    t = malloc(strlen(fmt) + strlen(filename) + (int)(1 + log10(lineno >= 0? lineno : -lineno)) + 1);
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
    const char * fmt = "m4_define( [[%s]], [[%s]])m4_dnl\n";
    char * str;

    val = val?val:"";
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
    const char * fmt = "m4_undefine( [[%s]])m4_dnl\n";
    char * str;

    str = (char*)malloc(strlen(fmt) + strlen(def) + 2);

    sprintf(str, fmt, def);
    buf_append(buf, &str, 1);
    return buf;
}

/* create buf with 0 elements, each of size elem_size. */
void
buf_init(struct Buf *buf, size_t elem_size)
{
    buf->elts = (void *) 0;
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
    buf->elts = (void *) 0;
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
/* input buffering support */

static void
buf_append_null(struct Buf *buf)
{
    char null = '\0';
    buf_append(buf, &null, sizeof(char));
}

/* get pointer to the start of the first element */
static void*
buf_first_elt(struct Buf *buf)
{
    return buf->elts;
}

/* get pointer to the start of the last element */
static void*
buf_last_elt(struct Buf *buf)
{
    size_t first = (size_t)buf_first_elt(buf);

    if (buf->nelts < 1) {
	return NULL;
    }

    return (void*)(first + buf->elt_size * (buf->nelts - 1));
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
    char c;

    buf = scanner->buffer;
    in = scanner->in.file;

    /* remove last (null) element */
    buf->nelts--;

    for (i = 0; i < n; i++) {
	if ((c = fgetc(in)) == EOF) {
	    scanner->atEOI = 1;
	    break;
	}
	buf_append(buf, &c, sizeof(char));
    }

    /* (scanner->null - eltSize) should be the last input element,
     * we put a literal null after this element for debugging
     */
    buf_append_null(buf);
    scanner->null = (char*)buf_last_elt(buf);
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

	bufFirst = buf_first_elt(buf);

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

static char*
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

static perplex_t
newScanner()
{
    perplex_t scanner;
    scanner = (perplex_t)calloc(1, sizeof(struct perplex));

    scanner->in.file = NULL;
    scanner->in.string = NULL;

    scanner->atEOI = 0;

    scanner->cursor = scanner->marker = scanner->null = NULL;

    scanner->tokenStart = NULL;
    scanner->buffer = NULL;
    scanner->tokenText = NULL;
    scanner->appData = NULL;

    scanner->condition = 0;

    return scanner;
}

/* public functions */

perplex_t
perplexFileScanner(FILE *input)
{
    perplex_t scanner = newScanner();

    scanner->in.file = input;
    scanner->in.string = NULL;

    scanner->buffer = (struct Buf*)malloc(sizeof(struct Buf));
    buf_init(scanner->buffer, sizeof(char));
    buf_append_null(scanner->buffer);

    scanner->null = scanner->marker = scanner->cursor = scanner->buffer->elts;

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

/* start-condition support */

static void
setCondition(perplex_t scanner, condition_t cond)
{
    scanner->condition = cond;
}

static condition_t
getCondition(perplex_t scanner)
{
    return scanner->condition;
}

#define YYGETCONDITION     getCondition(scanner)
#define YYSETCONDITION(c)  setCondition(scanner, c)
#define YYFILL(n)          bufferFill(scanner, n)

#define UPDATE_START  scanner->tokenStart = scanner->cursor;
#define IGNORE_TOKEN  UPDATE_START; continue;
#define yytext        getTokenText(scanner)

static void
copyTokenText(perplex_t scanner) {
    scanner->appData->tokenData.string = (char*)copyString(yytext);
}

static int perplexScan(perplex_t scanner);

int yylex(perplex_t scanner) {
    int ret;

    scanner->tokenText = NULL;

    ret = perplexScan(scanner);

    if (scanner->tokenText != NULL) {
	free(scanner->tokenText);
	scanner->tokenText = NULL;
    }

    return ret;
}

static int
perplexScan(perplex_t scanner) {
    char yych;

    UPDATE_START;

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
re2c:condenumprefix = "";
re2c:cond:goto = "continue;";
re2c:define:YYCONDTYPE = condition_t;
re2c:define:YYGETCONDITION:naked = 1;

<> :=> definitions

<rules>'"' :=> dquote_string
<rules>"'" :=> squote_string
<rules>'[' :=> bracket_string
<rules>"/*" :=> comment
<rules>"//" :=> line_comment

<definitions,rules>[^\n]"%%\n" {
    copyTokenText(scanner);
    return TOKEN_WORD;
}
<definitions,rules>"%%\n" {
    /* Matched "%%\n" appearing at start of file or start of line.
     * We don't match the optional preceeding '\n' so that '\n' can still
     * end a word (e.g. "foo\n%%\n" is a word followed by a separator).
     */
    if (YYGETCONDITION == definitions) {
	YYSETCONDITION(rules);
    } else {
	YYSETCONDITION(code);
    }
    return TOKEN_SEPARATOR;
}

<rules>[^\n]'<' {
    fprintf(stderr, "=== matched pseudo condition\n");
    copyTokenText(scanner);
    return TOKEN_WORD;
}
<rules>'<' {
    /* matched '<' at start of line or section */
    fprintf(stderr, "=== matched condition\n");
    if (strlen(yytext) == 1 || yytext[0] == '\n') {
	fprintf(stderr, "=== changing to condition_list\n");
	YYSETCONDITION(condition_list);
    }
    continue;
}
<rules>':'?"=>"[ \t\n]* {
    copyTokenText(scanner);
    return TOKEN_SPECIAL_OP;
}
<rules>'=' {
    if (scanner->inAction) {
	continue;
    } else {
	scanner->inDefinition = 1;
	copyTokenText(scanner);
	return TOKEN_SPECIAL_OP;
    }
}
<rules>';' {
    if (scanner->inDefinition) {
	scanner->inDefinition = 0;
	copyTokenText(scanner);
	return TOKEN_DEFINITION;
    }
    continue;
}
<rules>'{' {
    /* brace appears inside condition scope */
    if (scanner->conditionScope) {
	scanner->scopeBraceCount++;
    }

    scanner->braceCount++;
    if (scanner->braceCount == 1) {
	scanner->inAction = 1;
	return TOKEN_CODE_START;
    }
    continue;
}
<rules>'}' {
    /* brace appears inside condition scope */
    if (scanner->conditionScope) {

	scanner->scopeBraceCount--;

	/* brace closes scope */
	if (scanner->scopeBraceCount == 0) {
	    return TOKEN_END_SCOPE;
	}
    }

    scanner->braceCount--;

    /* brace closes rule code */
    if (scanner->braceCount == 0) {
	scanner->inAction = 0;
	return TOKEN_CODE_END;
    }
    continue;
}
<rules>[ \t\n] {
    /* matched single whitespace */
    if (strlen(yytext) == 1) {
	IGNORE_TOKEN;
    }

    /* matched end of word */
    copyTokenText(scanner);

    if (!scanner->inAction) {
	switch (scanner->appData->tokenData.string[0]) {
	    case '"':
	    case '\'':
	    case '[':
	    case '(':
	    case '.':
		return TOKEN_PATTERN;
	}
	return TOKEN_NAME;
    }

    return TOKEN_WORD;
}
<definitions,rules,code>[ \t\n] {
    /* matched single whitespace */
    if (strlen(yytext) == 1) {
	IGNORE_TOKEN;
    }

    /* matched end of word */
    copyTokenText(scanner);
    return TOKEN_WORD;
}

<dquote_string,squote_string,bracket_string>'\\'[^] {
    /* escape sequence */
    continue;
}
<dquote_string>'"' :=> rules
<squote_string>"'" :=> rules
<bracket_string>']' :=> rules
<comment>"*/" => rules {
    IGNORE_TOKEN;
}
<line_comment>'\n' => rules {
    IGNORE_TOKEN;
}

<condition_list>'>'[ \t\n]*'{' => rules {
    copyTokenText(scanner);

    /* trim everything after '>' */
    strchr(scanner->appData->tokenData.string, '>')[1] = '\0';

    scanner->conditionScope = 1;
    scanner->scopeBraceCount = 1;

    return TOKEN_START_SCOPE;
}
<condition_list>'>' => rules {
    fprintf(stderr, "=== end of conditon_list\n");
    fprintf(stderr, "=== conditon_list start %c\n", *scanner->tokenStart);

    if (strlen(yytext) == 2) {
	/* matched "<>" */
	fprintf(stderr, "=== empty condition\n");
	return TOKEN_EMPTY_COND;
    }

    fprintf(stderr, "=== condition %s\n", yytext);
    copyTokenText(scanner);
    return TOKEN_CONDITION;
}

<*>[^] {
    if (YYGETCONDITION == condition_list) {
	fprintf(stderr, "=== conditon_list start %c\n", *scanner->tokenStart);
	fprintf(stderr, "=== conditon_list char %c\n", scanner->cursor[-1]);
    }

    continue;
}

*/
    }
}

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
