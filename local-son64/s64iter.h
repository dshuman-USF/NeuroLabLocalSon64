// s64iter.h
/*
    Copyright (C) Cambridge Electronic Design Limited 2012-2015
    Author: Greg P. Smith
    Web: ced.co.uk email: greg@ced.co.uk

    This file is part of SON64, the 64-bit SON data library.

    SON64 is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    SON64 is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with SON64.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef __S64ITER_H__
#define __S64ITER_H__
#include <iterator>
#include <assert.h>
#include "s64.h"
//! \file s64iter.h
//! \brief Random access iterator to access items of user-defined size
/*!
It is often necessary to iterate through buffers of TExtMark objects where the iteration stride
is not defined by the type. The db_iterator<> class gives a way to unify the iterators for objects
with a defined size and those without. It allows you to use iterator[index] type code for all
objects that we store in the buffer.

Beware! You cannot use these iterators in algorithms that copy the objects pointed at because in
many cases, the iterators do not know how much data to copy. That is, sizeof(T) will not return
anything useful.
*/

namespace ceds64
{
    // This is a Random access iterator for types that can have a size that is not known at
    // compile time. It aims to be as efficient as a pointer if the type size is known at
    // compile time. YOU CANNOT COPY THE COMPLETE TYPE IF THE SIZE IS NOT KNOWN.

    //! Trait to signal type of unknown size at compile time. Fixed size by default.
    template <typename T>
    struct is_varsize
    {
        static const bool value = false;    //!< Default is a fixed size
    };

	//! TExtMark specialization as size is not known at compiler time
    template <>
    struct is_varsize<TExtMark>
    {
        static const bool value = true;     //!< Extended markers are variable size
    };

	//! const TExtMark specialization as size is not known at compiler time
    template <>
    struct is_varsize<const TExtMark>
    {
        static const bool value = true;     //!< const extended markers are variable size
    };

	//! TTextMark specialization as size is not known at compiler time
    template <>
    struct is_varsize<TTextMark>
    {
        static const bool value = true;     //!< TTextMark is variable size
    };

	//! const TTextMark specialization as size is not known at compiler time
    template <>
    struct is_varsize<const TTextMark>
    {
        static const bool value = true;     //!< const TTextMark is variable size
    };

	//! TAdcMark specialization as size is not known at compiler time
    template <>
    struct is_varsize<TAdcMark>
    {
        static const bool value = true;     //!< TAdcMark is variable size
    };

	//! const TAdcMark specialization as size is not known at compiler time
    template <>
    struct is_varsize<const TAdcMark>
    {
        static const bool value = true;     //!< const TAdcMark is variable size
    };

	//! TRealMark specialization as size is not known at compiler time
    template <>
    struct is_varsize<TRealMark>
    {
        static const bool value = true;     //!< TRealMark is variable size
    };

	//! const TRealMark specialization as size is not known at compiler time
    template <>
    struct is_varsize<const TRealMark>
    {
        static const bool value = true;     //!< const TRealMark is variable size
    };

	//! Iterator base class templated on concept of compile-time knowable size
	template<typename T, bool isVarsize = is_varsize<T>::value >
    class db_iterator_sizeof;

	//! Partial specialisation of base class for fixed size objects
    template<typename T>
    class db_iterator_sizeof<T, false>
    {
    public:
        typedef std::random_access_iterator_tag iterator_category;
        typedef T value_type;
        typedef ptrdiff_t difference_type;
        typedef T* pointer;
        typedef T& reference;
        db_iterator_sizeof(){}

        //! Constructor that defines the size of a fixed size object
        /*!
        \param sz For a fixed size object, the size is either 0 or must match the actual
               object size.
        */
        explicit db_iterator_sizeof(size_t sz)
        {
            assert(!sz ||(sz == sizeof(T)));
        }
        size_t size() const {return sizeof(T);} //!< Get the fixed object size
    };

