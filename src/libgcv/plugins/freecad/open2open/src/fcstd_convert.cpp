// Copyright (c) 2026, open2open contributors
// SPDX-License-Identifier: MIT
//
// fcstd_convert.cpp — FreeCAD .FCStd archive reader.
//
// Reads Document.xml, GuiDocument.xml, and DiffuseColor binary blobs from a
// FreeCAD .FCStd ZIP archive and returns structured per-object information
// including per-face colours.
//
// Binary DiffuseColor format (confirmed against FreeCAD App/PropertyColor.cpp):
//   uint32 count                  (little-endian, number of faces)
//   uint32 color[count]           (packed 0xRRGGBBAA, stored little-endian)
// FreeCAD alpha=0 means fully opaque (inverted convention vs. standard).
//
// Dependencies: ParseDiffuseColors has no external deps.
//              FCStd archive I/O uses BRL-CAD's local zlib-backed helper.

#include "open2open/fcstd_convert.h"

#include <cstdint>
#include <chrono>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#ifdef OPEN2OPEN_HAVE_LIBZIP
// OCCT LDOM XML parser
#include <LDOMParser.hxx>
#include <LDOM_Document.hxx>
#include <LDOM_Element.hxx>
#include <LDOM_Node.hxx>
#include <LDOM_NodeList.hxx>
#include <LDOMString.hxx>

// OCCT BRep reading (for FCStdFileToONX_Model)
#include <BRep_Builder.hxx>
#include <BRepTools.hxx>
#include <TopoDS_Shape.hxx>

// BRL-CAD local FCStd ZIP archive helper
#include "fcstd_zip.h"

// openNURBS
#include "opennurbs.h"

// open2open B-Rep converter
#include "open2open/brep_convert.h"

#include <map>
#include <sstream>
#endif // OPEN2OPEN_HAVE_LIBZIP

namespace open2open {

using FcstdClock = std::chrono::steady_clock;

static long long ElapsedMs(const FcstdClock::time_point& start,
                           const FcstdClock::time_point& end)
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
}

static bool FcstdTimingEnabled()
{
    const char* val = std::getenv("GCV_FREECAD_TIMING");
    return val && val[0] && val[0] != '0';
}

static bool FcstdImportHidden()
{
    const char* val = std::getenv("GCV_FREECAD_IMPORT_HIDDEN");
    return val && val[0] && val[0] != '0';
}

// ---------------------------------------------------------------------------
// ParseDiffuseColors: decode a binary DiffuseColor blob.
// Format: uint32 count (LE) + count × uint32 (LE, packed 0xRRGGBBAA).
// ---------------------------------------------------------------------------
bool ParseDiffuseColors(const void*              data,
                        std::size_t              size,
                        std::vector<FcstdColor>& out)
{
    out.clear();
    if (!data || size < 4)
        return false;

    const auto* bytes = static_cast<const unsigned char*>(data);

    // Read count as little-endian uint32
    std::uint32_t count =
        (std::uint32_t)bytes[0]        |
        ((std::uint32_t)bytes[1] << 8) |
        ((std::uint32_t)bytes[2] << 16)|
        ((std::uint32_t)bytes[3] << 24);

    if (count == 0)
        return true; // empty list is valid

    // Each entry is 4 bytes; check we have enough data
    if (size < 4 + (std::size_t)count * 4)
        return false;

    out.resize(count);
    for (std::uint32_t i = 0; i < count; ++i) {
        const auto* p = bytes + 4 + i * 4;
        // Little-endian uint32 → packed 0xRRGGBBAA
        std::uint32_t packed =
            (std::uint32_t)p[0]        |
            ((std::uint32_t)p[1] << 8) |
            ((std::uint32_t)p[2] << 16)|
            ((std::uint32_t)p[3] << 24);
        out[i] = static_cast<FcstdColor>(packed);
    }
    return true;
}

#ifdef OPEN2OPEN_HAVE_LIBZIP

// ---------------------------------------------------------------------------
// RAII wrapper around an FCStd ZIP archive handle.
// ---------------------------------------------------------------------------
struct ZipGuard {
    zip_t* z = nullptr;
    explicit ZipGuard(zip_t* p) : z(p) {}
    ~ZipGuard() { if (z) { zip_discard(z); z = nullptr; } }
    ZipGuard(const ZipGuard&)            = delete;
    ZipGuard& operator=(const ZipGuard&) = delete;
};

// ---------------------------------------------------------------------------
// Read one file from the archive into a string.  Returns empty on error.
// ---------------------------------------------------------------------------
static std::string ZipReadEntry(zip_t* z, const char* entry_name)
{
    zip_file_t* f = zip_fopen(z, entry_name, 0);
    if (!f)
        return {};

    zip_stat_t st;
    zip_stat_init(&st);
    if (zip_stat(z, entry_name, 0, &st) != 0 ||
        !(st.valid & ZIP_STAT_SIZE))
    {
        zip_fclose(f);
        return {};
    }

    std::string buf;
    buf.resize(st.size);
    zip_int64_t nr = zip_fread(f, &buf[0], st.size);
    zip_fclose(f);
    if (nr < 0 || (std::size_t)nr != st.size)
        return {};
    return buf;
}

