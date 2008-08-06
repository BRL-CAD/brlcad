// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
// (C) Copyright 2007 Anthony Williams
#ifndef BOOST_THREAD_LOCKS_HPP
#define BOOST_THREAD_LOCKS_HPP
#include <boost/thread/detail/config.hpp>
#include <boost/thread/exceptions.hpp>
#include <boost/thread/detail/move.hpp>
#include <algorithm>
#include <iterator>
#include <boost/thread/thread_time.hpp>
#include <boost/utility/enable_if.hpp>

#include <boost/config/abi_prefix.hpp>

namespace boost
{
    namespace detail
    {
        template<typename T>
        struct has_member_lock
        {
            typedef char true_type;
            struct false_type
            {
                true_type dummy[2];
            };
            
            template<typename U>
            static true_type has_member(U*,void (U::*dummy)()=&U::lock);
            static false_type has_member(void*);
            
            BOOST_STATIC_CONSTANT(bool, value=sizeof(has_member_lock<T>::has_member((T*)NULL))==sizeof(true_type));
        };

        template<typename T>
        struct has_member_unlock
        {
            typedef char true_type;
            struct false_type
            {
                true_type dummy[2];
            };
            
            template<typename U>
            static true_type has_member(U*,void (U::*dummy)()=&U::unlock);
            static false_type has_member(void*);
            
            BOOST_STATIC_CONSTANT(bool, value=sizeof(has_member_unlock<T>::has_member((T*)NULL))==sizeof(true_type));
        };
        
        template<typename T>
        struct has_member_try_lock
        {
            typedef char true_type;
            struct false_type
            {
                true_type dummy[2];
            };
            
            template<typename U>
            static true_type has_member(U*,bool (U::*dummy)()=&U::try_lock);
            static false_type has_member(void*);
            
            BOOST_STATIC_CONSTANT(bool, value=sizeof(has_member_try_lock<T>::has_member((T*)NULL))==sizeof(true_type));
        };

    }
    

    template<typename T>
    struct is_mutex_type
    {
        BOOST_STATIC_CONSTANT(bool, value = detail::has_member_lock<T>::value &&
                              detail::has_member_unlock<T>::value &&
                              detail::has_member_try_lock<T>::value);
        
    };
    

    struct defer_lock_t
    {};
    struct try_to_lock_t
    {};
    struct adopt_lock_t
    {};
    
    const defer_lock_t defer_lock={};
    const try_to_lock_t try_to_lock={};
    const adopt_lock_t adopt_lock={};

    template<typename Mutex>
    class shared_lock;

    template<typename Mutex>
    class upgrade_lock;

    template<typename Mutex>
    class lock_guard
    {
    private:
        Mutex& m;

        explicit lock_guard(lock_guard&);
        lock_guard& operator=(lock_guard&);
    public:
        explicit lock_guard(Mutex& m_):
            m(m_)
        {
            m.lock();
        }
        lock_guard(Mutex& m_,adopt_lock_t):
            m(m_)
        {}
        ~lock_guard()
        {
            m.unlock();
        }
    };


    template<typename Mutex>
    class unique_lock
    {
    private:
        Mutex* m;
        bool is_locked;
        explicit unique_lock(unique_lock&);
        unique_lock& operator=(unique_lock&);
    public:
        unique_lock():
            m(0),is_locked(false)
        {}
        
        explicit unique_lock(Mutex& m_):
            m(&m_),is_locked(false)
        {
            lock();
        }
        unique_lock(Mutex& m_,adopt_lock_t):
            m(&m_),is_locked(true)
        {}
        unique_lock(Mutex& m_,defer_lock_t):
            m(&m_),is_locked(false)
        {}
        unique_lock(Mutex& m_,try_to_lock_t):
            m(&m_),is_locked(false)
        {
            try_lock();
        }
        unique_lock(Mutex& m_,system_time const& target_time):
            m(&m_),is_locked(false)
        {
            timed_lock(target_time);
        }
        unique_lock(detail::thread_move_t<unique_lock<Mutex> > other):
            m(other->m),is_locked(other->is_locked)
        {
            other->is_locked=false;
            other->m=0;
        }
        unique_lock(detail::thread_move_t<upgrade_lock<Mutex> > other);

