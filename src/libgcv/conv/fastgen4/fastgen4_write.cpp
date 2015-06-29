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

#include "rt/func.h"
#include "../../plugin.h"


namespace
{


static const fastf_t INCHES_PER_MM = 1.0 / 25.4;


template <typename T> inline void
autoptr_wrap_bu_free(T *ptr)
{
    bu_free(ptr, "AutoPtr");
}


template <typename T, void free_fn(T *) = autoptr_wrap_bu_free>
struct AutoPtr {
    explicit AutoPtr(T *vptr = NULL) : ptr(vptr) {}

    ~AutoPtr()
    {
	free();
    }

    void free()
    {
	if (ptr) free_fn(ptr);

	ptr = NULL;
    }


    T *ptr;


private:
    AutoPtr(const AutoPtr &source);
    AutoPtr &operator=(const AutoPtr &source);
};


// Assignable/CopyConstructible for use with STL containers.
template <typename T>
class Triple
{
public:
    Triple()
    {
	VSETALL(m_value, 0);
    }


    Triple(T x, T y, T z)
    {
	VSET(m_value, x, y, z);
    }


    explicit Triple(const T *values)
    {
	VMOVE(*this, values);
    }


    operator const T *() const
    {
	return m_value;
    }


    operator T *()
    {
	return m_value;
    }


private:
    T m_value[3];
};


typedef Triple<fastf_t> Point;
typedef Triple<unsigned char> Color;


HIDDEN Color
color_from_floats(const float *float_color)
{
    Color result;
    result[0] = static_cast<unsigned char>(float_color[0] * 255.0 + 0.5);
    result[1] = static_cast<unsigned char>(float_color[1] * 255.0 + 0.5);
    result[2] = static_cast<unsigned char>(float_color[2] * 255.0 + 0.5);
    return result;
}


HIDDEN bool
mat_equal(const mat_t vmat_a, const mat_t vmat_b, const bn_tol &tol)
{
    BN_CK_TOL(&tol);

    const mat_t identity = MAT_INIT_IDN;
    const fastf_t * const mat_a = vmat_a ? vmat_a : identity;
    const fastf_t * const mat_b = vmat_b ? vmat_b : identity;

    return bn_mat_is_equal(mat_a, mat_b, &tol);
}


class DBInternal
{
public:
    static const directory &lookup(const db_i &db, const std::string &name);

    DBInternal();
    DBInternal(const db_i &db, const directory &dir);
    ~DBInternal();

    void load(const db_i &db, const directory &dir);
    const rt_db_internal &get() const;
    rt_db_internal &get();


private:
    DBInternal(const DBInternal &source);
    DBInternal &operator=(const DBInternal &source);

    bool m_valid;
    rt_db_internal m_internal;
};


const directory &
DBInternal::lookup(const db_i &db, const std::string &name)
{
    const directory *dir = db_lookup(&db, name.c_str(), LOOKUP_QUIET);

    if (!dir)
	throw std::invalid_argument("db_lookup() failed");

    return *dir;
}


DBInternal::DBInternal() :
    m_valid(false),
    m_internal()
{}


DBInternal::DBInternal(const db_i &db, const directory &dir) :
    m_valid(false),
    m_internal()
{
    load(db, dir);
}


inline
DBInternal::~DBInternal()
{
    if (m_valid)
	rt_db_free_internal(&m_internal);
}


void
DBInternal::load(const db_i &db, const directory &dir)
{
    RT_CK_DBI(&db);
    RT_CK_DIR(&dir);

    if (m_valid) {
	rt_db_free_internal(&m_internal);
	m_valid = false;
    }

    if (rt_db_get_internal(&m_internal, &dir, &db, NULL, &rt_uniresource) < 0)
	throw std::runtime_error("rt_db_get_internal() failed");

    m_valid = true;
    RT_CK_DB_INTERNAL(&m_internal);
}


const rt_db_internal &
DBInternal::get() const
{
    if (!m_valid)
	throw std::logic_error("invalid");

    return m_internal;
}


rt_db_internal &
DBInternal::get()
{
    if (!m_valid)
	throw std::logic_error("invalid");

    return m_internal;
}


class RecordWriter
{
public:
    class Record;

    RecordWriter();
    virtual ~RecordWriter();

    void write_comment(const std::string &value);


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
    static const std::size_t FIELD_WIDTH = 8;

    explicit Record(RecordWriter &writer);
    ~Record();

    template <typename T> Record &operator<<(const T &value);
    Record &operator<<(fastf_t value);
    Record &non_zero(fastf_t value);
    Record &text(const std::string &value);


private:
    static const std::size_t RECORD_WIDTH = 10;

    static std::string truncate_float(fastf_t value);

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
    sstream.precision(FIELD_WIDTH);
    sstream << std::fixed << value;
    std::string result = sstream.str().substr(0, FIELD_WIDTH);

    const std::size_t end_digit = result.find_last_not_of('0');
    const std::size_t end_point = result.find('.');

    // remove redundant trailing zeros
    result.erase(std::max(end_point + 2, end_digit + 1));

    if (end_point >= result.size() - 1)
	throw std::runtime_error("value too large");

    return result;
}


inline void
RecordWriter::write_comment(const std::string &value)
{
    (Record(*this) << "$COMMENT").text(" ").text(value);
}


// simple way to order records properly within the output file
class StringBuffer : public RecordWriter
{
public:
    StringBuffer();
    void write(RecordWriter &writer) const;


protected:
    virtual std::ostream &get_ostream();


private:
    std::ostringstream m_ostringstream;
};


inline
StringBuffer::StringBuffer() :
    m_ostringstream()
{}


inline void
StringBuffer::write(RecordWriter &writer) const
{
    RecordWriter::Record record(writer);
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
    typedef std::pair<std::size_t, std::size_t> SectionID; // group, component

    explicit FastgenWriter(const std::string &path);
    ~FastgenWriter();

    void write_section_color(const SectionID &section_id, const Color &color);
    SectionID write_compsplt(const SectionID &id, fastf_t z_coordinate);

    enum BooleanType { HOLE, WALL };
    void write_boolean(BooleanType type, const SectionID &section_a,
		       const SectionID &section_b, const SectionID *section_c = NULL,
		       const SectionID *section_d = NULL);

    SectionID take_next_section_id();

    RecordWriter &get_section_writer();


protected:
    virtual std::ostream &get_ostream();


private:
    static const std::size_t MAX_GROUP_ID = 49;
    static const std::size_t MAX_SECTION_ID = 999;

    std::size_t m_next_section_id[MAX_GROUP_ID + 1];
    StringBuffer m_sections;
    std::ofstream m_ostream, m_colors_ostream;
};


FastgenWriter::FastgenWriter(const std::string &path) :
    m_sections(),
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
    m_sections.write(*this);
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


inline RecordWriter &
FastgenWriter::get_section_writer()
{
    return m_sections;
}


void
FastgenWriter::write_section_color(const SectionID &section_id,
				   const Color &color)
{
    m_colors_ostream << section_id.second << ' '
		     << section_id.second << ' '
		     << static_cast<unsigned>(color[0]) << ' '
		     << static_cast<unsigned>(color[1]) << ' '
		     << static_cast<unsigned>(color[2]) << '\n';
}


FastgenWriter::SectionID
FastgenWriter::write_compsplt(const SectionID &id, fastf_t z_coordinate)
{
    const SectionID result = take_next_section_id();

    Record record(*this);
    record << "COMPSPLT" << id.first << id.second;
    record << result.first << result.second;
    record << z_coordinate * INCHES_PER_MM;

    return result;
}


void
FastgenWriter::write_boolean(BooleanType type, const SectionID &section_a,
			     const SectionID &section_b, const SectionID *section_c,
			     const SectionID *section_d)
{
    Record record(*this);
    record << (type == HOLE ? "HOLE" : "WALL");
    record << section_a.first << section_a.second;
    record << section_b.first << section_b.second;

    if (section_c)
	record << section_c->first << section_c->second;

    if (section_d)
	record << section_d->first << section_c->second;
}


// stores grid IDs and reuses duplicate grid points when possible
class GridManager
{
public:
    GridManager();

