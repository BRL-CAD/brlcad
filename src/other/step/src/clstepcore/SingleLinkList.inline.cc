
/*
* NIST STEP Core Class Library
* clstepcore/SingleLinkList.inline.cc
* April 1997
* David Sauder
* KC Morris

* Development of this software was funded by the United States Government,
* and is not subject to copyright.
*/

/* $Id: SingleLinkList.inline.cc,v 3.0.1.2 1997/11/05 21:59:23 sauderd DP3.1 $ */

#include <SingleLinkList.h>
#include <iostream>

SingleLinkNode * 	
SingleLinkNode::NextNode ()  const
{
    return next;
}

SingleLinkList::SingleLinkList ()  
  : head (0), tail (0)
{
}

SingleLinkList::~SingleLinkList ()  
{
    Empty ();
}

void
SingleLinkList::Empty ()  
{
    SingleLinkNode * tmp = head;
    while (tmp)  
      {
	  tmp = head -> NextNode ();
	  delete head;
	  head = tmp;
      }
}

SingleLinkNode *
SingleLinkList::NewNode () 
{
    //  defined in subtypes
    std::cerr << "\n\n******BUG****** a virtually defined function should \n"
	 << "be called for SingleLinkList::NewNode()\n\n";
    return new SingleLinkNode();
}

SingleLinkNode *
SingleLinkList::GetHead () const
{
    return (head);
}

int SingleLinkList::EntryCount() const
{
    int entryCount = 0;
    SingleLinkNode *entryPtr = head;

    while( entryPtr != 0 )
    {
	entryPtr = entryPtr->NextNode();
	entryCount++;
    }
    return entryCount;
}
