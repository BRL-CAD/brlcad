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
 *	This software is Copyright (C) 1985 by the United States Army.
 *	All rights reserved.
 */
#ifndef lint
static char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include <stdio.h>
#include <ctype.h>
#include <signal.h>
#include <errno.h>
#include <netdb.h>
#include <time.h>
#include <math.h>

#include <sys/types.h>
#include <sys/time.h>		/* for struct timeval */
#include <sys/socket.h>
#include <netinet/in.h>

#include "machine.h"
#include "vmath.h"
#include "raytrace.h"
#include "fb.h"
#include "pkg.h"

#include "./list.h"
#include "./rtsrv.h"
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
struct pkg_switch pkgswitch[] = {
	{ MSG_START, ph_start, "Startup ACK" },
	{ MSG_MATRIX, ph_default, "Set Matrix" },
	{ MSG_LINES, ph_default, "Compute lines" },
	{ MSG_END, ph_default, "End" },
	{ MSG_PIXELS, ph_pixels, "Pixels" },
	{ MSG_PRINT, ph_print, "Log Message" },
	{ 0, 0, (char *)0 }
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
	/* options */
	int		fr_width;	/* frame width (pixels) */
	int		fr_height;	/* frame height (pixels) */
	char		*fr_picture;	/* ptr to picture buffer */
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
	int		sr_started;
#define SRST_NEW	1		/* connected, no model loaded yet */
#define SRST_LOADING	2		/* loading, awaiting ready response */
#define SRST_READY	3		/* loaded, ready */
#define SRST_RESTART	4		/* about to restart */
	struct frame	*sr_curframe;	/* ptr to current frame */
	int		sr_index;	/* fr_servinit[] index */
	/* Timings */
	struct timeval	sr_sendtime;	/* time of last sending */
	double		sr_l_elapsed;	/* elapsed sec for last scanline */
	double		sr_w_elapsed;	/* weighted average of elapsed times */
	double		sr_s_elapsed;	/* sum of all elapsed times */
	double		sr_sq_elapsed;	/* sum of all elapsed times squared */
	double		sr_l_cpu;	/* cpu sec for last scanline */
	double		sr_s_cpu;	/* sum of all cpu times */
	int		sr_nlines;	/* number of lines summed over */
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
	char		*ht_path;
	struct ihost	*ht_next;
};
struct ihost		*HostHead;
#define IHOST_NULL	((struct ihost *)0)
struct ihost		*host_lookup_by_name();
struct ihost		*host_lookup_by_addr();
struct ihost		*host_lookup_by_hostent();

/* RT Options */
int		width = 64;
int		height = 64;
extern int	hypersample;
extern int	matflag;
double		perspective = 0;
int		benchmark = 0;
int		rdebug;
int		use_air = 0;
char		*outputfile;		/* output file name */
char		*framebuffer;
extern double	azimuth, elevation;
int		desiredframe;

struct rt_g	rt_g;

extern double	atof();

extern struct command_tab cmd_tab[];	/* given at end */

char	start_cmd[256];		/* contains file name & objects */

FILE	*helper_fp;		/* pipe to rexec helper process */
char	ourname[64];

extern char *malloc();

int	tcp_listen_fd;
extern int	pkg_permport;	/* libpkg/pkg_permserver() listen port */

/*
 *			E R R L O G
 *
 *  Log an error.  We supply the newline, and route to user.
 */
/* VARARGS */
void
errlog( str, a, b, c, d, e, f, g, h )
char *str;
{
	char buf[256];		/* a generous output line */

	(void)fprintf( stderr, str, a, b, c, d, e, f, g, h );
}

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
main(argc, argv)
int	argc;
char	**argv;
{
	register int i;
	register struct servers *sp;

	/* Random inits */
	gethostname( ourname, sizeof(ourname) );

	start_helper();

	FrameHead.fr_forw = FrameHead.fr_back = &FrameHead;
	for( sp = &servers[0]; sp < &servers[MAXSERVERS]; sp++ )  {
		sp->sr_work.li_forw = sp->sr_work.li_back = &sp->sr_work;
		sp->sr_pc = PKC_NULL;
	}

	/* Listen for our PKG connections */
	if( (tcp_listen_fd = pkg_permserver("rtsrv", "tcp", 8, errlog)) < 0 &&
	    (tcp_listen_fd = pkg_permserver("20210", "tcp", 8, errlog)) < 0 )
		exit(1);
	/* Now, pkg_permport has tcp port number */

	(void)signal( SIGPIPE, SIG_IGN );
	(void)signal( SIGINT, SIG_IGN );

	if( argc <= 1 )  {
		printf("Interactive REMRT listening at port %d\n", pkg_permport);
		clients = (1<<0);

		while(clients)  {
			check_input( 30 );	/* delay up to 30 secs */
		}
		/* Might want to see if any work remains, and if so,
		 * record it somewhere */
		printf("REMRT out of clients\n");
	} else {
		FILE	*fp;

		printf("Automatic REMRT listening at port %d\n", pkg_permport);
		clients = 0;
		clients = (1<<0);	/* for testing only */

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
		if( (fp = fopen(".remrtrc", "r")) == NULL )  {
			char	*home;
			char	path[128];

			if( (home = getenv("HOME")) == NULL )
				home = "/usr/brlcad/etc";
			sprintf( path, "%s/.remrtrc", home );
			fp = fopen( path, "r" );
		}
		if( fp != NULL )  {
			source(fp);
			fclose(fp);
		}

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
			APPEND_FRAME( fr, FrameHead.fr_back );
		} else {
			/* if -M, read RT script from stdin */
			clients = 0;
			eat_script( stdin );
		}
		cd_frames( 0, (char **)0 );

		/* Compute until no work remains */
		running = 1;
		while( FrameHead.fr_forw != &FrameHead )  {
			start_servers();
			check_input( 30 );	/* delay up to 30 secs */
		}
		printf("REMRT:  task accomplished\n");
	}
	exit(0);
}

