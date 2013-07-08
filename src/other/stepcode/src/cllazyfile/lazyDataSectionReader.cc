#include "lazyDataSectionReader.h"
#include "lazyFileReader.h"
#include "lazyInstMgr.h"
#include <iostream>

lazyDataSectionReader::lazyDataSectionReader( lazyFileReader * parent, std::ifstream & file,
        std::streampos start, sectionID sid ):
    sectionReader( parent, file, start, sid ) {
    _sectionIdentifier = "";
    std::cerr << "FIXME set _sectionIdentifier" << std::endl;
    _error = false;
}
