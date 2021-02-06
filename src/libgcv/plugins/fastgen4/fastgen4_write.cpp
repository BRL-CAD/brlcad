/*              F A S T G E N 4 _ W R I T E . C P P
 * BRL-CAD
 *
 * Copyright (c) 2015-2021 United States Government as represented by
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

#include <cmath>

#include <iomanip>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <cstdlib>

#ifndef HAVE_DECL_FSEEKO
#include "bio.h" /* for b_off_t */
extern "C" int fseeko(FILE *, b_off_t, int);
extern "C" b_off_t ftello(FILE *);
#endif
#include <fstream>

#include "raytrace.h"
#include "gcv/api.h"
#include "gcv/util.h"

namespace fastgen4_write
{


static const fastf_t inches_per_mm = 1.0 / 25.4;


template <typename T> void
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


// Assignable/CopyConstructible for use with STL containers.
template <typename T>
class Triple
{
public:
    explicit Triple() :
	m_value() // zero
    {}


    explicit Triple(const T *values)
    {
	VMOVE(*this, values);
    }


    const T &operator[](std::size_t index) const
    {
	if (index > 2)
	    throw std::invalid_argument("invalid index");

	return m_value[index];
    }


    T &operator[](std::size_t index)
    {
	return const_cast<T &>(const_cast<const Triple<T> &>(*this)[index]);
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

    for (std::size_t i = 0; i < 3; ++i)
	result[i] = static_cast<unsigned char>(float_color[i] * 255.0 + 0.5);

    return result;
}


class InvalidModelError : public std::runtime_error
{
public:
    explicit InvalidModelError(const std::string &value) :
	std::runtime_error(value)
    {}
};


// Assignable/CopyConstructible for use with STL containers.
class Matrix
{
public:
    explicit Matrix(const fastf_t *values = NULL);

    bool equal(const Matrix &other, const bn_tol &tol) const;


private:
    mat_t m_value;
};


Matrix::Matrix(const fastf_t *values) :
    m_value()
{
    if (values)
	MAT_COPY(m_value, values);
    else
	MAT_IDN(m_value);
}


bool
Matrix::equal(const Matrix &other, const bn_tol &tol) const
{
    BN_CK_TOL(&tol);

    return bn_mat_is_equal(m_value, other.m_value, &tol) != 0;
}


class DBInternal
{
public:
    static const directory &lookup(const db_i &db, const std::string &name);

    explicit DBInternal();
    explicit DBInternal(const db_i &db, const directory &dir);
    ~DBInternal();

    void load(const db_i &db, const directory &dir);
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
    RT_CK_DBI(&db);

    const directory * const dir = db_lookup(&db, name.c_str(), LOOKUP_QUIET);

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
    RT_CK_DBI(&db);
    RT_CK_DIR(&dir);

    load(db, dir);
}


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

    if (0 > rt_db_get_internal(&m_internal, &dir, &db, NULL, &rt_uniresource))
	throw std::runtime_error("rt_db_get_internal() failed");

    m_valid = true;
    RT_CK_DB_INTERNAL(&m_internal);
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

    explicit RecordWriter();
    virtual ~RecordWriter();

    void write_comment(const std::string &value);


protected:
    virtual std::ostream &get_ostream() = 0;


private:
    RecordWriter(const RecordWriter &source);
    RecordWriter &operator=(const RecordWriter &source);

    bool m_record_open;
};


RecordWriter::RecordWriter() :
    m_record_open(false)
{}


RecordWriter::~RecordWriter()
{}


class RecordWriter::Record
{
public:
    static const std::size_t field_width = 8;

    explicit Record(RecordWriter &writer);
    ~Record();

    template <typename T> Record &operator<<(const T &value);
    Record &operator<<(float value);
    Record &operator<<(double value);
    Record &non_zero(fastf_t value);
    Record &text(const std::string &value);


private:
    static const std::size_t record_width = 10;

    static std::string truncate_float(fastf_t value);

    std::size_t m_width;
    RecordWriter &m_writer;
};


RecordWriter::Record::Record(RecordWriter &writer) :
    m_width(0),
    m_writer(writer)
{
    if (m_writer.m_record_open)
	throw std::logic_error("record open");

    m_writer.m_record_open = true;
}


RecordWriter::Record::~Record()
{
    if (m_width)
	m_writer.get_ostream().put('\n');

    m_writer.m_record_open = false;
}


template <typename T> RecordWriter::Record &
RecordWriter::Record::operator<<(const T &value)
{
    if (++m_width > record_width)
	throw std::length_error("invalid record width");

    std::ostringstream sstream;
    sstream.exceptions(std::ostream::failbit | std::ostream::badbit);
    sstream << value;

    const std::string str_val = sstream.str();

    if (str_val.size() > field_width)
	throw std::length_error("length exceeds field width");

    m_writer.get_ostream() << std::left << std::setw(field_width) << str_val;
    return *this;
}


RecordWriter::Record &
RecordWriter::Record::operator<<(float value)
{
    return operator<<(static_cast<double>(value));
}


RecordWriter::Record &
RecordWriter::Record::operator<<(double value)
{
    std::string string_value = truncate_float(value);

    if (string_value == "-0.0")
	string_value.erase(0, 1);

    return operator<<(string_value);

    // quell warning of unused function
    // `float` and `double` overrides provided to match `fastf_t` despite templates
    bu_bomb("shouldn't reach here");
    return operator<<(static_cast<float>(0.0));
}


// ensure that the truncated value isn't "0.0"
RecordWriter::Record &
RecordWriter::Record::non_zero(fastf_t value)
{
    std::string result = truncate_float(value);

    if (result.find_first_not_of("-0.") == std::string::npos) {
	result.resize(field_width, '0');
	result.at(result.size() - 1) = '1';
    }

    return operator<<(result);
}


RecordWriter::Record &
RecordWriter::Record::text(const std::string &value)
{
    m_width = record_width;
    m_writer.get_ostream() << value;
    return *this;
}


std::string
RecordWriter::Record::truncate_float(fastf_t value)
{
    std::ostringstream sstream;
    sstream.exceptions(std::ostream::failbit | std::ostream::badbit);
    sstream.precision(field_width);
    sstream << std::fixed << value;
    std::string result = sstream.str().substr(0, field_width);

    const std::size_t end_digit = result.find_last_not_of('0');
    const std::size_t end_point = result.find('.');

    // remove redundant trailing zeros
    result.erase(std::min(result.size(), std::max(end_point + 2, end_digit + 1)));

    if (end_point >= result.size() - 1)
	throw InvalidModelError("value exceeds width of field");

    return result;
}


void
RecordWriter::write_comment(const std::string &value)
{
    (Record(*this) << "$COMMENT").text(" ").text(value);
}


// simple way to order records properly within the output file
class StringBuffer : public RecordWriter
{
public:
    explicit StringBuffer();

