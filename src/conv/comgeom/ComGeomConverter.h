#ifndef BRLCAD_COMGEOM_CONVERTER_H
#define BRLCAD_COMGEOM_CONVERTER_H

#include "common.h"
#include <string>

extern "C" {
#include "wdb.h"
#include "bu/file.h"
#include "bu/log.h"
#include "bu/app.h"
}

struct ConverterOptions {
    int version = 5;
    int verbose = 0;
    std::string suffix;
    bool legacy_truncate_names = false; // enable legacy suffix truncation (max 13 chars)
};

class ComGeomConverter {
public:
    ComGeomConverter(const std::string &inputPath,
                     const std::string &outputPath,
                     const ConverterOptions &opts);
    ~ComGeomConverter();
    int run();

private:
    bool openInput();
    bool openOutput();
    bool writeDbHeader(const std::string &title, const std::string &units);

private:
    std::string m_inPath, m_outPath;
    ConverterOptions m_opts;
    FILE *m_in = nullptr;
    rt_wdb *m_out = nullptr;
};

#endif
