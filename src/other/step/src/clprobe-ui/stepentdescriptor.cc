
/*
* NIST Data Probe Class Library
* clprobe-ui/stepentdescriptor.cc
* April 1997
* David Sauder

* Development of this software was funded by the United States Government,
* and is not subject to copyright.
*/

/* $Id: stepentdescriptor.cc,v 3.0.1.4 1997/11/05 23:01:12 sauderd DP3.1 $ */

#ifdef __O3DB__
#include <OpenOODB.h>
#endif
#include <stdlib.h>
#include <stdio.h>

#include <stepentdescriptor.h>
#include <Str.h>

/*
// in order to include xcanvas.h you will need to have a -I option 
// for /usr/local/include because <X11/Xlib.h> from /usr/local/include/
// gets included from InterViews/include/IV-X11/Xlib.h
#include <IV-X11/xcanvas.h>
#include <InterViews/window.h>
// xcanvas.h and window.h were included so I could write the 
// function IsMapped()
*/
#include <InterViews/canvas.h>
#include <InterViews/window.h>

#include <IV-2_6/InterViews/adjuster.h>
#include <IV-2_6/InterViews/border.h>
#include <IV-2_6/InterViews/defs.h>
#include <IV-2_6/InterViews/frame.h>
#include <IV-2_6/InterViews/glue.h>
#include <IV-2_6/InterViews/interactor.h>
#include <IV-2_6/InterViews/scroller.h>
#include <IV-2_6/InterViews/world.h>
#include <mymessage.h>

#include <scl_hash.h>
#include <probe.h>

extern struct Hash_Table *EntityDictionary;
extern struct Hash_Table *TypeDictionary;

extern Probe *dp;

/*
StepEntityDescriptor *GetStepEntityDescriptor(char *e)
{

//    EntityDescriptor & entDesc = 
//	(EntityDescriptor *)HASHfind(EntityDictionary, StrToUpper(e));
//    return new StepEntityDescriptor(entDesc);
}
*/

/*
Interactor *InsertInteractor (Interactor * i, Interactor * i2, Alignment align)
{
    extern World* dp_world;

    Frame* framedInteractor = new Frame(i, 2);
    Coord x, y;

//    if(i2->IsMapped()){
	i2->Align(BottomCenter, i->GetShape()->width, i->GetShape()->height, x, y);
	i2->GetRelative(x, y, dp_world);
	dp_world->InsertTransient(framedInteractor, i2, x, y, align);
//    } else {
//	x = y = 0;
//	dp_world->InsertTransient(framedInteractor, i2, x, y, align);
//    }
    return(framedInteractor);
}

	//////////////////////////////////

*/
void RemoveInteractor (Interactor* i, Interactor *i2)
{
    Frame* framedInteractor = (Frame*) i->Parent();

    i2->GetWorld()->Remove(framedInteractor);
    framedInteractor->Remove(i);
    delete framedInteractor;
}

//////////////////////////////////////////////////////////////////////////

Interactor* AddRightScroller (Interactor* grid)
{
  return new HBox (
              new MarginFrame (grid, 10),
              new VBorder,
              new VBox (
                   new UpMover (grid, 1),
                   new HBorder,
                   new VScroller (grid),
                   new HBorder,
                   new DownMover (grid, 1)
              )
         );
}

EntityDescriptorBlock::EntityDescriptorBlock (const EntityDescriptor *e, 
					     int maxAttrLen, int maxSubtypeLen)
{
    Init(e, 0, maxAttrLen, maxSubtypeLen);
    MonoScene::Insert(Body());
}

EntityDescriptorBlock::EntityDescriptorBlock (const EntityDescriptor *e, 
					     ButtonState *typeMessage,
					     int maxAttrLen, int maxSubtypeLen)
{
    Init (e, typeMessage, maxAttrLen, maxSubtypeLen);
    MonoScene::Insert(Body());
}

