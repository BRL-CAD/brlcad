/*                          T T C P . C
 * BRL-CAD
 *
 * Published in 2004-2025 by the United States Government.
 * This work is in the public domain.
 *
 */
/** @file ttcp.c
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

/* intentionally does not include common.h or bio.h so it may remain
 * stand-alone.  however, it does include config so it knows when to
 * declare functions
 */
#include "brlcad_config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <time.h>
#include <limits.h>

#ifdef HAVE_WINSOCK2_H
#  define USE_WINSOCK
#  include <winsock2.h>
#  include <ws2tcpip.h>
#endif
#ifdef HAVE_WINDOWS_H
#  include <windows.h>
#endif
#ifdef HAVE_SYS_SOCKET_H
#  include <sys/socket.h>
#endif
#ifdef HAVE_ARPA_INET_H
#  include <arpa/inet.h>
#endif
#ifdef HAVE_NETINET_IN_H
#  include <netinet/in.h>
#endif
#ifdef HAVE_NETDB_H
#  include <netdb.h>
#endif
#ifdef HAVE_UNISTD_H
#  include <unistd.h>
#endif
#ifdef HAVE_SYS_TIME_H
#  include <sys/time.h>
#endif
#ifdef HAVE_SYS_RESOURCE_H
#  include <sys/resource.h>
#endif
#ifdef HAVE_SYS_TYPES_H
#  include <sys/types.h>
#endif

#if !defined(HAVE_SOCKLEN_T)
#  ifdef USE_WINSOCK
typedef int socklen_t;
#  else
typedef unsigned int socklen_t;
#  endif
#endif

#ifdef USE_WINSOCK
typedef SOCKET socket_t;
#  define close_socket closesocket
#  define READSOCKET(s, b, l) recv((s), (b), (l), 0)
#  define WRITESOCKET(s, b, l) send((s), (b), (l), 0)
#  define ERRNO WSAGetLastError()
#else
typedef int socket_t;
#  define close_socket close
#  define READSOCKET(s, b, l) read((s), (b), (l))
#  define WRITESOCKET(s, b, l) write((s), (b), (l))
#  define ERRNO errno
#endif

#ifdef USE_WINSOCK
#  define perror_sock(msg) \
    fprintf(stderr, "ttcp%s: %s: WSAError=%d\n", trans?"-t":"-r", (msg), WSAGetLastError())
#else
#  define perror_sock(msg) perror(msg)
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
int port = 2000;		/* TCP port number (possible range 0 - 65535)*/
char *host;			/* ptr to name of host */
int trans;			/* 0=receive, !0=transmit mode */
int sinkmode;			/* 0=normal I/O, !0=sink/source mode */

struct hostent *addr;

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
 * This function performs the function of a read(II) but will
 * call read(II) multiple times in order to get the requested
 * number of characters.  This can be necessary because
 * network connections don't deliver data with the same
 * grouping as it is written with.  Written by Robert S. Miles, BRL.
 */
int
mread(socket_t fd, char *bufp, unsigned n)
{
    unsigned count = 0;
    int nread;
    while (count < n) {
        nread = READSOCKET(fd, bufp, n-count);
        if (nread < 0) {
            perror_sock("ttcp_mread");
            return -1;
        }
        if (nread == 0)
            return (int)count;
        count += (unsigned)nread;
        bufp += nread;
    }
    return (int)count;
}

static void
err(const char *s)
{
    fprintf(stderr, "ttcp%s: ", trans?"-t":"-r");
    perror_sock(s);
    exit(1);
}

void
mes(const char *s)
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

#if defined(HAVE_WINSOCK2_H) && !defined(HAVE_GETTIMEOFDAY)
static LARGE_INTEGER timer_freq, timer_start, timer_end;
static double win_timer0 = 0.0;
static double win_cput = 0.0;

void
prep_timer(void)
{
    QueryPerformanceFrequency(&timer_freq);
    QueryPerformanceCounter(&timer_start);
    win_timer0 = (double)timer_start.QuadPart / timer_freq.QuadPart;
}

