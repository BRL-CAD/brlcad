#include <stdlib.h>
#include <stdio.h>
#include <malloc.h>
#include "nlib.h"

int main(int argc, const char **argv) {
    int i = 0;
    int n_attempts = 10000;
    struct nc *c;
    if (nc_create(&c)) return -1;
    for (i = 0; i < n_attempts; i++) {
	add_number(c, rand() % 10001);
    }
    printf("Tried %d numbers\n", n_attempts);
    tally(c);
    nc_destroy(c);
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