    // return a vector of grid IDs corresponding to the given points,
    // with no duplicate indices in the returned vector
    std::vector<std::size_t> get_unique_grids(const std::vector<Point> &points);

    void write(RecordWriter &writer) const;


private:
    struct PointComparator {
	bool operator()(const Point &lhs, const Point &rhs) const;
    };

    static const std::size_t MAX_GRID_POINTS = 50000;

    std::size_t m_next_grid_id;
    std::map<Point, std::vector<std::size_t>, PointComparator> m_grids;
};


bool GridManager::PointComparator::operator()(const Point &lhs,
	const Point &rhs) const
{
#define COMPARE(a, b) \
    do { \
	if (!NEAR_EQUAL((a), (b), RT_LEN_TOL)) \
	    return (a) < (b); \
    } while (false)

    COMPARE(lhs[X], rhs[X]);
    COMPARE(lhs[Y], rhs[Y]);
    COMPARE(lhs[Z], rhs[Z]);
    return false;

#undef COMPARE
}

inline
GridManager::GridManager() :
    m_next_grid_id(1),
    m_grids()
{}


std::vector<std::size_t>
GridManager::get_unique_grids(const std::vector<Point> &points)
{
    std::vector<std::size_t> results(points.size());

    for (std::size_t i = 0; i < points.size(); ++i) {
	std::vector<std::size_t> temp(1);
	temp.at(0) = m_next_grid_id;
	std::pair<std::map<Point, std::vector<std::size_t>, PointComparator>::iterator, bool>
	found = m_grids.insert(std::make_pair(points.at(i), temp));
	results.at(i) = found.first->second.at(0);

	if (found.second) {
	    ++m_next_grid_id;
	    continue;
	}

	for (std::size_t j = 0, n = 0; j < i; ++j)
	    if (results.at(j) == results.at(i)) {
		if (++n < found.first->second.size()) {
		    results.at(i) = found.first->second.at(n);
		    j = 0;
		} else {
		    found.first->second.push_back(m_next_grid_id);
		    results.at(i) = m_next_grid_id++;
		    break;
		}
	    }
    }

    return results;
}


void
GridManager::write(RecordWriter &writer) const
{
    if (m_next_grid_id - 1 > MAX_GRID_POINTS)
	throw std::length_error("max grid points exceeded");

    for (std::map<Point, std::vector<std::size_t>, PointComparator>::const_iterator
	 it = m_grids.begin(); it != m_grids.end(); ++it)
	for (std::vector<std::size_t>::const_iterator id_it = it->second.begin();
	     id_it != it->second.end(); ++id_it) {
	    RecordWriter::Record record(writer);
	    record << "GRID" << *id_it << "";
	    record << it->first[X] * INCHES_PER_MM;
	    record << it->first[Y] * INCHES_PER_MM;
	    record << it->first[Z] * INCHES_PER_MM;
	}
}


class Section
{
public:
    Section(bool volume_mode);

    bool empty() const;
    bool has_color() const;
    Color get_color() const;
    void set_color(const Color &value);

    void write(FastgenWriter &writer, const std::string &name,
	       const FastgenWriter::SectionID &id) const;

    // create a comment describing an element
    void write_name(const std::string &value);

    void write_line(const fastf_t *point_a, const fastf_t *point_b,
		    fastf_t radius, fastf_t thickness);

    void write_sphere(const fastf_t *center, fastf_t radius,
		      fastf_t thickness = 0.0);

    void write_cone(const fastf_t *point_a, const fastf_t *point_b, fastf_t ro1,
		    fastf_t ro2, fastf_t ri1, fastf_t ri2);

    void write_triangle(const fastf_t *point_a, const fastf_t *point_b,
			const fastf_t *point_c, fastf_t thickness, bool grid_centered = true);

    void write_quad(const fastf_t *point_a, const fastf_t *point_b,
		    const fastf_t *point_c, const fastf_t *point_d, fastf_t thickness,
		    bool grid_centered = true);

    void write_hexahedron(const fastf_t points[8][3], fastf_t thickness = 0.0,
			  bool grid_centered = true);


private:
    static const std::size_t MAX_NAME_SIZE = RecordWriter::Record::FIELD_WIDTH * 3;

    const bool m_volume_mode;
    const std::size_t m_material_id;

    std::pair<bool, Color> m_color; // optional color

