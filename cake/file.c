/*
**	File system interface module.
*/

static	char
rcs_id[] = "$Header$";

#include	"cake.h"
#include	<errno.h>
#include	<sys/stat.h>
#ifndef		ATT
#include	<sys/dir.h>
#include	<sys/time.h>
#else
#include	<time.h>
#endif

typedef	struct	timeval		Tval;
typedef	struct	timezone	Tzone;
typedef	struct	stat		Stat;
typedef	struct	direct	Dirent;

extern	int	errno;

static	char	dir_name[30] = "/tmp/CakeXXXXXX";

/*
**	Create the directory to be used for all temporary storage.
*/
void
dir_start()
{
	reg	bool	successful;

	mktemp(dir_name);

#ifdef	ATT
	{
		extern	int	cake_proc();
		extern	int	cake_wait();
		reg	int	pid;
		int		code;

		sprintf(scratchbuf, "mkdir %s", dir_name);
		pid = cake_proc(new_name(scratchbuf), Exec, (char *) NULL,
			(Node *) NULL, (int (*)()) NULL, (List *) NULL);
		code = cake_wait(pid);
		successful = code == 0;
	}
#else
	successful = mkdir(dir_name, 0700) == 0;
#endif

	if (! successful)
	{
		sprintf(scratchbuf, "cake system error, mkdir %s", dir_name);
		perror(scratchbuf);
		exit_cake(FALSE);
	}

#ifdef	ATT
	if (chmod(dir_name, 0700) != 0)
	{
		sprintf(scratchbuf, "cake system error, chmod 700 %s", dir_name);
		perror(scratchbuf);
		exit_cake(FALSE);
	}
#endif
}

/*
**	Clean up /tmp when we are finished
*/
void
dir_finish()
{
#ifndef	LEAVE_DIR
#ifdef	ATT
	extern	int	cake_proc();
	extern	int	cake_wait();
	reg	int	pid;
	int		code;

	sprintf(scratchbuf, "/bin/rm -fr %s", dir_name);
	pid = cake_proc(new_name(scratchbuf), Exec, (char *) NULL,
		(Node *) NULL, (int (*)()) NULL, (List *) NULL);
	code = cake_wait(pid);
	if (code != 0)
	{
		fprintf(stderr, "cake system error: cannot remove %s\n", dir_name);
		return;
	}
#else
	char		buf[1024];
	reg	DIR	*dirp;
	reg	Dirent	*dp;
	reg	int	nameoffset;

	if ((dirp = opendir(dir_name)) == NULL)
	{
		sprintf(scratchbuf, "cake system error, opendir %s", dir_name);
		perror(scratchbuf);
		return;
	}

	strcpy(buf, dir_name);
	strcat(buf, "/");
	nameoffset = strlen(buf);

	for (dp = readdir(dirp); dp != NULL; dp = readdir(dirp))
		if (strdiff(dp->d_name, ".") && strdiff(dp->d_name, ".."))
		{
			buf[nameoffset] = '\0';
			strcat(buf, dp->d_name);
			cdebug("dir_finish unlink %s\n", buf);
			if (unlink(buf) != 0)
			{
				sprintf(scratchbuf, "cake system error, unlink %s", buf);
				perror(scratchbuf);
			}
		}

	closedir(dirp);

	cdebug("rmdir %s\n", dir_name);
	if (rmdir(dir_name) != 0)
	{
		sprintf(scratchbuf, "cake system error, rmdir %s", dir_name);
		perror(scratchbuf);
	}
#endif
#endif
}

/*
**	Get a new filename in our private directory.
**	These files are used to store shell scripts
**	and the output of actions.
**
**	These names MUST be different from those generated
**	by get_savename, so they have different prefixes.
*/

char *
get_newname()
{
	char		buf[100];
	static	int	count = 0;

	++count;
	sprintf(buf, "%s/gen_%d", dir_name, count);
	return new_name(buf);
}

