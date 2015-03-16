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
#include <stdexcept>

#include "../../plugin.h"


namespace
{


class FastgenWriter
{
public:
    FastgenWriter(const std::string &path);
    ~FastgenWriter();

    void write_comment(const std::string &value);


private:
    friend class Section;

    class Record;

    static const std::size_t MAX_GROUPS = 50;
    static const std::size_t MAX_COMPONENTS = 999;

    static void check_ids(std::size_t group_id, std::size_t component_id);

    std::size_t m_num_sections;
    bool m_record_open;
    std::ofstream m_ostream;
};


class FastgenWriter::Record
{
public:
    Record(FastgenWriter &writer);
    ~Record();

    template <typename T> Record &operator<<(const T &value);
    Record &text(const std::string &value);


private:
    static const std::size_t FIELD_WIDTH = 8;
    static const std::size_t RECORD_WIDTH = 10;

    std::size_t m_width;
    FastgenWriter &m_writer;
};


FastgenWriter::Record::Record(FastgenWriter &writer) :
    m_width(0),
    m_writer(writer)
{
    if (m_writer.m_record_open)
	throw std::logic_error("record open");

    m_writer.m_record_open = true;
}


FastgenWriter::Record::~Record()
{
    if (m_width) m_writer.m_ostream.put('\n');

    m_writer.m_record_open = false;
}


template <typename T> FastgenWriter::Record &
FastgenWriter::Record::operator<<(const T &value)
{
    if (++m_width > RECORD_WIDTH)
	throw std::logic_error("invalid record width");

    m_writer.m_ostream << std::left << std::setw(FIELD_WIDTH) << std::showpoint;
    m_writer.m_ostream << value;
    return *this;
}


FastgenWriter::Record &
FastgenWriter::Record::text(const std::string &value)
{
    m_width = RECORD_WIDTH;
    m_writer.m_ostream << value;
    return *this;
}


void
FastgenWriter::check_ids(std::size_t group_id, std::size_t component_id)
{
    if (group_id + 1 > MAX_GROUPS)
	throw std::invalid_argument("invalid group id");

    if (component_id + 1 > MAX_COMPONENTS)
	throw std::invalid_argument("invalid component id");
}


FastgenWriter::FastgenWriter(const std::string &path) :
    m_num_sections(0),
    m_record_open(false),
    m_ostream(path.c_str(), std::ofstream::out)
{
    m_ostream.exceptions(std::ofstream::failbit | std::ofstream::badbit);
}


FastgenWriter::~FastgenWriter()
{
    Record(*this) << "ENDDATA";
}


void
FastgenWriter::write_comment(const std::string &value)
{
    (Record(*this) << "$COMMENT").text(value);
}


class Section
{
public:
    Section(FastgenWriter &writer, const std::string &name, unsigned work_unit_code,
	    int logistics_control_number, std::size_t group_id, bool volume_mode,
	    unsigned char space_code = 6);

    std::size_t add_grid_point(double x, double y, double z);

    void add_sphere(std::size_t material_id, std::size_t g1, double thickness,
		    double radius);

    void add_line(std::size_t material_id, std::size_t g1, std::size_t g2,
		  double thickness, double radius);

    void add_triangle(std::size_t material_id, std::size_t g1, std::size_t g2,
		      std::size_t g3, double thickness, bool grid_centered = false);

    void add_quad(std::size_t material_id, std::size_t g1, std::size_t g2,
		  std::size_t g3, std::size_t g4, double thickness, bool grid_centered = false);


private:
    static const std::size_t MAX_GRID_POINTS = 50000;
    static const std::size_t MAX_HOLES = 40000;
    static const std::size_t MAX_WALLS = 40000;

