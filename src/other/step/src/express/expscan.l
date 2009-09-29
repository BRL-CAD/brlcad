%{

static char rcsid[] = "$Id: expscan.l,v 1.12 1997/05/29 20:17:34 sauderd Exp $";

/*
 * Lex source for Fed-X lexical analyzer.
 *
 * This software was developed by U.S. Government employees as part of
 * their official duties and is not subject to copyright.
 *
 * $Log: expscan.l,v $
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

extern void	yyerror();
extern int	yylineno;
extern Boolean	yyeof;
static int	nesting_level;

/* can't imagine this will ever be more than 2 or 3 - DEL */
#define MAX_NESTED_COMMENTS 20
static struct Symbol_ open_comment[MAX_NESTED_COMMENTS];

#undef yywrap
static int
yywrap()
{
	int i;

	for (i=0;i<nesting_level && i<MAX_NESTED_COMMENTS;i++) {
		ERRORreport_with_symbol(ERROR_unmatched_open_comment,
			&open_comment[i]);
		/* maybe not all, but at least some will be reported - DEL */
	}
	return 1;
}

#ifdef FLEX_SCANNER

#ifdef LEX
))))this line will generate an error during compilation intentionally - DEL
You used flex (in make_rules) but compiled with -DLEX (in Makefile)!
#endif

#define YY_USER_INIT	BEGIN code;	/* DEL */

#undef YY_DECL
#define YY_DECL int yylex(void)

/* here's the hook for the preprocessor */
#undef YY_INPUT
#define YY_INPUT(buf, result, max_size)					\
	(result) = SCANnextchar(buf)

/* I don't believe this is used anymore, but not sure */
/* in any case, experimental code above that attempts to skip over */
/* initial non-Express code explicitly refers to YY_INPUT */
#ifdef slow_yylineno
#undef YY_INPUT
#define YY_INPUT(buf,result,max_size)					\
 	result = (((buf[0] = getc(yyin)) == '\n')			\
		  ? (NEWLINE, 1)					\
		  : ((buf[0] == EOF) ? ((yyeof = True), YY_NULL) : 1))
#endif /*slow_yylineno*/

/*#define NEWLINE	((SCANbol = SCANtell()), yylineno++)*/
#define NEWLINE	yylineno++
#define LINENO_FUDGE yylineno

#else /* !FLEX_SCANNER (i.e, LEX) */

#ifdef FLEX
))))this line will generate an error during compilation intentionally - DEL
You used lex (in make_rules) but compiled with -DFLEX (in Makefile)!
#endif

#undef input
#undef unput

/* max page size 100 is guaranteed, but lex code uses BUFSIZ! - DEL */
static char unchar[BUFSIZ];		/* one char for lex unput storage */
static char *unchar_p = unchar;		/* whether unchar is in use */

static int first_time = True;

static int
input() {
	char c;

	if (unchar_p == unchar) {
		/* no chars pushed back */
		if (!SCANnextchar(&c)) return(0);
		if (c == '\n') {
		    yylineno++;
/*		    SCANbol = SCANtell();*/
		}
		return(c);
	} else {
		unchar_p--;
		if (*unchar_p == '\n') yylineno++;
		return(*unchar_p);
	}
}

/* if paranoid, should check that bounds are never exceeded, but maybe */
/* lex does this for us?   lex is not documented well enough to know */
#define unput(c) {yytchar=(c);if(yytchar=='\n')yylineno--;*unchar_p++=yytchar;}
/* really shouldn't use yytchar since it isn't part of the defined interface */


/* lex counts line for us, so turn this into a no-op - DEL */
#define NEWLINE
/* when lex looks ahead over a newline, error messages get thrown off */
/* Fortunately, we know when that occurs, so adjust for it by this hack */
#define LINENO_FUDGE (yylineno-1)

#endif

#ifdef FLEX_SCANNER
void exp_flex_init();
#endif

/* added for re-initializing parser -snc 22-Apr-1992 */
void
SCAN_lex_init(char *filename,FILE *fp) {
	int i;

        /* return to initial scan buffer */
        SCAN_current_buffer = 0;
        *(SCANcurrent = SCANbuffer.text) = '\0';
        SCANbuffer.readEof = False;
        SCANbuffer.file = fp;
	SCANbuffer.filename = (filename?filename:"");
	current_filename = SCANbuffer.filename;

        /* empty all scan buffers */
/*      for (i = 0; i < SCAN_NESTING_DEPTH; ++i)*/
/*          SCAN_buffers[i].text[0] = '\0';*/

#ifdef FLEX_SCANNER
	exp_flex_init();
#endif

#ifndef FLEX_SCANNER
        /* initialize un-got character pointer to start of buffer */
        unchar_p = unchar;

	first_time = True;
#endif
}


%}

%s comment code return_end_schema

digit	[0-9]
integer	{digit}+
letter	[A-Za-z]
id_char	[A-Za-z0-9_]

%%

%{
#ifndef FLEX_SCANNER
if (first_time) {
	BEGIN code;
	first_time = False;
}
#endif
%}