	//! Partial specialisation of base class for variable size objects
    template<typename T>
    class db_iterator_sizeof<T, true>
    {
        size_t m_sizeof;
    public:
        typedef std::random_access_iterator_tag iterator_category;
        typedef T value_type;
        typedef ptrdiff_t difference_type;
        typedef T* pointer;
        typedef T& reference;
        db_iterator_sizeof(){}

        //! Constructor that defines the size of variable size object
        /*!
        \param sz We know that this size _must_ be be at least as large as the base object.
                  It should also be rounded up to a multiple of 8 bytes for descendents of
                  TExtMark objects.
        */
        explicit db_iterator_sizeof(size_t sz)
            : m_sizeof(sz)
        {
            assert(sz >= sizeof(T));
        }

        size_t size() const {return m_sizeof;}  //!< Get the variable object size
    };

    //! The db_iterator template class
    /*!
    This class is a random access iterator that we use to hide the differences between data
    items that are a fixed size (for example TMarker) and those that have a size only known
    at runtime (TExtMark descendents: TRealMark, TTextMark, TAdcMark).

    For classes that are fixed size, this is equivalent to a random access iterator.

    If the type `T` is one of the standard types: TExtMark, TTextMark, TAdcMark, TRealMark
    then we will automatically detect that this is of variable size. Otherwise we assume
    the object is fixed size unless you provide the `isVarsize` template paramater as `true`.
    \tparam T         The type that we are iterating through.
    \tparam isVarsize Optional. If omitted, we assume that the type `T` is of fixed size 
                      `sizeof(T)` unless T is one of the known types: TExtMark, TTextMark,
                      TRealMark or TAdcMark. If present and `true`, `T` is a variable size
                      and you must provide a size when constructing a db_iterator.

    \warning You cannot use iterators to variable-sized items in STL algorithms that copy the
    objects pointed at because in the variable size case, `*iterator = ...` will assume that
    the object is `sizeof(T)` and this is precisely what it isn't. The algorithms may appear
    to work, but they will not copy enough data.
    */
    template<typename T, bool isVarsize = is_varsize<T>::value >
    class db_iterator : public db_iterator_sizeof<T, isVarsize>
    {
        typedef typename db_iterator::difference_type diff_t;
        typedef typename std::conditional<std::is_const<T>::value, const uint8_t*, uint8_t*>::type p1b;
        T* m_p;

    public:
        //! Explicit constructor
        /*!
        This is the one that you will normally be using to create an iterator through a
        buffer. If the object has variable size, you _must_ supply the size, in bytes of
        the instance of type `T` used for the channel data.
        \param p    A pointer to the type declared for the class. This will usually be to
                    the start of a data buffer.
        \param so   If this is a variable sized object, you _must_ supply the number of
                    bytes to add to the pointer to iterate (you can get this with the
                    CSon64File::ItemSize() function).
        */
        explicit db_iterator(T* p, const size_t so = 0)
            : db_iterator_sizeof<T, isVarsize>(so)
            , m_p(p)
        {}

        //! Copy constructor
        db_iterator(const db_iterator& rhs)
            : db_iterator_sizeof<T, isVarsize>(rhs)
            , m_p(rhs.m_p)
        {}

        //! Assignment operator
        db_iterator& operator=(const db_iterator& rhs){m_p = rhs.m_p; return *this;}

        //! Postfix increment
        /*
        The int parameter is a dummy that is provided to distinguish between the prefix
        and postfix formas of operator++.
        \return A _copy_ of the iterator before the increment.
        */
        db_iterator operator++(int)     //!< postfix increment
        {
            db_iterator ret( *this );   // take initial copy
            operator++();               // use prefix to handle this
            return ret;
        }

        //! Prefix increment the iterator
        /*!
        \return A reference to the modified iterator
        */
        db_iterator& operator++()       //!< prefix increment
        {
            if (isVarsize)
                m_p = reinterpret_cast<T*>(reinterpret_cast<p1b>(m_p) + this->size());
            else
                ++m_p;
            return (*this);
        }

        //! Postfix decrement
        /*
        The int parameter is a dummy that is provided to distinguish between the prefix
        and postfix formas of operator--.
        \return A _copy_ of the iterator before the decrement.
        */
        db_iterator operator--(int)   //!< postfix decrement
        {
            db_iterator ret( *this ); // take initial copy
            operator--();
            return ret;
        }

