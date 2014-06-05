#ifndef LAZYDATASECTIONREADER_H
#define LAZYDATASECTIONREADER_H

#include <map>
#include <iostream>
#include "sectionReader.h"
#include "lazyTypes.h"
#include "sc_memmgr.h"

#include "sc_export.h"

/** base class for data section readers
 * \sa lazyP21DataSectionReader
 * \sa lazyP28DataSectionReader
 */
class lazyDataSectionReader: public sectionReader {
    protected:
        bool _error, _completelyLoaded;
        std::string _sectionIdentifier;

        /// only makes sense to call the ctor from derived class ctors
        lazyDataSectionReader( lazyFileReader * parent, std::ifstream & file, std::streampos start, sectionID sid );
    public:
        virtual ~lazyDataSectionReader() {}
        bool success() {
            return !_error;
        }
};

#endif //LAZYDATASECTIONREADER_H

