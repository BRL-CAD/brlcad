/*			A P P L I C A T I O N . C
 *
 *	A toy application to test libredblack
 */

#include <stdio.h>
#include <string.h>
#include "redblack.h"

typedef struct
{
    long 	magic;		  /* Magic no. for integrity check */
    char	last[32];
    char	first[32];
    int		party;
} record;
#define		RECORD_MAGIC	0x12345678

/*
 *	The order functions
 */
last_name_sort (void *v1, void *v2)
{
    record *r1 = v1;
    record *r2 = v2;

    return(strcmp(r1 -> last, r2 -> last));
}

first_name_sort (void *v1, void *v2)
{
    record *r1 = v1;
    record *r2 = v2;

    return(strcmp(r1 -> first, r2 -> first));
}

party_sort (void *v1, void *v2)
{
    record *r1 = v1;
    record *r2 = v2;

    return ((r1 -> party < r2 -> party) ? -1 :
	    (r1 -> party > r2 -> party) ? 1 : 0);
}

#define		ORDER_LASTNAME		0
#define		ORDER_FIRSTNAME		1
#define		ORDER_PARTY		2
#define		DEMOCRAT		0
#define		REPUBLICAN		1

/* 
 *	The main driver
 */
main ()
{
    int			i;
    rb_tree		*tree;
    static record	pres[10] = 
			{
			    {RECORD_MAGIC, "Roosevelt", "Franklin", DEMOCRAT},
			    {RECORD_MAGIC, "Truman", "Harry", DEMOCRAT},
			    {RECORD_MAGIC, "Eisenhower", "Dwight", REPUBLICAN},
			    {RECORD_MAGIC, "Kennedy", "John", DEMOCRAT},
			    {RECORD_MAGIC, "Johnson", "Lyndon", DEMOCRAT},
			    {RECORD_MAGIC, "Nixon", "Richard", REPUBLICAN},
			    {RECORD_MAGIC, "Ford", "Gerald", REPUBLICAN},
			    {RECORD_MAGIC, "Carter", "Jimmy", DEMOCRAT},
			    {RECORD_MAGIC, "Reagan", "Ronald", REPUBLICAN},
			    {RECORD_MAGIC, "Bush", "George", REPUBLICAN}
			};
    record		*r;
    static char		*order_string[] =
			{
			    "last name", "first name", "party"
			};
    static int		(*comp_func[])() =
			{
			    last_name_sort, first_name_sort, party_sort
			};

    comp_func[0] = last_name_sort;
    if ((tree = rb_create("First test", 3, comp_func)) == RB_TREE_NULL)
    {
	fputs("rb_create() bombed\n", stderr);
	exit (1);
    }

    for (i = 0; i < 10; ++i)
	rb_insert(tree, (void *) &(pres[i]));

    for (i = 0; i < 3; ++i)
    {
	r = (record *) rb_min(tree, i);

	printf("Smallest %s is for %s %s\n",
	    order_string[i], r -> first, r -> last);
    }
}
