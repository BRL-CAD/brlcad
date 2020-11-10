
/*
 * Lex source for Fed-X lexical analyzer.
 *
 * This software was developed by U.S. Government employees as part of
 * their official duties and is not subject to copyright.
 *
 * $Log$
 * Revision 1.12  1997/05/29 20:17:34  sauderd
 * Made some changes to Symbol (to be Symbol_) and false and true to be False
 * and True. These changes affect the generated expscan.c file so that it will
 * compile.
 *
 * Revision 1.11  1994/11/22 18:32:39  clark
 * Part 11 IS; group reference
 *
 * Revision 1.10  1994/05/12  17:22:23  libes
 * added #ifdefs for flex
 *
 * Revision 1.9  1994/05/12  17:18:10  libes
 * made flex understand multiple files
 *
 * Revision 1.8  1994/05/11  19:50:00  libes
 * numerous fixes
 *
 * Revision 1.7  1993/10/15  18:47:26  libes
 * CADDETC certified
 *
 * Revision 1.5  1993/02/22  21:46:33  libes
 * fixed unmatched_open_comment handler
 *
 * Revision 1.4  1992/08/18  17:11:36  libes
 * rm'd extraneous error messages
 *
 * Revision 1.3  1992/06/08  18:05:20  libes
 * prettied up interface to print_objects_when_running
 *
 * Revision 1.2  1992/05/31  08:30:54  libes
 * multiple files
 *
 * Revision 1.1  1992/05/28  03:52:25  libes
 * Initial revision
 *
 * Revision 1.4  1992/05/05  19:49:03  libes
 * final alpha
 *
 * Revision 1.3  1992/02/12  07:02:49  libes
 * do sub/supertypes
 *
 * Revision 1.2  1992/02/09  00:49:04  libes
 * does ref/use correctly
 *
 * Revision 1.1  1992/02/05  08:40:30  libes
 * Initial revision
 *
 * Revision 1.0.1.1  1992/01/22  02:47:57  libes
 * copied from ~pdes
 *
 * Revision 4.9  1991/06/14  20:49:12  libes
 * removed old infinity, added backslash
 *
 * Revision 4.8.1.1  1991/05/16  04:07:57  libes
 * made scanner (under lex) understand hooks for doing include directive
 *
 * Revision 4.8.1.0  1991/05/16  01:10:15  libes
 * branch for fixes to old code
 *
 * Revision 4.8  1991/05/03  21:09:02  libes
 * Added sanity check to make sure lex/flex match -DLEX/FLEX
 *
 * Revision 4.7  1991/05/02  05:49:18  libes
 * fixed bug in testing for exceeding open_comment[nesting_level]
 *
 * Revision 4.6  1991/04/29  19:44:40  libes
 * Print all open comments rather than just one.
 *
 * Revision 4.5  1991/04/29  15:39:02  libes
 * Changed commenting style (back) as per SNC who claims that N9 meant to
 * say that tail remarks cannot occur in an open comment, nor can nested
 * comments begin in a tail remark.
 *
 * Revision 4.4  1991/04/29  15:01:46  libes
 * Add bounds checking to nesting level history
 *
 * Revision 4.3  1991/04/26  20:12:50  libes
 * Made scanner work with lex
 * 	Simulated exclusive states with inclusive states
 * 	Fixed line counting
 * Speeded up whitespace matching
 * Convert unknown chars to whitespace
 * Disabled default rule matching (enabled "jamming")
 * Enabled detection/diagnostics of unterminated comments and strings literals
 * Enabled detection/diagnostics of unexpected close comments
 * Disabled detection/diagnostics of nested comments
 *
 * Revision 4.2  1990/12/18  14:00:04  clark
 * Cosmetic changes
 *
 * Revision 4.1  90/09/13  16:29:00  clark
 * BPR 2.1 alpha
 * 
 */

#include "express/basic.h"
#include "express/error.h"
#include "express/lexact.h"
#include "express/express.h"
#include "expparse.h"
#include "expscan.h"

enum { INITIAL, code, comment, return_end_schema };

extern void	yyerror();
extern int	yylineno;
extern bool	yyeof;
static int	nesting_level = 0;

/* can't imagine this will ever be more than 2 or 3 - DEL */
#define MAX_NESTED_COMMENTS 20
static struct Symbol_ open_comment[MAX_NESTED_COMMENTS];

static_inline
int
SCANnextchar(char* buffer)
{
    extern bool SCANread(void);
#ifdef keep_nul
    static int escaped = 0;
#endif

    if (SCANtext_ready || SCANread()) {
#ifdef keep_nul
	if (!*SCANcurrent) {
	    buffer[0] = SCAN_ESCAPE;
	    *SCANcurrent = '0';
	    return 1;
	} else if ((*SCANcurrent == SCAN_ESCAPE) && !escaped) {
	    escaped = 1;
	    buffer[0] = SCAN_ESCAPE;
	    return 1;
	}
	SCANbuffer.numRead--;
#endif
	buffer[0] = *(SCANcurrent++);
	if (!isascii(buffer[0])) {
	    ERRORreport_with_line(ERROR_nonascii_char,yylineno,
				  0xff & buffer[0]);
	    buffer[0] = ' ';	/* substitute space */
	}
	return 1;
    } else
	return 0;
}

static int
yywrap()
{
    int i;

    for (i = 0; i < nesting_level && i < MAX_NESTED_COMMENTS; i++) {
	ERRORreport_with_symbol(ERROR_unmatched_open_comment,
		&open_comment[i]);
	/* maybe not all, but at least some will be reported - DEL */
    }
    return 1;
}

#define NEWLINE (yylineno++)