    GridManager m_grids;
    StringBuffer m_elements;
    std::size_t m_next_element_id;
};


Section::Section(bool volume_mode) :
    m_volume_mode(volume_mode),
    m_material_id(1),
    m_color(false, Color()),
    m_grids(),
    m_elements(),
    m_next_element_id(1)
{}


inline bool
Section::empty() const
{
    return m_next_element_id == 1;
}


inline bool
Section::has_color() const
{
    return m_color.first;
}


inline Color
Section::get_color() const
{
    if (!has_color())
	throw std::logic_error("no color information for this Section");

    return m_color.second;
}


void
Section::set_color(const Color &value)
{
    m_color.first = true;
    m_color.second = value;
}


void
Section::write(FastgenWriter &writer, const std::string &name,
	       const FastgenWriter::SectionID &id) const
{
    RecordWriter &sections = writer.get_section_writer();

    if (has_color())
	writer.write_section_color(id, get_color());

    {
	std::string short_name = name;

	if (short_name.size() > MAX_NAME_SIZE) {
	    sections.write_comment(short_name);
	    short_name = "..." + short_name.substr(short_name.size() - MAX_NAME_SIZE + 3);
	}

	RecordWriter::Record record(sections);
	record << "$NAME" << id.first << id.second;
	record << "" << "" << "" << "";
	record.text(short_name);
    }

    RecordWriter::Record(sections)
	    << "SECTION" << id.first << id.second << (m_volume_mode ? 2 : 1);

    m_grids.write(sections);
    m_elements.write(sections);
}


inline void
Section::write_name(const std::string &value)
{
    m_elements.write_comment(value);
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
    points.at(0) = Point(point_a);
    points.at(1) = Point(point_b);
    const std::vector<std::size_t> grids = m_grids.get_unique_grids(points);

    RecordWriter::Record record(m_elements);
    record << "CLINE" << m_next_element_id << m_material_id << grids.at(
	       0) << grids.at(1) << "" << "";

    if (m_volume_mode && NEAR_ZERO(thickness, RT_LEN_TOL))
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
    if (NEAR_EQUAL(thickness, 0.0, RT_LEN_TOL))
	thickness = radius;

    radius *= INCHES_PER_MM;
    thickness *= INCHES_PER_MM;

    if (radius <= 0.0 || thickness <= 0.0 || radius < thickness)
	throw std::invalid_argument("invalid value");

    std::vector<Point> points(1);
    points.at(0) = Point(center);
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
    points.at(0) = Point(point_a);
    points.at(1) = Point(point_b);
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
    points.at(0) = Point(point_a);
    points.at(1) = Point(point_b);
    points.at(2) = Point(point_c);
    const std::vector<std::size_t> grids = m_grids.get_unique_grids(points);

    RecordWriter::Record record(m_elements);
    record << "CTRI" << m_next_element_id << m_material_id;
    record << grids.at(0) << grids.at(1) << grids.at(2);
    record.non_zero(thickness);
    record << (grid_centered ? 1 : 2);
    ++m_next_element_id;
}


void
Section::write_quad(const fastf_t *point_a, const fastf_t *point_b,
		    const fastf_t *point_c, const fastf_t *point_d, fastf_t thickness,
		    bool grid_centered)
{
    thickness *= INCHES_PER_MM;

    if (thickness <= 0.0)
	throw std::invalid_argument("invalid thickness");

    std::vector<Point> points(4);
    points.at(0) = Point(point_a);
    points.at(1) = Point(point_b);
    points.at(2) = Point(point_c);
    points.at(3) = Point(point_d);
    const std::vector<std::size_t> grids = m_grids.get_unique_grids(points);

    RecordWriter::Record record(m_elements);
    record << "CQUAD" << m_next_element_id << m_material_id;
    record << grids.at(0) << grids.at(1) << grids.at(2) << grids.at(3);
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
	points.at(i) = Point(vpoints[i]);

    const std::vector<std::size_t> grids = m_grids.get_unique_grids(points);

    const bool has_thickness = !NEAR_EQUAL(thickness, 0.0, RT_LEN_TOL);

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


HIDDEN std::pair<fastf_t, bool>
get_face_info(const rt_bot_internal &bot, std::size_t i)
{
    RT_BOT_CK_MAGIC(&bot);

    if (i > bot.num_faces)
	throw std::invalid_argument("invalid face");

    // defaults
    std::pair<fastf_t, bool> result(1.0, true);

    // fastgen forbids a thickness of zero here
    // set a very small thickness if face thickness is zero
    if (bot.thickness)
	result.first = !NEAR_ZERO(bot.thickness[i],
				  RT_LEN_TOL) ? bot.thickness[i] : 2.0 * RT_LEN_TOL;

    if (bot.face_mode)
	result.second = !BU_BITTEST(bot.face_mode, i);

    return result;
}


HIDDEN void
write_bot(Section &section, const rt_bot_internal &bot)
{
    RT_BOT_CK_MAGIC(&bot);

    for (std::size_t i = 0; i < bot.num_faces; ++i) {
	const int * const face = &bot.faces[i * 3];
	const std::pair<fastf_t, bool> face_info = get_face_info(bot, i);
	const fastf_t * const v1 = &bot.vertices[face[0] * 3];
	const fastf_t * const v2 = &bot.vertices[face[1] * 3];
	const fastf_t * const v3 = &bot.vertices[face[2] * 3];

	if (i + 1 < bot.num_faces) {
	    // quick check for CQUAD-compatible faces
	    const int * const next_face = &bot.faces[(i + 1) * 3];
	    const std::pair<fastf_t, bool> next_face_info = get_face_info(bot, i + 1);

	    if (NEAR_EQUAL(face_info.first, next_face_info.first, RT_LEN_TOL)
		&& face_info.second == next_face_info.second)
		if (face[0] == next_face[0] && face[2] == next_face[1]) {
		    const fastf_t * const v4 = &bot.vertices[next_face[2] * 3];
		    section.write_quad(v1, v2, v3, v4, face_info.first, face_info.second);
		    ++i;
		    continue;
		}
	}

	section.write_triangle(v1, v2, v3, face_info.first, face_info.second);
    }
}


HIDDEN bool
ell_is_sphere(const rt_ell_internal &ell)
{
    RT_ELL_CK_MAGIC(&ell);

    // based on rt_sph_prep()

    fastf_t mag_a, mag_b, mag_c;
    vect_t Au, Bu, Cu; // A, B, C with unit length
    fastf_t f;

    // Validate that |A| > 0, |B| > 0, |C| > 0
    mag_a = MAGNITUDE(ell.a);
    mag_b = MAGNITUDE(ell.b);
    mag_c = MAGNITUDE(ell.c);

    if (NEAR_ZERO(mag_a, RT_LEN_TOL) || NEAR_ZERO(mag_b, RT_LEN_TOL)
	|| NEAR_ZERO(mag_c, RT_LEN_TOL))
	return false;

    // Validate that |A|, |B|, and |C| are nearly equal
    if (!NEAR_EQUAL(mag_a, mag_b, RT_LEN_TOL)
	|| !NEAR_EQUAL(mag_a, mag_c, RT_LEN_TOL))
	return false;

    // Create unit length versions of A, B, C
    f = 1.0 / mag_a;
    VSCALE(Au, ell.a, f);
    f = 1.0 / mag_b;
    VSCALE(Bu, ell.b, f);
    f = 1.0 / mag_c;
    VSCALE(Cu, ell.c, f);

    // Validate that A.B == 0, B.C == 0, A.C == 0 (check dir only)
    f = VDOT(Au, Bu);

    if (!NEAR_ZERO(f, RT_DOT_TOL))
	return false;

    f = VDOT(Bu, Cu);

    if (!NEAR_ZERO(f, RT_DOT_TOL))
	return false;

    f = VDOT(Au, Cu);

    if (!NEAR_ZERO(f, RT_DOT_TOL))
	return false;

    return true;
}


// Determines whether a tgc can be represented by a CCONE2 object.
// Assumes that `tgc` is a valid rt_tgc_internal.
HIDDEN bool
tgc_is_ccone2(const rt_tgc_internal &tgc)
{
    RT_TGC_CK_MAGIC(&tgc);

    // ensure non-zero magnitudes
    if (NEAR_ZERO(MAGNITUDE(tgc.h), RT_LEN_TOL)
	|| NEAR_ZERO(MAGNITUDE(tgc.a), RT_LEN_TOL)
	|| NEAR_ZERO(MAGNITUDE(tgc.b), RT_LEN_TOL))
	return false;

    // ensure |A| == |B| and |C| == |D|
    if (!NEAR_EQUAL(MAGNITUDE(tgc.a), MAGNITUDE(tgc.b), RT_LEN_TOL)
	|| !NEAR_EQUAL(MAGNITUDE(tgc.c), MAGNITUDE(tgc.d), RT_LEN_TOL))
	return false;

    // ensure h, a, b are mutually orthogonal
    if (!NEAR_ZERO(VDOT(tgc.h, tgc.a), RT_DOT_TOL)
	|| !NEAR_ZERO(VDOT(tgc.h, tgc.b), RT_DOT_TOL)
	|| !NEAR_ZERO(VDOT(tgc.a, tgc.b), RT_DOT_TOL))
	return false;

    {
	// ensure unit vectors are equal
	vect_t a_norm, b_norm, c_norm, d_norm;
	VMOVE(a_norm, tgc.a);
	VMOVE(b_norm, tgc.b);
	VMOVE(c_norm, tgc.c);
	VMOVE(d_norm, tgc.d);
	VUNITIZE(a_norm);
	VUNITIZE(b_norm);
	VUNITIZE(c_norm);
	VUNITIZE(d_norm);

	if (!VNEAR_EQUAL(a_norm, c_norm, RT_LEN_TOL)
	    || !VNEAR_EQUAL(b_norm, d_norm, RT_LEN_TOL))
	    return false;
    }

    return true;
}


HIDDEN void
path_to_mat(const db_i &db, const db_full_path &path, mat_t result)
{
    RT_CK_DBI(&db);
    RT_CK_FULL_PATH(&path);

    db_full_path temp;
    db_full_path_init(&temp);
    db_dup_full_path(&temp, &path);
    AutoPtr<db_full_path, db_free_full_path> autofree_path(&temp);

    if (!db_path_to_mat(const_cast<db_i *>(&db), &temp, result, 0, &rt_uniresource))
	throw std::runtime_error("db_path_to_mat() failed");
}


HIDDEN void
apply_path_xform(const db_i &db, const mat_t matrix, rt_db_internal &internal)
{
    RT_CK_DBI(&db);
    RT_CK_DB_INTERNAL(&internal);

    if (rt_obj_xform(&internal, matrix, &internal, 0, const_cast<db_i *>(&db),
		     &rt_uniresource))
	throw std::runtime_error("rt_obj_xform() failed");
}


HIDDEN const directory &
get_parent_dir(const db_full_path &path)
{
    RT_CK_FULL_PATH(&path);

    if (path.fp_len < 2)
	throw std::invalid_argument("toplevel");

    return *path.fp_names[path.fp_len - 2];
}


HIDDEN const db_full_path
get_parent_path(const db_full_path &path)
{
    RT_CK_FULL_PATH(&path);

    if (path.fp_len < 2)
	throw std::invalid_argument("toplevel");

    db_full_path temp = path;
    DB_FULL_PATH_POP(&temp);
    return temp;
}


// get the parent region's directory
HIDDEN const directory &
get_region_dir(const db_i &db, const db_full_path &path)
{
    RT_CK_DBI(&db);
    RT_CK_FULL_PATH(&path);

    for (std::size_t i = 0; i < path.fp_len; ++i)
	if (DB_FULL_PATH_GET(&path, i)->d_flags & RT_DIR_REGION)
	    return *DB_FULL_PATH_GET(&path, i);

    throw std::invalid_argument("no parent region");
}


// get the parent region's path
HIDDEN const db_full_path
get_region_path(const db_i &db, const db_full_path &path)
{
    RT_CK_DBI(&db);
    RT_CK_FULL_PATH(&path);

    const directory &region_dir = get_region_dir(db, path);
    db_full_path result = path;

    while (DB_FULL_PATH_CUR_DIR(&result) != &region_dir)
	DB_FULL_PATH_POP(&result);

    return result;
}


// helper; sets `outer` and `inner` if they exist in
// a tree describing `outer` - `inner`
HIDDEN bool
get_cutout(const db_i &db, const db_full_path &parent_path, DBInternal &outer,
	   DBInternal &inner)
{
    RT_CK_DBI(&db);
    RT_CK_FULL_PATH(&parent_path);

    DBInternal comb_db_internal(db, *DB_FULL_PATH_CUR_DIR(&parent_path));
    const rt_comb_internal &comb_internal = *static_cast<rt_comb_internal *>
					    (comb_db_internal.get().idb_ptr);
    RT_CK_COMB(&comb_internal);

    const tree::tree_node &t = comb_internal.tree->tr_b;

    if (t.tb_op != OP_SUBTRACT || !t.tb_left || !t.tb_right
	|| t.tb_left->tr_op != OP_DB_LEAF || t.tb_right->tr_op != OP_DB_LEAF)
	return false;

    const directory &outer_dir = DBInternal::lookup(db, t.tb_left->tr_l.tl_name);
    const directory &inner_dir = DBInternal::lookup(db, t.tb_right->tr_l.tl_name);
    outer.load(db, outer_dir);
    inner.load(db, inner_dir);

    {
	mat_t matrix;
	db_full_path temp;
	db_full_path_init(&temp);
	AutoPtr<db_full_path, db_free_full_path> autofree_temp(&temp);
	db_dup_full_path(&temp, &parent_path);

	db_add_node_to_full_path(&temp, const_cast<directory *>(&outer_dir));
	path_to_mat(db, temp, matrix);
	apply_path_xform(db, matrix, outer.get());

	DB_FULL_PATH_POP(&temp);
	db_add_node_to_full_path(&temp, const_cast<directory *>(&inner_dir));
	path_to_mat(db, temp, matrix);
	apply_path_xform(db, matrix, inner.get());

    }

    return true;
}


// test for and create ccone2 elements
HIDDEN bool
find_ccone2_cutout(Section &section, const db_i &db,
		   const db_full_path &parent_path, std::set<const directory *> &completed)
{
    RT_CK_DBI(&db);
    RT_CK_FULL_PATH(&parent_path);

    const directory &parent_dir = *DB_FULL_PATH_CUR_DIR(&parent_path);

    try {
	if (completed.count(&parent_dir))
	    return true; // already processed
    } catch (const std::invalid_argument &) {
	return false;
    }

    std::pair<DBInternal, DBInternal> internals;

    if (!get_cutout(db, parent_path, internals.first, internals.second))
	return false;

    if ((internals.first.get().idb_minor_type != ID_TGC
	 && internals.first.get().idb_minor_type != ID_REC)
	|| (internals.second.get().idb_minor_type != ID_TGC
	    && internals.second.get().idb_minor_type != ID_REC))
	return false;

    const rt_tgc_internal &outer_tgc = *static_cast<rt_tgc_internal *>
				       (internals.first.get().idb_ptr);
    const rt_tgc_internal &inner_tgc = *static_cast<rt_tgc_internal *>
				       (internals.second.get().idb_ptr);
    RT_TGC_CK_MAGIC(&outer_tgc);
    RT_TGC_CK_MAGIC(&inner_tgc);

    if (!VNEAR_EQUAL(outer_tgc.v, inner_tgc.v, RT_LEN_TOL)
	|| !VNEAR_EQUAL(outer_tgc.h, inner_tgc.h, RT_LEN_TOL))
	return false;

    // check cone geometry
    if (!tgc_is_ccone2(outer_tgc) || !tgc_is_ccone2(inner_tgc))
	return false;

    const fastf_t ro1 = MAGNITUDE(outer_tgc.a);
    const fastf_t ro2 = MAGNITUDE(outer_tgc.c);
    const fastf_t ri1 = MAGNITUDE(inner_tgc.a);
    const fastf_t ri2 = MAGNITUDE(inner_tgc.c);

    // check radii
    if (ri1 >= ro1 || ri2 >= ro2)
	return false;

    point_t v2;
    VADD2(v2, outer_tgc.v, outer_tgc.h);
    section.write_name(parent_dir.d_namep);
    section.write_cone(outer_tgc.v, v2, ro1, ro2, ri1, ri2);
    completed.insert(&parent_dir);
    return true;
}


// test for and create CSPHERE elements
HIDDEN bool
find_csphere_cutout(Section &section, const db_i &db,
		    const db_full_path &parent_path, std::set<const directory *> &completed)
{
    RT_CK_DBI(&db);
    RT_CK_FULL_PATH(&parent_path);

    const directory &parent_dir = *DB_FULL_PATH_CUR_DIR(&parent_path);

    try {
	if (completed.count(&parent_dir))
	    return true; // already processed
    } catch (const std::invalid_argument &) {
	return false;
    }

    std::pair<DBInternal, DBInternal> internals;

    if (!get_cutout(db, parent_path, internals.first, internals.second))
	return false;

    if ((internals.first.get().idb_minor_type != ID_SPH
	 && internals.first.get().idb_minor_type != ID_ELL)
	|| (internals.second.get().idb_minor_type != ID_SPH
	    && internals.second.get().idb_minor_type != ID_ELL))
	return false;

    const rt_ell_internal &outer_ell = *static_cast<rt_ell_internal *>
				       (internals.first.get().idb_ptr);
    const rt_ell_internal &inner_ell = *static_cast<rt_ell_internal *>
				       (internals.second.get().idb_ptr);
    RT_ELL_CK_MAGIC(&outer_ell);
    RT_ELL_CK_MAGIC(&inner_ell);

    if (!VNEAR_EQUAL(outer_ell.v, inner_ell.v, RT_LEN_TOL))
	return false;

    // check sphere geometry
    if (!ell_is_sphere(outer_ell) || !ell_is_sphere(inner_ell))
	return false;

    const fastf_t r_outer = MAGNITUDE(outer_ell.a);
    const fastf_t r_inner = MAGNITUDE(inner_ell.a);

    // check radii
    if (r_inner >= r_outer)
	return false;

    completed.insert(&parent_dir);
    section.write_name(parent_dir.d_namep);
    section.write_sphere(outer_ell.v, r_outer, r_outer - r_inner);
    return true;
}


// test for and create CHEX1 elements
HIDDEN bool
get_chex1(Section &section, const rt_bot_internal &bot)
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
	    if (!NEAR_EQUAL(bot.thickness[i], hex_thickness, RT_LEN_TOL))
		return false;
    }

