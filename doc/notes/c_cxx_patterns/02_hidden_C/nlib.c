#include <malloc.h>
#include "nlib.h"

struct nc {
    int num_cnt;
    int *numbers;
};

int
nc_create(struct nc **n)
{
    if (!n) return -1;
    (*n) = malloc(sizeof(struct nc));
    if (!*n) return -1;
    (*n)->num_cnt = 0;
    (*n)->numbers = NULL;
    return 0;
}

void
nc_destroy(struct nc *n)
{
    if (!n) return;
    if (n && n->numbers) free(n->numbers);
    free(n);
}

void
add_number(struct nc *n, int num)
{
    int i = 0;
    if (!n) return;
    for (i = 0; i < n->num_cnt; i++) {
	if (n->numbers[i] == num) return;
    }
    n->num_cnt++;
    n->numbers = realloc(n->numbers, n->num_cnt * sizeof(int));
    n->numbers[n->num_cnt - 1] = num;
}

void
tally(struct nc *n){
    if (!n) return;
    printf("Added %d unique numbers\n", n->num_cnt);
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

