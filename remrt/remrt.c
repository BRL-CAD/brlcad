/*
 *  			R E M R T . C
 *  
 *  Controller for network ray-tracing
 *  Operating as both a network client and server,
 *  starts remote invocations of "rtsrv" via "rsh", then
 *  handles incomming connections offering cycles,
 *  accepting input (solicited and unsolicited) via the pkg.c routines,
 *  and storing the results in files and/or a framebuffer.
 *
 *  Author -
 *	Michael John Muuss
 *
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005
 *  
 *  Copyright Notice -
 *	This software is Copyright (C) 1989 by the United States Army.
 *	All rights reserved.
 */
static char RCSid[] = "@(#)$Header$ (BRL)";

#include <stdio.h>
#include <ctype.h>
#include <signal.h>
#include <errno.h>
#include <netdb.h>
#include <math.h>
#ifdef SYSV
# include <string.h>
#else
# include <strings.h>
# define strchr(s, c)	index(s, c)
# define strrchr(s, c)	rindex(s, c)
#endif

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#if !defined(CRAY1)
#include <sys/time.h>		/* for struct timeval */
#endif
#if !defined(sun)
#include <time.h>
#endif

#include "machine.h"
#include "vmath.h"
#include "raytrace.h"
#include "fb.h"
#include "pkg.h"

#include "./list.h"
#include "./remrt.h"
#include "./inout.h"
#include "./protocol.h"

extern int	getopt();
extern char	*optarg;
extern int	optind;
extern char	*getenv();

#ifdef SYSV
#define vfork	fork
#endif SYSV

void		read_rc_file();
void		check_input();
void		addclient();
void		start_servers();
void		eat_script();
void		interactive_cmd();
void		prep_frame();
void		frame_is_done();
void		destroy_frame();
void		schedule();
void		list_remove();
void		write_fb();
void		repaint_fb();
void		size_display();
void		send_start();
void		send_restart();
void		send_loglvl();
void		send_matrix();
void		send_to_lines();
void		pr_list();
void		mathtab_constant();
void		add_host();
void		host_helper();
void		start_helper();
void		build_start_cmd();
void		drop_server();
void		send_do_lines();

struct vls  {
	char	*vls_str;
	int	vls_cur;	/* Length, not counting the null */
	int	vls_max;
};

void
vls_init( vp )
register struct vls	*vp;
{
	vp->vls_cur = vp->vls_max = 0;
	vp->vls_str = (char *)0;
}

void
vls_cat( vp, s )
register struct vls	*vp;
char		*s;
{
	register int	len;

	len = strlen(s);
	if( vp->vls_cur <= 0 )  {
		vp->vls_str = rt_malloc( vp->vls_max = len*4, "vls initial" );
		vp->vls_cur = 0;
	}
	if( vp->vls_cur + len >= vp->vls_max )  {
		vp->vls_max = (vp->vls_max + len) * 2;
		vp->vls_str = rt_realloc( vp->vls_str, vp->vls_max, "vls" );
	}
	bcopy( s, vp->vls_str + vp->vls_cur, len+1 );
	vp->vls_cur += len;
}

void
vls_2cat( op, ip )
register struct vls	*op, *ip;
{
	vls_cat( op, ip->vls_str );
	rt_free( ip->vls_str, "vls_2cat input vls_str" );
	vls_init( ip );
}

FBIO *fbp = FBIO_NULL;		/* Current framebuffer ptr */
int cur_fbwidth;		/* current fb width */

int running = 0;		/* actually working on it */
int detached = 0;		/* continue after EOF */

/*
 * Package Handlers.
 */
void	ph_default();	/* foobar message handler */
void	ph_pixels();
void	ph_print();
void	ph_start();
void	ph_version();
void	ph_cmd();
struct pkg_switch pkgswitch[] = {
	{ MSG_START,	ph_start,	"Startup ACK" },
	{ MSG_MATRIX,	ph_default,	"Set Matrix" },
	{ MSG_LINES,	ph_default,	"Compute lines" },
	{ MSG_END,	ph_default,	"End" },
	{ MSG_PIXELS,	ph_pixels,	"Pixels" },
	{ MSG_PRINT,	ph_print,	"Log Message" },
	{ MSG_VERSION,	ph_version,	"Protocol version check" },
	{ MSG_CMD,	ph_cmd,		"Run one command" },
	{ 0,		0,		(char *)0 }
};

int clients;
int print_on = 1;

#define NFD 32
#define MAXSERVERS	NFD		/* No relay function yet */

struct list *FreeList;

struct frame {
	struct frame	*fr_forw;
	struct frame	*fr_back;
	int		fr_number;	/* frame number */
	char		*fr_filename;	/* name of output file */
	/* options */
	int		fr_width;	/* frame width (pixels) */
	int		fr_height;	/* frame height (pixels) */
	struct list	fr_todo;	/* work still to be done */
	/* Timings */
	struct timeval	fr_start;	/* start time */
	struct timeval	fr_end;		/* end time */
	long		fr_nrays;	/* rays fired so far */
	double		fr_cpu;		/* CPU seconds used so far */
	/* Current view */
	struct vls	fr_cmd;		/* RT options & command string */
	char		fr_servinit[MAXSERVERS]; /* sent server options & matrix? */
};
struct frame FrameHead;
struct frame *FreeFrame;
#define FRAME_NULL	((struct frame *)0)

struct servers {
	struct pkg_conn	*sr_pc;		/* PKC_NULL means slot not in use */
	struct list	sr_work;
	struct ihost	*sr_host;	/* description of this host */
	int		sr_lump;	/* # lines to send at once */
	int		sr_state;	/* Server state, SRST_xxx */
#define SRST_UNUSED	0		/* unused slot */
#define SRST_NEW	1		/* connected, awaiting vers check */
#define SRST_VERSOK	2		/* version OK, no model loaded yet */
#define SRST_LOADING	3		/* loading, awaiting ready response */
#define SRST_READY	4		/* loaded, ready */
#define SRST_RESTART	5		/* about to restart */
	struct frame	*sr_curframe;	/* ptr to current frame */
	int		sr_index;	/* fr_servinit[] index */
	/* Timings */
	struct timeval	sr_sendtime;	/* time of last sending */
	double		sr_l_elapsed;	/* last: elapsed_sec */
	double		sr_l_el_rate;	/* last: pix/elapsed_sec */
	double		sr_w_elapsed;	/* weighted avg: pix/elapsed_sec */
	double		sr_s_elapsed;	/* sum of pix/elapsed_sec */
	double		sr_sq_elapsed;	/* sum of pix/elapsed_sec squared */
	double		sr_l_cpu;	/* cpu_sec for last scanline */
	double		sr_s_cpu;	/* sum of all cpu times */
	int		sr_nsamp;	/* number of samples summed over */
	double		sr_l_percent;	/* last: percent of CPU */
} servers[MAXSERVERS];
#define SERVERS_NULL	((struct servers *)0)

/* Internal Host table */
struct ihost {
	char		*ht_name;	/* Official name of host */
	int		ht_when;	/* When to use this host */
#define HT_ALWAYS	1		/* Always try to use this one */
#define HT_NIGHT	2		/* Only use at night */
#define HT_PASSIVE	3		/* May call in, never initiated */
	int		ht_where;	/* Where to find database */
#define HT_CD		1		/* cd to ht_path first */
#define HT_CONVERT	2		/* cd to ht_path, asc2g database */
	char		*ht_path;	/* remote directory to run in */
	struct ihost	*ht_next;
};
struct ihost		*HostHead;
#define IHOST_NULL	((struct ihost *)0)
struct ihost		*host_lookup_by_name();
struct ihost		*host_lookup_by_addr();
struct ihost		*host_lookup_by_hostent();
struct ihost		*make_default_host();

/* variables shared with viewing model */
extern double	AmbientIntensity;
extern double	azimuth, elevation;
extern int	lightmodel;
int		use_air = 0;

/* variables shared with worker() */
extern int	hypersample;
extern int	jitter;
extern fastf_t	rt_perspective;
extern fastf_t	aspect;
extern fastf_t	eye_backoff;
extern int	width;
extern int	height;

/* variables shared with do.c */
extern int	matflag;
extern int	interactive;
extern int	benchmark;
int		rdebug;
extern char	*outputfile;		/* output file name */
extern char	*framebuffer;
extern int	desiredframe;

extern struct rt_g	rt_g;

extern struct command_tab cmd_tab[];	/* given at end */

char	file_basename[128];	/* contains last component of file name */
char	file_fullname[128];	/* contains full file name */
char	object_list[512];	/* contains list of "MGED" objects */

FILE	*helper_fp;		/* pipe to rexec helper process */
char	ourname[128];

extern char *malloc();

int	tcp_listen_fd;
extern int	pkg_permport;	/* libpkg/pkg_permserver() listen port */

/*
 *			T V S U B
 */
void
tvsub(tdiff, t1, t0)
struct timeval *tdiff, *t1, *t0;
{

