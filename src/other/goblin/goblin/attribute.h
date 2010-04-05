
//  This file forms part of the GOBLIN C++ Class Library.
//
//  Initially written by Birk Eisermann, August 2005
//
//  Copying, compiling, distribution and modification
//  of this source code is permitted only in accordance
//  with the GOBLIN general licence information.

/// \file attribute.h
/// \brief #attribute class interface and templates

#ifndef _ATTRIBUTE_H_
#define _ATTRIBUTE_H_

#include <vector>

#include "globals.h"
#include "goblinController.h"

using namespace std;


/// \addtogroup groupAttributes
/// @{

/// \brief  Non-template base class for attribute templates
class attributeBase {};

/// \brief  Class for storing array/vector like data assigned with graph objects
///
/// This class extends the STL vector functionality as follows:
/// -  A default value applies to indices not represented by the vector
/// -  Lazy computation of minimum and maximum values
/// -  Update operations to support the insertion and deletion of graph nodes and arcs

template<typename T>
class attribute : public attributeBase
{
private:

    /// \brief  A vector to hold the attribute values
    vector<T> data;

    /// \brief  A default attribute value
    T defaultValue;

    /// \brief  The minimum index which achieves the minimal value
    TIndex infimum;

    /// \brief  The maximum index which achieves the maximal value
    TIndex supremum;

public:

    /// \brief  Create a new attribute
    /// \param _size          The initial size of the vector representation
    /// \param _defaultValue  The value assigned with non-represented indices
    attribute(TIndex _size,const T& _defaultValue) throw();

    /// \brief  Clone an existing attribute
    /// \param dupAttr  The attribute to copy from
    attribute(const attribute<T> &dupAttr) throw();

    /// \brief  Return the size of the encapsulated vector
    /// \return  The number of represented indices
    size_t Size() const throw();

#ifndef DO_NOT_DOCUMENT_THIS

    class proxy
    {
    private:
        attribute<T>* pAttr;
        unsigned int  index;
        explicit proxy(attribute<T>* pinitAttr): pAttr(pinitAttr) {};
        friend class attribute<T>;

    public:
        inline void operator= (const T newvalue);
        inline operator T () const;
    };

private:
    proxy tmp_value;

public:
    proxy& operator[](TIndex i) throw(ERRange)
    {
        tmp_value.index = i;
        return tmp_value;
    }

#endif // DO_NOT_DOCUMENT_THIS

    /// \brief  Return the attribute value for a given index
    /// \param index     An attribute index
    /// \return          The attribute value for this index
    ///
    /// This also works for indices which not actually represented.
    /// In that case, the default value is returned
    T       GetValue(TIndex index) throw();

    /// \brief  Set the attribute value for a given index
    /// \param index     An attribute index
    /// \param newValue  The desired value for this index
    ///
    /// This method may grow the encapsulated vector so that the index is represented
    void    SetValue(TIndex index, const T newValue) throw();

    /// \brief  Set the default attribute value
    /// \param _defaultValue  The desired default value
    ///
    /// This function does not immediately affect the vector data representation.
    /// It is applied whenever the vector data representation grows, namely by the
    /// methods #IncreaseSize(), #AppendItems() and #SetValue()
    void    SetDefaultValue(const T _defaultValue) throw();

    /// \brief  Set the default attribute value and apply it to all indices
    /// \param _defaultValue  The desired default value
    ///
    /// This function disallocates the vector data representation
    void    SetConstant(const T _defaultValue) throw();

    /// \brief  Set the default attribute value and apply it to all indices
    /// \param _defaultValue  The desired default value
    ///
    /// This function does not release the vector data representation
    void    Assign(const T _defaultValue) throw();

    /// \brief  Export the encapsulated vector
    vector<T>* GetVector() throw();
    /// \brief  Export the encapsulated array
    T* GetArray() throw();

    friend class attributePool;
    friend class graphRepresentation;

protected:
    /// \brief  Increase the attribute size to a given value
    /// \param _size  The desired number of represented attribute values
    void IncreaseSize(TIndex _size) throw(ERRange);
    /// \brief  Set the attribute capacity, even if the attribute size is == 0
    /// \param capacity  The desired number of allocated attribute values
    ///
    /// This is intended to avoid reallocations when the index range increases
    void SetCapacity(TIndex capacity) throw(ERRange);

