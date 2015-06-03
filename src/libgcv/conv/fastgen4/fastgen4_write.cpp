/*              F A S T G E N 4 _ W R I T E . C P P
 * BRL-CAD
 *
 * Copyright (c) 2015 United States Government as represented by
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
/** @file fastgen4_write.cpp
 *
 * FASTGEN4 export plugin for libgcv.
 *
 */


#include "common.h"

#include <fstream>
#include <iomanip>
#include <set>
#include <sstream>
#include <stdexcept>
#include <map>

#include "../../plugin.h"


namespace
{


static const fastf_t INCHES_PER_MM = 1.0 / 25.4;


template <typename T> HIDDEN inline void
autofreeptr_wrap_bu_free(T *ptr)
{
    bu_free(ptr, "AutoFreePtr");
}


template <typename T, void free_fn(T *) = autofreeptr_wrap_bu_free>
struct AutoFreePtr {
    AutoFreePtr(T *vptr) : ptr(vptr) {}

    ~AutoFreePtr()
    {
	if (ptr) free_fn(ptr);
    }


    T * const ptr;


private:
    AutoFreePtr(const AutoFreePtr &source);
    AutoFreePtr &operator=(const AutoFreePtr &source);
};


class RecordWriter
{
public:
    class Record;

    RecordWriter();
    virtual ~RecordWriter();


protected:
    virtual std::ostream &get_ostream() = 0;


private:
    bool m_record_open;
};


inline
RecordWriter::RecordWriter() :
    m_record_open(false)
{}


inline
RecordWriter::~RecordWriter()
{}


class RecordWriter::Record
{
public:
    Record(RecordWriter &writer);
    ~Record();

    template <typename T> Record &operator<<(const T &value);
    Record &operator<<(fastf_t value);
    Record &non_zero(fastf_t value);
    Record &text(const std::string &value);

    static std::string truncate_float(fastf_t value);


private:
    static const std::size_t FIELD_WIDTH = 8;
    static const std::size_t RECORD_WIDTH = 10;

    std::size_t m_width;
    RecordWriter &m_writer;
};


RecordWriter::Record::Record(RecordWriter &writer) :
    m_width(0),
    m_writer(writer)
{
    if (m_writer.m_record_open)
	throw std::logic_error("logic error: record open");

    m_writer.m_record_open = true;
}


RecordWriter::Record::~Record()
{
    if (m_width) m_writer.get_ostream().put('\n');

    m_writer.m_record_open = false;
}


template <typename T> RecordWriter::Record &
RecordWriter::Record::operator<<(const T &value)
{
    if (++m_width > RECORD_WIDTH)
	throw std::logic_error("logic error: invalid record width");

    std::ostringstream sstream;

    if (!(sstream << value))
	throw std::invalid_argument("failed to convert value");

    const std::string str_val = sstream.str();

    if (str_val.size() > FIELD_WIDTH)
	throw std::invalid_argument("length exceeds field width");

    m_writer.get_ostream() << std::left << std::setw(FIELD_WIDTH) << str_val;
    return *this;
}


inline
RecordWriter::Record &
RecordWriter::Record::operator<<(fastf_t value)
{
    return operator<<(truncate_float(value));
}


// ensure that the truncated value != 0.0
RecordWriter::Record &
RecordWriter::Record::non_zero(fastf_t value)
{
    std::string result = truncate_float(value);

    if (result.find_first_not_of("-0.") == std::string::npos) {
	result.resize(FIELD_WIDTH, '0');
	result.at(result.size() - 1) = '1';
    }

    return operator<<(result);
}


RecordWriter::Record &
RecordWriter::Record::text(const std::string &value)
{
    m_width = RECORD_WIDTH;
    m_writer.get_ostream() << value;
    return *this;
}


std::string
RecordWriter::Record::truncate_float(fastf_t value)
{
    std::ostringstream sstream;
    sstream << std::fixed << std::showpoint << value;
    return sstream.str().substr(0, FIELD_WIDTH);
}


class StringBuffer : public RecordWriter
{
public:
    StringBuffer(RecordWriter &writer);
    virtual ~StringBuffer();


protected:
    virtual std::ostream &get_ostream();


private:
    RecordWriter &m_writer;
    std::ostringstream m_ostringstream;
};


inline
StringBuffer::StringBuffer(RecordWriter &writer) :
    m_writer(writer),
    m_ostringstream()
{}


inline
StringBuffer::~StringBuffer()
{
    RecordWriter::Record record(m_writer);
    record.text(m_ostringstream.str());
}


inline
std::ostream &
StringBuffer::get_ostream()
{
    return m_ostringstream;
}


class FastgenWriter : public RecordWriter
{
public:
    typedef std::pair<std::size_t, std::size_t> SectionID;

    FastgenWriter(const std::string &path);
    ~FastgenWriter();

    void write_comment(const std::string &value);
    void write_section_color(const SectionID &section_id,
			     const unsigned char *color);
    void write_hole(const SectionID &surrounding_id,
		    const std::vector<SectionID> &subtracted_ids);
    void write_wall(const SectionID &surrounding_id,
		    const std::vector<SectionID> &subtracted_ids);

