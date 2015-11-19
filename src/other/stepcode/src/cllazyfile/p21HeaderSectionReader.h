#ifndef P21HEADERSECTIONREADER_H
#define P21HEADERSECTIONREADER_H

#include "headerSectionReader.h"
#include "sc_memmgr.h"
#include "sc_export.h"

class SC_LAZYFILE_EXPORT p21HeaderSectionReader: public headerSectionReader {
    public:
        p21HeaderSectionReader( lazyFileReader * parent, std::ifstream & file, std::streampos start, sectionID sid );
        void findSectionStart();
        /** gets information (start, end, name, etc) about the next
         * instance in the file and returns it in a namedLazyInstance
         * \sa lazyP21DataSectionReader::nextInstance()
         */
        const namedLazyInstance nextInstance();
};

#endif //P21HEADERSECTIONREADER_H

