/*                        S E A R C H . H
 * BRL-CAD
 *
 * Copyright (c) 2008-2014 United States Government as represented by
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
/** @addtogroup librt */
/** @{ */
/** @file search.h
 *
 * Functionality for searching .g files
 */

#ifndef RT_SEARCH_H
#define RT_SEARCH_H

#include "common.h"

#include "bu/list.h"
#include "bu/ptbl.h"

#include "./defines.h"

/**
 * Programmatic interface to the find-command style search functionality
 * available in librt for databases.  These functions search the
 * database using a supplied list of filter criteria and return either
 * db_full_path instances or directory pointers (depending on the function).
 * Both types of returns use a bu_ptbl container to hold the set of results.
 *
 * Design notes:
 *
 * * As long as struct db_i retains its pointer back to its parent rt_wdb
 *   structure, and dbip is a parameter in rt_wdb, only one of the two is
 *   needed as a parameter and either will work.  Probably go with rt_wdb,
 *   since it isn't tagged as private within the data structure definition,
 *   but on the other hand some ways db_i would be preferable since it
 *   would allow the search functions to break out cleanly into a
 *   hypothetical libgio/libdb that is separate from the raytracing.
 *   Unfortunately, the need to get to the internal form of some of the
 *   primitives means we do need rt_wdb available for now.  If the ways
 *   search currently is accessing rt_wdb could be avoided with a future
 *   improvement/redesign, then db_i and using the private link to rt_wdb
 *   is justified.  Need to discuss before this API is finalized.
 *
 * * Plan strings are the most intuitive way for humans to spell out a search
 *   pattern - db_search_formplan becomes a behind-the-scenes function that
 *   the user then doesn't have to worry about.  Only counterargument would
 *   be re-using plans already built from a string, and the slight overhead
 *   of rebuilding the plan from a string for repeated search calls isn't
 *   sufficient justification for the added API complexity without hard
 *   evidence that complexity is needed.
 *
 * * Offer simple function calls for the common cases of one path and an
 *   array of paths, and for both input cases support returning either
 *   a db_full_path set or a unique directory pointer set via table.  This
 *   should cover the most common programmatic situations, while still
 *   allowing commands enough flexibility to do what they need to (see,
 *   for example, combining results from search sets (multiple arrays
 *   of paths) in libged's search result consolidation.)
 *
 * * Need to add a plan option for dealing with hidden geometry during the search,
 *   maybe -nohide or something like that...  The traversal by default shouldn't
 *   traverse down anything hidden, but we should be able to override that at user request.
 *
 * WARNING:  THESE FUNCTIONS ARE STILL IN DEVELOPMENT - IT IS NOT YET
 * ASSUMED THAT THE SEARCH API IS IN ITS FINAL FORM - DO NOT DEPEND ON IT
 * REMAINING THE SAME UNTIL THIS WARNING IS REMOVED
 *
 */
#define DB_SEARCH_TREE             0x0   /**< @brief Do a hierarchy-aware search.  This is the default. */
#define DB_SEARCH_FLAT             0x1   /**< @brief Do a flat search without hierarchy */
#define DB_SEARCH_HIDDEN           0x2   /**< @brief Search using hidden objects */
#define DB_SEARCH_RETURN_UNIQ_DP   0x4   /**< @brief Return the set of unique directory pointers instead of full paths */
RT_EXPORT extern int db_search(struct bu_ptbl *results,
                               const char *plan_string,
                               int path_cnt,
                               struct directory **paths,
                               struct rt_wdb *wdbp,
                               int s_flags);

/**
 * Properly free the tables returned by db_search
 */
RT_EXPORT extern void db_free_search_tbl(struct bu_ptbl *search_results);



/***************************************************************
 * DEPRECATED - all structures and functions below this notice
 * are replaced by functionality above.  Remove after 7.27.0
 * development starts.
 ***************************************************************/

struct db_full_path_list {
    struct bu_list l;
    struct db_full_path *path;
    int local;
};
DEPRECATED RT_EXPORT extern int db_full_path_list_add(const char *path, int local, struct db_i *dbip, struct db_full_path_list *path_list);
DEPRECATED RT_EXPORT extern void db_free_full_path_list(struct db_full_path_list *path_list);
DEPRECATED RT_EXPORT extern void *db_search_formplan(char **argv,
					  struct db_i *dbip,
					  struct rt_wdb *wdbp);
DEPRECATED RT_EXPORT extern void db_search_freeplan(void **plan);
DEPRECATED RT_EXPORT extern struct db_full_path_list *db_search_full_paths(void *searchplan,
								struct db_full_path_list *path_list,
								struct db_i *dbip,
								struct rt_wdb *wdbp);
DEPRECATED RT_EXPORT extern struct bu_ptbl *db_search_unique_objects(void *searchplan,
							  struct db_full_path_list *path_list,
							  struct db_i *dbip,
							  struct rt_wdb *wdbp);


#endif /* RT_SEARCH_H*/
/** @} */
/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
