/* alloc.c - memory allocator for fixed size blocks */

/*

This code is replacement for malloc() and free().  It takes advantage
of the fact that all of my memory allocation is of known fixed-size
blocks.  This particular implementation, however, is extremely general
and will do allocation of any number of different fixed-size blocks.

I will just give a simple example here.  To allocate struct foo's, declare a handle to the foo space as:

    struct freelist_head foo_freelist;

Initialize it:

    memory_init(&foo_freelist,sizeof(struct foo),1000,200);

Here we have asked for an initial allocation of 1000 foo's.  When that
runs out, further allocations will automatically be performed 200
foo's at a time.  (Each allocation calls sbrk() so you want to
minimize them.)

To actually allocate and deallocate foo's, it helps to define two
macros as follow:

    #define foo_new()   (struct foo *)new(&foo_freelist)
    #define foo_destroy(x)  destroy(&oct_freelist_head,(Freelist *)(char *)x)

Now you can say things like:

    foo1 = foo_new();
    foo_destroy(foo1);

*/

#include <stdlib.h>
#include <string.h>

#include "express/alloc.h"
#include "express/error.h"

/* just in case we are compiling by hand */
#ifndef ALLOC
#define ALLOC
#endif /*ALLOC*/

/** chop up big block into linked list of small blocks
 * return 0 for failure
 * \param flh freelist head
 * \param bytes new memory size
 */
Freelist * create_freelist( struct freelist_head * flh, int bytes ) {
    Freelist * current = ( Freelist * )malloc( bytes );
    if( current == 0 ) {
        return( 0 );
    }

    flh->freelist = current;

#ifndef NOSTAT
    flh->create++;

    /* set max to point to end of freelist */
    if( ( char * )flh->freelist + bytes > ( char * )flh->max ) {
        flh->max = ( char * )flh->freelist + bytes;
    }
#endif

    while( ( char * )current + flh->size <
            ( ( char * )flh->freelist + bytes ) ) {
        current->next = ( Freelist * )( &current->memory + flh->size );
        current = current->next;
    }
    current->next = NULL;
    return( current );
}

void
_ALLOCinitialize() {
#ifdef DEBUG_MALLOC
    malloc_debug( 2 );
#endif
}

/**
 * \param flh freelist head
 * \param size size of a single element
 * \param alloc1 number to allocate initially
 * \param alloc2 number to allocate if we run out
 */
void ALLOCinitialize( struct freelist_head * flh, unsigned int size, int alloc1, int alloc2 ) {
    flh->size_elt = size;   /* kludge for calloc-like behavior */
#ifndef NOSTAT
    flh->alloc = flh->dealloc = flh->create = 0;
    flh->max = 0;
#endif

    /* make block large enough to hold the linked list pointer */
    flh->size = ( size > sizeof( Freelist * ) ? size : sizeof( Freelist * ) );
    /* set up for future allocations */
    flh->bytes = flh->size * alloc2;

#ifdef REAL_MALLOC
    return;
    /*NOTREACHED*/
#else
    if( 0 == create_freelist( flh, flh->size * alloc1 ) ) {
        ERRORnospace();
    }

#ifdef SPACE_PROFILE
    flh->count = 0;
#endif /*SPACE_PROFILE*/

#endif
}

void * ALLOC_new( struct freelist_head * flh ) {
    void *obj;

#ifndef NOSTAT
    flh->alloc++;
#endif

#ifdef REAL_MALLOC
    return( calloc( 1, flh->size_elt ) );
    /*NOTREACHED*/
#else
    if( flh->freelist == NULL && 0 == create_freelist( flh, flh->bytes ) ) {
        ERRORnospace();
    }

    obj = &flh->freelist->memory;
    flh->freelist = flh->freelist->next;

#ifndef NOSTAT
    if( obj > flh->max ) {
        abort();
    }
#endif

#ifdef SPACE_PROFILE
    flh->count++;
#endif /*SPACE_PROFILE*/

    /* calloc-like */
    memset( obj, 0, flh->size_elt );

    return( obj );
#endif
}

void ALLOC_destroy( struct freelist_head * flh, Freelist * link ) {
#ifndef NOSTAT
    flh->dealloc++;
#endif

#ifdef REAL_MALLOC
    free( link );
    return;
    /*NOTREACHED*/
#else

    link->next = flh->freelist;
    flh->freelist = link;

#ifdef SPACE_PROFILE
    flh->count--;
#endif /*SPACE_PROFILE*/

#endif
}

#ifdef ALLOC_MAIN
struct freelist_head oct_freelist;

#define new_oct()    (struct oct *)new(&oct_freelist)
#define destroy_oct(oct) (destroy(&oct_freelist,(Freelist *)(char *)oct))

struct oct {
    char a[16];
};

main() {
    struct oct * o1, *o2, *o3, *o4, *o5, *o6;

    memory_init( &oct_freelist, sizeof( struct oct ), 5, 2 );

    o1 = new_oct();
    fprintf( stderr, "o1 = %x\n", o1 );
    o2 = new_oct();
    fprintf( stderr, "o2 = %x\n", o2 );
    o3 = new_oct();
    fprintf( stderr, "o3 = %x\n", o3 );
    o4 = new_oct();
    fprintf( stderr, "o4 = %x\n", o4 );
    o5 = new_oct();
    fprintf( stderr, "o5 = %x\n", o5 );
    o6 = new_oct();
    fprintf( stderr, "o6 = %x\n", o6 );
    destroy_oct( o1 );
    destroy_oct( o2 );
    o1 = new_oct();
    fprintf( stderr, "o1 = %x\n", o1 );
    o2 = new_oct();
    fprintf( stderr, "o2 = %x\n", o2 );
    o3 = new_oct();
    fprintf( stderr, "o3 = %x\n", o3 );
    o4 = new_oct();
    fprintf( stderr, "o4 = %x\n", o4 );
    o5 = new_oct();
    fprintf( stderr, "o5 = %x\n", o5 );
}
#endif /*ALLOC_MAIN*/
