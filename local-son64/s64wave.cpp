// s64wave.cpp
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

//! \file s64wave.cpp
//! \brief This file contains classes to handle waves (with gaps) in a data block.
//! \internal

#include <iostream>      // for debugging messages
#include <assert.h>
#include "s64priv.h"
#include "s64chan.h"
#include "s64range.h"

using namespace std;
using namespace ceds64;

//! CAdcChan constructor
/*!
 The CAdcChan/CRealWChan classes handle a list of objects that are at a constant time
 separation, set by the  tDivide (ticks per item). We are allowed gaps in the list. Once
 we have data at a time, if the  first item is at time t, all subsequent items should be
 at time t+n*tDivide. However, we do not force this, however, if this is not the case, we
 will insert a gap - that is contiguous data is data at exactly that interval.

 At the moment we have waves of short (Adc) and of float (RealWave), but waves of
 arbitrary data are possible. We will write the class to deal with an arbitrary size and
 consider specialising it later.
 \param file    The data file that owns this channel.
 \param nChan   The channel number.
 \param tDivide The sample interval in file timebase units.
*/
CAdcChan::CAdcChan(TSon64File& file, TChanNum nChan, TSTime64 tDivide)
    : CSon64Chan(file, nChan, Adc)
{
    assert(tDivide > 0);            // should have been checked before we get here
    if (!m_bmRead.HasDataBlock())   // this should NOT have one
        m_bmRead.SetDataBlock(new CAdcBlock(nChan, tDivide));
    m_chanHead.m_nObjSize = sizeof(short);
    m_chanHead.m_tDivide = tDivide;
}

//! Write contiguous data to the channel at time tFrom
TSTime64 CAdcChan::WriteData(const short* pData, size_t count, TSTime64 tFrom)
{
    if (count == 0)                     // beware NULL operations
        return tFrom;

    TChanLock lock(m_mutex);            // take ownership of the channel

    // We allow overwriting of wave data if the times match exactly.
    TSTime64 tLast = MaxTimeNoLock();   // get maximum time (but not held in CAdcChan)
    TSTime64 tOff = tLast - tFrom;      // time past to do
    if (tOff >= 0)
    {
        int nWr = ChangeData(pData, count, tFrom);  // attempt to change old data
        if (nWr < 0)
            return nWr;

        TSTime64 tEnd = tFrom + (count-1)*m_chanHead.m_tDivide;
        if (tEnd <= tLast)                      // if no new data...
            return tEnd + m_chanHead.m_tDivide; // ...we are done.

        size_t nSkip = 1 + static_cast<size_t>(tOff / m_chanHead.m_tDivide);
        count -= nSkip;
        pData += nSkip;
        tFrom += nSkip*m_chanHead.m_tDivide;
    }

    // make sure we have a write buffer
    int err = m_pWr ? 0 : InitWriteBlock(new CAdcBlock(m_nChan, m_chanHead.m_tDivide));

    // Write buffer holds last data written, if full, data is on disk
    while (count && (err == 0))
    {
        CAdcBlock* pWr = static_cast<CAdcBlock*>(m_pWr.get()); // get the raw pointer
        size_t nCopy = pWr->AddData(pData, count, tFrom);
        count -= nCopy;
        tFrom += nCopy*m_chanHead.m_tDivide;    // Cannot call ChanDivde() due to lock
        if ((nCopy==0) || pWr->SpaceContiguous() == 0)  // if buffer is full
        {
            err = AppendBlock(pWr);     // add to the file on disk, clear unsaved flag...
            if (count)                  // ...if more to write...
                pWr->clear();           // ...set empty so we can write more.
        }
    }
    return err<0 ? err : tFrom;
}

//! Overwrite existing data
/*!
If the tFrom is not aligned properly we simple-mindedly work out indices to overwrite.
If there are gaps we skip over them. Data could be in the read buffer, or pending
write in the write buffer.
\param pData Points at the data to be written.
\param count The number of data points available for writing.
\param tFrom The time of the first data point in pData.
\return Negative error code, 0 if all changeable data has been changed
*/
int CAdcChan::ChangeData(const short* pData, size_t count, TSTime64 tFrom)
{
    assert(count > 0);          // madness check

    // If our change overlaps the write buffer, deal with this first.
    if (m_pWr)
    {
        size_t first;           // will be index of the first one used
        int nUsed = m_pWr->ChangeWave(pData, count, tFrom, first);
        if (nUsed > 0)          // if we updated something...
        {
            assert(first < count);
            count = first;      // change count left to do
            if (count == 0)     // if we used the lot...
                return 0;       // ...there is nothing left to do
        }
        else if (nUsed == 0)    // if data is after block...
            return 0;           // ...then nothing left to do
    }

    // Now process the blocks already written to disk
    int iRet = m_bmRead.LoadBlock(tFrom);   // seek the start time

    while( count && (iRet == 0))
    {
        size_t first;           // will be index of the first one used
        int nUsed = m_bmRead.DataBlock().ChangeWave(pData, count, tFrom, first);
        if (nUsed > 0)          // if we updated something...
        {
            first += nUsed;     // next index in pData to use
            assert(first <= count);
            count -= first;     // count left to do
            pData += first;     // next pointer
            tFrom += first*m_chanHead.m_tDivide; // time matching new pData
        }
        else if (nUsed < 0)     // if past the data...
            break;              // ..we are done

        iRet = m_bmRead.SaveIfUnsaved(); // save if unsaved data
        if ((iRet == 0) && count)
            iRet = m_bmRead.NextBlock();    // returns 1 if we hit the end
    }

    return (iRet < 0) ? iRet : 0;
}

