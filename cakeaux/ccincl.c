/*
**	Ccincl main file.
*/

static	char
rcs_id[] = "$Header$";

#include	<ctype.h>
#include	<stdio.h>
#include	<sys/types.h>
#include	<sys/stat.h>
#include	"std.h"

typedef	struct	stat	Stat;

#define	MAXDIRS		10
#define	MAXSTOPS	10
#define	MAXIGNORES	10
#define	MAXFILES	64
#define	MAXBUF		512

char	*dir_list[MAXDIRS];
char	*stop_list[MAXSTOPS];
char	*ign_list[MAXIGNORES];
char	*file_list[MAXFILES];
bool	search_list[MAXFILES];
ino_t	ino_list[MAXFILES];
int	dircount = 0;
int	stopcount = 0;
int	igncount = 0;
int	filecount = 0;

char	*currdir = ".";
char	*filename;
Stat	statbuf;
int	rflag = FALSE;
int	fflag = FALSE;
int	errorcount = 0;


/*
**	Tell the unfortunate user how to use ccincl.
*/

void
usage()
{
	printf("Usage: ccincl [-rf] [-ifile] [-sfile] [-Cdir] [-Idir] ... file ...\n");
	exit(1);
}

main(argc, argv)
int	argc;
char	**argv;
{
	extern	FILE	*yyin;
	register	int	i;

	dir_list[dircount++] = ".";
	while (argc > 1 && argv[1][0] == '-')
	{
		for (i = 1; argv[1][i] != '\0'; i++)
		{
			switch (argv[1][i])
			{

		case 'f':
				fflag = TRUE;
				break;
		case 'r':
				rflag = TRUE;
				break;
		case 'i':
				if (i != 1)
					usage();

				ign_list[igncount++] = &argv[1][2];
				file_list[filecount] = &argv[1][2];

				if (stat(&argv[1][2], &statbuf) != 0)
				{
					printf("ccincl: cannot find %s\n", &argv[1][2]);
					exit(1);
				}

				search_list[filecount] = FALSE;
				ino_list[filecount] = statbuf.st_ino;
				filecount++;
				goto nextword;
		case 's':
				if (i != 1)
					usage();

				stop_list[stopcount++] = &argv[1][2];
				file_list[filecount] = &argv[1][2];

				if (stat(&argv[1][2], &statbuf) != 0)
				{
					printf("ccincl: cannot find %s\n", &argv[1][2]);
					exit(1);
				}

				search_list[filecount] = FALSE;
				ino_list[filecount] = statbuf.st_ino;
				filecount++;
				goto nextword;
		case 'C':
				if (i != 1)
					usage();

				currdir = &argv[1][2];
				goto nextword;
		case 'I':
				if (i != 1)
					usage();

				dir_list[dircount++] = &argv[1][2];
				goto nextword;
		default:
				usage();
			}
		}

nextword:
		argc--;
		argv++;
	}

	dir_list[dircount++] = "/usr/include";
	if (dircount > MAXDIRS)
	{
		printf("ccincl: too many dir_list.\n");
		exit(1);
	}

	if (argc < 2)
		usage();
	
	while (argc > 1)
	{
		file_list[filecount] = argv[1];
		if (stat(argv[1], &statbuf) != 0)
		{
			printf("ccincl: cannot find %s\n", argv[1]);
			exit(1);
		}

		search_list[filecount] = TRUE;
		ino_list[filecount] = statbuf.st_ino;
		filecount++;
		argc--;
		argv++;
	}
	
	for (i = 0; i < filecount; i++)
	{
		if (! search_list[i])
			continue;

		filename = file_list[i];
		if ((yyin = fopen(filename, "r")) == NULL)
		{
			fflush(stdout);
			perror("ccincl");
			printf("ccincl: cannot open %s\n", filename);
			exit(1);
		}

		yylex();
		fclose(yyin);
	}

	exit(errorcount);
}

void
process(line)
register	char	*line;
{
	extern	char	*malloc();
	extern	int	yylineno;
	char		buf[MAXBUF];
	register	char	*s, *start;
	register	int	i, startdir;
	register	bool	index;
	register	char	endchar;
	register	bool	found;

	for (s = line+1; *s != '\0' && isspace(*s); s++)
		;
	
	if (strndiff(s, "include", 7))
		return;
	
	for (s += 7; *s != '\0' && isspace(*s); s++)
		;
	
	if (*s == '<')
		endchar = '>', startdir = 1;
	else
		endchar = '"', startdir = 0;

	start = s+1;
	for (s = start; *s != '\0' && *s != endchar; s++)
		;

	if (*s != endchar)
	{
		printf("ccincl: %s(%d) bad include syntax\n", filename, yylineno);
		errorcount++;
		return;
	}

	/* terminate arg (now pointed to by start) */
	*s = '\0';

	/* handle absolute pathnames */
	if (*start == '/')
	{
		sprintf(buf, "%s", start);
		goto end;
	}

	/* handle relative pathnames */
	if (*start == '.')
	{
		sprintf(buf, "%s/%s", currdir, start);
		goto end;
	}

	/* handle implicit pathnames */
	found = FALSE;
	for (i = startdir; i < dircount; i++)
	{
		if (i == 0)
		{
			if (streq(currdir, "."))
				strcpy(buf, "");
			else
			{
				strcpy(buf, dir_list[i]);
				strcat(buf, "/");
			}

			strcat(buf, start);
		}
		else
		{
			strcpy(buf, dir_list[i]);
			strcat(buf, "/");
			strcat(buf, start);
		}

		if (strlen(buf) > MAXBUF)
		{
			printf("ccincl: buffer length exceeded\n");
			exit(1);
		}

		if (stat(buf, &statbuf) == 0)
		{
			found = TRUE;
			break;
		}
	}

	if (! found)
	{
		printf("ccincl: cannot find %s\n", start);
		errorcount++;
		return;
	}

end:
	if (stat(buf, &statbuf) != 0)
	{
		printf("ccincl: cannot stat %s\n", buf);
		errorcount++;
		return;
	}

	found = FALSE;
	for (i = 0; i < filecount; i++)
		if (ino_list[i] == statbuf.st_ino)
		{
			found = TRUE;
			index = i;
			break;
		}

	if (! found)
	{
		index = filecount;
		ino_list[filecount]    = statbuf.st_ino;
		search_list[filecount] = FALSE;
		file_list[filecount]   = malloc(strlen(buf)+1);
		strcpy(file_list[filecount], buf);
		filecount++;
	}

	for (i = 0; i < igncount; i++)
		if (streq(file_list[index], ign_list[i]))
			return;

	if (fflag)
		printf("%s: ", filename);

	printf("%s\n", file_list[index]);

	if (rflag)
	{
		for (i = 0; i < stopcount; i++)
			if (streq(buf, stop_list[i]))
				return;

		search_list[filecount] = TRUE;
	}
}
