/*                 F C S T D _ D O C U M E N T . C P P
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 */

#include "common.h"

#include "fcstd_document.h"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <exception>
#include <limits>
#include <locale>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string_view>

#ifdef GCV_FREECAD_MINIZIP
#  include <unzip.h>
#else
#  include <archive.h>
#  include <archive_entry.h>
#endif
#include <expat.h>
#include <gp_Quaternion.hxx>
#include <gp_Trsf.hxx>

namespace freecad {

#ifdef GCV_FREECAD_MINIZIP
struct Archive::Impl {
    explicit Impl(const char *path) : file(unzOpen64(path))
    {
	if (!file)
	    throw std::runtime_error("could not open FCStd ZIP archive");
    }

    ~Impl()
    {
	if (open)
	    unzCloseCurrentFile(file);
	unzClose(file);
    }

    unzFile file;
    bool positioned = false;
    bool open = false;
    bool finished = false;
};

Archive::Archive(const char *path) : impl(new Impl(path))
{
}

Archive::~Archive() = default;

bool
Archive::next(std::string &name)
{
    if (impl->open) {
	impl->open = false;
	if (unzCloseCurrentFile(impl->file) != UNZ_OK)
	    throw std::runtime_error("could not close FCStd ZIP member");
    }
    // Traversing the central directory avoids reading the local header of
    // every member while looking for the document or a selected shape.
    const int status = impl->positioned ? unzGoToNextFile(impl->file) : unzGoToFirstFile(impl->file);
    impl->positioned = true;
    impl->finished = false;
    if (status == UNZ_END_OF_LIST_OF_FILE)
	return false;
    unz_file_info64 info = {};
    if (status != UNZ_OK ||
	    unzGetCurrentFileInfo64(impl->file, &info, nullptr, 0, nullptr, 0, nullptr, 0) != UNZ_OK)
	throw std::runtime_error("could not read FCStd ZIP directory");
    if (!info.size_filename)
	throw std::runtime_error("FCStd archive member has no name");
    name.resize(info.size_filename + 1);
    if (unzGetCurrentFileInfo64(impl->file, nullptr, name.data(), name.size(),
	    nullptr, 0, nullptr, 0) != UNZ_OK)
	throw std::runtime_error("could not read FCStd ZIP member name");
    name.resize(info.size_filename);
    return true;
}

std::ptrdiff_t
Archive::read(void *buffer, size_t size)
{
    if (impl->finished || !size)
	return 0;
    if (!impl->open) {
	if (unzOpenCurrentFile(impl->file) != UNZ_OK)
	    return -1;
	impl->open = true;
    }
    const size_t maximum_read = std::numeric_limits<int>::max();
    const int count = unzReadCurrentFile(impl->file, buffer,
	static_cast<unsigned int>(std::min(size, maximum_read)));
    if (count != 0)
	return count;
    impl->open = false;
    impl->finished = true;
    return unzCloseCurrentFile(impl->file) == UNZ_OK ? 0 : -1;
}
#else
struct Archive::Impl {
    struct archive *reader = nullptr;
};

static std::runtime_error
archive_error(struct archive *input)
{
    const char *message = archive_error_string(input);
    return std::runtime_error(message ? message : "could not read FCStd archive");
}

Archive::Archive(const char *path) : impl(new Impl)
{
    constexpr size_t archive_block_size = 64 * 1024;
    auto *archive = archive_read_new();
    if (!archive)
	throw std::runtime_error("could not allocate FCStd archive reader");
    if (archive_read_support_format_zip(archive) != ARCHIVE_OK ||
	    archive_read_open_filename(archive, path, archive_block_size) != ARCHIVE_OK) {
	const auto error = archive_error(archive);
	archive_read_free(archive);
	throw error;
    }
    impl->reader = archive;
}

Archive::~Archive()
{
    archive_read_free(impl->reader);
}

bool
Archive::next(std::string &name)
{
    struct archive_entry *entry = nullptr;
    const int status = archive_read_next_header(impl->reader, &entry);
    if (status == ARCHIVE_EOF)
	return false;
    if (status != ARCHIVE_OK)
	throw archive_error(impl->reader);
    const char *pathname = archive_entry_pathname(entry);
    if (!pathname)
	throw std::runtime_error("FCStd archive member has no name");
    name = pathname;
    return true;
}

std::ptrdiff_t
Archive::read(void *buffer, size_t size)
{
    return archive_read_data(impl->reader, buffer, size);
}
#endif

Matrix
placement_matrix(const gp_Trsf &transform)
{
    Matrix matrix = MAT_INIT_IDN;
    constexpr int affine_rows = 3;
    constexpr int affine_columns = 4;
    for (int row = 0; row < affine_rows; ++row) {
	for (int column = 0; column < affine_columns; ++column) {
	    const double value = transform.Value(row + 1, column + 1);
	    if (!std::isfinite(value))
		throw std::runtime_error("invalid FreeCAD placement matrix");
	    matrix[row * affine_columns + column] = value;
	}
    }
    return matrix;
}

using XmlAttributes = const XML_Char *const *;

static std::string
attribute(XmlAttributes attributes, std::string_view key)
{
    for (; *attributes; attributes += 2) {
	if (key == attributes[0])
	    return attributes[1];
    }
    return {};
}

static double
xml_number(const std::string &text, double absent)
{
    if (text.empty())
	return absent;
    std::istringstream in(text);
    in.imbue(std::locale::classic());
    double result = 0.0;
    if (!(in >> result) || !std::isfinite(result) || !(in >> std::ws).eof())
	throw std::runtime_error("invalid numeric FCStd property");
    return result;
}

static double
number_attribute(XmlAttributes attributes, const char *key, double absent = 0.0)
{
    return xml_number(attribute(attributes, key), absent);
}

static Matrix
read_placement(XmlAttributes attributes)
{
    const auto w = attribute(attributes, "Q3");
    gp_Quaternion rotation;
    if (!w.empty()) {
	rotation.Set(number_attribute(attributes, "Q0"), number_attribute(attributes, "Q1"),
	    number_attribute(attributes, "Q2"), xml_number(w, 1.0));
    } else {
	const double angle = number_attribute(attributes, "A");
	if (!ZERO(angle))
	    rotation.SetVectorAndAngle(gp_Vec(number_attribute(attributes, "Ox"),
		number_attribute(attributes, "Oy"), number_attribute(attributes, "Oz", 1.0)), angle);
    }
    return placement_matrix(rotation, {number_attribute(attributes, "Px"),
	number_attribute(attributes, "Py"), number_attribute(attributes, "Pz")});
}

Matrix
placement_matrix(const gp_Quaternion &rotation, const std::array<double, 3> &position)
{
    if (ZERO(rotation.SquareNorm()))
	throw std::runtime_error("invalid FreeCAD placement quaternion");
    gp_Trsf transform;
    transform.SetRotation(rotation);
    transform.SetTranslationPart(gp_Vec(position[0], position[1], position[2]));
    return placement_matrix(transform);
}

static size_t
read_count(const std::string &value)
{
    unsigned int count = 0;
    const auto end = value.data() + value.size();
    const auto converted = std::from_chars(value.data(), end, count);
    if (converted.ec != std::errc() || converted.ptr != end)
	throw std::runtime_error("invalid FreeCAD link count");
    return count;
}

static bool
xml_bool(const std::string &value)
{
    return value == "true" || value == "1";
}

static bool
is_scalar(std::string_view element)
{
    return element == "String" || element == "Bool" ||
	element == "Integer" || element == "Unsigned" ||
	element == "Float" || element == "StringHasher";
}

static std::optional<Rgb>
packed_color(XmlAttributes attributes, const char *key)
{
    std::istringstream in(attribute(attributes, key));
    in.imbue(std::locale::classic());
    uint32_t rgba = 0;
    if (!(in >> rgba) || !(in >> std::ws).eof())
	return std::nullopt;
    return unpack_rgb(rgba);
}

Rgb
unpack_rgb(uint32_t rgba)
{
    return {static_cast<unsigned char>(rgba >> 24),
	static_cast<unsigned char>(rgba >> 16), static_cast<unsigned char>(rgba >> 8)};
}

static void
read_property(Object &object, XmlAttributes attributes, const std::string &property,
	std::string_view element, bool view)
{
    if (property == "Visibility" && element == "Bool") {
	if (view || !object.has_view_visibility)
	    object.visible = xml_bool(attribute(attributes, "value"));
	object.has_view_visibility |= view;
    } else if (view && property == "ShapeColor" && element == "Color") {
	object.color = packed_color(attributes, "value");
    } else if (view && property == "ShapeMaterial" && element == "PropertyMaterial") {
	object.material_color = packed_color(attributes, "diffuseColor");
	const auto value = attribute(attributes, "transparency");
	if (!value.empty())
	    object.material_transparency = std::clamp(xml_number(value, 0.0), 0.0, 1.0);
    } else if (view && property == "Transparency" && element == "Integer") {
	object.transparency = std::clamp(number_attribute(attributes, "value") / 100.0, 0.0, 1.0);
    } else if (view && property == "ShapeAppearance" && element == "MaterialList") {
	object.appearance_file = attribute(attributes, "file");
    } else if (view && property == "DiffuseColor" && element == "ColorList") {
	object.diffuse_file = attribute(attributes, "file");
    } else if (view && property == "OverrideMaterial" && element == "Bool") {
	object.override_material = xml_bool(attribute(attributes, "value"));
    } else if (!view) {
	if (property == "Shape" && element == "Part")
	    object.shape_file = attribute(attributes, "file");
	else if (property == "Mesh" && element == "Mesh")
	    object.mesh_file = attribute(attributes, "file");
	else if (property == "Placement" && element == "PropertyPlacement")
	    object.placement = read_placement(attributes);
	else if (property == "LinkPlacement" && element == "PropertyPlacement") {
	    object.link_placement = read_placement(attributes);
	    object.has_link_placement = true;
	} else if (property == "LinkedObject" && (element == "XLink" || element == "LinkSub")) {
	    object.linked_object = attribute(attributes, element == "XLink" ? "name" : "value");
	    if (element == "XLink")
		object.linked_file = attribute(attributes, "file");
	    const auto count = attribute(attributes, "count");
	    if (!count.empty())
		object.linked_subobject_count = read_count(count);
	    auto sub = attribute(attributes, "sub");
	    if (!sub.empty())
		object.linked_subobjects.push_back(std::move(sub));
	} else if (property == "LinkedObject" && element == "Link")
	    object.linked_object = attribute(attributes, "value");
	else if (property == "LinkedObject")
	    object.unsupported_link = true;
	else if (property == "LinkTransform" && element == "Bool")
	    object.link_transform = xml_bool(attribute(attributes, "value"));
	else if (property == "VisibilityList" && element == "BoolList")
	    object.element_visibility = attribute(attributes, "value");
	else if (property == "ElementCount" && element == "Integer" && object.is_link())
	    object.element_count = read_count(attribute(attributes, "value"));
	else if (property == "PlacementList" && element == "PlacementList" && object.is_link())
	    object.array_placements = attribute(attributes, "file");
	else if (property == "ScaleList" && element == "VectorList" && object.is_link())
	    object.array_scales = attribute(attributes, "file");
	else if (property == "Suppressed" && element == "Bool" && object.is_link_element())
	    object.suppressed = xml_bool(attribute(attributes, "value"));
	else if (property == "Scale" && element == "Float")
	    object.scale = number_attribute(attributes, "value", 1.0);
	else if (property == "ScaleVector" && element == "PropertyVector") {
	    object.has_scale_vector = true;
	    object.scale_vector = {number_attribute(attributes, "valueX", 1.0),
		number_attribute(attributes, "valueY", 1.0),
		number_attribute(attributes, "valueZ", 1.0)};
	}
	if (is_scalar(element))
	    object.properties[property] = attribute(attributes, "value");
    }
}

// The separator must be forbidden in XML so it cannot appear in a namespace URI.
constexpr XML_Char xml_namespace_separator = '\x1f';

struct XmlContext {
    Document &document;
    XML_Parser parser;
    bool view;
    bool version_only;
    size_t depth = 0;
    std::string section = {};
    Object *object = nullptr;
    std::string property = {};
    std::exception_ptr error = {};
    bool saw_doctype = false;
};

static void
read_element(XmlContext &context, std::string_view element, XmlAttributes attributes)
{
    const auto separator = element.find(xml_namespace_separator);
    if (separator != std::string_view::npos)
	element.remove_prefix(separator + 1);

    const size_t depth = context.depth++;
    const bool view = context.view;
    auto &document = context.document;
    auto &section = context.section;
    auto &object = context.object;
    auto &property = context.property;
    if (depth == 0) {
	if (element != "Document" && !(view && element == "GuiDocument"))
	    throw std::runtime_error("unexpected root element in FCStd XML");
	if (!view) {
	    std::istringstream version(attribute(attributes, "ProgramVersion"));
	    int major = 0;
	    int minor = 0;
	    char dot = 0;
	    if (version >> major >> dot >> minor && dot == '.')
		document.legacy_alpha = major < 1 || (major == 1 && minor < 1);
	}
    } else if (context.version_only) {
	return;
    } else if (depth == 1) {
	section = element;
	object = nullptr;
    } else if (!view && section == "Properties") {
	if (depth == 2)
	    property = element == "Property" ? attribute(attributes, "name") : "";
	else if (depth == 3 && !property.empty() && is_scalar(element))
	    document.properties[property] = attribute(attributes, "value");
    } else if (depth == 2) {
	object = nullptr;
	const std::string name = attribute(attributes, "name");
	if (name.empty())
	    return;
	if (!view && section == "Objects" && element == "Object") {
	    Object &definition = document.objects[name];
	    if (!definition.type.empty())
		throw std::runtime_error("duplicate object in FCStd document");
	    definition.type = attribute(attributes, "type");
	} else if ((!view && section == "ObjectData" && element == "Object") ||
		(view && section == "ViewProviderData" && element == "ViewProvider"))
	    object = &document.objects[name];
    } else if (object) {
	if (depth == 4)
	    property = element == "Property" ? attribute(attributes, "name") : "";
	else if (depth == 5 && !property.empty())
	    read_property(*object, attributes, property, element, view);
	else if (!view && depth == 6 && element == "Link" &&
		property == (object->is_link_group() ? "ElementList" : "Group")) {
	    const std::string child = attribute(attributes, "value");
	    // Empty link group slots still occupy an index in VisibilityList.
	    if (!child.empty() || property == "ElementList")
		object->children.push_back(child);
	} else if (!view && depth == 6 && element == "Link" && property == "ElementList" &&
		object->is_link()) {
	    object->array_elements.push_back(attribute(attributes, "value"));
	} else if (!view && depth == 6 && property == "LinkedObject") {
	    if (element == "Sub")
		object->linked_subobjects.push_back(attribute(attributes, "value"));
	    else
		object->unsupported_link = true;
	}
    }
}

static void XMLCALL
start_element(void *data, const XML_Char *element, const XML_Char **attributes) noexcept
{
    auto &context = *static_cast<XmlContext *>(data);
    try {
	read_element(context, element, attributes);
    } catch (...) {
	// Report C++ errors after control returns from Expat.
	context.error = std::current_exception();
	XML_StopParser(context.parser, XML_FALSE);
    }
}

static void XMLCALL
end_element(void *data, const XML_Char *) noexcept
{
    --static_cast<XmlContext *>(data)->depth;
}

static void XMLCALL
start_doctype(void *data, const XML_Char *, const XML_Char *, const XML_Char *, int) noexcept
{
    auto &context = *static_cast<XmlContext *>(data);
    context.saw_doctype = true;
    XML_StopParser(context.parser, XML_FALSE);
}

static void
read_document_xml(Document &document, Archive &archive, bool view, bool version_only = false)
{
    std::unique_ptr<XML_ParserStruct, decltype(&XML_ParserFree)> parser(
	XML_ParserCreateNS(nullptr, xml_namespace_separator), XML_ParserFree);
    if (!parser)
	throw std::runtime_error("could not open FCStd XML parser");

    XmlContext context{document, parser.get(), view, version_only};
    XML_SetUserData(parser.get(), &context);
    XML_SetElementHandler(parser.get(), start_element, end_element);
    XML_SetStartDoctypeDeclHandler(parser.get(), start_doctype);

    constexpr int chunk_size = 64 * 1024;
    for (;;) {
	void *buffer = XML_GetBuffer(parser.get(), chunk_size);
	if (!buffer)
	    throw std::runtime_error("could not allocate FCStd XML input buffer");
	const auto count = archive.read(buffer, chunk_size);
	if (count < 0)
	    throw std::runtime_error("could not read FCStd XML member");
	if (XML_ParseBuffer(parser.get(), static_cast<int>(count), count == 0) != XML_STATUS_OK) {
	    if (context.error)
		std::rethrow_exception(context.error);
	    if (context.saw_doctype)
		throw std::runtime_error("unexpected document type in FCStd XML");
	    const char *detail = XML_ErrorString(XML_GetErrorCode(parser.get()));
	    throw std::runtime_error("malformed FCStd XML at line " +
		std::to_string(XML_GetCurrentLineNumber(parser.get())) + ": " + detail);
	}
	if (count == 0)
	    return;
    }
}

Document
read_document(const char *path, const char *view_path)
{
    Archive input(path);
    Document document;
    bool found_document = false;
    bool found_view = false;
    std::string name;
    while (input.next(name)) {
	if (name == "Document.xml") {
	    if (found_document)
		throw std::runtime_error("duplicate FCStd Document.xml member");
	    read_document_xml(document, input, false);
	    found_document = true;
	} else if (!view_path && name == "GuiDocument.xml") {
	    if (found_view)
		throw std::runtime_error("duplicate FCStd GuiDocument.xml member");
	    read_document_xml(document, input, true);
	    found_view = true;
	}
    }
    if (!found_document)
	throw std::runtime_error("FCStd archive has no Document.xml");
    if (view_path) {
	// Headless FreeCAD saves geometry without the view providers. Read
	// appearance and visibility from the original archive.
	Archive view(view_path);
	while (view.next(name)) {
	    if (name == "Document.xml")
		read_document_xml(document, view, false, true);
	    else if (name == "GuiDocument.xml")
		read_document_xml(document, view, true);
	}
    }
    return document;
}

}

// Local Variables:
// mode: C++
// tab-width: 8
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