    /// \brief  Set the attribute capacity, but only if the attribute size is > 0
    /// \param capacity  The desired number of allocated attribute values
    void ReserveItems(TIndex capacity) throw(ERRange);
    /// \brief  Insert trailing items to this attribute, but only if the attribute size is > 0
    /// \param numItems  The number of indices to be added to the vector representation
    void AppendItems(TIndex numItems) throw();
    /// \brief  Erase trailing items from this attribute
    /// \param numItems  The number of indices to be eliminated from the vector representation
    void EraseItems(TIndex numItems) throw(ERRange);
    /// \brief  Swap two attribute values represented by the encapsulated vector
    /// \param index1  A represented index
    /// \param index2  A represented index
    void SwapItems(TIndex index1,TIndex index2) throw(ERRange);

public:
    /// \brief  Return the minimum index which achieves the minimal value
    TIndex  MinIndex() throw();
    /// \brief  Return the maximum index which achieves the maximal value
    TIndex  MaxIndex() throw();

    /// \brief  Return the minimal represented attribute value
    T       MinValue() throw();
    /// \brief  Return the maximal represented attribute value
    T       MaxValue() throw();

    /// \brief  Return the default value
    T       DefaultValue() throw();
    /// \brief  Check if a vector representation is present and needed
    bool    IsConstant() throw();

    /// \brief  Determine minimal and maximal indices
    ///
    /// This is executed with the first call to #MinValue(), #MaxValue(),
    /// #MinIndex(), #MaxIndex() and, occasionally, when attribute values
    /// are updated.
    void    ComputeBounds() throw();
    /// \brief  Invalidate the minimal and maximal indices
    ///
    /// This is occasionally called by #SetValue() and #EraseItems().
    /// It needs to be called explicitly when a vector or array is exported
    /// and values are manipulated
    void    ReleaseBounds() throw();

};

/// @}


template<typename T> inline
attribute<T>::attribute(TIndex _size,const T& _defaultValue) throw() :
        data(_size,_defaultValue),tmp_value(this)
{
    defaultValue = _defaultValue;
    infimum = supremum = NoIndex;
}


template<typename T> inline
attribute<T>::attribute(const attribute<T> &dupAttr) throw() :
    data(dupAttr.data),tmp_value(this)
{
    defaultValue = dupAttr.defaultValue;
    infimum = dupAttr.infimum;
    supremum = dupAttr.supremum;
    size_t dupCap = dupAttr.data.capacity();
    SetCapacity(dupCap);
}


template<typename T> inline
size_t attribute<T>::Size() const throw()
{
    if (&data==NULL) return 0;

    return data.size();
}


template<typename T> inline
attribute<T>::proxy::operator T() const
{
    return pAttr->GetValue(index);
}


template<typename T> inline
T attribute<T>::GetValue(TIndex index) throw()
{
    if (index>=Size()) return defaultValue;

    return data[index];
}


template<typename T> inline
void attribute<T>::proxy::operator=(const T newvalue) {
    pAttr->SetValue(index,newvalue);
}


template<typename T> inline
void attribute<T>::SetValue(TIndex index, const T newValue) throw()
{
    if (index>=Size() && newValue!=defaultValue)
    {
        data.insert(data.end(),index-Size(),defaultValue);
    }

    if (   (index==infimum  && newValue>data[index])
        || (index==supremum && newValue<data[index])
       )
    {
        ReleaseBounds();
    }

    data[index] = newValue;

    if (   infimum!=NoIndex
        && (   newValue<data[infimum]
            || (newValue==data[infimum] && index<infimum)
           )
       )
    {
        infimum = index;
    }

    if (   supremum!=NoIndex
        && (   newValue>data[supremum]
            || (newValue==data[supremum] && index>supremum)
           )
       )
    {
        supremum = index;
    }
}


template<typename T> inline
void attribute<T>::SetDefaultValue(const T _defaultValue) throw()
{
    defaultValue = _defaultValue;
}


template<typename T> inline
void attribute<T>::SetConstant(const T _defaultValue) throw()
{
    data.erase(data.begin(),data.end());
    vector<T>(data).swap(data);
    defaultValue = _defaultValue;
    ReleaseBounds();
}


template<typename T> inline
void attribute<T>::Assign(const T newvalue) throw()
{
    defaultValue = newvalue;

    if (Size()>0)
    {
        data.assign(data.size(),newvalue);
        infimum = 0;
        supremum = data.size()-1;
    }
}


