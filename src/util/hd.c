/*				H D 
 *
 *    Give a good ole' CPM style Hex dump of a file or standard input.
 *
 *    Author -
 *	Lee A. Butler	butler@BRL.MIL
 *
 *    Options
 *    h    help
 *    o    offset from begining of data from which to start dump
 *
 *  Distribution Status -
 *	Public Domain, Distribution Unlimitied.
 */
#ifndef lint
static const char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "common.h"

#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif
                                                                                                                                                                            

#include <stdio.h>
#ifdef HAVE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif
#include <ctype.h>

#include "machine.h"

/* declarations to support use of getopt() system call */
static char	*options = "o:";
static char	*progname = "(noname)";

static long offset=0;	 /* offset from begining of file from which to start */

#define DUMPLEN 16    /* number of bytes to dump on one line */

/*
 *    D U M P --- Dump file in hex
 */
void dump(FILE *fd)
{
	register int	i;
	register char	*p;
	int		bytes;
	long		addr = 0L;
	static char	buf[DUMPLEN];    /* input buffer */

	if (offset != 0)  {	/* skip over "offset" bytes first */
	    if (fseek(fd, offset, 0)) {	

	    	/* If fseek fails, try reading our way to the desired offset.
	    	 * The fseek will fail if we're reading from a pipe.
		 */

		addr=0;
		while (addr < offset) {
			if ((i=fread(buf, 1, sizeof(buf), fd)) == 0) {
				fprintf(stderr,"%s: offset exceeds end of input!\n", progname);
				exit(-1);
			}
			else addr += i;
		}
	    } else addr = offset;
	}

	/* dump address, Hex of buffer and ASCII of buffer */
	while ((bytes=fread(buf, 1, sizeof(buf), fd)) > 0) {

		/* print the offset into the file */
		printf("%08lx", addr);

		/* produce the hexadecimal dump */
		for (i=0,p=buf ; i < DUMPLEN ; ++i) {
			if (i < bytes) {
				if (i%4 == 0) printf("  %02x", *p++ & 0x0ff);
				else printf(" %02x", *p++ & 0x0ff);
			}
			else {
				if (i%4 == 0) printf("    ");
				else printf("   ");
			}
		}

		/* produce the ASCII dump */
		printf(" |");
		for (i=0, p=buf ; i < bytes ; ++i,++p) {
			if (isascii(*p) && isprint(*p)) putchar(*p);
			else putchar('.');
		}
		printf("|\n");
		addr += DUMPLEN;
	}
}

/*
 *	U S A G E --- Print helpful message and bail out
 */
void usage(void)
{
	(void) fprintf(stderr,"Usage: %s [-o offset] [file...]\n", progname);
	exit(1);
}

/*    M A I N
 *
 *    Parse arguemnts and  call 'dump' to perform primary task.
 */
int
main(int ac, char **av)
{
	int  c, optlen, files;
	FILE *fd;
	char *eos;
	long newoffset;

	progname = *av;

	/* Get # of options & turn all the option flags off */
	optlen = strlen(options);

	for (c=0 ; c < optlen ; c++)  /* NIL */;

	/* Turn off getopt's error messages */
	opterr = 0;

	/* get all the option flags from the command line */
	while ((c=getopt(ac,av,options)) != EOF)
		if (c == 'o'){
			newoffset = strtol(optarg, &eos, 0);

			if (eos != optarg) 
				offset = newoffset;
			else
				fprintf(stderr,"%s: error parsing offset \"%s\"\n",
					progname, optarg);
		}
		else usage();

	if (offset%DUMPLEN != 0) offset -= offset % DUMPLEN;

	if (optind >= ac ) {
		/* no file left, try processing stdin */
		if (isatty(fileno(stdin))) usage();
		else dump(stdin);
	}
	else {
		/* process each remaining arguments */
		for (files = ac-optind; optind < ac; optind++) {
			if ((fd=fopen(av[optind], "r")) == (FILE *)NULL) {
				perror(av[optind]);
				exit (-1);
			}
			if (files > 1) printf("/**** %s ****/\n", av[optind]);
			dump(fd);
			(void)fclose(fd);
		}
	}
	return 0;
}

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
