/*
 *			J O V E _ I O . C
 *
 * $Revision$
 *
 * $Log$
 * Revision 14.1  2004/11/16 19:42:20  morrison
 * dawn of a new revision.  it shall be numbered 14 to match release 7.  begin the convergence by adding emacs/vi local variable footer blocks to encourage consistent formatting.
 *
 * Revision 1.1  2004/05/20 14:50:00  morrison
 * Sources that are external to BRL-CAD are moved from the top level to src/other/.
 *
 * Revision 11.5  2000/08/24 23:12:23  mike
 *
 * lint, RCSid
 *
 * Revision 11.4  1999/05/13  19:54:41  jra
 * Linux "stat" corrupts its args on failure!!!!!
 *
 * Revision 11.3  1999/01/12  03:05:25  mike
 * Recognize file suffixes for C++ code as well as C code.
 *
 * Revision 11.2  1995/06/21  03:41:51  gwyn
 * Eliminated trailing blanks.
 * TempFile is now an array.
 * Improved use of tempnam().
 * Changed memcpy calls back to bcopy.  (Fixed confusing use of SYSV vs. SYS5.)
 * Use LBSIZE instead of hard-wired 100.
 *
 * Revision 11.1  95/01/04  10:35:15  mike
 * Release_4.4
 *
 * Revision 10.5  95/01/03  21:35:28  butler
 * lseek takes an off_t as its second arg under BSDI and NetBSD
 *
 * Revision 10.4  1994/09/17  04:57:35  butler
 * changed all calls to bcopy to be memcpy instead.  Useful for Solaris 5.2
 *
 * Revision 10.3  1993/10/26  06:31:25  mike
 * Modified to use tempnam(), so that $TMPDIR from user's environment
 * will be honored if set
 *
 * Revision 10.2  92/01/31  16:56:12  mike
 * Must do chown() *after* close(), so that on NFS clients, we don't
 * loose write permission on the file before we write any data into it.
 *
 * Revision 10.1  91/10/12  06:54:00  mike
 * Release_4.0
 *
 * Revision 2.11  91/08/30  22:56:06  mike
 * __CRAY1 too
 *
 * Revision 2.10  91/08/30  22:37:07  mike
 * Fixed in-core buffering to operate on the array subscripts, rather than
 * a (char *) pointer, as this is more portable, and avoids CRAY problems.
 * In addition, turned off the in-core buffering on CRAY1 (and X and Y)
 * systems, as it consumes an extra 256 Kbytes, which on smaller Crays
 * isn't worth it.  Their I/O is plenty fast, and memory is usually tight.
 *
 * Revision 2.9  91/08/30  20:29:44  mike
 * Added extern int read().
 *
 * Revision 2.8  91/08/30  19:34:16  mike
 * Changed VMUNIX to !defined(pdp11)
 *
 * Revision 2.7  91/08/30  19:17:44  mike
 * Stardent ANSI lint
 *
 * Revision 2.6  91/08/30  18:46:05  mike
 * Changed from BSD index/rindex nomenclature to SYSV strchr/strrchr.
 *
 * Revision 2.5  91/08/30  18:11:04  mike
 * Made explicit that termcap.h to be used is the local version
 *
 * Revision 2.4  91/08/30  17:54:34  mike
 * Changed #include directives to distinguish between local and system header
 * files.
 *
 * Revision 2.3  91/08/30  17:49:08  mike
 * Paul Stay mods for ANSI C
 *
 * Revision 2.2  87/04/14  20:27:17  dpk
 * Commented out the bcopy routine.  Will use memcpy for bcopy.
 *
 * Revision 2.1  86/09/23  22:28:40  mike
 * Externs now declared properly.
 * I/O fixes for SysV
 *
 * Revision 2.1  86/09/23  22:26:37  mike
 * Externs now declared properly.
 * I/O fixes for SysV
 *
 * Revision 2.0  84/12/26  16:46:34  dpk
 * System as distributed to Berkeley 26 Dec 84
 *
 *
 * Revision 1.3  84/03/20  22:29:18  dpk
 * Improved file handling, fixed "Free line in list" loop
 *
 * Revision 1.2  83/12/16  00:08:33  dpk
 * Added distinctive RCS header
 *
 */
