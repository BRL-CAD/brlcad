/*
**	A program to filter out references.
*/

#include	<stdio.h>
#include	"std.h"

int	more = TRUE;

main()
{
	extern	char	yytext[];
	register	FILE	*out;
	register	int	code;
	register	bool	init, ignore;

	out = popen("sed -e 's/[ 	]*$//'", "w");

	code = yylex();
	while (more)
	{
		if (code == 2)
		{
			ignore = FALSE;
			init = TRUE;
			code = yylex();
			while (more && code != -2)
			{
				if (streq(yytext, "{"))
					ignore = TRUE;
				or (streq(yytext, "}"))
					ignore = FALSE;
				or (ignore)
					;
				or (streq(yytext, ","))
				{
					putc('\n', out);
					init = TRUE;
				}
				or (streq(yytext, " "))
				{
					if (! init)
						putc(' ', out);
				}
				else
				{
					fprintf(out, "%s", yytext);
					init = FALSE;
				}

				code = yylex();
			}

			putc('\n', out);
		}

		code = yylex();
	}

	pclose(out);
	exit(0);
}
