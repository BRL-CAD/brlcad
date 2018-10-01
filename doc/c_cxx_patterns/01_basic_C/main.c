#include <stdlib.h>
#include <stdio.h>
#include <malloc.h>
#include "nlib.h"

int main(int argc, const char **argv) {
    int i = 0;
    int n_attempts = 10000;
    struct nc c;
    c.num_cnt = 0;
    c.numbers = NULL;
    for (i = 0; i < n_attempts; i++) {
	add_number(&c, rand() % 10001);
    }
    printf("Tried %d numbers\n", n_attempts);
    printf("Added %d unique numbers\n", c.num_cnt);
    free(c.numbers);
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