/*
**	Save a nonvolatile file for later comparison.
**	Any slashes in the filename are replaced by colons
**	to avoid reference to nonexistent directories.
*/
void
save_novol(node)
reg	Node	*node;
{
	char		buf[MAXSIZE];
	reg	FILE	*ifp, *ofp;
	reg	int	c;

	if (nflag)
	{
		node->n_stime = node->n_rtime;
		return;
	}

	put_trail("save_novol", "start");
	cdebug("saving %s\n", node->n_name);
	node->n_stime = node->n_rtime;

	get_savename(node->n_name, buf);
	if ((ifp = fopen(node->n_name, "r")) == NULL)
	{
		if (errno == ENOENT)
		{
			cdebug("%s does not exist, considered volatile\n", node->n_name);
			reset_node(node, nf_NONVOL);
			put_trail("save_novol", "finish");
			return;
		}

		sprintf(scratchbuf, "cake system error, fopen (r) %s", node->n_name);
		perror(scratchbuf);
		printf("cake: considering %s volatile\n", node->n_name);
		reset_node(node, nf_NONVOL);
		put_trail("save_novol", "finish");
		return;
	}

	if ((ofp = fopen(buf, "w")) == NULL)
	{
		sprintf(scratchbuf, "cake system error, fopen (w) %s", buf);
		perror(scratchbuf);
		printf("cake: considering %s volatile\n", node->n_name);
		reset_node(node, nf_NONVOL);
		put_trail("save_novol", "finish");
		return;
	}

	while ((c = getc(ifp)) != EOF)
		putc(c, ofp);
	
	fclose(ifp);
	fclose(ofp);
	put_trail("save_novol", "finish");
}

/*
**	See if the given file has been changed since saved.
*/

bool
diff_novol(node)
reg	Node	*node;
{
	char		buf[MAXSIZE];
	reg	FILE	*i1fp, *i2fp;
	reg	int	c;
	reg	bool	diff;

	if (nflag)
		return FALSE;

	put_trail("diff_novol", "start");
	cdebug("comparing saved copy of %s: ", node->n_name);

	get_savename(node->n_name, buf);
	if ((i1fp = fopen(node->n_name, "r")) == NULL)
	{
		sprintf(scratchbuf, "cake system error, fopen (r) %s", node->n_name);
		perror(scratchbuf);
		printf("cake: considering %s different\n", node->n_name);
		reset_node(node, nf_NONVOL);
		put_trail("diff_novol", "finish");
		return TRUE;;
	}

	if ((i2fp = fopen(buf, "r")) == NULL)
	{
		sprintf(scratchbuf, "cake system error, fopen (r) %s", buf);
		perror(scratchbuf);
		printf("cake: considering %s different\n", node->n_name);
		reset_node(node, nf_NONVOL);
		put_trail("diff_novol", "finish");
		return TRUE;;
	}

	diff = FALSE;
	while ((c = getc(i1fp)) != EOF)
		if (getc(i2fp) != c)
		{
			diff = TRUE;
			break;
		}
	
	if (getc(i2fp) != EOF)
		diff = TRUE;
	
	fclose(i1fp);
	fclose(i2fp);
	cdebug("diff_novol unlink %s\n", buf);
	if (unlink(buf) != 0)
	{
		sprintf(scratchbuf, "cake system error, unlink %s", buf);
		perror(scratchbuf);
		put_trail("diff_novol", "finish");
		return TRUE;
	}

	cdebug("%s\n", diff? "different": "same");
	put_trail("diff_novol", "finish");
	return diff;
}

/*
**	Get the the name of the saved copy of the given file.
*/
void
get_savename(name, buf)
reg	char	*name;
reg	char	buf[];
{
	extern	char	*noslash();

	strcpy(buf, dir_name);
	strcat(buf, "/save_");
	strcat(buf, noslash(name));
}

/*
**	Remove any slashes from a filename.
*/

char *
noslash(name)
reg	char	*name;
{
	char		buf[MAXSIZE];
	reg	int	i;
	reg	char	*s;

	i = 0;
	for (s = name; *s != '\0'; s++)
		if (*s == '/')
			buf[i++] = ':';
		else
			buf[i++] = *s;
	
	buf[i] = '\0';
	return new_name(buf);
}

