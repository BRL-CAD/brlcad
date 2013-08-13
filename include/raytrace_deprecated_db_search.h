/* Deprecated search API, included by raytrace.h until time to remove it */


/**
 * search the database using a supplied list of filter criteria.
 * db_search_full_paths returns a bu_list of db_full_path structs to
 * instances of objects matching the filter criteria.  Note that this is
 * a full path tree search of the entire database, not just the toplevel
 * objects that would be reported by the ls command.  E.g., a
 * database with the following objects:
 *
 *       r1            r2
 *       |             |
 *       s1            s1
 *
 * would, if searched from the top level for s1,  return both
 *
 *  /r1/s1
 *
 * and
 *
 *  /r2/s1
 *
 * instead of just s1.  To iterate over the results, see examples of
 * iterating over bu_list structures.  (Bear in mind the db_full_path
 * structures in the list are individually malloced.)
 *
 * To return only unique objects, use
 * db_search_unique_objects, which would return just
 *
 * s1
 *
 * in the above example.  db_search_unique_objects returns a bu_ptbl of
 * (struct directory *) pointers.  To iterate over this list use
 * BU_PTBL_LEN to get the size of the table and BU_PTBL_GET in a for
 * loop to access each element.
 *
 */

/* search.c */

struct db_full_path_list {
    struct bu_list l;
    struct db_full_path *path;
    int local;
};

/**
 * Add an object to the db_full_path_list based on its database object name
 */
RT_EXPORT extern int db_full_path_list_add(const char *path, int local, struct db_i *dbip, struct db_full_path_list *path_list);

/**
 * Free all entries and the list of a db_full_path_list
 */
RT_EXPORT extern void db_free_full_path_list(struct db_full_path_list *path_list);

/**
 * Low level command to process the command line and create a "plan" corresponding to the
 * command arguments.
 */
RT_EXPORT extern void *db_search_formplan(char **argv,
					  struct db_i *dbip,
					  struct rt_wdb *wdbp);

/**
 * release memory for the formulated plan returned by
 * db_search_formplan().
 */
RT_EXPORT extern void db_search_freeplan(void **plan);

/**
 * Low level routines for invocation of search plans
 */
RT_EXPORT extern struct db_full_path_list *db_search_full_paths(void *searchplan,
								struct db_full_path_list *path_list,
								struct db_i *dbip,
								struct rt_wdb *wdbp);

RT_EXPORT extern struct bu_ptbl *db_search_unique_objects(void *searchplan,
							  struct db_full_path_list *path_list,
							  struct db_i *dbip,
							  struct rt_wdb *wdbp);

/**
 * Use the string form of a search plan to build and execute a search
 */
RT_EXPORT extern struct db_full_path_list *db_search_full_paths_strplan(const char *plan_string,
								struct db_full_path_list *path_list,
								struct db_i *dbip,
								struct rt_wdb *wdbp);

RT_EXPORT extern struct bu_ptbl *db_search_unique_objects_strplan(const char *plan_string,
							  struct db_full_path_list *path_list,
							  struct db_i *dbip,
							  struct rt_wdb *wdbp);


