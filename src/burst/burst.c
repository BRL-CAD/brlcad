/*                         B U R S T . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2011 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 *
 */
/** @file burst/burst.c
 *
 */

#include "common.h"

#include <assert.h>
#include <stdio.h>
#include <signal.h>

#include "./burst.h"
#include "./trie.h"
#include "./ascii.h"
#include "./extern.h"

#ifndef SIGCLD
#  define SIGCLD SIGCHLD
#endif
#ifndef SIGTSTP
#  define SIGTSTP 18
#endif

#define DEBUG_BURST 0 /* 1 enables debugging for this module */

/*
  int getCommand(char *name, char *buf, int len, FILE *fp)

  Read next command line into buf and stuff the command name into name
  from input stream fp.  buf must be at least len bytes long.

  RETURN:	1 for success

  0 for end of file
*/
static int
getCommand(char *name, char *buf, int len, FILE *fp)
{
    assert(name != NULL);
    assert(buf != NULL);
    assert(fp != NULL);
    while (bu_fgets(buf, len, fp) != NULL) {
	if (buf[0] != CHAR_COMMENT) {
	    if (sscanf(buf, "%1330s", name) == 1) {
		/* LNBUFSZ */
		buf[strlen(buf)-1] = NUL; /* clobber newline */
		return 1;
	    } else /* Skip over blank lines. */
		continue;
	} else {
	    /* Generate comment command. */
	    bu_strlcpy(name, CMD_COMMENT, LNBUFSZ);
	    return 1;
	}
    }
    return 0; /* EOF */
}


/*
  void setupSigs(void)

  Initialize all signal handlers.
*/

static void
setupSigs(void)
{
    int i;
    for (i = 0; i < NSIG; i++)
	switch (i) {
	    case SIGINT :
		if ((norml_sig = signal(i, SIG_IGN)) == SIG_IGN)
		    abort_sig = SIG_IGN;
		else {
		    norml_sig = intr_sig;
		    abort_sig = abort_RT;
		    (void) signal(i,  norml_sig);
		}
		break;
	    case SIGCHLD :
		break; /* leave SIGCLD alone */
	    case SIGPIPE :
		(void) signal(i, SIG_IGN);
		break;
	    case SIGQUIT :
		break;
	    case SIGTSTP :
		break;
	}
    return;
}


/*
  int parsArgv(int argc, char **argv)

  Parse program command line.
*/
static int
parsArgv(int argc, char **argv)
{
    int c;
/* Parse options.						*/
    while ((c = bu_getopt(argc, argv, "b")) != -1) {
	switch (c) {
	    case 'b' :
		tty = 0;
		break;
	    case '?' :
		return 0;
	}
    }
    return 1;
}


/*
  void readBatchInput(FILE *fp)

  Read and execute commands from input stream fp.
*/
void
readBatchInput(FILE *fp)
{
    assert(fp != (FILE *) NULL);
    batchmode = 1;
    while (getCommand(cmdname, cmdbuf, LNBUFSZ, fp)) {
	Func *cmdfunc;
	if ((cmdfunc = getTrie(cmdname, cmdtrie)) == NULL) {
	    int i, len = strlen(cmdname);
	    brst_log("ERROR -- command syntax:\n");
	    brst_log("\t%s\n", cmdbuf);
	    brst_log("\t");
	    for (i = 0; i < len; i++)
		brst_log(" ");
	    brst_log("^\n");
	} else
	    if (BU_STR_EQUAL(cmdname, CMD_COMMENT)) {
		/* special handling for comments */
		cmdptr = cmdbuf;
		cmdbuf[strlen(cmdbuf)-1] = '\0'; /* clobber newline */
		(*cmdfunc)((HmItem *) 0);
	    } else {
		/* Advance pointer past nul at end of
		   command name. */
		cmdptr = cmdbuf + strlen(cmdname) + 1;
		(*cmdfunc)((HmItem *) 0);
	    }
    }
    batchmode = 0;
    return;
}


/*
  int main(int argc, char *argv[])
*/
int
main(int argc, char *argv[])
{
    bu_setlinebuf(stderr);

    tmpfp = bu_temp_file(tmpfname, TIMER_LEN);
    if (!tmpfp) {
	bu_exit(EXIT_FAILURE, "ERROR: Unable to create temporary file.\n");
    }
    if (! parsArgv(argc, argv)) {
	prntUsage();
	return EXIT_FAILURE;
    }

    setupSigs();
    if (! initUi()) /* must be called before any output is produced */
	return EXIT_FAILURE;

#if DEBUG_BURST
    prntTrie(cmdtrie, 0);
#endif
    assert(airids.i_next == NULL);
    assert(armorids.i_next == NULL);
    assert(critids.i_next == NULL);

    if (! isatty(0) || ! tty)
	readBatchInput(stdin);
    if (tty)
	(void) HmHit(mainhmenu);
    exitCleanly(EXIT_SUCCESS);

    /* not reached */
    return EXIT_SUCCESS;
}


/*
  void exitCleanly(int code)

  Should be only exit from program after success of initUi().
*/
void
exitCleanly(int code)
{
    if (tty)
	closeUi(); /* keep screen straight */
    (void) fclose(tmpfp);
    if (unlink(tmpfname) == -1)
	locPerror(tmpfname);
    exit(code);
}


/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
