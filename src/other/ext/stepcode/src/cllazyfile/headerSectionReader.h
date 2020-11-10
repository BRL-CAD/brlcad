#ifndef HEADERSECTIONREADER_H
#define HEADERSECTIONREADER_H

#include <iostream>
#include <fstream>

#include "judyL2Array.h"
#include "lazyFileReader.h"
#include "sectionReader.h"
#include "lazyTypes.h"
#include "sc_memmgr.h"

#include "sc_export.h"

///differs from the lazyDataSectionReader in that all instances are always loaded
class SC_LAZYFILE_EXPORT headerSectionReader: public sectionReader {
    protected:
        instancesLoaded_t * _headerInstances;

        /// must derive from this class
        headerSectionReader( lazyFileReader * parent, std::ifstream & file, std::streampos start, sectionID sid ):
            sectionReader( parent, file, start, sid ) {
            _headerInstances = new instancesLoaded_t;
        }
    public:
        instancesLoaded_t * getInstances() const {
            return _headerInstances;
        }

        ~headerSectionReader() {
            //FIXME delete each instance?! maybe add to clear, since it iterates over everything already
            //enum clearHow { rawData, deletePointers }
            _headerInstances->clear();
        }
};

#endif  //HEADERSECTIONREADER_H

