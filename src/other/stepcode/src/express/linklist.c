

/*
 * This work was supported by the United States Government, and is
 * not subject to copyright.
 *
 * $Log: linklist.c,v $
 * Revision 1.3  1997/01/21 19:19:51  dar
 * made C++ compatible
 *
 * Revision 1.2  1993/10/15  18:49:55  libes
 * CADDETC certified
 *
 * Revision 1.4  1993/01/19  22:17:27  libes
 * *** empty log message ***
 *
 * Revision 1.3  1992/08/18  17:16:22  libes
 * rm'd extraneous error messages
 *
 * Revision 1.2  1992/06/08  18:08:05  libes
 * prettied up interface to print_objects_when_running
 */

#include "express/linklist.h"

void LISTinitialize( void ) {
}

void LISTcleanup( void ) {
}

Linked_List LISTcreate() {
    Linked_List list = LIST_new();
    list->mark = LINK_new();
    list->mark->next = list->mark->prev = list->mark;
    return( list );
}

Linked_List LISTcopy( Linked_List src ) {
    Linked_List dst = LISTcreate();
    LISTdo( src, x, void * )
    LISTadd_last( dst, x );
    LISTod
    return dst;
}


void LISTfree( Linked_List list ) {
    Link p, q = list->mark->next;

    for( p = q->next; p != list->mark; q = p, p = p->next ) {
        LINK_destroy( q );
    }
    if( q != list->mark ) {
        LINK_destroy( q );
    }
    LINK_destroy( list->mark );
    LIST_destroy( list );
}

void LISTsort( Linked_List list, int (*comp)(void*, void*)) {
    unsigned int moved;
    Link node, prev;

    if (LISTempty(list))
        return;

    while (true) {
        for ( node = list->mark->next, moved = 0; node != list->mark; node = node->next ) {
            prev = node->prev;
            if (prev != list->mark && comp(prev->data, node->data) < 0) {
                LISTswap(prev, node);
                moved++;
            }
        }
        if (moved == 0)
            break;
    }
}

void LISTswap( Link p, Link q ) {
    void *tmp;

    if( p == LINK_NULL || q == LINK_NULL || p == q )
        return;

    tmp = p->data;
    p->data = q->data;
    q->data = tmp;
}


void *LISTadd_first( Linked_List list, void *item ) {
    Link        node;

    node = LINK_new();
    node->data = item;
    ( node->next = list->mark->next )->prev = node;
    ( list->mark->next = node )->prev = list->mark;
    return item;
}

void *LISTadd_last( Linked_List list, void *item ) {
    Link        node;

    node = LINK_new();
    node->data = item;
    ( node->prev = list->mark->prev )->next = node;
    ( list->mark->prev = node )->next = list->mark;
    return item;
}

void *LISTadd_after( Linked_List list, Link link, void *item ) {
    Link node;

    if( link == LINK_NULL ) {
        LISTadd_first( list, item );
    } else {
        node = LINK_new();
        node->data = item;
        ( node->next = link->next )->prev = node;
        ( link->next = node )->prev = link;
    }
    return item;
}

void *LISTadd_before( Linked_List list, Link link, void *item ) {
    Link node;

    if( link == LINK_NULL ) {
        LISTadd_last( list, item );
    } else {
        node = LINK_new();
        node->data = item;

        link->prev->next = node;    /* fix up previous link */
        node->prev = link->prev;
        node->next = link;
        link->prev = node;      /* fix up next link */
    }
    return item;
}


void *LISTremove_first( Linked_List list ) {
    Link        node;
    void *item;

    node = list->mark->next;
    if( node == list->mark ) {
        ERRORreport( EMPTY_LIST, "LISTremove_first" );
        return NULL;
    }
    item = node->data;
    ( list->mark->next = node->next )->prev = list->mark;
    LINK_destroy( node );
    return item;
}

void *LISTget_first( Linked_List list ) {
    Link node;
    void *item;

    node = list->mark->next;
    if( node == list->mark ) {
        return NULL;
    }
    item = node->data;
    return item;
}

void *LISTget_second( Linked_List list ) {
    Link        node;
    void *item;

    node = list->mark->next;
    if( node == list->mark ) {
        return NULL;
    }
    node = node->next;
    if( node == list->mark ) {
        return NULL;
    }
    item = node->data;
    return item;
}

/** first is 1, not 0 */
void *LISTget_nth( Linked_List list, int n ) {
    int count = 1;
    Link node;

    for( node = list->mark->next; node != list->mark; node = node->next ) {
        if( n == count++ ) {
            return( node->data );
        }
    }
    return( 0 );
}

int LISTget_length( Linked_List list ) {
    Link node;
    int count = 0;

    if( !list ) {
        return 0;
    }

    for( node = list->mark->next; node != list->mark; node = node->next ) {
        count++;
    }
    return count;
}

bool LISTempty( Linked_List list ) {
    if( !list ) {
        return true;
    }
    if( list->mark->next == list->mark ) {
        return true;
    }
    return false;
}