// Read contiguous data into the buffer from the channel. We ignore the filter as it
// does not apply to waveform data.
// pData    The buffer to append dat to
// r        The range and max items are in here
// tFirst   if r.First() then fill in with first time, otherwise leave alone.
//          if not first time, data must start at r.From().
// returns  Number of points read.
int CAdcChan::ReadData(short* pData, CSRange& r, TSTime64& tFirst, const CSFilter*)
{
    if (!r.HasRange())                      // if nothing possible...
        return 0;                           // ...bail out now

    int nRead = 0;                          // will be the count of read data

    TChanLock lock(m_mutex);                // take ownership of the channel

    // If we have a write buffer, and our time range includes data in the buffer, use that
    // in preference to reading from the disk. If the write buffer exists, it holds the
    // last block and holds at least 1 data value.
   TSTime64 tBufStart = m_pWr ? m_pWr->FirstTime() : TSTIME64_MAX;

    if (r.From() < tBufStart)     // Only read the disk if could be data
    {
        int err = m_bmRead.LoadBlock(r.From());    // get the block
        if (err < 0)
            return err;

        // If the data is on disk, read it, but if we reach the write buffer we
        // are done and collect the write buffer data. This is to make sure that
        // if the write buffer was partially written we get the new data.
        while ((err == 0) && (m_bmRead.DataBlock().FirstTime() < tBufStart))
        {
            size_t nCopy = m_bmRead.DataBlock().GetData(pData, r, tFirst);
            nRead += static_cast<int>(nCopy);
            if (!r.CanContinue())
                return nRead;
            err = m_bmRead.NextBlock();         // fetch next block
        }
    }

    // If we get here with room in the buffer and not done, then we must look in the write system to
    // see if there is pending data. BEWARE: if not first, then r.From() must match buffer start or
    // we cannot continue.
    // if a buffer, and something left to do and possible data
    if (m_pWr && r.CanContinue() && (r.Upto() > tBufStart) &&
        (r.First() || (r.From() == tBufStart))) // ...either first or contiguous
    {
        size_t nCopy = m_pWr->GetData(pData, r, tFirst);
        nRead += static_cast<int>(nCopy);
    }

    return nRead;
}


//========================= buffered Adc channel =======================================
//! Buffered wave channel constructor constructor
/*!
\param file    The data file that owns this channel.
\param nChan   The channel number.
\param tDivide The sample interval in file timebase units.
\param bSize   The buffer size in waveform points.
*/
CBAdcChan::CBAdcChan(TSon64File& file, TChanNum nChan, TSTime64 tDivide, size_t bSize)
    : m_nMinMove( bSize >> CircBuffMinShift )
    , CAdcChan(file, nChan, tDivide)
{
    m_pCirc = std::make_unique<circ_buff>(bSize, tDivide);
}

void CBAdcChan::Save(TSTime64 t, bool bSave)
{
    TBufLock lock(m_mutBuf);                            // acquire the buffer
    TSTime64 tLast = LastCommittedWriteTime();          // cannot change before this
    m_st.SetSave(t < tLast ? tLast+1 : t, bSave);
}

void CBAdcChan::SaveRange(TSTime64 tFrom, TSTime64 tUpto)
{
    TBufLock lock(m_mutBuf);                            // acquire the buffer
    TSTime64 tLast = LastCommittedWriteTime();          // cannot change before this
    m_st.SaveRange(tFrom < tLast ? tLast : tFrom, tUpto);
}

bool CBAdcChan::IsSaving(TSTime64 tAt) const
{
    TBufLock lock(m_mutBuf);                            // acquire the buffer
    return m_st.IsSaving(tAt);
}

int  CBAdcChan::NoSaveList(TSTime64* pTimes, int nMax, TSTime64 tFrom, TSTime64 tUpto) const
{
    TBufLock lock(m_mutBuf);                            // acquire the buffer
    return m_st.NoSaveList(pTimes, nMax, tFrom, tUpto);
}

void CBAdcChan::LatestTime(TSTime64 t)
{
    TBufLock lock(m_mutBuf);                            // acquire the buffer
    TSTime64 tLast = m_pCirc ? m_pCirc->LastTime() : -1;
    m_st.SetDeadRange(tLast, t);
}

void CBAdcChan::ResizeCircular(size_t nItems)
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

