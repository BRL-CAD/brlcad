/*                         D E N S I T Y . H
 * BRL-CAD
 *
 * Copyright (c) 2008-2021 United States Government as represented by
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
/** @addtogroup libanalyze
 *
 * Density related routines
 *
 */
/** @{ */
/** @file analyze/density.h */

#ifndef ANALYZE_DENSITY_H
#define ANALYZE_DENSITY_H

#include "common.h"
#include "vmath.h"
#include "analyze/defines.h"

__BEGIN_DECLS

/*
 *     Density specific structures
 */

#define DENSITY_MAGIC 0xaf0127

/* the entries in the density table */
struct density_entry {
    uint32_t magic;
    double grams_per_cu_mm;
    char *name;
};

/**
 *     Loading information from a .density file and querying it.
 */
#if defined(__cplusplus)
extern "C" {
#endif
struct analyze_densities_impl;
struct analyze_densities {
    struct analyze_densities_impl *i;
};
#if defined(__cplusplus)
}
#endif

ANALYZE_EXPORT extern int analyze_densities_create(struct analyze_densities **a);
ANALYZE_EXPORT extern void analyze_densities_destroy(struct analyze_densities *a);
ANALYZE_EXPORT extern int analyze_densities_init(struct analyze_densities *a);
ANALYZE_EXPORT extern void analyze_densities_clear(struct analyze_densities *a);

/* Update an entry if one matching id already exists, else create a new entry.
 * Unlike the loading and writing functions, this command uses g/mm^3 */
ANALYZE_EXPORT extern int analyze_densities_set(struct analyze_densities *a, long int id, fastf_t density, const char *name, struct bu_vls *msgs);

/* Accepts a buffer, typically read from a .density file.  Expects units of g/cm^3.
 * (TODO - Ugh - need to come up with a way to define the units in the file so we aren't
 * tied to that forever...)
 *
 * Returns the number of valid density entries loaded. The optional ecnt variable, if
 * supplied, will count the number of lines where the parser failed to recognize either
 * a commented line or a valid density.  Calling codes may then decide if they want to
 * accept the partial result or fail hard based on the ecnt. */
ANALYZE_EXPORT extern int analyze_densities_load(struct analyze_densities *a, const char *buff, struct bu_vls *msgs, int *ecnt);

/* Creates a .density buffer from a, writing units of g/cm^3.  Returns length of buff
 * (TODO - Ugh - need to come up with a way to define the units so we aren't
 * tied to g/cm^3 that forever...) */
ANALYZE_EXPORT extern long int analyze_densities_write(char **buff, struct analyze_densities *a);

/* This mapping will be unique - each id may have at most one entry in the database. Caller
 * is responsible for freeing the resulting string. */
ANALYZE_EXPORT extern char *analyze_densities_name(struct analyze_densities *a, long int id);

/* Because this mapping is not guaranteed to be unique, this function has to be
 * a bit more sophisticated in what it does.
 *
 * If ids is NULL or max_ids == 0, it will return the count of the ids
 * associated with name in the database.
 *
 * If an ids array is available, it returns two pieces of data - the id array
 * of length max_ids (defined by the caller) will hold up to max_ids material
 * IDs that have the specified name.  In this mode, the return code will either
 * be the number of IDs written to ids (if positive) or the number of
 * additional ids found that would not fit in the ids array (if negative).
 */
ANALYZE_EXPORT extern long int analyze_densities_id(long int *ids, long int max_ids, struct analyze_densities *a, const char *name);

/* Note: to use a name for density lookup, supply one of the outputs of
 * analyze_densities_id to this function.
 *
 * Density is returned in g/mm^3 */
ANALYZE_EXPORT extern fastf_t analyze_densities_density(struct analyze_densities *a, long int id);

/* Provide a means to iterate over all defined densities.  To get the lowest valid id,
 * supply -1 to curr_id.  Given a valid id, supply it to curr_id to get the next highest
 * id.  When there isn't a valid next id, -1 is returned. To count the number of density
 * definitions active, do the following:
 *
 * long int curr_id = -1;
 * long int dcnt = 0;
 * while ((curr_id = analyze_densities_next(a, curr_id)) != -1) dcnt++;
 *
 */
ANALYZE_EXPORT extern long int analyze_densities_next(struct analyze_densities *a, long int curr_id);

__END_DECLS

#endif /* ANALYZE_DENSITY_H */

/** @} */

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
