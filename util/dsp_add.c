/*	D S P _ A D D . C --- add 2 files of network unsigned shorts
 *
 *	Options
 *	h	help
 */
#include "conf.h"

#include <stdio.h>

#include "machine.h"
#include "externs.h"
#include "bu.h"
#include "vmath.h"
#include "bn.h"


/* declarations to support use of getopt() system call */
char *options = "hw:n:s:";
extern char *optarg;
extern int optind, opterr, getopt();

char *progname = "(noname)";

int width;
int length;
int conv = 1;	/* flag: do conversion from network to host format */

/*
 *	U S A G E --- tell user how to invoke this program, then exit
 */
void usage(char *s)
{
	if (s) (void)fputs(s, stderr);

	(void) fprintf(stderr, "Usage: %s [ -%s ] dsp_1 dsp_2 > dsp_3\n",
			progname, options);
	exit(1);
}

/*
 *	P A R S E _ A R G S --- Parse through command line flags
 */
int parse_args(int ac, char *av[])
{
	int  c;
	char *strrchr();

	if (  ! (progname=strrchr(*av, '/'))  )
		progname = *av;
	else
		++progname;

	/* Turn off getopt's error messages */
	opterr = 0;

	/* get all the option flags from the command line */
	while ((c=getopt(ac,av,options)) != EOF)
		switch (c) {
		case '?'	:
		case 'w'	:
			c = atoi(optarg);
			if (c > 0) width = c;
			else {
				fprintf(stderr,
				    "%s: Width must be greater than 0 (-w %s)",
					progname, optarg);
				exit(-1);
			}
			break;
		case 'n'	:
			c = atoi(optarg);
			if (c > 0) length = c;
			else {
				fprintf(stderr,
				   "%s: Length must be greater than 0 (-n %s)",
					progname, optarg);
				exit(-1);
			}
			break;
		case 's'	:
			c = atoi(optarg);
			if (c > 0) width = length = c;
			else {
				fprintf(stderr,
				    "%s: size must be greater than 0 (-s %s)",
					progname, optarg);
				exit(-1);
			}
			break;
		case 'h'	:
		default		: usage("Bad or help flag specified\n"); break;
		}

	return(optind);
}

void
read_and_convert(FILE *inp, FILE *in,
		 int count,
		 short *buf1, short *buf2,
		 int in_cookie, int out_cookie)
{
	
	int got;
	short *tmp;

	tmp = (short *)bu_malloc(sizeof(short)*count, "tmp");

	if (fread(tmp, sizeof(short), count, inp) != count) {
		fprintf(stderr, "read error on file 1\n");
		exit(-1);
	}

	got = bu_cv_w_cookie(buf1, out_cookie, count, tmp, in_cookie, count);

	if (got != count) bu_bomb("bu_cv_w_cookie failed\n");

	if (fread(tmp, sizeof(short), count, in) != count) {
		fprintf(stderr, "read error on file 1\n");
		exit(-1);
	}

	got = bu_cv_w_cookie(buf2, out_cookie, count, tmp, in_cookie, count);

	if (got != count) bu_bomb("bu_cv_w_cookie failed\n");

	bu_free(tmp, "tmp");
}




/*
 *	M A I N
 *
 *	Call parse_args to handle command line arguments first, then
 *	process input.
 */
int
main(int ac, char *av[])
{
	int arg_count;
	FILE *inp, *in;
	short *buf1, *buf2, s;
	int count, i, v;
	int in_cookie, out_cookie;

	arg_count = parse_args(ac, av);
	
	if (width == 0 || length == 0) 
		usage("must specify both grid dimensions\n");

	count = width * length;

	if (isatty(fileno(stdout))) usage("Redirect standard output\n");

	if (arg_count >= ac) usage("No files specified\n");


	/* Open the files */

	if ((inp = fopen(av[arg_count], "r")) == (FILE *)NULL) {
		perror(av[arg_count]);
		return -1;
	}
	arg_count++;

	if ((in = fopen(av[arg_count], "r")) == (FILE *)NULL) {
		perror(av[arg_count]);
		return -1;
	}


	buf1 = bu_malloc(sizeof(short)*count, "buf1");
	buf2 = bu_malloc(sizeof(short)*count, "buf1");

	/* Read the terrain data 
	 * Convert from network to host format (unless the user says not to)
	 */

	in_cookie = bu_cv_cookie("nus");
	out_cookie = bu_cv_cookie("hus");

	if (conv && bu_cv_optimize(in_cookie) != bu_cv_optimize(out_cookie))
		read_and_convert(inp, in, count, buf1, buf2, 
				 in_cookie, out_cookie);
	else {
		fread(buf1, sizeof(short), count, inp);
		fread(buf2, sizeof(short), count, in);
	}


	/* add the two datasets together */

	for (i=0; i < count ; i++) {
		v = buf1[i] + buf2[i];
		s = (short)v;

		if (s != v) {
			bu_log("error adding value #%d, (%d+%d)\n",
			       i, buf1[i], buf2[i]);
		}
		buf1[i] = s;
	}

	if (fwrite(buf1, sizeof(short), count, stdout) != count)
		fprintf(stderr, "Error writing data\n");

	return 0;
}
