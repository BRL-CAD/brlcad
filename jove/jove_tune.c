/*
 *		J O V E _ T U N E . C
 */

#ifdef SYS5_SGI
char	*TempFile	= "jovetXXXXXX";
#else
char	*TempFile	= "/usr/tmp/jovetXXXXXX";
#endif			/* Where jove should put the temp file */
char	*ProcTmp	= "/tmp/jovepXXXXXX";
			/* Where jove should put process temp files */
char	*BinShell	= "/bin/sh";	/* /bin/sh or /bin/csh */
					/* getenv("SHELL") will overide */
char	*Describe	= "/usr/brl/lib/jove/describe.com";
char	*Findcom	= "/usr/brl/lib/jove/findcom";
char	*Joverc		= "/usr/brl/lib/jove/joverc";
			/* Where to search for the describe command */
int	Dfltmode	= 0644;

#ifdef pdp11
char	*cerrfmt = "\\([^:]*\\):\\([0-9][0-9]*\\):";
	/* Very old pdp11 C error scanf format string for errors of the form
	 * filename:linenum: error message
	 */
#else
# ifdef CRAY
	/* Cray has their own stupid error format */
char	*cerrfmt = " \\* [FW] \\* :\\([^:]*\\): \\([0-9][0-9]*\\):";
	/* C error scanf format string for errors of the form
	 * sp * sp [FW] sp * sp :filename: sp linenum:sp error message.
	 */
# else
char	*cerrfmt = "\"\\([^:]*\\)\", line \\([0-9][0-9]*\\):";
	/* C error scanf format string for errors of the form
	 * "filename", line linenum: error message.
	 * used on all SYSV and all modern BSD systems.
	 */
# endif
#endif

char	*lerrfmt = "\"\\([^:]*\\)\", line \\([0-9][0-9]*\\):";
	/* Lint error for errors of the form
	 * "filename", line linenum: error message.
	 */

