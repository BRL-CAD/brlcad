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

#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>

static struct	timeval time0;	/* Time at which timeing started */
static struct	rusage ru0;	/* Resource utilization at the start */

/*
 *			T I M E R _ P R E P
 */
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
	printf("%s: ", str);
	prusage(&ru0, &ru1, &timedol, &time0);
	tvsub( &td, &ru1.ru_utime, &ru0.ru_utime );
	usert = td.tv_sec + ((double)td.tv_usec) / 1000000;
	return( usert );
}

ruadd(ru, ru2)
	register struct rusage *ru, *ru2;
{
	register long *lp, *lp2;
	register int cnt;

	tvadd(&ru->ru_utime, &ru2->ru_utime);
	tvadd(&ru->ru_stime, &ru2->ru_stime);
	if (ru2->ru_maxrss > ru->ru_maxrss)
		ru->ru_maxrss = ru2->ru_maxrss;
	cnt = &ru->ru_last - &ru->ru_first + 1;
	lp = &ru->ru_first; lp2 = &ru2->ru_first;
	do
		*lp++ += *lp2++;
	while (--cnt > 0);
}

prusage(r0, r1, e, b)
	register struct rusage *r0, *r1;
	struct timeval *e, *b;
{
	register time_t t =
	    (r1->ru_utime.tv_sec-r0->ru_utime.tv_sec)*100+
	    (r1->ru_utime.tv_usec-r0->ru_utime.tv_usec)/10000+
	    (r1->ru_stime.tv_sec-r0->ru_stime.tv_sec)*100+
	    (r1->ru_stime.tv_usec-r0->ru_stime.tv_usec)/10000;
	register char *cp;
	register int i;
	int ms =
	    (e->tv_sec-b->tv_sec)*100 + (e->tv_usec-b->tv_usec)/10000;

	cp = "%Uuser %Ssys %Ereal %P %Xi+%Dd[%M]rss %F+%Rpf %Ccsw %Wswap";
	for (; *cp; cp++)
	if (*cp != '%')
		putchar(*cp);
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
		printf("%d%%", (int) (t*100 / ((ms ? ms : 1))));
		break;

	case 'W':
		i = r1->ru_nswap - r0->ru_nswap;
		printf("%d", i);
		break;

	case 'X':
		printf("%d", t == 0 ? 0 : (r1->ru_ixrss-r0->ru_ixrss)/t);
		break;

	case 'D':
		printf("%d", t == 0 ? 0 :
		    (r1->ru_idrss+r1->ru_isrss-(r0->ru_idrss+r0->ru_isrss))/t);
		break;

	case 'K':
		printf("%d", t == 0 ? 0 :
		    ((r1->ru_ixrss+r1->ru_isrss+r1->ru_idrss) -
		    (r0->ru_ixrss+r0->ru_idrss+r0->ru_isrss))/t);
		break;

	case 'M':
		printf("%d", r1->ru_maxrss/2);
		break;

	case 'F':
		printf("%d", r1->ru_majflt-r0->ru_majflt);
		break;

	case 'R':
		printf("%d", r1->ru_minflt-r0->ru_minflt);
		break;

	case 'I':
		printf("%d", r1->ru_inblock-r0->ru_inblock);
		break;

	case 'O':
		printf("%d", r1->ru_oublock-r0->ru_oublock);
		break;
	case 'C':
		printf("%d+%d", r1->ru_nvcsw-r0->ru_nvcsw,
			r1->ru_nivcsw-r0->ru_nivcsw );
		break;
	}
	putchar('\n');
}

pdeltat(t1, t0)
	struct timeval *t1, *t0;
{
	struct timeval td;

	tvsub(&td, t1, t0);
	printf("%d.%01d", td.tv_sec, td.tv_usec/100000);
}

tvadd(tsum, t0)
	struct timeval *tsum, *t0;
{

	tsum->tv_sec += t0->tv_sec;
	tsum->tv_usec += t0->tv_usec;
	if (tsum->tv_usec > 1000000)
		tsum->tv_sec++, tsum->tv_usec -= 1000000;
}

tvsub(tdiff, t1, t0)
	struct timeval *tdiff, *t1, *t0;
{

	tdiff->tv_sec = t1->tv_sec - t0->tv_sec;
	tdiff->tv_usec = t1->tv_usec - t0->tv_usec;
	if (tdiff->tv_usec < 0)
		tdiff->tv_sec--, tdiff->tv_usec += 1000000;
}

psecs(l)
	long l;
{
	register int i;

	i = l / 3600;
	if (i) {
		printf("%d:", i);
		i = l % 3600;
		p2dig(i / 60);
		goto minsec;
	}
	i = l;
	printf("%d", i / 60);
minsec:
	i %= 60;
	printf(":");
	p2dig(i);
}

p2dig(i)
	register int i;
{

	printf("%d%d", i / 10, i % 10);
}
