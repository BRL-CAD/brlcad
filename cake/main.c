/*
**	Cake main file.
*/

static	char
rcs_id[] = "$Header$";

#include	"cake.h"
#if __STDC__
# include <unistd.h>
#else
# if defined(__sgi) && defined(__mips)
	extern	unsigned short	geteuid();
# endif
#endif
#include	<pwd.h>
#include	<signal.h>
#include	<sys/stat.h>

typedef	struct	passwd	Pwent;
typedef	struct	stat	Stat;

#if !defined(__convex__) && !__STDC__ && !__EXTENSIONS__
extern	Pwent	*getpwuid();
#endif

#if __STDC__ && !defined(CRAY2)
extern char	*tempnam(const char *, const char *);
#else
# if defined(ATT)
extern char	*tempnam();
# else
/* On non-ANSI BSD systems, use mktemp instead */
#  define USE_MKTEMP	1
static char	template[] = "/tmp/cakeXXXXX";
# endif
#endif

int	Gflag = FALSE;
int	Lflag = FALSE;
int	Rflag = FALSE;
int	Xflag = FALSE;
int	Zflag = FALSE;
int	bflag = FALSE;
int	cflag = FALSE;
int	dflag = FALSE;
int	gflag = FALSE;
int	iflag = FALSE;
int	kflag = TRUE;
int	nflag = FALSE;
int	qflag = FALSE;
int	rflag = FALSE;
int	sflag = FALSE;
int	tflag = FALSE;
int	vflag = FALSE;
int	wflag = FALSE;
int	xflag = FALSE;
int	zflag = FALSE;

char	*cakefile  = NULL;		/* also used in entry.c */
char	*shellfile[2] = { SYSTEM_CMD, SCRIPT_CMD };
char	*metachars = METACHARS;
int	maxprocs   = 1;
List	*active_procs;
char	scratchbuf[128];

int	cakedebug	= FALSE;
int	entrydebug	= FALSE;
int	patdebug	= FALSE;
int	lexdebug	= FALSE;

char	cakeflagbuf[MAXSIZE];
char	*cppargv[MAXARGS];
int	cppargc = 0;

#if defined(YACC_DEBUG)
extern	int	yydebug;
#endif

extern	void	statistics();


/*
**	Tell the unfortunate user how to use cake.
*/
void
usage()
{
	fprintf(stderr, "Usage: cake [-abcdgiknqrstvwxzGLRXZ] [-ffile]\n");
	fprintf(stderr, "       [-Ddefn] [-Idir] [-Uname] [-S shell] [-T metachars] [file ...]\n");
	exit(1);
}

