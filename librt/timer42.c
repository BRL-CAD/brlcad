/*
 *			T I M E R . C
 *
 * Function -
 *	To provide timing information for RT.
 *
 * Based on CSH sh.time.c module.
 *
 * $Revision$
 */
#ifndef lint
static char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include <stdio.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>

static struct	timeval time0;	/* Time at which timeing started */
static struct	rusage ru0;	/* Resource utilization at the start */

static void prusage();
static void pdeltat();
static void tvadd();
static void tvsub();
static void psecs();
static void p2dig();

/*
 *			T I M E R _ P R E P
 */
void
timer_prep()
{
	gettimeofday(&time0, (struct timezone *)0);
	getrusage(RUSAGE_SELF, &ru0);
}

/*
 *			T I M E R _ P R I N T
 * 
 */
double
timer_print(str)
char *str;
{
	struct timeval timedol;
	struct rusage ru1;
	struct timeval td;
	double usert;

	getrusage(RUSAGE_SELF, &ru1);
	gettimeofday(&timedol, (struct timezone *)0);
	fprintf(stderr,"%s: ", str);
	prusage(&ru0, &ru1, &timedol, &time0);
	tvsub( &td, &ru1.ru_utime, &ru0.ru_utime );
	usert = td.tv_sec + ((double)td.tv_usec) / 1000000;
	return( usert );
}

static void
prusage(r0, r1, e, b)
	register struct rusage *r0, *r1;
	struct timeval *e, *b;
{
	register time_t t;
	register char *cp;
	register int i;
	int ms;

	t = (r1->ru_utime.tv_sec-r0->ru_utime.tv_sec)*100+
	    (r1->ru_utime.tv_usec-r0->ru_utime.tv_usec)/10000+
	    (r1->ru_stime.tv_sec-r0->ru_stime.tv_sec)*100+
	    (r1->ru_stime.tv_usec-r0->ru_stime.tv_usec)/10000;
	ms =  (e->tv_sec-b->tv_sec)*100 + (e->tv_usec-b->tv_usec)/10000;

	cp = "%Uuser %Ssys %Ereal %P %Xi+%Dd[%M]rss %F+%Rpf %Ccsw %Wswap";
	for (; *cp; cp++)  {
		if (*cp != '%')
			putc(*cp, stderr);
		else if (cp[1]) switch(*++cp) {

		case 'U':
			pdeltat(&r1->ru_utime, &r0->ru_utime);
			break;

		case 'S':
			pdeltat(&r1->ru_stime, &r0->ru_stime);
			break;

		case 'E':
			psecs(ms / 100);
			break;

		case 'P':
			fprintf(stderr,"%d%%", (int) (t*100 / ((ms ? ms : 1))));
			break;

		case 'W':
			i = r1->ru_nswap - r0->ru_nswap;
			fprintf(stderr,"%d", i);
			break;

		case 'X':
			fprintf(stderr,"%d", t == 0 ? 0 : (r1->ru_ixrss-r0->ru_ixrss)/t);
			break;

		case 'D':
			fprintf(stderr,"%d", t == 0 ? 0 :
			    (r1->ru_idrss+r1->ru_isrss-(r0->ru_idrss+r0->ru_isrss))/t);
			break;

		case 'K':
			fprintf(stderr,"%d", t == 0 ? 0 :
			    ((r1->ru_ixrss+r1->ru_isrss+r1->ru_idrss) -
			    (r0->ru_ixrss+r0->ru_idrss+r0->ru_isrss))/t);
			break;

		case 'M':
			fprintf(stderr,"%d", r1->ru_maxrss/2);
			break;

		case 'F':
			fprintf(stderr,"%d", r1->ru_majflt-r0->ru_majflt);
			break;

		case 'R':
			fprintf(stderr,"%d", r1->ru_minflt-r0->ru_minflt);
			break;

		case 'I':
			fprintf(stderr,"%d", r1->ru_inblock-r0->ru_inblock);
			break;

		case 'O':
			fprintf(stderr,"%d", r1->ru_oublock-r0->ru_oublock);
			break;
		case 'C':
			fprintf(stderr,"%d+%d", r1->ru_nvcsw-r0->ru_nvcsw,
				r1->ru_nivcsw-r0->ru_nivcsw );
			break;
		}
	}
	putc('\n',stderr);
}

static void
pdeltat(t1, t0)
	struct timeval *t1, *t0;
{
	struct timeval td;

	tvsub(&td, t1, t0);
	fprintf(stderr,"%d.%01d", td.tv_sec, td.tv_usec/100000);
}

static void
tvadd(tsum, t0)
	struct timeval *tsum, *t0;
{

	tsum->tv_sec += t0->tv_sec;
	tsum->tv_usec += t0->tv_usec;
	if (tsum->tv_usec > 1000000)
		tsum->tv_sec++, tsum->tv_usec -= 1000000;
}

static void
tvsub(tdiff, t1, t0)
	struct timeval *tdiff, *t1, *t0;
{

	tdiff->tv_sec = t1->tv_sec - t0->tv_sec;
	tdiff->tv_usec = t1->tv_usec - t0->tv_usec;
	if (tdiff->tv_usec < 0)
		tdiff->tv_sec--, tdiff->tv_usec += 1000000;
}

static void
psecs(l)
	long l;
{
	register int i;

	i = l / 3600;
	if (i) {
		fprintf(stderr,"%d:", i);
		i = l % 3600;
		p2dig(i / 60);
		goto minsec;
	}
	i = l;
	fprintf(stderr,"%d", i / 60);
minsec:
	i %= 60;
	fprintf(stderr,":");
	p2dig(i);
}

static void
p2dig(i)
	register int i;
{

	fprintf(stderr,"%d%d", i / 10, i % 10);
}
