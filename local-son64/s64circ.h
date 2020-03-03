// s64circ.h
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

#ifndef __S64CIRC_H__
#define __S64CIRC_H__
#include <iterator>
#include "s64.h"
#include "s64filt.h"
#include "s64range.h"
#include "s64iter.h"
/*!
\file s64circ.h
\internal
\brief Templates for classes CircBuffer and circ_iterator that implement circular buffers

Templates for classes used to implement circular buffers used when saving data to
allow us to take a data stream and decide what we want to save later. These buffer
expect (espcially the waveform versions) continuous data. The separate CSaveTimes
class can then be used to decide which data ranges get passed on to the disk buffers
(which means that they will be saved).
*/

using namespace ceds64;

namespace ceds64
{
    // Forward declare the two mutually referential templates
    template<typename T> class CircBuffer;
    template<typename T, typename C = CircBuffer<T>> class circ_iterator;

    //! Template for random access iterator to a circular buffer
    /*!
    \internal
    This is uses as the basis of the circular channel buffers used for new data files
    that allow us to handle streamed data with optional saving and restrospective
    decisions about the data to write to disk.
    \tparam T   The type of the data object that the channel uses, for example
                TAdc, TSTime64, TMarker, TRealMark...
    \tparam C   The class of the container buffer used to store the data.
    */
    template<typename T, typename C> class circ_iterator
    {
    protected:
        const C& m_cb;                  //!< the container
        T* m_pItem;                     //!< points at an item in the container

    public:
        typedef std::random_access_iterator_tag iterator_category;
        typedef T value_type;
        typedef ptrdiff_t difference_type;
        typedef T* pointer;
        typedef T& reference;

        //! Construct a new iterator
        /*!
        \param cb    The container buffer.
        \param pItem Pointer to the data item.
        */
        explicit circ_iterator(const C& cb, T* pItem)
            : m_cb( cb )
            , m_pItem( pItem )
        {}

        //! Copy constructor
        /*!
        \param rhs The object to copy
        */
        circ_iterator(const circ_iterator& rhs)
            : m_cb(rhs.m_cb)
            , m_pItem( rhs.m_pItem)
        {}

        //! Test inequality
        /*!
        \param rhs The circular iterator to compare with.
        */
        bool operator!=(const circ_iterator& rhs)   //!< Test inequality
        {
            assert(&m_cb == &rhs.m_cb);
            return m_pItem != rhs.m_pItem;
        }

        //! Test equality
        /*!
        \param rhs The circular iterator to compare with.
        */
        bool operator==(const circ_iterator& rhs)   //!< Test equality
        {
            assert(&m_cb == &rhs.m_cb);
            return m_pItem == rhs.m_pItem;
        }

        //! Increment the iterator (postfix form)
        /*!
        \return A copy of the unmodified iterator.
        */
        circ_iterator operator++(int)   // postfix
        {
            circ_iterator ret( *this ); // take initial copy
            operator++();               // use prefix operator
            return ret;
        }

        //! Increment the iterator (prefix form)
        /*!
        \return A reference to the modified iterator.
        */
        circ_iterator& operator++()     // prefix - specialized for TExtmark
        {
            if (++m_pItem == &(*m_cb.m_iE))
                m_pItem = m_cb.m_pD;
            return (*this);
        }

        //! Decrement the iterator (postfix form)
        /*!
        \return A copy of the unmodified iterator.
        */
        circ_iterator operator--(int)   // postfix
        {
            circ_iterator ret( *this ); // take initial copy
            operator--();               // use prefix operator
            --m_pItem;
            return ret;
        }

        //! Decrement the iterator (prefix form)
        /*!
        \return A reference to the modified iterator.
        */
        circ_iterator& operator--()     // prefix - specialized for TExtmark
        {
            if (m_pItem == m_cb.m_pD)
                m_pItem = &(*m_cb.m_iE);
            --m_pItem;
            return (*this);
        }
    
        //! Add a distance to a copy of the iterator and return the result
        /*!
        \param d The distance to add.
        \return  An iterator to the result.
        */
        circ_iterator operator+(difference_type d) const
        {
            assert((m_pItem >= m_cb.m_pD) && (abs(d) <= m_cb.m_nSize));
            circ_iterator ret( *this );
            ret += d;
            return ret;
        }
    
        //! Subtract a distance from a copy of the iterator and return the result
        /*!
        \param d The distance to subtract.
        \return  An iterator to the result.
        */
        circ_iterator operator-(difference_type d) const
        {
            assert((m_pItem >= m_cb.m_pD) && (abs(d) <= m_cb.m_nSize));
            circ_iterator ret( *this );
            ret -= d;
            return ret;
        }

        //! Subtract an iterator from this and return the distance
        /*!
        \param rhs The iterator to subtract. It must point to the same buffer!
        \return    The distance between the iterators.
        */
        difference_type operator-(const circ_iterator& rhs) const
        {
            assert(&m_cb == &rhs.m_cb);
            return Index(m_pItem) - Index(rhs.m_pItem);
        }

        //! Add a distance to the iterator
        /*!
        \param d The distance to move the iterator (less than the allocated size)
        \return  A reference to the modified iterator.
        */
        circ_iterator& operator+=(difference_type d) // - specialized for TExtmark
        {
            m_pItem += d;
            Wrap();
            return (*this);
        }

        //! Subtract a distance from the iterator
        /*!
        \param d The distance to move the iterator (less than the allocated size)
        \return  A reference to the modified iterator.
        */
        circ_iterator& operator-=(difference_type d) //  - specialized for TExtmark
        {
            m_pItem -= d;
            Wrap();
            return (*this);
        }

        T& operator*() const        //! Convert to a reference to the item
        {
            return *m_pItem;
        }

        T* operator->() const       //!< Convert to a pointer to the item
        {
            return m_pItem;
        }

        //! Array index operations for TExtMark
        /*!
        \param d The index to apply
        \return  A reference to the item at this index
        */
        T& operator[](difference_type d) // specialize for TExtMark
        {
            assert((d > -m_cb.m_nSize) && (d < m_cb.m_nSize));
            d += m_cb.m_nFirst;
            if (d >= m_cb.m_nAllocated)
                d -= m_cb.m_nAllocated;
            else if (d < 0)
                d += m_cb.m_nAllocated;
            return m_pItem[d];
        }

        //! Compare with another iterator
        /*!
        \param rhs The iterator to compare with. It must be to the same container.
        \return    true if we are less than the rhs iterator.
        */
        bool operator<(const circ_iterator& rhs)
        {
            assert(&m_cb == &rhs.m_cb);     // Assert that the containers are the same
            return Index(m_pItem) < Index(rhs.m_pItem);
        }

    private:
        void Wrap() // specialized for TExtMark
        {
            if (m_pItem < m_cb.m_pD)
                m_pItem += m_cb.m_nAllocated;
            else if (m_pItem >= m_cb.m_pE)
                m_pItem -= m_cb.m_nAllocated;
        }

        difference_type Index(const T* pItem) const
        {
            auto d = pItem - (const T*)m_cb.m_pD; // offset into buffer
            d -= m_cb.m_nFirst;         // offset from start
            if (d < 0)
                d += m_cb.m_nAllocated;

            // It is valid for an index to the next to be one past the data available
            assert((d >= 0) && (d <= static_cast<difference_type>(m_cb.m_nSize)));
            return d;
        }
    };

