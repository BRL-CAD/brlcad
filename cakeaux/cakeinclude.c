/*
 *			C A K E I N C L U D E . C
 *
 *  This program is intended to be a faster replacement for the
 *  cakeinclude.sh script used for BRL-CAD distributions.
 *  It therefore embodies the same conventions and restrictions.
 *
 *  This program prints the names of all header files that a given
 *  C source module includes.  No attempt is made to deal with
 *  conditional inclusion of header files.
 *
 *  The BRL-CAD convention for writing #include directives is:
 *	#include "./file"	gives file in "current" directory (SRCDIR)
 *	#include "/file"	gives file at absolute path
 *	#include "file"		gives file in ${INCDIR}
 *	#include <file>		leaves to CPP
 *
 *  Note that there must be whitespace between the "#" and the word "include"
 *  to defeat the detection of the include.
 *  This allows conditional includes (if needed) to be indicated
 *  by a # SP include, so as to not cause trouble.
 *
 *  This routine does not handle nested #includes, which is bad style anyway.
 *
 *  Options -
 *	i dir	Specify Include dir
 *	s dir	Specify Source dir
 *	b #	Buffer size
 *	h	help
 *
 *  Author -
 *	Lee A. Butler
 *  
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 *  
 *  Distribution Status -
 *	Public Domain, Distribution Unlimitied.
 */
#ifndef lint
static char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include <stdio.h>

/* declarations to support use of getopt() system call */
char *options = "hb:i:s:";
char optflags[sizeof(options)];
extern char *optarg;
extern int optind, opterr, getopt();

char *progname = "(noname)";
char *buffer, *malloc(), *realloc();
unsigned bufsiz=16384;
char *srcdir = ".";
char *incdir = ".";
int status;
FILE *fd, *fopen();

/*
 *	U S A G E --- tell user how to invoke this program, then exit
 */
void usage()
{
	(void) fprintf(stderr, "Usage: %s [ -%s ] file [ file ... ]\n",
			progname, options);
	exit(1);
}

/*
 *	P A R S E _ A R G S --- Parse through command line flags
 */
int parse_args(ac, av)
int ac;
char *av[];
{
	int  c;

	progname = *av;
	
	/* turn all the option flags off */
	for (c=strlen(options) ; c-- ; optflags[c] = '\0');
	
	/* Turn off getopt's error messages */
	opterr = 0;

	/* get all the option flags from the command line */
	while ((c=getopt(ac,av,options)) != EOF)
		switch (c) {
		case 'b'	: if ((bufsiz = atoi(optarg)) < 11)
					usage();
				break;
		case 'i':
			incdir = optarg;
			break;
		case 's':
			srcdir = optarg;
			break;
		default		: usage(); break; 
		}

	return(optind);
}

/*	C H K I N C L
 *
 *	check an include statement for suitability
 */
int chkincl(j, i)
register int j, *i;
{
	int k;

	/* skip over any white space */
	while (j < *i && buffer[j] == ' ' || buffer[j] == '\t')  j++;

	if (j >= *i) return(j);

	/* ignore the #include <> format and # include " " format.
	 * we are only interested in lines of the form:
	 * #include "filespec"
	 */
	if (buffer[j++] != '"') return(j);

	/* find end of filename */
label:	for (k = j ; k < *i && buffer[k] != '"' && buffer[k] != '\n' ; k++)
		;

	if (buffer[k] == '\n')
		return(k);
	else if (k >= *i) {
		/* we hit the end of the buffer in the middle of an include
		 * statement.  If the buffer wasn't full, we hit EOF and can
		 * just give up.  On the other hand, if the buffer was full,
		 * we should try to extend the buffer and read in the rest
		 * of the include statment
		 */
		if (*i < bufsiz-1) return(k);
		if ((buffer=realloc(buffer, bufsiz+1024)) == (char *)NULL) {
			fprintf(stderr, "%s: Error extending buffer\n",
				progname);
			exit(-1);
		}
		/* read a little more data */			
		bufsiz += 1024;
		while ( *i < bufsiz-1 && 
		    (status=fread(&buffer[*i], 1, bufsiz-1-(*i), fd)) )
			*i += status;

		goto label;

	}
	else
		buffer[k] = '\0';

	/* ignore the "...debug.h" files */
	if (k-7 >= j && !strncmp("debug.h", &buffer[k-7], 7))
		return(k);

	if (buffer[j] == '.' && buffer[j+1] == '/')
		(void)printf("%s%s\n", srcdir, &buffer[j+1]);
	else if (buffer[j] == '/')
		(void)printf("%s\n", &buffer[j]);
	else
		(void)printf("%s/%s\n", incdir, &buffer[j]);

	return(k);
}


/*
 *	M A I N
 *
 *	Call parse_args to handle command line arguments first, then
 *	process input.
 */
int main(ac,av)
int ac;
char *av[];
{
	int arg_index;
	int j, eob, newline;

	/* parse command flags, and make sure there are arguments
	 * left over for processing.
	 */
	if ((arg_index = parse_args(ac, av)) >= ac) usage();

	if ((buffer=malloc(bufsiz)) == (char *)0) {
		fprintf(stderr, "%s: Unable to allocate %d bytes for buffer\n",
			progname, bufsiz);
		exit(-1);
	}

	/* This part might be done in a loop, to match usage message? */

	if ((fd = fopen(av[arg_index], "r")) == (FILE *)NULL) {
		perror(av[arg_index]);
		exit(-1);
	}

	/* fill buffer */
	for (eob=0 ; eob < bufsiz-1 && 
	    (status=fread(&buffer[eob], 1, bufsiz-1-eob, fd)) ; eob += status)
		;

	if (eob == 0) {
		fprintf(stderr, "%s: Error reading file \"%s\"\n",
				progname, av[arg_index]);
		exit(-1);
	}
	
	newline = 1;
	for (j = 0 ; j < eob ; j++) {
		if (newline && !strncmp("#include", &buffer[j], 8))
			j = chkincl(j+8, &eob);

		newline = (buffer[j] == '\n');
	}

	(void)fclose(fd);
}
