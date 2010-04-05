
//  This file forms part of the GOBLIN C++ Class Library.
//
//  Initially written by Birk Eisermann, August 2005
//
//  Copying, compiling, distribution and modification
//  of this source code is permitted only in accordance
//  with the GOBLIN general licence information.

/// \file attributePool.cpp
/// \brief #attributePool class implementation

#include "attributePool.h"


attributePool::attributePool(const TPoolTable* _table,TPoolEnum _dim,TAttributeType _poolType) throw() :
    table(_table), dim((unsigned short)_dim), attributes(), tokens(), poolType(_poolType)
{
}


attributePool::attributePool(const attributePool& P) throw() :
    table(P.table), dim(P.dim), attributes(), tokens(), poolType(ATTR_FULL_RANK)
{
    P.ExportAttributes(*this);
}


attributePool::~attributePool() throw()
{
    Flush();
}


void attributePool::Flush() throw()
{
    list<attributeBase*>::iterator I = attributes.begin();
    list<unsigned short>::iterator J = tokens.begin();

    while (I!=attributes.end())
    {
        #define _STATEMENT(TType) delete A;
        #include "switchBaseTypes.h"
        #undef _STATEMENT

        I++;
        J++;
    }

    attributes.erase(attributes.begin(),attributes.end());
    tokens.erase(tokens.begin(),tokens.end());
}


TPoolEnum attributePool::ReadAttribute(goblinRootObject& X,goblinImport& F) throw()
{
    char* label = F.Scan();

    if (strcmp(label,"")==0)
    {
        return TPoolEnum(dim);
    }

    TPoolEnum thisToken = TPoolEnum(0);

    while (strcmp(label,table[thisToken].tokenLabel)!=0)
    {
        thisToken = TPoolEnum(int(thisToken)+1);

        if (thisToken==TPoolEnum(dim))
        {
            while (!F.Tail()) label = F.Scan();

            return TPoolEnum(dim-1);
        }
    }

    // Handle synonymous tokens
    thisToken = TPoolEnum(table[thisToken].primaryIndex);

    switch (table[thisToken].arrayType)
    {
        case TYPE_NODE_INDEX:
        {
            attribute<TNode>* A = MakeAttribute<TNode>(X,thisToken,ATTR_REQ_SINGLETON);
            F.ReadTupleValues(table[thisToken].arrayType,A->Size(),A->GetArray());
            break;
        }
        case TYPE_ARC_INDEX:
        {
            attribute<TArc>* A = MakeAttribute<TArc>(X,thisToken,ATTR_REQ_SINGLETON);
            F.ReadTupleValues(table[thisToken].arrayType,A->Size(),A->GetArray());
            break;
        }
        case TYPE_FLOAT_VALUE:
        {
            attribute<TFloat>* A = MakeAttribute<TFloat>(X,thisToken,ATTR_REQ_SINGLETON);
            F.ReadTupleValues(table[thisToken].arrayType,A->Size(),A->GetArray());
            break;
        }
        case TYPE_CAP_VALUE:
        {
            attribute<TCap>* A = MakeAttribute<TCap>(X,thisToken,ATTR_REQ_SINGLETON);
            F.ReadTupleValues(table[thisToken].arrayType,A->Size(),A->GetArray());
            break;
        }
        case TYPE_INDEX:
        {
            attribute<TIndex>* A = MakeAttribute<TIndex>(X,thisToken,ATTR_REQ_SINGLETON);
            F.ReadTupleValues(table[thisToken].arrayType,A->Size(),A->GetArray());
            break;
        }
        case TYPE_ORIENTATION:
        {
            attribute<char>* A = MakeAttribute<char>(X,thisToken,ATTR_REQ_SINGLETON);
            F.ReadTupleValues(table[thisToken].arrayType,A->Size(),A->GetArray());
            break;
        }
        case TYPE_INT:
        {
            attribute<int>* A = MakeAttribute<int>(X,thisToken,ATTR_REQ_SINGLETON);
            F.ReadTupleValues(table[thisToken].arrayType,A->Size(),A->GetArray());
            break;
        }
        case TYPE_DOUBLE:
        {
            attribute<double>* A = MakeAttribute<double>(X,thisToken,ATTR_REQ_SINGLETON);
            F.ReadTupleValues(table[thisToken].arrayType,A->Size(),A->GetArray());
            break;
        }
        case TYPE_BOOL:
        {
            attribute<bool>* A = MakeAttribute<bool>(X,thisToken,ATTR_REQ_SINGLETON);
            F.ReadTupleValues(table[thisToken].arrayType,A->Size(),A->GetArray());
            break;
        }
        case TYPE_CHAR:
        {
            if (table[thisToken].arrayDim==DIM_STRING)
            {
                ReadStringAttribute(F,thisToken);
            }
            else
            {
                attribute<char>* A = MakeAttribute<char>(X,thisToken,ATTR_REQ_SINGLETON);
                F.ReadTupleValues(table[thisToken].arrayType,A->Size(),A->GetArray());
            }
            break;
        }
        case TYPE_VAR_INDEX:
        {
            attribute<TVar>* A = MakeAttribute<TVar>(X,thisToken,ATTR_REQ_SINGLETON);
            F.ReadTupleValues(table[thisToken].arrayType,A->Size(),A->GetArray());
            break;
        }
        case TYPE_RESTR_INDEX:
        {
            attribute<TRestr>* A = MakeAttribute<TRestr>(X,thisToken,ATTR_REQ_SINGLETON);
            F.ReadTupleValues(table[thisToken].arrayType,A->Size(),A->GetArray());
            break;
        }
        case TYPE_SPECIAL:
        {
            X.ReadSpecial(F,*this,thisToken);
            break;
        }
    }

    if (table[thisToken].arrayDim==DIM_STRING)
    {
        // Implement deletion of empty "" strings later
    }
    else if (   table[thisToken].arrayType!=TYPE_SPECIAL
             && table[thisToken].arrayDim!=DIM_SINGLETON
             && F.Constant()
            )
    {
        if (poolType==ATTR_FULL_RANK)
        {
            ReleaseAttribute(thisToken);
        }
        else
        {
            list<attributeBase*>::iterator I = attributes.begin();
            list<unsigned short>::iterator J = tokens.begin();

            while (I != attributes.end() && thisToken!=(*J)) {I++; J++;}

            #define _STATEMENT(TType) A -> SetConstant(A->GetValue(0));
            #include "switchBaseTypes.h"
            #undef _STATEMENT
        }
    }

    return thisToken;
}


