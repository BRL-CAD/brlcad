/*
 *			R T S Y N C . C
 *
 *  Main program to tightly synchronize a network distributed array of
 *  processors.  Interfaces with MGED via the VRMGR protocol,
 *  and with RTNODE.
 *
 *  The heart of the "real time ray-tracing" project.
 *
 *  Author -
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
static char RCSid[] = "@(#)$Header$ (ARL)";
#endif

#include "conf.h"

#include <stdio.h>
#include <ctype.h>
#include <signal.h>
#include <errno.h>
#include <netdb.h>
#include <math.h>
#ifdef USE_STRING_H
# include <string.h>
#else
# include <strings.h>
#endif

#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>

/*
 *  The situation with sys/time.h and time.h is crazy.
 *  We need sys/time.h for struct timeval,
 *  and time.h for struct tm.
 *
 *  on BSD (and SGI 4D), sys/time.h includes time.h,
 *  on the XMP (UNICOS 3 & 4), time.h includes sys/time.h,
 *  on the Cray-2, there is no automatic including.
 *
 *  Note that on many SYSV machines, the Cakefile has to set BSD
 */
#if BSD && !SYSV
#  include <sys/time.h>		/* includes <time.h> */
#else
#  if CRAY1 && !__STDC__
#	include <time.h>	/* includes <sys/time.h> */
#  else
#	include <sys/time.h>
#	include <time.h>
#  endif
#endif

#include "machine.h"
#include "vmath.h"
#include "rtstring.h"
#include "rtlist.h"
#include "raytrace.h"
#include "pkg.h"
#include "fb.h"
#include "tcl.h"
#include "tk.h"
#include "externs.h"

#include "./ihost.h"

#define pkg_send_vls(type,vlsp,pkg)	pkg_send( (type), bu_vls_addr((vlsp)), bu_vls_strlen((vlsp))+1, (pkg) )

extern int	pkg_permport;	/* libpkg/pkg_permserver() listen port */

/*
 *  Package Handlers for the VRMGR protocol
 */
#define VRMSG_ROLE	1	/* from MGED: Identify role of machine */
#define VRMSG_CMD	2	/* to MGED: Command to process */
#define VRMSG_EVENT	3	/* from MGED: device event */
#define VRMSG_POV	4	/* from MGED: point of view info */
#define VRMSG_VLIST	5	/* transfer binary vlist block */

void	ph_default();	/* foobar message handler */
void	vrmgr_ph_role();
void	vrmgr_ph_event();
void	vrmgr_ph_pov();
static struct pkg_switch vrmgr_pkgswitch[] = {
	{ VRMSG_ROLE,		vrmgr_ph_role,	"Declare role" },
	{ VRMSG_CMD,		ph_default,	"to MGED:  command" },
	{ VRMSG_EVENT,		vrmgr_ph_event,	"MGED device event" },
	{ VRMSG_POV,		vrmgr_ph_pov,	"MGED point of view" },
	{ VRMSG_VLIST,		ph_default,	"binary vlist block" },
	{ 0,			0,		(char *)0 }
};

int			vrmgr_listen_fd;	/* for new connections */
struct pkg_conn		*vrmgr_pc;		/* connection to VRMGR */
struct ihost		*vrmgr_ihost;		/* host of vrmgr_pc */
char			*pending_pov;		/* pending new POV */
int			print_on = 1;

/*
 *  Package handlers for the RTSYNC protocol.
 *  Numbered differently, to prevent confusion with other PKG protocols.
 */
#define RTSYNCMSG_PRINT	 999	/* StoM:  Diagnostic message */
#define RTSYNCMSG_ALIVE	1001	/* StoM:  protocol version, # of processors */
#define RTSYNCMSG_OPENFB 1002	/* both:  width height framebuffer */
#define RTSYNCMSG_DIRBUILD 1003	/* both:  database */
#define RTSYNCMSG_GETTREES 1004	/* both:  treetop(s) */
#define RTSYNCMSG_CMD	1006	/* MtoS:  Any Tcl command */
#define RTSYNCMSG_POV	1007	/* MtoS:  pov, min_res, start&end lines */
#define RTSYNCMSG_HALT	1008	/* MtoS:  abandon frame & xmit, NOW */
#define RTSYNCMSG_DONE	1009	/* StoM:  halt=0/1, res, elapsed, etc... */

void	rtsync_ph_alive();
void	rtsync_ph_openfb();
void	rtsync_ph_dirbuild();
void	rtsync_ph_gettrees();
void	rtsync_ph_done();
void	ph_print();
static struct pkg_switch rtsync_pkgswitch[] = {
	{ RTSYNCMSG_DONE,	rtsync_ph_done, "RTNODE assignment done" },
	{ RTSYNCMSG_ALIVE,	rtsync_ph_alive, "RTNODE is alive" },
	{ RTSYNCMSG_OPENFB,	rtsync_ph_openfb, "RTNODE open(ed) fb" },
	{ RTSYNCMSG_DIRBUILD,	rtsync_ph_dirbuild, "RTNODE dirbuilt/built" },
	{ RTSYNCMSG_GETTREES,	rtsync_ph_gettrees, "RTNODE prep(ed) db" },
	{ RTSYNCMSG_POV,	ph_default,	"POV" },
	{ RTSYNCMSG_HALT,	ph_default,	"HALT" },
	{ RTSYNCMSG_CMD,	ph_default,	"CMD" },
	{ RTSYNCMSG_PRINT,	ph_print,	"Log Message" },
	{ 0,			0,		(char *)0 }
};

