/*                   B U _ O B S E R V E R . H
 * BRL-CAD
 *
 * Copyright (c) 2004-2022 United States Government as represented by
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

#ifndef BU_OBSERVER_H
#define BU_OBSERVER_H

#include "common.h"

#include "bu/defines.h"
#include "bu/magic.h"
#include "bu/vls.h"

__BEGIN_DECLS

/** @addtogroup bu_observer
 * @brief
 * libbu observer.
 */
/** @{ */
/** @file bu/observer.h */

/* Generic observer cmd execution function */
typedef void (bu_observer_eval_t)(void *, const char *);

/**
 * TBD
 */
struct bu_observer {
    uint32_t magic;
    struct bu_vls observer;
    struct bu_vls cmd;
};
typedef struct bu_observer bu_observer_t;
#define BU_OBSERVER_NULL ((struct bu_observer *)0)

struct bu_observer_list {
    size_t size, capacity;
    struct bu_observer *observers;
};

/**
 * asserts the integrity of a non-head node bu_observer struct.
 */
#define BU_CK_OBSERVER(_op) BU_CKMAG(_op, BU_OBSERVER_MAGIC, "bu_observer magic")

/**
 * initializes a bu_observer struct without allocating any memory.
 */
#define BU_OBSERVER_INIT(_op) { \
	(_op)->magic = BU_OBSERVER_MAGIC; \
	BU_VLS_INIT(&(_op)->observer); \
	BU_VLS_INIT(&(_op)->cmd); \
    }

/**
 * macro suitable for declaration statement initialization of a bu_observer
 * struct.  does not allocate memory.  not suitable for a head node.
 */
#define BU_OBSERVER_INIT_ZERO { BU_OBSERVER_MAGIC, BU_VLS_INIT_ZERO, BU_VLS_INIT_ZERO }

#define BU_OBSERVER_LIST_INIT_ZERO { 0, 0, NULL }

/**
 * returns truthfully whether a bu_observer has been initialized.
 */
#define BU_OBSERVER_IS_INITIALIZED(_op) (((struct bu_observer *)(_op) != BU_OBSERVER_NULL) && LIKELY((_op)->magic == BU_OBSERVER_MAGIC))

/** @brief Routines for implementing the observer pattern. */

/**
 * runs a given command, calling the corresponding observer callback
 * if it matches.
 */
BU_EXPORT extern int bu_observer_cmd(void *clientData, int argc, const char *argv[]);

/**
 * Notify observers.
 */
BU_EXPORT extern void bu_observer_notify(void *context, struct bu_observer_list *observers, char *self, bu_observer_eval_t *ofunc);

/**
 * Free observers.
 */
BU_EXPORT extern void bu_observer_free(struct bu_observer_list *);

/** @} */

__END_DECLS

#endif  /* BU_OBSERVER_H */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
