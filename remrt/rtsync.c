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
#include "externs.h"

#include "./ihost.h"

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
#define RTSYNCMSG_POV	1007	/* MtoS:  pov, min_res, start&end lines */
#define RTSYNCMSG_HALT	1008	/* MtoS:  abandon frame & xmit, NOW */
#define RTSYNCMSG_DONE	1009	/* StoM:  halt=0/1, res, elapsed, etc... */

void	rtsync_ph_alive();
void	rtsync_ph_done();
void	ph_print();
static struct pkg_switch rtsync_pkgswitch[] = {
	{ RTSYNCMSG_ALIVE,	rtsync_ph_alive, "RTNODE is alive" },
	{ RTSYNCMSG_POV,	ph_default,	"POV" },
	{ RTSYNCMSG_HALT,	ph_default,	"HALT" },
	{ RTSYNCMSG_DONE,	rtsync_ph_done, "RTNODE assignment done" },
	{ RTSYNCMSG_PRINT,	ph_print,	"Log Message" },
	{ 0,			0,		(char *)0 }
};

int			rtsync_listen_fd;	/* for new connections */

/*
 *			R T N O D E
 *
 *  One per compute server host.
 */
struct rtnode {
	int		fd;
	struct pkg_conn	*pkg;
	struct ihost	*host;
	int		ncpus;		/* Ready when > 0, for now */
};
#define MAX_NODES	32
struct rtnode	rtnodes[MAX_NODES];

static fd_set	select_list;			/* master copy */
static int	max_fd;

static	FBIO	*fbp;
static	char	*framebuffer = "vj:0";
static	int	width = 0;		/* use default size */
static	int	height = 0;

void	new_rtnode();
void	drop_rtnode();
void	setup_socket();
char	*stamp();


static char usage[] = "\
Usage:  rtsync [-h] [-S squaresize] [-W width] [-N height] [-F framebuffer]\n\
";

/*
 *			G E T _ A R G S
 */
