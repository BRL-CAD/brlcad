/*LINTLIBRARY*/
/*
	Author:		Gary S. Moss
			U. S. Army Ballistic Research Laboratory
			Aberdeen Proving Ground
			Maryland 21005-5066
			(301)278-6647 or AV-298-6647
*/
#ifndef lint
static char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include <stdio.h>
#include <fcntl.h>

#ifndef _NFILE
#define _NFILE	64
#endif

#ifdef SYSV
#include <termio.h>
#include <memory.h>

static struct termio	save_tio[_NFILE], curr_tio[_NFILE];
#endif /* SYSV */

#ifdef BSD
#include <sys/ioctl.h>
/****#ifdef	TANDEM	/* 7th Edition derivatives */
#define TCSETA	TIOCSETP
#define TCGETA	TIOCGETP
#ifndef	XTABS
#define	XTABS	(TAB1 | TAB2)
#endif /* XTABS */

static struct sgttyb	save_tio[_NFILE], curr_tio[_NFILE];
#endif /* BSD */

static int		fileStatus[_NFILE];
int			reset_Fil_Stat();
void			save_Tty(), reset_Tty();
void			set_Cbreak(), clr_Cbreak();
void			set_Raw(), clr_Raw();
void			set_Echo(), clr_Echo();
void			set_Tabs(), clr_Tabs();
void			set_HUPCL();
void			clr_CRNL();
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

/*	s e t _ C b r e a k ( )
	Set CBREAK mode, 'fd'.
 */
void
set_Cbreak( fd )
int	fd;
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

/*	c l r _ R a w ( )
	Set cooked mode, 'fd'.
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

/*	s e t _ R a w ( )
	Set raw mode, 'fd'.
 */
void
set_Raw( fd )
int	fd;
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

/*	s e t _ E c h o ( )
	Set echo mode, 'fd'.
 */
void
set_Echo( fd )
int	fd;
	{
#ifdef BSD
	curr_tio[fd].sg_flags |= ECHO;		/* Echo mode ON.	*/
#else
	curr_tio[fd].c_lflag |= ECHO;		/* Echo mode ON.	*/
#endif
	(void) ioctl( fd, TCSETA, &curr_tio[fd] );
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
#else
	curr_tio[fd].c_lflag &= ~ECHO;		/* Echo mode OFF.	*/
#endif
	(void) ioctl( fd, TCSETA, &curr_tio[fd] );
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
#else
#ifndef CRAY2
	curr_tio[fd].c_oflag |= TAB3;		/* Tab expansion ON.	*/
#endif
#endif
	(void) ioctl( fd, TCSETA, &curr_tio[fd] );
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
#else
#ifndef CRAY2
	curr_tio[fd].c_oflag &= ~TAB3;		/* Tab expans. OFF.	*/
#endif
#endif
	(void) ioctl( fd, TCSETA, &curr_tio[fd] );
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
#else
	curr_tio[fd].c_cflag |= HUPCL;
	(void) ioctl( fd, TCSETA, &curr_tio[fd] );
#endif
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
#else
#ifndef CRAY2
	curr_tio[fd].c_oflag &= ~(ONLCR|OCRNL);
	curr_tio[fd].c_iflag &= ~(ICRNL|INLCR);
#endif
#endif
	(void) ioctl( fd, TCSETA, &curr_tio[fd] );
	}

/*	g e t _ O _ S p e e d ( )
	Get the terminals output speed, 'fd'.
 */
unsigned short
get_O_Speed( fd )
	{
#ifdef BSD
	return	(unsigned short) save_tio[fd].sg_ospeed;
#else
#ifndef CRAY2
	return	save_tio[fd].c_cflag & CBAUD;
#endif
#endif
	}

/*	s a v e _ T t y ( )
	Get and save terminal parameters, 'fd'.
 */
void
save_Tty( fd )
int	fd;
	{
	(void) ioctl( fd, TCGETA, &save_tio[fd] );
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
	(void) ioctl( fd, TCSETA, &save_tio[fd] ); /* Write setting.		*/
	return;
	}

/*	s a v e _ F i l _ S t a t ( )
	Save file status flags for 'fd'.
 */
save_Fil_Stat( fd )
int	fd;
	{
	return	fileStatus[fd] = fcntl( fd, F_GETFL, 0 );
	}

/*	r e s e t _ F i l _ S t a t ( )
	Restore file status flags for file desc. 'fd' to what they were the
	last time saveFilStat(fd) was called.
 */
reset_Fil_Stat( fd )
int	fd;
	{
	return	fcntl( fd, F_SETFL, fileStatus[fd] );
	}

/*	s e t _ O _ N D E L A Y ( )
	Set non-blocking read on 'fd'.
 */
set_O_NDELAY( fd )
int	fd;
	{
	return	fcntl( fd, F_SETFL, O_NDELAY );
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
	(void)bcopy( (char *)from, (char*)to, sizeof(struct sgttyb) );
#else
	(void) memcpy( (char *) to, (char *) from, sizeof(struct termio) );
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