    SectionID take_next_section_id();


protected:
    virtual std::ostream &get_ostream();


private:
    static const std::size_t MAX_GROUP_ID = 49;
    static const std::size_t MAX_SECTION_ID = 999;

    std::size_t m_next_section_id[MAX_GROUP_ID + 1];
    bool m_record_open;
    std::ofstream m_ostream, m_colors_ostream;
};


FastgenWriter::FastgenWriter(const std::string &path) :
    m_record_open(false),
    m_ostream(path.c_str(), std::ofstream::out),
    m_colors_ostream((path + ".colors").c_str(), std::ofstream::out)
{
    m_ostream.exceptions(std::ofstream::failbit | std::ofstream::badbit);
    m_colors_ostream.exceptions(std::ofstream::failbit | std::ofstream::badbit);

    for (std::size_t i = 0; i <= MAX_GROUP_ID; ++i)
	m_next_section_id[i] = 1;
}


inline
FastgenWriter::~FastgenWriter()
{
    Record(*this) << "ENDDATA";
}


inline std::ostream &
FastgenWriter::get_ostream()
{
    return m_ostream;
}


FastgenWriter::SectionID
FastgenWriter::take_next_section_id()
{
    std::size_t group_id = 0;

    while (m_next_section_id[group_id] > MAX_SECTION_ID)
	if (++group_id > MAX_GROUP_ID)
	    throw std::length_error("maximum number of sections");

    return std::make_pair(group_id, m_next_section_id[group_id]++);
}


inline void
FastgenWriter::write_comment(const std::string &value)
{
    (Record(*this) << "$COMMENT").text(" ").text(value);
}


void
FastgenWriter::write_section_color(const SectionID &section_id,
				   const unsigned char *color)
{
    m_colors_ostream << section_id.second << ' '
		     << section_id.second << ' '
		     << static_cast<unsigned>(color[0]) << ' '
		     << static_cast<unsigned>(color[1]) << ' '
		     << static_cast<unsigned>(color[2]) << '\n';
}


void
FastgenWriter::write_hole(const SectionID &surrounding_id,
			  const std::vector<SectionID> &subtracted_ids)
{
    if (subtracted_ids.empty() || subtracted_ids.size() > 3)
	throw std::invalid_argument("invalid number of IDs");

    Record record(*this);
    record << "HOLE";
    record << surrounding_id.first << surrounding_id.second;

    for (std::vector<SectionID>::const_iterator it = subtracted_ids.begin();
	 it != subtracted_ids.end(); ++it)
	record << it->first << it->second;
}


void
FastgenWriter::write_wall(const SectionID &surrounding_id,
			  const std::vector<SectionID> &subtracted_ids)
{
    if (subtracted_ids.empty() || subtracted_ids.size() > 3)
	throw std::invalid_argument("invalid number of IDs");

    Record record(*this);
    record << "WALL";
    record << surrounding_id.first << surrounding_id.second;

    for (std::vector<SectionID>::const_iterator it = subtracted_ids.begin();
	 it != subtracted_ids.end(); ++it)
	record << it->first << it->second;
}


class Point
{
public:
    Point()
    {
	VSETALL(*this, 0.0);
    }


    Point(const fastf_t *values)
    {
	VMOVE(*this, values);
    }


    operator const fastf_t *() const
    {
	return m_point;
    }


    operator fastf_t *()
    {
	return m_point;
    }


private:
    point_t m_point;
};


class GridManager
{
public:
    GridManager(RecordWriter &writer);
    ~GridManager();

    // return a vector of grid IDs corresponding to the given points,
    // with no duplicate indices in the returned vector.
    std::vector<std::size_t> get_unique_grids(const std::vector<Point> &points);


private:
    struct PointComparator {
	bool operator()(const Point &lhs, const Point &rhs) const;
    };

    static const std::size_t MAX_GRID_POINTS = 50000;