// FreeCAD may serialize datum lines/axes with near-infinite edge parameter
// ranges.  Those appear in BRP text as very long decimal tokens and can make
// libc strtod stall inside BRepTools::Read, so skip them before handing the
// text to OCCT.
static bool BrpHasExtremeNumbers(const std::string& brp_content)
{
    const int kMaxTokenDigits = 30;
    int digits = 0;

    for (char ch : brp_content) {
        if (ch >= '0' && ch <= '9') {
            ++digits;
            if (digits > kMaxTokenDigits)
                return true;
        } else if (ch == '.' || ch == '-' || ch == '+') {
            continue;
        } else {
            digits = 0;
        }
    }

    return false;
}

// ---------------------------------------------------------------------------
// LDOM helper: safe attribute read (returns empty string if not found).
// ---------------------------------------------------------------------------
static std::string LAttr(const LDOM_Element& elem, const char* name)
{
    LDOMString ls = elem.getAttribute(LDOMString(name));
    if (ls == nullptr)
        return {};
    const char* cs = ls.GetString();
    return cs ? std::string(cs) : std::string();
}


static std::string XmlUnescape(std::string value)
{
    struct Replacement {
        const char* from;
        const char* to;
    };
    static const Replacement replacements[] = {
        {"&quot;", "\""},
        {"&apos;", "'"},
        {"&lt;", "<"},
        {"&gt;", ">"},
        {"&amp;", "&"}
    };

    for (const Replacement& r : replacements) {
        std::string::size_type pos = 0;
        while ((pos = value.find(r.from, pos)) != std::string::npos) {
            value.replace(pos, std::strlen(r.from), r.to);
            pos += std::strlen(r.to);
        }
    }

    return value;
}


static std::string XmlTagAttr(const std::string& tag, const char* attr)
{
    std::string needle = std::string(attr) + "=";
    std::string::size_type pos = tag.find(needle);
    if (pos == std::string::npos)
        return {};

    pos += needle.size();
    if (pos >= tag.size() || (tag[pos] != '"' && tag[pos] != '\''))
        return {};

    char quote = tag[pos++];
    std::string::size_type end = tag.find(quote, pos);
    if (end == std::string::npos)
        return {};

    return XmlUnescape(tag.substr(pos, end - pos));
}


static std::string FindXmlPropertyValue(const std::string& block,
                                        const char* prop_name,
                                        const char* child_tag,
                                        const char* child_attr)
{
    std::string prop_needle = std::string("<Property name=\"") + prop_name + "\"";
    std::string::size_type prop_pos = block.find(prop_needle);
    if (prop_pos == std::string::npos)
        return {};

    std::string::size_type prop_end = block.find("</Property>", prop_pos);
    if (prop_end == std::string::npos)
        return {};

    std::string child_needle = std::string("<") + child_tag;
    std::string::size_type child_pos = block.find(child_needle, prop_pos);
    if (child_pos == std::string::npos || child_pos > prop_end)
        return {};

    std::string::size_type child_end = block.find('>', child_pos);
    if (child_end == std::string::npos || child_end > prop_end)
        return {};

    return XmlTagAttr(block.substr(child_pos, child_end - child_pos + 1), child_attr);
}


static int CountBrepObjects(const std::map<std::string, FcstdObject>& objects)
{
    int count = 0;
    for (const auto& kv : objects) {
        if (!kv.second.brp_file.empty())
            ++count;
    }
    return count;
}


static std::map<std::string, FcstdObject>
ParseDocumentXmlTextFallback(const std::string& xml_content, FcstdDocMeta& meta)
{
    std::map<std::string, FcstdObject> result;

    meta.label = FindXmlPropertyValue(xml_content, "Label", "String", "value");
    meta.created_by = FindXmlPropertyValue(xml_content, "CreatedBy", "String", "value");
    meta.creation_date = FindXmlPropertyValue(xml_content, "CreationDate", "String", "value");
    meta.last_modified_date = FindXmlPropertyValue(xml_content, "LastModifiedDate", "String", "value");
    meta.comment = FindXmlPropertyValue(xml_content, "Comment", "String", "value");
    meta.company = FindXmlPropertyValue(xml_content, "Company", "String", "value");

    std::string::size_type objects_pos = xml_content.find("<Objects");
    std::string::size_type objects_end = xml_content.find("</Objects>", objects_pos);
    for (std::string::size_type pos = objects_pos;
         pos != std::string::npos && pos < objects_end;
         pos = xml_content.find("<Object ", pos + 1))
    {
        std::string::size_type tag_end = xml_content.find('>', pos);
        if (tag_end == std::string::npos || tag_end > objects_end)
            break;

        std::string tag = xml_content.substr(pos, tag_end - pos + 1);
        std::string type = XmlTagAttr(tag, "type");
        std::string name = XmlTagAttr(tag, "name");
        if (!type.empty() && !name.empty()) {
            result[name].name = name;
            result[name].type = type;
        }
    }

    std::string::size_type data_pos = xml_content.find("<ObjectData");
    std::string::size_type data_end = xml_content.find("</ObjectData>", data_pos);
    for (std::string::size_type pos = xml_content.find("<Object ", data_pos);
         pos != std::string::npos && pos < data_end;
         pos = xml_content.find("<Object ", pos + 1))
    {
        std::string::size_type tag_end = xml_content.find('>', pos);
        if (tag_end == std::string::npos || tag_end > data_end)
            break;

        std::string tag = xml_content.substr(pos, tag_end - pos + 1);
        std::string name = XmlTagAttr(tag, "name");
        if (name.empty())
            continue;

        std::string::size_type block_end = xml_content.find("</Object>", tag_end);
        if (block_end == std::string::npos || block_end > data_end)
            break;

        std::string block = xml_content.substr(tag_end + 1, block_end - tag_end - 1);
        result[name].name = name;

        std::string label = FindXmlPropertyValue(block, "Label", "String", "value");
        if (!label.empty())
            result[name].label = label;

        std::string brp = FindXmlPropertyValue(block, "Shape", "Part", "file");
        if (!brp.empty())
            result[name].brp_file = brp;
    }

    return result;
}


