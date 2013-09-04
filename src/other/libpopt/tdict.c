#include "system.h"
#include <stdio.h>
#include "popt.h"

static int _debug = 0;
static int _verbose = 1;
static const char * dictfn = "/usr/share/dict/words";
static poptBits dictbits = NULL;
static struct {
    unsigned total;
    unsigned hits;
    unsigned misses;
} e;

static int loadDict(const char * fn, poptBits * ap)
{
    char b[BUFSIZ];
    size_t nb = sizeof(b);
    FILE * fp = fopen(fn, "r");
    char * t, *te;
    int nlines = -1;

    if (fp == NULL || ferror(fp)) goto exit;

    nlines = 0;
    while ((t = fgets(b, nb, fp)) != NULL) {
	while (*t && isspace(*t)) t++;
	if (*t == '#') continue;
	te = t + strlen(t);
	while (te-- > t && isspace(*te)) *te = '\0';
	if (*t == '\0') continue;
	if (ap) {
if (_debug)
fprintf(stderr, "==> poptSaveBits(%p, \"%s\")\n", *ap, t);
	    (void) poptSaveBits(ap, 0, t);
	}
	nlines++;
    }
exit:
    if (fp) (void) fclose(fp);
    return nlines;
}

static struct poptOption options[] = {
  { "debug", 'd', POPT_BIT_SET|POPT_ARGFLAG_TOGGLE, &_debug, 1,
        "Set debugging.", NULL },
  { "verbose", 'v', POPT_BIT_SET|POPT_ARGFLAG_TOGGLE, &_verbose, 1,
        "Set verbosity.", NULL },

  POPT_AUTOALIAS
  POPT_AUTOHELP
  POPT_TABLEEND
};

int main(int argc, const char ** argv)
{
    poptContext optCon = NULL;
    const char ** av = NULL;
    poptBits avbits = NULL;
    int ec = 2;		/* assume failure */
    int rc;

#if defined(HAVE_MCHECK_H) && defined(HAVE_MTRACE)
    mtrace();   /* Trace malloc only if MALLOC_TRACE=mtrace-output-file. */
#endif

    /* XXX Scale the Bloom filters in popt. */
    if ((rc = loadDict(dictfn, NULL)) <= 0)
	goto exit;
    _poptBitsK = 2;
    _poptBitsM = 0;
    _poptBitsN = _poptBitsK * rc;

    optCon = poptGetContext("tdict", argc, argv, options, 0);

    /* Read all the options (if any). */
    while ((rc = poptGetNextOpt(optCon)) > 0) {
        char * optArg = poptGetOptArg(optCon);
	if (optArg) free(optArg);
        switch (rc) {
        default:	goto exit;	break;
        }
    }
    if (rc < -1) {
        fprintf(stderr, "tdict: %s: %s\n",
                poptBadOption(optCon, POPT_BADOPTION_NOALIAS),
                poptStrerror(rc));
	goto exit;
    }

    if ((rc = loadDict(dictfn, &dictbits)) <= 0)
	goto exit;

    av = poptGetArgs(optCon);
    if ((rc = poptBitsArgs(optCon, &avbits)) != 0)
	goto exit;
    if (avbits) {
	poptBits Ibits = NULL;
	(void) poptBitsUnion(&Ibits, dictbits);
	rc = poptBitsIntersect(&Ibits, avbits);
	fprintf(stdout, "===== %s words are in %s\n", (rc ? "Some" : "No"), dictfn);
	if (Ibits) free(Ibits);
    }
    if (av && avbits)
    while (*av) {
	rc = poptBitsChk(dictbits, *av);
	if (rc < 0) goto exit;
	e.total++;
	if (rc > 0) {
	    if (_verbose)
		fprintf(stdout, "%s:\tYES\n", *av);
	    e.hits++;
	} else {
	    if (_verbose)
		fprintf(stdout, "%s:\tNO\n", *av);
	    e.misses++;
	}
	av++;
    }

    ec = 0;

exit:
    fprintf(stdout, "===== poptBits N:%u M:%u K:%u (%uKb) total(%u) = hits(%u) + misses(%u)\n",
	_poptBitsN, _poptBitsM, _poptBitsK, (((_poptBitsM/8)+1)+1023)/1024, e.total, e.hits, e.misses);
    if (avbits) free(avbits);
    optCon = poptFreeContext(optCon);
#if defined(HAVE_MCHECK_H) && defined(HAVE_MTRACE)
    muntrace();   /* Trace malloc only if MALLOC_TRACE=mtrace-output-file. */
#endif
    return ec;
}
