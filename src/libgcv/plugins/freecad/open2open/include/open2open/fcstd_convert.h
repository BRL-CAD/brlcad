// Copyright (c) 2026, open2open contributors
// SPDX-License-Identifier: MIT
//
// fcstd_convert.h — FreeCAD .FCStd archive reader.
//
// Provides read-only access to the metadata and display colours stored in a
// FreeCAD `.FCStd` archive, complementing the B-Rep conversion functions in
// `brep_convert.h`.
//
// A `.FCStd` file is a standard ZIP archive containing:
//
//   Document.xml      — object tree (type, name, label, shape file mapping)
//   GuiDocument.xml   — per-object display settings (colour, visibility …)
//   PartShape.brp,    — OpenCASCADE B-Rep shapes in OCCT BRep text format
//   PartShape1.brp …
//   DiffuseColor,     — per-face colour lists (binary, one per shape)
//   DiffuseColor1 …
//
// DiffuseColor binary format
// --------------------------
// Each `DiffuseColor` file contains:
//   uint32  count             (little-endian: number of faces)
//   uint32  color[count]      (per face, packed as 0xRRGGBBAA little-endian)
//
// Note: FreeCAD stores alpha = 0 for fully opaque faces (inverted convention).
//
// Usage
// -----
//   open2open::FcstdDoc doc;
//   if (open2open::ReadFcstdDoc("/path/to/Model.FCStd", doc)) {
//       for (auto& obj : doc.objects) {
//           std::printf("name=%s brp=%s faces=%zu\n",
//                       obj.label.c_str(), obj.brp_file.c_str(),
//                       obj.face_colors.size());
//       }
//   }
//
// Dependencies
// ------------
// In this BRL-CAD-local copy, FCStd archive I/O is backed by zlib through the
// plugin's narrow archive helper.  OPEN2OPEN_HAVE_LIBZIP is kept as the
// existing open2open compile guard for the FCStd read/write entry points.

#ifndef OPEN2OPEN_FCSTD_CONVERT_H
#define OPEN2OPEN_FCSTD_CONVERT_H

#include <cstdint>
#include <string>
#include <vector>

