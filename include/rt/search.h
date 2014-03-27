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
/** @file include/rt/search.h
 *
 * Functionality for searching .g files
 */

#ifndef RT_SEARCH_H
#define RT_SEARCH_H

#include "common.h"

#include "bu/list.h"
#include "bu/ptbl.h"

#include "rt/defines.h"

#define DB_SEARCH_TREE             0x0   /**< @brief Do a hierarchy-aware search.  This is the default. */
#define DB_SEARCH_FLAT             0x1   /**< @brief Do a flat search without hierarchy */
#define DB_SEARCH_HIDDEN           0x2   /**< @brief Search using hidden objects */
#define DB_SEARCH_RETURN_UNIQ_DP   0x4   /**< @brief Return the set of unique directory pointers instead of full paths */
#define DB_SEARCH_QUIET            0x8   /**< @brief Silence all warnings */

/**
 * @brief Search for objects in a geometry database using filters
 *
 * The db_search function is a programmatic interface to the find-command
 * style search functionality available in librt for databases.  This
 * function searches the database using a supplied list of filter criteria.
 *
 * @param[out] results A bu_ptbl holding the pointers to either db_full_path or directory objects - these are the results of the search process.
 * @param search_flags A bit field (32 bit integer) for setting various search options.
 * @param plan_string  A string defining the filters to be used in the search.
 * @param path_cnt     The number of paths supplied in the paths array.
 * @param paths        The array of directory paths to use for search inputs.  If NULL, db_search will generate a list of all toplevel objects to use.
 * @param dbip         The database instance pointer corresponding to the current geometry database.
 *
 * @return Negative return values are an indication of a problem with the search, and non-negative values indicate a successful search.  Non-negative returns will correspond to the number of objects found.
 *
 * @retval -2 Return code when db_search is called with a NULL dbip.
 * @retval -1 Return code when the plan search string is invalid.
 * @retval 0  Return code when the search completed successfully but no matches were found.
 * @retval >0 Return code when the search completed successfully and matched one or more objects.
 *
 * The following example assumes a database instance pointer (dbip) is available and ready to use.
 *
 * @code
 * int i = 0;
 * struct bu_ptbl results = BU_PTBL_INIT_ZERO;
 * struct bu_vls fullpath_str = BU_VLS_INIT_ZERO;
 * const char *plan = "-name *.s -or -name *.r"
 * if (db_search(&results, DB_SEARCH_HIDDEN | DB_SEARCH_QUIET , plan, 0, NULL, dbip) > 0) {
 *     for (i = (int)BU_PTBL_LEN(&results) - 1; i >= 0; i--) {
 * 	bu_vls_trunc(&fullpath_str, 0);
 * 	db_fullpath_to_vls(&fullpath_str, (struct db_full_path *)BU_PTBL_GET(&results, i), NULL, 0);
 * 	bu_log("%s\n", bu_vls_addr(&fullpath_string));
 *     }
 * }
 * db_free_search_tbl(&results);
 * @endcode
 *
 */
RT_EXPORT extern int db_search(struct bu_ptbl *results,
                               int search_flags,
                               const char *plan_string,
                               int path_cnt,
                               struct directory **paths,
                               struct rt_wdb *wdbp
);

/**
 * Properly free the table contents returned by db_search.  The bu_ptbl
 * itself, if not put on the stack, will need to be freed by the same
 * calling function that allocated it.
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