    RecordWriter &m_writer;
    std::size_t m_next_grid_id;
    std::map<Point, std::vector<std::size_t>, PointComparator> m_grids;
};


bool GridManager::PointComparator::operator()(const Point &lhs,
	const Point &rhs) const
{
#define COMPARE(a, b) do { if (!EQUAL((a), (b))) return (a) < (b); } while (false)

    COMPARE(lhs[X], rhs[X]);
    COMPARE(lhs[Y], rhs[Y]);
    COMPARE(lhs[Z], rhs[Z]);
    return false;

#undef COMPARE
}

inline
GridManager::GridManager(RecordWriter &writer) :
    m_writer(writer),
    m_next_grid_id(1),
    m_grids()
{}


GridManager::~GridManager()
{
    for (std::map<Point, std::vector<std::size_t>, PointComparator>::const_iterator
	 it = m_grids.begin(); it != m_grids.end(); ++it)
	for (std::vector<std::size_t>::const_iterator id_it = it->second.begin();
	     id_it != it->second.end(); ++id_it) {
	    FastgenWriter::Record record(m_writer);
	    record << "GRID" << *id_it << "";
	    record << it->first[X] * INCHES_PER_MM;
	    record << it->first[Y] * INCHES_PER_MM;
	    record << it->first[Z] * INCHES_PER_MM;
	}
}


std::vector<std::size_t>
GridManager::get_unique_grids(const std::vector<Point> &points)
{
    std::vector<std::size_t> results(points.size());

    for (std::size_t i = 0; i < points.size(); ++i) {
	std::vector<std::size_t> temp(1);
	temp.at(0) = m_next_grid_id;
	std::pair<std::map<Point, std::vector<std::size_t>, PointComparator>::iterator, bool>
	found = m_grids.insert(std::make_pair(points.at(i), temp));

	if (found.second) {
	    ++m_next_grid_id;
	    results[i] = found.first->second.at(0);
	    continue;
	}

	std::size_t n = 0;
	results[i] = found.first->second.at(n);

	for (std::size_t j = 0; j < i; ++j)
	    if (results[j] == results[i]) {
		if (++n < found.first->second.size()) {
		    results[i] = found.first->second.at(n);
		    j = 0;
		} else {
		    found.first->second.push_back(m_next_grid_id);
		    results[i] = m_next_grid_id++;
		    break;
		}
	    }

	if (results[i] > MAX_GRID_POINTS)
	    throw std::length_error("invalid grid ID");
    }

    return results;
}


class Section
{
public:
    Section(FastgenWriter &writer, const std::string &name, bool volume_mode,
	    const unsigned char *color = NULL);

    void write_line(const fastf_t *point_a, const fastf_t *point_b,
		    fastf_t radius, fastf_t thickness);

    void write_sphere(const fastf_t *center, fastf_t radius,
		      fastf_t thickness = 0.0);

    void write_cone(const fastf_t *point_a, const fastf_t *point_b, fastf_t ro1,
		    fastf_t ro2, fastf_t ri1, fastf_t ri2);

    void write_triangle(const fastf_t *point_a, const fastf_t *point_b,
			const fastf_t *point_c, fastf_t thickness, bool grid_centered = true);

    void write_hexahedron(const fastf_t points[8][3], fastf_t thickness = 0.0,
			  bool grid_centered = true);


private:
    static const std::size_t MAX_NAME_SIZE = 25;

    Section(const Section &source);
    Section &operator=(const Section &source);

    const bool m_volume_mode;
    const std::size_t m_material_id;

