/*
**	need: a simple utility for text processing with cake
*/

#include	<stdio.h>
#include	"std.h"

#define	MAXLINE	256

main(argc, argv)
int	argc;
char	**argv;
{
	register	FILE	*fp;
	char		line[MAXLINE];

	if (argc != 2 && argc != 3)
	{
		printf("Usage: need program [file]\n");
		exit(127);
	}

	if (argc == 2)
		fp = stdin;
	else
	{
		if ((fp = fopen(argv[2], "r")) == NULL)
		{
			printf("need: cannot open %s\n", argv[2]);
			exit(127);
		}
	}

	while (getline(fp, line, MAXLINE) > 0)
		if (match(line, "NEED"))
			exit(match(line, argv[1])? 0: 1);
	
	exit(127);
}

int
getline(fp, s, lim)
register	FILE	*fp;
char		s[];
register	int	lim;
{
	register	int	c, i;

	i = 0;
	while (--lim > 0 && (c = getc(fp)) != EOF && c != '\n')
		s[i++] = c;

	if (c == '\n')
		s[i++] = c;

	s[i] = '\0';
	return i;
}

int
match(s, t)
register	char	*s;
register	char	*t;
{
	register	int	i, j;

	for (; *s != '\0'; s++)
	{
		for (i = 0, j = 0; t[j] != '\0' && s[i] == t[j]; i++, j++)
			;
		if (t[j] == '\0')
			return TRUE;
	}

	return FALSE;
}
