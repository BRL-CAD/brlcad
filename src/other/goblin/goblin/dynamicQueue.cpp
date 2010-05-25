
//  This file forms part of the GOBLIN C++ Class Library.
//
//  Initially written by Christian Fremuth-Paeger, September 2001
//
//  Copying, compiling, distribution and modification
//  of this source code is permitted only in accordance
//  with the GOBLIN general licence information.

/// \file   dynamicQueue.cpp
/// \brief  #dynamicQueue class implementation

#include "dynamicQueue.h"


template <class TItem,class TKey>
dynamicQueue<TItem,TKey>::dynamicQueue(TItem nn,goblinController &thisContext)
    throw() : managedObject(thisContext)
{
    n = nn;
    first = NULL;
    length = 0;

    this -> LogEntry(LOG_MEM,"...Dynamic queue instanciated");
}


template <class TItem,class TKey>
unsigned long dynamicQueue<TItem,TKey>::Size() const throw()
{
    return
          sizeof(dynamicQueue<TItem,TKey>)
        + managedObject::Allocated()
        + dynamicQueue::Allocated();
}


template <class TItem,class TKey>
unsigned long dynamicQueue<TItem,TKey>::Allocated() const throw()
{
    return length*sizeof(queueMember);
}


template <class TItem,class TKey>
char* dynamicQueue<TItem,TKey>::Display() const throw()
{
    this -> LogEntry(MSG_TRACE,"Queue");

    if (Empty()) this -> LogEntry(MSG_TRACE2,"    ---");
    else
    {
        queueMember *temp;
        TItem counter = 0;

        THandle LH = this->LogStart(MSG_TRACE2,"   ");

        for (temp=first; temp->next!=NULL; temp=temp->next)
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

        sprintf(this->CT.logBuffer,"%lu (last in)",static_cast<unsigned long>(temp->index));
        this -> LogEnd(LH,this->CT.logBuffer);
    }

    return NULL;
}


template <class TItem,class TKey>
dynamicQueue<TItem,TKey>::~dynamicQueue() throw()
{
    while (!Empty()) Delete();

    this -> LogEntry(LOG_MEM,"...Dynamic queue disallocated");
}


template <class TItem,class TKey>
void dynamicQueue<TItem,TKey>::Insert(TItem w,TKey alpha) throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (w>=n) NoSuchItem("Insert",w);

    #endif

    queueMember* temp = new queueMember;
    temp -> index = w;
    temp -> next = NULL;

    if (first==NULL)
    {
        first = temp;
        last = temp;
    }
    else
    {
        last -> next = temp;
        last = temp;
    }

    length++;
}


template <class TItem,class TKey>
TItem dynamicQueue<TItem,TKey>::Delete() throw(ERRejected)
{
    #if defined(_FAILSAVE_)

    if (Empty()) this -> Error(ERR_REJECTED,"Delete","Queue is empty");

    #endif

    queueMember *temp = first->next;
    TItem w = first->index;
    delete first;
    first = temp;
    length--;
    return w;
}


template <class TItem,class TKey>
void dynamicQueue<TItem,TKey>::ChangeKey(TItem w,TKey alpha) throw(ERRange,ERRejected)
{
}


template <class TItem,class TKey>
TItem dynamicQueue<TItem,TKey>::Peek() const throw(ERRejected)
{
    #if defined(_FAILSAVE_)

    if (Empty()) this -> Error(ERR_REJECTED,"Peek","Queue is empty");

    #endif

    return first->index;
}


template <class TItem,class TKey>
bool dynamicQueue<TItem,TKey>::Empty() const throw()
{
    return (first==NULL);;
}


template <class TItem,class TKey>
void dynamicQueue<TItem,TKey>::Init() throw()
{
    while (!Empty()) Delete();
}


template <class TItem,class TKey>
TItem dynamicQueue<TItem,TKey>::Cardinality() const throw()
{
    return length;
}


template class dynamicQueue<unsigned short,TFloat>;
template class dynamicQueue<unsigned long,TFloat>;