int			rtsync_listen_fd;	/* for new connections */

#define STATE_NEWBORN	0		/* No packages received yet */
#define STATE_ALIVE	1		/* ALIVE pkg recv'd */
#define STATE_OPENFB	2		/* OPENFB ack pkg received */
#define STATE_DIRBUILT	3		/* DIRBUILD ack pkg received */
#define STATE_PREPPED	4		/* GETTREES ack pkg received */
CONST char *states[] = {
	"NEWBORN",
	"ALIVE",
	"OPENFB",
	"DIRBUILT",
	"PREPPED",
	"n+1"
};

/*
 *			R T N O D E
 *
 *  One per compute server host.
 */
struct rtnode {
	int		fd;
	struct pkg_conn	*pkg;
	struct ihost	*host;
	int		state;
	int		ncpus;		/* Ready when > 0, for now */
	int		busy;
	Tcl_File	tcl_file;	/* Tcl's name for this fd */
};
#define MAX_NODES	32
struct rtnode	rtnodes[MAX_NODES];

static fd_set	select_list;			/* master copy */
static int	max_fd;

static	FBIO	*fbp;
static	char	*framebuffer;
static	int	width = 0;		/* use default size */
static	int	height = 0;
int		debug = 0;

/* Variables linked to Tcl/Tk */

Tcl_Interp	*interp = NULL;
Tk_Window	tkwin;

CONST char	*database;
struct bu_vls	treetops;

struct timeval	time_start;

BU_EXTERN(void dispatcher, (ClientData clientData));
void	new_rtnode();
void	drop_rtnode();
void	setup_socket();
char	*stamp();


static char usage[] = "\
Usage:  rtsync [-d#] [-h] [-S squaresize] [-W width] [-N height] [-F framebuffer]\n\
	model.g treetop(s)\n\
";

/*
 *			G E T _ A R G S
 */
get_args( argc, argv )
register char **argv;
{
	register int c;

	while ( (c = getopt( argc, argv, "d:hF:s:w:n:S:W:N:" )) != EOF )  {
		switch( c )  {
		case 'd':
			debug = atoi(optarg);
			break;
		case 'h':
			/* high-res */
			height = width = 1024;
			break;
		case 'F':
			framebuffer = optarg;
			break;
		case 's':
		case 'S':
			height = width = atoi(optarg);
			break;
		case 'w':
		case 'W':
			width = atoi(optarg);
			break;
		case 'n':
		case 'N':
			height = atoi(optarg);
			break;

		default:		/* '?' */
			return(0);
		}
	}

	return(1);		/* OK */
}

/*
 *			N O D E _ S E N D
 *
 *  Arrange to send a string to all rtnode processes,
 *  for them to run as a TCL command.
 */
int
node_send( clientData, interp, argc, argv )
ClientData clientData;
Tcl_Interp *interp;
int argc;
char **argv;
{
	struct bu_vls	cmd;
	int		i;

	if( argc < 2 )  {
		Tcl_AppendResult(interp, "Usage: node_send command(s)\n", NULL);
		return TCL_ERROR;
	}

	bu_vls_init(&cmd);
	bu_vls_from_argv( &cmd, argc-1, argv+1 );

	bu_log("node_send: %s\n", bu_vls_addr(&cmd));

	for( i = MAX_NODES-1; i >= 0; i-- )  {
		if( rtnodes[i].fd <= 0 )  continue;
		if( rtnodes[i].ncpus <= 0 )  continue;
		if( pkg_send_vls( RTSYNCMSG_CMD, &cmd, rtnodes[i].pkg ) < 0 )  {
			drop_rtnode(i);
			continue;
		}
	}
	bu_vls_free(&cmd);
	return TCL_OK;
}

/*
 *			V R M G R _ S E N D
 *
 *  Arrange to send a string to the VRMGR process
 *  for it to run as a TCL command.
 */
int
vrmgr_send( clientData, interp, argc, argv )
ClientData clientData;
Tcl_Interp *interp;
int argc;
char **argv;
{
	struct bu_vls	cmd;
	int		i;

	if( argc < 2 )  {
		Tcl_AppendResult(interp, "Usage: vrmgr_send command(s)\n", NULL);
		return TCL_ERROR;
	}

	if( !vrmgr_pc )  {
		Tcl_AppendResult(interp, "vrmgr is not presently connected\n", NULL);
		return TCL_ERROR;
	}

	bu_vls_init(&cmd);
	bu_vls_from_argv( &cmd, argc-1, argv+1 );
	bu_vls_putc( &cmd, '\n');

	if( pkg_send_vls( VRMSG_CMD, &cmd, vrmgr_pc ) < 0 )  {
		pkg_close(vrmgr_pc);
		vrmgr_pc = 0;
		Tcl_AppendResult(interp, "Error writing to vrmgr\n", NULL);
		bu_vls_free(&cmd);
		return TCL_ERROR;
	}
	bu_vls_free(&cmd);
	return TCL_OK;
}