int
process_args(vector, count, base)
reg	char	**vector;
reg	int	*count;
reg	int	base;
{
	reg	int	i, j;

	j = 0;
	cdebug("process args:");
	while (*count > base && vector[base][0] == '-')
	{
		putflag(base, vector[base]);

		for (i = 1; vector[base][i] != '\0'; i++)
		{
			switch (vector[base][i])
			{

#ifdef	CAKEDEBUG
		case 'C':	cdebug(" -C");
				cakedebug  = ! cakedebug;
				break;
		
		case 'E':	cdebug(" -E");
				entrydebug = ! entrydebug;
				break;
		
		case 'P':	cdebug(" -P");
				patdebug   = ! patdebug;
				break;
		
		case 'W':	cdebug(" -W");
				lexdebug   = TRUE;
				break;
		
#if defined(YACC_DEBUG)
		case 'Y':	cdebug(" -Y");
				yydebug    = TRUE;
				break;
#endif /* YACC_DEBUG */
#endif /* CAKEDEBUG */
		case 'G':	cdebug(" -G");
				Gflag = TRUE;
				break;
		
		case 'L':	cdebug(" -L");
				Lflag = TRUE;
				break;
		
		case 'R':	cdebug(" -R");
				Rflag = TRUE;
				break;
		
		case 'X':	cdebug(" -X");
				Xflag = TRUE;
				break;
		
		case 'Z':	cdebug(" -Z");
				Zflag = TRUE;
				break;
		
		case 'a':	cdebug(" -a");
				kflag = FALSE;
				break;
		
		case 'b':	cdebug(" -b");
				bflag = TRUE;
				break;
		
		case 'c':	cdebug(" -c");
				cflag = TRUE;
				break;
		
		case 'd':	cdebug(" -d");
				dflag = TRUE;
				break;
		
		case 'g':	cdebug(" -g");
				gflag = TRUE;
				break;
		
		case 'i':	cdebug(" -i");
				iflag = TRUE;
				break;
		
		case 'k':	cdebug(" -k");
				kflag = TRUE;
				break;
		
		case 'n':	cdebug(" -n");
				nflag = TRUE;
				tflag = FALSE;
				qflag = FALSE;
				break;
		
		case 'q':	cdebug(" -q");
				qflag = TRUE;
				nflag = FALSE;
				tflag = FALSE;
				break;
		
		case 'r':	cdebug(" -r");
				rflag = TRUE;
				break;
		
		case 's':	cdebug(" -s");
				sflag = TRUE;
				break;
		
		case 't':	cdebug(" -t");
				tflag = TRUE;
				nflag = FALSE;
				qflag = FALSE;
				break;
		
		case 'v':	cdebug(" -v");
				vflag = TRUE;
				break;
		
		case 'w':	cdebug(" -w");
				wflag = TRUE;
				break;
		
		case 'x':	cdebug(" -x");
				xflag = TRUE;
				break;
		
		case 'z':	cdebug(" -z");
				zflag = TRUE;
				break;
		
		case 'D':
		case 'I':
		case 'U':	if (i != 1)
					usage();

				cdebug(" %s", vector[base]);
				cppargv[cppargc++] = new_name(vector[base]);
				goto nextword;
		
		case 'N':	putflag(base, vector[base+1]);
				sscanf(vector[base+1], "%d", &maxprocs);
				if (vector[base][i+1] != '\0')
					usage();

				cdebug(" -N %d", maxprocs);
				(*count)--;
				vector++, j++;
				goto nextword;
		
		case 'S':	putflag(base, vector[base+1]);
				if (vector[base][i+1] == '1')
					shellfile[0] = new_name(vector[base+1]);
				or (vector[base][i+1] == '2')
					shellfile[1] = new_name(vector[base+1]);
				else
					usage();

				if (vector[base][i+2] != '\0')
					usage();

				cdebug(" -S%c %s", vector[base][i+1], vector[base+1]);
				(*count)--;
				vector++, j++;
				goto nextword;
		
		case 'T':	putflag(base, vector[base+1]);
				metachars = new_name(vector[base+1]);
				if (vector[base][i+1] != '\0')
					usage();

				cdebug(" -T %s", metachars);
				(*count)--;
				vector++, j++;
				goto nextword;
		
		case 'f':	putflag(base, vector[base+1]);
				cakefile = new_name(vector[base+1]);
				if (vector[base][i+1] != '\0')
					usage();

				cdebug(" -f %s", cakefile);
				(*count)--;
				vector++, j++;
				goto nextword;

		default:	usage();
			}
		}

nextword:
		(*count)--;
		vector++, j++;
	}

	cdebug(" \nreturn %d\n", j);	/* BRL MOD */
	return j;
}

/*
**	Put a flag into the CAKEFLAGS definition.
*/
void
putflag(base, flag)
reg	int	base;
reg	char	*flag;
{
	if (base == 1)
	{
		strcat(cakeflagbuf, " ");
		strcat(cakeflagbuf, flag);
		if (strlen(cakeflagbuf) >= (unsigned)MAXSIZE)
		{
			fprintf(stderr, "cake: CAKEFLAGS too long\n");
			exit(1);
		}
	}
}

void
exit_cake(needtrail)
reg	int	needtrail;
{
	if (cakedebug && needtrail)
		get_trail(stdout);
	else
		dir_finish();

	exit(1);
}

/*
**	Handle bus errors and segmentation violations.
*/
void
cake_abort(signo)
int	signo;
{

	signal(SIGINT,  SIG_IGN);
	signal(SIGQUIT, SIG_IGN);

	printf("cake: aborting on signal %d\n", signo);
	if (cakedebug)
		get_trail(stdout);

	signal(SIGQUIT, SIG_DFL);
	kill(getpid(), SIGQUIT);
}

/*
**	Handle user interrupts.
*/

void
cake_finish(sig)
int	sig;
{
	reg	List	*ptr;
	reg	Proc	*proc;

	signal(SIGINT,  SIG_IGN);
	signal(SIGQUIT, SIG_IGN);

	printf("*** Interrupt\n");
	fflush(stdout);
	for_list (ptr, active_procs)
	{
		proc = (Proc *) ldata(ptr);
		if (proc->pr_node != NULL)
			cake_error(proc->pr_node);
	}

	exit_cake(FALSE);
}