        operator detail::thread_move_t<unique_lock<Mutex> >()
        {
            return move();
        }

        detail::thread_move_t<unique_lock<Mutex> > move()
        {
            return detail::thread_move_t<unique_lock<Mutex> >(*this);
        }

        unique_lock& operator=(detail::thread_move_t<unique_lock<Mutex> > other)
        {
            unique_lock temp(other);
            swap(temp);
            return *this;
        }

        unique_lock& operator=(detail::thread_move_t<upgrade_lock<Mutex> > other)
        {
            unique_lock temp(other);
            swap(temp);
            return *this;
        }

        void swap(unique_lock& other)
        {
            std::swap(m,other.m);
            std::swap(is_locked,other.is_locked);
        }
        void swap(detail::thread_move_t<unique_lock<Mutex> > other)
        {
            std::swap(m,other->m);
            std::swap(is_locked,other->is_locked);
        }
        
        ~unique_lock()
        {
            if(owns_lock())
            {
                m->unlock();
            }
        }
        void lock()
        {
            if(owns_lock())
            {
                throw boost::lock_error();
            }
            m->lock();
            is_locked=true;
        }
        bool try_lock()
        {
            if(owns_lock())
            {
                throw boost::lock_error();
            }
            is_locked=m->try_lock();
            return is_locked;
        }
        template<typename TimeDuration>
        bool timed_lock(TimeDuration const& relative_time)
        {
            is_locked=m->timed_lock(relative_time);
            return is_locked;
        }
        
        bool timed_lock(::boost::system_time const& absolute_time)
        {
            is_locked=m->timed_lock(absolute_time);
            return is_locked;
        }
        void unlock()
        {
            if(!owns_lock())
            {
                throw boost::lock_error();
            }
            m->unlock();
            is_locked=false;
        }
            
        typedef void (unique_lock::*bool_type)();
        operator bool_type() const
        {
            return is_locked?&unique_lock::lock:0;
        }
        bool operator!() const
        {
            return !owns_lock();
        }
        bool owns_lock() const
        {
            return is_locked;
        }

        Mutex* mutex() const
        {
            return m;
        }

        Mutex* release()
        {
            Mutex* const res=m;
            m=0;
            is_locked=false;
            return res;
        }

        friend class shared_lock<Mutex>;
        friend class upgrade_lock<Mutex>;
    };

    template<typename Mutex>
    void swap(unique_lock<Mutex>& lhs,unique_lock<Mutex>& rhs)
    {
        lhs.swap(rhs);
    }

    template<typename Mutex>
    class shared_lock
    {
    protected:
        Mutex* m;
        bool is_locked;
    private:
        explicit shared_lock(shared_lock&);
        shared_lock& operator=(shared_lock&);
    public:
        shared_lock():
            m(0),is_locked(false)
        {}
        
        explicit shared_lock(Mutex& m_):
            m(&m_),is_locked(false)
        {
            lock();
        }
        shared_lock(Mutex& m_,adopt_lock_t):
            m(&m_),is_locked(true)
        {}
        shared_lock(Mutex& m_,defer_lock_t):
            m(&m_),is_locked(false)
        {}
        shared_lock(Mutex& m_,try_to_lock_t):
            m(&m_),is_locked(false)
        {
            try_lock();
        }
        shared_lock(Mutex& m_,system_time const& target_time):
            m(&m_),is_locked(false)
        {
            timed_lock(target_time);
        }

        shared_lock(detail::thread_move_t<shared_lock<Mutex> > other):
            m(other->m),is_locked(other->is_locked)
        {
            other->is_locked=false;
            other->m=0;
        }

        shared_lock(detail::thread_move_t<unique_lock<Mutex> > other):
            m(other->m),is_locked(other->is_locked)
        {
            if(is_locked)
            {
                m->unlock_and_lock_shared();
            }
            other->is_locked=false;
            other->m=0;
        }

