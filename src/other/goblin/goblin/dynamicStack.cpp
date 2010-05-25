
//  This file forms part of the GOBLIN C++ Class Library.
//
//  Initially written by Christian Fremuth-Paeger, September 1998
//
//  Copying, compiling, distribution and modification
//  of this source code is permitted only in accordance
//  with the GOBLIN general licence information.

/// \file   dynamicStack.cpp
/// \brief  #dynamicStack class implementation

#include "dynamicStack.h"


template <class TItem,class TKey>
dynamicStack<TItem,TKey>::dynamicStack(TItem nn,goblinController &thisContext)
    throw() : managedObject(thisContext)
{
    n = nn;
    top = NULL;
    depth = 0;

    this -> LogEntry(LOG_MEM,"...Dynamic stack instanciated");
}


template <class TItem,class TKey>
unsigned long dynamicStack<TItem,TKey>::Size() const throw()
{
    return
          sizeof(dynamicStack<TItem,TKey>)
        + managedObject::Allocated()
        + dynamicStack::Allocated();
}


template <class TItem,class TKey>
unsigned long dynamicStack<TItem,TKey>::Allocated() const throw()
{
    return depth*sizeof(stackMember);
}


template <class TItem,class TKey>
char* dynamicStack<TItem,TKey>::Display() const throw()
{
    this -> LogEntry(MSG_TRACE,"Stack");

    if (Empty()) this -> LogEntry(MSG_TRACE2,"    ---");
    else
    {
        stackMember *temp;
        TItem counter = 0;

        THandle LH = this->LogStart(MSG_TRACE2,"   ");

        for (temp=top; temp->prev!=NULL; temp=temp->prev)
        {
            if (counter>0 && counter%10==0)
            {
                this -> LogEnd(LH);
                LH = this->LogStart(MSG_TRACE2,"   ");
            }

            sprintf(this->CT.logBuffer,"%lu, ",static_cast<unsigned long>(temp->index));
            this -> LogAppend(LH,this->CT.logBuffer);

            counter++;
        }

        if (counter>0 && counter%10==0)
        {
            this -> LogEnd(LH);
            LH = this->LogStart(MSG_TRACE2,"   ");
        }

        sprintf(this->CT.logBuffer,"%lu (bottom)",static_cast<unsigned long>(temp->index));
        this -> LogEnd(LH,this->CT.logBuffer);
    }

    return NULL;
}


template <class TItem,class TKey>
dynamicStack<TItem,TKey>::~dynamicStack() throw()
{
    while (!Empty()) Delete();

    this -> LogEntry(LOG_MEM,"...Dynamic stack disallocated");
}


template <class TItem,class TKey>
void dynamicStack<TItem,TKey>::Insert(TItem w,TKey alpha) throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (w>=n) NoSuchItem("Insert",w);

    #endif

    stackMember *temp = new stackMember;
    temp -> prev = top;
    temp -> index = w;
    top = temp;
    depth++;
}


template <class TItem,class TKey>
TItem dynamicStack<TItem,TKey>::Delete() throw(ERRejected)
{
    #if defined(_FAILSAVE_)

    if (Empty()) this -> Error(ERR_REJECTED,"Delete","Queue is empty");

    #endif

    stackMember *temp = top->prev;
    TItem w = top->index;
    delete top;
    top = temp;
    depth--;
    return w;
}


template <class TItem,class TKey>
void dynamicStack<TItem,TKey>::ChangeKey(TItem w,TKey alpha) throw(ERRange,ERRejected)
{
}


template <class TItem,class TKey>
TItem dynamicStack<TItem,TKey>::Peek() const throw(ERRejected)
{
    #if defined(_FAILSAVE_)

    if (Empty()) this -> Error(ERR_REJECTED,"Peek","Queue is empty");

    #endif

    return top->index;
}


template <class TItem,class TKey>
bool dynamicStack<TItem,TKey>::Empty() const throw()
{
    return (top==NULL);
}


template <class TItem,class TKey>
void dynamicStack<TItem,TKey>::Init() throw()
{
    while (!Empty()) Delete();
}


template <class TItem,class TKey>
TItem dynamicStack<TItem,TKey>::Cardinality() const throw()
{
    return depth;
}


template class dynamicStack<unsigned short,TFloat>;
template class dynamicStack<unsigned long,TFloat>;