    FastgenWriter &m_writer;
    StringBuffer m_elements;
    GridManager m_grids;
    std::size_t m_next_element_id;
};


Section::Section(FastgenWriter & writer, const std::string & name,
		 bool volume_mode, const unsigned char *color) :
    m_volume_mode(volume_mode),
    m_material_id(1),
    m_writer(writer),
    m_elements(m_writer),
    m_grids(m_writer),
    m_next_element_id(1)
{
    const FastgenWriter::SectionID id = m_writer.take_next_section_id();

    {
	std::string record_name;

	if (name.size() > MAX_NAME_SIZE) {
	    m_writer.write_comment(name);
	    record_name = "..." + name.substr(name.size() - MAX_NAME_SIZE + 3);
	} else {
	    record_name = name;
	}

	RecordWriter::Record record(m_writer);
	record << "$NAME" << id.first << id.second;
	record << "" << "" << "" << "";
	record.text(record_name);
    }

    RecordWriter::Record(m_writer) << "SECTION" << id.first << id.second <<
				   (m_volume_mode ? 2 : 1);

    if (color)
	m_writer.write_section_color(id, color);
}


void
Section::write_line(const fastf_t *point_a, const fastf_t *point_b,
		    fastf_t radius, fastf_t thickness)
{
    radius *= INCHES_PER_MM;
    thickness *= INCHES_PER_MM;

    if ((!m_volume_mode && thickness <= 0.0) || thickness < 0.0)
	throw std::invalid_argument("invalid thickness");

    if (radius <= 0.0 || radius < thickness)
	throw std::invalid_argument("invalid radius");

    std::vector<Point> points(2);
    points.at(0) = point_a;
    points.at(1) = point_b;
    const std::vector<std::size_t> grids = m_grids.get_unique_grids(points);

    RecordWriter::Record record(m_elements);
    record << "CLINE" << m_next_element_id << m_material_id << grids.at(
	       0) << grids.at(1) << "" << "";

    if (m_volume_mode)
	record << thickness;
    else
	record.non_zero(thickness);

    record << radius;
    ++m_next_element_id;
}


void
Section::write_sphere(const fastf_t *center, fastf_t radius,
		      fastf_t thickness)
{
    radius *= INCHES_PER_MM;
    thickness *= INCHES_PER_MM;

    if (EQUAL(thickness, 0.0))
	thickness = radius;

    if (radius <= 0.0 || thickness <= 0.0 || radius < thickness)
	throw std::invalid_argument("invalid value");

    std::vector<Point> points(1);
    points.at(0) = center;
    const std::vector<std::size_t> grids = m_grids.get_unique_grids(points);

    RecordWriter::Record record(m_elements);
    record << "CSPHERE" << m_next_element_id << m_material_id;
    record << grids.at(0) << "" << "" << "";
    record.non_zero(thickness).non_zero(radius);
    ++m_next_element_id;
}


void
Section::write_cone(const fastf_t *point_a, const fastf_t *point_b, fastf_t ro1,
		    fastf_t ro2, fastf_t ri1, fastf_t ri2)
{
    ro1 *= INCHES_PER_MM;
    ro2 *= INCHES_PER_MM;
    ri1 *= INCHES_PER_MM;
    ri2 *= INCHES_PER_MM;

    if (!m_volume_mode)
	throw std::logic_error("CCONE2 elements can only be used in volume-mode components");

    if (ri1 < 0.0 || ri2 < 0.0 || ri1 >= ro1 || ri2 >= ro2)
	throw std::invalid_argument("invalid radius");

    std::vector<Point> points(2);
    points.at(0) = point_a;
    points.at(1) = point_b;
    const std::vector<std::size_t> grids = m_grids.get_unique_grids(points);

    {
	RecordWriter::Record record1(m_elements);
	record1 << "CCONE2" << m_next_element_id << m_material_id;
	record1 << grids.at(0) << grids.at(1);
	record1  << "" << "" << "" << ro1 << m_next_element_id;
    }

    {
	RecordWriter::Record record2(m_elements);
	record2 << m_next_element_id << ro2 << ri1 << ri2;
    }

    ++m_next_element_id;
}


void
Section::write_triangle(const fastf_t *point_a, const fastf_t *point_b,
			const fastf_t *point_c, fastf_t thickness, bool grid_centered)
{
    thickness *= INCHES_PER_MM;

    if (thickness <= 0.0)
	throw std::invalid_argument("invalid thickness");

    std::vector<Point> points(3);
    points.at(0) = point_a;
    points.at(1) = point_b;
    points.at(2) = point_c;
    const std::vector<std::size_t> grids = m_grids.get_unique_grids(points);

    RecordWriter::Record record(m_elements);
    record << "CTRI" << m_next_element_id << m_material_id;
    record << grids.at(0) << grids.at(1) << grids.at(2);
    record.non_zero(thickness);
    record << (grid_centered ? 1 : 2);
    ++m_next_element_id;
}


void
Section::write_hexahedron(const fastf_t vpoints[8][3], fastf_t thickness,
			  bool grid_centered)
{
    thickness *= INCHES_PER_MM;

    if (thickness < 0.0)
	throw std::invalid_argument("invalid thickness");

    std::vector<Point> points(8);

    for (int i = 0; i < 8; ++i)
	points.at(i) = vpoints[i];

    const std::vector<std::size_t> grids = m_grids.get_unique_grids(points);

    const bool has_thickness = !EQUAL(thickness, 0.0);

    {
	RecordWriter::Record record1(m_elements);
	record1 << (has_thickness ? "CHEX1" : "CHEX2");
	record1 << m_next_element_id << m_material_id;

	for (std::size_t i = 0; i < 6; ++i)
	    record1 << grids.at(i);

	record1 << m_next_element_id;
    }

    RecordWriter::Record record2(m_elements);
    record2 << m_next_element_id << grids.at(6) << grids.at(7);

    if (has_thickness)
	record2 << "" << "" << "" << "" << thickness << (grid_centered ? 1 : 2);

    ++m_next_element_id;
}


HIDDEN void
write_bot(Section & section, const rt_bot_internal & bot)
{
    RT_BOT_CK_MAGIC(&bot);

    for (std::size_t i = 0; i < bot.num_faces; ++i) {
	fastf_t thickness = 1.0;
	bool grid_centered = false;

	if (bot.mode == RT_BOT_PLATE) {
	    // fg4 does not allow zero thickness
	    // set a very small thickness if face thickness is zero
	    if (bot.thickness)
		thickness = !ZERO(bot.thickness[i]) ? bot.thickness[i] : 2 * SMALL_FASTF;

	    if (bot.face_mode)
		grid_centered = !BU_BITTEST(bot.face_mode, i);
	}

	const int * const face = &bot.faces[i * 3];
	const fastf_t * const v1 = &bot.vertices[face[0] * 3];
	const fastf_t * const v2 = &bot.vertices[face[1] * 3];
	const fastf_t * const v3 = &bot.vertices[face[2] * 3];
	section.write_triangle(v1, v2, v3, thickness, grid_centered);
    }
}


HIDDEN bool
ell_is_sphere(const rt_ell_internal & ell)
{
    RT_ELL_CK_MAGIC(&ell);

    // based on rt_sph_prep()

    fastf_t magsq_a, magsq_b, magsq_c;
    vect_t Au, Bu, Cu; // A, B, C with unit length
    fastf_t f;

    // Validate that |A| > 0, |B| > 0, |C| > 0
    magsq_a = MAGSQ(ell.a);
    magsq_b = MAGSQ(ell.b);
    magsq_c = MAGSQ(ell.c);

    if (ZERO(magsq_a) || ZERO(magsq_b) || ZERO(magsq_c))
	return false;

    // Validate that |A|, |B|, and |C| are nearly equal
    if (!EQUAL(magsq_a, magsq_b) || !EQUAL(magsq_a, magsq_c))
	return false;

    // Create unit length versions of A, B, C
    f = 1.0 / sqrt(magsq_a);
    VSCALE(Au, ell.a, f);
    f = 1.0 / sqrt(magsq_b);
    VSCALE(Bu, ell.b, f);
    f = 1.0 / sqrt(magsq_c);
    VSCALE(Cu, ell.c, f);

    // Validate that A.B == 0, B.C == 0, A.C == 0 (check dir only)
    f = VDOT(Au, Bu);

    if (!ZERO(f))
	return false;

    f = VDOT(Bu, Cu);

    if (!ZERO(f))
	return false;

    f = VDOT(Au, Cu);

    if (!ZERO(f))
	return false;

    return true;
}


// Determines whether a tgc can be represented by a CCONE2 object.
// Assumes that tgc is a valid rt_tgc_internal.
HIDDEN bool
tgc_is_ccone(const rt_tgc_internal & tgc)
{
    RT_TGC_CK_MAGIC(&tgc);

    if (VZERO(tgc.a) || VZERO(tgc.b))
	return false;

    if (!ZERO(VDOT(tgc.h, tgc.a)) || !ZERO(VDOT(tgc.h, tgc.b)))
	return false;

    {
	vect_t a_norm, b_norm, c_norm, d_norm;
	VMOVE(a_norm, tgc.a);
	VMOVE(b_norm, tgc.b);
	VMOVE(c_norm, tgc.c);
	VMOVE(d_norm, tgc.d);
	VUNITIZE(a_norm);
	VUNITIZE(b_norm);
	VUNITIZE(c_norm);
	VUNITIZE(d_norm);

	if (!VEQUAL(a_norm, c_norm) || !VEQUAL(b_norm, d_norm))
	    return false;
    }

    return true;
}


HIDDEN bool
tree_entirely_unions(const tree *tree)
{
    if (!tree)
	return true;

    RT_CK_TREE(tree);

    switch (tree->tr_op) {
	case OP_SOLID:
	case OP_REGION:
	case OP_NOP:
	case OP_NMG_TESS:
	case OP_DB_LEAF:
	    return true; // leaf

	case OP_UNION:
	    return tree_entirely_unions(tree->tr_b.tb_left)
		   && tree_entirely_unions(tree->tr_b.tb_right);

	default:
	    return false; // non-union operation
    }
}


HIDDEN rt_db_internal
get_parent(const db_i & db, const db_full_path & path)
{
    RT_CK_DBI(&db);
    RT_CK_FULL_PATH(&path);

    if (path.fp_len < 2)
	throw std::invalid_argument("toplevel");

    rt_db_internal comb_db_internal;

    if (rt_db_get_internal(&comb_db_internal, path.fp_names[path.fp_len - 2], &db,
			   NULL, &rt_uniresource) < 0)
	throw std::runtime_error("rt_db_get_internal() failed");

    rt_comb_internal &comb_internal = *static_cast<rt_comb_internal *>
				      (comb_db_internal.idb_ptr);
    RT_CK_COMB(&comb_internal);
    return comb_db_internal;
}


// Determines whether the parent comb is simple enough
// to be represented by fastgen4.
HIDDEN bool
comb_representable(const db_i & db, const db_full_path & path)
{
#if 0
    RT_CK_DBI(&db);
    RT_CK_FULL_PATH(&path);

    rt_db_internal comb_db_internal = get_parent(db, path);
    AutoFreePtr<rt_db_internal, rt_db_free_internal> autofree_comb_db_internal(
	&comb_db_internal);

    rt_comb_internal &comb_internal = *static_cast<rt_comb_internal *>
				      (comb_db_internal.idb_ptr);
    return tree_entirely_unions(comb_internal.tree);
#else
    (void)db;
    (void)path;
    return true;
#endif
}


HIDDEN std::pair<rt_db_internal, rt_db_internal>
get_cutout(const db_i & db, const std::string & name)
{
    RT_CK_DBI(&db);

    db_full_path path;

    if (db_string_to_path(&path, &db, name.c_str()))
	throw std::logic_error("logic error: invalid path");

    AutoFreePtr<db_full_path, db_free_full_path> autofree_path(&path);

    rt_db_internal comb_db_internal = get_parent(db, path);
    AutoFreePtr<rt_db_internal, rt_db_free_internal> autofree_comb_db_internal(
	&comb_db_internal);
    rt_comb_internal &comb_internal = *static_cast<rt_comb_internal *>
				      (comb_db_internal.idb_ptr);

    const directory *outer_directory, *inner_directory;
    {
	const tree::tree_node &t = comb_internal.tree->tr_b;

	if (t.tb_op != OP_SUBTRACT || !t.tb_left || !t.tb_right
	    || t.tb_left->tr_op != OP_DB_LEAF || t.tb_right->tr_op != OP_DB_LEAF)
	    throw std::invalid_argument("not a cutout");

	outer_directory = db_lookup(&db, t.tb_left->tr_l.tl_name, false);
	inner_directory = db_lookup(&db, t.tb_right->tr_l.tl_name, false);

	// check for nonexistent members
	if (!outer_directory || !inner_directory)
	    throw std::invalid_argument("nonexistent member");
    }

    std::pair<rt_db_internal, rt_db_internal> result;

    if (rt_db_get_internal(&result.first, outer_directory, &db, NULL,
			   &rt_uniresource) < 0)
	throw std::runtime_error("rt_db_get_internal() failed");

    if (rt_db_get_internal(&result.second, inner_directory, &db, NULL,
			   &rt_uniresource) < 0) {
	rt_db_free_internal(&result.second);
	throw std::runtime_error("rt_db_get_internal() failed");
    }

    return result;
}


HIDDEN bool
find_ccone_cutout(Section & section, const db_i & db,
		  const std::string & name)
{
    RT_CK_DBI(&db);

    std::pair<rt_db_internal, rt_db_internal> internals;

    try {
	internals = get_cutout(db, name);
    } catch (const std::invalid_argument &) {
	return false;
    }

    AutoFreePtr<rt_db_internal, rt_db_free_internal> autofree_outer_db_internal(
	&internals.first);
    AutoFreePtr<rt_db_internal, rt_db_free_internal> autofree_inner_db_internal(
	&internals.second);

    if ((internals.first.idb_minor_type != ID_TGC
	 && internals.first.idb_minor_type != ID_REC)
	|| (internals.second.idb_minor_type != ID_TGC
	    && internals.second.idb_minor_type != ID_REC))
	return false;

    const rt_tgc_internal &outer_tgc = *static_cast<rt_tgc_internal *>
				       (internals.first.idb_ptr);
    const rt_tgc_internal &inner_tgc = *static_cast<rt_tgc_internal *>
				       (internals.second.idb_ptr);
    RT_TGC_CK_MAGIC(&outer_tgc);
    RT_TGC_CK_MAGIC(&inner_tgc);

    if (!VEQUAL(outer_tgc.v, inner_tgc.v) || !VEQUAL(outer_tgc.h, inner_tgc.h))
	return false;

    // check cone geometry
    if (!tgc_is_ccone(outer_tgc) || !tgc_is_ccone(inner_tgc))
	return false;

    const fastf_t ro1 = MAGNITUDE(outer_tgc.a);
    const fastf_t ro2 = MAGNITUDE(outer_tgc.b);
    const fastf_t ri1 = MAGNITUDE(inner_tgc.a);
    const fastf_t ri2 = MAGNITUDE(inner_tgc.b);

    // check radii
    if (ri1 >= ro1 || ri2 >= ro2)
	return false;

    point_t v2;
    VADD2(v2, outer_tgc.v, outer_tgc.h);
    section.write_cone(outer_tgc.v, v2, ro1, ro2, ri1, ri2);
    return true;
}


HIDDEN bool
find_csphere_cutout(Section & section, const db_i & db,
		    const std::string & name)
{
    RT_CK_DBI(&db);

    std::pair<rt_db_internal, rt_db_internal> internals;

    try {
	internals = get_cutout(db, name);
    } catch (const std::invalid_argument &) {
	return false;
    }

    AutoFreePtr<rt_db_internal, rt_db_free_internal> autofree_outer_db_internal(
	&internals.first);
    AutoFreePtr<rt_db_internal, rt_db_free_internal> autofree_inner_db_internal(
	&internals.second);

    if ((internals.first.idb_minor_type != ID_SPH
	 && internals.first.idb_minor_type != ID_ELL)
	|| (internals.second.idb_minor_type != ID_SPH
	    && internals.second.idb_minor_type != ID_ELL))
	return false;

    const rt_ell_internal &outer_ell = *static_cast<rt_ell_internal *>
				       (internals.first.idb_ptr);
    const rt_ell_internal &inner_ell = *static_cast<rt_ell_internal *>
				       (internals.second.idb_ptr);
    RT_ELL_CK_MAGIC(&outer_ell);
    RT_ELL_CK_MAGIC(&inner_ell);

    if (!VEQUAL(outer_ell.v, inner_ell.v))
	return false;

    // check sphere geometry
    if (!ell_is_sphere(outer_ell) || !ell_is_sphere(inner_ell))
	return false;

    const fastf_t r_outer = MAGNITUDE(outer_ell.a);
    const fastf_t r_inner = MAGNITUDE(inner_ell.a);

    // check radii
    if (r_inner >= r_outer)
	return false;

    // FIXME: use parent's name
    section.write_sphere(outer_ell.v, r_outer, r_outer - r_inner);
    return true;
}


HIDDEN bool
get_chex1(Section & section, const rt_bot_internal & bot)
{
    RT_BOT_CK_MAGIC(&bot);

    fastf_t hex_thickness = 0.0;
    bool hex_grid_centered = true;

    if (bot.num_vertices != 8 || bot.num_faces != 12)
	return false;

    if (bot.mode != RT_BOT_SOLID && bot.mode != RT_BOT_PLATE)
	return false;

    if (bot.thickness) {
	hex_thickness = bot.thickness[0];

	for (std::size_t i = 1; i < bot.num_faces; ++i)
	    if (!EQUAL(bot.thickness[i], hex_thickness))
		return false;
    }

    if (bot.face_mode) {
	const bool face_mode = BU_BITTEST(bot.face_mode, 0);
	hex_grid_centered = !face_mode;

	std::size_t count = 0;
	BU_BITV_LOOP_START(bot.face_mode) {
	    ++count;
	}
	BU_BITV_LOOP_END;

	if (face_mode) {
	    if (count != bot.num_faces)
		return false;
	} else {
	    if (count)
		return false;
	}
    }

    const int hex_faces[12][3] = {
	{ 0, 1, 4 },
	{ 1, 5, 4 },
	{ 1, 2, 5 },
	{ 2, 6, 5 },
	{ 2, 3, 6 },
	{ 3, 7, 6 },
	{ 3, 0, 7 },
	{ 0, 4, 7 },
	{ 4, 6, 7 },
	{ 4, 5, 6 },
	{ 0, 1, 2 },
	{ 0, 2, 3 }
    };

    for (int i = 0; i < 12; ++i) {
	const int *face = &bot.faces[i * 3];

	if (face[0] != hex_faces[i][0] || face[1] != hex_faces[i][1]
	    || face[2] != hex_faces[i][2])
	    return false;
    }

    fastf_t points[8][3];

    for (int i = 0; i < 8; ++i)
	VMOVE(points[i], &bot.vertices[i * 3]);

    section.write_hexahedron(points, hex_thickness, hex_grid_centered);
    return true;
}


struct ConversionData {
    FastgenWriter &m_writer;
    const bn_tol &m_tol;
    const bool m_convert_primitives;

