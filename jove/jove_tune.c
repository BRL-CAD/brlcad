/*
 *		J O V E _ T U N E . C
 */

#include	"./jove.h"

#if ! HAS_TEMPNAM
char	TempFile[]	= "/usr/tmp/jovetXXXXXX";
			/* Where mktemp should put the temp file */

char	ProcTmp[]	= "/usr/tmp/jovepXXXXXX";
			/* Where jove should put process temp files */
#endif

char	BinShell[]	= "/bin/sh";	/* /bin/sh (preferably) or /bin/csh */
					/* getenv("SHELL") will overide */
/* Following paths are edited during the "make" process to create tune.c from jove_tune.c: */
char	Describe[]	= "LibDir/describe.com";
char	Findcom[]	= "BinDir/findcom";
char	Joverc[]	= "LibDir/joverc";
			/* Where to search for the describe command */
int	Dfltmode	= 0644;

#ifdef pdp11
char	cerrfmt[] = "\\([^:]*\\):\\([0-9][0-9]*\\):";
	/* Very old pdp11 C error scanf format string for errors of the form
	 * filename:linenum: error message
	 */
#else
# ifdef CRAY
	/* Cray has their own stupid error format */
char	cerrfmt[] = " \\* [FW] \\* :\\([^:]*\\): \\([0-9][0-9]*\\):";
	/* C error scanf format string for errors of the form
	 * sp * sp [FW] sp * sp :filename: sp linenum:sp error message.
	 */
# else
#    if	(defined(__sgi) && defined(__mips))
char	cerrfmt[] = "[^:]*: [^:]*: \\([^:]*\\), line \\([0-9][0-9]*\\):";
	/* SGI has a new format, too.
	 * accom: Error: filename, line linenum: error message.
	 *         ^___ or "Warning ###" here
	 */
#    else
char	cerrfmt[] = "\"\\([^:]*\\)\", line \\([0-9][0-9]*\\):";
	/* C error scanf format string for errors of the form
	 * "filename", line linenum: error message.
	 * used on all SYSV and all modern BSD systems.
	 */
#    endif
# endif
#endif

char	lerrfmt[] = "\"\\([^:]*\\)\", line \\([0-9][0-9]*\\):";
	/* Lint error for errors of the form
	 * "filename", line linenum: error message.
	 */