	tdiff->tv_sec = t1->tv_sec - t0->tv_sec;
	tdiff->tv_usec = t1->tv_usec - t0->tv_usec;
	if (tdiff->tv_usec < 0)
		tdiff->tv_sec--, tdiff->tv_usec += 1000000;
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
 *			M A I N
 */
int
main(argc, argv)
int	argc;
char	**argv;
{
	register struct servers *sp;
	struct timeval	now;

	/* Random inits */
	gethostname( ourname, sizeof(ourname) );
	fprintf(stderr,"%s: %s\n", ourname, RCSid+13);
	fflush(stderr);

	width = height = 512;			/* same size as RT */

	start_helper();

	FrameHead.fr_forw = FrameHead.fr_back = &FrameHead;
	for( sp = &servers[0]; sp < &servers[MAXSERVERS]; sp++ )  {
		sp->sr_work.li_forw = sp->sr_work.li_back = &sp->sr_work;
		sp->sr_pc = PKC_NULL;
	}

	/* Listen for our PKG connections */
	if( (tcp_listen_fd = pkg_permserver("rtsrv", "tcp", 8, rt_log)) < 0 &&
	    (tcp_listen_fd = pkg_permserver("4446", "tcp", 8, rt_log)) < 0 )
		exit(1);
	/* Now, pkg_permport has tcp port number */

	(void)signal( SIGPIPE, SIG_IGN );

	if( argc <= 1 )  {
		(void)signal( SIGINT, SIG_IGN );
		rt_log("Interactive REMRT listening at port %d\n", pkg_permport);
		clients = (1<<fileno(stdin));

		/* Read .remrtrc file to acquire server info */
		read_rc_file();

		while(clients)  {
			check_input( 30 );	/* delay up to 30 secs */

			(void)gettimeofday( &now, (struct timezone *)0 );
			schedule( &now );
		}
		/* Might want to see if any work remains, and if so,
		 * record it somewhere */
		rt_log("REMRT out of clients\n");
	} else {
		rt_log("Automatic REMRT listening at port %d, reading script on stdin\n",
			pkg_permport);
		clients = 0;

		/* parse command line args for sizes, etc */
		if( !get_args( argc, argv ) )  {
			fprintf(stderr,"remrt:  bad arg list\n");
			exit(1);
		}

		/* take note of database name and treetops */
		if( optind+2 > argc )  {
			fprintf(stderr,"remrt:  insufficient args\n");
			exit(2);
		}
		build_start_cmd( argc, argv, optind );

		/* Read .remrtrc file to acquire servers */
		read_rc_file();

		/* Collect up results of arg parsing */
		/* automatic: outputfile, width, height */
		if( framebuffer || outputfile == (char *)0 )  {
			init_fb(framebuffer);
		}

		/* Build queue of work to be done */
		if( !matflag )  {
			struct frame	*fr;
			char	buf[128];
			/* if not -M, queue off single az/el frame */
			GET_FRAME(fr);
			prep_frame(fr);
			sprintf(buf, "ae %g %g;", azimuth, elevation);
			vls_cat( &fr->fr_cmd, buf);
			if( create_outputfilename( fr ) < 0 )  {
				FREE_FRAME(fr);
			} else {
				APPEND_FRAME( fr, FrameHead.fr_back );
			}
		} else {
			/* if -M, read RT script from stdin */
			clients = 0;
			eat_script( stdin );
		}
		cd_frames( 0, (char **)0 );

		/* Compute until no work remains */
		running = 1;
		while( FrameHead.fr_forw != &FrameHead )  {
			(void)gettimeofday( &now, (struct timezone *)0 );
			start_servers( &now );
			check_input( 30 );	/* delay up to 30 secs */

			(void)gettimeofday( &now, (struct timezone *)0 );
			schedule( &now );
		}
		rt_log("REMRT:  task accomplished\n");
	}
	return(0);			/* exit(0); */
}

/*
 *			R E A D _ R C _ F I L E
 *
 *  Read a .remrt file.  While this file can contain any valid commands,
 *  the intention is primarily to permit "automatic" registration of
 *  server hosts via "host" commands.
 */
void
read_rc_file()
{
	FILE	*fp;
	char	*home;
	char	path[128];

	if( (fp = fopen(".remrtrc", "r")) != NULL )  {
		source(fp);
		fclose(fp);
		return;
	}

	if( (home = getenv("HOME")) != NULL )  {
		sprintf( path, "%s/.remrtrc", home );
		if( (fp = fopen( path, "r" )) != NULL )  {
			source(fp);
			fclose(fp);
			return;
		}
	}

	if( (fp = fopen("/usr/brlcad/etc/.remrtrc", "r")) != NULL )  {
		source(fp);
		fclose(fp);
		return;
	}
}

/*
 *			C H E C K _ I N P U T
 */
void
check_input(waittime)
int waittime;
{
	static long	ibits;
	register int	i;
	static struct timeval tv;

	tv.tv_sec = waittime;
	tv.tv_usec = 0;

	ibits = clients|(1<<tcp_listen_fd);
	i=select(32, (char *)&ibits, (char *)0, (char *)0, &tv);
	if( i < 0 )  {
		perror("select");
		return;
	}
	/* First, accept any pending connections */
	if( ibits & (1<<tcp_listen_fd) )  {
		register struct pkg_conn *pc;
		pc = pkg_getclient(tcp_listen_fd, pkgswitch, rt_log, 1);
		if( pc != PKC_NULL && pc != PKC_ERROR )
			addclient(pc);
		ibits &= ~(1<<tcp_listen_fd);
	}
	/* Second, give priority to getting traffic off the network */
	for( i=0; i<NFD; i++ )  {
		register struct pkg_conn *pc;

		if( !feof(stdin) && i == fileno(stdin) )  continue;
		if( i == tcp_listen_fd )  continue;
		if( !(ibits&(1<<i)) )  continue;
		pc = servers[i].sr_pc;
		if( pkg_get(pc) < 0 )
			drop_server( &servers[i] );
		ibits &= ~(1<<i);
	}
	/* Finally, handle any command input (This can recurse via "read") */
	if( waittime>0 &&
	    !feof(stdin) &&
	    fileno(stdin) >= 0 &&
	    (ibits & (1<<(fileno(stdin)))) )  {
		interactive_cmd(stdin);
	}
}

/*
 *			A D D C L I E N T
 */
void
addclient(pc)
struct pkg_conn *pc;
{
	register struct servers *sp;
	register struct frame	*fr;
	struct ihost	*ihp;
	int		on = 1;
	auto int	fromlen;
	struct sockaddr_in from;
	int fd;

	fd = pc->pkc_fd;

	fromlen = sizeof (from);
	if (getpeername(fd, &from, &fromlen) < 0) {
		perror("getpeername");
		close(fd);
		return;
	}
	if (setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &on, sizeof (on)) < 0)
		perror("setsockopt (SO_KEEPALIVE)");

	if( (ihp = host_lookup_by_addr( &from, 1 )) == IHOST_NULL )  {
		/* Disaster */
		rt_log("abandoning this unknown server!!\n");
		close( fd );
		/* Maybe free the pkg struct? */
		return;
	}

	clients |= 1<<fd;

	sp = &servers[fd];
	bzero( (char *)sp, sizeof(*sp) );
	sp->sr_pc = pc;
	sp->sr_work.li_forw = sp->sr_work.li_back = &(sp->sr_work);
	sp->sr_state = SRST_NEW;
	sp->sr_curframe = FRAME_NULL;
	sp->sr_lump = 32;
	sp->sr_index = fd;
	sp->sr_host = ihp;

	/* Clear any frame state that may remain from an earlier server */
	for( fr = FrameHead.fr_forw; fr != &FrameHead; fr = fr->fr_forw)  {
		fr->fr_servinit[sp->sr_index] = 0;
	}
}

/*
 *			D R O P _ S E R V E R
 */
void
drop_server(sp)
register struct servers	*sp;
{
	register struct list	*lhp, *lp;
	register struct pkg_conn *pc;
	register struct frame	*fr;
	int fd;
	int	oldstate;

	if( sp == SERVERS_NULL || sp->sr_host == IHOST_NULL )  {
		rt_log("drop_server(x%x), sr_host=0\n", sp);
		return;
	}
	oldstate = sp->sr_state;
	rt_log("dropping %s\n", sp->sr_host->ht_name);

	pc = sp->sr_pc;
	if( pc == PKC_NULL )  {
		rt_log("drop_server(x%x), sr_pc=0\n", sp);
		return;
	}
	fd = pc->pkc_fd;
	if( fd <= 3 || fd >= NFD )  {
		rt_log("drop_server: fd=%d is unreasonable, forget it!\n", fd);
		return;
	}
	clients &= ~(1<<fd);

	sp->sr_pc = PKC_NULL;
	sp->sr_state = SRST_UNUSED;
	sp->sr_index = -1;
	sp->sr_host = IHOST_NULL;

	/* After most server state has been zapped, close PKG connection */
	pkg_close(pc);

	if( oldstate != SRST_READY )  return;

	/* Need to requeue any work that was in progress */
	lhp = &(sp->sr_work);
	while( (lp = lhp->li_forw) != lhp )  {
		fr = lp->li_frame;
		DEQUEUE_LIST( lp );
		rt_log("requeueing fr%d %d..%d\n",
			fr->fr_number,
			lp->li_start, lp->li_stop);
		APPEND_LIST( lp, &(fr->fr_todo) );
	}
}

/*
 *			S T A R T _ S E R V E R S
 *
 *  Scan the ihost table.  For all eligible hosts that don't
 *  presently have a server running, try to start a server.
 */
#define SERVER_CHECK_INTERVAL	(10*60)		/* seconds */
struct timeval	last_server_check_time;

void
start_servers( nowp )
struct timeval	*nowp;
{
	register struct ihost	*ihp;
	register struct servers	*sp;
	int	night;
	int	add;

	if( file_fullname[0] == '\0' )  return;

	if( tvdiff( nowp, &last_server_check_time ) < SERVER_CHECK_INTERVAL )
		return;

	rt_log("seeking servers to start\n");
	night = is_night( nowp );
	for( ihp = HostHead; ihp != IHOST_NULL; ihp = ihp->ht_next )  {

		/* Skip hosts which are not eligible for add/drop */
		add = 1;
		switch( ihp->ht_when )  {
		case HT_ALWAYS:
			break;
		case HT_NIGHT:
			if( night )
				add = 1;
			else
				add = 0;
			break;
		default:
		case HT_PASSIVE:
			continue;
		}

		/* See if this host is already in contact as a server */
		for( sp = &servers[0]; sp < &servers[MAXSERVERS]; sp++ )  {
			if( sp->sr_pc == PKC_NULL )  continue;
			if( sp->sr_host != ihp )  continue;

			/* This host is a server */
			if( add == 0 )  {
				/* Drop this host -- out of time range */
				rt_log("auto dropping %s:  out of time range\n",
					ihp->ht_name );
				drop_server( sp );
			} else {
				/* Host already serving, do nothing more */
			}
			goto next_host;
		}

		/* This host is not presently in contact */
		if( add )  {
			rt_log("auto adding %s\n", ihp->ht_name);
			add_host( ihp );
		}

next_host:	continue;
	}
	last_server_check_time = *nowp;		/* struct copy */
}

/*
 *			I S _ N I G H T
 *
 *  Indicate whether the given time is "night", ie, off-peak time.
 *  The simple algorithm used here does not take into account
 *  using machines in another time zone, nor is it nice to
 *  machines used by hackers who stay up late.
 */
int
is_night( tv )
struct timeval	*tv;
{
	struct tm	*tp;

	tp = localtime( &tv->tv_sec );

	/* Sunday(0) and Saturday(6) are "night" */
	if( tp->tm_wday == 0 || tp->tm_wday == 6 )  return(1);
	if( tp->tm_hour < 8 || tp->tm_hour >= 18 )  return(1);
	return(0);
}

/*
 *			E A T _ S C R I P T
 */