/*
 *			R E P R E P
 *
 *  Make all the nodes re-prep.
 */
reprep( clientData, interp, argc, argv )
ClientData clientData;
Tcl_Interp *interp;
int argc;
char **argv;
{
	int		i;
	struct bu_vls	cmd;

	if( argc != 1 )  {
		Tcl_AppendResult(interp, "Usage: reprep\n", NULL);
		return TCL_ERROR;
	}

	bu_vls_init(&cmd);
	bu_vls_strcpy( &cmd, "rt_clean");

	bu_log("reprep\n");

	for( i = MAX_NODES-1; i >= 0; i-- )  {
		if( rtnodes[i].fd <= 0 )  continue;
		if( rtnodes[i].ncpus <= 0 )  continue;
		if( rtnodes[i].state != STATE_PREPPED )  {
			Tcl_AppendResult(interp, "reprep: host ",
				rtnodes[i].host->ht_name,
				" is in state ",
				states[rtnodes[i].state],
				"\n", NULL);
			drop_rtnode(i);
			continue;
		}
		/* change back to STATE_DIRBUILT and send GETTREES commands */
		if( change_state( i, STATE_PREPPED, STATE_DIRBUILT ) < 0 )  {
			drop_rtnode(i);
			continue;
		}

		/* Receipt of GETTREES will automatically "clean" previous model first */
		if( pkg_send_vls( RTSYNCMSG_GETTREES, &treetops,
		     rtnodes[i].pkg ) < 0 )  {
			drop_rtnode(i);
		     	continue;
		}
	}
	return TCL_OK;
}

/**********************************************************************/

/*
 *			S T D I N _ E V E N T _ H A N D L E R
 *
 *  Read Tcl commands from a "tty" file descriptor.
 *  Called from the TCL/Tk event handler
 */
void
stdin_event_handler(clientData, mask)
ClientData	clientData;	/* fd */
int		mask;
{
	char	buf[511+1];
	int	fd;
	int	got;

	fd = (int)clientData;

	got = read(fd, buf, 511);
	if( got >= 0 && got < 511 )  buf[got] = '\0';
	else	buf[0] = '\0';

	if( got <= 0 )  {
		bu_log("EOF on stdin\n");
		Tcl_DeleteFileHandler( Tcl_GetFile(clientData, TCL_UNIX_FD) );
		return;
	}

	/* Do something here.  Eventually, feed off to Tcl interp. */
	if( Tcl_Eval( interp, buf ) == TCL_OK )  {
		bu_log("%s\n", interp->result);

		Tcl_DoWhenIdle( dispatcher, (ClientData)0 );
	} else {
		bu_log("ERROR %s\n", interp->result);
	}
}

/*
 *			P K G _ E V E N T _ H A N D L E R
 *
 *  Generic event handler to read from a LIBPKG connection.
 *  Called from the TCL/Tk event handler
 */
void
pkg_event_handler(clientData, mask)
ClientData	clientData;	/* *pc */
int		mask;
{
	struct pkg_conn	*pc;
	int	val;

	pc = (struct pkg_conn *)clientData;

	val = pkg_suckin(pc);
	if( val < 0 ) {
		bu_log("pkg_suckin() error\n");
	} else if( val == 0 )  {
		bu_log("EOF on pkg connection\n");
	}
	if( val <= 0 )  {
		Tcl_DeleteFileHandler(
			Tcl_GetFile((ClientData)pc->pkc_fd, TCL_UNIX_FD) );
		pkg_close(pc);
		return;
	}
	if( pkg_process( pc ) < 0 )
		bu_log("pc:  pkg_process error encountered\n");

	Tcl_DoWhenIdle( dispatcher, (ClientData)0 );
}

/*
 *			V R M G R _ E V E N T _ H A N D L E R
 *
 *  Event handler to read VRMGR commands from a LIBPKG connection.
 *  Called from the TCL/Tk event handler.
 */
void
vrmgr_event_handler(clientData, mask)
ClientData	clientData;	/* *pc */
int		mask;
{
	struct pkg_conn	*pc;
	int	val;

	pc = (struct pkg_conn *)clientData;

	val = pkg_suckin(pc);
	if( val < 0 ) {
		bu_log("vrmgr: pkg_suckin() error\n");
	} else if( val == 0 )  {
		bu_log("vrmgr: EOF on pkg connection\n");
	}
	if( val <= 0 )  {
		Tcl_DeleteFileHandler(
			Tcl_GetFile((ClientData)pc->pkc_fd, TCL_UNIX_FD) );
		pkg_close(pc);
		vrmgr_pc = 0;
		vrmgr_ihost = IHOST_NULL;
		return;
	}
	if( pkg_process( pc ) < 0 )
		bu_log("vrmgr:  pkg_process error encountered\n");

	Tcl_DoWhenIdle( dispatcher, (ClientData)0 );
}

/*
 *			R T N O D E _ E V E N T _ H A N D L E R
 *
 *  Read from a LIBPKG connection associated with an rtnode.
 *  Called from the TCL/Tk event handler
 */