template<typename T> inline
void attribute<T>::ReserveItems(TIndex capacity) throw(ERRange)
{
    if (Size()>0) SetCapacity(capacity);
}


template<typename T> inline
void attribute<T>::SetCapacity(TIndex capacity) throw(ERRange)
{
    if (capacity==data.capacity())
    {
        return;
    }
    else if (capacity>data.capacity())
    {
        data.reserve(capacity);
    }
    else if (capacity>=Size())
    {
        data.swap(data);
    }
    else
    {
        throw ERRange();
    }
}


template<typename T> inline
void attribute<T>::IncreaseSize(TIndex _size) throw(ERRange)
{
    if (_size>data.size())
    {
        data.insert(data.end(),_size-data.size(),defaultValue);
    }
    else if (_size<data.size())
    {
        throw ERRange();
    }
}


template<typename T> inline
void attribute<T>::AppendItems(TIndex numItems) throw()
{
    if (numItems==0 || data.size()==0) return;

    if (infimum!=NoIndex && defaultValue<data[infimum]) infimum = data.size();

    data.insert(data.end(),numItems,defaultValue);

    if (supremum!=NoIndex && defaultValue>=data[supremum]) supremum = data.size()-1;
}


template<typename T> inline
void attribute<T>::EraseItems(TIndex numItems) throw(ERRange)
{
    if (numItems==0 || data.size()==0) return;

    if (numItems<=data.size())
    {
        // When reading from file, default values are stored at data[0]
        if (numItems==data.size()) defaultValue = data[0];

        if (   (infimum!=NoIndex && infimum>=data.size()-numItems)
            || (supremum!=NoIndex && supremum>=data.size()-numItems)
           )
        {
            ReleaseBounds();
        }

        data.erase(data.end()-numItems,data.end());
    }
    else
    {
        throw ERRange();
    }
}


template<typename T> inline
void attribute<T>::SwapItems(TIndex index1,TIndex index2) throw(ERRange)
{
    if (data.size()==0) return;

    if (index1==index2) return;

    if (index1<data.size() && index2<data.size())
    {
        if (index1==infimum)
        {
            infimum = index2;
        }
        else if (index2==infimum)
        {
            infimum = index1;
        }

        if (index1==supremum)
        {
            supremum = index2;
        }
        else if (index2==supremum)
        {
            supremum = index1;
        }

        T swap = data[index1];
        data[index1] = data[index2];
        data[index2] = swap;
    }
    else
    {
         throw ERRange();
    }
}


template<typename T> inline
void attribute<T>::ComputeBounds() throw()
{
    if (data.size()==0)
    {
        supremum = infimum = NoIndex;
        return;
    }

    supremum = infimum = 0;
    T min = data[0];
    T max = data[0];

    for (TIndex i=1; i<data.size(); i++)
    {
        T c = data[i];

        if (c<min)
        {
            min = c;
            infimum = i;
        }
        else if (c>=max)
        {
            max = c;
            supremum = i;
        }
    }
}


template<typename T> inline
void attribute<T>::ReleaseBounds() throw()
{
    supremum = infimum = NoIndex;
}


template<typename T> inline
T attribute<T>::DefaultValue() throw()
{
    return defaultValue;
}


template<typename T> inline
bool attribute<T>::IsConstant() throw()
{
    if (data.size()==0) return true;

    if (MinValue()<MaxValue() || MinValue()!=defaultValue) return false;

    return true;
}


template<typename T> inline
TIndex attribute<T>::MinIndex() throw()
{
    if (data.size()==0) return NoIndex;

    if (infimum==NoIndex) ComputeBounds();

    return infimum;
}


template<typename T> inline
TIndex attribute<T>::MaxIndex() throw()
{
    if (data.size()==0) return NoIndex;

    if (supremum==NoIndex) ComputeBounds();

    return supremum;
}


template<typename T> inline
T attribute<T>::MinValue() throw()
{
    if (data.size()==0) return defaultValue;

    if (infimum==NoIndex) ComputeBounds();

    return data[infimum];
}


template<typename T> inline
T attribute<T>::MaxValue() throw()
{
    if (data.size()==0) return defaultValue;

    if (supremum==NoIndex) ComputeBounds();

    return data[supremum];
}


template<typename T> inline
vector<T>* attribute<T>::GetVector() throw()
{
    return &data;
}


template<typename T> inline
T* attribute<T>::GetArray() throw()
{
    const T& _ref = data[0];
    return &const_cast<T&>(_ref);
}

#endif