        //! Prefix decrement the iterator
        /*!
        \return A reference to the modified iterator
        */
        db_iterator& operator--()
        {
            if (isVarsize)
                m_p = reinterpret_cast<T*>(reinterpret_cast<p1b>(m_p) - this->size());
            else
                --m_p;
            return (*this);
        }
    
        //! Add distance to iterator
        /*!
        \param d The distance to add to the iterator
        \return  An iterator d places beyond the current position
        */
        db_iterator operator+(diff_t d) const
        {
            if (isVarsize)
                return db_iterator(reinterpret_cast<T*>(reinterpret_cast<p1b>(m_p) + d*this->size()), this->size());
            else
                return db_iterator(m_p + d);
        }
    
        //! Subtract distance from iterator
        /*!
        \param d The distance to subtract from the iterator
        \return  An iterator d places before the current position
        */
        db_iterator operator-(diff_t d) const   //!< operator-
        {
            if (isVarsize)
                return db_iterator(reinterpret_cast<T*>(reinterpret_cast<p1b>(m_p) - d*this->size()), this->size());
            else
                return db_iterator(m_p-d);
        }

        //! Difference of two iterators into the same container
        /*!
        \param rhs Another iterator to the same container
        \return    The distance of (this - rhs)
        */
        diff_t operator-(const db_iterator& rhs) const
        {
            if (isVarsize)
            {
                assert(this->size() == rhs.size());
                return (reinterpret_cast<p1b>(m_p) - reinterpret_cast<p1b>(rhs.m_p))/this->size();
            }
            else
                return m_p - rhs.m_p;
        }

        //! Add immediate to iterator
        /*!
        \param d The distance to add
        \return  A reference to the modified iterator
        */
        db_iterator& operator+=(diff_t d)
        {
            if (isVarsize)
                m_p = reinterpret_cast<T*>(reinterpret_cast<p1b>(m_p) + d*this->size());
            else
                m_p += d;
            return (*this);
        }

        //! Subtract immediate from iterator
        /*!
        \param d The distance to subtract
        \return  A reference to the modified iterator
        */
        db_iterator& operator-=(diff_t d)
        {
            if (isVarsize)
                m_p = reinterpret_cast<T*>(reinterpret_cast<p1b>(m_p) - d*this->size());
            else
                m_p -= d;
            return (*this);
        }

        T& operator*() const        //!< operator*
        {
            return *m_p;
        }

        T* operator->() const       //!< operator->
        {
            return m_p;
        }

        //! Array access through the iterator
        /*!
        \param d The index to apply (which must be valid)
        \return  A reference to the item at the index
        */
        T& operator[](diff_t d)
        {
           if (isVarsize)
               return *(*this + d);
           else
               return m_p[d];
        }

        //! Const array access through the iterator
        /*!
        \param d The index to apply (which must be valid)
        \return  A reference to the item at the index
        */
        T& operator[](diff_t d) const
        {
           if (isVarsize)
               return *(*this + d);
           else
               return m_p[d];
        }

        bool operator!=(const db_iterator& rhs){return m_p != rhs.m_p;} //!< not equals comparison
        bool operator==(const db_iterator& rhs){return m_p == rhs.m_p;} //!< equals comparison
        bool operator<(const db_iterator& rhs){return m_p < rhs.m_p;}   //!< less than comparison
        bool operator<=(const db_iterator& rhs){return m_p <= rhs.m_p;} //!< less than or equal comparison
        bool operator>(const db_iterator& rhs){return m_p > rhs.m_p;}   //!< greater than comparison
        bool operator>=(const db_iterator& rhs){return m_p >= rhs.m_p;} //!< greater than or equal comparison

        //! Assign an object taking account of the size
        /*!
        \param v Reference to the object to assign
        */
        void assign(const T& v)
        {
            if (isVarsize)
                memcpy(m_p, &v, this->size());
            else
                *m_p = v;
        }
    };
}
#endif