%{
/* Added * at the end of next rule (to make lexer faster) - DEL */
%}
[\t ]*		;
\n		{ NEWLINE; }

<code>"--"[^\n]*\n	{ NEWLINE; SCANsave_comment(); }

"(*"	{
	/* nested comment errors will occur with most deeply nested - DEL */
	if (nesting_level < MAX_NESTED_COMMENTS) {
		open_comment[nesting_level].line = yylineno;
		open_comment[nesting_level].filename = current_filename;
	}
	nesting_level++;
	BEGIN comment;
}

%{
/* real literal (like, think it'll fly?!) */
%}

<code>{integer}"."{integer}?([eE][+-]?{integer})?	{
	return SCANprocess_real_literal();
}

%{
/* integer literal */
%}

<code>{integer}	{
	return SCANprocess_integer_literal();
}

%{
/* binary literal */
%}

<code>"\%"[01]+	{
	return SCANprocess_binary_literal();
}

%{
/* identifier/keyword */
%}

<code>{letter}{id_char}*	{
	return SCANprocess_identifier_or_keyword();
}

%{
/* bad identifier */
%}

<code>[_A-Za-z]{id_char}*	{
	ERRORreport_with_line(ERROR_bad_identifier,yylineno,yytext);
	return SCANprocess_identifier_or_keyword();
}

%{
/* string literal */
%}

<code>'([^'\n]|'')*'	{	/* ' keep font-lock happy */
	return SCANprocess_string();
}

<code>'([^'\n]|'')*\n	{
	ERRORreport_with_line(ERROR_unterminated_string,LINENO_FUDGE);
	NEWLINE;
	return SCANprocess_string();
}

<code>\"[^\"\n]*\"	{
	return SCANprocess_encoded_string();
}

<code>\"[^\"\n]*\n	{
	ERRORreport_with_line(ERROR_unterminated_string,LINENO_FUDGE);
	NEWLINE;
	return SCANprocess_encoded_string();
}


<code>";"[ \t]*"--"[^\n]*\n {
		  NEWLINE;
		  return SCANprocess_semicolon(1);
}
<code>";"	{ return SCANprocess_semicolon(0); }

<code>":="	{ return TOK_ASSIGNMENT; }
<code>":"	{ return TOK_COLON; }
<code>","	{ return TOK_COMMA; }
<code>"||"	{ return TOK_CONCAT_OP; }
<code>"."	{ return TOK_DOT; }
<code>"="	{ return TOK_EQUAL; }
<code>"**"	{ return TOK_EXP; }
<code>"|"	{ return TOK_SUCH_THAT; }
<code>"<*"	{ return TOK_ALL_IN; }
<code>">="	{ return TOK_GREATER_EQUAL; }
<code>">"	{ return TOK_GREATER_THAN; }
<code>"?"	{ return TOK_QUESTION_MARK; }
<code>":=:"	{ return TOK_INST_EQUAL; }
<code>":<>:"	{ return TOK_INST_NOT_EQUAL; }
<code>"["	{ return TOK_LEFT_BRACKET; }
<code>"{"	{ return TOK_LEFT_CURL; }
<code>"("	{ return TOK_LEFT_PAREN; }
<code>"<="	{ return TOK_LESS_EQUAL; }
<code>"<"	{ return TOK_LESS_THAN; }
<code>"-"	{ return TOK_MINUS; }
<code>"<>"	{ return TOK_NOT_EQUAL; }
<code>"+"	{ return TOK_PLUS; }
<code>"/"	{ return TOK_REAL_DIV; }
<code>"]"	{ return TOK_RIGHT_BRACKET; }
<code>"}"	{ return TOK_RIGHT_CURL; }
<code>")"	{ return TOK_RIGHT_PAREN; }
<code>"*"	{ return TOK_TIMES; }
<code>"\\"	{ return TOK_BACKSLASH; }

<return_end_schema>X	{
	BEGIN code;
	return TOK_END_SCHEMA;
}

<comment>"*)"	{
	if (0 == --nesting_level) BEGIN code;
}

<code>"*)"	{
	ERRORreport_with_line(ERROR_unmatched_close_comment,yylineno);
}	

<comment>\n		{ NEWLINE; }
<comment>[^*()\n]*	;
<comment>[*()]		;

%{
/* As per N15, 7.1.5.3, all other recognized chars are incorrect - DEL */
%}
<code>[$%&@\^{}~] {
	ERRORreport_with_line(ERROR_unexpected_character,yylineno,yytext[0]);
}

%{
/* ... and unrecognized characters are treated as whitespace - DEL */
%}
.			;

%%
void
SCANskip_to_end_schema()
{
	while (yylex() != TOK_END_SCHEMA);

	unput('X');	/* any old character */

	BEGIN return_end_schema;
}

#ifdef FLEX_SCANNER
/* required because yy_init isn't known when we need to re-init it */
void
exp_flex_init()
{
	/* yy_init is what tells flex to reinitialize its buffers */

	yy_init = 1;
}
#endif
