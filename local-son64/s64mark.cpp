// s64mark.cpp
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
#include <iostream>      // for debugging messages
#include <assert.h>
#include "s64priv.h"
#include "s64chan.h"
#include "s64range.h"

using namespace std;
using namespace ceds64;

//=========================== CMarkerChan =================================
/*!
Constructor for a Marker channel.
\param file  The file that this channel is part of.
\param nChan The channel number in the file.
\param kind  The type of the channel (Marker or EventBoth)
*/
CMarkerChan::CMarkerChan(TSon64File& file, TChanNum nChan, TDataKind kind)
    : CSon64Chan(file, nChan, kind)
{
    assert((kind == Marker) || (kind == EventBoth));
    m_chanHead.m_nObjSize = sizeof(TMarker);
    if (!m_bmRead.HasDataBlock())       // this should NOT have one
        m_bmRead.SetDataBlock(new CMarkerBlock(nChan));
}

// Write event times to the event channel.
// pData    The list of times to add. They must be in order and be after any previous
//          times written to the file.
// count    The number of items to write.
// You MUST NOT be holding the channel mutex to call this.
int CMarkerChan::WriteData(const TMarker* pData, size_t count)
{
    if (count == 0)                     // beware NULL operations
        return 0;

    TChanLock lock(m_mutex);            // take ownership of the channel
    return WriteDataLocked(pData, count);
}

// Private routine to handle the writing when we already own the channel. This is
// used by the EventBoth code.
int CMarkerChan::WriteDataLocked(const TMarker* pData, size_t count)
{
    if (pData[0] <= m_chanHead.m_lastTime)
        return OVER_WRITE;

    // make sure we have a write buffer
    int err = m_pWr ? 0 : InitWriteBlock(new CMarkerBlock(m_nChan));

    // Write buffers holds last data written, if full, data is on disk
    while ((err == 0) && count)
    {
        CMarkerBlock* pWr = static_cast<CMarkerBlock*>(m_pWr.get()); // get the raw pointer
        size_t nCopy = pWr->AddData(pData, count);  // clears first if full
        count -= nCopy;
        if (pWr->full())                // if write buffer is full...
            err = AppendBlock(pWr);     // ...write it to the file, clear unsaved flag
    }

    return err;
}

// If we are buffered, the buffering has already been taken care of before we get
// here, so we need not worry about it. Note that we may need to change data both
// in the buffer and here if the buffered data was already committed.
// returns 1 if found and done, 0 if not found, -ve if an error.
int CMarkerChan::EditMarker(TSTime64 t, const TMarker* pM, size_t nCopy)
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

//-------------------------------- Handle level channels ---------------------------------------------
/*!
 Valid for EventBoth channels only. You MUST hold the channel lock to call this.
 Get the last level written to the channel. This is only used when we want to write
 data to the channel, and when we do this we will have allocated m_pWr. If there is
 no m_pWr, then just report the initial channel level.
*/
bool CMarkerChan::LastWriteLevel() const
{
    assert(EventBoth == ChanKind());    // warn of stupid call
    if (!m_pWr || m_pWr->empty())       // If no data...
        return GetInitLevel();          // ...report channel level
    return m_pWr->LastCode() != 0;      // the last marker code
}

