/*
**	needed: an even simpler utility for text processing with cake
*/

#include	<stdio.h>
#include	"std.h"

#define	MAXLINE	256

main(argc, argv)
int	argc;
char	**argv;
{
	extern	char	*match();
	register	FILE	*fp;
	register	char	*rest;
	char		line[MAXLINE];

	if (argc != 2)
	{
		printf("Usage: needed file\n");
		exit(127);
	}

	if ((fp = fopen(argv[1], "r")) == NULL)
	{
		printf("needed: cannot open %s\n", argv[1]);
		exit(127);
	}

	while (getline(fp, line, MAXLINE) > 0)
		if ((rest = match(line, "NEED")) != NULL)
		{
			printf("%s", rest);
			exit(0);
		}

	exit(0);
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

char *
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
			return s+j;
	}

	return NULL;
}
