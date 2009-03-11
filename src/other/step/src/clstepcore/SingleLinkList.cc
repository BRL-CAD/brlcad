
/*
* NIST STEP Core Class Library
* clstepcore/SingleLinkList.cc
* April 1997
* David Sauder
* KC Morris

* Development of this software was funded by the United States Government,
* and is not subject to copyright.
*/

/* $Id: SingleLinkList.cc,v 3.0.1.4 1997/11/05 21:59:23 sauderd DP3.1 $ */

#include <SingleLinkList.h>

void 
SingleLinkList::DeleteFollowingNodes (SingleLinkNode *item)
{
 if (head) {
  SingleLinkNode * trailer = 0;
  SingleLinkNode * leader = head;
  while (leader) {
   if (leader == item)  {
       while(leader)
       {
	   if(trailer) trailer->next = leader->next;
	   else if(leader == head) 
	   {
	       head = leader->next;
	       trailer = head;
	   }
	   if(leader == tail) tail = trailer;
	   delete leader;
	   leader = trailer->next;
       }
   }
   else {
    if(trailer)
     trailer = trailer->NextNode();
    else trailer = leader;
    leader = leader->NextNode();
   }
  }
 }
}

void
SingleLinkList::AppendNode (SingleLinkNode * item)  
{
    if (head)  {
	tail -> next = item;
	tail = item;
    }
    else head = tail = item;
    item->owner = this;
}

void 
SingleLinkList::DeleteNode (SingleLinkNode * item)
{
 if (head) {
  SingleLinkNode * trailer = 0;
  SingleLinkNode * leader = head;
  while (leader) {
   if (leader == item)  {
    if(trailer) trailer->next = leader->next;
    leader = leader->next;
    if(item == head) head = item->next;
    if(item == tail) tail = trailer;
    delete item;
   }
   else {
    if(trailer)
     trailer = trailer->NextNode();
    else trailer = leader;
    leader = leader->NextNode();
   }
  }
 }
}
