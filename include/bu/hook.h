/*                        H O O K . H
 * BRL-CAD
 *
 * Copyright (c) 2004-2016 United States Government as represented by
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
#include "bu/list.h"


__BEGIN_DECLS

/** @addtogroup bu_log
 *
 * @brief
 * BRL-CAD support library, error logging routines.
 *
 * Note that the user may provide his own logging routine, by replacing these
 * functions.  That is why this is in file of its own.  For example, LGT and
 * RTSRV take advantage of this.
 *
 * Here is an example of how to set up a custom logging callback.  While bu_log
 * presently writes to STDERR by default, this behavior should not be relied
 * upon and may be changed to STDOUT in the future without notice.
 *
 @code
 --- BEGIN EXAMPLE ---

 int log_output_to_file(void *data, void *str)
 {
   FILE *fp = (FILE *)data;
   fprintf(fp, "LOG: %s", str);
   return 0;
 }

 int main(int ac, char *av[])
 {
   FILE *fp = fopen("whatever.log", "w+");
   bu_log_add_hook(log_output_to_file, (void *)fp);
   bu_log("Logging to file.\n");
   bu_log_delete_hook(log_output_to_file, (void *)fp);
   bu_log("Logging to stderr.\n");
   fclose(fp);
   return 0;
 }

 --- END EXAMPLE ---
 @endcode
 *
 */
/** @{ */
/** @file bu/log.h */


/** log indentation hook */
typedef int (*bu_hook_t)(void *, void *);

struct bu_hook_list {
    struct bu_list l; /**< linked list */
    bu_hook_t hookfunc; /**< function to call */
    void *clientdata; /**< data for caller */
};
typedef struct bu_hook_list bu_hook_list_t;
#define BU_HOOK_LIST_NULL ((struct bu_hook_list *) 0)

/**
 * assert the integrity of a non-head node bu_hook_list struct.
 */
#define BU_CK_HOOK_LIST(_hl) BU_CKMAG(_hl, BU_HOOK_LIST_MAGIC, "bu_hook_list")

/**
 * initialize a bu_hook_list struct without allocating any memory.
 * this macro is not suitable for initialization of a list head node.
 */
#define BU_HOOK_LIST_INIT(_hl) { \
	BU_LIST_INIT_MAGIC(&(_hl)->l, BU_HOOK_LIST_MAGIC); \
	(_hl)->hookfunc = (_hl)->clientdata = NULL; \
    }

/**
 * macro suitable for declaration statement initialization of a
 * bu_hook_list struct.  does not allocate memory.  not suitable for
 * initialization of a list head node.
 */
#define BU_HOOK_LIST_INIT_ZERO { {BU_HOOK_LIST_MAGIC, BU_LIST_NULL, BU_LIST_NULL}, NULL, NULL }

/**
 * returns truthfully whether a non-head node bu_hook_list has been
 * initialized via BU_HOOK_LIST_INIT() or BU_HOOK_LIST_INIT_ZERO.
 */
#define BU_HOOK_LIST_IS_INITIALIZED(_p) (((struct bu_hook_list *)(_p) != BU_HOOK_LIST_NULL) && LIKELY((_p)->l.magic == BU_HOOK_LIST_MAGIC))

/** @brief BRL-CAD support library's hook utility. */
BU_EXPORT extern void bu_hook_list_init(struct bu_hook_list *hlp);
BU_EXPORT extern void bu_hook_add(struct bu_hook_list *hlp,
				  bu_hook_t func,
				  void *clientdata);
BU_EXPORT extern void bu_hook_delete(struct bu_hook_list *hlp,
				     bu_hook_t func,
				     void *clientdata);
BU_EXPORT extern void bu_hook_call(struct bu_hook_list *hlp,
				   void *buf);
BU_EXPORT extern void bu_hook_save_all(struct bu_hook_list *hlp,
				       struct bu_hook_list *save_hlp);
BU_EXPORT extern void bu_hook_delete_all(struct bu_hook_list *hlp);
BU_EXPORT extern void bu_hook_restore_all(struct bu_hook_list *hlp,
					  struct bu_hook_list *restore_hlp);

/** @} */

__END_DECLS

#endif  /* BU_LOG_H */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