void
rtnode_event_handler(clientData, mask)
ClientData	clientData;	/* subscript to rtnodes[] */
int		mask;
{
	int	i;

	i = (int)clientData;
	if( rtnodes[i].fd == 0 )  {
		bu_log("rtnode_event_handler(%d) no fd?\n", i);
		return;
	}
	if( pkg_process( rtnodes[i].pkg ) < 0 ) {
		bu_log("pkg_process error encountered (1)\n");
	}
	if( pkg_suckin( rtnodes[i].pkg ) <= 0 )  {
		/* Probably EOF */
		drop_rtnode( i );
		return;
	}
	if( pkg_process( rtnodes[i].pkg ) < 0 ) {
		bu_log("pkg_process error encountered (2)\n");
	}

	Tcl_DoWhenIdle( dispatcher, (ClientData)0 );
}

/*
 *			F B _ E V E N T _ H A N D L E R
 */
void
fb_event_handler(clientData, mask)
ClientData	clientData;	/* FBIO * */
int		mask;
{
	FBIO	*fbp;
bu_log("fb_event_handler() called\n");

	fbp = (FBIO *)clientData;
	fb_poll(fbp);

	Tcl_DoWhenIdle( dispatcher, (ClientData)0 );
}

/*
 *			V R M G R _ L I S T E N _ H A N D L E R
 */
void
vrmgr_listen_handler(clientData, mask)
ClientData	clientData;	/* fd */
int		mask;
{

	/* Accept any new VRMGR connections.  Only one at a time is permitted. */
	if( vrmgr_pc )  {
		bu_log("New VRMGR connection received with one still active, dropping old one.\n");
		Tcl_DeleteFileHandler(
			Tcl_GetFile((ClientData)vrmgr_pc->pkc_fd, TCL_UNIX_FD) );
		pkg_close( vrmgr_pc );
		vrmgr_pc = 0;
		vrmgr_ihost = IHOST_NULL;
	}
	vrmgr_pc = pkg_getclient( vrmgr_listen_fd, vrmgr_pkgswitch, bu_log, 0 );
	vrmgr_ihost = host_lookup_of_fd(vrmgr_pc->pkc_fd);
	if( vrmgr_ihost == IHOST_NULL )  {
		bu_log("Unable to get hostname of VRMGR, abandoning it\n");
		pkg_close( vrmgr_pc );
		vrmgr_pc = 0;
	} else {
		bu_log("%s VRMGR link with %s, fd=%d\n",
			stamp(),
			vrmgr_ihost->ht_name, vrmgr_pc->pkc_fd);
		Tcl_CreateFileHandler(
			Tcl_GetFile((ClientData)vrmgr_pc->pkc_fd, TCL_UNIX_FD),
			TCL_READABLE|TCL_EXCEPTION, vrmgr_event_handler,
			(ClientData)vrmgr_pc );
		FD_SET(vrmgr_pc->pkc_fd, &select_list);
		if( vrmgr_pc->pkc_fd > max_fd )  max_fd = vrmgr_pc->pkc_fd;
		setup_socket( vrmgr_pc->pkc_fd );
	}

	Tcl_DoWhenIdle( dispatcher, (ClientData)0 );
}

/*
 *			R T S Y N C _ L I S T E N _ H A N D L E R
 */
void
rtsync_listen_handler(clientData, mask)
ClientData	clientData;	/* fd */
int		mask;
{
	new_rtnode( pkg_getclient( (int)clientData,
		rtsync_pkgswitch, bu_log, 0 ) );

	Tcl_DoWhenIdle( dispatcher, (ClientData)0 );
}

/**********************************************************************/

/*
 *			M A I N
 */