//  This file forms part of the GOBLIN C++ Class Library.
//
//  Initially written by Christian Fremuth-Paeger, September 2001
//
//  Copying, compiling, distribution and modification
//  of this source code is permitted only in accordance
//  with the GOBLIN general licence information.

/// \file   dynamicQueue.cpp
/// \brief  #dynamicQueue class implementation

#include "dynamicQueue.h"


template <class TItem,class TKey>
dynamicQueue<TItem,TKey>::dynamicQueue(TItem nn,goblinController &thisContext)
    throw() : managedObject(thisContext)
{
    n = nn;
    first = NULL;
    length = 0;

    this -> LogEntry(LOG_MEM,"...Dynamic queue instanciated");
}


template <class TItem,class TKey>
unsigned long dynamicQueue<TItem,TKey>::Size() const throw()
{
    return
          sizeof(dynamicQueue<TItem,TKey>)
        + managedObject::Allocated()
        + dynamicQueue::Allocated();
}


template <class TItem,class TKey>
unsigned long dynamicQueue<TItem,TKey>::Allocated() const throw()
{
    return length*sizeof(queueMember);
}


template <class TItem,class TKey>
char* dynamicQueue<TItem,TKey>::Display() const throw()
{
    this -> LogEntry(MSG_TRACE,"Queue");

    if (Empty()) this -> LogEntry(MSG_TRACE2,"    ---");
    else
    {
        queueMember *temp;
        TItem counter = 0;

        THandle LH = this->LogStart(MSG_TRACE2,"   ");

        for (temp=first; temp->next!=NULL; temp=temp->next)
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

        sprintf(this->CT.logBuffer,"%lu (last in)",static_cast<unsigned long>(temp->index));
        this -> LogEnd(LH,this->CT.logBuffer);
    }

    return NULL;
}


template <class TItem,class TKey>
dynamicQueue<TItem,TKey>::~dynamicQueue() throw()
{
    while (!Empty()) Delete();

    this -> LogEntry(LOG_MEM,"...Dynamic queue disallocated");
}


template <class TItem,class TKey>
void dynamicQueue<TItem,TKey>::Insert(TItem w,TKey alpha) throw(ERRange)
{
    #if defined(_FAILSAVE_)

    if (w>=n) NoSuchItem("Insert",w);

    #endif

    queueMember* temp = new queueMember;
    temp -> index = w;
    temp -> next = NULL;

    if (first==NULL)
    {
        first = temp;
        last = temp;
    }
    else
    {
        last -> next = temp;
        last = temp;
    }

    length++;
}


template <class TItem,class TKey>
TItem dynamicQueue<TItem,TKey>::Delete() throw(ERRejected)
{
    #if defined(_FAILSAVE_)

    if (Empty()) this -> Error(ERR_REJECTED,"Delete","Queue is empty");

    #endif

    queueMember *temp = first->next;
    TItem w = first->index;
    delete first;
    first = temp;
    length--;
    return w;
}


template <class TItem,class TKey>
void dynamicQueue<TItem,TKey>::ChangeKey(TItem w,TKey alpha) throw(ERRange,ERRejected)
{
}


template <class TItem,class TKey>
TItem dynamicQueue<TItem,TKey>::Peek() const throw(ERRejected)
{
    #if defined(_FAILSAVE_)

    if (Empty()) this -> Error(ERR_REJECTED,"Peek","Queue is empty");

    #endif

    return first->index;
}


template <class TItem,class TKey>
bool dynamicQueue<TItem,TKey>::Empty() const throw()
{
    return (first==NULL);;
}


template <class TItem,class TKey>
void dynamicQueue<TItem,TKey>::Init() throw()
{
    while (!Empty()) Delete();
}


template <class TItem,class TKey>
TItem dynamicQueue<TItem,TKey>::Cardinality() const throw()
{
    return length;
}


template class dynamicQueue<unsigned short,TFloat>;
template class dynamicQueue<unsigned long,TFloat>;