/*
**	Perform a stat on the given file. The only relevant info
**	is the return code, indicating whether the file exists or not
**	and its last modify (or status change) date. Note that if a
**	file does not exist, it is by definition volatile. An aside
**	considers all directories precious.
*/
void
node_stat(node)
reg	Node	*node;
{
	extern	int	stat();
	Stat		statbuf;

	if (node->n_name != (char *) NULL && stat(node->n_name, &statbuf) == 0)
	{
		set_node(node, nf_EXIST);
		if ((statbuf.st_mode & S_IFMT) == S_IFDIR)
			set_node(node, nf_PRECIOUS);

		node->n_rtime = cflag? statbuf.st_ctime: statbuf.st_mtime;
	}
	else
	{
		reset_node(node, nf_EXIST);
		reset_node(node, nf_NONVOL);
		node->n_rtime = GENESIS;
	}
}

/*
**	Restore the previous stat of a nonvol file,
**	both in the filesystem and in cake's memory.
**	However, make sure that it will not need to be
**	remade again and again to discover this anew
**	while its ancestors remain the same.
*/
void
node_resetstat(node)
reg	Node	*node;
{
	node_stat(node);
	if (off_node(node, nf_EXIST))
		return;

	cake_utimes(node, max(node->n_stime, node->n_utime));
	node->n_utime = node->n_rtime;
}

/*
**	Set up the stat of a file as the latest ancestor's,
**	both in the filesystem and in cake's memory.
*/
void
node_setstat(node)
reg	Node	*node;
{
	extern	time_t	get_youngest();
	reg	time_t	youngest;

	node_stat(node);
	if (off_node(node, nf_EXIST))
		return;

	youngest = get_youngest(node, FALSE);
	if (youngest != node->n_utime && youngest != GENESIS)
	{
		fprintf(stderr, "cake internal error: youngest is mismatched for %s\n",
			node->n_name);
		exit_cake(TRUE);
	}

	cake_utimes(node, node->n_utime);
}

/*
**	Return the time, converting formats.
*/

time_t
cake_gettime()
{
#ifdef	ATT
	extern	time_t	time();
	long		val;

	if (time(&val) == (time_t)-1)
	{
		sprintf(scratchbuf, "cake system error, time");
		perror(scratchbuf);
		exit_cake(FALSE);
	}

	return (time_t) val;
#else
	Tzone		tzone;
	Tval		tval;

	if (gettimeofday(&tval, &tzone) != 0)
	{
		sprintf(scratchbuf, "cake system error, gettimeofday");
		perror(scratchbuf);
		exit_cake(FALSE);
	}

	return (time_t) tval.tv_sec;
#endif
}

/*
**	Issue the system call utimes after expanding its arguments
**	from time_t to Tval. If the time given is GENESIS, use the
**	current time instead.
*/

#ifdef	ATT
typedef	struct	utimbuf
{
	time_t	actime;
	time_t	modtime;
} Utimbuf;
#endif

