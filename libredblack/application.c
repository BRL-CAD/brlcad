/*			A P P L I C A T I O N . C
 *
 *	A toy application to test libredblack
 *
 *	Author:	Paul Tanenbaum
 *
 */

#include <stdio.h>
#include <string.h>
#include "redblack.h"

#define		ORDER_LASTNAME		0
#define		ORDER_FIRSTNAME		1
#define		ORDER_PARTY		2
#define		DEMOCRAT		0
#define		REPUBLICAN		2
#define		ZEMOCRAT		1

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
last_name_sort (v1, v2)

void	*v1;
void	*v2;

{
    int		result;
    record	*r1 = v1;
    record	*r2 = v2;

    if (((result = strcmp(r1 -> last, r2 -> last)) == 0)
    &&	((result = strcmp(r1 -> first, r2 -> first)) == 0))
	result = (r1 -> party < r2 -> party) ? -1 :
		 (r1 -> party > r2 -> party) ?  1 : 0;

    return(result);
}

first_name_sort (v1, v2)

void	*v1;
void	*v2;

{
    int		result;
    record	*r1 = v1;
    record	*r2 = v2;

    if (((result = strcmp(r1 -> first, r2 -> first)) == 0)
    &&	((result = strcmp(r1 -> last, r2 -> last)) == 0))
	result = (r1 -> party < r2 -> party) ? -1 :
		 (r1 -> party > r2 -> party) ?  1 : 0;

    return(result);
}

party_sort (v1, v2)

void	*v1;
void	*v2;

{
    record	*r1 = v1;
    record	*r2 = v2;

    return ((r1 -> party < r2 -> party) ? -1 :
	    (r1 -> party > r2 -> party) ? 1 : 0);
}

void describe_president (v)

void	*v;

{
    record	*p = v;

    if (*((long *)(p)) != (RECORD_MAGIC))
    {
	rt_log(
	    "Error: Bad %s pointer x%x s/b x%x was x%x, file %s, line %d\n",
	    "president record", p, RECORD_MAGIC, *((long *)(p)),
	    __FILE__, __LINE__);
	exit (0);
    }
    rt_log("%-16s %-16s %s\n",
	p -> last, p -> first,
	(p -> party == DEMOCRAT) ? "Democrat" :
	(p -> party == REPUBLICAN) ? "Republican" :
	(p -> party == ZEMOCRAT) ? "Zemocrat" : "Huhh?");
    fflush(stderr);
}

void print_last_name (v)

void	*v;

{
    record	*p = v;

    rt_log("%-16s\n", p -> last);
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
			    {RECORD_MAGIC, "Washington", "George", DEMOCRAT},
			    {RECORD_MAGIC, "Adams", "John", DEMOCRAT},
			    {RECORD_MAGIC, "Jefferson", "Thomas", DEMOCRAT},
			    {RECORD_MAGIC, "Madison", "James", DEMOCRAT},
			    {RECORD_MAGIC, "Monroe", "James", DEMOCRAT},
			    {RECORD_MAGIC, "Adams", "John Quincy", DEMOCRAT},
			    {RECORD_MAGIC, "Jackson", "Andrew", DEMOCRAT},
			    {RECORD_MAGIC, "Van Buren", "Martin", DEMOCRAT},
			    {RECORD_MAGIC, "Harrison", "William", DEMOCRAT},
			    {RECORD_MAGIC, "Tyler", "James", DEMOCRAT},
			    {RECORD_MAGIC, "Polk", "James Knox", DEMOCRAT},
			    {RECORD_MAGIC, "Taylor", "Zachary", DEMOCRAT},
			    {RECORD_MAGIC, "Fillmore", "Millard", DEMOCRAT},
			    {RECORD_MAGIC, "Pierce", "Franklin", DEMOCRAT},
			    {RECORD_MAGIC, "Buchannan", "James", DEMOCRAT},
			    {RECORD_MAGIC, "Lincoln", "Abraham", DEMOCRAT},
			    {RECORD_MAGIC, "Johnson", "Andrew", DEMOCRAT},
			    {RECORD_MAGIC, "Grant", "Ulysses", DEMOCRAT},
			    {RECORD_MAGIC, "Hayes", "Rutherford", DEMOCRAT},
			    {RECORD_MAGIC, "Garfield", "James", DEMOCRAT},
			    {RECORD_MAGIC, "Arthur", "Chester", DEMOCRAT},
			    {RECORD_MAGIC, "Cleveland", "Grover", DEMOCRAT},
			    {RECORD_MAGIC, "Harrison", "Benjamin", DEMOCRAT},
			    {RECORD_MAGIC, "McKinley", "William", DEMOCRAT},
			    {RECORD_MAGIC, "Roosevelt", "Theodore", DEMOCRAT},
			    {RECORD_MAGIC, "Taft", "William", DEMOCRAT},
			    {RECORD_MAGIC, "Wilson", "Woodrow", DEMOCRAT},
			    {RECORD_MAGIC, "Harding", "Warren", DEMOCRAT},
			    {RECORD_MAGIC, "Coolidge", "Calvin", DEMOCRAT},
			    {RECORD_MAGIC, "Hoover", "Herbert", DEMOCRAT},
			    {RECORD_MAGIC, "Roosevelt", "Franklin", DEMOCRAT},
			    {RECORD_MAGIC, "Truman", "Harry", DEMOCRAT},
			    {RECORD_MAGIC, "Eisenhower", "Dwight", REPUBLICAN},
			    {RECORD_MAGIC, "Kennedy", "John", DEMOCRAT},
			    {RECORD_MAGIC, "Johnson", "Lyndon", DEMOCRAT},
			    {RECORD_MAGIC, "Nixon", "Richard", REPUBLICAN},
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
    tree -> rbt_print = describe_president;

    for (i = 0; i < nm_presidents - 1; ++i)
    {
	rb_insert(tree, (void *) &(pres[i]));
    }
    
    rt_log("Before we begin...\n");
    rb_diagnose_tree(tree, ORDER_FIRSTNAME, INORDER);
    exit (0);

    /*
     *	Delete Reagan and Eisenhower
     */
    r = (record *) rb_search(tree, ORDER_LASTNAME, (void *) (pres + 8));
    if (r != NULL)
	rb_delete(tree, ORDER_LASTNAME);
    rt_log("After deleting Reagan...\n");
    rb_diagnose_tree(tree, ORDER_LASTNAME, PREORDER);

    r = (record *) rb_search(tree, ORDER_LASTNAME, (void *) (pres + 2));
    if (r != NULL)
	rb_delete(tree, ORDER_LASTNAME);
    rt_log("After deleting Eisenhower...\n");
    rb_diagnose_tree(tree, ORDER_LASTNAME, PREORDER);
    
    /*
     *	Insert Clinton
     */
    rb_insert(tree, (void *) &(pres[nm_presidents - 1]));
    rt_log("After inserting Clinton...\n");
    rb_diagnose_tree(tree, ORDER_LASTNAME, PREORDER);

    /*
     *	Delete Roosevelt
     */
    r = (record *) rb_search(tree, ORDER_LASTNAME, (void *) (pres + 0));
    if (r != NULL)
	rb_delete(tree, ORDER_LASTNAME);
    rt_log("After deleting Roosevelt...\n");
    rb_diagnose_tree(tree, ORDER_LASTNAME, PREORDER);
}
