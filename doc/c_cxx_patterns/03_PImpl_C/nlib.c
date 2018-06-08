#include <malloc.h>
#include "nlib.h"

struct nc_impl {
    int num_cnt;
    int *numbers;
};

int
nc_init(struct nc *n)
{
    if (!n) return -1;
    n->i = malloc(sizeof(struct nc_impl));
    if (!n->i) return -1;
    n->i->num_cnt = 0;
    n->i->numbers = NULL;
    return 0;
}

void
nc_clear(struct nc *n)
{
    if (!n) return;
    if (n->i && n->i->numbers) free(n->i->numbers);
    if (n->i) free(n->i);
}

int
nc_create(struct nc **n)
{
    if (!n) return -1;
    (*n) = malloc(sizeof(struct nc));
    if (!*n) return -1;
    return nc_init(*n);
}

void
nc_destroy(struct nc *n)
{
    if (!n) return;
    nc_clear(n);
    free(n);
}

void
add_number(struct nc *n, int num)
{
    int i = 0;
    if (!n) return;
    for (i = 0; i < n->i->num_cnt; i++) {
	if (n->i->numbers[i] == num) return;
    }
    n->i->num_cnt++;
    n->i->numbers = realloc(n->i->numbers, n->i->num_cnt * sizeof(int));
    n->i->numbers[n->i->num_cnt - 1] = num;
}

void
tally(struct nc *n){
    if (!n || !n->i) return;
    printf("Added %d unique numbers\n", n->i->num_cnt);
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

