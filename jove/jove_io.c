/*
 *			J O V E _ I O . C 
 *
 * $Revision$
 *
 * $Log$
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
static char RCSid[] = "@(#)$Header$";
#endif

/*
   Jonathan Payne at Lincoln-Sudbury Regional High School 5-25-83
  
   Commands to read/write files/regions.  */

#include "./jove.h"
#include <termcap.h>

#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>

extern	int errno;
extern	int Dfltmode;
extern	char *TempFile;

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

/***********************************/

IOclose()
{
	if (io > 0)
		ignore(close(io)), io = -1;
	nextip = iobuff;
	ninbuf = 0;
}

/* This reads a line from the input file into buf. */

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

FileMess(file, lines, chars)
char	*file;
long	chars;
{
	s_mess("\"%s\" %d lines %ld characters", file, lines, chars);
}

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

bufname(bp)
BUFFER	*bp;
{
	char	tmp[100],
		tmp1[100],
		*cp;
	int	try = 1;

	if ((cp = bp->b_fname) == 0)
		complain("No file name");
		
	if (*cp == '.')
		++cp;
	strcpy(tmp, cp);
	cp = rindex(tmp, '/');
	if (cp)
		strncpy(tmp, &cp[1], sizeof tmp);

	strcpy(tmp1, tmp);
	while (buf_exists(tmp)) {
		ignore(sprintf(tmp, "%s[%d]", tmp1, try));
		try++;
	}
	setbname(bp, tmp);
}

SaveFile()
{
	if (IsModified(curbuf)) {
		file_write(curbuf->b_fname, 0);
		SetUnmodified(curbuf);
	} else
		message("No changes need be written");
}

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

WrtReg()
{
	DoWriteReg(0);
}

AppReg()
{
	DoWriteReg(1);
}

DoWriteReg(app)
{
	char	fname[100];
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

WriteFile()
{
	char	*fname,
		fnamebuf[100];

	fname = ask(curbuf->b_fname, FuncName());
	strncpy(fnamebuf, fname, sizeof fnamebuf);

	filemunge(fnamebuf);
	setfname(curbuf, fnamebuf);
	set_ino(curbuf);
	file_write(curbuf->b_fname, exp_p);
	SetUnmodified(curbuf);
}

char	*SaveName();

file_write(fname, app)
char	*fname;
{
	struct stat	inode;
	register int	exists;
	register int	linked;
	int		saveit = 0;
	int		overwrite = 0;

	if (fname == 0 || *fname == '\0')
		complain("I need a file name");

	io = -1;
	inode.st_mode = Dfltmode;
	inode.st_nlink = 1;
	inode.st_uid = getuid();
	inode.st_gid = getgid();
	exists = !stat(fname, &inode);
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

		/*  This will fail on all but USG systems, ignore it  */
		chown (fname, inode.st_uid, inode.st_gid);
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
}

char *
SaveName (fname)
char *fname;
{
	register int	len = strlen(fname);
	register char *cp;

#ifndef V4_2BSD
	if (cp = rindex(fname, '/'))
		cp++;
	else
		cp = fname;

	if (strlen(cp) > 13)
		complain("Cannot make backup name; \"%s\" too long", cp);
#endif
	cp = emalloc (len+2);
	strncpy (cp, fname, len);
	cp[len] = '~';
	cp[len+1] = 0;
	return (cp);
}

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

setcmode()
{
	register int	len;

	len = curbuf->b_fname ? strlen(curbuf->b_fname) : 0;
	
	if (len < 2 || IsDisabled(globflags, CMODE))
		return;
	if (curbuf->b_fname[--len] == 'c' && curbuf->b_fname[--len] == '.')
		OnFlag(globflags, CMODE);
}

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

InsFile()
{
	char	*fname;

	fname = ask(curbuf->b_fname, FuncName());
	read_file(fname);
	SetModified(curbuf);
}

dolseek(fd, offset, whence)
long	offset;
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

tmpinit()
{
#ifdef SYS5_SGI
	tfname = (char *) xtempnam(NULL, TempFile);
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
	extern int	read(),
			write();
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

#ifdef	VMUNIX
#define	INCORB	64
char	incorb[INCORB+1][BSIZ];
#define	pagrnd(a)	((char *)(((int)a)&~(BSIZ-1)))
#endif

blkio(b, buf, iofcn)
short	b;
char	*buf;
int	(*iofcn)();
{

#ifdef VMUNIX
	if (b < INCORB) {
		if (iofcn == read) {
			bcopy(pagrnd(incorb[b+1]), buf, BSIZ);
			return;
		}
		bcopy(buf, pagrnd(incorb[b+1]), BSIZ);
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

getDOT()
{
	ignore(getline(curline->l_dline, linebuf));
}

SavLine(addr, buf)
LINE	*addr;
char	*buf;
{
	addr->l_dline = putline(buf);
}
