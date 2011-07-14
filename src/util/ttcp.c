/*                          T T C P . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2011 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file util/ttcp.c
 *
 * Test TCP connection.  Makes a connection on port 2000
 * and transfers zero buffers or data copied from stdin.
 *
 * Usable on 4.2, 4.3, and 4.1a systems by defining one of
 * BSD42 BSD43 (BSD41a)
 *
 * Modified for operation under 4.2BSD, 18 Dec 84
 * T.C. Slattery, USNA
 * Minor improvements, Mike Muuss and Terry Slattery, 16-Oct-85.
 *
 * Mike Muuss and Terry Slattery have released this code to the Public Domain.
 */

#define BSD43
/* #define BSD42 */
/* #define BSD41a */

/* does not include common.h or bio.h so it may remain stand-alone */

#ifndef _WIN32
#  include <unistd.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <sys/types.h>
#ifndef _WIN32
#  include <sys/socket.h>
#endif
#include <netinet/in.h>
#include <arpa/inet.h>
#ifndef _WIN32
#  include <sys/time.h>		/* struct timeval */
#  include <netdb.h>
#else
#  include <windows.h>
#endif

#if defined(SYSV) || defined(__HAIKU__)
#  include <sys/times.h>
#  include <sys/param.h>
#else
#  include <sys/resource.h>
#endif


struct sockaddr_in sinme;
struct sockaddr_in sinhim;
struct sockaddr_in sindum;
struct sockaddr_in frominet;

int domain;
socklen_t fromlen;

int udp = 0;			/* 0 = tcp, !0 = udp */
int options = 0;		/* socket options */
int one = 1;                    /* for 4.3 BSD style setsockopt() */
short port = 2000;		/* TCP port number */
char *host;			/* ptr to name of host */
int trans;			/* 0=receive, !0=transmit mode */
int sinkmode;			/* 0=normal I/O, !0=sink/source mode */

struct hostent *addr;

/* Usage broken into two strings to avoid the 509 C90 'minimum' */
char Usage[] = "\
Usage: ttcp -t [-options] host <in\n\
	-l##	length of bufs written to network (default 1024)\n\
	-s	source a pattern to network\n\
	-n##	number of bufs written to network (-s only, default 1024)\n\
	-p##	port number to send to (default 2000)\n\
	-u	use UDP instead of TCP\n\
";
char Usage2[] = "\
Usage: ttcp -r [-options] >out\n\
	-l##	length of network read buf (default 1024)\n\
	-s	sink (discard) all data from network\n\
	-p##	port number to listen at (default 2000)\n\
	-B	Only output full blocks, as specified in -l## (for TAR)\n\
	-u	use UDP instead of TCP\n\
";

char stats[128];
long nbytes;			/* bytes on net */
int b_flag = 0;			/* use mread() */

double cput, realt;		/* user, real time (seconds) */

/*
 * M R E A D
 *
 * This function performs the function of a read(II) but will
 * call read(II) multiple times in order to get the requested
 * number of characters.  This can be necessary because
 * network connections don't deliver data with the same
 * grouping as it is written with.  Written by Robert S. Miles, BRL.
 */
int
mread(int fd, char *bufp, unsigned n)
{
    unsigned count = 0;
    int nread;

    do {
	nread = read(fd, bufp, n-count);
	if (nread < 0) {
	    perror("ttcp_mread");
	    return -1;
	}
	if (nread == 0)
	    return (int)count;
	count += (unsigned)nread;
	bufp += nread;
    } while (count < n);

    return (int)count;
}


static void
err(char *s)
{
    fprintf(stderr, "ttcp%s: ", trans?"-t":"-r");
    perror(s);
    fprintf(stderr, "errno=%d\n", errno);
    exit(1);
}


void
mes(char *s)
{
    fprintf(stderr, "ttcp%s: %s\n", trans?"-t":"-r", s);
}


