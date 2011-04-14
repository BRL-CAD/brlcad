/*
 *			S C R I P T S O R T . C
 *
 * read an rt/mged animation script and sort it.
 *
 * Author -
 *	Christopher T. Johnson
 *	Geometric Solutions, Inc.
 *	100 Custis St., Suite 2
 *	Aberdeen, MD, 21001
 *
 * Copyright Notice -
 *	This software is Copyright (C) 1994 Geometric Solutions, Inc.
 *	Contributed to the US Army for unlimited distribution.
 *
 */
#undef DEBUG

#include "common.h"

#include <stdlib.h>
#include <string.h>
#include "bio.h"

#include "bu.h"

#include "./tokens.h"


#define OPT_STR "qb:fso:"

#define FLAG_CLEAN	0x1
#define FLAG_SCRIPT	0x2

#define MAGIC	0x0deadbefL


struct  frame {
    struct bu_list	l;

    int	number;
    long	flags;
    long	location;
    long	length;
    char	*text;
    int	tp;
    int	tl;
};


struct bu_list head = {MAGIC, &head, &head};
struct frame globals;

extern FILE *yyin;
extern int yylex(void);


int verbose;		/* print status on stderr */
int specify_base;	/* user specified a base */
int user_base;		/* value of user-specified base */
int force_shell;	/* force shell script for each frame */
int suppress_shell;	/* suppress shell script for each frame */
int frame_offset;	/* offset added to frame numbers */


static void
printframe(struct frame *fp)
{
    fprintf(stdout, "start %d;%s\n", fp->number,
	    (fp->flags & FLAG_CLEAN) ? "clean ;" : "");
    if (fp->text) {
	fprintf(stdout, "%s", fp->text);
    }
    fprintf(stdout, "end;\n");
    if ((force_shell || (fp->flags & FLAG_SCRIPT)) && !suppress_shell) {
	fprintf(stdout, "!end_of_frame.sh %d\n", fp->number);
    }
}


static void
sf(int start, int skip)
{
    int i;
    struct frame *runner;

/*
 * skip to staring point.
 */
    i = 0;
    runner = (struct frame *)head.forw;
    while (&runner->l != &head && i<start) {
	runner = (struct frame *)runner->l.forw;
	i++;
    }
    if (&runner->l == &head) return;
/*
 * now start the main loop.
 */
    while (&runner->l != &head) {
	printframe(runner);
	for (i=0; i<skip; i++) {
	    runner = (struct frame *)runner->l.forw;
	    if (&runner->l == &head) return;
	}
    }
}


void
squirtframes(int base)
{

    sf(0, base);	/* start by outputing every base entries at one */

    while (base > 1 ) {
	sf(base/2, base);
	base /= 2;
    }
}


void addtext(struct frame *fp, char *tp)
{
    char *p;
    int length;
#ifdef DEBUG
    fprintf(stderr, "addtext: %s\n", tp);
#endif
    if (fp->l.magic != MAGIC) abort();
    length = strlen(tp) + 1;	/* length of text string and NULL */
    length += 1;			/* For the Space or newline */
    if (fp->text) {
	length += fp->tp;
    }
    if (length > fp->tl || !fp->text) {
	fp->tl = (length/1024)*1024 + 1024;
	p = (char *) bu_malloc(fp->tl, "text area");
	*p = '\0';

	if (fp->text) {
	    bu_strlcpy(p, fp->text, fp->tl);
	    bu_free(fp->text, "text area");
	}
	fp->text = p;
    }
    bu_strlcat(&fp->text[fp->tp], tp, fp->tl);

    if (*tp == ';') {
	bu_strlcat(&fp->text[fp->tp], "\n", fp->tl);
    } else {
	bu_strlcat(&fp->text[fp->tp], " ", fp->tl);
    }

    fp->tp += strlen(tp)+1;
}
int token = SHELL;

struct frame *getframe(FILE *in)
{
    extern FILE *yyin;
    extern char yytext[];
    struct frame *new;

    yyin = in;

/*
 * If we are not IN a frame, then every thing is part of the globals
 * and gets attached to the head.
 */
    if (token == SHELL) token = yylex();
    while (token != START && (token != 0)) {
	addtext(&globals, yytext);
	token = yylex();
    }
    if (!token) return NULL;
/*
 * The next token MUST be a frame number!
 */
    token = yylex();
    if (!token) return NULL;
    if (token != INT) {
	fprintf(stderr, "getframe: BAD start format. Skipping.\n");
	while ((token=yylex()) != END);
	token = yylex();	/* the semi-colon. */
	return NULL;
    }
/*
 * get a frame and set it up.
 */
    new = (struct frame *) bu_calloc(1, sizeof(struct frame), "struct frame");
    BU_LIST_INIT(&(new->l));
    BU_LIST_MAGIC_SET(&(new->l), MAGIC);
    new->number = atoi(yytext);
    new->number += frame_offset;
/*
 * The next token should be SEMI COLON;
 */
    token = yylex();
    if (!token) {
	new->l.magic = -1;
	bu_free(new, "struct frame");
	return NULL;
    }

