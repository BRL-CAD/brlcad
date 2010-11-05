#ifndef BOOST_ENABLE_SHARED_FROM_THIS_HPP_INCLUDED
#define BOOST_ENABLE_SHARED_FROM_THIS_HPP_INCLUDED

//
//  enable_shared_from_this.hpp
//
//  Copyright (c) 2002 Peter Dimov
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
//
//  http://www.boost.org/libs/smart_ptr/enable_shared_from_this.html
//

#include <boost/config.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/assert.hpp>
#include <boost/detail/workaround.hpp>

namespace boost
{

#if !defined( BOOST_NO_MEMBER_TEMPLATE_FRIENDS )

template< class T > class enable_shared_from_this;
template< class T, class Y > void sp_accept_owner( shared_ptr<Y> * ptr, enable_shared_from_this<T> const * pe );
template< class T, class Y > void sp_accept_owner( shared_ptr<Y> * ptr, enable_shared_from_this<T> const * pe, void * /*pd*/ );

#endif

template< class T > class enable_shared_from_this
{
protected:

    enable_shared_from_this()
    {
    }

    enable_shared_from_this(enable_shared_from_this const &)
    {
    }

    enable_shared_from_this & operator=(enable_shared_from_this const &)
    {
        return *this;
    }

// virtual destructor because we need a vtable for dynamic_cast from base to derived to work
    virtual ~enable_shared_from_this()
    {
        BOOST_ASSERT( _shared_count.use_count() <= 1 ); // make sure no dangling shared_ptr objects exist
    }

public:

    shared_ptr<T> shared_from_this()
    {
        init_weak_once();
        T * p = dynamic_cast<T *>( this );
        return shared_ptr<T>( detail::shared_count( _weak_count ), p );
    }

    shared_ptr<T const> shared_from_this() const
    {
        init_weak_once();
        T const * p = dynamic_cast<T const *>( this );
        return shared_ptr<T const>( detail::shared_count( _weak_count ), p );
    }

private:

    mutable detail::weak_count _weak_count;
    mutable detail::shared_count _shared_count;

    void init_weak_once() const
    {
        if( _weak_count.empty() )
        {
            detail::shared_count( (void*)0, detail::sp_deleter_wrapper() ).swap( _shared_count );
            _weak_count = _shared_count;
        }
    }

#if !defined( BOOST_NO_MEMBER_TEMPLATE_FRIENDS )

    template< class U, class Y > friend void sp_accept_owner( shared_ptr<Y> * ptr, enable_shared_from_this<U> const * pe );
    template< class U, class Y > friend void sp_accept_owner( shared_ptr<Y> * ptr, enable_shared_from_this<U> const * pe, void * /*pd*/ );

#else

public:

#endif

    template<typename U>
    void sp_accept_owner( shared_ptr<U> & owner ) const
    {
        if( _weak_count.use_count() == 0 )
        {
            _weak_count = owner.get_shared_count();
        }
        else if( !_shared_count.empty() )
        {
            BOOST_ASSERT( owner.unique() ); // no weak_ptrs to owner should exist either, but there's no way to check that
            detail::sp_deleter_wrapper * pd = detail::basic_get_deleter<detail::sp_deleter_wrapper>( _shared_count );
            BOOST_ASSERT( pd != 0 );
            pd->set_deleter( owner.get_shared_count() );

            owner.reset( _shared_count, owner.get() );
            detail::shared_count().swap( _shared_count );
        }
    }
};

template< class T, class Y > inline void sp_accept_owner( shared_ptr<Y> * ptr, enable_shared_from_this<T> const * pe )
{
    if( pe != 0 )
    {
        pe->sp_accept_owner( *ptr );
    }
}

template< class T, class Y > inline void sp_accept_owner( shared_ptr<Y> * ptr, enable_shared_from_this<T> const * pe, void * /*pd*/ )
{
    if( pe != 0 )
    {
        pe->sp_accept_owner( *ptr );
    }
}

} // namespace boost

#endif  // #ifndef BOOST_ENABLE_SHARED_FROM_THIS_HPP_INCLUDED
