/* dummy test to exercise bu_parallel */
#include "conf.h"

#include <stdio.h>
#include <unistd.h>
#include <math.h>
#include <stdlib.h>
#include <limits.h>

#include "machine.h"
#include "bu.h"
#include "vmath.h"
#include "bn.h"
#include "raytrace.h"

/* workers acquire semaphore number 0 on smp machines */
#define P2G_WORKER RT_SEM_LAST
#define P2G_INIT_COUNT P2G_WORKER+1

int iterations=512;
double result=0.0;

void computeSomething( int pid, void *arg ) {
	int i;

	double x;
	double y;

	while (iterations > 0) {

		x = y = 1.0;
		
		for (i = 0; i < 100000L; i++) {
			x += sqrt((double)i);
			y += cbrt((double)i);
		}

		
		/* this doesn't need to be semaphored, but I want to grab at least more
		 * than one semaphore in the work block
		 */
		bu_semaphore_acquire(P2G_WORKER);

		if ( (iterations % 32) ==  0 ) {
			bu_log("%d", iterations / 32);
		} else if ( (iterations % 8) == 0 ) {
			bu_log(".");
		}

		i=random();
		bu_semaphore_release(P2G_WORKER);
		
		/*	bu_log ("random: %d\n", i); */
		
		/* add some per process variability */
		for ( i=random() / (LONG_MAX / 2) ; i > 0; i-- ) {
			x += 1;
			y += 1;
			x -= 1;
			y -= 1;
		}
		
		bu_semaphore_acquire(P2G_WORKER);
		result = (x - y);
		iterations--;
		bu_semaphore_release(P2G_WORKER);
	}

}

int
main(int ac, char *av[])
{
	int ncpu=1;

	ncpu=bu_avail_cpus();

	if (ac == 2) {
		ncpu=atoi(av[1]);
	}

	if (ncpu > 1) {
		bu_log("Found %d cpu\'s!  Sweet.\n", ncpu);
	}
			
	/* the first critical section semaphore is for coordinating work, the
	 * second for writing out the final record and cleaning up. 
	 */
	/* XXX must use RT_SEM_LAST if we plan on calling bu_parallel since the
	 * semaphore count is held in a global
	 */
	bu_semaphore_init(P2G_INIT_COUNT);

	
	bu_log("before: %f\n", result);

	/* XXX I do not like the idea of having to pass everything around in global
	 * space. but forking on our own is just as bad (need good IPC)
	 */
	bu_parallel(computeSomething, ncpu, NULL);

	bu_log("\nafter: %f\n", result);
		
	bu_log("\n...done!\n");
	
	return 0;
}
