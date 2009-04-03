#ifndef stepentdescriptor_h
#define stepentdescriptor_h

/*
* NIST Data Probe Class Library
* clprobe-ui/stepentdescriptor.h
* April 1997
* David Sauder

* Development of this software was funded by the United States Government,
* and is not subject to copyright.
*/

/* $Id: stepentdescriptor.h,v 3.0.1.2 1997/11/05 23:01:11 sauderd DP3.1 $ */

#include <STEPattribute.h>
#include <SingleLinkList.h>
#include <ExpDict.h>
#include <variousmessage.h>

#include <IV-2_6/InterViews/box.h>
#include <IV-2_6/InterViews/button.h>
#include <IV-2_6/InterViews/interactor.h>
#include <IV-2_6/InterViews/scene.h>
#include <IV-2_6/InterViews/strbrowser.h>
#include <strdisplay.h>

#include <IV-2_6/_enter.h>

class EntityDescriptorBlock : public MonoScene
{
public:

protected:
    const EntityDescriptor *ed;

    ButtonState *typeMsg;

    BoldMessage *name;

    ButtonState *attrBS;
    StringBrowser *attrListBrowser;
    const AttrDescriptorList *attrList;
    
    ButtonState *subtypeBS;
    StringBrowser *subtypeListBrowser;
    const EntityDescriptorList *subtypeList;

    ButtonState *openCloseBS;
    CheckBox *openCloseBut;
    int lastCloseVal;

    VBox *bodyBox;
    HBox *attrSubtypeBox;

private:

public:
    EntityDescriptorBlock() { }
    EntityDescriptorBlock (const EntityDescriptor *e, 
			  int maxAttrLen = 30, int maxSubtypeLen = 30);
    EntityDescriptorBlock (const EntityDescriptor *e, ButtonState *typeMessage,
			  int maxAttrLen = 30, int maxSubtypeLen = 30);
    ~EntityDescriptorBlock() { }

    const EntityDescriptor *EntityDesc() const { return ed; }
//    void EntityDesc(EntityDescriptor *e) { ed = e; }

    virtual void Update();

protected:

private:
    Interactor *Body();
    void Init (const EntityDescriptor *e, ButtonState *typeMessage,
	      int maxAttrLen = 30, int maxSubtypeLen = 30);

};

class EntityDescBlockList;

class EntityDescBlockNode : public SingleLinkNode
{
    friend EntityDescBlockList;
    
  private:
  protected:
    EntityDescriptorBlock * edBlock;
  public:
    char *ClassName () { return "EntityDescBlockNode"; }

    EntityDescriptorBlock *EDBlock()         { return edBlock; }
    void EDBlock(EntityDescriptorBlock *edb) { edBlock = edb; }

    EntityDescBlockNode() { edBlock = 0; }
    EntityDescBlockNode(EntityDescriptorBlock *edb) { edBlock = edb; }
    ~EntityDescBlockNode() { }
};

class EntityDescBlockList : public SingleLinkList
{
  private:
  protected:
  public:
    EntityDescBlockList()  { }
    ~EntityDescBlockList() { } 
    char *ClassName () { return "EntityDescBlockList"; }

    virtual SingleLinkNode * NewNode () { return new EntityDescBlockNode; } 
    EntityDescBlockNode * PrependNode (EntityDescriptorBlock * edb);
    void PrependNode (EntityDescBlockNode * node)
    {
	if (head) {
	    node->next = head;
	    head = node;
	}
	else head = tail = node;
    }
};

class StepEntityDescriptor : public VBox
{
private:

protected:
    EntityDescBlockList *edbList;
    PushButton *close;
    ButtonState *closeBS;
    const EntityDescriptor *entDesc;

    StringDisplay *strDisp;
    ButtonState *strDispBS;

public:
    StepEntityDescriptor(const EntityDescriptor *ed);
    ~StepEntityDescriptor() { }
    
    const EntityDescriptor * EntityDesc() { return entDesc; }

    void Insert(EntityDescriptorBlock *edb);
    void Insert(Interactor *i);
    virtual void Update();

    char *LongestAttrInBlock (const EntityDescriptor *entity);
    char *FindLongestAttribute ();

    char *LongestSubtNameInBlock (const EntityDescriptor *entity);
    char *FindLongestSubtName ();

    boolean IsMapped ();

};

#include <IV-2_6/_leave.h>

#endif
