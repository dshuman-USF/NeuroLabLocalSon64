// s64event.cpp
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

// circular buffer specialization for events

//! Find the item r.Max() before r.Upto()
/*!
We know that this is the first buffer we look at, so no need to be contiguous.
\param r       The range to search. r.From() is the earliest to go back to, r.Upto()
                is the end of the range.
\param pFilter nullptr or a filter object to filter the data.
\return        -1 if not found or the time.
*/
template<>
TSTime64 CircBuffer<TSTime64>::PrevNTime(CSRange& r, const CSFilter* pFilter) const
{
    if (r.Upto() <= FirstTime())  // nothing wanted here
        return -1;
    auto it = Find(r.Upto());   // find first at or after the range end
    if (it == begin())          // if this is start, then...
        return -1;              // ...nothing in here is wanted

    auto i = it - begin();      // index into buffer
    auto n = r.Max();           // number to skip
    if (static_cast<uint32_t>(i) >= n)  // if found in the buffer
    {
        r.ZeroMax();            // flag we are done
        return operator[](i-n);
    }

    r.ReduceMax(static_cast<size_t>(i));    // reduce count to go back
    r.SetUpto(FirstTime());     // reduce time range to search
    return -1;
}

//=========================== CEventChan =================================
//! Construct a CEventChan
/*!
\param file     The file that this channel belongs to.
\param nChan    The channel number in the file.
\param evtKind  The type of the channel, one of EventFall or EventRise.
*/
CEventChan::CEventChan(TSon64File& file, TChanNum nChan, TDataKind evtKind)
    : CSon64Chan(file, nChan, evtKind)
{
    assert((evtKind == EventFall) || (evtKind == EventRise));
    m_chanHead.m_nObjSize = sizeof(TSTime64);
    if (!m_bmRead.HasDataBlock())       // this should NOT have one
        m_bmRead.SetDataBlock(new CEventBlock(nChan));
}

// Write event times to the event channel.
// pData    The list of times to add. They must be in order and be after any previous
//          times written to the file.
// count    The number of items to write.
// You MUST NOT be holding the channel mutex to call this.
int CEventChan::WriteData(const TSTime64* pData, size_t count)
{
    if (count == 0)                     // beware NULL operations
        return 0;

    TChanLock lock(m_mutex);            // take ownership of the channel
    if (pData[0] <= m_chanHead.m_lastTime)
        return OVER_WRITE;

    // Make sure we have a write buffer
    int err = m_pWr ? 0 : InitWriteBlock(new CEventBlock(m_nChan));

    // Write buffers holds last data written, if full, data is on disk
    while ((err == 0) && count)
    {
        CEventBlock* pWr = static_cast<CEventBlock*>(m_pWr.get()); // get the raw pointer
        size_t nCopy = pWr->AddData(pData, count);
        count -= nCopy;                 // keep counter lined up with the pointer
        if ( pWr->full() )              // if buffer is full...
            err = AppendBlock(pWr);     // ...write it to the file, clears unsaved flag
    }
    return err;
}

//=================================== Buffered version ===============================================
//! Construct a buffered Event channel
/*!
\param file     The file that this channel belongs to.
\param nChan    The channel number in the file.
\param evtKind  The type of the channel, one of EventFall or EventRise.
\param bSize    The buffer size in data items (events).
*/
CBEventChan::CBEventChan(TSon64File& file, TChanNum nChan, TDataKind evtKind, size_t bSize)
    : m_nMinMove( bSize >> CircBuffMinShift )
    , CEventChan(file, nChan, evtKind)
{
    m_pCirc = std::make_unique<circ_buff>(bSize);
}

void CBEventChan::Save(TSTime64 t, bool bSave)
{
    TBufLock lock(m_mutBuf);                            // acquire the buffer
    TSTime64 tLast = LastCommittedWriteTime();          // cannot change before this
    m_st.SetSave(t < tLast ? tLast+1 : t, bSave);
}

void CBEventChan::SaveRange(TSTime64 tFrom, TSTime64 tUpto)
{
    TBufLock lock(m_mutBuf);                            // acquire the buffer
    TSTime64 tLast = LastCommittedWriteTime()+1;        // cannot change before this
    m_st.SaveRange(tFrom < tLast ? tLast : tFrom, tUpto);
}

bool CBEventChan::IsSaving(TSTime64 tAt) const
{
    TBufLock lock(m_mutBuf);                            // acquire the buffer
    return m_st.IsSaving(tAt);
}