void
pattern(char *cp, int cnt)
{
    char c;
    c = 0;
    while (cnt-- > 0) {
	while (!isprint((c&0x7F))) c++;
	*cp++ = (c++&0x7F);
    }
}


/******* timing *********/

#if defined(SYSV) || defined(__HAIKU__)
/* was long instead of time_t */
extern time_t time(time_t *);
static time_t time0;
static struct tms tms0;
#else
static struct timeval time0;	/* Time at which timeing started */
static struct rusage ru0;	/* Resource utilization at the start */

static void prusage(struct rusage *r0, struct rusage *r1, struct timeval *e, struct timeval *b, char *outp);
static void tvadd(struct timeval *tsum, struct timeval *t0, struct timeval *t1);
static void tvsub(struct timeval *tdiff, struct timeval *t1, struct timeval *t0);
static void psecs(long int l, char *cp);
#endif

/*
 * P R E P _ T I M E R
 */
void
prep_timer(void)
{
#if defined(SYSV) || defined(__HAIKU__)
    (void)time(&time0);
    (void)times(&tms0);
#else
    gettimeofday(&time0, (struct timezone *)0);
    getrusage(RUSAGE_SELF, &ru0);
#endif
}


/*
 * R E A D _ T I M E R
 *
 */
double
read_timer(char *str, int len)
{
#if defined(SYSV) || defined(__HAIKU__)
    time_t now;
    struct tms tmsnow;
    char line[132] = {0};

    (void)time(&now);
    realt = now-time0;
    (void)times(&tmsnow);
    cput = tmsnow.tms_utime - tms0.tms_utime;
    if (cput < 0.00001) cput = 0.01;
    if (realt < 0.00001) realt = cput;
    sprintf(line, "%g CPU secs in %g elapsed secs (%g%%)",
	    cput, realt,
	    cput/realt*100);
    strncpy(str, line, len);
    return cput;
#else
    /* BSD */
    struct timeval timedol;
    struct rusage ru1;
    struct timeval td;
    struct timeval tend, tstart;
    char line[132] = {0};

    getrusage(RUSAGE_SELF, &ru1);
    gettimeofday(&timedol, (struct timezone *)0);
    prusage(&ru0, &ru1, &timedol, &time0, line);
    strncpy(str, line, len);

    /* Get real time */
    tvsub(&td, &timedol, &time0);
    realt = td.tv_sec + ((double)td.tv_usec) / 1000000;

    /* Get CPU time (user+sys) */
    tvadd(&tend, &ru1.ru_utime, &ru1.ru_stime);
    tvadd(&tstart, &ru0.ru_utime, &ru0.ru_stime);
    tvsub(&td, &tend, &tstart);
    cput = td.tv_sec + ((double)td.tv_usec) / 1000000;
    if (cput < 0.00001) cput = 0.00001;
    return cput;
#endif
}