    //! The circular buffer used for Waveforms
    /*!
    The circular buffer for waveforms only holds contiguous data. We expect you to be
    passing in contiguous data, so this should not be a problem. If you are not storing
    contiguous data, you may find you do better to not use the buffered type for storing
    data and just use the standard write buffer as each time we detect non-contiguous
    data we empty the buffer and start again.

    We could get rid of the m_tDirty if we arranged to overwrite any data in the write buffer
    as well as data in the circular buffer when an overwrite occurs. This would be worse in
    the case of many overwrites or overwrites of non-saved data, but no worse in the case of
    one overwrite. This would involve changes to WriteData.

    \tparam T The underlying type stored in the buffer (short for Adc, float for RealWave).
    */
    template<typename T>
    class CircWBuffer
    {
        friend class circ_iterator<T, CircWBuffer<T> >;
        typedef db_iterator<T> db_iter;
        typedef size_t TBufInd;         //!< Index into the physical buffer

        // Remember that the order of members sets the order of construction
        T* m_pD;                        //!< Owns the assigned memory, used to construct iterators
        db_iter m_iD;                   //!< iterator to start of the buffer
        db_iter m_iE;                   //!< iterator to past the end of allocated space
        TBufInd m_nSize;                //!< items in the buffer
        TBufInd m_nAllocated;           //!< space reserved in the buffer
        const TBufInd m_nItemSize;      //!< bytes per item
        TBufInd m_nFirst;               //!< the index of the first item in the buffer
        TBufInd m_nNext;                //!< index of next to add. If == m_nFirst, we are empty
        TSTime64 m_tFirst;              //!< time of first item in the buffer
        TSTime64 m_tDivide;             //!< time per point
        TSTime64 m_tDirty;              //!< oldest dirty time in circular buffer
    public:
		//! Structure to describe a contiguous range of data of type T (no wrap)
        struct range
        {
            TSTime64 m_tStart;          //!< Start time of the range
            T* m_pData;					//!< The actual data
            size_t m_n;					//!< Number of data items
        };

        //! Construct a new circular waveform buffer
        /*!
        \param size The size of the buffer in data items.
        \param tDvd The channel divider (so we can calculate times)
        */
        explicit CircWBuffer(size_t size, TSTime64 tDvd)
            : m_pD( nullptr )           // order set by order of member!
            , m_iD( nullptr )
            , m_iE( nullptr )
            , m_nSize( 0 )
            , m_nAllocated( 0 )
            , m_nItemSize( sizeof(T) )
            , m_nFirst( 0 )
            , m_nNext( 0 )
            , m_tFirst( -1 )
            , m_tDivide( tDvd )
            , m_tDirty( -1 )            // set by change and externally when saving
        {
            reallocate( size );
        }

        virtual ~CircWBuffer()
        {
            delete[] m_pD;
        }