// ---------------------------------------------------------------------------
// Walk an LDOM subtree looking for <Property name="propName"> and return the
// first child element's attribute `childAttr`.
// ---------------------------------------------------------------------------
static std::string FindPropertyValue(const LDOM_Element& parent,
                                     const char* prop_name,
                                     const char* child_tag,
                                     const char* child_attr)
{
    LDOM_NodeList props = parent.getElementsByTagName(LDOMString("Property"));
    int n = props.getLength();
    for (int i = 0; i < n; ++i) {
        LDOM_Node node = props.item(i);
        if (node.getNodeType() != LDOM_Node::ELEMENT_NODE)
            continue;
        LDOM_Element propElem = (const LDOM_Element&)node;
        if (LAttr(propElem, "name") != prop_name)
            continue;
        // Find child element with tag child_tag
        LDOM_NodeList children = propElem.getElementsByTagName(LDOMString(child_tag));
        if (children.getLength() > 0) {
            LDOM_Node cn = children.item(0);
            if (cn.getNodeType() == LDOM_Node::ELEMENT_NODE) {
                LDOM_Element ce = (const LDOM_Element&)cn;
                return LAttr(ce, child_attr);
            }
        }
        break;
    }
    return {};
}

// ---------------------------------------------------------------------------
// Parse Document.xml: extract per-object name, type, label, brp_file.
// Returns a map from object internal name → FcstdObject (partially filled).
// ---------------------------------------------------------------------------
static std::map<std::string, FcstdObject>
ParseDocumentXml(const std::string& xml_content, FcstdDocMeta& meta)
{
    std::map<std::string, FcstdObject> result;
    if (xml_content.empty())
        return result;

    std::istringstream iss(xml_content);
    LDOMParser parser;
    if (parser.parse(iss))
        return ParseDocumentXmlTextFallback(xml_content, meta); // parse error

    LDOM_Document doc = parser.getDocument();
    LDOM_Element  root = doc.getDocumentElement();
    if (root == nullptr)
        return ParseDocumentXmlTextFallback(xml_content, meta);

    // --- Document-level metadata: look for Properties/Property ---
    {
        meta.label         = FindPropertyValue(root, "Label",            "String",  "value");
        meta.created_by    = FindPropertyValue(root, "CreatedBy",        "String",  "value");
        meta.creation_date = FindPropertyValue(root, "CreationDate",     "String",  "value");
        meta.last_modified_date = FindPropertyValue(root, "LastModifiedDate", "String", "value");
        meta.comment       = FindPropertyValue(root, "Comment",          "String",  "value");
        meta.company       = FindPropertyValue(root, "Company",          "String",  "value");
    }

    // --- Object type list: <Objects><Object type="..." name="..." ...> ---
    {
        LDOM_NodeList objects = root.getElementsByTagName(LDOMString("Object"));
        int n = objects.getLength();
        for (int i = 0; i < n; ++i) {
            LDOM_Node node = objects.item(i);
            if (node.getNodeType() != LDOM_Node::ELEMENT_NODE)
                continue;
            LDOM_Element elem = (const LDOM_Element&)node;
            std::string type = LAttr(elem, "type");
            std::string name = LAttr(elem, "name");
            if (!type.empty() && !name.empty()) {
                // This is the <Object type="Part::Feature" name="..." id="..."/>
                // element in the <Objects> section — record the type.
                result[name].name = name;
                result[name].type = type;
            }
        }
    }

    // --- ObjectData: <Object name="..."><Properties>...</Properties></Object> ---
    // Fills label and brp_file by looking for Property name="Label" and "Shape".
    {
        LDOM_NodeList objects = root.getElementsByTagName(LDOMString("Object"));
        int n = objects.getLength();
        for (int i = 0; i < n; ++i) {
            LDOM_Node node = objects.item(i);
            if (node.getNodeType() != LDOM_Node::ELEMENT_NODE)
                continue;
            LDOM_Element elem = (const LDOM_Element&)node;
            // Only ObjectData/Object elements have Properties children but no type
            if (!LAttr(elem, "type").empty())
                continue;
            std::string name = LAttr(elem, "name");
            if (name.empty())
                continue;

            // Label
            std::string label = FindPropertyValue(elem, "Label", "String", "value");
            if (!label.empty())
                result[name].label = label;

            // Shape → brp file path
            std::string brp = FindPropertyValue(elem, "Shape", "Part", "file");
            if (!brp.empty())
                result[name].brp_file = brp;

            // App::Part membership: look for <Property name="Group" type="...">
            // containing child <LinkList> or <LinkSub> elements with child names.
            // FreeCAD stores the group as: <Property name="Group"><LinkList>
            //   <Link value="Child1"/><Link value="Child2"/>...</LinkList></Property>
            {
                auto it = result.find(name);
                if (it != result.end() && it->second.type == "App::Part") {
                    const std::string& partLabel =
                        it->second.label.empty() ? name : it->second.label;
                    for (LDOM_Node n = elem.getFirstChild();
                         !n.isNull(); n = n.getNextSibling())
                    {
                        if (n.getNodeType() != LDOM_Node::ELEMENT_NODE) continue;
                        LDOM_Element propElem = (const LDOM_Element&)n;
                        if (LAttr(propElem, "name") != "Group") continue;
                        LDOM_NodeList links =
                            propElem.getElementsByTagName(LDOMString("Link"));
                        for (int li = 0; li < links.getLength(); ++li) {
                            LDOM_Node ln = links.item(li);
                            if (ln.getNodeType() != LDOM_Node::ELEMENT_NODE) continue;
                            LDOM_Element le = (const LDOM_Element&)ln;
                            std::string child_name = LAttr(le, "value");
                            if (!child_name.empty())
                                result[child_name].part_label = partLabel;
                        }
                        break;
                    }
                }
            }
        }
    }

    if (CountBrepObjects(result) == 0)
        return ParseDocumentXmlTextFallback(xml_content, meta);

    return result;
}