TSTime64 CBAdcChan::WriteData(const short* pData, size_t count, TSTime64 tFrom)
{
    if (count == 0)                                     // detect stup cases...
        return tFrom;                                   // ...and do nothing

    TBufLock lock(m_mutBuf);                            // acquire the buffer
    if (!m_pCirc || !m_pCirc->capacity())
        return CAdcChan::WriteData(pData, count, tFrom);

    // Now see if we are overwriting, in which case pass on to change code
    TSTime64 tLast = m_pCirc->LastTime();               // last time in buffer or -1
    if (tFrom <= tLast)                                 // if we overlap written data
    {
        int iRet = ChangeData(pData, count, tFrom);     // do whatever is needed
        if (iRet < 0)
            return iRet;

        // CHECK: the rounding up needs checking. Calculate points written from buffer
        size_t nWritten = count;        // default is that we wrote the lot
        TSTime64 tOff = m_pCirc->LastTime() - tFrom;    // time past to do
        if (tOff >= 0)                   // if some data left to do
        {
            nWritten = static_cast<size_t>(tOff / m_chanHead.m_tDivide) + 1;
            if (nWritten > count)       // cannot have written...
                nWritten = count;       // ...more than we have
        }

        count -= nWritten;
        pData += nWritten;
        tFrom += nWritten*m_chanHead.m_tDivide;

        if (count == 0)                 // if job is done...
            return tFrom;               // ...we are finsihed
    }

    int err = 0;
    if (tFrom != tLast + m_chanHead.m_tDivide)          // if not contiguous
    {
        err = CommitToWriteBuffer(TSTIME64_MAX);        // flush the lot
        if (err)
            return err;
        m_pCirc->flush(tFrom);                          // buffer is now empty
        m_st.SetFirstTime(tFrom);                       // so move saved times on
    }
    else if (m_pCirc->empty())
        m_pCirc->SetFirstTime(tFrom);

    // At this point, data is contiguous or the buffer is empty. The buffer start
    // is set.
    size_t written = m_pCirc->add(pData, count);        // add what we can
    tFrom += written*m_chanHead.m_tDivide;              // move time onwards
    count -= written;                                   // reduce what is left
    if (count == 0)                                     // if all added ok...
        return tFrom;                                   // ...we are done

    // At this point, the circular buffer is full and we have more data to write we
    // set ourselves a minimum number of points to move at a time to avoid thrashing
    pData += written;                                   // move data pointer onwards
    size_t spaceNeeded = count;
    if (spaceNeeded < m_nMinMove)                       // Don't move tiny ammounts...
        spaceNeeded = m_nMinMove;                       // ...as inefficient

    // To move data to the disk buffer, we need contiguous elements from our circular buffer.
    const size_t capacity = m_pCirc->capacity();        // how much we could put in
    if (spaceNeeded >= capacity)                        // we have a lot to write
    {
        err = CommitToWriteBuffer(TSTIME64_MAX);        // commit everything we have
        if (err)
            return err;
        m_pCirc->flush(tFrom);                          // buffer is now empty
        m_st.SetFirstTime(tFrom);                       // and move time on

        // We want to leave the buffer as full as we can so that oldest data possible is
        // visible to the user. OPTIMISE: if buffer state is all or none, we could skip
        // moving data through the buffer and just write it.
        while (count > capacity)                        // if more than can fit in buffer
        {
            size_t nWrite = std::min(count-capacity, capacity); // max to write
            m_pCirc->add(pData, nWrite);                // start time already set
            count -= nWrite;
            pData += nWrite;
            err = CommitToWriteBuffer(TSTIME64_MAX);    // flush all to disk buffers
            if (err)
                return err;
            tFrom += nWrite* m_chanHead.m_tDivide;      // move time on
            m_pCirc->flush(tFrom);                      // Empty, set start time
        }
        m_st.SetFirstTime(tFrom);                       // committed up to here
    }
    else
    {
        TSTime64 tUpto = m_pCirc->FirstTime() + spaceNeeded*m_chanHead.m_tDivide;       // final time
        err = CommitToWriteBuffer(tUpto);               // Commit up to needed time
        if (err)
            return err;

        // The buffer is currently full, and spaceNeeded < capacity, so free() will
        // not call flush, so the buffer start time will still be valid.
        m_pCirc->free(spaceNeeded);                     // release used space
    }

    // Any data written here MUST be contiguous
    written = m_pCirc->add(pData, count);               // this should now fit
    assert(written == count);

    return tFrom + written * m_chanHead.m_tDivide;
}

// Change any data that overlaps our buffer. We could be fussy and insist that
// times match exactly. This implementation takes data as extending up to the
// next data point. Returns 0 if done, -ve means an error.
int CBAdcChan::ChangeData(const short* pData, size_t count, TSTime64 tFrom)
{
    // Start by handling any changes in written data.
    int iRet = CAdcChan::ChangeData(pData, count, tFrom);
    if ((iRet < 0) || !m_pCirc || m_pCirc->empty())
        return iRet;

    return m_pCirc->change(pData, count, tFrom);
}

int CBAdcChan::ReadData(short* pData, CSRange& r, TSTime64& tFirst, const CSFilter* pFilter)
{
    assert(r.HasRange());
    TBufLock lock(m_mutBuf);                    // acquire the buffer
    if (!m_pCirc || m_pCirc->empty())
        return CAdcChan::ReadData(pData, r, tFirst, pFilter);

    TSTime64 tReadEnd( r.Upto() );              // save end
    TSTime64 tBufStart = m_pCirc->FirstTime();
    r.SetUpto(std::min(tBufStart, r.Upto()));   // limit disk reads
    int nRead = CAdcChan::ReadData(pData, r, tFirst, pFilter);
    if ((nRead < 0) || r.IsTimedOut())          // read error or timed out
        return nRead;

    pData += nRead;                             // where to read to
    r.SetUpto(tReadEnd);                        // restore end
    int nBuff = m_pCirc->read(pData, r.Max(), r.From(), tReadEnd, tFirst, r.First());
    r.ReduceMax(static_cast<size_t>(nBuff));
    nRead += nBuff;
    return nRead;
}

// Return the last time that we know of for this channel or -1 if no data
TSTime64 CBAdcChan::MaxTime() const
{
    TBufLock lock(m_mutBuf);                // acquire the buffer
    if (m_pCirc && !m_pCirc->empty())       // if data in the circular buffer...
        return m_pCirc->LastTime();         // ...return the time
    return CAdcChan::MaxTime();
}

// Find the event that is n before the tFrom. Put another way, find the event that if we read n
// events, the last event read would be the one before tFrom.
// r        The range to search, back to r.From(), first before r.Upto().
//          r.Max() is number to skip. This is uint32_t in an effort to discourage
//          backward searches through the entire file.
// pFilter  Not used in this class; will filter the events.
TSTime64 CBAdcChan::PrevNTime(CSRange& r, const CSFilter* pFilter, bool bAsWave)
{
    if (!r.HasRange())                      // Check it is even possible
        return -1;

    TBufLock lock(m_mutBuf);                // acquire the buffer
    if (m_pCirc && !m_pCirc->empty())
    {
        TSTime64 t = m_pCirc->PrevNTime(r);
        if (!r.HasRange())                  // If changed and cannot continue...
            return t;                       // ...this is the last time we found
    }

    return CAdcChan::PrevNTime(r, pFilter);
}

bool CBAdcChan::IsModified() const
{
    TBufLock lock(m_mutBuf);                // acquire the buffer
    if (m_pCirc && !m_pCirc->empty())
    {
        TSTime64 tFrom, tTo;
        return m_st.FirstSaveRange(&tFrom, &tTo, TSTIME64_MAX);
    }
    return CAdcChan::IsModified();
}

uint64_t CBAdcChan::GetChanBytes() const
{
    TBufLock lock(m_mutBuf);                    // acquire the buffer
    uint64_t total = CAdcChan::GetChanBytes();  // get committed bytes
    if (m_pCirc && !m_pCirc->empty())
    {
        TSTime64 t = LastCommittedWriteTime();
        total += m_pCirc->Count(t+1)*sizeof(short); // add uncommitted bytes in buffer
    }
    return total;
}

// Make sure that anything committable in the circular buffer is committed
int CBAdcChan::Commit()
{
    TBufLock lock(m_mutBuf);                // acquire the buffer
    int err = 0;
    if (m_pCirc && !m_pCirc->empty())
        err = CommitToWriteBuffer();

    if (err == 0)
        err = CAdcChan::Commit();           // flush data through to disk
    return err;
}

//! Commit all data up to the nominated time to the write buffer.
/*!
We are to take notice of the m_st list. We know that the list holds where we reached
in committing.
\param tUpto Commit data up to this time.
\return 0 if OK or a negative error code.
*/
int CBAdcChan::CommitToWriteBuffer(TSTime64 tUpto)
{
    assert(m_pCirc);
    TSTime64 tFrom, tTo;
    if (!m_st.FirstSaveRange(&tFrom, &tTo, tUpto, m_pCirc->FirstDirty()))
        return 0;
    do
    {
        CircWBuffer<short>::range r[2];    // pair of ranges
        size_t n = m_pCirc->contig_range(tFrom, tTo, r);
        for (size_t i = 0; i<n; ++i)
        {
            TSTime64 t = CAdcChan::WriteData(r[i].m_pData, r[i].m_n, r[i].m_tStart);
            if (t < 0)
                return (int)t;
        }

        if (n > 0)      // Say where we have written up to
            m_pCirc->SetCleanUpTo(r[n - 1].m_tStart + r[n - 1].m_n * m_chanHead.m_tDivide);
    }while(m_st.NextSaveRange(&tFrom, &tTo, tUpto));
    return 0;
}

//===================================== CRealWChan ======================================
/*!
Constructor for CRealWChan.

\param file    The file that this channel belongs to.
\param nChan   The channel number.
\param tDivide The sample interval in file time units.
*/
CRealWChan::CRealWChan(TSon64File& file, TChanNum nChan, TSTime64 tDivide)
    : CSon64Chan(file, nChan, RealWave)
{
    assert(tDivide > 0);        // should have been checked before we get here
    if (!m_bmRead.HasDataBlock())       // this should NOT have one
        m_bmRead.SetDataBlock(new CRealWaveBlock(nChan, tDivide));
    m_chanHead.m_nObjSize = sizeof(float);
    m_chanHead.m_tDivide = tDivide;
}

// Write contiguus data to the channel at time tFrom
TSTime64 CRealWChan::WriteData(const float* pData, size_t count, TSTime64 tFrom)
{
    if (count == 0)                     // beware NULL operations
        return tFrom;

    TChanLock lock(m_mutex);            // take ownership of the channel

    // We allow overwriting of wave data if the times match exactly.
    TSTime64 tLast = MaxTimeNoLock();   // get maximum time (but not held in CAdcChan)
    TSTime64 tOff = tLast - tFrom;      // time past to do
    if (tOff >= 0)
    {
        int nWr = ChangeData(pData, count, tFrom);  // attempt to change old data
        if (nWr < 0)
            return nWr;

        TSTime64 tEnd = tFrom + (count-1)*m_chanHead.m_tDivide;
        if (tEnd <= tLast)                      // if no new data...
            return tEnd + m_chanHead.m_tDivide; // ...we are done.

        size_t nSkip = 1 + static_cast<size_t>(tOff / m_chanHead.m_tDivide);
        count -= nSkip;
        pData += nSkip;
        tFrom += nSkip*m_chanHead.m_tDivide;
    }

    int err = 0;
    if (!m_pWr)                         // make sure we have a write buffer
         err = InitWriteBlock(new CRealWaveBlock(m_nChan, m_chanHead.m_tDivide));

    if (err == 0)
    {
        // Write buffer holds last data written, if full, data is on disk
        while (count && (err == 0))
        {
            CRealWaveBlock* pWr = static_cast<CRealWaveBlock*>(m_pWr.get()); // get the raw pointer
            size_t nCopy = pWr->AddData(pData, count, tFrom);
            count -= nCopy;
            tFrom += nCopy*m_chanHead.m_tDivide; // Do not call ChanDivde() as will deadlock
            if ((nCopy==0) || pWr->SpaceContiguous() == 0)  // if buffer is full
            {
                err = AppendBlock(pWr);     // add to the disk, clear unsaved flag
                if (count)                  // if more to write...
                    pWr->clear();           // ...prepare to add more
            }
        }
    }
    return err<0 ? err : tFrom;
}

// Overwrite existing data. If the tFrom is not aligned properly we simple-mindedly
// work out indices to overwrite... if there are gaps we skip over them.
// Data could be in the read buffer, or pending write in the write buffer.
int CRealWChan::ChangeData(const float* pData, size_t count, TSTime64 tFrom)
{
    assert(count > 0);          // madness check

    // If our change overlaps the write buffer, deal with this first.
    if (m_pWr)
    {
        size_t first;           // will be index of the first one used
        int nUsed = m_pWr->ChangeWave(pData, count, tFrom, first);
        if (nUsed > 0)          // if we updated something...
        {
            assert(first < count);
            count = first;      // change count left to do
            if (count == 0)     // if we used the lot...
                return 0;       // ...there is nothing left to do
        }
        else if (nUsed == 0)    // if data is after block...
            return 0;           // ...then nothing left to do
    }

    // Now process the blocks already written to disk
    int iRet = m_bmRead.LoadBlock(tFrom);   // seek the start time

    while( count && (iRet == 0))
    {
        size_t first;           // will be index of the first one used
        int nUsed = m_bmRead.DataBlock().ChangeWave(pData, count, tFrom, first);
        if (nUsed > 0)          // if we updated something...
        {
            first += nUsed;     // next index in pData to use
            assert(first <= count);
            count -= first;     // count left to do
            pData += first;     // next pointer
            tFrom += first*m_chanHead.m_tDivide; // Start time matching new pData
        }
        else if (nUsed < 0)     // if past the data...
            break;              // ..we are done

        iRet = m_bmRead.SaveIfUnsaved(); // save if unsaved data
        if ((iRet == 0) && count)
            iRet = m_bmRead.NextBlock();
    }

    return (iRet < 0) ? iRet : 0;
}

// Read contiguous data into the buffer from the channel. We ignore the filter as it
// does not apply to waveform data.
// pData    The buffer to append dat to
// r        The range and max items are in here
// tFirst   if r.First() then fill in with first time, otherwise leave alone.
//          if not first time, data must start at r.From().
// returns  Number of points read.
int CRealWChan::ReadData(float* pData, CSRange& r, TSTime64& tFirst, const CSFilter*)
{
    if (!r.HasRange())                      // if nothing possible...
        return 0;                           // ...bail out now

    int nRead = 0;                          // will be the count of read data

    TChanLock lock(m_mutex);                // take ownership of the channel

    // If we have a write buffer, and our time range includes data in the buffer, use that
    // in preference to reading from the disk. If the write buffer exists, it holds the
    // last block and holds at least 1 data value.
    TSTime64 tBufStart = m_pWr ? m_pWr->FirstTime() : TSTIME64_MAX;

    if (r.From() < tBufStart)   // Only search disk if we need to
    {
        int err = m_bmRead.LoadBlock(r.From());    // get the block
        if (err < 0)
            return err;

        // If the data is on disk, read it, but if we reach the write buffer we
        // are done and collect the write buffer data. This is to make sure that
        // if the write buffer was partially written we get the new data.
        while ((err == 0) && (m_bmRead.DataBlock().FirstTime() < tBufStart))
        {
            size_t nCopy = m_bmRead.DataBlock().GetData(pData, r, tFirst);
            nRead += static_cast<int>(nCopy);
            if (!r.CanContinue())
                return nRead;
            err = m_bmRead.NextBlock();         // fetch next block
        }
    }

    // If we get here with room in the buffer and not done, then we must look in the write system to
    // see if there is pending data. BEWARE: if not first, then r.From() must match buffer start or
    // we cannot continue.
    // if a buffer, and something left to do and possible data
    if (m_pWr && r.CanContinue() && (r.Upto() > tBufStart) &&
        (r.First() || (r.From() == tBufStart))) // ...either first or contiguous
    {
        size_t nCopy = m_pWr->GetData(pData, r, tFirst);
        nRead += static_cast<int>(nCopy);
    }

    return nRead;
}

// Get wavedata from RealWave channel as shorts.
int CRealWChan::ReadData(short* pData, CSRange& r, TSTime64& tFirst, const CSFilter*)
{
    vector<float> pF(r.Max());                  // space to collect data
    int iRet = ReadData(pF.data(), r, tFirst);  // collect the data
    if (iRet > 0)
        float2short(pData, pF.data(), static_cast<size_t>(iRet));
    return iRet;
}

//========================= buffered RealWave channel =======================================
//! Buffered RealWave channel constructor
/*!
\param file     The file that owns this channel.
\param nChan    The channel number in the file.
\param tDivide  The sample interval in file time units.
\param bSize    The desired circular buffer size in waveform points.
*/
CBRealWChan::CBRealWChan(TSon64File& file, TChanNum nChan, TSTime64 tDivide, size_t bSize)
    : m_nMinMove( bSize >> CircBuffMinShift )
    , CRealWChan(file, nChan, tDivide)
{
    m_pCirc = std::make_unique<circ_buff>(bSize, tDivide);
}

void CBRealWChan::Save(TSTime64 t, bool bSave)
{
    TBufLock lock(m_mutBuf);                            // acquire the buffer
    TSTime64 tLast = LastCommittedWriteTime();          // cannot change before this
    m_st.SetSave(t < tLast ? tLast+1 : t, bSave);
}

void CBRealWChan::SaveRange(TSTime64 tFrom, TSTime64 tUpto)
{
    TBufLock lock(m_mutBuf);                            // acquire the buffer
    TSTime64 tLast = LastCommittedWriteTime();          // cannot change before this
    m_st.SaveRange(tFrom < tLast ? tLast : tFrom, tUpto);
}

bool CBRealWChan::IsSaving(TSTime64 tAt) const
{
    TBufLock lock(m_mutBuf);                            // acquire the buffer
    return m_st.IsSaving(tAt);
}

int  CBRealWChan::NoSaveList(TSTime64* pTimes, int nMax, TSTime64 tFrom, TSTime64 tUpto) const
{
    TBufLock lock(m_mutBuf);                            // acquire the buffer
    return m_st.NoSaveList(pTimes, nMax, tFrom, tUpto);
}

void CBRealWChan::LatestTime(TSTime64 t)
{
    TBufLock lock(m_mutBuf);                            // acquire the buffer
    TSTime64 tLast = m_pCirc ? m_pCirc->LastTime() : -1;
    m_st.SetDeadRange(tLast, t);
}