main(argc, argv)
int	argc;
char	*argv[];
{
	width = height = 256;	/* keep it modest, by default */

	if ( !get_args( argc, argv ) ) {
		(void)fputs(usage, stderr);
		exit( 1 );
	}

	if( optind >= argc )  {
		fprintf(stderr,"rtsync:  MGED database not specified\n");
		(void)fputs(usage, stderr);
		exit(1);
	}
	database = argv[optind++];
	if( optind >= argc )  {
		fprintf(stderr,"rtsync:  tree top(s) not specified\n");
		(void)fputs(usage, stderr);
		exit(1);
	}

	bu_vls_init( &treetops );
	bu_vls_from_argv( &treetops, argc - optind, argv+optind );
	bu_log("DB: %s %s\n", database, bu_vls_addr(&treetops) );

	BU_LIST_INIT( &rt_g.rtg_vlfree );

	/* Initialize the Tcl interpreter */
	interp = Tcl_CreateInterp();

	/* This runs the init.tcl script */
	if( Tcl_Init(interp) == TCL_ERROR )
		bu_log("Tcl_Init error %s\n", interp->result);
	bn_tcl_setup(interp);
	rt_tcl_setup(interp);
	Tcl_SetVar(interp, "cpu_count", "0", TCL_GLOBAL_ONLY );
	/* Don't allow unknown commands to be fed to the shell */
	Tcl_SetVar( interp, "tcl_interactive", "0", TCL_GLOBAL_ONLY );

	/* This runs the tk.tcl script */
	if(Tk_Init(interp) == TCL_ERROR)  bu_bomb("Try setting TK_LIBRARY environment variable\n");
	if((tkwin = Tk_MainWindow(interp)) == NULL)
		bu_bomb("Tk_MainWindow failed\n");
	/* Don't insist on a click to position the window, just make it */
	(void)Tcl_Eval( interp, "wm geometry . =+1+1");

	/* Let main window pop up before running script */
	while( Tcl_DoOneEvent(TCL_DONT_WAIT) != 0 ) ;
# if 0
	(void)Tcl_Eval( interp, "wm withdraw .");
# endif
	if( Tcl_EvalFile( interp, "/m/cad/remrt/rtsync.tcl" ) != TCL_OK )  {
		bu_log("%s\n",
			Tcl_GetVar(interp,"errorInfo", TCL_GLOBAL_ONLY) );
		bu_log("\n*** Startup Script Aborted ***\n\n");
	}

	/* Incorporate built-in commands */
	(void)Tcl_CreateCommand(interp, "vrmgr_send", vrmgr_send,
		(ClientData)NULL, (Tcl_CmdDeleteProc *)NULL);
	(void)Tcl_CreateCommand(interp, "node_send", node_send,
		(ClientData)NULL, (Tcl_CmdDeleteProc *)NULL);
	(void)Tcl_CreateCommand(interp, "reprep", reprep,
		(ClientData)NULL, (Tcl_CmdDeleteProc *)NULL);

	/* Accept commands on stdin */
	if( isatty(fileno(stdin)) )  {
		Tcl_CreateFileHandler(
			Tcl_GetFile((ClientData)fileno(stdin), TCL_UNIX_FD),
			TCL_READABLE|TCL_EXCEPTION, stdin_event_handler,
			(ClientData)fileno(stdin) );
		bu_log("rtsync accepting commands on stdin\n");
	}


	/* Connect up with framebuffer, for control & size purposes */
	if( !framebuffer )  framebuffer = getenv("FB_FILE");
	if( !framebuffer )  rt_bomb("rtsync:  No -F and $FB_FILE not set\n");
	if( (fbp = fb_open(framebuffer, width, height)) == FBIO_NULL )
		exit(1);

	if( width <= 0 || fb_getwidth(fbp) < width )
		width = fb_getwidth(fbp);
	if( height <= 0 || fb_getheight(fbp) < height )
		height = fb_getheight(fbp);

	/* Listen for VRMGR Master PKG connections from MGED */
	if( (vrmgr_listen_fd = pkg_permserver("5555", "tcp", 8, bu_log)) < 0 )  {
		bu_log("Unable to listen on 5555\n");
		exit(1);
	}
	Tcl_CreateFileHandler(
		Tcl_GetFile((ClientData)vrmgr_listen_fd, TCL_UNIX_FD),
		TCL_READABLE|TCL_EXCEPTION, vrmgr_listen_handler,
		(ClientData)vrmgr_listen_fd);

	/* Listen for our RTSYNC client's PKG connections */
	if( (rtsync_listen_fd = pkg_permserver("rtsrv", "tcp", 8, bu_log)) < 0 )  {
		int	i;
		char	num[8];
		/* Do it by the numbers */
		for(i=0; i<10; i++ )  {
			sprintf( num, "%d", 4446+i );
			if( (rtsync_listen_fd = pkg_permserver(num, "tcp", 8, bu_log)) < 0 )
				continue;
			break;
		}
		if( i >= 10 )  {
			bu_log("Unable to find a port to listen on\n");
			exit(1);
		}
	}
	Tcl_CreateFileHandler(
		Tcl_GetFile((ClientData)rtsync_listen_fd, TCL_UNIX_FD),
		TCL_READABLE|TCL_EXCEPTION, rtsync_listen_handler,
		(ClientData)rtsync_listen_fd);
	/* Now, pkg_permport has tcp port number */
	bu_log("%s RTSYNC listening on %s port %d\n",
		stamp(),
		get_our_hostname(),
		pkg_permport);

	(void)signal( SIGPIPE, SIG_IGN );

	if( fbp && fbp->if_selfd > 0 )  {
		Tcl_CreateFileHandler(
			Tcl_GetFile((ClientData)fbp->if_selfd, TCL_UNIX_FD),
			TCL_READABLE|TCL_EXCEPTION, fb_event_handler,
			(ClientData)fbp );
	}

	Tcl_DoWhenIdle( dispatcher, (ClientData)0 );

	for(;;)  {
		Tcl_DoOneEvent(0);
	}
}

/*
 *			D I S P A T C H E R
 *
 *  Where all the work gets sent out.
 *  
 *  This is only called once and then evaporates;
 *  the event-handlers are responsible for queueing up more when
 *  something might have happened, with:
 *	Tcl_DoWhenIdle( dispatcher, (ClientData)0 );
 */
