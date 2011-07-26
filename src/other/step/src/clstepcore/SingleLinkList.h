#ifndef singlelinklist_h
#define	singlelinklist_h

/*
* NIST STEP Core Class Library
* clstepcore/SingleLinkList.h
* April 1997
* David Sauder
* KC Morris

* Development of this software was funded by the United States Government,
* and is not subject to copyright.
*/

/* $Id: SingleLinkList.h,v 3.0.1.4 1997/11/05 21:59:22 sauderd DP3.1 $ */

#ifdef __O3DB__
#include <OpenOODB.h>
#endif

class SingleLinkList  {

    // node which represents the value is contained in the subclass
	//  since it may have different types for different lists
    
  protected:
    
    class  SingleLinkNode *  head;
    SingleLinkNode *  tail;

  public:
    
    virtual SingleLinkNode *NewNode();
    virtual void AppendNode (SingleLinkNode *);
    virtual void DeleteNode (SingleLinkNode *);

    virtual void Empty ();
    virtual void DeleteFollowingNodes (SingleLinkNode *);
    virtual SingleLinkNode * GetHead () const;
    
    int EntryCount() const;

    SingleLinkList ();
    virtual ~SingleLinkList ();

}
;


class SingleLinkNode {
    friend class SingleLinkList;
  protected:

  public:
    SingleLinkList *owner;
    SingleLinkNode *next;

    virtual SingleLinkNode *NextNode () const;
    SingleLinkNode() : owner(0), next(0)  { }
    virtual ~SingleLinkNode() { }

};

#endif