void attributePool::ReadPool(goblinRootObject& X,goblinImport& F) throw()
{
    while ( ReadAttribute(X,F) != TPoolEnum(dim) ) {};
/*
    list<attributeBase*>::iterator I = attributes.begin();
    list<unsigned short>::iterator J = tokens.begin();

    while (I != attributes.end())
    {
        cout << "token = " << table[(*J)].tokenLabel << endl;

        #define _STATEMENT(TType) cout << "  size = " << A->Size() << endl;
        #include "switchBaseTypes.h"
        #undef _STATEMENT

        I++;
        J++;
    }
*/
}


bool attributePool::ReadStringAttribute(goblinImport& F,TPoolEnum token)
    throw(ERParse)
{
    size_t length = 0;

    while (!F.Tail())
    {
        char* label = F.Scan();

        if (strlen(label)>0 && length==0)
        {
            ImportArray<char>(token,label,strlen(label)+1);
        }

        ++length;
    }

    return (length==1);
}


void attributePool::WriteAttribute(const goblinRootObject& X,goblinExport& F,TPoolEnum token,
    attributeBase* _attribute) const throw()
{
    if (!_attribute) _attribute = FindAttribute(token);

    if (!_attribute) return;

    TBaseType attributeType = table[token].arrayType;
    const char* attributeLabel = table[token].tokenLabel;

    if (attributeType==TYPE_SPECIAL)
    {
        X.WriteSpecial(F,*this,token);
        return;
    }

    if (table[token].arrayDim==DIM_STRING)
    {
        if (attributeType==TYPE_CHAR)
        {
            attribute<char>* A = static_cast<attribute<char>*>(_attribute);
            char* charStr = A->GetArray();
            F.WriteAttribute<char*>(&charStr,attributeLabel,1,char(0));
        }

        return;
    }

    switch (attributeType)
    {
        case TYPE_NODE_INDEX:
        {
            attribute<TNode>* A = static_cast<attribute<TNode>*>(_attribute);
            F.WriteAttribute(A->GetArray(),attributeLabel,A->Size(),NoNode);
            break;
        }
        case TYPE_ARC_INDEX:
        {
            attribute<TArc>* A = static_cast<attribute<TArc>*>(_attribute);
            F.WriteAttribute(A->GetArray(),attributeLabel,A->Size(),NoArc);
            break;
        }
        case TYPE_FLOAT_VALUE:
        {
            attribute<TFloat>* A = static_cast<attribute<TFloat>*>(_attribute);
            F.WriteAttribute(A->GetArray(),attributeLabel,A->Size(),InfFloat);
            break;
        }
        case TYPE_CAP_VALUE:
        {
            attribute<TCap>* A = static_cast<attribute<TCap>*>(_attribute);
            F.WriteAttribute(A->GetArray(),attributeLabel,A->Size(),InfCap);
            break;
        }
        case TYPE_INDEX:
        {
            attribute<TIndex>* A = static_cast<attribute<TIndex>*>(_attribute);
            F.WriteAttribute(A->GetArray(),attributeLabel,A->Size(),NoIndex);
            break;
        }
        case TYPE_ORIENTATION:
        {
            attribute<char>* A = static_cast<attribute<char>*>(_attribute);
            F.WriteAttribute(A->GetArray(),attributeLabel,A->Size(),char(0));
            break;
        }
        case TYPE_INT:
        {
            attribute<int>* A = static_cast<attribute<int>*>(_attribute);
            F.WriteAttribute(A->GetArray(),attributeLabel,A->Size(),-1);
            break;
        }
        case TYPE_DOUBLE:
        {
            attribute<double>* A = static_cast<attribute<double>*>(_attribute);
            F.WriteAttribute(A->GetArray(),attributeLabel,A->Size(),double(0.0));
            break;
        }
        case TYPE_BOOL:
        {
            attribute<bool>* A = static_cast<attribute<bool>*>(_attribute);
            F.WriteAttribute(A->GetArray(),attributeLabel,A->Size(),false);
            break;
        }
        case TYPE_CHAR:
        {
            attribute<char>* A = static_cast<attribute<char>*>(_attribute);
            F.WriteAttribute(A->GetArray(),attributeLabel,A->Size(),char(0));
            break;
        }
        case TYPE_VAR_INDEX:
        {
            attribute<TVar>* A = static_cast<attribute<TVar>*>(_attribute);
            F.WriteAttribute(A->GetArray(),attributeLabel,A->Size(),NoVar);
            break;
        }
        case TYPE_RESTR_INDEX:
        {
            attribute<TRestr>* A = static_cast<attribute<TRestr>*>(_attribute);
            F.WriteAttribute(A->GetArray(),attributeLabel,A->Size(),NoRestr);
            break;
        }
        case TYPE_SPECIAL:
        {
            break;
        }
    }
}


