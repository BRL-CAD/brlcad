static char rcsid[] = "$Id: hash.c,v 1.8 1997/10/22 16:36:49 sauderd Exp $";

/*
 * Dynamic hashing, after CACM April 1988 pp 446-457, by Per-Ake Larson.
 * Coded into C, with minor code improvements, and with hsearch(3) interface, 
 * by ejp@ausmelb.oz, Jul 26, 1988: 13:16;
 * also, hcreate/hdestroy routines added to simulate hsearch(3).
 *
 * move to local hash table, cleanup abstraction and storage allocation;
 * convert to ANSII C.
 * by snc (clark@cme.nbs.gov), 10-Mar-1989
 *
 * these routines simulate hsearch(3) and family, with the important
 * difference that the hash table is dynamic - can grow indefinitely
 * beyond its original size (as supplied to hcreate()).
 *
 * Performance appears to be comparable to that of hsearch(3).
 * The 'source-code' options referred to in hsearch(3)'s 'man' page
 * are not implemented; otherwise functionality is identical.
 *
 * Compilation controls:
 * HASH_DEBUG controls some informative traces, mainly for debugging.
 * HASH_STATISTICS causes HashAccesses and HashCollisions to be maintained;
 * when combined with HASH_DEBUG, these are displayed by hdestroy().
 *
 * Problems & fixes to ejp@ausmelb.oz. WARNING: relies on pre-processor
 * concatenation property, in probably unnecessary code 'optimisation'.
 *
 * snc: fixed concatenation for ANSII C, 10-Mar-1989
 *
 * $Log: hash.c,v $
 * Revision 1.8  1997/10/22 16:36:49  sauderd
 * Changed the use and definitions of the compiler macros MUL and DIV. They
 * seem to be defined and used only within hash.h and hash.c
 *
 * Revision 1.7  1997/01/21 19:19:51  dar
 * made C++ compatible
 *
 * Revision 1.6  1994/03/23  20:06:51  libes
 * botched free in hash destroy
 *
 * Revision 1.5  1993/10/15  18:49:55  libes
 * CADDETC certified
 *
 * Revision 1.4  1992/08/18  18:27:10  libes
 * changed DEBUG to HASH_DEBUG
 *
 * Revision 1.3  1992/08/18  17:16:22  libes
 * rm'd extraneous error messages
 *
 * Revision 1.2  1992/05/31  08:37:18  libes
 * multiple files
 *
 * Revision 1.1  1992/05/28  03:56:55  libes
 * Initial revision
 *
 * Revision 1.4  1992/05/14  10:15:04  libes
 * don't remember
 *
 * Revision 1.3  1992/02/12  07:06:49  libes
 * y
 * y
 * do sub/supertype
 *
 * Revision 1.2  1992/02/09  00:51:02  libes
 * does ref/use correctly
 *
 * Revision 1.1  1992/02/05  08:34:24  libes
 * Initial revision
 *
 * Revision 1.0.1.1  1992/01/22  02:40:04  libes
 * copied from ~pdes
 *
 * Revision 1.9  1992/01/22  02:26:58  libes
 * added code to test hash package
 *
 * Revision 1.8  1991/09/17  02:53:23  libes
 * fixed bug in HASHcopy (new hash tables were coming out empty)
 *
 * Revision 1.7  1991/08/30  16:34:36  libes
 * fixed HASHlist to use state from parameter instead of static
 *
 * Revision 1.6  1991/08/13  22:19:39  libes
 * forgot to declare s, s2, pp, and q.  Oops.
 *
 * Revision 1.5  1991/08/06  19:03:07  libes
 * fixed bugs relating to my previous addition (delete function)
 * added HASHcopy to support DICT_copy via OBJcopy
 *
 * Revision 1.4  1991/07/19  03:57:46  libes
 * added action HASH_DELETE
 * made action HASH_INSERT return failure upon duplicate entry
 *
 * Revision 1.3  1991/02/26  19:40:45  libes
 * Added functions for visiting every member of the hash table, to duplicate
 * similar functionality of linked lists.  Note that current implementation
 * can only walk through one hash table at a time.  This can be improved, but
 * the application code doesn't currently need such a feature.
 *
 * Revision 1.2  1990/09/06  11:06:56  clark
 * BPR 2.1 alpha
 *
 * Revision 1.1  90/06/11  16:44:43  clark
 * Initial revision
 * 
 */