        shared_lock(detail::thread_move_t<upgrade_lock<Mutex> > other):
            m(other->m),is_locked(other->is_locked)
        {
            if(is_locked)
            {
                m->unlock_upgrade_and_lock_shared();
            }
            other->is_locked=false;
            other->m=0;
        }

        operator detail::thread_move_t<shared_lock<Mutex> >()
        {
            return move();
        }

        detail::thread_move_t<shared_lock<Mutex> > move()
        {
            return detail::thread_move_t<shared_lock<Mutex> >(*this);
        }


        shared_lock& operator=(detail::thread_move_t<shared_lock<Mutex> > other)
        {
            shared_lock temp(other);
            swap(temp);
            return *this;
        }

        shared_lock& operator=(detail::thread_move_t<unique_lock<Mutex> > other)
        {
            shared_lock temp(other);
            swap(temp);
            return *this;
        }

        shared_lock& operator=(detail::thread_move_t<upgrade_lock<Mutex> > other)
        {
            shared_lock temp(other);
            swap(temp);
            return *this;
        }

        void swap(shared_lock& other)
        {
            std::swap(m,other.m);
            std::swap(is_locked,other.is_locked);
        }
        
        ~shared_lock()
        {
            if(owns_lock())
            {
                m->unlock_shared();
            }
        }
        void lock()
        {
            if(owns_lock())
            {
                throw boost::lock_error();
            }
            m->lock_shared();
            is_locked=true;
        }
        bool try_lock()
        {
            if(owns_lock())
            {
                throw boost::lock_error();
            }
            is_locked=m->try_lock_shared();
            return is_locked;
        }
        bool timed_lock(boost::system_time const& target_time)
        {
            if(owns_lock())
            {
                throw boost::lock_error();
            }
            is_locked=m->timed_lock_shared(target_time);
            return is_locked;
        }
        void unlock()
        {
            if(!owns_lock())
            {
                throw boost::lock_error();
            }
            m->unlock_shared();
            is_locked=false;
        }
            
        typedef void (shared_lock::*bool_type)();
        operator bool_type() const
        {
            return is_locked?&shared_lock::lock:0;
        }
        bool operator!() const
        {
            return !owns_lock();
        }
        bool owns_lock() const
        {
            return is_locked;
        }

    };

    template<typename Mutex>
    class upgrade_lock
    {
    protected:
        Mutex* m;
        bool is_locked;
    private:
        explicit upgrade_lock(upgrade_lock&);
        upgrade_lock& operator=(upgrade_lock&);
    public:
        upgrade_lock():
            m(0),is_locked(false)
        {}
        
        explicit upgrade_lock(Mutex& m_):
            m(&m_),is_locked(false)
        {
            lock();
        }
        upgrade_lock(Mutex& m_,adopt_lock_t):
            m(&m_),is_locked(true)
        {}
        upgrade_lock(Mutex& m_,defer_lock_t):
            m(&m_),is_locked(false)
        {}
        upgrade_lock(Mutex& m_,try_to_lock_t):
            m(&m_),is_locked(false)
        {
            try_lock();
        }
        upgrade_lock(detail::thread_move_t<upgrade_lock<Mutex> > other):
            m(other->m),is_locked(other->is_locked)
        {
            other->is_locked=false;
            other->m=0;
        }

        upgrade_lock(detail::thread_move_t<unique_lock<Mutex> > other):
            m(other->m),is_locked(other->is_locked)
        {
            if(is_locked)
            {
                m->unlock_and_lock_upgrade();
            }
            other->is_locked=false;
            other->m=0;
        }

        operator detail::thread_move_t<upgrade_lock<Mutex> >()
        {
            return move();
        }

        detail::thread_move_t<upgrade_lock<Mutex> > move()
        {
            return detail::thread_move_t<upgrade_lock<Mutex> >(*this);
        }


        upgrade_lock& operator=(detail::thread_move_t<upgrade_lock<Mutex> > other)
        {
            upgrade_lock temp(other);
            swap(temp);
            return *this;
        }

        upgrade_lock& operator=(detail::thread_move_t<unique_lock<Mutex> > other)
        {
            upgrade_lock temp(other);
            swap(temp);
            return *this;
        }

