#ifndef LINKED_LIST_H
#define LINKED_LIST_H

/*
 * This work was supported by the United States Government, and is
 * not subject to copyright.
 *
 * $Log: linklist.h,v $
 * Revision 1.3  1997/01/21 19:15:23  dar
 * made C++ compatible
 *
 * Revision 1.2  1993/10/15  18:49:23  libes
 * CADDETC certified
 *
 * Revision 1.5  1993/03/19  20:44:12  libes
 * added included
 *
 * Revision 1.4  1993/01/19  22:17:27  libes
 * *** empty log message ***
 *
 * Revision 1.3  1992/08/18  17:15:40  libes
 * rm'd extraneous error messages
 *
 * Revision 1.2  1992/06/08  18:07:35  libes
 * prettied up interface to print_objects_when_running
 */

/*************/
/* constants */
/*************/

#define LINK_NULL   (Link)NULL
#define LIST_NULL   (Linked_List)NULL

/*****************/
/* packages used */
/*****************/

#include <sc_export.h>
#include "basic.h"
#include "alloc.h"

/************/
/* typedefs */
/************/

typedef struct Linked_List_ *Linked_List;

/****************/
/* modules used */
/****************/

#include "error.h"

/***************************/
/* hidden type definitions */
/***************************/

typedef struct Link_ {
    struct Link_   *next;
    struct Link_   *prev;
    void *data;
} *Link;

struct Linked_List_ {
    Link mark;
};

/********************/
/* global variables */
/********************/

extern SC_EXPRESS_EXPORT struct freelist_head LINK_fl;
extern SC_EXPRESS_EXPORT struct freelist_head LIST_fl;

/******************************/
/* macro function definitions */
/******************************/

#define LINK_new()  (struct Link_ *)ALLOC_new(&LINK_fl)
#define LINK_destroy(x) ALLOC_destroy(&LINK_fl,(Freelist *)x)
#define LIST_new()  (struct Linked_List_ *)ALLOC_new(&LIST_fl)
#define LIST_destroy(x) ALLOC_destroy(&LIST_fl,(Freelist *)x)

/** accessing links */
#define LINKdata(link)  (link)->data
#define LINKnext(link)  (link)->next
#define LINKprev(link)  (link)->prev

/** iteration */
#define LISTdo(list, elt, type) LISTdo_n( list, elt, type, a )

/** LISTdo_n: LISTdo with nesting
 * parameter 'uniq' changes the variable names, allowing us to nest it without -Wshadow warnings
 */
#define LISTdo_n(list, elt, type, uniq)                                         \
   {struct Linked_List_ * _ ## uniq ## l = (list);                              \
    type        elt;                                                            \
    Link        _ ## uniq ## p;                                                 \
    if( _ ## uniq ## l ) {                                                      \
    for( _ ## uniq ## p = _ ## uniq ## l->mark;                                 \
       ( _ ## uniq ## p = _ ## uniq ## p->next ) != _ ## uniq ## l->mark; ) {   \
        ( elt ) = ( type ) ( ( _ ## uniq ## p )->data );

#define LISTdo_links(list, link)                    \
   {Linked_List     __i = (list);                   \
    Link        link;                               \
    if (__i != LIST_NULL) {                         \
    for ((link) = __i->mark; ((link) = (link)->next) != __i->mark; ) {

#define LISTod  }}}

/** accessing */
#define LISTpeek_first(list)                        \
    (((struct Linked_List_*)list)->mark->next->data)

/** function aliases */
#define LISTadd_all(list, items)                    \
    LISTdo(items, e, Generic) {                     \
        LISTadd_last(list, e);                      \
    } LISTod;

/***********************/
/* function prototypes */
/***********************/

extern SC_EXPRESS_EXPORT void LISTinitialize(void);
extern SC_EXPRESS_EXPORT void LISTcleanup(void);
extern SC_EXPRESS_EXPORT Linked_List LISTcreate(void);
extern SC_EXPRESS_EXPORT Linked_List LISTcopy(Linked_List);
extern SC_EXPRESS_EXPORT void LISTsort(Linked_List, int (*comp)(void *, void *));
extern SC_EXPRESS_EXPORT void LISTswap(Link, Link);
extern SC_EXPRESS_EXPORT void   *LISTadd_first(Linked_List, void *);
extern SC_EXPRESS_EXPORT void   *LISTadd_last(Linked_List, void *);
extern SC_EXPRESS_EXPORT void   *LISTadd_after(Linked_List, Link, void *);
extern SC_EXPRESS_EXPORT void   *LISTadd_before(Linked_List, Link, void *);
extern SC_EXPRESS_EXPORT void   *LISTremove_first(Linked_List);
extern SC_EXPRESS_EXPORT void   *LISTget_first(Linked_List);
extern SC_EXPRESS_EXPORT void   *LISTget_second(Linked_List);
extern SC_EXPRESS_EXPORT void   *LISTget_nth(Linked_List, int);
extern SC_EXPRESS_EXPORT void LISTfree(Linked_List);
extern SC_EXPRESS_EXPORT int  LISTget_length(Linked_List);
extern SC_EXPRESS_EXPORT bool LISTempty(Linked_List list);

#endif /*LINKED_LIST_H*/
