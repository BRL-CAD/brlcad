/*
 *				F B S E R V _ O B J . C
 *
 * A framebuffer server object contains the attributes and
 * methods for implementing an fbserv. This code was developed
 * in large part by modifying the stand-alone version of fbserv.
 * 
 * Source -
 *	SLAD CAD Team
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005
 *
 * Author -
 *	Robert G. Parker
 *
 * Authors of fbserv -
 *	Phillip Dykstra
 *	Michael John Muuss
 *
 */

#include "conf.h"

#include <stdio.h>
#include <ctype.h>

#include "tcl.h"
#include "machine.h"
#include "externs.h"		/* For malloc, getopt */
#include "bu.h"
#include "vmath.h"
#include "raytrace.h"
#include "dm.h"
#include "../libfb/pkgtypes.h"

int fbs_open();
int fbs_close();

HIDDEN void new_client();
HIDDEN void drop_client();
HIDDEN void new_client_handler();
HIDDEN void existing_client_handler();
HIDDEN void comm_error();
HIDDEN void setup_socket();

/*
 * Package Handlers.
 */
void	fbs_pkgfoo();	/* foobar message handler */
void	fbs_rfbopen(), fbs_rfbclose(), fbs_rfbclear(), fbs_rfbread(), fbs_rfbwrite();
void	fbs_rfbcursor(), fbs_rfbgetcursor();
void	fbs_rfbrmap(), fbs_rfbwmap();
void	fbs_rfbhelp();
void	fbs_rfbreadrect(), fbs_rfbwriterect();
void	fbs_rfbbwreadrect(), fbs_rfbbwwriterect();
void	fbs_rfbpoll(), fbs_rfbflush(), fbs_rfbfree();
void	fbs_rfbview(), fbs_rfbgetview();
void	fbs_rfbsetcursor();
/* Old Routines */
void	fbs_rfbscursor(), fbs_rfbwindow(), fbs_rfbzoom();

static struct pkg_switch pkg_switch[] = {
	{ MSG_FBOPEN,		fbs_rfbopen,	"Open Framebuffer" },
	{ MSG_FBCLOSE,		fbs_rfbclose,	"Close Framebuffer" },
	{ MSG_FBCLEAR,		fbs_rfbclear,	"Clear Framebuffer" },
	{ MSG_FBREAD,		fbs_rfbread,	"Read Pixels" },
	{ MSG_FBWRITE,		fbs_rfbwrite,	"Write Pixels" },
	{ MSG_FBWRITE + MSG_NORETURN,	fbs_rfbwrite,	"Asynch write" },
	{ MSG_FBCURSOR,		fbs_rfbcursor,	"Cursor" },
	{ MSG_FBGETCURSOR,	fbs_rfbgetcursor,	"Get Cursor" },	   /*NEW*/
	{ MSG_FBSCURSOR,	fbs_rfbscursor,	"Screen Cursor" }, /*OLD*/
	{ MSG_FBWINDOW,		fbs_rfbwindow,	"Window" },	   /*OLD*/
	{ MSG_FBZOOM,		fbs_rfbzoom,	"Zoom" },	   /*OLD*/
	{ MSG_FBVIEW,		fbs_rfbview,	"View" },	   /*NEW*/
	{ MSG_FBGETVIEW,	fbs_rfbgetview,	"Get View" },	   /*NEW*/
	{ MSG_FBRMAP,		fbs_rfbrmap,	"R Map" },
	{ MSG_FBWMAP,		fbs_rfbwmap,	"W Map" },
	{ MSG_FBHELP,		fbs_rfbhelp,	"Help Request" },
	{ MSG_ERROR,		fbs_pkgfoo,		"Error Message" },
	{ MSG_CLOSE,		fbs_pkgfoo,		"Close Connection" },
	{ MSG_FBREADRECT, 	fbs_rfbreadrect,	"Read Rectangle" },
	{ MSG_FBWRITERECT,	fbs_rfbwriterect,	"Write Rectangle" },
	{ MSG_FBWRITERECT + MSG_NORETURN, fbs_rfbwriterect,"Write Rectangle" },
	{ MSG_FBBWREADRECT, 	fbs_rfbbwreadrect,"Read BW Rectangle" },
	{ MSG_FBBWWRITERECT,	fbs_rfbbwwriterect,"Write BW Rectangle" },
	{ MSG_FBBWWRITERECT+MSG_NORETURN, fbs_rfbbwwriterect,"Write BW Rectangle" },
	{ MSG_FBFLUSH,		fbs_rfbflush,	"Flush Output" },
	{ MSG_FBFLUSH + MSG_NORETURN, fbs_rfbflush, "Flush Output" },
	{ MSG_FBFREE,		fbs_rfbfree,	"Free Resources" },
	{ MSG_FBPOLL,		fbs_rfbpoll,	"Handle Events" },
	{ MSG_FBSETCURSOR,	fbs_rfbsetcursor,	"Set Cursor Shape" },
	{ MSG_FBSETCURSOR + MSG_NORETURN, fbs_rfbsetcursor, "Set Cursor Shape" },
	{ 0,			NULL,		NULL }
};