void EntityDescriptorBlock::Init(const EntityDescriptor *e, ButtonState *typeMessage,
				 int maxAttrLen, int maxSubtypeLen)
{
      //////////////////////////////////////////////
      // initialize the member variables
      //////////////////////////////////////////////
    ed = e;
    typeMsg = typeMessage;
    name = new BoldMessage((const char *)(ed->Name()));
    
    attrBS = new ButtonState(0);
    attrBS->Attach(this);
    attrList = &( ((EntityDescriptor *)ed)->ExplicitAttr());

    subtypeBS = new ButtonState;
    subtypeBS->Attach(this);
    subtypeList = &( ((EntityDescriptor *)ed)->Subtypes());

    // lastCloseVal needs to be set the same as the ButtonState's orig. value
    lastCloseVal = 2;
    openCloseBS = new ButtonState(2);
    openCloseBS->Attach(this);
    openCloseBut = new CheckBox("collapse", openCloseBS, 1, 2);

      //////////////////////////////////////////////
      // find out how many attrs there are
      //////////////////////////////////////////////
    int attrCount = attrList->EntryCount();
    if(attrCount < 2) attrCount = 2;

      //////////////////////////////////////////////
      // find out how many subtypes there are
      //////////////////////////////////////////////
    int subtypeCount = subtypeList->EntryCount();

    // determine the height of the box as the highest of the attribute 
    // box or subtype box.
    int entryCount = max(attrCount, subtypeCount);
//////////////////
    
    std::string str;
    char endchar;

    attrListBrowser = new StringBrowser(attrBS, entryCount, maxAttrLen);
    AttrDescLinkNode *attrPtr = (AttrDescLinkNode *)attrList->GetHead();
    while( attrPtr != 0)
    {
	endchar = '\0';
	str.set_null();

	const AttrDescriptor *ad = attrPtr->AttrDesc();

	if( ad->Derived() == LTrue )
	{
	    str.Append('<');
	    endchar = '>';
	}
/* // for when inverse information is included
	else if( ad->Inverse() == OPLOG(LTrue) )
	{
	    str.Append('(');
	    endchar = ')';
	}
*/
	std::string tmp;
	str.Append( ad->AttrExprDefStr(tmp) );
/* // built by hand
	str.Append( ad->Name() );
	str.Append( " : " );
	if( ad->Optional() == OPLOG(LTrue) )
	{
	    str.Append("OPTIONAL ");
	}
	str.Append( attrPtr->AttrDesc()->DomainType()->AttrTypeName() );
*/
	str.Append( endchar );

////////////////////
	attrListBrowser->Append(str.chars());

/*
	attrDefinition = str.chars();
	const char *attrDefPtr = attrDefinition;

	char attrStr[maxAttrLen + 1];
	attrStr[0] = 0;

	strncpy(attrStr, attrDefPtr, maxAttrLen);
	attrStr[maxAttrLen] = 0;
	attrListBrowser->Append(attrStr);
	
	// if the attr def was longer than maxAttrLen continue the definition
	// on lines following starting 5 spaces in.
	attrDefPtr = &attrDefPtr[maxAttrLen];
	while(strlen(attrDefPtr) > 0)
	{
	    memset(attrStr, ' ', 5);
	    memset(attrStr + 5, 0, maxAttrLen - 4);
	    strncat(attrStr, attrDefPtr, maxAttrLen - 5);
	    attrListBrowser->Append(attrStr);
	    attrDefPtr = &attrDefPtr[strlen(attrStr) - 5];
	}
*/

	attrPtr = (AttrDescLinkNode *)attrPtr->NextNode();
    }

//////////////
    subtypeListBrowser = new StringBrowser(subtypeBS, entryCount, maxSubtypeLen);
    EntityDescLinkNode *subtypePtr = 
				(EntityDescLinkNode *)subtypeList->GetHead();
    while( subtypePtr != 0)
    {
	subtypeListBrowser->Append(subtypePtr->EntityDesc()->Name());
	subtypePtr = (EntityDescLinkNode *)subtypePtr->NextNode();
    }
}

Interactor *EntityDescriptorBlock::Body()
{
//    Message *attrsAndSubtypesTitle = new Message("attributes & subtypes");
//    Message *attrsTitle = new Message("attributes");
//    Message *subtypesTitle = new Message("subtypes");
    attrSubtypeBox = new HBox(
		 AddRightScroller(attrListBrowser),
		 new VBorder,
		 AddRightScroller(subtypeListBrowser)
	);
    
    char schemaStr[BUFSIZ];
    schemaStr[0] = '\0';
    const char *schemaStrPtr = ed->OriginatingSchema()->Name();
    if(schemaStrPtr)
	strcpy(schemaStr, schemaStrPtr);
    else
	strcpy(schemaStr, " ");
    MyMessage *schemaMessage = new MyMessage(schemaStr);
    
    bodyBox = new VBox(
	    new HBox(
		     name,
//		     new HGlue(round(.02*inch), round(.02*inch), 0),
//		     attrsAndSubtypesTitle,
		     new HBox(
			      new HGlue(round(.5*inch), round(.5*inch), hfil),
			      schemaMessage,
			      new HGlue(round(.2*inch), round(.2*inch), 0),
			      openCloseBut
			),
		     new HGlue(round(.02*inch), round(.02*inch), 0)
//		     new HGlue(round(.5*inch), round(.5*inch), hfil)
	    ),
	    new HBorder,
	    attrSubtypeBox
	);
    return bodyBox;
}

