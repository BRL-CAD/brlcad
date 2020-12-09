/** \file inverse_attr3.cc
 * Oct 2013
 * Test inverse attributes; uses a tiny schema similar to a subset of IFC2x3
 *
 * This test originally used STEPfile, which didn't work. Fixing STEPfile would have been very difficult, it uses lazyInstMgr now.
 */
#include <sc_cf.h>
#include <lazyInstMgr.h>
#include <lazyRefs.h>
#include <sdai.h>
#include <STEPattribute.h>
#include <ExpDict.h>
#include <Registry.h>
#include <errordesc.h>
#include <algorithm>
#include <string>
#include <superInvAttrIter.h>

#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif
#include <sc_getopt.h>
#include "schema.h"

int main( int argc, char * argv[] ) {
    int exitStatus = EXIT_SUCCESS;
    if( argc != 2 ) {
        cerr << "Wrong number of args!" << endl;
        exit( EXIT_FAILURE );
    }
    lazyInstMgr lim;
    lim.initRegistry( SchemaInit );

    lim.openFile( argv[1] );

//find attributes
    instanceTypes_t::cvector * insts = lim.getInstances( "window" );
    if( !insts || insts->empty() ) {
        cout << "No window instances found!" << endl;
        exit( EXIT_FAILURE );
    }
    SdaiWindow * instance = dynamic_cast< SdaiWindow * >( lim.loadInstance( insts->at( 0 ) ) );
    if( !instance ) {
        cout << "Problem loading instance" << endl;
        exit( EXIT_FAILURE );
    }
    cout << "instance #" << instance->StepFileId() << endl;

    SDAI_Application_instance::iAMap_t::value_type v = instance->getInvAttr("isdefinedby");
    iAstruct attr = v.second; //instance->getInvAttr(ia);
        if( attr.a && attr.a->EntryCount() ) {
            cout << "Map: found " << attr.a->EntryCount() << " inverse references." << endl;
        } else {
            cout << "Map: found no inverse references. ias " << (void *) &(v.second) << ", ia " << (void*) v.first << endl;
            exitStatus = EXIT_FAILURE;
        }

    EntityAggregate * aggr = instance->isdefinedby_(); //should be filled in when the file is loaded? not sure how to do it using STEPfile...
    if( attr.a != aggr ) {
        cout << "Error! got different EntityAggregate's when using map vs method" << endl;
        exitStatus = EXIT_FAILURE;
    }
    if( aggr && aggr->EntryCount() ) {
        cout << "Found " << aggr->EntryCount() << " inverse references." << endl;
    } else {
        cout << "inverse attr is not defined" << endl;
        exitStatus = EXIT_FAILURE;
    }
    exit( exitStatus );
}