//! Utility routine to convert event times to markers for uses as a level channel
/*!
We do not want duplicated times to be added, so if there are an even number of events at the same
time we skip them. An odd number is treated as a single event.
\param marks    A vector of Marker data that is returned sized to count holding the times of the
                data in pData and with all codes set to 0 except the first, which is set alternately
                to 1 and 0 with the first value being !bLastLevel.
\param pData    Points at an array of TSTime64 times in ascending order or size at least count.
\param count    The number of items to convert.
\param bLastLevel The level before the current one. true for high, false for low.
*/
static size_t Level2Marker(vector<TMarker> &marks, const TSTime64* pData, size_t count, bool bLastLevel)
{
    if (count == 0)
        return 0;

    marks.resize(count);                // make sure we have space
    size_t o=0;                         // input and output pointers
    bool bSave = true;
    TSTime64 tLast = pData[0];
    bLastLevel = !bLastLevel;                   // invert the level (the next expected one to write)
    for (size_t in = 1; in < count; ++in)
    {
        if (pData[in] != tLast)
        {
            if (bSave)                          // if odd number at this time
            {
                marks[o].m_time = tLast;
#ifndef CEDS64_BIGENDIAN
                marks[o].m_int[0] = bLastLevel; // set first 4 marker codes to 0 or 1;
#else
                marks[o].m_int[0] = 0;          // set first 4 marker codes to 0
                marks[o].m_code[0] = bLastLevel; // set first marker code to 0 or 1
#endif
                marks[o].m_int[1] = 0;          // set second set to 0
                ++o;
            }
            else
                bSave = true;
            tLast = pData[in];                  // the new last time
        }
        else
            bSave = !bSave;
        bLastLevel = !bLastLevel;               // invert the level (the next expected one to write)
    }

    // We still have the last item to add (or not)
    if (bSave)
    {
        marks[o].m_time = tLast;
#ifndef CEDS64_BIGENDIAN
        marks[o].m_int[0] = bLastLevel; // set first 4 marker codes to 0 or 1;
#else
        marks[o].m_int[0] = 0;          // set first 4 marker codes to 0
        marks[o].m_code[0] = bLastLevel; // set first marker code to 0 or 1
#endif
        marks[o].m_int[1] = 0;          // set second set to 0
        ++o;
    }
    return o;
}

//! Append eventBoth data to the channel.
/*!
 In order for the locking to work, this is over-ridden and NEVER CALLED if the CBMarkerChan exists.
 We assume that data is being sequenced correctly. That is, if not data is held in the channel that
 GetInitLevel() tells us the initial level, otherwise this is the opposite to the last level held in
 the file.
*/
int CMarkerChan::WriteData(const TSTime64* pData, size_t count)
{
    if (count == 0)
        return 0;

    TChanLock lock(m_mutex);            // take ownership of the channel
    if (EventBoth != ChanKind())
        return CHANNEL_TYPE;

    int err = 0;
    if (!m_pWr)                         // make sure we have a write buffer
        err = InitWriteBlock(new CMarkerBlock(m_nChan));

    if (err == 0)
    {
        bool bLastLevel = LastWriteLevel();
        vector<TMarker> marks;
        count = Level2Marker(marks, pData, count, bLastLevel);
        err = WriteDataLocked(&marks[0], count);
    }
    return err;
}

//! Read marker data as eventBoth data
/*!
 This treats code 0 in the first marker code as low and anything else as high.
 It can cope with no data, in which case we return the level of the previous event.
 If there is no previous event, it returns the basic level set for the channel. We
 assume that the data was written correctly, that is, it alternates between low and
 high. If the data does not do this, then read it as markers and treat appropriately.
 \param pData   To holds the returned times
 \param r       The time range and maximum number of events to read
 \param bLevel  Returned as the level of the first event, or the level that the next
                event would be if there was one (true = high, false = low).
 \return        The number of events returned or a negative error code.
*/
int CMarkerChan::ReadLevelData(TSTime64 *pData, CSRange &r, bool& bLevel)
{
    int nRead = 0;
    if (r.Max())
    {
        TMarker m;                          // space to read one marker
        CSRange r1(r);                      // holds a copy of the range
        r1.ReduceMax(r1.Max()-1);           // set a single item
#ifdef USE_FILTER
        CSFilter f;     // by default it accepts everything
        f.Control(0, -1, CSFilter::eS_clr);   // clear entire layer
        f.Control(0, 0, CSFilter::eS_set);    // set items 0 and 1
        f.Control(0, 1, CSFilter::eS_set);
        nRead = ReadData(&m, r1, &f);       // get a single item
#else
        nRead = ReadData(&m, r1);           // code 0 is low, anything else is high
#endif
        if (nRead == 1)
        {
            bLevel = m.m_code[0] != 0;      // are we low or high?
            nRead = ReadData(pData, r);     // collect the rest
        }
        else if (nRead == 0)                // we still want the level
        {
            CSRange rTemp(-1, r.From(), 1); // GCC will not pass constructed as ref
            TSTime64 t = PrevNTime(rTemp);
            if (t == -1)                    // no previous data
                bLevel = !GetInitLevel();   // so ask channel
            else if (t >= 0)
            {
                r1.SetFrom(t);
                r1.SetUpto(t+1);
                ReadData(&m, r1);           // get the data point
                bLevel = m.m_code[0] == 0;  // and report it
            }
            else
                nRead = static_cast<int>(t); // an error code
        }
    }
    return nRead;
}