main(argc, argv)
int	argc;
char	**argv;
{
	Stat		statbuf;
	int		envc;
	char		*envv[MAXARGS];
	reg	Pwent	*pwent;
	reg	char	*envstr;
	reg	int	status;
	reg	Node	*rootnode;
	char		*newcakefile;		/* /tmp/cakef###.c, for CC -E */

	signal(SIGINT,  cake_finish);
	signal(SIGQUIT, cake_finish);
	if(cakedebug)  {
		/* If debugging enabled, catch signals to do get_trail() */
		signal(SIGILL,  cake_abort);
		signal(SIGTRAP, cake_abort);
		signal(SIGIOT,  cake_abort);
#ifdef SIGEMT
		signal(SIGEMT,  cake_abort);
#endif
		signal(SIGFPE,  cake_abort);
		signal(SIGBUS,  cake_abort);
		signal(SIGSEGV, cake_abort);
#ifdef SIGSYS
		signal(SIGSYS,  cake_abort);
#endif
	}
	signal(SIGPIPE, cake_abort);
	signal(SIGALRM, cake_abort);

#if defined(YACC_DEBUG)
	yydebug = FALSE;
#endif
	active_procs = makelist0();

	if (rindex(argv[0], 'f') != NULL
	&&  streq(rindex(argv[0], 'f'), "fake"))
		cakedebug = TRUE;

	init_sym();
	cppargv[cppargc++] = new_name(CPP);
#if defined(CPP_OPTIONS)
	cppargv[cppargc++] = new_name(CPP_OPTIONS);	
#endif
#if defined(CPP_OPTIONS2)
	cppargv[cppargc++] = new_name(CPP_OPTIONS2);
#endif
#if defined(CPP_OPTIONS3)
	cppargv[cppargc++] = new_name(CPP_OPTIONS3);
#endif
	strcpy(cakeflagbuf, "-DCAKEFLAGS=");

	if ((envstr = getenv("CAKE")) != NULL)
	{
		envc = parse_args(envstr, envv);
		process_args(envv, &envc, 0);
		if (envc > 0)
			fprintf(stderr, "cake: non-options in environment ignored\n");
	}

	argv += process_args(argv, &argc, 1);

#if defined(CAKEDEBUG) && !defined(ATT)
	if (cakedebug || entrydebug || patdebug || lexdebug)
		setlinebuf(stdout);
#endif

	if (cakefile == NULL)
	{
		if (stat("cakefile", &statbuf) == 0)
			cakefile = "cakefile";
		or (stat("Cakefile", &statbuf) == 0)
			cakefile = "Cakefile";
		or (stat("recipe", &statbuf) == 0)
			cakefile = "recipe";
		or (stat("Recipe", &statbuf) == 0)
			cakefile = "Recipe";
		else
		{
			fprintf(stderr, "cake: cannot locate a cakefile\n");
			exit(1);
		}
	}

	if (gflag)
		cakefile = dir_setup(cakefile);		/* changes directory */

	/*
	 *  In order to use more modern CC -E commands, the name of
	 *  the Cakefile string has to end in a ".c".
	 *  So, copy the cakefile into /tmp, and pre-process that one.
	 *  -Mike Muuss, ARL, 25-Aug-93.
	 */
	{
		char	*newbase;
		char	cmd[256];

#if USE_MKTEMP
		(void)mktemp(template);
		newbase = template;
#else
		newbase = tempnam((char *)NULL, "cakef");
		if( newbase == NULL )  exit(17);
#endif
		newcakefile = malloc(strlen(newbase)+3);	/* room for .c */
		if( newcakefile == NULL )  exit(18);
		strcpy( newcakefile, newbase );
		strcat( newcakefile, ".c" );
		free(newbase);

		sprintf(cmd, "cp %s %s", cakefile, newcakefile);
		system(cmd);
	}

	if( (pwent = getpwuid(geteuid())) == (Pwent *)0 )  {
		printf("cake: Warning: unable to get home directory for uid %d\n",
			geteuid() );
	} else {
		strcpy(scratchbuf, "-I");
		strcat(scratchbuf, pwent->pw_dir);
		strcat(scratchbuf, ULIB);
		cppargv[cppargc++] = new_name(scratchbuf);
	}
	strcpy(scratchbuf, "-I");
	strcat(scratchbuf, SLIB);
	cppargv[cppargc++] = new_name(scratchbuf);
	cppargv[cppargc++] = cakeflagbuf;
#if 0
	cppargv[cppargc++] = cakefile;
#else
	cppargv[cppargc++] = "-I.";
	cppargv[cppargc++] = newcakefile;
#endif
	cppargv[cppargc]   = NULL;

	if (cakedebug)
	{
		reg	int	i;

		for (i = 0; i < cppargc; i++)
			printf("%s ", cppargv[i]);
		printf("\n");
	}

	if ((yyin = cake_popen(cppargv, "r")) == NULL)
	{
		fprintf(stderr, "cake: cannot open cpp filter\n");
		(void)unlink(newcakefile);
		exit(1);
	}
 

	if (Zflag)
	{
		reg	int	c;

		while ((c = getc(yyin)) != EOF)
			putchar(c);

		cake_pclose(yyin);
		(void)unlink(newcakefile);
		exit(0);
	}

	yyinit();
	init_entry();
	if (yyparse())
	{
		fprintf(stderr, "cake: cannot parse %s\n", cakefile);
		exit(1);
	}

	shell_setup(shellfile[0], 0);
	shell_setup(shellfile[1], 1);
	meta_setup(metachars);

	cake_pclose(yyin);
	(void)unlink(newcakefile);

	dir_start();
	prep_entries();
	final_entry(argc, argv);

	rootnode = chase(CHASEROOT, 0, (Entry *) NULL);

	if (! qflag)
		execute(rootnode);
	
	dir_finish();
	cleanup();
#ifdef	STATS_FILE
	statistics();
#endif

	status = (off_node(rootnode, nf_ERR) && is_ok(rootnode))? 0: 1;
	cdebug("exit status %d\n", status);
	exit(status);
}