void attributePool::WritePool(const goblinRootObject& X,goblinExport& F,const char* poolLabel) const throw()
{
    F.StartTuple(poolLabel,0);

    list<attributeBase*>::const_iterator I = attributes.begin();
    list<TPoolEnum>::const_iterator J = tokens.begin();

    while (I != attributes.end())
    {
        TPoolEnum token = *J;

        if (table[token].arrayType!=TYPE_SPECIAL)
        {
            WriteAttribute(X,F,token,*I);
        }

        I++;
        J++;
    }

    for (TPoolEnum token=0;token<dim;token++)
    {
        if (table[token].arrayType==TYPE_SPECIAL)
        {
            X.WriteSpecial(F,*this,token);
        }
    }

    F.EndTuple();
}


void attributePool::ExportAttributes(attributePool& P) const throw()
{
    list<attributeBase*>::const_iterator I = attributes.begin();
    list<unsigned short>::const_iterator J = tokens.begin();

    while (I != attributes.end())
    {
        // The following conditional prevents from copying
        // attributes associated with graph nodes and arcs.

        if (   table[(*J)].arrayDim == DIM_SINGLETON
            || table[(*J)].arrayDim == DIM_PAIR
            || table[(*J)].arrayDim == DIM_STRING
           )
        {
            #define _STATEMENT(TType) P.ImportAttribute<TType>(*A,*J);
            #include "switchBaseTypes.h"
            #undef _STATEMENT
        }

        I++;
        J++;
    }
}