#ifndef lint
static const char RCSid[] = "@(#)$Header$";
#endif

/*
   Jonathan Payne at Lincoln-Sudbury Regional High School 5-25-83

   Commands to read/write files/regions.  */

#include <stdio.h>
#include "./jove.h"
#include "./termcap.h"

#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>

extern	int errno;
extern	int Dfltmode;
extern	char TempFile[];

int	BackupFiles = SAVE_NO;

char	*WERROR	= "Write error";
long	count = 0;
int	nlines = 0;

char	iobuff[LBSIZE],
	*nextip;
int	ninbuf;

/**** Shared with jove_screen.c ****/
char	ibuff1[BSIZ],	/* Holds block `iblock1' of the tmp file */
	ibuff2[BSIZ],	/*   "     "   `iblock2' of the tmp file */
	obuff[BSIZ];	/* Holds the last block of the tmp file */
int	ichng1,		/* ibuff1 should be written to its
			 * blocks when it is used to read
			 * another block.
			 */
	ichng2,		/* "" */
	iblock1,	/* Block number of ibuff1 */
	iblock2,	/*		   ibuff2 */
	oblock,		/* 		   obuff  */
	nleft,		/* Number of good characters left in current block */
	hitin2,		/* Last read was in ibuff2 */
	tmpfd;
disk_line	tline;		/* Pointer to end of tmp file */

char	*tfname;

extern int	read(), write();

void	dofread();
void	DoWriteReg();
void	file_write();
void	setcmode();
void	dolseek();
void	blkio();
void	lsave();
void	getDOT();
void	SavLine();

/***********************************/

void
IOclose()
{
	if (io > 0)
		ignore(close(io)), io = -1;
	nextip = iobuff;
	ninbuf = 0;
}

/* This reads a line from the input file into buf. */

int
getfline(buf)
char	*buf;
{
	register int	c;
	register char	*lp,
			*fp;

	lp = buf;
	*lp = '\0';
	fp = nextip;
	do {
		if (--ninbuf < 0) {
			ninbuf = read(io, iobuff, LBSIZE) - 1;
			fp = iobuff;
			if (ninbuf < 0) {
				*lp = '\0';
				IOclose();
				return EOF;
			}
		}
		c = *fp++;
		if (c == '\0')
			continue;
		if (lp >= &buf[LBSIZE - 1]) {
			message("Line to long...");
			redisplay();
			IOclose();
			return EOF;
		}

		*lp++ = c;
		count++;
	} while (c != '\n');
	*--lp = 0;
	nextip = fp;
	return 0;
}

/* Write the region from line1/char1 to line2/char2 to the open
   fildes `io'.  */

int	EndWNewline = 1;

void
putreg(line1, char1, line2, char2)
LINE	*line1,
	*line2;
{
	int n;
	register char *fp, *lp;
	register int	nib;

	count = nlines = 0;
	nib = LBSIZE;
	fp = iobuff;
	lsave();	/* Need this! */

	while (line1 != line2->l_next) {
		lp = getline(line1->l_dline, linebuf) + char1;
		if (line1 == line2)
			linebuf[char2] = '\0';
		for (;;) {
			if (--nib < 0) {
				n = fp - iobuff;
				if (write(io, iobuff, n) != n)
					goto werror;
				nib = LBSIZE-1;
				count += n;
				fp = iobuff;
			}
			if ((*fp++ = *lp++) == 0) {
				if (line1 != line2) {
					nlines++;
					fp[-1] = '\n';
				} else
					fp--;	/* Don't write the NULL!! */
				break;
			}
		}
		line1 = line1->l_next;
		char1 = 0;
	}
	n = fp - iobuff;

	if (write(io, iobuff, n) != n)
		goto werror;
	count += n;
	getDOT();		/* What ever was in linebuf */
	IOclose();
	return;
werror:
	error(WERROR);
	/* NOTREACHED */
}

