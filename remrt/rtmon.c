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
#include <fcntl.h>
#include <pwd.h>
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
int		main_argc;
char		**main_argv;

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
	FILE	*fp;
	double	d;
	/* File format:  scale-factor TAB #Mhz SPACE IP99 */
	if( (fp = popen("grep \"`hinv | head -1 | cut '-d ' -f 2,4`\" 1cpu.speeds", "r")) != NULL )  {
		d = 0.9876;
		if( fscanf( fp, "%lf", &d ) != 1 )
			return 1.0;
		fclose(fp);
		return d;
	}
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

CONST char *prog_paths[] = {
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
run_prog(fd, argc, argv, program)
int	fd;
int	argc;
char	**argv;
char	*program;	/* name of program to run */
{
	CONST char **pp;
	struct bu_vls	path;

	bu_vls_init(&path);

	argv[0] = program;

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

	for( pp = prog_paths; *pp != NULL; pp++ )  {
		int	stat;
		int	pid;

		bu_vls_strcpy( &path, *pp );
		bu_vls_putc( &path, '/' );
		bu_vls_strcat( &path, program );
		if( access( bu_vls_addr(&path), X_OK ) )  continue;

		/* Reap any prior dead children.  Sanity */
		(void)wait3( &stat, WNOHANG, NULL );

		if( (pid = fork()) == 0 )  {
			/* Child process */
			close(fd);
			for(fd=0; fd<=32; fd++) (void)close(fd);
			if( open("/dev/null", 0) < 0 ) perror("/dev/null fd0");
			if( open("/dev/null", 1) < 0 ) perror("/dev/null fd1");
			if( open("/dev/null", 1) < 0 ) perror("/dev/null fd2");
			(void)execv( bu_vls_addr(&path), argv );
			/* If execv() succeeds, there is no return and
			 * Process will be reaped in wait3() in main(),
			 * else failure will be reaped below.
			 */
			exit(42);
		}
		sleep(1);	/* Give exec enough time to fail */
		if( wait3( &stat, WNOHANG, NULL ) == pid )  {
			/* It died. */
			/* Be robust in the face of 'wrong architecture' errors. */
			if( WIFEXITED(stat) )  {
				if( WEXITSTATUS(stat) == 42 )   {
					continue;	/* Try another path */
				}
				if( WEXITSTATUS(stat) == 0 )  {
					/* normal exit, probably detatched */
					goto ok;
				}
			}
			bu_vls_putc(&path, ' ');
			bu_vls_from_argv(&path, argc, argv );
			fprintf(ofp, "FAIL %s died with status=x%x\n",
				bu_vls_addr(&path), stat);
			fflush(ofp);
			bu_vls_free(&path);
			return;
		}

ok:
		bu_vls_putc(&path, ' ');
		bu_vls_from_argv(&path, argc, argv );
		fprintf(ofp, "OK %s\n", bu_vls_addr(&path) );
		fflush(ofp);
		bu_vls_free(&path);
		return;
	}
	fprintf(ofp, "FAIL Unable to find executable %s.\n", program);
	fflush(ofp);
	bu_vls_free(&path);
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
	int		argc;
#define MAX_ARGS	50
	char	*argv[MAX_ARGS+2];
	struct ihost	*him;

	ifp = fdopen( fd, "r" );
	ofp = fdopen( fd, "w" );
	bu_setlinebuf( ifp );
	bu_setlinebuf( ofp );

	our_hostname = get_our_hostname();	/* ihost.c */

	/* First step, ensure access from proper machine.
	 * XXX For now, just use DNS.  Later, require a "user" command.
	 */
	if( (him = host_lookup_of_fd(fd)) == IHOST_NULL )  {
		fprintf( ofp, "FAIL unable to obtain your hostname\n");
		exit(0);
	}
	if( strcmp( &him->ht_name[strlen(him->ht_name)-4], ".mil" ) != 0 )  {
		fprintf( ofp, "FAIL connection from unauthorized host %s\n",
			him->ht_name);
		exit(0);
	}

	/* Main loop:  process commands. */
	bu_vls_init(&str);
	while( !feof(ifp ) )  {
		bu_vls_trunc( &str, 0 );
		if( bu_vls_gets( &str, ifp ) < 0 )  break;

		argc = bu_argv_from_string( argv, MAX_ARGS, bu_vls_addr(&str) );

		if( strcmp( argv[0], "status" ) == 0 )  {
			send_status(fd);
			continue;
		}
		if( strcmp( argv[0], "rtnode" ) == 0 )  {
			run_prog(fd, argc, argv, "rtnode");
			continue;
		}
		if( strcmp( argv[0], "find" ) == 0 )  {
			find(fd, argv);
			continue;
		}
		if( strcmp( argv[0], "restart" ) == 0 )  {
			kill( getppid(), SIGUSR1 );
			fprintf( ofp, "OK restart signal sent\n");
			exit(0);
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
 *			B E C O M E _ U S E R
 *
 *  Returns -
 *	1	Didn't work, but nothing we can do about it.
 *	0	OK
 *	-1	Failure to find user name.
 */
int
become_user( name )
CONST char	*name;
{
	struct passwd	*pw;

	if( getuid() != 0 )  return 1;

	setpwent();
	if( (pw = getpwnam(name)) == NULL )  return -1;
	setgid(pw->pw_gid);
	setuid(pw->pw_uid);
	if( getuid() != pw->pw_uid )  {
		fprintf(stderr, "rtmon: wanted %d, got %d?\n", pw->pw_uid, getuid() );
		if( getuid() == 0 )  return -1;
		return 1;
	}
	return 0;
}

/*
 *			R E S T A R T _ S I G N A L
 *
 *  Have main process re-exec itself, if possible.
 */
void
restart_signal(foo)
int	foo;
{
	char	buf[32];

	/* Re-establish handler, in case restart does not work */
	(void)signal( SIGUSR1, restart_signal );

	sprintf( buf, "%d", atoi(main_argv[1])+1 );

	/* If argv[0] has full path, use it */
	if( access( main_argv[0], X_OK ) == 0 )  {
		execl( main_argv[0], main_argv[0], buf, NULL );
		perror(main_argv[0]);
	}
	/* Try to find our executable in one of the usual places. */
	run_prog( 2, main_argc, main_argv, "rtmon" );

	/* If that doesn't work either, just go back to what we were doing. */
	fprintf(stderr, "rtmon: unable to reload, continuing.\n");
	return;
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

	main_argc = argc;
	main_argv = argv;

	if ( !get_args( argc, argv ) ) {
		(void)fputs(usage, stderr);
		exit( 1 );
	}

	close(0);	/* shut stdin */

	setuid(0);	/* in case we're only set-uid */

	/* Set real-time scheduler priority.  May need root privs. */
	bu_set_realtime();

	/* Drop down to normal user privs */
	if( become_user( "cmike" ) != 0 )  {
		if( become_user( "mike" ) != 0 )  {
			/* Failsafe to prevent running as root */
			setgid(42);
			setuid(53);
			if(getuid()==0) exit(42);
		}
	}

	/* Find current directory */
	if( (start_dir = getcwd(NULL,4096-8)) == NULL )  {
		perror("getcwd");
		start_dir = ".";
	}

	/* Accept restart signals */
	(void)signal( SIGUSR1, restart_signal );

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

	if( fcntl(listenfd, F_SETFD, FD_CLOEXEC ) < 0 )  {
		perror("rtmon: FD_CLOEXEC");
		/* Keep going */
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
