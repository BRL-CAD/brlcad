/*
**	Cake interface to the other processes.
*/

static	char
rcs_id[] = "$Header$";

#include	"cake.h"
#include	<ctype.h>
#include	<errno.h>
#include	<signal.h>
#include	<fcntl.h>
#if defined(vax) && defined(BSD)
# undef BSD	/* is defined in sys/param.h */
#endif
#include	<sys/param.h>

extern	List	*active_procs;		/* of Proc	*/

/*
**	Process the given command in the manner described by the args.
*/

int
cake_proc(cmd, type, file, node, func, args)
reg	char	*cmd;
reg	A_kind	type;
reg	char	*file;
reg	Node	*node;
reg	int	(*func)();
reg	List	*args;
{
	char		*argv[MAXARGS];
	reg	Proc	*proc;
	reg	int	pid;
	reg	char	*script_filename;

	cmd = strip_backslash(cmd);

#ifdef	CAKEDEBUG
	if (file != NULL)
		cdebug("file is %s\n", file);
	else
		cdebug("file is NULL\n");
#endif

	if (type == Exec && has_meta(cmd, TRUE))
		type = System;

	cdebug("cake_proc, type %d: %s\n", type, cmd);
	fflush(stdout);

	if (type == Exec)
	{
		if( parse_args(cmd, argv) < 1)  {		/* BRL */
			printf("cake_proc:  Null exec command!\n");
			exit_cake(FALSE);
		}
	}
	or (type == Script)
	{
		reg	FILE	*script_fp;

		script_filename = get_newname();
		if ((script_fp = fopen(script_filename, "w")) == NULL)
		{
			sprintf(scratchbuf, "cake system error, fopen %s", script_filename);
			perror(scratchbuf);
			exit_cake(FALSE);
		}

		fprintf(script_fp, "%s", cmd);
		fclose(script_fp);
	}

	mutex_lock();
	if ((pid = fork()) == 0)
	{
		reg	int	fd;

#ifdef	CLOSE_ALL
		for (fd = 3; fd < NOFILE; fd++)
			close(fd);
#endif

		if (file != NULL)
		{
			if (close(1) != 0)
			{
				perror("cake system error, close stdout");
				_exit(127);
			}

#ifdef ATT
			if ((fd = open(file, O_WRONLY|O_CREAT, 0600)) < 0)
#else
			if ((fd = creat(file, 0600)) < 0)	/* BRL */
#endif
			{
				perror("cake system error, reopen stdout");
				_exit(127);
			}

			if (fd != 1)
			{
				fprintf(stderr, "cake system error: reopen stdout gives fd %d\n", fd);
				_exit(127);
			}
		}

		switch (type)
		{

	case Script:	if (shell_opt[1] != NULL)
				execl(shell_path[1], shell_cmd[1], shell_opt[1], script_filename, 0);
			else
				execl(shell_path[1], shell_cmd[1], script_filename, 0);

			perror("cake system error, shell script exec");
			_exit(127);

	case System:	if (shell_opt[0] != NULL)
				execl(shell_path[0], shell_cmd[0], shell_opt[0], cmd, 0);
			else
				execl(shell_path[0], shell_cmd[0], cmd, 0);

			perror("cake system error, shell exec");
			_exit(127);

	case Exec:	execvp(argv[0], argv);
			sprintf(scratchbuf, "cake system error, %s exec", argv[0]);
			perror(scratchbuf);
			_exit(127);

	default:	fprintf(stderr, "cake internal error: type = %x in cake_proc\n", type);
			_exit(127);
		}
	}
	or (pid == -1)
	{
		perror("cake system error, fork");
		exit_cake(FALSE);
	}

	proc = make(Proc);
	proc->pr_pid  = pid;
	proc->pr_node = node;
	proc->pr_func = func;
	proc->pr_args = args;
	proc->pr_run  = TRUE;
	addtail(active_procs, proc);
	cdebug("cake_proc pid = %d\n", pid);

	mutex_unlock();
	if (type == Script && ! cakedebug)
	{
		cdebug("cake_proc unlink script_filename %s\n", script_filename);
		if (unlink(script_filename) != 0)
		{
			sprintf(scratchbuf, "cake system error, unlink %s", script_filename);
			perror(scratchbuf);
			exit_cake(FALSE);
		}
	}

	return pid;
}