#ifdef	STATS_FILE
#ifdef	ATT
#include	<sys/times.h>

typedef	struct	tms	Tms;
#else
#include	<sys/time.h>
#include	<sys/resource.h>

typedef	struct	rusage	Rusage;
#endif

void
statistics()
{
	extern	char	*getlogin();
#ifdef ANTIQUE
	extern		getpw();	/* ancient */
#endif

	extern	int	out_tried, out_found;
	extern	int	stat_tried, stat_found;
	FILE		*sfp;

	if ((sfp = fopen(STATS_FILE, "a")) != NULL)
	{
#ifdef	ATT
		Tms	tbuf;
#else
		Rusage	s, c;
#endif
		long	su, ss, cu, cs;
		char	*usr;

		if ((usr = getlogin()) == NULL)
		{
#ifdef ANTIQUE
			char	buf[256];
			char	*usr_end;

			if (getpw(getuid(), buf) != 0)

				usr = "NULL";
			else
			{
				usr = buf;
				if ((usr_end = index(usr, ':')) != NULL)
					*usr_end = '\0';
				else
					usr = "NULL";
			}
#else
			struct	passwd *pwent;

			if ((pwent = getpwent()) == NULL)
				usr = "NULL";
			else
				usr = pwent->pw_name;
#endif
			usr = new_name(usr);
		}

#ifdef	ATT
		if (times(&tbuf) == -1)
		{
			fclose(sfp);
			return;
		}

		su = tbuf.tms_utime*100/TIMERES;
		ss = tbuf.tms_stime*100/TIMERES;
		cu = tbuf.tms_cutime*100/TIMERES;
		cs = tbuf.tms_cstime*100/TIMERES;
#else
		getrusage(RUSAGE_SELF,     &s);
		getrusage(RUSAGE_CHILDREN, &c);

		su = s.ru_utime.tv_sec*100 + s.ru_utime.tv_usec/10000;
		ss = s.ru_stime.tv_sec*100 + s.ru_stime.tv_usec/10000;
		cu = c.ru_utime.tv_sec*100 + c.ru_utime.tv_usec/10000;
		cs = c.ru_stime.tv_sec*100 + c.ru_stime.tv_usec/10000;
#endif
		fprintf(sfp, "%s %ld %ld %ld %ld ", usr, su, ss, cu, cs);
		fprintf(sfp, "%d %d %d %d %d\n", sbrk(0),
			out_tried, out_found, stat_tried, stat_found);

		fclose(sfp);
	}
}
#endif