        //! Set the buffer size
        /*!
        This will throw if we run out of memory. This trashes eall the pointers. It
        preserves the channel divide.
        \param size The new size of the buffer.
        */
        void reallocate(size_t size)
        {
            T* p = new T[ size ];
            delete[] m_pD;          // release old buffer
            m_pD = p;               // use new pointer
            m_nSize = 0;            // no data items
            m_iD = db_iter(static_cast<T*>(m_pD), m_nItemSize); // update the iterators
            m_iE = m_iD + size;     // the end of the buffer
            m_nAllocated = size;    // new allocated size
            m_nFirst = m_nNext = 0; // adjust the pointers for new buffer
            m_tFirst = -1;
            m_tDirty = -1;
        }

        size_t size() const {return m_nSize;}       //!< The number of items in the buffer
        bool empty() const {return m_nSize == 0;}   //!< True if the buffer is empty
        
        // The capacity must be 1 less otherwise begin() and end() are the same when
        // the buffer is full, which will break algorithms.
        size_t capacity() const {return m_nAllocated ? m_nAllocated-1 : 0;} //!< Get buffer capaciaty
        size_t space() const {return m_nAllocated - 1 - m_nSize;}   //!< Get free items in the buffer

        bool wraps() const          //!< True if the data wraps around the end of the buffer
        {
            return m_nFirst + m_nSize > m_nAllocated;
        }

        T& front() const            //!< reference to the oldest item in the buffer (assume present)
        {
            assert(m_nSize);        // We MUST have some data for this to be valid
            return m_pD[m_nFirst];
        }

        T& back() const             //!< Reference to the newest item in the buffer (assume present)
        {
            assert(m_nSize);        // We MUST have data for this to be valid
            TBufInd index = m_nNext ? m_nNext : m_nAllocated;
            return m_pD[index - 1];
        }

        TSTime64 FirstTime() const      //!< Get the first time in the buffer
        {
            return m_nSize ? m_tFirst : -1;
        }

        TSTime64 FirstDirty() const     //!< Get the start time of the dirty data
        {
            return m_tFirst > m_tDirty ? m_tFirst : m_tDirty;
        }

        //! Set where the last write of the buffer ended
        /*!
        \param tWroteUpTo The time written up to (but not including) from the circular buffer
                          to the disk. This is the time of the next expected write, so all data
                          at or after this time is 'dirty' (not written).
        */
        void SetCleanUpTo(TSTime64 tWroteUpTo)
        {
            m_tDirty = tWroteUpTo;
        }

        TSTime64 LastTime() const   //!< Get the time of the last buffer item or -1 if none
        {
            return m_nSize ? m_tFirst + m_tDivide*(m_nSize-1) : -1;
        }

        //! Set the time of the first buffer item
        /*!
        \param t The time to set
        */
        void SetFirstTime(TSTime64 t)
        {
            m_tFirst = t;
        }

        //! Array access (read/write) to the buffer
        /*!
        \param i Array index relative to the oldest item in the buffer. This index is
                 assumed to be legal.
        \return  A reference to the data at this index
        */
        T& operator[](TBufInd i)
        {
            assert(i < m_nSize);
            i += m_nFirst;
            if (i >= m_nAllocated)
                i -= m_nAllocated;
            return m_pD[i];
        }

        //! Array access (read) to the buffer
        /*!
        \param i Array index relative to the oldest item in the buffer. This index is
                 assumed to be legal.
        \return  A copy of the data at this index
        */
        T operator[](TBufInd i) const
        {
            assert(i < m_nSize);
            i += m_nFirst;
            if (i >= m_nAllocated)
                i -= m_nAllocated;
            return m_pD[i];
        }

        //! Copy contiguous data to the buffer if space. If not space, copy what we can.
        /*!
        Move as much data as possible into the buffer and return the number of items
        copied. If not all copied, then the buffer is full.
        \param pData Points at an array of data to be copied.
        \param n     The number of items we could like to copy
        \return      The number of items that were copied
        */
        size_t add(const T* pData, size_t n)
        {
            size_t nSpace = space();    // Maximum items we could copy
            n = nSpace = std::min(n, nSpace); // what we will copy
            if (nSpace > 0)
            {
                // There is space, so we will copy the lot
                m_nSize += n;               // say the new buffer size after the copy
                size_t nCopy = m_nAllocated - m_nNext;  // space to end...
                if (nCopy > n)              // If this is more than we want to copy...
                    nCopy = n;              // ...just copy the original size

                memcpy(&m_pD[m_nNext], pData, nCopy*m_nItemSize); // first part of move
                m_nNext += nCopy;           // next index to use
                if (m_nNext >= m_nAllocated)
                    m_nNext = 0;

                n -= nCopy;                 // number left to copy
                if (n > 0)                  // if more left to do
                {
                    pData += nCopy;         // move source pointer onwards
                    memcpy(m_pD, pData, n*m_nItemSize);
                    m_nNext = n;            // we have wrapped, so next is here
                }
            }
            return nSpace;
        }

