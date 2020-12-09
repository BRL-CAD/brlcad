
/** \file STEPinvAttrList.cc
 * derived from STEPattributeList.cc
 */

#include <STEPinvAttrList.h>
#include <ExpDict.h>
#include "sc_memmgr.h"

invAttrListNodeI::invAttrListNodeI(Inverse_attribute* a, setterI_t s, getterI_t g): invAttrListNode(a), set( s ), get( g ) {}
invAttrListNodeA::invAttrListNodeA(Inverse_attribute* a, setterA_t s, getterA_t g): invAttrListNode(a), set( s ), get( g ) {}

invAttrListNodeI::~invAttrListNodeI() {}
invAttrListNodeA::~invAttrListNodeA() {}

STEPinvAttrList::STEPinvAttrList() {}
STEPinvAttrList::~STEPinvAttrList() {}

invAttrListNode * STEPinvAttrList::operator []( int n ) {
    int x = 0;
    invAttrListNode * a = ( invAttrListNode * )head;
    int cnt =  EntryCount();
    if( n < cnt ) {
        while( a && ( x < n ) ) {
            a = ( invAttrListNode * )( a->next );
            x++;
        }
    }
    if( !a ) {
        cerr << "\nERROR in STEP Core library:  " << __FILE__ <<  ":"
        << __LINE__ << "\n" << _POC_ << "\n\n";
    }
    return a;
}

int STEPinvAttrList::list_length() {
    return EntryCount();
}

void STEPinvAttrList::push( Inverse_attribute * a, setterA_t s, getterA_t g ) {
    invAttrListNode * an = ( invAttrListNode * )head;

    // if the attribute already exists in the list, don't push it
    while( an ) {
        if( a == ( an -> attr ) ) {
            return;
        }
        an = ( invAttrListNode * )( an->next );
    }
    invAttrListNode * ialn = (invAttrListNode *) new invAttrListNodeA( a, s, g );
    AppendNode( ialn );
}

void STEPinvAttrList::push( Inverse_attribute * a, setterI_t s, getterI_t g ) {
    invAttrListNode * an = ( invAttrListNode * )head;

    // if the attribute already exists in the list, don't push it
    while( an ) {
        if( a == ( an -> attr ) ) {
            return;
        }
        an = ( invAttrListNode * )( an->next );
    }
    invAttrListNode * ialn = (invAttrListNode *) new invAttrListNodeI( a, s, g );
    AppendNode( ialn );
}
