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

    fprintf(stderr, "last_name_sort('%s', '%s')\n", r1 -> last, r2 -> last);
    fflush(stderr);
    return(strcmp(r1 -> last, r2 -> last));
}

first_name_sort (void *v1, void *v2)
{
    record *r1 = v1;
    record *r2 = v2;

    fprintf(stderr, "first_name_sort('%s', '%s')\n", r1 -> first, r2 -> first);
    fflush(stderr);
    return(strcmp(r1 -> first, r2 -> first));
}

party_sort (void *v1, void *v2)
{
    record *r1 = v1;
    record *r2 = v2;

    fprintf(stderr, "party_sort(%d, %d)\n", r1 -> party, r2 -> party);
    fflush(stderr);
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
    int		i;
    rb_tree	*tree;
    record	pres[6];
    record	*r;
    static char	*order_string[] =
		{
		    "last name", "first name", "party"
		};
    static int	(*comp_func[])() =
		{
		    last_name_sort, first_name_sort, party_sort
		};

    comp_func[0] = last_name_sort;
    if ((tree = rb_create("First test", 3, comp_func)) == RB_TREE_NULL)
    {
	fputs("rb_create() bombed\n", stderr);
	exit (1);
    }
    rb_describe(tree);

    /*
     *	Fill the records
     */
    pres[0].magic = RECORD_MAGIC;
    strcpy(pres[0].last, "Roosevelt");
    strcpy(pres[0].first, "Franklin");
    pres[0].party = DEMOCRAT;
    /* */
    pres[1].magic = RECORD_MAGIC;
    strcpy(pres[1].last, "Truman");
    strcpy(pres[1].first, "Harry");
    pres[1].party = DEMOCRAT;
    /* */
    pres[2].magic = RECORD_MAGIC;
    strcpy(pres[2].last, "Eisenhower");
    strcpy(pres[2].first, "Dwight");
    pres[2].party = REPUBLICAN;
    /* */
    pres[3].magic = RECORD_MAGIC;
    strcpy(pres[3].last, "Kennedy");
    strcpy(pres[3].first, "John");
    pres[3].party = DEMOCRAT;
    /* */
    pres[4].magic = RECORD_MAGIC;
    strcpy(pres[4].last, "Johnson");
    strcpy(pres[4].first, "Lyndon");
    pres[4].party = DEMOCRAT;
    /* */
    pres[5].magic = RECORD_MAGIC;
    strcpy(pres[5].last, "Nixon");
    strcpy(pres[5].first, "Richard");
    pres[5].party = REPUBLICAN;

    printf("pres = %x = (void *) %x\n", pres, (void *) pres);
    for (i = 0; i < 6; ++i)
    {
	rb_insert(tree, (void *) &(pres[i]));
	fprintf(stderr, "Reached line %d\n", __LINE__);fflush(stderr);
    }
    rb_describe(tree);
    fprintf(stderr, "Reached line %d\n", __LINE__);fflush(stderr);

    for (i = 0; i < 3; ++i)
    {
	r = (record *) rb_min(tree, i);
	fprintf(stderr, "Reached line %d\n", __LINE__);fflush(stderr);

	printf("Smallest %s is for %s %s\n",
	    order_string[i], r -> first, r -> last);
    }
}
