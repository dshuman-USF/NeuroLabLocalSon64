// s64xmark.cpp
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

//! \file s64xmark.cpp
//! \brief This file contains classes to handle extended marker channels.

#include <iostream>      // for debugging messages
#include <assert.h>
#include "s64priv.h"
#include "s64chan.h"
#include "s64range.h"

using namespace std;
using namespace ceds64;

    // circ_iterator specializations for TExtMark which is a variable size

    //! Increment operator specialization for TExtMark
    /*!
    \return A reference to the incremented iterator
    */
    template <>
    circ_iterator<TExtMark>& circ_iterator<TExtMark>::operator++()
    {
        m_pItem = reinterpret_cast<TExtMark*>(reinterpret_cast<uint8_t*>(m_pItem) + m_cb.m_nItemSize);
        if (m_pItem == &(*m_cb.m_iE))
            m_pItem = &(*m_cb.m_iD);
        return (*this);
    }

    //! Decrement operator specialization for TExtMark
    /*!
    \return A reference to the decremented iterator
    */
    template <>
    circ_iterator<TExtMark>& circ_iterator<TExtMark>::operator--()
    {
        if (m_pItem == &(*m_cb.m_iD))
            m_pItem = &(*m_cb.m_iE);
        m_pItem = reinterpret_cast<TExtMark*>(reinterpret_cast<uint8_t*>(m_pItem) - m_cb.m_nItemSize);
        return (*this);
    }

    //! Wrap m_pItem if it has been incremented past the buffer (specialization for TExtMark)
    /*!
    This relies on the  pointer being no more than m_nAllocated items beyond the buffer.
    */
    template<>
    void circ_iterator<TExtMark>::Wrap()
    {
        if (m_pItem < &(*m_cb.m_iD))
            m_pItem = reinterpret_cast<TExtMark*>(reinterpret_cast<uint8_t*>(m_pItem) + m_cb.m_nItemSize*m_cb.m_nAllocated);
        else if (m_pItem >= &(*m_cb.m_iE))
           m_pItem = reinterpret_cast<TExtMark*>(reinterpret_cast<uint8_t*>(m_pItem) - m_cb.m_nItemSize*m_cb.m_nAllocated);
    }

    //! Modify this iterator (specialization for TExtMark)
    /*!
    \param d The distance to add to the iterator by (which must be less than than the
             buffer size).
    \return  A reference to the modified iterator.
    */
    template<>
    circ_iterator<TExtMark>& circ_iterator<TExtMark>::operator+=(difference_type d)
    {
        m_pItem = reinterpret_cast<TExtMark*>(reinterpret_cast<uint8_t*>(m_pItem) + m_cb.m_nItemSize*d);
        Wrap();
        return (*this);
    }

//! Modify this iterator (specialization for TExtMark)
/*!
\param d The distance to subtract from the iterator by (which must be less than than the
         buffer size).
\return  A reference to the modified iterator.
*/
template<>
circ_iterator<TExtMark>& circ_iterator<TExtMark>::operator-=(difference_type d)
{
    m_pItem = reinterpret_cast<TExtMark*>(reinterpret_cast<uint8_t*>(m_pItem) - m_cb.m_nItemSize*d);
    Wrap();
    return (*this);
}

//! Array access (read/write) specialization for TExtMark.
/*!
\param d Array index relative to the oldest item in the buffer. This index is
         assumed to be legal (assert if not).
\return  A reference to the data at this index
*/
template<>
TExtMark& circ_iterator<TExtMark>::operator[](difference_type d)
{
    assert((d > -static_cast<difference_type>(m_cb.m_nSize)) &&
            (d < static_cast<difference_type>(m_cb.m_nSize)));
    d += m_cb.m_nFirst;
    if (d >= static_cast<difference_type>(m_cb.m_nAllocated))
        d -= m_cb.m_nAllocated;
    else if (d < 0)
        d += m_cb.m_nAllocated;
    return *reinterpret_cast<TExtMark*>(reinterpret_cast<uint8_t*>(&(*m_cb.m_iD)) + m_cb.m_nItemSize*d);
}