        void swap(upgrade_lock& other)
        {
            std::swap(m,other.m);
            std::swap(is_locked,other.is_locked);
        }
        
        ~upgrade_lock()
        {
            if(owns_lock())
            {
                m->unlock_upgrade();
            }
        }
        void lock()
        {
            if(owns_lock())
            {
                throw boost::lock_error();
            }
            m->lock_upgrade();
            is_locked=true;
        }
        bool try_lock()
        {
            if(owns_lock())
            {
                throw boost::lock_error();
            }
            is_locked=m->try_lock_upgrade();
            return is_locked;
        }
        void unlock()
        {
            if(!owns_lock())
            {
                throw boost::lock_error();
            }
            m->unlock_upgrade();
            is_locked=false;
        }
            
        typedef void (upgrade_lock::*bool_type)();
        operator bool_type() const
        {
            return is_locked?&upgrade_lock::lock:0;
        }
        bool operator!() const
        {
            return !owns_lock();
        }
        bool owns_lock() const
        {
            return is_locked;
        }
        friend class shared_lock<Mutex>;
        friend class unique_lock<Mutex>;
    };


    template<typename Mutex>
    unique_lock<Mutex>::unique_lock(detail::thread_move_t<upgrade_lock<Mutex> > other):
        m(other->m),is_locked(other->is_locked)
    {
        other->is_locked=false;
        if(is_locked)
        {
            m->unlock_upgrade_and_lock();
        }
    }

    template <class Mutex>
    class upgrade_to_unique_lock
    {
    private:
        upgrade_lock<Mutex>* source;
        unique_lock<Mutex> exclusive;

        explicit upgrade_to_unique_lock(upgrade_to_unique_lock&);
        upgrade_to_unique_lock& operator=(upgrade_to_unique_lock&);
    public:
        explicit upgrade_to_unique_lock(upgrade_lock<Mutex>& m_):
            source(&m_),exclusive(move(*source))
        {}
        ~upgrade_to_unique_lock()
        {
            if(source)
            {
                *source=move(exclusive);
            }
        }

        upgrade_to_unique_lock(detail::thread_move_t<upgrade_to_unique_lock<Mutex> > other):
            source(other->source),exclusive(move(other->exclusive))
        {
            other->source=0;
        }
        
        upgrade_to_unique_lock& operator=(detail::thread_move_t<upgrade_to_unique_lock<Mutex> > other)
        {
            upgrade_to_unique_lock temp(other);
            swap(temp);
            return *this;
        }
        void swap(upgrade_to_unique_lock& other)
        {
            std::swap(source,other.source);
            exclusive.swap(other.exclusive);
        }
        typedef void (upgrade_to_unique_lock::*bool_type)(upgrade_to_unique_lock&);
        operator bool_type() const
        {
            return exclusive.owns_lock()?&upgrade_to_unique_lock::swap:0;
        }
        bool operator!() const
        {
            return !owns_lock();
        }
        bool owns_lock() const
        {
            return exclusive.owns_lock();
        }
    };

