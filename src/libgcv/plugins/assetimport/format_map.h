/* F O R M A T _ M A P . H
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
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
#ifndef ASSETIMPORT_FORMAT_MAP_H
#define ASSETIMPORT_FORMAT_MAP_H

#include "common.h"
#include <algorithm>
#include <cstdint>
#include <initializer_list>
#include <limits>
#include <string>
#include <utility>
#include <vector>
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <assimp/version.h>
#include "bu/str.h"
#include "bu/endian.h"
#include "../mime_catalog.h"
#include "gltf_draco.h"
#include "zip_mime.h"
#include "blender_mime.h"
#include "x_mime.h"

static inline bool
assetimport_gltf_importer(const char *name)
{
    return BU_STR_EQUAL(name, "glTF Importer") || BU_STR_EQUAL(name, "glTF2 Importer");
}

static inline bool
assetimport_gltf_host()
{
    /* The SDK swaps GLB headers but copies accessor payload words. */
    return bu_byteorder() == BU_LITTLE_ENDIAN && sizeof(float) == sizeof(uint32_t) && std::numeric_limits<float>::is_iec559;
}

static inline bool
assetimport_draco_input()
{
    /* The SDK exposes no public codec query. Test the linked decoder once,
     * rather than treating our compiler settings as the SDK's settings. */
    static const bool available = []() {
	Assimp::Importer importer;
	const aiScene *scene = importer.ReadFileFromMemory(assetimport_draco_triangle,
	    sizeof(assetimport_draco_triangle) - 1, aiProcess_ValidateDataStructure, "gltf");
	if (!scene) return false;
	bool valid = scene->mNumMeshes == 1 && scene->mMeshes[0]->mNumVertices == 3 && scene->mMeshes[0]->mNumFaces == 1;
	if (valid) {
	    const auto *mesh = scene->mMeshes[0];
	    ai_real min_x = mesh->mVertices[0].x, max_x = min_x, min_y = mesh->mVertices[0].y, max_y = min_y;
	    for (unsigned int i = 1; i < mesh->mNumVertices; ++i) {
		min_x = std::min(min_x, mesh->mVertices[i].x);
		max_x = std::max(max_x, mesh->mVertices[i].x);
		min_y = std::min(min_y, mesh->mVertices[i].y);
		max_y = std::max(max_y, mesh->mVertices[i].y);
	    }
	    constexpr ai_real minimum_unit_extent = 0.5;
	    valid = mesh->mFaces[0].mNumIndices == 3 && max_x - min_x > minimum_unit_extent && max_y - min_y > minimum_unit_extent;
	}
	return valid;
    }();
    return available;
}

/* Bind SDK format identities deliberately. An extension collision must
 * never turn support for one format into support for an unrelated type.
 * Unmapped SDK formats remain unavailable until their identity is audited. */
