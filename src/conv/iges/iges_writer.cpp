/* BRL-CAD
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include "common.h"
#include "iges_writer.h"

#include <algorithm>
#include <cmath>
#include <ctime>
#include <filesystem>
#include <iomanip>
#include <limits>
#include <locale>
#include <stdexcept>

#include "bu/app.h"
#include "bu/file.h"
#include "bu/log.h"
#include "rt/geom.h"
#include "brlcad_version.h"

namespace brlcad {
namespace iges {
namespace {
constexpr size_t DATA_COLUMNS = 72;
constexpr size_t PARAMETER_COLUMNS = 64;
constexpr size_t FIELD_COLUMNS = 8;
constexpr int MAX_SEQUENCE = 9999999;
constexpr int DEPENDENT_GEOMETRY = 10001;
constexpr int DEFINITION_STATUS = 10201;
constexpr int PROPERTY_STATUS = 10301;
constexpr size_t COPY_BUFFER_SIZE = 16384;

void
write_bytes(FILE *output, const std::string_view &bytes)
{
    if (std::fwrite(bytes.data(), 1, bytes.size(), output) != bytes.size())
	throw std::runtime_error("cannot write IGES output");
}

std::string
field(int64_t value, bool zero_fill = false)
{
    std::ostringstream text;
    text.imbue(std::locale::classic());
    text << std::setfill(zero_fill ? '0' : ' ') << std::setw(FIELD_COLUMNS) << value;
    const std::string result = text.str();
    if (result.size() != FIELD_COLUMNS)
	throw std::length_error("IGES directory value exceeds its eight-column field");
    return result;
}

void
record(FILE *output, std::string data, char section, int sequence, int owner = 0)
{
    const size_t width = section == 'P' ? PARAMETER_COLUMNS : DATA_COLUMNS;
    if (data.size() > width || sequence <= 0 || sequence > MAX_SEQUENCE)
	throw std::length_error("IGES physical record limit exceeded");
    data.resize(width, ' ');
    if (section == 'P')
	data += field(owner);
    std::ostringstream suffix;
    suffix << section << std::setfill('0') << std::setw(7) << sequence << '\n';
    data += suffix.str();
    write_bytes(output, data);
}

int
records(FILE *output, const std::string &data, char section, int &sequence, int owner = 0)
{
    const size_t width = section == 'P' ? PARAMETER_COLUMNS : DATA_COLUMNS;
    const int initial_sequence = sequence;
    std::string line;
    const auto flush = [&]() {
	if (sequence == MAX_SEQUENCE)
	    throw std::length_error("IGES section exceeds the sequence-number limit");
	record(output, line, section, ++sequence, owner);
	line.clear();
    };
    for (size_t offset = 0; offset < data.size();) {
	if (section == 'S') {
	    line = data.substr(offset, width);
	    offset += line.size();
	    flush();
	    continue;
	}
	size_t header_end = offset;
	while (header_end < data.size() && data[header_end] >= '0' && data[header_end] <= '9')
	    ++header_end;
	const bool string = header_end > offset && header_end < data.size() && data[header_end] == 'H';
	size_t end;
	if (string) {
	    const size_t length = std::stoull(data.substr(offset, header_end - offset));
	    ++header_end;
	    if (length >= data.size() - header_end)
		throw std::logic_error("unterminated generated Hollerith field");
	    end = header_end + length + 1;
	} else {
	    end = data.find_first_of(",;", offset);
	    if (end == std::string::npos)
		throw std::logic_error("unterminated generated numeric field");
	    ++end;
	}
	// IGES 5.3 section 2.2.2 allows only strings to cross a record
	// boundary, and keeps the entire count-plus-H prefix on one line.
	const size_t indivisible = (string ? header_end : end) - offset;
	if (indivisible > width)
	    throw std::length_error("IGES field exceeds the physical record width");
	if (line.size() + indivisible > width)
	    flush();
	while (offset < end) {
	    const size_t count = std::min(end - offset, width - line.size());
	    line.append(data, offset, count);
	    offset += count;
	    if (line.size() == width)
		flush();
	}
    }
    if (!line.empty())
	flush();
    return sequence - initial_sequence;
}

void
copy_section(FILE *input, FILE *output)
{
    if (std::fflush(input) || bu_fseek(input, 0, SEEK_SET))
	throw std::runtime_error("cannot rewind IGES scratch section");
    std::array<char, COPY_BUFFER_SIZE> buffer;
    size_t count;
    while ((count = std::fread(buffer.data(), 1, buffer.size(), input)) != 0)
	write_bytes(output, std::string_view(buffer.data(), count));
    if (std::ferror(input))
	throw std::runtime_error("cannot read IGES scratch section");
}

EntitySpec
topology_specification(int type, int form = 1)
{
    EntitySpec specification(type);
    specification.form = form;
    specification.status = DEPENDENT_GEOMETRY;
    return specification;
}
} // namespace

ParameterWriter::ParameterWriter(int type)
{
    stream_.imbue(std::locale::classic());
    stream_ << std::setprecision(std::numeric_limits<double>::max_digits10) << type;
}

ParameterWriter &
ParameterWriter::integer(int64_t value)
{
    stream_ << ',' << value;
    return *this;
}

ParameterWriter &
ParameterWriter::real(double value)
{
    if (!std::isfinite(value))
	throw std::domain_error("non-finite IGES numeric parameter");
    stream_ << ',' << value;
    return *this;
}

ParameterWriter &
ParameterWriter::text(const std::string &value)
{
    if (value.find_first_of("\r\n") != std::string::npos || value.find('\0') != std::string::npos)
	throw std::domain_error("IGES text contains a physical record delimiter");
    stream_ << ',';
    if (!value.empty())
	stream_ << value.size() << 'H' << value;
    return *this;
}

ParameterWriter &
ParameterWriter::empty()
{
    stream_ << ',';
    return *this;
}

ParameterWriter &
ParameterWriter::point(const fastf_t *value)
{
    return real(value[0]).real(value[1]).real(value[2]);
}

ParameterWriter &
ParameterWriter::reals(const double *values, size_t count)
{
    for (size_t index = 0; index < count; ++index)
	real(values[index]);
    return *this;
}

std::string
ParameterWriter::finish() const
{
    return stream_.str() + ';';
}

Writer::Writer(const ExportOptions &options, std::set<std::string> roots) :
    options_(options), roots_(std::move(roots)),
    directories_(bu_temp_file(nullptr, 0)),
    parameters_(bu_temp_file(nullptr, 0))
{
    if (!directories_ || !parameters_)
	throw std::runtime_error("cannot create IGES scratch sections");
}

int
Writer::entity(const EntitySpec &specification, const ParameterWriter &parameters)
{
    if (directory_sequence_ > MAX_SEQUENCE - 2)
	throw std::length_error("IGES directory section is full");
    const int id = directory_sequence_ + 1;
    const int parameter_start = parameter_sequence_ + 1;
    const int count = records(parameters_.get(), parameters.finish(), 'P', parameter_sequence_, id);
    const std::string first = field(specification.type) + field(parameter_start) +
	field(specification.structure) + field(0) + field(0) + field(0) +
	field(specification.transform) + field(0) + field(specification.status, true);
    const std::string second = field(specification.type) + field(0) + field(specification.color) +
	field(count) + field(specification.form) + field(0) + field(0) +
	std::string(FIELD_COLUMNS, ' ') + field(0);
    record(directories_.get(), first, 'D', ++directory_sequence_);
    record(directories_.get(), second, 'D', ++directory_sequence_);
    ++type_counts_[specification.type];
    return id;
}

int
Writer::name_property(const std::string &name)
{
    if (name.empty())
	return 0;
    ParameterWriter parameters(406);
    parameters.integer(1).text(name);
    EntitySpec specification(406);
    specification.form = 15;
    specification.status = PROPERTY_STATUS;
    return entity(specification, parameters);
}

int
Writer::attribute_definition()
{
    if (attribute_definition_)
	return attribute_definition_;
    ParameterWriter parameters(322);
    parameters.text("BRL-CAD region and material properties").integer(5001).integer(9);
    constexpr int ATTRIBUTE_TYPES[] = {3, 3, 6, 1, 1, 1, 1, 1, 6};
    for (size_t index = 0; index < std::size(ATTRIBUTE_TYPES); ++index)
	parameters.integer(index + 1).integer(ATTRIBUTE_TYPES[index]).integer(1);
    EntitySpec specification(322);
    specification.status = DEFINITION_STATUS;
    attribute_definition_ = entity(specification, parameters);
    return attribute_definition_;
}

int
Writer::attributes(const struct rt_comb_internal &combination)
{
    const std::string shader = bu_vls_cstr(&combination.shader);
    const auto separator = shader.find(' ');
    ParameterWriter parameters(422);
    parameters.text(shader.substr(0, separator)).text(separator == std::string::npos ? "" : shader.substr(separator + 1));
    parameters.integer(combination.region_flag != 0).integer(combination.region_id)
	.integer(combination.aircode).integer(combination.GIFTmater).integer(combination.los)
	.integer(combination.inherit != 0).integer(combination.rgb_valid != 0);
    EntitySpec specification(422);
    specification.structure = -attribute_definition();
    specification.status = PROPERTY_STATUS;
    return entity(specification, parameters);
}

int
Writer::color(const unsigned char rgb[3])
{
    ParameterWriter parameters(314);
    constexpr double BYTE_TO_PERCENT = 100.0 / 255.0;
    for (size_t channel = 0; channel < 3; ++channel)
	parameters.real(rgb[channel] * BYTE_TO_PERCENT);
    EntitySpec specification(314);
    specification.status = DEFINITION_STATUS;
    return -entity(specification, parameters);
}

int
Writer::named_entity(EntitySpec specification, ParameterWriter &parameters,
    const std::string &name, const struct rt_comb_internal *properties)
{
    const int name_id = name_property(name);
    const int attributes_id = properties ? attributes(*properties) : 0;
    if (properties && properties->rgb_valid)
	specification.color = color(properties->rgb);
    if (name_id || attributes_id) {
	parameters.integer(0).integer((name_id != 0) + (attributes_id != 0));
	if (attributes_id)
	    parameters.integer(attributes_id);
	if (name_id)
	    parameters.integer(name_id);
    }
    specification.status = roots_.count(name) ? 1 : DEPENDENT_GEOMETRY;
    return entity(specification, parameters);
}

int
Writer::transform(const mat_t matrix)
{
    ParameterWriter parameters(124);
    if (ZERO(matrix[15]))
	throw std::domain_error("singular combination transform");
    for (size_t row = 0; row < 3; ++row)
	for (size_t column = 0; column < 4; ++column)
	    parameters.real(matrix[4 * row + column] / matrix[15]);
    return entity(EntitySpec(124), parameters);
}

int
Writer::instance(ExportedEntity definition, const mat_t matrix)
{
    if (!definition)
	return 0;
    ParameterWriter parameters(430);
    parameters.integer(definition.directory);
    EntitySpec specification = topology_specification(430, definition.brep ? 1 : 0);
    specification.transform = transform(matrix);
    return entity(specification, parameters);
}

int
Writer::assembly(const std::string &name, const std::vector<int> &members)
{
    if (members.empty() || std::any_of(members.begin(), members.end(), [](int id) { return id <= 0; }))
	return 0;
    ParameterWriter parameters(184);
    parameters.integer(members.size());
    for (int member : members)
	parameters.integer(member);
    for (size_t index = 0; index < members.size(); ++index)
	parameters.integer(0);
    EntitySpec specification(184);
    specification.form = 1;
    return named_entity(specification, parameters, name);
}

void
Writer::omission(const std::string &message)
{
    ++omissions_;
    bu_log("g-iges: %s\n", message.c_str());
}

int
Writer::group(const std::string &name, const std::vector<int> &members)
{
    if (members.empty())
	return 0;
    ParameterWriter parameters(402);
    parameters.integer(members.size());
    for (int member : members)
	parameters.integer(member);
    EntitySpec specification(402);
    specification.form = 7;
    return named_entity(specification, parameters, name);
}

void
Writer::finish(FILE *output, const std::string &source, const std::string &destination)
{
    int start_sequence = 0;
    records(output, "BRL-CAD IGES export", 'S', start_sequence);
    // ParameterWriter's leading integer is discarded for the Global section,
    // which starts with default parameter and record delimiters instead.
    ParameterWriter global(0);
    global.empty().text(std::filesystem::path(source).filename().string())
	.text(destination.empty() ? "stdout" : std::filesystem::path(destination).filename().string())
	.text(brlcad_version()).text("BRL-CAD").integer(32).integer(38).integer(6).integer(308).integer(15)
	.text("g-iges").real(1.0).integer(2).text("MM").empty().real(1.0);
    const std::time_t now = std::time(nullptr);
    const std::tm *calendar = std::gmtime(&now);
    if (!calendar)
	throw std::runtime_error("cannot generate IGES timestamp");
    std::ostringstream timestamp;
    timestamp << std::put_time(calendar, "%Y%m%d.%H%M%S");
    global.text(timestamp.str()).real(options_.tolerance.dist).real(0.0)
	.text("Unknown").text("Unknown").integer(11).integer(0).text(timestamp.str());
    int global_sequence = 0;
    records(output, global.finish().substr(1), 'G', global_sequence);
    copy_section(directories_.get(), output);
    copy_section(parameters_.get(), output);
    std::ostringstream counts;
    counts << 'S' << std::setw(7) << start_sequence << 'G' << std::setw(7) << global_sequence
	<< 'D' << std::setw(7) << directory_sequence_ << 'P' << std::setw(7) << parameter_sequence_;
    record(output, counts.str(), 'T', 1);
    if (std::fflush(output) || std::ferror(output))
	throw std::runtime_error("cannot flush IGES output");
}

void
Writer::print_statistics() const
{
    size_t total = 0;
    for (const auto &count : type_counts_) {
	if (options_.verbose)
	    bu_log("IGES: %zu entities of type %d\n", count.second, count.first);
	total += count.second;
    }
    bu_log("IGES: wrote %zu entities\n", total);
}

int
Writer::nurbs_surface(int k1, int k2, int m1, int m2, int rational,
    int closed_u, int closed_v, int periodic_u, int periodic_v,
    const double *u_knots, const double *v_knots, const double *weights,
    const double *controls, double u0, double u1, double v0, double v1)
{
    if (k1 < m1 || k2 < m2 || m1 < 1 || m2 < 1)
	return 0;
    ParameterWriter parameters(128);
    parameters.integer(k1).integer(k2).integer(m1).integer(m2)
	.integer(closed_u).integer(closed_v).integer(!rational).integer(periodic_u).integer(periodic_v);
    const size_t count = (static_cast<size_t>(k1) + 1) * (static_cast<size_t>(k2) + 1);
    parameters.reals(u_knots, static_cast<size_t>(k1) + m1 + 2)
	.reals(v_knots, static_cast<size_t>(k2) + m2 + 2).reals(weights, count)
	.reals(controls, 3 * count).real(u0).real(u1).real(v0).real(v1);
    return entity(topology_specification(128, 0), parameters);
}

int
Writer::nurbs_curve(int k, int m, int rational, int planar, int closed, int periodic,
    const double *knots, const double *weights, const double *controls,
    double v0, double v1, double nx, double ny, double nz)
{
    if (k < m || m < 1)
	return 0;
    ParameterWriter parameters(126);
    parameters.integer(k).integer(m).integer(planar).integer(closed).integer(!rational).integer(periodic)
	.reals(knots, static_cast<size_t>(k) + m + 2).reals(weights, static_cast<size_t>(k) + 1)
	.reals(controls, 3 * (static_cast<size_t>(k) + 1)).real(v0).real(v1).real(nx).real(ny).real(nz);
    return entity(topology_specification(126, 0), parameters);
}

int
Writer::composite_curve(const int *members, int count)
{
    if (count < 1)
	return 0;
    ParameterWriter parameters(102);
    parameters.integer(count);
    for (int index = 0; index < count; ++index)
	parameters.integer(members[index]);
    return entity(topology_specification(102, 0), parameters);
}

int
Writer::curve_on_surface(int surface, int parameter_curve, int model_curve)
{
    ParameterWriter parameters(142);
    // A singular boundary has no complete model-space curve.  Advertise
    // the parameter-space boundary alone, never UV coordinates as XYZ.
    const int preference = parameter_curve ? (model_curve ? 3 : 1) : 2;
    parameters.integer(0).integer(surface).integer(parameter_curve).integer(model_curve).integer(preference);
    return entity(topology_specification(142, 0), parameters);
}

int
Writer::trimmed_surface(int surface, int outer, const int *inner, int count)
{
    ParameterWriter parameters(144);
    parameters.integer(surface).integer(1).integer(count).integer(outer);
    for (int index = 0; index < count; ++index)
	parameters.integer(inner[index]);
    return entity(topology_specification(144, 0), parameters);
}

int
Writer::vertex_list(const double *vertices, size_t count)
{
    ParameterWriter parameters(502);
    parameters.integer(count).reals(vertices, 3 * count);
    return count ? entity(topology_specification(502), parameters) : 0;
}

int
Writer::edge_list(int vertices, const BrepEdge *edges, size_t count)
{
    ParameterWriter parameters(504);
    parameters.integer(count);
    for (size_t index = 0; index < count; ++index)
	parameters.integer(edges[index].curve_de).integer(vertices).integer(edges[index].start_vertex + 1)
	    .integer(vertices).integer(edges[index].end_vertex + 1);
    return vertices && count ? entity(topology_specification(504), parameters) : 0;
}

int
Writer::loop(int vertices, int edges, const BrepLoopUse *uses, size_t count)
{
    ParameterWriter parameters(508);
    parameters.integer(count);
    for (size_t index = 0; index < count; ++index) {
	const auto &use = uses[index];
	parameters.integer(use.kind).integer(use.kind == BrepLoopUse::Vertex ? vertices : edges)
	    .integer(use.index + 1).integer(use.orientation != 0).integer(use.parameter_curve_de > 0);
	if (use.parameter_curve_de > 0)
	    parameters.integer(use.isoparametric != 0).integer(use.parameter_curve_de);
    }
    return vertices && count ? entity(topology_specification(508), parameters) : 0;
}

int
Writer::face(int surface, const int *loops, size_t count, int has_outer)
{
    ParameterWriter parameters(510);
    parameters.integer(surface).integer(count).integer(has_outer != 0);
    for (size_t index = 0; index < count; ++index)
	parameters.integer(loops[index]);
    return surface && count ? entity(topology_specification(510), parameters) : 0;
}

int
Writer::shell(const int *faces, const int *orientations, size_t count)
{
    ParameterWriter parameters(514);
    parameters.integer(count);
    for (size_t index = 0; index < count; ++index)
	parameters.integer(faces[index]).integer(orientations[index] != 0);
    return count ? entity(topology_specification(514), parameters) : 0;
}

int
Writer::solid(const char *name, int outer_shell, int outer_orientation,
    const int *void_shells, const int *void_orientations, size_t count)
{
    if (!outer_shell)
	return 0;
    ParameterWriter parameters(186);
    parameters.integer(outer_shell).integer(outer_orientation != 0).integer(count);
    for (size_t index = 0; index < count; ++index)
	parameters.integer(void_shells[index]).integer(void_orientations[index] != 0);
    EntitySpec specification(186);
    return named_entity(specification, parameters, name ? name : "");
}

} // namespace iges
} // namespace brlcad

/*
 * Local Variables:
 * mode: C++
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