//  This file forms part of the GOBLIN C++ Class Library.
//
//  Initially written by Christian Fremuth-Paeger, September 1998
//
//  Copying, compiling, distribution and modification
//  of this source code is permitted only in accordance
//  with the GOBLIN general licence information.

/// \file   dynamicStack.cpp
/// \brief  #dynamicStack class implementation

#include "dynamicStack.h"


template <class TItem,class TKey>
dynamicStack<TItem,TKey>::dynamicStack(TItem nn,goblinController &thisContext)
    throw() : managedObject(thisContext)
{
    n = nn;
    top = NULL;
    depth = 0;

    this -> LogEntry(LOG_MEM,"...Dynamic stack instanciated");
}


template <class TItem,class TKey>
unsigned long dynamicStack<TItem,TKey>::Size() const throw()
{
    return
          sizeof(dynamicStack<TItem,TKey>)
        + managedObject::Allocated()
        + dynamicStack::Allocated();
}


template <class TItem,class TKey>
unsigned long dynamicStack<TItem,TKey>::Allocated() const throw()
{
    return depth*sizeof(stackMember);
}


template <class TItem,class TKey>
char* dynamicStack<TItem,TKey>::Display() const throw()
{
    this -> LogEntry(MSG_TRACE,"Stack");

    if (Empty()) this -> LogEntry(MSG_TRACE2,"    ---");
    else
    {
        stackMember *temp;
        TItem counter = 0;

        THandle LH = this->LogStart(MSG_TRACE2,"   ");

        for (temp=top; temp->prev!=NULL; temp=temp->prev)
        {
            if (counter>0 && counter%10==0)
            {
                this -> LogEnd(LH);
                LH = this->LogStart(MSG_TRACE2,"   ");
            }

            sprintf(this->CT.logBuffer,"%lu, ",static_cast<unsigned long>(temp->index));
            this -> LogAppend(LH,this->CT.logBuffer);

            counter++;
        }

        if (counter>0 && counter%10==0)
        {
            this -> LogEnd(LH);
            LH = this->LogStart(MSG_TRACE2,"   ");
        }

        sprintf(this->CT.logBuffer,"%lu (bottom)",static_cast<unsigned long>(temp->index));
        this -> LogEnd(LH,this->CT.logBuffer);
    }

    return NULL;
}


template <class TItem,class TKey>
dynamicStack<TItem,TKey>::~dynamicStack() throw()
{
    while (!Empty()) Delete();

    this -> LogEntry(LOG_MEM,"...Dynamic stack disallocated");
}


template <class TItem,class TKey>
void dynamicStack<TItem,TKey>::Insert(TItem w,TKey alpha) throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (w>=n) NoSuchItem("Insert",w);

    #endif

    stackMember *temp = new stackMember;
    temp -> prev = top;
    temp -> index = w;
    top = temp;
    depth++;
}


template <class TItem,class TKey>
TItem dynamicStack<TItem,TKey>::Delete() throw(ERRejected)
{
    #if defined(_FAILSAVE_)

    if (Empty()) this -> Error(ERR_REJECTED,"Delete","Queue is empty");

    #endif

    stackMember *temp = top->prev;
    TItem w = top->index;
    delete top;
    top = temp;
    depth--;
    return w;
}


template <class TItem,class TKey>
void dynamicStack<TItem,TKey>::ChangeKey(TItem w,TKey alpha) throw(ERRange,ERRejected)
{
}


template <class TItem,class TKey>
TItem dynamicStack<TItem,TKey>::Peek() const throw(ERRejected)
{
    #if defined(_FAILSAVE_)

    if (Empty()) this -> Error(ERR_REJECTED,"Peek","Queue is empty");

    #endif

    return top->index;
}


template <class TItem,class TKey>
bool dynamicStack<TItem,TKey>::Empty() const throw()
{
    return (top==NULL);
}


template <class TItem,class TKey>
void dynamicStack<TItem,TKey>::Init() throw()
{
    while (!Empty()) Delete();
}


template <class TItem,class TKey>
TItem dynamicStack<TItem,TKey>::Cardinality() const throw()
{
    return depth;
}


template class dynamicStack<unsigned short,TFloat>;
template class dynamicStack<unsigned long,TFloat>;
