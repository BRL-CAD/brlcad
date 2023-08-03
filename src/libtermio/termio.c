/*                        T E R M I O . C
 * BRL-CAD
 *
 * Copyright (c) 2007-2023 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file libtermio/termio.c
 *
 */

#include "common.h"

#ifdef HAVE_SYS_FILE_H
#  include <sys/file.h>
#endif

#ifdef HAVE_SYS_IOCTL_COMPAT_H
#  if defined(__FreeBSD__) && !defined(COMPAT_43TTY) /* TODO: figure out a better way, mebbe 43bsd tty semantics isn't right anymore? */
#    define COMPAT_43TTY 1
#  endif
#  include <sys/ioctl_compat.h>
#  if !defined(OCRNL)
#    define OCRNL 0000010
#  endif

#endif

#include "bio.h"

/*
 * This file will work IFF one of these three flags is set:
 * HAVE_TERMIOS_H	use POXIX termios and tcsetattr() call with XOPEN flags
 * SYSV			use SysV Rel3 termio and TCSETA ioctl
 * BSD			use Version 7 / BSD sgttyb and TIOCSETP ioctl
 */

#include <string.h>
#if defined(HAVE_MEMORY_H)
#  include <memory.h>
#endif

#if defined(HAVE_TERMIOS_H)
#  undef SYSV
#  undef BSD
#  include <termios.h>

static struct termios save_tio[FOPEN_MAX], curr_tio[FOPEN_MAX];

#else	/* !defined(HAVE_TERMIOS_H) */

#  ifdef SYSV
#    undef BSD
#    include <termio.h>
#    include <memory.h>
static struct termio save_tio[FOPEN_MAX], curr_tio[FOPEN_MAX];
#  endif /* SYSV */

#  ifdef BSD
#    undef SYSV
#    include <sys/ioctl.h>

static struct sgttyb save_tio[FOPEN_MAX], curr_tio[FOPEN_MAX];
#  endif /* BSD */

#endif /* HAVE_TERMIOS_H */

#include "libtermio.h"

static void
copy_Tio(
#if defined(BSD)
	struct sgttyb *to, struct sgttyb *from
#elif defined(SYSV)
	struct termio *to, struct termio *from
#elif defined(HAVE_TERMIOS_H)
	struct termios *to, struct termios*from
#endif
	)
{
    (void)memcpy((char *) to, (char *) from, sizeof(*from));
    return;
}


/*
   Clear echo mode, 'fd'.
*/
void
clr_Echo(int fd)
{
#ifdef BSD
    curr_tio[fd].sg_flags &= ~ECHO;		/* Echo mode OFF.	*/
    (void) ioctl(fd, TIOCSETP, &curr_tio[fd]);
#endif
#ifdef SYSV
    curr_tio[fd].c_lflag &= ~ECHO;		/* Echo mode OFF.	*/
    (void) ioctl(fd, TCSETA, &curr_tio[fd]);
#endif
#ifdef HAVE_TERMIOS_H
    curr_tio[fd].c_lflag &= ~ECHO;		/* Echo mode OFF.	*/
    (void)tcsetattr(fd, TCSANOW, &curr_tio[fd]);
#endif
    return;
}

/*
   Set the terminal back to the mode that the user had last time
   save_Tty() was called for 'fd'.
*/
void
reset_Tty(int fd)
{
#ifdef BSD
    (void) ioctl(fd, TIOCSETP, &save_tio[fd]); /* Write setting.		*/
#endif
#ifdef SYSV
    (void) ioctl(fd, TCSETA, &save_tio[fd]); /* Write setting.		*/
#endif
#ifdef HAVE_TERMIOS_H
    (void)tcsetattr(fd, TCSAFLUSH, &save_tio[fd]);
#endif
    return;
}

/*
   Get and save terminal parameters, 'fd'.
*/
void
save_Tty(int fd)
{
#ifdef BSD
    (void) ioctl(fd, TIOCGETP, &save_tio[fd]);
#endif
#ifdef SYSV
    (void) ioctl(fd, TCGETA, &save_tio[fd]);
#endif
#ifdef HAVE_TERMIOS_H
    (void)tcgetattr(fd, &save_tio[fd]);
#endif
    copy_Tio(&curr_tio[fd], &save_tio[fd]);
    return;
}

/*
   Set CBREAK mode, 'fd'.
*/
void
set_Cbreak(int fd)
{
#ifdef BSD
    curr_tio[fd].sg_flags |= CBREAK;	/* CBREAK mode ON.	*/
    (void) ioctl(fd, TIOCSETP, &curr_tio[fd]);
#endif
#ifdef SYSV
    curr_tio[fd].c_lflag &= ~ICANON;	/* Canonical input OFF. */
    curr_tio[fd].c_cc[VMIN] = 1;
    curr_tio[fd].c_cc[VTIME] = 0;
    (void) ioctl(fd, TCSETA, &curr_tio[fd]);
#endif
#ifdef HAVE_TERMIOS_H
    curr_tio[fd].c_lflag &= ~ICANON;	/* Canonical input OFF. */
    curr_tio[fd].c_cc[VMIN] = 1;
    curr_tio[fd].c_cc[VTIME] = 0;
    (void)tcsetattr(fd, TCSANOW, &curr_tio[fd]);
#endif
    return;
}

/*
   Set raw mode, 'fd'.
*/
void
set_Raw(int fd)
{
#ifdef BSD
    curr_tio[fd].sg_flags |= RAW;		/* Raw mode ON.		*/
    (void) ioctl(fd, TIOCSETP, &curr_tio[fd]);
#endif
#ifdef SYSV
    curr_tio[fd].c_lflag &= ~ICANON;	/* Canonical input OFF. */
    curr_tio[fd].c_lflag &= ~ISIG;		/* Signals OFF.		*/
    curr_tio[fd].c_cc[VMIN] = 1;
    curr_tio[fd].c_cc[VTIME] = 0;
    (void) ioctl(fd, TCSETA, &curr_tio[fd]);
#endif
#ifdef HAVE_TERMIOS_H
    curr_tio[fd].c_lflag &= ~ICANON;	/* Canonical input OFF. */
    curr_tio[fd].c_lflag &= ~ISIG;		/* Signals OFF.		*/
    curr_tio[fd].c_cc[VMIN] = 1;
    curr_tio[fd].c_cc[VTIME] = 0;
    (void)tcsetattr(fd, TCSANOW, &curr_tio[fd]);
#endif
    return;
}

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
