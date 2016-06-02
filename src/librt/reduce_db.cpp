/*            R E D U C E _ H I E R A R C H Y . C P P
 * BRL-CAD
 *
 * Copyright (c) 2016 United States Government as represented by
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
/** @file reduce_hierarchy.cpp
 *
 * Reduce a database hierarchy.
 *
 */


#include "common.h"

#include "rt/db_internal.h"
#include "rt/db_io.h"
#include "rt/nongeom.h"
#include "rt/search.h"
#include "rt/tree.h"

#include <set>


namespace
{


template <typename T>
void
autoptr_wrap_bu_free(T *ptr)
{
    bu_free(ptr, "AutoPtr");
}


template <typename T, void free_fn(T *) = autoptr_wrap_bu_free>
struct AutoPtr {
    explicit AutoPtr(T *vptr = NULL) :
	ptr(vptr)
    {}


    ~AutoPtr()
    {
	if (ptr)
	    free_fn(ptr);
    }


    T *ptr;


private:
    AutoPtr(const AutoPtr &source);
    AutoPtr &operator=(const AutoPtr &source);
};


HIDDEN void
remove_dead_references_leaf_func(db_i *db, rt_comb_internal *comb, tree *tree,
				 void *user1, void *UNUSED(user2), void *UNUSED(user3), void *UNUSED(user4))
{
    RT_CK_DBI(db);
    RT_CK_COMB(comb);
    RT_CK_TREE(tree);

    std::set<std::string> &failed_members = *static_cast<std::set<std::string> *>
					    (user1);

    if (!db_lookup(db, tree->tr_l.tl_name, false))
	failed_members.insert(tree->tr_l.tl_name);
}


HIDDEN void
remove_dead_references(db_i &db)
{
    RT_CK_DBI(&db);

    AutoPtr<directory *> comb_dirs;
    std::size_t num_combs = db_ls(&db, DB_LS_COMB, NULL, &comb_dirs.ptr);

    for (std::size_t i = 0; i < num_combs; ++i) {
	rt_db_internal internal;

	if (0 > rt_db_get_internal(&internal, comb_dirs.ptr[i], &db, NULL,
				   &rt_uniresource))
	    bu_bomb("rt_db_get_internal() failed");

	AutoPtr<rt_db_internal, rt_db_free_internal> autofree_internal(&internal);

	rt_comb_internal &comb = *static_cast<rt_comb_internal *>(internal.idb_ptr);
	RT_CK_COMB(&comb);

	std::set<std::string> failed_members;
	db_tree_funcleaf(&db, &comb, comb.tree, remove_dead_references_leaf_func,
			 &failed_members, NULL, NULL, NULL);

	for (std::set<std::string>::const_iterator it = failed_members.begin();
	     it != failed_members.end(); ++it)
	    if (0 != db_tree_del_dbleaf(&comb.tree, it->c_str(), &rt_uniresource, 0))
		bu_bomb("db_tree_del_dbleaf() failed");

	if (!comb.tree || !db_tree_nleaves(comb.tree)) {
	    if (db_delete(&db, comb_dirs.ptr[i]) || db_dirdelete(&db, comb_dirs.ptr[i]))
		bu_bomb("failed to delete directory");

	    remove_dead_references(db);
	    return;
	} else {
	    if (rt_db_put_internal(comb_dirs.ptr[i], &db, &internal, &rt_uniresource))
		bu_bomb("rt_db_put_internal() failed");
	    else
		autofree_internal.ptr = NULL;
	}
    }
}


}


extern "C"
{
    void
    rt_reduce_db(db_i *db)
    {
	RT_CK_DBI(db);

	remove_dead_references(*db);
    }
}


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