char *
IOerr(err, file)
char	*err, *file;
{
	return sprint("Couldn't %s \"%s\"", err, file);
}

void
read_file(file)
char	*file;
{
	BUFLOC	save;

	setcmode();

	io = open(file, 0);
	if (io == -1) {
		curbuf->b_ino = -1;
		s_mess(IOerr("open", file));
		return;
	}
	DOTsave(&save);
	set_ino(curbuf);
	dofread(file);
	SetDot(&save);
	getDOT();
	IOclose();
	if (access (file, 02))
		SetReadOnly(curbuf);
}

void
FileMess(file, lines, chars)
char	*file;
long	chars;
{
	s_mess("\"%s\" %d lines %ld characters", file, lines, chars);
}

void
dofread(file)
char	*file;
{
	char	end[LBSIZE];
	int	xeof = 0;

	lsave();
	nlines = 0;
	count = 0L;
	s_mess("\"%s\"", file);
	UpdateMesg();
	ignore(getline(curline->l_dline, end));
	strcpy(genbuf, end);
	strcpy(end, &end[curchar]);
	if ((xeof = getfline(linebuf)) == 0)
		linecopy(genbuf, curchar, linebuf);

	curline->l_dline = putline(genbuf);
	if (!xeof) do {
		xeof = getfline(linebuf);
		nlines++;
		curline = listput(curbuf, curline);
		curline->l_dline = putline(linebuf) | DIRTY;
	} while (!xeof);

	linecopy(linebuf, (curchar = strlen(linebuf)), end);
	curline->l_dline = putline(linebuf);
	FileMess(file, nlines, count);
}

void
bufname(bp)
BUFFER	*bp;
{
	char	tmp[LBSIZE],
		tmp1[LBSIZE],
		*cp;
	int	try = 1;

	if ((cp = bp->b_fname) == 0)
		complain("No file name");

	if (*cp == '.')
		++cp;
	strcpy(tmp, cp);
	cp = strrchr(tmp, '/');
	if (cp)
		strncpy(tmp, &cp[1], sizeof tmp);

	strcpy(tmp1, tmp);
	while (buf_exists(tmp)) {
		ignore(sprintf(tmp, "%s[%d]", tmp1, try));
		try++;
	}
	setbname(bp, tmp);
}

void
SaveFile()
{
	if (IsModified(curbuf)) {
		file_write(curbuf->b_fname, 0);
		SetUnmodified(curbuf);
	} else
		message("No changes need be written");
}

void
filemunge(fname)
char	*fname;
{
	struct stat	stbuf;

	if (stat(fname, &stbuf))
		return;
	if ((stbuf.st_ino != curbuf->b_ino
	   || stbuf.st_dev != curbuf->b_dev)
	  && (stbuf.st_mode & S_IFMT) != S_IFCHR)
		confirm("\"%s\" already exist; are you sure? ", fname);
}

void
WrtReg()
{
	DoWriteReg(0);
}

void
AppReg()
{
	DoWriteReg(1);
}

void
DoWriteReg(app)
{
	char	fname[LBSIZE];
	MARK	*mp = CurMark();

	/* Won't get here if there isn't a mark */
	strcpy(fname, ask((char *)0, FuncName()));

	if (!app)
		filemunge(fname);
	if (app) {
		io = open(fname, 1);	/* Writing */
		if (io == -1)
			io = creat(fname, Dfltmode);
		else
			dolseek(io, 0L, 2);
	} else
		io = creat(fname, Dfltmode);
	if (io == -1)
		complain(IOerr("create", fname));

	s_mess("\"%s\"", fname);
	UpdateMesg();			/* Update mesg line */
	if (inorder(mp->m_line, mp->m_char, curline, curchar))
		putreg(mp->m_line, mp->m_char, curline, curchar);
	else
		putreg(curline, curchar, mp->m_line, mp->m_char);
	FileMess(fname, nlines, count);
	IOclose();
}