//=================================== Buffered version ===============================================
//! Buffered marker channel constructor
/*!
\param file  The file that this channel is part of.
\param nChan The channel number in the file.
\param kind  The type of the channel (Marker or EventBoth).
\param bSize Size of the circular buffer in data items.
*/
CBMarkerChan::CBMarkerChan(TSon64File& file, TChanNum nChan, TDataKind kind, size_t bSize)
    : CMarkerChan(file, nChan, kind)
    , m_nMinMove( bSize >> CircBuffMinShift )
{
    m_pCirc = std::make_unique<circ_buff>(bSize);
}

void CBMarkerChan::Save(TSTime64 t, bool bSave)
{
    TBufLock lock(m_mutBuf);                            // acquire the buffer
    TSTime64 tLast = LastCommittedWriteTime();          // cannot change before this
    m_st.SetSave(t < tLast ? tLast+1 : t, bSave);
}

void CBMarkerChan::SaveRange(TSTime64 tFrom, TSTime64 tUpto)
{
    TBufLock lock(m_mutBuf);                            // acquire the buffer
    TSTime64 tLast = LastCommittedWriteTime();          // cannot change before this
    m_st.SaveRange(tFrom < tLast ? tLast : tFrom, tUpto);
}

bool CBMarkerChan::IsSaving(TSTime64 tAt) const
{
    TBufLock lock(m_mutBuf);                            // acquire the buffer
    return m_st.IsSaving(tAt);
}

int  CBMarkerChan::NoSaveList(TSTime64* pTimes, int nMax, TSTime64 tFrom, TSTime64 tUpto) const
{
    TBufLock lock(m_mutBuf);                            // acquire the buffer
    return m_st.NoSaveList(pTimes, nMax, tFrom, tUpto);
}

void CBMarkerChan::LatestTime(TSTime64 t)
{
    TBufLock lock(m_mutBuf);                            // acquire the buffer
    TSTime64 tLast = m_pCirc ? m_pCirc->LastTime() : -1;
    m_st.SetDeadRange(tLast, t, eSaveTimes::eST_MaxDeadEvents);
}

//! Resize the circular buffer
void CBMarkerChan::ResizeCircular(size_t nItems)
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

//! Get the last written level state (true=high, false=low)
/*!
 Use for EventBoth channels only. You MUST hold the buffer mutex.
 If we have anything in our circular buffer, look at the last item and report
 the level. If we have none, get the initial level as we cannot have anything
 in the CMarkerChan buffer (otherwise we would items in the circular buffer).
 We cannot call CMarkerChan::LastWriteLevel() a we do not have a lock on the channel.
 \return    true if the code of the last item written was not 0. If there are no
            items at all it returns the channel state at the start of sampling.
*/
bool CBMarkerChan::LastWriteLevel() const
{
    assert(EventBoth == ChanKind());
    if (!m_pCirc || m_pCirc->empty())           // if no buffer or empty...
        return CMarkerChan::LastWriteLevel();   // ...ask the channel
    return m_pCirc->back().m_code[0] != 0;
}

//! Write level event data (and avoid duplicating times)
/*!
\param pData    The event times to write to the buffer as Markers.
\param count    The number of items to write.
*/
int CBMarkerChan::WriteData(const TSTime64* pData, size_t count)
{
    if (count == 0)
        return 0;

    if (EventBoth != ChanKind())
        return CHANNEL_TYPE;

    TBufLock lock(m_mutBuf);            // acquire the buffer
    bool bLastLevel = LastWriteLevel();
    vector<TMarker> marks;              // buffer space for converting data
    count = Level2Marker(marks, pData, count, bLastLevel);  // convert data

    // Will we be duplicating a time? If so, 2 at the same time are treated as
    // a glitch, so just dump the repeated time and say we wrote one less.
    if (m_pCirc && !m_pCirc->empty() && (m_pCirc->back().m_time == pData[0]))
    {
        m_pCirc->sub();                 // remove the last duplicated time
        if (count > 1)
            return WriteDataLocked(&marks[1], count-1); // skip first item
        else
            return 0;
    }
    else
        return WriteDataLocked(&marks[0], count);       // write as marker
}

//! Write Marker data into a Marker channel
int CBMarkerChan::WriteData(const TMarker* pData, size_t count)
{
    if (count == 0)                                     // detect stupid cases...
        return 0;                                       // ...and do nothing

    TBufLock lock(m_mutBuf);                            // acquire the buffer
    return WriteDataLocked(pData, count);
}

