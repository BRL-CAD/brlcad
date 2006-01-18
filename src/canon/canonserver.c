/*                   C A N O N S E R V E R . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2006 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this file; see the file named COPYING for more
 * information.
 *
 */
/** @file canonserver.c
 *
 * MDQS interface for Canon CLC500 on SGI SCSI bus
 *
 *	Options
 *	h	help
 */
#include <stdio.h>
#include <ctype.h>
#ifdef HAVE_SYS_WAIT_H
#  include <sys/wait.h>
#endif

#include "queue.h"
#include "qreplys.h"

#define DEBUG
extern char	*Qdatadir;	/* queue file directory name */
static int  lpoptions = 0;	/* line-printer style options */
static char *progname = "(noname)";

static char ipu_prog[64] = "/usr/brlcad/bin/pix-ipu";

int
print(file, copies)
     char *file;
     int copies;
{
#define MAXARGS 200
    char *argv[MAXARGS];
    int argc;
    char linebuf[1024];
    char *p;
    pid_t pid, rpid;
    int retcode;

#ifdef DEBUG
    (void)fprintf(stderr, "Printing %d copies of \"%s\"\n", copies, file);
    (void)fflush(stderr);
#endif
    if ((pid = fork()) == 0) {
	/* child */
	(void)close(3);
	(void)close(4);

	/* open the data file */
	if (freopen(file, "r", stdin) == (FILE *)NULL)
	    exit(2000);

	/* read command line arguments from head of data file */
	fgets(linebuf, sizeof(linebuf)-1, stdin);
	if ( *(p = &linebuf[strlen(linebuf)-1]) == '\n')
	    *p = '\0';

	/* create argv for exec() */
	argc = 1;
	p = linebuf;

	if (strncmp(p, "CLC500", 6)) {
	    fprintf(stderr, "Bad Magic number in image request\n");
	    retmsg( RP_FATAL, "Bad Magic number in image request\n");
	    exit( 10 );
	}
	p += 6;

	while (*p && argc < MAXARGS-1) {
	    /* skip initial white space */
	    while (*p && isascii(*p) && isspace(*p))
		p++;

	    argv[argc++] = p;

	    /* find the end of a word */
	    while (*p && isascii(*p) && !isspace(*p))
		p++;

	    *p++ = '\0';
	}
	argv[argc] = (char *)NULL;
	argv[0] = "pix-ipu";

#ifdef DEBUG
	fprintf(stderr,
		"execl(\"%s\") ", ipu_prog);

	for (argc=0 ; argv[argc] != (char *)NULL ; argc++)
	    fprintf(stderr, ", \"%s\"", argv[argc]);

	fprintf(stderr, ")\n");
	fflush(stderr);
#endif

	/* hack the kernel's idea of my file pointer to be at
	 * the begining of the image data
	 */
	lseek(fileno(stdin), ftell(stdin), 0);

	execv(ipu_prog, argv);
	perror(ipu_prog);
	exit(16);
    }

    while ((rpid = wait(&retcode)) != pid && rpid != -1)
	;

    return(retcode);
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
    int c;
    char ans='\0';
    char	linebuf[LINESIZ];
    char	filename[LINESIZ];
    struct request r;


#ifdef DEBUG
    (void)freopen("/tmp/canonlog", "a", stderr);
#endif
    if (gethdr( stdin, &r ) == NULL ) {
	(void)fprintf(stderr, "%s: \"%s\"\n", progname,
		      "error reading request header");
	retmsg( RP_FATAL, "error reading request header");
	exit( 10 );
    }
    while ( fgets( linebuf, (int)sizeof linebuf, stdin ) != NULL ) {
	register char *cp;

	if ( *(cp = &linebuf[strlen( linebuf ) - 1]) == '\n' )
	    *cp = '\0';

	cp = &linebuf[0];
#ifdef DEBUG
	fprintf(stderr, "%s\n", cp);
#endif
	switch ( *cp++ ) {
	case 'B' :	/* ignore and remove banner */
	    filename[sizeof(filename)-1] = '\0';
	    strncpy(filename, Qdatadir, sizeof(filename)-1);
	    strncat(filename, cp,
		    sizeof(filename)-strlen(filename)-1);

	    if ( unlink(filename) != 0 )
		(void)fprintf(stderr,
			      "couldn't unlink banner \"%s\"\n",
			      filename);
	    break;
	case 'D' :	/* change working directory */
	    if ( chdir( cp ) != 0 ) {
		(void)fprintf(stderr,
			      "%s: chdir error\n", progname);
		(void)chdir( "/tmp" );	/* safe */
	    }
	    break;
	case 'F' :	/* print user file */
	    if (print(cp, r.r_copies)
		&& lpoptions & NOKEEP
		&& unlink( cp ) ) {
		fprintf(stderr,
			"couldn't unlink data file \"%s\"\n",
			cp);
	    }
	    break;
	case 'I':		/* print spooled file */
	    filename[sizeof(filename)-1] = '\0';
	    strncpy(filename, Qdatadir, sizeof(filename)-1);
	    strncat(filename, cp,
		    sizeof(filename)-strlen(filename)-1);

	    print(filename, r.r_copies);
	    if ( unlink( filename ) != 0 )
		fprintf(stderr,
			"couldn't unlink spooled file \"%s\"\n",
			filename );
	    break;
	case 'O':		/* line-printer style options */
	    lpoptions |= atoi( cp );
	    break;
	case 'X':		/* extended options */
	case 'U':		/* user name */
	case 'H':		/* print header */
	case 'T':		/* title */
	default:		/* ignore unknown controls */
	    break;
	}
    }

    retmsg( RP_OK, (char *)0 );

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