double
read_timer(char *str, int len)
{
    double end;
    QueryPerformanceCounter(&timer_end);
    end = (double)timer_end.QuadPart / timer_freq.QuadPart;
    realt = end - win_timer0;
    cput = realt; /* No per-thread/user time on Windows w/o more work */
    sprintf(str, "%g CPU secs in %g elapsed secs (Windows)", cput, realt);
    if ((int)strlen(str) >= len) str[len-1] = '\0';
    return cput;
}
#elif defined(HAVE_GETTIMEOFDAY) && defined(HAVE_GETRUSAGE)
static struct timeval time0;
static struct rusage ru0;

void
prep_timer(void)
{
    gettimeofday(&time0, (struct timezone *)0);
    getrusage(RUSAGE_SELF, &ru0);
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
prusage(struct rusage *r0, struct rusage *r1, struct timeval *e, struct timeval *b, char *outp)
{
    struct timeval tdiff;
    time_t t;
    int ms;
    t = (r1->ru_utime.tv_sec-r0->ru_utime.tv_sec)*100+
        (r1->ru_utime.tv_usec-r0->ru_utime.tv_usec)/10000+
        (r1->ru_stime.tv_sec-r0->ru_stime.tv_sec)*100+
        (r1->ru_stime.tv_usec-r0->ru_stime.tv_usec)/10000;
    ms = (e->tv_sec-b->tv_sec)*100 + (e->tv_usec-b->tv_usec)/10000;
    sprintf(outp, "%ld.%01ld user+sys, %d ms real",
            (long int)tdiff.tv_sec, (long int)tdiff.tv_usec/100000L, ms*10);
}

double
read_timer(char *str, int len)
{
    struct timeval timedol;
    struct rusage ru1;
    struct timeval td;
    getrusage(RUSAGE_SELF, &ru1);
    gettimeofday(&timedol, (struct timezone *)0);
    prusage(&ru0, &ru1, &timedol, &time0, str);

    /* Get real time */
    tvsub(&td, &timedol, &time0);
    realt = td.tv_sec + ((double)td.tv_usec) / 1000000;

    /* Get CPU time (user+sys) */
    tvsub(&td, &ru1.ru_utime, &ru0.ru_utime);
    cput = td.tv_sec + ((double)td.tv_usec) / 1000000;
    if (cput < 0.00001) cput = 0.00001;
    if ((int)strlen(str) >= len) str[len-1] = '\0';
    return cput;
}
#else
static clock_t time0;

void
prep_timer(void)
{
    time0 = clock();
}

double
read_timer(char *str, int len)
{
    clock_t now;
    now = clock();
    realt = (double)(now - time0) / CLOCKS_PER_SEC;
    cput = realt;
    sprintf(str, "%g CPU secs in %g elapsed secs (clock)", cput, realt);
    if ((int)strlen(str) >= len) str[len-1] = '\0';
    return cput;
}
#endif

int
Nread(socket_t fd, char *buf, int count)
{
    struct sockaddr_in from;
    socklen_t len;
    int cnt;
    len = (socklen_t)sizeof(from);
    if (udp) {
#ifdef USE_WINSOCK
        cnt = recvfrom(fd, buf, count, 0, (struct sockaddr *)&from, &len);
#else
        cnt = recvfrom(fd, (void *)buf, (size_t)count, 0, (struct sockaddr *)&from, &len);
#endif
    } else {
        if (b_flag)
            cnt = mread(fd, buf, count);	/* fill buf */
        else
            cnt = READSOCKET(fd, buf, count);
    }
    return cnt;
}

int
delay(int us)
{
#if defined(HAVE_WINSOCK2_H) && defined(HAVE_WINDOWS_H)
    Sleep(us / 1000);
#elif defined(HAVE_SELECT)
    {
        struct timeval tv;
        tv.tv_sec = 0;
        tv.tv_usec = us;
        (void)select(1, (fd_set *)0, (fd_set *)0, (fd_set *)0, &tv);
    }
#else
    {
        clock_t start = clock();
        while (((double)(clock() - start) * 1000000.0 / (double)CLOCKS_PER_SEC) < (double)us) {}
    }
#endif
    return 1;
}

int
Nwrite(socket_t fd, char *buf, int count)
{
    int cnt;
    errno = 0;
    if (udp) {
    again:
#ifdef USE_WINSOCK
        cnt = sendto(fd, buf, count, 0, (const struct sockaddr *)&sinhim, sizeof(sinhim));
        if (cnt<0 && ERRNO == WSAENOBUFS) {
#else
        cnt = sendto(fd, (const void *)buf, (size_t) count, 0,
            (const struct sockaddr *)&sinhim,
            sizeof(sinhim));
        if (cnt<0 && ERRNO == ENOBUFS) {
#endif
            delay(18000);
            errno = 0;
            goto again;
        }
    } else {
        cnt = WRITESOCKET(fd, buf, count);
    }
    return cnt;
}

int
main(int argc, char **argv)
{
    socket_t fd;			/* fd of network socket */
    char *buf;			/* ptr to dynamic buffer */
    int buflen = 1024;		/* length of buffer */
    int nbuf = 1024;		/* number of buffers to send in sinkmode */
    unsigned long addr_tmp;
    int cnt;
#ifdef USE_WINSOCK
    WSADATA wsaData;

    _setmode(_fileno(stdin), _O_BINARY);
    _setmode(_fileno(stdout), _O_BINARY);

    if (WSAStartup(MAKEWORD(2,2), &wsaData) != 0) {
        fprintf(stderr, "WSAStartup failed\n");
        exit(1);
    }
#endif

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
#ifdef SO_DEBUG
            options |= SO_DEBUG;
#endif
            break;
        case 'n':
            nbuf = atoi(&argv[0][2]);
            if (nbuf < 0) {
                printf("Negative buffer count.\n");
                return -1;
            }
            if (nbuf >= INT_MAX) {
                printf("Too many buffers specified.\n");
                return -1;
            }
            break;
        case 'l':
            buflen = atoi(&argv[0][2]);
            if (buflen <= 0) {
                printf("Invalid buffer length.\n");
                return -1;
            }
            if (buflen >= INT_MAX) {
                printf("Buffer length too large.\n");
                return -1;
            }
            break;
        case 's':
            sinkmode = 1;	/* source or sink, really */
            break;
        case 'p':
            port = atoi(&argv[0][2]);
            if (port < 0) {
                port = 0;
            }
            if (port > 65535) {
                port = 65535;
            }
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
            sinhim.sin_addr.s_addr = inet_addr(host);
        } else {
#if defined(HAVE_GETADDRINFO)
            {
                struct addrinfo hints, *res;
                int result;
                memset(&hints, 0, sizeof(hints));
                hints.ai_family = AF_INET;
                hints.ai_socktype = udp ? SOCK_DGRAM : SOCK_STREAM;
                result = getaddrinfo(host, NULL, &hints, &res);
                if (result != 0 || res == NULL)
                    err("bad hostname");
                sinhim = *(struct sockaddr_in *)res->ai_addr;
                freeaddrinfo(res);
            }
#elif defined(HAVE_GETHOSTBYNAME)
            addr = (struct hostent *)gethostbyname(host);
            if (addr == NULL)
                err("bad hostname");
            sinhim.sin_family = addr->h_addrtype;
            memcpy((char*)&addr_tmp, addr->h_addr_list[0], addr->h_length);
            sinhim.sin_addr.s_addr = addr_tmp;
#else
            fprintf(stderr, "No hostname resolution available\n");
            exit(1);
#endif
        }
        sinhim.sin_port = htons(port);
        sinme.sin_port = 0;		/* free choice */
        sinme.sin_family = AF_INET; /* Ensure family is set for sender */
    } else {
        /* rcvr */
        sinme.sin_family = AF_INET; /* Ensure family is set for receiver */
        sinme.sin_port =  htons(port);
    }

    buf = (char *)malloc(buflen);
    if (buf == NULL)
        err("malloc");
    fprintf(stderr, "ttcp%s: nbuf=%d, buflen=%d, port=%d\n",
        trans?"-t":"-r",
        nbuf, buflen, port);

    fd = socket(AF_INET, udp?SOCK_DGRAM:SOCK_STREAM, 0);
    if (fd < 0)
        err("socket");
    mes("socket");

    if (bind(fd, (const struct sockaddr *)&sinme, sizeof(sinme)) < 0)
        err("bind");

    if (!udp) {
        if (trans) {
            /* We are the client if transmitting */
            if (options) {
                if (setsockopt(fd, SOL_SOCKET, options, (char*)&one, sizeof(one)) < 0)
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
                if (setsockopt(fd, SOL_SOCKET, options, (char*)&one, sizeof(one)) < 0)
                    err("setsockopt");
            }
            fromlen = (socklen_t)sizeof(frominet);
            domain = AF_INET;
            fd = accept(fd, (struct sockaddr *)&frominet, &fromlen);
            if (fd < 0)
                err("accept");
            mes("accept");
        }
    }
    prep_timer();
    errno = 0;
    if (sinkmode) {
        int going = 0;
        if (trans) {
            pattern(buf, buflen);
            if (udp) (void)Nwrite(fd, buf, 4); /* rcvr start */
            while (nbuf-- && Nwrite(fd, buf, buflen) == buflen)
                nbytes += buflen;
            if (udp) (void)Nwrite(fd, buf, 4); /* rcvr end */
        } else {
            while ((cnt=Nread(fd, buf, buflen)) > 0) {
                /* --- UDP control packet fix: skip 4-byte all-zero packets --- */
                if (udp && cnt == 4) {
                    int all_zero = 1;
                    for (int i = 0; i < 4; i++) {
                        if (buf[i] != 0) {
                            all_zero = 0;
                            break;
                        }
                    }
                    if (all_zero)
                        continue; /* skip control packet */
                }
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
        if (trans) {
            while (
#if defined(USE_WINSOCK)
                (cnt = _read(0, buf, buflen)) > 0
#elif defined(HAVE_UNISTD_H)
                (cnt = read(0, buf, buflen)) > 0
#else
                (cnt = fread(buf, 1, buflen, stdin)) > 0
#endif
                && Nwrite(fd, buf, cnt) == cnt)
                nbytes += cnt;
        } else {
            while ((cnt=Nread(fd, buf, buflen)) > 0) {
                /* --- UDP control packet fix: skip 4-byte all-zero packets --- */
                if (udp && cnt == 4) {
                    int all_zero = 1;
                    for (int i = 0; i < 4; i++) {
                        if (buf[i] != 0) {
                            all_zero = 0;
                            break;
                        }
                    }
                    if (all_zero)
                        continue; /* skip UDP end marker */
                }
#if defined(USE_WINSOCK)
                if (_write(1, buf, cnt) != cnt) break;
#elif defined(HAVE_UNISTD_H)
                if (write(1, buf, cnt) != cnt) break;
#else
                if (fwrite(buf, 1, cnt, stdout) != (size_t)cnt) break;
#endif
                nbytes += cnt;
            }
        }
    }
    if (errno) err("IO");
    (void)read_timer(stats, sizeof(stats));
    if (udp&&trans) {
        memset(buf, 0, 4); /* Make sure the first 4 bytes are zero */
        (void)Nwrite(fd, buf, 4); /* rcvr end */
        (void)Nwrite(fd, buf, 4); /* rcvr end */
        (void)Nwrite(fd, buf, 4); /* rcvr end */
        (void)Nwrite(fd, buf, 4); /* rcvr end */
    }
    free(buf);
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
#ifdef USE_WINSOCK
    WSACleanup();
#endif
    return 0;

usage:
    fprintf(stderr, "%s%s", Usage, Usage2);
#ifdef USE_WINSOCK
    WSACleanup();
#endif
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