/*
**	The cake code allows many child processes to be running
**	at any given time. Cake waits for these processes
**	when their results are required. However, there is no
**	necessary relationship between between when a process exits
**	and when its product is needed.
**
**	The list of active processes contains all running processes
**	and all exited processes that haven't been waited for yet.
**	Cake_wait maintains this list. Whenever cake_wait gets
**	an exit report from wait, it invokes the function (if any)
**	associated with the exited process. If the exited process
**	is the one cake_wait is looking for, it returns; otherwise
**	it calls wait again and again until it finds that process.
*/

int
cake_wait(pid)
reg	int	pid;
{
	reg	int	exitpid;
	reg	List	*ptr;
	reg	Proc	*proc;
	Wait		status;

	mutex_lock();
	ptr  = find_process(pid);
	proc = (Proc *) ldata(ptr);
	if (! proc->pr_run)
	{
		status = proc->pr_stat;
		delete(active_procs, ptr);
		mutex_unlock();
		return status.w_status;
	}

#if defined(__convexc__) || defined(__bsdi__)
	while ((exitpid = wait(&status.w_status)) != -1)
#else
	while ((exitpid = wait(&status)) != -1)
#endif
	{
		mutex_lock();
		ptr  = find_process(exitpid);
		proc = (Proc *) ldata(ptr);
		proc->pr_run  = FALSE;
		proc->pr_stat = status;
		cdebug("cake_wait pid = %d, status = %d (x%x)\n", exitpid, status.w_status, status.w_status);
		fflush(stdout);

		if (proc->pr_func != NULL)
		{
			cdebug("cake_wait calling function at %x\n", proc->pr_func);
			fflush(stdout);
			(*proc->pr_func)(status, proc->pr_args);
		}

		if (exitpid == pid)
		{
			delete(active_procs, ptr);
			mutex_unlock();
			return status.w_status; /* normal return */
		}

		mutex_unlock();
	}

	fprintf(stderr, "cake internal error: waiting for nonactive process %s\n", pid);
	exit_cake(TRUE);
	return status.w_status;	/* to shut up lint */
}

/*
**	Find an active process in the active process list.
*/

List *
find_process(pid)
reg	int	pid;
{
	reg	List	*ptr;
	reg	Proc	*proc;
	reg	bool	found;

	found = FALSE;
	for_list (ptr, active_procs)
	{
		proc = (Proc *) ldata(ptr);
		if (proc->pr_pid == pid)
		{
			found = TRUE;
			break;
		}
	}

	if (! found)
	{
		fprintf(stderr, "cake internal error: cannot find active process %d\n", pid);
		exit_cake(TRUE);
	}

	return ptr;
}

/*
**	Open a filter the cake way, with an execv instead of an execl.
*/

#define		READSIDE	0
#define		WRITESIDE	1

static	int	popen_pid[NOFILE];

FILE *
cake_popen(argv, mode)
reg	char	*argv[MAXARGS];
reg	char	*mode;
{
	int		pdesc[2];
	reg	int	parent_end, child_end;
	reg	int	replaced, pid;
	reg	Proc	*proc;

	if (pipe(pdesc) < 0)
		return NULL;

	if (mode[0] == 'r')
	{
		parent_end = pdesc[READSIDE];
		child_end  = pdesc[WRITESIDE];
		replaced   = 1;
	}
	else
	{
		parent_end = pdesc[WRITESIDE];
		child_end  = pdesc[READSIDE];
		replaced   = 0;
	}

	fflush(stdout);
	mutex_lock();
	if ((pid = fork()) == 0)
	{
		close(parent_end);
		close(replaced);
		if (dup(child_end) != replaced)
			_exit(127);

		close(child_end);
		execvp(argv[0], argv);
		sprintf(scratchbuf, "cake system error, %s exec", argv[0]);
		perror(scratchbuf);
		_exit(127);
	}
	or (pid == -1)
	{
		close(parent_end);
		close(child_end);
		perror("cake system error, fork");
		exit_cake(FALSE);
	}

	proc = make(Proc);
	proc->pr_pid  = pid;
	proc->pr_func = NULL;
	proc->pr_args = NULL;
	proc->pr_run  = TRUE;
	addtail(active_procs, proc);

	close(child_end);
	popen_pid[parent_end] = pid;
	mutex_unlock();

	return fdopen(parent_end, mode);
}