    if (token != SEMI) {
	fprintf(stderr, "getframe: Missing semi colon after start %%d.\n");
	fprintf(stderr, "getframe: Inserting semi colon.\n");
    }
/*
 * Now comes the the rest.
 */
    while ((token = yylex()) != END && (token)) {
	if (token == CLEAN) {
	    (void) yylex(); /* skip semi-colon */
	    new->flags |= FLAG_CLEAN;
	} else {
	    addtext(new, yytext);
	    /* Can't concatenate commands to comments. */
	    if (token == COMMENT) {
		addtext(new, "\n");
	    }
	}
    }
    token = yylex();	/* scarf the semi-colon */
    token = yylex();	/* Get the next token.  It could be shell */
    if (token == SHELL) {
	new->flags |= FLAG_SCRIPT;
    }
    if (verbose) {
	fprintf(stderr, "scriptsort: Frame %d(%d)\n", new->number, new->tp);
    }
    return new;
}


void
bubblesort(void)
{
    struct frame *a, *b;

    a = (struct frame *)head.forw;
    while (a->l.forw != &head ) {
	b = (struct frame *)a->l.forw;
	if (a->number > b->number) {
	    BU_LIST_DEQUEUE(&b->l);
	    BU_LIST_INSERT(&a->l, &b->l);
	    if (b->l.back != &head) {
		a = (struct frame *)b->l.back;
	    };
	} else {
	    a=(struct frame *)a->l.forw;
	}
    }
}


void
merge(void)
{
    struct frame *cur, *next;

    for (BU_LIST_FOR(cur, frame, &head)) {
	next = BU_LIST_NEXT(frame, &cur->l);
	if (BU_LIST_IS_HEAD(next, &head)) break;
	if (cur->number == next->number) {
	    if (next->text) addtext(cur, next->text);
	    cur->flags |= next->flags;
	    BU_LIST_DEQUEUE(&next->l);
	    if (next->text) bu_free(next->text, "text area");
	    next->text = NULL;
	    next->l.magic = -1;
	    bu_free(next, "struct frame");
	    cur = BU_LIST_PREV(frame, &cur->l);
	}
    }
}


int
get_args(int argc, char **argv)
{
    int c;
    verbose = 1;
    specify_base = force_shell = suppress_shell = 0;
    frame_offset = 0;
    while ( (c=bu_getopt(argc, argv, OPT_STR)) != -1) {
	switch (c) {
	    case 'q':
		verbose = 0;
		break;
	    case 'b':
		specify_base = 1;
		user_base = atoi(bu_optarg);
		break;
	    case 'f':
		force_shell = 1;
		suppress_shell = 0;
		break;
	    case 's':
		suppress_shell = 1;
		force_shell = 0;
		break;
	    case 'o':
		frame_offset = atoi(bu_optarg);
		break;
	    default:
		fprintf(stderr, "Unknown option: -%c\n", c);
		return 0;
	}
    }
    return 1;
}


/*
 *			M A I N
 */
int
main(int argc, char *argv[])
{
    struct frame *new, *lp;

    int base, count;

    if (!get_args(argc, argv)) {
	return 1;
    }
    if (verbose) fprintf(stderr, "scriptsort: starting.\n");

    BU_LIST_INIT(&head);
    globals.text=NULL;
    globals.tp=globals.tl=0;
    globals.flags=globals.location=globals.length = 0;
    globals.l.magic = MAGIC;

    if (verbose) fprintf(stderr, "scriptsort: reading.\n");

    while ((new=getframe(stdin)) != NULL) {
	BU_LIST_INSERT(&head, &new->l);
    }
    if (verbose) fprintf(stderr, "scriptsort: sorting.\n");
    bubblesort();
    if (verbose) fprintf(stderr, "scriptsort: merging.\n");
    merge();

    if (verbose) fprintf(stderr, "scriptsort: squirting.\n");
    if (specify_base) {
	base = user_base;
    } else {
	base = 1; /* prints frames in natural order */
    }
    if (base <= 0) {
	/*compute base as largest power of 2 less than num of frames*/
	base = 1;
	count = 2;
	for ( BU_LIST_FOR( lp, frame, &head ) ) {
	    if (count-- <= 0) {
		base *= 2;
		count = base - 1;
	    }
	}
    } else {
	unsigned int left, right, mask, bits;
	bits = sizeof(int)*4;		/* assumes 8 bit byte */
	mask = (1<<bits)-1;		/* Makes a low bit mask */
	/* assumes power 2 bytes/int */
	right = base;

	while (bits) {
	    left = (right >> bits) & mask;
	    right = right&mask;
	    if (left && right) {
		fprintf(stderr, "scriptsort: base(%d) not power of two.\n",
			base);
		fprintf(stderr, "left=0x%x, right=0x%x, mask=0x%x, bits=%d\n", left, right, mask, bits);
		base = 1;
		fprintf(stderr, "setting base to %d.", base);
		break;
	    }
	    if (left) right = left;
	    bits = bits >> 1;
	    mask = mask >> bits;
	}
    }

    if (globals.text) {
	fprintf(stdout, "%s", globals.text);
    }
    squirtframes(base);	/* must be a power of 2 */

    return 0;
}

int
yywrap(void) {
    return 1;
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