// ---------------------------------------------------------------------------
// Parse GuiDocument.xml: extract per-object ShapeColor and DiffuseColor refs.
// Updates result (FcstdObject::shape_color, diffuse_file field, visibility).
// Returns a map from internal name → diffuse color file name in archive.
// ---------------------------------------------------------------------------
static void ParseGuiDocumentXml(const std::string& xml_content,
                                 std::map<std::string, FcstdObject>& objects,
                                 std::map<std::string, std::string>& diffuse_files)
{
    if (xml_content.empty())
        return;

    std::istringstream iss(xml_content);
    LDOMParser parser;
    if (parser.parse(iss))
        return;

    LDOM_Document doc = parser.getDocument();
    LDOM_Element  root = doc.getDocumentElement();
    if (root == nullptr)
        return;

    // Each <ViewProvider name="..."> holds per-object GUI properties
    LDOM_NodeList vps = root.getElementsByTagName(LDOMString("ViewProvider"));
    int n = vps.getLength();
    for (int i = 0; i < n; ++i) {
        LDOM_Node node = vps.item(i);
        if (node.getNodeType() != LDOM_Node::ELEMENT_NODE)
            continue;
        LDOM_Element vp = (const LDOM_Element&)node;
        std::string name = LAttr(vp, "name");
        if (name.empty())
            continue;

        // ShapeColor: <Property name="ShapeColor"><PropertyColor value="0xRRGGBBAA"/></Property>
        {
            std::string val = FindPropertyValue(vp, "ShapeColor", "PropertyColor", "value");
            if (!val.empty()) {
                try {
                    unsigned long packed = std::stoul(val, nullptr, 0);
                    objects[name].shape_color = static_cast<FcstdColor>(packed);
                } catch (...) {}
            }
        }

        // Visibility: <Property name="Visibility"><Bool value="true/false"/></Property>
        {
            std::string val = FindPropertyValue(vp, "Visibility", "Bool", "value");
            if (val == "false")
                objects[name].visible = false;
        }

        // DiffuseColor: <Property name="DiffuseColor">
        //                 <ColorList file="DiffuseColor" />
        //               </Property>
        {
            std::string file = FindPropertyValue(vp, "DiffuseColor", "ColorList", "file");
            if (!file.empty())
                diffuse_files[name] = file;
        }
    }
}

// ---------------------------------------------------------------------------
// ReadFcstdDoc: open an FCStd archive and populate a FcstdDoc.
// ---------------------------------------------------------------------------
bool ReadFcstdDoc(const std::string& path, FcstdDoc& doc)
{
    doc = FcstdDoc{};

    int err = 0;
    zip_t* raw_zip = zip_open(path.c_str(), ZIP_RDONLY, &err);
    if (!raw_zip)
        return false;
    ZipGuard guard(raw_zip);

    // Read Document.xml
    std::string doc_xml = ZipReadEntry(raw_zip, "Document.xml");
    if (doc_xml.empty())
        return false; // must have Document.xml

    // Read GuiDocument.xml (optional — may not be present in all FCStd files)
    std::string gui_xml = ZipReadEntry(raw_zip, "GuiDocument.xml");

    // Parse Document.xml → object map + doc meta
    std::map<std::string, FcstdObject> obj_map;
    obj_map = ParseDocumentXml(doc_xml, doc.meta);

    // Parse GuiDocument.xml → fill in shape_color + collect diffuse file refs
    std::map<std::string, std::string> diffuse_file_refs; // obj_name → archive entry
    ParseGuiDocumentXml(gui_xml, obj_map, diffuse_file_refs);

    // For each object with a DiffuseColor reference, read and parse the binary
    for (auto& kv : diffuse_file_refs) {
        const std::string& obj_name  = kv.first;
        const std::string& zip_entry = kv.second;
        if (obj_map.find(obj_name) == obj_map.end())
            continue;

        std::string blob = ZipReadEntry(raw_zip, zip_entry.c_str());
        if (blob.empty())
            continue;

        ParseDiffuseColors(blob.data(), blob.size(),
                           obj_map[obj_name].face_colors);
    }

    // Collect objects that have a BRep file (i.e. have geometric content).
    // Skip origin axes, planes, and other non-geometric objects.
    doc.objects.clear();
    for (auto& kv : obj_map) {
        FcstdObject& obj = kv.second;
        if (obj.brp_file.empty())
            continue;
        doc.objects.push_back(std::move(obj));
    }

    return true;
}

