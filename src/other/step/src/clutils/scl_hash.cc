/*  "$Id: scl_hash.cc,v 3.0.1.2 1997/11/05 22:33:50 sauderd DP3.1 $"; */

/*
 * Dynamic hashing, after CACM April 1988 pp 446-457, by Per-Ake Larson.
 * Coded into C, with minor code improvements, and with hsearch(3) interface, 
 * by ejp@ausmelb.oz, Jul 26, 1988: 13:16;
 * also, hcreate/hdestroy routines added to simulate hsearch(3).
 */

#ifdef __O3DB__
#include <OpenOODB.h>
#endif
#include <scl_hash.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*************/
/* constants */
/*************/

/*#define NULL			0*/
#define HASH_NULL		(Hash_TableP)NULL

#define SEGMENT_SIZE		256
#define SEGMENT_SIZE_SHIFT	8	/* log2(SEGMENT_SIZE)	*/
#define PRIME1			37
#define PRIME2			1048583
#define MAX_LOAD_FACTOR	5

/*void *malloc(unsigned);*/
/*void *calloc(unsigned,unsigned);*/

/************/
/* typedefs */
/************/

typedef unsigned long Address;

/******************************/
/* macro function definitions */
/******************************/

/*
** Fast arithmetic, relying on powers of 2
*/

#define MUL(x,y)		((x) << (y##_SHIFT))
#define DIV(x,y)		((x) >> (y##_SHIFT))
#define MOD(x,y)		((x) & ((y)-1))

/*#define HASH_Table_new()	malloc(sizeof(struct Hash_Table))*/
#define HASH_Table_new()	new Hash_Table
/*#define HASH_Table_destroy(x)	free((char *) x)*/
#define HASH_Table_destroy(x)	delete x
/*#define HASH_Element_new()	malloc(sizeof(struct Element))*/
#define HASH_Element_new()	new Element
/*#define HASH_Element_destroy(x)	free((char *) x)*/
#define HASH_Element_destroy(x)	delete x

typedef struct Element *ElementP;
typedef struct Hash_Table *Hash_TableP;

/*
** Internal routines
*/

Address		HASHhash(char*, Hash_TableP);
static void	HASHexpand_table(Hash_TableP);

# if HASH_STATISTICS
static long		HashAccesses, HashCollisions;
# endif

void *
HASHfind(Hash_TableP t,char *s)
{
	struct Element e;
	struct Element *ep;

	e.key = s;
	e.symbol =0;  /*  initialize to 0 - 25-Apr-1994 - kcm */
	ep = HASHsearch(t,&e,HASH_FIND);
	return(ep?ep->data:0);
}

void
HASHinsert(Hash_TableP t,char *s,void *data)
{
	struct Element e, *e2;

	e.key = s;
	e.data = data;
	e.symbol =0;  /*  initialize to 0 - 25-Apr-1994 - kcm */
	e2 = HASHsearch(t,&e,HASH_INSERT);
	if (e2) {
		printf("Redeclaration of %s\n",s);
	}
}

Hash_TableP
HASHcreate(unsigned count)
{
    int		i;
    Hash_TableP	table;

    /*
    ** Adjust Count to be nearest higher power of 2, 
    ** minimum SEGMENT_SIZE, then convert into segments.
    */
    i = SEGMENT_SIZE;
    while (i < count)
	i <<= 1;
    count = DIV(i, SEGMENT_SIZE);

    table = (Hash_TableP) HASH_Table_new();
    table->SegmentCount = table->p = (short) (table->KeyCount = 0);
    /*
    ** First initialize directory to 0\'s 
    ** DIRECTORY_SIZE must be same as in header
    */
    for (i = 0; i < DIRECTORY_SIZE; i++)  
      table->Directory[i] = 0;
    /*
    ** Allocate initial 'i' segments of buckets
    */
    for (i = 0; i < count; i++)  
/*	table->Directory[i] = (struct Element **) calloc(SEGMENT_SIZE,sizeof(struct Element));*/
      {
	table->Directory[i] = new struct Element * [SEGMENT_SIZE];
	for (int h =0; h < SEGMENT_SIZE; h++)  // initialize to NULL
	  table->Directory[i][h] = 0;
      }

    table->SegmentCount = count;
    table->maxp = MUL(count, SEGMENT_SIZE);
    table->MinLoadFactor = 1;
    table->MaxLoadFactor = MAX_LOAD_FACTOR;
# if DEBUG
    fprintf(stderr, 
	    "[HASHcreate] table %x count %d maxp %d SegmentCount %d\n", 
	    table, 
	    count, 
	    table->maxp, 
	    table->SegmentCount);
# endif
# if HASH_STATISTICS
	HashAccesses = HashCollisions = 0;
# endif
    return(table);
}