int  CBEventChan::NoSaveList(TSTime64* pTimes, int nMax, TSTime64 tFrom, TSTime64 tUpto) const
{
    TBufLock lock(m_mutBuf);                            // acquire the buffer
    return m_st.NoSaveList(pTimes, nMax, tFrom, tUpto);
}

void CBEventChan::LatestTime(TSTime64 t)
{
    TBufLock lock(m_mutBuf);                            // acquire the buffer
    TSTime64 tLast = m_pCirc ? m_pCirc->LastTime() : -1;
    m_st.SetDeadRange(tLast, t, eSaveTimes::eST_MaxDeadEvents);
}

// Setting 0 size kills off circular buffering and frees up resources
void CBEventChan::ResizeCircular(size_t nItems)
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

int CBEventChan::WriteData(const TSTime64* pData, size_t count)
{
    if (count == 0)                                     // detect stup cases...
        return 0;                                       // ...and do nothing

    TBufLock lock(m_mutBuf);                            // acquire the buffer
    if (!m_pCirc || !m_pCirc->capacity())               // if no buffer or no space
        return CEventChan::WriteData(pData, count);

    size_t written = m_pCirc->add(pData, count);        // write what we can
    count -= written;
    if (count == 0)                                     // if all added ok...
        return 0;                                       // ...we are done

    // To get here, the circular buffer is full. 
    pData += written;
    size_t spaceNeeded = count;
    if (spaceNeeded < m_nMinMove)                       // Don't move tiny ammounts...
        spaceNeeded = m_nMinMove;                       // ...as inefficient

    // To move data to the disk buffer, we need contiguous elements from our circular buffer.
    int err = 0;
    const size_t capacity = m_pCirc->capacity();        // how much we could put in
    TSTime64 tNewFirstInBuffer;                         // will be first time in buffer
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
        tNewFirstInBuffer = *pData;                     // the new first time
    }
    else
    {
        tNewFirstInBuffer = (*m_pCirc)[spaceNeeded];    // Will end up as the first time
        err = CommitToWriteBuffer(tNewFirstInBuffer);   // flush up to needed time
        if (err)
            return err;
        m_pCirc->free(spaceNeeded);                     // release used space
    }

    m_st.SetFirstTime(tNewFirstInBuffer);           // committed up to here
    written = m_pCirc->add(pData, count);           // this should now fit
    assert(written == count);

    return 0;
}

//! Commit all data up to the nominated time to the write buffer.
/*!
We are to take notice of the m_st list. We know that the list holds where we reached
in committing.
\param tUpto The time up to which (but not including) we are to commit.
\return      S64_OK (0) or a negative error code.
*/
int CBEventChan::CommitToWriteBuffer(TSTime64 tUpto)
{
    assert(m_pCirc);
    TSTime64 tFrom, tTo;
    TSTime64 tLastWrite = LastCommittedWriteTime();
    if (!m_st.FirstSaveRange(&tFrom, &tTo, tUpto, tLastWrite+1))
        return 0;
    do
    {
        CircBuffer<TSTime64>::range r[2];    // pair of ranges
        size_t n = m_pCirc->contig_range(tFrom, tTo, r);
        for (size_t i = 0; i<n; ++i)
        {
            int err = CEventChan::WriteData(r[i].m_pData, r[i].m_n);
            if (err)
                return err;
        }
    }while(m_st.NextSaveRange(&tFrom, &tTo, tUpto));
    return 0;
}

// Limit disk reads to stop at the first buffer time, then read from the buffer.
int CBEventChan::ReadData(TSTime64* pData, CSRange& r, const CSFilter* pFilter)
{
    assert(r.HasRange());
    TBufLock lock(m_mutBuf);                    // acquire the buffer
    if (!m_pCirc || m_pCirc->empty())
        return CEventChan::ReadData(pData, r, pFilter);

    TSTime64 tReadEnd( r.Upto() );              // save end
    TSTime64 tBufStart = m_pCirc->FirstTime();
    r.SetUpto(std::min(tBufStart, r.Upto()));   // limit disk reads
    int nRead = CEventChan::ReadData(pData, r, pFilter);
    if ((nRead < 0) || r.IsTimedOut())          // read error or timed out
        return nRead;

    // Now read data from the circular buffer
    if ((r.Max()) && (tReadEnd > tBufStart))    // if buffered data waiting for us
    {
        pData += nRead;                         // where to read to
        CircBuffer<TSTime64>::range x[2];       // will hold 0, 1 or 2 circ buff ranges
        size_t n = m_pCirc->contig_range(r.From(), tReadEnd, x);
        for (size_t i = 0; r.Max() && (i<n); ++i)
        {
            size_t nCopy = std::min(r.Max(), x[i].m_n); // how much to copy
            memcpy(pData, x[i].m_pData, nCopy*sizeof(TSTime64));
            pData += nCopy;
            nRead += static_cast<int>(nCopy);
            r.ReduceMax(static_cast<size_t>(nCopy));
        }
    }
    return nRead;
}