// ---------------------------------------------------------------------------
// FCStdFileToONX_Model — convert an FCStd file → ONX_Model.
// ---------------------------------------------------------------------------
int FCStdFileToONX_Model(const std::string& path,
                         ONX_Model&         model,
                         double             tol)
{
    bool timing = FcstdTimingEnabled();
    bool import_hidden = FcstdImportHidden();
    FcstdClock::time_point total_start = FcstdClock::now();

    FcstdDoc doc;
    if (!ReadFcstdDoc(path, doc)) {
        std::fprintf(stderr, "FCStd conversion summary for %s: ReadFcstdDoc failed\n", path.c_str());
        return 0;
    }
    FcstdClock::time_point doc_done = FcstdClock::now();
    if (timing) {
        std::fprintf(stderr, "FCStd timing: %s document objects=%zu read_ms=%lld\n",
                     path.c_str(), doc.objects.size(), ElapsedMs(total_start, doc_done));
        std::fflush(stderr);
    }

    // Open the archive a second time for B-Rep reading
    int err = 0;
    zip_t* raw_zip = zip_open(path.c_str(), ZIP_RDONLY, &err);
    if (!raw_zip)
        return 0;
    ZipGuard guard(raw_zip);

    int nConverted = 0;
    int nBrpEmpty = 0;
    int nBrpExtreme = 0;
    int nShapeNull = 0;
    int nConversionFailed = 0;
    int nInvalid = 0;
    int nHiddenSkipped = 0;
    int nCacheHits = 0;
    std::map<std::string, std::unique_ptr<ON_Brep>> brep_cache;

    int obj_index = 0;
    for (const FcstdObject& obj : doc.objects) {
        ++obj_index;
        if (obj.brp_file.empty()) continue;
        if (!import_hidden && !obj.visible) {
            ++nHiddenSkipped;
            if (timing) {
                std::fprintf(stderr,
                             "FCStd timing: skip %d/%zu name=\"%s\" type=\"%s\" hidden brp=\"%s\"\n",
                             obj_index, doc.objects.size(), obj.name.c_str(), obj.type.c_str(),
                             obj.brp_file.c_str());
                std::fflush(stderr);
            }
            continue;
        }

        FcstdClock::time_point obj_start = FcstdClock::now();
        if (timing) {
            std::fprintf(stderr,
                         "FCStd timing: begin %d/%zu name=\"%s\" type=\"%s\" visible=%d brp=\"%s\"\n",
                         obj_index, doc.objects.size(), obj.name.c_str(), obj.type.c_str(),
                         obj.visible ? 1 : 0, obj.brp_file.c_str());
            std::fflush(stderr);
        }

        // Read the OCCT BRep text from the archive
        std::string brp_content = ZipReadEntry(raw_zip, obj.brp_file.c_str());
        FcstdClock::time_point zip_done = FcstdClock::now();
        if (brp_content.empty()) {
            ++nBrpEmpty;
            if (timing) {
                std::fprintf(stderr, "FCStd timing: skip %s empty read_ms=%lld\n",
                             obj.name.c_str(), ElapsedMs(obj_start, zip_done));
                std::fflush(stderr);
            }
            continue;
        }
        if (BrpHasExtremeNumbers(brp_content)) {
            ++nBrpExtreme;
            if (timing) {
                std::fprintf(stderr, "FCStd timing: skip %s extreme read_ms=%lld size=%zu\n",
                             obj.name.c_str(), ElapsedMs(obj_start, zip_done), brp_content.size());
                std::fflush(stderr);
            }
            continue;
        }

        ON_Brep* brep = nullptr;
        bool cache_hit = false;
        FcstdClock::time_point read_done = zip_done;
        FcstdClock::time_point convert_done = zip_done;
        FcstdClock::time_point valid_done = zip_done;

        auto cache_it = brep_cache.find(brp_content);
        if (cache_it != brep_cache.end()) {
            brep = ON_Brep::New(*cache_it->second);
            cache_hit = true;
            ++nCacheHits;
        } else {
        // Parse the BRep using OCCT BRep reader
        BRep_Builder builder;
        TopoDS_Shape shape;
        std::istringstream iss(brp_content);
        BRepTools::Read(shape, iss, builder);
        read_done = FcstdClock::now();
        if (shape.IsNull()) {
            ++nShapeNull;
            if (timing) {
                std::fprintf(stderr, "FCStd timing: skip %s null read_ms=%lld brep_read_ms=%lld size=%zu\n",
                             obj.name.c_str(), ElapsedMs(obj_start, zip_done),
                             ElapsedMs(zip_done, read_done), brp_content.size());
                std::fflush(stderr);
            }
            continue;
        }

        // Convert to ON_Brep
        brep = new ON_Brep();
        if (!open2open::OCCTToON_Brep(shape, *brep, tol)) {
            delete brep;
            ++nConversionFailed;
            if (timing) {
                FcstdClock::time_point convert_done = FcstdClock::now();
                std::fprintf(stderr,
                             "FCStd timing: skip %s conversion_failed read_ms=%lld brep_read_ms=%lld convert_ms=%lld size=%zu\n",
                             obj.name.c_str(), ElapsedMs(obj_start, zip_done),
                             ElapsedMs(zip_done, read_done), ElapsedMs(read_done, convert_done),
                             brp_content.size());
                std::fflush(stderr);
            }
            continue;
        }
        convert_done = FcstdClock::now();
        if (!brep->IsValid()) {
            delete brep;
            ++nInvalid;
            if (timing) {
                FcstdClock::time_point valid_done = FcstdClock::now();
                std::fprintf(stderr,
                             "FCStd timing: skip %s invalid read_ms=%lld brep_read_ms=%lld convert_ms=%lld valid_ms=%lld size=%zu\n",
                             obj.name.c_str(), ElapsedMs(obj_start, zip_done),
                             ElapsedMs(zip_done, read_done), ElapsedMs(read_done, convert_done),
                             ElapsedMs(convert_done, valid_done), brp_content.size());
                std::fflush(stderr);
            }
            continue;
        }
        valid_done = FcstdClock::now();
        brep_cache.emplace(brp_content, std::unique_ptr<ON_Brep>(ON_Brep::New(*brep)));
        }

        // Build attributes
        ON_3dmObjectAttributes* attrs = new ON_3dmObjectAttributes();
        // Name
        if (!obj.label.empty())
            attrs->m_name = ON_wString(obj.label.c_str());
        else if (!obj.name.empty())
            attrs->m_name = ON_wString(obj.name.c_str());

        // Overall shape colour
        {
            float r, g, b, a;
            FcstdColorToRGBAf(obj.shape_color, r, g, b, a);
            attrs->m_color = ON_Color(
                (int)(r * 255), (int)(g * 255), (int)(b * 255),
                (int)((1.0f - a) * 255));
            attrs->SetColorSource(ON::color_from_object);
        }

        // Layer name → use App::Part label if available, else type-based group
        {
            // If the object belongs to an App::Part container, use its label
            // as the layer name; otherwise fall back to type-based grouping.
            std::string layerName;
            if (!obj.part_label.empty()) {
                layerName = obj.part_label;
            } else if (!obj.type.empty()) {
                layerName =
                    (obj.type.find("Part::") != std::string::npos) ? "Part"
                  : (obj.type.find("PartDesign::") != std::string::npos) ? "PartDesign"
                  : "Other";
            }

            if (!layerName.empty()) {
                ON_wString onLayerName(layerName.c_str());
                int layerIdx = -1;
                ONX_ModelComponentIterator lit(model,
                    ON_ModelComponent::Type::Layer);
                for (const ON_ModelComponent* mc = lit.FirstComponent();
                     mc != nullptr; mc = lit.NextComponent())
                {
                    const ON_Layer* l = ON_Layer::Cast(mc);
                    if (l && l->Name() == onLayerName) {
                        layerIdx = l->Index();
                        break;
                    }
                }
                if (layerIdx < 0)
                    layerIdx = model.AddLayer(
                        static_cast<const wchar_t*>(onLayerName),
                        ON_Color::UnsetColor);
                attrs->m_layer_index = layerIdx;
            }
        }

        model.AddManagedModelGeometryComponent(brep, attrs);
        ++nConverted;
        FcstdClock::time_point add_done = FcstdClock::now();
        if (timing) {
            if (cache_hit) {
                std::fprintf(stderr,
                             "FCStd timing: cached %s read_ms=%lld clone_add_ms=%lld size=%zu faces=%d\n",
                             obj.name.c_str(), ElapsedMs(obj_start, zip_done),
                             ElapsedMs(zip_done, add_done), brp_content.size(), brep->m_F.Count());
            } else {
                std::fprintf(stderr,
                             "FCStd timing: done %s read_ms=%lld brep_read_ms=%lld convert_ms=%lld valid_ms=%lld add_ms=%lld size=%zu faces=%d\n",
                             obj.name.c_str(), ElapsedMs(obj_start, zip_done),
                             ElapsedMs(zip_done, read_done), ElapsedMs(read_done, convert_done),
                             ElapsedMs(convert_done, valid_done), ElapsedMs(valid_done, add_done),
                             brp_content.size(), brep->m_F.Count());
            }
            std::fflush(stderr);
        }
    }

    if (timing) {
        FcstdClock::time_point total_done = FcstdClock::now();
        std::fprintf(stderr,
                     "FCStd timing: total converted=%d objects=%zu total_ms=%lld hidden=%d cached=%d empty=%d extreme=%d null_shapes=%d conversion_failed=%d invalid=%d\n",
                     nConverted, doc.objects.size(), ElapsedMs(total_start, total_done), nHiddenSkipped,
                     nCacheHits, nBrpEmpty, nBrpExtreme, nShapeNull, nConversionFailed, nInvalid);
        std::fflush(stderr);
    }

    if (nConverted == 0 && !doc.objects.empty()) {
        std::fprintf(stderr,
                     "FCStd conversion summary for %s: objects=%zu hidden=%d empty=%d extreme=%d null_shapes=%d conversion_failed=%d invalid=%d\n",
                     path.c_str(), doc.objects.size(), nHiddenSkipped, nBrpEmpty,
                     nBrpExtreme, nShapeNull, nConversionFailed, nInvalid);
    }

    return nConverted;
}