void
WriteFile()
{
	char	*fname,
		fnamebuf[LBSIZE];

	fname = ask(curbuf->b_fname, FuncName());
	strncpy(fnamebuf, fname, sizeof fnamebuf);

	filemunge(fnamebuf);
	setfname(curbuf, fnamebuf);
	set_ino(curbuf);
	file_write(curbuf->b_fname, jove_exp_p);
	SetUnmodified(curbuf);
}

char	*SaveName();

void
file_write(fname, app)
char	*fname;
{
	struct stat	inode;
	register int	exists;
	register int	linked;
	int		saveit = 0;
	int		overwrite = 0;
	int		do_chown = 0;

	if (fname == 0 || *fname == '\0')
		complain("I need a file name");

	io = -1;
	inode.st_mode = Dfltmode;
	inode.st_nlink = 1;
	inode.st_uid = getuid();
	inode.st_gid = getgid();
	exists = !stat(fname, &inode);
	if( !exists )
	{
		/* Linux "stat" corrupts its args on failure */
		inode.st_mode = Dfltmode;
		inode.st_nlink = 1;
		inode.st_uid = getuid();
		inode.st_gid = getgid();
	}
	linked = (inode.st_nlink > 1);

	if (IsReadOnly(curbuf) && exists) {
		register char *yorn;

		yorn = ask ("Y", "%s is read-only, recreate? (yes) ", fname);
		if (*yorn == 'N' || *yorn == 'n')
			complain ("File save aborted.");
		if (unlink(fname) != 0)
			complain ("Cannot unlink %s", fname);
	}

	if (app) {
		/*  Appending to a file  */
		io = open(fname, 1);	/* Writing */
		if (io == -1)
			io = creat(fname, Dfltmode);
		else
			dolseek(io, 0L, 2);
	} else {
		register char	*yorn;

		if (linked) {
			yorn = ask("Y", "%s has links, overwrite? (yes) ", fname);
			overwrite = (*yorn == 'Y' || *yorn == 'y');
		}
		if (exists && BackupFiles == SAVE_ASK) {
			yorn = ask("N", "Create backup file %s~ (no) ", fname);
			saveit = (*yorn == 'Y' || *yorn == 'y');
		}

		/* Write in a whole new file */
		s_mess("\"%s\"", fname);
		if (exists && (saveit || BackupFiles == SAVE_ALWAYS)) {
			register char *savefile;

			savefile = SaveName(fname);
		 	unlink (savefile);
		   	if (linked && overwrite) {
		   		/*  We need to "cp" the file  */
		   		s_mess("\"%s\" [No backup with overwrite]", fname);
		   	} else {
			 	if (link (fname, savefile))
			   		s_mess("\"%s\" [No backup possible]", fname);
			 	else
		 			unlink (fname);
		 	}
			free (savefile);
		}
		io = creat(fname, inode.st_mode&07777);
		do_chown = 1;
	}
	if (io == -1)
		complain(IOerr("create", fname));

	ClrReadOnly(curbuf);
	if (access(fname, 02))
		SetReadOnly(curbuf);

	UpdateMesg();		/* Update mesg line */
	if (EndWNewline) {	/* Make sure file ends with a newline */
		BUFLOC	save;

		DOTsave(&save);
		Eof();
		if (length(curline))	/* Not a blank line */
			DoTimes(LineInsert, 1);	/* Make it blank */
		SetDot(&save);
	}
	putreg(curbuf->b_zero, 0, curbuf->b_dol, length(curbuf->b_dol));
	FileMess(fname, nlines, count);
	IOclose();
	if( do_chown )  {
		/*  This will fail on all but USG systems, ignore it.
		 *  Must be done *after* close, so that on NFS clients,
		 *  we don't loose write permission on the file before
		 *  we write any data into it...
		 */
		chown (fname, inode.st_uid, inode.st_gid);
	}
}

char *
SaveName (fname)
char *fname;
{
	register int	len = strlen(fname);
	register char *cp;

#ifndef V4_2BSD
	if (cp = strrchr(fname, '/'))
		cp++;
	else
		cp = fname;

	if ( (int)strlen(cp) > 13)
		complain("Cannot make backup name; \"%s\" too long", cp);
#endif
	cp = emalloc (len+2);
	strncpy (cp, fname, len);
	cp[len] = '~';
	cp[len+1] = 0;
	return (cp);
}