static inline std::vector<const char *>
assetimport_import_identities(const char *importer, const std::string &extension)
{
    struct mapping { const char *importer; const char *extensions; const char *mime; };
    static const mapping formats[] = {
	{"Irrlicht Mesh Reader", " xml irrmesh ", "model/vnd.irrlicht.mesh+xml"},
	{"XGL Importer", " xgl ", "model/vnd.realitywave.xgl+xml"},
	{"XGL Importer", " zgl ", "model/vnd.realitywave.zgl"},
	{"Ogre3D Mesh Importer", " mesh ", "model/vnd.ogre.mesh"},
	{"Ogre3D Mesh Importer", " mesh.xml ", "model/vnd.ogre.mesh+xml"},
	{"Quake III BSP Importer", " bsp ", "model/vnd.idsoftware.bsp"},
	{"Quake III BSP Importer", " bsp pk3 ", "application/vnd.idsoftware.pk3+zip"},
	{"Unreal Mesh Importer", " 3d ", "model/vnd.epicgames.unreal.mesh"},
	{"Unreal Mesh Importer", " uc ", "text/vnd.epicgames.unrealscript"},
	{"Irrlicht Scene Reader", " irr xml ", "model/vnd.irrlicht.scene+xml"},
	{"Quick3D Importer", " q3o q3s ", "model/vnd.quick3d"},
	{"MMD Importer", " pmx ", "model/vnd.mikumikudance.pmx"},
	{"Raw Importer", " raw ", "model/vnd.povray.raw"},
	{"BVH Importer (MoCap)", " bvh ", "model/vnd.biovision.bvh"},
	{"CharacterStudio Motion Importer (MoCap)", " csm ", "model/vnd.autodesk.csm"},
	{"LightWave Scene Importer", " mot ", "model/vnd.newtek.motion"},
	{"Neutral File Format Importer", " nff enff ", "model/prs.haines.nff"},
	{"Neutral File Format Importer", " nff enff ", "model/vnd.sense8.nff"},
	{"Valve SMD Importer", " smd vta ", "model/vnd.valve.smd"},
	{"Doom 3 / MD5 Mesh Importer", " md5mesh md5anim md5camera ", "model/vnd.idsoftware.md5"},
	{"Quake Mesh / 3D GameStudio Mesh Importer", " mdl ", "model/vnd.idsoftware.mdl"},
	{"Quake Mesh / 3D GameStudio Mesh Importer", " mdl ", "model/vnd.conitec.mdl"},
	{"Quake Mesh / 3D GameStudio Mesh Importer", " mdl ", "model/vnd.valve.mdl"},
	{"3D GameStudio Heightmap (HMP) Importer", " hmp ", "image/vnd.conitec.hmp"},
	{"Return To Castle Wolfenstein Mesh Importer", " mdc ", "model/vnd.idsoftware.mdc"},
	{"Milkshape 3D Importer", " ms3d ", "model/vnd.chumbalumsoft.ms3d"},
	{"Nendo Mesh Importer", " ndo ", "model/vnd.izware.nendo"},
	{"Silo SIB Importer", " sib ", "model/vnd.nevercenter.sib"},
	{"TrueSpace Object Importer", " cob scn ", "model/vnd.caligari.cob"},
	{"Terragen Heightmap Importer", " ter ", "application/terragen"},
	{"ASE Importer", " ase ask ", "model/vnd.autodesk.ase"},
	{"BlitzBasic 3D Importer", " b3d ", "model/vnd.blitzresearch.b3d"},
	{"Inter-Quake Model Importer", " iqm ", "model/vnd.interquake.iqm"},
	{"Quake II Mesh Importer", " md2 ", "model/vnd.idsoftware.md2"},
	{"Quake III Mesh Importer", " md3 ", "model/vnd.idsoftware.md3"},
	{"LightWave Scene Importer", " lws ", "image/x-lws"},
	{"Direct3D XFile Importer", " x ", "model/vnd.microsoft.x"},
	{"Wavefront Object Importer", " obj ", "model/obj"},
	{"Additive manufacturing file format(AMF) Importer", " amf ", "model/amf"},
	{"Discreet 3DS Importer", " 3ds prj ", "image/x-3ds"},
	{"Stanford Polygon Library (PLY) Importer", " ply ", "application/x-ply"},
	{"Stereolithography (STL) Importer", " stl ", "model/stl"},
	{"LightWave/Modo Object Importer", " lwo ", "image/x-lwo"},
	{"LightWave/Modo Object Importer", " lxo ", "model/vnd.foundry.lxo"},
	{"Drawing Interchange Format (DXF) Importer", " dxf ", "image/vnd.dxf"},
	{"OFF Importer", " off ", "model/vnd.geomview.off"},
	{"AC3D Importer", " ac acc ac3d ", "model/vnd.inivis.ac3d"},
	{"Industry Foundation Classes (IFC) Importer", " ifc step stp ", "application/p21"},
	{"Industry Foundation Classes (IFC) Importer", " ifczip ", "application/zip"},
	{"Collada Importer", " dae xml ", "model/vnd.collada+xml"},
	{"Collada Importer", " zae ", assetimport_zip::zae_type},
	{"Open Game Engine Exchange", " ogex ", "model/vnd.opengex"},
	{"Blender 3D Importer (http://www.blender3d.org)", " blend ", "application/x-blender"},
	{"Autodesk FBX Importer", " fbx ", "application/vnd.autodesk.fbx"},
	{"Assimp Binary Importer", " assbin ", "model/vnd.assimp.binary"},
	{"glTF Importer", " gltf ", "model/gltf+json"},
	{"glTF Importer", " glb ", "model/gltf-binary"},
	{"glTF2 Importer", " gltf ", "model/gltf+json"},
	{"glTF2 Importer", " glb vrm ", "model/gltf-binary"},
	{"3mf Importer", " 3mf ", "model/3mf"},
	{"Extensible 3D(X3D, X3DB) Importer", " x3d ", "model/x3d+xml"},
	{"Extensible 3D(X3D, X3DB) Importer", " x3db ", "model/x3d+fastinfoset"}
    };
    std::string token = " " + extension + " ";
    std::vector<const char *> identities;
    for (const auto &format : formats)
	if (BU_STR_EQUAL(importer, format.importer) && std::string(format.extensions).find(token) != std::string::npos)
	    identities.push_back(format.mime);
    return identities;
}

