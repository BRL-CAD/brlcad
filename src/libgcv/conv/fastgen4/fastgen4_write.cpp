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
    void write_name(std::size_t group_id, std::size_t component_id,
		    std::size_t work_unit_code, int logistics_control_number,
		    const std::string &name);

private:
    class Record;

    bool m_record_open;
    bool m_section_written;
    std::ofstream m_ostream;
};


class FastgenWriter::Record
{
public:
    Record(FastgenWriter &writer);
    ~Record();

    template <typename T> Record &field(const T &value);

private:
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
FastgenWriter::Record::field(const T &value)
{
    const std::size_t FIELD_WIDTH = 8;
    const std::size_t RECORD_WIDTH = 10;

    if (++m_width > RECORD_WIDTH)
	throw std::logic_error("invalid record width");

    m_writer.m_ostream << std::left << std::setw(FIELD_WIDTH) << value;
    return *this;
}


FastgenWriter::FastgenWriter(const std::string &path) :
    m_record_open(false),
    m_section_written(false),
    m_ostream(path.c_str(), std::ofstream::out)
{
    m_ostream.exceptions(std::ofstream::failbit | std::ofstream::badbit);
}


FastgenWriter::~FastgenWriter()
{
    Record(*this).field("ENDDATA");
}


void
FastgenWriter::write_comment(const std::string &value)
{
    if (m_record_open)
	throw std::logic_error("record open");

    m_ostream << "$COMMENT" << value << '\n';
}


void
FastgenWriter::write_name(std::size_t group_id, std::size_t component_id,
			  std::size_t work_unit_code, int logistics_control_number,
			  const std::string &name)
{
    if (group_id > 9)
	throw std::invalid_argument("invalid group id");

    if (component_id > 999)
	throw std::invalid_argument("invalid component id");

    if (work_unit_code < 11 || work_unit_code > 99)
	throw std::invalid_argument("invalid work unit code");

    Record record(*this);
    record.field("$NAME").field(group_id).field(component_id).field(
	work_unit_code).field(logistics_control_number).field(name);
}


}


extern "C" {


    HIDDEN int
    gcv_fastgen4_write(const char *path, struct db_i *UNUSED(dbip),
		       const struct gcv_opts *UNUSED(options))
    {
	FastgenWriter writer(path);
	writer.write_comment("test comment");

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
