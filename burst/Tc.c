/*
	XXX Outdated : should not be used
  
	SCCS id:	%Z% %M%	%I%
	Modified: 	%G% at %U%
	Retrieved: 	%H% at %T%
	SCCS archive:	%P%

	Author:	Gary S. Moss
	U. S. Army Ballistic Research Laboratory
	Aberdeen Proving Ground
	Maryland 21005-5066
*/

#if 0

/*LINTLIBRARY*/
#ifndef lint
static char RCSid[] = "@(#)$Header$ (BRL)";
static char sccsTag[] = "%Z% %M% %I%, modified %G% at %U%, archive %P%";
#endif

#include <stdio.h>
#include <fcntl.h>

#ifdef SYSV

/**#ifndef	TANDEM	*/
/* USG derivatives */
#include <termio.h>
#include <memory.h>
#ifndef _NFILE
#define _NFILE	32
#endif /* _NFILE */
static struct termio	save_tio[_NFILE], curr_tio[_NFILE];
#endif /* SYSV */

#ifdef BSD
#include <sys/ioctl.h>
#ifdef HAVE_IOCTL_COMPAT_H
#include <sys/ioctl_compat.h>
#endif
#ifndef _NFILE
#define _NFILE	32
#endif /* _NFILE */
/****#ifdef	TANDEM	*/
/* 7th Edition derivatives */
#define TCSETA	TIOCSETP
#define TCGETA	TIOCGETP
#ifndef	XTABS
#define	XTABS	(TAB1 | TAB2)
#endif /* XTABS */

static struct sgttyb	save_tio[_NFILE], curr_tio[_NFILE];
#endif /* BSD */

void			TcSaveTty(), TcResetTty();
void			TcSetCbreak(), TcClrCbreak();
void			TcSetRaw(), TcClrRaw();
void			TcSetEcho(), TcClrEcho();
void			TcSetTabs(), TcClrTabs();
void			TcSetHUPCL();
void			TcClrCRNL();
void			prnt_Tio();
static void		copy_Tio();

/*	
	void	TcClrCbreak( int fd )

	Clear CBREAK mode, for file descriptor "fd".
 */
void
#if __STDC__
TcClrCbreak( int fd )
#else
TcClrCbreak( fd )
int	fd;
#endif
	{
#ifdef BSD
	curr_tio[fd].sg_flags &= ~CBREAK;	/* CBREAK mode OFF.	*/
#else
	curr_tio[fd].c_lflag |= ICANON;		/* Canonical input ON.	*/
#ifndef CRAY2
	curr_tio[fd].c_cc[VEOF] = 4;		/* defaults!		*/
	curr_tio[fd].c_cc[VEOL] = 0;		/*   best we can do.... */
#endif
#endif
	(void) ioctl( fd, TCSETA, &curr_tio[fd] );
	return;
	}

/*
	void	TcSetCbreak( int fd )

	Set CBREAK mode, "fd".
 */
void
#if __STDC__
TcSetCbreak( int fd )
#else
TcSetCbreak( fd )
int	fd;
#endif
	{
#ifdef BSD
	curr_tio[fd].sg_flags |= CBREAK;	/* CBREAK mode ON.	*/
#else
	curr_tio[fd].c_lflag &= ~ICANON;	/* Canonical input OFF. */
#ifndef CRAY2
	curr_tio[fd].c_cc[VMIN] = 1;
	curr_tio[fd].c_cc[VTIME] = 0;
#endif
#endif
	(void) ioctl( fd, TCSETA, &curr_tio[fd] );
	return;
	}

/*
	void	TcClrRaw( int fd )

	Set cooked mode, "fd".
 */
void
clr_Raw( fd )
int	fd;
	{
#ifdef BSD
	curr_tio[fd].sg_flags &= ~RAW;		/* Raw mode OFF.	*/
#else
	curr_tio[fd].c_lflag |= ICANON;		/* Canonical input ON.	*/
#ifndef CRAY2
	curr_tio[fd].c_lflag |= ISIG;		/* Signals ON.		*/
	curr_tio[fd].c_cc[VEOF] = 4;		/* defaults!		*/
	curr_tio[fd].c_cc[VEOL] = 0;		/*   best we can do.... */
#endif
#endif
	(void) ioctl( fd, TCSETA, &curr_tio[fd] );
	return;
	}

/*
	void	TcSetRaw( int fd )

	Set raw mode, "fd".
 */
void
#if __STDC__
TcSetRaw( int fd )
#else
TcSetRaw( fd )
int	fd;
#endif
	{
#ifdef BSD
	curr_tio[fd].sg_flags |= RAW;		/* Raw mode ON.		*/
#else
	curr_tio[fd].c_lflag &= ~ICANON;	/* Canonical input OFF. */
#ifndef CRAY2
	curr_tio[fd].c_lflag &= ~ISIG;		/* Signals OFF.		*/
	curr_tio[fd].c_cc[VMIN] = 1;
	curr_tio[fd].c_cc[VTIME] = 0;
#endif
#endif
	(void) ioctl( fd, TCSETA, &curr_tio[fd] );
	return;
	}

/*
	void	TcSetEcho( int fd )

	Set echo mode, "fd".
 */
void
#if __STDC__
TcSetEcho( int fd )
#else
TcSetEcho( fd )
int	fd;
#endif
	{
#ifdef BSD
	curr_tio[fd].sg_flags |= ECHO;		/* Echo mode ON.	*/
#else
	curr_tio[fd].c_lflag |= ECHO;		/* Echo mode ON.	*/
#endif
	(void) ioctl( fd, TCSETA, &curr_tio[fd] );
	return;
	}

/*
	void	TcClrEcho( int fd )

	Clear echo mode, "fd".
 */