HIDDEN FBIO *curr_fbp;		/* current framebuffer pointer */

int
fbs_open(interp, fbsp, port)
     Tcl_Interp *interp;
     struct fbserv_obj *fbsp;
     int port;
{
  return TCL_OK;
}

int
fbs_close(interp, fbsp)
     Tcl_Interp *interp;
     struct fbserv_obj *fbsp;
{
  return TCL_OK;
}

/*
 *			N E W _ C L I E N T
 */
HIDDEN void
new_client(fbsp, pcp)
     struct fbserv_obj *fbsp;
     struct pkg_conn	*pcp;
{ 
    return;
}

/*
 *			D R O P _ C L I E N T
 */
HIDDEN void
drop_client(fbsp, sub)
     struct fbserv_obj *fbsp;
     int sub;
{
	return;
}

/*
 * Accept any new client connections.
 */
HIDDEN void
new_client_handler(clientData, mask)
ClientData clientData;
int mask;
{
	return;
}

/*
 * Process arrivals from existing clients.
 */
HIDDEN void
existing_client_handler(clientData, mask)
ClientData clientData;
int mask;
{
	return;
}

HIDDEN void
setup_socket(fd)
int	fd;
{
	return;
}

/*
 *			C O M M _ E R R O R
 *
 *  Communication error.  An error occured on the PKG link.
 */
HIDDEN void
comm_error(str)
char *str;
{
	return;
}

/*
 * This is where we go for message types we don't understand.
 */
void
fbs_pkgfoo(pcp, buf)
struct pkg_conn *pcp;
char *buf;
{
  	return;
}

/******** Here's where the hooks lead *********/

void
fbs_rfbopen(pcp, buf)
struct pkg_conn *pcp;
char *buf;
{
	return;
}

void
fbs_rfbclose(pcp, buf)
struct pkg_conn *pcp;
char *buf;
{
	return;
}

void
fbs_rfbfree(pcp, buf)
struct pkg_conn *pcp;
char *buf;
{
	return;
}

void
fbs_rfbclear(pcp, buf)
struct pkg_conn *pcp;
char *buf;
{
	return;
}

void
fbs_rfbread(pcp, buf)
struct pkg_conn *pcp;
char *buf;
{
	return;
}

void
fbs_rfbwrite(pcp, buf)
struct pkg_conn *pcp;
char *buf;
{
	return;
}

/*
 *			R F B R E A D R E C T
 */
void
fbs_rfbreadrect(pcp, buf)
struct pkg_conn *pcp;
char *buf;
{
	return;
}

/*
 *			R F B W R I T E R E C T
 */
void
fbs_rfbwriterect(pcp, buf)
struct pkg_conn *pcp;
char *buf;
{
	return;
}

/*
 *			R F B B W R E A D R E C T
 */
void
fbs_rfbbwreadrect(pcp, buf)
struct pkg_conn *pcp;
char *buf;
{
	return;
}

/*
 *			R F B B W W R I T E R E C T
 */
void
fbs_rfbbwwriterect(pcp, buf)
struct pkg_conn *pcp;
char *buf;
{
	return;
}

void
fbs_rfbcursor(pcp, buf)
struct pkg_conn *pcp;
char *buf;
{
	return;
}

void
fbs_rfbgetcursor(pcp, buf)
struct pkg_conn *pcp;
char *buf;
{
	return;
}

void
fbs_rfbsetcursor(pcp, buf)
struct pkg_conn *pcp;
char		*buf;
{
	return;
}

/*OLD*/
void
fbs_rfbscursor(pcp, buf)
struct pkg_conn *pcp;
char *buf;
{
	return;
}

/*OLD*/
void
fbs_rfbwindow(pcp, buf)
struct pkg_conn *pcp;
char *buf;
{
	return;
}

/*OLD*/
void
fbs_rfbzoom(pcp, buf)
struct pkg_conn *pcp;
char *buf;
{
	return;
}

void
fbs_rfbview(pcp, buf)
struct pkg_conn *pcp;
char *buf;
{
	return;
}

void
fbs_rfbgetview(pcp, buf)
struct pkg_conn *pcp;
char *buf;
{
	return;
}

void
fbs_rfbrmap(pcp, buf)
struct pkg_conn *pcp;
char *buf;
{
	return;
}

/*
 *			R F B W M A P
 *
 *  Accept a color map sent by the client, and write it to the framebuffer.
 *  Network format is to send each entry as a network (IBM) order 2-byte
 *  short, 256 red shorts, followed by 256 green and 256 blue, for a total
 *  of 3*256*2 bytes.
 */
void
fbs_rfbwmap(pcp, buf)
struct pkg_conn *pcp;
char *buf;
{
	return;
}

void
fbs_rfbflush(pcp, buf)
struct pkg_conn *pcp;
char *buf;
{
	return;
}

void
fbs_rfbpoll(pcp, buf)
struct pkg_conn *pcp;
char *buf;
{
	return;
}

/*
 *  At one time at least we couldn't send a zero length PKG
 *  message back and forth, so we receive a dummy long here.
 */
void
fbs_rfbhelp(pcp, buf)
struct pkg_conn *pcp;
char *buf;
{
	return;
}