        //! Add waveform data at time t to the buffer.
        /*!
        If the data is contiguous, or circular buffer is empty just add it. If the data is
        not contiguous we cannot add as the buffer MUST be flushed before we can continue.
        \param pData The data to add to the circular buffer.
        \param n     The number of data items to add.
        \param t     The time of the first item in pData.
        \return      The number of data items that were copied
        */
        size_t add(const T* pData, size_t n, TSTime64 t)
        {
            assert((m_nSize == 0) || (m_tFirst >= 0));  // sanity check
            if (m_nSize == 0)           // if empty
                m_tFirst = t;
            else if (m_tFirst + m_nSize*m_tDivide != t) // ...not contiguous
                return 0;               // cannot add
            return add(pData, n);       // add contiguous or new data
        }

        //! Modify the data already in the buffer
        /*!
        \param pData Points at the data items used to modify the buffer.
        \param count The number of items in pData.
        \param tFrom The time of the first item in pData. If the data is not aligned
                     with the buffer, we change from the next point. This may be a
                     mismatch with the disk buffer code which rounds down. However, any
                     sensible code always matches the times.
        \return      0 or an error code. At the moment, we always return 0. The only use
                     of return would be if we decide that non-aligned is an error.
        */
        int change(const T* pData, size_t count, TSTime64 tFrom)
        {
            TSTime64 tEnd = tFrom + count* m_tDivide;
            TSTime64 tCircLastTime = LastTime();
            if ((tEnd <= m_tFirst) || (tFrom > tCircLastTime))
                return 0;

            // To get here we will change at least 1 point
            // Now "align" data with buffer. Index is index to buffer
            TSTime64 tOff = m_tFirst - (m_tDivide - 1);
            size_t index = m_nFirst;    // index to m_tFirst data in circ buffer
            if (tFrom < tOff)           // We must lose some points before...
            {
                tOff = m_tFirst - tFrom;    // offset to the buffer data
                size_t nSkip = static_cast<size_t>(tOff / m_tDivide);
                pData += nSkip;
                count -= nSkip;
                tFrom += nSkip * m_tDivide;
            }
            else
            {
                tOff = tFrom - tOff;
                index += static_cast<size_t>(tOff / m_tDivide);
                if (index >= m_nAllocated)
                    index -= m_nAllocated;
            }

            if (tFrom < m_tDirty)       // if we modify the buffer...
                m_tDirty = tFrom;       // ...remember the earliest dirty time

            // Could check for alignment here.
            // if (tFrom != m_tFirst + index*m_tDivide)
            //     return NOT_ALIGNED;

            // Now check that data does not exceed the buffer contents.
            if (tEnd > tCircLastTime + m_tDivide)
                count = static_cast<size_t>((tCircLastTime - tFrom) / m_tDivide) + 1;
            assert(count < m_nAllocated);

            // Now overwrite the data in the buffer
            if (index + count > m_nAllocated)   // if it wraps...
            {
                size_t nCopy = m_nAllocated - index;
                memcpy(m_pD+index, pData, nCopy*sizeof(T));
                pData += nCopy;
                count -= nCopy;
                index = 0;
            }
            memcpy(m_pD+index, pData, count*sizeof(T));
            return 0;
        }

        //! Read data from the circular buffer to a linear array
        /*!
        \param pData The destination buffer to hold the read data.
        \param nMax  The maximum number of points to read into the pData buffer.
        \param tFrom The first time we are interested in data for.
        \param tUpto The non-inclusive end time of the read.
        \param tFirst Returened set to the time of the first point (if any read).
        \param bFirst If true, the data need not be contiguous, that is the time returned
                      in tFirst need not match tFrom.
        \return       The number of points read.
        */
        int read(T* pData, size_t nMax, TSTime64 tFrom, TSTime64 tUpto, TSTime64& tFirst, bool bFirst)
        {
            if ((nMax == 0) || (m_nSize == 0) || (tUpto <= m_tFirst) ||
                (!bFirst && (tFrom != m_tFirst)))
                return 0;
            
            range x[2];         // will hold 0, 1 or 2 circ buff ranges
            int nRead = 0;
            size_t n = contig_range(tFrom, tUpto, x);
            for (size_t i = 0; nMax && (i<n); ++i)
            {
                if ((i==0) && bFirst)
                    tFirst = x[0].m_tStart;
                size_t nCopy = std::min(nMax, x[i].m_n); // how much to copy
                memcpy(pData, x[i].m_pData, nCopy*sizeof(T));
                pData += nCopy;
                nRead += static_cast<int>(nCopy);
                nMax -= nCopy;
            }
            return nRead;
        }

        //! Get the buffer index of the first item at or after a time
        /*!
        \param t    The time to locate in the buffer
        \return     The index from the start of the physical buffer to the value.
        */
        TBufInd LowerBound(TSTime64 t) const
        {
            if (t <= m_tFirst)          // if before or at start...
                return m_nFirst;        // ...first point is the lower bound
            TSTime64 tEnd = m_tFirst + m_nSize*m_tDivide;
            if (t >= tEnd)
                return m_nNext;

            // To get here, time is accounted for by data in the buffer
            TBufInd i = m_nFirst;
            TSTime64 tOff = t - m_tFirst;
            if (wraps())
            {
                TSTime64 tSecond = m_tFirst + (m_nAllocated-m_nFirst)*m_tDivide;
                if (t >= tSecond)       // If the time is in the second section...
                {
                    tOff = t - tSecond;
                    i = 0;
                }
            }
            i += static_cast<TBufInd>((tOff + m_tDivide - 1) / m_tDivide);    // buffer index
            return i;
        }

