/*
 *			C A K E I N C L U D E . C
 *  This program lists the names of all header files that a given C source
 *  module includes.  It is intended to be a faster replacement for the
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
char *options = "hb:i:s:d";
char optflags[sizeof(options)];
extern char *optarg;
extern int optind, opterr, getopt();

char *progname = "(noname)";
char *malloc(), *realloc();
char *srcdir = ".";
char *incdir = ".";
int status;
unsigned bsize=16384;
FILE *fopen();
int debug = 0;

/*
 *	U S A G E --- tell user how to invoke this program, then exit
 */
void usage()
{
	(void) fprintf(stderr,
		"Usage: %s [ -s srcdir ] [ -i incdir ] [ -b bufsize ] file\n",
			progname);
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
		case 'b'	: if ((bsize = atoi(optarg)) < 11)
					usage();
				break;
		case 'i':
			incdir = optarg;
			break;
		case 's':
			srcdir = optarg;
			break;
		case 'd':
			debug = !debug;
			break;
		default		: usage(); break; 
		}

	return(optind);
}

/*	C H K I N C L
 *
 *	check an include statement for suitability
 */
int chkincl(fd, bufpos, eob_cnt_p, buffer, bufsiz)
FILE *fd;
char *buffer;
unsigned *bufsiz;
register int bufpos, *eob_cnt_p;
{
	int k;
	char name[1024];

	/* skip over any white space */
	while (bufpos < *eob_cnt_p &&
	    buffer[bufpos] == ' ' || buffer[bufpos] == '\t')
		bufpos++;

	if (bufpos >= *eob_cnt_p) return(bufpos);

	/* ignore the #include <> format and # include " " format.
	 * we are only interested in lines of the form:
	 * #include "filespec"
	 */
	if (buffer[bufpos++] != '"') return(bufpos);

	/* find end of filename */
label:	for (k = bufpos ;
	     k < *eob_cnt_p && buffer[k] != '"' && buffer[k] != '\n' ;
	     k++)
		;

	if (buffer[k] == '\n')
		return(k);
	else if (k >= *eob_cnt_p) {
		/* we hit the end of the buffer in the middle of an include
		 * statement.  If the buffer wasn't full, we hit EOF and can
		 * just give up.  On the other hand, if the buffer was full,
		 * we should try to extend the buffer and read in the rest
		 * of the include statment
		 */
		if (*eob_cnt_p < (*bufsiz) -1) return(k);
		if ((buffer=realloc(buffer, (*bufsiz)+1024)) == (char *)NULL){
			fprintf(stderr, "%s: Error extending buffer\n",
				progname);
			exit(-1);
		}
		/* read a little more data */
		*bufsiz += 1024;
		while ( *eob_cnt_p < (*bufsiz)-1 && 
		    (status=fread(&buffer[*eob_cnt_p], 1, (*bufsiz)-1-(*eob_cnt_p), fd)) )
			*eob_cnt_p += status;

		goto label;

	}
	else
		buffer[k] = '\0';

	/* ignore the "...debug.h" files */
	if (k-7 >= bufpos && !strncmp("debug.h", &buffer[k-7], 7))
		return(k);

	if (buffer[bufpos] == '.' && buffer[bufpos+1] == '/') {
		(void)printf("%s%s\n", srcdir, &buffer[bufpos+1]);

		if (strlen(srcdir)+strlen(&buffer[bufpos+1]) < sizeof(name)){
			sprintf(name, "%s%s", srcdir, &buffer[bufpos+1]);
			get_includes(name);
		}

	} else if (buffer[bufpos] == '/') {
		(void)printf("%s\n", &buffer[bufpos]);

		if (strlen(&buffer[bufpos]) < sizeof(name)) {
			strcpy(name, &buffer[bufpos]);
			get_includes(name);
		}
	} else {
		(void)printf("%s/%s\n", incdir, &buffer[bufpos]);

		if (strlen(incdir) + strlen(&buffer[bufpos]) < sizeof(name)) {
			sprintf(name, "%s/%s", incdir, &buffer[bufpos]);
			get_includes(name);
		}

	}


	return(k);
}

/*
 *			G E T _ I N C L U D E S
 */
get_includes(s)
char *s;
{
	int bufpos, eob, newline;
	char *buffer;
	unsigned bufsiz= bsize;
	FILE *fd, *fopen();

	if ((buffer=malloc(bufsiz)) == (char *)0) {
		fprintf(stderr, "%s: Unable to allocate %u bytes for buffer\n",
			progname, bufsiz);
		exit(-1);
	}

	if (debug)
		(void)fprintf(stderr, "Visiting \"%s\"\n", s);

	/* This part might be done in a loop, to match usage message? */

	if ((fd = fopen(s, "r")) == (FILE *)NULL) {
		(void)fprintf(stderr, "%s:  ", progname);
		perror(s);
		exit(-1);
	}

	/* fill buffer */
	for (eob=0 ; eob < bufsiz-1 && 
	    (status=fread(&buffer[eob], 1, bufsiz-1-eob, fd)) ; eob += status)
		;

	if (eob == 0) {
		fprintf(stderr, "%s: Error reading file \"%s\"\n",
				progname, s);
		perror("fread");
		exit(-1);
	}
	
	newline = 1;
	for (bufpos = 0 ; bufpos < eob ; bufpos++) {
		if (newline && !strncmp("#include", &buffer[bufpos], 8))
			bufpos= chkincl(fd, bufpos+8, &eob, buffer, &bufsiz);

		newline = (buffer[bufpos] == '\n');
	}

	(void)fclose(fd);
	if (debug)
		(void)fprintf(stderr, "leaving \"%s\"\n", s);
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
	char *cp;
	char *last_slash=(char *)NULL;

	/* parse command flags, and make sure there are arguments
	 * left over for processing.
	 */
	if ((arg_index = parse_args(ac, av)) >= ac) usage();

	/* if the source file has a path, use that as source directory */
	cp = av[arg_index];
	while( *cp != '\0' )
	{
		if( *cp == '/' )
			last_slash = cp;
		cp++;
	}

	if( last_slash )
	{
		int len;

		len = last_slash - av[arg_index];
		srcdir = (char *)malloc( len + 1 );
		strncpy( srcdir, av[arg_index], len );
		srcdir[len] = '\0';
	}

	if( debug )  {
		if( incdir )  fprintf(stderr, "include dir = '%s'\n", incdir );
		if( srcdir )  fprintf(stderr, "source dir  = '%s'\n", srcdir );
	}

	get_includes(av[arg_index]);
	return(0);
}
