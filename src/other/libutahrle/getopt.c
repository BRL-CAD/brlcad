/*
 * This software is copyrighted as noted below.  It may be freely copied,
 * modified, and redistributed, provided that the copyright notice is
 * preserved on all copies.
 *
 * There is no warranty or other guarantee of fitness for this software,
 * it is provided solely "as is".  Bug reports or fixes may be sent
 * to the author, who may or may not act on them as he desires.
 *
 * You may not include this software in a program or other software product
 * without supplying the source, or without informing the end-user that the
 * source is available for no extra charge.
 *
 * If you modify this software, you should include a notice giving the
 * name of the person performing the modification, the date of modification,
 * and the reason for such modification.
 */
/*
 * From Henry Spencer @ U of Toronto Zoology, slightly edited.
 */

/*
 * getopt - get option letter from argv
 */

#include "rle_config.h"
#include <stdio.h>

char	*optarg;	/* Global argument pointer. */
int	optind = 0;	/* Global argv index. */

static char	*scan = NULL;	/* Private scan pointer. */

extern char	*index();

int
getopt(argc, argv, optstring)
int argc;
char *argv[];
char *optstring;
{
    register char c;
    register char *place;

    optarg = NULL;

    if (scan == NULL || *scan == '\0') {
	if (optind == 0)
	    optind++;

	if (optind >= argc || argv[optind][0] != '-' ||
	    argv[optind][1] == '\0')
	    return( EOF );

	if (strcmp(argv[optind], "--")==0) {
	    optind++;
	    return( EOF );
	}

	scan = argv[optind]+1;
	optind++;
    }

    c = *scan++;
    place = index(optstring, c);

    if (place == NULL || c == ':') {
	fprintf(stderr, "%s: unknown option -%c\n", argv[0], c);
	return( '?' );
    }

    place++;
    if (*place == ':') {
	if (*scan != '\0') {
	    optarg = scan;
	    scan = NULL;
	} else {
	    if (optind >= argc) {
		fprintf(stderr,
			"%s: missing argument after -%c\n",
			argv[0], c);
		return( '?' );
	    }
	    optarg = argv[optind];
	    optind++;
	}
    }

    return( c );
}