void
eat_script( fp )
FILE	*fp;
{
	char	*buf;
	char	*ebuf;
	int	argc;
	char	*argv[64];
	struct vls	body;
	struct vls	prelude;
	int	frame;
	struct frame	*fr;

	vls_init( &prelude );
	while( (buf = rt_read_cmd( fp )) != (char *)0 )  {
		if( strncmp( buf, "start", 5 ) != 0 )  {
			vls_cat( &prelude, buf );
			vls_cat( &prelude, ";" );
			rt_free( buf, "prelude line" );
			continue;
		}

		/* Gobble until "end" keyword seen */
		vls_init( &body );
		while( (ebuf = rt_read_cmd( fp )) != (char *)0 )  {
			if( strncmp( ebuf, "end", 3 ) == 0 )  {
				rt_free( ebuf, "end line" );
				break;
			}
			vls_cat( &body, ebuf );
			vls_cat( &body, ";" );
			rt_free( ebuf, "script line" );
		}

		/* buf has saved "start" line in it */
		argc = rt_split_cmd( argv, 64, buf );
		if( argc < 2 )  {
			rt_log("bad 'start' line\n");
			rt_free( buf, "bad start line" );
			goto out;
		}
		frame = atoi( argv[1] );
		if( frame < desiredframe )  {
			rt_free( body.vls_str, "script" );
			goto bad;
		}
		/* Might see if frame file exists 444 mode, then skip also */
		GET_FRAME(fr);
		fr->fr_number = frame;
		prep_frame(fr);
		if( prelude.vls_cur > 0 )  {
			vls_2cat( &fr->fr_cmd, &prelude );
		}
		if( body.vls_cur > 0 )  {
			vls_2cat( &fr->fr_cmd, &body );
		}
		if( create_outputfilename( fr ) < 0 )  {
			FREE_FRAME(fr);
		} else {
			APPEND_FRAME( fr, FrameHead.fr_back );
		}
bad:
		rt_free( buf, "command line" );
	}
out:	;
	/* Doing an fclose here can result in a server showing up on fd 0 */
	/* fclose(fp); */
}

/*
 *  			S T R I N G 2 I N T
 *  
 *  Convert a string to an integer.
 *  A leading "0x" implies HEX.
 *  If needed, octal might be done, but it seems unwise...
 *
 *  For general conversion, this is pretty feeble.  Is more needed?
 */
int
string2int(str)
register char *str;
{
	auto int ret;
	int cnt;

	ret = 0;
	if( str[0] == '0' && str[1] == 'x' )
		cnt = sscanf( str+2, "%x", &ret );
	else
		cnt = sscanf( str, "%d", &ret );
	if( cnt != 1 )
		rt_log("string2int(%s) = %d?\n", str, ret );
	return(ret);
}

/*
 *			G E T _ S E R V E R _ B Y _ N A M E
 */
struct servers *
get_server_by_name( str )
char *str;
{
	register struct servers *sp;
	struct ihost	*ihp;

	if( isdigit( *str ) )  {
		int	i;
		i = atoi( str );
		if( i < 0 || i >= MAXSERVERS )  return( SERVERS_NULL );
		return( &servers[i] );
	}

	if( (ihp = host_lookup_by_name( str, 0 )) == IHOST_NULL )
		return( SERVERS_NULL );

	for( sp = &servers[0]; sp < &servers[MAXSERVERS]; sp++ )  {
		if( sp->sr_pc == PKC_NULL )  continue;
		if( sp->sr_host == ihp )  return(sp);
	}
	return( SERVERS_NULL );
}

/*
 *			I N T E R A C T I V E _ C M D
 */
void
interactive_cmd(fp)
FILE *fp;
{
	char buf[BUFSIZ];
	register char *pos;
	register int i;

	pos = buf;

	/* Get first line */
	*pos = '\0';
	(void)fgets( pos, sizeof(buf), fp );
	i = strlen(buf);

	/* If continued, get more */
	while( pos[i-1]=='\n' && pos[i-2]=='\\' )  {
		pos += i-2;	/* zap NL and backslash */
		*pos = '\0';
		rt_log("-> "); (void)fflush(stderr);
		(void)fgets( pos, sizeof(buf)-strlen(buf), fp );
		i = strlen(pos);
	}

	if( feof(fp) ) {
		register struct servers *sp;

		if( fp != stdin )  return;

		/* Eof on stdin */
		clients &= ~(1<<fileno(fp));

		/* We might want to wait if something is running? */
		for( sp = &servers[0]; sp < &servers[MAXSERVERS]; sp++ )  {
			if( sp->sr_pc == PKC_NULL )  continue;
			if( !running )
				drop_server(sp);
			else
				send_restart(sp);
		}
		/* The rest happens when the connections close */
		return;
	}
	if( (i=strlen(buf)) <= 0 )  return;

	/* Feeble allowance for comments */
	if( buf[0] == '#' )  return;

	(void)rt_do_cmd( (struct rt_i *)0, buf, cmd_tab );
}

/*
 *			P R E P _ F R A M E
 *
 * Fill in frame structure after reading MAT
 */
void
prep_frame(fr)
register struct frame *fr;
{
	register struct list *lp;
	char buf[BUFSIZ];

	/* Get local buffer for image */
	fr->fr_width = width;
	fr->fr_height = height;

	fr->fr_cmd.vls_cur = 0;	/* free existing? */
	sprintf(buf, "opt -w%d -n%d -H%d -p%g -U%d -J%x -A%g -l%d -E%g",
		fr->fr_width, fr->fr_height,
		hypersample, rt_perspective,
		use_air, jitter,
		AmbientIntensity, lightmodel,
		eye_backoff );
	vls_cat( &fr->fr_cmd, buf );
	if( interactive )  vls_cat( &fr->fr_cmd, " -I");
	if( benchmark )  vls_cat( &fr->fr_cmd, " -B");
	if( aspect != 1.0 )  {
		sprintf(buf, " -V%g", aspect);
		vls_cat( &fr->fr_cmd, buf );
	}
	vls_cat( &fr->fr_cmd, ";" );

	bzero( fr->fr_servinit, sizeof(fr->fr_servinit) );

	fr->fr_start.tv_sec = fr->fr_end.tv_sec = 0;
	fr->fr_start.tv_usec = fr->fr_end.tv_usec = 0;
	fr->fr_nrays = 0;
	fr->fr_cpu = 0.0;

	/* Build work list */
	fr->fr_todo.li_forw = fr->fr_todo.li_back = &(fr->fr_todo);
	GET_LIST(lp);
	lp->li_frame = fr;
	lp->li_start = 0;
	lp->li_stop = fr->fr_width*fr->fr_height-1;	/* last pixel # */
	APPEND_LIST( lp, fr->fr_todo.li_back );
}

/*
 *			D O _ A _ F R A M E
 */
do_a_frame()
{
	register struct frame *fr;
	if( running )  {
		rt_log("already running, please wait or STOP\n");
		return;
	}
	if( file_fullname[0] == '\0' )  {
		rt_log("need LOAD before GO\n");
		return;
	}
	if( (fr = FrameHead.fr_forw) == &FrameHead )  {
		rt_log("No frames to do!\n");
		return;
	}
	running = 1;
}

/*
 *			C R E A T E _ O U T P U T F I L E N A M E
 *
 *  Build and save the file name.
 *  If the file will not be able to be written,
 *  signal error here.
 *
 *  Returns -
 *	-1	error, drop this frame
 *	 0	OK
 */
create_outputfilename( fr )
register struct frame	*fr;
{
	char	name[512];
	struct stat	sb;
	int		fd;

	/* Always create a file name to write into */
	if( outputfile )  {
		sprintf( name, "%s.%d", outputfile, fr->fr_number );
	} else {
		sprintf( name, "/usr/tmp/remrt%d", getpid() );
		(void)unlink(name);	/* remove any previous one */
	}
	fr->fr_filename = rt_strdup( name );

	/*
	 *  There are several cases:
	 *	file does not exist, create it
	 *	file exists, is not writable -- skip this frame
	 *	file exists, is writable -- eliminate all non-black pixels
	 *		from work-to-do queue
	 */
	if( access( fr->fr_filename, 0 ) < 0 )  {
		/* File does not yet exist */
		if( (fd = creat( fr->fr_filename, 0644 )) < 0 )  {
			/* Unable to create new file */
			perror( fr->fr_filename );
			return( -1 );		/* skip this frame */
		}
		(void)close(fd);
		return(0);			/* OK */
	}
	/* The file exists */
	if( access( fr->fr_filename, 2 ) < 0 )  {
		/* File exists, and is not writable, skip this frame */
		perror( fr->fr_filename );
		return( -1 );			/* skip this frame */
	}
	/* The file exists and is writable */
	if( stat( fr->fr_filename, &sb ) >= 0 && sb.st_size > 0 )  {
		/* The file has existing contents, dequeue all non-black
		 * pixels.
XXX
		 */
		rt_log("...need to scan %s for non-black pixels (deferred)\n",
			fr->fr_filename );
	}
	return(0);				/* OK */
}

/*
 *			F R A M E _ I S _ D O N E
 */
void
frame_is_done(fr)
register struct frame *fr;
{
	double	delta;

	(void)gettimeofday( &fr->fr_end, (struct timezone *)0 );
	delta = tvdiff( &fr->fr_end, &fr->fr_start);
	if( delta < 0.0001 )  delta=0.0001;
	rt_log("frame %d DONE: %g elapsed sec, %d rays/%g cpu sec\n",
		fr->fr_number,
		delta,
		fr->fr_nrays,
		fr->fr_cpu );
	rt_log("  RTFM=%g rays/sec (%g rays/cpu sec)\n",
		fr->fr_nrays/delta,
		fr->fr_nrays/fr->fr_cpu );

	/* Write-protect file, to prevent re-computation */
	if( chmod( fr->fr_filename, 0444 ) < 0 )
		perror( fr->fr_filename );

	destroy_frame( fr );
}

/*
 *			D E S T R O Y _ F R A M E
 */
void
destroy_frame( fr )
register struct frame	*fr;
{
	register struct list	*lp;

	/*
	 *  Need to remove any pending work.
	 *  What about work already assigned that will dribble in?
	 */
	while( (lp = fr->fr_todo.li_forw) != &(fr->fr_todo) )  {
		DEQUEUE_LIST( lp );
		FREE_LIST(lp);
	}

	DEQUEUE_FRAME(fr);
	FREE_FRAME(fr);
}

