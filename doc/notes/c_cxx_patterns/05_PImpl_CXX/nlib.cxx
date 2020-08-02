#include <iostream>
#include <set>
#include "nlib.h"

struct nc_impl {
    std::set<int> numbers;
};

int
nc_init(struct nc *n)
{
    if (!n) return -1;
    n->i = new nc_impl;
    return (!n->i) ? -1 : 0;
}

void
nc_clear(struct nc *n)
{
    if (!n) return;
    n->i->numbers.clear();
    delete n->i;
}

int
nc_create(struct nc **n)
{
    if (!n) return -1;
    (*n) = new nc;
    return nc_init(*n);
}

void
nc_destroy(struct nc *n)
{
    if (!n) return;
    nc_clear(n);
    delete n;
}

void
add_number(struct nc *n, int num)
{
    if (!n || !n->i) return;
    n->i->numbers.insert(num);
}

void
tally(struct nc *n){
    if (!n || !n->i) return;
    std::cout << "Added " << n->i->numbers.size() << " unique numbers" << std::endl;
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

