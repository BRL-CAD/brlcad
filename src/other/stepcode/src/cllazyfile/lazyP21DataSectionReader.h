#ifndef LAZYP21DATASECTIONREADER_H
#define LAZYP21DATASECTIONREADER_H

#include "lazyDataSectionReader.h"
#include "lazyFileReader.h"
#include "sc_memmgr.h"
#include "sc_export.h"

class SC_LAZYFILE_EXPORT lazyP21DataSectionReader: public lazyDataSectionReader {
    protected:
    public:
        lazyP21DataSectionReader( lazyFileReader * parent, std::ifstream & file, std::streampos start, sectionID sid );

        void findSectionStart() {
            _sectionStart = findNormalString( "DATA", true );
        }
        /** gets information (start, end, name, etc) about the next
         * instance in the file and returns it in a namedLazyInstance
         * \sa p21HeaderSectionReader::nextInstance()
         */
        const namedLazyInstance nextInstance();

};

#endif //LAZYP21DATASECTIONREADER_H