/*
 *			T H I S _ F R A M E _ D O N E
 *
 *  All work on this frame is done when there is no more work to be sent out,
 *  and none of the servers have outstanding assignments for this frame.
 *
 *  Returns -
 *	 0	not done
 *	!0	all done
 */
int
this_frame_done( fr )
register struct frame	*fr;
{
	register struct servers	*sp;
	register struct list	*lhp, *lp;

	if( fr->fr_todo.li_forw != &(fr->fr_todo) )
		return(1);		/* more work still to be sent */

	for( sp = &servers[0]; sp < &servers[MAXSERVERS]; sp++ )  {
		if( sp->sr_pc == PKC_NULL )  continue;
		lhp = &(sp->sr_work);
		for( lp = lhp->li_forw; lp != lhp; lp=lp->li_forw )  {
			if( fr != lp->li_frame )  continue;
			return(0);		/* nope, still more work */
		}
	}
	return(1);			/* All done */
}

/*
 *			A L L _ D O N E
 *
 *  All work is done when there is no more work to be sent out,
 *  and there is no more work pending in any of the servers.
 *
 *  Returns -
 *	 0	not done
 *	!0	all done
 */
int
all_done()
{
	register struct servers	*sp;
	register struct frame	*fr;

	for( fr = FrameHead.fr_forw; fr != &FrameHead; fr = fr->fr_forw)  {
		if( fr->fr_todo.li_forw == &(fr->fr_todo) )
			continue;
		return(0);		/* nope, still more work */
	}

	for( sp = &servers[0]; sp < &servers[MAXSERVERS]; sp++ )  {
		if( sp->sr_pc == PKC_NULL )  continue;
		if( sp->sr_work.li_forw == &(sp->sr_work) )  continue;
		return(0);		/* nope, still more work */
	}
	return(1);			/* All done */
}

/*
 *			S C H E D U L E
 *
 *  This routine is called by the main loop, after each batch of PKGs
 *  have arrived.
 *
 *  If there is work to do, and a free server, send work.
 *  One assignment will be given to each free server.  If there are
 *  servers that do not have a proper number of assignments (to ensure
 *  good pipelining), then additional passes will be made, untill all
 *  servers have a proper number of assignments.
 *
 *  There could be some difficulties with the linked lists changing
 *  while in an unsolicited pkg receive that happens when a pkg send
 *  operation is performed under here.  scheduler_going flag is
 *  insurance against unintended recursion, which should be enough.
 *
 *  When done, we leave the last finished frame around for attach/release.
 */
void
schedule( nowp )
struct timeval	*nowp;
{
	register struct servers *sp;
	register struct frame *fr;
	register struct frame *fr2;
	int		another_pass;
	static int	scheduler_going = 0;	/* recursion protection */
	int		ret;

	if( scheduler_going )  {
		/* recursion protection */
		return;
	}
	scheduler_going = 1;

	if( file_fullname[0] == '\0' )  goto out;

	/* Kick off any new servers */
	for( sp = &servers[0]; sp < &servers[MAXSERVERS]; sp++ )  {
		if( sp->sr_pc == PKC_NULL )  continue;
		if( sp->sr_state != SRST_VERSOK )  continue;

		/* advance this server to SRST_LOADING */
		send_loglvl(sp);

		/* An error may have caused connection to drop */
		if( sp->sr_pc == PKC_NULL )  continue;
		send_start(sp);
	}

	/* Look for finished frames */
	fr = FrameHead.fr_forw;
	while( fr && fr != &FrameHead )  {
		if( fr->fr_todo.li_forw != &(fr->fr_todo) )
			goto next_frame;	/* unassigned work remains */

		if( this_frame_done( fr ) )  {
			/* No servers are still working on this frame */
			fr2 = fr->fr_forw;
			frame_is_done( fr );	/* will dequeue */
			fr = fr2;
			continue;
		}
next_frame: ;
		fr = fr->fr_forw;
	}

	if( !running )  goto out;
	if( all_done() )  {
		running = 0;
		rt_log("REMRT:  All work done!\n");
		if( detached )  exit(0);
		goto out;
	}

	/* Keep assigning work until all servers are fully loaded */
top:
	for( fr = FrameHead.fr_forw; fr != &FrameHead; fr = fr->fr_forw)  {
		do {
			another_pass = 0;
			if( fr->fr_todo.li_forw == &(fr->fr_todo) )
				break;	/* none waiting here */

			/*
			 *  This loop attempts to make one assignment to
			 *  each of the workers, before looping back to
			 *  make additional assignments.
			 *  This should keep all workers evenly "stoked".
			 */
			for( sp = &servers[0]; sp < &servers[MAXSERVERS]; sp++ )  {
				if( sp->sr_pc == PKC_NULL )  continue;

				if( (ret = task_server(fr,sp,nowp)) < 0 )  {
					/* fr is no longer valid */
					goto top;
				} else if( ret > 0 )  {
					another_pass++;
				}
			}
		} while( another_pass > 0 );
	}
	/* No work remains to be assigned, or servers are stuffed full */
out:
	scheduler_going = 0;
	return;
}

#define ASSIGNMENT_TIME		5		/* desired seconds/result */
#define TARDY_SERVER_INTERVAL	(9*60)		/* max seconds of silence */
#define N_SERVER_ASSIGNMENTS	3		/* desired # of assignments */
#define MAX_LUMP		(8*1024)	/* max pixels/assignment */

/*
 *			T A S K _ S E R V E R
 *
 *  If this server is ready, and has fewer than N_SERVER_ASSIGNMENTS,
 *  dispatch one unit of work to it.
 *  The return code indicates if the server is sated or not.
 *
 *  Returns -
 *	-1	when 'fr' is no longer valid
 *	0	when this server winds up with a full workload
 *	1	when this server needs additional work
 */
int
task_server( fr, sp, nowp )
register struct servers	*sp;
register struct frame	*fr;
struct timeval		*nowp;
{
	register struct list	*lp;
	int			a, b;

	if( sp->sr_pc == PKC_NULL )  return(0);

	if( sp->sr_state != SRST_READY )
		return(0);	/* not running yet */

	/* Sanity check */
	if( fr->fr_filename == (char *)0 ||
	    fr->fr_filename[0] == '\0' )  {
		rt_log("task_server: fr %d: null filename!\n",
			fr->fr_number);
		destroy_frame( fr );	/* will dequeue */
		return(-1);		/* restart scan */
	}

	/*
	 *  Check for tardy server.
	 *  The assignments are estimated to take only ASSIGNMENT_TIME
	 *  seconds, so waiting many minutes is unreasonable.
	 *  However, if the picture "suddenly" became very complex,
	 *  or a system got very busy,
	 *  the estimate could be quite low.
	 *  This mechanism exists mostly to protect against servers that
	 *  go into "black hole" mode while REMRT is running unattended.
	 */
	if( server_q_len(sp) > 0 &&
	    sp->sr_sendtime.tv_sec > 0 && 
	    tvdiff( nowp, &sp->sr_sendtime ) > TARDY_SERVER_INTERVAL )  {
		rt_log("%s: *TARDY*\n", sp->sr_host->ht_name);
		drop_server( sp );
		return(0);	/* not worth giving another assignment */
	}

	if( server_q_len(sp) >= N_SERVER_ASSIGNMENTS )
		return(0);	/* plenty busy */

	if( (lp = fr->fr_todo.li_forw) == &(fr->fr_todo) )  {
		/*  No more work to assign in this frame,
		 *  on next pass, caller should advance to next frame.
		 */
		return(1);	/* need more work */
	}

	/*
	 *  Provide info about this frame, if this is the first assignment
	 *  of work from this frame to this server.
	 */
	if( fr->fr_servinit[sp->sr_index] == 0 )  {
		send_matrix( sp, fr );
		fr->fr_servinit[sp->sr_index] = 1;
		sp->sr_curframe = fr;
	}

	/* Special handling for the first assignment of each frame */
	if( fr->fr_start.tv_sec == 0 )  {
		/* Note when actual work on this frame was started */
		(void)gettimeofday( &fr->fr_start, (struct timezone *)0 );
	}

	/*
	 *  Make this assignment size based on weighted average of
	 *  past behavior.  Using rays/elapsed_sec metric takes into
	 *  account:
	 *	remote processor speed
	 *	remote processor load
	 *	available network bandwidth & load
	 *	local processing delays
	 */
	sp->sr_lump = ASSIGNMENT_TIME * sp->sr_w_elapsed;
	if( sp->sr_lump < 32 )  sp->sr_lump = 32;
	if( sp->sr_lump > MAX_LUMP )  sp->sr_lump = MAX_LUMP;

	a = lp->li_start;
	b = a+sp->sr_lump-1;	/* work increment */
	if( b >= lp->li_stop )  {
		b = lp->li_stop;
		DEQUEUE_LIST( lp );
		FREE_LIST( lp );
		lp = LIST_NULL;
	} else
		lp->li_start = b+1;

	/* Record newly allocated pixel range */
	GET_LIST(lp);
	lp->li_frame = fr;
	lp->li_start = a;
	lp->li_stop = b;
	APPEND_LIST( lp, &(sp->sr_work) );
	send_do_lines( sp, a, b, fr->fr_number );

	/* See if server will need more assignments */
	if( server_q_len(sp) < N_SERVER_ASSIGNMENTS )
		return(1);
	return(0);
}

/*
 *			S E R V E R _ Q _ L E N
 *
 *  Report number of assignments that a server has
 */
int
server_q_len( sp )
register struct servers	*sp;
{
	register struct list	*lp;
	register int		count;

	count = 0;
	for( lp = sp->sr_work.li_forw; lp != &(sp->sr_work); lp = lp->li_forw )  {
		count++;
	}
	return(count);
}

/*
 *			R E A D _ M A T R I X
 */
int
read_matrix( fp, fr )
register FILE *fp;
register struct frame *fr;
{
	register int i;
	char number[128];
	char	cmd[128];

	/* Visible part is from -1 to +1 in view space */
	if( fscanf( fp, "%s", number ) != 1 )  goto out;
	sprintf( cmd, "viewsize %s; eye_pt ", number );
	vls_cat( &(fr->fr_cmd), cmd );

	for( i=0; i<3; i++ )  {
		if( fscanf( fp, "%s", number ) != 1 )  goto out;
		sprintf( cmd, "%s ", number );
		vls_cat( &fr->fr_cmd, cmd );
	}

	sprintf( cmd, "; viewrot " );
	vls_cat( &fr->fr_cmd, cmd );

	for( i=0; i < 16; i++ )  {
		if( fscanf( fp, "%s", number ) != 1 )  goto out;
		sprintf( cmd, "%s ", number );
		vls_cat( &fr->fr_cmd, cmd );
	}
	vls_cat( &fr->fr_cmd, "; ");

	if( feof(fp) ) {
out:
		rt_log("EOF on frame file.\n");
		return(-1);
	}
	return(0);			/* OK */
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
	rt_log("ctl: unable to handle %s message: len %d",
		pc->pkc_switch[i].pks_title, pc->pkc_len);
	*buf = '*';
	(void)free(buf);
}