void
initlist(bp)
BUFFER	*bp;
{
	lfreelist(bp->b_zero);
	ignore(listput(bp, (LINE *) 0));		/* First line in buffer */

	bp->b_dot->l_dline = putline("") | DIRTY;
	bp->b_char = 0;
	AllMarkSet(bp, bp->b_dot, 0);
	if (bp == curbuf)  {
		getDOT();
	}
	initwinds(bp);
}

void
setcmode()
{
	register int	len;
	register char	*name = curbuf->b_fname;

	if( !name )  return;
	len = strlen(name);

	if (len < 2 || IsDisabled(globflags, CMODE))
		return;

	/* Find trailing dot, if any */
	while( --len >= 0 )  {
		if(name[len] == '.') break;
	}
	if( len < 0 )  return;
	name += len+1;		/* Now points at char after the dot */

	/* Compare against known names for C and C++ code */
	if( name[1] == '\0' )  {
		switch(name[0]) {
		case 'C':
		case 'c':
		case 'h':
		case 'H':
			OnFlag(globflags, CMODE);
			return;
		}
	}
	else if(
		strcmp( name, "cxx" ) == 0 ||
		strcmp( name, "cpp" ) == 0 ||
		strcmp( name, "CC" ) == 0 ||
		strcmp( name, "c++" ) == 0 ||
		strcmp( name, "hh" ) == 0 ||
		strcmp( name, "hpp" ) == 0 ||
		strcmp( name, "hxx" ) == 0 ||
		strcmp( name, "h++" ) == 0
	)
		OnFlag(globflags, CMODE);
}

void
ReadFile()
{
	char	*fname;

	if (IsModified(curbuf))
		confirm("%s modified, shall I read anyway? ", filename(curbuf));

	fname = ask(curbuf->b_fname, FuncName());
	SetUnmodified(curbuf);
	setfname(curbuf, fname);
	initlist(curbuf);
	read_file(curbuf->b_fname);
}

void
InsFile()
{
	char	*fname;

	fname = ask(curbuf->b_fname, FuncName());
	read_file(fname);
	SetModified(curbuf);
}

void
dolseek(fd, offset, whence)
#if defined(__bsdi__) || defined(__NetBSD__)
off_t	offset;
#else
long	offset;
#endif
{
	if (lseek(fd, offset, whence) == -1)
		complain("lseek failed");
}

/*
   Jonathan Payne at Lincoln-Sudbury Regional High School 5-25-83

   Much of this code was lifted from VI (ex_temp.c).  These functions
   deal with (put/get)ing lines in/from the tmp file.  */

#include "./jove_temp.h"

int	DOLsave = 0;	/* Do Lsave flag.  If lines aren't being save
			   when you think they should have been, this
			   flag is probably not being set, or is being
			   cleared before lsave() was called. */

void
tmpinit()
{
#if HAS_TEMPNAM
	/* Honor $TMPDIR in user's environment */
	tfname = tempnam((char *)NULL, "jovet");
#else
	tfname = mktemp(TempFile);
#endif
	tline = 2;
	iblock1 = oblock = iblock2 = -1;
	hitin2 = ichng1 = ichng2 = 0;
	ignore(close(creat(tfname, 0600)));
	tmpfd = open(tfname, 2);
	ignore(unlink(tfname));
	if (tmpfd == -1) {
		putstr(sprint("%s?\n", tfname));
		finish(0);
	}
}

/* Get a line at `tl' in the tmp file into `buf' which should be LBSIZE
   long. */

char *
getline(tl, buf)
disk_line	tl;
char	*buf;
{
	register char	*bp,
			*lp;
	register int	nl;

	lp = buf;
	bp = getblock(tl, READ);
	nl = nleft;
	tl &= ~OFFMSK;
	while (*lp++ = *bp++) {
		if (--nl == 0) {
			/* += INCRMT moves tl to the next block in
			   the tmp file. */
			bp = getblock(tl += INCRMT, READ);
			nl = nleft;
		}
	}

	return buf;
}