        //! Find a time in the circular buffer
        /*!
        \param t    The time to search for.
        \return     A circular iterator to the first element at or after t. if set to end(),
                    then no data found.
        */
        circ_iterator<T> Find(TSTime64 t) const
        {
            return circ_iterator<T, CircWBuffer<T> >(*this, m_pD+LowerBound(t));
        }

        //! Return 0, 1, 2 buffer sections that hold a range
        /*!
        Given a time range, it can not be in the buffer, or it can lie in 1 linear buffer
        range, or 2 linear buffer ranges if the ranges wrap around the end of the buffer.
        \param tFrom    The start time of the range to search for in the buffer.
        \param tUpto    The non-inclusive end time of the range
        \param r        Points at an array of _at least_ 2 range objects. 0, 1 or 2 of these
                        are filled with the data ranges to use in the buffer. If 2 ranges are
                        set, r[0] is the first and r[1] is second and contiguous.
        \return         The number of elements of r that are filled in (0, 1 or 2).
        */
        size_t contig_range(TSTime64 tFrom, TSTime64 tUpto, range* r) const
        {
            assert(m_nSize < m_nAllocated);
            assert(tFrom < tUpto);
            TBufInd p1 = LowerBound(tFrom);
            TBufInd p2 = LowerBound(tUpto);
            r[0].m_pData = m_pD+p1;
            size_t p1Logical = (p1 < m_nFirst) ? p1 + m_nAllocated : p1;
            r[0].m_tStart = m_tFirst + (p1Logical - m_nFirst) * m_tDivide;
            if (p2 < p1)                // data wraps around the end of the buffer
            {
                r[0].m_n = m_nAllocated-p1; // Extend to the end of the buffer
                if (p2 > 0)                 // if data in second section...
                {
                    r[1].m_pData = m_pD;    // starts at buffer start
                    r[1].m_n = p2;          // this is number of points
                    r[1].m_tStart = r[0].m_tStart + r[0].m_n*m_tDivide;
                    return 2;               // Two ranges needed to span the data
                }
                else
                    return 1;           // all in the first range
            }
            else
            {
                r[0].m_n = p2-p1;           // p2==p1 always means empty
                assert(r[0].m_n <= m_nSize);
                return r[0].m_n ? 1 : 0;    // one or zero ranges
            }
        }

        //! Return the number of items that line in a time range in the buffer
        /*!
        \param tFrom    The start of the time range to search for.
        \param tUpto    The non-inclusive end of the time range.
        \return         The number of items in the range
        */
        size_t Count(TSTime64 tFrom, TSTime64 tUpto = TSTIME64_MAX)
        {
            range x[2];                     // get the 0, 1 or 2 ranges
            size_t n = contig_range(tFrom, tUpto, x);
            size_t total = 0;
            for (size_t i=0; i<n; ++i)
                total += x[i].m_n;
            return total;
        }

        //! Free all items from the buffer
        /*!
        \param tNext The time to set m_tFirst to (as no data in the buffer). If you
                     do not set this, ,_tFirst is set to -1.
        */
        void flush(TSTime64 tNext = -1) // free all items
        {
            m_nFirst = m_nNext = m_nSize = 0;   // no items in the buffer
            m_tFirst = tNext;           // set time for next data or -1 for none
        }

        //! Free items from the start of the circular buffer
        /*!
        \param nFree The number of items to free from the start of the buffer.
        \warning This can leave the start time unset (-1) if it empties the buffer which
                 happens if `nFree >= m_nSize`.
        */
        void free(size_t nFree)         // free items from start of buffer
        {
            if (nFree >= m_nSize)
                flush();                // sets m_tFirst to -1
            else
            {
                m_nFirst += nFree;      // release the items
                if (m_nFirst >= m_nAllocated)
                    m_nFirst -= m_nAllocated;   // wrap index
                m_nSize -= nFree;
                m_tFirst += nFree*m_tDivide;    // move the start time onwards
            }
        }

        circ_iterator<T> begin() const      //!< circular iterator to the start of the data
        {
            return circ_iterator<T>(*this, &(*(m_iD+m_nFirst)));
        }

        circ_iterator<T> end() const        //!< ciruclar iterator to the end of the data
        {
            int i = m_nFirst + m_nSize;
            if (i >= m_nAllocated)
                i -= m_nAllocated;
            return circ_iterator<T>(*this, &(*(m_iD+i)));
        }