/*
 *			P H _ S T A R T
 *
 *  The server answers our MSG_START with various prints, etc,
 *  and then responds with a MSG_START in return, which indicates
 *  that he is ready to accept work now.
 */
void
ph_start(pc, buf)
register struct pkg_conn *pc;
char *buf;
{
	register struct servers *sp;

	sp = &servers[pc->pkc_fd];
	if( strcmp( PROTOCOL_VERSION, buf ) != 0 )  {
		rt_log("ERROR %s: protocol version mis-match\n",
			sp->sr_host->ht_name);
		rt_log(" local='%s'\n", PROTOCOL_VERSION );
		rt_log("remote='%s'\n", buf );
		drop_server( sp );
		if(buf) (void)free(buf);
		return;
	}
	if(buf) (void)free(buf);
	if( sp->sr_pc != pc )  {
		rt_log("unexpected MSG_START from fd %d\n", pc->pkc_fd);
		return;
	}
	if( sp->sr_state != SRST_LOADING )  {
		rt_log("MSG_START in state %d?\n", sp->sr_state);
		drop_server( sp );
		return;
	}
	sp->sr_state = SRST_READY;
}

/*
 *			P H _ P R I N T
 */
void
ph_print(pc, buf)
register struct pkg_conn *pc;
char *buf;
{
	if(print_on)
		rt_log("%s:%s", servers[pc->pkc_fd].sr_host->ht_name, buf );
	if(buf) (void)free(buf);
}

/*
 *			P H _ V E R S I O N
 */
void
ph_version(pc, buf)
register struct pkg_conn *pc;
char	*buf;
{
	register struct servers	*sp;

	sp = &servers[pc->pkc_fd];
	if( strcmp( PROTOCOL_VERSION, buf ) != 0 )  {
		rt_log("ERROR %s: protocol version mis-match\n",
			sp->sr_host->ht_name);
		rt_log("  local='%s'\n", PROTOCOL_VERSION );
		rt_log(" remote='%s'\n", buf );
		drop_server( sp );
	} else {
		if( sp->sr_state != SRST_NEW )  {
			rt_log("NOTE %s:  VERSION message unexpected\n",
				sp->sr_host->ht_name);
		}
		sp->sr_state = SRST_VERSOK;
	}
	if(buf) (void)free(buf);
}

/*
 *			P H _ C M D
 */
void
ph_cmd(pc, buf)
register struct pkg_conn *pc;
char	*buf;
{
	register struct servers	*sp;

	sp = &servers[pc->pkc_fd];
	rt_log("%s: cmd '%s'\n", sp->sr_host->ht_name, buf );
	(void)rt_do_cmd( (struct rt_i *)0, buf, cmd_tab );
	if(buf) (void)free(buf);
	drop_server( sp );
}

/*
 *			P H _ P I X E L S
 *
 *  When a scanline is received from a server, file it away.
 */
void
ph_pixels(pc, buf)
register struct pkg_conn *pc;
char *buf;
{
	register int		i;
	register struct servers	*sp;
	register struct frame	*fr;
	struct line_info	info;
	struct timeval		tvnow;
	int			npix;
	int			fd;
	int			cnt;

	(void)gettimeofday( &tvnow, (struct timezone *)0 );

	sp = &servers[pc->pkc_fd];
	if( (sp->sr_l_elapsed = tvdiff( &tvnow, &sp->sr_sendtime )) < 0.1 )
		sp->sr_l_elapsed = 0.1;

	i = struct_import( (stroff_t)&info, desc_line_info, buf );
	if( i < 0 || i != info.li_len )  {
		rt_log("struct_import error, %d, %d\n", i, info.li_len);
		goto out;
	}
#if 0
	rt_log("%s:fr=%d, %d..%d, ry=%d, cpu=%g, el=%g\n",
		sp->sr_host->ht_name,
		info.li_frame, info.li_startpix, info.li_endpix,
	info.li_nrays, info.li_cpusec, sp->sr_l_elapsed );
#endif

	/* XXX this is bogus -- assignments may have moved on to subsequent
	 * frames.  Need to search frame list */
	if( (fr = sp->sr_curframe) == FRAME_NULL )  {
		rt_log("%s: no current frame, discarding\n");
		goto out;
	}

	/* Don't be so trusting... */
	if( info.li_frame != fr->fr_number )  {
		rt_log("frame number mismatch, claimed=%d, actual=%d\n",
			info.li_frame, fr->fr_number );
		drop_server( sp );
		goto out;
	}
	if( info.li_startpix < 0 ||
	    info.li_endpix >= fr->fr_width*fr->fr_height )  {
		rt_log("pixel numbers out of range\n");
		drop_server( sp );
		goto out;
	}

	/* Stash pixels in bottom-to-top .pix order */
	npix = info.li_endpix - info.li_startpix + 1;
	i = npix*3;
	if( pc->pkc_len - info.li_len < i )  {
		rt_log("short scanline, s/b=%d, was=%d\n",
			i, pc->pkc_len - info.li_len );
		i = pc->pkc_len - info.li_len;
		drop_server( sp );
		goto out;
	}
	/* Write pixels into file */
	/* Later, can implement FD cache here */
	if( (fd = open( fr->fr_filename, 2 )) < 0 )  {
		perror( fr->fr_filename );
		/* Now what? Drop frame? */
	}
	if( lseek( fd, info.li_startpix*3L, 0 ) < 0 )  {
		perror( fr->fr_filename );
		/* Again, now what? */
		(void)close(fd);	/* prevent write */
	}
	if( (cnt = write( fd, buf+info.li_len, i )) != i )  {
		perror("write");
		rt_log("write s/b %d, got %d\n", i, cnt );
		/* Again, now what? */
	}
	(void)close(fd);

	/* If display attached, also draw it */
	if( fbp != FBIO_NULL )  {
		write_fb( buf + info.li_len, fr,
			info.li_startpix, info.li_endpix+1 );
	}

	/* Stash the statistics that came back */
	fr->fr_nrays += info.li_nrays;
	fr->fr_cpu += info.li_cpusec;
	sp->sr_l_el_rate = npix / sp->sr_l_elapsed;
#define BLEND	0.8
	sp->sr_w_elapsed = BLEND * sp->sr_w_elapsed +
		(1.0-BLEND) * sp->sr_l_el_rate;
	sp->sr_sendtime = tvnow;		/* struct copy */
	sp->sr_l_cpu = info.li_cpusec;
	sp->sr_s_cpu += info.li_cpusec;
	sp->sr_s_elapsed += sp->sr_l_el_rate;
	sp->sr_sq_elapsed += sp->sr_l_el_rate * sp->sr_l_el_rate;
	sp->sr_nsamp++;
	sp->sr_l_percent = info.li_percent;

	/* Remove from work list */
	list_remove( &(sp->sr_work), info.li_startpix, info.li_endpix );

out:
	if(buf) (void)free(buf);
}

/*
 *			L I S T _ R E M O V E
 *
 * Given pointer to head of list of ranges, remove the range that's done
 */
void
list_remove( lhp, a, b )
register struct list *lhp;
int		a, b;
{
	register struct list *lp;

	for( lp=lhp->li_forw; lp != lhp; lp=lp->li_forw )  {
		if( lp->li_start == a )  {
			if( lp->li_stop == b )  {
				DEQUEUE_LIST(lp);
				return;
			}
			lp->li_start = b+1;
			return;
		}
		if( lp->li_stop == b )  {
			lp->li_stop = a-1;
			return;
		}
		if( a > lp->li_stop )  continue;
		if( b < lp->li_start ) continue;
		/* Need to split range into two ranges */
		/* (start..a-1) and (b+1..stop) */
		{
			register struct list *lp2;
			rt_log("splitting range into (%d %d) (%d %d)\n",
				lp->li_start, a-1,
				b+1, lp->li_stop);
			GET_LIST(lp2);
			lp2->li_start = b+1;
			lp2->li_stop = lp->li_stop;
			lp->li_stop = a-1;
			APPEND_LIST( lp2, lp );
			return;
		}
	}
}

/*
 *			W R I T E _ F B
 *
 *  Buffer 'pp' contains pixels numbered 'a' through (not including) 'b'.
 *  Write them out, clipping them to the current screen.
 */
void
write_fb( pp, fr, a, b )
RGBpixel	*pp;
struct frame	*fr;
int		a;
int		b;
{
	register int	x, y;	/* screen coordinates of pixel 'a' */
	int	offset;		/* pixel offset beyond 'pp' */
	int	pixels_todo;	/* # of pixels in buffer to be written */
	int	write_len;	/* # of pixels to write on this scanline */
	int	len_to_eol;	/* # of pixels from 'a' to end of scanline */

	size_display(fr);

	x = a % fb_getwidth(fbp);
	y = (a / fb_getwidth(fbp)) % fb_getheight(fbp);
	pixels_todo = b - a;

	/* Simple case */
	if( fr->fr_width == fb_getwidth(fbp) )  {
		fb_write( fbp, x, y,
			pp, pixels_todo );
		return;
	}

	/* Hard case -- clip drawn region to the framebuffer */
	offset = 0;
	while( pixels_todo > 0 )  {
		write_len = fb_getwidth(fbp) - x;
		len_to_eol = fr->fr_width - x;
		if( write_len > pixels_todo )  write_len = pixels_todo;
		if( write_len > 0 )
			fb_write( fbp, x, y, pp+offset, write_len );
		offset += len_to_eol;
		a += len_to_eol;
		y = (y+1) % fb_getheight(fbp);
		x = 0;
		pixels_todo -= len_to_eol;
	}
}

/*
 *		R E P A I N T _ F B
 *
 *  Repaint the frame buffer from the stored file.
 *  Sort of a cheap "pix-fb", built in.
 */