void
dispatcher(clientData)
ClientData clientData;
{
	register int	i;
	int		cpu_count = 0;
	int		start_line;
	int		lowest_index = 0;
	char		buf[32];

	if( !pending_pov )  return;

	cpu_count = 0;
	for( i = MAX_NODES-1; i >= 0; i-- )  {
		if( rtnodes[i].fd <= 0 )  continue;
		if( rtnodes[i].state != STATE_PREPPED )  continue;
		if( rtnodes[i].ncpus <= 0 )  continue;
		if( rtnodes[i].busy )  return;	/* Still working on last one */
		cpu_count += rtnodes[i].ncpus;
		lowest_index = i;
	}
	bu_log("%s dispatcher() has %d cpus\n", stamp(), cpu_count);
	sprintf(buf, "%d", cpu_count);
	Tcl_SetVar(interp, "cpu_count", buf, TCL_GLOBAL_ONLY );
	if( cpu_count <= 0 )  return;

	/* Record starting time for this frame */
	(void)gettimeofday( &time_start, (struct timezone *)NULL );

	/* Have some CPUS! Parcel up 'height' scanlines. */
	start_line = 0;
	for( i = MAX_NODES-1; i >= 0; i-- )  {
		int	end_line;
		int	count;
		struct bu_vls	msg;

		if( start_line >= height )  break;
		if( rtnodes[i].fd <= 0 )  continue;
		if( rtnodes[i].state != STATE_PREPPED )  continue;

		if( i <= lowest_index )  {
			end_line = height-1;
		} else {
			count = (int)ceil( ((double)rtnodes[i].ncpus) /
				cpu_count * height );
			end_line = start_line + count;
			if( end_line > height-1 )  end_line = height-1;
		}

		bu_vls_init( &msg );
		bu_vls_printf( &msg, "%d %d %d %s\n",
			256,
			start_line, end_line,
			pending_pov+4 );
		if( pkg_send_vls( RTSYNCMSG_POV, &msg, rtnodes[i].pkg ) < 0 )  {
			drop_rtnode( i );
			bu_vls_free(&msg);
			continue;	/* Don't update start_line */
		}
		bu_log("%s sending %d..%d to %s\n", stamp(), start_line, end_line, rtnodes[i].host->ht_name);

		bu_vls_free(&msg);
		start_line = end_line + 1;
		rtnodes[i].busy = 1;
	}

	free( pending_pov );
	pending_pov = NULL;
}

/*
 *			N E W _ R T N O D E
 */
void
new_rtnode(pcp)
struct pkg_conn	*pcp;
{
	register int	i;
	struct ihost	*host;

	if( pcp == PKC_ERROR )
		return;

	if( !(host = host_lookup_of_fd(pcp->pkc_fd)) )  {
		bu_log("%s Unable to get host name of new connection, dropping\n", stamp() );
		pkg_close(pcp);
		return;
	}
	bu_log("%s Connection from %s\n", stamp(), host->ht_name);

	for( i = MAX_NODES-1; i >= 0; i-- )  {
		if( rtnodes[i].fd != 0 )  continue;
		/* Found an available slot */
		bzero( (char *)&rtnodes[i], sizeof(rtnodes[0]) );
		rtnodes[i].state = STATE_NEWBORN;
		rtnodes[i].pkg = pcp;
		rtnodes[i].fd = pcp->pkc_fd;
		FD_SET(pcp->pkc_fd, &select_list);
		if( pcp->pkc_fd > max_fd )  max_fd = pcp->pkc_fd;
		setup_socket( pcp->pkc_fd );
		rtnodes[i].host = host;
		rtnodes[i].tcl_file = 
			Tcl_GetFile((ClientData)pcp->pkc_fd, TCL_UNIX_FD);
		Tcl_CreateFileHandler( rtnodes[i].tcl_file,
			TCL_READABLE|TCL_EXCEPTION, rtnode_event_handler,
			(ClientData)i );
		return;
	}
	bu_log("rtsync: too many rtnode clients.  My cup runneth over!\n");
	pkg_close(pcp);
}

/*
 *			D R O P _ R T N O D E
 */
void
drop_rtnode( sub )
int	sub;
{
	bu_log("%s Dropping %s\n", stamp(), rtnodes[sub].host->ht_name);

	if( rtnodes[sub].pkg != PKC_NULL )  {
		pkg_close( rtnodes[sub].pkg );
		rtnodes[sub].pkg = PKC_NULL;
	}
	if( rtnodes[sub].fd != 0 )  {
		FD_CLR( rtnodes[sub].fd, &select_list );
		close( rtnodes[sub].fd );
		rtnodes[sub].fd = 0;
	}
	Tcl_DeleteFileHandler( rtnodes[sub].tcl_file );
}

/*
 *			S E T U P _ S O C K E T
 */
void
setup_socket(fd)
int	fd;
{
	int	on = 1;

#if defined(SO_KEEPALIVE)
	if( setsockopt( fd, SOL_SOCKET, SO_KEEPALIVE, (char *)&on, sizeof(on)) < 0 ) {
		perror( "setsockopt (SO_KEEPALIVE)");
	}
#endif
#if defined(SO_RCVBUF)
	/* try to set our buffers up larger */
	{
		int	m, n;
		int	val;
		int	size;

		for( size = 256; size > 16; size /= 2 )  {
			val = size * 1024;
			m = setsockopt( fd, SOL_SOCKET, SO_RCVBUF,
				(char *)&val, sizeof(val) );
			val = size * 1024;
			n = setsockopt( fd, SOL_SOCKET, SO_SNDBUF,
				(char *)&val, sizeof(val) );
			if( m >= 0 && n >= 0 )  break;
		}
		if( m < 0 || n < 0 )  perror("rtsync setsockopt()");
	}
#endif
}

