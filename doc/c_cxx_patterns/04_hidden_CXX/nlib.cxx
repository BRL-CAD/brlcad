#include <iostream>
#include <set>
#include "nlib.h"

struct nc {
    std::set<int> numbers;
};

int
nc_create(struct nc **n)
{
    if (!n) return -1;
    (*n) = new nc;
    return (!*n) ? -1 : 0;
}

void
nc_destroy(struct nc *n)
{
    if (!n) return;
    delete n;
}

void
add_number(struct nc *n, int num)
{
    if (!n) return;
    n->numbers.insert(num);
}

void
tally(struct nc *n){
    std::cout << "Added " << n->numbers.size() << " unique numbers" << std::endl;
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

