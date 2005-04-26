/*LINTLIBRARY*/
/*
	Author:		Gary S. Moss
			U. S. Army Ballistic Research Laboratory
			Aberdeen Proving Ground
			Maryland 21005-5066
			(301)278-6647 or AV-298-6647
*/
#ifndef lint
static const char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "common.h"



#include <stdio.h>

#ifdef HAVE_SYS_FILE_H
#  include <sys/file.h>
#endif

#ifdef HAVE_FCNTL_H
#  include <fcntl.h>
#endif

#if HAVE_SYS_IOCTL_COMPAT_H
#	include <sys/ioctl_compat.h>
#	define TAB3 (TAB1|TAB2)
#	if !defined(OCRNL)
#		define OCRNL   0000010
#	endif
#endif



#include "machine.h"

/*
 *  This file will work IFF one of these three flags is set:
 *	HAVE_TERMIOS_H	use POXIX termios and tcsetattr() call with XOPEN flags
 *	SYSV		use SysV Rel3 termio and TCSETA ioctl
 *	BSD		use Version 7 / BSD sgttyb and TIOCSETP ioctl
 */

#if defined(HAVE_MEMORY_H)
#  include <memory.h>
#endif

#if defined(HAVE_TERMIOS_H)
#  undef SYSV
#  undef BSD
#  include <termios.h>

static struct termios	save_tio[FOPEN_MAX], curr_tio[FOPEN_MAX];

#else	/* !defined(HAVE_TERMIOS_H) */

#  ifdef SYSV
#    undef BSD
#    include <termio.h>
#    include <memory.h>
	static struct termio	save_tio[FOPEN_MAX], curr_tio[FOPEN_MAX];
#  endif /* SYSV */

#  ifdef BSD
#    undef SYSV
#    include <sys/ioctl.h>
#      ifndef	XTABS
#	define	XTABS	(TAB1 | TAB2)
#      endif /* XTABS */

	static struct sgttyb	save_tio[FOPEN_MAX], curr_tio[FOPEN_MAX];
#  endif /* BSD */

#endif /* HAVE_TERMIOS_H */

#include "libtermio.h"

static int		fileStatus[FOPEN_MAX];
void			prnt_Tio();
static void		copy_Tio();

/*	c l r _ C b r e a k ( )
	Clear CBREAK mode, for file descriptor 'fd'.
 */
void
clr_Cbreak( fd )
int	fd;
	{
#ifdef BSD
	curr_tio[fd].sg_flags &= ~CBREAK;	/* CBREAK mode OFF.	*/
	(void) ioctl( fd, TIOCSETP, &curr_tio[fd] );
#endif
#ifdef SYSV
	curr_tio[fd].c_lflag |= ICANON;		/* Canonical input ON.	*/
#ifndef CRAY2
	curr_tio[fd].c_cc[VEOF] = 4;		/* defaults!		*/
	curr_tio[fd].c_cc[VEOL] = 0;		/*   best we can do.... */
#endif
	(void) ioctl( fd, TCSETA, &curr_tio[fd] );
#endif
#ifdef HAVE_TERMIOS_H
	curr_tio[fd].c_lflag |= ICANON;		/* Canonical input ON.	*/
	curr_tio[fd].c_cc[VEOF] = 4;		/* defaults!		*/
	curr_tio[fd].c_cc[VEOL] = 0;		/*   best we can do.... */
	(void)tcsetattr( fd, TCSAFLUSH, &curr_tio[fd] );
#endif
	return;
	}

/*	s e t _ C b r e a k ( )
	Set CBREAK mode, 'fd'.
 */
void
set_Cbreak( fd )
int	fd;
	{
#ifdef BSD
	curr_tio[fd].sg_flags |= CBREAK;	/* CBREAK mode ON.	*/
	(void) ioctl( fd, TIOCSETP, &curr_tio[fd] );
#endif
#ifdef SYSV
	curr_tio[fd].c_lflag &= ~ICANON;	/* Canonical input OFF. */
#ifndef CRAY2
	curr_tio[fd].c_cc[VMIN] = 1;
	curr_tio[fd].c_cc[VTIME] = 0;
#endif
	(void) ioctl( fd, TCSETA, &curr_tio[fd] );
#endif
#ifdef HAVE_TERMIOS_H
	curr_tio[fd].c_lflag &= ~ICANON;	/* Canonical input OFF. */
	curr_tio[fd].c_cc[VMIN] = 1;
	curr_tio[fd].c_cc[VTIME] = 0;
	(void)tcsetattr( fd, TCSANOW, &curr_tio[fd] );
#endif
	return;
	}

/*	c l r _ R a w ( )
	Set cooked mode, 'fd'.
 */
void
clr_Raw( fd )
int	fd;
	{
#ifdef BSD
	curr_tio[fd].sg_flags &= ~RAW;		/* Raw mode OFF.	*/
	(void) ioctl( fd, TIOCSETP, &curr_tio[fd] );
#endif
#ifdef SYSV
	curr_tio[fd].c_lflag |= ICANON;		/* Canonical input ON.	*/
#ifndef CRAY2
	curr_tio[fd].c_lflag |= ISIG;		/* Signals ON.		*/
	curr_tio[fd].c_cc[VEOF] = 4;		/* defaults!		*/
	curr_tio[fd].c_cc[VEOL] = 0;		/*   best we can do.... */
#endif
	(void) ioctl( fd, TCSETA, &curr_tio[fd] );
#endif
#ifdef HAVE_TERMIOS_H
	curr_tio[fd].c_lflag |= ICANON;		/* Canonical input ON.	*/
	curr_tio[fd].c_lflag |= ISIG;		/* Signals ON.		*/
	curr_tio[fd].c_cc[VEOF] = 4;		/* defaults!		*/
	curr_tio[fd].c_cc[VEOL] = 0;		/*   best we can do.... */
	(void)tcsetattr( fd, TCSAFLUSH, &curr_tio[fd] );
#endif
	return;
	}

/*	s e t _ R a w ( )
	Set raw mode, 'fd'.
 */
void
set_Raw( fd )
int	fd;
	{
#ifdef BSD
	curr_tio[fd].sg_flags |= RAW;		/* Raw mode ON.		*/
	(void) ioctl( fd, TIOCSETP, &curr_tio[fd] );
#endif
#ifdef SYSV
	curr_tio[fd].c_lflag &= ~ICANON;	/* Canonical input OFF. */
#ifndef CRAY2
	curr_tio[fd].c_lflag &= ~ISIG;		/* Signals OFF.		*/
	curr_tio[fd].c_cc[VMIN] = 1;
	curr_tio[fd].c_cc[VTIME] = 0;
#endif
	(void) ioctl( fd, TCSETA, &curr_tio[fd] );
#endif
#ifdef HAVE_TERMIOS_H
	curr_tio[fd].c_lflag &= ~ICANON;	/* Canonical input OFF. */
	curr_tio[fd].c_lflag &= ~ISIG;		/* Signals OFF.		*/
	curr_tio[fd].c_cc[VMIN] = 1;
	curr_tio[fd].c_cc[VTIME] = 0;
	(void)tcsetattr( fd, TCSANOW, &curr_tio[fd] );
#endif
	return;
	}

/*	s e t _ E c h o ( )
	Set echo mode, 'fd'.
 */
void
set_Echo( fd )
int	fd;
	{
#ifdef BSD
	curr_tio[fd].sg_flags |= ECHO;		/* Echo mode ON.	*/
	(void) ioctl( fd, TIOCSETP, &curr_tio[fd] );
#endif
#ifdef SYSV
	curr_tio[fd].c_lflag |= ECHO;		/* Echo mode ON.	*/
	(void) ioctl( fd, TCSETA, &curr_tio[fd] );
#endif
#ifdef HAVE_TERMIOS_H
	curr_tio[fd].c_lflag |= ECHO;		/* Echo mode ON.	*/
	(void)tcsetattr( fd, TCSANOW, &curr_tio[fd] );
#endif
	return;
	}

/*	c l r _ E c h o ( )
	Clear echo mode, 'fd'.
 */
void
clr_Echo( fd )
int	fd;
	{
#ifdef BSD
	curr_tio[fd].sg_flags &= ~ECHO;		/* Echo mode OFF.	*/
	(void) ioctl( fd, TIOCSETP, &curr_tio[fd] );
#endif
#ifdef SYSV
	curr_tio[fd].c_lflag &= ~ECHO;		/* Echo mode OFF.	*/
	(void) ioctl( fd, TCSETA, &curr_tio[fd] );
#endif
#ifdef HAVE_TERMIOS_H
	curr_tio[fd].c_lflag &= ~ECHO;		/* Echo mode OFF.	*/
	(void)tcsetattr( fd, TCSANOW, &curr_tio[fd] );
#endif
	return;
	}

