
//  This file forms part of the GOBLIN C++ Class Library.
//
//  Initially written by Birk Eisermann, August 2005
//
//  Copying, compiling, distribution and modification
//  of this source code is permitted only in accordance
//  with the GOBLIN general licence information.

/// \file   attributePool.h
/// \brief  #attributePool class interface and templates

#ifndef _ATTRIBUTE_POOL_H_
#define _ATTRIBUTE_POOL_H_

#include <list>

#include "attribute.h"
#include "fileImport.h"
#include "fileExport.h"


/// \addtogroup groupAttributes
/// @{

/// \brief A class for graph attribute management
///
/// The objects of this class are containers for #attribute objects.
/// The potentially contained attributes are specified by a #TPoolTable[]
/// array and a matching enum type.
///
/// Attribute pools manage the following:
/// -  Lookup and initialization of attribute values
/// -  Memory savings by support for constant attribute values
/// -  File import [export] from [to] tree like formats
/// -  Automatic shrinking and expansion when the graph is manipulated

class attributePool
{
public:

    /// \brief Specifies how many indices are represented by embedded attributes
    enum TAttributeType {
        ATTR_FULL_RANK = 0,      ///< \brief All referenced attributes have full rank
        ATTR_ALLOW_NULL = 1,     ///< \brief Referenced attributes have either full or zero rank
        ATTR_REQ_SINGLETON = 2   ///< \brief Referenced attributes have either full rank or rank 1
    };

protected:

    const TPoolTable*         table;
    const unsigned short      dim;
    list<attributeBase*>      attributes;
    list<TPoolEnum>           tokens;
    const TAttributeType      poolType;

    /// \brief Lookup for the specified attribute
    attributeBase* FindAttribute(TPoolEnum token) const throw();

public:

    /// \brief Construct an attribute pool
    /// \param _table     A pool table
    /// \param _dim       The number of records in this table
    /// \param _poolType  Specifies if zero size attributes are allowed
    attributePool(const TPoolTable* _table,TPoolEnum _dim,TAttributeType _poolType) throw();

    /// \brief Copy constructor
    attributePool(const attributePool& P) throw();

    /// \brief Destruct an attribute pool
    ~attributePool() throw();

    size_t Size() const throw() {return 0;};

    /// \brief Delete all attributes from this pool
    void Flush() throw();

    /// \brief Read pool data as a part of the graph object file representation
    TPoolEnum ReadAttribute(goblinRootObject&,goblinImport& F) throw();

    /// \brief Read pool data as a part of the graph object file representation
    void ReadPool(goblinRootObject& F,goblinImport&) throw();

    /// \brief Read a string attribute as a part of the graph object file representation
    bool ReadStringAttribute(goblinImport& F,TPoolEnum token) throw(ERParse);

    /// \brief Write an attribute to s file
    void WriteAttribute(const goblinRootObject&,goblinExport& F,TPoolEnum token,attributeBase* _attribute=NULL) const throw();

    /// \brief Write all pool data to file
    void WritePool(const goblinRootObject&,goblinExport& F,const char*) const throw();

    /// \brief Copy all attributes into a specified attribute pool
    ///
    /// But only those attribute which are not associated with graph nodes,
    /// layout nodes or graph arcs.
    void ExportAttributes(attributePool& P) const throw();

    /// \brief Copy an attribute into this attribute pool
    template <typename T> void ImportAttribute(attribute<T>& A,TPoolEnum token) throw();

    /// \brief Assign an attribute with a given sequence of values
    template <typename T> attribute<T>* ImportArray(TPoolEnum token,const T* _value,size_t size) throw();

    /// \brief Allocate a new attribute and add it to the pool
    /// \param X              The data object to which the attributes belong
    /// \param _token         The index of the attribute in the pool table
    /// \param attributeType  Specifies the attribute rank
    /// \param _value_ptr     Either NULL or a pointer to the default value
    ///
    /// Other than the #InitAttribute() and #RawAttribute() methods which
    /// result in full rank attributes, this operation supports constant attributes
    template <typename T> attribute<T>* MakeAttribute(
        goblinRootObject& X,TPoolEnum _token,TAttributeType attributeType,void* _value_ptr = NULL) throw();

    /// \brief Remove an attribute from the pool and deallocate it
    void ReleaseAttribute(TPoolEnum token) throw();

    /// \brief Direct access to an attribute in this pool
    template <typename T> attribute<T>* GetAttribute(TPoolEnum token) const throw();

    /// \brief Export the vector encapsulated into a pool attribute
    template <typename T> vector<T>* GetVector(TPoolEnum token) const throw();

    /// \brief Export the array encapsulated into a pool attribute
    template <typename T> T* GetArray(TPoolEnum token) const throw();

    /// \brief Obtain a full rank attribute, initialized with a default value
    template <typename T> attribute<T>* InitAttribute(goblinRootObject& X,TPoolEnum token,T _value) throw();

    /// \brief Obtain a full rank vector, initialized with a default value
    template <typename T> vector<T>* InitVector(goblinRootObject& X,TPoolEnum token,T _value) throw();

