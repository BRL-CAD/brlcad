/* F O R M A T _ M A P . H
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
#ifndef GCV_GDAL_FORMAT_MAP_H
#define GCV_GDAL_FORMAT_MAP_H
#include "common.h"
#include "bu/str.h"

namespace gcv_gdal {
struct identity_binding {
    const char *driver;
    const char *mime_type;
};
/* These are published format identities, not names generated from SDK
 * backend IDs. Variant facts remain unknown without source observations. */
inline constexpr identity_binding identity_bindings[] = {
    {"ACE2", "application/x-ace2"},
    {"ADRG", "image/adrg"},
    {"AIG", "application/aig"},
    {"AirSAR", "image/x-airsar"},
    {"BSB", "image/bsb"},
    {"CEOS", "image/ceos"},
    {"COASP", "application/x-coasp"},
    {"COSAR", "application/x-cosar"},
    {"CPG", "application/x-cpg"},
    {"CTG", "application/x-ctg"},
    {"DIMAP", "image/x-dimap"},
    {"DOQ1", "application/x-doq1"},
    {"DOQ2", "application/x-doq2"},
    {"ECRGTOC", "application/x-ecrg-toc"},
    {"EIR", "image/eir"},
    {"ENVI", "application/x-envi"},
    {"ERS", "application/x-ers"},
    {"ESAT", "image/envisat"},
    {"FAST", "application/x-fast"},
    {"GFF", "application/gff"},
    {"GSC", "application/x-gsc"},
    {"GTX", "application/x-gtx"},
    {"GenBin", "application/x-generic-bin"},
    {"ILWIS", "image/ilwis"},
    {"ISIS2", "application/x-isis2"},
    {"ISIS3", "application/x-isis3"},
    {"JAXAPALSAR", "application/jaxa-pal-sar"},
    {"JDEM", "application/jdem"},
    {"KRO", "application/x-kro"},
    {"L1B", "application/x-l1b"},
    {"LAN", "application/x-lan"},
    {"LCP", "application/x-lcp"},
    {"LOSLAS", "application/x-los-las"},
    {"MAP", "application/x-map"},
    {"MFF", "image/x-mff"},
    {"MFF2", "image/x-mff2"},
    {"MSGN", "application/x-msgn"},
    {"NDF", "application/x-ndf"},
    {"NGSGEOID", "application/x-ngs-geoid"},
    {"NTv2", "application/x-ntv2"},
    {"NWT_GRC", "application/x-nwt-grc"},
    {"NWT_GRD", "application/x-nwt-grd"},
    {"PAux", "application/x-p-aux"},
    {"PCRaster", "image/x-pcraster"},
    {"PDS", "application/x-pds"},
    {"RIK", "application/x-rik"},
    {"RMF", "application/x-rmf"},
    {"RPFTOC", "application/x-rpf-toc"},
    {"RS2", "application/x-rs2"},
    {"SAGA", "application/x-sdat"},
    {"SAR_CEOS", "image/sar-ceos"},
    {"SNODAS", "application/x-snodas"},
    {"SRP", "image/x-srp"},
    {"SRTMHGT", "application/x-srtmhgt"},
    {"TIL", "application/x-til"},
    {"TSX", "application/x-tsx"},
    {"USGSDEM", "application/x-usgs-dem"},
    {"VRT", "application/vrt"},
};

inline const char *identity(const char *driver)
{
    for (const auto &binding : identity_bindings)
	if (BU_STR_EQUAL(driver, binding.driver)) return binding.mime_type;
    return NULL;
}
}
#endif
