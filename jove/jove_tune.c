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

char	*cerrfmt = "\\([^:]*\\):\\([0-9][0-9]*\\):";
	/* C error scanf format string for errors of the form
	 * filename:linenum: error message
	 */

char	*lerrfmt = "\"\\([^:]*\\)\", line \\([0-9][0-9]*\\):";
	/* Lint error for errors of the form
	 * "filename", line linenum: error message.
	 */