void
repaint_fb( fr )
register struct frame *fr;
{
	register int	y;
	char		*line;
	int		nby;
	FILE		*fp;
	int		w;
	int		cnt;

	if( fbp == FBIO_NULL ) return;
	size_display(fr);

	if( fr->fr_filename == (char *)0 )  return;

	/* Draw the accumulated image */
	nby = 3*fr->fr_width;
	line = rt_malloc( nby, "scanline" );
	if( (fp = fopen( fr->fr_filename, "r" )) == NULL )  {
		perror( fr->fr_filename );
		return;
	}
	w = fr->fr_width;
	if( w > fb_getwidth(fbp) )  w = fb_getwidth(fbp);

	for( y=0; y < fr->fr_height; y++ )  {
		cnt = fread( line, nby, 1, fp );
		/* Write out even partial results, then quit */
		fb_write( fbp, 0, y, line, w );
		if( cnt != 1 )  break;
	}
}

/*
 *			I N I T _ F B
 */
int
init_fb(name)
char *name;
{
	if( fbp != FBIO_NULL )  fb_close(fbp);
	if( (fbp = fb_open( name, width, height )) == FBIO_NULL )  {
		rt_log("fb_open %d,%d failed\n", width, height);
		return(-1);
	}
	fb_wmap( fbp, COLORMAP_NULL );	/* Standard map: linear */
	cur_fbwidth = 0;
	return(0);
}

/*
 *			S I Z E _ D I S P L A Y
 */
void
size_display(fp)
register struct frame	*fp;
{
	if( cur_fbwidth == fp->fr_width )
		return;
	if( fbp == FBIO_NULL )
		return;
	if( fp->fr_width > fb_getwidth(fbp) )  {
		rt_log("Warning:  fb not big enough for %d pixels, display truncated\n", fp->fr_width );
		cur_fbwidth = fp->fr_width;
		return;
	}
	cur_fbwidth = fp->fr_width;

	fb_zoom( fbp, fb_getwidth(fbp)/fp->fr_width,
		fb_getheight(fbp)/fp->fr_height );
	/* center the view */
	fb_window( fbp, fp->fr_width/2, fp->fr_height/2 );
}

/*
 *			S E N D _ S T A R T
 */
void
send_start(sp)
register struct servers *sp;
{
	register struct ihost	*ihp;
	char	cmd[512];

	if( sp->sr_pc == PKC_NULL )  return;
	if( file_fullname[0] == '\0' || sp->sr_state != SRST_VERSOK )  return;

	ihp = sp->sr_host;
	switch( ihp->ht_where )  {
	case HT_CD:
		if( pkg_send( MSG_CD, ihp->ht_path, strlen(ihp->ht_path)+1, sp->sr_pc ) < 0 )
			drop_server(sp);
		sprintf( cmd, "%s %s", file_basename, object_list );
		break;
	case HT_CONVERT:
		/* Conversion was done when server was started */
		sprintf( cmd, "%s %s", file_basename, object_list );
		break;
	default:
		rt_log("send_start: ht_where=%d unimplemented\n", ihp->ht_where);
		drop_server(sp);
		return;
	}

	if( pkg_send( MSG_START, cmd, strlen(cmd)+1, sp->sr_pc ) < 0 )
		drop_server(sp);
	sp->sr_state = SRST_LOADING;
}

/*
 *			S E N D _ R E S T A R T
 */
void
send_restart(sp)
register struct servers *sp;
{
	if( sp->sr_pc == PKC_NULL )  return;

	if( pkg_send( MSG_RESTART, "", 0, sp->sr_pc ) < 0 )
		drop_server(sp);
	sp->sr_state = SRST_RESTART;
}

/*
 *			S E N D _ L O G L V L
 */
void
send_loglvl(sp)
register struct servers *sp;
{
	if( sp->sr_pc == PKC_NULL )  return;

	if( pkg_send( MSG_LOGLVL, print_on?"1":"0", 2, sp->sr_pc ) < 0 )
		drop_server(sp);
}

/*
 *			S E N D _ M A T R I X
 *
 *  Send current options, and the view matrix information.
 */
void
send_matrix(sp, fr)
struct servers *sp;
register struct frame *fr;
{
	if( sp->sr_pc == PKC_NULL )  return;
	if( pkg_send( MSG_MATRIX,
	    fr->fr_cmd.vls_str, fr->fr_cmd.vls_cur+1, sp->sr_pc
	    ) < 0 )
		drop_server(sp);
}

/*
 *			S E N D _ D O _ L I N E S
 */
void
send_do_lines( sp, start, stop, fr )
register struct servers *sp;
int		start;
int		stop;
int		fr;
{
	char obuf[128];

	if( sp->sr_pc == PKC_NULL )  return;

	sprintf( obuf, "%d %d %d", start, stop, fr );
	if( pkg_send( MSG_LINES, obuf, strlen(obuf)+1, sp->sr_pc ) < 0 )
		drop_server(sp);

	(void)gettimeofday( &sp->sr_sendtime, (struct timezone *)0 );
}

/*
 *			P R _ L I S T
 */
void
pr_list( lhp )
register struct list *lhp;
{
	register struct list *lp;

	for( lp = lhp->li_forw; lp != lhp; lp = lp->li_forw  )  {
		rt_log("\t%d..%d frame %d\n",
			lp->li_start, lp->li_stop,
			lp->li_frame->fr_number );
	}
}

void
mathtab_constant()
{
	/* Called on -B (benchmark) flag, by get_args() */
}

/*
 *			A D D _ H O S T
 *
 *  There are two message formats:
 *	HT_CD		host, port, rem_dir
 *	HT_CONVERT	host, port, rem_dir, loc_db, rem_db
 */
void
add_host( ihp )
struct ihost	*ihp;
{
	/* Send message to helper process */
	switch( ihp->ht_where )  {
	case HT_CD:
		fprintf( helper_fp,
			"%s %d %s\n",
			ihp->ht_name, pkg_permport, ihp->ht_path );
		break;
	case HT_CONVERT:
		if( file_fullname[0] == '\0' )  {
			rt_log("unable to add CONVERT host %s until database given\n",
				ihp->ht_name);
			return;
		}
		fprintf( helper_fp,
			"%s %d %s %s %s\n",
			ihp->ht_name, pkg_permport, ihp->ht_path,
			file_fullname, file_basename );
		break;
	default:
		rt_log("add_host:  ht_where=%d?\n", ihp->ht_where );
		break;
	}
	fflush( helper_fp );

	/* Wait briefly to try and catch the incomming connection,
	 * in case there are several of these spawned in a row.
	 */
	check_input(1);
}

#ifdef sgi
#	define RSH	"/usr/bsd/rsh"
#endif
#ifdef CRAY2
#	define RSH	"/usr/bin/remsh"
#endif
#ifndef RSH
#	define RSH	"/usr/ucb/rsh"
#endif

/*
 *			H O S T _ H E L P E R
 *
 *  This loop runs in the child process of the real REMRT, and is directed
 *  to initiate contact with new hosts via a one-way pipe from the parent.
 *  In some cases, starting RTSRV on the indicated host is sufficient;
 *  in other cases, the portable version of the database needs to be
 *  sent first.
 *
 *  For now, a limitation is that the local and remote databases are
 *  given the same name.  If relative path names are used, this should
 *  not be a problem, but this could be changed later.
 */
void
host_helper(fp)
FILE	*fp;
{
	char	line[512];
	char	cmd[128];
	char	host[128];
	char	loc_db[128];
	char	rem_db[128];
	char	rem_dir[128];
	int	port;
	int	cnt;
	int	i;
	int	pid;

	while(1)  {
		line[0] = '\0';
		(void)fgets( line, sizeof(line), fp );
		if( feof(fp) )  break;

		loc_db[0] = '\0';
		rem_db[0] = '\0';
		rem_dir[0] = '\0';
		cnt = sscanf( line, "%s %d %s %s %s",
			host, &port, rem_dir, loc_db, rem_db );
		if( cnt != 3 && cnt != 5 )  {
			rt_log("host_helper: cnt=%d, aborting\n", cnt);
			break;
		}

		if( cnt == 3 )  {
			sprintf(cmd,
				"cd %s; rtsrv %s %d",
				rem_dir, ourname, port );
#if 0
			rt_log("%s\n", cmd); fflush(stdout);
#endif

			pid = fork();
			if( pid == 0 )  {
				/* 1st level child */
				(void)close(0);
				for(i=3; i<40; i++)  (void)close(i);
				if( vfork() == 0 )  {
					/* worker Child */
					execl(
						RSH,
						"rsh", host,
						"-n", cmd, 0 );
					execlp(
						"rsh",
						"rsh", host,
						"-n", cmd, 0 );
					perror("rsh execl");
					_exit(0);
				}
				_exit(0);
			} else if( pid < 0 ) {
				perror("fork");
			} else {
				(void)wait(0);
			}
		} else {
			sprintf(cmd,
			 "g2asc<%s|%s %s \"cd %s; asc2g>%s; rtsrv %s %d\"",
				loc_db,
				RSH, host,
				rem_dir, rem_db,
				ourname, port );
#if 0
			rt_log("%s\n", cmd); fflush(stdout);
#endif

			pid = fork();
			if( pid == 0 )  {
				/* 1st level child */
				(void)close(0);
				for(i=3; i<40; i++)  (void)close(i);

				if( vfork() == 0 )  {
					/* worker Child */
					execl("/bin/sh", "remrt_sh", 
						"-c", cmd, 0);
					perror("/bin/sh");
					_exit(0);
				}
				_exit(0);
			} else if( pid < 0 ) {
				perror("fork");
			} else {
				(void)wait(0);
			}
		}
	}
}

/*
 *			S T A R T _ H E L P E R
 */
void
start_helper()
{
	int	fds[2];
	int	pid;

	if( pipe(fds) < 0 )  {
		perror("pipe");
		exit(1);
	}

	pid = fork();
	if( pid == 0 )  {
		/* Child process */
		FILE	*fp;

		(void)close(fds[1]);
		if( (fp = fdopen( fds[0], "r" )) == NULL )  {
			perror("fdopen");
			exit(3);
		}
		host_helper( fp );
		/* No more commands from parent */
		exit(0);
	} else if( pid < 0 )  {
		perror("fork");
		exit(2);
	}
	/* Parent process */
	if( (helper_fp = fdopen( fds[1], "w" )) == NULL )  {
		perror("fdopen");
		exit(4);
	}
	(void)close(fds[0]);
}

/*
 *			B U I L D _ S T A R T _ C M D
 */
