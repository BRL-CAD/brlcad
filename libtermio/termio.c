/*LINTLIBRARY*/
/*
	SCCS id:	@(#) termio.c	1.1
	Last edit: 	3/13/85 at 19:00:54
	Retrieved: 	8/13/86 at 03:17:50
	SCCS archive:	/m/cad/fb_utils/RCS/s.termio.c

	Author:		Gary S. Moss
			U. S. Army Ballistic Research Laboratory
			Aberdeen Proving Ground
			Maryland 21005
			(301)278-6647 or AV-283-6647
 */
static
char	sccsTag[] = "@(#) termio.c	1.1	last edit 3/13/85 at 19:00:54";
#include <stdio.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <termio.h>
#include <memory.h>

static struct termio	save_tio[_NFILE], curr_tio[_NFILE];
static int		fileStatus[_NFILE];
int			reset_Fil_Stat(), read_Key_Brd();
void			save_Tty(), reset_Tty();
void			set_Raw(), clr_Raw();
void			set_Echo(), clr_Echo();
void			set_Tabs(), clr_Tabs();
void			prnt_Tio();
static void		copy_Tio();

/*	c l r _ R a w ( )
	Set cooked mode, for file descriptor 'fd'.
 */
void
clr_Raw( fd )
int	fd;
	{
	curr_tio[fd].c_lflag |= ICANON;		/* Raw mode OFF.	*/
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
	curr_tio[fd].c_lflag &= ~ICANON;	/* Raw mode ON.		*/
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
	curr_tio[fd].c_lflag |= ECHO;		/* Echo mode ON.	*/
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
	curr_tio[fd].c_lflag &= ~ECHO;		/* Echo mode OFF.	*/
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
	curr_tio[fd].c_oflag |= TAB3;		/* Tab expansion ON.	*/
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
	curr_tio[fd].c_oflag &= ~TAB3;		/* Tab expans. OFF.	*/
	(void) ioctl( fd, TCSETA, &curr_tio[fd] );
	return;
	}

/*	g e t _ O _ S p e e d ( )
	Get the terminals output speed, 'fd'.
 */
unsigned short
get_O_Speed( fd )
	{
	return	save_tio[fd].c_cflag & CBAUD;
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

/*	r e a d _ K e y _ B r d ( )
 */
read_Key_Brd()
	{
	int	c;

	return	read( 0, (char *) &c, 1 ) == 0 ? 0 : c;
	}

/*	c o p y _ T i o ( )
 */
static void
copy_Tio( to, from )
struct termio	*to, *from;
	{
	(void) memcpy( (char *) to, (char *) from, sizeof(struct termio) );
	return;
	}

/*	p r n t _ T i o ( )
 */
void
prnt_Tio( msg, tio_ptr )
char		*msg;
struct termio	*tio_ptr;
	{
	register int	i;

	(void) fprintf( stderr, "%s :\n\r", msg );
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
	}
