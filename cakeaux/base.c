/*
**	A program to prepare a base for spell and style
*/

#include	<stdio.h>
#include	"std.h"

typedef	struct	s_entry
{
	bool	reflexive;
	char	*replacement;
} Entry;

Entry	table[5] =	/* we use subscripts 1 to 4 */
{
	{	FALSE,	""		},
	{	TRUE,	"EQUATION"	},
	{	FALSE,	"(REFERENCE)"	},
	{	FALSE,	"PROGRAM"	},
	{	FALSE,	"PICTURE"	},
};

int	more = TRUE;

main()
{
	extern	char	yytext[];
	register	int	code;

	code = yylex();
	while (more)
	{
		if (code <= 0)
			fputs(yytext, stdout);
		else
		{
			register	int	oldcode;
			register	int	newcode;

			oldcode = code;
			newcode = table[code].reflexive? code: -code;

			code = yylex();
			while (more && code != newcode)
				code = yylex();
			
			printf(table[oldcode].replacement);
		}

		code = yylex();
	}

	exit(0);
}