using assetimport_profile = std::vector<std::pair<std::string, std::string>>;

/* Identity registration is useful even when the current GCV reader cannot
 * preserve the source data. Generated skeleton previews are not payloads;
 * a listed extension does not establish an implemented encoding. */
static inline bool
assetimport_assbin_host()
{
    if ((bu_byteorder() != BU_LITTLE_ENDIAN && bu_byteorder() != BU_BIG_ENDIAN) ||
	sizeof(unsigned int) != sizeof(uint32_t)) return false;
    return aiGetCompileFlags() & ASSIMP_CFLAGS_DOUBLE_SUPPORT ?
	sizeof(double) == sizeof(uint64_t) && std::numeric_limits<double>::is_iec559 :
	sizeof(float) == sizeof(uint32_t) && std::numeric_limits<float>::is_iec559;
}

static inline const char *
assetimport_assbin_order()
{
    return bu_byteorder() == BU_BIG_ENDIAN ? "big-endian" : "little-endian";
}

static inline const char *
assetimport_real_width()
{
    return aiGetCompileFlags() & ASSIMP_CFLAGS_DOUBLE_SUPPORT ? "64" : "32";
}

static inline bool
assetimport_geometry_input(const char *type)
{
    if (BU_STR_EQUAL(type, "model/vnd.assimp.binary")) return assetimport_assbin_host();
    if (BU_STR_EQUAL(type, "model/gltf+json") || BU_STR_EQUAL(type, "model/gltf-binary")) return assetimport_gltf_host();
    /* Its packed vertex decoder relies on native bitfield allocation. */
    if (BU_STR_EQUAL(type, "model/vnd.epicgames.unreal.mesh") || BU_STR_EQUAL(type, "text/vnd.epicgames.unrealscript"))
	return bu_byteorder() == BU_LITTLE_ENDIAN;
    /* These SDK parsers read native float/int words directly. */
    if (BU_STR_EQUAL(type, "model/vnd.mikumikudance.pmx") || BU_STR_EQUAL(type, "application/vnd.idsoftware.pk3+zip") ||
	BU_STR_EQUAL(type, "model/vnd.blitzresearch.b3d"))
	return bu_byteorder() == BU_LITTLE_ENDIAN && sizeof(float) == sizeof(uint32_t) &&
	    sizeof(int) == sizeof(uint32_t) && std::numeric_limits<float>::is_iec559;
    return !BU_STR_EQUAL(type, "model/vnd.biovision.bvh") &&
	!BU_STR_EQUAL(type, "model/vnd.autodesk.csm") && !BU_STR_EQUAL(type, "model/vnd.newtek.motion") &&
	/* This SDK's X3D reader passes its input to an XML parser. */
	!BU_STR_EQUAL(type, "model/x3d+fastinfoset") &&
	/* The BSP driver opens a ZIP archive even for its .bsp association. */
	!BU_STR_EQUAL(type, "model/vnd.idsoftware.bsp") &&
	/* IFCZIP member handling needs correction before advertising a reader. */
	!BU_STR_EQUAL(type, "application/zip");
}