    // for find_ccone_cutout()
    const db_i &m_db;
    std::set<std::string> m_recorded_ccones;
};


HIDDEN bool
convert_primitive(ConversionData & data, const rt_db_internal & internal,
		  const std::string & name)
{
    switch (internal.idb_type) {
	case ID_CLINE: {
	    const rt_cline_internal &cline = *static_cast<rt_cline_internal *>
					     (internal.idb_ptr);
	    RT_CLINE_CK_MAGIC(&cline);

	    point_t v2;
	    VADD2(v2, cline.v, cline.h);

	    Section section(data.m_writer, name, true);
	    section.write_line(cline.v, v2, cline.radius, cline.thickness);
	    break;
	}

	case ID_ELL:
	case ID_SPH: {
	    const rt_ell_internal &ell = *static_cast<rt_ell_internal *>(internal.idb_ptr);
	    RT_ELL_CK_MAGIC(&ell);

	    if (internal.idb_type != ID_SPH && !ell_is_sphere(ell))
		return false;

	    Section section(data.m_writer, name, true);

	    if (!find_csphere_cutout(section, data.m_db, name))
		section.write_sphere(ell.v, MAGNITUDE(ell.a));

	    break;
	}

	case ID_TGC:
	case ID_REC: {
	    const rt_tgc_internal &tgc = *static_cast<rt_tgc_internal *>(internal.idb_ptr);
	    RT_TGC_CK_MAGIC(&tgc);

	    if (internal.idb_type != ID_REC && !tgc_is_ccone(tgc))
		return false;

	    Section section(data.m_writer, name, true);

	    if (!find_ccone_cutout(section, data.m_db, name)) {
		point_t v2;
		VADD2(v2, tgc.v, tgc.h);
		section.write_cone(tgc.v, v2, MAGNITUDE(tgc.a), MAGNITUDE(tgc.b), 0.0, 0.0);
	    }

	    break;
	}

	case ID_ARB8: {
	    const rt_arb_internal &arb = *static_cast<rt_arb_internal *>(internal.idb_ptr);
	    RT_ARB_CK_MAGIC(&arb);

	    Section section(data.m_writer, name, true);
	    section.write_hexahedron(arb.pt);
	    break;
	}

	case ID_BOT: {
	    const rt_bot_internal &bot = *static_cast<rt_bot_internal *>(internal.idb_ptr);
	    RT_BOT_CK_MAGIC(&bot);

	    Section section(data.m_writer, name, bot.mode == RT_BOT_SOLID);

	    if (!get_chex1(section, bot))
		write_bot(section, bot);

	    break;
	}

	default:
	    return false;
    }

    return true;
}


HIDDEN void
write_nmg_region(nmgregion *nmg_region, const db_full_path *path,
		 int UNUSED(region_id), int UNUSED(material_id), float *color,
		 void *client_data)
{
    NMG_CK_REGION(nmg_region);
    NMG_CK_MODEL(nmg_region->m_p);
    RT_CK_FULL_PATH(path);

    ConversionData &data = *static_cast<ConversionData *>(client_data);
    const std::string name = AutoFreePtr<char>(db_path_to_string(path)).ptr;

    shell *vshell;

    for (BU_LIST_FOR(vshell, shell, &nmg_region->s_hd)) {
	NMG_CK_SHELL(vshell);

	rt_bot_internal *bot = nmg_bot(vshell, &data.m_tol);

	// fill in an rt_db_internal with our new bot so we can free it
	rt_db_internal internal;
	RT_DB_INTERNAL_INIT(&internal);
	internal.idb_major_type = DB5_MAJORTYPE_BRLCAD;
	internal.idb_minor_type = ID_BOT;
	internal.idb_meth = &OBJ[ID_BOT];
	internal.idb_ptr = bot;

	try {
	    unsigned char char_color[3];
	    char_color[0] = static_cast<unsigned char>(color[0] * 255.0 + 0.5);
	    char_color[1] = static_cast<unsigned char>(color[1] * 255.0 + 0.5);
	    char_color[2] = static_cast<unsigned char>(color[2] * 255.0 + 0.5);
	    Section section(data.m_writer, name, bot->mode == RT_BOT_SOLID, char_color);
	    write_bot(section, *bot);
	} catch (const std::runtime_error &e) {
	    bu_log("FAILURE: write_bot() failed on object '%s': %s\n", name.c_str(),
		   e.what());
	}

	internal.idb_meth->ft_ifree(&internal);
    }
}


HIDDEN tree *
convert_leaf(db_tree_state *tree_state, const db_full_path *path,
	     rt_db_internal *internal, void *client_data)
{
    RT_CK_DBTS(tree_state);
    RT_CK_FULL_PATH(path);
    RT_CK_DB_INTERNAL(internal);

    ConversionData &data = *static_cast<ConversionData *>(client_data);
    const std::string name = AutoFreePtr<char>(db_path_to_string(path)).ptr;
    bool converted = false;

    if (internal->idb_major_type == DB5_MAJORTYPE_BRLCAD
	&& data.m_convert_primitives && comb_representable(data.m_db, *path))
	try {
	    converted = convert_primitive(data, *internal, name);
	} catch (const std::runtime_error &e) {
	    bu_log("FAILURE: convert_primitive() failed on object '%s': %s\n", name.c_str(),
		   e.what());
	}

    if (!converted)
	return nmg_booltree_leaf_tess(tree_state, path, internal, client_data);

    tree *result;
    RT_GET_TREE(result, tree_state->ts_resp);
    result->tr_op = OP_NOP;
    return result;
}


HIDDEN tree *
convert_region(db_tree_state *tree_state, const db_full_path *path,
	       tree *current_tree, void *client_data)
{
    RT_CK_DBTS(tree_state);
    RT_CK_FULL_PATH(path);
    RT_CK_TREE(current_tree);

    ConversionData &data = *static_cast<ConversionData *>(client_data);
    const std::string name = AutoFreePtr<char>(db_path_to_string(path)).ptr;

    gcv_region_end_data gcv_data = {write_nmg_region, &data};
    return gcv_region_end(tree_state, path, current_tree, &gcv_data);
}


HIDDEN int
gcv_fastgen4_write(const char *path, struct db_i *dbip,
		   const struct gcv_opts *UNUSED(options))
{
    RT_CK_DBI(dbip);

    // Set to true to directly translate any primitives that can be represented by fg4.
    // Due to limitations in the fg4 format, boolean operations can not be represented.
    const bool convert_primitives = false;

    FastgenWriter writer(path);
    writer.write_comment(dbip->dbi_title);
    writer.write_comment("g -> fastgen4 conversion");

    const rt_tess_tol ttol = {RT_TESS_TOL_MAGIC, 0.0, 0.01, 0.0};
    const bn_tol tol = {BN_TOL_MAGIC, BN_TOL_DIST, BN_TOL_DIST * BN_TOL_DIST, 1.0e-6, 1.0 - 1.0e-6};
    ConversionData conv_data = {writer, tol, convert_primitives, *dbip, std::set<std::string>()};

    {
	model *vmodel;
	db_tree_state initial_tree_state = rt_initial_tree_state;
	initial_tree_state.ts_tol = &tol;
	initial_tree_state.ts_ttol = &ttol;
	initial_tree_state.ts_m = &vmodel;

	db_update_nref(dbip, &rt_uniresource);
	directory **results;
	std::size_t num_objects = db_ls(dbip, DB_LS_TOPS, NULL, &results);
	AutoFreePtr<char *> object_names(db_dpv_to_argv(results));
	bu_free(results, "tops");

	vmodel = nmg_mm();
	db_walk_tree(dbip, static_cast<int>(num_objects),
		     const_cast<const char **>(object_names.ptr), 1,
		     &initial_tree_state, NULL, convert_region, convert_leaf, &conv_data);
	nmg_km(vmodel);
    }

    return 1;
}


static const struct gcv_converter converters[] = {
    {"fg4", NULL, gcv_fastgen4_write},
    {NULL, NULL, NULL}
};


}


extern "C" {


    struct gcv_plugin_info gcv_plugin_conv_fastgen4_write = {converters};


}


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
