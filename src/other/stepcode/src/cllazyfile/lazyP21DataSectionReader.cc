#include <assert.h>
#include <set>
#include "lazyP21DataSectionReader.h"
#include "lazyInstMgr.h"

lazyP21DataSectionReader::lazyP21DataSectionReader( lazyFileReader * parent, std::ifstream & file,
        std::streampos start, sectionID sid ):
    lazyDataSectionReader( parent, file, start, sid ) {
    findSectionStart();
    namedLazyInstance nl;
    while( nl = nextInstance(), ( ( nl.loc.begin > 0 ) && ( nl.name != 0 ) ) ) {
        parent->getInstMgr()->addLazyInstance( nl );
    }

    if(  sectionReader::_error->severity() <= SEVERITY_WARNING ) {
        sectionReader::_error->PrintContents( std::cerr );
        if(  sectionReader::_error->severity() <= SEVERITY_INPUT_ERROR ) {
            _error = true;
            return;        
        }
    }
        
    if( !_file.good() ) {
        _error = true;
        return;
    }
    if( nl.loc.instance == 0 ) {
        //check for ENDSEC;
        skipWS();
        std::streampos pos = _file.tellg();
        if( _file.get() == 'E' && _file.get() == 'N' && _file.get() == 'D'
                && _file.get() == 'S' && _file.get() == 'E' && _file.get() == 'C'
                && ( skipWS(), _file.get() == ';' ) ) {
            _sectionEnd = _file.tellg();
        } else {
            _file.seekg( pos );
            char found[26] = { '\0' };
            _file.read( found, 25 );
            std::cerr << "expected 'ENDSEC;', found " << found << std::endl;
            _error = true;
        }
    }
}

// part of readdata1
//if this changes, probably need to change sectionReader::getType()
const namedLazyInstance lazyP21DataSectionReader::nextInstance() {
    std::streampos end = -1;
    namedLazyInstance i;

    i.refs = 0;
    i.loc.begin = _file.tellg();
    i.loc.instance = readInstanceNumber();
    if( ( _file.good() ) && ( i.loc.instance > 0 ) ) {
        skipWS();
        i.loc.section = _sectionID;
        i.name = getDelimitedKeyword( ";( /\\" );
        if( _file.good() ) {
            end = seekInstanceEnd( & i.refs );
        }
    }
    if( ( i.loc.instance == 0 ) || ( !_file.good() ) || ( end == ( std::streampos ) - 1 ) ) {
        //invalid instance, so clear everything
        _file.seekg( i.loc.begin );
        i.loc.begin = -1;
        if( i.refs ) {
            delete i.refs;
        }
        i.name = 0;
    }
    return i;
}