void EntityDescriptorBlock::Update()
{
    int val = 0;

    // check to see if the subtype ButtonState was set... i.e. 
    // that means an event happened saying the user wanted to pop up a SED
    subtypeBS->GetValue(val);
    if(val) // if the value is set (not zero) 
    {
	subtypeBS->SetValue(0); // reset the value to zero
	if(val == '\r')
	{
	    // get the string the user selected
	    int index = subtypeListBrowser->Selection();
	    char * entity = subtypeListBrowser->String(index);

	    // get the head of the subtype list
	    SingleLinkNode * temp = 
				((EntityDescriptor *)ed)->Subtypes().GetHead();

	    const EntityDescriptor * subtypeED; 
	    if (!temp) // set error -- no subtypes found
		;
	    else 
		subtypeED = ((EntityDescLinkNode *)temp) -> EntityDesc ();
	
	    // find the EntityDescriptor for the entity the user selected
	    while (temp && strcmp (subtypeED -> Name (), entity)) 
		// go thru subtypes until the right one is found
	    {
		temp =  temp -> NextNode ();
		subtypeED =  ((EntityDescLinkNode *)temp) -> EntityDesc ();
	    }

	    if (!temp) // set error -- subtype not found
		;
	    else 
		subtypeED = ((EntityDescLinkNode *)temp) -> EntityDesc ();
	      
	    dp->InsertSED(new StepEntityDescriptor (subtypeED));
	}
    }


    // check to see if the attribute list ButtonState was set... i.e. 
    // that means an event happened saying the user wanted info on an attribute
    attrBS->GetValue(val);
    if(val)
    {
	attrBS->SetValue(0);
	if(val == '\r')
	{
	    int index = attrListBrowser->Selection();

	    AttrDescLinkNode *attrPtr = (AttrDescLinkNode *)
		( ((EntityDescriptor *)ed)->ExplicitAttr().GetHead());

	    char attrStr[BUFSIZ];
	    attrStr[0] = 0;
	    while( attrPtr != 0 && index > 0)
	    {
		index--;
		attrPtr = (AttrDescLinkNode *)attrPtr->NextNode();
	    }
	    std::string tmp;
//	    attrStr = attrPtr->AttrDesc()->AttrExprDefStr(tmp);

//	    attrStr = TypeString(attrPtr->AttrDesc());
	    std::string tmp2;
	    strncpy(attrStr,
		    attrPtr->AttrDesc()->DomainType()->TypeString(tmp2),
		    BUFSIZ);
	    attrStr[BUFSIZ-1] = 0;

//	    AttrDescriptorList *attrDescList = ed->ExplicitAttr();
//	    AttrDescriptor *attrDesc = attrDescList->GetHead();

//	    char * attrStr = attrListBrowser->String(index);
	    typeMsg->SetValue((char *)attrStr);
	    cout << "Selected attr: '" << attrStr << "'\n";
	}
    }

    openCloseBS->GetValue(val);
//	openCloseBS->SetValue(0);
    if(val != lastCloseVal)
    {
	if(val == 1)
	{
	    bodyBox->Remove(attrSubtypeBox);
	    Change();
	}     
	else if(val == 2)
	{
	    bodyBox->Insert(attrSubtypeBox);
	    bodyBox->Change();
	    Change();
	}     
    }
    lastCloseVal = val;
}

///////////////////////////////////////////////////////////////////////////////

EntityDescBlockNode *
EntityDescBlockList::PrependNode (EntityDescriptorBlock * edb)
{ 
    EntityDescBlockNode *node = (EntityDescBlockNode *) NewNode();
    node->EDBlock(edb);
    if (head) {
	node->next = head;
	head = node;
    }
    else head = tail = node;

    return node;
}    

///////////////////////////////////////////////////////////////////////////////