/* initialize pointer to beginning of hash table so we can step through it */
/* on repeated calls to HASHlist - DEL */
void
HASHlistinit(Hash_TableP table,HashEntry *he)
{
	he->i = he->j = 0;
	he->p = 0;
	he->table = table;
	he->type = '*';
	he->e =0; /*  initialize to 0 - 25-Apr-1994 - kcm  */
}

void
HASHlistinit_by_type(Hash_TableP table,HashEntry *he,char type)
{
	he->i = he->j = 0;
	he->p = 0;
	he->table = table;
	he->type = type;
	he->e =0; /*  initialize to 0 - 25-Apr-1994 - kcm  */
}

/* provide a way to step through the hash */
struct Element *
HASHlist(HashEntry *he)
{
	int i2 = he->i;
	int j2 = he->j;
	struct Element **s;

	he->e = 0;

	for (he->i = i2; he->i < he->table->SegmentCount; he->i++) {
	    /* test probably unnecessary	*/
	    if ((s = he->table->Directory[he->i]) != NULL) {
		for (he->j = j2; he->j < SEGMENT_SIZE; he->j++) {
			if (!he->p) he->p = s[he->j];

			/* if he->p is defined, prepare to return it (by
			   setting it to he->e) and begin looking for a new value
			   for he->p
			 */
		retry:
			if (he->p) {
				if ((he->type != '*') &&
				    (he->type != he->p->type)) {
					he->p = he->p->next;
					goto retry;
				}
				if (he->e) return(he->e);
				he->e = he->p;
				he->p = he->p->next;
			}

			/* avoid incrementing he->j by returning here */
			if (he->p) return(he->e);
	        }
		j2 = 0;
	   }
	}
	/* if he->e was set then it is last one */
	return(he->e);
}

void
HASHdestroy(Hash_TableP table)
{
    int		i, j;
    struct Element **s;
    struct Element *p, *q;

    if (table != HASH_NULL) {
	for (i = 0; i < table->SegmentCount; i++) {
	    /* test probably unnecessary	*/
	    if ((s = table->Directory[i]) != NULL) {
		for (j = 0; j < SEGMENT_SIZE; j++) {
		    p = s[j];
		    while (p != NULL) {
			q = p->next;
			HASH_Element_destroy(p);
			p = q;
		    }
		}
/*		free((char *) table->Directory[i]);*/
		delete [] table->Directory[i];
	    }
	}
	HASH_Table_destroy(table);
# if HASH_STATISTICS && DEBUG
	fprintf(stderr, 
		"[hdestroy] Accesses %ld Collisions %ld\n", 
		HashAccesses, 
		HashCollisions);
# endif
    }
}

struct Element *
HASHsearch(Hash_TableP table, struct Element *item, Action action)
{
    Address	h;
    struct Element **CurrentSegment;
    int		SegmentIndex;
    int		SegmentDir;
    struct Element **p;
    struct Element *q;
    struct Element *deleteme;

# if HASH_STATISTICS
    HashAccesses++;
# endif
    h = HASHhash(item->key, table);
    SegmentDir = (int) DIV(h, SEGMENT_SIZE);
    SegmentIndex = (int) MOD(h, SEGMENT_SIZE);
    /*
    ** valid segment ensured by HASHhash()
    */
    CurrentSegment = table->Directory[SegmentDir];
    p = CurrentSegment + SegmentIndex;
    q = *p;
    /*
    ** Follow collision chain
    ** Now and after we finish this loop
    **	p = &element, and
    **	q = element
    */
    while (q != NULL && strcmp(q->key, item->key))
    {
	p = &q->next;
	q = *p;
# if HASH_STATISTICS
	HashCollisions++;
# endif
    }
    /* at this point, we have either found the element or it doesn't exist */
  switch (action) {
  case HASH_FIND:
    return((struct Element *)q);
  case HASH_DELETE:
    if (!q) return(0);
    /* at this point, element exists and action == DELETE */
    deleteme = q;
    *p = q->next;
    /*STRINGfree(deleteme->key);*/
    HASH_Element_destroy(deleteme);
    --table->KeyCount;
    return(deleteme);	/* of course, user shouldn't deref this! */
  case HASH_INSERT:
    /* if trying to insert it (twice), let them know */
    if (q != NULL) return(q); /* was return(0);!!!!!?!?! */

    /* at this point, element does not exist and action == INSERT */
    q = (ElementP) HASH_Element_new();
    *p = q;				/* link into chain	*/
    /*
    ** Initialize new element
    */
/* I don't see the point of copying the key!!!! */
/*    q->key = STRINGcopy(item->key);*/
    q->key = item->key;
    q->data = item->data;
    q->symbol = item->symbol;
    q->type = item->type;
    q->next = NULL;
    /*
    ** table over-full?
    */
    if (++table->KeyCount / MUL(table->SegmentCount, SEGMENT_SIZE) > table->MaxLoadFactor)
	HASHexpand_table(table);		/* doesn't affect q	*/
  }
  return((struct Element *)0);	/* was return (Element)q */
}