    /// \brief Obtain a full rank array, initialized with a default value
    template <typename T> T* InitArray(goblinRootObject& X,TPoolEnum token,T _value) throw();

    /// \brief Obtain a full rank attribute without initialization
    template <typename T> attribute<T>* RawAttribute(goblinRootObject& X,TPoolEnum token) throw();

    /// \brief Obtain a full rank vector without initialization
    template <typename T> vector<T>* RawVector(goblinRootObject& X,TPoolEnum token) throw();

    /// \brief Obtain a full rank array without initialization
    template <typename T> T* RawArray(goblinRootObject& X,TPoolEnum token) throw();

    /// \brief Set the capacity of all attributes of the specified type
    void ReserveItems(TArrayDim attributeType,TIndex capacity) throw(ERRange);

    /// \brief Add numItems to all attributes of the specified type
    void AppendItems(TArrayDim attributeType,TIndex numItems) throw();

    /// \brief Delete numItems from all attributes of the specified type
    void EraseItems(TArrayDim attributeType,TIndex numItems) throw(ERRange);

    /// \brief Swap the specified items in all attributes of the specified type
    void SwapItems(TArrayDim attributeType,TIndex i1,TIndex i2) throw(ERRange);

    /// \brief Return the attribute value for a given index
    template <typename T> T GetValue(TPoolEnum token,TIndex i,T _default) const throw();

    /// \brief Return the default attribute value
    template <typename T> T DefaultValue(TPoolEnum token,T _default) const throw();

    /// \brief Return the minimum attribute value
    template <typename T> T MinValue(TPoolEnum token,T _default) const throw();

    /// \brief Return the maximum attribute value
    template <typename T> T MaxValue(TPoolEnum token,T _default) const throw();

    /// \brief Return an index at which the minimum attribute value is achieved
    template <typename T> TIndex MinIndex(TPoolEnum token) const throw();

    /// \brief Return an index at which the maximum attribute value is achieved
    template <typename T> TIndex MaxIndex(TPoolEnum token) const throw();

    /// \brief Check if attribute values coincide for all indices
    template <typename T> bool IsConstant(TPoolEnum token) const throw();

};

/// @}


template <typename T>
attribute<T>* attributePool::MakeAttribute(goblinRootObject& X,TPoolEnum _token,
    TAttributeType attributeType,void* _value_ptr) throw()
{
    // Handle synonymous tokens
    _token = TPoolEnum(table[_token].primaryIndex);

    attribute<T>* pAttribute = GetAttribute<T>(_token);

    if (!_value_ptr) _value_ptr = DefaultValueAsVoidPtr(table[_token].arrayType);

    if (pAttribute)
    {
        if (attributeType==ATTR_ALLOW_NULL)
        {
            if (pAttribute->Size()>0)
            {
                pAttribute -> EraseItems(pAttribute->Size());
                pAttribute -> SetCapacity(0);
            }
        }
        else
        {
            size_t size = X.SizeInfo(table[_token].arrayDim,SIZE_ACTUAL);
            size_t capacity = X.SizeInfo(table[_token].arrayDim,SIZE_RESERVED);

            if (attributeType==ATTR_REQ_SINGLETON && size==0)
            {
                if (pAttribute->Size()>1)
                {
                    pAttribute -> EraseItems(pAttribute->Size()-1);
                    pAttribute -> SetCapacity(1);
                }
                else if (pAttribute->Size()==0)
                {
                    pAttribute -> IncreaseSize(1);
                }
            }
            else
            {
                if (pAttribute->Size()<size)
                {
                    pAttribute -> SetCapacity(capacity);
                    pAttribute -> IncreaseSize(size);
                }
                else if (pAttribute->Size()>size)
                {
                    pAttribute -> EraseItems(pAttribute->Size()-size);
                    pAttribute -> SetCapacity(capacity);
                }
            }
        }

        pAttribute -> SetDefaultValue(*reinterpret_cast<T*>(_value_ptr));

        return pAttribute;
    }

    if (attributeType==ATTR_ALLOW_NULL)
    {
        pAttribute = new attribute<T>(0,*reinterpret_cast<T*>(_value_ptr));
    }
    else
    {
        size_t size = X.SizeInfo(table[_token].arrayDim,SIZE_ACTUAL);
        size_t capacity = X.SizeInfo(table[_token].arrayDim,SIZE_RESERVED);

        if (capacity==0)
        {
            if (attributeType==ATTR_FULL_RANK)
            {
                return NULL;
            }
            else
            {
                // attributeType==ATTR_REQ_SINGLETON
                capacity = size = 1;
            }
        }

        pAttribute = new attribute<T>(size,*reinterpret_cast<T*>(_value_ptr));
        pAttribute -> SetCapacity(capacity);
    }

    attributes.insert(attributes.begin(),pAttribute);
    tokens.insert(tokens.begin(),(unsigned short)_token);

    return pAttribute;
}