// ---------------------------------------------------------------------------
// ONX_ModelToFCStdFile — convert ONX_Model → FreeCAD .FCStd archive.
// ---------------------------------------------------------------------------

/// Add a string blob to the ZIP archive under @p name.
/// Returns true on success.
static bool ZipAddStringEntry(zip_t*             z,
                              const std::string& name,
                              const std::string& content)
{
    zip_source_t* src = zip_source_buffer(z,
                                          content.data(),
                                          content.size(),
                                          0 /*freep*/);
    if (!src) return false;
    zip_int64_t idx = zip_file_add(z, name.c_str(), src, ZIP_FL_OVERWRITE);
    if (idx < 0) {
        zip_source_free(src);
        return false;
    }
    return true;
}

int ONX_ModelToFCStdFile(const std::string& path,
                         const ONX_Model&   model,
                         double             tol)
{
    // Collect geometry objects
    struct ShapeEntry {
        std::string          name;
        std::string          label;
        std::string          brp_file;        ///< e.g. "PartShape.brp"
        std::string          diffuse_file;    ///< e.g. "DiffuseColor" (may be empty)
        ON_Color             color;
        std::vector<FcstdColor> face_colors;  ///< per-face colours (FreeCAD convention)
    };
    std::vector<ShapeEntry> entries;
    std::vector<std::string> brp_contents;

    {
        ONX_ModelComponentIterator it(model,
            ON_ModelComponent::Type::ModelGeometry);
        int shapeIdx = 0;
        for (const ON_ModelComponent* mc = it.FirstComponent();
             mc != nullptr; mc = it.NextComponent())
        {
            const ON_ModelGeometryComponent* mgc =
                ON_ModelGeometryComponent::Cast(mc);
            if (!mgc) continue;
            const ON_Geometry* geom = mgc->Geometry(nullptr);
            if (!geom) continue;

            // Only handle B-Rep / extrusion objects
            const ON_Brep* brep = nullptr;
            ON_Brep* tempBrep = nullptr;
            if (geom->ObjectType() == ON::brep_object) {
                brep = ON_Brep::Cast(geom);
            } else if (geom->ObjectType() == ON::extrusion_object) {
                const ON_Extrusion* ext = ON_Extrusion::Cast(geom);
                if (ext) { tempBrep = ext->BrepForm(); brep = tempBrep; }
            }
            if (!brep) continue;

            // Convert to OCCT
            TopoDS_Shape shape = open2open::ON_BrepToOCCT(*brep, tol);
            if (tempBrep) { delete tempBrep; tempBrep = nullptr; }
            if (shape.IsNull()) continue;

            // Serialize to BRep text
            std::ostringstream oss;
            BRepTools::Write(shape, oss);
            std::string brp_content = oss.str();
            if (brp_content.empty()) continue;

            // Build entry name
            std::string internalName;
            std::string label;
            const ON_3dmObjectAttributes* attrs = mgc->Attributes(nullptr);
            if (attrs && attrs->m_name.IsNotEmpty()) {
                ON_String utf8;
                utf8 = attrs->m_name;
                label = utf8.Array();
            } else {
                label = "Shape" + std::to_string(shapeIdx);
            }
            // FreeCAD internal names: e.g. "Body", "Pad", etc.
            internalName = "Shape_" + std::to_string(shapeIdx);

            std::string brpFile = (shapeIdx == 0) ? "PartShape.brp"
                : "PartShape" + std::to_string(shapeIdx) + ".brp";

            ON_Color color =
                (attrs && attrs->ColorSource() == ON::color_from_object)
                    ? attrs->m_color
                    : ON_Color(200, 200, 200);

            // Per-face colours from ON_BrepFace::PerFaceColor()
            std::vector<FcstdColor> faceColors;
            if (brep) {
                const int nf = brep->m_F.Count();
                bool hasPerFace = false;
                for (int fi = 0; fi < nf; ++fi) {
                    if (brep->m_F[fi].PerFaceColor() != ON_Color::UnsetColor) {
                        hasPerFace = true;
                        break;
                    }
                }
                if (hasPerFace) {
                    faceColors.resize(static_cast<size_t>(nf));
                    for (int fi = 0; fi < nf; ++fi) {
                        ON_Color fc = brep->m_F[fi].PerFaceColor();
                        if (fc == ON_Color::UnsetColor) fc = color;
                        // Pack as 0xRRGGBBAA (FreeCAD: alpha=0=opaque)
                        std::uint32_t packed =
                            ((std::uint32_t)fc.Red()   << 24) |
                            ((std::uint32_t)fc.Green() << 16) |
                            ((std::uint32_t)fc.Blue()  <<  8) |
                            0u; // alpha=0 = opaque in FreeCAD
                        faceColors[static_cast<size_t>(fi)] = packed;
                    }
                }
            }

            std::string diffuseFile;
            if (!faceColors.empty()) {
                diffuseFile = (shapeIdx == 0) ? "DiffuseColor"
                    : "DiffuseColor" + std::to_string(shapeIdx);
            }

            entries.push_back({internalName, label, brpFile, diffuseFile,
                                color, std::move(faceColors)});
            brp_contents.push_back(std::move(brp_content));
            ++shapeIdx;
        }
    }

    if (entries.empty()) return 0;

    // Build Document.xml
    std::ostringstream docXml;
    docXml << "<?xml version='1.0' encoding='utf-8'?>\n";
    docXml << "<!--\n FreeCAD Document format v0.1\n Generated by open2open\n-->\n";
    docXml << "<Document SchemaVersion=\"4\" ProgramVersion=\"0.21\" FileVersion=\"1\">\n";
    docXml << "  <Properties Count=\"0\"/>\n";
    docXml << "  <Objects Count=\"" << entries.size() << "\">\n";
    for (const auto& e : entries) {
        docXml << "    <Object type=\"Part::Feature\" name=\"" << e.name
               << "\" id=\"" << e.name << "\"/>\n";
    }
    docXml << "  </Objects>\n";
    docXml << "  <ObjectData Count=\"" << entries.size() << "\">\n";
    for (const auto& e : entries) {
        docXml << "    <Object name=\"" << e.name << "\">\n";
        docXml << "      <Properties Count=\"2\">\n";
        docXml << "        <Property name=\"Label\" type=\"App::PropertyString\">\n";
        docXml << "          <String value=\"" << e.label << "\"/>\n";
        docXml << "        </Property>\n";
        docXml << "        <Property name=\"Shape\" type=\"Part::PropertyPartShape\">\n";
        docXml << "          <Part file=\"" << e.brp_file << "\"/>\n";
        docXml << "        </Property>\n";
        docXml << "      </Properties>\n";
        docXml << "    </Object>\n";
    }
    docXml << "  </ObjectData>\n";
    docXml << "</Document>\n";

    // Build GuiDocument.xml
    std::ostringstream guiXml;
    guiXml << "<?xml version='1.0' encoding='utf-8'?>\n";
    guiXml << "<Document SchemaVersion=\"1\">\n";
    guiXml << "  <ViewProviderData Count=\"" << entries.size() << "\">\n";
    for (size_t i = 0; i < entries.size(); ++i) {
        const auto& e = entries[i];
        // Pack colour as 0xRRGGBBAA hex string (FreeCAD convention; alpha=0 = opaque)
        char hexColor[16];
        std::snprintf(hexColor, sizeof(hexColor), "0x%02X%02X%02X00",
                      e.color.Red(), e.color.Green(), e.color.Blue());
        const int propCount = e.diffuse_file.empty() ? 1 : 2;
        guiXml << "    <ViewProvider name=\"" << e.name << "\">\n";
        guiXml << "      <Properties Count=\"" << propCount << "\">\n";
        guiXml << "        <Property name=\"ShapeColor\" type=\"App::PropertyColor\">\n";
        guiXml << "          <PropertyColor value=\"" << hexColor << "\"/>\n";
        guiXml << "        </Property>\n";
        if (!e.diffuse_file.empty()) {
            guiXml << "        <Property name=\"DiffuseColor\""
                      " type=\"App::PropertyColorList\">\n";
            guiXml << "          <ColorList file=\"" << e.diffuse_file << "\"/>\n";
            guiXml << "        </Property>\n";
        }
        guiXml << "      </Properties>\n";
        guiXml << "    </ViewProvider>\n";
    }
    guiXml << "  </ViewProviderData>\n";
    guiXml << "</Document>\n";

    // Create the ZIP archive
    int err = 0;
    zip_t* raw_zip = zip_open(path.c_str(),
                              ZIP_CREATE | ZIP_TRUNCATE, &err);
    if (!raw_zip) return 0;

    // Keep string buffers alive until zip_close
    std::string docXmlStr  = docXml.str();
    std::string guiXmlStr  = guiXml.str();

    int nWritten = 0;
    bool ok = ZipAddStringEntry(raw_zip, "Document.xml",    docXmlStr)
           && ZipAddStringEntry(raw_zip, "GuiDocument.xml", guiXmlStr);
    if (ok) {
        for (size_t i = 0; i < entries.size(); ++i) {
            if (ZipAddStringEntry(raw_zip, entries[i].brp_file, brp_contents[i]))
                ++nWritten;
            // Per-face DiffuseColor binary blob
            if (!entries[i].diffuse_file.empty() &&
                !entries[i].face_colors.empty())
            {
                const auto& fc = entries[i].face_colors;
                // Write: uint32 count (LE) + count * uint32 (LE, 0xRRGGBBAA)
                std::uint32_t count = static_cast<std::uint32_t>(fc.size());
                std::string blob;
                blob.resize(4 + count * 4);
                auto* p = reinterpret_cast<unsigned char*>(&blob[0]);
                p[0] = count & 0xFF;
                p[1] = (count >> 8) & 0xFF;
                p[2] = (count >> 16) & 0xFF;
                p[3] = (count >> 24) & 0xFF;
                p += 4;
                for (std::uint32_t ci = 0; ci < count; ++ci) {
                    std::uint32_t v = fc[ci];
                    p[0] = v & 0xFF;
                    p[1] = (v >> 8) & 0xFF;
                    p[2] = (v >> 16) & 0xFF;
                    p[3] = (v >> 24) & 0xFF;
                    p += 4;
                }
                ZipAddStringEntry(raw_zip, entries[i].diffuse_file, blob);
            }
        }
    }

    if (zip_close(raw_zip) < 0) {
        zip_discard(raw_zip);
        return 0;
    }

    return nWritten;
}

#else // !OPEN2OPEN_HAVE_LIBZIP

bool ReadFcstdDoc(const std::string& /*path*/, FcstdDoc& /*doc*/)
{
    return false; // FCStd archive support not compiled
}

#endif // OPEN2OPEN_HAVE_LIBZIP

} // namespace open2open