/*
 *			C H A N G E _ S T A T E
 */
int
change_state( i, old, new )
int	i;
int	old;
int	new;
{
	bu_log("%s %s %s --> %s\n", stamp(),
		rtnodes[i].host->ht_name,
		states[rtnodes[i].state], states[new] );
	if( rtnodes[i].state != old )  {
		bu_log("%s was in state %s, should have been %s, dropping\n",
			rtnodes[i].host->ht_name,
			states[rtnodes[i].state], states[old] );
		drop_rtnode(i);
		return -1;
	}
	rtnodes[i].state = new;
	return 0;
}

/*
 *			V R M G R _ P H _ R O L E
 *
 *  The ROLE package should be the first thing that MGED says to
 *  the VRMGR (i.e. to us).
 *  There is no need to strictly check the protocol;
 *  if we get a ROLE package, it better be of type "master".
 *  If no ROLE package is sent, no big deal.
 */
void
vrmgr_ph_role(pc, buf)
register struct pkg_conn *pc;
char			*buf;
{
#define MAXARGS 32
	char		*argv[MAXARGS];
	int		argc;

	bu_log("%s VRMGR host %s, role %s\n",
		stamp(),
		vrmgr_ihost->ht_name, buf);

	argc = rt_split_cmd( argv, MAXARGS, buf );
	if( argc < 1 )  {
		bu_log("bad role command\n");
	}

	if( strcmp( argv[0], "master" ) != 0 )  {
		bu_log("ERROR %s: bad role '%s', dropping vrmgr\n",
			vrmgr_ihost->ht_name, buf );
		FD_CLR(vrmgr_pc->pkc_fd, &select_list);
		pkg_close( vrmgr_pc );
		vrmgr_pc = 0;
		vrmgr_ihost = 0;
	}
	if(buf) (void)free(buf);
}

/*
 *			V R M G R _ P H _ E V E N T
 *
 *  These are from slave MGEDs for relay to the master MGED.
 *  We don't expect any of these.
 */
void
vrmgr_ph_event(pc, buf)
register struct pkg_conn *pc;
char			*buf;
{
	register struct servers	*sp;

	bu_log("%s VRMGR unexpectely got event '%s'", stamp(), buf );
	if( buf )  free(buf);
}

/*
 *			V R M G R _ P H _ P O V
 *
 *  Accept a new point-of-view from the MGED master.
 *  If there is an existing POV which has not yet been rendered,
 *  drop it, and use the new one instead, to catch up.
 *
 *  We retain the buffer from LIBPKG until the POV is processed.
 */
void
vrmgr_ph_pov(pc, buf)
register struct pkg_conn *pc;
char			*buf;
{
	if( debug )  bu_log("%s %s\n", stamp(), buf);
	if( pending_pov )  free(pending_pov);
	pending_pov = buf;
}

/*
 *			P H _ D E F A U L T
 */
void
ph_default(pc, buf)
register struct pkg_conn *pc;
char *buf;
{
	register int i;

	for( i=0; pc->pkc_switch[i].pks_handler != NULL; i++ )  {
		if( pc->pkc_switch[i].pks_type == pc->pkc_type )  break;
	}
	bu_log("rtsync: unable to handle %s message: len %d",
		pc->pkc_switch[i].pks_title, pc->pkc_len);
	*buf = '*';
	if( buf )  free(buf);
}

/*
 *			R T S Y N C _ P H _ A L I V E
 */
void
rtsync_ph_alive(pc, buf)
register struct pkg_conn *pc;
char			*buf;
{
	register int	i;
	int		ncpu;
	struct bu_vls	cmd;

	ncpu = atoi(buf);
	if( buf )  free(buf);

	for( i = MAX_NODES-1; i >= 0; i-- )  {
		if( rtnodes[i].pkg != pc )  continue;

		/* Found it */
		bu_log("%s %s ALIVE %d cpus\n", stamp(),
			rtnodes[i].host->ht_name,
			ncpu );

		rtnodes[i].ncpus = ncpu;

		if( change_state( i, STATE_NEWBORN, STATE_ALIVE ) < 0 )
			return;

		bu_vls_init( &cmd );
		bu_vls_printf( &cmd, "%d %d %s", width, height, framebuffer );

		if( pkg_send_vls( RTSYNCMSG_OPENFB, &cmd, rtnodes[i].pkg ) < 0 )  {
			bu_vls_free( &cmd );
			drop_rtnode(i);
		     	return;
		}
		bu_vls_free( &cmd );
		return;
	}
	rt_bomb("ALIVE Message received from phantom pkg?\n");
}

/*
 *			R T S Y N C _ P H _ O P E N F B
 */
