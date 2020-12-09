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
class SC_LAZYFILE_EXPORT lazyDataSectionReader: public sectionReader {
    protected:
        bool _error, _completelyLoaded;
#ifdef _MSC_VER
#pragma warning( push )
#pragma warning( disable: 4251 )
#endif
        std::string _sectionIdentifier;
#ifdef _MSC_VER
#pragma warning( pop )
#endif

        /// only makes sense to call the ctor from derived class ctors
        lazyDataSectionReader( lazyFileReader * parent, std::ifstream & file, std::streampos start, sectionID sid );
    public:
        virtual ~lazyDataSectionReader() {}
        bool success() {
            return !_error;
        }
};

#endif //LAZYDATASECTIONREADER_H

