/*			A P P L I C A T I O N . C
 *
 *	A toy application to test libredblack
 */

#include <stdio.h>
#include <string.h>
#include "redblack.h"

#define		ORDER_LASTNAME		0
#define		ORDER_FIRSTNAME		1
#define		ORDER_PARTY		2
#define		DEMOCRAT		0
#define		REPUBLICAN		1

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
    record	*r1 = v1;
    record	*r2 = v2;

    return(strcmp(r1 -> last, r2 -> last));
}

first_name_sort (void *v1, void *v2)
{
    record	*r1 = v1;
    record	*r2 = v2;

    return(strcmp(r1 -> first, r2 -> first));
}

party_sort (void *v1, void *v2)
{
    record	*r1 = v1;
    record	*r2 = v2;

    return ((r1 -> party < r2 -> party) ? -1 :
	    (r1 -> party > r2 -> party) ? 1 : 0);
}

void describe_president (void *v)
{
    record	*p = v;

    if (*((long *)(p)) != (RECORD_MAGIC))
    {
	fprintf(stderr,
	    "Error: Bad %s pointer x%x s/b x%x was x%x, file %s, line %d\n", \
	    "president record", p, RECORD_MAGIC, *((long *)(p)),
	    __FILE__, __LINE__);
	exit (0);
    }
    printf("%-16s %-16s %s\n",
	p -> last, p -> first,
	(p -> party == DEMOCRAT) ? "Democrat" : "Republican");
    fflush(stdout);
}

/* 
 *	The main driver
 */
main ()
{
    int			i;
    int			nm_presidents;
    rb_tree		*tree;
    record		p;
    static record	pres[] = 
			{
#if 1
			    {RECORD_MAGIC, "Roosevelt", "Franklin", DEMOCRAT},
			    {RECORD_MAGIC, "Truman", "Harry", DEMOCRAT},
			    {RECORD_MAGIC, "Eisenhower", "Dwight", REPUBLICAN},
			    {RECORD_MAGIC, "Kennedy", "John", DEMOCRAT},
			    {RECORD_MAGIC, "Johnson", "Lyndon", DEMOCRAT},
			    {RECORD_MAGIC, "Nixon", "Richard", REPUBLICAN},
#endif
			    {RECORD_MAGIC, "Ford", "Gerald", REPUBLICAN},
			    {RECORD_MAGIC, "Carter", "Jimmy", DEMOCRAT},
			    {RECORD_MAGIC, "Reagan", "Ronald", REPUBLICAN},
			    {RECORD_MAGIC, "Bush", "George", REPUBLICAN},
			    {RECORD_MAGIC, "Clinton", "Bill", DEMOCRAT}
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

    nm_presidents = sizeof(pres) / sizeof(record);
    comp_func[0] = last_name_sort;
    if ((tree = rb_create("First test", 3, comp_func)) == RB_TREE_NULL)
    {
	fputs("rb_create() bombed\n", stderr);
	exit (1);
    }

    printf("There are %d presidents\n", nm_presidents);
    for (i = 0; i < nm_presidents; ++i)
	rb_insert(tree, (void *) &(pres[i]));

    for (i = 0; i < 3; ++i)
    {
	r = (record *) rb_min(tree, i);
	printf("Smallest %s is for %s %s\n",
	    order_string[i], r -> first, r -> last);
	r = (record *) rb_max(tree, i);
	printf("Largest %s is for %s %s\n",
	    order_string[i], r -> first, r -> last);
    }

    printf("\n\n\nBy last name...\n");
	rb_walk(tree, ORDER_LASTNAME, describe_president);
    printf("\n\n\nBy first name...\n");
	rb_walk(tree, ORDER_FIRSTNAME, describe_president);
    printf("\n\n\nBy party...\n");
	rb_walk(tree, ORDER_PARTY, describe_president);

    p.magic = RECORD_MAGIC;
    strcpy(p.first, "Lyndon");
    r = (record *) rb_search(tree, ORDER_FIRSTNAME, (void *) (&p));
    if (r == 0)
    {
	printf("It ain't there\n");
    }
    else
    {
	printf("I found it...\n");
	describe_president((void *) r);
    }

    {
	void left_rotate();

	printf("First names as we begin...\n");
	rb_diagnose_tree(tree, ORDER_FIRSTNAME);

	left_rotate(tree->rbt_root[ORDER_FIRSTNAME], ORDER_FIRSTNAME);
	printf("First names after left rotation...\n");
	rb_diagnose_tree(tree, ORDER_FIRSTNAME);

	right_rotate(tree->rbt_root[ORDER_FIRSTNAME], ORDER_FIRSTNAME);
	printf("First names after derotation...\n");
	rb_diagnose_tree(tree, ORDER_FIRSTNAME);

	right_rotate(tree->rbt_root[ORDER_FIRSTNAME], ORDER_FIRSTNAME);
	printf("First names after right rotation...\n");
	rb_diagnose_tree(tree, ORDER_FIRSTNAME);
    }
}
