#include <assert.h>
#include <string.h>

#include "p21HeaderSectionReader.h"
#include "headerSectionReader.h"
#include "sectionReader.h"
#include "lazyInstMgr.h"
#include "judyL2Array.h"


void p21HeaderSectionReader::findSectionStart() {
    _sectionStart = findNormalString( "HEADER", true );
    assert( _file.is_open() && _file.good() );
}

p21HeaderSectionReader::p21HeaderSectionReader( lazyFileReader * parent, std::ifstream & file,
        std::streampos start, sectionID sid ):
    headerSectionReader( parent, file, start, sid ) {
    findSectionStart();
    findSectionEnd();
    _file.seekg( _sectionStart );
    namedLazyInstance nl;
    while( nl = nextInstance(), ( nl.loc.begin > 0 ) ) {
        std::streampos pos = _file.tellg();
        _headerInstances->insert( nl.loc.instance, getRealInstance( _lazyFile->getInstMgr()->getHeaderRegistry(), nl.loc.begin, nl.loc.instance, nl.name, "", true ) );
        _file.seekg( pos ); //reset stream position for next call to nextInstance()
    }
    _file.seekg( _sectionEnd );
}

// part of readdata1
const namedLazyInstance p21HeaderSectionReader::nextInstance() {
    namedLazyInstance i;
    static instanceID nextFreeInstance = 4; // 1-3 are reserved per 10303-21

    i.loc.begin = _file.tellg();
    i.loc.section = _sectionID;
    skipWS();
    if( i.loc.begin <= 0 ) {
        i.name = 0;
    } else {
        i.name = getDelimitedKeyword( ";( /\\" );

        if( 0 == strcmp( "FILE_DESCRIPTION", i.name ) ) {
            i.loc.instance = 1;
        } else if( 0 == strcmp( "FILE_NAME", i.name ) ) {
            i.loc.instance = 2;
        } else if( 0 == strcmp( "FILE_SCHEMA", i.name ) ) {
            i.loc.instance = 3;
        } else {
            i.loc.instance = nextFreeInstance++;
        }

        assert( strlen( i.name ) > 0 );

        std::streampos end = seekInstanceEnd( 0 ); //no references in file header
        if( ( (signed long int)end == -1 ) || ( end >= _sectionEnd ) ) {
            //invalid instance, so clear everything
            i.loc.begin = -1;
            i.name = 0;
        }
    }
    return i;
}