get_args( argc, argv )
register char **argv;
{
	register int c;

	while ( (c = getopt( argc, argv, "hF:s:w:n:S:W:N:" )) != EOF )  {
		switch( c )  {
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
	if( argc > optind )
		return(0);	/* print usage */

	return(1);		/* OK */
}

/*
 *			M A I N
 */
main(argc, argv)
int	argc;
char	*argv[];
{
	if ( !get_args( argc, argv ) ) {
		(void)fputs(usage, stderr);
		exit( 1 );
	}

	RT_LIST_INIT( &rt_g.rtg_vlfree );

	/* Connect up with framebuffer, for control & size purposes */
	if( (fbp = fb_open(framebuffer, width, height)) == FBIO_NULL )
		exit(1);

	width = fb_getwidth(fbp);
	height = fb_getheight(fbp);

	/* Listen for VRMGR Master PKG connections from MGED */
	if( (vrmgr_listen_fd = pkg_permserver("5555", "tcp", 8, rt_log)) < 0 )  {
		rt_log("Unable to listen on 5555\n");
		exit(1);
	}

	/* Listen for our RTSYNC client's PKG connections */
	if( (rtsync_listen_fd = pkg_permserver("rtsrv", "tcp", 8, rt_log)) < 0 )  {
		int	i;
		char	num[8];
		/* Do it by the numbers */
		for(i=0; i<10; i++ )  {
			sprintf( num, "%d", 4446+i );
			if( (rtsync_listen_fd = pkg_permserver(num, "tcp", 8, rt_log)) < 0 )
				continue;
			break;
		}
		if( i >= 10 )  {
			rt_log("Unable to find a port to listen on\n");
			exit(1);
		}
	}
	/* Now, pkg_permport has tcp port number */
	rt_log("%s RTSYNC listening on %s port %d\n",
		stamp(),
		get_our_hostname(),
		pkg_permport);

	(void)signal( SIGPIPE, SIG_IGN );

	/* Establish select_list and maxfd, to poll critical fd's */
	FD_ZERO(&select_list);
	FD_SET(vrmgr_listen_fd, &select_list);
	FD_SET(rtsync_listen_fd, &select_list);
	max_fd = vrmgr_listen_fd;
	if( rtsync_listen_fd > max_fd )  max_fd = rtsync_listen_fd;

	if( fbp->if_selfd > 0 )  {
		FD_SET(fbp->if_selfd, &select_list);
		if( fbp->if_selfd > max_fd )  max_fd = fbp->if_selfd;
	}

	/*
	 *  Main loop
	 */
	for(;;)  {
		fd_set infds;
		struct timeval tv;
		register int	i;

		infds = select_list;	/* struct copy */

		tv.tv_sec = 60L;
		tv.tv_usec = 0L;
		if( (select( max_fd+1, &infds, (fd_set *)0, (fd_set *)0, 
			     &tv )) == 0 ) {
			/* printf("select timeout\n"); */
			if(fbp) fb_poll(fbp);
			continue;
		}
#if 0
		printf("infds = x%x, select_list=x%x\n", infds.fds_bits[0], select_list.fds_bits[0] );
#endif
		/* Handle any events from the framebuffer */
		if (fbp && fbp->if_selfd > 0 && FD_ISSET(fbp->if_selfd, &infds))
			fb_poll(fbp);

		/* Accept any new VRMGR connections.  Only one at a time is permitted. */
		if( vrmgr_listen_fd > 0 && FD_ISSET(vrmgr_listen_fd, &infds))  {
			if( vrmgr_pc )  {
				rt_log("New VRMGR connection received with one still active, dropping old one.\n");
				FD_CLR(vrmgr_pc->pkc_fd, &select_list);
				pkg_close( vrmgr_pc );
				vrmgr_pc = 0;
				vrmgr_ihost = IHOST_NULL;
			}
			vrmgr_pc = pkg_getclient( vrmgr_listen_fd, vrmgr_pkgswitch, rt_log, 0 );
			vrmgr_ihost = host_lookup_of_fd(vrmgr_pc->pkc_fd);
			if( vrmgr_ihost == IHOST_NULL )  {
				rt_log("Unable to get hostname of VRMGR, abandoning it\n");
				FD_CLR(vrmgr_pc->pkc_fd, &select_list);
				pkg_close( vrmgr_pc );
				vrmgr_pc = 0;
			} else {
				rt_log("%s VRMGR link with %s, fd=%d\n",
					stamp(),
					vrmgr_ihost->ht_name, vrmgr_pc->pkc_fd);
				FD_SET(vrmgr_pc->pkc_fd, &select_list);
				if( vrmgr_pc->pkc_fd > max_fd )  max_fd = vrmgr_pc->pkc_fd;
				setup_socket( vrmgr_pc->pkc_fd );
			}
		}

		/* Accept any new RTNODE connections */
		if( rtsync_listen_fd > 0 && FD_ISSET(rtsync_listen_fd, &infds))  {
			new_rtnode( pkg_getclient( rtsync_listen_fd, rtsync_pkgswitch, rt_log, 0 ) );
		}

		/* Process arrivals from VRMGR link */
		if( vrmgr_pc && FD_ISSET(vrmgr_pc->pkc_fd, &infds) )  {
			if( pkg_suckin( vrmgr_pc ) <= 0 )  {
				/* Probably an EOF */
				FD_CLR(vrmgr_pc->pkc_fd, &select_list);
				pkg_close( vrmgr_pc );
				vrmgr_pc = 0;
				vrmgr_ihost = IHOST_NULL;
			} else {
				if( pkg_process( vrmgr_pc ) < 0 )
					rt_log("VRMGR pkg_process error\n");
			}
		}

		/* Process arrivals from existing rtnodes */
		for( i = MAX_NODES-1; i >= 0; i-- )  {
			if( rtnodes[i].fd == 0 )  continue;
			if( pkg_process( rtnodes[i].pkg ) < 0 ) {
				rt_log("pkg_process error encountered (1)\n");
			}
			if( ! FD_ISSET( rtnodes[i].fd, &infds ) )  continue;
			if( pkg_suckin( rtnodes[i].pkg ) <= 0 )  {
				/* Probably EOF */
				drop_rtnode( i );
				continue;
			}
			if( pkg_process( rtnodes[i].pkg ) < 0 ) {
				rt_log("pkg_process error encountered (2)\n");
			}
		}

		/* Dispatch work */
		dispatcher();

	}
}

/*
 *			D I S P A T C H E R
 */
int
dispatcher()
{
	register int	i;
	int		ncpu = 0;
	static int	sent_one = 0;
	int		start_line;
	int		lowest_index = 0;

	if( !pending_pov )  return 0;
	if( sent_one )  return 0;

	for( i = MAX_NODES-1; i >= 0; i-- )  {
		if( rtnodes[i].fd != 0 )  continue;
		if( rtnodes[i].ncpus <= 0 )  continue;
		ncpu += rtnodes[i].ncpus;
		lowest_index = i;
	}
	if( ncpu <= 0 )  return 0;

	/* Have some CPUS! Parcel up 'height' scanlines. */
	start_line = 0;
	for( i = MAX_NODES-1; i >= 0; i-- )  {
		int	end_line;
		int	count;
		struct rt_vls	msg;

		if( start_line >= height )  break;
		if( rtnodes[i].fd != 0 )  continue;
		if( rtnodes[i].ncpus <= 0 )  continue;

		if( i <= lowest_index )  {
			end_line = height-1;
		} else {
			count = (int)ceil( ((double)rtnodes[i].ncpus) / ncpu * height );
			end_line = start_line + count;
			if( end_line > height-1 )  end_line = height-1;
		}

		rt_vls_init( &msg );
		rt_vls_printf( &msg, "%d %d %d %s\n",
			256,
			start_line, end_line,
			pending_pov+4 );
		if( pkg_send( RTSYNCMSG_POV, rt_vls_addr(&msg), rt_vls_strlen(&msg)+1, rtnodes[i].pkg ) < 0 )  {
			drop_rtnode( i );
			rt_vls_free(&msg);
			continue;	/* Don't update start_line */
		}
		rt_log("%s sending %d..%d to %s\n", stamp(), start_line, end_line, rtnodes[i].host->ht_name);

		rt_vls_free(&msg);
		start_line = end_line + 1;
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

	if( pcp == PKC_ERROR )
		return;

	for( i = MAX_NODES-1; i >= 0; i-- )  {
		if( rtnodes[i].fd != 0 )  continue;
		/* Found an available slot */
		rtnodes[i].pkg = pcp;
		rtnodes[i].fd = pcp->pkc_fd;
		FD_SET(pcp->pkc_fd, &select_list);
		if( pcp->pkc_fd > max_fd )  max_fd = pcp->pkc_fd;
		setup_socket( pcp->pkc_fd );
		rtnodes[i].host = host_lookup_of_fd(pcp->pkc_fd);
rt_log("%s Connection from %s\n", stamp(), rtnodes[i].host->ht_name);
		return;
	}
	rt_log("rtsync: too many rtnode clients.  My cup runneth over!\n");
	pkg_close(pcp);
}

/*
 *			D R O P _ R T N O D E
 */
void
drop_rtnode( sub )
int	sub;
{

	if( rtnodes[sub].pkg != PKC_NULL )  {
		pkg_close( rtnodes[sub].pkg );
		rtnodes[sub].pkg = PKC_NULL;
	}
	if( rtnodes[sub].fd != 0 )  {
		FD_CLR( rtnodes[sub].fd, &select_list );
		close( rtnodes[sub].fd );
		rtnodes[sub].fd = 0;
	}
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
	if( setsockopt( fd, SOL_SOCKET, SO_KEEPALIVE, &on, sizeof(on)) < 0 ) {
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

	rt_log("%s VRMGR host %s, role %s\n",
		stamp(),
		vrmgr_ihost->ht_name, buf);

	argc = rt_split_cmd( argv, MAXARGS, buf );
	if( argc < 1 )  {
		rt_log("bad role command\n");
	}

	if( strcmp( argv[0], "master" ) != 0 )  {
		rt_log("ERROR %s: bad role '%s', dropping vrmgr\n",
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

	rt_log("%s VRMGR unexpectely got event '%s'", stamp(), buf );
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
rt_log("%s %s\n", stamp(), buf);
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
	rt_log("rtsync: unable to handle %s message: len %d",
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
	struct rtnode	*np;
	register int	i;
	int		ncpu;

	ncpu = atoi(buf);

	for( i = MAX_NODES-1; i >= 0; i-- )  {
		if( rtnodes[i].pkg != pc )  continue;

		/* Found it */
		rt_log("%s ALIVE %d cpus, %s\n", stamp(),
			ncpu, rtnodes[i].host->ht_name );

		np->ncpus = ncpu;
		if( buf )  free(buf);
	}
	rt_bomb("ALIVE Message received from phantom pkg?\n");
}

/*
 *			R T S Y N C _ P H _ D O N E
 */
void
rtsync_ph_done(pc, buf)
register struct pkg_conn *pc;
char			*buf;
{
	struct ihost	*ihp;

	rt_log("It's done! %s\n", buf);
	ihp = host_lookup_of_fd( pc->pkc_fd );
	if(ihp) rt_log("  %s is done\n", ihp->ht_name );
	if( buf )  free(buf);
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
	long		now;
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

		rt_log("%s %s: %s",
			stamp(),
			ihp ? ihp->ht_name : "NONAME",
			buf );
		if( buf[strlen(buf)-1] != '\n' )
			rt_log("\n");
	}
	if(buf) (void)free(buf);
}
