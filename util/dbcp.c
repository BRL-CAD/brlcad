/*
 *	dbcp.c
 *
 *	Double-buffered copy program for V7
 *
 *	Doug Kingston @ DPW
 *
 *	Compile:  cc dbcp.c -o dbcp -i -O -7
 *	Usage:    dbcp {nblocks} < inputfile > outputfile
 */

#include	<stdio.h>

#define	STOP	0170
#define	GO	0017

struct pipefds {
	int	p_rd;
	int	p_wr;
};

int	pid;
long	count;

char	errbuf[BUFSIZ];

extern char *malloc();
extern int errno;

main (argc, argv)
int	argc;
char	**argv;
{
	register char	*buffer;
	register unsigned int	size;
	register unsigned int	nread;
	int	rfd;		/* pipe to read message from */
	int	wfd;		/* pipe to write message to */
	int	exitval;
	int	saverrno;
	char	msgchar;
	struct pipefds par2chld, chld2par;

	if (argc != 2) {
		fprintf(stderr, "Usage:  %s blocksize < input > output\n", argv[0]);
		fprintf(stderr, "        (blocksize = number of 512 byte sectors)\n");
		exit(1);
	}
	size = 512 * atoi(argv[1]);

	setbuf (stderr, errbuf);
	if ((buffer = malloc(size)) == NULL) {
		fprintf(stderr, "dbcp: Insufficient buffer memory\n");
		exit (88);
	}
	if (pipe (&par2chld) < 0 || pipe (&chld2par) < 0) {
		perror ("dbcp: Can't pipe");
		exit (89);
	}

	switch (pid = fork()) {
	case -1:
		perror ("dbcp: Can't fork");
		exit (99);

	case 0:
		/*  Child  */
		close (par2chld.p_wr);
		close (chld2par.p_rd);
		wfd = chld2par.p_wr;
		rfd = par2chld.p_rd;
		msgchar = GO;		/* Prime the pump, so to speak */
		goto childstart;

	default:
		/*  Parent  */
		close (par2chld.p_rd);
		close (chld2par.p_wr);
		wfd = par2chld.p_wr;
		rfd = chld2par.p_rd;
		break;
	}

	exitval = 0;
	count = 0L;
	while (1) {
		if ((nread = read (0, buffer, size)) != size) {
			saverrno = errno;
			msgchar = STOP;
		} else
			msgchar = GO;
		if(write (wfd, &msgchar, 1) != 1) {
			perror("dbcp: message send");
			prs("Can't send READ message\n");
		}
		if ((int)nread == (-1)) {
			errno = saverrno;
			perror ("input read");
			prs("read error on input\n");
			break;
		}
		if(nread == 0) {
			prs("EOF on input\n");
			break;
		}
		if(nread != size)
			prs("partial read (nread = %u)\n", nread);
		if (read(rfd, &msgchar, 1) != 1) {
			perror("dbcp: WRITE message error");
			exitval = 69;
			break;
		}
		if (msgchar == STOP) {
			prs("Got STOP WRITE with %u left\n", nread);
			break;
		} else if (msgchar != GO) {
			prs("Got bad WRITE message 0%o\n", msgchar&0377);
			exitval = 19;
			break;
		}
		if (write(1, buffer, nread) != nread) {
			perror("output write");
			msgchar = STOP;
		} else {
			count++;
			msgchar = GO;
		}
		if (nread != size)
			break;
childstart:
		if (write (wfd, &msgchar, 1) != 1) {
			perror("dbcp: message send");
			prs("Can't send WRITE message\n");
			break;
		}
		if (msgchar == STOP) {
			prs ("write error on output\n");
			break;
		}
		if (read(rfd, &msgchar, 1) != 1) {
			perror("dbcp: READ message error");
			exitval = 79;
			break;
		}
		if (msgchar == STOP) {
			prs("Got STOP READ\n");
			break;
		} else if (msgchar != GO) {
			prs("Got bad READ message 0%o\n", msgchar&0377);
			exitval = 39;
			break;
		}
	}

	prs ("%ld records copied\n", count);
	if(pid)
		while (wait(&saverrno) > 0);	/* rip off saverrno */
	exit(exitval);
}

prs (fmt, a, b, c)
char	*fmt, *a, *b, *c;
{
	fprintf(stderr, "dbcp: (%s) ", pid ? "PARENT" : "CHILD");
	fprintf(stderr, fmt, a, b, c);
	fflush(stderr);
}
