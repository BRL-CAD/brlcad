/** \file inverse_attr.cc
** 1-Jul-2012
** Test inverse attributes; uses a tiny schema similar to a subset of IFC2x3
**
*/
#include <sc_cf.h>
extern void SchemaInit( class Registry & );
#include "sc_version_string.h"
#include <STEPfile.h>
#include <sdai.h>
#include <STEPattribute.h>
#include <ExpDict.h>
#include <Registry.h>
#include <errordesc.h>
#include <algorithm>
#include <string>
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif
#include <sc_getopt.h>
#include "schema.h"

int main( int argc, char * argv[] ) {
    Registry  registry( SchemaInit );
    InstMgr   instance_list;
    STEPfile  sfile( registry, instance_list, "", false );
    if( argc != 2 ) {
        exit( EXIT_FAILURE );
    }
    sfile.ReadExchangeFile( argv[1] );

    if( sfile.Error().severity() <= SEVERITY_INCOMPLETE ) {
        sfile.Error().PrintContents( cout );
        exit( EXIT_FAILURE );
    }
//find attributes
    SdaiWindow * instance = ( SdaiWindow * ) instance_list.GetApplication_instance( "window" );
    if( !instance ) {
        cout << "NULL" << endl;
        exit( EXIT_FAILURE );
    }
    cout << "instance #" << instance->StepFileId() << endl;

    /* The inverse could be set with
     *    const Inverse_attribute * ia =...;
     *    const EntityDescriptor * inv_ed = reg.FindEntity( ia->inverted_entity_id_() );
     *    instance->isdefinedby_(inv_ed);
     */
    EntityAggregate * aggr = instance->isdefinedby_();
    if( aggr ) {
        cout << aggr->EntryCount() << endl;
    } else {
        cout << "inverse attr is not defined" << endl;
        exit( EXIT_FAILURE );
    }
    exit( EXIT_SUCCESS );
}

