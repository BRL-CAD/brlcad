/*	I M O D . C --- program to manipulate files of short ints
 *
 *	Author	Lee Butler	Dec. 19 1988
 *
 *
 *	Options:
 *	h	help
 *	a	add int
 *	s	subtract int
 *	m	multiply int
 *	d	divide int
 */
#include <stdio.h>

/* declarations to support use of getopt() system call */
char *options = "ha:s:m:d:";
char optflags[sizeof(options)];
extern char *optarg;
extern int optind, opterr, getopt();

char *progname = "(noname)";

#define STKSIZE 16	/* Number of operations we can have */
#define EOP	0	/* end of operations */
#define	ADD	1
#define SUB	2
#define MUL	3
#define DIV	4

struct op {
	int n, func;
} stack[STKSIZE];
int stklen=0;

#define BUFSIZ 1024
short sb[BUFSIZ];


/*
 *	D O I T --- Main function of program
 */
void doit(fd)
FILE *fd;
{
	short s;
	int i, n, j;
	
	while ((n=fread(sb, sizeof(*sb), BUFSIZ, stdin)) > 0) {
		for (j=0 ; j < n ; j++) {
			register int tmp = (int)sb[j];

			for (i=0 ; i < stklen ; ++i)
				switch (stack[i].func) {
				case ADD	: tmp += stack[i].n; break;
				case SUB	: tmp -= stack[i].n; break;
				case MUL	: tmp *= stack[i].n; break;
				case DIV	: tmp /= stack[i].n; break;
				default		: fprintf(stderr, "Unknow operation\n"); exit(1); break;
				}
			sb[j] = (short)tmp;
		}
		if (fwrite(sb, sizeof(*sb), n, stdout) != n) {
			fprintf(stderr, "Error writing stdout\n");
			exit(1);
		}
	}
}

/*	O F F S E T
 *
 *	return offset of character c in string s, or strlen(s) if c not in s
 */
int offset(s, c)
char s[], c;
{
	register unsigned int i=0;
	while (s[i] != '\0' && s[i] != c) i++;
	return(i);
}

void usage()
{
	fprintf(stderr, "Usage: %s [-a n -s n -m n -d n ] < file \n", progname);
	exit(1);
}

/*
 *	P U S H
 *
 *	push an operation and an argument onto a stack
 */
void push(f, n)
int f, n;
{
	if (stklen >= STKSIZE) usage();

	stack[stklen].n = n;
	stack[stklen++].func = f;
}

main(ac,av)
int ac;
char *av[];
{
	int  c, optlen;
	FILE *fd, *fopen();

	progname = *av;
	if (ac < 3 || isatty(fileno(stdin))) usage();
	
	/* Get # of options & turn all the option flags off */
	optlen = strlen(options);
	for (c=0 ; c < optlen ; optflags[c++] = '\0');


	/* clear the operation stack */
	for (stklen = 0 ; stklen < STKSIZE ; stklen ++) {
		stack[stklen].n = 0;
		stack[stklen].func = 0;
	}
	stklen = 0;

	/* Turn off getopt's error messages */
	opterr = 0;

	/* get all the option flags from the command line */
	while ((c=getopt(ac,av,options)) != EOF) {
		switch (c) {
		case '?'	: usage(); break;
		case 'a'	: push(ADD, atoi(optarg)); break;
		case 's'	: push(SUB, atoi(optarg)); break;
		case 'm'	: push(MUL, atoi(optarg)); break;
		case 'd'	: push(DIV, atoi(optarg)); break;
		case 'h'	: 
		default		: usage(); break;
		}
	}

	if (optind >= ac) doit(stdin);
	else usage();
}
