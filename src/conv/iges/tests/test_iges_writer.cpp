/* BRL-CAD
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include "common.h"
#include "../iges_writer.h"

#include <stdexcept>
#include <limits>

#include "bu/app.h"
#include "bu/log.h"

using namespace brlcad::iges;

int
main()
{
    try {
	ExportOptions options;
	Writer first(options, {"sphere"});
	Writer second(options, {"sphere"});
	const std::string name = std::string(70, 'N') + ",with;delimiters";
	const auto populate = [&](Writer &writer) {
	    ParameterWriter parameters(158);
	    parameters.real(1.2345678901234567).real(123.45678901234567)
		.real(-123.45678901234567).real(1234.5678901234567);
	    return writer.named_entity(EntitySpec(158), parameters, name);
	};
	const int sphere_id = populate(first);
	if (sphere_id != populate(second))
	    throw std::runtime_error("writers do not own independent sequence numbers");
	File output(bu_temp_file(nullptr, 0));
	if (!output)
	    throw std::runtime_error("cannot create test output");
	first.finish(output.get(), "source.g", "output.igs");
	std::rewind(output.get());
	std::string contents;
	std::array<char, 4096> buffer;
	size_t count;
	while ((count = std::fread(buffer.data(), 1, buffer.size(), output.get())) != 0)
	    contents.append(buffer.data(), count);
	if (std::ferror(output.get()))
	    throw std::runtime_error("cannot read test output");
	std::istringstream physical(contents);
	std::string line;
	while (std::getline(physical, line)) {
	    if (line.size() != 80)
		throw std::runtime_error("writer produced a nonstandard physical record width");
	    if (line[72] != 'P' || std::stoi(line.substr(64, 8)) != sphere_id)
		continue;
	    const std::string payload = line.substr(0, 64);
	    const auto end = payload.find_last_not_of(' ');
	    if (end == std::string::npos || (payload[end] != ',' && payload[end] != ';'))
		throw std::runtime_error("numeric parameter crossed a physical record boundary");
	}
	const Document document = Document::parse_buffer(contents);
	if (!document.valid() || document.find(158).size() != 1 || document.find(406).size() != 1) {
	    for (const auto &diagnostic : document.diagnostics())
		bu_log("%s: %s\n", diagnostic.code.c_str(), diagnostic.message.c_str());
	    throw std::runtime_error("writer did not produce a validated document");
	}
	std::string restored;
	const auto *property = document.parameters(document.find(406).front()->id);
	if (!property || !property->values[2].string(restored) || restored != name)
	    throw std::runtime_error("continued Hollerith metadata changed");
	bool rejected = false;
	try {
	    ParameterWriter invalid(406);
	    invalid.text("invalid\nmetadata");
	} catch (const std::domain_error &) {
	    rejected = true;
	}
	if (!rejected)
	    throw std::runtime_error("physical record delimiter was accepted in metadata");
	for (double value : {std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN()}) {
	    rejected = false;
	    try {
		ParameterWriter invalid(110);
		invalid.real(value);
	    } catch (const std::domain_error &) {
		rejected = true;
	    }
	    if (!rejected)
		throw std::runtime_error("non-finite numeric parameter was accepted");
	}
	return 0;
    } catch (const std::exception &error) {
	bu_log("IGES writer test: %s\n", error.what());
	return 1;
    }
}

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
