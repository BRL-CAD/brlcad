/*
 *			G E T O P T . C
 *
 *  Important note -
 *	If getopt() it going to be used more than once, it is necessary
 *	to reinitialize optind=1 before beginning on the next argument list.
 *  
 *  Distribution Status -
 *	Public Domain, Distribution Unlimitied.
 */
#ifndef lint
static char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include <stdio.h>

#if defined(SYSV) || defined(CRAY)
#define index(s, c)	strchr(s, c)
extern char	*strchr();
#else
extern char	*index();
#endif

/*
 * get option letter from argument vector
 */
int	opterr = 1;		/* set to zero to suppress errors */
int	optind = 1;		/* index into parent argv vector */
int	optopt;			/* character checked for validity */
char	*optarg;		/* argument associated with option */

#define BADCH	(int)'?'
#define EMSG	""
#define tell(s)	if(opterr)  { \
		fputs(*nargv,stderr);fputs(s,stderr); \
		fputc(optopt,stderr);fputc('\n',stderr); \
	} return(BADCH);

getopt(nargc,nargv,ostr)
int	nargc;
char	**nargv,
	*ostr;
{
	static char	*place = EMSG;	/* option letter processing */
	register char	*oli;		/* option letter list index */

	if(*place=='\0') {			/* update scanning pointer */
		if(optind >= nargc || *(place = nargv[optind]) != '-' ||
		   !*++place)  {
		   	place = EMSG;
			return(EOF);
		}
		if (*place == '-') {	/* found "--" */
			place = EMSG;
			++optind;
			return(EOF);
		}
	}				/* option letter okay? */
	if ((optopt = (int)*place++) == (int)':' || !(oli = index(ostr,optopt))) {
		if(*place == '\0') {
			++optind;
			place = EMSG;
		}
		tell(": illegal option -- ");
	}
	if (*++oli != ':') {		/* don't need argument */
		optarg = NULL;
		if (*place == '\0') {
			++optind;
			place = EMSG;
		}
	}
	else {				/* need an argument */
		if (*place) optarg = place;	/* no white space */
		else if (nargc <= ++optind) {	/* no arg */
			place = EMSG;
			tell(": option requires an argument -- ");
		}
	 	else optarg = nargv[optind];	/* white space */
		place = EMSG;
		++optind;
	}
	return(optopt);			/* dump back option letter */
}