#define HASH_C
#include <stdlib.h>
#include "express/hash.h"

/*
** Internal routines
*/

static_inline Address	HASHhash(char*, Hash_Table);
static void		HASHexpand_table(Hash_Table);

/*
** Local data
*/

# if HASH_STATISTICS
static long		HashAccesses, HashCollisions;
# endif

/*
** Code
*/

void
HASHinitialize()
{
  if ( HASH_Table_fl.size_elt == 0 )
	MEMinitialize(&HASH_Table_fl,sizeof(struct Hash_Table_),50,50);
  if ( HASH_Element_fl.size_elt == 0 )
	MEMinitialize(&HASH_Element_fl,sizeof(struct Element_),500,100);
}

Hash_Table
HASHcreate(unsigned count)
{
    int		i;
    Hash_Table	table;

    /*
    ** Adjust Count to be nearest higher power of 2, 
    ** minimum SEGMENT_SIZE, then convert into segments.
    */
    i = SEGMENT_SIZE;
    while (i < count)
	i <<= 1;
    count = DIV(i, SEGMENT_SIZE_SHIFT);

    table = HASH_Table_new();
#if 0
    table->in_use = 0;
#endif
    table->SegmentCount = table->p = table->KeyCount = 0;
    /*
    ** Allocate initial 'i' segments of buckets
    */
    for (i = 0; i < count; i++)
	CALLOC(table->Directory[i], SEGMENT_SIZE, Element)
	    /*,   "segment in HASHcreate");*/

    table->SegmentCount = count;
    table->maxp = MUL(count, SEGMENT_SIZE_SHIFT);
    table->MinLoadFactor = 1;
    table->MaxLoadFactor = MAX_LOAD_FACTOR;
# if HASH_DEBUG
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
HASHlistinit(Hash_Table table,HashEntry *he)
{
	he->i = he->j = 0;
	he->p = 0;
	he->table = table;
	he->type = '*';
#if 0
	table->in_use = 1;
#endif
}

void
HASHlistinit_by_type(Hash_Table table,HashEntry *he,char type)
{
	he->i = he->j = 0;
	he->p = 0;
	he->table = table;
	he->type = type;
#if 0
	table->in_use = 1;
#endif
}

#if 0
/* if you don't step to the end, you can clear the flag this way */
void
HASHlistend(HashEntry *he)
{
	he->table->in_use = 0;
}
#endif

/* provide a way to step through the hash */
Element
HASHlist(HashEntry *he)
{
	int i2 = he->i;
	int j2 = he->j;
	Segment s;

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
#if 0
	he->table->in_use = 0;
#endif
	return(he->e);
}

#if 0
/* this verifies no one else is walking through the table that we might screw up */
/* it should be called before adding, deleting or destroying a table */
HASH_in_use(Hash_Table table,char *action)
{
	fprintf(stderr,"HASH: attempted to %s but hash table in use\n",action);
}
#endif

void
HASHdestroy(Hash_Table table)
{
    int		i, j;
    Segment	s;
    Element	p, q;

    if (table != HASH_NULL) {
#if 0
	if (table->in_use) HASH_in_use(table,"destroy hash table");
#endif
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
		free(table->Directory[i]);
	    }
	}
	HASH_Table_destroy(table);
# if HASH_STATISTICS && HASH_DEBUG
	fprintf(stderr, 
		"[hdestroy] Accesses %ld Collisions %ld\n", 
		HashAccesses, 
		HashCollisions);
# endif
    }
}