#if !defined(SYSV) && !defined(__HAIKU__)
static void
prusage(struct rusage *r0,
	struct rusage *r1,
	struct timeval *e,
	struct timeval *b,
	char *outp)
{
    struct timeval tdiff;
    time_t t;
    char *cp;
    int i;
    int ms;

    t = (r1->ru_utime.tv_sec-r0->ru_utime.tv_sec)*100+
	(r1->ru_utime.tv_usec-r0->ru_utime.tv_usec)/10000+
	(r1->ru_stime.tv_sec-r0->ru_stime.tv_sec)*100+
	(r1->ru_stime.tv_usec-r0->ru_stime.tv_usec)/10000;
    ms =  (e->tv_sec-b->tv_sec)*100 + (e->tv_usec-b->tv_usec)/10000;

#define END(x) {while (*x) x++;}
    cp = "%Uuser %Ssys %Ereal %P %Xi+%Dd %Mmaxrss %F+%Rpf %Ccsw";
    for (; *cp; cp++) {
	if (*cp != '%')
	    *outp++ = *cp;
	else if (cp[1]) switch (*++cp) {

	    case 'U':
		tvsub(&tdiff, &r1->ru_utime, &r0->ru_utime);
		sprintf(outp, "%ld.%01ld", (long int)tdiff.tv_sec,
			(long int)tdiff.tv_usec/100000L);
		END(outp);
		break;

	    case 'S':
		tvsub(&tdiff, &r1->ru_stime, &r0->ru_stime);
		sprintf(outp, "%ld.%01ld", (long int)tdiff.tv_sec, (long int)tdiff.tv_usec/100000L);
		END(outp);
		break;

	    case 'E':
		psecs(ms / 100, outp);
		END(outp);
		break;

	    case 'P':
		sprintf(outp, "%d%%", (int) (t*100 / ((ms ? ms : 1))));
		END(outp);
		break;

	    case 'W':
		i = r1->ru_nswap - r0->ru_nswap;
		sprintf(outp, "%d", i);
		END(outp);
		break;

	    case 'X':
		sprintf(outp, "%ld", t == 0 ? 0 : (r1->ru_ixrss-r0->ru_ixrss)/t);
		END(outp);
		break;

	    case 'D':
		sprintf(outp, "%ld", t == 0 ? 0 :
			(r1->ru_idrss+r1->ru_isrss-(r0->ru_idrss+r0->ru_isrss))/t);
		END(outp);
		break;

	    case 'K':
		sprintf(outp, "%ld", t == 0 ? 0 :
			((r1->ru_ixrss+r1->ru_isrss+r1->ru_idrss) -
			 (r0->ru_ixrss+r0->ru_idrss+r0->ru_isrss))/t);
		END(outp);
		break;

	    case 'M':
		sprintf(outp, "%ld", r1->ru_maxrss/2);
		END(outp);
		break;

	    case 'F':
		sprintf(outp, "%ld", r1->ru_majflt-r0->ru_majflt);
		END(outp);
		break;

	    case 'R':
		sprintf(outp, "%ld", r1->ru_minflt-r0->ru_minflt);
		END(outp);
		break;

	    case 'I':
		sprintf(outp, "%ld", r1->ru_inblock-r0->ru_inblock);
		END(outp);
		break;

	    case 'O':
		sprintf(outp, "%ld", r1->ru_oublock-r0->ru_oublock);
		END(outp);
		break;
	    case 'C':
		sprintf(outp, "%ld+%ld", r1->ru_nvcsw-r0->ru_nvcsw,
			r1->ru_nivcsw-r0->ru_nivcsw);
		END(outp);
		break;
	}
    }
    *outp = '\0';
}


static void
tvadd(struct timeval *tsum, struct timeval *t0, struct timeval *t1)
{

    tsum->tv_sec = t0->tv_sec + t1->tv_sec;
    tsum->tv_usec = t0->tv_usec + t1->tv_usec;
    if (tsum->tv_usec > 1000000)
	tsum->tv_sec++, tsum->tv_usec -= 1000000;
}


static void
tvsub(struct timeval *tdiff, struct timeval *t1, struct timeval *t0)
{

    tdiff->tv_sec = t1->tv_sec - t0->tv_sec;
    tdiff->tv_usec = t1->tv_usec - t0->tv_usec;
    if (tdiff->tv_usec < 0)
	tdiff->tv_sec--, tdiff->tv_usec += 1000000;
}


static void
psecs(long l, char *cp)
{
    int i;

    i = l / 3600;
    if (i) {
	sprintf(cp, "%d:", i);
	END(cp);
	i = l % 3600;
	sprintf(cp, "%d%d", (i/60) / 10, (i/60) % 10);
	END(cp);
    } else {
	i = l;
	sprintf(cp, "%d", i / 60);
	END(cp);
    }
    i %= 60;
    *cp++ = ':';
    sprintf(cp, "%d%d", i / 10, i % 10);
}
#endif

/*
 * N R E A D
 */
