/*
**	Track down ultimate source files.
*/

#include	<stdio.h>
#include	<sys/types.h>
#include	<sys/stat.h>
#include	"std.h"

main(argc, argv)
int	argc;
char	**argv;
{
	char		buf[80];
	struct	stat	statbuf;
	reg	int	i, j;
	reg	int	lastsuf, firstfile;
	reg	bool	found;

	lastsuf = 0;
	firstfile = argc;
	for (i = 1; i < argc; i++)
	{
		if (argv[i][0] == '.')
			lastsuf = i;
		or (firstfile == argc)
			firstfile = i;
	}

	if (lastsuf > firstfile)
	{
		fprintf(stderr, "gsrc: mixed suffixes and filenames\n");
		exit(1);
	}

	for (i = firstfile; i < argc; i++)
	{
		found = FALSE;
		for (j = lastsuf; j > 0; j--)
		{
			sprintf(buf, "%s%s", argv[i], argv[j]);
			if (stat(buf, &statbuf) == 0)
			{
				if (! found)
					found = TRUE;
				else
					printf("%s%s\n", argv[i], argv[j]);
			}
		}
	}

	exit(0);
}
