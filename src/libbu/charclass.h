/*                     C H A R C L A S S . H
 *
 * Based off of OpenBSD's charclass.h v 1.1 2008/10/01
 *
 * Public domain, 2008, Todd C. Miller <Todd.Miller@courtesan.com>
 *
 * POSIX character class support for fnmatch() and glob().
 */

#include "common.h"

#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include "bio.h"

/* isblank appears to be obsolete in newer ctype.h files so use
 * fnblank instead when looking for the "blank" character class.
 */
static inline int
fnblank(int c)
{
#ifdef isblank
    return isblank((unsigned char)c);
#else
    return c == ' ' || c == '\t';
#endif
}


static inline int
fnalnum(int c)
{
    return isalnum((unsigned char)c);
}


static inline int
fnalpha(int c)
{
    return isalpha((unsigned char)c);
}


static inline int
fncntrl(int c)
{
    return iscntrl((unsigned char)c);
}


static inline int
fndigit(int c)
{
    return isdigit((unsigned char)c);
}


static inline int
fngraph(int c)
{
    return isgraph((unsigned char)c);
}


static inline int
fnlower(int c)
{
    return islower((unsigned char)c);
}


static inline int
fnprint(int c)
{
    return isprint((unsigned char)c);
}


static inline int
fnpunct(int c)
{
    return ispunct((unsigned char)c);
}


static inline int
fnspace(int c)
{
    return isspace((unsigned char)c);
}


static inline int
fnupper(int c)
{
    return isupper((unsigned char)c);
}


static inline int
fnxdigit(int c)
{
    return isxdigit((unsigned char)c);
}


typedef struct _charclass {
    const char *idstring;	/* identifying string */
    int (*checkfun)(int);	/* testing function */
} CHARCLASS;

static const struct cclass {
	const char *name;
	int (*isctype)(int);
} cclasses[] = {
	{ "alnum",	fnalnum },
	{ "alpha",	fnalpha },
	{ "blank",	fnblank },
	{ "cntrl",	fncntrl },
	{ "digit",	fndigit },
	{ "graph",	fngraph },
	{ "lower",	fnlower },
	{ "print",	fnprint },
	{ "punct",	fnpunct },
	{ "space",	fnspace },
	{ "upper",	fnupper },
	{ "xdigit",	fnxdigit },
	{ NULL,		NULL }
};

#define NCCLASSES	(sizeof(cclasses) / sizeof(cclasses[0]) - 1)

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