static inline std::vector<assetimport_profile>
assetimport_import_profiles(const char *importer, const char *mime_type)
{
    /* The shared XML parser detects Unicode widths and Latin-1 declarations;
     * ASCII is a supported subset of its eight-bit input. */
    constexpr const char *xml_encodings[] = {"ascii", "iso-8859-1", "utf-8", "utf-16le", "utf-16be", "utf-32le", "utf-32be"};
    if (BU_STR_EQUAL(importer, "Wavefront Object Importer"))
	return {{{"obj.serialization", "text"}, {"text.code-unit-width", "8"}}};
    if (BU_STR_EQUAL(importer, "Open Game Engine Exchange"))
	return {{{"opengex.serialization", "text"}, {"text.code-unit-width", "8"}}};
    if (BU_STR_EQUAL(importer, "Autodesk FBX Importer"))
	return {{{"fbx.serialization", "text"}, {"text.code-unit-width", "8"}, {"fbx.text-bom", "false"}},
	    {{"fbx.serialization", "binary"}, {"encoding", "binary"}, {"byte-order", "little-endian"}}};
    if (BU_STR_EQUAL(importer, "3mf Importer"))
	return {{{"encoding", "binary"}, {"container", "zip"}}};
    if (BU_STR_EQUAL(importer, "Discreet 3DS Importer"))
	return {{{"encoding", "binary"}, {"byte-order", "little-endian"}}};
    if (BU_STR_EQUAL(importer, "Stereolithography (STL) Importer")) {
	std::vector<assetimport_profile> profiles = {
	    {{"stl.serialization", "text"}, {"stl.utf8-bom", "false"}},
	    {{"stl.serialization", "text"}, {"stl.utf8-bom", "true"}, {"encoding", "utf-8"}}};
	/* The SDK's count and float reads use native little-endian words. */
	if (bu_byteorder() == BU_LITTLE_ENDIAN && sizeof(float) == sizeof(uint32_t) && std::numeric_limits<float>::is_iec559)
	    profiles.push_back({{"stl.serialization", "binary"}, {"encoding", "binary"}, {"byte-order", "little-endian"}});
	return profiles;
    }
    if (BU_STR_EQUAL(importer, "Stanford Polygon Library (PLY) Importer"))
	return {{{"encoding", "ascii"}}, {{"encoding", "binary"}, {"byte-order", "little-endian"}},
	    {{"encoding", "binary"}, {"byte-order", "big-endian"}}};
    if (BU_STR_EQUAL(importer, "Direct3D XFile Importer")) {
	std::vector<assetimport_profile> profiles;
	for (const char *serialization : {"text", "binary"})
	    for (const char *width : {"32", "64"})
		for (const char *compression : {"none", "mszip"}) {
		    const bool binary = BU_STR_EQUAL(serialization, "binary");
		    const bool compressed = BU_STR_EQUAL(compression, "mszip");
		    if (compressed && !assetimport_x::compressed_input()) continue;
		    if (binary && !assetimport_x::binary_width(width)) continue;
		    assetimport_profile profile = {{"x.serialization", serialization}, {"x.float-width", width},
			{"compression", compression}, {"x.header-code-unit-width", "8"}};
		    if (binary || compressed) profile.emplace_back("encoding", "binary");
		    if (binary) profile.emplace_back("byte-order", "little-endian");
		    profiles.push_back(std::move(profile));
		}
	return profiles;
    }
    if (BU_STR_EQUAL(importer, "Assimp Binary Importer")) {
	std::vector<assetimport_profile> profiles;
	for (const char *compression : {"none", "zlib"})
	    profiles.push_back({{"encoding", "binary"}, {"byte-order", assetimport_assbin_order()}, {"version", "1.0"},
		{"assimp.real-width", assetimport_real_width()}, {"assimp.shortened", "false"}, {"compression", compression}});
	return profiles;
    }
    if (BU_STR_EQUAL(importer, "AC3D Importer"))
	return {{{"ac3d.serialization", "text"}, {"text.code-unit-width", "8"}}};
    if (BU_STR_EQUAL(importer, "ASE Importer"))
	return {{{"ase.serialization", "text"}, {"text.code-unit-width", "8"}}};
    if (BU_STR_EQUAL(importer, "Blender 3D Importer (http://www.blender3d.org)")) {
	std::vector<assetimport_profile> profiles;
	for (const char *width : {"32", "64"})
	    for (const char *order : {"little-endian", "big-endian"})
		for (const char *compression : {"none", "gzip"}) {
		    if (BU_STR_EQUAL(compression, "gzip") && !assetimport_blender::gzip_input()) continue;
		    profiles.push_back({{"encoding", "binary"}, {"blender.format-version", "0"},
			{"blender.pointer-width", width}, {"byte-order", order}, {"compression", compression}});
		}
	return profiles;
    }
    if (BU_STR_EQUAL(importer, "Additive manufacturing file format(AMF) Importer")) {
	std::vector<assetimport_profile> profiles;
	for (const char *encoding : xml_encodings)
	    profiles.push_back({{"encoding", encoding}, {"container", "none"}, {"xml.root-qname", "amf"}, {"xml.root-localname", "amf"}});
	return profiles;
    }
    if (BU_STR_EQUAL(importer, "Drawing Interchange Format (DXF) Importer"))
	return {{{"dxf.serialization", "text"}}};
    if (BU_STR_EQUAL(importer, "OFF Importer")) {
	std::vector<assetimport_profile> profiles;
	for (const char *encoding : {"ascii", "utf-8"})
	    for (const char *dimension : {"1", "2", "3"})
		profiles.push_back({{"encoding", encoding}, {"off.dimension", dimension}, {"off.header-comments", "false"}});
	return profiles;
    }
    if (BU_STR_EQUAL(importer, "Industry Foundation Classes (IFC) Importer")) return {{}};
    if (BU_STR_EQUAL(importer, "Collada Importer") && assetimport_zip::is_zae(mime_type))
	return {{{"encoding", "binary"}, {"container", "zip"}, {"collada.schema-version", "1.4"}},
	    {{"encoding", "binary"}, {"container", "zip"}, {"collada.schema-version", "1.5"}}};
    if (assetimport_gltf_importer(importer) && mime_type) {
	bool binary = BU_STR_EQUAL(mime_type, "model/gltf-binary");
	bool modern = BU_STR_EQUAL(importer, "glTF2 Importer");
	assetimport_profile profile = {{"version", modern ? "2.0" : "1.0"}, {"encoding", binary ? "binary" : "utf-8"}};
	if (binary) {
	    profile.emplace_back("gltf.container-version", modern ? "2" : "1");
	    profile.emplace_back("byte-order", "little-endian");
	}
	return {profile};
    }
    if (BU_STR_EQUAL(importer, "Unreal Mesh Importer") && mime_type) {
	if (BU_STR_EQUAL(mime_type, "model/vnd.epicgames.unreal.mesh"))
	    return {{{"encoding", "binary"}, {"byte-order", "little-endian"}, {"unreal.part", "data"}, {"unreal.vertex-stride", "4"}},
		{{"encoding", "binary"}, {"byte-order", "little-endian"}, {"unreal.part", "animation"}, {"unreal.vertex-stride", "4"}}};
	std::vector<assetimport_profile> profiles;
	for (const char *encoding : {"utf-8", "utf-16le", "utf-16be", "utf-32le", "utf-32be"})
	    profiles.push_back({{"encoding", encoding}});
	return profiles;
    }
    if (BU_STR_EQUAL(importer, "Quake III BSP Importer") && mime_type && BU_STR_EQUAL(mime_type, "application/vnd.idsoftware.pk3+zip"))
	return {{{"encoding", "binary"}, {"container", "zip"}}};
    if (BU_STR_EQUAL(importer, "Ogre3D Mesh Importer") && mime_type) {
	if (BU_STR_EQUAL(mime_type, "model/vnd.ogre.mesh"))
	    return {{{"encoding", "binary"}, {"byte-order", "little-endian"}, {"version", "1.8"}, {"ogre.serializer", "[MeshSerializer_v1.8]"}}};
	std::vector<assetimport_profile> profiles;
	for (const char *encoding : xml_encodings)
	    profiles.push_back({{"encoding", encoding}, {"compression", "none"}, {"xml.root-qname", "mesh"}, {"xml.root-localname", "mesh"}});
	return profiles;
    }
    if (BU_STR_EQUAL(importer, "XGL Importer") && mime_type) {
	std::vector<assetimport_profile> profiles;
	bool compressed = BU_STR_EQUAL(mime_type, "model/vnd.realitywave.zgl");
	for (const char *encoding : xml_encodings) {
	    assetimport_profile profile = {{compressed ? "xml.encoding" : "encoding", encoding},
		{"compression", compressed ? "zlib" : "none"}, {"xml.root-qname", "WORLD"}, {"xml.root-localname", "WORLD"}};
	    if (compressed) profile.emplace_back("encoding", "binary");
	    profiles.push_back(std::move(profile));
	}
	return profiles;
    }
    if (BU_STR_EQUAL(importer, "Irrlicht Mesh Reader") || BU_STR_EQUAL(importer, "Irrlicht Scene Reader") ||
	BU_STR_EQUAL(importer, "Collada Importer") || (mime_type && BU_STR_EQUAL(mime_type, "model/x3d+xml"))) {
	std::vector<assetimport_profile> profiles;
	const char *root = BU_STR_EQUAL(importer, "Irrlicht Mesh Reader") ? "mesh" :
	    BU_STR_EQUAL(importer, "Irrlicht Scene Reader") ? "irr_scene" :
	    BU_STR_EQUAL(importer, "Collada Importer") ? "COLLADA" : "X3D";
	for (const char *encoding : xml_encodings) {
	    assetimport_profile profile = {{"encoding", encoding}, {"xml.root-qname", root}, {"xml.root-localname", root}};
	    if (BU_STR_EQUAL(importer, "Collada Importer")) {
		for (const char *schema : {"1.4", "1.5"}) {
		    auto versioned = profile;
		    versioned.emplace_back("collada.schema-version", schema);
		    profiles.push_back(std::move(versioned));
		}
	    } else profiles.push_back(std::move(profile));
	}
	return profiles;
    }
    if (BU_STR_EQUAL(importer, "Quick3D Importer")) {
	std::vector<assetimport_profile> profiles;
	for (const char *version : {"3.0", "3.1"})
	    for (const char *kind : {"object", "scene"})
		profiles.push_back({{"version", version}, {"quick3d.kind", kind}, {"encoding", "binary"}, {"byte-order", "little-endian"}});
	return profiles;
    }
    if (BU_STR_EQUAL(importer, "MMD Importer")) {
	std::vector<assetimport_profile> profiles;
	for (const char *version : {"2.0", "2.1"})
	    for (const char *text : {"utf-8", "utf-16le"})
		/* Additional-channel conversion indexes the SDK array incorrectly. */
		profiles.push_back({{"version", version}, {"encoding", "binary"}, {"byte-order", "little-endian"},
		    {"pmx.text-encoding", text}, {"pmx.additional-uv", "0"}, {"pmx.header-size", "8"}});
	return profiles;
    }
    if (BU_STR_EQUAL(importer, "Raw Importer") || BU_STR_EQUAL(importer, "LightWave Scene Importer"))
	return {{{"encoding", "ascii"}}};
    if (BU_STR_EQUAL(importer, "Neutral File Format Importer"))
	return {{{"encoding", "ascii"}}};
    if (BU_STR_EQUAL(importer, "LightWave/Modo Object Importer")) {
	std::vector<assetimport_profile> profiles;
	if (!mime_type) return profiles;
	std::vector<const char *> forms;
	if (BU_STR_EQUAL(mime_type, "image/x-lwo")) forms = {"LWOB", "LWO2"};
	if (BU_STR_EQUAL(mime_type, "model/vnd.foundry.lxo")) forms = {"LXOB"};
	for (const char *form : forms)
	    profiles.push_back({{"iff.form", form}, {"encoding", "binary"}, {"byte-order", "big-endian"}});
	return profiles;
    }
    if (BU_STR_EQUAL(importer, "Valve SMD Importer"))
	return {{{"version", "1"}, {"encoding", "ascii"}, {"smd.kind", "mesh"}}};
    if (BU_STR_EQUAL(importer, "Doom 3 / MD5 Mesh Importer"))
	return {{{"version", "10"}, {"encoding", "ascii"}, {"md5.kind", "mesh"}}};
    if (BU_STR_EQUAL(importer, "Quake Mesh / 3D GameStudio Mesh Importer")) {
	std::vector<assetimport_profile> profiles;
	if (!mime_type) return profiles;
	std::vector<const char *> versions;
	if (BU_STR_EQUAL(mime_type, "model/vnd.idsoftware.mdl")) versions = {"6"};
	if (BU_STR_EQUAL(mime_type, "model/vnd.conitec.mdl")) versions = {"2", "3", "4", "5", "7"};
	if (BU_STR_EQUAL(mime_type, "model/vnd.valve.mdl")) versions = {"10"};
	for (const char *version : versions) {
	    assetimport_profile profile = {{"version", version}, {"encoding", "binary"}, {"byte-order", "little-endian"}};
	    if (BU_STR_EQUAL(mime_type, "model/vnd.valve.mdl")) profile.emplace_back("valve.mdl-kind", "model");
	    profiles.push_back(std::move(profile));
	}
	return profiles;
    }
    const struct {
	const char *importer;
	const char *encoding;
	const char *byte_order;
	std::initializer_list<const char *> versions;
    } formats[] = {
	{"3D GameStudio Heightmap (HMP) Importer", "binary", "little-endian", {"5", "7"}},
	{"Quake II Mesh Importer", "binary", "little-endian", {"8"}},
	{"Quake III Mesh Importer", "binary", "little-endian", {"15"}},
	{"Inter-Quake Model Importer", "binary", "little-endian", {"2"}},
	{"Return To Castle Wolfenstein Mesh Importer", "binary", "little-endian", {"2"}},
	{"Milkshape 3D Importer", "binary", "little-endian", {"4"}},
	{"Terragen Heightmap Importer", "binary", "little-endian", {NULL}},
	{"BlitzBasic 3D Importer", "binary", "little-endian", {NULL}},
	{"Nendo Mesh Importer", "binary", "big-endian", {"1.0", "1.1", "1.2"}},
	{"Silo SIB Importer", "binary", "little-endian", {"1", "2"}},
	{"TrueSpace Object Importer", "ascii", "little-endian", {"00.01"}},
	{"TrueSpace Object Importer", "binary", "little-endian", {"00.01"}}
    };
    std::vector<assetimport_profile> profiles;
    for (const auto &format : formats)
	if (BU_STR_EQUAL(importer, format.importer))
	    for (const char *version : format.versions) {
		assetimport_profile traits = {{"encoding", format.encoding}, {"byte-order", format.byte_order}};
		if (version) traits.emplace_back("version", version);
		profiles.push_back(std::move(traits));
	    }
    return profiles;
}

