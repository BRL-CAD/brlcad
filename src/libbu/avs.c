/*                           A V S . C
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

#include "common.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

#include "bu/avs.h"
#include "bu/debug.h"
#include "bu/log.h"
#include "bu/malloc.h"
#include "bu/str.h"

#define AVS_ALLOCATION_INCREMENT 32

void
bu_avs_init_empty(struct bu_attribute_value_set *avsp)
{
    avsp->magic = BU_AVS_MAGIC;
    avsp->count = 0;
    avsp->max = 0;
    avsp->readonly_min = avsp->readonly_max = NULL;
    avsp->avp = (struct bu_attribute_value_pair *)NULL;
}


void
bu_avs_init(struct bu_attribute_value_set *avsp, size_t len, const char *str)
{
    if (UNLIKELY(bu_debug & BU_DEBUG_AVS))
	bu_log("bu_avs_init(%p, len=%zu, %s)\n", (void *)avsp, len, str);

    avsp->magic = BU_AVS_MAGIC;
    if (UNLIKELY(len == 0))
	len = AVS_ALLOCATION_INCREMENT + AVS_ALLOCATION_INCREMENT;
    avsp->count = 0;
    avsp->max = len;
    avsp->readonly_min = avsp->readonly_max = NULL;
    avsp->avp = (struct bu_attribute_value_pair *)bu_calloc(avsp->max, sizeof(struct bu_attribute_value_pair), str);
}


struct bu_attribute_value_set *
bu_avs_new(size_t len, const char *str)
{
    struct bu_attribute_value_set *avsp;

    BU_ALLOC(avsp, struct bu_attribute_value_set);
    bu_avs_init(avsp, len, "bu_avs_new");

    if (UNLIKELY(bu_debug & BU_DEBUG_AVS))
	bu_log("bu_avs_new(len=%zu, %s) = %p\n", len, str, (void *)avsp);

    return avsp;
}


int
bu_avs_add(struct bu_attribute_value_set *avsp, const char *name, const char *value)
{
    struct bu_attribute_value_pair *app;

    /* no work? */
    if (!avsp && !name)
	return 0;

    /* no source is okay even though nothing is added */
    if (UNLIKELY(!name)) {
	return 0;
    }
    /* empty is unexpected, but not a reason to halt */
    if (UNLIKELY(strlen(name) == 0)) {
	bu_log("WARNING: bu_avs_add() received an attribute name with zero length\n");
	return 0;
    }

    /* no target is not okay */
    BU_CK_AVS(avsp);

    /* does this attribute already exist? */
    if (avsp->count) {
	for (BU_AVS_FOR(app, avsp)) {
	    if (!BU_STR_EQUAL(app->name, name))
		continue;

	    /* found a match, replace it fully */
	    if (app->name && AVS_IS_FREEABLE(avsp, app->name))
		bu_free((void *)app->name, "app->name");
	    if (app->value && AVS_IS_FREEABLE(avsp, app->value))
		bu_free((void *)app->value, "app->value");
	    app->name = bu_strdup(name);
	    if (value) {
		app->value = bu_strdup(value);
	    } else {
		app->value = (char *)NULL;
	    }
	    return 1;
	}
    }

    if (avsp->count >= avsp->max) {
	/* Allocate more space first */
	avsp->max += AVS_ALLOCATION_INCREMENT;
	if (avsp->avp) {
	    avsp->avp = (struct bu_attribute_value_pair *)bu_realloc(
		avsp->avp,  avsp->max * sizeof(struct bu_attribute_value_pair),
		"attribute_value_pair.avp[] (add)");
	} else {
	    avsp->avp = (struct bu_attribute_value_pair *)bu_calloc(
		avsp->max, sizeof(struct bu_attribute_value_pair),
		"attribute_value_pair.avp[] (add)");
	}
    }

    /* add the new attribute */
    app = &avsp->avp[avsp->count++];
    /* FIXME: returning 0 when app is null causes crashing problems
     * (observed in comgeom-g, probably in others - need to understand
     * why. */
    if (app) {
	app->name = bu_strdup(name);
	if (value) {
	    app->value = bu_strdup(value);
	} else {
	    app->value = (char *)NULL;
	}
    }
    return 2;
}


int
bu_avs_add_vls(struct bu_attribute_value_set *avsp, const char *name, const struct bu_vls *value_vls)
{
    BU_CK_AVS(avsp);
    BU_CK_VLS(value_vls);

    return bu_avs_add(avsp, name, bu_vls_addr(value_vls));
}


void
bu_avs_merge(struct bu_attribute_value_set *dest, const struct bu_attribute_value_set *src)
{
    struct bu_attribute_value_pair *app;

    BU_CK_AVS(dest);
    BU_CK_AVS(src);

    if (src->count < 1) {
	return;
    }

    for (BU_AVS_FOR(app, src)) {
	(void)bu_avs_add(dest, app->name, app->value);
    }
}


