/* M I M E _ O B J . H
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 * See the file named COPYING for more information.
 */
#ifndef BU_MIME_OBJ_H
#define BU_MIME_OBJ_H
#include "common.h"
#include <algorithm>
#include <cmath>
#include <istream>
#include <locale>
#include <sstream>
#include <string>
#include "bu/mime.h"
#include "mime_unicode.h"

namespace mime_obj {
inline constexpr const char *type = "model/obj";

/* Keep reference presence distinct from opening or interpreting an MTL
 * file. Only a completed record establishes a reference in a prefix. */
class material_references {
    enum phase {indent, keyword, payload, ignored} state = indent;
    std::string word;
    bool pending = false;
public:
    bool present = false;
    void feed(int c)
    {
	if (c == '\r' || c == '\n') {
	    present = present || pending;
	    pending = false; word.clear(); state = indent;
	    return;
	}
	if (state == ignored) return;
	if (state == indent) {
	    if (mime_unicode::text_source::space(c)) return;
	    state = keyword;
	}
	if (state == keyword) {
	    if (!mime_unicode::text_source::space(c)) {
		if (word.size() >= sizeof("mtllib") - 1 || c >= 0x80) state = ignored;
		else word += static_cast<char>(c);
		return;
	    }
	    state = word == "mtllib" ? payload : ignored;
	}
	if (state == payload && !mime_unicode::text_source::space(c)) {
	    pending = c != '#';
	    state = ignored;
	}
    }
};

struct profile {
    bool identified = false, incomplete = false, bom = false;
    bool repertoire_known = false, ascii_only = false;
    bool references_known = false, references = false;
    std::string encoding, width;
    void observe_text(mime_unicode::text_source &source, bool whole)
    {
	width = source.width();
	bom = source.marked();
	if (bom) encoding = source.encoding();
	if (whole) {
	    encoding = source.finish_encoding();
	    repertoire_known = !encoding.empty();
	    ascii_only = source.ascii;
	}
    }
    bool append_traits(bu_mime_traits &traits) const
    {
	return bu_mime_traits_add(&traits, "obj.serialization", "text") &&
	    bu_mime_traits_add(&traits, "text.code-unit-width", width.c_str()) &&
	    bu_mime_traits_add(&traits, "obj.text-bom", bom ? "true" : "false") &&
	    (encoding.empty() || bu_mime_traits_add(&traits, "encoding", encoding.c_str())) &&
	    (!repertoire_known || bu_mime_traits_add(&traits, "text.ascii-only", ascii_only ? "true" : "false")) &&
	    (!references_known || bu_mime_traits_add(&traits, "obj.material-library-reference", references ? "true" : "false"));
    }
};

/* Decoders establish the format independently of the conservative
 * recognition grammar. Scan original bytes only for requested whole-file
 * properties, without loading sidecars or buffering long comment lines. */
inline profile observe(mime_unicode::text_source &source, bool charset, bool materials)
{
    profile result;
    material_references references;
    if (charset || materials) {
	for (int c = source.get(); c >= 0; c = source.get())
	    if (materials) references.feed(c);
	if (materials) {
	    references.feed('\n');
	    result.references_known = references.present || source.valid();
	    result.references = references.present;
	}
    }
    result.observe_text(source, charset || materials);
    return result;
}

inline bool numbers(const std::string &text, size_t minimum)
{
    std::istringstream values(text);
    values.imbue(std::locale::classic());
    size_t count = 0;
    for (;;) {
	values >> std::ws;
	if (values.eof()) return count >= minimum;
	double value;
	if (!(values >> value) || !std::isfinite(value)) return false;
	if (values.peek() >= 0 && !mime_unicode::text_source::space(values.peek())) return false;
	++count;
    }
}

inline bool indices(const std::string &text, size_t minimum)
{
    std::istringstream values(text);
    values.imbue(std::locale::classic());
    size_t count = 0;
    for (;;) {
	values >> std::ws;
	if (values.eof()) return count >= minimum;
	size_t fields = 0;
	for (;;) {
	    long index;
	    if (!(values >> index) || !index) return false;
	    ++fields;
	    if (values.peek() != '/') break;
	    if (fields >= 3) return false;
	    values.get();
	    if (values.peek() == '/' && fields == 1) {++fields; values.get();}
	    /* Formatted extraction otherwise skips a missing field's blanks. */
	    if (mime_unicode::text_source::space(values.peek())) return false;
	}
	if (values.peek() >= 0 && !mime_unicode::text_source::space(values.peek())) return false;
	++count;
    }
}

inline bool known_record(const std::string &word, bool prefix = false)
{
    for (const char *name : {"v", "vn", "vt", "vp", "f", "l", "p", "o", "g", "s", "usemtl", "mtllib", "bevel", "c_interp", "d_interp",
	"lod", "shadow_obj", "trace_obj", "ctech", "stech", "cstype", "deg", "bmat", "step",
	"curv", "curv2", "surf", "parm", "trim", "hole", "scrv", "sp", "end", "con", "mg"})
	if (word == name || (prefix && std::string(name).compare(0, word.size(), word) == 0)) return true;
    return false;
}

/* OBJ has no magic number. Metadata alone is insufficient: require
 * plausible vertex records and indexed geometry, with only OBJ records
 * in the available text. Full decoders still validate the model. */
inline profile inspect(std::istream &stream, bool whole)
{
    profile result;
    mime_unicode::text_source source(stream);
    material_references references;
    size_t vertices = 0, needed_vertices = 0;
    bool geometry = false;
    const bool byte_text = source.encoding() == "utf-8";
    while (source.peek() >= 0) {
	std::string line;
	bool ended = false;
	for (int c = source.get(); c >= 0; c = source.get()) {
	    references.feed(c);
	    if (c == '\r' || c == '\n') {ended = true; break;}
	    if (!c) return {};
	    if (byte_text) line += static_cast<char>(c);
	    else mime_unicode::characters::append_scalar(line, static_cast<uint32_t>(c));
	}
	if (!ended && !whole) {
	    std::istringstream partial(line);
	    partial.imbue(std::locale::classic());
	    std::string word;
	    partial >> word;
	    if (!word.empty() && word[0] != '#' && !known_record(word, true)) return {};
	    result.incomplete = true;
	    break;
	}
	const auto comment = line.find('#');
	if (comment != std::string::npos) line.resize(comment);
	std::istringstream record(line);
	record.imbue(std::locale::classic());
	std::string word;
	if (!(record >> word)) continue;
	std::string data;
	std::getline(record, data);
	if (word == "v") {
	    constexpr size_t coordinates = 3;
	    if (!numbers(data, coordinates)) return {};
	    ++vertices;
	} else if (word == "vn" || word == "vt" || word == "vp") {
	    if (!numbers(data, word == "vn" ? 3 : 1)) return {};
	} else if (word == "f" || word == "l" || word == "p") {
	    const size_t minimum = word == "f" ? 3 : word == "l" ? 2 : 1;
	    if (!indices(data, minimum)) return {};
	    geometry = true;
	    needed_vertices = std::max(needed_vertices, minimum);
	} else if (!known_record(word)) return {};
    }
    if (whole) references.feed('\n');
    result.identified = geometry && vertices >= needed_vertices;
    result.incomplete = result.incomplete || (!whole && !result.identified);
    result.references_known = (whole && source.valid()) || references.present;
    result.references = references.present;
    result.observe_text(source, whole);
    return result;
}
}
#endif
