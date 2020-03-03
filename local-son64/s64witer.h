// s64witer.h
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

#ifndef __S64WITER_H__
#define __S64WITER_H__
//! \file s64witer.h
//! \brief Headers for the waveform iterators used in waveform data blocks in 64-bit son library
//! \internal

#include <iterator>
#include "s64.h"
#include "s64iter.h"
#include "s64dblk.h"
#include <stddef.h>

// Iterator to work through the wave data types attached to a file. This is a forward
// iterator as the items are not of a fixed size, as they contain varying numbers of items.

// We should conside moving the TWave<> declaration here and using this to simplify the
// wv_iterator... stuff as <W, T> can probably be just <T> as W is TWave<T>.

namespace ceds64
{
	//! Base class for the data block waveform iterator
    template<typename W, typename T, bool isVarsize = is_varsize<T>::value >
    class wv_iterator_sizeof;

	//! Sepcialisation for non-variable size object (like short, float)
    template<typename W, typename T>
    class wv_iterator_sizeof<W, T, false>
    {
    public:
        typedef std::forward_iterator_tag iterator_category;
        typedef T value_type;
        typedef ptrdiff_t difference_type;
        typedef T* pointer;
        typedef T& reference;
        wv_iterator_sizeof(){}  //!< Default constructor
        explicit wv_iterator_sizeof(size_t sz){assert(!sz ||(sz == sizeof(T)));} //!< Construct with a size must match
        size_t size() const {return sizeof(T);}	//!< return the size of the waveform object
    };

	//! Specialisation for variable-sized object (currently unused)
    template<typename W, typename T>
    class wv_iterator_sizeof<W, T, true>
    {
        size_t m_sizeof;
    public:
        typedef std::forward_iterator_tag iterator_category;
        typedef T value_type;
        typedef ptrdiff_t difference_type;
        typedef T* pointer;
        typedef T& reference;
        wv_iterator_sizeof(){}          //! Default constructor does not set size

        //! Constructor for a variable-sized object
        /*!
        We do not use this (yet) as all wave-type objects we store have a know size, but the
        library would work for an object which all items in a channel had a fixed, but not
        known until runtime size. However, great care would have to be taken when dereferencing
        such an object.
        */
        explicit wv_iterator_sizeof(size_t sz)
            : m_sizeof(sz)
        {
            assert(sz >= sizeof(T));
        }

        size_t size() const {return m_sizeof;}	//!< return the size of the waveform object
    };

	//! Iterator used to navigate waveform sub-blocks in a wave channel data block
	/*!
	 * Waveform data stored in a data block on disk can have gaps, so data consists of blocks of
	 * contiguous data, the size of each block depends on the number of contiguous data points.
	 * The wv_iterator<W, T> object iterates through these blocks. This iterator is used with
	 * CAdcBlock and CRealWaveBlock.
	 *
     * This is templated based on the type and the concept of having a compile-time known
	 * size or not of the items making up the waveform. At the moment, we implement waves
	 * that are short or float, so the size is known. When we say variable size, we mean a
	 * size that is fixed for the channel, but user definable. For example, we could have a
	 * wave of structures that consist of a float value and a char array. If the char array
	 * size were known at compile time, it would be fixed size, but if the user could choose
	 * the size for the channel dynamically, it would be variable size. We then have two partial
	 * specializations, one for a known size and one for a variable size.
	 *
	 * \tparam W This is used as TWave<short>, const TWave<short>, TWave<float> and const TWave<float>
	 *           and defines objects that hold the waveform sections that we are iterating through.
	 *           However, W need not be TWave<T> as long as it fulfills the following specification:
	 *           W must have member W::m_nItems holding the number of items of type T stored, and the
	 *           type expects end with an array of items of type T, starting at W::m_data. The items of
	 *			 This type are packed together in memory and on disk, each W is aligned to an 8 byte
	 *           boundary.
	 * \tparam T The underlying type of the waveform.
	 */
    template<typename W, typename T, bool isVarsize = is_varsize<T>::value >
    class wv_iterator : public wv_iterator_sizeof<W, T, isVarsize>
    {
        typedef typename std::conditional<std::is_const<W>::value, const uint8_t*, uint8_t*>::type p1b;
        W* m_p;							//!< Points at the sub-blocks (usually TWave<T> or const TWave<T>)
    public:
        //! Constructor that allows you to set the size of the object
        /*!
        \param p    Points at the start of the memory to iterate through
        \param so   Size of the T object or 0 when sizeof(T) is the correct size to use
        */
        explicit wv_iterator(W* p, size_t so = 0 ) :  wv_iterator_sizeof<W, T, isVarsize>(so), m_p(p) {}

        //! Copy constructor
        wv_iterator(const wv_iterator& rhs) : wv_iterator_sizeof<W, T, isVarsize>( rhs ), m_p(rhs.m_p) {}

        //! Standard operator=
        wv_iterator& operator=(const wv_iterator& rhs){m_p = rhs.m_p; return *this;}

        //! Assign a new pointer without any other changes
        wv_iterator& operator=(W* p){m_p = p; return *this;}

		//! Postfix operator is implemented in terms of the prefix.
        wv_iterator operator++(int)     // postfix
        {
            wv_iterator ret( *this );   // take initial copy
            operator++();               // use prefix to handle this
            return ret;
        }

		//! Increment the iterator to the next item.
		/*!
		 * Sub-blocks are of type W and are aligned as closely as possible on 8 byte boundaries, so
		 * there can be some dead space between them. This code moves the m_p pointer on to the next
		 * block. There is nothing here to stop you iterating past the end.
		 */
        wv_iterator& operator++()       // prefix
        {
            size_t increment = ((this->size()*m_p->m_nItems + offsetof(W, m_data) + 7) >> 3) << 3;
            m_p = reinterpret_cast<W*>(reinterpret_cast<p1b>(m_p) + increment);
            return (*this);
        }

        W& operator*() const        //!< Dereference the object to get a reference
        {
            return *m_p;
        }

        W* operator->() const        //!< Get a simple pointer
        {
            return m_p;
        }

        bool operator!=(const wv_iterator& rhs){return m_p != rhs.m_p;} //!< Test pointer inequality
        bool operator==(const wv_iterator& rhs){return m_p == rhs.m_p;} //!< Test pointer equality
        bool operator<(const wv_iterator& rhs){return m_p < rhs.m_p;}   //!< Test pointer less than
        bool operator<=(const wv_iterator& rhs){return m_p <= rhs.m_p;} //!< Test pointer less than or equal
        bool operator>(const wv_iterator& rhs){return m_p > rhs.m_p;}   //!< Test pointer greater
        bool operator>=(const wv_iterator& rhs){return m_p >= rhs.m_p;} //!< Test pointer greater or equal
    };
}
#endif