/*	s e t _ T a b s ( )
	Turn on tab expansion, 'fd'.
 */
void
set_Tabs( fd )
int	fd;
	{
#ifdef BSD
	curr_tio[fd].sg_flags |= XTABS;		/* Tab expansion ON.	*/
	(void) ioctl( fd, TIOCSETP, &curr_tio[fd] );
#endif
#ifdef SYSV
#ifndef CRAY2
	curr_tio[fd].c_oflag |= TAB3;		/* Tab expansion ON.	*/
#endif
	(void) ioctl( fd, TCSETA, &curr_tio[fd] );
#endif
#ifdef HAVE_TERMIOS_H
	curr_tio[fd].c_oflag |= TAB3;		/* Tab expansion ON.	*/
	(void)tcsetattr( fd, TCSANOW, &curr_tio[fd] );
#endif
	return;
	}

/*	c l r _ T a b s ( )
	Turn off tab expansion, 'fd'.
 */
void
clr_Tabs( fd )
int	fd;
	{
#ifdef BSD
	curr_tio[fd].sg_flags &= ~XTABS;	/* Tab expans. OFF.	*/
	(void) ioctl( fd, TIOCSETP, &curr_tio[fd] );
#endif
#ifdef SYSV
#ifndef CRAY2
	curr_tio[fd].c_oflag &= ~TAB3;		/* Tab expans. OFF.	*/
#endif
	(void) ioctl( fd, TCSETA, &curr_tio[fd] );
#endif
#ifdef HAVE_TERMIOS_H
	curr_tio[fd].c_oflag &= ~TAB3;		/* Tab expans. OFF.	*/
	(void)tcsetattr( fd, TCSANOW, &curr_tio[fd] );
#endif
	return;
	}

/*	s e t _ H U P C L ( )
	Turn on "Hang up on last close", 'fd'.
 */
void
set_HUPCL( fd )
int	fd;
	{
#ifndef CRAY2
#ifdef BSD
	(void) ioctl( fd, TIOCHPCL, NULL );
#endif
#ifdef SYSV
	curr_tio[fd].c_cflag |= HUPCL;
	(void) ioctl( fd, TCSETA, &curr_tio[fd] );
#endif
#endif
#ifdef HAVE_TERMIOS_H
	curr_tio[fd].c_cflag |= HUPCL;
	(void)tcsetattr( fd, TCSANOW, &curr_tio[fd] );
#endif
	return;
	}

/*	c l r _ C R N L ( )
	Turn off CR/LF mapping, fd.
 */
void
clr_CRNL( fd )
	{
#ifdef BSD
	curr_tio[fd].sg_flags &= ~CRMOD;
	(void) ioctl( fd, TIOCSETP, &curr_tio[fd] );
#endif
#ifdef SYSV
#ifndef CRAY2
	curr_tio[fd].c_oflag &= ~(ONLCR|OCRNL);
	curr_tio[fd].c_iflag &= ~(ICRNL|INLCR);
#endif
	(void) ioctl( fd, TCSETA, &curr_tio[fd] );
#endif
#ifdef HAVE_TERMIOS_H
	curr_tio[fd].c_oflag &= ~(ONLCR|OCRNL);
	curr_tio[fd].c_iflag &= ~(ICRNL|INLCR);
	(void)tcsetattr( fd, TCSAFLUSH, &curr_tio[fd] );
#endif
	}

/*	g e t _ O _ S p e e d ( )
	Get the terminals output speed, 'fd'.
 */
unsigned short
get_O_Speed( fd )
	{
#ifdef BSD
	return	(unsigned short) save_tio[fd].sg_ospeed;
#endif
#ifdef SYSV
#ifndef CRAY2
	return	save_tio[fd].c_cflag & CBAUD;
#endif
#endif
#ifdef HAVE_TERMIOS_H
	return	cfgetospeed( &save_tio[fd] );
#endif
	}

/*	s a v e _ T t y ( )
	Get and save terminal parameters, 'fd'.
 */