/*
** Internal routines
*/

Address
HASHhash(char* Key, Hash_TableP table)
{
    Address		h, address;
    register unsigned char	*k = (unsigned char*)Key;

    h = 0;
    /*
    ** Convert string to integer
    */
    /*SUPPRESS 112*/
    while (*k)
	/*SUPPRESS 8*/	/*SUPPRESS 112*/
	h = h*PRIME1 ^ (*k++ - ' ');
    h %= PRIME2;
    address = MOD(h, table->maxp);
    if (address < table->p)
	address = MOD(h, (table->maxp << 1));	/* h % (2*table->maxp)	*/
    return(address);
}

static
void
HASHexpand_table(Hash_TableP table)
{
    Address	NewAddress;
    int		OldSegmentIndex, NewSegmentIndex;
    int		OldSegmentDir, NewSegmentDir;
    struct Element **OldSegment, **NewSegment;
    struct Element *Current, **Previous, **LastOfNew;

    if (table->maxp + table->p < MUL(DIRECTORY_SIZE, SEGMENT_SIZE)) {
	/*
	** Locate the bucket to be split
	*/
	OldSegmentDir = DIV(table->p, SEGMENT_SIZE);
	OldSegment = table->Directory[OldSegmentDir];
	OldSegmentIndex = MOD(table->p, SEGMENT_SIZE);
	/*
	** Expand address space; if necessary create a new segment
	*/
	NewAddress = table->maxp + table->p;
	NewSegmentDir = (int) DIV(NewAddress, SEGMENT_SIZE);
	NewSegmentIndex = (int) MOD(NewAddress, SEGMENT_SIZE);
	if (NewSegmentIndex == 0)
/*    table->Directory[NewSegmentDir] = (struct Element **) calloc(SEGMENT_SIZE,sizeof(struct Element));*/
      {
	table->Directory[NewSegmentDir] = new struct Element * [SEGMENT_SIZE];
	for (int h =0; h < SEGMENT_SIZE; h++)  // initialize to NULL
	  table->Directory[NewSegmentDir][h] = 0;
      }

	NewSegment = table->Directory[NewSegmentDir];
	/*
	** Adjust state variables
	*/
	table->p++;
	if (table->p == table->maxp) {
	    table->maxp <<= 1;	/* table->maxp *= 2	*/
	    table->p = 0;
	}
	table->SegmentCount++;
	/*
	** Relocate records to the new bucket
	*/
	Previous = &OldSegment[OldSegmentIndex];
	Current = *Previous;
	LastOfNew = &NewSegment[NewSegmentIndex];
	*LastOfNew = NULL;
	while (Current != NULL) {
	    if (HASHhash(Current->key, table) == NewAddress) {
		/*
		** Attach it to the end of the new chain
		*/
		*LastOfNew = Current;
		/*
		** Remove it from old chain
		*/
		*Previous = Current->next;
		LastOfNew = &Current->next;
		Current = Current->next;
		*LastOfNew = NULL;
	    } else {
		/*
		** leave it on the old chain
		*/
		Previous = &Current->next;
		Current = Current->next;
	    }
	}
    }
}

/* following code is for testing hash package */
#ifdef HASHTEST
struct Element e1, e2, e3, *e;
struct Hash_Table *t;
HashEntry he;

main()
{
	e1.key = "foo";		e1.data = (char *)1;
	e2.key = "bar";		e2.data = (char *)2;
	e3.key = "herschel";	e3.data = (char *)3;

	t = HASHcreate(100);
	e = HASHsearch(t,&e1,HASH_INSERT);
	e = HASHsearch(t,&e2,HASH_INSERT);
	e = HASHsearch(t,&e3,HASH_INSERT);
	HASHlistinit(t,&he);
	for (;;) {
		e = HASHlist(&he);
		if (!e) exit(0);
		printf("found key %s, data %d\n",e->key,(int)e->data);
	}
}
#endif