bool CBEventChan::IsModified() const
{
    TBufLock lock(m_mutBuf);                // acquire the buffer
    if (m_pCirc && !m_pCirc->empty())
    {
        TSTime64 tFrom, tTo;
        return m_st.FirstSaveRange(&tFrom, &tTo, TSTIME64_MAX);
    }
    return CEventChan::IsModified();
}

uint64_t CBEventChan::GetChanBytes() const
{
    TBufLock lock(m_mutBuf);                     // acquire the buffer
    uint64_t total = CEventChan::GetChanBytes(); // get committed bytes
    if (m_pCirc && !m_pCirc->empty())
    {
        TSTime64 t = LastCommittedWriteTime();
        total += m_pCirc->Count(t+1)*sizeof(TSTime64);    // add uncommitted bytes in buffer
    }
    return total;
}

// Make sure that anything committable in the circular buffer is committed
int CBEventChan::Commit()
{
    TBufLock lock(m_mutBuf);                // acquire the buffer
    int err = 0;
    if (m_pCirc && !m_pCirc->empty())
        err = CommitToWriteBuffer();

    if (err == 0)
        err = CEventChan::Commit();         // flush data through to disk
    return err;
}

// Find the event that is n before the sTime. Put another way, find the event that if we read n
// events, the last event read would be the one before sTime.
// r        The range to search, back to r.From(), first before r.Upto().
//          r.Max() is number to skip. This is uint32_t in an effort to discourage
//          backward searches through the entire file.
// pFilter  Not used in this class; will filter the events.
TSTime64 CBEventChan::PrevNTime(CSRange& r, const CSFilter* pFilter, bool bAsWave)
{
    if (!r.HasRange())                      // Check it is even possible
        return -1;

    TBufLock lock(m_mutBuf);                // acquire the buffer
    if (m_pCirc && !m_pCirc->empty())
    {
        TSTime64 t = m_pCirc->PrevNTime(r, nullptr);
        if (!r.HasRange())
            return t;
    }

    return CEventChan::PrevNTime(r, pFilter);
}

//! Get the maximum time held in the channel
/*!
If we have no circular buffer, or the buffer is empty, we return the standard unbuffered
maximum time (which will likely be -1 as circular buffers on exist when writing new files).
\return The maximum time or -1.
*/
TSTime64 CBEventChan::MaxTime() const
{
    TBufLock lock(m_mutBuf);                // acquire the buffer
    return (!m_pCirc || m_pCirc->empty()) ? CEventChan::MaxTime() : m_pCirc->LastTime();
}

//============================= File routines =======================================

//======================= Create a new event channel =======================
// chan     The channel number in the file (0 up to m_vChanHead.size())
// dRate    The expected channel event rate in Hz
// evtKind  The channel type, being EventFall, EventRise or EventBoth
// iPhyCh   The physical channel number (application use), not checked
int TSon64File::SetEventChan(TChanNum chan, double dRate, TDataKind evtKind, int iPhyCh)
{
    if (evtKind == EventBoth)       // be generous and don't throw an error
        return SetLevelChan(chan, dRate, iPhyCh);

    if (evtKind == Marker)          // ditto for marker
        return SetMarkerChan(chan, dRate, Marker, iPhyCh);

    if ((evtKind != EventFall) && (evtKind != EventRise))
        return BAD_PARAM;
    TChWrLock lock(m_mutChans);     // we will be changing m_vChan
    int err = ResetForReuse(chan);  // check channel in a decent state
    if (err == S64_OK)
    {
        m_vChan[chan].reset(m_bOldFile ? new CEventChan(*this, chan, evtKind) : new CBEventChan(*this, chan, evtKind));
        m_vChan[chan]->SetPhyChan(iPhyCh);
        m_vChan[chan]->SetIdealRate(dRate);
    }
    return err;
}

int TSon64File::WriteEvents(TChanNum chan, const TSTime64* pData, size_t count)
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

int TSon64File::ReadEvents(TChanNum chan, TSTime64* pData, int nMax, TSTime64 tFrom, TSTime64 tUpto, const CSFilter* pFilter)
{
    assert(nMax > 0);
    if ((nMax <= 0) || (tUpto < 0) || (tFrom >= tUpto) )
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