//! Write Marker data into a Marker channel when you hold the channel lock
int CBMarkerChan::WriteDataLocked(const TMarker* pData, size_t count)
{
    if (!m_pCirc || !m_pCirc->capacity())               // If buffer is no use to us...
        return CMarkerChan::WriteData(pData, count);    // ...use the unbuffered version

    size_t written = m_pCirc->add(pData, count);        // attempt add to buffer
    count -= written;
    if (count == 0)                                     // if all added ok...
        return 0;                                       // ...we are done

    // To get here the circular buffer is now full
    pData += written;
    size_t spaceNeeded = count;
    if (spaceNeeded < m_nMinMove)                       // Don't move tiny ammounts...
        spaceNeeded = m_nMinMove;                       // ...as inefficient

    // To move data to the disk buffer, we need contiguous elements from our circular buffer.
    int err = 0;
    const size_t capacity = m_pCirc->capacity();        // how much we could put in
    if (spaceNeeded >= capacity)                        // we have a lot to write
    {
        err = CommitToWriteBuffer(TSTIME64_MAX);        // commit everything we have
        if (err)
            return err;
        m_pCirc->flush();                               // buffer is now empty
        m_st.SetFirstTime(pData[0]);                    // and move time on

        // We want to leave the buffer as full as we can so that oldest data possible is
        // visible to the user. OPTIMISE: if buffer state is all or none, we could skip
        // moving data through the buffer and just write it.
        while (count > capacity)                        // if more than can fit in buffer
        {
            size_t nWrite = std::min(count-capacity, capacity); // max to write
            m_pCirc->add(pData, nWrite);
            count -= nWrite;
            pData += nWrite;
            err = CommitToWriteBuffer(TSTIME64_MAX);    // flush all to disk buffers
            if (err)
                return err;
            m_pCirc->flush();                           // buffer is now empty
        }
        m_st.SetFirstTime(pData[0]);                    // committed up to here
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

    written = m_pCirc->add(pData, count);           // this should now fit
    assert(written == count);

    return 0;
}

//! Commit all data up to the nominated time to the write buffer.
/*!
We are to take notice of the m_st list. We know that the list holds where we reached
in committing. However, if this is an EventBoth channel, then we cannot skip over items,
so we write everything. In theory it would be possible to handle this, perhaps by
always returning the state to the initial state at the start and end of each section,
but for now we just save the lot.
\param tUpto The time up to which, but not including, we are to commit.
\return      S64_OK (0) or a negative error code.
*/
int CBMarkerChan::CommitToWriteBuffer(TSTime64 tUpto)
{
    assert(m_pCirc);
    TSTime64 tFrom, tTo;
    if (m_chanHead.m_chanKind == EventBoth) // If a level event channel...
    {                                       // ...ignore save ranges
        tFrom = LastCommittedWriteTime()+1; // Do NOT write stuff already written   
        tTo = tUpto;
        if (tFrom < tUpto)                  // Only if stuff to write
        {
            CircBuffer<TMarker>::range r[2];    // pair of ranges
            size_t n = m_pCirc->contig_range(tFrom, tTo, r);
            for (size_t i = 0; i<n; ++i)
            {
                int err = CMarkerChan::WriteData(r[i].m_pData, r[i].m_n);
                if (err)
                    return err;
            }
        }
    }
    else
    {
        TSTime64 tLastWrite = LastCommittedWriteTime();
        if (!m_st.FirstSaveRange(&tFrom, &tTo, tUpto, tLastWrite+1))
            return 0;

        do
        {
            CircBuffer<TMarker>::range r[2];    // pair of ranges
            size_t n = m_pCirc->contig_range(tFrom, tTo, r);
            for (size_t i = 0; i<n; ++i)
            {
                int err = CMarkerChan::WriteData(r[i].m_pData, r[i].m_n);
                if (err)
                    return err;
            }
        }while(m_st.NextSaveRange(&tFrom, &tTo, tUpto));
    }
    return 0;
}

// Limit disk reads to stop at the first buffer time, then read from the buffer.
int CBMarkerChan::ReadData(TSTime64* pData, CSRange& r, const CSFilter* pFilter)
{
    assert(r.HasRange());
    if (TestNullFilter(pFilter))                // if filter removes everything...
        return 0;                               // ...we are done

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
        CircBuffer<TMarker>::range x[2];        // will hold 0, 1 or 2 circ buff ranges
        size_t n = m_pCirc->contig_range(r.From(), tReadEnd, x);
        for (size_t i = 0; r.Max() && (i<n); ++i)
        {
            if (pFilter)
            {
                TMarker* pFrom = x[i].m_pData;  // points to source to move
                size_t nCopied = 0;             // number actually copied
                TMarker* pLimit = pFrom + x[i].m_n;
                while (pFrom < pLimit)
                {
                    if (pFilter->Filter(*pFrom))
                    {
                        *pData++ = *pFrom;
                        if (++nCopied >= r.Max())
                            break;
                    }
                    ++pFrom;
                }
                nRead += static_cast<int>(nCopied);
                r.ReduceMax(nCopied);
            }
            else
            {
                size_t nCopy = std::min(r.Max(), x[i].m_n); // maximum items to copy
                TMarker* pFrom = x[i].m_pData;
                TMarker* pLimit = pFrom + nCopy;
                while (pFrom < pLimit)
                    *pData++ = *pFrom++;
                nRead += static_cast<int>(nCopy);
                r.ReduceMax(nCopy);
            }
        }
    }
    return nRead;
}


