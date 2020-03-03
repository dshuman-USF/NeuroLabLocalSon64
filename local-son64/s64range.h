//s64range.h
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

#ifndef __S64RANGE_H__
#define __S64RANGE_H__
//! \file s64range.h
//! \brief CSRange type definitions
/*!
\internal
 This class allows us to pass around a half open time range [tFrom, tUpto) starting
 at tFrom up to but not including tUpto. It also has the maximum item count and includes
 stuff used to help time things out. For now, the time out is a counter of operations,
 but it could just as easily be a timer.
*/

#include "s64.h"

namespace ceds64
{
    struct TChanHead;

    //! This is passed around reading routine to keep track of the work to be done
    /*!
    Ranges are use in a forward sense when reading data back. They are used in a backwards
    sens when searching for the nth item before a given time (backwards). Ranges provide
    a convenient shorthand, saving passing a lot of arguments to functions.
    */
    class CSRange
    {
        TSTime64    m_tFrom;        //!< Start of time range, if search back, end
        TSTime64    m_tUpto;        //!, non-inclusive end, if backwards, non-inc start
        size_t      m_nMax;         //!< An item count
        int         m_nAllowed;     //!< used to time out operations
        uint16_t    m_nFlags;       //!< bit 0 is first time flag for contiguous waveforms
        uint16_t    m_nUnused;      //!< Was m_nTrace for reading from WaveMark as wave
        const TChanHead* m_pChanHead; //!< Needed to extract waveform from WaveMark
    public:
        //! Construct a range
        /*!
        \param tFrom    The start of the range for forward, the end for backward
        \param tUpto    The non-inclusive end for forward, non-inclusive star for backward.
                        A valid range has tFrom < tUpto, else the range is empty.
        \param nMax     The number of items to read (forwards) or to skip (backwards)
        \param bFirst   Usually true. Used when reading waveforms as m_tFrom must match the
                        data time if !bFirst for data to be contiguous.
        \param nAllowed Some operations, such as searching backwards through filtered events
                        can take a long time. We use this to cause them to stop after a given
                        number of iterations (returning the CALL_AGAIN error) so as to give
                        other operations (writes on a different thread) a chance to run.
        */
        CSRange(TSTime64 tFrom, TSTime64 tUpto, size_t nMax, bool bFirst = true, int nAllowed = 10)
            : m_tFrom(tFrom), m_tUpto(tUpto), m_nMax(nMax), m_nAllowed(nAllowed),
            m_nFlags(bFirst), m_nUnused(0), m_pChanHead( nullptr )
        {
            assert(tFrom < tUpto);
        }

        // Access
        TSTime64 From() const {return m_tFrom;} //!< Get the start time of the range
        TSTime64 Upto() const {return m_tUpto;} //!< Get the up to but not including time

        //! Modify the start time of the range
        /*!
        \param tFrom The new range start time
        \return      True if the range is still valid (tFrom < m_tUpto).
        */
        bool SetFrom(TSTime64 tFrom)
        {
            m_tFrom = tFrom;
            return tFrom < m_tUpto;
        }

        //! Modify the end time of the range
        /*!
        \param tUpto The new range up to but not including time
        \return      True if the range is still valid (tFrom < m_tUpto).
        */
        bool SetUpto(TSTime64 tUpto)
        {
            m_tUpto = tUpto; 
            return m_tFrom < tUpto;
        }

        size_t Max() const {return m_nMax;} //!< Get the number of items remaining
        bool First() const {return (m_nFlags & 1) != 0;}    //!< true if first flag is set
        void NotFirst(){m_nFlags &= 0xfffe;}                //!< true if first flag not set

        //! Called when no more data can be found during a backward search of AdcMark
        /*!
        This kills off the range by setting m_nMax to zero.
        \return If nothing yet found (still first time), return -1 else the end of the
                time range (which is the current best backwards search).
        */
        TSTime64 LastFound()
        {
            m_nMax = 0;
            return First() ? -1 : m_tUpto;
        }

        //! Set the channel head (needed when extracting wavefrom from AdcMark)
        /*!
        \param pChanHead Pointer to the channel head
        */
        void SetChanHead(const TChanHead* pChanHead){m_pChanHead = pChanHead;}
        const TChanHead* ChanHead() const {return m_pChanHead;} //!< Get the channel head

        // Operations

        //! Reduce the available count by n because n items have been read/skipped.
        /*!
        \param n The number of items to remove.
        */
        void ReduceMax(size_t n)
        {
            if (n > m_nMax)
                m_nMax = 0;
            else
                m_nMax -= n;
        }

        void ZeroMax(){m_nMax = 0;} //!< Set no items (which will kill the range)

        //! Check that reading/skipping more items is possible
        bool HasRange() const
        {
            return (m_tFrom < m_tUpto) && (m_tUpto > 0) && m_nMax;
        }

        //! Reduce the timeout count and test if timed out
        /*!
        \return True if we are timed out, false if not
        */
        bool TimeOut()
        {
            return (m_nAllowed > 0) ? (--m_nAllowed <= 0) : false;
        }

        //! You are timed out if the count is exhausted AND a range exists
        bool IsTimedOut() const {return (m_nAllowed <= 0) && HasRange();}

        //! You can continue if the count is not exhaused AND a range exists
        bool CanContinue() const {return m_nAllowed && HasRange();}

        //! Set the time out counter
        /*!
        \param nAllowed The new time out counter (default is 10).
        */
        void SetTimeOut(int nAllowed = 10){m_nAllowed = nAllowed;}

        //! Conditionally mark a range as done (no operation to do)
        /*!
        The range is marked as done by making the start the same as the upto, that is by
        making the range empty.
        \param bDone If true, the m_tFrom is set to m_tUpto, else no change.
        \return      The end of the range
        */
        TSTime64 SetDone(bool bDone = true)
        {
            if (bDone)
                m_tFrom = m_tUpto;
            return m_tUpto;
        }
    };
}
#endif