    namespace detail
    {
        template<typename Mutex>
        class try_lock_wrapper:
            private unique_lock<Mutex>
        {
            typedef unique_lock<Mutex> base;
        public:
            try_lock_wrapper()
            {}
            
            explicit try_lock_wrapper(Mutex& m):
                base(m,try_to_lock)
            {}

            try_lock_wrapper(Mutex& m_,adopt_lock_t):
                base(m_,adopt_lock)
            {}
            try_lock_wrapper(Mutex& m_,defer_lock_t):
                base(m_,defer_lock)
            {}
            try_lock_wrapper(Mutex& m_,try_to_lock_t):
                base(m_,try_to_lock)
            {}
            try_lock_wrapper(detail::thread_move_t<try_lock_wrapper<Mutex> > other):
                base(detail::thread_move_t<base>(*other))
            {}

            operator detail::thread_move_t<try_lock_wrapper<Mutex> >()
            {
                return move();
            }

            detail::thread_move_t<try_lock_wrapper<Mutex> > move()
            {
                return detail::thread_move_t<try_lock_wrapper<Mutex> >(*this);
            }

            try_lock_wrapper& operator=(detail::thread_move_t<try_lock_wrapper<Mutex> > other)
            {
                try_lock_wrapper temp(other);
                swap(temp);
                return *this;
            }

            void swap(try_lock_wrapper& other)
            {
                base::swap(other);
            }
            void swap(detail::thread_move_t<try_lock_wrapper<Mutex> > other)
            {
                base::swap(*other);
            }

            void lock()
            {
                base::lock();
            }
            bool try_lock()
            {
                return base::try_lock();
            }
            void unlock()
            {
                base::unlock();
            }
            bool owns_lock() const
            {
                return base::owns_lock();
            }
            Mutex* mutex() const
            {
                return base::mutex();
            }
            Mutex* release()
            {
                return base::release();
            }
            bool operator!() const
            {
                return !this->owns_lock();
            }

            typedef typename base::bool_type bool_type;
            operator bool_type() const
            {
                return static_cast<base const&>(*this);
            }
        };

        template<typename Mutex>
        void swap(try_lock_wrapper<Mutex>& lhs,try_lock_wrapper<Mutex>& rhs)
        {
            lhs.swap(rhs);
        }
        
        template<typename MutexType1,typename MutexType2>
        unsigned try_lock_internal(MutexType1& m1,MutexType2& m2)
        {
            boost::unique_lock<MutexType1> l1(m1,boost::try_to_lock);
            if(!l1)
            {
                return 1;
            }
            if(!m2.try_lock())
            {
                return 2;
            }
            l1.release();
            return 0;
        }

        template<typename MutexType1,typename MutexType2,typename MutexType3>
        unsigned try_lock_internal(MutexType1& m1,MutexType2& m2,MutexType3& m3)
        {
            boost::unique_lock<MutexType1> l1(m1,boost::try_to_lock);
            if(!l1)
            {
                return 1;
            }
            if(unsigned const failed_lock=try_lock_internal(m2,m3))
            {
                return failed_lock+1;
            }
            l1.release();
            return 0;
        }


        template<typename MutexType1,typename MutexType2,typename MutexType3,
                 typename MutexType4>
        unsigned try_lock_internal(MutexType1& m1,MutexType2& m2,MutexType3& m3,
                                   MutexType4& m4)
        {
            boost::unique_lock<MutexType1> l1(m1,boost::try_to_lock);
            if(!l1)
            {
                return 1;
            }
            if(unsigned const failed_lock=try_lock_internal(m2,m3,m4))
            {
                return failed_lock+1;
            }
            l1.release();
            return 0;
        }

        template<typename MutexType1,typename MutexType2,typename MutexType3,
                 typename MutexType4,typename MutexType5>
        unsigned try_lock_internal(MutexType1& m1,MutexType2& m2,MutexType3& m3,
                                   MutexType4& m4,MutexType5& m5)
        {
            boost::unique_lock<MutexType1> l1(m1,boost::try_to_lock);
            if(!l1)
            {
                return 1;
            }
            if(unsigned const failed_lock=try_lock_internal(m2,m3,m4,m5))
            {
                return failed_lock+1;
            }
            l1.release();
            return 0;
        }


        template<typename MutexType1,typename MutexType2>
        unsigned lock_helper(MutexType1& m1,MutexType2& m2)
        {
            boost::unique_lock<MutexType1> l1(m1);
            if(!m2.try_lock())
            {
                return 1;
            }
            l1.release();
            return 0;
        }

        template<typename MutexType1,typename MutexType2,typename MutexType3>
        unsigned lock_helper(MutexType1& m1,MutexType2& m2,MutexType3& m3)
        {
            boost::unique_lock<MutexType1> l1(m1);
            if(unsigned const failed_lock=try_lock_internal(m2,m3))
            {
                return failed_lock;
            }
            l1.release();
            return 0;
        }