// Limit disk reads to stop at the first buffer time, then read from the buffer.
int CBMarkerChan::ReadData(TMarker* pData, CSRange& r, const CSFilter* pFilter)
{
    if (!r.HasRange())
        assert(r.HasRange());
    if (TestNullFilter(pFilter))                // if filter removes everything...
        return 0;                               // ...we are done

    TBufLock lock(m_mutBuf);                    // acquire the buffer
    if (!m_pCirc || m_pCirc->empty())           // make sure we have one
        return CMarkerChan::ReadData(pData, r, pFilter);

    TSTime64 tReadEnd( r.Upto() );              // save end
    TSTime64 tBufStart = m_pCirc->FirstTime();
    r.SetUpto(std::min(tBufStart, r.Upto()));   // limit disk reads
    int nRead = CMarkerChan::ReadData(pData, r, pFilter);
    if ((nRead < 0) || r.IsTimedOut())          // read error or timed out
        return nRead;

    // Now read data from the circular buffer
    if ((r.Max()) && (tReadEnd > tBufStart))    // if buffered data waiting for us
    {
        pData += nRead;                         // where to read to
        CircBuffer<TMarker>::range x[2];        // will hold 0, 1 or 2 circ buff ranges
        size_t n = m_pCirc->contig_range(r.From(), tReadEnd, x);
        for (size_t i = 0; r.Max() && (i<n); ++i)
        {
            if (pFilter)
            {
                TMarker* pFrom = x[i].m_pData;  // points to source to move
                size_t nCopied = 0;             // number actually copied
                TMarker* pLimit = pFrom + x[i].m_n;
                while (pFrom < pLimit)
                {
                    if (pFilter->Filter(*pFrom))
                    {
                        *pData++ = *pFrom;
                        if (++nCopied >= r.Max())
                            break;
                    }
                    ++pFrom;
                }
                nRead += static_cast<int>(nCopied);
                r.ReduceMax(nCopied);
            }
            else
            {
                size_t nCopy = std::min(r.Max(), x[i].m_n); // maximum items to copy
                memcpy(pData, x[i].m_pData, nCopy*sizeof(TMarker));
                pData += nCopy;
                nRead += static_cast<int>(nCopy);
                r.ReduceMax(nCopy);
            }
        }
    }
    return nRead;
}

bool CBMarkerChan::IsModified() const
{
    TBufLock lock(m_mutBuf);                // acquire the buffer
    if (m_pCirc && !m_pCirc->empty())
    {
        TSTime64 tFrom, tTo;
        return m_st.FirstSaveRange(&tFrom, &tTo, TSTIME64_MAX);
    }
    return CMarkerChan::IsModified();
}

uint64_t CBMarkerChan::GetChanBytes() const
{
    TBufLock lock(m_mutBuf);                        // acquire the buffer
    uint64_t total = CMarkerChan::GetChanBytes();   // get committed bytes
    if (m_pCirc && !m_pCirc->empty())
    {
        TSTime64 t = LastCommittedWriteTime();
        total += m_pCirc->Count(t+1)*sizeof(TMarker);    // add uncommitted bytes in buffer
    }
    return total;
}