    void write(RecordWriter &writer) const;


protected:
    virtual std::ostream &get_ostream();


private:
    std::ostringstream m_ostream;
};


StringBuffer::StringBuffer() :
    m_ostream()
{
    m_ostream.exceptions(std::ostream::failbit | std::ostream::badbit);
}


void
StringBuffer::write(RecordWriter &writer) const
{
    RecordWriter::Record record(writer);
    record.text(m_ostream.str());
}


std::ostream &
StringBuffer::get_ostream()
{
    return m_ostream;
}


class FastgenWriter : public RecordWriter
{
public:
    typedef std::pair<std::size_t, std::size_t> SectionID; // group, component

    explicit FastgenWriter(const std::string &path);
    ~FastgenWriter();

    void write_section_color(const SectionID &section_id, const Color &color);
    SectionID write_compsplt(const SectionID &id, fastf_t z_coordinate);

    enum BooleanType {bool_hole, bool_wall};
    void write_boolean(BooleanType type, const SectionID &section_a,
		       const SectionID &section_b, const SectionID *section_c = NULL,
		       const SectionID *section_d = NULL);

    SectionID take_next_section_id();

    RecordWriter &get_section_writer();


protected:
    virtual std::ostream &get_ostream();


private:
    static const std::size_t max_group_id = 49;
    static const std::size_t max_section_id = 999;
    static const std::size_t max_bools = 40000;

    SectionID m_next_section_id;
    std::size_t m_num_holes, m_num_walls;
    StringBuffer m_sections;
    std::ofstream m_ostream, m_colors_ostream;
};


FastgenWriter::FastgenWriter(const std::string &path) :
    m_next_section_id(0, 1),
    m_num_holes(0),
    m_num_walls(0),
    m_sections(),
    m_ostream(path.c_str(), std::ostream::out),
    m_colors_ostream((path + ".colors").c_str(), std::ostream::out)
{
    m_ostream.exceptions(std::ostream::failbit | std::ostream::badbit);
    m_colors_ostream.exceptions(std::ostream::failbit | std::ostream::badbit);
}


FastgenWriter::~FastgenWriter()
{
    m_sections.write(*this);
    Record(*this) << "ENDDATA";
}


std::ostream &
FastgenWriter::get_ostream()
{
    return m_ostream;
}


FastgenWriter::SectionID
FastgenWriter::take_next_section_id()
{
    const SectionID result = m_next_section_id;

    if (result.first > max_group_id)
	throw InvalidModelError("maximum Sections exceeded");

    if (++m_next_section_id.second > max_section_id) {
	++m_next_section_id.first;
	m_next_section_id.second = 1;
    }

    return result;
}


RecordWriter &
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
    z_coordinate *= inches_per_mm;

    Record record(*this);
    const SectionID result = take_next_section_id();

    record << "COMPSPLT" << id.first << id.second;
    record << result.first << result.second;
    record << z_coordinate;

    return result;
}


void
FastgenWriter::write_boolean(BooleanType type, const SectionID &section_a,
			     const SectionID &section_b, const SectionID *section_c,
			     const SectionID *section_d)
{
    Record record(*this);

    if (type == bool_hole) {
	if (++m_num_holes > max_bools)
	    throw InvalidModelError("maximum HOLE records exceeded");

	record << "HOLE";
    } else if (type == bool_wall) {
	if (++m_num_walls > max_bools)
	    throw InvalidModelError("maximum WALL records exceeded");

	record << "WALL";
    } else
	throw std::invalid_argument("unknown Boolean type");

    record << section_a.first << section_a.second;
    record << section_b.first << section_b.second;

    if (section_c)
	record << section_c->first << section_c->second;

    if (section_d)
	record << section_d->first << section_d->second;
}


// stores grid IDs and reuses duplicate grid points when possible
class GridManager
{
public:
    explicit GridManager();

    // return a vector of grid IDs corresponding to the given points,
    // with no duplicate indices in the returned vector
    std::vector<std::size_t> get_unique_grids(const std::vector<Point> &points);

    void write(RecordWriter &writer) const;


private:
    struct PointComparator {
	bool operator()(const Point &lhs, const Point &rhs) const;
    };

    static const std::size_t max_grid_points = 50000;

    std::size_t m_next_grid_id;
    std::map<Point, std::vector<std::size_t>, PointComparator> m_grids;
};


bool
GridManager::PointComparator::operator()(const Point &lhs,
	const Point &rhs) const
{
    for (std::size_t i = 0; i < 3; ++i)
	if (!NEAR_EQUAL(lhs[i], rhs[i], RT_LEN_TOL))
	    return lhs[i] < rhs[i];

    return false;
}


GridManager::GridManager() :
    m_next_grid_id(1),
    m_grids()
{}


