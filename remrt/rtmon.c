/*
 *			R T M O N . C
 *
 *  Remote ray-tracer system monitoring program.
 *  Hang around indefinitely to answer questions about available hardware,
 *  system load, raw processor speed, etc.,
 *  and initiate a server process when requested.
 *
 *  To simplify the hurdles of getting through the various
 *  telnet/rlogin/Kerberos/Secure-ID/SSH hurdles at different sites,
 *  so that a simple Tcl script stands a chance of getting servers started.
 *
 *  For security reasons, this program should not use Tcl (or any other
 *  interpreter.  However, output should be Tcl-friendly pure ASCII strings.
 *  This precludes use of LIBPKG on the connections.
 *
 *  Author -
 *	Michael John Muuss
 *  
 *  Source -
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5068  USA
 *  
 *  Distribution Status -
 *	Public Domain, Distribution Unlimited.
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
#include <sys/wait.h>

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

#if IRIX >= 6
# include <sched.h>
struct sched_param param;
#endif

#include "machine.h"
#include "vmath.h"
#include "bu.h"
#include "externs.h"

#include "./ihost.h"

int		debug = 0;

char		*our_hostname;

FILE		*ifp;
FILE		*ofp;

char		*start_dir;

static char	usage[] = "\
Usage:  rtmon [-d#]\n\
";

/*
 *			B U _ G E T _ 1 C P U _ S P E E D
 */
#if defined(IRIX)
# include <sys/utsname.h>
#endif
#define CPU_SPEED_FILE	"./1cpu.speeds"
fastf_t
bu_get_1cpu_speed()
{
#if defined(IRIX)
	struct utsname	ut;

#if 0
	if( uname(&ut) >= 0 )  {
		/* Key on type of CPU board, e.g. IP6 */
		return lookup( CPU_SPEED_FILE, ut.machine );
	}
#endif
#endif
	return 1.0;
}

/*
 *			S E N D _ S T A T U S
 */
void
send_status(fd)
int	fd;
{
	struct bu_vls	str;

	bu_vls_init(&str);

	our_hostname = get_our_hostname();	/* ihost.c */

	bu_vls_printf( &str, "hostname %s", our_hostname );
	bu_vls_printf( &str, " ncpu %d", bu_avail_cpus() );
	bu_vls_printf( &str, " pub_cpu %d", bu_get_public_cpus() );
	bu_vls_printf( &str, " load %g", bu_get_load_average() );
	bu_vls_printf( &str, " realt %d", bu_set_realtime() );
	bu_vls_printf( &str, " 1cpu_speed %g", bu_get_1cpu_speed() );

	bu_vls_putc( &str, '\n' );

	(void)write( fd, bu_vls_addr(&str), bu_vls_strlen(&str) );
	bu_vls_free( &str );
}

CONST char *rtnode_paths[] = {
	".",
	"/m/cad/.remrt.8d",
	"/n/vapor/m/cad/.remrt.8d",
	"/m/cad/.remrt.6d",
	"/n/vapor/m/cad/.remrt.6d",
	"/usr/brlcad/bin",
	NULL
};

/*
 *			R U N _ R T N O D E
 */
