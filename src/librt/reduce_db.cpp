/*                   R E D U C E _ D B . C P P
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
/** @file reduce_db.cpp
 *
 * Reduce a database hierarchy.
 *
 */


#include "common.h"

#include "bu/str.h"
#include "rt/db_internal.h"
#include "rt/db_io.h"
#include "rt/nongeom.h"
#include "rt/search.h"
#include "rt/tree.h"
#include "rt/db_attr.h"

#include <map>
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
	    if (db_tree_del_dbleaf(&comb.tree, it->c_str(), &rt_uniresource, 0))
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


namespace
{


class Hierarchy;


struct Combination {
    Combination();
    Combination(db_i &db, directory &dir);
    Combination(const Combination &source);
    Combination &operator=(const Combination &source);

    void write();

    bool has_unmergeable_member_comb(const Hierarchy &hierarchy,
				     const Combination &other) const;
    void merge_member_comb_if_present(const Combination &other);


    struct Member {
	Member(directory &dir, int operation, const fastf_t *matrix);

	directory *m_dir;
	int m_operation;
	mat_t m_matrix;
    };


    db_i *m_db;
    directory *m_dir;
    std::list<Member> m_members;
    std::map<std::string, std::string> m_attributes;
};


class Hierarchy
{
public:
    explicit Hierarchy(db_i &db);

    void merge();
    void write();