const char *
bu_avs_get(const struct bu_attribute_value_set *avsp, const char *name)
{
    struct bu_attribute_value_pair *app;

    BU_CK_AVS(avsp);

    if (avsp->count < 1)
	return NULL;

    if (!name)
	return NULL;

    for (BU_AVS_FOR(app, avsp)) {
	if (!BU_STR_EQUAL(app->name, name)) {
	    continue;
	}
	return app->value;
    }
    return NULL;
}

const char *
bu_avs_get_all(const struct bu_attribute_value_set *avsp, const char *title) {
    struct bu_attribute_value_pair *avpp;
    size_t i;
    struct bu_vls str = BU_VLS_INIT_ZERO;

    BU_CK_AVS(avsp);

    bu_vls_init(&str);
    if (title) {
        bu_vls_strcat(&str, title);
        bu_vls_strcat(&str, "=\"");
    }

    avpp = avsp->avp;
    for (i = 0; i < avsp->count; i++, avpp++) {
        bu_vls_strcat(&str, "\t\t(");
        bu_vls_strcat(&str, avpp->name ? avpp->name : "NULL");
        bu_vls_strcat(&str, " : ");
        bu_vls_strcat(&str, avpp->value ? avpp->value : "NULL");
        bu_vls_strcat(&str, ")\n");
    }

    if (title) {
        bu_vls_strcat(&str, "\"");
    }

    const char * attributes = bu_vls_strgrab(&str);

    return attributes;
}


int
bu_avs_remove(struct bu_attribute_value_set *avsp, const char *name)
{
    struct bu_attribute_value_pair *app, *epp;

    BU_CK_AVS(avsp);

    if (UNLIKELY(!name)) {
	return -1;
    }

    if (avsp->count < 1) {
	return -1;
    }

    for (BU_AVS_FOR(app, avsp)) {
	if (!BU_STR_EQUAL(app->name, name))
	    continue;
	if (app->name && AVS_IS_FREEABLE(avsp, app->name))
	    bu_free((void *)app->name, "app->name");
	app->name = NULL;	/* sanity */
	if (app->value && AVS_IS_FREEABLE(avsp, app->value))
	    bu_free((void *)app->value, "app->value");
	app->value = NULL;	/* sanity */

	/* Move last one down to replace it */
	epp = &avsp->avp[--avsp->count];
	if (app != epp) {
	    *app = *epp; /* struct copy */
	}

	/* sanity */
	epp->name = NULL;
	epp->value = NULL;
    }

    return 0;
}


void
bu_avs_free(struct bu_attribute_value_set *avsp)
{
    struct bu_attribute_value_pair *app;

    BU_CK_AVS(avsp);

    if (UNLIKELY(avsp->max < 1))
	return;

    if (avsp->count) {
	for (BU_AVS_FOR(app, avsp)) {
	    if (app->name && AVS_IS_FREEABLE(avsp, app->name)) {
		bu_free((void *)app->name, "app->name");
	    }
	    app->name = NULL;	/* sanity */
	    if (app->value && AVS_IS_FREEABLE(avsp, app->value)) {
		bu_free((void *)app->value, "app->value");
	    }
	    app->value = NULL;	/* sanity */
	}
	avsp->count = 0;
    }
    if (LIKELY(avsp->avp != NULL)) {
	bu_free((void *)avsp->avp, "bu_avs_free avsp->avp");
	avsp->avp = NULL; /* sanity */
	avsp->max = 0;
    }
}


void
bu_avs_print(const struct bu_attribute_value_set *avsp, const char *title)
{
    struct bu_attribute_value_pair *avpp;
    size_t i;

    BU_CK_AVS(avsp);

    if (title) {
	bu_log("%s: %zu attributes:\n", title, avsp->count);
    }

    avpp = avsp->avp;
    for (i = 0; i < avsp->count; i++, avpp++) {
	bu_log("  %s = %s\n",
	       avpp->name ? avpp->name : "NULL",
	       avpp->value ? avpp->value : "NULL");
    }
}


void
bu_avs_add_nonunique(struct bu_attribute_value_set *avsp, const char *name, const char *value)
{
    struct bu_attribute_value_pair *app;

    BU_CK_AVS(avsp);

    /* nothing to do */
    if (UNLIKELY(name == NULL)) {
	if (UNLIKELY(value != NULL)) {
	    bu_log("WARNING: bu_avs_add_nonunique given NULL name and non-null value\n");
	}
	return;
    }

    if (avsp->count >= avsp->max) {
	/* Allocate more space first */
	avsp->max += AVS_ALLOCATION_INCREMENT;
	if (avsp->avp) {
	    avsp->avp = (struct bu_attribute_value_pair *)bu_realloc(
		avsp->avp,  avsp->max * sizeof(struct bu_attribute_value_pair),
		"attribute_value_pair.avp[] (add)");
	} else {
	    avsp->avp = (struct bu_attribute_value_pair *)bu_malloc(
		avsp->max * sizeof(struct bu_attribute_value_pair),
		"attribute_value_pair.avp[] (add)");
	}
    }

    app = &avsp->avp[avsp->count++];
    app->name = bu_strdup(name);
    if (value) {
	app->value = bu_strdup(value);
    } else {
	app->value = (char *)NULL;
    }
}


/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