void
run_rtnode(fd, argv)
int	fd;
char	**argv;
{
	CONST char **pp;
	struct bu_vls	path;

	bu_vls_init(&path);

	argv[0] = "rtnode";

	/* Set up environment variables appropriately */
	if( access( "/m/cad/libtcl/library/.", X_OK ) == 0 )  {
		putenv( "TCL_LIBRARY=/m/cad/libtcl/library" );
		putenv( "TK_LIBRARY=/m/cad/libtk/library" );
	} else if( access( "/n/vapor/m/cad/libtcl/library/.", X_OK ) == 0 )  {
		putenv( "TCL_LIBRARY=/n/vapor/m/cad/libtcl/library" );
		putenv( "TK_LIBRARY=/n/vapor/m/cad/libtk/library" );
	} else {
		putenv( "TCL_LIBRARY=/usr/brlcad/libtcl/library" );
		putenv( "TK_LIBRARY=/usr/brlcad/libtk/library" );
	}

	for( pp = rtnode_paths; *pp != NULL; pp++ )  {
		bu_vls_strcpy( &path, *pp );
		bu_vls_strcat( &path, "/rtnode" );
		if( access( bu_vls_addr(&path), X_OK ) )  continue;

		if( fork() == 0 )  {
			/* Child process */
			close(fd);
			(void)execv( bu_vls_addr(&path), argv );
			perror("execv");
			/* If execv() succeeds, there is no return */
			/* Process will be reaped in wait3() in main() */
			exit(42);
		}
		fprintf(ofp, "OK %s\n", bu_vls_addr(&path) );
		fflush(ofp);
		return;
	}
	fprintf(ofp, "FAIL Unable to locate rtnode executable\n");
	fflush(ofp);
}

char *find_paths[] = {
	"start_dir",		/* replaced at runtime */
	"/m/cad",
	"/n/vapor/m/cad",
	"/vld/mike",
	"/vld/butler",
	"/r/mike",
	NULL
};

/*
 *			F I N D
 *
 *  Given a partial path specification (e.g. ../.db.6d/moss.g) of a file,
 *  rummage around and try to find it in likely places,
 *  and change into its directory, so that
 *  texture maps and stuff from the same directory will all be found.
 *  A nasty huristic, and fairly ARL-specific, but necessary.
 */
void
find(fd, argv)
int	fd;
char	**argv;
{
	char	**pp;
	char	*slash;

	find_paths[0] = start_dir;

	for( pp = find_paths; *pp != NULL; pp++ )  {
		if( chdir(*pp) < 0 )  {
			perror(*pp);
			continue;
		}
		if( access( argv[1], R_OK ) )  continue;

		/* OK, the path looks good, now get into the directory */
		if( (slash = strrchr( argv[1], '/' )) != NULL )  {
			*slash = '\0';
			if( chdir( argv[1] ) < 0 )  {
				perror(argv[1]);
				fprintf(ofp, "FAIL Unable to cd %s after cd %s\n", argv[1], *pp);
				fflush(ofp);
				return;
			}
			fprintf(ofp, "OK %s/%s %s\n", *pp, argv[1], slash+1 );
			fflush(ofp);
			return;
		}

		fprintf(ofp, "OK %s %s\n", *pp, argv[1] );
		fflush(ofp);
		return;
	}
	fprintf(ofp, "FAIL Unable to locate file %s\n", argv[1]);
	fflush(ofp);
}

/*
 *			S E R V E R _ P R O C E S S
 *
 *  Manage all conversation on one connection.
 *  There will be a separate process for each open connection.
 *  Each command will be acknowledged by a single line response.
 *  For security reasons, this does NOT want to be done via TCL.
 */
void
server_process(fd)
int	fd;
{
	struct bu_vls	str;
#define MAX_ARGS	50
	char	*argv[MAX_ARGS+2];

	ifp = fdopen( fd, "r" );
	ofp = fdopen( fd, "w" );
	bu_setlinebuf( ifp );
	bu_setlinebuf( ofp );

	bu_vls_init(&str);
	while( !feof(ifp ) )  {
		bu_vls_trunc( &str, 0 );
		if( bu_vls_gets( &str, ifp ) < 0 )  break;

		(void)bu_argv_from_string( argv, MAX_ARGS, bu_vls_addr(&str) );

		if( strcmp( argv[0], "status" ) == 0 )  {
			send_status(fd);
			continue;
		}
		if( strcmp( argv[0], "rtnode" ) == 0 )  {
			run_rtnode(fd, argv);
			continue;
		}
		if( strcmp( argv[0], "find" ) == 0 )  {
			find(fd, argv);
			continue;
		}
		if( strcmp( argv[0], "quit" ) == 0 )
			exit(0);
		fprintf( ofp, "ERROR Unknown command: %s\n", bu_vls_addr(&str) );
	}

	close(fd);
	exit(0);
}