/*
 *			C H E C K _ I N P U T
 */
check_input(waittime)
int waittime;
{
	static long ibits, obits, ebits;
	register struct list *lp;
	register int i;
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
		pc = pkg_getclient(tcp_listen_fd, pkgswitch, errlog, 1);
		if( pc != PKC_NULL && pc != PKC_ERROR )
			addclient(pc);
		ibits &= ~(1<<tcp_listen_fd);
	}
	/* Second, give priority to getting traffic off the network */
	for( i=0; i<NFD; i++ )  {
		register struct pkg_conn *pc;

		if( i == fileno(stdin) )  continue;
		if( i == tcp_listen_fd )  continue;
		if( !(ibits&(1<<i)) )  continue;
		pc = servers[i].sr_pc;
		if( pkg_get(pc) < 0 )
			dropclient(pc);
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
addclient(pc)
struct pkg_conn *pc;
{
	register struct servers *sp;
	struct ihost	*ihp;
	int		on = 1;
	int		options = 0;
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
		printf("abandoning this unknown server!!\n");
		close( fd );
		/* Maybe free the pkg struct? */
		return;
	}

	clients |= 1<<fd;

	sp = &servers[fd];
	bzero( (char *)sp, sizeof(*sp) );
	sp->sr_pc = pc;
	sp->sr_work.li_forw = sp->sr_work.li_back = &(sp->sr_work);
	sp->sr_started = SRST_NEW;
	sp->sr_curframe = FRAME_NULL;
	sp->sr_lump = 3;
	sp->sr_index = fd;
	sp->sr_host = ihp;

	printf("%s: ACCEPT\n", sp->sr_host->ht_name );

	if( start_cmd[0] != '\0' )  {
		send_start(sp);
		send_loglvl(sp);
	}
}

/*
 *			D R O P C L I E N T
 */
dropclient(pc)
register struct pkg_conn *pc;
{
	register struct list *lhp, *lp;
	register struct servers *sp;
	register struct frame *fr;
	int fd;

	fd = pc->pkc_fd;
	sp = &servers[fd];
	printf("REMRT closing fd %d %s\n", fd, sp->sr_host->ht_name);
	if( fd <= 3 || fd >= NFD )  {
		printf("That's unreasonable, forget it!\n");
		return;
	}
	pkg_close(pc);
	clients &= ~(1<<fd);
	sp->sr_pc = PKC_NULL;

	/* Need to remove any work in progress, and re-schedule */
	lhp = &(sp->sr_work);
	while( (lp = lhp->li_forw) != lhp )  {
		fr = lp->li_frame;
		fr->fr_servinit[sp->sr_index] = 0;

		DEQUEUE_LIST( lp );
		printf("requeueing fr%d %d..%d\n",
			fr->fr_number,
			lp->li_start, lp->li_stop);
		APPEND_LIST( lp, &(fr->fr_todo) );
		printf("ToDo:\n");
		pr_list(&(fr->fr_todo));
	}
	if(running) schedule();
}

/*
 *			S T A R T _ S E R V E R S
 *
 *  Scan the ihost table.  For all eligible hosts that don't
 *  presently have a server running, try to start a server.
 */
#define SERVER_CHECK_INTERVAL	(10*60)		/* seconds */
struct timeval	last_server_check_time;

