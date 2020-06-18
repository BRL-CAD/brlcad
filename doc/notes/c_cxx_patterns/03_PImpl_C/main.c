#include <stdlib.h>
#include <stdio.h>
#include <malloc.h>
#include "nlib.h"

int main(int argc, const char **argv) {
    int i = 0;
    int n_attempts = 10000;

    /* Example on stack */
    {
	struct nc c1;
	nc_init(&c1);
	for (i = 0; i < n_attempts; i++) {
	    add_number(&c1, rand() % 10001);
	}
	printf("Stack: Tried %d numbers\n", n_attempts);
	tally(&c1);
	nc_clear(&c1);
    }

    /* Example explicitly allocated */
    {
	struct nc *c2;
	if (nc_create(&c2)) return -1;
	for (i = 0; i < n_attempts; i++) {
	    add_number(c2, rand() % 10001);
	}
	printf("Allocated: Tried %d numbers\n", n_attempts);
	tally(c2);
	nc_destroy(c2);
    }


    return 0;
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