/*
StepEntityDescriptor::StepEntityDescriptor()
{ 
    edbList = new EntityDescBlockList;
    Scene::Insert(new HBorder(2));
    entDesc = 0;

    closeBS = new ButtonState(0);
    closeBS->Attach(this);
    close = new PushButton("close", closeBS, 1);
    entDesc = ed;

}
*/

char * 
StepEntityDescriptor::LongestAttrInBlock(const EntityDescriptor *entity)
{
    int longestLen = 0;
    static char longestAttrStr[BUFSIZ];
    longestAttrStr[0] = 0;

    const char *attrStr;
    int attrLen;

    if(entity)
    {
	AttrDescLinkNode *attrPtr = 
	    (AttrDescLinkNode *)( ((EntityDescriptor *)entity)->ExplicitAttr().GetHead());
	std::string tmp;
	while( attrPtr != 0)
	{
	    attrStr = attrPtr->AttrDesc()->AttrExprDefStr(tmp);
	    attrLen = strlen(attrStr);
	    if( attrPtr->AttrDesc()->Derived() == LTrue )
	    {
		attrLen = attrLen + 2;
	    }
/*
	    // for when inverse information is else included
	    else if( attrPtr->AttrDesc()->Inverse() == OPLOG(LTrue) )
	    {
		attrLen = attrLen + 2;
	    }
*/

	    if(attrLen > longestLen)
	    {
		strcpy(longestAttrStr, attrStr);
		longestLen = attrLen;
	    }
	    attrPtr = (AttrDescLinkNode *)attrPtr->NextNode();
	}
    }
    return longestAttrStr;
}

char * 
StepEntityDescriptor::FindLongestAttribute()
{
    int longestLen = 0;
    static char longestAttrStr[BUFSIZ];
    longestAttrStr[0] = 0;

    const EntityDescriptor *ed = entDesc;

    strcpy(longestAttrStr, LongestAttrInBlock(entDesc));
    longestLen = strlen(longestAttrStr);

    char *attrStr = 0;
    int attrLen = 0;

    EntityDescLinkNode *edNode = 
	(EntityDescLinkNode *)( (entDesc->Supertypes()).GetHead() );
    while (edNode)
    {
	ed = edNode->EntityDesc();

	if(ed)
	{
	    attrStr = LongestAttrInBlock(ed);
	    attrLen = strlen(attrStr);
	    if(attrLen > longestLen)
	    {
		strcpy(longestAttrStr, attrStr);
		longestLen = attrLen;
	    }
	    edNode = (EntityDescLinkNode *)( (ed->Supertypes()).GetHead() );
	}
	else
	    edNode = 0;
    }
    return longestAttrStr;
}
//////////////////

char * 
StepEntityDescriptor::LongestSubtNameInBlock(const EntityDescriptor *entity)
{
    int longestLen = 0;
    static char longestSubtypeStr[BUFSIZ];
    longestSubtypeStr[0] = 0;

    const char *subtypeStr;
    int subtypeLen;

    if(entity)
    {
	EntityDescLinkNode *subtypePtr = 
		  (EntityDescLinkNode *)( ((EntityDescriptor *)entity)->Subtypes().GetHead());
	while( subtypePtr != 0)
	{
	    subtypeStr = subtypePtr->EntityDesc()->Name();
	    subtypeLen = strlen(subtypeStr);
	    if(subtypeLen > longestLen)
	    {
		strcpy(longestSubtypeStr, subtypeStr);
		longestLen = subtypeLen;
	    }
	    subtypePtr = (EntityDescLinkNode *)subtypePtr->NextNode();
	}
    }
    return longestSubtypeStr;
}

char * 
StepEntityDescriptor::FindLongestSubtName()
{
    int longestLen = 0;
    static char longestSubtypeStr[BUFSIZ];
    longestSubtypeStr[0] = 0;

    const EntityDescriptor *ed = entDesc;

    strcpy(longestSubtypeStr, LongestSubtNameInBlock(entDesc));
    longestLen = strlen(longestSubtypeStr);

    char *subtypeStr = 0;
    int subtypeLen = 0;

    EntityDescLinkNode *edNode = 
	(EntityDescLinkNode *)( (entDesc->Supertypes()).GetHead() );
    while (edNode)
    {
	ed = edNode->EntityDesc();

	subtypeStr = LongestSubtNameInBlock(ed);
	subtypeLen = strlen(subtypeStr);
	if(subtypeLen > longestLen)
	{
	    strcpy(longestSubtypeStr, subtypeStr);
	    longestLen = subtypeLen;
	}
	edNode = (EntityDescLinkNode *)( ( ((EntityDescriptor *)ed)->Supertypes()).GetHead() );
    }
    return longestSubtypeStr;
}
//////////////////

