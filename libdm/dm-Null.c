#include "conf.h"
#include "tcl.h"

#include <stdio.h>
#include <sys/time.h>
#include "machine.h"
#include "bu.h"
#include "dm.h"
#include "dm-Null.h"

static void	Nu_input();
void	Nu_void();
int	Nu_int0();
unsigned Nu_unsign();

struct dm dm_Null = {
  Nu_int0,
  Nu_int0, Nu_void,
  Nu_input,
  Nu_void, Nu_void,
  Nu_void, Nu_void,
  Nu_void,
  Nu_void, Nu_void,
  Nu_void,
  Nu_int0,
  Nu_unsign, Nu_unsign,
  Nu_void,
  Nu_void,
  Nu_void,
  Nu_void, Nu_void, Nu_int0, Nu_int0,
  0,			/* no displaylist */
  0,			/* no display to release */
  0.0,
  "nu", "Null Display",
  0,
  0,
  0,
  0
};

int Nu_int0() { return TCL_OK; }
void Nu_void() { ; }
unsigned Nu_unsign() { return TCL_OK; }

/*
 *
 * Implicit Return -
 *	If any files are ready for input, their bits will be set in 'input'.
 *	Otherwise, 'input' will be all zeros.
 */
static void
Nu_input( input, noblock )
fd_set		*input;
int		noblock;
{
	struct timeval	tv;
	int		width;
	int		cnt;

	if( !isatty(fileno(stdin)) )  return;	/* input awaits! */

#if defined(_SC_OPEN_MAX)
	if( (width = sysconf(_SC_OPEN_MAX)) <= 0 || width > 32)
#endif
		width = 32;

	if( noblock )  {
		/* 1/20th second */
		tv.tv_sec = 0;
		tv.tv_usec = 50000;
	} else {
		/* Wait a VERY long time for user to type something */
		tv.tv_sec = 9999999;
		tv.tv_usec = 0;
	}
	cnt = select( width, input, (fd_set *)0,  (fd_set *)0, &tv );
	if( cnt < 0 )  {
		perror("Nu_input/select()");
	}
}

