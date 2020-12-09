
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

#include "SdaiTEST_ARRAY_BOUNDS_EXPR.h"

int main( int argc, char * argv[] ) {

    if( argc != 2 ) {
        cerr << "Wrong number of args. Use: " << argv[0] << " file.stp" << endl;
        exit( 1 );
    }

    Registry  registry( SchemaInit );
    InstMgr   instance_list;
    STEPfile  sfile( registry, instance_list, "", false );

    sfile.ReadExchangeFile( argv[1] );
    Severity readSev = sfile.Error().severity();
    if( readSev != SEVERITY_NULL ) {
        sfile.Error().PrintContents( cout );
        exit( EXIT_FAILURE );
    }

    // Keeps track of the last processed ent id
    int search_index = 0;

    //find structured_mesh, find the array attribute, and check its bounds as much as possible. need an instance to check 100%.
    const EntityDescriptor * ed = registry.FindEntity( "Structured_mesh" );
    AttrDescItr aditr( ed->ExplicitAttr() );
    const AttrDescriptor * attrDesc = aditr.NextAttrDesc();
    int descAggrCount = 0;
    while( attrDesc != 0 ) {
        if( ( attrDesc->NonRefType() == ARRAY_TYPE ) && ( attrDesc->AggrElemType() == sdaiINTEGER ) ) {
            cout << "Array attribute: " << attrDesc->Name();
            const AggrTypeDescriptor * atd = ( const AggrTypeDescriptor * ) attrDesc->DomainType();
            if( !( atd->Bound1Type() == bound_constant ) || !( atd->Bound2Type() == bound_runtime ) ) {
                cerr << "Invalid bounds. Exiting." << endl;
                exit( EXIT_FAILURE );
            }
            cout << " -- bound 1 is a constant (" << atd->Bound1() << "). bound 2 depends upon the instance." << endl;
            descAggrCount++;
        }
        attrDesc = aditr.NextAttrDesc();
    }
    if( descAggrCount != 1 ) {
        cerr << "Expected 1 aggregate attribute descriptor, found " << descAggrCount << ". Exiting." << endl;
        exit( EXIT_FAILURE );
    }


    // Loop over the instances in the file, check the bound for each
    SdaiStructured_mesh * ent;
    while( ENTITY_NULL != ( ent = ( SdaiStructured_mesh * )
                                  instance_list.GetApplication_instance( "Structured_mesh", search_index ) ) ) {
        SDAI_Integer b2;
        int instAggrCnt = 0;
        IntAggregate * vertex_counts = ent->vertex_counts_();
        int cnt = ent->AttributeCount();
        STEPattributeList & sal = ent->attributes;
        int id = ent->StepFileId();

        cout << "Ent #" << id << " - ";
        if( cnt != 3 ) {
            cerr << "Expected 3 attributes, found " << cnt << ". Exiting." << endl;
            exit( EXIT_FAILURE );
        }
        if( id > 2 ) {
            cerr << "Expected 2 instances, found " << cnt << ". Exiting." << endl;
            exit( EXIT_FAILURE );
        }
        //loop over the attributes
        for( int i = 0; i < cnt; i++ ) {
            const AttrDescriptor * ad = sal[i].getADesc();
            if( ( ad->NonRefType() == ARRAY_TYPE ) && ( ad->AggrElemType() == sdaiINTEGER ) ) {
                b2 = ( ( AggrTypeDescriptor * ) ad->DomainType() )->Bound2Runtime( ent );
                cout << "bound 2: " << b2 << " values: ";
                instAggrCnt++;
                if( ( ( id == 1 ) && ( b2 != 3 ) ) || ( ( id == 2 ) && ( b2 != 5 ) ) ) {
                    cerr << "Instance " << id << ": value " << b2 << " is invalid for bound 2.";
                    cerr << " Expecting 3 for instance #1 or 5 for #2." << endl;
                    exit( EXIT_FAILURE );
                }
            }
        }

        int node = 0;
        int aggrValues[2][5] = {{1, 2, 3, -1, -1}, {9, 34, 0, 3, 999999}};
        IntNode * aggrNode = ( IntNode * ) vertex_counts->GetHead();
        while( aggrNode != 0 ) {
            if( node >= b2 ) {
                cerr << "Instance " << id << ": Number of values exceeds upper bound. Exiting." << endl;
                exit( EXIT_FAILURE );
            }
            cout << aggrNode->value << " ";
            if( aggrValues[id - 1][node] != aggrNode->value ) {
                cerr << "Instance " << id << ": aggregate value " << aggrNode->value << " at index " << node << " is incorrect. Exiting." << endl;
                exit( EXIT_FAILURE );
            }
            aggrNode = ( IntNode * ) aggrNode->NextNode();
            node++;
        }
        cout << endl;

        if( instAggrCnt != 1 ) {
            cerr << "Expected 1 aggregate attribute in this instance, found " << instAggrCnt << ". Exiting." << endl;
            exit( EXIT_FAILURE );
        }


        MgrNode * mnode = instance_list.FindFileId( id );
        search_index = instance_list.GetIndex( mnode ) + 1;
    }
}