int
cake_pclose(fp)
reg	FILE	*fp;
{
	int		code;
	reg	int	f;

	f = fileno(fp);
	fclose(fp);
	code = cake_wait(popen_pid[f]);
	return code;
}

/*
**	Parse the given command into argv, argc.
*/

int
parse_args(cmd, vector)
reg	char	*cmd;
reg	char	**vector;
{
	char		buf[MAXARGSIZE];
	reg	int	i, count;
	reg	char	*s;
	reg	bool	instring;

	cdebug("parse_args: ");

	s = cmd;
	for (count = 0; *s != '\0'; count++)
	{
		while (*s != '\0' && isspace(*s))
			s++;

		instring = FALSE;
		for (i = 0; *s != '\0' && (! isspace(*s) || instring); s++)
		{
			if (*s == '\\')
			{
				if (s[1] != '\0')
					buf[i++] = *++s;
			}
			or (*s == '"')
				instring = ! instring;
			else
				buf[i++] = *s;
		}

		buf[i] = '\0';
		if (i >= MAXARGSIZE)
		{
			fprintf(stderr, "cake: argument '%s' too long\n", buf);
			exit_cake(FALSE);
		}

		if (count >= MAXARGS)
		{
			fprintf(stderr, "cake: '%s' has too many arguments\n", cmd);
			exit_cake(FALSE);
		}

		if (i == 0)
			count--;
		else
		{
			cdebug("<%s>", buf);
			vector[count] = new_name(buf);
		}
	}

	vector[count] = NULL;
	cdebug("/%d\n", count);
	return count;
}

/*
**	Strip one level of backslashes from the given string.
*/

char *
strip_backslash(str)
reg	char	*str;
{
	char		buf[MAXSIZE];
	reg	char	*s;
	reg	int	i;

	if (index(str, '\\') == NULL)
		return str;

	for (i = 0, s = str; *s != '\0'; s++)
	{
		if (*s != '\\')
			buf[i++] = *s;
		or (s[1] != '\0')
			buf[i++] = *++s;
	}

	buf[i] = '\0';
	if (i >= MAXSIZE)
	{
		fprintf(stderr, "cake: command '%s' too long.\n", str);
		exit_cake(FALSE);
	}

	return new_name(buf);
}

/*
**	These functions implement mutual exclusion.
**	They prevent cake from being interrupted
**	between calls to lock and unlock.
**	This is used to preserve the consistency
**	of the active_procs data structure in the presence
**	of multiple executing children.
*/

#if 0		/* mutual exclusion is not necessary & has bug */
#ifdef	ATT
int	(*signalint)();
int	(*signalquit)();
#else
int	signalmask;
#endif
#endif

void
mutex_lock()
{
#if 0
#ifdef	ATT
	signalint  = signal(SIGINT,  SIG_IGN);
	signalquit = signal(SIGQUIT, SIG_IGN);
#else
	signalmask = sigblock(mask(SIGINT)|mask(SIGQUIT));
#endif
#endif
}

void
mutex_unlock()
{
#if 0
#ifdef	ATT
	signal(SIGINT,  signalint);
	signal(SIGQUIT, signalquit);
#else
	sigsetmask(signalmask);
#endif
#endif
}
