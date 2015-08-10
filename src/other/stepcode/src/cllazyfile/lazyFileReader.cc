#include <assert.h>


#include "lazyFileReader.h"
#include "lazyDataSectionReader.h"
#include "headerSectionReader.h"
#include "lazyInstMgr.h"

void lazyFileReader::initP21() {
    _header = new p21HeaderSectionReader( this, _file, 0, -1 );

    for( ;; ) {
        lazyDataSectionReader * r;
        r = new lazyP21DataSectionReader( this, _file, _file.tellg(), _parent->countDataSections() );
        if( !r->success() ) {
            delete r; //last read attempt failed
            std::cerr << "Corrupted data section" << std::endl;
            break;
        }
        _parent->registerDataSection( r );

        //check for new data section (DATA) or end of file (END-ISO-10303-21;)
        while( isspace( _file.peek() ) && _file.good() ) {
            _file.ignore( 1 );
        }
        if( needKW( "END-ISO-10303-21;" ) ) {
            break;
        } else if( !needKW( "DATA" ) ) {
            std::cerr << "Corrupted file - did not find new data section (\"DATA\") or end of file (\"END-ISO-10303-21;\") at offset " << _file.tellg() << std::endl;
            break;
        }
    }
}

bool lazyFileReader::needKW( const char * kw ) {
    const char * c = kw;
    bool found = true;
    while( *c ) {
        if( *c != _file.get() ) {
            found = false;
            break;
        }
        c++;
    }
    return found;
}

instancesLoaded_t * lazyFileReader::getHeaderInstances() {
    return _header->getInstances();
}

lazyFileReader::lazyFileReader( std::string fname, lazyInstMgr * i, fileID fid ): _fileName( fname ), _parent( i ), _fileID( fid ) {
    _file.open( _fileName.c_str() );
    _file.imbue( std::locale::classic() );
    _file.unsetf( std::ios_base::skipws );
    assert( _file.is_open() && _file.good() );

    detectType();
    switch( _fileType ) {
        case Part21:
            initP21();
            break;
        case Part28:
            //initP28();
            //break;
        default:
            std::cerr << "Reached default case, " << __FILE__ << ":" << __LINE__ << std::endl;
            abort();
    }
}

lazyFileReader::~lazyFileReader() {
    delete _header;
}