void CBRealWChan::ResizeCircular(size_t nItems)
{
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

TSTime64 CBRealWChan::WriteData(const float* pData, size_t count, TSTime64 tFrom)
{
    if (count == 0)                                     // detect stup cases...
        return tFrom;                                   // ...and do nothing

    TBufLock lock(m_mutBuf);                            // acquire the buffer
    if (!m_pCirc || !m_pCirc->capacity())
        return CRealWChan::WriteData(pData, count, tFrom);

    // Now see if we are overwriting, in which case pass on to change code
    TSTime64 tLast = m_pCirc->LastTime();               // last time in buffer or -1
    if (tFrom <= tLast)                                 // if we overlap written data
    {
        int iRet = ChangeData(pData, count, tFrom);     // do whatever is needed
        if (iRet < 0)
            return iRet;

        // CHECK: the rounding up needs checking. Calculate points written from buffer
        size_t nWritten = count;        // default is that we wrote the lot
        TSTime64 tOff = m_pCirc->LastTime() - tFrom;    // time past to do
        if (tOff >= 0)                   // if some data left to do
        {
            nWritten = static_cast<size_t>(tOff / m_chanHead.m_tDivide) + 1;
            if (nWritten > count)       // cannot have written...
                nWritten = count;       // ...more than we have
        }

        count -= nWritten;
        pData += nWritten;
        tFrom += nWritten*m_chanHead.m_tDivide;

        if (count == 0)                 // if job is done...
            return tFrom;               // ...we are finsihed
    }

    int err = 0;
    if (tFrom != tLast + m_chanHead.m_tDivide)          // if not contiguous
    {
        err = CommitToWriteBuffer(TSTIME64_MAX);        // flush the lot
        if (err)
            return err;
        m_pCirc->flush(tFrom);                          // buffer is now empty
        m_st.SetFirstTime(tFrom);                       // so move saved times on
    }
    else if (m_pCirc->empty())
        m_pCirc->SetFirstTime(tFrom);

    // At this point, data is contiguous or the buffer is empty. The buffer start
    // is set.
    size_t written = m_pCirc->add(pData, count);        // add what we can
    tFrom += written*m_chanHead.m_tDivide;              // move time onwards
    count -= written;                                   // reduce what is left
    if (count == 0)                                     // if all added ok...
        return tFrom;                                   // ...we are done

    // At this point, the circular buffer is full and we have more data to write we
    // set ourselves a minimum number of points to move at a time to avoid thrashing
    pData += written;                                   // move data pointer onwards
    size_t spaceNeeded = count;
    if (spaceNeeded < m_nMinMove)                       // Don't move tiny ammounts...
        spaceNeeded = m_nMinMove;                       // ...as inefficient

    // To move data to the disk buffer, we need contiguous elements from our circular buffer.
    const size_t capacity = m_pCirc->capacity();        // how much we could put in
    if (spaceNeeded >= capacity)                        // we have a lot to write
    {
        err = CommitToWriteBuffer(TSTIME64_MAX);        // commit everything we have
        if (err)
            return err;
        m_pCirc->flush(tFrom);                          // buffer is now empty
        m_st.SetFirstTime(tFrom);                       // and move time on

        // We want to leave the buffer as full as we can so that oldest data possible is
        // visible to the user. OPTIMISE: if buffer state is all or none, we could skip
        // moving data through the buffer and just write it.
        while (count > capacity)                        // if more than can fit in buffer
        {
            size_t nWrite = std::min(count-capacity, capacity); // max to write
            m_pCirc->add(pData, nWrite);                // start time already set
            count -= nWrite;
            pData += nWrite;
            err = CommitToWriteBuffer(TSTIME64_MAX);    // flush all to disk buffers
            if (err)
                return err;
            tFrom += nWrite* m_chanHead.m_tDivide;      // move time on
            m_pCirc->flush(tFrom);                      // Empty, set start time
        }
        m_st.SetFirstTime(tFrom);                       // committed up to here
    }
    else
    {
        TSTime64 tUpto = m_pCirc->FirstTime() + spaceNeeded*m_chanHead.m_tDivide;       // final time
        err = CommitToWriteBuffer(tUpto);               // Commit up to needed time
        if (err)
            return err;

        // The buffer is currently full, and spaceNeeded < capacity, so free() will
        // not call flush, so the buffer start time will still be valid.
        m_pCirc->free(spaceNeeded);                     // release used space
    }

    // Any data written here MUST be contiguous
    written = m_pCirc->add(pData, count);               // this should now fit
    assert(written == count);

    return tFrom + written*m_chanHead.m_tDivide;
}

// Change any data that overlaps our buffer. We could be fussy and insist that
// times match exactly. This implementation takes data as extending up to the
// next data point. Returns 0 if done, -ve means an error.
int CBRealWChan::ChangeData(const float* pData, size_t count, TSTime64 tFrom)
{
    assert(m_pCirc);
    // Start by handling any changes in written data.
    int iRet = CRealWChan::ChangeData(pData, count, tFrom);
    if ((iRet < 0) || !m_pCirc || m_pCirc->empty())
        return iRet;

    return m_pCirc->change(pData, count, tFrom);
}

int CBRealWChan::ReadData(float* pData, CSRange& r, TSTime64& tFirst, const CSFilter* pFilter)
{
    assert(r.HasRange());
    TBufLock lock(m_mutBuf);                    // acquire the buffer
    if (!m_pCirc || m_pCirc->empty())
        return CRealWChan::ReadData(pData, r, tFirst, pFilter);

    TSTime64 tReadEnd( r.Upto() );              // save end
    TSTime64 tBufStart = m_pCirc->FirstTime();
    r.SetUpto(std::min(tBufStart, r.Upto()));   // limit disk reads
    int nRead = CRealWChan::ReadData(pData, r, tFirst, pFilter);
    if ((nRead < 0) || r.IsTimedOut())          // read error or timed out
        return nRead;

    pData += nRead;                             // where to read to
    r.SetUpto(tReadEnd);                        // restore end
    int nBuff = m_pCirc->read(pData, r.Max(), r.From(), tReadEnd, tFirst, r.First());
    r.ReduceMax(static_cast<size_t>(nBuff));
    nRead += nBuff;
    return nRead;
}

// Return the last time that we know of for this channel or -1 if no data
TSTime64 CBRealWChan::MaxTime() const
{
    TBufLock lock(m_mutBuf);                // acquire the buffer
    if (m_pCirc && !m_pCirc->empty())       // if data in the circular buffer...
        return m_pCirc->LastTime();         // ...return the time
    return CRealWChan::MaxTime();
}

// Find the event that is n before the tFrom. Put another way, find the event that if we read n
// events, the last event read would be the one before tFrom.
// r        The range to search, back to r.From(), first before r.Upto().
//          r.Max() is number to skip. This is uint32_t in an effort to discourage
//          backward searches through the entire file.
// pFilter  Not used in this class; will filter the events.
TSTime64 CBRealWChan::PrevNTime(CSRange& r, const CSFilter* pFilter, bool bAsWave)
{
    if (!r.HasRange())                      // Check it is even possible
        return -1;

    TBufLock lock(m_mutBuf);                // acquire the buffer
    if (m_pCirc && !m_pCirc->empty())
    {
        TSTime64 t = m_pCirc->PrevNTime(r);
        if (!r.HasRange())                  // if changed and cannot continue...
            return t;                       // ...this is the last time we found
    }

    return CRealWChan::PrevNTime(r, pFilter);
}

bool CBRealWChan::IsModified() const
{
    TBufLock lock(m_mutBuf);                // acquire the buffer
    if (m_pCirc && !m_pCirc->empty())
    {
        TSTime64 tFrom, tTo;
        return m_st.FirstSaveRange(&tFrom, &tTo, TSTIME64_MAX);
    }
    return CRealWChan::IsModified();
}

uint64_t CBRealWChan::GetChanBytes() const
{
    TBufLock lock(m_mutBuf);                        // acquire the buffer
    uint64_t total = CRealWChan::GetChanBytes();    // get committed bytes
    if (m_pCirc && !m_pCirc->empty())
    {
        TSTime64 t = LastCommittedWriteTime();
        total += m_pCirc->Count(t+1)*sizeof(float); // add uncommitted bytes in buffer
    }
    return total;
}

// Make sure that anything committable in the circular buffer is committed
int CBRealWChan::Commit()
{
    TBufLock lock(m_mutBuf);                // acquire the buffer
    int err = 0;
    if (m_pCirc && !m_pCirc->empty())
        err = CommitToWriteBuffer();

    if (err == 0)
        err = CRealWChan::Commit();           // flush data through to disk
    return err;
}

//! Commit all data up to a nominated time to the write buffer.
/*!
We are to take notice of the m_st list. We know that the list holds where we reached
in committing.
\param tUpto The time limit (non-inclusive) for saving.
*/
int CBRealWChan::CommitToWriteBuffer(TSTime64 tUpto)
{
    assert(m_pCirc);
    TSTime64 tFrom, tTo;
    if (!m_st.FirstSaveRange(&tFrom, &tTo, tUpto, m_pCirc->FirstDirty()))
        return 0;
    do
    {
        CircWBuffer<float>::range r[2];    // pair of ranges
        size_t n = m_pCirc->contig_range(tFrom, tTo, r);
        for (size_t i = 0; i<n; ++i)
        {
            TSTime64 t = CRealWChan::WriteData(r[i].m_pData, r[i].m_n, r[i].m_tStart);
            if (t<0)
                return (int)t;
        }

        if (n > 0)      // Say where we have written up to
            m_pCirc->SetCleanUpTo(r[n - 1].m_tStart + r[n - 1].m_n * m_chanHead.m_tDivide);
    }while(m_st.NextSaveRange(&tFrom, &tTo, tUpto));
    return 0;
}

//========================= file commands ==============================================
// chan     The channel number in the file (0 up to m_vChanHead.size())
// tDvd     The channel clock divisor to use from the file rate.
// wKind    The type of the waveform, Adc or RealWave
// dRate    The ideal sampling rate or 0 if we are to use tDvd to figure it out.
// iPhyCh   The physical channel number (application use), not checked
int TSon64File::SetWaveChan(TChanNum chan, TSTime64 tDvd, TDataKind wKind, double dRate, int iPhyCh)
{
    TChWrLock lock(m_mutChans);     // we will be changing m_vChan
    int err = ResetForReuse(chan);  // check channel in a decent state
    if (err == S64_OK)
    {
        if (tDvd <= 0)              // channel divide MUST be sensible
            return BAD_PARAM;

        switch(wKind)
        {
        case Adc:     
            m_vChan[chan].reset(m_bOldFile ? new CAdcChan(*this, chan, tDvd) : new CBAdcChan(*this, chan, tDvd));
            break;
        case RealWave:
            m_vChan[chan].reset(m_bOldFile ? new CRealWChan(*this, chan, tDvd) : new CBRealWChan(*this, chan, tDvd));
            break;
        default:
            return CHANNEL_TYPE;
        }
   
        m_vChan[chan]->SetPhyChan(iPhyCh);

        if (dRate <= 0.0)
            dRate = 1.0 /(tDvd * GetTimeBase());
        m_vChan[chan]->SetIdealRate(dRate);
    }
    return err;
}

TSTime64 TSon64File::WriteWave(TChanNum chan, const short* pData, size_t count, TSTime64 tFrom)
{
    if (m_bReadOnly)
        return READ_ONLY;
    if (count == 0)
        return tFrom;

    TChRdLock lock(m_mutChans);     // we are not changing the #chans
    if ((chan >= m_vChanHead.size()) || !m_vChan[chan])
        return NO_CHANNEL;

    return m_vChan[chan]->WriteData(pData, count, tFrom);
}

TSTime64 TSon64File::WriteWave(TChanNum chan, const float* pData, size_t count, TSTime64 tFrom)
{
    if (m_bReadOnly)
        return READ_ONLY;
    if (count == 0)
        return tFrom;

    TChRdLock lock(m_mutChans);     // we are not changing the #chans
    if ((chan >= m_vChanHead.size()) || !m_vChan[chan])
        return NO_CHANNEL;

    return m_vChan[chan]->WriteData(pData, count, tFrom);
}

int TSon64File::ReadWave(TChanNum chan, short* pData, int nMax, TSTime64 tFrom, TSTime64 tUpto, TSTime64& tFirst, const CSFilter* pFilter)
{
    assert((nMax>0) && (tFrom < tUpto) && (tUpto > 0));
    if ((nMax <= 0) || (tUpto <= 0) || (tFrom >= tUpto))
        return 0;
    TChRdLock lock(m_mutChans);     // we are not changing the #chans
    if ((chan >= m_vChanHead.size()) || !m_vChan[chan])
        return NO_CHANNEL;

    CSRange r(tFrom, tUpto, nMax);   // make a range object to manage the request
    int nGot = 0;
    while (true)
    {
        int n = m_vChan[chan]->ReadData(pData, r, tFirst, pFilter);
        if (n < 0)
            return n;

        nGot += n;
        if (!r.IsTimedOut() || !r.Max())
            return nGot;

        r.SetTimeOut();             // allow to run again
        pData += n;                 // move pointer onwards
    }
}

int TSon64File::ReadWave(TChanNum chan, float* pData, int nMax, TSTime64 tFrom, TSTime64 tUpto, TSTime64& tFirst, const CSFilter* pFilter)
{
    assert((nMax > 0) && (tFrom < tUpto) && (tUpto > 0));
    if ((nMax <= 0) || (tUpto <= 0) || (tFrom >= tUpto))
        return 0;
    TChRdLock lock(m_mutChans);     // we are not changing the #chans
    if ((chan >= m_vChanHead.size()) || !m_vChan[chan])
        return NO_CHANNEL;

    CSRange r(tFrom, tUpto, nMax);   // make a range object to manage the request
    int nGot = 0;
    while (true)
    {
        int n = m_vChan[chan]->ReadData(pData, r, tFirst, pFilter);
        if (n < 0)
            return n;

        nGot += n;
        if (!r.IsTimedOut() || !r.Max())
            return nGot;

        r.SetTimeOut();             // allow to run again
        pData += n;                 // move pointer onwards
    }
}