    typedef std::map<directory *, Combination> Combs;
    Combs m_combinations;

private:
    db_i &m_db;
    std::set<directory *> m_removed;
};


void
Hierarchy::merge()
{
    for (Combs::iterator it = m_combinations.begin(); it != m_combinations.end();) {
	bool possible = true;

	// check if the current comb can be merged by all parents
	for (Combs::iterator j_it = m_combinations.begin();
	     j_it != m_combinations.end(); ++j_it)
	    if (j_it->second.has_unmergeable_member_comb(*this, it->second)) {
		possible = false;
		break;
	    }

	if (!possible) {
	    ++it;
	    continue;
	}

	for (Combs::iterator j_it = m_combinations.begin();
	     j_it != m_combinations.end(); ++j_it)
	    j_it->second.merge_member_comb_if_present(it->second);

	// now, `it` can be removed from the database; defer until write()
	if (!m_removed.insert(it->first).second)
	    bu_bomb("already processed");

	const Combs::iterator temp = it++;
	m_combinations.erase(temp);
    }
}


void
Hierarchy::write()
{
    for (std::set<directory *>::const_iterator it = m_removed.begin();
	 it != m_removed.end(); ++it)
	if (db_delete(&m_db, *it) || db_dirdelete(&m_db, *it))
	    bu_bomb("failed to delete object");

    for (Combs::iterator it = m_combinations.begin(); it != m_combinations.end();
	 ++it)
	it->second.write();
}


Hierarchy::Hierarchy(db_i &db) :
    m_combinations(),
    m_db(db),
    m_removed()
{
    db_update_nref(&db, &rt_uniresource);

    AutoPtr<directory *> comb_dirs;
    std::size_t num_combs = db_ls(&db, DB_LS_COMB, NULL, &comb_dirs.ptr);

    for (std::size_t i = 0; i < num_combs; ++i)
	m_combinations[comb_dirs.ptr[i]] = Combination(db, *comb_dirs.ptr[i]);
}


Combination::Member::Member(directory &dir, int operation,
			    const fastf_t *matrix) :
    m_dir(&dir),
    m_operation(operation),
    m_matrix()
{
    if (matrix)
	MAT_COPY(m_matrix, matrix);
    else
	MAT_IDN(m_matrix);
}


Combination::Combination() :
    m_db(NULL),
    m_dir(NULL),
    m_members(),
    m_attributes()
{}


Combination::Combination(db_i &db, directory &dir) :
    m_db(&db),
    m_dir(&dir),
    m_members(),
    m_attributes()
{
    rt_db_internal internal;

    if (0 > rt_db_get_internal(&internal, m_dir, &db, NULL, &rt_uniresource))
	bu_bomb("rt_db_get_internal() failed");

    AutoPtr<rt_db_internal, rt_db_free_internal> autofree_internal(&internal);

    rt_comb_internal &comb = *static_cast<rt_comb_internal *>(internal.idb_ptr);
    RT_CK_COMB(&comb);

    const std::size_t node_count = db_tree_nleaves(comb.tree);
    AutoPtr<rt_tree_array> tree_list((struct rt_tree_array *)bu_calloc(node_count,
				     sizeof(rt_tree_array), "tree list"));
    const std::size_t actual_count = (struct rt_tree_array *)db_flatten_tree(
					 tree_list.ptr, comb.tree, OP_UNION, false, &rt_uniresource) - tree_list.ptr;
    BU_ASSERT_SIZE_T(actual_count, ==, node_count);

    for (std::size_t i = 0; i < node_count; ++i) {
	struct directory * const temp = db_lookup(&db,
					tree_list.ptr[i].tl_tree->tr_l.tl_name, true);
	m_members.push_back(Member(*temp, tree_list.ptr[i].tl_op,
				   tree_list.ptr[i].tl_tree->tr_l.tl_mat));
    }

    bu_attribute_value_set avs;
    const AutoPtr<bu_attribute_value_set, bu_avs_free> autofree_avs;

    if (db5_get_attributes(m_db, &avs, m_dir))
	bu_bomb("db5_get_attributes() failed");

    for (std::size_t i = 0; i < avs.count; ++i)
	m_attributes[avs.avp[i].name] = avs.avp[i].value;
}


Combination::Combination(const Combination &source) :
    m_db(source.m_db),
    m_dir(source.m_dir),
    m_members(source.m_members),
    m_attributes(source.m_attributes)
{}


Combination &
Combination::operator=(const Combination &source)
{
    if (this != &source) {
	m_db = source.m_db;
	m_dir = source.m_dir;
	m_members = source.m_members;
	m_attributes = source.m_attributes;
    }

    return *this;
}


bool
Combination::has_unmergeable_member_comb(const Hierarchy &hierarchy,
	const Combination &other) const
{
    if (!other.m_dir->d_nref)
	return true;

    bool is_member = false;

    for (std::list<Member>::const_iterator it = m_members.begin();
	 it != m_members.end(); ++it)
	if (it->m_dir == other.m_dir) {
	    if (it->m_operation != OP_UNION)
		return true;
	    else
		is_member = true;
	}

    if (!is_member)
	return false;

    for (std::map<std::string, std::string>::const_iterator it =
	     other.m_attributes.begin(); it != other.m_attributes.end(); ++it) {
	// TODO: allow the user to specify ignored attributes
	if (it->first == "rhino::type" || it->first == "rhino::uuid") continue;

	const std::map<std::string, std::string>::const_iterator found =
	    m_attributes.find(it->first);

	if (found == m_attributes.end() || found->second != it->second)
	    for (std::list<Member>::const_iterator kt = m_members.begin();
		 kt != m_members.end(); ++kt) {
		const Combination &temp = hierarchy.m_combinations.at(kt->m_dir);

		if (!temp.m_attributes.count(it->first)
		    || temp.m_attributes.at(it->first) != it->second)
		    return true;
	    }
    }

    for (std::list<Member>::const_iterator it = other.m_members.begin();
	 it != other.m_members.end(); ++it)
	if (it->m_operation != OP_UNION)
	    return true;

    return false;
}


void
Combination::merge_member_comb_if_present(const Combination &other)
{
    bool is_member = false;

    for (std::list<Member>::iterator it = m_members.begin(); it != m_members.end();)
	if (it->m_dir == other.m_dir) {
	    for (std::list<Member>::const_iterator member_it = other.m_members.begin();
		 member_it != other.m_members.end(); ++member_it) {
		Member temp = *member_it;
		bn_mat_mul2(it->m_matrix, temp.m_matrix);
		m_members.push_back(temp);
	    }

	    std::list<Member>::iterator next = it;
	    ++next;
	    m_members.erase(it);
	    it = next;
	    is_member = true;
	} else
	    ++it;

    if (is_member)
	for (std::map<std::string, std::string>::const_iterator it =
		 other.m_attributes.begin(); it != other.m_attributes.end(); ++it)
	    m_attributes[it->first] = it->second;
}


void Combination::write()
{
    if (m_members.empty())
	bu_bomb("combination has no members");

    rt_db_internal internal;

    if (0 > rt_db_get_internal(&internal, m_dir, m_db, NULL, &rt_uniresource))
	bu_bomb("rt_db_get_internal() failed");

    AutoPtr<rt_db_internal, rt_db_free_internal> autofree_internal(&internal);

    rt_comb_internal &comb = *static_cast<rt_comb_internal *>(internal.idb_ptr);
    RT_CK_COMB(&comb);

    db_free_tree(comb.tree, &rt_uniresource);
    AutoPtr<rt_tree_array> nodes((struct rt_tree_array *)bu_calloc(m_members.size(),
				 sizeof(rt_tree_array), "nodes"));

    std::size_t i = 0;
    for (std::list<Member>::const_iterator it = m_members.begin();
	 it != m_members.end(); ++it, ++i) {
	nodes.ptr[i].tl_op = it->m_operation;
	tree *&ptree = nodes.ptr[i].tl_tree;
	BU_GET(ptree, tree);
	RT_TREE_INIT(ptree);
	ptree->tr_l.tl_op = OP_DB_LEAF;
	ptree->tr_l.tl_mat = static_cast<fastf_t *>(bu_calloc(1, sizeof(mat_t),
			     "tl_mat"));
	MAT_COPY(ptree->tr_l.tl_mat, it->m_matrix);
	ptree->tr_l.tl_name = bu_strdup(it->m_dir->d_namep);
    }

    comb.tree = db_mkbool_tree(nodes.ptr, m_members.size(), &rt_uniresource);

    if (!comb.tree)
	bu_bomb("db_mkbool_tree() failed");

    if (rt_db_put_internal(m_dir, m_db, &internal, &rt_uniresource))
	bu_bomb("rt_db_put_internal() failed");

    bu_attribute_value_set avs;
    bu_avs_init(&avs, m_attributes.size(), "avs");

    for (std::map<std::string, std::string>::const_iterator it =
	     m_attributes.begin(); it != m_attributes.end(); ++it)
	if (2 != bu_avs_add(&avs, it->first.c_str(), it->second.c_str()))
	    bu_bomb("bu_avs_add() failed");

    if (db5_replace_attributes(m_dir, &avs, m_db))
	bu_bomb("db5_replace_attributes() failed");
}


}


extern "C"
{
    void
    rt_reduce_db(db_i *db)
    {
	RT_CK_DBI(db);

	remove_dead_references(*db);
	Hierarchy temp(*db);
	temp.merge();
	temp.write();
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