void
build_start_cmd( argc, argv, startc )
int	argc;
char	**argv;
int	startc;
{
	register char	*cp;
	register int	i;
	int		len;

	if( startc+2 > argc )  {
		rt_log("build_start_cmd:  need file and at least one object\n");
		file_fullname[0] = '\0';
		return;
	}

	strncpy( file_fullname, argv[startc], sizeof(file_fullname) );

	/* Save last component of file name */
	if( (cp = strrchr( argv[startc], '/' )) != (char *)0 )  {
		(void)strncpy( file_basename, cp+1, sizeof(file_basename) );
	} else {
		(void)strncpy( file_basename, argv[startc], sizeof(file_basename) );
	}

	/* Build new object_list[] string */
	cp = object_list;
	for( i=startc+1; i < argc; i++ )  {
		if( i > startc+1 )  *cp++ = ' ';
		len = strlen( argv[i] );
		bcopy( argv[i], cp, len );
		cp += len;
	}
	*cp++ = '\0';
}

/*
 *			H O S T _ L O O K U P _ B Y _ H O S T E N T
 */
struct ihost *
host_lookup_by_hostent( addr, enter )
struct hostent	*addr;
int		enter;
{
	register struct ihost	*ihp;

	/* Search list for existing instance */
	for( ihp = HostHead; ihp != IHOST_NULL; ihp = ihp->ht_next )  {
		if( strcmp( ihp->ht_name, addr->h_name ) != 0 )
			continue;
		return( ihp );
	}
	if( enter == 0 )
		return( IHOST_NULL );

	/* If not found and enter==1, enter in host table w/defaults */
	/* Note: gethostbyxxx() routines keep stuff in a static buffer */
	return( make_default_host( addr->h_name ) );
}

/*
 *			M A K E _ D E F A U L T _ H O S T
 *
 *  Add a new host entry to the list of known hosts, with
 *  default parameters.
 *  This routine is used to handle unexpected volunteers.
 */
struct ihost *
make_default_host( name )
char	*name;
{
	register struct ihost	*ihp;

	GETSTRUCT( ihp, ihost );

	/* Make private copy of host name -- callers have static buffers */
	ihp->ht_name = rt_strdup( name );

	/* Default host parameters */
	ihp->ht_when = HT_PASSIVE;
	ihp->ht_where = HT_CONVERT;
	ihp->ht_path = "/tmp";

	/* Add to linked list of known hosts */
	ihp->ht_next = HostHead;
	HostHead = ihp;
	return(ihp);
}

struct ihost *
host_lookup_by_addr( from, enter )
struct sockaddr_in	*from;
int	enter;
{
	struct hostent	*addr;
	unsigned long	addr_tmp;
	char		name[64];

#ifdef CRAY
	addr_tmp = from->sin_addr;
	addr = gethostbyaddr(&addr_tmp, sizeof (struct in_addr),
		from->sin_family);
#else
	addr_tmp = from->sin_addr.s_addr;
	addr = gethostbyaddr(&from->sin_addr, sizeof (struct in_addr),
		from->sin_family);
#endif
	if( addr != NULL )
		return( host_lookup_by_hostent( addr, enter ) );

	/* Host name is not known */
	addr_tmp = ntohl(addr_tmp);
	sprintf( name, "%d.%d.%d.%d",
		(addr_tmp>>24) & 0xff,
		(addr_tmp>>16) & 0xff,
		(addr_tmp>> 8) & 0xff,
		(addr_tmp    ) & 0xff );
	if( enter == 0 )  {
		rt_log("%s: unknown host\n");
		return( IHOST_NULL );
	}

	/* Print up a hostent structure */
	return( make_default_host( name ) );
}

struct ihost *
host_lookup_by_name( name, enter )
char	*name;
int	enter;
{
	struct sockaddr_in	sockhim;
	struct hostent		*addr;

	/* Determine name to be found */
	if( isdigit( *name ) )  {
		/* Numeric */
		sockhim.sin_family = AF_INET;
#ifdef CRAY
		sockhim.sin_addr = inet_addr(name);
#else
		sockhim.sin_addr.s_addr = inet_addr(name);
#endif
		return( host_lookup_by_addr( &sockhim, enter ) );
	} else {
		addr = gethostbyname(name);
	}
	if( addr == NULL )  {
		rt_log("%s:  bad host\n", name);
		return( IHOST_NULL );
	}
	return( host_lookup_by_hostent( addr, enter ) );
}

/*** Commands ***/

cd_load( argc, argv )
int	argc;
char	**argv;
{
	register struct servers *sp;

	if( running )  {
		rt_log("Can't load while running!!\n");
		return;
	}

	/* Really ought to reset here, too */
	if(file_fullname[0] != '\0' )  {
		rt_log("Was loaded with %s, restarting all\n", file_fullname);
		for( sp = &servers[0]; sp < &servers[MAXSERVERS]; sp++ )  {
			if( sp->sr_pc == PKC_NULL )  continue;
			send_restart( sp );
		}
	}

	build_start_cmd( argc, argv, 1 );
}

cd_debug( argc, argv )
int	argc;
char	**argv;
{
	register struct servers *sp;
	int	len;

	len = strlen(argv[1])+1;
	for( sp = &servers[0]; sp < &servers[MAXSERVERS]; sp++ )  {
		if( sp->sr_pc == PKC_NULL )  continue;
		if( pkg_send( MSG_OPTIONS, argv[1], len, sp->sr_pc) < 0 )
			drop_server(sp);
	}
}

cd_f( argc, argv )
int	argc;
char	**argv;
{

	width = height = atoi( argv[1] );
	if( width < 4 || width > 16*1024 )
		width = 64;
	rt_log("width=%d, height=%d, takes effect after next MAT\n",
		width, height);
}

cd_hyper( argc, argv )
int	argc;
char	**argv;
{
	hypersample = atoi( argv[1] );
	rt_log("hypersample=%d, takes effect after next MAT\n", hypersample);
}

cd_bench( argc, argv )
int	argc;
char	**argv;
{
	benchmark = atoi( argv[1] );
	rt_log("Benchmark flag=%d, takes effect after next MAT\n", benchmark);
}

cd_persp( argc, argv )
int	argc;
char	**argv;
{
	rt_perspective = atof( argv[1] );
	if( rt_perspective < 0.0 )  rt_perspective = 0.0;
	rt_log("perspective angle=%g, takes effect after next MAT\n", rt_perspective);
}

cd_read( argc, argv )
int	argc;
char	**argv;
{
	register FILE *fp;

	if( (fp = fopen(argv[1], "r")) == NULL )  {
		perror(argv[1]);
		return;
	}
	source(fp);
	fclose(fp);
	rt_log("read file done\n");
}

source(fp)
FILE	*fp;
{
	while( !feof(fp) )  {
		/* do one command from file */
		interactive_cmd(fp);
		/* Without delay, see if anything came in */
		check_input(0);
	}
}

cd_detach( argc, argv )
int	argc;
char	**argv;
{
	detached = 1;
	clients &= ~(1<<0);	/* drop stdin */
	close(0);
}

cd_file( argc, argv )
int	argc;
char	**argv;
{
	if(outputfile)  rt_free(outputfile, "outputfile");
	outputfile = rt_strdup( argv[1] );
}

/*
 *			C D _ M A T
 *
 *  Read one specific matrix from an old-format eyepoint file.
 */
cd_mat( argc, argv )
int	argc;
char	**argv;
{
	register FILE *fp;
	register struct frame *fr;
	int	i;

	GET_FRAME(fr);
	prep_frame(fr);

	if( argc >= 3 )  {
		fr->fr_number = atoi(argv[2]);
	} else {
		fr->fr_number = 0;
	}
	if( (fp = fopen(argv[1], "r")) == NULL )  {
		perror(argv[1]);
		return;
	}

	/* Find the one desired frame */
	for( i=fr->fr_number; i>=0; i-- )  {
		if(read_matrix( fp, fr ) < 0 ) break;
	}
	fclose(fp);

	if( create_outputfilename( fr ) < 0 )  {
		FREE_FRAME(fr);
	} else {
		APPEND_FRAME( fr, FrameHead.fr_back );
	}
}

cd_movie( argc, argv )
int	argc;
char	**argv;
{
	register FILE	*fp;
	register struct frame	*fr;
	struct frame		dummy_frame;
	int		a,b;
	int		i;

	/* movie mat a b */
	if( running )  {
		rt_log("already running, please wait\n");
		return;
	}
	if( file_fullname[0] == '\0' )  {
		rt_log("need LOAD before MOVIE\n");
		return;
	}
	a = atoi( argv[2] );
	b = atoi( argv[3] );
	if( (fp = fopen(argv[1], "r")) == NULL )  {
		perror(argv[1]);
		return;
	}
	/* Skip over unwanted beginning frames */
	for( i=0; i<a; i++ )  {
		if(read_matrix( fp, &dummy_frame ) < 0 ) break;
	}
	for( i=a; i<b; i++ )  {
		GET_FRAME(fr);
		prep_frame(fr);
		fr->fr_number = i;
		if(read_matrix( fp, fr ) < 0 ) break;
		if( create_outputfilename( fr ) < 0 )  {
			FREE_FRAME(fr);
		} else {
			APPEND_FRAME( fr, FrameHead.fr_back );
		}
	}
	fclose(fp);
	rt_log("Movie ready\n");
}

cd_add( argc, argv )
int	argc;
char	**argv;
{
	register int i;
	struct ihost	*ihp;

	for( i=1; i<argc; i++ )  {
		if( (ihp = host_lookup_by_name( argv[i], 1 )) != IHOST_NULL )  {
			add_host( ihp );
		}
	}
}

cd_drop( argc, argv )
int	argc;
char	**argv;
{
	register struct servers *sp;

	sp = get_server_by_name( argv[1] );
	if( sp == SERVERS_NULL || sp->sr_pc == PKC_NULL )  return;
	drop_server(sp);
}

cd_restart( argc, argv )
int	argc;
char	**argv;
{
	register struct servers *sp;

	if( argc <= 1 )  {
		/* Restart all */
		rt_log("restarting all\n");
		for( sp = &servers[0]; sp < &servers[MAXSERVERS]; sp++ )  {
			if( sp->sr_pc == PKC_NULL )  continue;
			send_restart( sp );
		}
		return;
	}
	sp = get_server_by_name( argv[1] );
	if( sp == SERVERS_NULL || sp->sr_pc == PKC_NULL )  return;
	send_restart( sp );
	/* The real action takes place when he closes the conn */
}