void
save_Tty( fd )
int	fd;
	{
#if BSD
	(void) ioctl( fd, TIOCGETP, &save_tio[fd] );
#endif
#ifdef SYSV
	(void) ioctl( fd, TCGETA, &save_tio[fd] );
#endif
#ifdef HAVE_TERMIOS_H
	(void)tcgetattr( fd, &save_tio[fd] );
#endif
	copy_Tio( &curr_tio[fd], &save_tio[fd] );
	return;
	}

/*	r e s e t _ T t y ( )
	Set the terminal back to the mode that the user had last time
	save_Tty() was called for 'fd'.
 */
void
reset_Tty( fd )
int	fd;
	{
#if BSD
	(void) ioctl( fd, TIOCSETP, &save_tio[fd] ); /* Write setting.		*/
#endif
#if SYSV
	(void) ioctl( fd, TCSETA, &save_tio[fd] ); /* Write setting.		*/
#endif
#if HAVE_TERMIOS_H
	(void)tcsetattr( fd, TCSAFLUSH, &save_tio[fd] );
#endif
	return;
	}

/*	s a v e _ F i l _ S t a t ( )
	Save file status flags for 'fd'.
 */
int
save_Fil_Stat( fd )
int	fd;
	{
	return	fileStatus[fd] = fcntl( fd, F_GETFL, 0 );
	}

/*	r e s e t _ F i l _ S t a t ( )
	Restore file status flags for file desc. 'fd' to what they were the
	last time saveFilStat(fd) was called.
 */
int
reset_Fil_Stat( fd )
int	fd;
	{
	return	fcntl( fd, F_SETFL, fileStatus[fd] );
	}

/*	s e t _ O _ N D E L A Y ( )
	Set non-blocking read on 'fd'.
 */
int
set_O_NDELAY( fd )
int	fd;
	{
#if defined(SYSV) || defined(BSD)
	return	fcntl( fd, F_SETFL, O_NDELAY );
#else
#if HAVE_TERMIOS_H
	return	fcntl( fd, F_SETFL, FNDELAY );
#endif
#endif
	}

/*	c o p y _ T i o ( )						*/
static void
copy_Tio( to, from )
#ifdef BSD
struct sgttyb	*to, *from;
#endif
#ifdef SYSV
struct termio	*to, *from;
#endif
#ifdef HAVE_TERMIOS_H
struct termios	*to;
struct termios	*from;
#endif
	{
#ifdef BSD
	(void)bcopy( (char *)from, (char*)to, sizeof(struct sgttyb) );
#endif
#ifdef SYSV
	(void) memcpy( (char *) to, (char *) from, sizeof(struct termio) );
#endif
#ifdef HAVE_TERMIOS_H
	(void) memcpy( (char *) to, (char *) from, sizeof(struct termios) );
#endif
	return;
	}

/*	p r n t _ T i o ( )						*/
void
prnt_Tio( msg, tio_ptr )
char		*msg;
#ifdef BSD
struct sgttyb	*tio_ptr;
#endif
#ifdef SYSV
struct termio	*tio_ptr;
#endif
#ifdef HAVE_TERMIOS_H
struct termios	*tio_ptr;
#endif
	{	register int	i;
	(void) fprintf( stderr, "%s :\n\r", msg );
#ifdef BSD
	(void) fprintf( stderr, "\tsg_ispeed=%d\n\r", (int) tio_ptr->sg_ispeed );
	(void) fprintf( stderr, "\tsg_ospeed=%d\n\r", (int) tio_ptr->sg_ospeed );
	(void) fprintf( stderr, "\tsg_erase='%c'\n\r", tio_ptr->sg_erase );
	(void) fprintf( stderr, "\tsg_kill='%c'\n\r", tio_ptr->sg_kill );
	(void) fprintf( stderr, "\tsg_flags=0x%x\n\r", tio_ptr->sg_flags );
#endif
#ifdef SYSV

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
#ifdef HAVE_TERMIOS_H

	(void) fprintf( stderr, "\tc_iflag=0x%x\n\r", (unsigned int)tio_ptr->c_iflag );
	(void) fprintf( stderr, "\tc_oflag=0x%x\n\r", (unsigned int)tio_ptr->c_oflag );
	(void) fprintf( stderr, "\tc_cflag=0x%x\n\r", (unsigned int)tio_ptr->c_cflag );
	(void) fprintf( stderr, "\tc_lflag=0x%x\n\r", (unsigned int)tio_ptr->c_lflag );
	for( i = 0; i < NCCS; ++i )
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

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
