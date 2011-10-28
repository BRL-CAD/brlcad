
/*
* NIST STEP Core Class Library
* clstepcore/STEPattributeList.cc
* April 1997
* K. C. Morris
* David Sauder

* Development of this software was funded by the United States Government,
* and is not subject to copyright.
*/

/* $Id: STEPattributeList.cc,v 3.0.1.3 1997/11/05 21:59:26 sauderd DP3.1 $ */

#include <STEPattributeList.h>
#include <STEPattribute.h>

//#include <stdlib.h>

AttrListNode::AttrListNode(STEPattribute *a)
{
    attr = a;
}

AttrListNode::~AttrListNode()
{
}


STEPattributeList::STEPattributeList()
{
}

STEPattributeList::~STEPattributeList()
{
}

STEPattribute& STEPattributeList::operator [] (int n)
{
    int x = 0;
    AttrListNode* a = (AttrListNode *)head;
    int cnt =  EntryCount();
    if (n < cnt)
	while (a && (x < n))
	{
	    a = (AttrListNode *)(a->next);
	    x++;
	}
    if (a)  return *(a->attr);

   // else
    cerr << "\nERROR in STEP Core library:  " << __FILE__ <<  ":"
      << __LINE__ << "\n" << _POC_ << "\n\n";
}

int STEPattributeList::list_length()
{
    return EntryCount();
}

void STEPattributeList::push(STEPattribute *a)
{
    AttrListNode *saln = new AttrListNode(a);
    AppendNode (saln);
}










/*
STEPattributeListNode NilSTEPattributeListNode;

class init_NilSTEPattributeListNode
{
public:
  inline init_NilSTEPattributeListNode() 
  {
    NilSTEPattributeListNode.tl = &NilSTEPattributeListNode;
    NilSTEPattributeListNode.ref = -1;
  }
};

static init_NilSTEPattributeListNode NilSTEPattributeListNode_initializer;

*/