namespace open2open {

// ---------------------------------------------------------------------------
// Per-object display colour  (packed 0xRRGGBBAA, alpha=0 → fully opaque)
// ---------------------------------------------------------------------------
using FcstdColor = std::uint32_t;

// Convenience helpers for unpacking a FcstdColor.
inline std::uint8_t FcstdColorR(FcstdColor c) { return (c >> 24) & 0xFFu; }
inline std::uint8_t FcstdColorG(FcstdColor c) { return (c >> 16) & 0xFFu; }
inline std::uint8_t FcstdColorB(FcstdColor c) { return (c >>  8) & 0xFFu; }
// alpha=0 means fully opaque in FreeCAD's convention.
inline std::uint8_t FcstdColorA(FcstdColor c) { return (c      ) & 0xFFu; }

// Convert FreeCAD packed color to a normalised [0,1] RGBA tuple.
// FreeCAD alpha=0 → standard alpha=1 (opaque).
inline void FcstdColorToRGBAf(FcstdColor c,
                               float& r, float& g, float& b, float& a)
{
    r = FcstdColorR(c) / 255.0f;
    g = FcstdColorG(c) / 255.0f;
    b = FcstdColorB(c) / 255.0f;
    std::uint8_t fc_alpha = FcstdColorA(c);
    a = (fc_alpha == 0) ? 1.0f : (1.0f - fc_alpha / 255.0f);
}

// ---------------------------------------------------------------------------
// Parse a DiffuseColor binary blob (in memory) into per-face colors.
//
// @param data       Pointer to the raw bytes (e.g. content of DiffuseColor).
// @param size       Number of bytes.
// @param colors_out Receives count packed FcstdColor values; cleared first.
// @return           true on success; false if the data is truncated/invalid.
// ---------------------------------------------------------------------------
bool ParseDiffuseColors(const void*              data,
                        std::size_t              size,
                        std::vector<FcstdColor>& colors_out);

// ---------------------------------------------------------------------------
// Information about one geometric object inside an FCStd archive.
// ---------------------------------------------------------------------------
struct FcstdObject {
    std::string name;        ///< Internal name used as key (e.g. "Part__Feature")
    std::string type;        ///< FreeCAD object type (e.g. "Part::Feature")
    std::string label;       ///< User-facing label (e.g. "ESQ-126-38-G-D_socket")
    std::string part_label;  ///< Label of the containing App::Part (empty if none)
    std::string brp_file;    ///< Path inside ZIP to the BRep file (e.g. "PartShape.brp")
    FcstdColor  shape_color  = 0xC8C8C800u; ///< Overall shape colour (packed RGBA, FreeCAD alpha=0 → opaque)
    bool        visible      = true;
    std::vector<FcstdColor> face_colors; ///< Per-face colours (may be empty)
};

// ---------------------------------------------------------------------------
// Document-level metadata extracted from Document.xml.
// ---------------------------------------------------------------------------
struct FcstdDocMeta {
    std::string label;        ///< Document label (e.g. "ESQ-126-38-G-D")
    std::string created_by;
    std::string creation_date;
    std::string last_modified_date;
    std::string comment;
    std::string company;
};

// ---------------------------------------------------------------------------
// Everything read from one FCStd archive.
// ---------------------------------------------------------------------------
struct FcstdDoc {
    FcstdDocMeta            meta;
    std::vector<FcstdObject> objects; ///< Only objects with a BRep shape file
};

// ---------------------------------------------------------------------------
// Read an FCStd archive and populate a FcstdDoc.
//
// Reads Document.xml, GuiDocument.xml, and all DiffuseColor binary blobs.
// Objects that have no associated BRep file (e.g. App::Origin, App::Line) are
// silently skipped.
//
// @param path  Filesystem path to the .FCStd file.
// @param doc   Output: populated on success; unchanged on failure.
// @return      true if the archive was opened and at least Document.xml was
//              parsed; false if the file could not be opened as a ZIP archive.
//
// This function requires OPEN2OPEN_HAVE_LIBZIP, which enables the local
// zlib-backed FCStd archive implementation in the BRL-CAD plugin build.
// ---------------------------------------------------------------------------
bool ReadFcstdDoc(const std::string& path, FcstdDoc& doc);

#ifdef OPEN2OPEN_HAVE_LIBZIP
// Forward-declare the openNURBS model type in its actual namespace.
// Callers must include "opennurbs.h" themselves.
} // namespace open2open
class ONX_Model;
namespace open2open {

// ---------------------------------------------------------------------------
// Convert an FCStd file to an ONX_Model.
//
// Opens the archive at @p path, reads all B-Rep shapes (*.brp entries),
// converts each shape via OCCTToON_Brep(), and appends to @p model.
// Object name, display colour and layer name are propagated as model
// attributes.  Per-face colours from DiffuseColor are stored on the ON_Brep
// using the first face colour as the object colour.
//
// @param path     Filesystem path to the .FCStd file.
// @param model    Destination model.  Existing content is NOT cleared.
// @param tol      Linear tolerance passed to OCCTToON_Brep().
// @return         Number of shapes successfully converted (0 on total failure).
// ---------------------------------------------------------------------------
int FCStdFileToONX_Model(const std::string& path,
                         ::ONX_Model&       model,
                         double             tol = 1e-6);

// ---------------------------------------------------------------------------
// Convert an ONX_Model to a FreeCAD .FCStd archive.
//
// Writes each B-Rep geometry object in @p model as an OCCT BRep text file
// inside the ZIP archive, along with a minimal Document.xml that links
// object names to their shape files, and a GuiDocument.xml with display
// colours.
//
// @param path     Output filesystem path for the .FCStd file.
// @param model    Source openNURBS model.
// @param tol      Linear tolerance passed to ON_BrepToOCCT().
// @return         Number of shapes successfully written (0 on total failure).
// ---------------------------------------------------------------------------
int ONX_ModelToFCStdFile(const std::string& path,
                         const ::ONX_Model& model,
                         double             tol = 1e-6);
#endif // OPEN2OPEN_HAVE_LIBZIP

} // namespace open2open

#endif // OPEN2OPEN_FCSTD_CONVERT_H
