/*
 *			F B S E R V . C
 *
 *  This code was developed by modifying the stand-alone version of fbserv to work
 *  within MGED.
 *
 *  Author -
 *	Robert Parker
 *
 *  Authors of the stand-alone fbserv -
 *	Phillip Dykstra
 *	Michael John Muuss
 *  
 *  Source -
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5068  USA
 *  
 *  Distribution Notice -
 *	Re-distribution of this software is restricted, as described in
 *	your "Statement of Terms and Conditions for the Release of
 *	The BRL-CAD Package" agreement.
 *
 *  Copyright Notice -
 *	This software is Copyright (C) 1995 by the United States Army
 *	in all countries except the USA.  All rights reserved.
 */
#ifndef lint
static const char RCSid[] = "@(#)$Header$ (ARL)";
#endif

#include "common.h"



#include <stdio.h>
#include <ctype.h>

#include "tcl.h"
#include "machine.h"
#include "bu.h"
#include "vmath.h"
#include "raytrace.h"
#include "./ged.h"
#include "./mged_dm.h"
#include "../libfb/pkgtypes.h"
#include "./fbserv.h"

#define NET_LONG_LEN	4	/* # bytes to network long */

void set_port();

#ifdef LOCAL_STATIC
#undef LOCAL_STATIC
#endif
#define LOCAL_STATIC static

LOCAL_STATIC void new_client();
LOCAL_STATIC void drop_client();
LOCAL_STATIC void new_client_handler();
LOCAL_STATIC void existing_client_handler();
LOCAL_STATIC void comm_error();
LOCAL_STATIC void setup_socket();


/*
 *			N E W _ C L I E N T
 */
LOCAL_STATIC void
new_client(pcp)
struct pkg_conn	*pcp;
{
    return;
}

/*
 *			D R O P _ C L I E N T
 */
LOCAL_STATIC void
drop_client(sub)
int sub;
{
    return;
}

/*
 *			S E T _ P O R T
 */
void
set_port()
{
    return;
}

/*
 * Accept any new client connections.
 */
LOCAL_STATIC void
new_client_handler(clientData, mask)
ClientData clientData;
int mask;
{
    return;
}

/*
 * Process arrivals from existing clients.
 */
LOCAL_STATIC void
existing_client_handler(clientData, mask)
ClientData clientData;
int mask;
{
    return;
}

LOCAL_STATIC void
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
LOCAL_STATIC void
comm_error(str)
char *str;
{
    return;
}

/*
 * This is where we go for message types we don't understand.
 */
void
pkgfoo(pcp, buf)
struct pkg_conn *pcp;
char *buf;
{
    return;
}

/******** Here's where the hooks lead *********/

void
rfbopen(pcp, buf)
struct pkg_conn *pcp;
char *buf;
{
    return;
}

void
rfbclose(pcp, buf)
struct pkg_conn *pcp;
char *buf;
{
    return;
}

void
rfbfree(pcp, buf)
struct pkg_conn *pcp;
char *buf;
{
    return;
}

void
rfbclear(pcp, buf)
struct pkg_conn *pcp;
char *buf;
{
    return;
}

void
rfbread(pcp, buf)
struct pkg_conn *pcp;
char *buf;
{
    return;
}

void
rfbwrite(pcp, buf)
struct pkg_conn *pcp;
char *buf;
{
    return;
}

/*
 *			R F B R E A D R E C T
 */
void
rfbreadrect(pcp, buf)
struct pkg_conn *pcp;
char *buf;
{
    return;
}

/*
 *			R F B W R I T E R E C T
 */
void
rfbwriterect(pcp, buf)
struct pkg_conn *pcp;
char *buf;
{
    return;
}

/*
 *			R F B B W R E A D R E C T
 */
void
rfbbwreadrect(pcp, buf)
struct pkg_conn *pcp;
char *buf;
{
    return;
}

/*
 *			R F B B W W R I T E R E C T
 */
void
rfbbwwriterect(pcp, buf)
struct pkg_conn *pcp;
char *buf;
{
    return;
}

void
rfbcursor(pcp, buf)
struct pkg_conn *pcp;
char *buf;
{
    return;
}

void
rfbgetcursor(pcp, buf)
struct pkg_conn *pcp;
char *buf;
{
    return;
}

void
rfbsetcursor(pcp, buf)
struct pkg_conn *pcp;
char		*buf;
{
    return;
}

/*OLD*/
void
rfbscursor(pcp, buf)
struct pkg_conn *pcp;
char *buf;
{
    return;
}

/*OLD*/
void
rfbwindow(pcp, buf)
struct pkg_conn *pcp;
char *buf;
{
    return;
}

/*OLD*/
void
rfbzoom(pcp, buf)
struct pkg_conn *pcp;
char *buf;
{
    return;
}

void
rfbview(pcp, buf)
struct pkg_conn *pcp;
char *buf;
{
    return;
}

void
rfbgetview(pcp, buf)
struct pkg_conn *pcp;
char *buf;
{
    return;
}

void
rfbrmap(pcp, buf)
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
rfbwmap(pcp, buf)
struct pkg_conn *pcp;
char *buf;
{
    return;
}

void
rfbflush(pcp, buf)
struct pkg_conn *pcp;
char *buf;
{
    return;
}

void
rfbpoll(pcp, buf)
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
rfbhelp(pcp, buf)
struct pkg_conn *pcp;
char *buf;
{
    return;
}