void
rtsync_ph_openfb(pc, buf)
register struct pkg_conn *pc;
char			*buf;
{
	register int	i;
	int		ncpu;

	if( buf )  free(buf);

	for( i = MAX_NODES-1; i >= 0; i-- )  {
		if( rtnodes[i].pkg != pc )  continue;

		/* Found it */
		if( change_state( i, STATE_ALIVE, STATE_OPENFB ) < 0 )
			return;

		if( pkg_send( RTSYNCMSG_DIRBUILD, database, strlen(database)+1,
		     rtnodes[i].pkg ) < 0 )  {
			drop_rtnode(i);
		     	return;
		}
		return;
	}
	rt_bomb("OPENFB Message received from phantom pkg?\n");
}

/*
 *			R T S Y N C _ P H _ D I R B U I L D
 *
 *  Reply contains database title string.
 */
void
rtsync_ph_dirbuild(pc, buf)
register struct pkg_conn *pc;
char			*buf;
{
	register int	i;
	int		ncpu;

	for( i = MAX_NODES-1; i >= 0; i-- )  {
		if( rtnodes[i].pkg != pc )  continue;

		/* Found it */
		bu_log("%s %s %s\n", stamp(),
			rtnodes[i].host->ht_name, buf );
		if( buf )  free(buf);

		if( change_state( i, STATE_OPENFB, STATE_DIRBUILT ) < 0 )
			return;

		if( pkg_send_vls( RTSYNCMSG_GETTREES, &treetops,
		     rtnodes[i].pkg ) < 0 )  {
			drop_rtnode(i);
		     	return;
		}
		return;
	}
	rt_bomb("DIRBUILD Message received from phantom pkg?\n");
}

/*
 *			R T S Y N C _ P H _ G E T T R E E S
 *
 *  Reply contains name of first treetop.
 */
void
rtsync_ph_gettrees(pc, buf)
register struct pkg_conn *pc;
char			*buf;
{
	register int	i;
	int		ncpu;

	for( i = MAX_NODES-1; i >= 0; i-- )  {
		if( rtnodes[i].pkg != pc )  continue;

		/* Found it */
		bu_log("%s %s %s\n", stamp(),
			rtnodes[i].host->ht_name, buf );
		if( buf )  free(buf);

		if( change_state( i, STATE_DIRBUILT, STATE_PREPPED ) < 0 )
			return;

		/* No more dialog, next pkg will be a POV */

		return;
	}
	rt_bomb("GETTREES Message received from phantom pkg?\n");
}

/*
 *			T V D I F F
 *
 *  Return t1 - t0, as a floating-point number of seconds.
 */
double
tvdiff(t1, t0)
struct timeval	*t1, *t0;
{
	return( (t1->tv_sec - t0->tv_sec) +
		(t1->tv_usec - t0->tv_usec) / 1000000. );
}

/*
 *			R T S Y N C _ P H _ D O N E
 */
void
rtsync_ph_done(pc, buf)
register struct pkg_conn *pc;
char			*buf;
{
	register int	i;
	struct timeval	time_end;
	double		interval;

	for( i = MAX_NODES-1; i >= 0; i-- )  {
		if( rtnodes[i].pkg != pc )  continue;

		/* Found it */
		bu_log("%s DONE %s\n", stamp(), rtnodes[i].host->ht_name );
		rtnodes[i].busy = 0;
		if( buf )  free(buf);
		goto check_others;
	}
	rt_bomb("DONE Message received from phantom pkg?\n");

check_others:
	for( i = MAX_NODES-1; i >= 0; i-- )  {
		if( rtnodes[i].fd <= 0 )  continue;
		if( rtnodes[i].state != STATE_PREPPED )  continue;
		if( rtnodes[i].ncpus <= 0 )  continue;
		if( rtnodes[i].busy )  return;	/* Still working on last one */
	}
	/* This frame is now done, flush to screen */
	fb_flush(fbp);

	/* Compute total time for this frame */
	(void)gettimeofday( &time_end, (struct timezone *)NULL );
	interval = tvdiff( &time_end, &time_start );
	if( interval <= 0 )  interval = 999;
	bu_log("%s Frame complete in %g seconds (%g fps)\n",
		stamp(),
		interval, 1.0/interval );
}

/*
 *			S T A M P
 *
 *  Return a string suitable for use as a timestamp.
 *  Mostly for stamping log messages with.
 */
char *
stamp()
{
	static char	buf[128];
	time_t		now;
	struct tm	*tmp;
	register char	*cp;

	(void)time( &now );
	tmp = localtime( &now );
	sprintf( buf, "%2.2d/%2.2d %2.2d:%2.2d:%2.2d",
		tmp->tm_mon+1, tmp->tm_mday,
		tmp->tm_hour, tmp->tm_min, tmp->tm_sec );

	return(buf);
}

/*
 *			P H _ P R I N T
 */
void
ph_print(pc, buf)
register struct pkg_conn *pc;
char *buf;
{
	if(print_on)  {
		struct ihost	*ihp = host_lookup_of_fd(pc->pkc_fd);

		bu_log("%s %s: %s",
			stamp(),
			ihp ? ihp->ht_name : "NONAME",
			buf );
		if( buf[strlen(buf)-1] != '\n' )
			bu_log("\n");
	}
	if(buf) (void)free(buf);
}
