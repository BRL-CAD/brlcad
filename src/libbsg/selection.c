/*                  S E L E C T I O N . C
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
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
/** @file libbsg/selection.c
 *
 * First-class selection model for BSG interaction records.
 */

#include "common.h"

#include "bu/malloc.h"
#include "bu/ptbl.h"

#include "bsg/defines.h"
#include "bsg/interaction.h"
#include "bsg/selection.h"


struct bsg_selection {
    struct bu_ptbl records;
};


struct bsg_selection *
bsg_selection_create(void)
{
    struct bsg_selection *sel;
    BU_ALLOC(sel, struct bsg_selection);
    bu_ptbl_init(&sel->records, 8, "bsg_selection records");
    return sel;
}


void
bsg_selection_destroy(struct bsg_selection *sel)
{
    if (!sel)
	return;
    for (size_t i = 0; i < BU_PTBL_LEN(&sel->records); i++) {
	struct bsg_interaction_record *record =
	    (struct bsg_interaction_record *)BU_PTBL_GET(&sel->records, i);
	bsg_interaction_record_free(record);
    }
    bu_ptbl_free(&sel->records);
    bu_free(sel, "bsg_selection");
}


void
bsg_selection_add_record(struct bsg_selection *sel,
			 const struct bsg_interaction_record *record)
{
    if (!sel || !record)
	return;
    for (size_t i = 0; i < BU_PTBL_LEN(&sel->records); i++) {
	struct bsg_interaction_record *existing =
	    (struct bsg_interaction_record *)BU_PTBL_GET(&sel->records, i);
	if (bsg_interaction_record_equal(existing, record))
	    return;
    }
    struct bsg_interaction_record *clone = bsg_interaction_record_clone(record);
    if (!clone)
	return;
    bu_ptbl_ins(&sel->records, (long *)clone);
}


void
bsg_selection_remove_record(struct bsg_selection *sel,
			    const struct bsg_interaction_record *record)
{
    if (!sel || !record)
	return;
    size_t i = BU_PTBL_LEN(&sel->records);
    while (i > 0) {
	i--;
	struct bsg_interaction_record *existing =
	    (struct bsg_interaction_record *)BU_PTBL_GET(&sel->records, i);
	if (!bsg_interaction_record_equal(existing, record))
	    continue;
	bu_ptbl_rm(&sel->records, (long *)existing);
	bsg_interaction_record_free(existing);
    }
}


void
bsg_selection_clear(struct bsg_selection *sel)
{
    if (!sel)
	return;
    for (size_t i = 0; i < BU_PTBL_LEN(&sel->records); i++) {
	struct bsg_interaction_record *record =
	    (struct bsg_interaction_record *)BU_PTBL_GET(&sel->records, i);
	bsg_interaction_record_free(record);
    }
    bu_ptbl_reset(&sel->records);
}


int
bsg_selection_contains_record(const struct bsg_selection *sel,
			      const struct bsg_interaction_record *record)
{
    if (!sel || !record)
	return 0;
    for (size_t i = 0; i < BU_PTBL_LEN(&sel->records); i++) {
	struct bsg_interaction_record *existing =
	    (struct bsg_interaction_record *)BU_PTBL_GET(&sel->records, i);
	if (bsg_interaction_record_equal(existing, record))
	    return 1;
    }
    return 0;
}


size_t
bsg_selection_count(const struct bsg_selection *sel)
{
    if (!sel)
	return 0;
    return BU_PTBL_LEN(&sel->records);
}


const struct bsg_interaction_record *
bsg_selection_record(const struct bsg_selection *sel, size_t idx)
{
    if (!sel || idx >= BU_PTBL_LEN(&sel->records))
	return NULL;
    return (const struct bsg_interaction_record *)BU_PTBL_GET(&sel->records, idx);
}


/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
