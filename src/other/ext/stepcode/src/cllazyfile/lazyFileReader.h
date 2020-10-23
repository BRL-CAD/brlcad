#ifndef LAZYFILEREADER_H
#define LAZYFILEREADER_H

#include <vector>
#include <string>
#include <cstdlib>

#include "sc_export.h"

// PART 21
#include "lazyP21DataSectionReader.h"
#include "p21HeaderSectionReader.h"
#include "headerSectionReader.h"

/* // PART 28
 * #include "lazyP28DataSectionReader.h"
 * #include "p28HeaderSectionReader.h"
 */
#include "sc_memmgr.h"

class lazyInstMgr;
class Registry;
class headerSectionReader;

///read an exchange file of any supported type (currently only p21)
///for use only from within lazyInstMgr
class SC_LAZYFILE_EXPORT lazyFileReader {
    protected:
        std::string _fileName;
        lazyInstMgr * _parent;
        headerSectionReader * _header;
        std::ifstream _file;
        fileTypeEnum _fileType;
        fileID _fileID;

        void initP21();

        ///TODO detect file type; for now, assume all are Part 21
        void detectType() {
            _fileType = Part21;
        }
    public:
        fileID ID() const {
            return _fileID;
        }
        instancesLoaded_t * getHeaderInstances();

        lazyFileReader( std::string fname, lazyInstMgr * i, fileID fid );
        ~lazyFileReader();

        fileTypeEnum type() const {
            return _fileType;
        }
        lazyInstMgr * getInstMgr() const {
            return _parent;
        }

        bool needKW( const char * kw );
};

#endif //LAZYFILEREADER_H

