/*
 *			F I N D C O M . C
 *
 * $Revision$
 *
 * $Log$
 * Revision 11.1  95/01/04  10:35:04  mike
 * Release_4.4
 *
 * Revision 10.1  91/10/12  06:53:51  mike
 * Release_4.0
 *
 * Revision 2.1  87/05/05  21:08:18  dpk
 * Removed reference to _sobuf.
 *
 * Revision 2.0  84/12/26  16:44:29  dpk
 * System as distributed to Berkeley 26 Dec 84
 *
 * Revision 1.2  83/12/16  00:07:05  dpk
 * Added distinctive RCS header
 *
 */
#ifndef lint
static char RCSid[] = "@(#)$Header$";
#endif

#include <stdio.h>

extern char	Describe[];

main(argc, argv)
int	argc;
char	*argv[];
{
	register FILE	*fp;
	register char	*com;
	register int	len;
	char	line[200];

	if (argc != 2) {
		printf("Incorrect number of arguments to findcom\n");
		exit(1);
	}
	if ((fp = fopen(Describe, "r")) == NULL) {
		printf("Cannot open %s\n", Describe);
		exit(1);
	}

	com = argv[1];
	len = strlen(com);

	while (fgets(line, 200, fp)) {
		if (line[0] != '\n')
			continue;
		/* Next line begins a topic */
		fgets(line, 200, fp);
		if (strncmp(line + 5, com, len))
			continue;
		printf("%s\n", line);
		while (fgets(line, 200, fp) && line[0] != '\n')
			printf(line);
		putchar('\n');
		break;
	}
	fclose(fp);
	return 0;
}