    if (bot.face_mode) {
	const bool face_mode = BU_BITTEST(bot.face_mode, 0);
	hex_grid_centered = !face_mode;

	// check that all faces have a uniform face mode
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
	const int * const face = &bot.faces[i * 3];

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


typedef std::map<const directory *, const fastf_t *> LeafMap;


HIDDEN void
get_unioned(const db_i &db, const tree *tree, LeafMap &results)
{
    RT_CK_DBI(&db);

    if (!tree)
	return;

    RT_CK_TREE(tree);

    switch (tree->tr_op) {
	case OP_DB_LEAF: {
	    const directory *dir;

	    try {
		dir = &DBInternal::lookup(db, tree->tr_l.tl_name);
	    } catch (const std::invalid_argument &) {
		break;
	    }

	    results.insert(std::make_pair(dir, tree->tr_l.tl_mat));
	    break;
	}

	case OP_UNION:
	    get_unioned(db, tree->tr_b.tb_left, results);
	    get_unioned(db, tree->tr_b.tb_right, results);
	    break;

	case OP_SUBTRACT:
	    get_unioned(db, tree->tr_b.tb_left, results);
	    break;

	default:
	    break;
    }
}


HIDDEN void
get_intersected(const db_i &db, const tree *tree, LeafMap &results)
{
    RT_CK_DBI(&db);

    if (!tree)
	return;

    RT_CK_TREE(tree);

    switch (tree->tr_op) {
	case OP_INTERSECT:
	    get_unioned(db, tree->tr_b.tb_left, results);
	    get_unioned(db, tree->tr_b.tb_right, results);
	    get_intersected(db, tree->tr_b.tb_left, results);
	    get_intersected(db, tree->tr_b.tb_right, results);
	    break;

	case OP_UNION:
	    get_intersected(db, tree->tr_b.tb_left, results);
	    get_intersected(db, tree->tr_b.tb_right, results);
	    break;

	case OP_SUBTRACT:
	    get_intersected(db, tree->tr_b.tb_left, results);
	    break;

	default:
	    break;
    }
}


HIDDEN void
get_subtracted(const db_i &db, const tree *tree, LeafMap &results)
{
    RT_CK_DBI(&db);

    if (!tree)
	return;

    RT_CK_TREE(tree);

    switch (tree->tr_op) {
	case OP_UNION:
	case OP_INTERSECT:
	    get_subtracted(db, tree->tr_b.tb_left, results);
	    get_subtracted(db, tree->tr_b.tb_right, results);
	    break;

	case OP_SUBTRACT:
	    get_subtracted(db, tree->tr_b.tb_left, results);
	    get_unioned(db, tree->tr_b.tb_right, results);
	    break;

	default:
	    break;
    }
}


// Identifies which half of a COMPSPLT a given region represents.
// Returns an int and a pointer to the half's matrix within the region.
//   0 - not a COMPSPLT
//   1 - the intersected half
//   2 - the subtracted half
HIDDEN std::pair<int, const fastf_t *>
identify_compsplt(const db_i &db, const directory &parent_region_dir,
		  const directory &half_dir)
{
    RT_CK_DBI(&db);
    RT_CK_DIR(&parent_region_dir);
    RT_CK_DIR(&half_dir);

    DBInternal parent_region_internal(db, parent_region_dir);
    const rt_comb_internal &parent_region = *static_cast<rt_comb_internal *>
					    (parent_region_internal.get().idb_ptr);
    RT_CK_COMB(&parent_region);

    LeafMap leaves;
    get_intersected(db, parent_region.tree, leaves);

    LeafMap::const_iterator found = leaves.find(&half_dir);

    if (found != leaves.end())
	return std::make_pair(1, found->second);

    leaves.clear();
    get_subtracted(db, parent_region.tree, leaves);
    found = leaves.find(&half_dir);

    if (found != leaves.end())
	return std::make_pair(2, found->second);

    return std::make_pair(0, static_cast<fastf_t *>(NULL));
}


// returns:
// - set of regions that are joined to the region by a WALL
// - set of members to ignore
HIDDEN std::pair<std::set<const directory *>, std::set<const directory *> >
find_walls(const db_i &db, const directory &region_dir, const bn_tol &tol)
{
    RT_CK_DBI(&db);
    RT_CK_DIR(&region_dir);
    BN_CK_TOL(&tol);

    std::pair<std::set<const directory *>, std::set<const directory *> > results;
    LeafMap subtracted;
    DBInternal region_db_internal(db, region_dir);
    {
	const rt_comb_internal &region = *static_cast<rt_comb_internal *>
					 (region_db_internal.get().idb_ptr);
	RT_CK_COMB(&region);

	get_subtracted(db, region.tree, subtracted);

	if (subtracted.empty())
	    return results;
    }

    {
	AutoPtr<directory *> region_dirs;
	const std::size_t num_regions = db_ls(&db, DB_LS_REGION, NULL,
					      &region_dirs.ptr);

	for (std::size_t i = 0; i < num_regions; ++i) {
	    DBInternal this_region_db_internal(db, *region_dirs.ptr[i]);
	    const rt_comb_internal &region = *static_cast<rt_comb_internal *>
					     (this_region_db_internal.get().idb_ptr);
	    RT_CK_COMB(&region);

	    LeafMap leaves;
	    get_unioned(db, region.tree, leaves);
	    get_intersected(db, region.tree, leaves);
	    bool subset = !leaves.empty();

	    for (LeafMap::const_iterator leaf_it = leaves.begin(); leaf_it != leaves.end();
		 ++leaf_it) {
		const LeafMap::const_iterator subtracted_it = subtracted.find(leaf_it->first);

		if (subtracted_it == subtracted.end()
		    || !mat_equal(leaf_it->second, subtracted_it->second, tol)) {
		    subset = false;
		    break;
		}
	    }

	    if (subset) {
		results.first.insert(region_dirs.ptr[i]);

		for (LeafMap::const_iterator it = leaves.begin(); it != leaves.end(); ++it)
		    results.second.insert(it->first);
	    }
	}
    }

    return results;
}


// stores state for the ongoing conversion process
struct FastgenConversion {
    class RegionManager;

    FastgenConversion(const std::string &path, const db_i &db, const bn_tol &tol);
    ~FastgenConversion();

    RegionManager &get_region(const directory &region_dir);

    const db_i &m_db;
    const bn_tol &m_tol;
    std::set<const directory *> m_recorded_cutouts, m_failed_regions,
	m_facetize_regions;
    Section m_toplevels;


private:
    FastgenConversion(const FastgenConversion &source);
    FastgenConversion &operator=(const FastgenConversion &source);

    std::map<const directory *, RegionManager *> m_regions;
    FastgenWriter m_writer;
};


// Organize Section objects by their corresponding region, and store
// additional conversion state regarding those Sections.
class FastgenConversion::RegionManager
{
public:
    RegionManager(const db_i &db, const directory &region_dir, const bn_tol &tol);
    ~RegionManager();

    std::vector<FastgenWriter::SectionID> write(FastgenWriter &writer) const;

    void write_walls(FastgenWriter &writer,
		     const std::map<const directory *, std::vector<FastgenWriter::SectionID> > &ids)
    const;

    // don't write these Sections
    void disable();

    // mark these Sections as having a COMPSPLT
    void set_compsplt(fastf_t z_coordinate);

    // returns true if the given member shouldn't be written
    bool member_ignored(const directory &member_dir) const;

    void create_section(const db_full_path &region_instance_path);
    Section &get_section(const db_full_path &region_instance_path);


private:
    RegionManager(const RegionManager &source);
    RegionManager &operator=(const RegionManager &source);

    const directory &m_region_dir;
    bool m_enabled;
    std::pair<bool, fastf_t> m_compsplt;
    const std::pair<std::set<const directory *>, std::set<const directory *> >
    m_walls;
    std::map<std::string, Section *> m_sections;
};


FastgenConversion::RegionManager::RegionManager(const db_i &db,
	const directory &region_dir, const bn_tol &tol) :
    m_region_dir(region_dir),
    m_enabled(true),
    m_compsplt(false, 0.0),
    m_walls(find_walls(db, region_dir, tol)),
    m_sections()
{}


FastgenConversion::RegionManager::~RegionManager()
{
    for (std::map<std::string, Section *>::iterator it = m_sections.begin();
	 it != m_sections.end(); ++it)
	delete it->second;
}


std::vector<FastgenWriter::SectionID>
FastgenConversion::RegionManager::write(FastgenWriter &writer) const
{
    std::vector<FastgenWriter::SectionID> results;

    if (!m_enabled)
	return results;

    for (std::map<std::string, Section *>::const_iterator it = m_sections.begin();
	 it != m_sections.end(); ++it) {
	if (it->second->empty())
	    continue;

	const FastgenWriter::SectionID id = writer.take_next_section_id();

	if (m_compsplt.first) {
	    const FastgenWriter::SectionID split_id = writer.write_compsplt(id,
		    m_compsplt.second);

	    if (it->second->has_color()) {
		Color color = it->second->get_color();
		writer.write_section_color(split_id, color);
	    }
	}

	it->second->write(writer, it->first, id);
	results.push_back(id);
    }

    return results;
}


void
FastgenConversion::RegionManager::write_walls(FastgenWriter &writer,
	const std::map<const directory *, std::vector<FastgenWriter::SectionID> > &ids)
const
{
    typedef std::vector<FastgenWriter::SectionID> IDVector;

    const IDVector &this_region_ids = ids.at(&m_region_dir);

    for (IDVector::const_iterator this_id_it = this_region_ids.begin();
	 this_id_it != this_region_ids.end(); ++this_id_it)
	for (std::set<const directory *>::const_iterator wall_dir_it =
		 m_walls.first.begin(); wall_dir_it != m_walls.first.end(); ++wall_dir_it)
	    for (IDVector::const_iterator target_id_it = ids.at(*wall_dir_it).begin();
		 target_id_it != ids.at(*wall_dir_it).end(); ++target_id_it)
		writer.write_boolean(FastgenWriter::WALL, *this_id_it, *target_id_it);
}


void
FastgenConversion::RegionManager::disable()
{
    if (!m_enabled)
	throw std::logic_error("already disabled");

    m_enabled = false;
}


void
FastgenConversion::RegionManager::set_compsplt(fastf_t z_coordinate)
{
    if (m_compsplt.first)
	throw std::logic_error("already a COMPSPLT");

    m_compsplt.first = true;
    m_compsplt.second = z_coordinate;
}


bool
FastgenConversion::RegionManager::member_ignored(
    const directory &member_dir) const
{
    RT_CK_DIR(&member_dir);

    return !m_enabled || m_walls.second.count(&member_dir);
}


void
FastgenConversion::RegionManager::create_section(const db_full_path
	&region_instance_path)
{
    RT_CK_FULL_PATH(&region_instance_path);

    if (DB_FULL_PATH_CUR_DIR(&region_instance_path) != &m_region_dir)
	throw std::invalid_argument("invalid path");

    const std::string name = AutoPtr<char>(db_path_to_string(
	    &region_instance_path)).ptr;
    std::pair<std::map<std::string, Section *>::iterator, bool> found =
	m_sections.insert(std::make_pair(name, static_cast<Section *>(NULL)));

    if (found.second)
	found.first->second = new Section(true);
    else {
	// multiple references within the same comb tree
    }
}


Section &
FastgenConversion::RegionManager::get_section(const db_full_path
	&region_instance_path)
{
    RT_CK_FULL_PATH(&region_instance_path);

    const std::string name = AutoPtr<char>(db_path_to_string(
	    &region_instance_path)).ptr;
    return *m_sections.at(name);
}


// test for and create compsplts
HIDDEN bool
find_compsplt(FastgenConversion &data, const db_full_path &half_path,
	      const rt_half_internal &half)
{
    RT_CK_FULL_PATH(&half_path);
    RT_HALF_CK_MAGIC(&half);

    if (DB_FULL_PATH_CUR_DIR(&half_path)->d_nref < 2)
	return false;

    const vect_t normal = {0.0, 0.0, 1.0};

    if (!VNEAR_EQUAL(half.eqn, normal, RT_LEN_TOL))
	return false;

    const directory *parent_region_dir;

    try {
	parent_region_dir = &get_region_dir(data.m_db, half_path);
    } catch (const std::invalid_argument &) {
	return false;
    }

    const std::pair<int, const fastf_t *> this_compsplt = identify_compsplt(
		data.m_db, *parent_region_dir, *DB_FULL_PATH_CUR_DIR(&half_path));

    if (!this_compsplt.first)
	return false;

    // find the other half
    const directory *other_half_dir = NULL;
    {
	AutoPtr<directory *> region_dirs;
	const std::size_t num_regions = db_ls(&data.m_db, DB_LS_REGION, NULL,
					      &region_dirs.ptr);

	for (std::size_t i = 0; i < num_regions; ++i) {
	    if (region_dirs.ptr[i] == parent_region_dir)
		continue;

	    const std::pair<int, const fastf_t *> current = identify_compsplt(data.m_db,
		    *region_dirs.ptr[i], *DB_FULL_PATH_CUR_DIR(&half_path));

	    if (current.first && current.first != this_compsplt.first)
		if (mat_equal(current.second, this_compsplt.second, data.m_tol)) {
		    other_half_dir = region_dirs.ptr[i];
		    break;
		}
	}
    }

    if (!other_half_dir)
	return false;

    if (this_compsplt.first == 1)
	data.get_region(*parent_region_dir).disable();
    else
	data.get_region(*parent_region_dir).set_compsplt(half.eqn[3]);

    return true;
}


FastgenConversion::FastgenConversion(const std::string &path, const db_i &db,
				     const bn_tol &tol) :
    m_db(db),
    m_tol(tol),
    m_recorded_cutouts(),
    m_failed_regions(),
    m_facetize_regions(),
    m_toplevels(true),
    m_regions(),
    m_writer(path)
{
    RT_CK_DBI(&m_db);
    BN_CK_TOL(&m_tol);

    m_writer.write_comment(m_db.dbi_title);
    m_writer.write_comment("g -> fastgen4 conversion");

    AutoPtr<directory *> region_dirs;
    const std::size_t num_regions = db_ls(&db, DB_LS_REGION, NULL,
					  &region_dirs.ptr);

    // create a RegionManager for all regions in the database
    try {
	for (std::size_t i = 0; i < num_regions; ++i)
	    m_regions[region_dirs.ptr[i]] = new RegionManager(m_db, *region_dirs.ptr[i],
		    m_tol);
    } catch (...) {
	for (std::map<const directory *, RegionManager *>::iterator it =
		 m_regions.begin(); it != m_regions.end(); ++it)
	    delete it->second;

	throw;
    }
}


FastgenConversion::~FastgenConversion()
{
    if (!m_toplevels.empty())
	m_toplevels.write(m_writer, "toplevels", m_writer.take_next_section_id());

    std::map<const directory *, std::vector<FastgenWriter::SectionID> > ids;

    typedef std::map<std::string, std::map<const directory *, RegionManager *>::const_iterator>
    SortedRegionMap;
    SortedRegionMap sorted_regions;

    for (std::map<const directory *, RegionManager *>::const_iterator it =
	     m_regions.begin(); it != m_regions.end(); ++it)
	sorted_regions[it->first->d_namep] = it;

    for (SortedRegionMap::const_iterator it = sorted_regions.begin();
	 it != sorted_regions.end(); ++it)
	ids[it->second->first] = it->second->second->write(m_writer);

    for (SortedRegionMap::iterator it = sorted_regions.begin();
	 it != sorted_regions.end(); ++it) {
	it->second->second->write_walls(m_writer, ids);
	delete it->second->second;
    }
}


FastgenConversion::RegionManager &
FastgenConversion::get_region(const directory &region_dir)
{
    return *m_regions.at(&region_dir);
}


HIDDEN Section &
get_section(FastgenConversion &data, const db_full_path &path)
{
    RT_CK_FULL_PATH(&path);

    const directory *region_dir = NULL;

    try {
	region_dir = &get_region_dir(data.m_db, path);
    } catch (std::invalid_argument &) {}

    if (region_dir)
	return data.get_region(*region_dir).get_section(get_region_path(data.m_db,
		path));
    else
	return data.m_toplevels;

}


HIDDEN bool
convert_primitive(FastgenConversion &data, const db_full_path &path,
		  const rt_db_internal &internal, bool subtracted)
{
    RT_CK_FULL_PATH(&path);
    RT_CK_DB_INTERNAL(&internal);

    Section &section = get_section(data, path);

    switch (internal.idb_type) {
	case ID_CLINE: {
	    const rt_cline_internal &cline = *static_cast<rt_cline_internal *>
					     (internal.idb_ptr);
	    RT_CLINE_CK_MAGIC(&cline);

	    point_t v2;
	    VADD2(v2, cline.v, cline.h);
	    section.write_name(DB_FULL_PATH_CUR_DIR(&path)->d_namep);
	    section.write_line(cline.v, v2, cline.radius, cline.thickness);
	    break;
	}

	case ID_ELL:
	case ID_SPH: {
	    const rt_ell_internal &ell = *static_cast<rt_ell_internal *>(internal.idb_ptr);
	    RT_ELL_CK_MAGIC(&ell);

	    if (internal.idb_type != ID_SPH && !ell_is_sphere(ell))
		return false;

	    if (path.fp_len > 1
		&& find_csphere_cutout(section, data.m_db, get_parent_path(path),
				       data.m_recorded_cutouts))
		return true;

	    section.write_name(DB_FULL_PATH_CUR_DIR(&path)->d_namep);
	    section.write_sphere(ell.v, MAGNITUDE(ell.a));
	    break;
	}

	case ID_TGC:
	case ID_REC: {
	    const rt_tgc_internal &tgc = *static_cast<rt_tgc_internal *>(internal.idb_ptr);
	    RT_TGC_CK_MAGIC(&tgc);

	    if (internal.idb_type != ID_REC && !tgc_is_ccone2(tgc))
		return false;

	    if (path.fp_len > 1
		&& find_ccone2_cutout(section, data.m_db, get_parent_path(path),
				      data.m_recorded_cutouts))
		return true;

	    point_t v2;
	    VADD2(v2, tgc.v, tgc.h);
	    section.write_name(DB_FULL_PATH_CUR_DIR(&path)->d_namep);
	    section.write_cone(tgc.v, v2, MAGNITUDE(tgc.a), MAGNITUDE(tgc.c), 0.0, 0.0);
	    break;
	}

	case ID_ARB8: {
	    const rt_arb_internal &arb = *static_cast<rt_arb_internal *>(internal.idb_ptr);
	    RT_ARB_CK_MAGIC(&arb);

	    section.write_name(DB_FULL_PATH_CUR_DIR(&path)->d_namep);
	    section.write_hexahedron(arb.pt);
	    break;
	}

	case ID_BOT: {
	    const rt_bot_internal &bot = *static_cast<rt_bot_internal *>(internal.idb_ptr);
	    RT_BOT_CK_MAGIC(&bot);

	    section.write_name(DB_FULL_PATH_CUR_DIR(&path)->d_namep);

	    // FIXME Section section(bot.mode == RT_BOT_SOLID);
	    if (!get_chex1(section, bot))
		write_bot(section, bot);

	    break;
	}

	case ID_HALF: {
	    const rt_half_internal &half = *static_cast<rt_half_internal *>
					   (internal.idb_ptr);
	    RT_HALF_CK_MAGIC(&half);

	    return find_compsplt(data, path, half);
	}

	default:
	    return false;
    }

    return !subtracted;
}


HIDDEN void
write_nmg_region(nmgregion *nmg_region, const db_full_path *path,
		 int UNUSED(region_id), int UNUSED(material_id), float *UNUSED(color),
		 void *client_data)
{
    NMG_CK_REGION(nmg_region);
    NMG_CK_MODEL(nmg_region->m_p);
    RT_CK_FULL_PATH(path);

    FastgenConversion &data = *static_cast<FastgenConversion *>(client_data);
    Section &section = get_section(data, *path);
    shell *vshell;

    for (BU_LIST_FOR(vshell, shell, &nmg_region->s_hd)) {
	NMG_CK_SHELL(vshell);

	rt_bot_internal * const bot = nmg_bot(vshell, &data.m_tol);

	// fill in an rt_db_internal with our new bot so we can free it
	rt_db_internal internal;
	AutoPtr<rt_db_internal, rt_db_free_internal> autofree_internal(&internal);
	RT_DB_INTERNAL_INIT(&internal);
	internal.idb_major_type = DB5_MAJORTYPE_BRLCAD;
	internal.idb_minor_type = ID_BOT;
	internal.idb_meth = &OBJ[ID_BOT];
	internal.idb_ptr = bot;

	write_bot(section, *bot);
    }
}


HIDDEN int
convert_region_start(db_tree_state *tree_state, const db_full_path *path,
		     const rt_comb_internal * comb, void *client_data)
{
    RT_CK_DBTS(tree_state);
    RT_CK_FULL_PATH(path);
    RT_CK_COMB(comb);

    FastgenConversion &data = *static_cast<FastgenConversion *>(client_data);
    data.get_region(*DB_FULL_PATH_CUR_DIR(path)).create_section(*path);

    return 1;
}


HIDDEN tree *
convert_leaf(db_tree_state *tree_state, const db_full_path *path,
	     rt_db_internal *internal, void *client_data)
{
    RT_CK_DBTS(tree_state);
    RT_CK_FULL_PATH(path);
    RT_CK_DB_INTERNAL(internal);

    FastgenConversion &data = *static_cast<FastgenConversion *>(client_data);
    const bool subtracted = tree_state->ts_sofar & (TS_SOFAR_MINUS |
			    TS_SOFAR_INTER);
    const directory *region_dir = NULL;
    bool converted = false;

    try {
	region_dir = &get_region_dir(data.m_db, *path);
    } catch (const std::invalid_argument &) {}

    const bool facetize = data.m_facetize_regions.count(region_dir);

    if (region_dir
	&& data.get_region(*region_dir).member_ignored(get_parent_dir(*path)))
	converted = true;
    else if (!facetize)
	converted = convert_primitive(data, *path, *internal, subtracted);

    if (!converted) {
	if (!facetize && subtracted)
	    data.m_failed_regions.insert(region_dir);
	else
	    return nmg_booltree_leaf_tess(tree_state, path, internal, client_data);
    }

    tree *result;
    RT_GET_TREE(result, tree_state->ts_resp);
    result->tr_op = OP_NOP;
    return result;
}


HIDDEN tree *
convert_region_end(db_tree_state *tree_state, const db_full_path *path,
		   tree *current_tree, void *client_data)
{
    RT_CK_DBTS(tree_state);
    RT_CK_FULL_PATH(path);
    RT_CK_TREE(current_tree);

    FastgenConversion &data = *static_cast<FastgenConversion *>(client_data);
    Section &section = get_section(data, *path);
    section.set_color(color_from_floats(tree_state->ts_mater.ma_color));

    if (current_tree->tr_op != OP_NOP) {
	gcv_region_end_data gcv_data = {write_nmg_region, &data};
	return gcv_region_end(tree_state, path, current_tree, &gcv_data);
    }

    return current_tree;
}


HIDDEN std::set<const directory *>
do_conversion(db_i &db, const std::string &path,
	      const std::set<const directory *> &facetize_regions =
		  std::set<const directory *>())
{
    RT_CK_DBI(&db);

    const rt_tess_tol ttol = {RT_TESS_TOL_MAGIC, 0.0, 1.0e-2, 0.0};
    const bn_tol tol = {BN_TOL_MAGIC, BN_TOL_DIST, BN_TOL_DIST * BN_TOL_DIST, 1.0e-6, 1.0 - 1.0e-6};

    AutoPtr<model, nmg_km> nmg_model;
    db_tree_state initial_tree_state = rt_initial_tree_state;
    initial_tree_state.ts_tol = &tol;
    initial_tree_state.ts_ttol = &ttol;
    initial_tree_state.ts_m = &nmg_model.ptr;

    db_update_nref(&db, &rt_uniresource);
    AutoPtr<directory *> toplevel_dirs;
    const std::size_t num_objects = db_ls(&db, DB_LS_TOPS, NULL,
					  &toplevel_dirs.ptr);
    AutoPtr<char *> object_names(db_dpv_to_argv(toplevel_dirs.ptr));
    toplevel_dirs.free();

    nmg_model.ptr = nmg_mm();
    FastgenConversion data(path, db, tol);
    data.m_facetize_regions = facetize_regions;
    const int ret = db_walk_tree(&db, static_cast<int>(num_objects),
				 const_cast<const char **>(object_names.ptr), 1, &initial_tree_state,
				 convert_region_start, convert_region_end, convert_leaf, &data);

    if (ret)
	throw std::runtime_error("db_walk_tree() failed");

    return data.m_failed_regions;
}


HIDDEN int
gcv_fastgen4_write(const char *path, struct db_i *dbip,
		   const gcv_opts *UNUSED(options))
{
    RT_CK_DBI(dbip);

    std::set<const directory *> failed_regions = do_conversion(*dbip, path);

    // facetize all regions that contain an incompatible boolean operation
    if (!failed_regions.empty())
	if (!do_conversion(*dbip, path, failed_regions).empty())
	    throw std::runtime_error("failed to convert all regions");

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
