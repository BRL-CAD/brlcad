/* M I M E _ P R O D U C T _ F I X T U R E . H
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
#ifndef BU_MIME_PRODUCT_FIXTURE_H
#define BU_MIME_PRODUCT_FIXTURE_H
#include "common.h"
#include <string>
#include "mime_heightfield_fixture.h"

namespace mime_product_fixture {
inline std::string safe(const std::string &profile = "esa/safe/sentinel-1.0/sentinel-1/sar/level-1/grd/standard/iwdp")
{
    return "<xfdu:XFDU xmlns:xfdu=\"urn:ccsds:schema:xfdu:1\" version=\"" + profile + "\"/>";
}
inline std::string sentinel2(const std::string &level, const std::string &role, const std::string &schema_revision = "14")
{
    const bool product = role == "User_Product";
    const std::string schema = product ? "User_Product_Level-" + level : "S2_PDI_Level-" + level +
	(role == "Tile_ID" ? "_Tile_Metadata" : role == "Granule_ID" ? "_Granule_Metadata" : "_Datastrip_Metadata");
    return "<n:Level-" + level + "_" + role + " xmlns:n=\"https://psd-" + schema_revision +
	".sentinel2.eo.esa.int/PSD/" + schema + ".xsd\"/>";
}
/* The SDK's Identify callback requires at least 512 bytes, even when the
 * complete label describes only a two-by-two raster. */
inline constexpr size_t label_padding = 256;
inline std::string esri_json(unsigned int tile_size = 256, const std::string &format = "PNG")
{
    return "{\"version\":1.0,\"serviceDescription\":\"" + std::string(label_padding, ' ') + "\",\"tileBundlesPath\":\"./tile\",\"minLOD\":0,\"maxLOD\":0,\"spatialReference\":{\"wkid\":4326},"
	"\"storageInfo\":{\"storageFormat\":\"esriMapCacheStorageModeCompactV2\",\"packetSize\":128},"
	"\"tileInfo\":{\"rows\":" + std::to_string(tile_size) + ",\"cols\":" + std::to_string(tile_size) +
	",\"origin\":{\"x\":-1,\"y\":1},\"lods\":[{\"level\":0,\"resolution\":1}]},\"tileImageInfo\":{\"format\":\"" + format + "\"}}\n";
}
inline std::string esri_xml(unsigned int tile_size = 256, const std::string &format = "PNG")
{
    return "<CacheInfo><!--" + std::string(label_padding, ' ') + "--><TileCacheInfo><SpatialReference><WKT>EPSG:4326</WKT></SpatialReference>"
	"<TileOrigin><X>-1</X><Y>1</Y></TileOrigin><TileCols>" + std::to_string(tile_size) + "</TileCols><TileRows>" +
	std::to_string(tile_size) + "</TileRows><LODInfos><LODInfo><LevelID>0</LevelID><Resolution>1</Resolution>"
	"</LODInfo></LODInfos></TileCacheInfo><TileImageInfo><CacheTileFormat>" + format + "</CacheTileFormat></TileImageInfo>"
	"<CacheStorageInfo><StorageFormat>esriMapCacheStorageModeCompactV2</StorageFormat><PacketSize>128</PacketSize>"
	"</CacheStorageInfo></CacheInfo>\n";
}
inline std::string bundle(const std::string &tile)
{
    /* Independent fixture from the owner's Compact Cache V2 layout. One
     * occupied tile is sufficient to verify index addressing and samples. */
    constexpr size_t header_bytes = 64, tile_count = 128 * 128, index_bytes = tile_count * 8;
    std::string out(header_bytes + index_bytes + sizeof(uint32_t), 0);
    const auto word = [&](size_t offset, uint64_t value, size_t bytes = sizeof(uint32_t)) {
	mime_heightfield_fixture::word(out, offset, value, bytes, true);
    };
    word(0, 3); word(4, tile_count); word(8, tile.size()); word(12, 5);
    word(24, out.size() + tile.size(), sizeof(uint64_t)); word(32, 40, sizeof(uint64_t));
    word(40, index_bytes + 20); word(44, 3); word(48, 16); word(52, tile_count); word(56, 5); word(60, index_bytes);
    word(header_bytes, out.size() | (static_cast<uint64_t>(tile.size()) << 40), sizeof(uint64_t));
    word(header_bytes + index_bytes, tile.size());
    return out + tile;
}
}
#endif

/*
 * Local Variables:
 * mode: C++
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