        //! Find the item r.Max() before r.Upto()
        /*!
        We know that this is the first buffer we look at, so no need to be contiguous.
        \param r    The range to search. r.From() is the earliest to go back to, r.Upto()
                    is the end of the range.
        \return     -1 if not found or the time.
        */
        TSTime64 PrevNTime(CSRange& r) const
        {
            if (empty() ||              // if we have nothing, or...
                (r.Upto() <= m_tFirst)) // ...before the buffer starts
                return -1;

            TSTime64 tEnd = m_tFirst + m_nSize*m_tDivide;
            size_t index = (r.Upto() >= tEnd) ? m_nSize : static_cast<size_t>((r.Upto()-m_tFirst + m_tDivide-1) / m_tDivide);
            r.NotFirst();               // no longer first time
            if (r.Max() <= index)       // if data is in the buffer
            {
                index -= r.Max();       // form index to use
                r.ZeroMax();            // say we are done
                return m_tFirst + index*m_tDivide;
            }
            else
            {
                r.ReduceMax(index);     // If previous block is continguous...
                r.SetUpto(m_tFirst);    // ...we may be able to skip back further
                return m_tFirst;
            }
        }
    };

    //! The circular buffer used for events and markers.
	/*!
	There is no tDirty in this case as we only allow editing of attached data and this is
	handled for both the write buffer and for the circular buffer; we only write circular
	data once.
    \internal
	*/
    template<typename T>
    class CircBuffer
    {
        friend class circ_iterator<T, CircBuffer<T>>;
        typedef db_iterator<T> db_iter;
        typedef size_t TBufInd;         // our buffer index

        // Remember that the order of members sets the order of construction
        void* m_pD;                     //!< used to construct the iterators
        db_iter m_iD;                   //!< iterator to start of the buffer
        db_iter m_iE;                   //!< iterator to past the end of allocated space
        TBufInd m_nSize;                //!< items in the buffer
        TBufInd m_nAllocated;           //!< space reserved in the buffer
        const TBufInd m_nItemSize;      //!< bytes per item
        TBufInd m_nFirst;               //!< the index of the first item in the buffer
        TBufInd m_nNext;                //!< index of next to add. If == m_nFirst, we are empty

    public:
		//! Structure to hold a contiguous range (a range with no wrap around the end)
        struct range
        {
            T* m_pData;					//!< The start of the data
            size_t m_n;					//!< The number of items
        };

        //! Construct a circular buffer for variable or fixed size objects
        /*!
        \param size     The size ofthe circular buffer in items.
        \param itemSize Optional, the size of the items. If not supplied it is assumed to be
                        `sizeof(T)`. If supplied, it should be `>= sizeof(T)`.
        */
        explicit CircBuffer(size_t size, size_t itemSize = sizeof(T))
            : m_pD( nullptr )			// order set by order of member!
            , m_iD(nullptr, itemSize)
            , m_iE(nullptr, itemSize)
            , m_nSize( 0 )
            , m_nAllocated( 0 )
            , m_nItemSize( itemSize )
            , m_nFirst( 0 )
            , m_nNext( 0 )
        {
            reallocate(size);   // allocate memory
        }

        virtual ~CircBuffer()
        {
            ::free(m_pD);
        }

        size_t size() const {return m_nSize;}           //!< Get the number of items in the buffer
        bool empty() const {return m_nSize == 0;}       //!< Retunr true if the buffer is empty
        size_t ItemSize() const {return m_nItemSize;}   //!< Get bytes per data item
        
        //! Set the buffer size
        /*!
        This will throw if we run out of memory. This trashes all the pointers.
        \param size The new size of the buffer.
        */
        void reallocate(size_t size)
        {
            void* p = malloc( size * m_nItemSize );
            if (!p)
                return;
            ::free( m_pD);          // release old buffer
            m_pD = p;               // use new pointer
            m_nSize = 0;            // no data items
            m_iD = db_iter(static_cast<T*>(m_pD), m_nItemSize); // update the iterators
            m_iE = m_iD + size;     // the end of the buffer
            m_nAllocated = size;    // new allocated size
            m_nFirst = m_nNext = 0; // adjust the pointers for new buffer
        }
        
        // The capacity must be 1 less otherwise begin() and end() are the same when
        // the buffer is full, which will break algorithms.
        size_t capacity() const     //!< Size of the allocated buffer
        {
            return m_nAllocated ? m_nAllocated-1 : 0;
        }

        size_t space() const        //!< free space in the buffer
        {
            return m_nAllocated - 1 - m_nSize;
        }

        bool wraps() const          //!< True if the buffer wraps around the end
        {
            return m_nFirst + m_nSize > m_nAllocated;
        }

        T& front() const            //!< Get a reference to the first item
        {
            assert(m_nSize);        // The buffer must not be empty
            return m_iD[m_nFirst];
        }

        T& back() const             //!< Get a reference to the last item
        {
            assert(m_nSize);        // The buffer must not be empty
            TBufInd index = m_nNext ? m_nNext : m_nAllocated;
            return m_iD[index - 1];
        }

        TSTime64 FirstTime() const  //!< Get the oldest time in the buffer or -1 if none.
        {
            return m_nSize ? operator[](0) : TSTime64(-1);
        }

        TSTime64 LastTime() const   //!< Get the latest time in the buffer or -1 if none.
        {
            return m_nSize ? operator[](m_nSize-1) : TSTime64(-1);
        }