        template<typename MutexType1,typename MutexType2,typename MutexType3,
                 typename MutexType4>
        unsigned lock_helper(MutexType1& m1,MutexType2& m2,MutexType3& m3,
                             MutexType4& m4)
        {
            boost::unique_lock<MutexType1> l1(m1);
            if(unsigned const failed_lock=try_lock_internal(m2,m3,m4))
            {
                return failed_lock;
            }
            l1.release();
            return 0;
        }

        template<typename MutexType1,typename MutexType2,typename MutexType3,
                 typename MutexType4,typename MutexType5>
        unsigned lock_helper(MutexType1& m1,MutexType2& m2,MutexType3& m3,
                             MutexType4& m4,MutexType5& m5)
        {
            boost::unique_lock<MutexType1> l1(m1);
            if(unsigned const failed_lock=try_lock_internal(m2,m3,m4,m5))
            {
                return failed_lock;
            }
            l1.release();
            return 0;
        }
    }

    template<typename MutexType1,typename MutexType2>
    typename enable_if<is_mutex_type<MutexType1>, void>::type lock(MutexType1& m1,MutexType2& m2)
    {
        unsigned const lock_count=2;
        unsigned lock_first=0;
        while(true)
        {
            switch(lock_first)
            {
            case 0:
                lock_first=detail::lock_helper(m1,m2);
                if(!lock_first)
                    return;
                break;
            case 1:
                lock_first=detail::lock_helper(m2,m1);
                if(!lock_first)
                    return;
                lock_first=(lock_first+1)%lock_count;
                break;
            }
        }
    }

    template<typename MutexType1,typename MutexType2,typename MutexType3>
    void lock(MutexType1& m1,MutexType2& m2,MutexType3& m3)
    {
        unsigned const lock_count=3;
        unsigned lock_first=0;
        while(true)
        {
            switch(lock_first)
            {
            case 0:
                lock_first=detail::lock_helper(m1,m2,m3);
                if(!lock_first)
                    return;
                break;
            case 1:
                lock_first=detail::lock_helper(m2,m3,m1);
                if(!lock_first)
                    return;
                lock_first=(lock_first+1)%lock_count;
                break;
            case 2:
                lock_first=detail::lock_helper(m3,m1,m2);
                if(!lock_first)
                    return;
                lock_first=(lock_first+2)%lock_count;
                break;
            }
        }
    }

    template<typename MutexType1,typename MutexType2,typename MutexType3,
             typename MutexType4>
    void lock(MutexType1& m1,MutexType2& m2,MutexType3& m3,
              MutexType4& m4)
    {
        unsigned const lock_count=4;
        unsigned lock_first=0;
        while(true)
        {
            switch(lock_first)
            {
            case 0:
                lock_first=detail::lock_helper(m1,m2,m3,m4);
                if(!lock_first)
                    return;
                break;
            case 1:
                lock_first=detail::lock_helper(m2,m3,m4,m1);
                if(!lock_first)
                    return;
                lock_first=(lock_first+1)%lock_count;
                break;
            case 2:
                lock_first=detail::lock_helper(m3,m4,m1,m2);
                if(!lock_first)
                    return;
                lock_first=(lock_first+2)%lock_count;
                break;
            case 3:
                lock_first=detail::lock_helper(m4,m1,m2,m3);
                if(!lock_first)
                    return;
                lock_first=(lock_first+3)%lock_count;
                break;
            }
        }
    }

    template<typename MutexType1,typename MutexType2,typename MutexType3,
             typename MutexType4,typename MutexType5>
    void lock(MutexType1& m1,MutexType2& m2,MutexType3& m3,
              MutexType4& m4,MutexType5& m5)
    {
        unsigned const lock_count=5;
        unsigned lock_first=0;
        while(true)
        {
            switch(lock_first)
            {
            case 0:
                lock_first=detail::lock_helper(m1,m2,m3,m4,m5);
                if(!lock_first)
                    return;
                break;
            case 1:
                lock_first=detail::lock_helper(m2,m3,m4,m5,m1);
                if(!lock_first)
                    return;
                lock_first=(lock_first+1)%lock_count;
                break;
            case 2:
                lock_first=detail::lock_helper(m3,m4,m5,m1,m2);
                if(!lock_first)
                    return;
                lock_first=(lock_first+2)%lock_count;
                break;
            case 3:
                lock_first=detail::lock_helper(m4,m5,m1,m2,m3);
                if(!lock_first)
                    return;
                lock_first=(lock_first+3)%lock_count;
                break;
            case 4:
                lock_first=detail::lock_helper(m5,m1,m2,m3,m4);
                if(!lock_first)
                    return;
                lock_first=(lock_first+4)%lock_count;
                break;
            }
        }
    }