Element
HASHsearch(Hash_Table table, Element item, Action action)
{
    Address	h;
    Segment	CurrentSegment;
    int		SegmentIndex;
    int		SegmentDir;
    Segment	p;
    Element	q;
    Element	deleteme;

    assert(table != HASH_NULL);	/* Kinder really than return(NULL); */
# if HASH_STATISTICS
    HashAccesses++;
# endif
    h = HASHhash(item->key, table);
    SegmentDir = DIV(h, SEGMENT_SIZE_SHIFT);
    SegmentIndex = MOD(h, SEGMENT_SIZE);
    /*
    ** valid segment ensured by HASHhash()
    */
    CurrentSegment = table->Directory[SegmentDir];
    assert(CurrentSegment != NULL);	/* bad failure if tripped	*/
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
    return((Element)q);
  case HASH_DELETE:
    if (!q) return(0);
    /* at this point, element exists and action == DELETE */
#if 0
	if (table->in_use) HASH_in_use(table,"insert element");
#endif
    deleteme = q;
    *p = q->next;
    /*STRINGfree(deleteme->key);*/
    HASH_Element_destroy(deleteme);
    --table->KeyCount;
    return(deleteme);	/* of course, user shouldn't deref this! */
  case HASH_INSERT:
    /* if trying to insert it (twice), let them know */
    if (q != NULL) return(q); /* was return(0);!!!!!?!?! */

#if 0
	if (table->in_use) HASH_in_use(table,"delete element");
#endif
    /* at this point, element does not exist and action == INSERT */
    q = HASH_Element_new();
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
    if (++table->KeyCount / MUL(table->SegmentCount, SEGMENT_SIZE_SHIFT) > table->MaxLoadFactor)
	HASHexpand_table(table);		/* doesn't affect q	*/
  }
  return((Element)0);	/* was return (Element)q */
}

/*
** Internal routines
*/

static_inline
Address
HASHhash(char* Key, Hash_Table table)
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
HASHexpand_table(Hash_Table table)
{
    Address	NewAddress;
    int		OldSegmentIndex, NewSegmentIndex;
    int		OldSegmentDir, NewSegmentDir;
    Segment	OldSegment, NewSegment;
    Element	Current, *Previous, *LastOfNew;

    if (table->maxp + table->p < MUL(DIRECTORY_SIZE, SEGMENT_SIZE_SHIFT)) {
	/*
	** Locate the bucket to be split
	*/
	OldSegmentDir = DIV(table->p, SEGMENT_SIZE_SHIFT);
	OldSegment = table->Directory[OldSegmentDir];
	OldSegmentIndex = MOD(table->p, SEGMENT_SIZE);
	/*
	** Expand address space; if necessary create a new segment
	*/
	NewAddress = table->maxp + table->p;
	NewSegmentDir = DIV(NewAddress, SEGMENT_SIZE_SHIFT);
	NewSegmentIndex = MOD(NewAddress, SEGMENT_SIZE);
	if (NewSegmentIndex == 0)
	    CALLOC(table->Directory[NewSegmentDir], SEGMENT_SIZE, Element);
		 /*  "segment in HASHexpand_table");*/
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

/* make a complete copy of a hash table */
/* Note that individual objects are shallow-copied.  OBJcopy is not called! */
/* But then, it isn't called when objects are inserted/deleted so this seems */
/* reasonable - DEL */
Hash_Table
HASHcopy(Hash_Table oldtable)
{
	Hash_Table newtable;
	Segment s, s2;
	Element *pp;	/* old element */
	Element	*qq;	/* new element */
	int i, j;

	newtable = HASH_Table_new();
	for (i = 0; i < oldtable->SegmentCount; i++) {
		CALLOC(newtable->Directory[i], SEGMENT_SIZE, Element);
		   /*    "segment in HASHcopy");*/
	}

	newtable->p		= oldtable->p;
	newtable->SegmentCount	= oldtable->SegmentCount;
	newtable->maxp		= oldtable->maxp;
	newtable->MinLoadFactor	= oldtable->MinLoadFactor;
	newtable->MaxLoadFactor	= oldtable->MaxLoadFactor;
	newtable->KeyCount	= oldtable->KeyCount;

	for (i=0; i<oldtable->SegmentCount; i++) {
	  /* test probably unnecessary */
	  if ((s = oldtable->Directory[i]) != NULL) {
	    s2 = newtable->Directory[i];
	    for (j = 0; j < SEGMENT_SIZE; j++) {
	      qq = &s2[j];
	      for (pp = &s[j];*pp;pp = &(*pp)->next) {
		*qq = HASH_Element_new();
/*		(*qq)->key = STRINGcopy((*pp)->key);*/
		/* I really doubt it is necessary to copy the key!!! */
		(*qq)->key = (*pp)->key;
		(*qq)->data = (*pp)->data;
		(*qq)->symbol = (*pp)->symbol;
		(*qq)->type = (*pp)->type;
		(*qq)->next = NULL;
		qq = &((*qq)->next);
	      }
            }
          }
        }
	return(newtable);
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