/* when lex looks ahead over a newline, error messages get thrown off */
/* Fortunately, we know when that occurs, so adjust for it by this hack */
#define LINENO_FUDGE (yylineno - 1)

/* added for re-initializing parser -snc 22-Apr-1992 */
void
SCAN_lex_init(char *filename, FILE *fp) {

        /* return to initial scan buffer */
        SCAN_current_buffer = 0;
        *(SCANcurrent = SCANbuffer.text) = '\0';
        SCANbuffer.readEof = false;
        SCANbuffer.file = fp;
	SCANbuffer.filename = (filename ? filename : "");
	current_filename = SCANbuffer.filename;
}
%%
digit	= [0-9];
integer	= digit+;
letter	= [A-Za-z];
id_char	= [A-Za-z0-9_];

<> => code

/* Added * at the end of next rule (to make lexer faster) - DEL */
<*>[\t ]* { IGNORE_TOKEN; }
<*>'\n' {
    NEWLINE;
    IGNORE_TOKEN;
}

<code>"--"[^\n]*'\n' {
    NEWLINE;
    SCANsave_comment(yytext);
    IGNORE_TOKEN;
}

<*>"(*" => comment {
    /* nested comment errors will occur with most deeply nested - DEL */
    if (nesting_level < MAX_NESTED_COMMENTS) {
	open_comment[nesting_level].line = yylineno;
	open_comment[nesting_level].filename = current_filename;
    }
    nesting_level++;
    IGNORE_TOKEN;
}

<code> {

/* real literal (like, think it'll fly?!) */
(integer)'.'(integer)?([eE][+-]?(integer))? {
    return SCANprocess_real_literal(yytext);
}

/* integer literal */
integer {
    return SCANprocess_integer_literal(yytext);
}

/* binary literal */
'%'[01]+ {
    return SCANprocess_binary_literal(yytext);
}

/* identifier/keyword */
(letter)(id_char)* {
    return SCANprocess_identifier_or_keyword(yytext);
}

/* bad identifier */
[_A-Za-z]id_char* {
    ERRORreport_with_line(ERROR_bad_identifier, yylineno, yytext);
    return SCANprocess_identifier_or_keyword(yytext);
}

/* string literal */
"'"([^'\n]|"''")*"'" {
    /* ' keep font-lock happy */
    return SCANprocess_string(yytext);
}

"'"([^'\n]|"''")*'\n' {
    ERRORreport_with_line(ERROR_unterminated_string, LINENO_FUDGE);
    NEWLINE;
    return SCANprocess_string(yytext);
}

'"'[^\"\n]*'"' {
    return SCANprocess_encoded_string(yytext);
}

'"'[^\"\n]*'\n' {
    ERRORreport_with_line(ERROR_unterminated_string, LINENO_FUDGE);
    NEWLINE;
    return SCANprocess_encoded_string(yytext);
}

';'[ \t]*"--"[^\n]*'\n' {
    NEWLINE;
    return SCANprocess_semicolon(yytext, 1);
}

";"	{ return SCANprocess_semicolon(yytext, 0); }
":="	{ return TOK_ASSIGNMENT; }
":"	{ return TOK_COLON; }
","	{ return TOK_COMMA; }
"||"	{ return TOK_CONCAT_OP; }
"."	{ return TOK_DOT; }
"="	{ return TOK_EQUAL; }
"**"	{ return TOK_EXP; }
"|"	{ return TOK_SUCH_THAT; }
"<*"	{ return TOK_ALL_IN; }
">="	{ return TOK_GREATER_EQUAL; }
">"	{ return TOK_GREATER_THAN; }
"?"	{ return TOK_QUESTION_MARK; }
":=:"	{ return TOK_INST_EQUAL; }
":<>:"	{ return TOK_INST_NOT_EQUAL; }
"["	{ return TOK_LEFT_BRACKET; }
"{"	{ return TOK_LEFT_CURL; }
"("	{ return TOK_LEFT_PAREN; }
"<="	{ return TOK_LESS_EQUAL; }
"<"	{ return TOK_LESS_THAN; }
"-"	{ return TOK_MINUS; }
"<>"	{ return TOK_NOT_EQUAL; }
"+"	{ return TOK_PLUS; }
"/"	{ return TOK_REAL_DIV; }
"]"	{ return TOK_RIGHT_BRACKET; }
"}"	{ return TOK_RIGHT_CURL; }
")"	{ return TOK_RIGHT_PAREN; }
"*"	{ return TOK_TIMES; }
"\\"	{ return TOK_BACKSLASH; }

} /* <code> */

<return_end_schema>'X' => code {
    return TOK_END_SCHEMA;
}

<comment>"*)" {
    if (0 == --nesting_level) {
	YYSETCONDITION(code);
    }
    IGNORE_TOKEN;
}

<code>"*)" {
    ERRORreport_with_line(ERROR_unmatched_close_comment, yylineno);
    IGNORE_TOKEN;
}	

<comment> {

'\n' {
    NEWLINE;
    IGNORE_TOKEN;
}
[^*()\n]* { IGNORE_TOKEN; }
[*()] { IGNORE_TOKEN; }

} /* <comment> */

/* As per N15, 7.1.5.3, all other recognized chars are incorrect - DEL */
<code>[$%&@\^{}~] {
    ERRORreport_with_line(ERROR_unexpected_character,yylineno,yytext[0]);
    IGNORE_TOKEN;
}

/* ... and unrecognized characters are treated as whitespace - DEL */
<*>[^] { IGNORE_TOKEN; }

%%
void
SCANskip_to_end_schema(perplex_t scanner)
{
    while (yylex(scanner) != TOK_END_SCHEMA);

    perplexUnput(scanner, 'X');	/* any old character */

    YYSETCONDITION(return_end_schema);
}