void
#if __STDC__
TcClrEcho( int fd )
#else
TcClrEcho( fd )
int	fd;
#endif
	{
#ifdef BSD
	curr_tio[fd].sg_flags &= ~ECHO;		/* Echo mode OFF.	*/
#else
	curr_tio[fd].c_lflag &= ~ECHO;		/* Echo mode OFF.	*/
#endif
	(void) ioctl( fd, TCSETA, &curr_tio[fd] );
	return;
	}

/*
	void	TcSetTabs( int fd )

	Turn on tab expansion, "fd".
 */
void
#if __STDC__
TcSetTabs( int fd )
#else
TcSetTabs( fd )
int	fd;
#endif
	{
#ifdef BSD
	curr_tio[fd].sg_flags |= XTABS;		/* Tab expansion ON.	*/
#else
#ifndef CRAY2
	curr_tio[fd].c_oflag |= TAB3;		/* Tab expansion ON.	*/
#endif
#endif
	(void) ioctl( fd, TCSETA, &curr_tio[fd] );
	return;
	}

/*
	void	TcClrTabs( int fd )

	Turn off tab expansion, "fd".
 */
void
#if __STDC__
TcClrTabs( int fd )
#else
TcClrTabs( fd )
int	fd;
#endif
	{
#ifdef BSD
	curr_tio[fd].sg_flags &= ~XTABS;	/* Tab expans. OFF.	*/
#else
#ifndef CRAY2
	curr_tio[fd].c_oflag &= ~TAB3;		/* Tab expans. OFF.	*/
#endif
#endif
	(void) ioctl( fd, TCSETA, &curr_tio[fd] );
	return;
	}

/*
	void	TcSetHUPCL( int fd )

	Turn on "Hang up on last close", "fd".
 */
void
#if __STDC__
TcSetHUPCL( int fd )
#else
TcSetHUPCL( fd )
int	fd;
#endif
	{
#ifndef CRAY2
#ifdef BSD
	(void) ioctl( fd, TIOCHPCL, NULL );
#else
	curr_tio[fd].c_cflag |= HUPCL;
	(void) ioctl( fd, TCSETA, &curr_tio[fd] );
#endif
#endif
	return;
	}

/*
	void	TcClrCRNL( int fd )

	Turn off CR/LF mapping, "fd".
 */
void
#if __STDC__
TcClrCRNL( int fd )
#else
TcClrCRNL( fd )
#endif
	{
#ifdef BSD
	curr_tio[fd].sg_flags &= ~CRMOD;
#else
#ifndef CRAY2
	curr_tio[fd].c_oflag &= ~(ONLCR|OCRNL);
	curr_tio[fd].c_iflag &= ~(ICRNL|INLCR);
#endif
#endif
	(void) ioctl( fd, TCSETA, &curr_tio[fd] );
	}

/*
	void	TcSaveTty( int fd )

	Get and save terminal parameters, "fd".
 */
void
#if __STDC__
TcSaveTty( int fd )
#else
TcSaveTty( fd )
int	fd;
#endif
	{
	(void) ioctl( fd, TCGETA, &save_tio[fd] );
	copy_Tio( &curr_tio[fd], &save_tio[fd] );
	return;
	}

/*
	void	TcResetTty( int fd )

	Set the terminal back to the mode that the user had last time
	TcSaveTty() was called for "fd".
 */
void
#if __STDC__
TcResetTty( int fd )
#else
TcResetTty( fd )
int	fd;
#endif
	{
	(void) ioctl( fd, TCSETA, &save_tio[fd] ); /* Write setting.		*/
	return;
	}

/*	c o p y _ T i o ( )						*/
static void
copy_Tio( to, from )
#ifdef BSD
struct sgttyb	*to, *from;
#else
struct termio	*to, *from;
#endif
	{
#ifdef BSD
	(void) bcopy( (char *)from, (char*)to, sizeof(struct sgttyb) );
#else
	(void) memcpy( (char *) to, (char *) from, (int) sizeof(struct termio) );
#endif
	return;
	}

/*	p r n t _ T i o ( )						*/
void
prnt_Tio( msg, tio_ptr )
char		*msg;
#ifdef BSD
struct sgttyb	*tio_ptr;
#else
struct termio	*tio_ptr;
#endif
	{	register int	i;
	(void) fprintf( stderr, "%s :\n\r", msg );
#ifdef BSD
	(void) fprintf( stderr, "\tsg_ispeed=%d\n\r", (int) tio_ptr->sg_ispeed );
	(void) fprintf( stderr, "\tsg_ospeed=%d\n\r", (int) tio_ptr->sg_ospeed );
	(void) fprintf( stderr, "\tsg_erase='%c'\n\r", tio_ptr->sg_erase );
	(void) fprintf( stderr, "\tsg_kill='%c'\n\r", tio_ptr->sg_kill );
	(void) fprintf( stderr, "\tsg_flags=0x%x\n\r", tio_ptr->sg_flags );
#else

	(void) fprintf( stderr, "\tc_iflag=0x%x\n\r", tio_ptr->c_iflag );
	(void) fprintf( stderr, "\tc_oflag=0x%x\n\r", tio_ptr->c_oflag );
	(void) fprintf( stderr, "\tc_cflag=0x%x\n\r", tio_ptr->c_cflag );
	(void) fprintf( stderr, "\tc_lflag=0x%x\n\r", tio_ptr->c_lflag );
	(void) fprintf( stderr, "\tc_line=%c\n\r", tio_ptr->c_line );
	for( i = 0; i < NCC; ++i )
		{
		(void) fprintf( stderr,
				"\tc_cc[%d]=0%o\n\r",
				i,
				tio_ptr->c_cc[i]
				);
		}
#endif
	return;
	}

#endif
