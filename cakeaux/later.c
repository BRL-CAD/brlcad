/*
**	Find out which arg files are later than a reference file.
*/

static	char
rcs_id[] = "$Header$";

#include	<stdio.h>
#include	<sys/types.h>
#include	<sys/stat.h>
#include	"std.h"

typedef	struct	stat	Stat;

char	scratchbuf[128];


/*
**	Tell the unfortunate user how to use later.
*/

void
usage()
{
	printf("Usage: later [-cs] reffile file ...\n");
	exit(1);
}

main(argc, argv)
int	argc;
char	**argv;
{
	Stat		statbuf;
	register	time_t	reftime;
	register	int	i, n;
	register	bool	count  = FALSE;
	register	bool	silent = FALSE;

	while (argc > 1 && argv[1][0] == '-')
	{
		for (i = 1; argv[1][i] != '\0'; i++)
		{
			switch (argv[1][i])
			{

		case 'c':	count = TRUE;
				break;

		case 's':	silent = TRUE;
				break;

		default:	usage();

			}
		}

		argc--;
		argv++;
	}

	if (argc < 3)
		usage();
	
	if (stat(argv[1], &statbuf) != 0)
	{
		sprintf(scratchbuf, "later, stat %s", argv[1]);
		perror(scratchbuf);
		exit(127);
	}

	argv++;
	argc--;
	reftime = statbuf.st_mtime;
	n = 0;

	while (argc > 1)
	{
		if (stat(argv[1], &statbuf) != 0)
		{
			sprintf(scratchbuf, "later, stat %s", argv[1]);
			perror(scratchbuf);
			exit(127);
		}

		if (statbuf.st_mtime > reftime)
		{
			n++;
			if (! silent)
				printf("%s\n", argv[1]);
		}

		argc--;
		argv++;
	}

	exit(count? n: 0);
}