template<>
circ_iterator<TExtMark>::difference_type circ_iterator<TExtMark>::Index(const TExtMark* pItem) const
{
    auto d = reinterpret_cast<const uint8_t*>(pItem) - reinterpret_cast<const uint8_t*>(&(*m_cb.m_iD)); // bytes offset into buffer
    d /= m_cb.m_nItemSize;      // convert to index
    d -= m_cb.m_nFirst;         // offset from start
    if (d < 0)
        d += m_cb.m_nAllocated;
    // We have to allow for the index of end(), which means indices up to m_nSize are OK.
    assert((d >= 0) && (d <= static_cast<difference_type>(m_cb.m_nSize)));
    return d;
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
template<>
TSTime64 CircBuffer<TExtMark>::PrevNTimeW(CSRange& r, const CSFilter* pFilter, size_t nRow, TSTime64 tDvd) const
{
    const TSTime64 tWidth = static_cast<TSTime64>(nRow - 1)*tDvd; // offset to end of item
    if (r.Upto() <= FirstTime())  // nothing wanted here
        return -1;
    auto it = Find(r.Upto());   // find first at or after the range end
    if (it == begin())          // if this is start, then...
        return -1;              // ...nothing in here is wanted

    // There is at least 1 item in the buffer that could cross our time range
    --it;                       // point at first item we can use
    if (pFilter)                // if we have a filter
    {
        while (!pFilter->Filter(*it))
        {
            if (it == begin())  // if reached the start, no data found...
                return -1;      // ...nothing in this buffer
            --it;               // look at previous item
        }
    }

    // it is valid and points at the first item we might want. Check it overlaps our data
    TSTime64 t = it->m_time;    // time of first item in this TExtMark
    if (t + tWidth < r.From())  // If end of this is before the wanted range...
        return -1;              // ...there is nothing in the buffer

    TSTime64 tFirst = t;        // time of first returned element
    size_t firstI = 0;           // first element we want
    if (t < r.From())           // If not the first, we must work it out
    {
        firstI = static_cast<size_t>((t-r.From() + tDvd - 1) / tDvd);
        tFirst += firstI * tDvd; // time of this item
    }

    assert(firstI < nRow);       // check a sane result

    // Now calculate the last wanted index
    size_t lastI = static_cast<size_t>((r.Upto() - t - 1) / tDvd);
    if (lastI >= nRow)
        lastI = nRow-1;         // up to the end
    if (lastI - firstI + 1 > r.Max())
        lastI = firstI + r.Max() - 1;
//        TSTime64 tLast = t + lastI * tDvd;
//        if (tLast >= r.Upto())
//            lastI = static_cast<size_t>((r.Upto()-t-1) / tDvd);
    assert(lastI >= firstI);
    size_t nGot = lastI - firstI +1;
    r.ReduceMax(nGot);
    r.SetUpto(tFirst);
    r.NotFirst();               // future searches must match times exactly

    // Now look for overlapping data in previous extended markers
    while (it != begin())       // while not at buffer start
    {
        --it;                   // back to previous item (which exists)
        t = it->m_time;         // time of this item
        if (t + static_cast<int>(nRow) * tDvd < tFirst)
            return r.SetDone();

        if (pFilter)                // if we have a filter
        {
            while (!pFilter->Filter(*it))
            {
                if (it == begin())  // if reached the start, no data found...
                    return tFirst;  // ...nothing more in this buffer
                --it;
                t = it->m_time;         // time of this item
                if (t + static_cast<int>(nRow) * tDvd < tFirst) // check for overlap
                    return r.SetDone();
            }
        }
            
        // it points at data that overlaps or joins the next marker
        lastI = static_cast<size_t>((tFirst - t) / tDvd);    // index to the point that would be the same
        if ((t+lastI*tDvd ) != tFirst)
            return r.SetDone();     // data does not join up

        firstI = 0;                 // so first index is 0
        if (t < r.From())
            firstI = static_cast<size_t>((t - r.From()) / tDvd -1);
        if (lastI - firstI > r.Max())
            firstI = lastI - r.Max();
        tFirst = t + firstI * tDvd;  // time of the first point
        r.ReduceMax(lastI-firstI);
        r.SetUpto(tFirst);
        if (!r.HasRange())
            return tFirst;
    }

    return tFirst;
}



//=========================== CExtMarkChan =================================
//! Extended marker channel constructor
/*!
\param file     The file that owns this channel.
\param nChan    The channel number in the file.
\param xKind    The extended marker type: one of TextMark, RealMark or AdcMark.
\param nRow     The number of rows (points per trace)
\param nCol     The number of columns (traces), default is 1
\param tDvd     The sample interval per row point for the AdcMark data, default of 0
*/
CExtMarkChan::CExtMarkChan(TSon64File& file, TChanNum nChan, TDataKind xKind, size_t nRow, size_t nCol, TSTime64 tDvd)
    : CSon64Chan(file, nChan, xKind)
{
    assert(nRow && nCol && (nRow <= USHRT_MAX) && (nCol <= USHRT_MAX)); // beware madness
    size_t nObjSize = sizeof(TMarker);      // base size
    switch (xKind)
    {
    case AdcMark:
        nObjSize += sizeof(short)*nRow*nCol;
        m_chanHead.m_nItemSize = sizeof(short);
        break;
    case RealMark:
        nObjSize += sizeof(float)*nRow*nCol;
        m_chanHead.m_nItemSize = sizeof(float);
        break;
    case TextMark:
        assert(nCol == 1);
        nObjSize += nRow;
        m_chanHead.m_nItemSize = 1;
        break;
    }

    // Now all marker items are rounded up in size to the next multiple of 8
    nObjSize = ((nObjSize + 7) >> 3) << 3;  // round up the stored size
    m_bModified = (nObjSize !=  m_chanHead.m_nObjSize ) ||
                  (nRow != m_chanHead.m_nRows) ||
                  (nCol != m_chanHead.m_nColumns) ||
                  (tDvd != m_chanHead.m_tDivide);
    m_chanHead.m_nObjSize = static_cast<uint32_t>(nObjSize);  // pointer inc per item
    m_chanHead.m_nRows = static_cast<uint16_t>(nRow);    // store item information
    m_chanHead.m_nColumns = static_cast<uint16_t>(nCol);
    m_chanHead.m_tDivide = tDvd;            // for AdcMark

    if (!m_bmRead.HasDataBlock())           // this should NOT have one
        m_bmRead.SetDataBlock(new CExtMarkBlock(nChan, m_chanHead.m_nObjSize));
}

//! Write extended markers to an extended marker channel.
/*!
 You _must not_ be holding the channel mutex to call this. It is assumed that the new
 data is of the correct type, that is that it matches the channel.
 \param pData    The list of times to add. They must be in order and be after any 
                 previous times written to the file.
 \param count    The number of items to write.
 \return         0 if not detected error or an error code
*/
int CExtMarkChan::WriteData(const TExtMark* pData, size_t count)
{
    if (count == 0)                     // beware NULL operations
        return 0;

    TChanLock lock(m_mutex);            // take ownership of the channel
    if (pData[0] <= m_chanHead.m_lastTime)
        return OVER_WRITE;
    int err = 0;
    if (!m_pWr)                         // make sure we have a write buffer
        err = InitWriteBlock(new CExtMarkBlock(m_nChan, m_chanHead.m_nObjSize));

    // Write buffers holds last data written, if full, data is on disk
    while ((err == 0) && count)
    {
        CExtMarkBlock* pWr = static_cast<CExtMarkBlock*>(m_pWr.get()); // get the raw pointer
        size_t nCopy = pWr->AddData(pData, count);
        count -= nCopy;                 // keep counter lined up with the pointer
        if ( pWr->full() )              // if buffer is full...
            err = AppendBlock(pWr);     // ...add data to the file, clear unsaved flag
    }
    return err;
}

//! Read contiguous waveform data into the buffer from the channel.
/*!
This is used with AdcMark channels. For other channel types the CDataBlock::GetData() call
will return an error.
\param pData  The buffer to append data to
\param r      The range and max items are in here, plus the trace number to read when there is
              a choice.
\param tFirst if r.First() then fill in with first time, otherwise leave alone.
              if not first time, data must start at r.From() (or not contiguous).
\param pFilter Either nullptr or points at a filter for the data to read from.
\return       Number of points read or a negative error code.
*/
int CExtMarkChan::ReadData(short* pData, CSRange& r, TSTime64& tFirst, const CSFilter* pFilter)
{
    if (!r.HasRange() || TestNullFilter(pFilter)) // if nothing possible...
        return 0;                           // ...bail out now
    r.SetChanHead(&m_chanHead);             // So we can extract the data
    int nRead = 0;                          // will be the count of read data

    TChanLock lock(m_mutex);                // take ownership of the channel

    // If we have a write buffer, and our time range includes data in the buffer, use that
    // in preference to reading from the disk. If the write buffer exists, it holds the
    // last block and holds at least 1 data value.
    TSTime64 tBufStart = m_pWr ? m_pWr->FirstTime() : TSTIME64_MAX;

    if (r.From() < tBufStart)       // Look on disk if may be data
    {
        int err = m_bmRead.LoadBlock(r.From());    // get the block
        if (err < 0)
            return err;

        // If the data is on disk, read it, but if we reach the write buffer we
        // are done and collect the write buffer data. This is to make sure that
        // if the write buffer was partially written we get the new data.
        while ((err == 0) && (m_bmRead.DataBlock().FirstTime() < tBufStart))
        {
            size_t nCopy = m_bmRead.DataBlock().GetData(pData, r, tFirst, pFilter);
            nRead += static_cast<int>(nCopy);
            if (!r.CanContinue())
                return nRead;
            err = m_bmRead.NextBlock();         // fetch next block
        }
    }

    // If we get here with room in the buffer and not done, then we must look in the write system to
    // see if there is pending data. Beware that GetData() can set the count to 0 if it thinks no more
    // data is possible. This can happen when we have a write buffer that has been committed, but still
    // holds data. It tries to be helpful by setting the max value to 0 to stop further searches.
    // if a buffer, and something left to do and possible data
    if (m_pWr && r.CanContinue() && (r.Upto() > tBufStart) &&
        (r.First() || (r.From() == tBufStart))) // ...either first or contiguous
    {
        size_t nCopy = m_pWr->GetData(pData, r, tFirst, pFilter);
        nRead += static_cast<int>(nCopy);
    }

    return nRead;
}

// Read data from the channel. If the write buffer exists and overlaps the read request,
// use the data in the write buffer in preference to the data on disk.
int CExtMarkChan::ReadData(TExtMark* pData, CSRange& r, const CSFilter* pFilter)
{
    if (!r.HasRange() || TestNullFilter(pFilter)) // if nothing possible...
        return 0;                           // ...bail out now

    int nRead = 0;                          // will be the count of read data

    TChanLock lock(m_mutex);                // take ownership of the channel

    // If we have a write buffer, and our time range includes data in the buffer, use that
    // in preference to reading from the disk. If the write buffer exists, it holds the
    // last block and holds at least 1 data value.
    TSTime64 tBufStart = m_pWr ? m_pWr->FirstTime() : TSTIME64_MAX;

    if (r.From() < tBufStart)       // Don't look on disk if no data
    {
        int err = m_bmRead.LoadBlock(r.From());    // get the block
        if (err < 0)
            return err;

        // If the data is on disk, read it, but if we reach the write buffer we
        // are done and collect the write buffer data. This is to make sure that
        // if the write buffer was partially written we get the new data.
        while ((err == 0) && (m_bmRead.DataBlock().FirstTime() < tBufStart))
        {
            size_t nCopy = m_bmRead.DataBlock().GetData(pData, r, pFilter);
            nRead += static_cast<int>(nCopy);
            if (!r.CanContinue())
                return nRead;
            err = m_bmRead.NextBlock();         // fetch next block
        }
    }

    // If we get here with room in the buffer and not done, then we must look in the write system to
    // see if there is pending data.
    // if a buffer, and something left to do and possible data
    if (m_pWr && r.CanContinue() && (r.Upto() > tBufStart))
    {
        size_t nCopy = m_pWr->GetData(pData, r, pFilter);
        nRead += static_cast<int>(nCopy);
    }

    return nRead;
}

// If we are buffered, the buffering has already been taken care of before we get
// here, so we need not worry about it. Note that we may need to change data both
// in the buffer and here if the buffered data was already committed.
// returns 1 if found and done, 0 if not found, -ve if an error.
int CExtMarkChan::EditMarker(TSTime64 t, const TMarker* pM, size_t nCopy)
{
    TChanLock lock(m_mutex);                // take ownership of the channel

    if (nCopy > m_chanHead.m_nObjSize)
        return BAD_PARAM;

    // If in the buffer we just need to change there and mark the buffer as modified
    if (m_pWr && (t >= m_pWr->FirstTime())) // could be buffered
        return m_pWr->EditMarker(t, pM, nCopy);

    int err = m_bmRead.LoadBlock(t);        // get block with this data
    if (err < 0)
        return err;

    return m_bmRead.DataBlock().EditMarker(t, pM, nCopy);
}

//=================================== Buffered version ===============================================
//! Buffered extended marker channel constructor
/*!
\param file     The file that owns this channel.
\param nChan    The channel number in the file.
\param bSize    The buffer size in data items.
\param xKind    The extended marker type: one of TextMark, RealMark or AdcMark.
\param nRow     The number of rows (points per trace)
\param nCol     The number of columns (traces), default is 1
\param tDvd     The sample interval per row point for the AdcMark data, default of 0
*/
CBExtMarkChan::CBExtMarkChan(TSon64File& file, TChanNum nChan, size_t bSize, TDataKind xKind, size_t nRow, size_t nCol, TSTime64 tDvd)
    : m_nMinMove( bSize >> CircBuffMinShift )
    , CExtMarkChan(file, nChan, xKind, nRow, nCol, tDvd)
{
    m_pCirc = std::make_unique<circ_buff>(bSize, m_chanHead.m_nObjSize);
}

void CBExtMarkChan::Save(TSTime64 t, bool bSave)
{
    TBufLock lock(m_mutBuf);                            // acquire the buffer
    TSTime64 tLast = LastCommittedWriteTime();          // cannot change before this
    m_st.SetSave(t < tLast ? tLast+1 : t, bSave);
}

void CBExtMarkChan::SaveRange(TSTime64 tFrom, TSTime64 tUpto)
{
    TBufLock lock(m_mutBuf);                            // acquire the buffer
    TSTime64 tLast = LastCommittedWriteTime();          // cannot change before this
    m_st.SaveRange(tFrom < tLast ? tLast : tFrom, tUpto);
}

bool CBExtMarkChan::IsSaving(TSTime64 tAt) const
{
    TBufLock lock(m_mutBuf);                            // acquire the buffer
    return m_st.IsSaving(tAt);
}

int  CBExtMarkChan::NoSaveList(TSTime64* pTimes, int nMax, TSTime64 tFrom, TSTime64 tUpto) const
{
    TBufLock lock(m_mutBuf);                            // acquire the buffer
    return m_st.NoSaveList(pTimes, nMax, tFrom, tUpto);
}

void CBExtMarkChan::LatestTime(TSTime64 t)
{
    TBufLock lock(m_mutBuf);                            // acquire the buffer
    TSTime64 tLast = m_pCirc ? m_pCirc->LastTime() : -1;
    m_st.SetDeadRange(tLast, t, eSaveTimes::eST_MaxDeadEvents);
}

void CBExtMarkChan::ResizeCircular(size_t nItems)
{
    TBufLock lock(m_mutBuf);                            // acquire the buffer
    if (m_pCirc)
    {
        if (nItems > 0)
        {
            m_pCirc->reallocate(nItems);
            m_nMinMove = nItems >> CircBuffMinShift;
        }
        else
            m_pCirc.reset();
    }
}

int CBExtMarkChan::WriteData(const TExtMark* pData, size_t count)
{
    if (count == 0)                                     // detect stupid cases...
        return 0;                                       // ...and do nothing

    TBufLock lock(m_mutBuf);                            // acquire the buffer
    if (!m_pCirc || !m_pCirc->capacity())               // Make sure we have one
        return CExtMarkChan::WriteData(pData, count);

    size_t written = m_pCirc->add(pData, count);        // attempt add to buffer
    count -= written;

    if (count == 0)                                     // if all added ok...
        return 0;                                       // ...we are done

    db_iterator<const TExtMark> iData(pData, m_pCirc->ItemSize());
    iData += written;                                   // move on

    // To get here, the circular buffer is full. 
    size_t spaceNeeded = count;                         // space to create
    if (spaceNeeded < m_nMinMove)                       // Don't move tiny ammounts...
        spaceNeeded = m_nMinMove;                       // ...as inefficient

    // To move data to the disk buffer, we need contiguous elements from our circular buffer.
    int err = 0;
    const size_t capacity = m_pCirc->capacity();        // how much we could put in
    if (spaceNeeded >= capacity)                        // At least 1 complete buffer?
    {
        err = CommitToWriteBuffer(TSTIME64_MAX);        // commit everything we have
        if (err)
            return err;
        m_pCirc->flush();                               // buffer is now empty
        m_st.SetFirstTime(iData[0]);                    // and move time on

        // We want to leave the buffer as full as we can so that oldest data possible is
        // visible to the user. OPTIMISE: if buffer state is all or none, we could skip
        // moving data through the buffer and just write it.
        while (count > capacity)                        // if more than can fit in buffer
        {
            size_t nWrite = std::min(count-capacity, capacity); // max to write
            m_pCirc->add(&iData[0], nWrite);
            count -= nWrite;
            iData += nWrite;
            err = CommitToWriteBuffer(TSTIME64_MAX);    // flush all to disk buffers
            if (err)
                return err;
            m_pCirc->flush();                           // buffer is now empty
        }
        m_st.SetFirstTime(iData[0]);                    // committed up to here
    }
    else
    {
        TSTime64 tUpto = (*m_pCirc)[spaceNeeded];       // final time
        err = CommitToWriteBuffer(tUpto);               // flush up to needed time
        if (err)
            return err;
        m_pCirc->free(spaceNeeded);                     // release used space
        m_st.SetFirstTime(tUpto);                       // committed up to here
    }

    written = m_pCirc->add(&iData[0], count);           // this should now fit
    assert(written == count);

    return 0;
}

//! Commit data up to a specified time to the write buffer
/*!
We are to commit all data up to the nominated time to the write buffer. We are to take
notice of the m_st list. We know that the list holds where we reached in committing.
\param tUpto The time upto (but not including) which we are to commit.
\return      S64_OK (0) or a negative error code.
*/
int CBExtMarkChan::CommitToWriteBuffer(TSTime64 tUpto)
{
    assert(m_pCirc);
    TSTime64 tFrom, tTo;
    TSTime64 tLastWrite = LastCommittedWriteTime();
    if (!m_st.FirstSaveRange(&tFrom, &tTo, tUpto, tLastWrite+1))
        return 0;
    do
    {
        CircBuffer<TExtMark>::range r[2];    // pair of ranges
        size_t n = m_pCirc->contig_range(tFrom, tTo, r);
        for (size_t i = 0; i<n; ++i)
        {
            int err = CExtMarkChan::WriteData(r[i].m_pData, r[i].m_n);
            if (err)
                return err;
        }
    }while(m_st.NextSaveRange(&tFrom, &tTo, tUpto));
    return 0;
}

// Limit disk reads to stop at the first buffer time, then read from the buffer.
// The source and dest types must match exactly, else this will fail in inelegant
// and painful ways.
// pData    Points at the waveform array to be filled in.
// r        This has the time range and the trace that is to be filled in.
// tFirst   Only set if this is the first item added to the buffer.
int CBExtMarkChan::ReadData(short* pData, CSRange& r, TSTime64& tFirst, const CSFilter* pFilter)
{
    if (m_chanHead.m_chanKind != AdcMark)       // detect madness
        return CHANNEL_TYPE;
    if (!r.HasRange())                          // if nothing possible...
        return 0;                               // ...bail out now
    CSFilter::eActive eAct = pFilter ? pFilter->Active() : CSFilter::eA_all;;  // See if anything to do
    if (eAct == CSFilter::eA_none)              // bail now if nothing to do
        return 0;

    TBufLock lock(m_mutBuf);                    // acquire the buffer
    if (!m_pCirc || m_pCirc->empty())           // make sure we have one
        return CExtMarkChan::ReadData(pData, r, tFirst, pFilter);

    TSTime64 tReadEnd( r.Upto() );              // save end
    TSTime64 tBufStart = m_pCirc->FirstTime();
    r.SetUpto(std::min(tBufStart, r.Upto()));   // limit disk reads
    int nRead = CExtMarkChan::ReadData(pData, r, tFirst, pFilter);
    if ((nRead < 0) || r.IsTimedOut())          // read error or timed out
        return nRead;

    // Now read data from the circular buffer
    if ((r.Max()) && (tReadEnd > tBufStart))    // if buffered data waiting for us
    {
        pData += nRead;                         // where to read to
        TSTime64 tWide = (m_chanHead.m_nRows-1) * m_chanHead.m_tDivide;
        CircBuffer<TExtMark>::range x[2];        // will hold 0, 1 or 2 circ buff ranges
        size_t n = m_pCirc->contig_range(r.From(), tReadEnd + tWide, x);

        // See if the user wants to read data from a specified column
        int nColOffs = 0;                       // if we are to collect a specific column
        if (pFilter) 
        {
            nColOffs = pFilter->GetColumn();    // Get and check the desired column
            if ((nColOffs < 0) || (nColOffs >= m_chanHead.m_nColumns))
                nColOffs = 0;
        }
        for (size_t i = 0; r.Max() && (i<n); ++i)
        {
            db_iterator<const TExtMark> iFrom(x[i].m_pData, m_chanHead.m_nObjSize);
            size_t n = x[i].m_n;                // number of items to process

            do
            {
                if (eAct != CSFilter::eA_all)   // if filtering is needed
                {
                    while (n && !pFilter->Filter(*iFrom))
                        ++iFrom, --n;
                    if (!n)
                        return nRead;
                }

                // To get here, iFrom is an extended marker we can use
                TSTime64 tOffset = r.From() - iFrom->m_time;  // Time offset to desired item
                int index = (tOffset > 0) ? (int)(tOffset / m_chanHead.m_tDivide) : 0;  // no overflow
                TSTime64 tItem = iFrom->m_time + index * m_chanHead.m_tDivide;    // first time
                if (r.First())
                {
                    r.NotFirst();                           // no longer the first time
                    tFirst = tItem;                         // so set the first time
                }
                else if (tItem != r.From())                 // if not first. must be contiguous
                    return nRead;                           // not contiguous, so done
                
                int nMaxCopy = m_chanHead.m_nRows - index;  // max waveform points we could copy
                if (iFrom->m_time + tWide >= tReadEnd)      // are we limited by the end time?
                    nMaxCopy = (int)((tReadEnd - tItem + m_chanHead.m_tDivide - 1) / m_chanHead.m_tDivide);
                if (nMaxCopy > (int)r.Max())                // For read, cannot exceed int range
                    nMaxCopy = (int)r.Max();

                if (nMaxCopy > 0)                           // just in case of -ve...
                {
                    TAdcMark* pAM = (TAdcMark*)&(*iFrom);
                    short* pFrom = pAM->Shorts() + (nColOffs + (index*m_chanHead.m_nColumns));
                    for (int i=0; i<nMaxCopy; ++i)
                    {
                        *pData++ = *pFrom;
                        pFrom += m_chanHead.m_nColumns;
                    }
                    nRead += nMaxCopy;
                    r.ReduceMax(nMaxCopy);
                    r.SetFrom(tItem + nMaxCopy*m_chanHead.m_tDivide);
                    if (!r.HasRange())
                        return nRead;
                }
                ++iFrom, --n;                           // move onwards
            }while(n);
        }
    }
    return nRead;
}


// Limit disk reads to stop at the first buffer time, then read from the buffer.
// The source and dest types must match exactly, else this will fail in inelegant
// and painful ways.
int CBExtMarkChan::ReadData(TSTime64* pData, CSRange& r, const CSFilter* pFilter)
{
    assert(r.HasRange());
    if (TestNullFilter(pFilter))                // Check filter, if matches nothing...
        return 0;                               // ...then we are done

    TBufLock lock(m_mutBuf);                    // acquire the buffer
    if (!m_pCirc || m_pCirc->empty())           // make sure we have one
        return CSon64Chan::ReadData(pData, r, pFilter);

    TSTime64 tReadEnd( r.Upto() );              // save end
    TSTime64 tBufStart = m_pCirc->FirstTime();
    r.SetUpto(std::min(tBufStart, r.Upto()));   // limit disk reads
    int nRead = CSon64Chan::ReadData(pData, r, pFilter);
    if ((nRead < 0) || r.IsTimedOut())          // read error or timed out
        return nRead;

    // Now read data from the circular buffer
    if ((r.Max()) && (tReadEnd > tBufStart))    // if buffered data waiting for us
    {
        pData += nRead;                         // where to read to
        CircBuffer<TExtMark>::range x[2];        // will hold 0, 1 or 2 circ buff ranges
        size_t n = m_pCirc->contig_range(r.From(), tReadEnd, x);
        db_iterator<TSTime64> iTo(pData);
        for (size_t i = 0; r.Max() && (i<n); ++i)
        {
            db_iterator<const TExtMark> iFrom(x[i].m_pData, m_chanHead.m_nObjSize);
            if (pFilter)
            {
                auto iLimit = iFrom + x[i].m_n; // limit of source to move
                size_t nCopied = 0;             // number actually copied
                while (iFrom < iLimit)
                {
                    if (pFilter->Filter(*iFrom))
                    {
                        iTo.assign(*iFrom), ++iTo;
                        if (++nCopied >= r.Max())
                            break;
                    }
                    ++iFrom;
                }
                nRead += static_cast<int>(nCopied);
                r.ReduceMax(nCopied);
            }
            else
            {
                size_t nCopy = std::min(r.Max(), x[i].m_n); // maximum items to copy
                auto iLimit = iTo + nCopy;
                while (iTo < iLimit)
                    *iTo++ = *iFrom++;
                nRead += static_cast<int>(nCopy);
                r.ReduceMax(nCopy);
            }
        }
    }
    return nRead;
}

// Limit disk reads to stop at the first buffer time, then read from the buffer.
// The source and dest types must match exactly, else this will fail in inelegant
// and painful ways.
int CBExtMarkChan::ReadData(TMarker* pData, CSRange& r, const CSFilter* pFilter)
{
    assert(r.HasRange());
    if (TestNullFilter(pFilter))                // Check filter, if matches nothing...
        return 0;                               // ...then we are done

    TBufLock lock(m_mutBuf);                    // acquire the buffer
    if (!m_pCirc || m_pCirc->empty())           // make sure we have one
        return CSon64Chan::ReadData(pData, r, pFilter);

    TSTime64 tReadEnd( r.Upto() );              // save end
    TSTime64 tBufStart = m_pCirc->FirstTime();
    r.SetUpto(std::min(tBufStart, r.Upto()));   // limit disk reads
    int nRead = CSon64Chan::ReadData(pData, r, pFilter);
    if ((nRead < 0) || r.IsTimedOut())          // read error or timed out
        return nRead;

    // Now read data from the circular buffer
    if ((r.Max()) && (tReadEnd > tBufStart))    // if buffered data waiting for us
    {
        pData += nRead;                         // where to read to
        CircBuffer<TExtMark>::range x[2];        // will hold 0, 1 or 2 circ buff ranges
        size_t n = m_pCirc->contig_range(r.From(), tReadEnd, x);
        db_iterator<TMarker> iTo(pData);
        for (size_t i = 0; r.Max() && (i<n); ++i)
        {
            db_iterator<const TExtMark> iFrom(x[i].m_pData, m_chanHead.m_nObjSize);
            if (pFilter)
            {
                auto iLimit = iFrom + x[i].m_n; // limit of source to move
                size_t nCopied = 0;             // number actually copied
                while (iFrom < iLimit)
                {
                    if (pFilter->Filter(*iFrom))
                    {
                        iTo.assign(*iFrom), ++iTo;
                        if (++nCopied >= r.Max())
                            break;
                    }
                    ++iFrom;
                }
                nRead += static_cast<int>(nCopied);
                r.ReduceMax(nCopied);
            }
            else
            {
                size_t nCopy = std::min(r.Max(), x[i].m_n); // maximum items to copy
                auto iLimit = iTo + nCopy;
                while (iTo < iLimit)
                    *iTo++ = *iFrom++;
                nRead += static_cast<int>(nCopy);
                r.ReduceMax(nCopy);
            }
        }
    }
    return nRead;
}

// Limit disk reads to stop at the first buffer time, then read from the buffer.
// The source and dest types must match exactly, else this will fail in inelegant
// and painful ways.
int CBExtMarkChan::ReadData(TExtMark* pData, CSRange& r, const CSFilter* pFilter)
{
    assert(r.HasRange());
    if (TestNullFilter(pFilter))                // Check filter, if matches nothing...
        return 0;                               // ...then we are done

    TBufLock lock(m_mutBuf);                    // acquire the buffer
    if (!m_pCirc || m_pCirc->empty())           // make sure we have one
        return CExtMarkChan::ReadData(pData, r, pFilter);

    TSTime64 tReadEnd( r.Upto() );              // save end
    TSTime64 tBufStart = m_pCirc->FirstTime();
    r.SetUpto(std::min(tBufStart, r.Upto()));   // limit disk reads
    int nRead = CExtMarkChan::ReadData(pData, r, pFilter);
    if ((nRead < 0) || r.IsTimedOut())          // read error or timed out
        return nRead;

    // Now read data from the circular buffer
    if ((r.Max()) && (tReadEnd > tBufStart))    // if buffered data waiting for us
    {
        CircBuffer<TExtMark>::range x[2];        // will hold 0, 1 or 2 circ buff ranges
        size_t n = m_pCirc->contig_range(r.From(), tReadEnd, x);
        db_iterator<TExtMark> iTo(pData, m_chanHead.m_nObjSize);
        iTo += nRead;                           // move past data already read
        for (size_t i = 0; r.Max() && (i<n); ++i)
        {
            if (pFilter)
            {
                db_iterator<const TExtMark> iFrom(x[i].m_pData, m_chanHead.m_nObjSize);
                auto iLimit = iFrom + x[i].m_n; // limit of source to move
                size_t nCopied = 0;             // number actually copied
                while (iFrom < iLimit)
                {
                    if (pFilter->Filter(*iFrom))
                    {
                        iTo.assign(*iFrom), ++iTo;
                        if (++nCopied >= r.Max())
                            break;
                    }
                    ++iFrom;
                }
                nRead += static_cast<int>(nCopied);
                r.ReduceMax(nCopied);
            }
            else
            {
                size_t nCopy = std::min(r.Max(), x[i].m_n); // maximum items to copy
                memcpy(&(*iTo), x[i].m_pData, nCopy*m_chanHead.m_nObjSize);
                iTo += nCopy;
                nRead += static_cast<int>(nCopy);
                r.ReduceMax(nCopy);
            }
        }
    }
    return nRead;
}

bool CBExtMarkChan::IsModified() const
{
    TBufLock lock(m_mutBuf);                // acquire the buffer
    if (m_pCirc && !m_pCirc->empty())
    {
        TSTime64 tFrom, tTo;
        return m_st.FirstSaveRange(&tFrom, &tTo, TSTIME64_MAX);
    }
    return CExtMarkChan::IsModified();
}

uint64_t CBExtMarkChan::GetChanBytes() const
{
    TBufLock lock(m_mutBuf);                    // acquire the buffer
    uint64_t total = CExtMarkChan::GetChanBytes();  // get committed bytes
    if (m_pCirc && !m_pCirc->empty())
    {
        TSTime64 t = LastCommittedWriteTime();
        total += m_pCirc->Count(t+1)*m_chanHead.m_nObjSize;    // add uncommitted bytes in buffer
    }
    return total;
}

// Make sure that anything committable in the circular buffer is committed
int CBExtMarkChan::Commit()
{
    TBufLock lock(m_mutBuf);                // acquire the buffer
    int err = 0;
    if (m_pCirc && !m_pCirc->empty())
        err = CommitToWriteBuffer();

    if (err == 0)
        err = CExtMarkChan::Commit();       // flush data through to disk
    return err;
}

// Find the event that is n before the sTime. Put another way, find the event that if we read n
// events, the last event read would be the one before sTime.
// r        The range and number of items to skip back
// pFilter  To filter the events unless nullptr.
TSTime64 CBExtMarkChan::PrevNTime(CSRange& r, const CSFilter* pFilter, bool bAsWave)
{
    if (!r.HasRange() || TestNullFilter(pFilter)) // Check it is even possible
        return -1;

    TBufLock lock(m_mutBuf);                // acquire the buffer
    if (bAsWave && (m_chanHead.m_tDivide <= 0))
        return CHANNEL_TYPE;                // not possible

    if (m_pCirc && !m_pCirc->empty())
    {
        TSTime64 t = bAsWave ? m_pCirc->PrevNTimeW(r, pFilter, m_chanHead.m_nRows, m_chanHead.m_tDivide) : m_pCirc->PrevNTime(r, pFilter);
        if (!r.HasRange())
            return t;
    }

    return CExtMarkChan::PrevNTime(r, pFilter, bAsWave);
}

TSTime64 CBExtMarkChan::MaxTime() const
{
    TBufLock lock(m_mutBuf);                // acquire the buffer
    return (!m_pCirc || m_pCirc->empty()) ? CExtMarkChan::MaxTime() : m_pCirc->LastTime();
}
// Edit a marker at an exactly matching time.
int CBExtMarkChan::EditMarker(TSTime64 t, const TMarker* pM, size_t nCopy)
{
    TBufLock lock(m_mutBuf);                    // acquire the buffer
    if (!m_pCirc || m_pCirc->empty())           // make sure we have one
        return CExtMarkChan::EditMarker(t, pM, nCopy);

    if (nCopy > m_chanHead.m_nObjSize)
        return BAD_PARAM;

    int iReturn = 0;                            // 0 for not found
    auto it = m_pCirc->Find(t);                 // find the time in the buffer
    if (it != m_pCirc->end())                   // see if found
    {
        iReturn = 1;
        if (nCopy > sizeof(TSTime64))           // if anything to copy
        {
            bool bChange = memcmp(&it->m_code[0], &pM->m_code[0], nCopy-sizeof(TSTime64)) != 0;
            if (bChange)
                memcpy(&it->m_code[0], &pM->m_code[0], nCopy-sizeof(TSTime64));
        }
    }

    // Must also do this for any already committed data
    return CExtMarkChan::EditMarker(t, pM, nCopy) | iReturn;
}

//======================= Create a new marker channel =======================
// chan     The channel number in the file (0 up to m_vChanHead.size())
// dRate    The expected channel event rate in Hz
// nMax     The maximum characters to store in the channel
// iPhyCh   The physical channel number (application use), not checked
int TSon64File::SetTextMarkChan(TChanNum chan, double dRate, size_t nMax, int iPhyCh)
{
    return SetExtMarkChan(chan, dRate, TextMark, nMax, 1, iPhyCh);
}

// chan     The channel number in the file (0 up to m_vChanHead.size())
// dRate    The expected channel event rate in Hz
// kind     The channel type, being RealMark, TextMark, AdcMark
// nRows    The first dimension of the stored items
// nCols    The second dimension (1 for Textmark).
// iPhyCh   The physical channel number (application use), not checked
// tDvd     The channel divide for AdcMark channels
// nPre     pretrigger points for AdcMark channels
int TSon64File::SetExtMarkChan(TChanNum chan, double dRate, TDataKind kind, size_t nRows, size_t nCols, int iPhyCh, TSTime64 tDvd, int nPre)
{
    if ((nRows <= 0) || (nCols <= 0))
        return BAD_PARAM;

    size_t nItemSize;                   // bytes to store an item of this size

    switch(kind)
    {
    case RealMark:
        nItemSize = sizeof(float);
        break;
    case TextMark:
        nItemSize = sizeof(uint8_t);
        if (nCols > 1)
            return BAD_PARAM;
        break;
    case AdcMark:
        nItemSize = sizeof(short);
        if ((tDvd <= 0) || (nCols == 0))
            return BAD_PARAM;
        break;
    default:
        return NO_CHANNEL;
    }

    if ((nPre < 0) || (static_cast<size_t>(nPre) > nRows-1))
        return BAD_PARAM;

    size_t nObjSize = ((nRows*nCols*nItemSize + 7) >> 3) << 3;
    size_t nBuffItems = (DBSize-DBHSize) / (sizeof(TMarker) + nObjSize);
    if (nBuffItems < 1)                 // must have at least 1 item per buffer
        return BAD_PARAM;

    // We intend to change the channel pointers, so we need a write lock on the channels
    TChWrLock lock(m_mutChans);     // we will be changing m_vChan
    int err = ResetForReuse(chan);  // check channel in a decent state
    if (err == S64_OK)
    {
        m_vChan[chan].reset(m_bOldFile ?
                       new CExtMarkChan(*this, chan, kind, nRows, nCols, tDvd) :
                       new CBExtMarkChan(*this, chan, nBuffItems, kind, nRows, nCols, tDvd));
        m_vChan[chan]->SetPhyChan(iPhyCh);
        m_vChan[chan]->SetIdealRate(dRate);
        m_vChan[chan]->SetPreTrig(static_cast<uint16_t>(nPre));
    }

    return err;
}

int TSon64File::GetExtMarkInfo(TChanNum chan, size_t *pRows, size_t* pCols) const
{
    TChRdLock lock(m_mutChans);     // we are not changing the #chans
    if ((chan >= m_vChanHead.size()) || !m_vChan[chan])
        return NO_CHANNEL;
    if (pRows)
        *pRows = m_vChan[chan]->GetRows();
    if (pCols)
        *pCols = m_vChan[chan]->GetCols();
    return m_vChan[chan]->GetPreTrig();
}

// chan     The channel number in the file (0 up to m_vChanHead.size())
// pData    Points at an array of data
// count    Number of items to add
// return   S64_OK or a negative error code
int TSon64File::WriteExtMarks(TChanNum chan, const TExtMark* pData, size_t count)
{
    if (m_bReadOnly)
        return READ_ONLY;
    if (count == 0)
        return 0;

    TChRdLock lock(m_mutChans);     // we are not changing the #chans
    if ((chan >= m_vChanHead.size()) || !m_vChan[chan])
        return NO_CHANNEL;

    return m_vChan[chan]->WriteData(pData, count);
}

// chan     The channel number in the file (0 up to m_vChanHead.size())
// pData    Points at an array of data to be filled
// max      Maximum number of items to add to the array
// tFrom    Earliest time we are interested in
// tUpto    One tick beyond the times we are interested in (up to but not including)
// pFilter  Either nulptr or a pointer to a filter used to select the items to read.
// return   The number of items read or a negative error code.
int TSon64File::ReadExtMarks(TChanNum chan, TExtMark* pData, int nMax, TSTime64 tFrom, TSTime64 tUpto, const CSFilter* pFilter)
{
    if ((nMax <= 0) || (tFrom >= tUpto) || (tUpto < 0))
        return 0;

    TChRdLock lock(m_mutChans);     // we are not changing the #chans
    if ((chan >= m_vChanHead.size()) || !m_vChan[chan])
        return NO_CHANNEL;

    CSRange r(tFrom, tUpto, nMax);   // make a range object to manage the request
    int nGot = 0;
    while (true)
    {
        int n = m_vChan[chan]->ReadData(pData, r, pFilter);
        if (n < 0)
            return n;

        nGot += n;
        if (!r.IsTimedOut() || !r.Max())
            return nGot;

        r.SetTimeOut();             // allow to run again
        pData = (TExtMark*)((char*)pData + n*m_vChan[chan]->m_chanHead.m_nObjSize);
    }
}
