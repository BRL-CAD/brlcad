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
#include "memory.h"

/************/
/* typedefs */
/************/

typedef struct Linked_List_ * Linked_List;

/****************/
/* modules used */
/****************/

#include "error.h"

/***************************/
/* hidden type definitions */
/***************************/

typedef struct Link_ {
    struct Link_  * next;
    struct Link_  * prev;
    Generic     data;
} * Link;

struct Linked_List_ {
    Link mark;
};

/********************/
/* global variables */
/********************/

extern SC_EXPRESS_EXPORT Error ERROR_empty_list;
extern SC_EXPRESS_EXPORT struct freelist_head LINK_fl;
extern SC_EXPRESS_EXPORT struct freelist_head LIST_fl;

/******************************/
/* macro function definitions */
/******************************/

#define LINK_new()  (struct Link_ *)MEM_new(&LINK_fl)
#define LINK_destroy(x) MEM_destroy(&LINK_fl,(Freelist *)(Generic)x)
#define LIST_new()  (struct Linked_List_ *)MEM_new(&LIST_fl)
#define LIST_destroy(x) MEM_destroy(&LIST_fl,(Freelist *)(Generic)x)

/** accessing links */
#define LINKdata(link)  (link)->data
#define LINKnext(link)  (link)->next
#define LINKprev(link)  (link)->prev

/** iteration */
#define LISTdo(list, elt, type)                                 \
   {struct Linked_List_ * __l = (list);                         \
    type        elt;                                            \
    Link        __p;                                            \
    if (__l) {                                                  \
    for (__p = __l->mark; (__p = __p->next) != __l->mark; ) {   \
        (elt) = (type)((__p)->data);

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
#define LISTadd(list, item) LISTadd_last(list, item)
#define LISTadd_all(list, items)                    \
    LISTdo(items, e, Generic)                       \
    LISTadd(list, e);                       \
    LISTod;

/***********************/
/* function prototypes */
/***********************/

extern SC_EXPRESS_EXPORT void LISTinitialize PROTO( ( void ) );
extern SC_EXPRESS_EXPORT void LISTcleanup PROTO( ( void ) );
extern SC_EXPRESS_EXPORT Linked_List LISTcreate PROTO( ( void ) );
extern SC_EXPRESS_EXPORT Linked_List LISTcopy PROTO( ( Linked_List ) );
extern SC_EXPRESS_EXPORT Generic  LISTadd_first PROTO( ( Linked_List, Generic ) );
extern SC_EXPRESS_EXPORT Generic  LISTadd_last PROTO( ( Linked_List, Generic ) );
extern SC_EXPRESS_EXPORT Generic  LISTadd_after PROTO( ( Linked_List, Link, Generic ) );
extern SC_EXPRESS_EXPORT Generic  LISTadd_before PROTO( ( Linked_List, Link, Generic ) );
extern SC_EXPRESS_EXPORT Generic  LISTremove_first PROTO( ( Linked_List ) );
extern SC_EXPRESS_EXPORT Generic  LISTremove PROTO( ( Linked_List, Link ) );
extern SC_EXPRESS_EXPORT Generic  LISTget_first PROTO( ( Linked_List ) );
extern SC_EXPRESS_EXPORT Generic  LISTget_second PROTO( ( Linked_List ) );
extern SC_EXPRESS_EXPORT Generic  LISTget_nth PROTO( ( Linked_List, int ) );
extern SC_EXPRESS_EXPORT void LISTfree PROTO( ( Linked_List ) );
extern SC_EXPRESS_EXPORT int  LISTget_length PROTO( ( Linked_List ) );
extern SC_EXPRESS_EXPORT bool LISTempty( Linked_List list );

#endif /*LINKED_LIST_H*/