static inline gcv_mime_catalog::set_constraints
assetimport_import_sets(const char *importer, const char *mime_type)
{
    if (BU_STR_EQUAL(importer, "OFF Importer"))
	return {{"off.vertex-attributes", {"colors", "homogeneous-coordinates", "normals", "texture-coordinates"}}};
    if (BU_STR_EQUAL(importer, "Industry Foundation Classes (IFC) Importer")) return {{"p21.schemas", {"IFC2X3"}}};
    if (assetimport_zip::is_zae(mime_type)) return {};
    if (assetimport_gltf_importer(importer)) {
	std::vector<std::string> required;
	if (BU_STR_EQUAL(importer, "glTF2 Importer")) {
	    required = {"KHR_materials_pbrSpecularGlossiness", "KHR_materials_specular", "KHR_materials_unlit",
		"KHR_lights_punctual", "KHR_texture_transform", "KHR_materials_sheen", "KHR_materials_clearcoat",
		"KHR_materials_transmission", "KHR_materials_volume", "KHR_materials_ior", "KHR_materials_emissive_strength",
		"KHR_materials_anisotropy", "KHR_texture_basisu", "EXT_texture_webp"};
	    if (assetimport_draco_input()) required.emplace_back("KHR_draco_mesh_compression");
	}
	return {{"gltf.required-extensions", required}};
    }
    if (BU_STR_EQUAL(importer, "Irrlicht Mesh Reader"))
	return {{"xml.root-namespaces", {"http://irrlicht.sourceforge.net/IRRMESH_09_2007"}},
	    {"irrlicht.vertex-layouts", {"2tcoords", "standard", "tangents"}}};
    if (BU_STR_EQUAL(importer, "Irrlicht Scene Reader"))
	return {{"xml.root-namespaces", {}}, {"irrlicht.node-types", {"animatedmesh", "camera", "cube", "empty", "light", "mesh", "octtree", "skybox", "sphere"}}};
    if (BU_STR_EQUAL(importer, "Collada Importer"))
	return {{"xml.root-namespaces", {"http://www.collada.org/2005/11/COLLADASchema", "http://www.collada.org/2008/03/COLLADASchema"}}};
    if (BU_STR_EQUAL(importer, "MMD Importer"))
	return {{"pmx.index-widths", {"1", "2", "4"}}};
    if (BU_STR_EQUAL(importer, "Raw Importer"))
	return {{"raw.features", {"face-colors", "groups", "textures"}}};
    if (BU_STR_EQUAL(importer, "Neutral File Format Importer") && mime_type && BU_STR_EQUAL(mime_type, "model/prs.haines.nff"))
	return {{"nff.extensions", {"dod", "hex", "oct", "shader", "tess", "tet", "tpp"}}};
    return {};
}