    template<typename MutexType1,typename MutexType2>
    typename enable_if<is_mutex_type<MutexType1>, int>::type try_lock(MutexType1& m1,MutexType2& m2)
    {
        return ((int)detail::try_lock_internal(m1,m2))-1;
    }

    template<typename MutexType1,typename MutexType2,typename MutexType3>
    int try_lock(MutexType1& m1,MutexType2& m2,MutexType3& m3)
    {
        return ((int)detail::try_lock_internal(m1,m2,m3))-1;
    }

    template<typename MutexType1,typename MutexType2,typename MutexType3,typename MutexType4>
    int try_lock(MutexType1& m1,MutexType2& m2,MutexType3& m3,MutexType4& m4)
    {
        return ((int)detail::try_lock_internal(m1,m2,m3,m4))-1;
    }

    template<typename MutexType1,typename MutexType2,typename MutexType3,typename MutexType4,typename MutexType5>
    int try_lock(MutexType1& m1,MutexType2& m2,MutexType3& m3,MutexType4& m4,MutexType5& m5)
    {
        return ((int)detail::try_lock_internal(m1,m2,m3,m4,m5))-1;
    }
    

    template<typename Iterator>
    typename disable_if<is_mutex_type<Iterator>, void>::type lock(Iterator begin,Iterator end);
    
    namespace detail
    {
        template<typename Iterator>
        struct range_lock_guard
        {
            Iterator begin;
            Iterator end;
            
            range_lock_guard(Iterator begin_,Iterator end_):
                begin(begin_),end(end_)
            {
                lock(begin,end);
            }
            
            void release()
            {
                begin=end;
            }

            ~range_lock_guard()
            {
                for(;begin!=end;++begin)
                {
                    begin->unlock();
                }
            }
        };
    }

    template<typename Iterator>
    typename disable_if<is_mutex_type<Iterator>, Iterator>::type try_lock(Iterator begin,Iterator end)
    {
        if(begin==end)
        {
            return end;
        }
        typedef typename std::iterator_traits<Iterator>::value_type lock_type;
        unique_lock<lock_type> guard(*begin,try_to_lock);
        
        if(!guard.owns_lock())
        {
            return begin;
        }
        Iterator const failed=try_lock(++begin,end);
        if(failed==end)
        {
            guard.release();
        }
        
        return failed;
    }

    template<typename Iterator>
    typename disable_if<is_mutex_type<Iterator>, void>::type lock(Iterator begin,Iterator end)
    {
        typedef typename std::iterator_traits<Iterator>::value_type lock_type;
        
        if(begin==end)
        {
            return;
        }
        bool start_with_begin=true;
        Iterator second=begin;
        ++second;
        Iterator next=second;
        
        for(;;)
        {
            unique_lock<lock_type> begin_lock(*begin,defer_lock);
            if(start_with_begin)
            {
                begin_lock.lock();
                Iterator const failed_lock=try_lock(next,end);
                if(failed_lock==end)
                {
                    begin_lock.release();
                    return;
                }
                start_with_begin=false;
                next=failed_lock;
            }
            else
            {
                detail::range_lock_guard<Iterator> guard(next,end);
                if(begin_lock.try_lock())
                {
                    Iterator const failed_lock=try_lock(second,next);
                    if(failed_lock==next)
                    {
                        begin_lock.release();
                        guard.release();
                        return;
                    }
                    start_with_begin=false;
                    next=failed_lock;
                }
                else
                {
                    start_with_begin=true;
                    next=second;
                }
            }
        }
    }
    
}

#include <boost/config/abi_suffix.hpp>

#endif