start_servers()
{
	register struct ihost	*ihp;
	register struct servers	*sp;
	struct timeval	now;

	(void)gettimeofday( &now, (struct timezone *)0 );
	if( tvdiff( &now, &last_server_check_time ) < SERVER_CHECK_INTERVAL )
		return;

	printf("seeking servers to start\n");
	for( ihp = HostHead; ihp != IHOST_NULL; ihp = ihp->ht_next )  {

		/* Skip hosts which are not eligible */
		switch( ihp->ht_when )  {
		case HT_ALWAYS:
			break;
		case HT_NIGHT:
			if( is_night( &now ) )  break;
			printf("bypassing %s:  not night\n", ihp->ht_name);
			continue;
		default:
		case HT_PASSIVE:
			continue;
		}

		for( sp = &servers[0]; sp < &servers[MAXSERVERS]; sp++ )  {
			if( sp->sr_pc == PKC_NULL )  continue;
			if( sp->sr_host == ihp )  goto next_host;
		}
		/* This host is not busy, and is eligible */
		add_host( ihp->ht_name );

next_host:	continue;
	}
	last_server_check_time = now;		/* struct copy */
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
			printf("bad 'start' line\n");
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
		APPEND_FRAME( fr, FrameHead.fr_back );
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
		printf("string2int(%s) = %d?\n", str, ret );
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
interactive_cmd(fp)
FILE *fp;
{
	char buf[BUFSIZ];
	char obuf[256];
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
		printf("-> "); (void)fflush(stdout);
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
				dropclient(sp->sr_pc);
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
prep_frame(fr)
register struct frame *fr;
{
	register struct list *lp;
	char buf[BUFSIZ];

	/* Get local buffer for image */
	fr->fr_width = width;
	fr->fr_height = height;

	fr->fr_cmd.vls_cur = 0;	/* free existing? */
	if( benchmark )  vls_cat( &fr->fr_cmd, "opt -B;" );
	sprintf(buf, "opt -w%d -n%d -H%d -p%g;",
		fr->fr_width, fr->fr_height,
		hypersample, perspective );
	vls_cat( &fr->fr_cmd, buf );

	if( fr->fr_picture )  free(fr->fr_picture);
	fr->fr_picture = (char *)0;
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
	lp->li_stop = fr->fr_height-1;	/* last scanline # */
	APPEND_LIST( lp, fr->fr_todo.li_back );
}

/*
 *			D O _ A _ F R A M E
 */
do_a_frame()
{
	register struct frame *fr;
	if( running )  {
		printf("already running, please wait or STOP\n");
		return;
	}
	if( start_cmd[0] == '\0' )  {
		printf("need LOAD before GO\n");
		return;
	}
	if( (fr = FrameHead.fr_forw) == &FrameHead )  {
		printf("No frames to do!\n");
		return;
	}
	running = 1;
	schedule();
}

/*
 *			F R A M E _ I S _ D O N E
 */
frame_is_done(fr)
register struct frame *fr;
{
	char	name[256];
	double	delta;
	int	fd;
	int	cnt;

	(void)gettimeofday( &fr->fr_end, (struct timezone *)0 );
	delta = tvdiff( &fr->fr_end, &fr->fr_start);
	if( delta < 0.0001 )  delta=0.0001;
	printf("frame %d DONE: %g elapsed sec, %d rays/%g cpu sec\n",
		fr->fr_number,
		delta,
		fr->fr_nrays,
		fr->fr_cpu );
	printf("  RTFM=%g rays/sec (%g rays/cpu sec)\n",
		fr->fr_nrays/delta,
		fr->fr_nrays/fr->fr_cpu );

	if( outputfile )  {
		sprintf(name, "%s.%d", outputfile, fr->fr_number);
		cnt = fr->fr_width*fr->fr_height*3;
		if( (fd = creat(name, 0444)) > 0 )  {
			printf("Writing..."); fflush(stdout);
			if( write( fd, fr->fr_picture, cnt ) != cnt ) {
				perror(name);
				exit(3);
			} else
				printf(" %s\n", name);
			close(fd);
		} else {
			perror(name);
		}
	}
	if( fr->fr_picture )  {
		free(fr->fr_picture);
		fr->fr_picture = (char *)0;
	}
}

/*
 *			S C H E D U L E
 *
 *  If there is work to do, and a free server, send work
 *  When done, we leave the last finished frame around for attach/release.
 */
schedule()
{
	register struct list *lp;
	register struct servers *sp;
	register struct frame *fr;
	int a,b;
	int going;			/* number of servers still going */
	register struct frame *fr2;

	if( start_cmd[0] == '\0' )  return;
	going = 0;

	/* Look for finished frames */
	fr = FrameHead.fr_forw;
	while( fr != &FrameHead )  {
		if( (lp = fr->fr_todo.li_forw) != &(fr->fr_todo) )
			goto next_frame;	/* still work to be done */

		for( sp = &servers[0]; sp < &servers[MAXSERVERS]; sp++ )  {
			if( sp->sr_pc == PKC_NULL )  continue;
			if( sp->sr_curframe != fr )  continue;
			if( sp->sr_work.li_forw != &(sp->sr_work) )  {
				going++;
				goto next_frame;
			}
		}

		/* This server has just completed a frame */
		sp->sr_curframe = FRAME_NULL;
		fr2 = fr->fr_forw;
		DEQUEUE_FRAME(fr);
		frame_is_done( fr );
		FREE_FRAME(fr);
		fr = fr2;
		continue;

next_frame: ;
		fr = fr->fr_forw;
	}
	if( running == 0 || FrameHead.fr_forw == &FrameHead )  {
		/* No more work to send out */
		if( going > 0 )
			return;
		/* Nothing to do, and nobody still working */
		running = 0;
		printf("REMRT:  All work done!\n");
		if( detached )  exit(0);
		return;
	}

	/* Find first piece of work */
	if( !running )  return;
	for( fr = FrameHead.fr_forw; fr != &FrameHead; fr = fr->fr_forw)  {
top:
		if( (lp = fr->fr_todo.li_forw) == &(fr->fr_todo) )
			continue;	/* none waiting here */

		/*
		 *  Look for a server with fewer than XXX outstanding
		 *  assignments to dispatch work to
		 */
		for( sp = &servers[0]; sp < &servers[MAXSERVERS]; sp++ )  {
			if( sp->sr_pc == PKC_NULL )  continue;
			if( sp->sr_started == SRST_NEW )  {
				/*  advance to state 1 (loading) */
				send_start(sp);
				continue;
			}
			if( sp->sr_started != SRST_READY )
				continue;	/* not running yet */
			if( sp->sr_work.li_forw != &(sp->sr_work) )
				continue;	/* busy */

			if( fr->fr_servinit[sp->sr_index] == 0 )  {
				send_matrix( sp, fr );
				fr->fr_servinit[sp->sr_index] = 1;
				sp->sr_curframe = fr;
			}

			a = lp->li_start;
			b = a+sp->sr_lump-1;	/* work increment */
			if( b >= lp->li_stop )  {
				b = lp->li_stop;
				DEQUEUE_LIST( lp );
				FREE_LIST( lp );
				lp = LIST_NULL;
			} else
				lp->li_start = b+1;

			printf("fr%d %d..%d -> %s\n", fr->fr_number,
				a, b, sp->sr_host->ht_name);

			/* Record newly allocated scanlines */
			GET_LIST(lp);
			lp->li_frame = fr;
			lp->li_start = a;
			lp->li_stop = b;
			APPEND_LIST( lp, &(sp->sr_work) );
			if( fr->fr_start.tv_sec == 0 )  {
				(void)gettimeofday( &fr->fr_start, (struct timezone *)0 );
			}
			send_do_lines( sp, a, b );
			goto top;
		}
		if( sp >= &servers[MAXSERVERS] )  return;	/* no free servers */
	}
}

/*
 *			R E A D _ M A T R I X
 */
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
		printf("EOF on frame file.\n");
		return(-1);
	}
	return(0);	/* OK */
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
	errlog("ctl: unable to handle %s message: len %d",
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
	if(buf) (void)free(buf);
	if( sp->sr_pc != pc )  {
		printf("unexpected MSG_START from fd %d\n", pc->pkc_fd);
		return;
	}
	if( sp->sr_started != SRST_LOADING )  {
		printf("MSG_START in state %d?\n", sp->sr_started);
	}
	sp->sr_started = SRST_READY;
	printf("%s READY FOR WORK\n", sp->sr_host->ht_name);
	if(running) schedule();
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
		printf("%s:%s", servers[pc->pkc_fd].sr_host->ht_name, buf );
	if(buf) (void)free(buf);
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
	int line;
	register int i;
	register struct servers *sp;
	register struct frame *fr;
	struct line_info	info;
	struct timeval		tvnow;

	(void)gettimeofday( &tvnow, (struct timezone *)0 );

	sp = &servers[pc->pkc_fd];
	fr = sp->sr_curframe;

	i = struct_import( (stroff_t)&info, desc_line_info, buf );
	if( i < 0 || i != info.li_len )  {
		fprintf(stderr,"struct_inport error, %d, %d\n", i, info.li_len);
		return;
	}
fprintf(stderr,"PIXELS y=%d, rays=%d, cpu=%g\n", info.li_y, info.li_nrays, info.li_cpusec );

	/* Stash pixels in memory in bottom-to-top .pix order */
	if( fr->fr_picture == (char *)0 )  {
		i = fr->fr_width*fr->fr_height*3+3;
		printf("allocating %d bytes for image\n", i);
		fr->fr_picture = malloc( i );
		if( fr->fr_picture == (char *)0 )  {
			fprintf(stdout, "ph_pixels: malloc(%d) error\n",i);
			fprintf(stderr, "ph_pixels: malloc(%d) error\n",i);
			abort();
			exit(19);
		}
		bzero( fr->fr_picture, i );
	}
	i = fr->fr_width*3;
	if( pc->pkc_len - info.li_len < i )  {
		fprintf(stderr,"short scanline, s/b=%d, was=%d\n",
			i, pc->pkc_len - info.li_len );
		i = pc->pkc_len - info.li_len;
	}
	bcopy( buf+info.li_len, fr->fr_picture + info.li_y*fr->fr_width*3, i );

	/* If display attached, also draw it */
	if( fbp != FBIO_NULL )  {
		int maxx;
		maxx = i/3;
		if( maxx > fb_getwidth(fbp) )  {
			maxx = fb_getwidth(fbp);
			fprintf(stderr,"clipping fb scanline to %d\n", maxx);
		}
		size_display(fr);
		fb_write( fbp, 0, info.li_y%fb_getheight(fbp),
			buf+info.li_len, maxx );
	}
	if(buf) (void)free(buf);

	/* Stash the statistics that came back */
	fr->fr_nrays += info.li_nrays;
	fr->fr_cpu += info.li_cpusec;
	sp->sr_l_elapsed = tvdiff( &tvnow, &sp->sr_sendtime );
	sp->sr_w_elapsed = 0.9 * sp->sr_w_elapsed + 0.1 * sp->sr_l_elapsed;
	sp->sr_sendtime = tvnow;		/* struct copy */
	sp->sr_l_cpu = info.li_cpusec;
	sp->sr_s_cpu += info.li_cpusec;
	sp->sr_s_elapsed += sp->sr_l_elapsed;
	sp->sr_sq_elapsed += sp->sr_l_elapsed * sp->sr_l_elapsed;
	sp->sr_nlines++;

	/* Remove from work list */
	list_remove( &(sp->sr_work), info.li_y );
	if( running && sp->sr_work.li_forw == &(sp->sr_work) )
		schedule();
}

/*
 *			L I S T _ R E M O V E
 *
 * Given pointer to head of list of ranges, remove the item that's done
 */
list_remove( lhp, line )
register struct list *lhp;
{
	register struct list *lp;
	for( lp=lhp->li_forw; lp != lhp; lp=lp->li_forw )  {
		if( lp->li_start == line )  {
			if( lp->li_stop == line )  {
				DEQUEUE_LIST(lp);
				return;
			}
			lp->li_start++;
			return;
		}
		if( lp->li_stop == line )  {
			lp->li_stop--;
			return;
		}
		if( line > lp->li_stop )  continue;
		if( line < lp->li_start ) continue;
		/* Need to split range into two ranges */
		/* (start..line-1) and (line+1..stop) */
		{
			register struct list *lp2;
			printf("splitting range into %d %d %d\n", lp->li_start, line, lp->li_stop);
			GET_LIST(lp2);
			lp2->li_start = line+1;
			lp2->li_stop = lp->li_stop;
			lp->li_stop = line-1;
			APPEND_LIST( lp2, lp );
			return;
		}
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
		printf("fb_open %d,%d failed\n", width, height);
		return(-1);
	}
	fb_wmap( fbp, COLORMAP_NULL );	/* Standard map: linear */
	cur_fbwidth = 0;
	return(0);
}

/*
 *			S I Z E _ D I S P L A Y
 */
size_display(fp)
register struct frame	*fp;
{
	if( cur_fbwidth == fp->fr_width )
		return;
	if( fbp == FBIO_NULL )
		return;
	if( fp->fr_width > fb_getwidth(fbp) )  {
		printf("Warning:  fb not big enough for %d pixels, display truncated\n", fp->fr_width );
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
send_start(sp)
register struct servers *sp;
{
	if( start_cmd[0] == '\0' || sp->sr_started != SRST_NEW )  return;

	printf("Sending start to %s\n", sp->sr_host->ht_name);
	if( pkg_send( MSG_START, start_cmd, strlen(start_cmd)+1, sp->sr_pc ) < 0 )
		dropclient(sp->sr_pc);
	sp->sr_started = SRST_LOADING;
}

/*
 *			S E N D _ R E S T A R T
 */
send_restart(sp)
register struct servers *sp;
{
	if( pkg_send( MSG_RESTART, "", 0, sp->sr_pc ) < 0 )
		dropclient(sp->sr_pc);
	sp->sr_started = SRST_RESTART;
}

/*
 *			S E N D _ L O G L V L
 */
send_loglvl(sp)
register struct servers *sp;
{
	if( pkg_send( MSG_LOGLVL, print_on?"1":"0", 2, sp->sr_pc ) < 0 )
		dropclient(sp->sr_pc);
}

/*
 *			S E N D _ M A T R I X
 *
 *  Send current options, and the view matrix information.
 */
send_matrix(sp, fr)
struct servers *sp;
register struct frame *fr;
{

	if( pkg_send( MSG_MATRIX,
	    fr->fr_cmd.vls_str, fr->fr_cmd.vls_cur+1, sp->sr_pc
	    ) < 0 )
		dropclient(sp->sr_pc);
	printf("sent matrix to %s\n", sp->sr_host->ht_name);
}

/*
 *			S E N D _ D O _ L I N E S
 */
send_do_lines( sp, start, stop )
register struct servers *sp;
{
	char obuf[128];
	sprintf( obuf, "%d %d", start, stop );
	if( pkg_send( MSG_LINES, obuf, strlen(obuf)+1, sp->sr_pc ) < 0 )
		dropclient(sp->sr_pc);

	(void)gettimeofday( &sp->sr_sendtime, (struct timezone *)0 );
}

/*
 *			P R _ L I S T
 */
pr_list( lhp )
register struct list *lhp;
{
	register struct list *lp;

	for( lp = lhp->li_forw; lp != lhp; lp = lp->li_forw  )  {
		printf("\t%d..%d frame %d\n",
			lp->li_start, lp->li_stop,
			lp->li_frame->fr_number );
	}
}

mathtab_constant()
{
	/* Called on -B (benchmark) flag, by get_args() */
}

add_host( host )
char	*host;
{
	/* Send message to helper process */
	fprintf( helper_fp, "%s %d ", host, pkg_permport );
	fflush( helper_fp );

	/* Wait briefly to try and catch the incomming connection,
	 * in case there are several of these spawned in a row.
	 */
	check_input(1);
}

start_helper()
{
	int	fds[2];
	int	pid;
	int	i;

	if( pipe(fds) < 0 )  {
		perror("pipe");
		exit(1);
	}

	pid = fork();
	if( pid == 0 )  {
		/* Child process */
		FILE	*fp;
		char	host[128];
		int	port;

		(void)close(fds[1]);
		if( (fp = fdopen( fds[0], "r" )) == NULL )  {
			perror("fdopen");
			exit(3);
		}
		while( !feof(fp) )  {
			char cmd[128];

			if( fscanf( fp, "%s %d", host, &port ) != 2 )
				break;
			/* */
			sprintf(cmd,
				"cd cad/remrt; rtsrv %s %d",
				ourname, port );
			if( fork() == 0 )  {
				/* 1st level child */
				(void)close(0);
				for(i=3; i<40; i++)  (void)close(i);
				if( vfork() == 0 )  {
					/* worker Child */
					execl(
#ifdef sgi
						"/usr/bsd/rsh",
#else
						"/usr/ucb/rsh",
#endif
						"rsh", host,
						"-n", cmd, 0 );
					perror("execl");
					_exit(0);
				}
				_exit(0);
			} else {
				(void)wait(0);
			}

		}
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

build_start_cmd( argc, argv, startc )
int	argc;
char	**argv;
int	startc;
{
	register char	*cp;
	register int	i;
	int		len;

	/* Build new start_cmd[] string */
	cp = start_cmd;
	for( i=startc; i < argc; i++ )  {
		if( i > startc )  *cp++ = ' ';
		len = strlen( argv[i] );
		bcopy( argv[i], cp, len );
		cp += len;
	}
	*cp++ = '\0';
}

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
	GETSTRUCT( ihp, ihost );

	/* Duplicate those fields of interst -- 
	 * gethostbyxxx() routines keep stuff in a static buffer
	 */
	ihp->ht_name = rt_strdup( addr->h_name );

	/* Default host parameters */
	ihp->ht_when = HT_PASSIVE;
	ihp->ht_where = HT_CD;
	ihp->ht_path = ".";

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

#ifdef CRAY
	addr_tmp = from->sin_addr;
#else
	addr_tmp = from->sin_addr.s_addr;
#endif
	addr = gethostbyaddr(&from->sin_addr, sizeof (struct in_addr),
		from->sin_family);
	if( addr == NULL )  {
		/* Potentially, print up a hostent structure here */
		addr_tmp = ntohl(addr_tmp);
		printf("%d.%d.%d.%d: unknown host\n",
			(addr_tmp>>24) & 0xff,
			(addr_tmp>>16) & 0xff,
			(addr_tmp>> 8) & 0xff,
			(addr_tmp    ) & 0xff );
		return( IHOST_NULL );
	}
	return( host_lookup_by_hostent( addr, enter ) );
}

struct ihost *
host_lookup_by_name( name, enter )
char	*name;
int	enter;
{
	struct sockaddr_in	sin;
	struct hostent		*addr;

	/* Determine name to be found */
	if( isdigit( *name ) )  {
		/* Numeric */
		sin.sin_family = AF_INET;
#ifdef CRAY
		sin.sin_addr = inet_addr(name);
		addr = gethostbyaddr( &addr_tmp, sizeof(struct in_addr),
			sin.sin_family );
#else
		sin.sin_addr.s_addr = inet_addr(name);
		addr = gethostbyaddr( &sin.sin_addr, sizeof(struct in_addr),
			sin.sin_family );
#endif
	} else {
		addr = gethostbyname(name);
	}
	if( addr == NULL )  {
		printf("%s:  bad host\n", name);
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
		printf("Can't load while running!!\n");
		return;
	}

	/* Really ought to reset here, too */
	if(start_cmd[0] != '\0' )  {
		printf("Was loaded with %s, restarting all\n", start_cmd);
		for( sp = &servers[0]; sp < &servers[MAXSERVERS]; sp++ )  {
			if( sp->sr_pc == PKC_NULL )  continue;
			send_restart( sp );
		}
	}

	build_start_cmd( argc, argv, 1 );

	/* Start any idle servers */
	for( sp = &servers[0]; sp < &servers[MAXSERVERS]; sp++ )  {
		if( sp->sr_pc == PKC_NULL )  continue;
		if( sp->sr_started != SRST_NEW )  continue;
		send_start(sp);
	}
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
		(void)pkg_send( MSG_OPTIONS, argv[1], len, sp->sr_pc);
	}
}

cd_f( argc, argv )
int	argc;
char	**argv;
{

	width = height = atoi( argv[1] );
	if( width < 4 || width > 16*1024 )
		width = 64;
	printf("width=%d, height=%d, takes effect after next MAT\n",
		width, height);
}

cd_hyper( argc, argv )
int	argc;
char	**argv;
{
	hypersample = atoi( argv[1] );
	printf("hypersample=%d, takes effect after next MAT\n", hypersample);
}

cd_bench( argc, argv )
int	argc;
char	**argv;
{
	benchmark = atoi( argv[1] );
	printf("Benchmark flag=%d, takes effect after next MAT\n", benchmark);
}

cd_persp( argc, argv )
int	argc;
char	**argv;
{
	perspective = atof( argv[1] );
	if( perspective < 0.0 )  perspective = 0.0;
	printf("perspective angle=%g, takes effect after next MAT\n", perspective);
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
	printf("read file done\n");
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
	outputfile = rt_strdup( argv[1] );
	printf("frames will be recorded in %s.###\n", outputfile);
}

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
	for( i=fr->fr_number; i>=0; i-- )
		if(read_matrix( fp, fr ) < 0 ) break;
	fclose(fp);

	APPEND_FRAME( fr, FrameHead.fr_back );
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
		printf("already running, please wait\n");
		return;
	}
	if( start_cmd[0] == '\0' )  {
		printf("need LOAD before MOVIE\n");
		return;
	}
	a = atoi( argv[2] );
	b = atoi( argv[3] );
	if( (fp = fopen(argv[1], "r")) == NULL )  {
		perror(argv[1]);
		return;
	}
	/* Skip over unwanted beginning frames */
	for( i=0; i<a; i++ )
		if(read_matrix( fp, &dummy_frame ) < 0 ) break;
	for( i=a; i<b; i++ )  {
		GET_FRAME(fr);
		prep_frame(fr);
		fr->fr_number = i;
		if(read_matrix( fp, fr ) < 0 ) break;
		APPEND_FRAME( fr, FrameHead.fr_back );
	}
	fclose(fp);
	printf("Movie ready\n");
}

cd_add( argc, argv )
int	argc;
char	**argv;
{
	register int i;

	for( i=1; i<argc; i++ )  {
		add_host( argv[i] );
	}
}

cd_drop( argc, argv )
int	argc;
char	**argv;
{
	register struct servers *sp;

	sp = get_server_by_name( argv[1] );
	if( sp == SERVERS_NULL || sp->sr_pc == PKC_NULL )  return;
	dropclient(sp->sr_pc);
}

cd_restart( argc, argv )
int	argc;
char	**argv;
{
	register struct servers *sp;

	if( argc <= 1 )  {
		/* Restart all */
		printf("restarting all\n");
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
	printf("no more scanlines being scheduled, done soon\n");
	running = 0;
}

cd_reset( argc, argv )
int	argc;
char	**argv;
{
	register struct frame *fr;
	register struct list *lp;

	if( running )  {
		printf("must STOP before RESET!\n");
		return;
	}
	for( fr = FrameHead.fr_forw; fr != &FrameHead; fr=fr->fr_forw )  {
		if( fr->fr_picture )  free(fr->fr_picture);
		fr->fr_picture = (char *)0;
		/* Need to remove any pending work,
		 * work already assigned will dribble in.
		 */
		while( (lp = fr->fr_todo.li_forw) != &(fr->fr_todo) )  {
			DEQUEUE_LIST( lp );
			FREE_LIST(lp);
		}
		/* We will leave cleanups to schedule() */
	}
}

cd_attach( argc, argv )
int	argc;
char	**argv;
{
	register struct frame *fr;
	register int	i;
	int		maxx;
	char		*name;

	if( argc <= 1 )  {
		name = (char *)0;
	} else {
		name = argv[1];
	}
	if( init_fb(name) < 0 )  return;

	if( (fr = FrameHead.fr_forw) == &FrameHead )  return;

	/* Draw the accumulated image */
	if( fr->fr_picture == (char *)0 )  return;
	size_display(fr);
	if( fbp == FBIO_NULL ) return;

	/* Trim to what can be drawn */
	maxx = fr->fr_width;
	if( maxx > fb_getwidth(fbp) )
		maxx = fb_getwidth(fbp);
	for( i=0; i<fr->fr_height; i++ )  {
		fb_write( fbp, 0, i%fb_getheight(fbp),
			fr->fr_picture + i*fr->fr_width*3,
			maxx );
	}
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
	printf("Frames waiting:\n");
	for(fr=FrameHead.fr_forw; fr != &FrameHead; fr=fr->fr_forw) {
		printf("%5d\t", fr->fr_number);
		printf("width=%d, height=%d\n",
			fr->fr_width, fr->fr_height );
		if( fr->fr_picture )  printf(" (Pic)");
		printf("\n");
		printf("\tnrays = %d, cpu sec=%g\n", fr->fr_nrays, fr->fr_cpu);
		printf("       servinit: ");
		for( i=0; i<MAXSERVERS; i++ )
			printf("%d ", fr->fr_servinit[i]);
		printf("\n");
		pr_list( &(fr->fr_todo) );
		printf("cmd=%s\n", fr->fr_cmd.vls_str );
	}
}

cd_status( argc, argv )
int	argc;
char	**argv;
{
	register struct servers *sp;
    	int	num;

	if( start_cmd[0] == '\0' )
		printf("No model loaded yet\n");
	else
		printf("\n%s %s\n",
			running ? "RUNNING" : "loaded",
			start_cmd );

	if( fbp != FBIO_NULL )
		printf("Framebuffer is %s\n", fbp->if_name);
	else
		printf("No framebuffer\n");
	if( outputfile )
		printf("Output file: %s.###\n", outputfile );
	printf("Printing of remote messages is %s\n",
		print_on?"ON":"Off" );
    	printf("Listening at %s, port %d\n", ourname, pkg_permport);

	/* Print work assignments */
	printf("Servers:\n");
	for( sp = &servers[0]; sp < &servers[MAXSERVERS]; sp++ )  {
		if( sp->sr_pc == PKC_NULL )  continue;
		printf("  %2d  %s ", sp->sr_index, sp->sr_host->ht_name );
		switch( sp->sr_started )  {
		case SRST_NEW:
			printf("Idle"); break;
		case SRST_LOADING:
			printf("(Loading)"); break;
		case SRST_READY:
			printf("Ready"); break;
		case SRST_RESTART:
			printf("--about to restart--"); break;
		default:
			printf("Unknown"); break;
		}
		if( sp->sr_curframe != FRAME_NULL )
			printf(" frame %d\n", sp->sr_curframe->fr_number);
		else
			printf("\n");
		num = sp->sr_nlines<=0 ? 1 : sp->sr_nlines;
		printf("\tlast:  elapsed=%g, cpu=%g\n",
			sp->sr_l_elapsed,
			sp->sr_l_cpu );
		printf("\t avg:  elapsed=%g, weighted=%g, cpu=%g clump=%d\n",
			sp->sr_s_elapsed/num,
			sp->sr_w_elapsed,
			sp->sr_s_cpu/num,
			sp->sr_lump );

		pr_list( &(sp->sr_work) );
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
	printf("Printing of remote messages is %s\n",
		print_on?"ON":"Off" );
}

cd_go( argc, argv )
int	argc;
char	**argv;
{
	do_a_frame();
}

cd_help( argc, argv )
int	argc;
char	**argv;
{
	register struct command_tab	*tp;

	for( tp = cmd_tab; tp->ct_cmd != (char *)0; tp++ )  {
		printf("%s %s\t\t%s\n",
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
		printf("Registered Host Table:\n");
		for( ihp = HostHead; ihp != IHOST_NULL; ihp=ihp->ht_next )  {
			printf("%s ", ihp->ht_name);
			switch(ihp->ht_when)  {
			case HT_ALWAYS:
				printf("always ");
				break;
			case HT_NIGHT:
				printf("night ");
				break;
			case HT_PASSIVE:
				printf("passive ");
				break;
			default:
				printf("?when? ");
				break;
			}
			switch(ihp->ht_where)  {
			case HT_CD:
				printf("cd %s\n", ihp->ht_path);
				break;
			case HT_CONVERT:
				printf("convert %s\n", ihp->ht_path);
				break;
			default:
				printf("?where?\n");
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
		printf("unknown 'when' string '%s'\n", argv[2]);
	}

	/* Where */
	if( strcmp( argv[3], "cd" ) == 0 )  {
		ihp->ht_where = HT_CD;
		ihp->ht_path = rt_strdup( argv[4] );
	} else if( strcmp( argv[3], "convert" ) == 0 )  {
		ihp->ht_where = HT_CONVERT;
		ihp->ht_path = rt_strdup( argv[4] );
	} else {
		printf("unknown 'where' string '%s'\n", argv[3] );
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
