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

/* GNU lex (flex) does not define yylineno, may need to add a check
 * for __bsdi__ -- need to verify later 
 */
#if defined(YY_FLEX_MAJOR_VERSION) && !defined(YY_FLEX_LEX_COMPAT)
int yylineno=1;
#else
extern int yylineno;
#endif

yywrap()
{
	return 1;
}