struct assetimport_export_format {
    const char *id;
    const char *mime;
    const char *encoding;
    const char *version;
    const char *materials;
};

static inline const assetimport_export_format *
assetimport_export_identity(const char *id)
{
    /* GLB 1 omits body padding from its header lengths. The PLY writers
     * omit color properties while writing the adapter's vertex colors.
     * Keep their independent readers, but do not advertise these encoders
     * until the SDK's wire output is corrected. */
    static const assetimport_export_format formats[] = {
	{"collada", "model/vnd.collada+xml", "utf-8", "1.4.1", NULL},
	{"x", "model/vnd.microsoft.x", "ascii", "3.3", NULL},
	{"stp", "model/step", NULL, NULL, NULL},
	{"obj", "model/obj", NULL, NULL, "mtl"},
	{"objnomtl", "model/obj", NULL, NULL, "none"},
	{"stl", "model/stl", "ascii", NULL, NULL},
	{"stlb", "model/stl", "binary", NULL, NULL},
	{"ply", NULL, NULL, NULL, NULL},
	{"plyb", NULL, NULL, NULL, NULL},
	{"3ds", "image/x-3ds", "binary", NULL, NULL},
	{"gltf2", "model/gltf+json", "utf-8", "2.0", NULL},
	{"glb2", "model/gltf-binary", "binary", "2.0", NULL},
	{"gltf", "model/gltf+json", "utf-8", "1.0", NULL},
	{"glb", NULL, NULL, NULL, NULL},
	{"assbin", "model/vnd.assimp.binary", "binary", "1.0", NULL},
	{"assxml", "model/vnd.assimp+xml", "utf-8", NULL, NULL},
	{"assjson", "model/vnd.assimp+json", "utf-8", "100", NULL},
	{"x3d", "model/x3d+xml", "utf-8", "3.3", NULL},
	{"fbx", "application/vnd.autodesk.fbx", "binary", "7.5.0", NULL},
	{"fbxa", "application/vnd.autodesk.fbx", NULL, "7.5.0", NULL},
	{"3mf", "model/3mf", "binary", NULL, NULL},
	{"pbrt", "model/vnd.pbrt.scene", NULL, "4", NULL}
    };
    for (const auto &format : formats)
	if (BU_STR_EQUAL(id, format.id)) {
	    if (!format.mime) return NULL;
	    if (BU_STR_EQUAL(id, "assbin") && !assetimport_assbin_host()) return NULL;
	    if ((BU_STR_EQUAL(format.mime, "model/gltf+json") || BU_STR_EQUAL(format.mime, "model/gltf-binary")) &&
		!assetimport_gltf_host()) return NULL;
	    return &format;
	}
    return NULL;
}

