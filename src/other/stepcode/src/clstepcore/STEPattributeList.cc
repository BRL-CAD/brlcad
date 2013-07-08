
/*
* NIST STEP Core Class Library
* clstepcore/STEPattributeList.cc
* April 1997
* K. C. Morris
* David Sauder

* Development of this software was funded by the United States Government,
* and is not subject to copyright.
*/

#include <STEPattributeList.h>
#include <STEPattribute.h>
#include "sc_memmgr.h"

AttrListNode::AttrListNode( STEPattribute * a ) {
    attr = a;
}

AttrListNode::~AttrListNode() {
}


STEPattributeList::STEPattributeList() {
}

STEPattributeList::~STEPattributeList() {
}

STEPattribute & STEPattributeList::operator []( int n ) {
    int x = 0;
    AttrListNode * a = ( AttrListNode * )head;
    int cnt =  EntryCount();
    if( n < cnt )
        while( a && ( x < n ) ) {
            a = ( AttrListNode * )( a->next );
            x++;
        }
    if( a ) {
        return *( a->attr );
    }

    // else
    cerr << "\nERROR in STEP Core library:  " << __FILE__ <<  ":"
         << __LINE__ << "\n" << _POC_ << "\n\n";
    return *( STEPattribute * ) 0;
}

int STEPattributeList::list_length() {
    return EntryCount();
}

void STEPattributeList::push( STEPattribute * a ) {
    bool push = true;

    // if the attribute already exists in the list, we don't push it
    // TODO: does it break anything?
    AttrListNode * a2 = ( AttrListNode * )head;
    while( a2 && push ) {
        if( *a == *( a2 -> attr ) ) {
            push = false;
            break;
        }
        a2 = ( AttrListNode * )( a2->next );
    }

    if( push ) {
        a->refCount ++;
        AttrListNode * saln = new AttrListNode( a );
        AppendNode( saln );
    }
}