StepEntityDescriptor::StepEntityDescriptor(const EntityDescriptor *ed)
{ 
    entDesc = ed;
    edbList = new EntityDescBlockList;
    closeBS = new ButtonState(0);
    closeBS->Attach(this);
    close = new PushButton("close", closeBS, 1);

    Scene::Insert(
	new HBox(
		 new HGlue(round(.01*inch),round(.01*inch), 0),
		 close,
		 new HGlue(round(.3*inch),round(.3*inch), 0),
		 new Message("attributes"),
		 new HGlue(round(1*inch),round(1*inch), hfil),
		 new Message("subtypes"),
		 new HGlue(round(.5*inch),round(.5*inch), 0)
		)
	);
    

///////////////
    int maxAttrLen = strlen(FindLongestAttribute());
    int len = 5;
    maxAttrLen = max(maxAttrLen, len);

    int maxSubtypeLen = strlen(FindLongestSubtName());
    maxSubtypeLen = max(maxSubtypeLen, len);

    strDispBS = new ButtonState(0);
    strDispBS->Attach(this);
    strDisp = new StringDisplay(strDispBS, " ");
//////////////
    edbList->PrependNode(new EntityDescriptorBlock(ed, strDispBS, maxAttrLen, 
						   maxSubtypeLen));

    EntityDescLinkNode *parentNode = 
	(EntityDescLinkNode *)( ( ((EntityDescriptor *)ed)->Supertypes()).GetHead() );
    const EntityDescriptor *Parent;
    while (parentNode)
    {
	Parent = parentNode->EntityDesc();

	edbList->PrependNode(new EntityDescriptorBlock(Parent, strDispBS, 
						   maxAttrLen, maxSubtypeLen));
	parentNode =
		 (EntityDescLinkNode *)( ( ((EntityDescriptor *)Parent)->Supertypes()).GetHead() );
    }
    EntityDescBlockNode *edbNode;

    for(edbNode = (EntityDescBlockNode *)edbList->GetHead(); 
	edbNode;
	edbNode = (EntityDescBlockNode *)(edbNode->NextNode()))
    {
	Scene::Insert(new HBorder(2));
	Scene::Insert(edbNode->EDBlock());
    }
    Scene::Insert(new HBorder(2));
    Scene::Insert(
	new MarginFrame(
		new HBox(
			 new Message("Attr. Type: "),
			 new HGlue(round(.05*inch), round(.05*inch), 0),
			 new Frame(strDisp)
			), 2)
		 );
}

/*
StepEntityDescriptor::StepEntityDescriptor(Interactor *i) : (i)
{
    edbList = new EntityDescBlockList;
    Scene::Insert(new HBorder(2));
}
*/

void StepEntityDescriptor::Insert(Interactor *i) 
{
    Scene::Insert(i);
    Scene::Insert(new HBorder(2));
}

void StepEntityDescriptor::Insert(EntityDescriptorBlock *edb) 
{
    edbList->PrependNode(edb);
    
    Scene::Insert(edb);
    Scene::Insert(new HBorder(2));
}
void StepEntityDescriptor::Update()
{
    int val = 0;
    closeBS->GetValue(val);
    closeBS->SetValue(0);
    if (val == 1)
    {
	dp->RemoveSED(this);
//	RemoveInteractor (this, this);
	cout << "close entity display" << "\n";
    }
    char *strVal = 0;
//    strDispBS->GetValue(strVal); // this no longer works with Sun 
				   // C++ => error. Must assign as below DAS
    void * tmpVoidPtr = 0;
    strDispBS->GetValue(tmpVoidPtr); 
    strVal = (char *) tmpVoidPtr;

    strDispBS->SetValue(0);
    if(strVal)
	strDisp->Message(strVal);
}

// boolean i.e. true = 1 and false = 0
boolean StepEntityDescriptor::IsMapped()
{
    return (canvas != nil && canvas->status() == Canvas::mapped);
/* 
    if(canvas)
    {
	// this doesn't work
	int bool = canvas->window()->is_mapped();
	return bool;
    }
    else
	return false;
*/
}