static inline std::vector<std::pair<std::string, std::string>>
assetimport_export_traits(const assetimport_export_format &format)
{
    std::vector<std::pair<std::string, std::string>> traits;
    if (BU_STR_EQUAL(format.id, "stl") || BU_STR_EQUAL(format.id, "stlb")) {
	traits.emplace_back("stl.serialization", BU_STR_EQUAL(format.id, "stl") ? "text" : "binary");
	if (BU_STR_EQUAL(format.id, "stl")) traits.emplace_back("stl.utf8-bom", "false");
    }
    if (BU_STR_EQUAL(format.mime, "model/obj")) {
	traits.emplace_back("obj.serialization", "text");
	traits.emplace_back("text.code-unit-width", "8");
	traits.emplace_back("obj.text-bom", "false");
    }
    if (format.encoding) traits.emplace_back("encoding", format.encoding);
    if (format.version) traits.emplace_back("version", format.version);
    if (format.materials) traits.emplace_back("obj.materials", format.materials);
    if (BU_STR_EQUAL(format.id, "fbx") || BU_STR_EQUAL(format.id, "fbxa")) {
	const bool binary = BU_STR_EQUAL(format.id, "fbx");
	traits.emplace_back("fbx.serialization", binary ? "binary" : "text");
	traits.emplace_back("fbx.document-version-code", "7500");
	if (binary) traits.emplace_back("fbx.binary-header-version-code", "7500");
	else {
	    traits.emplace_back("text.code-unit-width", "8");
	    traits.emplace_back("fbx.text-bom", "false");
	}
    }
    if (BU_STR_EQUAL(format.id, "collada")) traits.emplace_back("collada.schema-version", "1.4");
    if (BU_STR_EQUAL(format.id, "x")) {
	traits.emplace_back("x.float-width", "32");
	traits.emplace_back("compression", "none");
	traits.emplace_back("x.serialization", "text");
	traits.emplace_back("x.header-code-unit-width", "8");
    }
    if (BU_STR_EQUAL(format.id, "assbin")) {
	traits.emplace_back("compression", "none");
	traits.emplace_back("assimp.real-width", assetimport_real_width());
	traits.emplace_back("byte-order", assetimport_assbin_order());
    }
    if (BU_STR_EQUAL(format.id, "assxml")) traits.emplace_back("assimp.format-id", "1");
    if (BU_STR_EQUAL(format.id, "assbin") || BU_STR_EQUAL(format.id, "assxml"))
	traits.emplace_back("assimp.shortened", "false");
    if (BU_STR_EQUAL(format.id, "3mf")) {
	traits.emplace_back("container", "zip");
	traits.emplace_back("3mf.model-encoding", "utf-8");
    }
    /* The SDK's stp exporter declares AUTOMOTIVE_DESIGN without an edition. */
    if (BU_STR_EQUAL(format.id, "stp")) traits.emplace_back("step.ap", "214");
    if (BU_STR_EQUAL(format.id, "x3d")) traits.emplace_back("x3d.profile", "Interchange");
    if (BU_STR_EQUAL(format.id, "stlb") || BU_STR_EQUAL(format.id, "3ds") || BU_STR_EQUAL(format.id, "fbx") ||
	BU_STR_EQUAL(format.mime, "model/gltf-binary"))
	traits.emplace_back("byte-order", "little-endian");
    if (BU_STR_EQUAL(format.id, "glb2")) traits.emplace_back("gltf.container-version", "2");
    return traits;
}

static inline std::vector<assetimport_profile>
assetimport_export_profiles(const assetimport_export_format &format)
{
    std::vector<assetimport_profile> profiles = {assetimport_export_traits(format)};
    if (BU_STR_EQUAL(format.id, "fbxa") || BU_STR_EQUAL(format.mime, "model/obj")) {
	profiles.push_back(profiles.front());
	profiles.back().emplace_back("encoding", "ascii");
    }
    return profiles;
}

#endif
