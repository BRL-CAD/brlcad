
#include "sc_version_string.h"
#include <STEPfile.h>
#include <sdai.h>
#include <STEPattribute.h>
#include <ExpDict.h>
#include <Registry.h>
#include <errordesc.h>
#include <algorithm>
#include <string>

#ifdef HAVE_STD_THREAD
# include <thread>
#else
# error Need std::thread for this test!
#endif

#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif

#include "SdaiAUTOMOTIVE_DESIGN.h"

//macro for 50 ms sleep (could be more for a larger file, may need reduced for a fast processor)
#ifndef __WIN32__
# define DELAY(t) usleep( t * 100 );
#else
#define DELAY(t) Sleep( t );
#endif


// NOTE this test requires std::thread, part of C++11. It will fail to compile otherwise.

void readProgressParallel( STEPfile & f, float & maxProgress ) {
    while( 1 ) {
        float p = f.GetReadProgress();
        if( p > maxProgress ) {
            maxProgress = p;
        }
        DELAY( 50 );
    }
}

void writeProgressParallel( STEPfile & f, float & maxProgress ) {
    while( 1 ) {
        float p = f.GetWriteProgress();
        if( p > maxProgress ) {
            maxProgress = p;
        }
        DELAY( 50 );
    }
}

int main( int argc, char * argv[] ) {
    float progress = 0.0;

    if( argc != 2 ) {
        cerr << "Wrong number of args. Use: " << argv[0] << " file.stp" << endl;
        exit( EXIT_FAILURE );
    }

    Registry  registry( SchemaInit );
    InstMgr   instance_list;
    STEPfile  sfile( registry, instance_list, "", false );

    // read the file
    std::thread r( readProgressParallel, std::ref( sfile ), std::ref( progress ) );
    sfile.ReadExchangeFile( argv[1] );
    r.detach();
    Severity readSev = sfile.Error().severity();
    if( readSev != SEVERITY_NULL ) {
        sfile.Error().PrintContents( cout );
        exit( EXIT_FAILURE );
    }
    if( progress < 55 ) { //55 is arbitrary. should be >50 due to how GetReadProgress() works.
        cerr << "Error: Read progress (" << progress << ") never exceeded the threshold (55). Exiting." << endl;
        exit( EXIT_FAILURE );
    } else {
        cout << "Read progress reached " << progress << "% - success." << endl;
    }
    progress = 0;

    // write the file
    std::thread w( writeProgressParallel, std::ref( sfile ), std::ref( progress ) );
    sfile.WriteExchangeFile( "out.stp" );
    w.detach();
    readSev = sfile.Error().severity();
    if( readSev != SEVERITY_NULL ) {
        sfile.Error().PrintContents( cout );
        exit( EXIT_FAILURE );
    }
    if( progress < 55 ) {
        cerr << "Error: Write progress (" << progress << ") never exceeded the threshold (55). Exiting." << endl;
        exit( EXIT_FAILURE );
    } else {
        cout << "Write progress reached " << progress << "% - success." << endl;
    }

    exit( EXIT_SUCCESS );
}