template <typename T>
void attributePool::ImportAttribute(attribute<T>& A,TPoolEnum token) throw()
{
    attribute<T>* pAttribute = new attribute<T>(A);

    attributes.insert(attributes.begin(),pAttribute);
    tokens.insert(tokens.begin(),(unsigned short)token);
}


attributeBase* attributePool::FindAttribute(TPoolEnum token) const throw()
{
    // Handle synonymous tokens
    token = TPoolEnum(table[token].primaryIndex);

    list<attributeBase*>::const_iterator I = attributes.begin();
    list<TPoolEnum>::const_iterator J = tokens.begin();

    while (I != attributes.end() && token!=(*J)) {I++; J++;}

    if (I == attributes.end()) return NULL;

    return (*I);
}


void attributePool::ReleaseAttribute(TPoolEnum token) throw()
{
    // Handle synonymous tokens
    token = TPoolEnum(table[token].primaryIndex);

    list<attributeBase*>::iterator I = attributes.begin();
    list<unsigned short>::iterator J = tokens.begin();

    while (I != attributes.end() && token!=(*J)) {I++; J++;}

    if (I == attributes.end()) return;

    #define _STATEMENT(TType) delete A;
    #include "switchBaseTypes.h"
    #undef _STATEMENT

    attributes.erase(I);
    tokens.erase(J);
}


void attributePool::ReserveItems(TArrayDim attributeType,TIndex capacity) throw(ERRange)
{
    list<attributeBase*>::iterator I = attributes.begin();
    list<unsigned short>::iterator J = tokens.begin();

    while (I != attributes.end())
    {
        if (table[(*J)].arrayDim == attributeType)
        {
            #define _STATEMENT(TType) A -> ReserveItems(capacity);
            #include "switchBaseTypes.h"
            #undef _STATEMENT
        }

        I++;
        J++;
    }
}


void attributePool::AppendItems(TArrayDim attributeType,TIndex numItems) throw()
{
    list<attributeBase*>::iterator I = attributes.begin();
    list<unsigned short>::iterator J = tokens.begin();

    while (I != attributes.end())
    {
        if (table[(*J)].arrayDim == attributeType)
        {
            #define _STATEMENT(TType) A -> AppendItems(numItems);
            #include "switchBaseTypes.h"
            #undef _STATEMENT
        }

        I++;
        J++;
    }
}


void attributePool::EraseItems(TArrayDim attributeType,TIndex numItems) throw(ERRange)
{
    list<attributeBase*>::iterator I = attributes.begin();
    list<unsigned short>::iterator J = tokens.begin();

    while (I != attributes.end())
    {
        if (table[(*J)].arrayDim == attributeType)
        {
            #define _STATEMENT(TType) A -> EraseItems(numItems);
            #include "switchBaseTypes.h"
            #undef _STATEMENT
        }

        I++;
        J++;
    }
}


void attributePool::SwapItems(TArrayDim attributeType,TIndex i1,TIndex i2) throw(ERRange)
{
    list<attributeBase*>::iterator I = attributes.begin();
    list<unsigned short>::iterator J = tokens.begin();

    while (I != attributes.end())
    {
        if (table[(*J)].arrayDim == attributeType)
        {
            #define _STATEMENT(TType) A -> SwapItems(i1,i2);
            #include "switchBaseTypes.h"
            #undef _STATEMENT
        }

        I++;
        J++;
    }
}