void
cake_utimes(node, newtime)
reg	Node	*node;
time_t		newtime;
{
#ifdef	ATT
	extern	int	utime();
#else
	extern	int	utimes();
#endif
	extern	int	stat();
	Stat		statbuf;
#ifdef	ATT
	Utimbuf		timbuf;
	long		vals[2];
#else
	Tzone		tzone;
	Tval		tvals[2];
#endif

	cdebug("resetting time for %s to %d, %s",
		node->n_name, newtime, ctime(&newtime));

#ifdef	ATT
	if (time(&vals[0]) == -1)
	{
		sprintf(scratchbuf, "cake system error, time");
		perror(scratchbuf);
		exit_cake(FALSE);
	}
#else
	if (gettimeofday(&tvals[0], &tzone) != 0)
	{
		sprintf(scratchbuf, "cake system error, gettimeofday");
		perror(scratchbuf);
		exit_cake(FALSE);
	}
#endif

	if (newtime == GENESIS)
	{
		cdebug("reinterpreting GENESIS\n");
#ifdef	ATT
		if (time(&vals[1]) == -1)
		{
			sprintf(scratchbuf, "cake system error, time");
			perror(scratchbuf);
			exit_cake(FALSE);
		}
#else
		if (gettimeofday(&tvals[1], &tzone) != 0)
		{
			sprintf(scratchbuf, "cake system error, gettimeofday");
			perror(scratchbuf);
			exit_cake(FALSE);
		}
#endif
	}
	else
	{
#ifdef	ATT
		vals[1] = (long) newtime;
#else
		tvals[1].tv_sec  = newtime;
		tvals[1].tv_usec = 0;
#endif
	}

#ifdef	ATT
	timbuf.actime  = vals[0];
	timbuf.modtime = vals[1];
	node->n_rtime  = vals[1];
#else
	node->n_rtime  = (time_t) tvals[1].tv_sec;
#endif

	if (nflag)
		return;

#ifdef	ATT
	if (utime(node->n_name, &timbuf) != 0)
#else
	if (utimes(node->n_name, tvals) != 0)
#endif
	{
		sprintf(scratchbuf, "cake system error, utime(s) %s", node->n_name);
		perror(scratchbuf);
		fprintf(stderr, "cake: continuing\n");
	}

#ifdef	CAKEDEBUG
	if (cakedebug)
	{
		if (stat(node->n_name, &statbuf) != 0)
			printf("cannot verify utimes\n");
		else
		{
			print_time("accessed", statbuf.st_atime);
			print_time("modified", statbuf.st_mtime);
			print_time("changed ", statbuf.st_ctime);
		}
	}
#endif
}

/*
**	See if the given file exists.
*/

bool
exist(name)
reg	char	*name;
{
	Stat	statbuf;

	if (stat(name, &statbuf) == 0)
		return TRUE;
	else
		return FALSE;
}

/*
**	Remove the given file.
*/
void
cake_remove(name)
reg	char	*name;
{
#ifdef	CAREFUL
	if (! Xflag)
	{
		extern	int	getpid();
		extern	int	cake_proc();
		extern	int	cake_wait();
		extern	char	*noslash();
		reg	int	pid, cakepid;
		Stat		statbuf;
		int		code;
		char		buf[80];

#ifndef	ATT
		if (lstat(name, &statbuf) != 0)
		{
			sprintf(scratchbuf, "cake system error, lstat %s", name);
			perror(scratchbuf);
			fprintf(stderr, "cake: continuing\n");
			return;
		}

		if ((statbuf.st_mode & S_IFMT) == S_IFLNK)
		{
			printf("removing the symbolic link %s\n", name);
			cdebug("cake_remove unlink %s\n", name);
			if (unlink(name) != 0)
			{
				sprintf(scratchbuf, "cake system error, unlink %s", name);
				perror(scratchbuf);
				fprintf(stderr, "cake: continuing\n");
			}

			return;
		}
#endif

		cakepid = getpid();
		printf("moving %s to /tmp\n", name);
		sprintf(buf, "mv %s /tmp/%s.%d", name, noslash(name), cakepid);
		pid = cake_proc(buf, Exec, (char *) NULL, (Node *) NULL,
			(int (*)()) NULL, (List *) NULL);
		code = cake_wait(pid);
		if (code != 0)
		{
			fprintf(stderr, "cake system error: '%s' failed\n", buf);
			fprintf(stderr, "cake: continuing\n");
			return;
		}

		sprintf(buf, "/tmp/%s.%d", noslash(name), cakepid);
		if (chmod(buf, 0600) != 0)
		{
			sprintf(scratchbuf, "cake system error, chmod %s", buf);
			perror(scratchbuf);
			fprintf(stderr, "cake: continuing\n");
			return;
		}

		return;
	}
#endif

	cdebug("cake_remove unlink %s\n", name);
	if (unlink(name) != 0)
	{
		sprintf(scratchbuf, "cake system error, unlink %s", name);
		perror(scratchbuf);
		fprintf(stderr, "cake: continuing\n");
	}
}
