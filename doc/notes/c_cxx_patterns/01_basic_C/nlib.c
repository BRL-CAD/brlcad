#include <malloc.h>
#include "nlib.h"

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

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */

