// s64dblk.cpp
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
#include <assert.h>
#include "s64dblk.h"
#include "s64range.h"

using namespace ceds64;

//! Test if a read will have any result and to check if a filter need be used
/*!
Called by all the GetData() routines to see if there is anything to do and if we
have a filter, do we need to use it (or does it remove all data).
\param r     The range parameter for the read operation.
\param pFilt Either nullptr or points at the filter to be used.
\return      eA_none if nothing to do, eA_all if ignore filter, eA_some if we 
             must use the filter.
*/
CSFilter::eActive CDataBlock::TestActive(CSRange& r, const CSFilter* pFilt) const
{
    // We do not check the time range as we would not be reading data from this
    // block unless the time ranges overlapped, so a time range check would be
    // duplicating work done elsewhere.
    if (!m_nItems)
    {
        r.SetDone();                    // badly wrong, so say we are done
        return CSFilter::eA_none;
    }

    if (!r.HasRange())
        return CSFilter::eA_none;

    // Get the filter active mode. Either all, none or some
    return pFilt ? pFilt->Active() : CSFilter::eA_all;
}

//================================ CEventBlock ======================================

//! Add event data into the buffer and report items added
/*!
The buffer can be in any state from empty to full. If it is full, we assume it has
already been written, so we clear it (set it empty). This means that we will always
add at least 1 item to the buffer (unless count is 0 on entry).
\param pData    Reference to a pointer to data to be added. This pointer is moved on
                by the number of items written.
\param count    The number of items we would like to add (MUST be > 0).
\return         The number of items added. This can only be 0 if count is 0.
*/
int CEventBlock::AddData(const TSTime64*& pData, size_t count)
{
    assert((LastTime() < pData[0]) && count);
    if ( full() )                           // if buffer is full (already written)...
        clear();                            // ...reuse it
    size_t n = capacity() - size();         // space left in buffer
    size_t nCopy = std::min(n, count);      // events to copy

    // We use memcopy rather than std::copy_n to avoid compiler warnings. We cannot
    // make m_event an array<> as it is part of a union.
    memcpy(&(*end()), pData, nCopy*m_itemSize);
    m_nItems += static_cast<uint32_t>(nCopy);
    pData += nCopy;                         // move the pointer on
    SetUnsaved();                           // Say we have unsaved data
    return static_cast<int>(nCopy);
}

// Copy events from the current read buffer in a given time range and indicate if we found events past the
// time range or we hit the maximum number. May need a filter argument when we expand the types.
// pEvents  The target buffer. This is updated.
// r        The range to fetch, including the max number to return. This is adjusted to show
//          done if we find data at or beyond the Upto time.
// pFilter  Does not apply to an event channel as no marker codes
// Returns  The number of events copied from the buffer.
int CEventBlock::GetData(TSTime64*& pData, CSRange& r, const CSFilter* pFilt) const
{
    assert(pData && r.HasRange() && m_nItems);
    if (TestActive(r, pFilt) == CSFilter::eA_none)
        return 0;                       // nothing to do
    citer it(cbegin());
    if (FirstTime() < r.From())         // not from the start?
    {
        it = std::lower_bound(cbegin(), cend(), r.From());
        if (it == cend())
            return 0;                   // nothing in this buffer
    }

    size_t nCopyMax = std::min(static_cast<size_t>(cend() - it), r.Max());
    iter iOut(pData, sizeof(TSTime64)); // output iterator
    iter iFrom(iOut);                   // where we started from
    iter iLimit(iOut + nCopyMax);       // limit of iteration
    while ((iOut < iLimit) && (r.Upto() > *it))
        iOut.assign(*it++), ++iOut;

    pData = &*iOut;                     // update the pointer
    auto nCopy = iOut - iFrom;          // number of items copied
    r.ReduceMax(nCopy);
    r.SetDone(iOut < iLimit);           // kill search if terminated by time

    return static_cast<int>(nCopy);
}

//! Get const iterator to where time t woould be placed in the block
/*!
This applies to TSTime64-based data types (TSTime64, TMarker, TExtMark descendents).
This can return cend(), so you must test against cbegin() and cend().
\param t    The time to locate the position of in the block.
\return     cbegin() if before or at the first item, cend() if beyond the last, else
            a const iterator to where it would go.
*/
CEventBlock::citer CEventBlock::IterFor(TSTime64 t) const
{
    if (!m_nItems || (FirstTime() > t)) // We want quick results if...
        return cbegin();                // ...no data or if before the buffer
    if (LastTime() < t)                 // Quick return if...
        return cend();                  // ...past the end, else we must search
    return std::lower_bound(cbegin(), cend(), t);
}

// Find the r.Max() item before r.Upto().
// Returns  found time or -1. Reduce r.Max() by the number of skipped items. If item is
//          found, r.Max() is set zero. If not found, if previous block could not
//          hold data as too early, zero r.Max()  else reduce by skipped items, return -1.
TSTime64 CEventBlock::PrevNTime(CSRange& r, const CSFilter* pFilt) const
{
    auto it = IterFor(r.Upto());        // find insertion point for r.Upto()
    if (it > cbegin())                  // if buffer has wanted times...
    {
        size_t n = r.Max();             // number to go back
        size_t i = static_cast<size_t>(it - cbegin()); // index of insert position (positive)
        if (i >= n)                     // if wanted time is in this buffer
        {
            r.ZeroMax();                    // reduce count to 0, flags we are done
            TSTime64 t = cbegin()[i-n];     // get the time from the buffer
            return (t >= r.From()) ? t : -1;    // return time, or no such time
        }

        r.ReduceMax(i);                 // reduce count left to do.
        if (!r.SetUpto(FirstTime()))    // new start time, if this kills range...
            r.ZeroMax();                // ...signal we are done
    }
    return -1;
}

//================================= Marker ======================================

//! Add marker data into the buffer and report items added
/*!
The buffer can be in any state from empty to full. If it is full, we assume it has
already been written, so we clear it (set it empty). This means that we will always
add at least 1 item to the buffer (unless count is 0 on entry).
\param pData    Reference to a pointer to data to be added. This pointer is moved on
                by the number of items written.
\param count    The number of items we would like to add. Must be > 0.
\return         The number of items added. This can only be 0 if count is 0.
*/
int CMarkerBlock::AddData(const TMarker*& pData, size_t count)
{
    assert((LastTime() < pData[0]) && count);
    if ( full() )                           // if buffer is full (already written)...
        clear();                            // ...reuse it
    size_t n = capacity() - size();         // space left in buffer
    size_t nCopy = std::min(n, count);      // events to copy

    // We use memcopy rather than std::copy_n to avoid compiler warnings. We cannot
    // make m_event an array<> as it is part of a union.
    memcpy(&(*end()), pData, nCopy*m_itemSize); // or nCopy*sizeof(TMarker) for speed
    m_nItems += static_cast<uint32_t>(nCopy);
    pData += nCopy;                             // move the pointer on
    SetUnsaved();                               // we have data to save
    return static_cast<int>(nCopy);
}

// Copy events from the current read buffer in a given time range and indicate if we found events past the
// time range or we hit the maximum number. May need a filter argument when we expand the types.
// pData    The target buffer. This is updated.
// r        The range to fetch, including the max number to return. This is adjusted to show
//          done if we find data at or beyond the Upto time.
// pFilter  The filter to use
// Returns  The number of events copied from the buffer.
int CMarkerBlock::GetData(TSTime64*& pData, CSRange& r, const CSFilter* pFilt) const
{
    assert(pData);
    CSFilter::eActive eAct = TestActive(r, pFilt);  // See if anything to do
    if (eAct == CSFilter::eA_none)      // bail now if nothing to do
        return 0;

    citer it(cbegin());
    if (FirstTime() < r.From())         // not from the start?
    {
        it = std::lower_bound(cbegin(), cend(), r.From());
        if (it == cend())
            return 0;                   // nothing in this buffer
    }

    size_t nCopyMax = std::min(static_cast<size_t>(cend() - it), r.Max());
    db_iterator<TSTime64> iOut(pData, sizeof(TSTime64));    // output iterator
    db_iterator<TSTime64> iFrom(iOut);                      // where we started from
    db_iterator<TSTime64> iLimit(iOut + nCopyMax);          // limit of iteration
    if (eAct == CSFilter::eA_all)
    {
        while ((iOut < iLimit) && (r.Upto() > *it))
            iOut.assign(*it++), ++iOut;
    }
    else
    {
        while ((iOut < iLimit) && (it < cend()) && (r.Upto() > *it))
        {
            if (pFilt->Filter(*it))
                iOut.assign(*it), ++iOut;
            ++it;
        }
    }

    pData = &*iOut;                     // update the pointer
    auto nCopy = iOut - iFrom;          // number of items copied
    r.ReduceMax(static_cast<size_t>(nCopy));
    r.SetDone((iOut < iLimit) && (it < cend())); // done if we were timed out

    return static_cast<int>(nCopy);
}

int CMarkerBlock::GetData(TMarker*& pData, CSRange& r, const CSFilter* pFilt) const
{
    assert(pData);
    CSFilter::eActive eAct = TestActive(r, pFilt);  // See if anything to do
    if (eAct == CSFilter::eA_none)      // bail now if nothing to do
        return 0;

    citer it(cbegin());
    if (FirstTime() < r.From())         // not from the start?
    {
        it = std::lower_bound(cbegin(), cend(), r.From());
        if (it == cend())
            return 0;                   // nothing in this buffer
    }

    size_t nCopyMax = std::min(static_cast<size_t>(cend() - it), r.Max());
    iter iOut(pData, sizeof(TMarker));  // output iterator
    iter iFrom(iOut);                   // where we started from
    iter iLimit(iOut + nCopyMax);       // limit of iteration
    if (eAct == CSFilter::eA_all)
    {
        while ((iOut < iLimit) && (r.Upto() > *it))
            iOut.assign(*it++), ++iOut;
    }
    else
    {
        while ((iOut < iLimit) && (it < cend()) && (r.Upto() > *it))
        {
            if (pFilt->Filter(*it))
                iOut.assign(*it), ++iOut;
            ++it;
        }
    }

    pData = &*iOut;                     // update the pointer
    auto nCopy = iOut - iFrom;          // number of items copied
    r.ReduceMax(static_cast<size_t>(nCopy));
    r.SetDone((iOut < iLimit) && (it < cend())); // done if we were timed out

    return static_cast<int>(nCopy);
}

//! Get iterator for a time
/*!
Given a time t, return the index into the buffer where this item would go. This applies
to TSTime64-based data types (TSTime64, TMarker, TExtMark descendents). This can return
cend(), so you must test against cbegin() and cend().
\param t    The time to find the iterator for
\return     Iterator that either points at time t or points where an item at time t would
            be inserted.
*/
CMarkerBlock::citer CMarkerBlock::IterFor(TSTime64 t) const
{
    if (!m_nItems || (FirstTime() > t)) // We want quick results if...
        return cbegin();                // ...no data or if before the buffer
    if (LastTime() < t)                 // Quick return if...
        return cend();                  // ...past the end, else we must search
    return std::lower_bound(cbegin(), cend(), t);
}

//! Edit a marker at a given time
/*!
Find a marker that exactly matches a given time. Then if nCopy exceeds sizeof(TSTime64)
we adjust the marker and mark the block as modified.
\param t    The time to find in the buffer
\param pM   The marker holding replacement information (ignore the time)
\param nCopy The number of bytes of marker info to change (but we do not change the time).
*/
int CMarkerBlock::EditMarker(TSTime64 t, const TMarker* pM, size_t nCopy)
{
    if (!m_nItems || (FirstTime() > t)) // We want quick results if...
        return 0;                       // no data or before the buffer
    if (LastTime() < t)                 // Quick return if...
        return 0;                       // ...past the end, else we must search
    auto it = std::lower_bound(begin(), end(), t);
    if (it->m_time != t)
        return 0;                       // no exact match

    if (nCopy > sizeof(TSTime64))       // if something to change
    {
        nCopy -= sizeof(TSTime64);      // what is to be copied
        bool bChange = memcmp(&it->m_code[0], &pM->m_code[0], nCopy) != 0;
        if (bChange)
        {
            memcpy(&it->m_code[0], &pM->m_code[0], nCopy);
            SetUnsaved();
        }
    }
    return 1;
}

// Find the r.Max() item before r.Upto().
// Returns  found time or -1. Reduce r.Max() by the number of skipped items. If item is
//          found, r.Max() is set zero. If not found, if previous block could not
//          hold data as too early, zero r.Max()  else reduce by skipped items, return -1.
TSTime64 CMarkerBlock::PrevNTime(CSRange& r, const CSFilter* pFilt) const
{
    auto it = IterFor(r.Upto());        // find insertion point for r.Upto()
    if (it > cbegin())                  // if buffer has wanted times...
    {
        size_t nSkipped = 0;            // counts the number we skipped
        if (pFilt)                      // if we must filter...
        {
            do
            {
                --it;                   // back to previous item (must exist)
                if (r.From() > *it)     // if items are no longer useful
                {
                    r.ZeroMax();
                    return -1;
                }

                if (pFilt->Filter(*it) && (++nSkipped >= r.Max()))
                {
                    r.ZeroMax();
                    return *it;
                }
            }while (it > cbegin());
        }
        else
        {
            size_t n = r.Max();             // number to go back
            size_t i = static_cast<size_t>(it - cbegin()); // index of insert position (positive)
            if (i >= n)                     // if wanted time is in this buffer
            {
                r.ZeroMax();                    // reduce count to 0, flags we are done
                TSTime64 t = cbegin()[i-n];     // get the time from the buffer
                return (t >= r.From()) ? t : -1;    // return time, or no such time
            }
            nSkipped = i;               // Not in buffer, so we have skipped this many
        }

        r.ReduceMax(nSkipped);          // reduce count left to do.
        if (!r.SetUpto(FirstTime()))    // new start time, if this kills range...
            r.ZeroMax();                // ...signal we are done
    }
    return -1;
}

//============================== Extended Marker ======================================

//! Add extended marker data into the buffer and report items added
/*!
The buffer can be in any state from empty to full. If it is full, we assume it has
already been written, so we clear it (set it empty). This means that we will always
add at least 1 item to the buffer (unless count is 0 on entry).
\param pData    Reference to a pointer to data to be added. This pointer is moved on
                by the number of items written.
\param count    The number of items we would like to add. _Must_ be > 0.
\return         The number of items added. This can only be 0 if count is 0.
*/
int CExtMarkBlock::AddData(const TExtMark*& pData, size_t count)
{
    assert(LastTime() < pData[0]);
    if ( full() )                           // if buffer is full (already written)...
        clear();                            // ...reuse it
    size_t n = capacity() - size();         // space left in buffer
    size_t nCopy = std::min(n, count);      // events to copy

    // We use memcopy rather than std::copy_n to avoid compiler warnings. We cannot
    // make m_event an array<> as it is part of a union.
    memcpy(&(*end()), pData, nCopy*m_itemSize);
    m_nItems += static_cast<uint32_t>(nCopy);

    // We cannot just say pData += nCopy as the size is not fixed
    citer iData(pData, m_itemSize);         // use an iterator...
    iData += nCopy;                         // ...to move the pointer onwards
    pData = &(*iData);                      // Save the new pointer
    SetUnsaved();                           // we have new data to save
    return static_cast<int>(nCopy);
}

// Copy waveform data from an extended marker block - it must be a WaveMark channel. Remember that
// The number of traces is TChanHead.m_nColumns (expected to be 1-4) and the number of items per trace
// is TChanHead.m_nRows. Each item starts at the time in the item and the last is at this time plus
// m_tDivide*(m_nRows-1) clock ticks.
// pData    The target buffer. This is updated
// r        The range to fetch, including the max number to return and the trace number. This is adjusted to show
//          done if we find data at or beyond the Upto time. The trace is assumed valid.
// tFirst   Updated if r.First() is true and we get some data.
// Returns  The number of contiguous data points, or 0 if none or not contig, or -ve error, in
//          which case the range is adjusted so it says we are done.
int CExtMarkBlock::GetData(short*& pData, CSRange& r, TSTime64& tFirst, const CSFilter* pFilt) const
{
    assert(pData && r.ChanHead() && (r.ChanHead()->m_chanKind == AdcMark));
    if (!r.ChanHead())
        return 0;
    const TChanHead& ch(*r.ChanHead());     // to save lots of typing
    if (ch.m_chanKind != AdcMark)
        return CHANNEL_TYPE;

    CSFilter::eActive eAct = TestActive(r, pFilt);  // See if anything to do
    if (eAct == CSFilter::eA_none)          // bail now if nothing to do
        return 0;

    TSTime64 tAdd = ch.m_tDivide * (ch.m_nRows-1);

    citer it(cbegin());
    if (FirstTime() < r.From()-tAdd)        // not from the start?
    {
        it = std::lower_bound(cbegin(), cend(), r.From()-tAdd);
        if (it == cend())                   // if not found...
        {
            r.SetDone();                    // Nothing to be found...
            return 0;                       // ...in this buffer
        }
    }

    // If the buffer were empty, FirstTime() would be -1, so we would not be here.
    if (it->m_time >= r.Upto())             // if this item is too late...
    {
        r.SetDone();
        r.ZeroMax();                        // no possibility of read continuing
        return 0;                           // ...nothing in this buffer
    }

    // See if the user wants to read data from a specified column
    int nColOffs = 0;                       // if we are to collect a specific column
    if (pFilt) 
    {
        nColOffs = pFilt->GetColumn();      // Get and check the desired column
        if ((nColOffs < 0) || (nColOffs >= ch.m_nColumns))
            nColOffs = 0;
    }

    int nTotPoints = 0;                     // points we have got
    do
    {
        // To get here we have data at or after r.From(). However, we might be filtered out, so
        // if we have a non-accept all filter we must scan for a wanted item
        if (eAct != CSFilter::eA_all)
        {
            while (!pFilt->Filter(*it) && (++it != cend()))
                if (it->m_time >= r.Upto())
                    return nTotPoints;
            if (it == cend())
                return nTotPoints;
        }

        // To get here we have some data in our time range and the filter is accepting the current value.
        // Now find the index to the required item.
        TSTime64 tThis = it->m_time;        // time of this item
        TSTime64 tBefore = r.From() - tThis;// how far before the desired time is this item
        int index = 0;                      // first point we want attached to this item
        if (tBefore > 0)                    // We may not want the first point
        {
            index = (int)((tBefore + ch.m_tDivide -1) / ch.m_tDivide);  // first desired point - this must be in sensible range
            assert(index < ch.m_nRows);     // detect madness
            tThis += index * ch.m_tDivide;  // time of first wanted point
        }

        if (r.First())                      // should we set the first time?
        {
            tFirst = tThis;                 // say we have data starting at this time
            r.NotFirst();                   // no longer the first read point
        }
        else if (tThis != r.From())
        {
            r.SetDone();                    // discontinuous data, so we are done
            r.ZeroMax();                    // no chance of continuing
            return nTotPoints;
        }

        int nMaxCopy = ch.m_nRows - index;  // points to copy
        if (it->m_time + tAdd >= r.Upto())  // if this item exceeds the time limit
            nMaxCopy = (int)((r.Upto() - tThis + ch.m_tDivide - 1) / ch.m_tDivide);
        if (nMaxCopy > (int)r.Max())        // Max() is always int range for read
            nMaxCopy = (int)r.Max();
        if (nMaxCopy > 0)
        {
            const TAdcMark* pEM = (TAdcMark*)(&(*it));  // Point at our extended marker
            const short *pFrom = pEM->Shorts() + (nColOffs + (index * ch.m_nColumns)); // point at desired item
            nTotPoints += nMaxCopy;
            r.SetFrom(tThis + nMaxCopy*ch.m_tDivide);   // next time to read from
            for (int i=0; i<nMaxCopy; ++i)
            {
                *pData++ = *pFrom;
                pFrom += ch.m_nColumns;
            }
            r.ReduceMax(nMaxCopy);
        }

        ++it;                               // move on to the next item
    }while(r.HasRange() && (it != cend()) && (it->m_time <= r.From()));
    return nTotPoints;
}


// Copy events from the current read buffer in a given time range and indicate if we found events past the
// time range or we hit the maximum number. May need a filter argument when we expand the types.
// pData    The target buffer. This is updated.
// r        The range to fetch, including the max number to return. This is adjusted to show
//          done if we find data at or beyond the Upto time.
// pFilter  The filter to use
// Returns  The number of events copied from the buffer.
int CExtMarkBlock::GetData(TSTime64*& pData, CSRange& r, const CSFilter* pFilt) const
{
    assert(pData);
    CSFilter::eActive eAct = TestActive(r, pFilt);  // See if anything to do
    if (eAct == CSFilter::eA_none)      // bail now if nothing to do
        return 0;

    citer it(cbegin());
    if (FirstTime() < r.From())         // not from the start?
    {
        it = std::lower_bound(cbegin(), cend(), r.From());
        if (it == cend())
            return 0;                   // nothing in this buffer
    }

    size_t nCopyMax = std::min(static_cast<size_t>(cend() - it), r.Max());
    db_iterator<TSTime64> iOut(pData, sizeof(TSTime64));    // output iterator
    db_iterator<TSTime64> iFrom(iOut);                      // where we started from
    db_iterator<TSTime64> iLimit(iOut + nCopyMax);          // limit of iteration
    if (eAct == CSFilter::eA_all)
    {
        while ((iOut < iLimit) && (r.Upto() > *it))
            iOut.assign(*it++), ++iOut;
    }
    else
    {
        while ((iOut < iLimit) && (it < cend()) && (r.Upto() > *it))
        {
            if (pFilt->Filter(*it))
                iOut.assign(*it), ++iOut;
            ++it;
        }
    }

    pData = &*iOut;                     // update the pointer
    auto nCopy = iOut - iFrom;          // number of items copied
    r.ReduceMax(static_cast<size_t>(nCopy));
    r.SetDone((iOut < iLimit) && (it < cend())); // done if we were timed out

    return static_cast<int>(nCopy);
}

int CExtMarkBlock::GetData(TMarker*& pData, CSRange& r, const CSFilter* pFilt) const
{
    assert(pData);
    CSFilter::eActive eAct = TestActive(r, pFilt);  // See if anything to do
    if (eAct == CSFilter::eA_none)      // bail now if nothing to do
        return 0;

    citer it(cbegin());
    if (FirstTime() < r.From())         // not from the start?
    {
        it = std::lower_bound(cbegin(), cend(), r.From());
        if (it == cend())
            return 0;                   // nothing in this buffer
    }

    size_t nCopyMax = std::min(static_cast<size_t>(cend() - it), r.Max());
    db_iterator<TMarker> iOut(pData, sizeof(TMarker));     // output iterator
    db_iterator<TMarker> iFrom(iOut);                      // where we started from
    db_iterator<TMarker> iLimit(iOut + nCopyMax);          // limit of iteration
    if (eAct == CSFilter::eA_all)
    {
        while ((iOut < iLimit) && (r.Upto() > *it))
            iOut.assign(*it++), ++iOut;
    }
    else
    {
        while ((iOut < iLimit) && (it < cend()) && (r.Upto() > *it))
        {
            if (pFilt->Filter(*it))
                iOut.assign(*it), ++iOut;
            ++it;
        }
    }

    pData = &*iOut;                     // update the pointer
    auto nCopy = iOut - iFrom;          // number of items copied
    r.ReduceMax(static_cast<size_t>(nCopy));
    r.SetDone((iOut < iLimit) && (it < cend())); // done if we were timed out

    return static_cast<int>(nCopy);
}

int CExtMarkBlock::GetData(TExtMark*& pData, CSRange& r, const CSFilter* pFilt) const
{
    assert(pData);
    CSFilter::eActive eAct = TestActive(r, pFilt);  // See if anything to do
    if (eAct == CSFilter::eA_none)      // bail now if nothing to do
        return 0;

    citer it(cbegin());
    if (FirstTime() < r.From())         // not from the start?
    {
        it = std::lower_bound(cbegin(), cend(), r.From());
        if (it == cend())
            return 0;                   // nothing in this buffer
    }

    size_t nCopyMax = std::min(static_cast<size_t>(cend() - it), r.Max());
    iter iOut(pData, m_itemSize);       // output iterator
    iter iFrom(iOut);                   // where we started from
    iter iLimit(iOut + nCopyMax);       // limit of iteration
    if (eAct == CSFilter::eA_all)
    {
        while ((iOut < iLimit) && (r.Upto() > *it))
            iOut.assign(*it++), ++iOut;
    }
    else
    {
        while ((iOut < iLimit) && (it < cend()) && (r.Upto() > *it))
        {
            if (pFilt->Filter(*it))
                iOut.assign(*it), ++iOut;
            ++it;
        }
    }

    pData = &*iOut;                     // update the pointer
    auto nCopy = iOut - iFrom;          // number of items copied
    r.ReduceMax(static_cast<size_t>(nCopy));
    r.SetDone((iOut < iLimit) && (it < cend())); // done if we were timed out

    return static_cast<int>(nCopy);
}

//! Get const iterator to where time t woould be placed in the block
/*!
This can return cend(), so you must test against cbegin() and cend().
\param t    The time to locate the position of in the block.
\return     cbegin() if before or at the first item, cend() if beyond the last, else
a const iterator to where it would go.
*/
CExtMarkBlock::citer CExtMarkBlock::IterFor(TSTime64 t) const
{
    if (!m_nItems || (FirstTime() > t)) // We want quick results if...
        return cbegin();                // ...no data or if before the buffer
    if (LastTime() < t)                 // Quick return if...
        return cend();                  // ...past the end, else we must search
    return std::lower_bound(cbegin(), cend(), t);
}

//! Edit a marker at a given time
/*!
Find a marker that exactly matches a given time. Then if nCopy exceeds sizeof(TSTime64)
we adjust the marker and mark the block as modified.
\param t    The time to find in the buffer
\param pM   The marker holding replacement information (ignore the time)
\param nCopy The number of bytes of marker info to change (but we do not change the time).
*/
int CExtMarkBlock::EditMarker(TSTime64 t, const TMarker* pM, size_t nCopy)
{
    if (!m_nItems || (FirstTime() > t)) // We want quick results if...
        return 0;                       // no data or before the buffer
    if (LastTime() < t)                 // Quick return if...
        return 0;                       // ...past the end, else we must search
    auto it = std::lower_bound(begin(), end(), t);
    if (it->m_time != t)
        return 0;                       // no exact match

    if (nCopy > sizeof(TSTime64))       // if something to change
    {
        nCopy -= sizeof(TSTime64);      // what is to be copied
        bool bChange = memcmp(&it->m_code[0], &pM->m_code[0], nCopy) != 0;
        if (bChange)
        {
            memcpy(&it->m_code[0], &pM->m_code[0], nCopy);
            SetUnsaved();
        }
    }
    return 1;
}

// Find the r.Max() item before r.Upto().
// Returns  found time or -1. Reduce r.Max() by the number of skipped items. If item is
//          found, r.Max() is set zero. If not found, if previous block could not
//          hold data as too early, zero r.Max()  else reduce by skipped items, return -1.
TSTime64 CExtMarkBlock::PrevNTime(CSRange& r, const CSFilter* pFilt) const
{
    auto it = IterFor(r.Upto());        // find insertion point for r.Upto()
    if (it > cbegin())                  // if buffer has wanted times...
    {
        size_t nSkipped = 0;            // counts the number we skipped
        if (pFilt)                      // if we must filter...
        {
            do
            {
                --it;                   // back to previous item (must exist)
                if (r.From() > *it)     // if items are no longer useful
                {
                    r.ZeroMax();
                    return -1;
                }

                if (pFilt->Filter(*it) && (++nSkipped >= r.Max()))
                {
                    r.ZeroMax();
                    return *it;
                }
            }while (it > cbegin());
        }
        else
        {
            size_t n = r.Max();             // number to go back
            size_t i = static_cast<size_t>(it - cbegin()); // index of insert position (positive)
            if (i >= n)                     // if wanted time is in this buffer
            {
                r.ZeroMax();                    // reduce count to 0, flags we are done
                TSTime64 t = cbegin()[i-n];     // get the time from the buffer
                return (t >= r.From()) ? t : -1;    // return time, or no such time
            }
            nSkipped = i;               // Not in buffer, so we have skipped this many
        }

        r.ReduceMax(nSkipped);          // reduce count left to do.
        if (!r.SetUpto(FirstTime()))    // new start time, if this kills range...
            r.ZeroMax();                // ...signal we are done
    }
    return -1;
}

//! Search an extended marker block for waveform data in a range and count backwards through it
/*!
Search backwards in the range defined by r. If this is the first call, find the last data in the
range, otherwise data must occur exactly lDvd before r.Upto(), else we do not match.
\param r        Defines the time range to search and if this is the first search or not.
\param pFilt    Defines a filter to apply to the the extended marker blocks
\param nRow     The number of logical items starting at each item time
\param lDvd     The sample interval in clock ticks between the items
*/
TSTime64 CExtMarkBlock::PrevNTimeW(CSRange& r, const CSFilter* pFilt, size_t nRow, TSTime64 lDvd) const
{
    if ((nRow == 0) || (lDvd <= 0))
        return CHANNEL_TYPE;

    auto it = IterFor(r.Upto());        // find insertion point for this time
    if (it == cbegin())                 // if at start
        return -1;                      // there is nothing here to find

    TSTime64 tWidth = static_cast<int>(nRow-1) * lDvd;

    do
    {
        --it;                               // point at first item that might hold data
        while (pFilt && !pFilt->Filter(*it))
            if (it == cbegin())             // if no data before, we are done
                return r.LastFound();       // -1 if first, else last time we found, kill range
            else if (r.From() > it->m_time + tWidth)
                return r.LastFound();       //
            else
                --it;

        // To get here it points at an item that we want to consider and it overlaps the range
        size_t lastI;                       // index to final point we consider
        TSTime64 t = it->m_time;            // time of first item
        if (t + tWidth < r.Upto())
            lastI = nRow - 1;               // last point is the wanted one
        else
            lastI = static_cast<size_t>((r.Upto() - t - 1) / lDvd);  // last point that is before r.Upto()

        size_t firstI;                      // index to first point we can use
        if (t >= r.From())
            firstI = 0;                     // first point is included
        else
            firstI = static_cast<size_t>((r.From() - t) / lDvd);

        assert(lastI >= firstI);            // detect madness
        if ((lastI - firstI + 1) > r.Max())
            firstI = lastI - r.Max() + 1;   // limit backwards distance

        TSTime64 tFirst = t + static_cast<int>(firstI) * lDvd;
        if (!r.First())                     // First time we accept first time before Upto()
        {
            TSTime64 tLast = t + static_cast<int>(lastI) * lDvd;
            if ( tLast + lDvd != r.Upto() ) // data MUST be contiguous
                return r.LastFound();       // say we are done, return last time
        }
        else
            r.NotFirst();                   // no longer first time

        r.SetUpto(tFirst);                  // set new last time
        r.ReduceMax(lastI-firstI+1);        // reduce items to go back over
    }while ((it != cbegin()) && r.HasRange());

    return r.Upto();                        // cannot be first time if we get here
}

//======================================= CAdcBlock ========================================

//! Get a reference to the final TWave in the block. This is only valid if m_nItems>0.
TWave<short>& CAdcBlock::back()
{
    assert(m_nItems);
    if (m_pBack)
        return *m_pBack;

    unsigned i = m_nItems;
    witer w(&m_adc);
    while (i > 1)
        ++w, --i;
    m_pBack = &(*w);            // save last item
    return *m_pBack;
}

CAdcBlock::witer CAdcBlock::end()
{
    if (m_nItems == 0)
        return begin();
    witer w(&back());
    return ++w;
}

const TWave<short>& CAdcBlock::back() const
{
    assert(m_nItems);
    if (m_pBack)
        return *m_pBack;

    unsigned i = m_nItems;
    cwiter w(&m_adc);
    while (i > 1)
        ++w, --i;
    m_pBack = const_cast<TWave<short>*>(&(*w)); // save last item
    return *m_pBack;
}

CAdcBlock::cwiter CAdcBlock::cend() const
{
    if (m_nItems == 0)
        return cbegin();
    cwiter w(&back());
    return ++w;
}

TSTime64 CAdcBlock::LastTime() const
{
    if (m_nItems)
    {
        const TWave<short>& last = back();
        return last.m_startTime + (last.m_nItems-1) * m_tDivide;
    }
    else
        return -1;
}

//! Add data into the buffer
/*!
Waveform buffers are more complicated than event buffers as we have to deal with
contiguous and non-contiguous additions. A non-contiguous addition means that we
need extra space for a sub-block header. If the buffer becomes full as a result
of adding data, the caller is responsible for writing it. Unlike the event-based
buffers, writing to a full buffer does not clear it.
\param pData    Reference to a pointer to the data to be added. If we add data,
                this is moved on by the number of points added.
\param count    The number of points to add. This must be > 0.
\param tFrom    The time of the first point to add. If the buffer holds data, this
                <i>must</i> be greater than the last time in the buffer.
\return         If this returns 0 (and count > 0) the buffer is full and needs
                to be written and cleared before we can add more data. Otherwise
                it returns the number of points written.
*/
int CAdcBlock::AddData(const short*& pData, size_t count, TSTime64 tFrom)
{
    assert(LastTime() < tFrom);     // This should never happen

    // Start by working out if this is a contiguous addition or non-contiguous.
    // Work out the maximum number of items we can add and an iterator to the
    // first item to add.
    witer w(begin());               // Iterator for the first wave segment
    bool bIsContiguous = false;
    size_t nMax;                    // max items to copy
    if (m_nItems)
    {
        bIsContiguous = tFrom == LastTime() + m_tDivide;   // can append to last block
        if (bIsContiguous)
        {
            nMax = SpaceContiguous();
            w = m_pBack;            // set iterator to last segment (to expand it)
        }
        else
        {
            nMax = SpaceNonContiguous();
            w = end();              // new segment at the end of the buffer
        }
    }
    else                            // easy case. Add data to an empty buffer.
        nMax = max_size();

    size_t nCopy = std::min(nMax, count);
    if (nCopy)                      // If there is space, copy what we can
    {
        if (!bIsContiguous)
        {
            w->m_startTime = tFrom; // initialise the new wave segment
            w->m_nItems = 0;        // no items yet
            w->m_pad = 0;           // not sure what this is for yet!
            ++m_nItems;             // we have another segment
            m_pBack = &*w;          // swe have a new last segment
        }

        memcpy(w->m_data + w->m_nItems, pData, nCopy*sizeof(short));
        w->m_nItems += static_cast<uint32_t>(nCopy); // set number of items in the segment
        pData += nCopy;             // move the pointer onwards
        SetUnsaved();               // we have new data to save
    }
    return static_cast<int>(nCopy);
}

// Given that we will be adding contiguous data to the end of a buffer, how
// many data points could we add.
size_t CAdcBlock::SpaceContiguous() const
{
    const TWave<short>& last = back();    // the block we are adding to
    const short* pNext = last.m_data + last.m_nItems;
    const short* pEnd = reinterpret_cast<const short*>(&m_doParent) + (DBSize / sizeof(short));
    return pEnd >= pNext ? pEnd - pNext : 0;
}

// Given that we will be adding a new TWave<short>, how many data points could we put in it?
// beware that cend() could be beyond the block.
size_t CAdcBlock::SpaceNonContiguous() const
{
    const TWave<short>& last = *cend();     // the block we will be adding to
    const short* pNext = last.m_data;       // we will add from here
    const short* pEnd = reinterpret_cast<const short*>(&m_doParent) + (DBSize / sizeof(short));
    return pEnd >= pNext ? pEnd - pNext : 0;
}

// Read data into a pointer and move the pointer onwards. If m_bFirst is true, we will accept
// the first data point at or after r.m_tFrom. If it is false then the first data point MUST
// be at m_tFrom, otherwise we must return 0 points.
// pData    Points at the buffer to be filled.
// r        The time range. If no more data is possible, call r.Done(true). If more is possible
//          update m_tFrom to the next contiguous time. m_bFirst determines if we must match
//          the start time or not.
// tFirst   Returned holding the time of the first point if m_bFirst is true and we get data.
// pFilter  Does not apply to this class (used when readng Adc from Adcmark)
// Returns  The number of contiguous data points, or 0 if none or not contig, or -ve error, in
//          which case the range is adjusted so it says we are done.
int CAdcBlock::GetData(short*& pData, CSRange& r, TSTime64& tFirst, const CSFilter* pFilter) const
{
    size_t nSect = m_nItems;                // number of discontiguous sections
    cwiter it = cbegin();                   // to read through wave sections
    while (nSect)
    {
        TSTime64 tStart = it->m_startTime;  // get time of first item
        if (tStart >= r.Upto())             // if start is past the range...
            break;

        // If the end time is before the range start, skip to next range
        assert(it->m_nItems);
        TSTime64 tLast = tStart + (it->m_nItems-1)*m_tDivide;
        if (tLast >= r.From())
        {
            // does this section contain some wanted data?
            TSTime64 tDiff = r.From() - tStart; // time offset to first item
            if (!r.First() && tDiff)        // must be contig, and might not be
            {
                if ((tDiff < 0) ||          // -ve is definitely non-contig
                    (tDiff % m_tDivide))    // non-zero means non-contig
                {
                    r.ZeroMax();            // cannot continue
                    break;                  // not contiguous, so done
                }
            }
            else if (tDiff < 0)
                tDiff = 0;

            // If we get here we have at least one wanted data point.
            size_t index = static_cast<size_t>((tDiff + m_tDivide - 1) / m_tDivide);
            size_t nItems = it->m_nItems;   // Number in the buffer
            if (tLast >= r.Upto())          // do we need to reduce count in the buffer
                nItems = static_cast<size_t>((r.Upto() - tStart + m_tDivide - 1)/m_tDivide);
            assert(index <= nItems);        // detect logic failure
            size_t nCopy = std::min(r.Max(), nItems - index);     // n to copy
            memcpy(pData, &(it->m_data[index]), nCopy*sizeof(short));   // do the copy
            pData += nCopy;                 // update the source pointer
            r.ReduceMax(nCopy);             // update the range
            if (r.First())                  // if this was the first of a range
            {
                r.NotFirst();               // no longer first
                tFirst = tStart + index * m_tDivide; // set first time
            }

            // Now record where we have got to and detect if no further contiguous read
            // is possible, to avoid un-necessary reads.
            r.SetFrom(tStart + (index+nCopy)*m_tDivide); // for next time
            if ((index + nCopy >= it->m_nItems) &&  // reached end of segment, and...
                (nSect > 1))                // ...and more segments (so not contiguous)
            {
                r.SetDone();                // stop this read
                r.ZeroMax();                // prevent further reads
            }
            return static_cast<int>(nCopy);
        }

        ++it;                   // onto the next section
        --nSect;                // reduce count of sections to search
    }

    r.SetDone(true);            // found nothing, so assume done
    return 0;
}

// Given contiguous data in pData of size count that starts at tFrom, use it to update
// any matching data points in the block.
// first    If the return value is >0, this is set to the index in pData of the first data
//          point that is used.
// returns  The number of items in pData that span the block (they might all be in gaps).
//          0 if no overlap and pData starts after the block, -1 if no overlap and pData
//          ends before the buffer.
int CAdcBlock::ChangeWave(const short* pData, size_t count, TSTime64 tFrom, size_t& first)
{
    if (m_nItems == 0)              // Write buffer, but no data in it...
        return -1;                  // ...we are done
    TSTime64 tBLast = LastTime();   // get last time in the block
    if (tFrom > tBLast)             // see if beyond the end...
        return 0;                   // ...if so, we are done
    TSTime64 tEnd = tFrom + count*m_tDivide - m_tDivide;   // time of last point
    TSTime64 tBFirst = FirstTime(); // first item time in the block
    if (tEnd < tBFirst)             // if all data is before the block...
        return -1;                  // ...we are done

    // We can now work out the first and last indices that will be involved.
    // If the change data does not align exactly we round down one place.
    if (tFrom > tBFirst)
        first = 0;                  // use the first point
    else
        first = static_cast<size_t>((tBFirst - tFrom) / m_tDivide);

    size_t last;                    // index beyond last used
    if (tEnd <= tBLast)             // If change data is no longer than the buffer...
        last = count-1;             // ...then last index is the final one
    else                            // else work out the final index
        last = static_cast<size_t>((tBLast - tFrom) / m_tDivide);

    // Now work through each wave segment in turn to update data
    for (auto it = begin(); it != end(); ++it)
    {
        size_t blIndex, daIndex;        // start index for block and data
        if (it->m_startTime >= tEnd)    // if beyond our data...
            break;                      // ...we are done
        TSTime64 tSgEnd = it->m_startTime + it->m_nItems*m_tDivide;
        if (tFrom >= tSgEnd)            // If before our data...
            continue;                   // ...skip to the next segment
        size_t nCopy;
        if (tFrom >= it->m_startTime)
        {
            daIndex = 0;            // will use 0, or not reached data
            blIndex = static_cast<size_t>((tFrom - it->m_startTime)/m_tDivide);
            nCopy = std::min(count, it->m_nItems - blIndex);
        }
        else
        {
            daIndex = static_cast<size_t>((it->m_startTime-tFrom)/m_tDivide);
            blIndex = 0;
            nCopy = std::min(static_cast<size_t>(it->m_nItems), count-daIndex);
        }
        std::copy_n(pData+daIndex, nCopy, it->m_data+blIndex);
        SetUnsaved();               // this block needs writing
    }

    return static_cast<int>(last - first + 1);  // points used (equal indices means 1 point)
}

//! Find the r.Max() point before a given time
/*!
If r.Max() is 1, find the last Adc point before r.Upto(). If r.Max() is > 1, find the
start time t where a contiguous read of r.Max() points from t to r.Upto() would reach
the last point before r.Upto(). If there is a gap, the read may extend past this point,
but is still ounts as finding it. That is, there can be a gap before r.Upto(), but points
before must be contiguous.
\param r     This defines the time range for the search and the number of items we
             want to go back.
\param pFilt If not nullptr (and the block is a marker or an extended marker type), use
             this to filter the data. That is, items not in the filter are ignored.
\return      -1 if not found (but r.Max() is reduced by the number of points skipped and
             the range is adjusted to replace the new end (r.tUpto) for the next search
             in the previous block). Otherwise, the found time and r.Max() is set 0.
*/
TSTime64 CAdcBlock::PrevNTime(CSRange& r, const CSFilter* pFilt) const
{
    // Start by finding the first data point before r.Upto() in the block. This means find the
    // first segment that contains the r.Upto() or later. We must also save the previous one
    // as we may need to go back if this is the first data point.
    auto it = cbegin();                 // get the first segment
    if (it->m_startTime >= r.Upto())    // if beyond the end time...
        return -1;                      // nothing to find here.
    auto last(it);                      // and save it as last
    size_t index = last->m_nItems;      // index to last to first point
    do
    {
        TSTime64 tSgStart = it->m_startTime;
        if (tSgStart >= r.Upto())       // last point of previous segment
            break;                      // cannot happen first time round
        else
        {
            TSTime64 tSgEnd = tSgStart + it->m_nItems*m_tDivide;
            if (tSgEnd >= r.Upto())     // time is in this block
            {
                index = 1+static_cast<size_t>((r.Upto() - 1 - it->m_startTime)/m_tDivide);
                last = it;              // set last to the one to use
                break;
            }
        }
        last = it;                      // save last in case in the gap
        index = last->m_nItems;         // in case we fall off the end
        ++it;
    }while(it != cend());

    // If we get here, last points at the segment, index is the index to the point
    // beyond the one before r.Upto(), which may not exist! Now check if this time
    // is acceptable - can only be non-contiguous if this is the first time.
    TSTime64 tUpto = last->m_startTime + index * m_tDivide;
    if (!r.First())
    {
        if (tUpto != r.Upto())
        {
            r.ZeroMax();                // flag we have found it
            return r.Upto();            // last found time is the result
        }
    }

    // Now figure out the index of the point we want.
    if (index >= r.Max())               // we have what we want
    {
        index -= r.Max();               // index of desired point (for time)               
        r.ZeroMax();                    // so caller knows we are done
    }
    else if (last != cbegin())          // if not first segment
    {
        index = 0;                      // no more contiguous stuff...
        r.ZeroMax();                    // ...so we are done
    }
    else                                // need to look at previous block
    {
        r.ReduceMax(index);             // reduce number to skip
        index = 0;                      // give a new end time
    }
    
    TSTime64 t = last->m_startTime + index * m_tDivide;
    if (t < r.From())                   // if found a time before we wanted
    {
        r.ZeroMax();                    // we are done, whatever
        size_t inc = (size_t)((r.From()-t+m_tDivide-1)/m_tDivide);
        if (index+inc < last->m_nItems) // can use some points
            t += inc*m_tDivide;         // new found time
        else
            t = r.First() ? -1 : r.Upto();
    }

    r.NotFirst();                       // clear the first time flag
    r.SetUpto(t);                       // in case we have to continue
    return t;
}

//=========================== CRealWaveBlock =================================
// Get a reference to the final TWave in the block. This is only valid if m_nItems>0.
TWave<float>& CRealWaveBlock::back()
{
    assert(m_nItems);
    if (m_pBack)
        return *m_pBack;

    unsigned i = m_nItems;
    witer w(&m_realwave);
    while (i > 1)
        ++w, --i;
    m_pBack = &(*w);            // save last item
    return *m_pBack;
}

CRealWaveBlock::witer CRealWaveBlock::end()
{
    if (m_nItems == 0)
        return begin();
    witer w(&back());
    return ++w;
}

const TWave<float>& CRealWaveBlock::back() const
{
    assert(m_nItems);
    if (m_pBack)
        return *m_pBack;

    unsigned i = m_nItems;
    cwiter w(&m_realwave);
    while (i > 1)
        ++w, --i;
    m_pBack = const_cast<TWave<float>*>(&(*w)); // save last item
    return *m_pBack;
}

CRealWaveBlock::cwiter CRealWaveBlock::cend() const
{
    if (m_nItems == 0)
        return cbegin();
    cwiter w(&back());
    return ++w;
}

TSTime64 CRealWaveBlock::LastTime() const
{
    if (m_nItems)
    {
        const TWave<float>& last = back();
        return last.m_startTime + (last.m_nItems-1) * m_tDivide;
    }
    else
        return -1;
}

// Add as much data as we can into the buffer. Return the number of items added and
// move the pointer on by this number of items. We will add the data to the end of
// the current buffer, if there is space.
int CRealWaveBlock::AddData(const float*& pData, size_t count, TSTime64 tFrom)
{
    assert(LastTime() < tFrom);
    witer w(begin());
    bool bIsContiguous = false;
    size_t nMax;            // max items to copy
    if (m_nItems)
    {
        bIsContiguous = tFrom == LastTime() + m_tDivide;   // can append to last block
        if (bIsContiguous)
        {
            nMax = SpaceContiguous();
            w = m_pBack;            // set iterator to desired item
        }
        else
        {
            nMax = SpaceNonContiguous();
            w = end();
        }
    }
    else    // easy case. Add data to an empty buffer.
        nMax = max_size();

    size_t nCopy = std::min(nMax, count);
    if (nCopy)
    {
        if (!bIsContiguous)
        {
            w->m_startTime = tFrom; // initialise the new wave segment
            w->m_nItems = 0;        // no items yet
            w->m_pad = 0;           // not sure what this is for yet!
            ++m_nItems;             // we have another segment
            m_pBack = &*w;          // so move the back pointer onwards
        }

        memcpy(&(w->m_data[w->m_nItems]), pData, nCopy*sizeof(float));
        w->m_nItems += static_cast<uint32_t>(nCopy);  // set number of items in the block
        pData += nCopy;             // move the pointer onwards
        SetUnsaved();               // we have new data to save
    }
    return static_cast<int>(nCopy);
}

// Given that we will be adding contiguous data to the end of a buffer, how
// many data points could we add.
size_t CRealWaveBlock::SpaceContiguous() const
{
    const TWave<float>& last = back();    // the block we are adding to
    const float* pNext = last.m_data + last.m_nItems;
    const float* pEnd = reinterpret_cast<const float*>(&m_doParent) + (DBSize / sizeof(float));
    return pEnd >= pNext ? pEnd - pNext : 0;
}

// Given that we will be adding a new TWave<short>, how many data points could we put in it?
// beware that cend() could be beyond the block.
size_t CRealWaveBlock::SpaceNonContiguous() const
{
    const TWave<float>& last = *cend();     // the block we will be adding to
    const float* pNext = last.m_data;       // we will add from here
    const float* pEnd = reinterpret_cast<const float*>(&m_doParent) + (DBSize / sizeof(float));
    return pEnd >= pNext ? pEnd - pNext : 0;
}

// Read data into a pointer and move the pointer onwards. If m_bFirst is true, we will accept
// the first data point at or after r.m_tFrom. If it is false then the first data point MUST
// be at m_tFrom, otherwise we must return 0 points.
// pData    Points at the buffer to be filled.
// r        The time range. If no more data is possible, call r.Done(true). If more is possible
//          update m_tFrom to the next contiguous time. m_bFirst determines if we must match
//          the start time or not.
// tFirst   Returned holding the time of the first point if m_bFirst is true and we get data.
// pFilter  Does not apply to this class (used when readng Adc from Adcmark)
// Returns  The number of contiguous data points, or 0 if none or not contig, or -ve error, in
//          which case the range is adjusted so it says we are done.
int CRealWaveBlock::GetData(float*& pData, CSRange& r, TSTime64& tFirst, const CSFilter* pFilter) const
{
    size_t nSect = m_nItems;                // number of discontiguous sections
    cwiter it = cbegin();                   // to read through wave sections
    while (nSect)
    {
        TSTime64 tStart = it->m_startTime;  // get time of first item
        if (tStart >= r.Upto())             // if start is past the range...
            break;

        // If the end time is before the range start, skip to next range
        assert(it->m_nItems);
        TSTime64 tLast = tStart + (it->m_nItems-1)*m_tDivide;
        if (tLast >= r.From())
        {
            // does this section contain some wanted data?
            TSTime64 tDiff = r.From() - tStart; // time offset to first item
            if (!r.First() && tDiff)        // must be contig, and might not be
            {
                if ((tDiff < 0) ||          // -ve is definitely non-contig
                    (tDiff % m_tDivide))    // non-zero means non-contig
                {
                    r.ZeroMax();            // cannot continue
                    break;                  // not contiguous, so done
                }
            }
            else if (tDiff < 0)
                tDiff = 0;

            // If we get here we have at least one wanted data point.
            size_t index = static_cast<size_t>((tDiff + m_tDivide - 1) / m_tDivide);
            size_t nItems = it->m_nItems;   // Number in the buffer
            if (tLast >= r.Upto())          // do we need to reduce count in the buffer
                nItems = static_cast<size_t>((r.Upto() - tStart + m_tDivide - 1)/m_tDivide);
            assert(index <= nItems);        // detect logic failure
            size_t nCopy = std::min(r.Max(), nItems - index);     // n to copy
            memcpy(pData, &(it->m_data[index]), nCopy*sizeof(float));   // do the copy
            pData += nCopy;                 // update the source pointer
            r.ReduceMax(nCopy);             // update the range
            if (r.First())                  // if this was the first of a range
            {
                r.NotFirst();               // no longer first
                tFirst = tStart + index * m_tDivide; // set first time
            }

            // Now record where we have got to and detect if no further contiguous read
            // is possible, to avoid un-necessary reads.
            r.SetFrom(tStart + (index+nCopy)*m_tDivide); // for next time
            if ((index + nCopy >= it->m_nItems) &&  // reached end of segment, and...
                (nSect > 1))                // ...and more segments (so not contiguous)
            {
                r.SetDone();                // stop this read
                r.ZeroMax();                // We know we must stop, so we are done
            }
            return static_cast<int>(nCopy);
        }

        ++it;                   // onto the next section
        --nSect;                // reduce count of sections to search
    }

    r.SetDone(true);            // found nothing, so assume done
    return 0;
}