cd_stop( argc, argv )
int	argc;
char	**argv;
{
	rt_log("no more scanlines being scheduled, done soon\n");
	running = 0;
}

cd_reset( argc, argv )
int	argc;
char	**argv;
{
	register struct frame *fr;

	if( running )  {
		rt_log("must STOP before RESET!\n");
		return;
	}
	do {
		fr = FrameHead.fr_forw;
		destroy_frame( fr );
	} while( FrameHead.fr_forw != &FrameHead );
}

cd_attach( argc, argv )
int	argc;
char	**argv;
{
	register struct frame *fr;
	char		*name;

	if( argc <= 1 )  {
		name = (char *)0;
	} else {
		name = argv[1];
	}
	if( init_fb(name) < 0 )  return;
	if( fbp == FBIO_NULL ) return;
	if( (fr = FrameHead.fr_forw) == &FrameHead )  return;

	repaint_fb( fr );
}

cd_release( argc, argv )
int	argc;
char	**argv;
{
	if(fbp != FBIO_NULL) fb_close(fbp);
	fbp = FBIO_NULL;
}

cd_frames( argc, argv )
int	argc;
char	**argv;
{
	register struct frame *fr;
	register int	i;

	/* Sumarize frames waiting */
	rt_log("Frames waiting:\n");
	for(fr=FrameHead.fr_forw; fr != &FrameHead; fr=fr->fr_forw) {
		rt_log("%5d\t", fr->fr_number);
		rt_log("width=%d, height=%d\n",
			fr->fr_width, fr->fr_height );
		if( fr->fr_filename )  rt_log(" %s\n", fr->fr_filename );

		rt_log("\tnrays = %d, cpu sec=%g\n", fr->fr_nrays, fr->fr_cpu);
		rt_log("       servinit: ");
		for( i=0; i<MAXSERVERS; i++ )
			rt_log("%d ", fr->fr_servinit[i]);
		rt_log("\n");
		pr_list( &(fr->fr_todo) );
		rt_log("cmd=%s\n", fr->fr_cmd.vls_str );
	}
}

cd_status( argc, argv )
int	argc;
char	**argv;
{
	register struct servers *sp;
    	int	num;

	if( file_fullname[0] == '\0' )
		rt_log("No model loaded yet\n");
	else
		rt_log("\n%s %s %s\n",
			running ? "RUNNING" : "loaded",
			file_fullname, object_list );

	if( fbp != FBIO_NULL )
		rt_log("Framebuffer is %s\n", fbp->if_name);
	else
		rt_log("No framebuffer\n");
	if( outputfile )
		rt_log("Output file: %s.###\n", outputfile );
	rt_log("Printing of remote messages is %s\n",
		print_on?"ON":"Off" );
    	rt_log("Listening at %s, port %d\n", ourname, pkg_permport);

	/* Print work assignments */
	rt_log("Servers:\n");
	for( sp = &servers[0]; sp < &servers[MAXSERVERS]; sp++ )  {
		if( sp->sr_pc == PKC_NULL )  continue;
		rt_log("  %2d  %s ", sp->sr_index, sp->sr_host->ht_name );
		switch( sp->sr_state )  {
		case SRST_NEW:
			rt_log("New"); break;
		case SRST_VERSOK:
			rt_log("Vers_OK"); break;
		case SRST_LOADING:
			rt_log("(Loading)"); break;
		case SRST_READY:
			rt_log("READY"); break;
		case SRST_RESTART:
			rt_log("--about to restart--"); break;
		default:
			rt_log("Unknown"); break;
		}
		if( sp->sr_curframe != FRAME_NULL )  {
			rt_log(" frame %d, assignments=%d\n",
				sp->sr_curframe->fr_number,
				server_q_len(sp) );
		}  else  {
			rt_log("\n");
		}
		num = sp->sr_nsamp<=0 ? 1 : sp->sr_nsamp;
		rt_log("\tlast:  elapsed=%g, cpu=%g, lump=%d\n",
			sp->sr_l_elapsed,
			sp->sr_l_cpu,
			sp->sr_lump );
		rt_log("\t avg:  elapsed=%gp/s, cpu=%g, weighted=%gp/s\n",
			(sp->sr_s_elapsed/num),
			sp->sr_s_cpu/num,
			sp->sr_w_elapsed );

		/* May want to print this only on debugging flag */
#if 0
		pr_list( &(sp->sr_work) );
#endif
	}
}

cd_clear( argc, argv )
int	argc;
char	**argv;
{
	if( fbp == FBIO_NULL )  return;
	fb_clear( fbp, PIXEL_NULL );
	cur_fbwidth = 0;
}

cd_print( argc, argv )
int	argc;
char	**argv;
{
	register struct servers *sp;

	if( argc > 1 )
		print_on = atoi(argv[1]);
	else
		print_on = !print_on;	/* toggle */

	for( sp = &servers[0]; sp < &servers[MAXSERVERS]; sp++ )  {
		if( sp->sr_pc == PKC_NULL )  continue;
		send_loglvl( sp );
	}
	rt_log("Printing of remote messages is %s\n",
		print_on?"ON":"Off" );
}

cd_go( argc, argv )
int	argc;
char	**argv;
{
	do_a_frame();
}

cd_wait( argc, argv )
int	argc;
char	**argv;
{
	struct timeval	now;

	clients &= ~(1<<fileno(stdin));
	while( running && clients && FrameHead.fr_forw != &FrameHead )  {
		check_input( 30 );	/* delay up to 30 secs */

		(void)gettimeofday( &now, (struct timezone *)0 );
		schedule( &now );
	}
	clients |= 1<<fileno(stdin);
}

cd_help( argc, argv )
int	argc;
char	**argv;
{
	register struct command_tab	*tp;

	for( tp = cmd_tab; tp->ct_cmd != (char *)0; tp++ )  {
		rt_log("%s %s\t\t%s\n",
			tp->ct_cmd, tp->ct_parms,
			tp->ct_comment );
	}
}

/*
 * host name always|night|passive cd|convert path
 */
cd_host( argc, argv )
int	argc;
char	**argv;
{
	register struct ihost	*ihp;

	if( argc < 5 )  {
		rt_log("Registered Host Table:\n");
		for( ihp = HostHead; ihp != IHOST_NULL; ihp=ihp->ht_next )  {
			rt_log("  %s ", ihp->ht_name);
			switch(ihp->ht_when)  {
			case HT_ALWAYS:
				rt_log("always ");
				break;
			case HT_NIGHT:
				rt_log("night ");
				break;
			case HT_PASSIVE:
				rt_log("passive ");
				break;
			default:
				rt_log("?when? ");
				break;
			}
			switch(ihp->ht_where)  {
			case HT_CD:
				rt_log("cd %s\n", ihp->ht_path);
				break;
			case HT_CONVERT:
				rt_log("convert %s\n", ihp->ht_path);
				break;
			default:
				rt_log("?where?\n");
				break;
			}
		}
		return;
	}

	if( (ihp = host_lookup_by_name( argv[1], 1 )) == IHOST_NULL )  return;

	/* When */
	if( strcmp( argv[2], "always" ) == 0 ) {
		ihp->ht_when = HT_ALWAYS;
	} else if( strcmp( argv[2], "night" ) == 0 )  {
		ihp->ht_when = HT_NIGHT;
	} else if( strcmp( argv[2], "passive" ) == 0 )  {
		ihp->ht_when = HT_PASSIVE;
	} else {
		rt_log("unknown 'when' string '%s'\n", argv[2]);
	}

	/* Where */
	if( strcmp( argv[3], "cd" ) == 0 )  {
		ihp->ht_where = HT_CD;
		ihp->ht_path = rt_strdup( argv[4] );
	} else if( strcmp( argv[3], "convert" ) == 0 )  {
		ihp->ht_where = HT_CONVERT;
		ihp->ht_path = rt_strdup( argv[4] );
	} else {
		rt_log("unknown 'where' string '%s'\n", argv[3] );
	}
}

struct command_tab cmd_tab[] = {
	"load",	"file obj(s)",	"specify database and treetops",
		cd_load,	3, 99,
	"read", "file",		"source a command file",
		cd_read,	2, 2,
	"file", "name",		"base name for storing frames",
		cd_file,	2, 2,
	"mat", "file [frame]",	"read one matrix from file",
		cd_mat,		2, 3,
	"movie", "file start_frame end_frame",	"read movie",
		cd_movie,	4, 4,
	"add", "host(s)",	"attach to hosts",
		cd_add,		2, 99,
	"drop", "host",		"drop first instance of 'host'",
		cd_drop,	2, 2,
	"restart", "[host]",	"restart one or all hosts",
		cd_restart,	1, 2,
	"go", "",		"start scheduling frames",
		cd_go,		1, 1,
	"stop", "",		"stop scheduling work",
		cd_stop,	1, 1,
	"reset", "",		"purge frame list of all work",
		cd_reset,	1, 1,
	"frames", "",		"summarize waiting frames",
		cd_frames,	1, 1,
	"stat", "",		"server status",
		cd_status,	1, 1,
	"detach", "",		"detatch from interactive keyboard",
		cd_detach,	1, 1,
	"host", "name always|night|passive cd|convert path", "register server host",
		cd_host,	1, 5,
	"wait", "",		"wait for current work assignment to finish",
		cd_wait,	1, 1,
	/* FRAME BUFFER */
	"attach", "[fb]",	"attach to frame buffer",
		cd_attach,	1, 2,
	"release", "",		"release frame buffer",
		cd_release,	1, 1,
	"clear", "",		"clear framebuffer",
		cd_clear,	1, 1,
	/* FLAGS */
	"debug", "options",	"set remote debugging flags",
		cd_debug,	2, 2,
	"f", "square_size",	"set square frame size",
		cd_f,		2, 2,
	"-H", "hypersample",	"set number of hypersamples/pixel",
		cd_hyper,	2, 2,
	"-B", "0|1",		"set benchmark flag",
		cd_bench,	2, 2,
	"p", "angle",		"set perspective angle (degrees) 0=ortho",
		cd_persp,	2, 2,
	"print", "[0|1]",	"set/toggle remote message printing",
		cd_print,	1, 2,
	/* HELP */
	"?", "",		"help",
		cd_help,	1, 1,
	(char *)0, (char *)0, (char *)0,
		0,		0, 0	/* END */
};

#ifdef CRAY2
gettimeofday( tvp, tzp )
struct timeval	*tvp;
struct timezone	*tzp;
{
	tvp->tv_sec = time( (long *)0 );
	tvp->tv_usec = 0;
}
#endif