/* Put `buf' and return the disk address */

disk_line
putline(buf)
char	*buf;
{
	register char	*bp,
			*lp;
	register disk_line	nl;
	disk_line	tl;

	lp = buf;
	tl = tline;
	bp = getblock(tl, WRITE);
	nl = nleft;
	tl &= ~OFFMSK;
	while (*bp = *lp++) {
		if (*bp++ == '\n') {
			*--bp = 0;
			break;
		}
		if (--nl == 0) {
			bp = getblock(tl += INCRMT, WRITE);
			nl = nleft;
		}
	}
	nl = tline;
	tline += (((lp - buf) + BNDRY - 1) >> SHFT) & 077776;
	return nl | DIRTY;	/* So it will be redisplayed */
}

/* Get a block which contains at least part of the line with the address
 * atl.  Returns a pointer to the block and sets the global variable
 * nleft (number of good characters left in the buffer)
 */

char *
getblock(atl, iof)
disk_line	atl;
{
	register int	bno,
			off;

	if (atl == 0)
		finish (1);			/* Panic */
	bno = (atl >> OFFBTS) & BLKMSK;
	off = (atl << SHFT) & LBTMSK;
	if (bno >= NMBLKS)
		error("Tmp file too large.  Get help");
	nleft = BSIZ - off;
	if (bno == iblock1) {
		ichng1 |= iof;
		return ibuff1 + off;
	}
	if (bno == iblock2) {
		ichng2 |= iof;
		return ibuff2 + off;
	}
	if (bno == oblock)
		return obuff + off;
	if (iof == READ) {
		if (hitin2 == 0) {
			if (ichng2)
				blkio(iblock2, ibuff2, write);
			ichng2 = 0;
			iblock2 = bno;
			blkio(bno, ibuff2, read);
			hitin2 = 1;
			return (ibuff2 + off);
		}
		hitin2 = 0;
		if (ichng1)
			blkio(iblock1, ibuff1, write);
		ichng1 = 0;
		iblock1 = bno;
		blkio(bno, ibuff1, read);
		return (ibuff1 + off);
	}
	if (oblock >= 0)
		blkio(oblock, obuff, write);
	oblock = bno;
	return obuff + off;
}

#if !defined(pdp11) && !defined(CRAY1) & !defined(__CRAY1)
/* Cache the first 64 disk blocks of temp file in memory, and block align */
#define	INCORB	64
char	incorb[(INCORB+1)*BSIZ];
#define	pageround(a)	((a) & ~(BSIZ-1))
#endif

void
blkio(b, buf, iofcn)
short	b;
char	*buf;
int	(*iofcn)();
{
	extern int read();

#ifdef INCORB
	if (b < INCORB) {
		if (iofcn == read) {
			bcopy(&incorb[pageround((b+1)*BSIZ)], buf, BSIZ);
			return;
		}
		bcopy(buf, &incorb[pageround((b+1)*BSIZ)], BSIZ);
		return;
	}
#endif
	ignore(lseek(tmpfd, (long) (unsigned) b * (long)BSIZ, 0));
	if ((*iofcn)(tmpfd, buf, BSIZ) != BSIZ)
		error("IO error");
}

#ifdef notdef
/* block copy from from to to, count bytes */
bcopy(from, to, count)
	register char *from, *to;
	register int count;
{
	while ((--count) >= 0)
		*to++ = *from++;
}
#endif

/*
 * Save the current contents of linebuf, if it has changed.
 */
void
lsave()
{
	char	tmp[LBSIZE];

	if (curbuf == 0 || !DOLsave)	/* Nothing modified recently */
		return;

	if (!strcmp(linebuf, getline(curline->l_dline, tmp)))
		return;		/* They are the same. */
	SavLine(curline, linebuf);	/* Put linebuf on the disk */
	DOLsave = 0;
}

void
getDOT()
{
	ignore(getline(curline->l_dline, linebuf));
}

void
SavLine(addr, buf)
LINE	*addr;
char	*buf;
{
	addr->l_dline = putline(buf);
}