// Given contiguous data in pData of size count that starts at tFrom, use it to update
// any matching data points in the block.
// first    If the return value is >0, this is set to the index in pData of the first data
//          point that is used.
// returns  The number of items in pData that span the block (they might all be in gaps).
//          0 if no overlap and pData starts after the block, -1 if no overlap and pData
//          ends before the buffer.
int CRealWaveBlock::ChangeWave(const float* pData, size_t count, TSTime64 tFrom, size_t& first)
{
    if (m_nItems == 0)              // Write buffer, but no data...
        return -1;                  // ...we are done
    TSTime64 tBLast = LastTime();   // get last time in the block
    if (tFrom > tBLast)             // see if beyond the end...
        return 0;                   // ...if so, we are done
    TSTime64 tEnd = tFrom + count*m_tDivide;   // time of point AFTER
    TSTime64 tBFirst = FirstTime(); // first item time in the block
    if (tEnd < tBFirst)             // if all data is before the block...
        return -1;                  // ...we are done

    // We can now work out the first and last indices that will be involved.
    // If the change data does not align exactly we round down one place.
    if (tFrom > tBFirst)
        first = 0;                  // use the first point
    else
        first = static_cast<size_t>((tBFirst - tFrom) / m_tDivide);

    size_t last;                    // index beyond last used
    if (tEnd <= tBLast)
        last = count;               // extends beyond the last
    else
        last = static_cast<size_t>((tBLast - tFrom) / m_tDivide);

    // Now work through each wave segment in turn to update data
    for (auto it = begin(); it != end(); ++it)
    {
        size_t blIndex, daIndex;        // start index for block and data
        if (it->m_startTime >= tEnd)    // if beyond our data...
            break;                      // ...we are done
        TSTime64 tSgEnd = it->m_startTime + it->m_nItems*m_tDivide;
        if (tFrom >= tSgEnd)            // If before our data...
            continue;                   // ...skip to the next segment
        size_t nCopy;
        if (tFrom >= it->m_startTime)
        {
            daIndex = 0;            // will use 0, or not reached data
            blIndex = static_cast<size_t>((tFrom - it->m_startTime)/m_tDivide);
            nCopy = std::min(count, it->m_nItems - blIndex);
        }
        else
        {
            daIndex = static_cast<size_t>((it->m_startTime-tFrom)/m_tDivide);
            blIndex = 0;
            nCopy = std::min(static_cast<size_t>(it->m_nItems), count-daIndex);
        }
        std::copy_n(pData+daIndex, nCopy, it->m_data+blIndex);
        SetUnsaved();               // this block needs writing
    }

    return static_cast<int>(last - first) + 1;  // points used
}

//! Find the r.Max() point before a given time
/*!
If r.Max() is 1, find the last Adc point before r.Upto(). If r.Max() is > 1, find the
start time t where a contiguous read of r.Max() points from t to r.Upto() would reach
the last point before r.Upto(). If there is a gap, the read may extend past this point,
but is still ounts as finding it. That is, there can be a gap before r.Upto(), but points
before must be contiguous.
\param r     This defines the time range for the search and the number of items we
              want to go back.
\param pFilt If not nullptr (and the block is a marker or an extended marker type), use
             this to filter the data. That is, items not in the filter are ignored.
\return      -1 if not found (but r.Max() is reduced by the number of points skipped and
             the range is adjusted to replace the new end (r.tUpto) for the next search
             in the previous block). Otherwise, the found time and r.Max() is set 0.
*/
TSTime64 CRealWaveBlock::PrevNTime(CSRange& r, const CSFilter* pFilt) const
{
    // Start by finding the first data point before r.Upto() in the block. This means find the
    // first segment that contains the r.Upto() or later. We must also save the previous one
    // as we may need to go back if this is the first data point.
    auto it = cbegin();                 // get the first segment
    if (it->m_startTime >= r.Upto())    // if beyond the end time...
        return -1;                      // nothing to find here.
    auto last(it);                      // and save it as last
    size_t index = last->m_nItems;      // index to last to first point
    do
    {
        TSTime64 tSgStart = it->m_startTime;
        if (tSgStart >= r.Upto())       // last point of previous segment
            break;                      // cannot happen first time round
        else
        {
            TSTime64 tSgEnd = tSgStart + it->m_nItems*m_tDivide;
            if (tSgEnd >= r.Upto())     // time is in this block
            {
                index = 1+static_cast<size_t>((r.Upto() - 1 - it->m_startTime)/m_tDivide);
                last = it;              // set last to the one to use
                break;
            }
        }
        last = it;                      // save last in case in the gap
        index = last->m_nItems;         // in case we fall off the end
        ++it;
    }while(it != cend());

    // If we get here, last points at the segment, index is the index to the point
    // beyond the one before r.Upto(), which may not exist! Now check if this time
    // is acceptable - can only be non-contiguous if this is the first time.
    TSTime64 tUpto = last->m_startTime + index * m_tDivide;
    if (!r.First())
    {
        if (tUpto != r.Upto())
        {
            r.ZeroMax();                // flag we have found it
            return r.Upto();            // last found time is the result
        }
    }

    // Now figure out the index of the point we want.
    if (index >= r.Max())               // we have what we want
    {
        index -= r.Max();               // index of desired point (for time)               
        r.ZeroMax();                    // so caller knows we are done
    }
    else if (last != cbegin())          // if not first segment
    {
        index = 0;                      // no more contiguous stuff...
        r.ZeroMax();                    // ...so we are done
    }
    else                                // need to look at previous block
    {
        r.ReduceMax(index);             // reduce number to skip
        index = 0;                      // give a new end time
    }
    
    TSTime64 t = last->m_startTime + index * m_tDivide;
    if (t < r.From())                   // if found a time before we wanted
    {
        r.ZeroMax();                    // we are done, whatever
        size_t inc = (size_t)((r.From()-t+m_tDivide-1)/m_tDivide);
        if (index+inc < last->m_nItems) // can use some points
            t += inc*m_tDivide;         // new found time
        else
            t = r.First() ? -1 : r.Upto();
    }

    r.NotFirst();                       // clear the first time flag
    r.SetUpto(t);                       // in case we have to continue
    return t;
}