/** \file inverse_attr2.cc
** 1-Jul-2012
** Test inverse attributes; uses a tiny schema similar to a subset of IFC2x3
**
*/
#include <sc_cf.h>
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

///second way of finding inverse attrs
bool findInverseAttrs2( InverseAItr iai, InstMgr & instList, Registry & reg ) {
    const Inverse_attribute * ia;
    int j = 0;
    while( 0 != ( ia = iai.NextInverse_attribute() ) ) {
        cout << "inverse attr #" << j << ", name: " << ia->Name() << ", inverted attr id: " << ia->inverted_attr_id_()
             << ", from entity: " << ia->inverted_entity_id_() << endl;

        //now find the entity containing the attribute in question
        const EntityDescriptor * inv_ed = reg.FindEntity( ia->inverted_entity_id_() );
        AttrDescItr attr_desc_itr( inv_ed->ExplicitAttr() );
        const AttrDescriptor * attrDesc;
        int k = 0;
        while( 0 != ( attrDesc = attr_desc_itr.NextAttrDesc() ) ) {
            if( !strcmp( ia->inverted_attr_id_(), attrDesc->Name() ) ) {
                cout << "attribute '" << attrDesc->Name() << "' is attribute #" << k
                     << " of '" << inv_ed->Name() << "'." << endl;

                // now go through the instList looking at each instance of
                // entity type 'inv_ed', looking for references to 'ed' in the
                // attribute described by 'attrDesc'
                int l = 0;
                SdaiReldefinesbytype * inst;
                while( 0 != ( inst = ( SdaiReldefinesbytype * ) instList.GetApplication_instance( inv_ed->Name(), l ) ) ) {
                    int i = inst->StepFileId();
                    if( i < l ) {
                        break;
                    }
                    STEPattributeList attrlist = inst->attributes;
                    if( attrlist.list_length() < k + 1 ) {
                        return false;
                    }
                    STEPattribute sa = attrlist[k];
                    if( sa.getADesc()->DomainType()->Type() == SET_TYPE ) {
                        STEPaggregate * aggr = sa.Aggregate();
                        if( !aggr || aggr->is_null() != 0 ) {
                            cout << "findInverseAttrs2 FAILED" << endl;
                            return false;
                        }
                    } else {
                        //something is wrong - it should be an aggregate (specifically, a SET)
                        return false;
                    }
                    l = i;
                }
            }
            k++;
        }
        j++;
    }
    return( j != 0 );
}


int main( int argc, char * argv[] ) {
    Registry  registry( SchemaInit );
    InstMgr   instance_list;
    STEPfile  sfile( registry, instance_list, "", false );
    bool inverseAttrsFound = false;
    if( argc != 2 ) {
        exit( EXIT_FAILURE );
    }
    sfile.ReadExchangeFile( argv[1] );

    if( sfile.Error().severity() <= SEVERITY_INCOMPLETE ) {
        sfile.Error().PrintContents( cout );
        exit( EXIT_FAILURE );
    }
    //find inverse attribute descriptors
    //first, find inverse attrs unique to this entity (i.e. not inherited)
    const EntityDescriptor * ed = registry.FindEntity( "window" );
    InverseAItr iaIter( &( ed->InverseAttr() ) ); //iterator for inverse attributes
    if( findInverseAttrs2( iaIter, instance_list, registry ) ) {
        inverseAttrsFound = true;
    }
    //now, find inherited inverse attrs
    EntityDescItr edi( ed->GetSupertypes() );
    const EntityDescriptor * super;
    while( 0 != ( super = edi.NextEntityDesc() ) ) {
        cout << "supertype " << super->Name() << endl;
        InverseAItr superIaIter( &( super->InverseAttr() ) );
        if( findInverseAttrs2( superIaIter, instance_list, registry ) ) {
            inverseAttrsFound = true;
        }
    }
    if( !inverseAttrsFound ) {
        cout << "no inverse attrs found" << endl;
        exit( EXIT_FAILURE );
    }
    exit( EXIT_SUCCESS );
}

