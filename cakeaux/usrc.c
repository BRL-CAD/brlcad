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
	reg	int	i, j, errcnt;
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
		fprintf(stderr, "usrc: mixed suffixes and filenames\n");
		exit(1);
	}

	errcnt = 0;
	for (i = firstfile; i < argc; i++)
	{
		found = FALSE;
		for (j = lastsuf; j > 0; j--)
		{
			sprintf(buf, "%s%s", argv[i], argv[j]);
			if (stat(buf, &statbuf) == 0)
			{
				found = TRUE;
				printf("%s\n", buf);
				break;
			}
		}

		if (! found)
		{
			fprintf(stderr, "usrc: cannot find source for %s\n", argv[i]);
			errcnt++;
		}
	}

	exit(errcnt);
}