/*
 *			G E T _ A R G S
 */
get_args( argc, argv )
register char **argv;
{
	register int c;

	while ( (c = getopt( argc, argv, "d:" )) != EOF )  {
		switch( c )  {
		case 'd':
			debug = atoi(optarg);
			break;
		default:		/* '?' */
			return(0);
		}
	}

	return(1);		/* OK */
}

/*
 *			M A I N
 */
main(argc, argv)
int	argc;
char	*argv[];
{
	struct sockaddr_in sinme;
	register struct servent *sp;
	struct	sockaddr *addr;			/* UNIX or INET addr */
	int	addrlen;			/* length of address */
	int	listenfd;
	int	on = 1;

	if ( !get_args( argc, argv ) ) {
		(void)fputs(usage, stderr);
		exit( 1 );
	}

	close(0);	/* shut stdin */

	/* Set real-time scheduler priority.  May need root privs. */
	bu_set_realtime();

	/* XXX Drop down to normal user privs */
	/* If "cmike" in /etc/passwd", use that, else use "mike" */
	setgid(42);
	setuid(53);

	/* Find current directory */
	if( (start_dir = getcwd(NULL,4096-8)) == NULL )  {
		perror("getcwd");
		start_dir = ".";
	}

	/* Hang a listen */
	bzero((char *)&sinme, sizeof(sinme));

	/* Determine port for service */
	sinme.sin_port = htons((unsigned short) 5353 );
	sinme.sin_family = AF_INET;
	addr = (struct sockaddr *) &sinme;
	addrlen = sizeof(struct sockaddr_in);

	if( (listenfd = socket(addr->sa_family, SOCK_STREAM, 0)) < 0 )  {
		perror( "rtmon: socket" );
		return -2;
	}

	if( addr->sa_family == AF_INET ) {
		if( setsockopt( listenfd, SOL_SOCKET, SO_REUSEADDR,
		    (char *)&on, sizeof(on) ) < 0 )  {
			perror( "rtmon: setsockopt SO_REUSEADDR" );
		}
	}

	if( bind(listenfd, addr, addrlen) < 0 )  {
		perror( "rtmon: bind" );
		close(listenfd);
		return -2;
	}

	if( listen(listenfd, 5) < 0 )  {
		perror( "rtmon: listen" );
		close(listenfd);
		return -2;
	}

	/*
	 *  For each new connection, fork a new exclusive server process.
	 *  Assume nothing about the state of the system here,
	 *  as this may be a very long-running (days, weeks) process.
	 *  Even the formal hostname may change in that period.
	 */
await:
	for(;;)  {
		struct timeval	tv;
		fd_set	fds;
		int	stat;
		int	val;

		/* Reap any dead children */
		(void)wait3( &stat, WNOHANG, NULL );

		FD_ZERO(&fds);
		FD_SET(listenfd, &fds);
		/* Hang in select() waiting for something to happen */
		tv.tv_sec = 10*60;	/* 10 minutes */
		tv.tv_usec = 0;

		val = select(32, &fds, (fd_set *)0, (fd_set *)0, &tv);
		if( val < 0 )  {
			perror("rtmon: select");
			return -4;
		}
		if( val==0 )  {
			/* At this point, ibits==0 */
			/* Select timed out */
			continue;
		}

		/* Accept any pending connections */
		if( FD_ISSET(listenfd, &fds) )  {
			auto struct sockaddr_in from;
			int s2;
			auto int fromlen = sizeof (from);
			int	pid;

			do  {
				s2 = accept(listenfd, (struct sockaddr *)&from, &fromlen);
				if (s2 < 0) {
					if(errno == EINTR)
						continue;
					perror("rtmon: accept" );
					goto await;
				}
			}  while( s2 < 0);

			if( (pid = fork()) == 0 )  {
				/* Child process */
				server_process(s2);
				exit(0);
			} else if( pid < 0 )  {
				perror("rtmon: fork");
			}
			close(s2);
		}
	}
}