std::vector<std::size_t>
GridManager::get_unique_grids(const std::vector<Point> &points)
{
    std::vector<std::size_t> results(points.size());

    for (std::size_t i = 0; i < points.size(); ++i) {
	std::pair<std::map<Point, std::vector<std::size_t>, PointComparator>::iterator, bool>
	found;
	{
	    std::vector<std::size_t> temp(1);
	    temp.at(0) = m_next_grid_id;
	    found = m_grids.insert(std::make_pair(points.at(i), temp));
	}
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
    if (m_next_grid_id - 1 > max_grid_points)
	throw InvalidModelError("maximum grid points exceeded");

    for (std::map<Point, std::vector<std::size_t>, PointComparator>::const_iterator
	 it = m_grids.begin(); it != m_grids.end(); ++it)
	for (std::vector<std::size_t>::const_iterator id_it = it->second.begin();
	     id_it != it->second.end(); ++id_it) {
	    RecordWriter::Record record(writer);
	    record << "GRID" << *id_it << "";
	    record << it->first[X] * inches_per_mm;
	    record << it->first[Y] * inches_per_mm;
	    record << it->first[Z] * inches_per_mm;
	}
}


class Section
{
public:
    class SectionModeError;

    explicit Section();

    bool empty() const;

    bool has_color() const;
    Color get_color() const;
    void set_color(const Color &value);

    void write(RecordWriter &writer, const FastgenWriter::SectionID &id,
	       const std::string &name) const;

    // create a comment describing an element
    void write_name(const std::string &value);

    void write_line(const fastf_t *point_a, const fastf_t *point_b, fastf_t radius,
		    fastf_t thickness);

    void write_sphere(const fastf_t *center, fastf_t radius, fastf_t thickness);

    // note: this element (CCONE1) is deprecated
    void write_thin_cone(const fastf_t *point_a, const fastf_t *point_b,
			 fastf_t radius1, fastf_t radius2, fastf_t thickness,
			 bool end1_open = true, bool end2_open = true);

    void write_cone(const fastf_t *point_a, const fastf_t *point_b, fastf_t ro1,
		    fastf_t ro2, fastf_t ri1, fastf_t ri2);

    void write_triangle(const fastf_t *point_a, const fastf_t *point_b,
			const fastf_t *point_c, fastf_t thickness, bool grid_centered = true);

    void write_quad(const fastf_t *point_a, const fastf_t *point_b,
		    const fastf_t *point_c, const fastf_t *point_d, fastf_t thickness,
		    bool grid_centered = true);

    void write_hexahedron(const fastf_t points[8][3], fastf_t thickness,
			  bool grid_centered = true);


    std::set<const directory *> m_completed_cutouts;


private:
    static const bool check_mode_errors = false;

    const bool m_volume_mode;
    const std::size_t m_material_id;

    std::pair<bool, Color> m_color; // optional color

    GridManager m_grids;
    StringBuffer m_elements;
    std::size_t m_next_element_id;
};


class Section::SectionModeError : public std::logic_error
{
public:
    explicit SectionModeError(const std::string &message) :
	std::logic_error("Section mode inconsistency: " + message)
    {}
};


Section::Section() :
    m_completed_cutouts(),
    m_volume_mode(!check_mode_errors),
    m_material_id(1),
    m_color(false, Color()),
    m_grids(),
    m_elements(),
    m_next_element_id(1)
{}


bool
Section::empty() const
{
    return m_next_element_id == 1;
}


bool
Section::has_color() const
{
    return m_color.first;
}


Color
Section::get_color() const
{
    if (!has_color())
	throw std::logic_error("no color information for this Section");

    return m_color.second;
}


void
Section::set_color(const Color &value)
{
    m_color = std::make_pair(true, value);
}


void
Section::write(RecordWriter &writer, const FastgenWriter::SectionID &id,
	       const std::string &name) const
{
    if (empty())
	throw std::logic_error("empty Section");

    {
	const std::size_t max_name_size = RecordWriter::Record::field_width * 3;
	std::string short_name = name;

	if (short_name.size() > max_name_size) {
	    writer.write_comment(short_name);
	    short_name = "..." + short_name.substr(short_name.size() - max_name_size + 3);
	}

	RecordWriter::Record record(writer);
	record << "$NAME" << id.first << id.second;
	record << "" << "" << "" << "";
	record.text(short_name);
    }

    RecordWriter::Record(writer)
	    << "SECTION" << id.first << id.second << (m_volume_mode ? 2 : 1);

    m_grids.write(writer);
    m_elements.write(writer);
}


void
Section::write_name(const std::string &value)
{
    m_elements.write_comment(value);
}


void
Section::write_line(const fastf_t *point_a, const fastf_t *point_b,
		    fastf_t radius, fastf_t thickness)
{
    radius *= inches_per_mm;
    thickness *= inches_per_mm;

    if (check_mode_errors)
	if (!m_volume_mode && NEAR_ZERO(thickness, RT_LEN_TOL))
	    throw SectionModeError("line with zero thickness in a plate-mode component");

    if (thickness < 0.0 || thickness > radius)
	throw std::invalid_argument("invalid thickness");

    if (radius <= 0.0)
	throw std::invalid_argument("invalid radius");

    std::vector<Point> points(2);
    points.at(0) = Point(point_a);
    points.at(1) = Point(point_b);
    const std::vector<std::size_t> grids = m_grids.get_unique_grids(points);

    RecordWriter::Record record(m_elements);
    record << "CLINE" << m_next_element_id << m_material_id;
    record << grids.at(0) << grids.at(1) << "" << "";

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
    radius *= inches_per_mm;
    thickness *= inches_per_mm;

    if (NEAR_ZERO(thickness, RT_LEN_TOL))
	thickness = radius;

    if (check_mode_errors)
	if (m_volume_mode && !NEAR_EQUAL(thickness, radius, RT_LEN_TOL))
	    throw SectionModeError("Sphere with thickness in a volume-mode Section");

    if (radius < thickness || thickness <= 0.0)
	throw std::invalid_argument("invalid value");

    std::vector<Point> points(1);
    points.at(0) = Point(center);
    const std::vector<std::size_t> grids = m_grids.get_unique_grids(points);

    RecordWriter::Record record(m_elements);
    record << "CSPHERE" << m_next_element_id << m_material_id;
    record << grids.at(0) << "" << "" << "";

    if (m_volume_mode)
	record << "";
    else
	record.non_zero(thickness);

    record.non_zero(radius);
    ++m_next_element_id;
}


void
Section::write_thin_cone(const fastf_t *point_a, const fastf_t *point_b,
			 fastf_t radius1, fastf_t radius2, fastf_t thickness,
			 bool end1_open, bool end2_open)
{
    radius1 *= inches_per_mm;
    radius2 *= inches_per_mm;
    thickness *= inches_per_mm;

    if (check_mode_errors)
	if (m_volume_mode)
	    throw SectionModeError("CCONE1 in volume-mode component should be a CCONE2");

    if (radius1 < 0.0 || radius2 < 0.0)
	throw std::invalid_argument("invalid radius");

    if (thickness <= 0.0 || (radius1 < thickness && radius2 < thickness))
	throw std::invalid_argument("invalid thickness");

    std::vector<Point> points(2);
    points.at(0) = Point(point_a);
    points.at(1) = Point(point_b);
    const std::vector<std::size_t> grids = m_grids.get_unique_grids(points);

    {
	RecordWriter::Record record1(m_elements);
	record1 << "CCONE1" << m_next_element_id << m_material_id;
	record1 << grids.at(0) << grids.at(1) << "" << "";
	record1 << thickness << radius1 << m_next_element_id;
    }

    {
	RecordWriter::Record record2(m_elements);
	record2 << m_next_element_id << radius2;

	end1_open = end1_open || !NEAR_ZERO(radius1, RT_LEN_TOL);
	end2_open = end2_open || !NEAR_ZERO(radius2, RT_LEN_TOL);

	if (!end1_open)
	    record2 << 2;
	else if (!end2_open)
	    record2 << "";

	if (!end2_open)
	    record2 << 2;
    }

    ++m_next_element_id;
}


void
Section::write_cone(const fastf_t *point_a, const fastf_t *point_b, fastf_t ro1,
		    fastf_t ro2, fastf_t ri1, fastf_t ri2)
{
    ro1 *= inches_per_mm;
    ro2 *= inches_per_mm;
    ri1 *= inches_per_mm;
    ri2 *= inches_per_mm;

    if (check_mode_errors)
	if (!m_volume_mode)
	    throw SectionModeError("CCONE2 element in plate-mode Section");

    if (ri1 < 0.0 || ri2 < 0.0 || ro1 < ri1 || ro2 < ri2)
	throw std::invalid_argument("invalid radius");

    if (NEAR_ZERO(ro1, RT_LEN_TOL) && NEAR_ZERO(ro2, RT_LEN_TOL))
	throw std::invalid_argument("invalid radius");

    std::vector<Point> points(2);
    points.at(0) = Point(point_a);
    points.at(1) = Point(point_b);
    const std::vector<std::size_t> grids = m_grids.get_unique_grids(points);

    {
	RecordWriter::Record record1(m_elements);
	record1 << "CCONE2" << m_next_element_id << m_material_id;
	record1 << grids.at(0) << grids.at(1);
	record1 << "" << "" << "" << ro1 << m_next_element_id;
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
    thickness *= inches_per_mm;

    if (check_mode_errors)
	if (NEAR_ZERO(thickness, RT_LEN_TOL) != m_volume_mode) {
	    if (m_volume_mode)
		throw SectionModeError("triangle with thickness in a volume-mode Section");
	    else
		throw SectionModeError("triangle without thickness in a plate-mode Section");
	}

    if (thickness < 0.0)
	throw std::invalid_argument("invalid thickness");

    std::vector<Point> points(3);
    points.at(0) = Point(point_a);
    points.at(1) = Point(point_b);
    points.at(2) = Point(point_c);
    const std::vector<std::size_t> grids = m_grids.get_unique_grids(points);

    RecordWriter::Record record(m_elements);
    record << "CTRI" << m_next_element_id << m_material_id;
    record << grids.at(0) << grids.at(1) << grids.at(2);

    if (!m_volume_mode) {
	record.non_zero(thickness);

	if (grid_centered)
	    record << 1;
    }

    ++m_next_element_id;
}


void
Section::write_quad(const fastf_t *point_a, const fastf_t *point_b,
		    const fastf_t *point_c, const fastf_t *point_d, fastf_t thickness,
		    bool grid_centered)
{
    thickness *= inches_per_mm;

    if (check_mode_errors)
	if (NEAR_ZERO(thickness, RT_LEN_TOL) != m_volume_mode) {
	    if (m_volume_mode)
		throw SectionModeError("quad with thickness in a volume-mode Section");
	    else
		throw SectionModeError("quad without thickness in a plate-mode Section");
	}

    if (thickness < 0.0)
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

    if (!m_volume_mode) {
	record.non_zero(thickness);

	if (grid_centered)
	    record << 1;
    }

    ++m_next_element_id;

}


void
Section::write_hexahedron(const fastf_t vpoints[8][3], fastf_t thickness,
			  bool grid_centered)
{
    thickness *= inches_per_mm;

    const bool has_thickness = !NEAR_ZERO(thickness, RT_LEN_TOL);

    if (check_mode_errors)
	if (m_volume_mode && has_thickness)
	    throw SectionModeError("hexahedron with thickness in a volume-mode Section");

    if (thickness < 0.0)
	throw std::invalid_argument("invalid thickness");

    std::vector<Point> points(8);

    for (std::size_t i = 0; i < 8; ++i)
	points.at(i) = Point(vpoints[i]);

    const std::vector<std::size_t> grids = m_grids.get_unique_grids(points);

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

    if (has_thickness) {
	record2 << "" << "" << "" << "" << thickness;

	if (grid_centered)
	    record2 << 1;
    }

    ++m_next_element_id;
}


// returns: thickness, thickness_is_centered
HIDDEN std::pair<fastf_t, bool>
get_face_info(const rt_bot_internal &bot, std::size_t i)
{
    RT_BOT_CK_MAGIC(&bot);

    if (i > bot.num_faces)
	throw std::invalid_argument("invalid face index");

    // defaults
    std::pair<fastf_t, bool> result(1.0, true);

    if (bot.thickness)
	result.first = bot.thickness[i];

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

	// quick check for CQUAD-compatible faces
	if (i + 1 < bot.num_faces) {
	    const int * const next_face = &bot.faces[(i + 1) * 3];
	    const std::pair<fastf_t, bool> next_face_info = get_face_info(bot, i + 1);

	    if (face[0] == next_face[0] && face[2] == next_face[1])
		if (NEAR_EQUAL(face_info.first, next_face_info.first, RT_LEN_TOL)
		    && face_info.second == next_face_info.second) {
		    const fastf_t * const v4 = &bot.vertices[next_face[2] * 3];
		    section.write_quad(v1, v2, v3, v4, face_info.first, face_info.second);
		    ++i;
		    continue;
		}
	}

	section.write_triangle(v1, v2, v3, face_info.first, face_info.second);
    }
}


/* FIXME: need to consolidate with similar code in rt_sph_prep(),
 * possibly in libbg or handled as a typing interface in librt as
 * other entities have similar needs, e.g., tgc, eto, superell, arb8.
 */
HIDDEN bool
ell_is_sphere(const rt_ell_internal &ell)
{
    RT_ELL_CK_MAGIC(&ell);

    // validate that |A| > 0, |B| > 0, |C| > 0
    if (NEAR_ZERO(MAGNITUDE(ell.a), RT_LEN_TOL)
	|| NEAR_ZERO(MAGNITUDE(ell.b), RT_LEN_TOL)
	|| NEAR_ZERO(MAGNITUDE(ell.c), RT_LEN_TOL))
	return false;

    // validate that |A|, |B|, and |C| are nearly equal
    if (!NEAR_EQUAL(MAGNITUDE(ell.a), MAGNITUDE(ell.b), RT_LEN_TOL)
	|| !NEAR_EQUAL(MAGNITUDE(ell.a), MAGNITUDE(ell.c), RT_LEN_TOL))
	return false;

    // validate that Au.Bu == 0, Bu.Cu == 0, Au.Cu == 0 (check dir only)
    {
	vect_t a_u, b_u, c_u; // A, B, C with unit length
	VMOVE(a_u, ell.a);
	VMOVE(b_u, ell.b);
	VMOVE(c_u, ell.c);
	VUNITIZE(a_u);
	VUNITIZE(b_u);
	VUNITIZE(c_u);

	if (!NEAR_ZERO(VDOT(a_u, b_u), RT_DOT_TOL))
	    return false;

	if (!NEAR_ZERO(VDOT(b_u, c_u), RT_DOT_TOL))
	    return false;

	if (!NEAR_ZERO(VDOT(a_u, c_u), RT_DOT_TOL))
	    return false;
    }

    return true;
}


HIDDEN bool
mutually_orthogonal(const fastf_t *vect_a, const fastf_t *vect_b,
		    const fastf_t *vect_c)
{
    return NEAR_ZERO(VDOT(vect_a, vect_b), RT_DOT_TOL)
	   && NEAR_ZERO(VDOT(vect_a, vect_c), RT_DOT_TOL)
	   && NEAR_ZERO(VDOT(vect_b, vect_c), RT_DOT_TOL);
}


// Determines whether a tgc can be represented by a CCONE2 element.
// Assumes that `tgc` is a valid rt_tgc_internal.
HIDDEN bool
tgc_is_ccone2(const rt_tgc_internal &tgc)
{
    RT_TGC_CK_MAGIC(&tgc);

    if (NEAR_ZERO(MAGNITUDE(tgc.h), RT_LEN_TOL))
	return false;

    // ensure |A| == |B| and |C| == |D|
    if (!NEAR_EQUAL(MAGNITUDE(tgc.a), MAGNITUDE(tgc.b), RT_LEN_TOL)
	|| !NEAR_EQUAL(MAGNITUDE(tgc.c), MAGNITUDE(tgc.d), RT_LEN_TOL))
	return false;

    // handle non-truncated cones
    if (NEAR_ZERO(MAGNITUDE(tgc.a), RT_LEN_TOL))
	return mutually_orthogonal(tgc.h, tgc.c, tgc.d);
    else if (NEAR_ZERO(MAGNITUDE(tgc.c), RT_LEN_TOL))
	return mutually_orthogonal(tgc.h, tgc.a, tgc.b);

    if (!mutually_orthogonal(tgc.h, tgc.a, tgc.b))
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
path_to_mat(db_i &db, const db_full_path &path, mat_t &result)
{
    RT_CK_DBI(&db);
    RT_CK_FULL_PATH(&path);

    db_full_path temp;
    const AutoPtr<db_full_path, db_free_full_path> autofree_path(&temp);
    db_full_path_init(&temp);
    db_dup_full_path(&temp, &path);

    if (!db_path_to_mat(&db, &temp, result, 0, &rt_uniresource))
	throw std::runtime_error("db_path_to_mat() failed");
}


HIDDEN void
apply_path_xform(db_i &db, const mat_t &matrix, rt_db_internal &internal)
{
    RT_CK_DBI(&db);
    RT_CK_DB_INTERNAL(&internal);

    if (rt_obj_xform(&internal, matrix, &internal, 0, &db))
	throw std::runtime_error("rt_obj_xform() failed");
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
get_region_dir(const db_full_path &path)
{
    RT_CK_FULL_PATH(&path);

    for (std::size_t i = 0; i < path.fp_len; ++i)
	if (DB_FULL_PATH_GET(&path, i)->d_flags & RT_DIR_REGION)
	    return *DB_FULL_PATH_GET(&path, i);

    throw std::invalid_argument("no parent region");
}


// get the parent region's path
HIDDEN const db_full_path
get_region_path(const db_full_path &path)
{
    RT_CK_FULL_PATH(&path);

    const directory &region_dir = get_region_dir(path);
    db_full_path result = path;

    while (DB_FULL_PATH_CUR_DIR(&result) != &region_dir)
	DB_FULL_PATH_POP(&result);

    return result;
}


HIDDEN bool
path_is_subtracted(db_i &db, const db_full_path &path)
{
    RT_CK_DBI(&db);
    RT_CK_FULL_PATH(&path);

    db_tree_state tree_state = rt_initial_tree_state;
    tree_state.ts_resp = &rt_uniresource;
    tree_state.ts_dbip = &db;

    db_full_path end_path;
    const AutoPtr<db_full_path, db_free_full_path> autofree_end_path(&end_path);
    db_full_path_init(&end_path);

    if (db_follow_path(&tree_state, &end_path, &path, false, 0))
	throw std::runtime_error("db_follow_path() failed");

    return (tree_state.ts_sofar & (TS_SOFAR_MINUS | TS_SOFAR_INTER)) != 0;
}


// helper; sets `outer` and `inner` if they exist in
// a tree describing `outer` - `inner`
HIDDEN bool
get_cutout(db_i &db, const db_full_path &parent_path, DBInternal &outer,
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
	const AutoPtr<db_full_path, db_free_full_path> autofree_temp(&temp);
	db_full_path_init(&temp);
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


// most uses of CCONE1 elements can be represented as CCONE2 elements.
// this handles the cases for which we can't do this.
// note: CCONE1 is deprecated.
HIDDEN bool
get_ccone1_cutout_helper(Section &section, const std::string &name,
			 const rt_tgc_internal &outer_tgc, const rt_tgc_internal &inner_tgc)
{
    RT_TGC_CK_MAGIC(&outer_tgc);
    RT_TGC_CK_MAGIC(&inner_tgc);

    const fastf_t radius1 = MAGNITUDE(outer_tgc.a);
    const fastf_t radius2 = MAGNITUDE(outer_tgc.c);
    const fastf_t length = MAGNITUDE(outer_tgc.h);
    const fastf_t inner_radius1 = MAGNITUDE(inner_tgc.a);
    const fastf_t inner_radius2 = MAGNITUDE(inner_tgc.c);
    const bool end1_open = VNEAR_EQUAL(inner_tgc.v, outer_tgc.v, RT_LEN_TOL);
    const bool end2_open = VNEAR_EQUAL(inner_tgc.h, outer_tgc.h, RT_LEN_TOL);

    // sin_angle = length / slant_length
    const fastf_t sin_angle = length / std::sqrt(length * length +
			      (radius2 - radius1) * (radius2 - radius1));

    fastf_t thickness = -1.0;

    if (!NEAR_ZERO(inner_radius1, RT_LEN_TOL)) {
	if (end1_open)
	    thickness = (radius1 - inner_radius1) * sin_angle;
	else {
	    thickness =
		(inner_radius1 - radius1) / ((radius2 - radius1) / length - 1.0 / sin_angle);

	    vect_t temp_base, h_dir;
	    VMOVE(h_dir, outer_tgc.h);
	    VUNITIZE(h_dir);
	    VJOIN1(temp_base, outer_tgc.v, thickness, h_dir);

	    if (!VNEAR_EQUAL(temp_base, inner_tgc.v, RT_LEN_TOL))
		return false;
	}
    }

    if (!NEAR_ZERO(inner_radius2, RT_LEN_TOL)) {
	fastf_t temp_thickness;

	if (end2_open)
	    temp_thickness = (radius2 - inner_radius2) * sin_angle;
	else
	    temp_thickness = (inner_radius2 - radius2) / ((radius1 - radius2) / length -
			     1.0 / sin_angle);

	if (thickness < 0.0)
	    thickness = temp_thickness;
	else if (!NEAR_EQUAL(temp_thickness, thickness, RT_LEN_TOL))
	    return false;
    }

    vect_t point_b;
    VADD2(point_b, outer_tgc.v, outer_tgc.h);
    section.write_name(name);

    if (thickness < 0.0) // `inner_radius1` and `inner_radius2` are zero
	section.write_cone(outer_tgc.v, point_b, radius1, radius2, 0.0, 0.0);
    else
	section.write_thin_cone(outer_tgc.v, point_b, radius1, radius2, thickness,
				end1_open, end2_open);

    return true;
}


// test for and create CCONE{1, 2} elements
HIDDEN bool
find_ccone_cutout(Section &section, db_i &db, const db_full_path &parent_path,
		  std::set<const directory *> &completed)
{
    RT_CK_DBI(&db);
    RT_CK_FULL_PATH(&parent_path);

    const directory &parent_dir = *DB_FULL_PATH_CUR_DIR(&parent_path);

    if (completed.count(&parent_dir))
	return true; // already processed

    DBInternal internal_first, internal_second;

    if (!get_cutout(db, parent_path, internal_first, internal_second))
	return false;

    if ((internal_first.get().idb_minor_type != ID_TGC
	 && internal_first.get().idb_minor_type != ID_REC)
	|| (internal_second.get().idb_minor_type != ID_TGC
	    && internal_second.get().idb_minor_type != ID_REC))
	return false;

    const rt_tgc_internal &outer_tgc = *static_cast<rt_tgc_internal *>
				       (internal_first.get().idb_ptr);
    const rt_tgc_internal &inner_tgc = *static_cast<rt_tgc_internal *>
				       (internal_second.get().idb_ptr);
    RT_TGC_CK_MAGIC(&outer_tgc);
    RT_TGC_CK_MAGIC(&inner_tgc);

    // check cone geometry
    if (!tgc_is_ccone2(outer_tgc) || !tgc_is_ccone2(inner_tgc))
	return false;

    const fastf_t ro1 = MAGNITUDE(outer_tgc.a);
    const fastf_t ro2 = MAGNITUDE(outer_tgc.c);
    const fastf_t ri1 = MAGNITUDE(inner_tgc.a);
    const fastf_t ri2 = MAGNITUDE(inner_tgc.c);

    // check radii
    if (ri1 > ro1 || ri2 > ro2)
	return false;

    if (!VNEAR_EQUAL(outer_tgc.v, inner_tgc.v, RT_LEN_TOL)
	|| !VNEAR_EQUAL(outer_tgc.h, inner_tgc.h, RT_LEN_TOL)) {
	if (!get_ccone1_cutout_helper(section, parent_dir.d_namep, outer_tgc,
				      inner_tgc))
	    return false;

	// this was a CCONE1
	completed.insert(&parent_dir);
	return true;
    }

    point_t v2;
    VADD2(v2, outer_tgc.v, outer_tgc.h);
    section.write_name(parent_dir.d_namep);
    section.write_cone(outer_tgc.v, v2, ro1, ro2, ri1, ri2);
    completed.insert(&parent_dir);
    return true;
}


// test for and create CSPHERE elements
HIDDEN bool
find_csphere_cutout(Section &section, db_i &db, const db_full_path &parent_path,
		    std::set<const directory *> &completed)
{
    RT_CK_DBI(&db);
    RT_CK_FULL_PATH(&parent_path);

    const directory &parent_dir = *DB_FULL_PATH_CUR_DIR(&parent_path);

    if (completed.count(&parent_dir))
	return true; // already processed

    DBInternal internal_first, internal_second;

    if (!get_cutout(db, parent_path, internal_first, internal_second))
	return false;

    if ((internal_first.get().idb_minor_type != ID_SPH
	 && internal_first.get().idb_minor_type != ID_ELL)
	|| (internal_second.get().idb_minor_type != ID_SPH
	    && internal_second.get().idb_minor_type != ID_ELL))
	return false;

    const rt_ell_internal &outer_ell = *static_cast<rt_ell_internal *>
				       (internal_first.get().idb_ptr);
    const rt_ell_internal &inner_ell = *static_cast<rt_ell_internal *>
				       (internal_second.get().idb_ptr);
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

    if (bot.num_vertices != 8 || bot.num_faces != 12)
	return false;

    if (bot.mode != RT_BOT_SOLID && bot.mode != RT_BOT_PLATE)
	return false;

    {
	const int hex_faces[12][3] = {
	    {0, 1, 4},
	    {1, 5, 4},
	    {1, 2, 5},
	    {2, 6, 5},
	    {2, 3, 6},
	    {3, 7, 6},
	    {3, 0, 7},
	    {0, 4, 7},
	    {4, 6, 7},
	    {4, 5, 6},
	    {0, 1, 2},
	    {0, 2, 3}
	};

	for (std::size_t i = 0; i < 12; ++i) {
	    const int * const face = &bot.faces[i * 3];

	    if (face[0] != hex_faces[i][0] || face[1] != hex_faces[i][1]
		|| face[2] != hex_faces[i][2])
		return false;
	}
    }

    const fastf_t hex_thickness = bot.thickness ? bot.thickness[0] : 0.0;
    const bool hex_grid_centered = bot.face_mode ? !BU_BITTEST(bot.face_mode,
				   0) : true;

    if (bot.thickness)
	for (std::size_t i = 0; i < bot.num_faces; ++i)
	    if (!NEAR_EQUAL(bot.thickness[i], hex_thickness, RT_LEN_TOL))
		return false;

    if (bot.face_mode) {
	// check that all faces have a uniform face mode
	std::size_t count = 0;
	BU_BITV_LOOP_START(bot.face_mode) {
	    ++count;
	}
	BU_BITV_LOOP_END;

	if (hex_grid_centered) {
	    if (count)
		return false;
	} else if (count != bot.num_faces)
	    return false;
    }

    fastf_t points[8][3];

    for (std::size_t i = 0; i < 8; ++i)
	VMOVE(points[i], &bot.vertices[i * 3]);

    section.write_hexahedron(points, hex_thickness, hex_grid_centered);
    return true;
}


typedef std::map<const directory *, Matrix> LeafMap;


HIDDEN void
get_unioned(const db_i &db, const tree *tree, LeafMap &results)
{
    RT_CK_DBI(&db);

    if (!tree)
	return;

    RT_CK_TREE(tree);

    switch (tree->tr_op) {
	case OP_DB_LEAF: {
	    const directory &dir = DBInternal::lookup(db, tree->tr_l.tl_name);
	    results.insert(std::make_pair(&dir, Matrix(tree->tr_l.tl_mat)));
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
	    get_intersected(db, tree->tr_b.tb_left, results);
	    get_intersected(db, tree->tr_b.tb_right, results);
	    get_unioned(db, tree->tr_b.tb_left, results);
	    get_unioned(db, tree->tr_b.tb_right, results);
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
// Assumes that `half_dir` represents a COMPSPLT-compatible halfspace.
// Returns the partition type and the halfspace's matrix within the region.
enum CompspltPartitionType {partition_none, partition_intersected, partition_subtracted};
typedef std::pair<CompspltPartitionType, Matrix> CompspltID;

HIDDEN CompspltID
identify_compsplt(const db_i &db, const directory &parent_region_dir,
		  const directory &half_dir)
{
    RT_CK_DBI(&db);
    RT_CK_DIR(&parent_region_dir);
    RT_CK_DIR(&half_dir);

    if (half_dir.d_minor_type != ID_HALF)
	throw std::invalid_argument("identify_compsplt(): not a halfspace");

    DBInternal parent_region_internal(db, parent_region_dir);
    const rt_comb_internal &parent_region = *static_cast<rt_comb_internal *>
					    (parent_region_internal.get().idb_ptr);
    RT_CK_COMB(&parent_region);

    LeafMap leaves;
    get_intersected(db, parent_region.tree, leaves);
    LeafMap::const_iterator found = leaves.find(&half_dir);

    if (found != leaves.end())
	return CompspltID(partition_intersected, found->second);

    leaves.clear();
    get_subtracted(db, parent_region.tree, leaves);
    found = leaves.find(&half_dir);

    if (found != leaves.end())
	return CompspltID(partition_subtracted, found->second);

    return CompspltID(partition_none, Matrix(NULL));
}


// returns:
// - set of regions that are joined to the region by a WALL
// - set of members to ignore
typedef std::pair<std::set<const directory *>, std::set<const directory *> >
WallsInfo;

HIDDEN WallsInfo
find_walls(const db_i &db, const directory &region_dir, const bn_tol &tol)
{
    RT_CK_DBI(&db);
    RT_CK_DIR(&region_dir);
    BN_CK_TOL(&tol);

    WallsInfo results;
    LeafMap subtracted;
    {
	DBInternal region_db_internal(db, region_dir);
	const rt_comb_internal &region = *static_cast<rt_comb_internal *>
					 (region_db_internal.get().idb_ptr);
	RT_CK_COMB(&region);

	get_subtracted(db, region.tree, subtracted);

	if (subtracted.empty())
	    return results;
    }

    AutoPtr<directory *> region_dirs;
    const std::size_t num_regions = db_ls(&db, DB_LS_REGION, NULL,
					  &region_dirs.ptr);

    for (std::size_t i = 0; i < num_regions; ++i) {
	LeafMap leaves;
	{
	    DBInternal this_region_db_internal(db, *region_dirs.ptr[i]);
	    const rt_comb_internal &region = *static_cast<rt_comb_internal *>
					     (this_region_db_internal.get().idb_ptr);
	    RT_CK_COMB(&region);

	    get_unioned(db, region.tree, leaves);
	    get_intersected(db, region.tree, leaves);
	}

	bool subset = !leaves.empty();

	for (LeafMap::const_iterator leaf_it = leaves.begin(); leaf_it != leaves.end();
	     ++leaf_it) {
	    const LeafMap::const_iterator subtracted_it = subtracted.find(leaf_it->first);

	    if (subtracted_it == subtracted.end()
		|| !leaf_it->second.equal(subtracted_it->second, tol)) {
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

    return results;
}


// stores state for the ongoing conversion process
class FastgenConversion
{
public:
    class RegionManager;

    explicit FastgenConversion(db_i &db, const bn_tol &tol,
			       const std::set<const directory *> &facetize_regions);
    ~FastgenConversion();

    RegionManager &get_region(const directory &region_dir);
    Section &get_section(const db_full_path &path);

    // NULL indicates toplevels
    bool do_force_facetize_region(const directory *region_dir) const;

    void write(const std::string &path) const;

    db_i &m_db;
    const bn_tol m_tol;
    std::set<const directory *> m_failed_regions;


private:
    FastgenConversion(const FastgenConversion &source);
    FastgenConversion &operator=(const FastgenConversion &source);

    const std::set<const directory *> m_facetize_regions;
    std::map<const directory *, RegionManager *> m_regions;
    Section m_toplevels;
};


// organize Section objects by their corresponding region, and store
// additional conversion state regarding those Sections.
class FastgenConversion::RegionManager
{
public:
    explicit RegionManager(const db_i &db, const directory &region_dir,
			   const bn_tol &tol);
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
    bool member_ignored(const db_full_path &member_path) const;

    void create_section(const db_full_path &region_instance_path);
    Section &get_section(const db_full_path &region_instance_path);


private:
    RegionManager(const RegionManager &source);
    RegionManager &operator=(const RegionManager &source);

    const directory &m_region_dir;
    const WallsInfo m_walls;
    bool m_enabled;
    std::pair<bool, fastf_t> m_compsplt;
    std::map<std::string, Section *> m_sections;
};


FastgenConversion::RegionManager::RegionManager(const db_i &db,
	const directory &region_dir, const bn_tol &tol) :
    m_region_dir(region_dir),
    m_walls(find_walls(db, region_dir, tol)),
    m_enabled(true),
    m_compsplt(false, 0.0),
    m_sections()
{
    RT_CK_DBI(&db);
    RT_CK_DIR(&m_region_dir);
    BN_CK_TOL(&tol);
}


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

	    if (it->second->has_color())
		writer.write_section_color(split_id, it->second->get_color());
	}

	it->second->write(writer.get_section_writer(), id, it->first);
	results.push_back(id);

	if (it->second->has_color())
	    writer.write_section_color(id, it->second->get_color());
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
		writer.write_boolean(FastgenWriter::bool_wall, *this_id_it, *target_id_it);
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

    m_compsplt = std::make_pair(true, z_coordinate);
}


bool
FastgenConversion::RegionManager::member_ignored(
    const db_full_path &member_path) const
{
    RT_CK_FULL_PATH(&member_path);

    if (!m_enabled)
	return true;

    for (db_full_path temp = member_path; temp.fp_len; DB_FULL_PATH_POP(&temp))
	if (m_walls.second.count(DB_FULL_PATH_CUR_DIR(&temp)))
	    return true;

    return false;
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
	found.first->second = new Section;
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
	parent_region_dir = &get_region_dir(half_path);
    } catch (const std::invalid_argument &) {
	return false;
    }

    const CompspltID this_compsplt = identify_compsplt(data.m_db,
				     *parent_region_dir, *DB_FULL_PATH_CUR_DIR(&half_path));

    if (this_compsplt.first == partition_none)
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

	    const CompspltID current = identify_compsplt(data.m_db, *region_dirs.ptr[i],
				       *DB_FULL_PATH_CUR_DIR(&half_path));

	    if (current.first != partition_none && current.first != this_compsplt.first)
		if (current.second.equal(this_compsplt.second, data.m_tol)) {
		    other_half_dir = region_dirs.ptr[i];
		    break;
		}
	}
    }

    if (!other_half_dir)
	return false;

    if (this_compsplt.first == partition_intersected)
	data.get_region(*parent_region_dir).set_compsplt(half.eqn[3]);
    else if (this_compsplt.first == partition_subtracted)
	data.get_region(*parent_region_dir).disable();
    else
	throw std::invalid_argument("unknown COMPSPLT partition type");

    return true;
}


FastgenConversion::FastgenConversion(db_i &db, const bn_tol &tol,
				     const std::set<const directory *> &facetize_regions) :
    m_db(db),
    m_tol(tol),
    m_failed_regions(),
    m_facetize_regions(facetize_regions),
    m_regions(),
    m_toplevels()
{
    RT_CK_DBI(&m_db);
    BN_CK_TOL(&m_tol);

    AutoPtr<directory *> region_dirs;
    const std::size_t num_regions = db_ls(&db, DB_LS_REGION, NULL,
					  &region_dirs.ptr);

    // create a RegionManager for all regions in the database
    try {
	for (std::size_t i = 0; i < num_regions; ++i) {
	    const std::pair<const directory *, RegionManager *>
	    temp(region_dirs.ptr[i], new RegionManager(m_db, *region_dirs.ptr[i], m_tol));

	    if (!m_regions.insert(temp).second) {
		delete temp.second;
		throw std::logic_error("RegionManager already exists");
	    }
	}
    } catch (...) {
	for (std::map<const directory *, RegionManager *>::iterator it =
		 m_regions.begin(); it != m_regions.end(); ++it)
	    delete it->second;

	throw;
    }
}


void FastgenConversion::write(const std::string &path) const
{
    FastgenWriter writer(path);
    writer.write_comment(m_db.dbi_title);
    writer.write_comment("libgcv fastgen4 conversion");

    if (!m_toplevels.empty())
	m_toplevels.write(writer.get_section_writer(), writer.take_next_section_id(),
			  "toplevels");

    std::map<const directory *, std::vector<FastgenWriter::SectionID> > ids;

    typedef std::map<std::string, std::map<const directory *, RegionManager *>::const_iterator>
    SortedRegionMap;
    SortedRegionMap sorted_regions;

    for (std::map<const directory *, RegionManager *>::const_iterator it =
	     m_regions.begin(); it != m_regions.end(); ++it)
	sorted_regions[it->first->d_namep] = it;

    for (SortedRegionMap::const_iterator it = sorted_regions.begin();
	 it != sorted_regions.end(); ++it)
	ids[it->second->first] = it->second->second->write(writer);

    for (SortedRegionMap::const_iterator it = sorted_regions.begin();
	 it != sorted_regions.end(); ++it)
	it->second->second->write_walls(writer, ids);
}


FastgenConversion::~FastgenConversion()
{
    for (std::map<const directory *, RegionManager *>::iterator it =
	     m_regions.begin(); it != m_regions.end(); ++it)
	delete it->second;
}


FastgenConversion::RegionManager &
FastgenConversion::get_region(const directory &region_dir)
{
    RT_CK_DIR(&region_dir);

    return *m_regions.at(&region_dir);
}


Section &
FastgenConversion::get_section(const db_full_path &path)
{
    RT_CK_FULL_PATH(&path);

    const directory *region_dir;

    try {
	region_dir = &get_region_dir(path);
    } catch (const std::invalid_argument &) {
	region_dir = NULL;
    }

    if (region_dir)
	return get_region(*region_dir).get_section(get_region_path(path));
    else
	return m_toplevels;

}


bool
FastgenConversion::do_force_facetize_region(const directory *region_dir) const
{
    if (region_dir)
	RT_CK_DIR(region_dir);

    return (m_facetize_regions.count(region_dir) > 0);
}


HIDDEN bool
convert_primitive(FastgenConversion &data, const db_full_path &path,
		  const rt_db_internal &internal, bool subtracted)
{
    RT_CK_FULL_PATH(&path);
    RT_CK_DB_INTERNAL(&internal);

    if (subtracted && path_is_subtracted(data.m_db, get_parent_path(path)))
	return false; // parent is subtracted; not a cutout

    Section &section = data.get_section(path);

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
				       section.m_completed_cutouts))
		return true;

	    section.write_name(DB_FULL_PATH_CUR_DIR(&path)->d_namep);
	    section.write_sphere(ell.v, MAGNITUDE(ell.a), 0.0);
	    break;
	}

	case ID_TGC:
	case ID_REC: {
	    const rt_tgc_internal &tgc = *static_cast<rt_tgc_internal *>(internal.idb_ptr);
	    RT_TGC_CK_MAGIC(&tgc);

	    if (!tgc_is_ccone2(tgc))
		return false;

	    if (path.fp_len > 1
		&& find_ccone_cutout(section, data.m_db, get_parent_path(path),
				     section.m_completed_cutouts))
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
	    section.write_hexahedron(arb.pt, 0.0);
	    break;
	}

	case ID_BOT: {
	    const rt_bot_internal &bot = *static_cast<rt_bot_internal *>(internal.idb_ptr);
	    RT_BOT_CK_MAGIC(&bot);

	    section.write_name(DB_FULL_PATH_CUR_DIR(&path)->d_namep);

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
    Section &section = data.get_section(*path);
    shell *current_shell;

    if (BU_LIST_NON_EMPTY(&nmg_region->s_hd))
	section.write_name("facetized");

    for (BU_LIST_FOR(current_shell, shell, &nmg_region->s_hd)) {
	NMG_CK_SHELL(current_shell);

	rt_bot_internal * const bot = nmg_bot(current_shell, &RTG.rtg_vlfree,
					      &data.m_tol);

	// fill in an rt_db_internal with our new bot so we can free it
	rt_db_internal internal;
	const AutoPtr<rt_db_internal, rt_db_free_internal> autofree_internal(&internal);
	RT_DB_INTERNAL_INIT(&internal);
	internal.idb_major_type = DB5_MAJORTYPE_BRLCAD;
	internal.idb_minor_type = ID_BOT;
	internal.idb_meth = &OBJ[internal.idb_minor_type];
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
    const directory *region_dir;

    try {
	region_dir = &get_region_dir(*path);
    } catch (const std::invalid_argument &) {
	region_dir = NULL;
    }

    const bool facetize = data.do_force_facetize_region(region_dir);
    const bool subtracted = (tree_state->ts_sofar & (TS_SOFAR_MINUS |
			     TS_SOFAR_INTER)) != 0;
    bool converted = false;

    if (data.m_failed_regions.count(region_dir) > 0)
	converted = true;
    else if (region_dir && data.get_region(*region_dir).member_ignored(*path))
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
    Section &section = data.get_section(*path);

    if (tree_state->ts_mater.ma_color_valid)
	section.set_color(color_from_floats(tree_state->ts_mater.ma_color));

    gcv_region_end_data gcv_data = {write_nmg_region, &data};
    return gcv_region_end(tree_state, path, current_tree, &gcv_data);
}


HIDDEN std::set<const directory *>
do_conversion(db_i &db, const struct gcv_opts &gcv_options,
	      const std::string &path,
	      const std::set<const directory *> &facetize_regions =
		  std::set<const directory *>())
{
    RT_CK_DBI(&db);

    AutoPtr<model, nmg_km> nmg_model;
    db_tree_state initial_tree_state = rt_initial_tree_state;
    initial_tree_state.ts_tol = &gcv_options.calculational_tolerance;
    initial_tree_state.ts_ttol = &gcv_options.tessellation_tolerance;;
    initial_tree_state.ts_m = &nmg_model.ptr;

    nmg_model.ptr = nmg_mm();
    FastgenConversion data(db, gcv_options.calculational_tolerance,
			   facetize_regions);
    const int ret = db_walk_tree(&db,
				 static_cast<int>(gcv_options.num_objects),
				 const_cast<const char **>(gcv_options.object_names),
				 1,
				 &initial_tree_state,
				 convert_region_start,
				 convert_region_end, convert_leaf, &data);

    if (ret)
	throw std::runtime_error("db_walk_tree() failed");

    data.write(path);

    return data.m_failed_regions;
}


HIDDEN int
fastgen4_write(struct gcv_context *context, const struct gcv_opts *gcv_options,
	       const void *UNUSED(options_data), const char *dest_path)
{
    try {
	const std::set<const directory *> failed_regions =
	    do_conversion(*context->dbip, *gcv_options, dest_path);

	// facetize all regions that contain incompatible boolean operations
	if (!failed_regions.empty())
	    if (!do_conversion(*context->dbip, *gcv_options, dest_path,
			       failed_regions).empty())
		throw std::runtime_error("failed to convert all regions");
    } catch (const InvalidModelError &exception) {
	std::cerr << "invalid input model ('" << exception.what() << "')\n";
	return 0;
    }

    return 1;
}

}


extern "C"
{
    struct gcv_filter gcv_conv_fastgen4_write =
    {"FASTGEN4 Writer", GCV_FILTER_WRITE, BU_MIME_MODEL_VND_FASTGEN, NULL, NULL, NULL, fastgen4_write::fastgen4_write};
}


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