template <typename T>
attribute<T>* attributePool::ImportArray(TPoolEnum token,const T* _value,size_t size)
    throw()
{
    attribute<T>* pAttribute = GetAttribute<T>(token);

    if (!pAttribute)
    {
        pAttribute = new attribute<T>(0,*reinterpret_cast<T*>(
                                        DefaultValueAsVoidPtr(table[token].arrayType)));

        attributes.insert(attributes.begin(),pAttribute);
        tokens.insert(tokens.begin(),(unsigned short)token);
    }

    size_t oldSize = pAttribute->Size();

    if (size>oldSize)
    {
        pAttribute -> IncreaseSize(size);
    }

    memcpy(pAttribute->GetArray(),_value,sizeof(T)*size);

    return pAttribute;
}


template <typename T>
attribute<T>* attributePool::GetAttribute(TPoolEnum token) const throw()
{
    attributeBase* _attribute = FindAttribute(token);

    if (!_attribute) return NULL;

    return static_cast<attribute<T>*>(_attribute);
}


template <typename T>
vector<T>* attributePool::GetVector(TPoolEnum token) const throw()
{
    attributeBase* _attribute = FindAttribute(token);

    if (!_attribute) return NULL;

    return static_cast<attribute<T>*>(_attribute)->GetVector();
}


template <typename T>
T* attributePool::GetArray(TPoolEnum token) const throw()
{
    if (poolType==ATTR_ALLOW_NULL) return NULL;

    attributeBase* _attributeBase = FindAttribute(token);

    if (!_attributeBase) return NULL;

    attribute<T>* _attribute = static_cast<attribute<T>*>(_attributeBase);

    if (_attribute->Size()==0) return NULL;

    return _attribute->GetArray();
}


template <typename T>
attribute<T>* attributePool::InitAttribute(goblinRootObject& X,TPoolEnum token,T _value) throw()
{
    attribute<T>* _attribute = GetAttribute<T>(token);

    if (!_attribute)
    {
        return MakeAttribute<T>(X,token,ATTR_FULL_RANK,
                                reinterpret_cast<void*>(&_value));
    }

    _attribute -> Assign(_value);

    return _attribute;
}


template <typename T>
vector<T>* attributePool::InitVector(goblinRootObject& X,TPoolEnum token,T _value) throw()
{
    return InitAttribute(X,token,_value)->GetVector();
}


template <typename T>
T* attributePool::InitArray(goblinRootObject& X,TPoolEnum token,T _value) throw()
{
    vector<T>* _vector = InitVector(X,token,_value);

    if (_vector->size()==0) return NULL;

    return &((*_vector)[0]);
}


template <typename T>
attribute<T>* attributePool::RawAttribute(goblinRootObject& X,TPoolEnum token) throw()
{
    if (poolType==ATTR_FULL_RANK)
    {
        attribute<T>* _attribute = GetAttribute<T>(token);

        if (_attribute) return _attribute;
    }

    return MakeAttribute<T>(X,token,ATTR_FULL_RANK);
}


template <typename T> vector<T>* attributePool::RawVector(goblinRootObject& X,TPoolEnum token) throw()
{
    return RawAttribute<T>(X,token)->GetVector();
}


template <typename T>
T* attributePool::RawArray(goblinRootObject& X,TPoolEnum token) throw()
{
    attribute<T>* _attribute = RawAttribute<T>(X,token);

    if (_attribute->Size()==0) return NULL;

    return _attribute->GetArray();
}


template <typename T>
T attributePool::GetValue(TPoolEnum token,TIndex i,T _default) const throw()
{
    attribute<T>* _attribute = GetAttribute<T>(token);

    if (_attribute) return _attribute->GetValue(i);

    return _default;
}


template <typename T>
T attributePool::DefaultValue(TPoolEnum token,T _default) const throw()
{
    attribute<T>* _attribute = GetAttribute<T>(token);

    if (_attribute) return _attribute->DefaultValue();

    return _default;
}


template <typename T>
T attributePool::MinValue(TPoolEnum token,T _default) const throw()
{
    attribute<T>* _attribute = GetAttribute<T>(token);

    if (_attribute) return _attribute->MinValue();

    return _default;
}


template <typename T>
T attributePool::MaxValue(TPoolEnum token,T _default) const throw()
{
    attribute<T>* _attribute = GetAttribute<T>(token);

    if (_attribute) return _attribute->MaxValue();

    return _default;
}


template <typename T>
TIndex attributePool::MinIndex(TPoolEnum token) const throw()
{
    attribute<T>* _attribute = GetAttribute<T>(token);

    if (_attribute) return _attribute->MinIndex();

    return NoIndex;
}


template <typename T>
TIndex attributePool::MaxIndex(TPoolEnum token) const throw()
{
    attribute<T>* _attribute = GetAttribute<T>(token);

    if (_attribute) return _attribute->MaxIndex();

    return NoIndex;
}


template <typename T>
bool attributePool::IsConstant(TPoolEnum token) const throw()
{
    attribute<T>* _attribute = GetAttribute<T>(token);

    if (_attribute) return _attribute->IsConstant();

    return true;
}


#endif
