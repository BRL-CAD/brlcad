%{
/*
**	Scanner for ccincl
*/

static	char
rcs_id[] = "$Header$";

#undef	YYLMAX
#define	YYLMAX	512
%}

nl	[\n\f]
nonl	[^\n\f]

%%

#{nonl}*{nl}		{
				process(yytext);
				fflush(stdout);
			}

{nonl}*{nl}		;

%%

#if !defined(__bsdi__) && !defined(__GNUC__)

extern int yylineno;

#else

/* GNU lex (flex) does not define yylineno */
int yylineno=1;

#endif

yywrap()
{
	return 1;
}