int
Nread(int fd, char *buf, int count)
{
    struct sockaddr_in from;
    socklen_t len = (socklen_t)sizeof(from);
    int cnt;

    if (udp) {
	cnt = recvfrom(fd, (void *)buf, (size_t)count, 0, (struct sockaddr *)&from, &len);
    } else {
	if (b_flag)
	    cnt = mread(fd, buf, count);	/* fill buf */
	else
	    cnt = read(fd, buf, count);
    }
    return cnt;
}


int
delay(int us)
{
    struct timeval tv;

    tv.tv_sec = 0;
    tv.tv_usec = us;
    (void)select(1, (fd_set *)0, (fd_set *)0, (fd_set *)0, &tv);
    return 1;
}


/*
 * N W R I T E
 */
int
Nwrite(int fd, char *buf, int count)
{
    int cnt;
    if (udp) {
    again:
	cnt = sendto(fd, (const void *)buf, (size_t) count, 0,
		     (const struct sockaddr *)&sinhim,
		     sizeof(sinhim));
	if (cnt<0 && errno == ENOBUFS) {
	    delay(18000);
	    errno = 0;
	    goto again;
	}
    } else {
	cnt = write(fd, buf, count);
    }
    return cnt;
}


int
main(int argc, char **argv)
{
    int fd;			/* fd of network socket */

    char *buf;			/* ptr to dynamic buffer */
    int buflen = 1024;		/* length of buffer */
    int nbuf = 1024;		/* number of buffers to send in sinkmode */

    unsigned long addr_tmp;

    if (argc < 2) goto usage;

    argv++; argc--;
    while (argc>0 && argv[0][0] == '-') {
	switch (argv[0][1]) {

	    case 'B':
		b_flag = 1;
		break;
	    case 't':
		trans = 1;
		break;
	    case 'r':
		trans = 0;
		break;
	    case 'd':
		options |= SO_DEBUG;
		break;
	    case 'n':
		nbuf = atoi(&argv[0][2]);
		break;
	    case 'l':
		buflen = atoi(&argv[0][2]);
		break;
	    case 's':
		sinkmode = 1;	/* source or sink, really */
		break;
	    case 'p':
		port = atoi(&argv[0][2]);
		break;
	    case 'u':
		udp = 1;
		break;
	    default:
		goto usage;
	}
	argv++; argc--;
    }
    if (trans) {
	/* xmitr */
	if (argc != 1) goto usage;
	memset((char *)&sinhim, 0, sizeof(sinhim));
	host = argv[0];
	if (atoi(host) > 0) {
	    /* Numeric */
	    sinhim.sin_family = AF_INET;
#ifdef cray
	    addr_tmp = inet_addr(host);
	    sinhim.sin_addr = addr_tmp;
#else
	    sinhim.sin_addr.s_addr = inet_addr(host);
#endif
	} else {
	    if ((addr=(struct hostent *)gethostbyname(host)) == NULL)
		err("bad hostname");
	    sinhim.sin_family = addr->h_addrtype;
	    memcpy((char*)&addr_tmp, addr->h_addr, addr->h_length);
#ifdef cray
	    sinhim.sin_addr = addr_tmp;
#else
	    sinhim.sin_addr.s_addr = addr_tmp;
#endif /* cray */
	}
	sinhim.sin_port = htons(port);
	sinme.sin_port = 0;		/* free choice */
    } else {
	/* rcvr */
	sinme.sin_port =  htons(port);
    }

    if ((buf = (char *)malloc(buflen)) == (char *)NULL)
	err("malloc");
    fprintf(stderr, "ttcp%s: nbuf=%d, buflen=%d, port=%d\n",
	    trans?"-t":"-r",
	    nbuf, buflen, port);

    if ((fd = socket(AF_INET, udp?SOCK_DGRAM:SOCK_STREAM, 0)) < 0)
	err("socket");
    mes("socket");

    if (bind(fd, (const struct sockaddr *)&sinme, sizeof(sinme)) < 0)
	err("bind");

    if (!udp) {
	if (trans) {
	    /* We are the client if transmitting */
	    if (options) {
#ifdef BSD42
		if (setsockopt(fd, SOL_SOCKET, options, 0, 0) < 0)
#else /* BSD43 */
		    if (setsockopt(fd, SOL_SOCKET, options, &one, sizeof(one)) < 0)
#endif
			err("setsockopt");
	    }
	    if (connect(fd, (const struct sockaddr *)&sinhim, sizeof(sinhim)) < 0)
		err("connect");
	    mes("connect");
	} else {
	    /* otherwise, we are the server and
	     * should listen for the connections
	     */
	    listen(fd, 0);   /* allow a queue of 0 */
	    if (options) {
#ifdef BSD42
		if (setsockopt(fd, SOL_SOCKET, options, 0, 0) < 0)
#else /* BSD43 */
		    if (setsockopt(fd, SOL_SOCKET, options, &one, sizeof(one)) < 0)
#endif
			err("setsockopt");
	    }
	    fromlen = (socklen_t)sizeof(frominet);
	    domain = AF_INET;
	    if ((fd=accept(fd, (struct sockaddr *)&frominet, &fromlen)) < 0)
		err("accept");
	    mes("accept");
	}
    }
    prep_timer();
    errno = 0;
    if (sinkmode) {
	int cnt;
	if (trans) {
	    pattern(buf, buflen);
	    if (udp)  (void)Nwrite(fd, buf, 4); /* rcvr start */
	    while (nbuf-- && Nwrite(fd, buf, buflen) == buflen)
		nbytes += buflen;
	    if (udp)  (void)Nwrite(fd, buf, 4); /* rcvr end */
	} else {
	    while ((cnt=Nread(fd, buf, buflen)) > 0) {
		static int going = 0;
		if (cnt <= 4) {
		    if (going)
			break;	/* "EOF" */
		    going = 1;
		    prep_timer();
		} else
		    nbytes += cnt;
	    }
	}
    } else {
	int cnt;
	if (trans) {
	    while ((cnt=read(0, buf, buflen)) > 0 &&
		   Nwrite(fd, buf, cnt) == cnt)
		nbytes += cnt;
	} else {
	    while ((cnt=Nread(fd, buf, buflen)) > 0 &&
		   write(1, buf, cnt) == cnt)
		nbytes += cnt;
	}
    }
    if (errno) err("IO");
    (void)read_timer(stats, sizeof(stats));
    if (udp&&trans) {
	(void)Nwrite(fd, buf, 4); /* rcvr end */
	(void)Nwrite(fd, buf, 4); /* rcvr end */
	(void)Nwrite(fd, buf, 4); /* rcvr end */
	(void)Nwrite(fd, buf, 4); /* rcvr end */
    }
    fprintf(stderr, "ttcp%s: %s\n", trans?"-t":"-r", stats);
    if (cput <= 0.0) cput = 0.001;
    if (realt <= 0.0) realt = 0.001;
    fprintf(stderr, "ttcp%s: %ld bytes processed\n",
	    trans?"-t":"-r", nbytes);
    fprintf(stderr, "ttcp%s: %9g CPU sec  = %9g KB/cpu sec,  %9g Kbits/cpu sec\n",
	    trans?"-t":"-r",
	    cput,
	    ((double)nbytes)/cput/1024,
	    ((double)nbytes)*8/cput/1024);
    fprintf(stderr, "ttcp%s: %9g real sec = %9g KB/real sec, %9g Kbits/sec\n",
	    trans?"-t":"-r",
	    realt,
	    ((double)nbytes)/realt/1024,
	    ((double)nbytes)*8/realt/1024);
    return 0;

 usage:
    fprintf(stderr, "%s%s", Usage, Usage2);
    return 1;
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