// Make sure that anything committable in the circular buffer is committed
int CBMarkerChan::Commit()
{
    TBufLock lock(m_mutBuf);                // acquire the buffer
    int err = 0;
    if (m_pCirc && !m_pCirc->empty())
        err = CommitToWriteBuffer();

    if (err == 0)
        err = CMarkerChan::Commit();        // flush data through to disk
    return err;
}

// Find the event that is n before the sTime. Put another way, find the event that if we read n
// events, the last event read would be the one before sTime.
// r        The range and number of items to skip back
// pFilter  To filter the events unless nullptr.
TSTime64 CBMarkerChan::PrevNTime(CSRange& r, const CSFilter* pFilter, bool bAsWave)
{
    if (!r.HasRange() || TestNullFilter(pFilter)) // Check it is even possible
        return -1;

    TBufLock lock(m_mutBuf);                // acquire the buffer
    if (m_pCirc && !m_pCirc->empty())
    {
        TSTime64 t = m_pCirc->PrevNTime(r, pFilter);
        if (r.Max() == 0)
            return t;
    }

    return CMarkerChan::PrevNTime(r, pFilter);
}

TSTime64 CBMarkerChan::MaxTime() const
{
    TBufLock lock(m_mutBuf);                // acquire the buffer
    return (!m_pCirc || m_pCirc->empty()) ? CMarkerChan::MaxTime() : m_pCirc->LastTime();
}

// Edit a marker at an exactly matching time.
int CBMarkerChan::EditMarker(TSTime64 t, const TMarker* pM, size_t nCopy)
{
    TBufLock lock(m_mutBuf);                    // acquire the buffer
    if (!m_pCirc || m_pCirc->empty())           // make sure we have one
        return CMarkerChan::EditMarker(t, pM, nCopy);

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
    return CMarkerChan::EditMarker(t, pM, nCopy) | iReturn;
}

//! Create a new marker channel
/*
 For this to work, the nominated channel must be in range and must not be in use (but
 it can have been used and then marked as deleted). If the channel was deleted you will
 reuse the existing channel space before any further space is allocated. This operation
 will reset the channel strings and settings apart from rate and physical channel.
 \param chan    The channel number in the file (0 up to m_vChanHead.size())
 \param dRate   The expected channel event rate in Hz
 \param kind    The channel type, being Marker or EventBoth
 \param iPhyCh  The physical channel number (application use), not checked
 \return        S64_OK if no problem detected or an error code
*/
int TSon64File::SetMarkerChan(TChanNum chan, double dRate, TDataKind kind, int iPhyCh)
{
    if ((kind != Marker) && (kind != EventBoth))
        return CHANNEL_TYPE; 
    TChWrLock lock(m_mutChans);     // we will be changing m_vChan
    int err = ResetForReuse(chan);  // check channel in a decent state
    if (err == S64_OK)
    {
        m_vChan[chan].reset(m_bOldFile ? new CMarkerChan(*this, chan, kind) : new CBMarkerChan(*this, chan, kind));
        m_vChan[chan]->SetPhyChan(iPhyCh);
        m_vChan[chan]->SetIdealRate(dRate);
    }
    return err;
}

//! Write marker data to a marker channel
/*!
 The channel must exist and the file should not be read only. The data must be
 in time order and occur after any data already written to the file.
 \param chan  The channel number in the file (0 up to m_vChanHead.size())
 \param pData Points at an array of data
 \param count Number of items to add
 \return      S64_OK (0) or an error code
*/
int TSon64File::WriteMarkers(TChanNum chan, const TMarker* pData, size_t count)
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

//! Read marker data from a marker or extended marker channel
/*!
 The channel must exist and be in use. You can read markers from any channel type that is
 descended from a marker, including EventBoth data (which is stored as a marker in this
 version of the SON library - the 32-bit SON library stored it as an event).
 \param chan    The channel number in the file (0 up to m_vChanHead.size())
 \param pData   Points at an array of data to be filled
 \param nMax    Maximum number of items to add to the array
 \param tFrom   Earliest time we are interested in
 \param tUpto   One tick beyond the times we are interested in (up to but not including)
 \param pFilter Either nullptr, omitted or points at a marker filter
 \return        The number of items read or a negative error code
*/
int TSon64File::ReadMarkers(TChanNum chan, TMarker* pData, int nMax, TSTime64 tFrom, TSTime64 tUpto, const CSFilter* pFilter)
{
    assert((nMax > 0) && (tFrom < tUpto) && (tUpto > 0));
    if (nMax <= 0)
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
        pData += n;                 // move pointer onwards
    }
}