    std::size_t m_num_grid_points;
    std::size_t m_num_elements;
    FastgenWriter &m_writer;
};


Section::Section(FastgenWriter &writer, const std::string &name,
		 unsigned work_unit_code, int logistics_control_number, std::size_t group_id,
		 bool volume_mode, unsigned char space_code) :
    m_num_grid_points(0),
    m_num_elements(0),
    m_writer(writer)
{
    FastgenWriter::check_ids(group_id, m_writer.m_num_sections + 1);

    if (work_unit_code < 11 || work_unit_code > 99 || space_code > 6)
	throw std::invalid_argument("invalid value");

    {
	FastgenWriter::Record record(m_writer);
	record << "$NAME" << group_id << m_writer.m_num_sections << work_unit_code <<
	       logistics_control_number;
	record.text(name);
    }

    FastgenWriter::Record record(m_writer);
    record << "SECTION" << group_id << m_writer.m_num_sections <<
	   (volume_mode ? 2 : 1);

    if (space_code != 6)
	record << space_code;

    ++m_writer.m_num_sections;
}


std::size_t
Section::add_grid_point(double x, double y, double z)
{
    if (m_num_grid_points >= MAX_GRID_POINTS)
	throw std::length_error("maximum GRID records");

    FastgenWriter::Record record(m_writer);
    record << "GRID" << m_num_grid_points << "";
    record << x << y << z;
    return m_num_grid_points++;
}


void
Section::add_sphere(std::size_t material_id, std::size_t g1, double thickness,
		    double radius)
{
    if (thickness < 0.0 || radius <= 0.0)
	throw std::invalid_argument("invalid value");

    FastgenWriter::Record(m_writer) << "CSPHERE" << m_num_elements++ << material_id
				    << g1 << thickness << radius;
}


void
Section::add_line(std::size_t material_id, std::size_t g1, std::size_t g2,
		  double thickness, double radius)
{
    if (thickness < 0.0 || radius <= 0.0)
	throw std::invalid_argument("invalid value");

    if (g1 == g2 || g1 >= m_num_grid_points || g2 >= m_num_grid_points)
	throw std::invalid_argument("invalid grid id");

    FastgenWriter::Record(m_writer) << "CLINE" << m_num_elements++ << material_id <<
				    g1 << g2 << thickness << radius;
}


void
Section::add_triangle(std::size_t material_id, std::size_t g1, std::size_t g2,
		      std::size_t g3, double thickness, bool grid_centered)
{
    if (thickness < 0.0)
	throw std::invalid_argument("invalid thickness");

    if (g1 == g2 || g1 == g3 || g2 == g3)
	throw std::invalid_argument("invalid grid id");

    if (g1 >= m_num_grid_points || g2 >= m_num_grid_points
	|| g3 >= m_num_grid_points)
	throw std::invalid_argument("invalid grid id");

    FastgenWriter::Record(m_writer) << "CTRI" << m_num_elements++ << material_id <<
				    g1 << g2 << g3 << thickness << (grid_centered ? 1 : 2);
}


void
Section::add_quad(std::size_t material_id, std::size_t g1, std::size_t g2,
		  std::size_t g3, std::size_t g4, double thickness, bool grid_centered)
{
    if (thickness < 0.0)
	throw std::invalid_argument("invalid thickness");

    if (g1 == g2 || g1 == g3 || g1 == g4 || g2 == g3 || g2 == g4 || g3 == g4)
	throw std::invalid_argument("repeated grid id");

    if (g1 >= m_num_grid_points || g2 >= m_num_grid_points
	|| g3 >= m_num_grid_points || g4 >= m_num_grid_points)
	throw std::invalid_argument("invalid grid id");

    FastgenWriter::Record(m_writer) << "CQUAD" << m_num_elements++ << material_id <<
				    g1 << g2 << g3 << g4 << thickness << (grid_centered ? 1 : 2);
}


HIDDEN tree *
convert_primitive(db_tree_state *tree_state, const db_full_path *path,
		  rt_db_internal *internal, void *client_data)
{
    FastgenWriter &writer = *static_cast<FastgenWriter *>(client_data);

    RT_CK_DBTS(tree_state);

    if (internal->idb_major_type != DB5_MAJORTYPE_BRLCAD)
	return NULL;

    switch (internal->idb_type) {
	case ID_ELL:
	case ID_SPH: {
	    Section section(writer, "sphere1", 11, 0, 0, 0, true);
	    std::size_t g1 = section.add_grid_point(0.0, 0.0, 0.0);
	    section.add_sphere(0, g1, 1.0, 10.0);
	    break;
	}

	default:
	    bu_log("skipping object '%s'\n", db_path_to_string(path));
	    break;
    }

    return NULL;
}


}


extern "C" {


    HIDDEN int
    gcv_fastgen4_write(const char *path, struct db_i *dbip,
		       const struct gcv_opts *UNUSED(options))
    {
	FastgenWriter writer(path);
	writer.write_comment("g -> fastgen4 conversion");

	{
	    directory **results;
	    db_update_nref(dbip, &rt_uniresource);
	    std::size_t num_objects = db_ls(dbip, DB_LS_TOPS, NULL, &results);
	    char **object_names = db_dpv_to_argv(results);
	    bu_free(results, "tops");

	    db_walk_tree(dbip, num_objects, const_cast<const char **>(object_names), 1,
			 &rt_initial_tree_state, NULL, NULL, convert_primitive, &writer);

	    bu_free(object_names, "object_names");
	}


	return 1;
    }


    static const struct gcv_converter converters[] = {
	{"fg4", NULL, gcv_fastgen4_write},
	{NULL, NULL, NULL}
    };


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
