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

yywrap()
{
	return 1;
}