        //! Array access (read/write) to the buffer
        /*!
        \param i Array index relative to the oldest item in the buffer. This index is
                 assumed to be legal.
        \return  A reference to the data at this index
        */
        T& operator[](TBufInd i)
        {
            assert(i < m_nSize);
            i += m_nFirst;
            if (i >= m_nAllocated)
                i -= m_nAllocated;
            return m_iD[i];
        }

        //! Array access (read) to the buffer
        /*!
        \param i Array index relative to the oldest item in the buffer. This index is
                 assumed to be legal.
        \return  A copy of the data at this index
        */
        T operator[](TBufInd i) const
        {
            assert(i < m_nSize);
            i += m_nFirst;
            if (i >= m_nAllocated)
                i -= m_nAllocated;
            return m_iD[i];
        }

        //! Copy to the buffer if there is space. If not space, copy what we can.
        /*!
        Move as much data as possible into the buffer and return the number of items
        copied. If not all copied, then the buffer is full.
        \param pData Points at an array of data to be copied.
        \param n     The number of items we could like to copy
        \return      The number of items that were copied
        */
        size_t add(const T* pData, size_t n)
        {
            size_t nSpace = space();    // The number we could copy
            n = nSpace = std::min(n, nSpace); // what we will copy
            if (nSpace > 0)
            {
                // Copy what we can
                m_nSize += n;               // say the new buffer size after the copy
                size_t nCopy = m_nAllocated - m_nNext;  // space to end...
                if (nCopy > n)
                    nCopy = n;

                // If this is a TExtMark type we cannot use m_pD+n_nNext or pData+nCopy
                memcpy(&m_iD[m_nNext], pData, nCopy*m_nItemSize); // first part of move
                m_nNext += nCopy;           // next index to use
                db_iterator<const T> iData(pData, m_nItemSize);
                pData = &iData[nCopy];      // move source pointer onwards
                n -= nCopy;                 // number left to copy
                if (m_nNext >= m_nAllocated)
                    m_nNext = 0;
                if (n > 0)
                {
                    memcpy(&m_iD[0], pData, n*m_nItemSize);
                    m_nNext = n;            // we have wrapped, so next is here
                }
            }
            return nSpace;
        }

        //! Remove the last item from the buffer (used to cope with simultaneous items)
        /*!
        The system is not designed to have multiple items at the same time. If we attempt to
        add multiple ones at the same time, this can cause trouble, especially in the case of
        LevelEvent data. This code removes the last item added to the circular buffer. It should
        be used with great care.
        */
        void sub()
        {
            if (m_nSize)
            {
                --m_nSize;
                m_nNext = m_nNext ? m_nNext-1 : m_nAllocated-1;
            }
        }

        //! Search for an item at or after a time
        /*!
        \param t The time to search for.
        \return  A linear iterator to either the first event after time t, or end() if none.
        */
        db_iter LowerBound(TSTime64 t) const
        {
            if (wraps())
            {
                if (m_iE[-1] >= t)      // search first half?
                    return std::lower_bound(m_iD+m_nFirst, m_iE, t);
                else
                    return std::lower_bound(m_iD, m_iD+m_nNext, t);
            }
            else
                return std::lower_bound(m_iD+m_nFirst, m_iD+m_nFirst+m_nSize, t);
        }

        //! Get an iterator to a time
        /*!
        \param t The time to search for in the buffer.
        \return  An iterator to the first event at or after time t or end() if no event.
        */
        circ_iterator<T> Find(TSTime64 t) const
        {
            return circ_iterator<T>(*this, &(*LowerBound(t)));
        }

        //! Determine how the buffer stores a range of data
        /*!
        \param tFrom The start of the time range to search for.
        \param tUpto The non-inclusive end of the range to search for.
        \param r     Points at an array of at least 2 range objects.
        \return      The number of range objects to describe the found data as 0 (no data),
                     1 (one linear range), 2 (two ranges that wrap around the buffer end)
        */
        // Return 0, 1, 2 buffer sections that hold a range
        size_t contig_range(TSTime64 tFrom, TSTime64 tUpto, range* r) const
        {
            assert(tFrom < tUpto);
            db_iter p1 = LowerBound(tFrom);
            db_iter p2 = LowerBound(tUpto);
            if (p2 < p1)                // we may have two ranges
            {
                r[0].m_pData = &(*p1);
                r[0].m_n = m_iE-p1;
                if (p2 > m_iD)          // beware the end at the start
                {
                    r[1].m_pData = &(*m_iD);
                    r[1].m_n = p2-m_iD;
                    return 2;           // Two ranges
                }
                else
                    return 1;           // all in the first range
            }
            else
            {
                r[0].m_pData = &(*p1);
                r[0].m_n = p2-p1;
                return r[0].m_n ? 1 : 0;    // one or zero ranges
            }
        }

        //! Return the number of items that line in a time range in the buffer
        /*!
        \param tFrom The start of the time range to search.
        \param tUpto The end of the time range.
        \return      The number of items in the range.
        */
        size_t Count(TSTime64 tFrom, TSTime64 tUpto = TSTIME64_MAX)
        {
            range x[2];                     // get the 0, 1 or 2 ranges
            size_t n = contig_range(tFrom, tUpto, x);
            size_t total = 0;
            for (size_t i=0; i<n; ++i)
                total += x[i].m_n;
            return total;
        }

