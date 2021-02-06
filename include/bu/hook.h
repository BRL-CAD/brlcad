/*                        H O O K . H
 * BRL-CAD
 *
 * Copyright (c) 2004-2021 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */

#ifndef BU_HOOK_H
#define BU_HOOK_H

#include "common.h"

#include "bu/defines.h"


__BEGIN_DECLS

/** @addtogroup bu_log
 *
 * These are hook routines for keeping track of callback functions.
 *
 */
/** @{ */
/** @file bu/log.h */


/** log indentation hook */
typedef int (*bu_hook_t)(void *, void *);

struct bu_hook {
    bu_hook_t hookfunc; /**< function to call */
    void *clientdata; /**< data for caller */
};


struct bu_hook_list {
    size_t size, capacity;
    struct bu_hook *hooks; /**< linked list */
};
typedef struct bu_hook bu_hook_list_t;


/**
 * macro suitable for declaration statement initialization of a
 * bu_hook_list struct.  does not allocate memory.  not suitable for
 * initialization of a list head node.
 */
#define BU_HOOK_LIST_INIT_ZERO { 0, 0, NULL}


/**
 * returns truthfully whether a non-head node bu_hook_list has been
 * initialized via BU_HOOK_LIST_INIT() or BU_HOOK_LIST_INIT_ZERO.
 */
#define BU_HOOK_LIST_IS_INITIALIZED(_p) ((_p)->capacity != 0)


/**
 * initialize a hook list to empty
 *
 * the caller is responsible for ensuring memory is not leaked.
 */
BU_EXPORT extern void bu_hook_list_init(struct bu_hook_list *hlp);


/**
 * add a hook to the list.
 *
 * in addition to the callback, the call may optionally provide a data
 * pointer that will get passed as the first argument to the 'func'
 * hook function.  the hook function may be NULL, which will result in
 * a no-op (skipped) when bu_hook_call() is called.
 */
BU_EXPORT extern void bu_hook_add(struct bu_hook_list *hlp,
				  bu_hook_t func,
				  void *clientdata);


/**
 * delete a hook from the list.
 *
 * this removes a specified callback function registered with a
 * particular data pointer via bu_hook_add() from the hook list.
 */
BU_EXPORT extern void bu_hook_delete(struct bu_hook_list *hlp,
				     bu_hook_t func,
				     void *clientdata);


/**
 * call all registered hooks.
 *
 * this invokes all callbacks added via bu_hook_add() passing any data
 * pointer as the first argument and the provided 'buf' argument as
 * the second argument, either of which may be NULL if desired.
 */
BU_EXPORT extern void bu_hook_call(struct bu_hook_list *hlp,
				   void *buf);


/**
 * copy all hooks from one list to another
 */
BU_EXPORT extern void bu_hook_save_all(struct bu_hook_list *source,
				       struct bu_hook_list *destination);


/**
 * delete all hooks in a list
 */
BU_EXPORT extern void bu_hook_delete_all(struct bu_hook_list *hlp);


/**
 * replace all hooks in a list with the hooks from another list
 *
 * all hooks from ther 'destination' hook list will be deleted and all
 * hooks in the 'source' list will be copied into the 'destination'
 * list.
 */
BU_EXPORT extern void bu_hook_restore_all(struct bu_hook_list *destination,
					  struct bu_hook_list *source);

/** @} */

__END_DECLS

#endif  /* BU_HOOK_H */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