// Locate a marker at a given time, then modify it
int TSon64File::EditMarker(TChanNum chan, TSTime64 t, const TMarker* pM, size_t nCopy)
{
    if (nCopy < sizeof(TSTime64))
        return BAD_PARAM;
    TChRdLock lock(m_mutChans);     // we are not changing the #chans
    if ((chan >= m_vChanHead.size()) || !m_vChan[chan])
        return NO_CHANNEL;
    return m_vChan[chan]->EditMarker(t, pM, nCopy);
}

//========================== Level channel File support ===============================

//! Create a new level channel
/*
 For this to work, the nominated channel must be in range and must not be in use (but
 it can have been used and then marked as deleted). If the channel was deleted you will
 reuse the existing channel space before any further space is allocated. This operation
 will reset the channel strings and settings apart from rate and physical channel.
 \param chan    The channel number in the file (0 up to m_vChanHead.size())
 \param dRate   The expected channel event rate in Hz
 \param evtKind The channel type, being EventFall, EventRise or EventBoth
 \param iPhyCh  The physical channel number (application use), not checked
 \return        S64_OK if no problem detected or an error code
*/
int TSon64File::SetLevelChan(TChanNum chan, double dRate, int iPhyCh)
{
    return SetMarkerChan(chan, dRate, EventBoth, iPhyCh);
}

//! Set the initial level for an EventBoth channel
/*!
 This is only valid for EventBoth channels and sets the initial level. Once a write to
 the channel has happened this become irrelevant (changes make no difference).
 \param chan    The channel number, must be EventBoth
 \param bLevel  false = low before start (first will be high), true = high before start
 \return        S64_OK (0) for no problem detected or a negative error code
*/
int TSon64File::SetInitLevel(TChanNum chan, bool bLevel)
{
    TChRdLock lock(m_mutChans);     // we are not changing the #chans
    if ((chan >= m_vChanHead.size()) || !m_vChan[chan])
        return NO_CHANNEL;

    return m_vChan[chan]->SetInitLevel(bLevel);
}

//! Write level data to a marker channel
/*!
 The channel must exist as an EventBoth channel and the file should not be read only. The
 data must be in time order and occur after any data already written to the file.
 \param chan  The channel number in the file (0 up to m_vChanHead.size())
 \param pData Points at an array of data
 \param count Number of items to add
 \return      S64_OK (0) or an error code
*/
int TSon64File::WriteLevels(TChanNum chan, const TSTime64* pData, size_t count)
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

//! Read level data from a marker or extended marker channel
/*!
 The channel must exist and be in use. You can read markers from any channel type that is
 descended from a marker, including EventBoth data (which is stored as a marker in this
 version of the SON library - the 32-bit SON library stored it as an event).
 \param chan   The channel number in the file (0 up to m_vChanHead.size())
 \param pData  Points at an array of data to be filled
 \param nMax   Maximum number of items to add to the array
 \param tFrom  Earliest time we are interested in
 \param tUpto  One tick beyond the times we are interested in (up to but not including)
 \param bLevel If the call returns with no error this is the level of the first event, or if
               no events, it is the level before the first event time.
 \return       The number of items read or a negative error code
*/
int TSon64File::ReadLevels(TChanNum chan, TSTime64* pData, int nMax, TSTime64 tFrom, TSTime64 tUpto, bool &bLevel)
{
    assert((nMax > 0) && (tFrom < tUpto) && (tUpto > 0));
    if (nMax <= 0)
        return 0;
    TChRdLock lock(m_mutChans);     // we are not changing the #chans
    if ((chan >= m_vChanHead.size()) || !m_vChan[chan])
        return NO_CHANNEL;

    CSRange r(tFrom, tUpto, nMax);   // make a range object to manage the request
    int nGot = 0;
    while (true)
    {
        int n = m_vChan[chan]->ReadLevelData(pData, r, bLevel);
        if (n < 0)
            return n;

        nGot += n;
        if (!r.IsTimedOut() || !r.Max())
            return nGot;

        r.SetTimeOut();             // allow to run again
        pData += n;                 // move pointer onwards
    }
}