        //! free all items, set the buffer empty
        void flush()
        {
            m_nFirst = m_nNext = m_nSize = 0;   // no items in the buffer
        }

        //! Free items from the start of the buffer
        /*!
        \param nFree The number of items to free.
        */
        void free(size_t nFree)
        {
            if (nFree >= m_nSize)
                flush();
            else
            {
                m_nFirst += nFree;      // release the items
                if (m_nFirst >= m_nAllocated)
                    m_nFirst -= m_nAllocated;   // wrap index
                m_nSize -= nFree;
            }
        }

        //! Get an iterator to the first buffer item
        /*!
        Beware that the iterators are only valid until the buffer is changed.
        \return an interator to the first item or end() if none.
        */
        circ_iterator<T> begin() const
        {
            return circ_iterator<T>(*this, &(*(m_iD+m_nFirst)));
        }

        //! Get an iterator past the end of the buffer
        /*!
        Beware that the iterators are only valid until the buffer is changed.
        \return an interator past the last buffer item.
        */
        circ_iterator<T> end() const
        {
            size_t i = m_nFirst + m_nSize;
            if (i >= m_nAllocated)
                i -= m_nAllocated;
            return circ_iterator<T>(*this, &(*(m_iD+i)));
        }

        //! Find the item r.Max() before r.Upto(), treating as a Wave
        /*!
        This is used by external markers, or to be more precise, TAdcMark data, but works for
        any external marker that behaves in the same way.
        \param r    The range to search. r.From() is the earliest to go back to, r.Upto()
                    is the end of the range.
        \param pFilter nullptr or a filter object to filter the data.
        \param nRow The number of data items per trace attached to each external marker.
        \param tDvd The channel divide for the waveform.
        \return     -1 if not found or the time.
        */
        TSTime64 PrevNTimeW(CSRange& r, const CSFilter* pFilter, size_t nRow, TSTime64 tDvd) const
        {
            return -1;                  // see specialisation for TExtMark
        }

        //! Find the item r.Max() before r.Upto()
        /*!
        We know that this is the first buffer we look at, so no need to be contiguous.
        \param r    The range to search. r.From() is the earliest to go back to, r.Upto()
                    is the end of the range.
        \param pFilter nullptr or a filter object to filter the data.
        \return     -1 if not found or the time.
        */
        TSTime64 PrevNTime(CSRange& r, const CSFilter* pFilter) const
        {
            if (r.Upto() <= FirstTime()) // nothing wanted here
                return -1;
            auto it = Find(r.Upto());   // find first at or after the range end
            if (it == begin())          // if this is start, then...
                return -1;              // ...nothing in here is wanted

            size_t nSkipped = 0;        // items skipped
            if (pFilter)                // if filter is defined
            {
                size_t nSkip = r.Max(); 
                do
                {
                    --it;                   // back to previous item
                    if (*it < r.From())     // not found...
                    {
                        r.ZeroMax();
                        return -1;
                    }

                    if (pFilter->Filter(*it))
                    {
                        ++nSkipped;
                        if (nSkipped == nSkip)
                        {
                            r.ZeroMax();
                            return *it;
                        }
                    }
                }while (it != begin());
            }
            else                        // no filtering needed
            {
                auto i = it - begin();  // index into buffer
                auto n = r.Max();       // number to skip
                if (static_cast<size_t>(i) >= n)  // if found in the buffer
                {
                    r.ZeroMax();        // flag we are done
                    return operator[](i-n);
                }
                nSkipped = static_cast<size_t>(i);
            }

            r.ReduceMax(nSkipped);      // reduce count to go back
            r.SetUpto(FirstTime());     // reduce time range to search
            return -1;
        }
    };

    // circ_iterator specializations for TExtMark which is a variable size
    // These are forward declared and the implementation is in s64xmark.cpp
    template <>
    circ_iterator<TExtMark>& circ_iterator<TExtMark>::operator++();

    template <>
    circ_iterator<TExtMark>& circ_iterator<TExtMark>::operator--();

    template<>
    void circ_iterator<TExtMark>::Wrap();

    template<>
    circ_iterator<TExtMark>& circ_iterator<TExtMark>::operator+=(difference_type d);

    template<>
    circ_iterator<TExtMark>& circ_iterator<TExtMark>::operator-=(difference_type d);

    template<>
    TExtMark& circ_iterator<TExtMark>::operator[](difference_type d);

    template<>
    circ_iterator<TExtMark>::difference_type circ_iterator<TExtMark>::Index(const TExtMark* pItem) const;

    template<>
    TSTime64 CircBuffer<TExtMark>::PrevNTimeW(CSRange& r, const CSFilter* pFilter, size_t nRow, TSTime64 tDvd) const;

    // Circular buffer specialization for TSTime64 - code is in s64event.cpp
    template<>
    TSTime64 CircBuffer<TSTime64>::PrevNTime(CSRange& r, const CSFilter* pFilter) const;
}
#endif